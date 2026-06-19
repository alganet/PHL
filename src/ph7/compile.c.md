# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5246/6569 lines (79.86%)

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
|       2 |   108 |  |
|       - |   109 | `	Label *aLabel;` |
|       - |   110 | `	sxu32 n;` |
|       - |   111 | `	/* Perform a linear scan on the label table */` |
|     150 |   112 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     330 |   113 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     274 |   114 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|       - |   115 | `			/* Jump destination found */` |
|      94 |   116 | `			aLabel[n].bRef = TRUE;` |
|      94 |   117 | `			if( ppOut ){` |
|      94 |   118 | `				*ppOut = &aLabel[n];` |
|      46 |   119 | `			}` |
|      94 |   120 | `			return SXRET_OK;` |
|       - |   121 | `		}` |
|      92 |   122 | `	}` |
|       - |   123 | `	/* No such destination */` |
|      57 |   124 | `	return SXERR_NOTFOUND;` |
|      76 |   125 |  |
|       - |   126 | `/*` |
|       - |   127 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |   128 | ` * compiled blocks.` |
|       - |   129 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |   130 | ` */` |
|    3422 |   131 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |   132 |  |
|    3424 |   133 | `	GenBlock *pBlock = pCurrent;` |
|    9693 |   134 | `	for(;;){` |
|   19388 |   135 | `		if( pBlock->iFlags & iBlockType ){` |
|    3316 |   136 | `			iCount--; /* Decrement nesting level */` |
|    3316 |   137 | `			if( iCount < 1 ){` |
|       - |   138 | `				/* Block meet with the desired criteria */` |
|    3290 |   139 | `				return pBlock;` |
|       - |   140 | `			}` |
|      13 |   141 | `		}` |
|       - |   142 | `		/* Point to the upper block */` |
|   16100 |   143 | `		pBlock = pBlock->pParent;` |
|   16100 |   144 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   145 | `			/* Forbidden */` |
|      69 |   146 | `			break;` |
|       - |   147 | `		}` |
|       2 |   148 | `	}` |
|       - |   149 | `	/* No such block */` |
|     136 |   150 | `	return 0;` |
|    1713 |   151 |  |
|       - |   152 | `/*` |
|       - |   153 | ` * Initialize a freshly allocated block instance.` |
|       - |   154 | ` */` |
|  742762 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       2 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  742764 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  742764 |   165 | `	pBlock->pUserData   = pUserData;` |
|  742764 |   166 | `	pBlock->pGen        = pGen;` |
|  742764 |   167 | `	pBlock->iFlags      = iType;` |
|  742764 |   168 | `	pBlock->pParent     = 0;` |
|  742764 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  742764 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  742764 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  739614 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       2 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  739616 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  739616 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  739616 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  739616 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  739616 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  739616 |   203 | `	pGen->pCurrent = pBlock;` |
|  739616 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  359238 |   206 | `		*ppBlock = pBlock;` |
|  179618 |   207 | `	}` |
|  739616 |   208 | `	return SXRET_OK;` |
|  369809 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  739606 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |   214 |  |
|  739608 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  739608 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  739608 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  739606 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |   222 |  |
|  739608 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  739608 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  739608 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  739608 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  739606 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |   232 |  |
|  739608 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  739608 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  739608 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  739608 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  739608 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  739608 |   247 | `	return SXRET_OK;` |
|  369805 |   248 |  |
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
|  210036 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  210038 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  210038 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  210038 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  210038 |   268 | `	return rc;` |
|       2 |   269 |  |
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
|  517372 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  517374 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
|  931264 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  413892 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  165274 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  248620 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   38586 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  210036 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  210036 |   301 | `		if( pInstr ){` |
|  210036 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  210036 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  210036 |   305 | `			aFix[n].nJumpType = -1;` |
|  105017 |   306 | `		}` |
|  105019 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  517374 |   309 | `	return nFixed;` |
|       2 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  210220 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  210222 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  210368 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     150 |   329 | `		pJump = &aJumps[n];` |
|       - |   330 | `		/* Extract the target label */` |
|     150 |   331 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     150 |   332 | `		if( rc != SXRET_OK ){` |
|       - |   333 | `			/* No such label */` |
|      57 |   334 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      57 |   335 | `			if( rc == SXERR_ABORT ){` |
|       3 |   336 | `				return SXERR_ABORT;` |
|       - |   337 | `			}` |
|      55 |   338 | `			continue;` |
|       - |   339 | `		}` |
|       - |   340 | `		/* Make sure the target label is reachable */` |
|      94 |   341 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|       9 |   342 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|       9 |   343 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   344 | `				return SXERR_ABORT;` |
|       - |   345 | `			}` |
|       4 |   346 | `		}` |
|       - |   347 | `		/* Fix the jump now the destination is resolved */` |
|      94 |   348 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      94 |   349 | `		if( pInstr ){` |
|      94 |   350 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   351 | `		}` |
|      48 |   352 | `	}` |
|  210220 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  210352 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      37 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      68 |   360 | `	}` |
|  210220 |   361 | `	return SXRET_OK;` |
|  105112 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  664726 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  664728 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  664728 |   370 | `	if( pEntry == 0 ){` |
|  288860 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  375870 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  375870 |   374 | `	return SXRET_OK;` |
|  332365 |   375 |  |
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
|  288858 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |   387 |  |
|  288860 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  288860 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  144429 |   390 | `	}` |
|  288860 |   391 | `	return SXRET_OK;` |
|       2 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  110750 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  110752 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  110752 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  110752 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  110752 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  110752 |   411 | `	return pObj;` |
|   55377 |   412 |  |
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
|  397482 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       2 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  397484 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      32 |   431 | `	if( p3 == 0 ){` |
|      30 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      30 |   433 | `		if( pMap == 0 ) return 0;` |
|      30 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      30 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      32 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      32 |   438 | `	return p3;` |
|  198743 |   439 |  |
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
|       2 |   477 |  |
|    1078 |   478 | `	if( base == 16 ){ return SyisHex(c); }` |
|     980 |   479 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     702 |   480 | `	return SyisDigit(c);` |
|     540 |   481 |  |
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
|  111340 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |   500 |  |
|  111342 |   501 | `	const char *z = pRaw->zString;` |
|  111342 |   502 | `	sxu32 n = pRaw->nByte;` |
|  111342 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  111342 |   505 | `	if( n < 2 ) return 0;` |
|    9370 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|    9335 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   34256 |   511 | `	for( i = 0; i < n; ++i ){` |
|   24902 |   512 | `		if( z[i] != '_' ) continue;` |
|     814 |   513 | `		if( i > 0 && i + 1 < n` |
|     543 |   514 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     540 |   515 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |   516 | `			continue; /* well-placed separator */` |
|       - |   517 | `		}` |
|       - |   518 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |   519 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      15 |   520 | `		start = i;` |
|      20 |   521 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |   522 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       5 |   523 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |   524 | `		}` |
|      15 |   525 | `		*pBadStart = &z[start];` |
|      15 |   526 | `		*pBadLen = n - start;` |
|      15 |   527 | `		return 1;` |
|     ! 0 |   528 | `	}` |
|    9356 |   529 | `	return 0;` |
|   55672 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  111340 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |   540 |  |
|  111342 |   541 | `	const char *zBad = 0;` |
|  111342 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  111342 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  111328 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      15 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      15 |   554 | `	return SXERR_SYNTAX;` |
|   55672 |   555 |  |
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
|  111326 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       2 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  111328 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  111328 |   581 | `	*pzAlloc = 0;` |
|  236120 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  125046 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   62398 |   584 | `	}` |
|  111328 |   585 | `	if( !hasUnderscore ){` |
|  111076 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  111076 |   587 | `		return SXRET_OK;` |
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
|   55665 |   604 |  |
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
|  111312 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   622 |  |
|  111314 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  111314 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  111314 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   55656 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  111314 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  111314 |   631 | `	if( rc != SXRET_OK ){` |
|      11 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  166955 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   55651 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  111304 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  111304 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  110752 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  110752 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  110752 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  110752 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   55377 |   649 | `	}else{` |
|       - |   650 | `		/* Real number */` |
|       - |   651 | `		ph7_value *pObj;` |
|       - |   652 | `		/* Reserve a new constant */` |
|     554 |   653 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     554 |   654 | `		if( pObj == 0 ){` |
|     ! 0 |   655 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   656 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   657 | `			return SXERR_ABORT;` |
|       - |   658 | `		}` |
|     554 |   659 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     554 |   660 | `		PH7_MemObjToReal(pObj);` |
|       - |   661 | `	}` |
|  111304 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  111304 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  111304 |   666 | `	return SXRET_OK;` |
|   55658 |   667 |  |
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
|   80174 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   680 |  |
|   80176 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   80176 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   80176 |   687 | `	zIn  = pStr->zString;` |
|   80176 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   80176 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    6450 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    6450 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   73728 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   29396 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   29396 |   700 | `		return SXRET_OK;` |
|       - |   701 | `	}` |
|       - |   702 | `	/* Reserve a new constant */` |
|   44334 |   703 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   44334 |   704 | `	if( pObj == 0 ){` |
|     ! 0 |   705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   706 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   707 | `		return SXERR_ABORT;` |
|       - |   708 | `	}` |
|   44334 |   709 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   710 | `	/* Compile the node */` |
|   44384 |   711 | `	for(;;){` |
|   88770 |   712 | `		if( zIn >= zEnd ){` |
|       - |   713 | `			/* End of input */` |
|   44334 |   714 | `			break;` |
|       - |   715 | `		}` |
|   44438 |   716 | `		zCur = zIn;` |
|  699300 |   717 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  654864 |   718 | `			zIn++;` |
|       2 |   719 | `		}` |
|   44438 |   720 | `		if( zIn > zCur ){` |
|       - |   721 | `			/* Append raw contents*/` |
|   44414 |   722 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   22206 |   723 | `		}` |
|   44438 |   724 | `		zIn++;` |
|   44438 |   725 | `		if( zIn < zEnd ){` |
|     126 |   726 | `			if( zIn[0] == '\\' ){` |
|       - |   727 | `				/* A literal backslash */` |
|      23 |   728 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     115 |   729 | `			}else if( zIn[0] == '\'' ){` |
|       - |   730 | `				/* A single quote */` |
|      11 |   731 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   732 | `			}else{` |
|       - |   733 | `				/* verbatim copy */` |
|      94 |   734 | `				zIn--;` |
|      94 |   735 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      94 |   736 | `				zIn++;` |
|       - |   737 | `			}` |
|      62 |   738 | `		}` |
|       - |   739 | `		/* Advance the stream cursor */` |
|   44438 |   740 | `		zIn++;` |
|       2 |   741 | `	}` |
|       - |   742 | `	/* Emit the load constant instruction */` |
|   44334 |   743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   44334 |   744 | `	if( pStr->nByte < 1024 ){` |
|       - |   745 | `		/* Install in the literal table */` |
|   44334 |   746 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   22166 |   747 | `	}` |
|       - |   748 | `	/* Node successfully compiled */` |
|   44334 |   749 | `	return SXRET_OK;` |
|   40089 |   750 |  |
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
|     108 |   769 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       2 |   770 |  |
|     110 |   771 | `	SyString *pIn = &pGen->pIn->sData;` |
|     110 |   772 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   773 | `	const char *zPrefix;` |
|       - |   774 | `	const char *z, *zEnd;` |
|       - |   775 | `	char *zBuf, *zDst;` |
|     110 |   776 | `	if( nIndent == 0 ){` |
|       - |   777 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      66 |   778 | `		*pOut = *pIn;` |
|      66 |   779 | `		return SXRET_OK;` |
|       - |   780 | `	}` |
|       - |   781 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   782 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   783 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   784 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   785 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   786 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      46 |   787 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      46 |   788 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   789 | `		zPrefix += 2;` |
|     ! 0 |   790 | `	}else{` |
|      46 |   791 | `		zPrefix += 1;` |
|       - |   792 | `	}` |
|       - |   793 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      46 |   794 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      46 |   795 | `	if( zBuf == 0 ){` |
|     ! 0 |   796 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   797 | `		return SXERR_ABORT;` |
|       - |   798 | `	}` |
|      46 |   799 | `	zDst = zBuf;` |
|      46 |   800 | `	z = pIn->zString;` |
|      46 |   801 | `	zEnd = z + pIn->nByte;` |
|     128 |   802 | `	while( z < zEnd ){` |
|      70 |   803 | `		const char *zLine = z;` |
|       - |   804 | `		sxu32 nLine;` |
|       - |   805 | `		int bEmpty;` |
|     798 |   806 | `		while( z < zEnd && z[0] != '\n' ){` |
|     730 |   807 | `			z++;` |
|       2 |   808 | `		}` |
|      70 |   809 | `		nLine = (sxu32)(z - zLine);` |
|      70 |   810 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      70 |   811 | `		if( !bEmpty ){` |
|       - |   812 | `			sxu32 i;` |
|      66 |   813 | `			if( nLine < nIndent ){` |
|     ! 0 |   814 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   815 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   816 | `					nIndent);` |
|     ! 0 |   817 | `				return SXERR_ABORT;` |
|       - |   818 | `			}` |
|     268 |   819 | `			for( i = 0; i < nIndent; i++ ){` |
|     212 |   820 | `				if( zLine[i] != zPrefix[i] ){` |
|       9 |   821 | `					unsigned char c = (unsigned char)zLine[i];` |
|       9 |   822 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |   823 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   824 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |   825 | `					}else{` |
|       7 |   826 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   827 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   828 | `							nIndent);` |
|       - |   829 | `					}` |
|       9 |   830 | `					return SXERR_ABORT;` |
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
|      56 |   847 |  |
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
|      44 |   862 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   863 |  |
|       - |   864 | `	SyString sStripped;` |
|       - |   865 | `	SyString *pStr;` |
|       - |   866 | `	ph7_value *pObj;` |
|       - |   867 | `	sxu32 nIdx;` |
|       - |   868 | `	sxi32 rc;` |
|      46 |   869 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      46 |   870 | `	if( rc != SXRET_OK ){` |
|       5 |   871 | `		return rc;` |
|       - |   872 | `	}` |
|      42 |   873 | `	pStr = &sStripped;` |
|      42 |   874 | `	nIdx = 0; /* Prevent compiler warning */` |
|      42 |   875 | `	if( pStr->nByte <= 0 ){` |
|       - |   876 | `		/* Empty string,load NULL */` |
|       7 |   877 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   878 | `		return SXRET_OK;` |
|       - |   879 | `	}` |
|       - |   880 | `	/* Reserve a new constant */` |
|      36 |   881 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      36 |   882 | `	if( pObj == 0 ){` |
|     ! 0 |   883 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   884 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   885 | `		return SXERR_ABORT;` |
|       - |   886 | `	}` |
|       - |   887 | `	/* No processing is done here, simply a memcpy() operation */` |
|      36 |   888 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   889 | `	/* Emit the load constant instruction */` |
|      36 |   890 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   891 | `	/* Node successfully compiled */` |
|      36 |   892 | `	return SXRET_OK;` |
|      24 |   893 |  |
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
|    2056 |   916 | `static sxi32 GenStateProcessStringExpression(` |
|       - |   917 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   918 | `	sxu32 nLine,         /* Line number */` |
|       - |   919 | `	const char *zIn,     /* Raw expression */` |
|       - |   920 | `	const char *zEnd     /* End of the expression */` |
|       - |   921 | `	)` |
|       2 |   922 |  |
|       - |   923 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |   924 | `	SySet sToken;` |
|       - |   925 | `	sxi32 rc;` |
|       - |   926 | `	/* Initialize the token set */` |
|    2058 |   927 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   928 | `	/* Preallocate some slots */` |
|    2058 |   929 | `	SySetAlloc(&sToken,0x08);` |
|       - |   930 | `	/* Tokenize the text */` |
|    2058 |   931 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   932 | `	/* Swap delimiter */` |
|    2058 |   933 | `	pTmpIn  = pGen->pIn;` |
|    2058 |   934 | `	pTmpEnd = pGen->pEnd;` |
|    2058 |   935 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2058 |   936 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   937 | `	/* Compile the expression */` |
|    2058 |   938 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   939 | `	/* Restore token stream */` |
|    2058 |   940 | `	pGen->pIn  = pTmpIn;` |
|    2058 |   941 | `	pGen->pEnd = pTmpEnd;` |
|       - |   942 | `	/* Release the token set */` |
|    2058 |   943 | `	SySetRelease(&sToken);` |
|       - |   944 | `	/* Compilation result */` |
|    2058 |   945 | `	return rc;` |
|       2 |   946 |  |
|       - |   947 | `/*` |
|       - |   948 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   949 | ` */` |
|   21864 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   21866 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   21866 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   21866 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   21866 |   960 | `	(*pCount)++;` |
|   21866 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   21866 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   21866 |   964 | `	return pConstObj;` |
|   10934 |   965 |  |
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
|   20420 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  1005 |  |
|   20422 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   20422 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   20422 |  1012 | `	zIn  = pStr->zString;` |
|   20422 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   20422 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     274 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     274 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   20150 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   20150 |  1024 | `	iCons = 0;` |
|   11102 |  1025 | `	for(;;){` |
|   33398 |  1026 | `		zCur = zIn;` |
|  162988 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  131648 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      65 |  1029 | `				break;` |
|  131524 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1934 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     967 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  129592 |  1034 | `			zIn++;` |
|       2 |  1035 | `		}` |
|   33398 |  1036 | `		if( zIn > zCur ){` |
|   15424 |  1037 | `			if( pObj == 0 ){` |
|   15044 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   15044 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    7521 |  1042 | `			}` |
|   15424 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    7711 |  1044 | `		}` |
|   33398 |  1045 | `		if( zIn >= zEnd ){` |
|   20150 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   13250 |  1048 | `		if( zIn[0] == '\\' ){` |
|   11194 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   11194 |  1051 | `			zIn++;` |
|   11194 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   11194 |  1055 | `			if( pObj == 0 ){` |
|    6824 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    6824 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3411 |  1060 | `			}` |
|   11194 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   11194 |  1062 | `			switch( zIn[0] ){` |
|       3 |  1063 | `			case '$':` |
|       - |  1064 | `				/* Dollar sign */` |
|       7 |  1065 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  1066 | `				break;` |
|      44 |  1067 | `			case '\\':` |
|       - |  1068 | `				/* A literal backslash */` |
|      90 |  1069 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      90 |  1070 | `				break;` |
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
|    5170 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   10342 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   10342 |  1086 | `				break;` |
|      19 |  1087 | `			case 'r':` |
|       - |  1088 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 |  1089 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 |  1090 | `				break;` |
|      24 |  1091 | `			case 't':` |
|       - |  1092 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 |  1093 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 |  1094 | `				break;` |
|       3 |  1095 | `			case 'v':` |
|       - |  1096 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1097 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1098 | `				break;` |
|       1 |  1099 | `			case '\'':` |
|       - |  1100 | `				/* Single quote */` |
|       3 |  1101 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  1102 | `				break;` |
|      66 |  1103 | `			case '"':` |
|       - |  1104 | `				/* Double quote */` |
|     134 |  1105 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     134 |  1106 | `				break;` |
|       6 |  1107 | `			case '0':` |
|       - |  1108 | `				/* NUL byte */` |
|      13 |  1109 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      13 |  1110 | `				break;` |
|     226 |  1111 | `			case 'x':` |
|     453 |  1112 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1113 | `					int c;` |
|       - |  1114 | `					/* Hex digit */` |
|     439 |  1115 | `					c = SyHexToint(zIn[1]) << 4;` |
|     439 |  1116 | `					if( &zIn[2] < zEnd ){` |
|     439 |  1117 | `						c +=  SyHexToint(zIn[2]);` |
|     219 |  1118 | `					}` |
|       - |  1119 | `					/* Output char */` |
|     439 |  1120 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     439 |  1121 | `					n += sizeof(char) * 2;` |
|     220 |  1122 | `				}else{` |
|       - |  1123 | `					/* Output literal character  */` |
|      15 |  1124 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1125 | `				}` |
|     453 |  1126 | `				break;` |
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
|   11194 |  1154 | `			zIn += n;` |
|   11194 |  1155 | `			continue;` |
|       - |  1156 | `		}` |
|    2058 |  1157 | `		if( zIn[0] == '{' ){` |
|       - |  1158 | `			/* Curly syntax */` |
|       - |  1159 | `			const char *zExpr;` |
|     128 |  1160 | `			sxi32 iNest = 1;` |
|     128 |  1161 | `			zIn++;` |
|     128 |  1162 | `			zExpr = zIn;` |
|       - |  1163 | `			/* Synchronize with the next closing curly braces */` |
|    1340 |  1164 | `			while( zIn < zEnd ){` |
|    1340 |  1165 | `				if( zIn[0] == '{' ){` |
|       - |  1166 | `					/* Increment nesting level */` |
|       9 |  1167 | `					iNest++;` |
|    1336 |  1168 | `				}else if(zIn[0] == '}' ){` |
|       - |  1169 | `					/* Decrement nesting level */` |
|     136 |  1170 | `					iNest--;` |
|     136 |  1171 | `					if( iNest <= 0 ){` |
|     128 |  1172 | `						break;` |
|       - |  1173 | `					}` |
|       4 |  1174 | `				}` |
|    1214 |  1175 | `				zIn++;` |
|       2 |  1176 | `			}` |
|       - |  1177 | `			/* Process the expression */` |
|     128 |  1178 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     128 |  1179 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1180 | `				return SXERR_ABORT;` |
|       - |  1181 | `			}` |
|     128 |  1182 | `			if( rc != SXERR_EMPTY ){` |
|     128 |  1183 | `				++iCons;` |
|      63 |  1184 | `			}` |
|     128 |  1185 | `			if( zIn < zEnd ){` |
|       - |  1186 | `				/* Jump the trailing curly */` |
|     128 |  1187 | `				zIn++;` |
|      63 |  1188 | `			}` |
|      65 |  1189 | `		}else{` |
|       - |  1190 | `			/* Simple syntax */` |
|    1932 |  1191 | `			const char *zExpr = zIn;` |
|       - |  1192 | `			/* Assemble variable name */` |
|     971 |  1193 | `			for(;;){` |
|       - |  1194 | `				/* Jump leading dollars */` |
|    3874 |  1195 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1932 |  1196 | `					zIn++;` |
|       2 |  1197 | `				}` |
|     971 |  1198 | `				for(;;){` |
|   11185 |  1199 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8272 |  1200 | `						zIn++;` |
|       2 |  1201 | `					}` |
|    1944 |  1202 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1203 | `						/* UTF-8 stream */` |
|     ! 0 |  1204 | `						zIn++;` |
|     ! 0 |  1205 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1206 | `							zIn++;` |
|     ! 0 |  1207 | `						}` |
|     ! 0 |  1208 | `						continue;` |
|       - |  1209 | `					}` |
|    1944 |  1210 | `					break;` |
|     ! 0 |  1211 | `				}` |
|    1944 |  1212 | `				if( zIn >= zEnd ){` |
|     124 |  1213 | `					break;` |
|       - |  1214 | `				}` |
|    1822 |  1215 | `				if( zIn[0] == '[' ){` |
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
|    1812 |  1233 | `				}else if(zIn[0] == '{' ){` |
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
|    1808 |  1251 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1252 | `					/* Member access operator '->' */` |
|      13 |  1253 | `					zIn += 2;` |
|    1802 |  1254 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1255 | `					/* Static member access operator '::' */` |
|     ! 0 |  1256 | `					zIn += 2;` |
|     ! 0 |  1257 | `				}else{` |
|     899 |  1258 | `					break;` |
|       - |  1259 | `				}` |
|       1 |  1260 | `			}` |
|       - |  1261 | `			/* Process the expression */` |
|    1932 |  1262 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1932 |  1263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1264 | `				return SXERR_ABORT;` |
|       - |  1265 | `			}` |
|    1932 |  1266 | `			if( rc != SXERR_EMPTY ){` |
|    1930 |  1267 | `				++iCons;` |
|     964 |  1268 | `			}` |
|       - |  1269 | `		}` |
|       - |  1270 | `		/* Invalidate the previously used constant */` |
|    2058 |  1271 | `		pObj = 0;` |
|       2 |  1272 | `	}/*for(;;)*/` |
|   20150 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1530 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     764 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   20150 |  1278 | `	return SXRET_OK;` |
|   10212 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   20360 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   20362 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   10180 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   20362 |  1290 | `	return rc;` |
|       2 |  1291 |  |
|       - |  1292 | `/*` |
|       - |  1293 | ` * Compile a Heredoc string.` |
|       - |  1294 | ` *  See the block-comment above for more information.` |
|       - |  1295 | ` */` |
|      64 |  1296 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1297 |  |
|       - |  1298 | `	SyString sOrig, sStripped;` |
|       - |  1299 | `	sxi32 rc;` |
|      66 |  1300 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      66 |  1301 | `	if( rc != SXRET_OK ){` |
|       5 |  1302 | `		return rc;` |
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
|      34 |  1314 |  |
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
|   19052 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1335 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1336 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1337 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1338 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1339 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1340 | `	)` |
|       2 |  1341 |  |
|       - |  1342 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1343 | `	sxi32 rc;` |
|       - |  1344 | `	/* Swap token stream */` |
|   19054 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   19054 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   19054 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   19054 |  1350 | `	return rc;` |
|       2 |  1351 |  |
|       - |  1352 | `/*` |
|       - |  1353 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1354 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1355 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1356 | ` * error message.` |
|       - |  1357 | ` * See the routine responible of compiling the array language construct` |
|       - |  1358 | ` * for more inforation.` |
|       - |  1359 | ` */` |
|      36 |  1360 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1361 |  |
|      38 |  1362 | `	sxi32 rc = SXRET_OK;` |
|      38 |  1363 | `	if( pRoot->pOp ){` |
|      19 |  1364 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1365 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 |  1366 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1367 | `			/* Unexpected expression */` |
|      11 |  1368 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1369 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 |  1370 | `			if( rc != SXERR_ABORT ){` |
|      11 |  1371 | `				rc = SXERR_INVALID;` |
|       5 |  1372 | `			}` |
|       7 |  1373 | `		}` |
|      31 |  1374 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1375 | `		/* Unexpected expression */` |
|       3 |  1376 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1377 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1378 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1379 | `			rc = SXERR_INVALID;` |
|       1 |  1380 | `		}` |
|       1 |  1381 | `	}` |
|      38 |  1382 | `	return rc;` |
|       2 |  1383 |  |
|       - |  1384 | `/*` |
|       - |  1385 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1386 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1387 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1388 | ` */` |
|   27698 |  1389 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 |  1390 |  |
|       - |  1391 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1392 | `	SyToken *pKey,*pCur;` |
|   27700 |  1393 | `	sxi32 iEmitRef = 0;` |
|   27700 |  1394 | `	sxi32 iSpread = 0;` |
|   27700 |  1395 | `	sxi32 nPair = 0;` |
|       - |  1396 | `	sxi32 iNest;` |
|       - |  1397 | `	sxi32 rc;` |
|   27700 |  1398 | `	xValidator = 0;` |
|   22586 |  1399 | `	for(;;){` |
|       - |  1400 | `		/* Jump leading commas */` |
|   51106 |  1401 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5934 |  1402 | `			pGen->pIn++;` |
|       2 |  1403 | `		}` |
|   45174 |  1404 | `		pCur = pGen->pIn;` |
|   45174 |  1405 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1406 | `			/* No more entry to process */` |
|   27684 |  1407 | `			break;` |
|       - |  1408 | `		}` |
|   17492 |  1409 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1410 | `			continue;` |
|       - |  1411 | `		}` |
|       - |  1412 | `		/* Compile the key if available */` |
|   17492 |  1413 | `		pKey = pCur;` |
|   17492 |  1414 | `		iNest = 0;` |
|   48912 |  1415 | `		while( pCur < pGen->pIn ){` |
|   32886 |  1416 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1462 |  1417 | `				break;` |
|       - |  1418 | `			}` |
|       - |  1419 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1420 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1421 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1422 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1423 | `			 */` |
|   31426 |  1424 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      84 |  1425 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      84 |  1426 | `				SyToken *pFn = pCur;` |
|      82 |  1427 | `				if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pGen->pIn` |
|     ! 0 |  1428 | `					&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       2 |  1429 | `					&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1430 | `					pFn = &pCur[1];` |
|     ! 0 |  1431 | `					nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1432 | `				}` |
|      84 |  1433 | `				if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1434 | `					pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1435 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1436 | `						pCur++;` |
|     ! 0 |  1437 | `					}` |
|       5 |  1438 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1439 | `						pCur++;` |
|       5 |  1440 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1441 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1442 | `						if( pCur < pGen->pIn ){` |
|       5 |  1443 | `							pCur++;` |
|       2 |  1444 | `						}` |
|       2 |  1445 | `					}` |
|       5 |  1446 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1447 | `						pCur++;` |
|     ! 0 |  1448 | `						if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1449 | `							&& pCur->sData.nByte == 1` |
|     ! 0 |  1450 | `							&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1451 | `							pCur++;` |
|     ! 0 |  1452 | `						}` |
|     ! 0 |  1453 | `						if( pCur < pGen->pIn` |
|     ! 0 |  1454 | `							&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1455 | `							pCur++;` |
|     ! 0 |  1456 | `						}` |
|     ! 0 |  1457 | `					}` |
|       - |  1458 | `					/* The rest of the entry is the arrow function body — no` |
|       - |  1459 | `					 * outer key to extract. Stop the scan here. */` |
|       5 |  1460 | `					pCur = pGen->pIn;` |
|       5 |  1461 | `					break;` |
|       - |  1462 | `				}` |
|       - |  1463 | `				/* Match expression (PHP 8.0): 'match (subject) { ... }'.` |
|       - |  1464 | `				 * The '=>' inside match arms is not an array key/value separator —` |
|       - |  1465 | `				 * it introduces each arm's result expression. Skip past the full` |
|       - |  1466 | `				 * match span so the outer scan sees no false '=>'. */` |
|      80 |  1467 | `				if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1468 | `					pCur++; /* past 'match' */` |
|       3 |  1469 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1470 | `						pCur++;` |
|       3 |  1471 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1472 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1473 | `						if( pCur < pGen->pIn ){` |
|       3 |  1474 | `							pCur++;` |
|       1 |  1475 | `						}` |
|       1 |  1476 | `					}` |
|       3 |  1477 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1478 | `						pCur++;` |
|       3 |  1479 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1480 | `							PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1481 | `						if( pCur < pGen->pIn ){` |
|       3 |  1482 | `							pCur++;` |
|       1 |  1483 | `						}` |
|       1 |  1484 | `					}` |
|       3 |  1485 | `					continue;` |
|       - |  1486 | `				}` |
|      38 |  1487 | `			}` |
|   31420 |  1488 | `			if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     258 |  1489 | `				iNest++;` |
|   31292 |  1490 | `			}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1491 | `				/* Don't worry about mismatched brackets here,the expression` |
|       - |  1492 | `				 * parser will shortly detect any syntax error.` |
|       - |  1493 | `				 */` |
|     258 |  1494 | `				iNest--;` |
|     128 |  1495 | `			}` |
|   31420 |  1496 | `			pCur++;` |
|       2 |  1497 | `		}` |
|   17492 |  1498 | `		rc = SXERR_EMPTY;` |
|   17492 |  1499 | `		if( pCur < pGen->pIn ){` |
|    1462 |  1500 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1501 | `				/* Missing value */` |
|      11 |  1502 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 |  1503 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1504 | `					return SXERR_ABORT;` |
|       - |  1505 | `				}` |
|      11 |  1506 | `				return SXRET_OK;` |
|       - |  1507 | `			}` |
|       - |  1508 | `			/* Compile the expression holding the key */` |
|    1452 |  1509 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1510 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1452 |  1511 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1512 | `				return SXERR_ABORT;` |
|       - |  1513 | `			}` |
|    1452 |  1514 | `			pCur++; /* Jump the '=>' operator */` |
|   16757 |  1515 | `		}else if( pKey == pCur ){` |
|       - |  1516 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1517 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1518 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1519 | `		}else{` |
|       - |  1520 | `			/* Reset back the cursor and point to the entry value */` |
|   16032 |  1521 | `			pCur = pKey;` |
|       - |  1522 | `		}` |
|   17482 |  1523 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1524 | `			/* No available key,load NULL */` |
|   16034 |  1525 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    8016 |  1526 | `		}` |
|   17482 |  1527 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1528 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      42 |  1529 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      42 |  1530 | `			iEmitRef = 1;` |
|      42 |  1531 | `			pCur++; /* Jump the '&' token */` |
|      42 |  1532 | `			if( pCur >= pGen->pIn ){` |
|       - |  1533 | `				/* Missing value */` |
|       3 |  1534 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1535 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1536 | `					return SXERR_ABORT;` |
|       - |  1537 | `				}` |
|       3 |  1538 | `				return SXRET_OK;` |
|       - |  1539 | `			}` |
|      19 |  1540 | `		}` |
|       - |  1541 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1542 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1543 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1544 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1545 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   17480 |  1546 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   17480 |  1547 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1548 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1549 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1550 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1551 | `			 * output is engine-portable. */` |
|       5 |  1552 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1553 | `				"syntax error, unexpected token \"...\"");` |
|       5 |  1554 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1555 | `				return SXERR_ABORT;` |
|       - |  1556 | `			}` |
|       5 |  1557 | `			return SXRET_OK;` |
|       - |  1558 | `		}` |
|       - |  1559 | `		/* Compile indice value */` |
|   17476 |  1560 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   17476 |  1561 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1562 | `			return SXERR_ABORT;` |
|       - |  1563 | `		}` |
|   17476 |  1564 | `		if( iSpread ){` |
|       - |  1565 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      58 |  1566 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   17448 |  1567 | `		}else if( iEmitRef ){` |
|       - |  1568 | `			/* Emit the load reference instruction */` |
|      38 |  1569 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1570 | `		}` |
|   17476 |  1571 | `		xValidator = 0;` |
|   17476 |  1572 | `		iEmitRef = 0;` |
|   17476 |  1573 | `		iSpread = 0;` |
|   17476 |  1574 | `		nPair++;` |
|       2 |  1575 | `	}` |
|       - |  1576 | `	/* Emit the load map instruction */` |
|   27684 |  1577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1578 | `	/* Node successfully compiled */` |
|   27684 |  1579 | `	return SXRET_OK;` |
|   13851 |  1580 |  |
|       - |  1581 | `/*` |
|       - |  1582 | ` * Compile the 'array' language construct.` |
|       - |  1583 | ` *	 According to the PHP language reference manual` |
|       - |  1584 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1585 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1586 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1587 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1588 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1589 | ` */` |
|   26984 |  1590 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1591 |  |
|       - |  1592 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   26986 |  1593 | `	pGen->pIn += 2;` |
|   26986 |  1594 | `	pGen->pEnd--;` |
|   13492 |  1595 | `	SXUNUSED(iCompileFlag);` |
|   26986 |  1596 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1597 |  |
|       - |  1598 | `/*` |
|       - |  1599 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1600 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1601 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1602 | ` */` |
|     714 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1604 |  |
|       - |  1605 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     716 |  1606 | `	pGen->pIn++;` |
|     716 |  1607 | `	pGen->pEnd--;` |
|     357 |  1608 | `	SXUNUSED(iCompileFlag);` |
|     716 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1610 |  |
|       - |  1611 | `/*` |
|       - |  1612 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1613 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1614 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1615 | ` * error message.` |
|       - |  1616 | ` * See the routine responible of compiling the list language construct` |
|       - |  1617 | ` * for more inforation.` |
|       - |  1618 | ` */` |
|     128 |  1619 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1620 |  |
|     130 |  1621 | `	sxi32 rc = SXRET_OK;` |
|     130 |  1622 | `	if( pRoot->pOp ){` |
|     ! 0 |  1623 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 |  1624 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1625 | `				/* Unexpected expression */` |
|     ! 0 |  1626 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1627 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1628 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1629 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1630 | `				}` |
|     ! 0 |  1631 | `		}` |
|     130 |  1632 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1633 | `		/* Unexpected expression */` |
|       5 |  1634 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1635 | `			"list(): Expecting a variable not an expression");` |
|       5 |  1636 | `		if( rc != SXERR_ABORT ){` |
|       5 |  1637 | `			rc = SXERR_INVALID;` |
|       2 |  1638 | `		}` |
|       2 |  1639 | `	}` |
|     130 |  1640 | `	return rc;` |
|       2 |  1641 |  |
|       - |  1642 | `/*` |
|       - |  1643 | ` * Compile the 'list' language construct.` |
|       - |  1644 | ` *  According to the PHP language reference` |
|       - |  1645 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1646 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1647 | ` *  Description` |
|       - |  1648 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1649 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1650 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1651 | ` *  Parameters` |
|       - |  1652 | ` *   $varname: A variable.` |
|       - |  1653 | ` *  Return Values` |
|       - |  1654 | ` *   The assigned array.` |
|       - |  1655 | ` */` |
|       - |  1656 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1657 | `struct NestedListEntry {` |
|       - |  1658 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1659 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1660 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1661 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1662 | `};` |
|       - |  1663 | `/*` |
|       - |  1664 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1665 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1666 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1667 | ` */` |
|      74 |  1668 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 |  1669 |  |
|       - |  1670 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1671 | `	SyToken *pNext;` |
|       - |  1672 | `	sxi32 nExpr;` |
|       - |  1673 | `	sxi32 rc;` |
|      76 |  1674 | `	nExpr = 0;` |
|      76 |  1675 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 |  1676 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 |  1677 | `		if( pGen->pIn < pNext ){` |
|       - |  1678 | `			/* Check for nested list() */` |
|     144 |  1679 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1680 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1681 | `				/* Record this nested list for post-processing */` |
|       3 |  1682 | `				SyToken *pListEnd = 0;` |
|       3 |  1683 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1684 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1685 | `				}` |
|       3 |  1686 | `				if( pListEnd ){` |
|       - |  1687 | `					struct NestedListEntry sEntry;` |
|       3 |  1688 | `					sEntry.nIndex = nExpr;` |
|       3 |  1689 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1690 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1691 | `					sEntry.isShort = 0;` |
|       3 |  1692 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1693 | `				}` |
|       - |  1694 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1695 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 |  1696 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1697 | `				/* Nested short destructuring [...] */` |
|      13 |  1698 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1699 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1700 | `				if( pBracketEnd ){` |
|       - |  1701 | `					struct NestedListEntry sEntry;` |
|      13 |  1702 | `					sEntry.nIndex = nExpr;` |
|      13 |  1703 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1704 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1705 | `					sEntry.isShort = 1;` |
|      13 |  1706 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1707 | `				}` |
|       - |  1708 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1709 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1710 | `			}else{` |
|       - |  1711 | `				/* Compile the expression holding the variable */` |
|     130 |  1712 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 |  1713 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1714 | `					SySetRelease(&sNested);` |
|     ! 0 |  1715 | `					return SXRET_OK;` |
|       - |  1716 | `				}` |
|       - |  1717 | `			}` |
|      73 |  1718 | `		}else{` |
|       - |  1719 | `			/* Empty entry,load NULL */` |
|      13 |  1720 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1721 | `		}` |
|     156 |  1722 | `		nExpr++;` |
|       - |  1723 | `		/* Advance the stream cursor */` |
|     156 |  1724 | `		pGen->pIn = &pNext[1];` |
|       2 |  1725 | `	}` |
|       - |  1726 | `	/* Emit the LOAD_LIST instruction */` |
|      76 |  1727 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1728 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1729 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1730 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1731 | `	 */` |
|      76 |  1732 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1733 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1734 | `		sxu32 i;` |
|      27 |  1735 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1736 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1737 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1738 | `			ph7_value *pIdx;` |
|       - |  1739 | `			sxu32 nConstIdx;` |
|       - |  1740 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1741 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1742 | `			/* Push the integer index for this nested entry */` |
|      15 |  1743 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1744 | `			if( pIdx == 0 ){` |
|     ! 0 |  1745 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1746 | `				SySetRelease(&sNested);` |
|     ! 0 |  1747 | `				return SXERR_ABORT;` |
|       - |  1748 | `			}` |
|      15 |  1749 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1750 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1751 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1752 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1753 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1754 | `			 */` |
|      15 |  1755 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1756 | `			/* Recursively compile the inner list */` |
|      15 |  1757 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1758 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1759 | `			if( apNested[i].isShort ){` |
|      13 |  1760 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1761 | `			}else{` |
|       3 |  1762 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1763 | `			}` |
|      15 |  1764 | `			pGen->pIn = pSavedIn;` |
|      15 |  1765 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1766 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1767 | `				SySetRelease(&sNested);` |
|     ! 0 |  1768 | `				return SXERR_ABORT;` |
|       - |  1769 | `			}` |
|       - |  1770 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1771 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1772 | `		}` |
|       6 |  1773 | `	}` |
|      76 |  1774 | `	SySetRelease(&sNested);` |
|       - |  1775 | `	/* Node successfully compiled */` |
|      76 |  1776 | `	return SXRET_OK;` |
|      39 |  1777 |  |
|      32 |  1778 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1779 |  |
|       - |  1780 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 |  1781 | `	pGen->pIn += 2;` |
|      34 |  1782 | `	pGen->pEnd--;` |
|      16 |  1783 | `	SXUNUSED(iCompileFlag);` |
|      34 |  1784 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1785 |  |
|      42 |  1786 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1787 |  |
|       - |  1788 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 |  1789 | `	pGen->pIn++;` |
|      44 |  1790 | `	pGen->pEnd--;` |
|      21 |  1791 | `	SXUNUSED(iCompileFlag);` |
|      44 |  1792 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1793 |  |
|       - |  1794 | `/* Forward declarations */` |
|       - |  1795 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1796 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1797 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1798 | `/*` |
|       - |  1799 | ` * Compile an annoynmous function or a closure.` |
|       - |  1800 | ` * According to the PHP language reference` |
|       - |  1801 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1802 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1803 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1804 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1805 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1806 | ` *  Example Anonymous function variable assignment example` |
|       - |  1807 | ` * <?php` |
|       - |  1808 | ` * $greet = function($name)` |
|       - |  1809 | ` * {` |
|       - |  1810 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1811 | ` * };` |
|       - |  1812 | ` * $greet('World');` |
|       - |  1813 | ` * $greet('PHP');` |
|       - |  1814 | ` * ?>` |
|       - |  1815 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1816 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1817 | ` */` |
|     230 |  1818 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1819 |  |
|       - |  1820 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1821 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1822 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1823 | `							  * one thread is allowed to compile the script.` |
|       - |  1824 | `						      */` |
|       - |  1825 | `	ph7_value *pObj;` |
|       - |  1826 | `	SyString sName;` |
|       - |  1827 | `	sxu32 nIdx;` |
|       - |  1828 | `	sxu32 nLen;` |
|       - |  1829 | `	sxi32 rc;` |
|       - |  1830 |  |
|     232 |  1831 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     232 |  1832 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1833 | `		pGen->pIn++;` |
|     ! 0 |  1834 | `	}` |
|       - |  1835 | `	/* Reserve a constant for the lambda */` |
|     232 |  1836 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     232 |  1837 | `	if( pObj == 0 ){` |
|     ! 0 |  1838 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1839 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1840 | `		return SXERR_ABORT;` |
|       - |  1841 | `	}` |
|       - |  1842 | `	/* Generate a unique name */` |
|     232 |  1843 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1844 | `	/* Make sure the generated name is unique */` |
|     232 |  1845 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1846 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1847 | `	}` |
|     232 |  1848 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     232 |  1849 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1850 | `	/* Compile the lambda body */` |
|     232 |  1851 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     232 |  1852 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1853 | `		return SXERR_ABORT;` |
|       - |  1854 | `	}` |
|     232 |  1855 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1856 | `		/* Emit the load closure instruction */` |
|      18 |  1857 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|      10 |  1858 | `	}else{` |
|       - |  1859 | `		/* Emit the load constant instruction */` |
|     216 |  1860 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1861 | `	}` |
|       - |  1862 | `	/* Node successfully compiled */` |
|     232 |  1863 | `	return SXRET_OK;` |
|     117 |  1864 |  |
|       - |  1865 | `/*` |
|       - |  1866 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1867 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1868 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1869 | ` */` |
|     150 |  1870 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1871 | `	ph7_gen_state *pGen,` |
|       - |  1872 | `	ph7_vm_func *pFunc,` |
|       - |  1873 | `	const char *zName,` |
|       - |  1874 | `	sxu32 nByte,` |
|       - |  1875 | `	SyString *aShadow,` |
|       - |  1876 | `	sxu32 nShadow)` |
|       2 |  1877 |  |
|       - |  1878 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  1879 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  1880 | `	sxu32 n, nEnv;` |
|       - |  1881 | `	char *zDup;` |
|     152 |  1882 | `	if( nByte == 0 ){` |
|     ! 0 |  1883 | `		return SXRET_OK;` |
|       - |  1884 | `	}` |
|     150 |  1885 | `	if( nByte == sizeof("this")-1` |
|      81 |  1886 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1887 | `		return SXRET_OK;` |
|       - |  1888 | `	}` |
|     182 |  1889 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     128 |  1890 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     125 |  1891 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      98 |  1892 | `			return SXRET_OK;` |
|       - |  1893 | `		}` |
|      17 |  1894 | `	}` |
|      53 |  1895 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  1896 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  1897 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  1898 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  1899 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  1900 | `			return SXRET_OK;` |
|       - |  1901 | `		}` |
|      15 |  1902 | `	}` |
|      53 |  1903 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  1904 | `	if( zDup == 0 ){` |
|     ! 0 |  1905 | `		return SXERR_ABORT;` |
|       - |  1906 | `	}` |
|      53 |  1907 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  1908 | `	sEnv.iFlags = 0;` |
|      53 |  1909 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  1910 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  1911 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  1912 | `	return SXRET_OK;` |
|      77 |  1913 |  |
|       - |  1914 | `/*` |
|       - |  1915 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  1916 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  1917 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  1918 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  1919 | ` */` |
|      14 |  1920 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  1921 | `	ph7_gen_state *pGen,` |
|       - |  1922 | `	ph7_vm_func *pFunc,` |
|       - |  1923 | `	const char *zIn,` |
|       - |  1924 | `	const char *zEnd,` |
|       - |  1925 | `	SyString *aShadow,` |
|       - |  1926 | `	sxu32 nShadow)` |
|       1 |  1927 |  |
|       - |  1928 | `	sxi32 rc;` |
|     159 |  1929 | `	while( zIn < zEnd ){` |
|     145 |  1930 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  1931 | `			zIn++;` |
|     ! 0 |  1932 | `			if( zIn < zEnd ){` |
|     ! 0 |  1933 | `				zIn++;` |
|     ! 0 |  1934 | `			}` |
|     ! 0 |  1935 | `			continue;` |
|       - |  1936 | `		}` |
|     144 |  1937 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  1938 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  1939 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  1940 | `			const char *zName;` |
|      13 |  1941 | `			zIn++; /* skip '$' */` |
|      13 |  1942 | `			zName = zIn;` |
|      39 |  1943 | `			while( zIn < zEnd ){` |
|      35 |  1944 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  1945 | `				if( c >= 0xc0 ){` |
|     ! 0 |  1946 | `					zIn++;` |
|     ! 0 |  1947 | `					while( zIn < zEnd` |
|     ! 0 |  1948 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1949 | `						zIn++;` |
|     ! 0 |  1950 | `					}` |
|     ! 0 |  1951 | `					continue;` |
|       - |  1952 | `				}` |
|      35 |  1953 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  1954 | `					break;` |
|       - |  1955 | `				}` |
|      27 |  1956 | `				zIn++;` |
|       1 |  1957 | `			}` |
|      13 |  1958 | `			if( zIn > zName ){` |
|      19 |  1959 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  1960 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  1961 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1962 | `					return SXERR_ABORT;` |
|       - |  1963 | `				}` |
|       6 |  1964 | `			}` |
|      13 |  1965 | `			continue;` |
|       - |  1966 | `		}` |
|     133 |  1967 | `		zIn++;` |
|       1 |  1968 | `	}` |
|      15 |  1969 | `	return SXRET_OK;` |
|       8 |  1970 |  |
|       - |  1971 | `/*` |
|       - |  1972 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  1973 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  1974 | ` *   - plain $<id> pairs` |
|       - |  1975 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  1976 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  1977 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  1978 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  1979 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  1980 | ` *     are never mistakenly captured.` |
|       - |  1981 | ` */` |
|     136 |  1982 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1983 | `	ph7_gen_state *pGen,` |
|       - |  1984 | `	ph7_vm_func *pFunc,` |
|       - |  1985 | `	SyToken *pStart,` |
|       - |  1986 | `	SyToken *pEnd,` |
|       - |  1987 | `	SyString *aShadow,` |
|       - |  1988 | `	sxu32 nShadow)` |
|       2 |  1989 |  |
|     138 |  1990 | `	SyToken *pScan = pStart;` |
|       - |  1991 | `	sxi32 rc;` |
|     512 |  1992 | `	while( pScan < pEnd ){` |
|     376 |  1993 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  1994 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  1995 | `				pScan->sData.zString,` |
|      14 |  1996 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  1997 | `				aShadow,nShadow);` |
|      15 |  1998 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1999 | `				return SXERR_ABORT;` |
|       - |  2000 | `			}` |
|      15 |  2001 | `			pScan++;` |
|      15 |  2002 | `			continue;` |
|       - |  2003 | `		}` |
|     362 |  2004 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2005 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2006 | `			SyToken *pFnKw = pScan;` |
|      20 |  2007 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2008 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2009 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2010 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2011 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2012 | `			}` |
|      21 |  2013 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2014 | `				SyToken *pInnerSigStart;` |
|       - |  2015 | `				SyToken *pInnerSigEnd;` |
|       - |  2016 | `				SyToken *pInnerBodyEnd;` |
|       - |  2017 | `				SyString *aInnerShadow;` |
|       - |  2018 | `				sxu32 nInnerShadow;` |
|       - |  2019 | `				sxu32 nInnerParamMax;` |
|       - |  2020 | `				SyToken *p;` |
|       - |  2021 | `				int iNestInner;` |
|      19 |  2022 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2023 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2024 | `					pScan++;` |
|     ! 0 |  2025 | `				}` |
|      19 |  2026 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2027 | `					pScan++;` |
|     ! 0 |  2028 | `					continue;` |
|       - |  2029 | `				}` |
|      19 |  2030 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2031 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2032 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2033 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2034 | `					pScan = pEnd;` |
|     ! 0 |  2035 | `					continue;` |
|       - |  2036 | `				}` |
|       - |  2037 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2038 | `				nInnerParamMax = 0;` |
|      57 |  2039 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2040 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2041 | `						nInnerParamMax++;` |
|       6 |  2042 | `					}` |
|      20 |  2043 | `				}` |
|      19 |  2044 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2045 | `					&pGen->pVm->sAllocator,` |
|      18 |  2046 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2047 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2048 | `					return SXERR_ABORT;` |
|       - |  2049 | `				}` |
|      19 |  2050 | `				nInnerShadow = 0;` |
|      25 |  2051 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2052 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2053 | `				}` |
|      57 |  2054 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2055 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2056 | `						continue;` |
|       - |  2057 | `					}` |
|      13 |  2058 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2059 | `						break;` |
|       - |  2060 | `					}` |
|      13 |  2061 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2062 | `						continue;` |
|       - |  2063 | `					}` |
|      13 |  2064 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2065 | `				}` |
|      19 |  2066 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2067 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2068 | `					pScan++;` |
|     ! 0 |  2069 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2070 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2071 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2072 | `						pScan++;` |
|     ! 0 |  2073 | `					}` |
|     ! 0 |  2074 | `					if( pScan < pEnd` |
|     ! 0 |  2075 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2076 | `						pScan++;` |
|     ! 0 |  2077 | `					}` |
|     ! 0 |  2078 | `				}` |
|      19 |  2079 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2080 | `					pScan++; /* past '=>' */` |
|       9 |  2081 | `				}` |
|      19 |  2082 | `				pInnerBodyEnd = pScan;` |
|      19 |  2083 | `				iNestInner = 0;` |
|     131 |  2084 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2085 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2086 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2087 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2088 | `						break;` |
|       - |  2089 | `					}` |
|     113 |  2090 | `					if( pInnerBodyEnd->nType &` |
|       - |  2091 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2092 | `						iNestInner++;` |
|     112 |  2093 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2094 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2095 | `						iNestInner--;` |
|       1 |  2096 | `					}` |
|     113 |  2097 | `					pInnerBodyEnd++;` |
|       1 |  2098 | `				}` |
|       - |  2099 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2100 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2101 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2102 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2103 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2104 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2105 | `				 *` |
|       - |  2106 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2107 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2108 | `				 * range after the '=' sign. */` |
|       - |  2109 | `				{` |
|      19 |  2110 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2111 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2112 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2113 | `						SyToken *pEq = 0;` |
|      13 |  2114 | `						int iNestArg = 0;` |
|      49 |  2115 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2116 | `							if( iNestArg == 0` |
|      39 |  2117 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2118 | `								break;` |
|       - |  2119 | `							}` |
|      37 |  2120 | `							if( pArgEnd->nType &` |
|       - |  2121 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2122 | `								iNestArg++;` |
|      37 |  2123 | `							}else if( pArgEnd->nType &` |
|       - |  2124 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2125 | `								iNestArg--;` |
|     ! 0 |  2126 | `							}` |
|      36 |  2127 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2128 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2129 | `								pEq = pArgEnd;` |
|       3 |  2130 | `							}` |
|      37 |  2131 | `							pArgEnd++;` |
|       1 |  2132 | `						}` |
|      13 |  2133 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2134 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2135 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2136 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2137 | `								return SXERR_ABORT;` |
|       - |  2138 | `							}` |
|       3 |  2139 | `						}` |
|      13 |  2140 | `						pArgStart = pArgEnd;` |
|      12 |  2141 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2142 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2143 | `							pArgStart++;` |
|       1 |  2144 | `						}` |
|       1 |  2145 | `					}` |
|       - |  2146 | `				}` |
|      28 |  2147 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2148 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2149 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2150 | `					return SXERR_ABORT;` |
|       - |  2151 | `				}` |
|      19 |  2152 | `				pScan = pInnerBodyEnd;` |
|      19 |  2153 | `				continue;` |
|       - |  2154 | `			}` |
|       1 |  2155 | `		}` |
|     344 |  2156 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     206 |  2157 | `			pScan++;` |
|     206 |  2158 | `			continue;` |
|       - |  2159 | `		}` |
|       - |  2160 | `		{` |
|       - |  2161 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     140 |  2162 | `			SyToken *pDollar = pScan;` |
|     207 |  2163 | `			while( &pDollar[1] < pEnd` |
|     140 |  2164 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2165 | `				pDollar++;` |
|     ! 0 |  2166 | `			}` |
|     140 |  2167 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2168 | `				break;` |
|       - |  2169 | `			}` |
|     140 |  2170 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2171 | `				pScan = pDollar + 1;` |
|     ! 0 |  2172 | `				continue;` |
|       - |  2173 | `			}` |
|     209 |  2174 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     138 |  2175 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      69 |  2176 | `				aShadow,nShadow);` |
|     140 |  2177 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2178 | `				return SXERR_ABORT;` |
|       - |  2179 | `			}` |
|     140 |  2180 | `			pScan = pDollar + 2;` |
|       - |  2181 | `		}` |
|       2 |  2182 | `	}` |
|     138 |  2183 | `	return SXRET_OK;` |
|      70 |  2184 |  |
|       - |  2185 | `/*` |
|       - |  2186 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2187 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2188 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2189 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2190 | ` * $this is also made available.` |
|       - |  2191 | ` */` |
|     118 |  2192 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2193 |  |
|       - |  2194 | `	ph7_vm_func *pFunc;` |
|       - |  2195 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2196 | `	GenBlock *pBlock;` |
|       - |  2197 | `	SySet *pInstrContainer;` |
|       - |  2198 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2199 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2200 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2201 | `	SyToken *pSavedEnd;` |
|       - |  2202 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2203 | `	char zName[512];` |
|       - |  2204 | `	static int iCnt = 1;` |
|       - |  2205 | `	char *zDup;` |
|       - |  2206 | `	sxu32 nLen;` |
|       - |  2207 | `	sxu32 nLine;` |
|     120 |  2208 | `	sxi32 iFlags = 0;` |
|     120 |  2209 | `	int bStatic = 0;` |
|       - |  2210 | `	sxi32 rc;` |
|       - |  2211 | `	sxu32 n;` |
|      59 |  2212 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2213 |  |
|     120 |  2214 | `	nLine = pGen->pIn->nLine;` |
|       - |  2215 | `	/* Optional 'static' prefix */` |
|     118 |  2216 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     120 |  2217 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2218 | `		bStatic = 1;` |
|       3 |  2219 | `		pGen->pIn++;` |
|       1 |  2220 | `	}` |
|       - |  2221 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     118 |  2222 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     120 |  2223 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2224 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2225 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2226 | `		return SXERR_SYNTAX;` |
|       - |  2227 | `	}` |
|     120 |  2228 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2229 | `	/* Optional '&' — return by reference */` |
|     120 |  2230 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2231 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2232 | `		pGen->pIn++;` |
|     ! 0 |  2233 | `	}` |
|       - |  2234 | `	/* Expect '(' */` |
|     120 |  2235 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2236 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2237 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2238 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2239 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2240 | `		}else{` |
|     ! 0 |  2241 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2242 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2243 | `		}` |
|       3 |  2244 | `		return SXERR_SYNTAX;` |
|       - |  2245 | `	}` |
|     118 |  2246 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2247 | `	/* Delimit the parameter list */` |
|     118 |  2248 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     118 |  2249 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2250 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2251 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2252 | `		return SXERR_SYNTAX;` |
|       - |  2253 | `	}` |
|       - |  2254 | `	/* Allocate the function state */` |
|     116 |  2255 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     116 |  2256 | `	if( pFunc == 0 ){` |
|     ! 0 |  2257 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2258 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2259 | `		return SXERR_ABORT;` |
|       - |  2260 | `	}` |
|       - |  2261 | `	/* Generate a unique lambda name */` |
|     116 |  2262 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     206 |  2263 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      92 |  2264 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2265 | `	}` |
|     116 |  2266 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     116 |  2267 | `	if( zDup == 0 ){` |
|     ! 0 |  2268 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2269 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2270 | `		return SXERR_ABORT;` |
|       - |  2271 | `	}` |
|     116 |  2272 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2273 | `	/* Collect function arguments */` |
|     116 |  2274 | `	if( pGen->pIn < pSigEnd ){` |
|      86 |  2275 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      86 |  2276 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2277 | `			return SXERR_ABORT;` |
|       - |  2278 | `		}` |
|      42 |  2279 | `	}` |
|       - |  2280 | `	/* Point past ')' and parse optional return type */` |
|     116 |  2281 | `	pGen->pIn = &pSigEnd[1];` |
|     116 |  2282 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     116 |  2283 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2284 | `		return SXERR_ABORT;` |
|     116 |  2285 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2286 | `		return SXERR_SYNTAX;` |
|       - |  2287 | `	}` |
|       - |  2288 | `	/* Expect '=>' */` |
|     116 |  2289 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2290 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2291 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2292 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2293 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2294 | `		}else{` |
|     ! 0 |  2295 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2296 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2297 | `		}` |
|       3 |  2298 | `		return SXERR_SYNTAX;` |
|       - |  2299 | `	}` |
|     114 |  2300 | `	pGen->pIn++; /* Jump '=>' */` |
|     114 |  2301 | `	pBodyStart = pGen->pIn;` |
|     114 |  2302 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2303 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2304 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2305 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2306 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     114 |  2307 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2308 | `	{` |
|     114 |  2309 | `		SyString *aShadow = 0;` |
|     114 |  2310 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     114 |  2311 | `		if( nShadow > 0 ){` |
|      84 |  2312 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      82 |  2313 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      84 |  2314 | `			if( aShadow == 0 ){` |
|     ! 0 |  2315 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2316 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2317 | `				return SXERR_ABORT;` |
|       - |  2318 | `			}` |
|     184 |  2319 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     102 |  2320 | `				aShadow[n] = aArgs[n].sName;` |
|      52 |  2321 | `			}` |
|      41 |  2322 | `		}` |
|     170 |  2323 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      56 |  2324 | `			aShadow,nShadow);` |
|     114 |  2325 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2326 | `			return SXERR_ABORT;` |
|       - |  2327 | `		}` |
|       - |  2328 | `	}` |
|       - |  2329 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2330 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2331 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2332 | `	 * $this. */` |
|     114 |  2333 | `	if( !bStatic ){` |
|       - |  2334 | `		char *zThisDup;` |
|     112 |  2335 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     112 |  2336 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2337 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2338 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2339 | `			return SXERR_ABORT;` |
|       - |  2340 | `		}` |
|     112 |  2341 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     112 |  2342 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     112 |  2343 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     112 |  2344 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     112 |  2345 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      55 |  2346 | `	}` |
|       - |  2347 | `	/* Arrow functions are always closures */` |
|     114 |  2348 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2349 | `	/* Compile the body expression as an implicit return */` |
|     170 |  2350 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      56 |  2351 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     114 |  2352 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2353 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2354 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2355 | `		return SXERR_ABORT;` |
|       - |  2356 | `	}` |
|     114 |  2357 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     114 |  2358 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     114 |  2359 | `	pSavedEnd = pGen->pEnd;` |
|     114 |  2360 | `	pGen->pIn = pBodyStart;` |
|     114 |  2361 | `	pGen->pEnd = pBodyEnd;` |
|     114 |  2362 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     114 |  2363 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2364 | `		return SXERR_ABORT;` |
|       - |  2365 | `	}` |
|       - |  2366 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2367 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2368 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2369 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     114 |  2370 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     114 |  2371 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     114 |  2372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     114 |  2373 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     114 |  2374 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2375 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     114 |  2376 | `	pGen->pIn = pBodyEnd;` |
|     114 |  2377 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2378 | `	/* Emit the load-closure instruction */` |
|     114 |  2379 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     114 |  2380 | `	return SXRET_OK;` |
|      61 |  2381 |  |
|       - |  2382 | `/*` |
|       - |  2383 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2384 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2385 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2386 | ` * expression's value.` |
|       - |  2387 | ` */` |
|     346 |  2388 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2389 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       2 |  2390 |  |
|       - |  2391 | `	SySet *pInstrContainer;` |
|       - |  2392 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2393 | `	GenBlock *pArmBlock;` |
|       - |  2394 | `	sxi32 rc;` |
|     348 |  2395 | `	pTmpIn  = pGen->pIn;` |
|     348 |  2396 | `	pTmpEnd = pGen->pEnd;` |
|     348 |  2397 | `	pGen->pIn  = pStart;` |
|     348 |  2398 | `	pGen->pEnd = pStop;` |
|     348 |  2399 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     348 |  2400 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2401 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2402 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2403 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2404 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2405 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     521 |  2406 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2407 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     348 |  2408 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2409 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2410 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2411 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2412 | `		return SXERR_ABORT;` |
|       - |  2413 | `	}` |
|     348 |  2414 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     348 |  2415 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     348 |  2416 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     348 |  2417 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     348 |  2418 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     348 |  2419 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     348 |  2420 | `	pGen->pIn  = pTmpIn;` |
|     348 |  2421 | `	pGen->pEnd = pTmpEnd;` |
|     348 |  2422 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2423 | `		return SXERR_ABORT;` |
|       - |  2424 | `	}` |
|     348 |  2425 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2426 | `		return SXERR_EMPTY;` |
|       - |  2427 | `	}` |
|     348 |  2428 | `	return SXRET_OK;` |
|     175 |  2429 |  |
|       - |  2430 | `/*` |
|       - |  2431 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2432 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2433 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2434 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2435 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2436 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2437 | ` */` |
|       - |  2438 | `/*` |
|       - |  2439 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2440 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2441 | ` * caller can bail out of the current expression.` |
|       - |  2442 | ` */` |
|       2 |  2443 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2444 |  |
|       - |  2445 | `	va_list ap;` |
|       - |  2446 | `	sxi32 rc;` |
|       - |  2447 | `	SyBlob sMsg;` |
|       3 |  2448 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2449 | `	va_start(ap,zFmt);` |
|       3 |  2450 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2451 | `	va_end(ap);` |
|       3 |  2452 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2453 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2454 | `	SyBlobRelease(&sMsg);` |
|       3 |  2455 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2456 | `		return SXERR_ABORT;` |
|       - |  2457 | `	}` |
|       3 |  2458 | `	return SXERR_SYNTAX;` |
|       2 |  2459 |  |
|       - |  2460 | `/*` |
|       - |  2461 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2462 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2463 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2464 | ` */` |
|     348 |  2465 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       2 |  2466 |  |
|     350 |  2467 | `	SyToken *pCur = pStart;` |
|     350 |  2468 | `	int iNest = 0;` |
|     812 |  2469 | `	while( pCur < pEnd ){` |
|     778 |  2470 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2471 | `			iNest++;` |
|     772 |  2472 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2473 | `			iNest--;` |
|     760 |  2474 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     316 |  2475 | `			return pCur;` |
|       - |  2476 | `		}` |
|     464 |  2477 | `		pCur++;` |
|       2 |  2478 | `	}` |
|      36 |  2479 | `	return pEnd;` |
|     176 |  2480 |  |
|      70 |  2481 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2482 |  |
|       - |  2483 | `	ph7_match *pMatch;` |
|       - |  2484 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      72 |  2485 | `	int bHasDefault = 0;` |
|       - |  2486 | `	sxu32 nLine;` |
|       - |  2487 | `	sxi32 rc;` |
|      35 |  2488 | `	SXUNUSED(iCompileFlag);` |
|      72 |  2489 | `	nLine = pGen->pIn->nLine;` |
|      72 |  2490 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2491 | `	/* Expect '(' */` |
|      72 |  2492 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2493 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2494 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2495 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2496 | `	}` |
|      72 |  2497 | `	pGen->pIn++; /* Jump '(' */` |
|      72 |  2498 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      72 |  2499 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2500 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2501 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2502 | `	}` |
|      72 |  2503 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2504 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2505 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2506 | `	}` |
|       - |  2507 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      72 |  2508 | `	pSavedEnd = pGen->pEnd;` |
|      72 |  2509 | `	pGen->pEnd = pSubjEnd;` |
|      72 |  2510 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      72 |  2511 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2512 | `		return SXERR_ABORT;` |
|       - |  2513 | `	}` |
|      72 |  2514 | `	pGen->pEnd = pSavedEnd;` |
|      72 |  2515 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2516 | `	/* Expect '{' */` |
|      72 |  2517 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2518 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2519 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2520 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2521 | `	}` |
|      72 |  2522 | `	pGen->pIn++; /* Jump '{' */` |
|      72 |  2523 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      72 |  2524 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2525 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2526 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2527 | `	}` |
|       - |  2528 | `	/* Allocate ph7_match container */` |
|      72 |  2529 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      72 |  2530 | `	if( pMatch == 0 ){` |
|     ! 0 |  2531 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2532 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2533 | `		return SXERR_ABORT;` |
|       - |  2534 | `	}` |
|      72 |  2535 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      72 |  2536 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2537 | `	/* Iterate arms */` |
|     250 |  2538 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2539 | `		ph7_match_arm sArm;` |
|       - |  2540 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     184 |  2541 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     184 |  2542 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     184 |  2543 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     184 |  2544 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2545 | `		/* 'default' arm? */` |
|     182 |  2546 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     103 |  2547 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2548 | `			if( bHasDefault ){` |
|       3 |  2549 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2550 | `					"Match expressions may only contain one default arm");` |
|       4 |  2551 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2552 | `			}` |
|      20 |  2553 | `			sArm.bDefault = 1;` |
|      20 |  2554 | `			bHasDefault = 1;` |
|      20 |  2555 | `			pGen->pIn++;` |
|      20 |  2556 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2557 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2558 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2559 | `			}` |
|      20 |  2560 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2561 | `		}else{` |
|       - |  2562 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     164 |  2563 | `			pCondStart = pGen->pIn;` |
|     164 |  2564 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2565 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     172 |  2566 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2567 | `				SySet sCondBc;` |
|       9 |  2568 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2569 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2570 | `						"syntax error, empty match condition expression");` |
|       - |  2571 | `				}` |
|       9 |  2572 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2573 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2574 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2575 | `					return SXERR_ABORT;` |
|       - |  2576 | `				}` |
|       9 |  2577 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2578 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2579 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2580 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2581 | `			}` |
|     164 |  2582 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2583 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2584 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2585 | `			}` |
|     162 |  2586 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2587 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2588 | `					"syntax error, empty match condition expression");` |
|       - |  2589 | `			}` |
|       - |  2590 | `			{` |
|       - |  2591 | `				SySet sCondBc;` |
|     162 |  2592 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     162 |  2593 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     162 |  2594 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2595 | `					return SXERR_ABORT;` |
|       - |  2596 | `				}` |
|     162 |  2597 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2598 | `			}` |
|     162 |  2599 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2600 | `		}` |
|       - |  2601 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     180 |  2602 | `		pResStart = pGen->pIn;` |
|     180 |  2603 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     180 |  2604 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2605 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2606 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2607 | `		}` |
|     180 |  2608 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     180 |  2609 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2610 | `			return SXERR_ABORT;` |
|       - |  2611 | `		}` |
|     180 |  2612 | `		pGen->pIn = pResEnd;` |
|     180 |  2613 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     148 |  2614 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2615 | `		}` |
|     180 |  2616 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       2 |  2617 | `	}` |
|      68 |  2618 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      68 |  2619 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      68 |  2620 | `	return SXRET_OK;` |
|      37 |  2621 |  |
|       - |  2622 | `/*` |
|       - |  2623 | ` * Compile a backtick quoted string.` |
|       - |  2624 | ` */` |
|       4 |  2625 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  2626 |  |
|       - |  2627 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2628 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2629 | `	 */` |
|       7 |  2630 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2631 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2632 | `		ph7_lib_version()` |
|       - |  2633 | `		);` |
|       - |  2634 | `	/* Load NULL */` |
|       5 |  2635 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2636 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2637 | `	/* Node successfully compiled */` |
|       5 |  2638 | `	return SXRET_OK;` |
|       1 |  2639 |  |
|       - |  2640 | `/*` |
|       - |  2641 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2642 | ` * construct.` |
|       - |  2643 | ` */` |
|      80 |  2644 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2645 |  |
|       - |  2646 | `	SyString *pName;` |
|       - |  2647 | `	sxu32 nKeyID;` |
|       - |  2648 | `	sxi32 rc;` |
|       - |  2649 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      82 |  2650 | `	pName = &pGen->pIn->sData;` |
|      82 |  2651 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      82 |  2652 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      82 |  2653 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2654 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2655 | `		/* Compile arguments one after one */` |
|       9 |  2656 | `		pTmp = pGen->pEnd;` |
|       - |  2657 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2658 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2659 | `		 *  mean that the following expression is valid:` |
|       - |  2660 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2661 | `		 */` |
|       9 |  2662 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2663 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2664 | `			if( pGen->pIn < pNext ){` |
|       9 |  2665 | `				pGen->pEnd = pNext;` |
|       9 |  2666 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2667 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2668 | `					return SXERR_ABORT;` |
|       - |  2669 | `				}` |
|       9 |  2670 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2671 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2672 | `					 * without the overhead of a function call.` |
|       - |  2673 | `					 * This is a very powerful optimization that improve` |
|       - |  2674 | `					 * performance greatly.` |
|       - |  2675 | `					 */` |
|       9 |  2676 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2677 | `				}` |
|       4 |  2678 | `			}` |
|       - |  2679 | `			/* Jump trailing commas */` |
|       9 |  2680 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2681 | `				pNext++;` |
|     ! 0 |  2682 | `			}` |
|       9 |  2683 | `			pGen->pIn = pNext;` |
|       1 |  2684 | `		}` |
|       - |  2685 | `		/* Restore token stream */` |
|       9 |  2686 | `		pGen->pEnd = pTmp;` |
|       5 |  2687 | `	}else{` |
|      74 |  2688 | `		sxi32 nArg = 0;` |
|      74 |  2689 | `		sxu32 nIdx = 0;` |
|      74 |  2690 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      74 |  2691 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2692 | `			return SXERR_ABORT;` |
|      74 |  2693 | `		}else if(rc != SXERR_EMPTY ){` |
|      74 |  2694 | `			nArg = 1;` |
|      36 |  2695 | `		}` |
|      74 |  2696 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2697 | `			ph7_value *pObj;` |
|       - |  2698 | `			/* Emit the call instruction */` |
|      26 |  2699 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      26 |  2700 | `			if( pObj == 0 ){` |
|     ! 0 |  2701 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2702 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2703 | `				return SXERR_ABORT;` |
|       - |  2704 | `			}` |
|      26 |  2705 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2706 | `			/* Install in the literal table */` |
|      26 |  2707 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2708 | `		}` |
|       - |  2709 | `		/* Emit the call instruction */` |
|      74 |  2710 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      74 |  2711 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2712 | `	}` |
|       - |  2713 | `	/* Node successfully compiled */` |
|      82 |  2714 | `	return SXRET_OK;` |
|      42 |  2715 |  |
|       - |  2716 | `/*` |
|       - |  2717 | ` * Compile a node holding a variable declaration.` |
|       - |  2718 | ` * According to the PHP language reference` |
|       - |  2719 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2720 | ` *  The variable name is case-sensitive.` |
|       - |  2721 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2722 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2723 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2724 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2725 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2726 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2727 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2728 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2729 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2730 | ` *  the chapter on Expressions.` |
|       - |  2731 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2732 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2733 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2734 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2735 | ` *  is being assigned (the source variable).` |
|       - |  2736 | ` */` |
|  986844 |  2737 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2738 |  |
|  986846 |  2739 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2740 | `	sxi32 iVv;` |
|       - |  2741 | `	sxi32 iP1;` |
|       - |  2742 | `	void *p3;` |
|       - |  2743 | `	sxi32 rc;` |
|  986846 |  2744 | `	iVv = -1; /* Variable variable counter */` |
| 1973702 |  2745 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  986858 |  2746 | `		pGen->pIn++;` |
|  986858 |  2747 | `		iVv++;` |
|       2 |  2748 | `	}` |
|  986846 |  2749 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2750 | `		/* Invalid variable name */` |
|     ! 0 |  2751 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2752 | `		if( rc == SXERR_ABORT ){` |
|       - |  2753 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2754 | `			return SXERR_ABORT;` |
|       - |  2755 | `		}` |
|     ! 0 |  2756 | `		return SXRET_OK;` |
|       - |  2757 | `	}` |
|  986846 |  2758 | `	p3  = 0;` |
|  986846 |  2759 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2760 | `		/* Dynamic variable creation */` |
|      18 |  2761 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 |  2762 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 |  2763 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2764 | `			/* Empty expression */` |
|       3 |  2765 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2766 | `			return SXRET_OK;` |
|       - |  2767 | `		}` |
|       - |  2768 | `		/* Compile the expression holding the variable name */` |
|      16 |  2769 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2770 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2771 | `			return SXERR_ABORT;` |
|      16 |  2772 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2773 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2774 | `			return SXRET_OK;` |
|       - |  2775 | `		}` |
|       7 |  2776 | `	}else{` |
|       - |  2777 | `		SyHashEntry *pEntry;` |
|       - |  2778 | `		SyString *pName;` |
|  986830 |  2779 | `		char *zName = 0;` |
|       - |  2780 | `		/* Extract variable name */` |
|  986830 |  2781 | `		pName = &pGen->pIn->sData;` |
|       - |  2782 | `		/* Advance the stream cursor */` |
|  986830 |  2783 | `		pGen->pIn++;` |
|  986830 |  2784 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  986830 |  2785 | `		if( pEntry == 0 ){` |
|       - |  2786 | `			/* Duplicate name */` |
|  132378 |  2787 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  132378 |  2788 | `			if( zName == 0 ){` |
|     ! 0 |  2789 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2790 | `				return SXERR_ABORT;` |
|       - |  2791 | `			}` |
|       - |  2792 | `			/* Install in the hashtable */` |
|  132378 |  2793 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   66190 |  2794 | `		}else{` |
|       - |  2795 | `			/* Name already available */` |
|  854454 |  2796 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2797 | `		}` |
|  986830 |  2798 | `		p3 = (void *)zName;` |
|       - |  2799 | `	}` |
|  986842 |  2800 | `	iP1 = 0;` |
|  986842 |  2801 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  359666 |  2802 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2803 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  359648 |  2804 | `			iP1 = 1;` |
|  179823 |  2805 | `		}` |
|  179832 |  2806 | `	}` |
|       - |  2807 | `	/* Emit the load instruction */` |
|  986842 |  2808 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  986854 |  2809 | `	while( iVv > 0 ){` |
|      13 |  2810 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2811 | `		iVv--;` |
|       1 |  2812 | `	}` |
|       - |  2813 | `	/* Node successfully compiled */` |
|  986842 |  2814 | `	return SXRET_OK;` |
|  493424 |  2815 |  |
|       - |  2816 | `/*` |
|       - |  2817 | ` * Load a literal.` |
|       - |  2818 | ` */` |
|  693620 |  2819 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 |  2820 |  |
|  693622 |  2821 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2822 | `	ph7_value *pObj;` |
|       - |  2823 | `	SyString *pStr;` |
|       - |  2824 | `	sxu32 nIdx;` |
|       - |  2825 | `	/* Extract token value */` |
|  693622 |  2826 | `	pStr = &pToken->sData;` |
|       - |  2827 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  693622 |  2828 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  147042 |  2829 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2830 | `			/* NULL constant are always indexed at 0 */` |
|   54174 |  2831 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   54174 |  2832 | `			return SXRET_OK;` |
|   92870 |  2833 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2834 | `			/* TRUE constant are always indexed at 1 */` |
|     572 |  2835 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     572 |  2836 | `			return SXRET_OK;` |
|       2 |  2837 | `		}` |
|  647702 |  2838 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  109942 |  2839 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2840 | `			/* FALSE constant are always indexed at 2 */` |
|   41546 |  2841 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   41546 |  2842 | `			return SXRET_OK;` |
|  554340 |  2843 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   98604 |  2844 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2845 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    9452 |  2846 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    9452 |  2847 | `			if( pObj == 0 ){` |
|     ! 0 |  2848 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2849 | `				return SXERR_ABORT;` |
|       - |  2850 | `			}` |
|    9452 |  2851 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2852 | `			/* Emit the load constant instruction */` |
|    9452 |  2853 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    9452 |  2854 | `			return SXRET_OK;` |
|  511519 |  2855 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   31862 |  2856 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2857 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2858 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2859 | `			if( pObj == 0 ){` |
|     ! 0 |  2860 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2861 | `				return SXERR_ABORT;` |
|       - |  2862 | `			}` |
|       7 |  2863 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2864 | `				SyString sNs;` |
|       7 |  2865 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2866 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2867 | `			}else{` |
|     ! 0 |  2868 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2869 | `			}` |
|       7 |  2870 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2871 | `			return SXRET_OK;` |
|  510644 |  2872 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   13344 |  2873 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  503966 |  2874 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   16786 |  2875 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2876 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2877 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2878 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2879 | `				/* Point to the upper block */` |
|      11 |  2880 | `				pBlock = pBlock->pParent;` |
|       1 |  2881 | `			}` |
|      11 |  2882 | `			if( pBlock == 0 ){` |
|       - |  2883 | `				/* Called in the global scope,load NULL */` |
|       5 |  2884 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2885 | `			}else{` |
|       - |  2886 | `				/* Extract the target function/method */` |
|       7 |  2887 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2888 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2889 | `					/* Not a class method,Load null */` |
|       3 |  2890 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2891 | `				}else{` |
|       5 |  2892 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2893 | `					if( pObj == 0 ){` |
|     ! 0 |  2894 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2895 | `						return SXERR_ABORT;` |
|       - |  2896 | `					}` |
|       5 |  2897 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2898 | `					/* Emit the load constant instruction */` |
|       5 |  2899 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2900 | `				}` |
|       - |  2901 | `			}` |
|      11 |  2902 | `			return SXRET_OK;` |
|       - |  2903 | `	}` |
|       - |  2904 | `	/* Query literal table */` |
|  587870 |  2905 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2906 | `		ph7_value *pLitObj;` |
|       - |  2907 | `		/* Unknown literal,install it in the literal table */` |
|  244060 |  2908 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  244060 |  2909 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2910 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2911 | `			return SXERR_ABORT;` |
|       - |  2912 | `		}` |
|  244060 |  2913 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  244060 |  2914 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  122029 |  2915 | `	}` |
|       - |  2916 | `	/* Emit the load constant instruction */` |
|  587870 |  2917 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  587870 |  2918 | `	return SXRET_OK;` |
|  346812 |  2919 |  |
|       - |  2920 | `/*` |
|       - |  2921 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2922 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2923 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2924 | ` * Otherwise, load the simple literal directly.` |
|       - |  2925 | ` */` |
|  693656 |  2926 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 |  2927 |  |
|       - |  2928 | `	sxi32 rc;` |
|  693658 |  2929 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2930 | `		return SXRET_OK;` |
|       - |  2931 | `	}` |
|       - |  2932 | `	/* Check if this is a multi-token namespace path */` |
|  693658 |  2933 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2934 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      38 |  2935 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      38 |  2936 | `		int isAbsolute = 0;` |
|      38 |  2937 | `		SyBlobReset(pWorker);` |
|       - |  2938 | `		/* Check for leading backslash (absolute path) */` |
|      38 |  2939 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      36 |  2940 | `			isAbsolute = 1;` |
|      36 |  2941 | `			pGen->pIn++; /* Skip leading backslash */` |
|      17 |  2942 | `		}` |
|       - |  2943 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      38 |  2944 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2945 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2946 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2947 | `		}` |
|       - |  2948 | `		/* Collect all path components */` |
|     134 |  2949 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     134 |  2950 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      50 |  2951 | `				SyBlobAppend(pWorker,"\\",1);` |
|      26 |  2952 | `			}else{` |
|      86 |  2953 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2954 | `			}` |
|     134 |  2955 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      38 |  2956 | `				pGen->pIn++;` |
|      38 |  2957 | `				break;` |
|       - |  2958 | `			}` |
|      98 |  2959 | `			pGen->pIn++;` |
|       2 |  2960 | `		}` |
|      38 |  2961 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2962 | `			ph7_value *pObj;` |
|       - |  2963 | `			SyString sPath;` |
|       - |  2964 | `			sxu32 nIdx;` |
|      38 |  2965 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2966 | `			/* Install in the literal table */` |
|      38 |  2967 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      18 |  2968 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 |  2969 | `				if( pObj == 0 ){` |
|     ! 0 |  2970 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2971 | `					return SXERR_ABORT;` |
|       - |  2972 | `				}` |
|      18 |  2973 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      18 |  2974 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 |  2975 | `			}` |
|       - |  2976 | `			/* Emit the load constant instruction.` |
|       - |  2977 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  2978 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      56 |  2979 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      18 |  2980 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      18 |  2981 | `				nIdx,0,0);` |
|      38 |  2982 | `			return SXRET_OK;` |
|       - |  2983 | `		}` |
|     ! 0 |  2984 | `	}` |
|       - |  2985 | `	/* Single-token literal: load directly */` |
|  693622 |  2986 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  693622 |  2987 | `	return rc;` |
|  346830 |  2988 |  |
|       - |  2989 | `/*` |
|       - |  2990 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2991 | ` */` |
|  693656 |  2992 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2993 |  |
|       - |  2994 | `	sxi32 rc;` |
|  693658 |  2995 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  693658 |  2996 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2997 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2998 | `		return rc;` |
|       - |  2999 | `	}` |
|       - |  3000 | `	/* Node successfully compiled */` |
|  693658 |  3001 | `	return SXRET_OK;` |
|  346830 |  3002 |  |
|       - |  3003 | `/*` |
|       - |  3004 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3005 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3006 | ` */` |
|       8 |  3007 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3008 |  |
|       - |  3009 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3010 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3011 | `		pGen->pIn++;` |
|       1 |  3012 | `	}` |
|       9 |  3013 | `	return SXRET_OK;` |
|       1 |  3014 |  |
|       - |  3015 | `/*` |
|       - |  3016 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3017 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3018 | ` */` |
|      58 |  3019 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 |  3020 |  |
|      60 |  3021 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 |  3022 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3023 | `			return TRUE;` |
|      24 |  3024 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  3025 | `			return TRUE;` |
|       2 |  3026 | `		}` |
|      45 |  3027 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3028 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3029 | `			return TRUE;` |
|       - |  3030 | `		}` |
|     ! 0 |  3031 | `	}` |
|       - |  3032 | `	/* Not a reserved constant */` |
|      52 |  3033 | `	return FALSE;` |
|      31 |  3034 |  |
|       - |  3035 | `/*` |
|       - |  3036 | ` * Compile the 'const' statement.` |
|       - |  3037 | ` * According to the PHP language reference` |
|       - |  3038 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3039 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3040 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3041 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3042 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3043 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3044 | ` *  Syntax` |
|       - |  3045 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3046 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3047 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3048 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3049 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3050 | ` *  to get a list of all defined constants.` |
|       - |  3051 | ` *` |
|       - |  3052 | ` * Symisc eXtension.` |
|       - |  3053 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3054 | ` *  would allow only simple scalar value.` |
|       - |  3055 | ` *  Example` |
|       - |  3056 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3057 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3058 | ` */` |
|      32 |  3059 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 |  3060 |  |
|       - |  3061 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 |  3062 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3063 | `	SyString *pName;` |
|       - |  3064 | `	sxi32 rc;` |
|      34 |  3065 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 |  3066 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3067 | `		/* Invalid constant name */` |
|       7 |  3068 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 |  3069 | `		if( rc == SXERR_ABORT ){` |
|       - |  3070 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3071 | `			return SXERR_ABORT;` |
|       - |  3072 | `		}` |
|       7 |  3073 | `		goto Synchronize;` |
|       - |  3074 | `	}` |
|       - |  3075 | `	/* Peek constant name */` |
|      28 |  3076 | `	pName = &pGen->pIn->sData;` |
|       - |  3077 | `	/* Make sure the constant name isn't reserved */` |
|      28 |  3078 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3079 | `		/* Reserved constant */` |
|       9 |  3080 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3081 | `		if( rc == SXERR_ABORT ){` |
|       - |  3082 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3083 | `			return SXERR_ABORT;` |
|       - |  3084 | `		}` |
|       9 |  3085 | `		goto Synchronize;` |
|       - |  3086 | `	}` |
|      20 |  3087 | `	pGen->pIn++;` |
|      20 |  3088 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3089 | `		/* Invalid statement*/` |
|       5 |  3090 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 |  3091 | `		if( rc == SXERR_ABORT ){` |
|       - |  3092 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3093 | `			return SXERR_ABORT;` |
|       - |  3094 | `		}` |
|       5 |  3095 | `		goto Synchronize;` |
|       - |  3096 | `	}` |
|      15 |  3097 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3098 | `	/* Allocate a new constant value container */` |
|      15 |  3099 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3100 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3101 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3102 | `		return SXERR_ABORT;` |
|       - |  3103 | `	}` |
|      15 |  3104 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3105 | `	/* Swap bytecode container */` |
|      15 |  3106 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3107 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3108 | `	/* Compile constant value */` |
|      15 |  3109 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3110 | `	/* Emit the done instruction */` |
|      15 |  3111 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3112 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3113 | `	if( rc == SXERR_ABORT ){` |
|       - |  3114 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3115 | `		return SXERR_ABORT;` |
|       - |  3116 | `	}` |
|      15 |  3117 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3118 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3119 | `	{` |
|       - |  3120 | `		SyBlob sFQN;` |
|       - |  3121 | `		SyString sFQNStr;` |
|      15 |  3122 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3123 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3124 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3125 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3126 | `		SyBlobRelease(&sFQN);` |
|       - |  3127 | `	}` |
|      15 |  3128 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3129 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3130 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3131 | `	}` |
|      15 |  3132 | `	return SXRET_OK;` |
|       9 |  3133 | `Synchronize:` |
|       - |  3134 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 |  3135 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 |  3136 | `		pGen->pIn++;` |
|       1 |  3137 | `	}` |
|      19 |  3138 | `	return SXRET_OK;` |
|      18 |  3139 |  |
|       - |  3140 | `/*` |
|       - |  3141 | ` * Compile the 'continue' statement.` |
|       - |  3142 | ` * According to the PHP language reference` |
|       - |  3143 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3144 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3145 | ` *  iteration.` |
|       - |  3146 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3147 | ` *  the purposes of continue.` |
|       - |  3148 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3149 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3150 | ` *  Note:` |
|       - |  3151 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3152 | ` */` |
|       - |  3153 | `/*` |
|       - |  3154 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3155 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3156 | ` * break/continue crosses a try boundary.` |
|       - |  3157 | ` *` |
|       - |  3158 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3159 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3160 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3161 | ` */` |
|    3284 |  3162 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 |  3163 |  |
|    3286 |  3164 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   19234 |  3165 | `	while( pBlock && pBlock != pTarget ){` |
|   15950 |  3166 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3167 | `			if( pBlock->pUserData ){` |
|       - |  3168 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3169 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3170 | `			}else{` |
|       - |  3171 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3172 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3173 | `				 * exception context from a sub-execution.` |
|       - |  3174 | `				 */` |
|     ! 0 |  3175 | `				break;` |
|       - |  3176 | `			}` |
|       1 |  3177 | `		}` |
|   15950 |  3178 | `		pBlock = pBlock->pParent;` |
|       2 |  3179 | `	}` |
|    3286 |  3180 |  |
|    3192 |  3181 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 |  3182 |  |
|       - |  3183 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3184 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3185 | `	sxu32 nLineLocal;` |
|       - |  3186 | `	sxi32 rc;` |
|    3194 |  3187 | `	nLineLocal = pGen->pIn->nLine;` |
|    3194 |  3188 | `	iLevel = 0;` |
|       - |  3189 | `	/* Jump the 'continue' keyword */` |
|    3194 |  3190 | `	pGen->pIn++;` |
|    3194 |  3191 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3192 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3193 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3194 | `		 */` |
|       - |  3195 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3196 | `		char *zAlloc = 0;` |
|       - |  3197 | `		SyString sNum;` |
|      16 |  3198 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3199 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3200 | `			return SXERR_ABORT;` |
|       - |  3201 | `		}` |
|      16 |  3202 | `		if( rc == SXRET_OK ){` |
|      20 |  3203 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3204 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3205 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3206 | `				return SXERR_ABORT;` |
|       - |  3207 | `			}` |
|      14 |  3208 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3209 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3210 | `		}` |
|      16 |  3211 | `		if( iLevel < 2 ){` |
|       3 |  3212 | `			iLevel = 0;` |
|       1 |  3213 | `		}` |
|      16 |  3214 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3215 | `	}` |
|       - |  3216 | `	/* Point to the target loop */` |
|    3194 |  3217 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3194 |  3218 | `	if( pLoop == 0 ){` |
|       - |  3219 | `		/* Illegal continue */` |
|      11 |  3220 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 |  3221 | `		if( rc == SXERR_ABORT ){` |
|       - |  3222 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3223 | `			return SXERR_ABORT;` |
|       - |  3224 | `		}` |
|       6 |  3225 | `	}else{` |
|    3184 |  3226 | `		sxu32 nInstrIdx = 0;` |
|       - |  3227 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3184 |  3228 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3184 |  3229 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3230 | `			/* According to the PHP language reference manual` |
|       - |  3231 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3232 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3233 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3234 | `			 */` |
|       5 |  3235 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3236 | `			if( rc == SXRET_OK ){` |
|       5 |  3237 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3238 | `			}` |
|       3 |  3239 | `		}else{` |
|       - |  3240 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3180 |  3241 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3180 |  3242 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3243 | `				JumpFixup sJumpFix;` |
|       - |  3244 | `				/* Post-continue */` |
|      14 |  3245 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3246 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3247 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3248 | `			}` |
|       - |  3249 | `		}` |
|       - |  3250 | `	}` |
|    3194 |  3251 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3252 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3253 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3254 | `	}` |
|       - |  3255 | `	/* Statement successfully compiled */` |
|    3194 |  3256 | `	return SXRET_OK;` |
|    1598 |  3257 |  |
|       - |  3258 | `/*` |
|       - |  3259 | ` * Compile the 'break' statement.` |
|       - |  3260 | ` * According to the PHP language reference` |
|       - |  3261 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3262 | ` *  structure.` |
|       - |  3263 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3264 | ` *  enclosing structures are to be broken out of.` |
|       - |  3265 | ` */` |
|     118 |  3266 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 |  3267 |  |
|       - |  3268 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3269 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3270 | `	sxi32 rc;` |
|     120 |  3271 | `	iLevel = 0;` |
|       - |  3272 | `	/* Jump the 'break' keyword */` |
|     120 |  3273 | `	pGen->pIn++;` |
|     120 |  3274 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3275 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3276 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3277 | `		 */` |
|       - |  3278 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3279 | `		char *zAlloc = 0;` |
|       - |  3280 | `		SyString sNum;` |
|      16 |  3281 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3282 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3283 | `			return SXERR_ABORT;` |
|       - |  3284 | `		}` |
|      16 |  3285 | `		if( rc == SXRET_OK ){` |
|      20 |  3286 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3287 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3288 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3289 | `				return SXERR_ABORT;` |
|       - |  3290 | `			}` |
|      14 |  3291 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3292 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3293 | `		}` |
|      16 |  3294 | `		if( iLevel < 2 ){` |
|       3 |  3295 | `			iLevel = 0;` |
|       1 |  3296 | `		}` |
|      16 |  3297 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3298 | `	}` |
|       - |  3299 | `	/* Extract the target loop */` |
|     120 |  3300 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     120 |  3301 | `	if( pLoop == 0 ){` |
|       - |  3302 | `		/* Illegal break */` |
|      17 |  3303 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 |  3304 | `		if( rc == SXERR_ABORT ){` |
|       - |  3305 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3306 | `			return SXERR_ABORT;` |
|       - |  3307 | `		}` |
|       9 |  3308 | `	}else{` |
|       - |  3309 | `		sxu32 nInstrIdx;` |
|       - |  3310 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     104 |  3311 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     104 |  3312 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     104 |  3313 | `		if( rc == SXRET_OK ){` |
|       - |  3314 | `			/* Fix the jump later when the jump destination is resolved */` |
|     104 |  3315 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      51 |  3316 | `		}` |
|       - |  3317 | `	}` |
|     120 |  3318 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3319 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3320 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3321 | `	}` |
|       - |  3322 | `	/* Statement successfully compiled */` |
|     120 |  3323 | `	return SXRET_OK;` |
|      61 |  3324 |  |
|       - |  3325 | `/*` |
|       - |  3326 | ` * Compile or record a label.` |
|       - |  3327 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3328 | ` * Example` |
|       - |  3329 | ` *  goto LABEL;` |
|       - |  3330 | ` *   echo 'Foo';` |
|       - |  3331 | ` *  LABEL:` |
|       - |  3332 | ` *   echo 'Bar';` |
|       - |  3333 | ` */` |
|     112 |  3334 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 |  3335 |  |
|       - |  3336 | `	GenBlock *pBlock;` |
|       - |  3337 | `	Label sLabel;` |
|       - |  3338 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 |  3339 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 |  3340 | `	if( pBlock ){` |
|       - |  3341 | `		sxi32 rc;` |
|       7 |  3342 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3343 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 |  3344 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3345 | `			return SXERR_ABORT;` |
|       - |  3346 | `		}` |
|       3 |  3347 | `	}else{` |
|     110 |  3348 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3349 | `		char *zDup;` |
|       - |  3350 | `		/* Initialize label fields */` |
|     110 |  3351 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3352 | `		/* Duplicate label name */` |
|     110 |  3353 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 |  3354 | `		if( zDup == 0 ){` |
|     ! 0 |  3355 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3356 | `			return SXERR_ABORT;` |
|       - |  3357 | `		}` |
|     110 |  3358 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 |  3359 | `		sLabel.bRef  = FALSE;` |
|     110 |  3360 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 |  3361 | `		pBlock = pGen->pCurrent;` |
|     218 |  3362 | `		while( pBlock ){` |
|     130 |  3363 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 |  3364 | `				break;` |
|       - |  3365 | `			}` |
|       - |  3366 | `			/* Point to the upper block */` |
|     110 |  3367 | `			pBlock = pBlock->pParent;` |
|       2 |  3368 | `		}` |
|     110 |  3369 | `		if( pBlock ){` |
|      22 |  3370 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 |  3371 | `		}else{` |
|      90 |  3372 | `			sLabel.pFunc = 0;` |
|       - |  3373 | `		}` |
|       - |  3374 | `		/* Insert in label set */` |
|     110 |  3375 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3376 | `	}` |
|     114 |  3377 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 |  3378 | `	return SXRET_OK;` |
|      58 |  3379 |  |
|       - |  3380 | `/*` |
|       - |  3381 | ` * Compile the so hated 'goto' statement.` |
|       - |  3382 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3383 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3384 | ` * a compiler it has to do this.` |
|       - |  3385 | ` * According to the PHP language reference manual` |
|       - |  3386 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3387 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3388 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3389 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3390 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3391 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3392 | ` *   of a multi-level break` |
|       - |  3393 | ` */` |
|     152 |  3394 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 |  3395 |  |
|       - |  3396 | `	JumpFixup sJump;` |
|       - |  3397 | `	sxi32 rc;` |
|     154 |  3398 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 |  3399 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3400 | `		/* Missing label */` |
|     ! 0 |  3401 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3402 | `		if( rc == SXERR_ABORT ){` |
|       - |  3403 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3404 | `			return SXERR_ABORT;` |
|       - |  3405 | `		}` |
|     ! 0 |  3406 | `		return SXRET_OK;` |
|       - |  3407 | `	}` |
|     154 |  3408 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3409 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3410 | `		if( rc == SXERR_ABORT ){` |
|       - |  3411 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3412 | `			return SXERR_ABORT;` |
|       - |  3413 | `		}` |
|       3 |  3414 | `	}else{` |
|     150 |  3415 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3416 | `		GenBlock *pBlock;` |
|       - |  3417 | `		char *zDup;` |
|       - |  3418 | `		/* Prepare the jump destination */` |
|     150 |  3419 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 |  3420 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3421 | `		/* Duplicate label name */` |
|     150 |  3422 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 |  3423 | `		if( zDup == 0 ){` |
|     ! 0 |  3424 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3425 | `			return SXERR_ABORT;` |
|       - |  3426 | `		}` |
|     150 |  3427 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 |  3428 | `		pBlock = pGen->pCurrent;` |
|     312 |  3429 | `		while( pBlock ){` |
|     196 |  3430 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 |  3431 | `				break;` |
|       - |  3432 | `			}` |
|       - |  3433 | `			/* Point to the upper block */` |
|     164 |  3434 | `			pBlock = pBlock->pParent;` |
|       2 |  3435 | `		}` |
|     150 |  3436 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 |  3437 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 |  3438 | `			if( rc == SXERR_ABORT ){` |
|       - |  3439 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3440 | `				return SXERR_ABORT;` |
|       - |  3441 | `			}` |
|       3 |  3442 | `		}` |
|     150 |  3443 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 |  3444 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 |  3445 | `		}else{` |
|     124 |  3446 | `			sJump.pFunc = 0;` |
|       - |  3447 | `		}` |
|       - |  3448 | `		/* Emit the unconditional jump */` |
|     150 |  3449 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 |  3450 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3451 | `		}` |
|       - |  3452 | `	}` |
|     154 |  3453 | `	pGen->pIn++; /* Jump the label name */` |
|     154 |  3454 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3455 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3456 | `	}` |
|       - |  3457 | `	/* Statement successfully compiled */` |
|     154 |  3458 | `	return SXRET_OK;` |
|      78 |  3459 |  |
|       - |  3460 | `/*` |
|       - |  3461 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3462 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3463 | ` * failure.` |
|       - |  3464 | ` */` |
|      20 |  3465 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3466 |  |
|       - |  3467 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3468 | `	sxu32 nRawObj;` |
|      10 |  3469 | `	sxu32 nObjIdx;` |
|       - |  3470 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3471 | `	 * a PHP block.` |
|       - |  3472 | `	 */` |
|      10 |  3473 | `Consume:` |
|      21 |  3474 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3475 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3476 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3477 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3478 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3479 | `			return SXERR_ABORT;` |
|       - |  3480 | `		}` |
|       - |  3481 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3482 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3483 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3484 | `		++nRawObj;` |
|     ! 0 |  3485 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3486 | `	}` |
|      21 |  3487 | `	if( nRawObj > 0 ){` |
|       - |  3488 | `		/* Emit the consume instruction */` |
|     ! 0 |  3489 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3490 | `	}` |
|      21 |  3491 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3492 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3493 | `		/* Reset the token set */` |
|     ! 0 |  3494 | `		SySetReset(pTokenSet);` |
|       - |  3495 | `		/* Tokenize input */` |
|     ! 0 |  3496 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3497 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3498 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3499 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3500 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3501 | `		/* Advance the stream cursor */` |
|     ! 0 |  3502 | `		pGen->pRawIn++;` |
|       - |  3503 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3504 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3505 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3506 | `			sxi32 rc;` |
|       - |  3507 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3508 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3509 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3510 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3511 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3512 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3513 | `				return SXERR_ABORT;` |
|     ! 0 |  3514 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3515 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3516 | `			}` |
|     ! 0 |  3517 | `			goto Consume;` |
|       - |  3518 | `		}` |
|     ! 0 |  3519 | `	}else{` |
|       - |  3520 | `		/* No more chunks to process */` |
|      21 |  3521 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3522 | `		return SXERR_EOF;` |
|       - |  3523 | `	}` |
|     ! 0 |  3524 | `	return SXRET_OK;` |
|      11 |  3525 |  |
|       - |  3526 | `/*` |
|       - |  3527 | ` * Compile a PHP block.` |
|       - |  3528 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3529 | ` * optionally delimited by braces {}.` |
|       - |  3530 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3531 | ` * and this function takes care of generating the appropriate error` |
|       - |  3532 | ` * message.` |
|       - |  3533 | ` */` |
|  381832 |  3534 | `static sxi32 PH7_CompileBlock(` |
|       - |  3535 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3536 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3537 | `	)` |
|       2 |  3538 |  |
|       - |  3539 | `	sxi32 rc;` |
|       - |  3540 | `	sxu32 nLine;` |
|  381834 |  3541 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  380380 |  3542 | `		nLine = pGen->pIn->nLine;` |
|  380380 |  3543 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  380380 |  3544 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3545 | `			return SXERR_ABORT;` |
|       - |  3546 | `		}` |
|  380380 |  3547 | `		pGen->pIn++;` |
|       - |  3548 | `		/* Compile until we hit the closing braces '}' */` |
|  519576 |  3549 | `		for(;;){` |
| 1039154 |  3550 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3551 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3552 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3553 | `			 	   return SXERR_ABORT;` |
|       - |  3554 | `				}` |
|      21 |  3555 | `				if( rc == SXERR_EOF ){` |
|       - |  3556 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3557 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3558 | `					break;` |
|       - |  3559 | `				}` |
|     ! 0 |  3560 | `			}` |
| 1039134 |  3561 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3562 | `				/* Closing braces found,break immediately*/` |
|  380360 |  3563 | `				pGen->pIn++;` |
|  380360 |  3564 | `				break;` |
|       - |  3565 | `			}` |
|       - |  3566 | `			/* Compile a single statement */` |
|  658776 |  3567 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  658776 |  3568 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3569 | `				return SXERR_ABORT;` |
|       - |  3570 | `			}` |
|       2 |  3571 | `		}` |
|  380380 |  3572 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  191645 |  3573 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3574 | `		pGen->pIn++;` |
|     ! 0 |  3575 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3576 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3577 | `			return SXERR_ABORT;` |
|       - |  3578 | `		}` |
|       - |  3579 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3580 | `		for(;;){` |
|     ! 0 |  3581 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3582 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3583 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3584 | `			 	   return SXERR_ABORT;` |
|       - |  3585 | `				}` |
|     ! 0 |  3586 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3587 | `					/* No more token to process */` |
|     ! 0 |  3588 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3589 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3590 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3591 | `					}` |
|     ! 0 |  3592 | `					break;` |
|       - |  3593 | `				}` |
|     ! 0 |  3594 | `			}` |
|     ! 0 |  3595 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3596 | `				sxi32 nKwrd;` |
|       - |  3597 | `				/* Keyword found */` |
|     ! 0 |  3598 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3599 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3600 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3601 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3602 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3603 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3604 | `						}` |
|     ! 0 |  3605 | `						break;` |
|       - |  3606 | `				}` |
|     ! 0 |  3607 | `			}` |
|       - |  3608 | `			/* Compile a single statement */` |
|     ! 0 |  3609 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3610 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3611 | `				return SXERR_ABORT;` |
|       - |  3612 | `			}` |
|     ! 0 |  3613 | `		}` |
|     ! 0 |  3614 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3615 | `	}else{` |
|       - |  3616 | `		/* Compile a single statement */` |
|    1456 |  3617 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1456 |  3618 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3619 | `			return SXERR_ABORT;` |
|       - |  3620 | `		}` |
|       - |  3621 | `	}` |
|       - |  3622 | `	/* Jump trailing semi-colons ';' */` |
|  381834 |  3623 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3624 | `		pGen->pIn++;` |
|     ! 0 |  3625 | `	}` |
|  381834 |  3626 | `	return SXRET_OK;` |
|  190918 |  3627 |  |
|       - |  3628 | `/*` |
|       - |  3629 | ` * Compile the gentle 'while' statement.` |
|       - |  3630 | ` * According to the PHP language reference` |
|       - |  3631 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3632 | ` *  The basic form of a while statement is:` |
|       - |  3633 | ` *  while (expr)` |
|       - |  3634 | ` *   statement` |
|       - |  3635 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3636 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3637 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3638 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3639 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3640 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3641 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3642 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3643 | ` *  while (expr):` |
|       - |  3644 | ` *    statement` |
|       - |  3645 | ` *   endwhile;` |
|       - |  3646 | ` */` |
|   12700 |  3647 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 |  3648 |  |
|   12702 |  3649 | `	GenBlock *pWhileBlock = 0;` |
|   12702 |  3650 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3651 | `	sxu32 nFalseJump;` |
|       - |  3652 | `	sxu32 nLine;` |
|       - |  3653 | `	sxi32 rc;` |
|   12702 |  3654 | `	nLine = pGen->pIn->nLine;` |
|       - |  3655 | `	/* Jump the 'while' keyword */` |
|   12702 |  3656 | `	pGen->pIn++;` |
|   12702 |  3657 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3658 | `		/* Syntax error */` |
|     ! 0 |  3659 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3660 | `		if( rc == SXERR_ABORT ){` |
|       - |  3661 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3662 | `			return SXERR_ABORT;` |
|       - |  3663 | `		}` |
|     ! 0 |  3664 | `		goto Synchronize;` |
|       - |  3665 | `	}` |
|       - |  3666 | `	/* Jump the left parenthesis '(' */` |
|   12702 |  3667 | `	pGen->pIn++;` |
|       - |  3668 | `	/* Create the loop block */` |
|   12702 |  3669 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   12702 |  3670 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3671 | `		return SXERR_ABORT;` |
|       - |  3672 | `	}` |
|       - |  3673 | `	/* Delimit the condition */` |
|   12702 |  3674 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   12702 |  3675 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3676 | `		/* Empty expression */` |
|       3 |  3677 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3678 | `		if( rc == SXERR_ABORT ){` |
|       - |  3679 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3680 | `			return SXERR_ABORT;` |
|       - |  3681 | `		}` |
|       1 |  3682 | `	}` |
|       - |  3683 | `	/* Swap token streams */` |
|   12702 |  3684 | `	pTmp = pGen->pEnd;` |
|   12702 |  3685 | `	pGen->pEnd = pEnd;` |
|       - |  3686 | `	/* Compile the expression */` |
|   12702 |  3687 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12702 |  3688 | `	if( rc == SXERR_ABORT ){` |
|       - |  3689 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3690 | `		return SXERR_ABORT;` |
|       - |  3691 | `	}` |
|       - |  3692 | `	/* Update token stream */` |
|   12702 |  3693 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3694 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3695 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3696 | `			return SXERR_ABORT;` |
|       - |  3697 | `		}` |
|     ! 0 |  3698 | `		pGen->pIn++;` |
|     ! 0 |  3699 | `	}` |
|       - |  3700 | `	/* Synchronize pointers */` |
|   12702 |  3701 | `	pGen->pIn  = &pEnd[1];` |
|   12702 |  3702 | `	pGen->pEnd = pTmp;` |
|       - |  3703 | `	/* Emit the false jump */` |
|   12702 |  3704 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3705 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12702 |  3706 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3707 | `	/* Compile the loop body */` |
|   12702 |  3708 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   12702 |  3709 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3710 | `		return SXERR_ABORT;` |
|       - |  3711 | `	}` |
|       - |  3712 | `	/* Emit the unconditional jump to the start of the loop */` |
|   12702 |  3713 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3714 | `	/* Fix all jumps now the destination is resolved */` |
|   12702 |  3715 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3716 | `	/* Release the loop block */` |
|   12702 |  3717 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3718 | `	/* Statement successfully compiled */` |
|   12702 |  3719 | `	return SXRET_OK;` |
|     ! 0 |  3720 | `Synchronize:` |
|       - |  3721 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3722 | `	 * compiling this erroneous block.` |
|       - |  3723 | `	 */` |
|     ! 0 |  3724 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3725 | `		pGen->pIn++;` |
|     ! 0 |  3726 | `	}` |
|     ! 0 |  3727 | `	return SXRET_OK;` |
|    6352 |  3728 |  |
|       - |  3729 | `/*` |
|       - |  3730 | ` * Compile the ugly do..while() statement.` |
|       - |  3731 | ` * According to the PHP language reference` |
|       - |  3732 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3733 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3734 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3735 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3736 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3737 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3738 | ` *  would end immediately).` |
|       - |  3739 | ` *  There is just one syntax for do-while loops:` |
|       - |  3740 | ` *  <?php` |
|       - |  3741 | ` *  $i = 0;` |
|       - |  3742 | ` *  do {` |
|       - |  3743 | ` *   echo $i;` |
|       - |  3744 | ` *  } while ($i > 0);` |
|       - |  3745 | ` * ?>` |
|       - |  3746 | ` */` |
|       2 |  3747 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3748 |  |
|       3 |  3749 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3750 | `	GenBlock *pDoBlock = 0;` |
|       - |  3751 | `	sxu32 nLine;` |
|       - |  3752 | `	sxi32 rc;` |
|       3 |  3753 | `	nLine = pGen->pIn->nLine;` |
|       - |  3754 | `	/* Jump the 'do' keyword */` |
|       3 |  3755 | `	pGen->pIn++;` |
|       - |  3756 | `	/* Create the loop block */` |
|       3 |  3757 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3758 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3759 | `		return SXERR_ABORT;` |
|       - |  3760 | `	}` |
|       - |  3761 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3762 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3763 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3764 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3765 | `		return SXERR_ABORT;` |
|       - |  3766 | `	}` |
|       3 |  3767 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3768 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3769 | `	}` |
|       3 |  3770 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3771 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3772 | `			/* Missing 'while' statement */` |
|       3 |  3773 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3774 | `			if( rc == SXERR_ABORT ){` |
|       - |  3775 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3776 | `				return SXERR_ABORT;` |
|       - |  3777 | `			}` |
|       3 |  3778 | `			goto Synchronize;` |
|       - |  3779 | `	}` |
|       - |  3780 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3781 | `	pGen->pIn++;` |
|     ! 0 |  3782 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3783 | `		/* Syntax error */` |
|     ! 0 |  3784 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3785 | `		if( rc == SXERR_ABORT ){` |
|       - |  3786 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3787 | `			return SXERR_ABORT;` |
|       - |  3788 | `		}` |
|     ! 0 |  3789 | `		goto Synchronize;` |
|       - |  3790 | `	}` |
|       - |  3791 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3792 | `	pGen->pIn++;` |
|       - |  3793 | `	/* Delimit the condition */` |
|     ! 0 |  3794 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3795 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3796 | `		/* Empty expression */` |
|     ! 0 |  3797 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3798 | `		if( rc == SXERR_ABORT ){` |
|       - |  3799 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3800 | `			return SXERR_ABORT;` |
|       - |  3801 | `		}` |
|     ! 0 |  3802 | `		goto Synchronize;` |
|       - |  3803 | `	}` |
|       - |  3804 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3805 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3806 | `		JumpFixup *aPost;` |
|       - |  3807 | `		VmInstr *pInstr;` |
|       - |  3808 | `		sxu32 nJumpDest;` |
|       - |  3809 | `		sxu32 n;` |
|     ! 0 |  3810 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3811 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3812 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3813 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3814 | `			if( pInstr ){` |
|       - |  3815 | `				/* Fix */` |
|     ! 0 |  3816 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3817 | `			}` |
|     ! 0 |  3818 | `		}` |
|     ! 0 |  3819 | `	}` |
|       - |  3820 | `	/* Swap token streams */` |
|     ! 0 |  3821 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3822 | `	pGen->pEnd = pEnd;` |
|       - |  3823 | `	/* Compile the expression */` |
|     ! 0 |  3824 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3825 | `	if( rc == SXERR_ABORT ){` |
|       - |  3826 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3827 | `		return SXERR_ABORT;` |
|       - |  3828 | `	}` |
|       - |  3829 | `	/* Update token stream */` |
|     ! 0 |  3830 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3831 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3832 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3833 | `			return SXERR_ABORT;` |
|       - |  3834 | `		}` |
|     ! 0 |  3835 | `		pGen->pIn++;` |
|     ! 0 |  3836 | `	}` |
|     ! 0 |  3837 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3838 | `	pGen->pEnd = pTmp;` |
|       - |  3839 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3840 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3841 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3842 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3843 | `	/* Release the loop block */` |
|     ! 0 |  3844 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3845 | `	/* Statement successfully compiled */` |
|     ! 0 |  3846 | `	return SXRET_OK;` |
|       1 |  3847 | `Synchronize:` |
|       - |  3848 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3849 | `	 * compiling this erroneous block.` |
|       - |  3850 | `	 */` |
|       3 |  3851 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3852 | `		pGen->pIn++;` |
|     ! 0 |  3853 | `	}` |
|       3 |  3854 | `	return SXRET_OK;` |
|       2 |  3855 |  |
|       - |  3856 | `/*` |
|       - |  3857 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3858 | ` * According to the PHP language reference` |
|       - |  3859 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3860 | ` *  The syntax of a for loop is:` |
|       - |  3861 | ` *  for (expr1; expr2; expr3)` |
|       - |  3862 | ` *   statement` |
|       - |  3863 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3864 | ` *  the beginning of the loop.` |
|       - |  3865 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3866 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3867 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3868 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3869 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3870 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3871 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3872 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3873 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3874 | ` *  of using the for truth expression.` |
|       - |  3875 | ` */` |
|   12712 |  3876 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 |  3877 |  |
|   12714 |  3878 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   12714 |  3879 | `	GenBlock *pForBlock = 0;` |
|       - |  3880 | `	sxu32 nFalseJump;` |
|       - |  3881 | `	sxu32 nLine;` |
|       - |  3882 | `	sxi32 rc;` |
|   12714 |  3883 | `	nLine = pGen->pIn->nLine;` |
|       - |  3884 | `	/* Jump the 'for' keyword */` |
|   12714 |  3885 | `	pGen->pIn++;` |
|   12714 |  3886 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3887 | `		/* Syntax error */` |
|     ! 0 |  3888 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3889 | `		if( rc == SXERR_ABORT ){` |
|       - |  3890 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3891 | `			return SXERR_ABORT;` |
|       - |  3892 | `		}` |
|     ! 0 |  3893 | `		return SXRET_OK;` |
|       - |  3894 | `	}` |
|       - |  3895 | `	/* Jump the left parenthesis '(' */` |
|   12714 |  3896 | `	pGen->pIn++;` |
|       - |  3897 | `	/* Delimit the init-expr;condition;post-expr */` |
|   12714 |  3898 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   12714 |  3899 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3900 | `		/* Empty expression */` |
|     ! 0 |  3901 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3902 | `		if( rc == SXERR_ABORT ){` |
|       - |  3903 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3904 | `			return SXERR_ABORT;` |
|       - |  3905 | `		}` |
|       - |  3906 | `		/* Synchronize */` |
|     ! 0 |  3907 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3908 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3909 | `			pGen->pIn++;` |
|     ! 0 |  3910 | `		}` |
|     ! 0 |  3911 | `		return SXRET_OK;` |
|       - |  3912 | `	}` |
|       - |  3913 | `	/* Swap token streams */` |
|   12714 |  3914 | `	pTmp = pGen->pEnd;` |
|   12714 |  3915 | `	pGen->pEnd = pEnd;` |
|       - |  3916 | `	/* Compile initialization expressions if available */` |
|   12714 |  3917 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3918 | `	/* Pop operand lvalues */` |
|   12714 |  3919 | `	if( rc == SXERR_ABORT ){` |
|       - |  3920 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3921 | `		return SXERR_ABORT;` |
|   12714 |  3922 | `	}else if( rc != SXERR_EMPTY ){` |
|   12712 |  3923 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6355 |  3924 | `	}` |
|   12714 |  3925 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3926 | `		/* Syntax error */` |
|     ! 0 |  3927 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3928 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3929 | `		if( rc == SXERR_ABORT ){` |
|       - |  3930 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3931 | `			return SXERR_ABORT;` |
|       - |  3932 | `		}` |
|     ! 0 |  3933 | `		return SXRET_OK;` |
|       - |  3934 | `	}` |
|       - |  3935 | `	/* Jump the trailing ';' */` |
|   12714 |  3936 | `	pGen->pIn++;` |
|       - |  3937 | `	/* Create the loop block */` |
|   12714 |  3938 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   12714 |  3939 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3940 | `		return SXERR_ABORT;` |
|       - |  3941 | `	}` |
|       - |  3942 | `	/* Deffer continue jumps */` |
|   12714 |  3943 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3944 | `	/* Compile the condition */` |
|   12714 |  3945 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12714 |  3946 | `	if( rc == SXERR_ABORT ){` |
|       - |  3947 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3948 | `		return SXERR_ABORT;` |
|   12714 |  3949 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3950 | `		/* Emit the false jump */` |
|   12712 |  3951 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3952 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12712 |  3953 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    6355 |  3954 | `	}` |
|   12714 |  3955 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3956 | `		/* Syntax error */` |
|       5 |  3957 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3958 | `			"for: Expected ';' after conditionals expressions");` |
|       5 |  3959 | `		if( rc == SXERR_ABORT ){` |
|       - |  3960 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3961 | `			return SXERR_ABORT;` |
|       - |  3962 | `		}` |
|       5 |  3963 | `		return SXRET_OK;` |
|       - |  3964 | `	}` |
|       - |  3965 | `	/* Jump the trailing ';' */` |
|   12710 |  3966 | `	pGen->pIn++;` |
|       - |  3967 | `	/* Save the post condition stream */` |
|   12710 |  3968 | `	pPostStart = pGen->pIn;` |
|       - |  3969 | `	/* Compile the loop body */` |
|   12710 |  3970 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   12710 |  3971 | `	pGen->pEnd = pTmp;` |
|   12710 |  3972 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   12710 |  3973 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3974 | `		return SXERR_ABORT;` |
|       - |  3975 | `	}` |
|       - |  3976 | `	/* Fix post-continue jumps */` |
|   12710 |  3977 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3978 | `		JumpFixup *aPost;` |
|       - |  3979 | `		VmInstr *pInstr;` |
|       - |  3980 | `		sxu32 nJumpDest;` |
|       - |  3981 | `		sxu32 n;` |
|      14 |  3982 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3983 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3984 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3985 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3986 | `			if( pInstr ){` |
|       - |  3987 | `				/* Fix jump */` |
|      14 |  3988 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3989 | `			}` |
|       8 |  3990 | `		}` |
|       6 |  3991 | `	}` |
|       - |  3992 | `	/* compile the post-expressions if available */` |
|   12710 |  3993 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3994 | `		pPostStart++;` |
|     ! 0 |  3995 | `	}` |
|   12710 |  3996 | `	if( pPostStart < pEnd ){` |
|       - |  3997 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   12710 |  3998 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   12710 |  3999 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12710 |  4000 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4001 | `			/* Syntax error */` |
|     ! 0 |  4002 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4003 | `			if( rc == SXERR_ABORT ){` |
|       - |  4004 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4005 | `				return SXERR_ABORT;` |
|       - |  4006 | `			}` |
|     ! 0 |  4007 | `			return SXRET_OK;` |
|       - |  4008 | `		}` |
|   12710 |  4009 | `		RE_SWAP_DELIMITER(pGen);` |
|   12710 |  4010 | `		if( rc == SXERR_ABORT ){` |
|       - |  4011 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4012 | `			return SXERR_ABORT;` |
|   12710 |  4013 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4014 | `			/* Pop operand lvalue */` |
|   12710 |  4015 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6354 |  4016 | `		}` |
|    6354 |  4017 | `	}` |
|       - |  4018 | `	/* Emit the unconditional jump to the start of the loop */` |
|   12710 |  4019 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4020 | `	/* Fix all jumps now the destination is resolved */` |
|   12710 |  4021 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4022 | `	/* Release the loop block */` |
|   12710 |  4023 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4024 | `	/* Statement successfully compiled */` |
|   12710 |  4025 | `	return SXRET_OK;` |
|    6358 |  4026 |  |
|       - |  4027 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4028 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4029 | ` * are allowed.` |
|       - |  4030 | ` */` |
|    6790 |  4031 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  4032 |  |
|    6792 |  4033 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6792 |  4034 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4035 | `		/* Unexpected expression */` |
|     ! 0 |  4036 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4037 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4038 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4039 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4040 | `		}` |
|     ! 0 |  4041 | `	}` |
|    6792 |  4042 | `	return rc;` |
|       2 |  4043 |  |
|       - |  4044 | `/*` |
|       - |  4045 | ` * Compile the 'foreach' statement.` |
|       - |  4046 | ` * According to the PHP language reference` |
|       - |  4047 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4048 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4049 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4050 | ` *  is a minor but useful extension of the first:` |
|       - |  4051 | ` *  foreach (array_expression as $value)` |
|       - |  4052 | ` *    statement` |
|       - |  4053 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4054 | ` *   statement` |
|       - |  4055 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4056 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4057 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4058 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4059 | ` *  to the variable $key on each loop.` |
|       - |  4060 | ` *  Note:` |
|       - |  4061 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4062 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4063 | ` *  Note:` |
|       - |  4064 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4065 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4066 | ` *  or after the foreach without resetting it.` |
|       - |  4067 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4068 | ` *  of copying the value.` |
|       - |  4069 | ` */` |
|    3460 |  4070 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 |  4071 |  |
|    3462 |  4072 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3462 |  4073 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3462 |  4074 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4075 | `	ph7_foreach_info *pInfo;` |
|       - |  4076 | `	sxu32 nFalseJump;` |
|       - |  4077 | `	VmInstr *pInstr;` |
|       - |  4078 | `	sxu32 nLine;` |
|       - |  4079 | `	sxi32 rc;` |
|    3462 |  4080 | `	nLine = pGen->pIn->nLine;` |
|       - |  4081 | `	/* Jump the 'foreach' keyword */` |
|    3462 |  4082 | `	pGen->pIn++;` |
|    3462 |  4083 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4084 | `		/* Syntax error */` |
|     ! 0 |  4085 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4086 | `		if( rc == SXERR_ABORT ){` |
|       - |  4087 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4088 | `			return SXERR_ABORT;` |
|       - |  4089 | `		}` |
|     ! 0 |  4090 | `		goto Synchronize;` |
|       - |  4091 | `	}` |
|       - |  4092 | `	/* Jump the left parenthesis '(' */` |
|    3462 |  4093 | `	pGen->pIn++;` |
|       - |  4094 | `	/* Create the loop block */` |
|    3462 |  4095 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3462 |  4096 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4097 | `		return SXERR_ABORT;` |
|       - |  4098 | `	}` |
|       - |  4099 | `	/* Delimit the expression */` |
|    3462 |  4100 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3462 |  4101 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4102 | `		/* Empty expression */` |
|     ! 0 |  4103 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4104 | `		if( rc == SXERR_ABORT ){` |
|       - |  4105 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4106 | `			return SXERR_ABORT;` |
|       - |  4107 | `		}` |
|       - |  4108 | `		/* Synchronize */` |
|     ! 0 |  4109 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4110 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4111 | `			pGen->pIn++;` |
|     ! 0 |  4112 | `		}` |
|     ! 0 |  4113 | `		return SXRET_OK;` |
|       - |  4114 | `	}` |
|       - |  4115 | `	/* Compile the array expression */` |
|    3462 |  4116 | `	pCur = pGen->pIn;` |
|   23204 |  4117 | `	while( pCur < pEnd ){` |
|   23204 |  4118 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3476 |  4119 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3476 |  4120 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4121 | `				/* Break with the first 'as' found */` |
|    3462 |  4122 | `				break;` |
|       - |  4123 | `			}` |
|       7 |  4124 | `		}` |
|       - |  4125 | `		/* Advance the stream cursor */` |
|   19744 |  4126 | `		pCur++;` |
|       2 |  4127 | `	}` |
|    3462 |  4128 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4129 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4130 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4131 | `		if( rc == SXERR_ABORT ){` |
|       - |  4132 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4133 | `			return SXERR_ABORT;` |
|       - |  4134 | `		}` |
|     ! 0 |  4135 | `		goto Synchronize;` |
|       - |  4136 | `	}` |
|       - |  4137 | `	/* Swap token streams */` |
|    3462 |  4138 | `	pTmp = pGen->pEnd;` |
|    3462 |  4139 | `	pGen->pEnd = pCur;` |
|    3462 |  4140 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3462 |  4141 | `	if( rc == SXERR_ABORT ){` |
|       - |  4142 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4143 | `		return SXERR_ABORT;` |
|       - |  4144 | `	}` |
|       - |  4145 | `	/* Update token stream */` |
|    3462 |  4146 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4147 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4148 | `		if( rc == SXERR_ABORT ){` |
|       - |  4149 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4150 | `			return SXERR_ABORT;` |
|       - |  4151 | `		}` |
|     ! 0 |  4152 | `		pGen->pIn++;` |
|     ! 0 |  4153 | `	}` |
|    3462 |  4154 | `	pCur++; /* Jump the 'as' keyword */` |
|    3462 |  4155 | `	pGen->pIn = pCur;` |
|    3462 |  4156 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4157 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4158 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4159 | `			return SXERR_ABORT;` |
|       - |  4160 | `		}` |
|     ! 0 |  4161 | `	}` |
|       - |  4162 | `	/* Create the foreach context */` |
|    3462 |  4163 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3462 |  4164 | `	if( pInfo == 0 ){` |
|     ! 0 |  4165 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4166 | `		return SXERR_ABORT;` |
|       - |  4167 | `	}` |
|       - |  4168 | `	/* Zero the structure */` |
|    3462 |  4169 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4170 | `	/* Initialize structure fields */` |
|    3462 |  4171 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4172 | `	/* Check if we have a key field */` |
|   10432 |  4173 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6972 |  4174 | `		pCur++;` |
|       2 |  4175 | `	}` |
|    3462 |  4176 | `	if( pCur < pEnd ){` |
|       - |  4177 | `		/* Compile the expression holding the key name */` |
|    3342 |  4178 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4179 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4180 | `			if( rc == SXERR_ABORT ){` |
|       - |  4181 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4182 | `				return SXERR_ABORT;` |
|       - |  4183 | `			}` |
|     ! 0 |  4184 | `		}else{` |
|    3342 |  4185 | `			pGen->pEnd = pCur;` |
|    3342 |  4186 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3342 |  4187 | `			if( rc == SXERR_ABORT ){` |
|       - |  4188 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4189 | `				return SXERR_ABORT;` |
|       - |  4190 | `			}` |
|    3342 |  4191 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3342 |  4192 | `			if( pInstr->p3 ){` |
|       - |  4193 | `				/* Record key name */` |
|    3342 |  4194 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1670 |  4195 | `			}` |
|    3342 |  4196 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4197 | `		}` |
|    3342 |  4198 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1670 |  4199 | `	}` |
|    3462 |  4200 | `	pGen->pEnd = pEnd;` |
|    3462 |  4201 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4202 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4203 | `		if( rc == SXERR_ABORT ){` |
|       - |  4204 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4205 | `			return SXERR_ABORT;` |
|       - |  4206 | `		}` |
|     ! 0 |  4207 | `		goto Synchronize;` |
|       - |  4208 | `	}` |
|    3462 |  4209 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4210 | `		pGen->pIn++;` |
|       - |  4211 | `		/* Pass by reference  */` |
|      11 |  4212 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4213 | `	}` |
|       - |  4214 | `	/* Check if the value target is list() */` |
|    3462 |  4215 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4216 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4217 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4218 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4219 | `		 */` |
|       - |  4220 | `		static int iForeachListCnt = 0;` |
|       - |  4221 | `		char zTmp[128];` |
|       - |  4222 | `		sxu32 nLen;` |
|       - |  4223 | `		char *zDup;` |
|      10 |  4224 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4225 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4226 | `		if( zDup == 0 ){` |
|     ! 0 |  4227 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4228 | `			return SXERR_ABORT;` |
|       - |  4229 | `		}` |
|      10 |  4230 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4231 | `		/* Save list() token boundaries */` |
|      10 |  4232 | `		pListStart = pGen->pIn;` |
|       - |  4233 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4234 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4235 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4236 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4237 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4238 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4239 | `				return SXERR_ABORT;` |
|       - |  4240 | `			}` |
|       3 |  4241 | `			goto Synchronize;` |
|       - |  4242 | `		}` |
|       7 |  4243 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4244 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4245 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4246 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4247 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4248 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4249 | `				return SXERR_ABORT;` |
|       - |  4250 | `			}` |
|     ! 0 |  4251 | `			goto Synchronize;` |
|       - |  4252 | `		}` |
|       7 |  4253 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4254 | `		pListEnd = pGen->pIn;` |
|       7 |  4255 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3457 |  4256 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4257 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4258 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4259 | `		 */` |
|       - |  4260 | `		static int iForeachShortListCnt = 0;` |
|       - |  4261 | `		char zTmp[128];` |
|       - |  4262 | `		sxu32 nLen;` |
|       - |  4263 | `		char *zDup;` |
|       3 |  4264 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  4265 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  4266 | `		if( zDup == 0 ){` |
|     ! 0 |  4267 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4268 | `			return SXERR_ABORT;` |
|       - |  4269 | `		}` |
|       3 |  4270 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4271 | `		/* Save [...] token boundaries */` |
|       3 |  4272 | `		pListStart = pGen->pIn;` |
|       - |  4273 | `		/* Advance past [...] */` |
|       3 |  4274 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  4275 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  4276 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4277 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4278 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4279 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4280 | `				return SXERR_ABORT;` |
|       - |  4281 | `			}` |
|     ! 0 |  4282 | `			goto Synchronize;` |
|       - |  4283 | `		}` |
|       3 |  4284 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  4285 | `		pListEnd = pGen->pIn;` |
|       3 |  4286 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  4287 | `	}else{` |
|       - |  4288 | `		/* Compile the expression holding the value name */` |
|    3452 |  4289 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3452 |  4290 | `		if( rc == SXERR_ABORT ){` |
|       - |  4291 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4292 | `			return SXERR_ABORT;` |
|       - |  4293 | `		}` |
|    3452 |  4294 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3452 |  4295 | `		if( pInstr->p3 ){` |
|       - |  4296 | `			/* Record value name */` |
|    3452 |  4297 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1725 |  4298 | `		}` |
|       - |  4299 | `	}` |
|       - |  4300 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3460 |  4301 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4302 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3460 |  4303 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4304 | `	/* Record the first instruction to execute */` |
|    3460 |  4305 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4306 | `	/* Emit the FOREACH_STEP instruction */` |
|    3460 |  4307 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4308 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3460 |  4309 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4310 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3460 |  4311 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4312 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4313 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4314 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4315 | `		 */` |
|       9 |  4316 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4317 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4318 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4319 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4320 | `		 */` |
|       9 |  4321 | `		pSavedIn = pGen->pIn;` |
|       9 |  4322 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4323 | `		pGen->pIn = pListStart;` |
|       9 |  4324 | `		pGen->pEnd = pListEnd;` |
|       9 |  4325 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4326 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4327 | `		}else{` |
|       7 |  4328 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4329 | `		}` |
|       9 |  4330 | `		pGen->pIn = pSavedIn;` |
|       9 |  4331 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4332 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4333 | `			return SXERR_ABORT;` |
|       - |  4334 | `		}` |
|       - |  4335 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4336 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4337 | `	}` |
|       - |  4338 | `	/* Compile the loop body */` |
|    3460 |  4339 | `	pGen->pIn = &pEnd[1];` |
|    3460 |  4340 | `	pGen->pEnd = pTmp;` |
|    3460 |  4341 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3460 |  4342 | `	if( rc == SXERR_ABORT ){` |
|       - |  4343 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4344 | `		return SXERR_ABORT;` |
|       - |  4345 | `	}` |
|       - |  4346 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3460 |  4347 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4348 | `	/* Fix all jumps now the destination is resolved */` |
|    3460 |  4349 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4350 | `	/* Release the loop block */` |
|    3460 |  4351 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4352 | `	/* Statement successfully compiled */` |
|    3460 |  4353 | `	return SXRET_OK;` |
|       1 |  4354 | `Synchronize:` |
|       - |  4355 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4356 | `	 * compiling this erroneous block.` |
|       - |  4357 | `	 */` |
|       3 |  4358 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4359 | `		pGen->pIn++;` |
|     ! 0 |  4360 | `	}` |
|       3 |  4361 | `	return SXRET_OK;` |
|    1732 |  4362 |  |
|       - |  4363 | `/*` |
|       - |  4364 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4365 | ` * According to the PHP language reference` |
|       - |  4366 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4367 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4368 | ` *  that is similar to that of C:` |
|       - |  4369 | ` *  if (expr)` |
|       - |  4370 | ` *   statement` |
|       - |  4371 | ` *  else construct:` |
|       - |  4372 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4373 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4374 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4375 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4376 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4377 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4378 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4379 | ` *  elseif` |
|       - |  4380 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4381 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4382 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4383 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4384 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4385 | ` *   <?php` |
|       - |  4386 | ` *    if ($a > $b) {` |
|       - |  4387 | ` *     echo "a is bigger than b";` |
|       - |  4388 | ` *    } elseif ($a == $b) {` |
|       - |  4389 | ` *     echo "a is equal to b";` |
|       - |  4390 | ` *    } else {` |
|       - |  4391 | ` *     echo "a is smaller than b";` |
|       - |  4392 | ` *    }` |
|       - |  4393 | ` *    ?>` |
|       - |  4394 | ` */` |
|  132234 |  4395 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 |  4396 |  |
|  132236 |  4397 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  132236 |  4398 | `	GenBlock *pCondBlock = 0;` |
|       - |  4399 | `	sxu32 nJumpIdx;` |
|       - |  4400 | `	sxu32 nKeyID;` |
|       - |  4401 | `	sxi32 rc;` |
|       - |  4402 | `	/* Jump the 'if' keyword */` |
|  132236 |  4403 | `	pGen->pIn++;` |
|  132236 |  4404 | `	pToken = pGen->pIn;` |
|       - |  4405 | `	/* Create the conditional block */` |
|  132236 |  4406 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  132236 |  4407 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4408 | `		return SXERR_ABORT;` |
|       - |  4409 | `	}` |
|       - |  4410 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   72426 |  4411 | `	for(;;){` |
|  144854 |  4412 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4413 | `			/* Syntax error */` |
|     ! 0 |  4414 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4415 | `				pToken--;` |
|     ! 0 |  4416 | `			}` |
|     ! 0 |  4417 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4418 | `			if( rc == SXERR_ABORT ){` |
|       - |  4419 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4420 | `				return SXERR_ABORT;` |
|       - |  4421 | `			}` |
|     ! 0 |  4422 | `			goto Synchronize;` |
|       - |  4423 | `		}` |
|       - |  4424 | `		/* Jump the left parenthesis '(' */` |
|  144854 |  4425 | `		pToken++;` |
|       - |  4426 | `		/* Delimit the condition */` |
|  144854 |  4427 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  144854 |  4428 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4429 | `			/* Syntax error */` |
|     ! 0 |  4430 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4431 | `				pToken--;` |
|     ! 0 |  4432 | `			}` |
|     ! 0 |  4433 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4434 | `			if( rc == SXERR_ABORT ){` |
|       - |  4435 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4436 | `				return SXERR_ABORT;` |
|       - |  4437 | `			}` |
|     ! 0 |  4438 | `			goto Synchronize;` |
|       - |  4439 | `		}` |
|       - |  4440 | `		/* Swap token streams */` |
|  144854 |  4441 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4442 | `		/* Compile the condition */` |
|  144854 |  4443 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4444 | `		/* Update token stream */` |
|  144854 |  4445 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4446 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4447 | `			pGen->pIn++;` |
|     ! 0 |  4448 | `		}` |
|  144854 |  4449 | `		pGen->pIn  = &pEnd[1];` |
|  144854 |  4450 | `		pGen->pEnd = pTmp;` |
|  144854 |  4451 | `		if( rc == SXERR_ABORT ){` |
|       - |  4452 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4453 | `			return SXERR_ABORT;` |
|       - |  4454 | `		}` |
|       - |  4455 | `		/* Emit the false jump */` |
|  144854 |  4456 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4457 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  144854 |  4458 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4459 | `		/* Compile the body */` |
|  144854 |  4460 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  144854 |  4461 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4462 | `			return SXERR_ABORT;` |
|       - |  4463 | `		}` |
|  144854 |  4464 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   40415 |  4465 | `			break;` |
|       - |  4466 | `		}` |
|       - |  4467 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   64028 |  4468 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   64028 |  4469 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   41224 |  4470 | `			break;` |
|       - |  4471 | `		}` |
|       - |  4472 | `		/* Emit the unconditional jump */` |
|   22806 |  4473 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4474 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   22806 |  4475 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   22806 |  4476 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   16484 |  4477 | `			pToken = &pGen->pIn[1];` |
|   16484 |  4478 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    6326 |  4479 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5095 |  4480 | `					break;` |
|       - |  4481 | `			}` |
|    6298 |  4482 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3148 |  4483 | `		}` |
|   12620 |  4484 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4485 | `		/* Synchronize cursors */` |
|   12620 |  4486 | `		pToken = pGen->pIn;` |
|       - |  4487 | `		/* Fix the false jump */` |
|   12620 |  4488 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 |  4489 | `	} /* For(;;) */` |
|       - |  4490 | `	/* Fix the false jump */` |
|  132236 |  4491 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  132236 |  4492 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   51408 |  4493 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4494 | `			/* Compile the else block */` |
|   10188 |  4495 | `			pGen->pIn++;` |
|   10188 |  4496 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   10188 |  4497 | `			if( rc == SXERR_ABORT ){` |
|       - |  4498 |  |
|     ! 0 |  4499 | `				return SXERR_ABORT;` |
|       - |  4500 | `			}` |
|    5093 |  4501 | `	}` |
|  132236 |  4502 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4503 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  132236 |  4504 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4505 | `	/* Release the conditional block */` |
|  132236 |  4506 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4507 | `	/* Statement successfully compiled */` |
|  132236 |  4508 | `	return SXRET_OK;` |
|     ! 0 |  4509 | `Synchronize:` |
|       - |  4510 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4511 | `	 */` |
|     ! 0 |  4512 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4513 | `		pGen->pIn++;` |
|     ! 0 |  4514 | `	}` |
|     ! 0 |  4515 | `	return SXRET_OK;` |
|   66119 |  4516 |  |
|       - |  4517 | `/*` |
|       - |  4518 | ` * Compile the global construct.` |
|       - |  4519 | ` * According to the PHP language reference` |
|       - |  4520 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4521 | ` *  to be used in that function.` |
|       - |  4522 | ` *  Example #1 Using global` |
|       - |  4523 | ` *  <?php` |
|       - |  4524 | ` *   $a = 1;` |
|       - |  4525 | ` *   $b = 2;` |
|       - |  4526 | ` *   function Sum()` |
|       - |  4527 | ` *   {` |
|       - |  4528 | ` *    global $a, $b;` |
|       - |  4529 | ` *    $b = $a + $b;` |
|       - |  4530 | ` *   }` |
|       - |  4531 | ` *   Sum();` |
|       - |  4532 | ` *   echo $b;` |
|       - |  4533 | ` *  ?>` |
|       - |  4534 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4535 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4536 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4537 | ` */` |
|      36 |  4538 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 |  4539 |  |
|      38 |  4540 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4541 | `	sxi32 nExpr;` |
|       - |  4542 | `	sxi32 rc;` |
|       - |  4543 | `	/* Jump the 'global' keyword */` |
|      38 |  4544 | `	pGen->pIn++;` |
|      38 |  4545 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4546 | `		/* Nothing to process */` |
|     ! 0 |  4547 | `		return SXRET_OK;` |
|       - |  4548 | `	}` |
|      38 |  4549 | `	pTmp = pGen->pEnd;` |
|      38 |  4550 | `	nExpr = 0;` |
|      84 |  4551 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      48 |  4552 | `		if( pGen->pIn < pNext ){` |
|      48 |  4553 | `			pGen->pEnd = pNext;` |
|      48 |  4554 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4555 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4556 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4557 | `					return SXERR_ABORT;` |
|       - |  4558 | `				}` |
|     ! 0 |  4559 | `			}else{` |
|      48 |  4560 | `				pGen->pIn++;` |
|      48 |  4561 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4562 | `					/* Emit a warning */` |
|     ! 0 |  4563 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4564 | `				}else{` |
|      48 |  4565 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      48 |  4566 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4567 | `						return SXERR_ABORT;` |
|      48 |  4568 | `					}else if(rc != SXERR_EMPTY ){` |
|      48 |  4569 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      48 |  4570 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4571 | `							/* Variable name, not a constant */` |
|      48 |  4572 | `							pLast->iP1 = 0;` |
|      23 |  4573 | `						}` |
|      48 |  4574 | `						nExpr++;` |
|      23 |  4575 | `					}` |
|       - |  4576 | `				}` |
|       - |  4577 | `			}` |
|      23 |  4578 | `		}` |
|       - |  4579 | `		/* Next expression in the stream */` |
|      48 |  4580 | `		pGen->pIn = pNext;` |
|       - |  4581 | `		/* Jump trailing commas */` |
|      58 |  4582 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      12 |  4583 | `			pGen->pIn++;` |
|       2 |  4584 | `		}` |
|       2 |  4585 | `	}` |
|       - |  4586 | `	/* Restore token stream */` |
|      38 |  4587 | `	pGen->pEnd = pTmp;` |
|      38 |  4588 | `	if( nExpr > 0 ){` |
|       - |  4589 | `		/* Emit the uplink instruction */` |
|      38 |  4590 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4591 | `	}` |
|      38 |  4592 | `	return SXRET_OK;` |
|      20 |  4593 |  |
|       - |  4594 | `/*` |
|       - |  4595 | ` * Compile the return statement.` |
|       - |  4596 | ` * According to the PHP language reference` |
|       - |  4597 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4598 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4599 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4600 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4601 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4602 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4603 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4604 | ` *  from within the main script file, then script execution end.` |
|       - |  4605 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4606 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4607 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4608 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4609 | ` */` |
|  208774 |  4610 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 |  4611 |  |
|  208776 |  4612 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4613 | `	sxi32 rc;` |
|       - |  4614 | `	/* Jump the 'return' keyword */` |
|  208776 |  4615 | `	pGen->pIn++;` |
|  208776 |  4616 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4617 | `		/* Compile the expression */` |
|  208752 |  4618 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  208752 |  4619 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4620 | `			return SXERR_ABORT;` |
|  208752 |  4621 | `		}else if(rc != SXERR_EMPTY ){` |
|  208752 |  4622 | `			nRet = 1;` |
|  104375 |  4623 | `		}` |
|  104375 |  4624 | `	}` |
|       - |  4625 | `	/* Emit the done instruction */` |
|  208776 |  4626 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  208776 |  4627 | `	return SXRET_OK;` |
|  104389 |  4628 |  |
|       - |  4629 | `/*` |
|       - |  4630 | ` * Compile a yield expression.` |
|       - |  4631 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4632 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4633 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4634 | ` */` |
|      34 |  4635 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  4636 |  |
|       - |  4637 | `	SyToken *pTmp, *pSplit;` |
|      36 |  4638 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      36 |  4639 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4640 | `	sxi32 rc;` |
|      17 |  4641 | `	(void)iCompileFlag;` |
|       - |  4642 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      36 |  4643 | `	pGen->pIn++;` |
|       - |  4644 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4645 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      36 |  4646 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4647 | `		/* Bare yield — no value */` |
|     ! 0 |  4648 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4649 | `		return SXRET_OK;` |
|       - |  4650 | `	}` |
|       - |  4651 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      36 |  4652 | `	pSplit = 0;` |
|       - |  4653 | `	{` |
|      36 |  4654 | `		SyToken *pCur = pGen->pIn;` |
|      36 |  4655 | `		sxi32 nNest = 0;` |
|      84 |  4656 | `		while( pCur < pGen->pEnd ){` |
|      56 |  4657 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4658 | `				nNest++;` |
|      56 |  4659 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4660 | `				nNest--;` |
|      56 |  4661 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 |  4662 | `				pSplit = pCur;` |
|       7 |  4663 | `				break;` |
|       - |  4664 | `			}` |
|      50 |  4665 | `			pCur++;` |
|       2 |  4666 | `		}` |
|       - |  4667 | `	}` |
|      36 |  4668 | `	pTmp = pGen->pEnd;` |
|      36 |  4669 | `	if( pSplit ){` |
|       - |  4670 | `		/* yield $key => $value */` |
|       7 |  4671 | `		pGen->pEnd = pSplit;` |
|       7 |  4672 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4673 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4674 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 |  4675 | `		pGen->pEnd = pTmp;` |
|       7 |  4676 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4677 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4678 | `		iP1 = 1;` |
|       7 |  4679 | `		iP2 = 1;` |
|       4 |  4680 | `	}else{` |
|       - |  4681 | `		/* yield $value */` |
|      30 |  4682 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      30 |  4683 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      30 |  4684 | `		if( rc != SXERR_EMPTY ){` |
|      30 |  4685 | `			iP1 = 1;` |
|      14 |  4686 | `		}` |
|       - |  4687 | `	}` |
|      36 |  4688 | `	pGen->pEnd = pTmp;` |
|      36 |  4689 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      36 |  4690 | `	return SXRET_OK;` |
|      19 |  4691 |  |
|       - |  4692 | `/*` |
|       - |  4693 | ` * Compile the die/exit language construct.` |
|       - |  4694 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4695 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4696 | ` */` |
|     112 |  4697 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 |  4698 |  |
|     114 |  4699 | `	sxi32 nExpr = 0;` |
|       - |  4700 | `	sxi32 rc;` |
|       - |  4701 | `	/* Jump the die/exit keyword */` |
|     114 |  4702 | `	pGen->pIn++;` |
|     114 |  4703 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4704 | `		/* Compile the expression */` |
|     114 |  4705 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     114 |  4706 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4707 | `			return SXERR_ABORT;` |
|     114 |  4708 | `		}else if(rc != SXERR_EMPTY ){` |
|     114 |  4709 | `			nExpr = 1;` |
|      56 |  4710 | `		}` |
|      56 |  4711 | `	}` |
|       - |  4712 | `	/* Emit the HALT instruction */` |
|     114 |  4713 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     114 |  4714 | `	return SXRET_OK;` |
|      58 |  4715 |  |
|       - |  4716 | `/*` |
|       - |  4717 | ` * Compile the 'echo' language construct.` |
|       - |  4718 | ` */` |
|   13206 |  4719 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 |  4720 |  |
|   13208 |  4721 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4722 | `	sxi32 rc;` |
|       - |  4723 | `	/* Jump the 'echo' keyword */` |
|   13208 |  4724 | `	pGen->pIn++;` |
|       - |  4725 | `	/* Compile arguments one after one */` |
|   13208 |  4726 | `	pTmp = pGen->pEnd;` |
|   28534 |  4727 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   15328 |  4728 | `		if( pGen->pIn < pNext ){` |
|   15328 |  4729 | `			pGen->pEnd = pNext;` |
|   15328 |  4730 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   15328 |  4731 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4732 | `				return SXERR_ABORT;` |
|   15328 |  4733 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4734 | `				/* Emit the consume instruction */` |
|   15304 |  4735 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    7651 |  4736 | `			}` |
|    7663 |  4737 | `		}` |
|       - |  4738 | `		/* Jump trailing commas */` |
|   17448 |  4739 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2122 |  4740 | `			pNext++;` |
|       2 |  4741 | `		}` |
|   15328 |  4742 | `		pGen->pIn = pNext;` |
|       2 |  4743 | `	}` |
|       - |  4744 | `	/* Restore token stream */` |
|   13208 |  4745 | `	pGen->pEnd = pTmp;` |
|   13208 |  4746 | `	return SXRET_OK;` |
|    6605 |  4747 |  |
|       - |  4748 | `/*` |
|       - |  4749 | ` * Compile the static statement.` |
|       - |  4750 | ` * According to the PHP language reference` |
|       - |  4751 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4752 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4753 | ` *  when program execution leaves this scope.` |
|       - |  4754 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4755 | ` * Symisc eXtension.` |
|       - |  4756 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4757 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4758 | ` *  Example` |
|       - |  4759 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4760 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4761 | ` */` |
|       4 |  4762 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 |  4763 |  |
|       - |  4764 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4765 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4766 | `	GenBlock *pBlock;` |
|       - |  4767 | `	SyString *pName;` |
|       - |  4768 | `	char *zDup;` |
|       - |  4769 | `	sxu32 nLine;` |
|       - |  4770 | `	sxi32 rc;` |
|       - |  4771 | `	/* Jump the static keyword */` |
|       5 |  4772 | `	nLine = pGen->pIn->nLine;` |
|       5 |  4773 | `	pGen->pIn++;` |
|       - |  4774 | `	/* Extract the enclosing function if any */` |
|       5 |  4775 | `	pBlock = pGen->pCurrent;` |
|       9 |  4776 | `	while( pBlock ){` |
|       9 |  4777 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       5 |  4778 | `			break;` |
|       - |  4779 | `		}` |
|       - |  4780 | `		/* Point to the upper block */` |
|       5 |  4781 | `		pBlock = pBlock->pParent;` |
|       1 |  4782 | `	}` |
|       5 |  4783 | `	if( pBlock == 0 ){` |
|       - |  4784 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4785 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4786 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4787 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4788 | `				return SXERR_ABORT;` |
|       - |  4789 | `			}` |
|     ! 0 |  4790 | `			goto Synchronize;` |
|       - |  4791 | `		}` |
|       - |  4792 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4793 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4794 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4795 | `			return SXERR_ABORT;` |
|     ! 0 |  4796 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4797 | `			/* Emit the POP instruction */` |
|     ! 0 |  4798 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4799 | `		}` |
|     ! 0 |  4800 | `		return SXRET_OK;` |
|       - |  4801 | `	}` |
|       5 |  4802 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4803 | `	/* Make sure we are dealing with a valid statement */` |
|       5 |  4804 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       2 |  4805 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4806 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4807 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4808 | `				return SXERR_ABORT;` |
|       - |  4809 | `			}` |
|       3 |  4810 | `			goto Synchronize;` |
|       - |  4811 | `	}` |
|       2 |  4812 | `	pGen->pIn++;` |
|       - |  4813 | `	/* Extract variable name */` |
|       2 |  4814 | `	pName = &pGen->pIn->sData;` |
|       2 |  4815 | `	pGen->pIn++; /* Jump the var name */` |
|       2 |  4816 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4817 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4818 | `		goto Synchronize;` |
|       - |  4819 | `	}` |
|       - |  4820 | `	/* Initialize the structure describing the static variable */` |
|       2 |  4821 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       2 |  4822 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4823 | `	/* Duplicate variable name */` |
|       2 |  4824 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       2 |  4825 | `	if( zDup == 0 ){` |
|     ! 0 |  4826 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4827 | `		return SXERR_ABORT;` |
|       - |  4828 | `	}` |
|       2 |  4829 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4830 | `	/* Check if we have an expression to compile */` |
|       2 |  4831 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4832 | `		SySet *pInstrContainer;` |
|       - |  4833 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4834 | `		 * Static variable can take any complex expression including function` |
|       - |  4835 | `		 * call as their initialization value.` |
|       - |  4836 | `		 * Example:` |
|       - |  4837 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4838 | `		 */` |
|       2 |  4839 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4840 | `		/* Swap bytecode container */` |
|       2 |  4841 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       2 |  4842 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4843 | `		/* Compile the expression */` |
|       2 |  4844 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4845 | `		/* Emit the done instruction */` |
|       2 |  4846 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4847 | `		/* Restore default bytecode container */` |
|       2 |  4848 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       1 |  4849 | `	}` |
|       - |  4850 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       2 |  4851 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       2 |  4852 | `	return SXRET_OK;` |
|       1 |  4853 | `Synchronize:` |
|       - |  4854 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4855 | `	 * statement.` |
|       - |  4856 | `	 */` |
|       5 |  4857 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4858 | `		pGen->pIn++;` |
|       1 |  4859 | `	}` |
|       3 |  4860 | `	return SXRET_OK;` |
|       3 |  4861 |  |
|       - |  4862 | `/*` |
|       - |  4863 | ` * Compile the var statement.` |
|       - |  4864 | ` * Symisc Extension:` |
|       - |  4865 | ` *      var statement can be used outside of a class definition.` |
|       - |  4866 | ` */` |
|       4 |  4867 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4868 |  |
|       - |  4869 | `	sxu32 nLine;` |
|       - |  4870 | `	sxi32 rc;` |
|       5 |  4871 | `	nLine = pGen->pIn->nLine;` |
|       - |  4872 | `	/* Jump the 'var' keyword */` |
|       5 |  4873 | `	pGen->pIn++;` |
|       5 |  4874 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4875 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4876 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4877 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4878 | `			pGen->pIn++;` |
|     ! 0 |  4879 | `		}` |
|     ! 0 |  4880 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4881 | `			return SXERR_ABORT;` |
|       - |  4882 | `		}` |
|     ! 0 |  4883 | `	}else{` |
|       - |  4884 | `		/* Compile the expression */` |
|       5 |  4885 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4886 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4887 | `			return SXERR_ABORT;` |
|       5 |  4888 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4889 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4890 | `		}` |
|       - |  4891 | `	}` |
|       5 |  4892 | `	return SXRET_OK;` |
|       3 |  4893 |  |
|       - |  4894 | `/*` |
|       - |  4895 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4896 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4897 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4898 | ` */` |
|       - |  4899 | `/*` |
|       - |  4900 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4901 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4902 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4903 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4904 | ` *` |
|       - |  4905 | ` * Resolution order:` |
|       - |  4906 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4907 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4908 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4909 | ` *` |
|       - |  4910 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4911 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4912 | ` * Returns the (possibly new) literal index.` |
|       - |  4913 | ` */` |
|  389774 |  4914 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 |  4915 |  |
|       - |  4916 | `	ph7_value *pLit;` |
|       - |  4917 | `	const char *zLit;` |
|       - |  4918 | `	SyString sQualified;` |
|       - |  4919 | `	sxu32 nLit;` |
|       - |  4920 | `	sxu32 k;` |
|       - |  4921 | `	sxu32 nNewIdx;` |
|       - |  4922 | `	int hasNsSep;` |
|       - |  4923 | `	SyHashEntry *pImport;` |
|       - |  4924 | `	ph7_value *pNew;` |
|  389776 |  4925 | `	if( pFromImport ){` |
|  372718 |  4926 | `		*pFromImport = 0;` |
|  186358 |  4927 | `	}` |
|  389776 |  4928 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  389776 |  4929 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4930 | `		return nOrigIdx;` |
|       - |  4931 | `	}` |
|  389776 |  4932 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  389776 |  4933 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4934 | `	/* Skip if already qualified (contains backslash) */` |
|  389776 |  4935 | `	hasNsSep = 0;` |
| 4217072 |  4936 | `	for( k = 0; k < nLit; k++ ){` |
| 3827306 |  4937 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1913650 |  4938 | `	}` |
|  389776 |  4939 | `	if( hasNsSep ){` |
|       9 |  4940 | `		return nOrigIdx;` |
|       - |  4941 | `	}` |
|       - |  4942 | `	/* Check use imports first (works even outside namespaces) */` |
|  389768 |  4943 | `	SyBlobReset(&pGen->sWorker);` |
|  389768 |  4944 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  389768 |  4945 | `	if( pImport ){` |
|      38 |  4946 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 |  4947 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 |  4948 | `		if( pFromImport ){` |
|      18 |  4949 | `			*pFromImport = 1;` |
|       8 |  4950 | `		}` |
|      20 |  4951 | `	}else{` |
|  389732 |  4952 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  389642 |  4953 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4954 | `		}` |
|       - |  4955 | `		/* Prepend current namespace */` |
|      92 |  4956 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      92 |  4957 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      92 |  4958 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4959 | `	}` |
|       - |  4960 | `	/* Look up or create a new literal for the qualified name */` |
|     128 |  4961 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     128 |  4962 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      54 |  4963 | `		return nNewIdx; /* Already interned */` |
|       - |  4964 | `	}` |
|      76 |  4965 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      76 |  4966 | `	if( pNew == 0 ){` |
|     ! 0 |  4967 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4968 | `	}` |
|      76 |  4969 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      76 |  4970 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      76 |  4971 | `	return nNewIdx;` |
|  194889 |  4972 |  |
|       - |  4973 | `/*` |
|       - |  4974 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4975 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4976 | ` */` |
|   85652 |  4977 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4978 |  |
|       - |  4979 | `	SyHashEntry *pImport;` |
|       - |  4980 | `	/* Check use imports first */` |
|   85654 |  4981 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   85654 |  4982 | `	if( pImport ){` |
|      14 |  4983 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  4984 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  4985 | `		return;` |
|       - |  4986 | `	}` |
|       - |  4987 | `	/* Prepend current namespace if active */` |
|   85642 |  4988 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4989 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4990 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4991 | `	}` |
|   85642 |  4992 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   42828 |  4993 |  |
|       - |  4994 | `/*` |
|       - |  4995 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4996 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4997 | ` * The caller must release pOut when done.` |
|       - |  4998 | ` */` |
|  120672 |  4999 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  5000 |  |
|  120674 |  5001 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      60 |  5002 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      60 |  5003 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5004 | `	}` |
|  120674 |  5005 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  120674 |  5006 |  |
|       - |  5007 | `/*` |
|       - |  5008 | ` * Compile a namespace statement` |
|       - |  5009 | ` * According to the PHP language reference manual` |
|       - |  5010 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5011 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5012 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5013 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5014 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5015 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5016 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5017 | ` *  programming world.` |
|       - |  5018 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5019 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5020 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5021 | ` *  classes/functions/constants.` |
|       - |  5022 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5023 | ` *  readability of source code.` |
|       - |  5024 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5025 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5026 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5027 | ` *       class MyClass {}` |
|       - |  5028 | ` *       function myfunction() {}` |
|       - |  5029 | ` *       const MYCONST = 1;` |
|       - |  5030 | ` *       $a = new MyClass;` |
|       - |  5031 | ` *       $c = new \my\name\MyClass;` |
|       - |  5032 | ` *       $a = strlen('hi');` |
|       - |  5033 | ` *       $d = namespace\MYCONST;` |
|       - |  5034 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5035 | ` *       echo constant($d);` |
|       - |  5036 | ` * NOTE` |
|       - |  5037 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5038 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5039 | ` */` |
|       - |  5040 | `/*` |
|       - |  5041 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5042 | ` */` |
|      14 |  5043 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 |  5044 |  |
|      15 |  5045 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       9 |  5046 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       9 |  5047 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       9 |  5048 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       9 |  5049 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       9 |  5050 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5051 | `	return "token";` |
|       8 |  5052 |  |
|     106 |  5053 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 |  5054 |  |
|       - |  5055 | `	sxu32 nLine;` |
|       - |  5056 | `	sxi32 rc;` |
|     108 |  5057 | `	nLine = pGen->pIn->nLine;` |
|     108 |  5058 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5059 | `	/* Reset namespace and clear previous use imports */` |
|     108 |  5060 | `	SyBlobReset(&pGen->sNamespace);` |
|     108 |  5061 | `	SyHashRelease(&pGen->hUseImports);` |
|     108 |  5062 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     108 |  5063 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     108 |  5064 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     108 |  5065 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     108 |  5066 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     108 |  5067 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5068 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5069 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5070 | `		return SXRET_OK;` |
|       - |  5071 | `	}` |
|     108 |  5072 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5073 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5074 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5075 | `		return SXRET_OK;` |
|       - |  5076 | `	}` |
|     108 |  5077 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5078 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5079 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5080 | `		return SXRET_OK;` |
|       - |  5081 | `	}` |
|       - |  5082 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     256 |  5083 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     150 |  5084 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5085 | `			/* Append backslash separator */` |
|      24 |  5086 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      24 |  5087 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5088 | `			}` |
|      13 |  5089 | `		}else{` |
|       - |  5090 | `			/* Append identifier */` |
|     128 |  5091 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5092 | `		}` |
|     150 |  5093 | `		pGen->pIn++;` |
|       2 |  5094 | `	}` |
|       - |  5095 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5096 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5097 | `	{` |
|     108 |  5098 | `		char *zNsDup = 0;` |
|     108 |  5099 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     158 |  5100 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5101 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5102 | `		}` |
|     108 |  5103 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5104 | `	}` |
|     108 |  5105 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 |  5106 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5107 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5108 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 |  5109 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5110 | `			return SXERR_ABORT;` |
|       - |  5111 | `		}` |
|       2 |  5112 | `	}` |
|     108 |  5113 | `	return SXRET_OK;` |
|      55 |  5114 |  |
|       - |  5115 | `/*` |
|       - |  5116 | ` * Compile the 'use' statement` |
|       - |  5117 | ` * According to the PHP language reference manual` |
|       - |  5118 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5119 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5120 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5121 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5122 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5123 | ` *  a function or constant is not supported.` |
|       - |  5124 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5125 | ` * NOTE` |
|       - |  5126 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5127 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5128 | ` */` |
|      68 |  5129 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 |  5130 |  |
|       - |  5131 | `	sxu32 nLine;` |
|       - |  5132 | `	sxi32 rc;` |
|       - |  5133 | `	SyBlob sPath;` |
|       - |  5134 | `	SyString sAlias;` |
|       - |  5135 | `	SyToken *pLast;` |
|       - |  5136 | `	char *zDup;` |
|       - |  5137 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5138 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5139 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      70 |  5140 | `	nLine = pGen->pIn->nLine;` |
|      70 |  5141 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5142 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      70 |  5143 | `	iUseType = 0;` |
|      70 |  5144 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5145 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5146 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5147 | `			iUseType = 1;` |
|      16 |  5148 | `			pGen->pIn++;` |
|      23 |  5149 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5150 | `			iUseType = 2;` |
|      16 |  5151 | `			pGen->pIn++;` |
|       7 |  5152 | `		}` |
|      14 |  5153 | `	}` |
|       - |  5154 | `	/* Select target hash tables based on import type */` |
|      70 |  5155 | `	switch( iUseType ){` |
|       7 |  5156 | `		case 1:` |
|      16 |  5157 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5158 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5159 | `			break;` |
|       7 |  5160 | `		case 2:` |
|      16 |  5161 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5162 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5163 | `			break;` |
|      20 |  5164 | `		default:` |
|      42 |  5165 | `			pGenHash = &pGen->hUseImports;` |
|      42 |  5166 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5167 | `			break;` |
|       - |  5168 | `	}` |
|      70 |  5169 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5170 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5171 | `	for(;;){` |
|      72 |  5172 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5173 | `			break;` |
|       - |  5174 | `		}` |
|      72 |  5175 | `		SyBlobReset(&sPath);` |
|      72 |  5176 | `		pLast = 0;` |
|       - |  5177 | `		/* Collect the full namespace path */` |
|     258 |  5178 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     188 |  5179 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     128 |  5180 | `				pLast = pGen->pIn;` |
|     128 |  5181 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 |  5182 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5183 | `				}` |
|     128 |  5184 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5185 | `			}` |
|     188 |  5186 | `			pGen->pIn++;` |
|       2 |  5187 | `		}` |
|      72 |  5188 | `		if( pLast == 0 ){` |
|       - |  5189 | `			/* Empty path */` |
|       5 |  5190 | `			break;` |
|       - |  5191 | `		}` |
|       - |  5192 | `		/* Default alias is the last component of the path */` |
|      68 |  5193 | `		sAlias = pLast->sData;` |
|       - |  5194 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5195 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      43 |  5196 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  5197 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  5198 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  5199 | `				sAlias = pGen->pIn->sData;` |
|      18 |  5200 | `				pGen->pIn++;` |
|       8 |  5201 | `			}` |
|       8 |  5202 | `		}` |
|       - |  5203 | `		/* Check for duplicate import alias (per-type) */` |
|      68 |  5204 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 |  5205 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5206 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5207 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 |  5208 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5209 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5210 | `				return SXERR_ABORT;` |
|       - |  5211 | `			}` |
|       2 |  5212 | `		}` |
|       - |  5213 | `		/* Register the import: alias -> FQN.` |
|       - |  5214 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5215 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5216 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     101 |  5217 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5218 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      68 |  5219 | `		if( zDup ){` |
|      68 |  5220 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      68 |  5221 | `			if( pVmHash ){` |
|       - |  5222 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5223 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      40 |  5224 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      40 |  5225 | `				if( zAliasDup ){` |
|      40 |  5226 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5227 | `				}` |
|      19 |  5228 | `			}` |
|      68 |  5229 | `			if( iUseType == 2 ){` |
|       - |  5230 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5231 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5232 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5233 | `				if( zAliasDup ){` |
|       - |  5234 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5235 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5236 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5237 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5238 | `					if( azPair ){` |
|      16 |  5239 | `						azPair[0] = zAliasDup;` |
|      16 |  5240 | `						azPair[1] = zDup;` |
|      16 |  5241 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5242 | `					}` |
|       7 |  5243 | `				}` |
|       7 |  5244 | `			}` |
|      33 |  5245 | `		}` |
|       - |  5246 | `		/* Check for comma (multiple use declarations) */` |
|      68 |  5247 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5248 | `			pGen->pIn++;` |
|       2 |  5249 | `		}else{` |
|      34 |  5250 | `			break;` |
|       - |  5251 | `		}` |
|       1 |  5252 | `	}` |
|      70 |  5253 | `	SyBlobRelease(&sPath);` |
|      70 |  5254 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5255 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5256 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5257 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5258 | `			return SXERR_ABORT;` |
|       - |  5259 | `		}` |
|       1 |  5260 | `	}` |
|      70 |  5261 | `	return SXRET_OK;` |
|      36 |  5262 |  |
|       - |  5263 | `/*` |
|       - |  5264 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5265 | ` *` |
|       - |  5266 | ` * According to the PHP language reference manual.` |
|       - |  5267 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5268 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5269 | ` *  declare (directive)` |
|       - |  5270 | ` *   statement` |
|       - |  5271 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5272 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5273 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5274 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5275 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5276 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5277 | ` * <?php` |
|       - |  5278 | ` * // these are the same:` |
|       - |  5279 | ` * // you can use this:` |
|       - |  5280 | ` * declare(ticks=1) {` |
|       - |  5281 | ` *   // entire script here` |
|       - |  5282 | ` * }` |
|       - |  5283 | ` * // or you can use this:` |
|       - |  5284 | ` * declare(ticks=1);` |
|       - |  5285 | ` * // entire script here` |
|       - |  5286 | ` * ?>` |
|       - |  5287 | ` *` |
|       - |  5288 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5289 | ` */` |
|       - |  5290 | `/*` |
|       - |  5291 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5292 | ` */` |
|      68 |  5293 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       2 |  5294 |  |
|     100 |  5295 | `	return SyStringLength(pName) == nWant` |
|      68 |  5296 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       2 |  5297 |  |
|       - |  5298 |  |
|      40 |  5299 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       2 |  5300 |  |
|      42 |  5301 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      42 |  5302 | `	SyToken *pBodyEnd = 0;` |
|       - |  5303 | `	SyToken *pBodyStart;` |
|       - |  5304 | `	SyToken *pCursor;` |
|       - |  5305 | `	int bHasStrictTypes;` |
|       - |  5306 | `	int bBlockForm;` |
|       - |  5307 | `	int bPlacementOk;` |
|       - |  5308 | `	sxi32 rc;` |
|      42 |  5309 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      42 |  5310 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5311 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5312 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5313 | `			return SXERR_ABORT;` |
|       - |  5314 | `		}` |
|       5 |  5315 | `		goto Synchro;` |
|       - |  5316 | `	}` |
|      38 |  5317 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      38 |  5318 | `	pBodyStart = pGen->pIn;` |
|       - |  5319 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      38 |  5320 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      38 |  5321 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5322 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5323 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5324 | `			return SXERR_ABORT;` |
|       - |  5325 | `		}` |
|     ! 0 |  5326 | `		return SXRET_OK;` |
|       - |  5327 | `	}` |
|       - |  5328 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5329 | `	 * now delimits the comma-separated directive list. */` |
|      38 |  5330 | `	pGen->pIn = &pBodyEnd[1];` |
|      38 |  5331 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5332 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5333 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5334 | `			return SXERR_ABORT;` |
|       - |  5335 | `		}` |
|     ! 0 |  5336 | `	}` |
|      38 |  5337 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      38 |  5338 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      38 |  5339 | `	bHasStrictTypes = 0;` |
|       - |  5340 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5341 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5342 | `	 * directive appears anywhere in the list, before validating values. */` |
|      38 |  5343 | `	pCursor = pBodyStart;` |
|      50 |  5344 | `	while( pCursor < pBodyEnd ){` |
|      46 |  5345 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      38 |  5346 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      34 |  5347 | `				bHasStrictTypes = 1;` |
|      34 |  5348 | `				break;` |
|       - |  5349 | `			}` |
|       2 |  5350 | `		}` |
|      13 |  5351 | `		pCursor++;` |
|       1 |  5352 | `	}` |
|      38 |  5353 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5354 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5355 | `			"strict_types declaration must not use block mode");` |
|       3 |  5356 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5357 | `		return SXRET_OK;` |
|       - |  5358 | `	}` |
|      36 |  5359 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       5 |  5360 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5361 | `			"strict_types declaration must be the very first statement in the script");` |
|       5 |  5362 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       5 |  5363 | `		return SXRET_OK;` |
|       - |  5364 | `	}` |
|       - |  5365 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      32 |  5366 | `	pCursor = pBodyStart;` |
|      62 |  5367 | `	while( pCursor < pBodyEnd ){` |
|       - |  5368 | `		SyToken *pNameTok;` |
|       - |  5369 | `		SyToken *pEqTok;` |
|       - |  5370 | `		SyToken *pValTok;` |
|       - |  5371 | `		SyString *pDirName;` |
|       - |  5372 | `		int bIsStrict;` |
|       - |  5373 | `		int iStrictValue;` |
|      34 |  5374 | `		pNameTok = pCursor;` |
|      34 |  5375 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5376 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5377 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5378 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5379 | `			return SXRET_OK;` |
|       - |  5380 | `		}` |
|      34 |  5381 | `		pEqTok = pNameTok + 1;` |
|      34 |  5382 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5383 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5384 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5385 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5386 | `			return SXRET_OK;` |
|       - |  5387 | `		}` |
|      34 |  5388 | `		pValTok = pEqTok + 1;` |
|      34 |  5389 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5390 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5391 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5392 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5393 | `			return SXRET_OK;` |
|       - |  5394 | `		}` |
|      34 |  5395 | `		pDirName = &pNameTok->sData;` |
|      34 |  5396 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      34 |  5397 | `		if( bIsStrict ){` |
|       - |  5398 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5399 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      30 |  5400 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5401 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5402 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5403 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5404 | `				return SXRET_OK;` |
|       - |  5405 | `			}` |
|      30 |  5406 | `			iStrictValue = -1;` |
|      30 |  5407 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      30 |  5408 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      30 |  5409 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      30 |  5410 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      28 |  5411 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5412 | `			}` |
|      30 |  5413 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5414 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5415 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5416 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5417 | `				return SXRET_OK;` |
|       - |  5418 | `			}` |
|      28 |  5419 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      15 |  5420 | `		}else{` |
|       - |  5421 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5422 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5423 | `			 * behavior don't regress. */` |
|       7 |  5424 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5425 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5426 | `				ph7_lib_version()` |
|       - |  5427 | `				);` |
|       - |  5428 | `		}` |
|      32 |  5429 | `		pCursor = pValTok + 1;` |
|       - |  5430 | `		/* Consume separating comma (or end). */` |
|      32 |  5431 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5432 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5433 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5434 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5435 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5436 | `				return SXRET_OK;` |
|       - |  5437 | `			}` |
|       3 |  5438 | `			pCursor++;` |
|       1 |  5439 | `		}` |
|       2 |  5440 | `	}` |
|       - |  5441 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5442 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5443 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      30 |  5444 | `	return SXRET_OK;` |
|       2 |  5445 | `Synchro:` |
|       - |  5446 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5447 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5448 | `		pGen->pIn++;` |
|       1 |  5449 | `	}` |
|       5 |  5450 | `	return SXRET_OK;` |
|      22 |  5451 |  |
|       - |  5452 | `/*` |
|       - |  5453 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5454 | ` * as follows:` |
|       - |  5455 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5456 | ` * {` |
|       - |  5457 | ` *   return "Making a cup of $type.\n";` |
|       - |  5458 | ` * }` |
|       - |  5459 | ` * Symisc eXtension.` |
|       - |  5460 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5461 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5462 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5463 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5464 | ` *      {` |
|       - |  5465 | ` *       var_dump($a);` |
|       - |  5466 | ` *      }` |
|       - |  5467 | ` *     //call test without args` |
|       - |  5468 | ` *      test();` |
|       - |  5469 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5470 | ` *      Example:` |
|       - |  5471 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5472 | ` * 3 -) Function overloading!!` |
|       - |  5473 | ` *      Example:` |
|       - |  5474 | ` *      function foo($a) {` |
|       - |  5475 | ` *   	  return $a.PHP_EOL;` |
|       - |  5476 | ` *	    }` |
|       - |  5477 | ` *	    function foo($a, $b) {` |
|       - |  5478 | ` *   	  return $a + $b;` |
|       - |  5479 | ` *	    }` |
|       - |  5480 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5481 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5482 | ` *      // Same arg` |
|       - |  5483 | ` *	   function foo(string $a)` |
|       - |  5484 | ` *	   {` |
|       - |  5485 | ` *	     echo "a is a string\n";` |
|       - |  5486 | ` *	     var_dump($a);` |
|       - |  5487 | ` *	   }` |
|       - |  5488 | ` *	  function foo(int $a)` |
|       - |  5489 | ` *	  {` |
|       - |  5490 | ` *	    echo "a is integer\n";` |
|       - |  5491 | ` *	    var_dump($a);` |
|       - |  5492 | ` *	  }` |
|       - |  5493 | ` *	  function foo(array $a)` |
|       - |  5494 | ` *	  {` |
|       - |  5495 | ` * 	    echo "a is an array\n";` |
|       - |  5496 | ` * 	    var_dump($a);` |
|       - |  5497 | ` *	  }` |
|       - |  5498 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5499 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5500 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5501 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5502 | ` * introduced by the PH7 engine.` |
|       - |  5503 | ` */` |
|   59846 |  5504 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 |  5505 |  |
|       - |  5506 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5507 | `	SySet *pInstrContainer;` |
|       - |  5508 | `	sxi32 rc;` |
|       - |  5509 | `	/* Swap token stream */` |
|   59848 |  5510 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   59848 |  5511 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   59848 |  5512 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5513 | `	/* Compile the expression holding the argument value */` |
|   59848 |  5514 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5515 | `	/* Emit the done instruction */` |
|   59848 |  5516 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   59848 |  5517 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   59848 |  5518 | `	RE_SWAP_DELIMITER(pGen);` |
|   59848 |  5519 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5520 | `		return SXERR_ABORT;` |
|       - |  5521 | `	}` |
|   59848 |  5522 | `	return SXRET_OK;` |
|   29925 |  5523 |  |
|       - |  5524 | `/*` |
|       - |  5525 | ` * Collect function arguments one after one.` |
|       - |  5526 | ` * According to the PHP language reference manual.` |
|       - |  5527 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5528 | ` * list of expressions.` |
|       - |  5529 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5530 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5531 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5532 | ` * for more information.` |
|       - |  5533 | ` * Example #1 Passing arrays to functions` |
|       - |  5534 | ` * <?php` |
|       - |  5535 | ` * function takes_array($input)` |
|       - |  5536 | ` * {` |
|       - |  5537 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5538 | ` * }` |
|       - |  5539 | ` * ?>` |
|       - |  5540 | ` * Making arguments be passed by reference` |
|       - |  5541 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5542 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5543 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5544 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5545 | ` * to the argument name in the function definition:` |
|       - |  5546 | ` * Example #2 Passing function parameters by reference` |
|       - |  5547 | ` * <?php` |
|       - |  5548 | ` * function add_some_extra(&$string)` |
|       - |  5549 | ` * {` |
|       - |  5550 | ` *   $string .= 'and something extra.';` |
|       - |  5551 | ` * }` |
|       - |  5552 | ` * $str = 'This is a string, ';` |
|       - |  5553 | ` * add_some_extra($str);` |
|       - |  5554 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5555 | ` * ?>` |
|       - |  5556 | ` *` |
|       - |  5557 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5558 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5559 | ` * on these extension.` |
|       - |  5560 | ` */` |
|   82984 |  5561 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       2 |  5562 |  |
|       - |  5563 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5564 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5565 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5566 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5567 | `	sxi32 rc;` |
|       - |  5568 |  |
|   82986 |  5569 | `	pIn = pGen->pIn;` |
|   82986 |  5570 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5571 | `	/* Process arguments one after one */` |
|  103796 |  5572 | `	for(;;){` |
|  207594 |  5573 | `		if( pIn >= pEnd ){` |
|       - |  5574 | `			/* No more arguments to process */` |
|   82974 |  5575 | `			break;` |
|       - |  5576 | `		}` |
|  124622 |  5577 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  124622 |  5578 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  124622 |  5579 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  124622 |  5580 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5581 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|  124622 |  5582 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   56824 |  5583 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   56824 |  5584 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      44 |  5585 | `				if( !bCtorCtx ){` |
|       5 |  5586 | `					if( bAbstractCtx ){` |
|       3 |  5587 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5588 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5589 | `					}else{` |
|       3 |  5590 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5591 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5592 | `					}` |
|       5 |  5593 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5594 | `						return SXERR_ABORT;` |
|       - |  5595 | `					}` |
|       5 |  5596 | `					return SXERR_SYNTAX;` |
|       - |  5597 | `				}` |
|      40 |  5598 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      40 |  5599 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5600 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      39 |  5601 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5602 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5603 | `				}else{` |
|      36 |  5604 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5605 | `				}` |
|      40 |  5606 | `				pIn++;` |
|      19 |  5607 | `			}` |
|   28409 |  5608 | `		}` |
|       - |  5609 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  157776 |  5610 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   97055 |  5611 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   67909 |  5612 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   66302 |  5613 | `			sxu32 nLineLocal = pIn->nLine;` |
|   66302 |  5614 | `			sxi32 iTFlags = 0;` |
|   66302 |  5615 | `			pGen->pIn = pIn;` |
|   66302 |  5616 | `			rc = GenStateParseUnionTypeDecl(` |
|   33150 |  5617 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   33150 |  5618 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5619 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5620 | `				/* bAllowVoid */ 0,` |
|   33150 |  5621 | `						nLineLocal);` |
|   66302 |  5622 | `			pIn = pGen->pIn;` |
|   66302 |  5623 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5624 | `				return SXERR_ABORT;` |
|   66302 |  5625 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5626 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5627 | `				return SXERR_SYNTAX;` |
|   66300 |  5628 | `			}else if( rc == SXERR_SYNTAX ){` |
|       5 |  5629 | `				if( pIn < pEnd ){` |
|       7 |  5630 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5631 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5632 | `						&pIn->sData);` |
|       3 |  5633 | `				}else{` |
|     ! 0 |  5634 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5635 | `						"syntax error, unexpected end of file");` |
|       - |  5636 | `				}` |
|       5 |  5637 | `				return SXERR_SYNTAX;` |
|       - |  5638 | `			}` |
|   66296 |  5639 | `			sArg.iFlags \|= iTFlags;` |
|   33147 |  5640 | `		}` |
|  124612 |  5641 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5642 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5643 | `			return rc;` |
|       - |  5644 | `		}` |
|  124612 |  5645 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5646 | `			/* Pass by reference,record that */` |
|    3178 |  5647 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3178 |  5648 | `			pIn++;` |
|    1588 |  5649 | `		}` |
|  124612 |  5650 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5651 | `			/* Variadic parameter: ...$args */` |
|      42 |  5652 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      42 |  5653 | `			pIn++;` |
|      20 |  5654 | `		}` |
|  124612 |  5655 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5656 | `			/* Invalid argument */` |
|     ! 0 |  5657 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5658 | `			return rc;` |
|       - |  5659 | `		}` |
|  124612 |  5660 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5661 | `		/* Copy argument name */` |
|  124612 |  5662 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  124612 |  5663 | `		if( zDup == 0 ){` |
|     ! 0 |  5664 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5665 | `			return SXERR_ABORT;` |
|       - |  5666 | `		}` |
|  124612 |  5667 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  124612 |  5668 | `		pIn++;` |
|  124612 |  5669 | `		if( pIn < pEnd ){` |
|   70002 |  5670 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5671 | `				SyToken *pDefend;` |
|   59850 |  5672 | `				sxi32 iNest = 0;` |
|   59850 |  5673 | `				pIn++; /* Jump the equal sign */` |
|   59850 |  5674 | `				pDefend = pIn;` |
|       - |  5675 | `				/* Process the default value associated with this argument */` |
|  125992 |  5676 | `				while( pDefend < pEnd ){` |
|   97632 |  5677 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   31490 |  5678 | `						break;` |
|       - |  5679 | `					}` |
|   66144 |  5680 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5681 | `						/* Increment nesting level */` |
|    3150 |  5682 | `						iNest++;` |
|   64570 |  5683 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5684 | `						/* Decrement nesting level */` |
|    3150 |  5685 | `						iNest--;` |
|    1574 |  5686 | `					}` |
|   66144 |  5687 | `					pDefend++;` |
|       2 |  5688 | `				}` |
|   59850 |  5689 | `				if( pIn >= pDefend ){` |
|       3 |  5690 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5691 | `					return rc;` |
|       - |  5692 | `				}` |
|       - |  5693 | `				/* Process default value */` |
|   59848 |  5694 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   59848 |  5695 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5696 | `					return rc;` |
|       - |  5697 | `				}` |
|       - |  5698 | `				/* Point beyond the default value */` |
|   59848 |  5699 | `				pIn = pDefend;` |
|   29923 |  5700 | `			}` |
|   70000 |  5701 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5702 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5703 | `				return rc;` |
|       - |  5704 | `			}` |
|   70000 |  5705 | `			pIn++; /* Jump the trailing comma */` |
|   34999 |  5706 | `		}` |
|       - |  5707 | `		/* Append argument signature */` |
|  124610 |  5708 | `		if( sArg.nType > 0 ){` |
|   66254 |  5709 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5710 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    9462 |  5711 | `				int marker = 'o';` |
|    9462 |  5712 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    9462 |  5713 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4732 |  5714 | `			}else{` |
|       - |  5715 | `				int c;` |
|   56794 |  5716 | `				c = 'n'; /* cc warning */` |
|       - |  5717 | `				/* Type leading character */` |
|   56794 |  5718 | `				switch(sArg.nType){` |
|     ! 0 |  5719 | `				case MEMOBJ_HASHMAP:` |
|       - |  5720 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5721 | `					c = 'h';` |
|     ! 0 |  5722 | `					break;` |
|    7906 |  5723 | `				case MEMOBJ_INT:` |
|       - |  5724 | `					/* Integer */` |
|   15814 |  5725 | `					c = 'i';` |
|   15814 |  5726 | `					break;` |
|       1 |  5727 | `				case MEMOBJ_BOOL:` |
|       - |  5728 | `					/* Bool */` |
|       3 |  5729 | `					c = 'b';` |
|       3 |  5730 | `					break;` |
|       1 |  5731 | `				case MEMOBJ_REAL:` |
|       - |  5732 | `					/* Float */` |
|       3 |  5733 | `					c = 'f';` |
|       3 |  5734 | `					break;` |
|   20481 |  5735 | `				case MEMOBJ_STRING:` |
|       - |  5736 | `					/* String */` |
|   40964 |  5737 | `					c = 's';` |
|   40964 |  5738 | `					break;` |
|       7 |  5739 | `				case MEMOBJ_OBJ:` |
|       - |  5740 | `					/* Object */` |
|      16 |  5741 | `					c = 'o';` |
|      14 |  5742 | `					break;` |
|     ! 0 |  5743 | `				default:` |
|     ! 0 |  5744 | `					break;` |
|       - |  5745 | `				}` |
|   56794 |  5746 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5747 | `			}` |
|   33128 |  5748 | `		}else{` |
|       - |  5749 | `			/* No type is associated with this parameter which mean` |
|       - |  5750 | `			 * that this function is not condidate for overloading.` |
|       - |  5751 | `			 */` |
|   58358 |  5752 | `			SyBlobRelease(&sSig);` |
|       - |  5753 | `		}` |
|       - |  5754 | `		/* Save in the argument set */` |
|  124610 |  5755 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 |  5756 | `	}` |
|   82974 |  5757 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5758 | `		/* Save function signature */` |
|   41048 |  5759 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   20523 |  5760 | `	}` |
|   82974 |  5761 | `	return SXRET_OK;` |
|   41494 |  5762 |  |
|       - |  5763 | `/*` |
|       - |  5764 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5765 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5766 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5767 | ` */` |
|  196928 |  5768 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5769 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5770 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5771 | `	)` |
|       2 |  5772 |  |
|       - |  5773 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5774 | `	GenBlock *pBlock;` |
|       - |  5775 | `	sxu32 nGotoOfft;` |
|       - |  5776 | `	sxi32 rc;` |
|       - |  5777 | `	/* Attach the new function */` |
|  196930 |  5778 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  196930 |  5779 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5780 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5781 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5782 | `		return SXERR_ABORT;` |
|       - |  5783 | `	}` |
|  196930 |  5784 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5785 | `	/* Swap bytecode containers */` |
|  196930 |  5786 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  196930 |  5787 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5788 | `	/* Emit constructor property promotion prologue:` |
|       - |  5789 | `	 *   $this->NAME = $NAME;` |
|       - |  5790 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5791 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5792 | `	{` |
|  196930 |  5793 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5794 | `		sxu32 i;` |
|  296232 |  5795 | `		for( i = 0; i < nArg; i++ ){` |
|   99304 |  5796 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5797 | `			char *zSrc;` |
|       - |  5798 | `			sxu32 nSrc,nName;` |
|       - |  5799 | `			SySet sToken;` |
|       - |  5800 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5801 | `			sxi32 rcPromote;` |
|   99304 |  5802 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   99274 |  5803 | `				continue;` |
|       - |  5804 | `			}` |
|       - |  5805 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5806 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5807 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5808 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5809 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      32 |  5810 | `			nName = SyStringLength(&pArg->sName);` |
|      32 |  5811 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      32 |  5812 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      32 |  5813 | `			if( zSrc == 0 ){` |
|     ! 0 |  5814 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5815 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5816 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5817 | `				return SXERR_ABORT;` |
|       - |  5818 | `			}` |
|       - |  5819 | `			{` |
|      32 |  5820 | `				char *z = zSrc;` |
|      32 |  5821 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      32 |  5822 | `				z += sizeof("$this->")-1;` |
|      32 |  5823 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      32 |  5824 | `				z += nName;` |
|      32 |  5825 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      32 |  5826 | `				z += sizeof(" = $")-1;` |
|      32 |  5827 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      32 |  5828 | `				z += nName;` |
|      32 |  5829 | `				*z = 0;` |
|       - |  5830 | `			}` |
|      32 |  5831 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      32 |  5832 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      32 |  5833 | `			pTmpIn = pGen->pIn;` |
|      32 |  5834 | `			pTmpEnd = pGen->pEnd;` |
|      32 |  5835 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      32 |  5836 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      32 |  5837 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      32 |  5838 | `			pGen->pIn = pTmpIn;` |
|      32 |  5839 | `			pGen->pEnd = pTmpEnd;` |
|      32 |  5840 | `			SySetRelease(&sToken);` |
|      32 |  5841 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5842 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5843 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5844 | `				return SXERR_ABORT;` |
|       - |  5845 | `			}` |
|       - |  5846 | `			/* Discard the assignment result — this is a statement expression. */` |
|      32 |  5847 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      17 |  5848 | `		}` |
|       - |  5849 | `	}` |
|       - |  5850 | `	/* Compile the body */` |
|  196930 |  5851 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5852 | `	/* Fix exception jumps now the destination is resolved */` |
|  196930 |  5853 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5854 | `	/* Emit the final return if not yet done */` |
|  196930 |  5855 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5856 | `	/* Fix gotos jumps now the destination is resolved */` |
|  196930 |  5857 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5858 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5859 | `	}` |
|  196930 |  5860 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5861 | `	/* Restore the default container */` |
|  196930 |  5862 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5863 | `	/* Leave function block */` |
|  196930 |  5864 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  196930 |  5865 | `	if( rc == SXERR_ABORT ){` |
|       - |  5866 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5867 | `		return SXERR_ABORT;` |
|       - |  5868 | `	}` |
|       - |  5869 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5870 | `	{` |
|  196930 |  5871 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5872 | `		sxu32 i;` |
| 3847596 |  5873 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3650686 |  5874 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      20 |  5875 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      20 |  5876 | `				break;` |
|       - |  5877 | `			}` |
| 1825335 |  5878 | `		}` |
|       - |  5879 | `	}` |
|       - |  5880 | `	/* All done, function body compiled */` |
|  196930 |  5881 | `	return SXRET_OK;` |
|   98466 |  5882 |  |
|       - |  5883 | `/*` |
|       - |  5884 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5885 | ` * According to the PHP language reference manual.` |
|       - |  5886 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5887 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5888 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5889 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5890 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5891 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5892 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5893 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5894 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5895 | ` *` |
|       - |  5896 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5897 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5898 | ` * on these extension.` |
|       - |  5899 | ` */` |
|       - |  5900 | `/*` |
|       - |  5901 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5902 | ` */` |
|     272 |  5903 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 |  5904 |  |
|       - |  5905 | `	sxu32 i;` |
|     786 |  5906 | `	for( i = 0; i < n; i++ ){` |
|     672 |  5907 | `		int a = zA[i], b = zB[i];` |
|     672 |  5908 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     672 |  5909 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     672 |  5910 | `		if( a != b ) return a - b;` |
|     258 |  5911 | `	}` |
|     116 |  5912 | `	return 0;` |
|     138 |  5913 |  |
|       - |  5914 | `/*` |
|       - |  5915 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5916 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5917 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5918 | ` */` |
|       - |  5919 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5920 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5921 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5922 |  |
|       - |  5923 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5924 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5925 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5926 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5927 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5928 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5929 |  |
|       - |  5930 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5931 | `struct PhlTypeAtom {` |
|       - |  5932 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5933 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5934 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5935 | `	sxu32 nCanon;` |
|       - |  5936 | `};` |
|       - |  5937 |  |
|       - |  5938 | `/*` |
|       - |  5939 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5940 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5941 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5942 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5943 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5944 | ` * already be consumed by the caller.` |
|       - |  5945 | ` */` |
|   66822 |  5946 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       2 |  5947 |  |
|   66824 |  5948 | `	SyToken *pIn = pGen->pIn;` |
|   66824 |  5949 | `	SyZero(pOut, sizeof(*pOut));` |
|   66824 |  5950 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   66824 |  5951 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5952 | `		return SXERR_SYNTAX;` |
|       - |  5953 | `	}` |
|       - |  5954 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   66824 |  5955 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5956 | `		pIn++;` |
|       8 |  5957 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5958 | `			return SXERR_SYNTAX;` |
|       - |  5959 | `		}` |
|       3 |  5960 | `	}` |
|   66824 |  5961 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5962 | `		return SXERR_SYNTAX;` |
|       - |  5963 | `	}` |
|   66824 |  5964 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   57188 |  5965 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   57188 |  5966 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      16 |  5967 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   57181 |  5968 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      52 |  5969 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   57149 |  5970 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   15984 |  5971 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   49133 |  5972 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   41088 |  5973 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   20599 |  5974 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      26 |  5975 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      44 |  5976 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      26 |  5977 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      20 |  5978 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  5979 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5980 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5981 | `			pOut->sClass = pIn->sData;` |
|       4 |  5982 | `		}else{` |
|       3 |  5983 | `			return SXERR_SYNTAX;` |
|       - |  5984 | `		}` |
|   57186 |  5985 | `		pIn++;` |
|   28594 |  5986 | `	}else{` |
|       - |  5987 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5988 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    9638 |  5989 | `		SyString *pT = &pIn->sData;` |
|    9638 |  5990 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      12 |  5991 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      12 |  5992 | `			pIn++;` |
|    9633 |  5993 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     100 |  5994 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     100 |  5995 | `			pIn++;` |
|    9579 |  5996 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5997 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5998 | `			pIn++;` |
|       2 |  5999 | `		}else{` |
|       - |  6000 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    9528 |  6001 | `			SyToken *pFirst = pIn;` |
|    9528 |  6002 | `			SyToken *pLast = pIn;` |
|    9528 |  6003 | `			pOut->nType = SXU32_HIGH;` |
|    9528 |  6004 | `			pOut->sClass = pIn->sData;` |
|    9528 |  6005 | `			pIn++;` |
|   14292 |  6006 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    9531 |  6007 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6008 | `				pLast = &pIn[1];` |
|       3 |  6009 | `				pIn += 2;` |
|       1 |  6010 | `			}` |
|    9528 |  6011 | `			if( pLast != pFirst ){` |
|       3 |  6012 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6013 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6014 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6015 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6016 | `			}` |
|       - |  6017 | `		}` |
|       - |  6018 | `	}` |
|   66822 |  6019 | `	pGen->pIn = pIn;` |
|   66822 |  6020 | `	return SXRET_OK;` |
|   33413 |  6021 |  |
|       - |  6022 |  |
|       - |  6023 | `/*` |
|       - |  6024 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6025 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6026 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6027 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6028 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6029 | ` */` |
|   66724 |  6030 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       2 |  6031 |  |
|       - |  6032 | `	int i;` |
|   66726 |  6033 | `	int nNonNull = 0;` |
|  133532 |  6034 | `	for( i = 0; i < nAtoms; i++ ){` |
|   66808 |  6035 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   66798 |  6036 | `			nNonNull++;` |
|   33398 |  6037 | `		}` |
|   33405 |  6038 | `	}` |
|   66726 |  6039 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6040 | `		/* Shorthand: ?T */` |
|      56 |  6041 | `		for( i = 0; i < nAtoms; i++ ){` |
|      56 |  6042 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      56 |  6043 | `			SyBlobAppend(pBlob, "?", 1);` |
|      56 |  6044 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6045 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       7 |  6046 | `			}else{` |
|      46 |  6047 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6048 | `			}` |
|      56 |  6049 | `			return;` |
|     ! 0 |  6050 | `		}` |
|     ! 0 |  6051 | `	}` |
|       - |  6052 | `	{` |
|   66672 |  6053 | `		int bFirst = 1;` |
|       - |  6054 | `		/* 1) Classes in declaration order */` |
|  133418 |  6055 | `		for( i = 0; i < nAtoms; i++ ){` |
|   66748 |  6056 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    9522 |  6057 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    9522 |  6058 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    9522 |  6059 | `				bFirst = 0;` |
|    4760 |  6060 | `			}` |
|   33375 |  6061 | `		}` |
|       - |  6062 | `		/* 2) Built-ins in canonical order */` |
|       - |  6063 | `		{` |
|       - |  6064 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6065 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6066 | `			int k;` |
|  466692 |  6067 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  743280 |  6068 | `				for( i = 0; i < nAtoms; i++ ){` |
|  400386 |  6069 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   57128 |  6070 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   57128 |  6071 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   57128 |  6072 | `						bFirst = 0;` |
|   57128 |  6073 | `						break;` |
|       - |  6074 | `					}` |
|  171631 |  6075 | `				}` |
|  200012 |  6076 | `			}` |
|       - |  6077 | `		}` |
|       - |  6078 | `		/* 3) null suffix */` |
|   66672 |  6079 | `		if( bNullable ){` |
|       6 |  6080 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       6 |  6081 | `			SyBlobAppend(pBlob, "null", 4);` |
|       2 |  6082 | `		}` |
|       - |  6083 | `	}` |
|   33364 |  6084 |  |
|       - |  6085 |  |
|       - |  6086 | `/*` |
|       - |  6087 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6088 | ` *` |
|       - |  6089 | ` * Outputs:` |
|       - |  6090 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6091 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6092 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6093 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6094 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6095 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6096 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6097 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6098 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6099 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6100 | ` *` |
|       - |  6101 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6102 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6103 | ` */` |
|   66734 |  6104 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6105 | `	ph7_gen_state *pGen,` |
|       - |  6106 | `	sxu32 *pnType,` |
|       - |  6107 | `	SyString *pClass,` |
|       - |  6108 | `	SySet *pAlts,` |
|       - |  6109 | `	sxi32 *piTypeFlags,` |
|       - |  6110 | `	SyString *pTypeText,` |
|       - |  6111 | `	int iNullableFlag,` |
|       - |  6112 | `	int iUnionFlag,` |
|       - |  6113 | `	int bAllowVoid,` |
|       - |  6114 | `	sxu32 nLine` |
|       2 |  6115 | `){` |
|       - |  6116 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   66736 |  6117 | `	int nAtoms = 0;` |
|   66736 |  6118 | `	int bShortNullable = 0;` |
|   66736 |  6119 | `	int bExplicitNull = 0;` |
|       - |  6120 | `	sxi32 rc;` |
|   66736 |  6121 | `	*pnType = 0;` |
|   66736 |  6122 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   66736 |  6123 | `	*piTypeFlags = 0;` |
|   66736 |  6124 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6125 |  |
|   66736 |  6126 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6127 | `		return SXRET_OK;` |
|       - |  6128 | `	}` |
|       - |  6129 | ``	/* Optional `?` shorthand prefix */`` |
|   66734 |  6130 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      52 |  6131 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      52 |  6132 | `		bShortNullable = 1;` |
|      52 |  6133 | `		pGen->pIn++;` |
|      52 |  6134 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6135 | `			return SXERR_SYNTAX;` |
|       - |  6136 | `		}` |
|      25 |  6137 | `	}` |
|       - |  6138 | `	/* First atom is mandatory */` |
|   66736 |  6139 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   66736 |  6140 | `	if( rc != SXRET_OK ){` |
|       3 |  6141 | `		return rc;` |
|       - |  6142 | `	}` |
|   66734 |  6143 | `	nAtoms = 1;` |
|       - |  6144 | ``	/* Subsequent atoms separated by `\|` */`` |
|  100232 |  6145 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   66868 |  6146 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      92 |  6147 | `		if( bShortNullable ){` |
|       - |  6148 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6149 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6150 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6151 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6152 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6153 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6154 | `		}` |
|      90 |  6155 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6156 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6157 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6158 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6159 | `		}` |
|      90 |  6160 | ``		pGen->pIn++; /* skip `\|` */`` |
|      90 |  6161 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      90 |  6162 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6163 | `			return rc;` |
|       - |  6164 | `		}` |
|      90 |  6165 | `		nAtoms++;` |
|       2 |  6166 | `	}` |
|       - |  6167 | `	/* Validation pass.` |
|       - |  6168 | `	 *` |
|       - |  6169 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6170 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6171 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6172 | `	 */` |
|       - |  6173 | `	{` |
|       - |  6174 | `		int i, j;` |
|   66732 |  6175 | `		int bHasNonNull = 0;` |
|  133544 |  6176 | `		for( i = 0; i < nAtoms; i++ ){` |
|   66820 |  6177 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     100 |  6178 | `				if( nAtoms > 1 ){` |
|       3 |  6179 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6180 | `						"Void can only be used as a standalone type");` |
|       3 |  6181 | `					return SXERR_SYNTAX;` |
|       - |  6182 | `				}` |
|      98 |  6183 | `				if( !bAllowVoid ){` |
|     ! 0 |  6184 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6185 | `						"void cannot be used here");` |
|     ! 0 |  6186 | `					return SXERR_SYNTAX;` |
|       - |  6187 | `				}` |
|      98 |  6188 | `				if( bShortNullable ){` |
|     ! 0 |  6189 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6190 | `						"Void type cannot be nullable");` |
|     ! 0 |  6191 | `					return SXERR_SYNTAX;` |
|       - |  6192 | `				}` |
|      48 |  6193 | `			}` |
|   66818 |  6194 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6195 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6196 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6197 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6198 | `				 * does not return), and folding them would mislead any` |
|       - |  6199 | `				 * future return-enforcement work. */` |
|       3 |  6200 | `				if( nAtoms > 1 ){` |
|       3 |  6201 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6202 | `						"never can only be used as a standalone type");` |
|       3 |  6203 | `					return SXERR_SYNTAX;` |
|       - |  6204 | `				}` |
|     ! 0 |  6205 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6206 | `					"never type is not yet implemented");` |
|     ! 0 |  6207 | `				return SXERR_SYNTAX;` |
|       - |  6208 | `			}` |
|   66816 |  6209 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      12 |  6210 | `				bExplicitNull = 1;` |
|       7 |  6211 | `			}else{` |
|   66806 |  6212 | `				bHasNonNull = 1;` |
|       - |  6213 | `			}` |
|       - |  6214 | `			/* Duplicate detection */` |
|   66934 |  6215 | `			for( j = 0; j < i; j++ ){` |
|     122 |  6216 | `				int bDup = 0;` |
|     122 |  6217 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      16 |  6218 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6219 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6220 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6221 | `								aAtoms[j].sClass.zString,` |
|      12 |  6222 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6223 | `							bDup = 1;` |
|     ! 0 |  6224 | `						}` |
|       8 |  6225 | `					}else{` |
|       3 |  6226 | `						bDup = 1;` |
|       - |  6227 | `					}` |
|       7 |  6228 | `				}` |
|     122 |  6229 | `				if( bDup ){` |
|       - |  6230 | `					const char *zName;` |
|       - |  6231 | `					sxu32 nName;` |
|       3 |  6232 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6233 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6234 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6235 | `					}else{` |
|       3 |  6236 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6237 | `						nName = aAtoms[i].nCanon;` |
|       - |  6238 | `					}` |
|       4 |  6239 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6240 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6241 | `					return SXERR_SYNTAX;` |
|       - |  6242 | `				}` |
|      61 |  6243 | `			}` |
|   33408 |  6244 | `		}` |
|   66726 |  6245 | `		if( !bHasNonNull && bExplicitNull ){` |
|     ! 0 |  6246 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6247 | `				"Null can not be used as a standalone type");` |
|     ! 0 |  6248 | `			return SXERR_SYNTAX;` |
|       - |  6249 | `		}` |
|       - |  6250 | `	}` |
|       - |  6251 | `	/* Compute nullability flag */` |
|   66726 |  6252 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      60 |  6253 | `		*piTypeFlags \|= iNullableFlag;` |
|      29 |  6254 | `	}` |
|       - |  6255 | `	/* Build canonical type text */` |
|   66726 |  6256 | `	if( pTypeText ){` |
|       - |  6257 | `		SyBlob sBlob;` |
|   66726 |  6258 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  100064 |  6259 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   33362 |  6260 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   66726 |  6261 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   99944 |  6262 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   66628 |  6263 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   66630 |  6264 | `			if( zDup ){` |
|   66630 |  6265 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   33314 |  6266 | `			}` |
|   33314 |  6267 | `		}` |
|   66726 |  6268 | `		SyBlobRelease(&sBlob);` |
|   33362 |  6269 | `	}` |
|       - |  6270 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6271 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6272 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6273 | `	{` |
|   66726 |  6274 | `		int nNonNull = 0;` |
|   66726 |  6275 | `		int iNonNullIdx = -1;` |
|       - |  6276 | `		int i;` |
|  133532 |  6277 | `		for( i = 0; i < nAtoms; i++ ){` |
|   66808 |  6278 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   66798 |  6279 | `				nNonNull++;` |
|   66798 |  6280 | `				iNonNullIdx = i;` |
|   33398 |  6281 | `			}` |
|   33405 |  6282 | `		}` |
|   66726 |  6283 | `		if( nNonNull <= 1 ){` |
|       - |  6284 | `			/* Fast path: store as single type. */` |
|   66670 |  6285 | `			if( iNonNullIdx >= 0 ){` |
|   66670 |  6286 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   66670 |  6287 | `				if( pA->nType == SXU32_HIGH ){` |
|   14258 |  6288 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4752 |  6289 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    9506 |  6290 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    9506 |  6291 | `					*pnType = SXU32_HIGH;` |
|    9506 |  6292 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   61918 |  6293 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      98 |  6294 | `					*pnType = MEMOBJ_VOID;` |
|      50 |  6295 | `				}else{` |
|       - |  6296 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6297 | `					 * pass above rejects it as not-yet-implemented. */` |
|   57070 |  6298 | `					*pnType = pA->nType;` |
|       - |  6299 | `				}` |
|   33334 |  6300 | `			}` |
|   33336 |  6301 | `		}else{` |
|       - |  6302 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      58 |  6303 | `			*piTypeFlags \|= iUnionFlag;` |
|     190 |  6304 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6305 | `				ph7_type_alt sAlt;` |
|     134 |  6306 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     130 |  6307 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     130 |  6308 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      41 |  6309 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      13 |  6310 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      28 |  6311 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      28 |  6312 | `					sAlt.nType = SXU32_HIGH;` |
|      28 |  6313 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      15 |  6314 | `				}else{` |
|     104 |  6315 | `					sAlt.nType = aAtoms[i].nType;` |
|     104 |  6316 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6317 | `				}` |
|     130 |  6318 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      66 |  6319 | `			}` |
|       - |  6320 | `		}` |
|       - |  6321 | `	}` |
|   66726 |  6322 | `	return SXRET_OK;` |
|   33369 |  6323 |  |
|       - |  6324 |  |
|       - |  6325 | `/*` |
|       - |  6326 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6327 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6328 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6329 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6330 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6331 | `` *          and union types `: T\|U`.`` |
|       - |  6332 | ` */` |
|  278954 |  6333 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 |  6334 |  |
|  278956 |  6335 | `	sxi32 iFlags = 0;` |
|       - |  6336 | `	sxi32 rc;` |
|       - |  6337 | `	sxu32 nLine;` |
|  278956 |  6338 | `	pFunc->nReturnType = 0;` |
|  278956 |  6339 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  278956 |  6340 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  278956 |  6341 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  278654 |  6342 | `		return SXRET_OK;` |
|       - |  6343 | `	}` |
|     304 |  6344 | `	pGen->pIn++; /* Skip ':' */` |
|     304 |  6345 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6346 | `		return SXRET_OK;` |
|       - |  6347 | `	}` |
|     304 |  6348 | `	nLine = pGen->pIn->nLine;` |
|     304 |  6349 | `	rc = GenStateParseUnionTypeDecl(` |
|     151 |  6350 | `		pGen,` |
|     151 |  6351 | `		&pFunc->nReturnType,` |
|     151 |  6352 | `		&pFunc->sReturnClass,` |
|     151 |  6353 | `		&pFunc->aReturnUnion,` |
|       - |  6354 | `		&iFlags,` |
|     151 |  6355 | `		&pFunc->sReturnTypeName,` |
|       - |  6356 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6357 | `		/* iUnionFlag */ 0,` |
|       - |  6358 | `		/* bAllowVoid */ 1,` |
|     151 |  6359 | `		nLine);` |
|     151 |  6360 | `	(void)iFlags;` |
|     304 |  6361 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6362 | `		return SXERR_ABORT;` |
|       - |  6363 | `	}` |
|     304 |  6364 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6365 | `		/* Error already reported */` |
|     ! 0 |  6366 | `		return SXERR_SYNTAX;` |
|       - |  6367 | `	}` |
|     304 |  6368 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6369 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6370 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6371 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6372 | `				&pGen->pIn->sData);` |
|       3 |  6373 | `		}else{` |
|     ! 0 |  6374 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6375 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6376 | `		}` |
|       5 |  6377 | `		return SXERR_SYNTAX;` |
|       - |  6378 | `	}` |
|     300 |  6379 | `	return SXRET_OK;` |
|  139479 |  6380 |  |
|       - |  6381 |  |
|   41906 |  6382 | `static sxi32 GenStateCompileFunc(` |
|       - |  6383 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6384 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6385 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6386 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6387 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6388 | `	)` |
|       2 |  6389 |  |
|       - |  6390 | `	ph7_vm_func *pFunc;` |
|       - |  6391 | `	SyToken *pEnd;` |
|       - |  6392 | `	sxu32 nLine;` |
|       - |  6393 | `	char *zName;` |
|       - |  6394 | `	sxi32 rc;` |
|       - |  6395 | `	/* Extract line number */` |
|   41908 |  6396 | `	nLine = pGen->pIn->nLine;` |
|       - |  6397 | `	/* Jump the left parenthesis '(' */` |
|   41908 |  6398 | `	pGen->pIn++;` |
|       - |  6399 | `	/* Delimit the function signature */` |
|   41908 |  6400 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   41908 |  6401 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6402 | `		/* Syntax error */` |
|       7 |  6403 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 |  6404 | `		if( rc == SXERR_ABORT ){` |
|       - |  6405 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6406 | `			return SXERR_ABORT;` |
|       - |  6407 | `		}` |
|       7 |  6408 | `		pGen->pIn = pGen->pEnd;` |
|       7 |  6409 | `		return SXRET_OK;` |
|       - |  6410 | `	}` |
|       - |  6411 | `	/* Create the function state */` |
|   41902 |  6412 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   41902 |  6413 | `	if( pFunc == 0 ){` |
|     ! 0 |  6414 | `		goto OutOfMem;` |
|       - |  6415 | `	}` |
|       - |  6416 | `	/* Build the function name, prepending namespace if active */` |
|   41909 |  6417 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6418 | `		SyBlob sFQN;` |
|       - |  6419 | `		sxu32 nLen;` |
|      16 |  6420 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6421 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6422 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6423 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6424 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6425 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6426 | `		SyBlobRelease(&sFQN);` |
|      16 |  6427 | `		if( zName == 0 ){` |
|     ! 0 |  6428 | `			goto OutOfMem;` |
|       - |  6429 | `		}` |
|      16 |  6430 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6431 | `	}else{` |
|   41888 |  6432 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   41888 |  6433 | `		if( zName == 0 ){` |
|     ! 0 |  6434 | `			goto OutOfMem;` |
|       - |  6435 | `		}` |
|   41888 |  6436 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6437 | `	}` |
|   41902 |  6438 | `	if( pGen->pIn < pEnd ){` |
|       - |  6439 | `		/* Collect function arguments */` |
|   29052 |  6440 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   29052 |  6441 | `		if( rc == SXERR_ABORT ){` |
|       - |  6442 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6443 | `			return SXERR_ABORT;` |
|       - |  6444 | `		}` |
|   14525 |  6445 | `	}` |
|       - |  6446 | `	/* Point past ')' and parse optional return type ': type' */` |
|   41902 |  6447 | `	pGen->pIn = &pEnd[1];` |
|       - |  6448 | `	{` |
|   41902 |  6449 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   41902 |  6450 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6451 | `			return SXERR_ABORT;` |
|   41902 |  6452 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6453 | `			return SXERR_SYNTAX;` |
|       - |  6454 | `		}` |
|       - |  6455 | `	}` |
|   41898 |  6456 | `	if( bHandleClosure ){` |
|       - |  6457 | `		ph7_vm_func_closure_env sEnv;` |
|     232 |  6458 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     230 |  6459 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     125 |  6460 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      18 |  6461 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6462 | `				/* Closure,record environment variable */` |
|      18 |  6463 | `				pGen->pIn++;` |
|      18 |  6464 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6465 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6466 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6467 | `						return SXERR_ABORT;` |
|       - |  6468 | `					}` |
|     ! 0 |  6469 | `				}` |
|      18 |  6470 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6471 | `				/* Compile until we hit the first closing parenthesis */` |
|      38 |  6472 | `				while( pGen->pIn < pGen->pEnd ){` |
|      38 |  6473 | `					int iFlagsLocal = 0;` |
|      38 |  6474 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      18 |  6475 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      18 |  6476 | `						break;` |
|       - |  6477 | `					}` |
|      22 |  6478 | `					nLineLocal = pGen->pIn->nLine;` |
|      22 |  6479 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6480 | `						/* Pass by reference,record that */` |
|     ! 0 |  6481 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6482 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6483 | `							);` |
|     ! 0 |  6484 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6485 | `						pGen->pIn++;` |
|     ! 0 |  6486 | `					}` |
|      20 |  6487 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      22 |  6488 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6489 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6490 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6491 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6492 | `								return SXERR_ABORT;` |
|       - |  6493 | `							}` |
|       - |  6494 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6495 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6496 | `								pGen->pIn++;` |
|     ! 0 |  6497 | `							}` |
|     ! 0 |  6498 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6499 | `								pGen->pIn++;` |
|     ! 0 |  6500 | `							}` |
|     ! 0 |  6501 | `							break;` |
|       - |  6502 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6503 | `					}else{` |
|       - |  6504 | `						SyString *pNameLocal;` |
|       - |  6505 | `						char *zDup;` |
|       - |  6506 | `						/* Duplicate variable name */` |
|      22 |  6507 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      22 |  6508 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      22 |  6509 | `						if( zDup ){` |
|       - |  6510 | `							/* Zero the structure */` |
|      22 |  6511 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      22 |  6512 | `							sEnv.iFlags = iFlagsLocal;` |
|      22 |  6513 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      22 |  6514 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      22 |  6515 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6516 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6517 | `									got_this = 1;` |
|     ! 0 |  6518 | `							}` |
|       - |  6519 | `							/* Save imported variable */` |
|      22 |  6520 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      12 |  6521 | `						}else{` |
|     ! 0 |  6522 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6523 | `							 return SXERR_ABORT;` |
|       - |  6524 | `						}` |
|       - |  6525 | `					}` |
|      22 |  6526 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      28 |  6527 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6528 | `						/* Ignore trailing commas */` |
|       7 |  6529 | `						pGen->pIn++;` |
|       1 |  6530 | `					}` |
|       2 |  6531 | `				}` |
|      18 |  6532 | `				if( !got_this ){` |
|       - |  6533 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6534 | `					 * available to the closure environment.` |
|       - |  6535 | `					 */` |
|      18 |  6536 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      18 |  6537 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      18 |  6538 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      18 |  6539 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      18 |  6540 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       8 |  6541 | `				}` |
|      18 |  6542 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6543 | `					/* Mark as closure */` |
|      18 |  6544 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       8 |  6545 | `				}` |
|       8 |  6546 | `		}` |
|     115 |  6547 | `	}` |
|       - |  6548 | `	/* Compile the body */` |
|   41898 |  6549 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   41898 |  6550 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6551 | `		return SXERR_ABORT;` |
|       - |  6552 | `	}` |
|   41898 |  6553 | `	if( ppFunc ){` |
|     232 |  6554 | `		*ppFunc = pFunc;` |
|     115 |  6555 | `	}` |
|   41898 |  6556 | `	rc = SXRET_OK;` |
|   41898 |  6557 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6558 | `		/* Finally register the function */` |
|   41882 |  6559 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   20940 |  6560 | `	}` |
|   41898 |  6561 | `	if( rc == SXRET_OK ){` |
|   41898 |  6562 | `		return SXRET_OK;` |
|       - |  6563 | `	}` |
|       - |  6564 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6565 | `OutOfMem:` |
|       - |  6566 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6567 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6568 | `	 */` |
|     ! 0 |  6569 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6570 | `	return SXERR_ABORT;` |
|   20955 |  6571 |  |
|       - |  6572 | `/*` |
|       - |  6573 | ` * Compile a standard PHP function.` |
|       - |  6574 | ` *  Refer to the block-comment above for more information.` |
|       - |  6575 | ` */` |
|   41682 |  6576 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 |  6577 |  |
|       - |  6578 | `	SyString *pName;` |
|       - |  6579 | `	sxi32 iFlags;` |
|       - |  6580 | `	sxu32 nLine;` |
|       - |  6581 | `	sxi32 rc;` |
|       - |  6582 |  |
|   41684 |  6583 | `	nLine = pGen->pIn->nLine;` |
|   41684 |  6584 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   41684 |  6585 | `	iFlags = 0;` |
|   41684 |  6586 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6587 | `		/* Return by reference,remember that */` |
|       7 |  6588 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6589 | `		/* Jump the '&' token */` |
|       7 |  6590 | `		pGen->pIn++;` |
|       3 |  6591 | `	}` |
|   41684 |  6592 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6593 | `		/* Invalid function name */` |
|       5 |  6594 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 |  6595 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6596 | `			return SXERR_ABORT;` |
|       - |  6597 | `		}` |
|       - |  6598 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 |  6599 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 |  6600 | `			pGen->pIn++;` |
|       1 |  6601 | `		}` |
|       5 |  6602 | `		return SXRET_OK;` |
|       - |  6603 | `	}` |
|   41680 |  6604 | `	pName = &pGen->pIn->sData;` |
|   41680 |  6605 | `	nLine = pGen->pIn->nLine;` |
|       - |  6606 | `	/* Jump the function name */` |
|   41680 |  6607 | `	pGen->pIn++;` |
|   41680 |  6608 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6609 | `		/* Syntax error */` |
|       3 |  6610 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6611 | `		if( rc == SXERR_ABORT ){` |
|       - |  6612 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6613 | `			return SXERR_ABORT;` |
|       - |  6614 | `		}` |
|       - |  6615 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6616 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6617 | `			pGen->pIn++;` |
|     ! 0 |  6618 | `		}` |
|       3 |  6619 | `		return SXRET_OK;` |
|       - |  6620 | `	}` |
|       - |  6621 | `	/* Compile function body */` |
|   41678 |  6622 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   41678 |  6623 | `	return rc;` |
|   20843 |  6624 |  |
|       - |  6625 | `/*` |
|       - |  6626 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6627 | ` * According to the PHP language reference manual` |
|       - |  6628 | ` *  Visibility:` |
|       - |  6629 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6630 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6631 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6632 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6633 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6634 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6635 | ` */` |
|  297286 |  6636 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 |  6637 |  |
|  297288 |  6638 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    9528 |  6639 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  287762 |  6640 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   40970 |  6641 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6642 | `	}` |
|       - |  6643 | `	/* Assume public by default */` |
|  246794 |  6644 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  148645 |  6645 |  |
|       - |  6646 | `/*` |
|       - |  6647 | ` * Compile a class constant.` |
|       - |  6648 | ` * According to the PHP language reference manual` |
|       - |  6649 | ` *  Class Constants` |
|       - |  6650 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6651 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6652 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6653 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6654 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6655 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6656 | ` * Symisc eXtension.` |
|       - |  6657 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6658 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6659 | ` *  Example:` |
|       - |  6660 | ` *   class Test{` |
|       - |  6661 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6662 | ` *   };` |
|       - |  6663 | ` *   var_dump(TEST::MyConst);` |
|       - |  6664 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6665 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6666 | ` */` |
|      32 |  6667 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6668 |  |
|      34 |  6669 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6670 | `	SySet *pInstrContainer;` |
|       - |  6671 | `	ph7_class_attr *pCons;` |
|       - |  6672 | `	SyString *pName;` |
|       - |  6673 | `	sxi32 rc;` |
|       - |  6674 | `	/* Extract visibility level */` |
|      34 |  6675 | `	iProtection = GetProtectionLevel(iProtection);` |
|      34 |  6676 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      16 |  6677 | `loop:` |
|       - |  6678 | `	/* Mark as constant */` |
|      34 |  6679 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      34 |  6680 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6681 | `		/* Invalid constant name */` |
|     ! 0 |  6682 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6683 | `		if( rc == SXERR_ABORT ){` |
|       - |  6684 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6685 | `			return SXERR_ABORT;` |
|       - |  6686 | `		}` |
|     ! 0 |  6687 | `		goto Synchronize;` |
|       - |  6688 | `	}` |
|       - |  6689 | `	/* Peek constant name */` |
|      34 |  6690 | `	pName = &pGen->pIn->sData;` |
|       - |  6691 | `	/* Make sure the constant name isn't reserved */` |
|      34 |  6692 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6693 | `		/* Reserved constant name */` |
|     ! 0 |  6694 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6695 | `		if( rc == SXERR_ABORT ){` |
|       - |  6696 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6697 | `			return SXERR_ABORT;` |
|       - |  6698 | `		}` |
|     ! 0 |  6699 | `		goto Synchronize;` |
|       - |  6700 | `	}` |
|       - |  6701 | `	/* Advance the stream cursor */` |
|      34 |  6702 | `	pGen->pIn++;` |
|      34 |  6703 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6704 | `		/* Invalid declaration */` |
|     ! 0 |  6705 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6706 | `		if( rc == SXERR_ABORT ){` |
|       - |  6707 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6708 | `			return SXERR_ABORT;` |
|       - |  6709 | `		}` |
|     ! 0 |  6710 | `		goto Synchronize;` |
|       - |  6711 | `	}` |
|      34 |  6712 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6713 | `	/* Allocate a new class attribute */` |
|      34 |  6714 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      34 |  6715 | `	if( pCons == 0 ){` |
|     ! 0 |  6716 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6717 | `		return SXERR_ABORT;` |
|       - |  6718 | `	}` |
|       - |  6719 | `	/* Swap bytecode container */` |
|      34 |  6720 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      34 |  6721 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6722 | `	/* Compile constant value.` |
|       - |  6723 | `	 */` |
|      34 |  6724 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      34 |  6725 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6726 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6727 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6728 | `			return SXERR_ABORT;` |
|       - |  6729 | `		}` |
|       1 |  6730 | `	}` |
|       - |  6731 | `	/* Emit the done instruction */` |
|      34 |  6732 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      34 |  6733 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      34 |  6734 | `	if( rc == SXERR_ABORT ){` |
|       - |  6735 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6736 | `		return SXERR_ABORT;` |
|       - |  6737 | `	}` |
|       - |  6738 | `	/* All done,install the constant */` |
|      34 |  6739 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      34 |  6740 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6741 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6742 | `		return SXERR_ABORT;` |
|       - |  6743 | `	}` |
|      34 |  6744 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6745 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6746 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6747 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6748 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6749 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6750 | `				pTok--;` |
|     ! 0 |  6751 | `			}` |
|     ! 0 |  6752 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6753 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6754 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6755 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6756 | `				return SXERR_ABORT;` |
|       - |  6757 | `			}` |
|     ! 0 |  6758 | `		}else{` |
|     ! 0 |  6759 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6760 | `				goto loop;` |
|       - |  6761 | `			}` |
|       - |  6762 | `		}` |
|     ! 0 |  6763 | `	}` |
|      34 |  6764 | `	return SXRET_OK;` |
|     ! 0 |  6765 | `Synchronize:` |
|       - |  6766 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6767 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6768 | `		pGen->pIn++;` |
|     ! 0 |  6769 | `	}` |
|     ! 0 |  6770 | `	return SXERR_CORRUPT;` |
|      18 |  6771 |  |
|       - |  6772 | `/*` |
|       - |  6773 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6774 | ` * According to the PHP language reference manual` |
|       - |  6775 | ` *  Properties` |
|       - |  6776 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6777 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6778 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6779 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6780 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6781 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6782 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6783 | ` * Symisc eXtension.` |
|       - |  6784 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6785 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6786 | ` *  Example:` |
|       - |  6787 | ` *   class Test{` |
|       - |  6788 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6789 | ` *   };` |
|       - |  6790 | ` *   var_dump(TEST::myVar);` |
|       - |  6791 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6792 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6793 | ` */` |
|       - |  6794 | `/*` |
|       - |  6795 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6796 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6797 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6798 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6799 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6800 | ` */` |
|  155140 |  6801 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 |  6802 |  |
|  155142 |  6803 | `	SyToken *p = pStart;` |
|  155142 |  6804 | `	if( p >= pEnd ) return 0;` |
|  155142 |  6805 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6806 | `		p++;` |
|      16 |  6807 | `		if( p >= pEnd ) return 0;` |
|       7 |  6808 | `	}` |
|  155142 |  6809 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6810 | `		p++;` |
|       3 |  6811 | `		if( p >= pEnd ) return 0;` |
|       1 |  6812 | `	}` |
|  155142 |  6813 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6814 | `		return 0;` |
|       - |  6815 | `	}` |
|       - |  6816 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6817 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6818 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6819 | `	 * dispatch site. */` |
|  155142 |  6820 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  155124 |  6821 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  155179 |  6822 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3317 |  6823 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  155010 |  6824 | `			return 0;` |
|       - |  6825 | `		}` |
|      57 |  6826 | `	}` |
|     134 |  6827 | `	p++;` |
|       - |  6828 | `	/* Consume optional namespace path */` |
|     136 |  6829 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6830 | `		p += 2;` |
|       1 |  6831 | `	}` |
|       - |  6832 | ``	/* Consume any `\| Type` union alternatives */`` |
|     216 |  6833 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      86 |  6834 | `		&& p->sData.zString[0] == '\|' ){` |
|      14 |  6835 | `		p++;` |
|      14 |  6836 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      14 |  6837 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      14 |  6838 | `		p++;` |
|      14 |  6839 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6840 | `			p += 2;` |
|     ! 0 |  6841 | `		}` |
|       2 |  6842 | `	}` |
|     134 |  6843 | `	if( p >= pEnd ) return 0;` |
|     134 |  6844 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   77572 |  6845 |  |
|       - |  6846 |  |
|       - |  6847 | `/*` |
|       - |  6848 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6849 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6850 | ` * if not). Recognized forms:` |
|       - |  6851 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6852 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6853 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6854 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6855 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6856 | ` * on unrecoverable error.` |
|       - |  6857 | ` *` |
|       - |  6858 | ` * When a type is parsed:` |
|       - |  6859 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6860 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6861 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6862 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6863 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6864 | ` */` |
|     132 |  6865 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6866 | `	ph7_gen_state *pGen,` |
|       - |  6867 | `	sxu32 *pnType,` |
|       - |  6868 | `	SyString *pClass,` |
|       - |  6869 | `	sxi32 *piTypeFlags,` |
|       - |  6870 | `	SyString *pTypeText,` |
|       - |  6871 | `	SySet *pAlts` |
|       2 |  6872 | `){` |
|     134 |  6873 | `	sxi32 iFlags = 0;` |
|       - |  6874 | `	sxi32 rc;` |
|     134 |  6875 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6876 | `		return SXRET_OK;` |
|       - |  6877 | `	}` |
|       - |  6878 | `	/* If the first token is '$', there's no type */` |
|     134 |  6879 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6880 | `		return SXRET_OK;` |
|       - |  6881 | `	}` |
|     134 |  6882 | `	rc = GenStateParseUnionTypeDecl(` |
|      66 |  6883 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6884 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6885 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6886 | `		/* bAllowVoid */ 0,` |
|     132 |  6887 | `		pGen->pIn->nLine);` |
|     134 |  6888 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6889 | `		return rc;` |
|       - |  6890 | `	}` |
|       - |  6891 | `	/* Verify next token is '$' (start of property name) */` |
|     134 |  6892 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6893 | `		return SXERR_SYNTAX;` |
|       - |  6894 | `	}` |
|     134 |  6895 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     134 |  6896 | `	return SXRET_OK;` |
|      68 |  6897 |  |
|       - |  6898 |  |
|       - |  6899 | `/*` |
|       - |  6900 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  6901 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  6902 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  6903 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  6904 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  6905 | ` * by the type parser itself before reaching here.` |
|       - |  6906 | ` *` |
|       - |  6907 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  6908 | ` * use in the error message.` |
|       - |  6909 | ` */` |
|     198 |  6910 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  6911 | `	sxu32 nType,` |
|       - |  6912 | `	const SyString *pClass,` |
|       - |  6913 | `	const char **pzName,` |
|       - |  6914 | `	sxu32 *pnName)` |
|       2 |  6915 |  |
|       - |  6916 | `	const char *z;` |
|       - |  6917 | `	sxu32 n;` |
|     200 |  6918 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     170 |  6919 | `		return 0;` |
|       - |  6920 | `	}` |
|      32 |  6921 | `	z = pClass->zString;` |
|      32 |  6922 | `	n = pClass->nByte;` |
|      32 |  6923 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       5 |  6924 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  6925 | `	}` |
|      28 |  6926 | `	if( n == 5 && SyMemcmpNoCase(z,"mixed",5) == 0 ){` |
|     ! 0 |  6927 | `		*pzName = "mixed"; *pnName = 5; return 1;` |
|       - |  6928 | `	}` |
|      28 |  6929 | `	if( n == 8 && SyMemcmpNoCase(z,"iterable",8) == 0 ){` |
|     ! 0 |  6930 | `		*pzName = "iterable"; *pnName = 8; return 1;` |
|       - |  6931 | `	}` |
|      28 |  6932 | `	return 0;` |
|     101 |  6933 |  |
|       - |  6934 |  |
|       - |  6935 | `/*` |
|       - |  6936 | ` * Validate a parsed property type (main atom + any union alternatives)` |
|       - |  6937 | ` * against the disallowed-pseudo-types list. Emits a PHP-compatible` |
|       - |  6938 | ` * "Property C::$x cannot have type T" error on rejection, where T is` |
|       - |  6939 | ` * the full canonical type text (matching PHP's error wording for` |
|       - |  6940 | `` * unions like `callable\|int`).`` |
|       - |  6941 | ` *` |
|       - |  6942 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  6943 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  6944 | ` */` |
|     170 |  6945 | `static sxi32 GenStateValidatePropertyType(` |
|       - |  6946 | `	ph7_gen_state *pGen,` |
|       - |  6947 | `	ph7_class *pClass,` |
|       - |  6948 | `	const SyString *pPropName,` |
|       - |  6949 | `	sxu32 nType,` |
|       - |  6950 | `	const SyString *pTypeClass,` |
|       - |  6951 | `	const SyString *pTypeText,` |
|       - |  6952 | `	SySet *pUnionAlts,` |
|       - |  6953 | `	sxu32 nLine)` |
|       2 |  6954 |  |
|     172 |  6955 | `	const char *zBad = 0;` |
|     172 |  6956 | `	sxu32 nBad = 0;` |
|       - |  6957 | `	SyString sFallback;` |
|       - |  6958 | `	const SyString *pBad;` |
|       - |  6959 | `	sxi32 rc;` |
|     172 |  6960 | `	int bDisallowed = 0;` |
|     172 |  6961 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       3 |  6962 | `		bDisallowed = 1;` |
|     171 |  6963 | `	}else if( pUnionAlts ){` |
|       - |  6964 | `		sxu32 i;` |
|      42 |  6965 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      30 |  6966 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      30 |  6967 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  6968 | `				bDisallowed = 1;` |
|       3 |  6969 | `				break;` |
|       - |  6970 | `			}` |
|      15 |  6971 | `		}` |
|       7 |  6972 | `	}` |
|     172 |  6973 | `	if( !bDisallowed ){` |
|     168 |  6974 | `		return SXRET_OK;` |
|       - |  6975 | `	}` |
|       - |  6976 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  6977 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  6978 | `	 * canonical spelling if the type text is unavailable. */` |
|       5 |  6979 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       5 |  6980 | `		pBad = pTypeText;` |
|       3 |  6981 | `	}else{` |
|     ! 0 |  6982 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  6983 | `		pBad = &sFallback;` |
|       - |  6984 | `	}` |
|       7 |  6985 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6986 | `		"Property %z::$%z cannot have type %z",` |
|       2 |  6987 | `		&pClass->sName,pPropName,pBad);` |
|       5 |  6988 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6989 | `		return SXERR_ABORT;` |
|       - |  6990 | `	}` |
|       5 |  6991 | `	return SXERR_SYNTAX;` |
|      87 |  6992 |  |
|       - |  6993 |  |
|   60312 |  6994 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6995 |  |
|   60314 |  6996 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6997 | `	ph7_class_attr *pAttr;` |
|       - |  6998 | `	SyString *pName;` |
|       - |  6999 | `	sxi32 rc;` |
|   60314 |  7000 | `	sxu32 nType = 0;` |
|       - |  7001 | `	SyString sTypeClass;` |
|       - |  7002 | `	SyString sTypeText;` |
|       - |  7003 | `	SySet aUnionAlts;` |
|   60314 |  7004 | `	sxi32 iTypeFlags = 0;` |
|   60314 |  7005 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   60314 |  7006 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   60314 |  7007 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7008 | `	/* Extract visibility level */` |
|   60314 |  7009 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7010 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   60380 |  7011 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     134 |  7012 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     134 |  7013 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7014 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7015 | `			goto Synchronize;` |
|     134 |  7016 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7017 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7018 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7019 | `				&pGen->pIn->sData);` |
|     ! 0 |  7020 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7021 | `				return SXERR_ABORT;` |
|       - |  7022 | `			}` |
|     ! 0 |  7023 | `			goto Synchronize;` |
|     134 |  7024 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7025 | `			return SXERR_ABORT;` |
|       - |  7026 | `		}` |
|      66 |  7027 | `	}` |
|     ! 0 |  7028 | `loop:` |
|   60318 |  7029 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7030 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7031 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7032 | `			return SXERR_ABORT;` |
|       - |  7033 | `		}` |
|     ! 0 |  7034 | `		goto Synchronize;` |
|       - |  7035 | `	}` |
|   60318 |  7036 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   60318 |  7037 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7038 | `		/* Invalid attribute name */` |
|     ! 0 |  7039 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7040 | `		if( rc == SXERR_ABORT ){` |
|       - |  7041 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7042 | `			return SXERR_ABORT;` |
|       - |  7043 | `		}` |
|     ! 0 |  7044 | `		goto Synchronize;` |
|       - |  7045 | `	}` |
|       - |  7046 | `	/* Peek attribute name */` |
|   60318 |  7047 | `	pName = &pGen->pIn->sData;` |
|       - |  7048 | `	/* Advance the stream cursor */` |
|   60318 |  7049 | `	pGen->pIn++;` |
|   60318 |  7050 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7051 | `		/* Invalid declaration */` |
|       3 |  7052 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7053 | `		if( rc == SXERR_ABORT ){` |
|       - |  7054 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7055 | `			return SXERR_ABORT;` |
|       - |  7056 | `		}` |
|       3 |  7057 | `		goto Synchronize;` |
|       - |  7058 | `	}` |
|       - |  7059 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7060 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7061 | `	 * by the type parser. */` |
|   60316 |  7062 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     206 |  7063 | `		rc = GenStateValidatePropertyType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7064 | `			&sTypeText,` |
|     136 |  7065 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,nLine);` |
|     138 |  7066 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7067 | `			return SXERR_ABORT;` |
|     138 |  7068 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7069 | `			goto Synchronize;` |
|       - |  7070 | `		}` |
|      68 |  7071 | `	}` |
|       - |  7072 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   60316 |  7073 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7074 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7075 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7076 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7077 | `			return SXERR_ABORT;` |
|       - |  7078 | `		}` |
|       3 |  7079 | `		goto Synchronize;` |
|       - |  7080 | `	}` |
|       - |  7081 | `	/* Allocate a new class attribute */` |
|   60314 |  7082 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   60314 |  7083 | `	if( pAttr == 0 ){` |
|     ! 0 |  7084 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7085 | `		return SXERR_ABORT;` |
|       - |  7086 | `	}` |
|   60314 |  7087 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     136 |  7088 | `		pAttr->nType = nType;` |
|     136 |  7089 | `		pAttr->sClass = sTypeClass;` |
|     136 |  7090 | `		pAttr->sTypeName = sTypeText;` |
|     136 |  7091 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7092 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  7093 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  7094 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  7095 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  7096 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  7097 | `			sxu32 i;` |
|      32 |  7098 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      22 |  7099 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      22 |  7100 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      12 |  7101 | `			}` |
|       5 |  7102 | `		}` |
|      67 |  7103 | `	}` |
|   60314 |  7104 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7105 | `		SySet *pInstrContainer;` |
|   19274 |  7106 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7107 | `		/* Swap bytecode container */` |
|   19274 |  7108 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   19274 |  7109 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7110 | `		/* Compile attribute value.` |
|       - |  7111 | `		 */` |
|   19274 |  7112 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   19274 |  7113 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7114 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7115 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7116 | `				return SXERR_ABORT;` |
|       - |  7117 | `			}` |
|     ! 0 |  7118 | `		}` |
|       - |  7119 | `		/* Emit the done instruction */` |
|   19274 |  7120 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   19274 |  7121 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    9636 |  7122 | `	}` |
|       - |  7123 | `	/* All done,install the attribute */` |
|   60314 |  7124 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   60314 |  7125 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7126 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7127 | `		return SXERR_ABORT;` |
|       - |  7128 | `	}` |
|   60314 |  7129 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7130 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7131 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7132 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7133 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7134 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7135 | `				pTok--;` |
|     ! 0 |  7136 | `			}` |
|     ! 0 |  7137 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7138 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7139 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7140 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7141 | `				return SXERR_ABORT;` |
|       - |  7142 | `			}` |
|     ! 0 |  7143 | `		}else{` |
|       5 |  7144 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7145 | `				goto loop;` |
|       - |  7146 | `			}` |
|       - |  7147 | `		}` |
|     ! 0 |  7148 | `	}` |
|   60310 |  7149 | `	SySetRelease(&aUnionAlts);` |
|   60310 |  7150 | `	return SXRET_OK;` |
|       2 |  7151 | `Synchronize:` |
|       - |  7152 | `	/* Synchronize with the first semi-colon */` |
|      11 |  7153 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7154 | `		pGen->pIn++;` |
|       1 |  7155 | `	}` |
|       5 |  7156 | `	SySetRelease(&aUnionAlts);` |
|       5 |  7157 | `	return SXERR_CORRUPT;` |
|   30158 |  7158 |  |
|       - |  7159 | `/*` |
|       - |  7160 | ` * Compile a class method.` |
|       - |  7161 | ` *` |
|       - |  7162 | ` * Refer to the official documentation for more information` |
|       - |  7163 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7164 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7165 | ` * overloading and many more.` |
|       - |  7166 | ` */` |
|  236942 |  7167 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7168 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7169 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7170 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7171 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7172 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7173 | `	)` |
|       2 |  7174 |  |
|  236944 |  7175 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7176 | `	ph7_class_method *pMeth;` |
|       - |  7177 | `	sxi32 iFuncFlags;` |
|       - |  7178 | `	SyString *pName;` |
|       - |  7179 | `	SyToken *pEnd;` |
|       - |  7180 | `	sxi32 rc;` |
|       - |  7181 | `	/* Extract visibility level */` |
|  236944 |  7182 | `	iProtection = GetProtectionLevel(iProtection);` |
|  236944 |  7183 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  236944 |  7184 | `	iFuncFlags = 0;` |
|  236944 |  7185 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7186 | `		/* Invalid method name */` |
|     ! 0 |  7187 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7188 | `		if( rc == SXERR_ABORT ){` |
|       - |  7189 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7190 | `			return SXERR_ABORT;` |
|       - |  7191 | `		}` |
|     ! 0 |  7192 | `		goto Synchronize;` |
|       - |  7193 | `	}` |
|  236944 |  7194 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7195 | `		/* Return by reference,remember that */` |
|     ! 0 |  7196 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7197 | `		/* Jump the '&' token */` |
|     ! 0 |  7198 | `		pGen->pIn++;` |
|     ! 0 |  7199 | `	}` |
|  236944 |  7200 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7201 | `		/* Invalid method name */` |
|     ! 0 |  7202 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7203 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7204 | `			return SXERR_ABORT;` |
|       - |  7205 | `		}` |
|     ! 0 |  7206 | `		goto Synchronize;` |
|       - |  7207 | `	}` |
|       - |  7208 | `	/* Peek method name */` |
|  236944 |  7209 | `	pName = &pGen->pIn->sData;` |
|  236944 |  7210 | `	nLine = pGen->pIn->nLine;` |
|       - |  7211 | `	/* Jump the method name */` |
|  236944 |  7212 | `	pGen->pIn++;` |
|  236944 |  7213 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7214 | `		/* Abstract method */` |
|   81902 |  7215 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7216 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7217 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7218 | `				&pClass->sName,pName);` |
|     ! 0 |  7219 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7220 | `				return SXERR_ABORT;` |
|       - |  7221 | `			}` |
|     ! 0 |  7222 | `		}` |
|       - |  7223 | `		/* Assemble method signature only */` |
|   81902 |  7224 | `		doBody = FALSE;` |
|   40950 |  7225 | `	}` |
|  236944 |  7226 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7227 | `		/* Syntax error */` |
|     ! 0 |  7228 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7229 | `		if( rc == SXERR_ABORT ){` |
|       - |  7230 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7231 | `			return SXERR_ABORT;` |
|       - |  7232 | `		}` |
|     ! 0 |  7233 | `		goto Synchronize;` |
|       - |  7234 | `	}` |
|       - |  7235 | `	/* Allocate a new class_method instance */` |
|  236944 |  7236 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  236944 |  7237 | `	if( pMeth == 0 ){` |
|     ! 0 |  7238 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7239 | `		return SXERR_ABORT;` |
|       - |  7240 | `	}` |
|       - |  7241 | `	/* Jump the left parenthesis '(' */` |
|  236944 |  7242 | `	pGen->pIn++;` |
|  236944 |  7243 | `	pEnd = 0; /* cc warning */` |
|       - |  7244 | `	/* Delimit the method signature */` |
|  236944 |  7245 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  236944 |  7246 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7247 | `		/* Syntax error */` |
|       3 |  7248 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7249 | `		if( rc == SXERR_ABORT ){` |
|       - |  7250 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7251 | `			return SXERR_ABORT;` |
|       - |  7252 | `		}` |
|       3 |  7253 | `		goto Synchronize;` |
|       - |  7254 | `	}` |
|       - |  7255 | `	{` |
|  236942 |  7256 | `		int bIsCtor = 0;` |
|  236942 |  7257 | `		int bAbstractCtor = 0;` |
|  345914 |  7258 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  140591 |  7259 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  227446 |  7260 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   18994 |  7261 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7262 | `				bAbstractCtor = 1;` |
|       2 |  7263 | `			}else{` |
|   18992 |  7264 | `				bIsCtor = 1;` |
|       - |  7265 | `			}` |
|    9496 |  7266 | `		}` |
|  236942 |  7267 | `		if( pGen->pIn < pEnd ){` |
|       - |  7268 | `			/* Collect method arguments */` |
|   53852 |  7269 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   53852 |  7270 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7271 | `				return SXERR_ABORT;` |
|       - |  7272 | `			}` |
|   26925 |  7273 | `		}` |
|       - |  7274 | `	}` |
|       - |  7275 | `	/* Point past ')' and parse optional return type ': type' */` |
|  236942 |  7276 | `	pGen->pIn = &pEnd[1];` |
|       - |  7277 | `	{` |
|  236942 |  7278 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  236942 |  7279 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7280 | `			return SXERR_ABORT;` |
|  236942 |  7281 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7282 | `			goto Synchronize;` |
|       - |  7283 | `		}` |
|       - |  7284 | `	}` |
|       - |  7285 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7286 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7287 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7288 | `	{` |
|  236942 |  7289 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7290 | `		sxu32 i;` |
|  322362 |  7291 | `		for( i = 0; i < nArg; i++ ){` |
|   85430 |  7292 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7293 | `			ph7_class_attr *pAttr;` |
|   85430 |  7294 | `			sxi32 iAttrFlags = 0;` |
|   85430 |  7295 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   85392 |  7296 | `				continue;` |
|       - |  7297 | `			}` |
|      40 |  7298 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7299 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7300 | `					"Cannot declare variadic promoted property");` |
|       3 |  7301 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7302 | `					return SXERR_ABORT;` |
|       - |  7303 | `				}` |
|       3 |  7304 | `				goto Synchronize;` |
|       - |  7305 | `			}` |
|       - |  7306 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7307 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7308 | `			 * appear as an alternative of a union type. */` |
|      36 |  7309 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       8 |  7310 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      53 |  7311 | `				rc = GenStateValidatePropertyType(pGen,pClass,&pArg->sName,` |
|      34 |  7312 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7313 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7314 | `					nLine);` |
|      36 |  7315 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7316 | `					return SXERR_ABORT;` |
|      36 |  7317 | `				}else if( rc != SXRET_OK ){` |
|       5 |  7318 | `					goto Synchronize;` |
|       - |  7319 | `				}` |
|      15 |  7320 | `			}` |
|       - |  7321 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      34 |  7322 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7323 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7324 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7325 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7326 | `					return SXERR_ABORT;` |
|       - |  7327 | `				}` |
|       3 |  7328 | `				goto Synchronize;` |
|       - |  7329 | `			}` |
|      32 |  7330 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      28 |  7331 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7332 | `			}` |
|      32 |  7333 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7334 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7335 | `			}` |
|      32 |  7336 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7337 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7338 | `			}` |
|      32 |  7339 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      32 |  7340 | `			if( pAttr == 0 ){` |
|     ! 0 |  7341 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7342 | `				return SXERR_ABORT;` |
|       - |  7343 | `			}` |
|      32 |  7344 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      28 |  7345 | `				pAttr->nType = pArg->nType;` |
|      28 |  7346 | `				pAttr->sClass = pArg->sClass;` |
|      28 |  7347 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      28 |  7348 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7349 | `					sxu32 k;` |
|     ! 0 |  7350 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7351 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7352 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7353 | `					}` |
|     ! 0 |  7354 | `				}` |
|      13 |  7355 | `			}` |
|      32 |  7356 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      32 |  7357 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7358 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7359 | `				return SXERR_ABORT;` |
|       - |  7360 | `			}` |
|      17 |  7361 | `		}` |
|       - |  7362 | `	}` |
|  236934 |  7363 | `	if( doBody ){` |
|       - |  7364 | `		/* Compile method body */` |
|  155034 |  7365 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  155034 |  7366 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7367 | `			return SXERR_ABORT;` |
|       - |  7368 | `		}` |
|   77518 |  7369 | `	}else{` |
|       - |  7370 | `		/* Only method signature is allowed */` |
|   81902 |  7371 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7372 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7373 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7374 | `				if( rc == SXERR_ABORT ){` |
|       - |  7375 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7376 | `					return SXERR_ABORT;` |
|       - |  7377 | `				}` |
|     ! 0 |  7378 | `				return SXERR_CORRUPT;` |
|       - |  7379 | `			}` |
|       - |  7380 | `	}` |
|       - |  7381 | `	/* All done,install the method */` |
|  236934 |  7382 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  236934 |  7383 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7384 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7385 | `		return SXERR_ABORT;` |
|       - |  7386 | `	}` |
|  236934 |  7387 | `	return SXRET_OK;` |
|       5 |  7388 | `Synchronize:` |
|       - |  7389 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7390 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      21 |  7391 | `		pGen->pIn++;` |
|       1 |  7392 | `	}` |
|      11 |  7393 | `	return SXERR_CORRUPT;` |
|  118473 |  7394 |  |
|       - |  7395 | `/*` |
|       - |  7396 | ` * Compile an object interface.` |
|       - |  7397 | ` *  According to the PHP language reference manual` |
|       - |  7398 | ` *   Object Interfaces:` |
|       - |  7399 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7400 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7401 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7402 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7403 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7404 | ` */` |
|   34672 |  7405 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 |  7406 |  |
|   34674 |  7407 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7408 | `	ph7_class *pClass,*pBase;` |
|       - |  7409 | `	SyToken *pEnd,*pTmp;` |
|       - |  7410 | `	SyString *pName;` |
|       - |  7411 | `	sxi32 nKwrd;` |
|       - |  7412 | `	sxi32 rc;` |
|       - |  7413 | `	/* Jump the 'interface' keyword */` |
|   34674 |  7414 | `	pGen->pIn++;` |
|       - |  7415 | `	/* Extract interface name */` |
|   34674 |  7416 | `	pName = &pGen->pIn->sData;` |
|       - |  7417 | `	/* Advance the stream cursor */` |
|   34674 |  7418 | `	pGen->pIn++;` |
|       - |  7419 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7420 | `		SyBlob sFQN;` |
|       - |  7421 | `		SyString sFQNStr;` |
|   34674 |  7422 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   34674 |  7423 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   34674 |  7424 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   34674 |  7425 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   34674 |  7426 | `		SyBlobRelease(&sFQN);` |
|       - |  7427 | `	}` |
|   34674 |  7428 | `	if( pClass == 0 ){` |
|     ! 0 |  7429 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7430 | `		return SXERR_ABORT;` |
|       - |  7431 | `	}` |
|       - |  7432 | `	/* Mark as an interface */` |
|   34674 |  7433 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7434 | `	/* Assume no base class is given */` |
|   34674 |  7435 | `	pBase = 0;` |
|   34674 |  7436 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    9454 |  7437 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    9454 |  7438 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7439 | `			SyBlob sResolved;` |
|       - |  7440 | `			SyString sBaseName;` |
|       - |  7441 | `			sxu32 nRefLine;` |
|       - |  7442 | `			/* Extract base interface */` |
|    9454 |  7443 | `			pGen->pIn++;` |
|    9454 |  7444 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9454 |  7445 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9454 |  7446 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7447 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7448 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7449 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7450 | `					pName);` |
|     ! 0 |  7451 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7452 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7453 | `					return SXERR_ABORT;` |
|       - |  7454 | `				}` |
|     ! 0 |  7455 | `				return SXRET_OK;` |
|       - |  7456 | `			}` |
|   14180 |  7457 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    9452 |  7458 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9454 |  7459 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7460 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7461 | `			/* Only interfaces is allowed */` |
|    9454 |  7462 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7463 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7464 | `			}` |
|    9454 |  7465 | `			if( pBase == 0 ){` |
|     ! 0 |  7466 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7467 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7468 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7469 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7470 | `					return SXERR_ABORT;` |
|       - |  7471 | `				}` |
|     ! 0 |  7472 | `			}` |
|    9454 |  7473 | `			SyBlobRelease(&sResolved);` |
|    4726 |  7474 | `		}` |
|    4726 |  7475 | `	}` |
|   34674 |  7476 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7477 | `		/* Syntax error */` |
|     ! 0 |  7478 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7479 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7480 | `		if( rc == SXERR_ABORT ){` |
|       - |  7481 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7482 | `			return SXERR_ABORT;` |
|       - |  7483 | `		}` |
|     ! 0 |  7484 | `		return SXRET_OK;` |
|       - |  7485 | `	}` |
|   34674 |  7486 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   34674 |  7487 | `	pEnd = 0; /* cc warning */` |
|       - |  7488 | `	/* Delimit the interface body */` |
|   34674 |  7489 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   34674 |  7490 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7491 | `		/* Syntax error */` |
|     ! 0 |  7492 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7493 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7494 | `		if( rc == SXERR_ABORT ){` |
|       - |  7495 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7496 | `			return SXERR_ABORT;` |
|       - |  7497 | `		}` |
|     ! 0 |  7498 | `		return SXRET_OK;` |
|       - |  7499 | `	}` |
|       - |  7500 | `	/* Swap token stream */` |
|   34674 |  7501 | `	pTmp = pGen->pEnd;` |
|   34674 |  7502 | `	pGen->pEnd = pEnd;` |
|       - |  7503 | `	/* Start the parse process` |
|       - |  7504 | `	 * Note (According to the PHP reference manual):` |
|       - |  7505 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7506 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7507 | `	 */` |
|   58280 |  7508 | `	for(;;){` |
|       - |  7509 | `		/* Jump leading/trailing semi-colons */` |
|  198450 |  7510 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   81890 |  7511 | `			pGen->pIn++;` |
|       2 |  7512 | `		}` |
|  116562 |  7513 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7514 | `			/* End of interface body */` |
|   34672 |  7515 | `			break;` |
|       - |  7516 | `		}` |
|   81892 |  7517 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7518 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7519 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7520 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7521 | `			if( rc == SXERR_ABORT ){` |
|       - |  7522 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7523 | `				return SXERR_ABORT;` |
|       - |  7524 | `			}` |
|     ! 0 |  7525 | `			goto done;` |
|       - |  7526 | `		}` |
|       - |  7527 | `		/* Extract the current keyword */` |
|   81892 |  7528 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   81892 |  7529 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7530 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7531 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7532 | `			const char *zKind = "member";` |
|       3 |  7533 | `			SyString *pMemberName = 0;` |
|       3 |  7534 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7535 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7536 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7537 | `					zKind = "constant";` |
|       3 |  7538 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7539 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7540 | `					}` |
|       1 |  7541 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7542 | `					zKind = "method";` |
|     ! 0 |  7543 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7544 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7545 | `					}` |
|     ! 0 |  7546 | `				}` |
|       1 |  7547 | `			}` |
|       3 |  7548 | `			if( pMemberName ){` |
|       4 |  7549 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7550 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7551 | `			}else{` |
|     ! 0 |  7552 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7553 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7554 | `			}` |
|       3 |  7555 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7556 | `				return SXERR_ABORT;` |
|       - |  7557 | `			}` |
|       3 |  7558 | `			goto done;` |
|       - |  7559 | `		}` |
|   81890 |  7560 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7561 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7562 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7563 | `			if( rc == SXERR_ABORT ){` |
|       - |  7564 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7565 | `				return SXERR_ABORT;` |
|       - |  7566 | `			}` |
|     ! 0 |  7567 | `			goto done;` |
|       - |  7568 | `		}` |
|   81890 |  7569 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7570 | `			/* Advance the stream cursor */` |
|   81886 |  7571 | `			pGen->pIn++;` |
|   81886 |  7572 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7573 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7574 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7575 | `				if( rc == SXERR_ABORT ){` |
|       - |  7576 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7577 | `					return SXERR_ABORT;` |
|       - |  7578 | `				}` |
|     ! 0 |  7579 | `				goto done;` |
|       - |  7580 | `			}` |
|   81886 |  7581 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   81886 |  7582 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7583 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7584 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7585 | `				if( rc == SXERR_ABORT ){` |
|       - |  7586 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7587 | `					return SXERR_ABORT;` |
|       - |  7588 | `				}` |
|     ! 0 |  7589 | `				goto done;` |
|       - |  7590 | `			}` |
|   40942 |  7591 | `		}` |
|   81890 |  7592 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7593 | `			/* Parse constant */` |
|       3 |  7594 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7595 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7596 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7597 | `					return SXERR_ABORT;` |
|       - |  7598 | `				}` |
|     ! 0 |  7599 | `				goto done;` |
|       - |  7600 | `			}` |
|       2 |  7601 | `		}else{` |
|   81888 |  7602 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   81888 |  7603 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7604 | `				/* Static method,record that */` |
|    9446 |  7605 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7606 | `				/* Advance the stream cursor */` |
|    9446 |  7607 | `				pGen->pIn++;` |
|    9444 |  7608 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    9446 |  7609 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7610 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7611 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7612 | `						if( rc == SXERR_ABORT ){` |
|       - |  7613 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7614 | `							return SXERR_ABORT;` |
|       - |  7615 | `						}` |
|     ! 0 |  7616 | `						goto done;` |
|       - |  7617 | `				}` |
|    4722 |  7618 | `			}` |
|       - |  7619 | `			/* Process method signature (no body for interface methods) */` |
|   81888 |  7620 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   81888 |  7621 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7622 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7623 | `					return SXERR_ABORT;` |
|       - |  7624 | `				}` |
|     ! 0 |  7625 | `				goto done;` |
|       - |  7626 | `			}` |
|       - |  7627 | `		}` |
|       2 |  7628 | `	}` |
|       - |  7629 | `	/* Install the interface */` |
|   34672 |  7630 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   34672 |  7631 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7632 | `		/* Inherit from the base interface */` |
|    9454 |  7633 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    4726 |  7634 | `	}` |
|   34672 |  7635 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7636 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7637 | `		return SXERR_ABORT;` |
|       - |  7638 | `	}` |
|   17335 |  7639 | `done:` |
|       - |  7640 | `	/* Point beyond the interface body */` |
|   34674 |  7641 | `	pGen->pIn  = &pEnd[1];` |
|   34674 |  7642 | `	pGen->pEnd = pTmp;` |
|   34674 |  7643 | `	return PH7_OK;` |
|   17338 |  7644 |  |
|       - |  7645 | `/*` |
|       - |  7646 | ` * Compile a user-defined class.` |
|       - |  7647 | ` * According to the PHP language reference manual` |
|       - |  7648 | ` *  class` |
|       - |  7649 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7650 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7651 | ` *  of the properties and methods belonging to the class.` |
|       - |  7652 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7653 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7654 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7655 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7656 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7657 | ` *  (called "methods").` |
|       - |  7658 | ` */` |
|       - |  7659 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7660 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7661 | `struct TraitUseEntry {` |
|       - |  7662 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7663 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7664 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7665 | `};` |
|       - |  7666 | `/*` |
|       - |  7667 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7668 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7669 | ` */` |
|   85914 |  7670 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7671 |  |
|       - |  7672 | `	ph7_class **apIface;` |
|       - |  7673 | `	sxu32 nIface,i;` |
|       - |  7674 | `	sxi32 rc;` |
|   85916 |  7675 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7676 | `		return SXRET_OK;` |
|       - |  7677 | `	}` |
|   85916 |  7678 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   85916 |  7679 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  171098 |  7680 | `	for(i = 0; i < nIface; i++){` |
|   85184 |  7681 | `		ph7_class *pIface = apIface[i];` |
|       - |  7682 | `		SyHashEntry *pEntry;` |
|   85184 |  7683 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  227224 |  7684 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  142042 |  7685 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7686 | `			ph7_class_method *pImplMeth;` |
|  142042 |  7687 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7688 | `			/* Find the implementing method in the class */` |
|  142042 |  7689 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  142042 |  7690 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 |  7691 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7692 | `			}` |
|       - |  7693 | `			/* Check visibility: interface methods must be implemented as public */` |
|  142028 |  7694 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7695 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7696 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7697 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7698 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7699 | `					return SXERR_ABORT;` |
|       - |  7700 | `				}` |
|       1 |  7701 | `			}` |
|       - |  7702 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7703 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7704 | `			 */` |
|       - |  7705 | `			{` |
|  142028 |  7706 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  142028 |  7707 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  142028 |  7708 | `				int sigError = 0;` |
|  142028 |  7709 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7710 | `					sigError = 1;` |
|  142027 |  7711 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7712 | `					/* Extra parameters must all have default values */` |
|       5 |  7713 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7714 | `					sxu32 k;` |
|       7 |  7715 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 |  7716 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7717 | `							sigError = 1;` |
|       3 |  7718 | `							break;` |
|       - |  7719 | `						}` |
|       2 |  7720 | `					}` |
|       2 |  7721 | `				}` |
|  142028 |  7722 | `				if( sigError ){` |
|       - |  7723 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7724 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7725 | `					sxu32 j;` |
|       5 |  7726 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 |  7727 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7728 | `					/* Build implementing method signature */` |
|       5 |  7729 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 |  7730 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 |  7731 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 |  7732 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 |  7733 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7734 | `					}` |
|       - |  7735 | `					/* Build interface method signature */` |
|       5 |  7736 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 |  7737 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 |  7738 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 |  7739 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 |  7740 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7741 | `					}` |
|       7 |  7742 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7743 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7744 | `						&pClass->sName,pMName,` |
|       4 |  7745 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7746 | `						&pIface->sName,pMName,` |
|       4 |  7747 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 |  7748 | `					SyBlobRelease(&sImplSig);` |
|       5 |  7749 | `					SyBlobRelease(&sIfaceSig);` |
|       5 |  7750 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7751 | `						return SXERR_ABORT;` |
|       - |  7752 | `					}` |
|       2 |  7753 | `				}` |
|       - |  7754 | `			}` |
|       2 |  7755 | `		}` |
|   42593 |  7756 | `	}` |
|   85916 |  7757 | `	return SXRET_OK;` |
|   42959 |  7758 |  |
|       - |  7759 | `/*` |
|       - |  7760 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7761 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7762 | ` */` |
|   85914 |  7763 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7764 |  |
|       - |  7765 | `	ph7_class_method *pMeth;` |
|       - |  7766 | `	SyHashEntry *pEntry;` |
|       - |  7767 | `	sxu32 nAbstract;` |
|       - |  7768 | `	SyBlob sMsg;` |
|       - |  7769 | `	sxi32 rc;` |
|       - |  7770 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   85916 |  7771 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      24 |  7772 | `		return SXRET_OK;` |
|       - |  7773 | `	}` |
|       - |  7774 | `	/* Count abstract methods */` |
|   85894 |  7775 | `	nAbstract = 0;` |
|   85894 |  7776 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  833326 |  7777 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  747434 |  7778 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  747434 |  7779 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 |  7780 | `			nAbstract++;` |
|       8 |  7781 | `		}` |
|       2 |  7782 | `	}` |
|   85894 |  7783 | `	if( nAbstract == 0 ){` |
|   85880 |  7784 | `		return SXRET_OK;` |
|       - |  7785 | `	}` |
|       - |  7786 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 |  7787 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 |  7788 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7789 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7790 | `		&pClass->sName,nAbstract,` |
|       7 |  7791 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7792 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7793 | `	/* Second pass: list methods with origins */` |
|       - |  7794 | `	{` |
|      15 |  7795 | `		sxu32 nListed = 0;` |
|      15 |  7796 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 |  7797 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 |  7798 | `			ph7_class *pOrigin = 0;` |
|       - |  7799 | `			SyString *pMName;` |
|      19 |  7800 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 |  7801 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7802 | `				continue;` |
|       - |  7803 | `			}` |
|      17 |  7804 | `			pMName = &pMeth->sFunc.sName;` |
|      17 |  7805 | `			if( nListed > 0 ){` |
|       3 |  7806 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7807 | `			}` |
|       - |  7808 | `			/* Find the origin of this abstract method.` |
|       - |  7809 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7810 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7811 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7812 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7813 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7814 | `			 * class's namespace.` |
|       - |  7815 | `			 */` |
|       - |  7816 | `			{` |
|       - |  7817 | `				ph7_class **apIface;` |
|       - |  7818 | `				ph7_class **apTrait;` |
|       - |  7819 | `				ph7_class *pWalk;` |
|       - |  7820 | `				sxu32 i;` |
|       - |  7821 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7822 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7823 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7824 | `				 */` |
|      17 |  7825 | `				if( pClass->pBase ){` |
|       9 |  7826 | `					pWalk = pClass->pBase;` |
|      17 |  7827 | `					while( pWalk ){` |
|       - |  7828 | `						ph7_class_method *pParentMeth;` |
|      11 |  7829 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 |  7830 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7831 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7832 | `							 * in this class's ancestor chain.` |
|       - |  7833 | `							 */` |
|      11 |  7834 | `							int fromIface = 0;` |
|      11 |  7835 | `							ph7_class *pAnc = pWalk;` |
|      15 |  7836 | `							while( pAnc ){` |
|       - |  7837 | `								ph7_class **apPI;` |
|       - |  7838 | `								sxu32 j;` |
|      13 |  7839 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 |  7840 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 |  7841 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 |  7842 | `										fromIface = 1;` |
|       9 |  7843 | `										break;` |
|       - |  7844 | `									}` |
|     ! 0 |  7845 | `								}` |
|      13 |  7846 | `								if( fromIface ) break;` |
|       5 |  7847 | `								pAnc = pAnc->pBase;` |
|       1 |  7848 | `							}` |
|      11 |  7849 | `							if( !fromIface ){` |
|       3 |  7850 | `								pOrigin = pWalk;` |
|       3 |  7851 | `								break;` |
|       - |  7852 | `							}` |
|       4 |  7853 | `						}` |
|       9 |  7854 | `						pWalk = pWalk->pBase;` |
|       1 |  7855 | `					}` |
|       4 |  7856 | `				}` |
|       - |  7857 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7858 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7859 | `				 */` |
|      17 |  7860 | `				if( !pOrigin ){` |
|      15 |  7861 | `					pWalk = pClass;` |
|      37 |  7862 | `					while( pWalk && !pOrigin ){` |
|      23 |  7863 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 |  7864 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 |  7865 | `							ph7_class *pIface = apIface[i];` |
|      13 |  7866 | `							ph7_class *pDeepest = 0;` |
|      25 |  7867 | `							while( pIface ){` |
|      13 |  7868 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 |  7869 | `									pDeepest = pIface;` |
|       6 |  7870 | `								}` |
|      13 |  7871 | `								pIface = pIface->pBase;` |
|       1 |  7872 | `							}` |
|      13 |  7873 | `							if( pDeepest ){` |
|      13 |  7874 | `								pOrigin = pDeepest;` |
|      13 |  7875 | `								break;` |
|       - |  7876 | `							}` |
|     ! 0 |  7877 | `						}` |
|      23 |  7878 | `						pWalk = pWalk->pBase;` |
|       1 |  7879 | `					}` |
|       7 |  7880 | `				}` |
|       - |  7881 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 |  7882 | `				if( !pOrigin ){` |
|       3 |  7883 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7884 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7885 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7886 | `							pOrigin = pClass;` |
|       3 |  7887 | `							break;` |
|       - |  7888 | `						}` |
|     ! 0 |  7889 | `					}` |
|       1 |  7890 | `				}` |
|       - |  7891 | `			}` |
|      17 |  7892 | `			if( pOrigin ){` |
|      17 |  7893 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 |  7894 | `			}else{` |
|       - |  7895 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7896 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7897 | `			}` |
|      17 |  7898 | `			nListed++;` |
|       1 |  7899 | `		}` |
|       - |  7900 | `	}` |
|      15 |  7901 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 |  7902 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7903 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 |  7904 | `	SyBlobRelease(&sMsg);` |
|      15 |  7905 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7906 | `		return SXERR_ABORT;` |
|       - |  7907 | `	}` |
|      15 |  7908 | `	return SXRET_OK;` |
|   42959 |  7909 |  |
|       - |  7910 | `/*` |
|       - |  7911 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  7912 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  7913 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  7914 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  7915 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  7916 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  7917 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  7918 | ` */` |
|   85628 |  7919 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       2 |  7920 |  |
|   85630 |  7921 | `	int isAbsolute = 0;` |
|   85630 |  7922 | `	SyToken *pStart = pGen->pIn;` |
|       - |  7923 | `	SyBlob sName;` |
|   85630 |  7924 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      30 |  7925 | `		isAbsolute = 1;` |
|      30 |  7926 | `		pGen->pIn++;` |
|      14 |  7927 | `	}` |
|   85630 |  7928 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       7 |  7929 | `		pGen->pIn = pStart;` |
|       7 |  7930 | `		return SXERR_INVALID;` |
|       - |  7931 | `	}` |
|   85624 |  7932 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   85624 |  7933 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   85624 |  7934 | `	pGen->pIn++;` |
|  128451 |  7935 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   42831 |  7936 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  7937 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  7938 | `		pGen->pIn++;` |
|      13 |  7939 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  7940 | `		pGen->pIn++;` |
|       1 |  7941 | `	}` |
|   85624 |  7942 | `	if( isAbsolute ){` |
|      28 |  7943 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      15 |  7944 | `	}else{` |
|       - |  7945 | `		SyString sRaw;` |
|   85598 |  7946 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   85598 |  7947 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  7948 | `	}` |
|   85624 |  7949 | `	SyBlobRelease(&sName);` |
|   85624 |  7950 | `	return SXRET_OK;` |
|   42816 |  7951 |  |
|       - |  7952 | `/*` |
|       - |  7953 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  7954 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  7955 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  7956 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  7957 | ` * either direction cannot run unbounded.` |
|       - |  7958 | ` */` |
|       - |  7959 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    9556 |  7960 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       2 |  7961 |  |
|       - |  7962 | `	ph7_class **apParent;` |
|       - |  7963 | `	sxu32 n;` |
|   15982 |  7964 | `	while( pInterface ){` |
|   12732 |  7965 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  7966 | `			return FALSE;` |
|       - |  7967 | `		}` |
|   15892 |  7968 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    6320 |  7969 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    6308 |  7970 | `			return TRUE;` |
|       - |  7971 | `		}` |
|    6426 |  7972 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    6426 |  7973 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  7974 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  7975 | `				return TRUE;` |
|       - |  7976 | `			}` |
|     ! 0 |  7977 | `		}` |
|    6426 |  7978 | `		pInterface = pInterface->pBase;` |
|    6426 |  7979 | `		iDepth++;` |
|       2 |  7980 | `	}` |
|    3252 |  7981 | `	return FALSE;` |
|    4780 |  7982 |  |
|    9556 |  7983 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       2 |  7984 |  |
|    9558 |  7985 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       2 |  7986 |  |
|       - |  7987 | `/*` |
|       - |  7988 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  7989 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  7990 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  7991 | ` */` |
|    6306 |  7992 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       2 |  7993 |  |
|    6312 |  7994 | `	while( pBase ){` |
|      10 |  7995 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  7996 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  7997 | `			return TRUE;` |
|       - |  7998 | `		}` |
|      10 |  7999 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8000 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8001 | `			return TRUE;` |
|       - |  8002 | `		}` |
|       5 |  8003 | `		pBase = pBase->pBase;` |
|       1 |  8004 | `	}` |
|    6304 |  8005 | `	return FALSE;` |
|    3155 |  8006 |  |
|   85930 |  8007 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 |  8008 |  |
|   85932 |  8009 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8010 | `	ph7_class *pClass,*pBase;` |
|       - |  8011 | `	SyToken *pEnd,*pTmp;` |
|       - |  8012 | `	sxi32 iProtection;` |
|       - |  8013 | `	SySet aInterfaces;` |
|       - |  8014 | `	SySet aUseEntries;` |
|       - |  8015 | `	sxi32 iAttrflags;` |
|       - |  8016 | `	SyString *pName;` |
|       - |  8017 | `	sxi32 nKwrd;` |
|       - |  8018 | `	sxi32 rc;` |
|       - |  8019 | `	/* Jump the 'class' keyword */` |
|   85932 |  8020 | `	pGen->pIn++;` |
|   85932 |  8021 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8022 | `		/* Syntax error */` |
|     ! 0 |  8023 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8024 | `		if( rc == SXERR_ABORT ){` |
|       - |  8025 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8026 | `			return SXERR_ABORT;` |
|       - |  8027 | `		}` |
|       - |  8028 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8029 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8030 | `			pGen->pIn++;` |
|     ! 0 |  8031 | `		}` |
|     ! 0 |  8032 | `		return SXRET_OK;` |
|       - |  8033 | `	}` |
|       - |  8034 | `	/* Extract class name */` |
|   85932 |  8035 | `	pName = &pGen->pIn->sData;` |
|       - |  8036 | `	/* Advance the stream cursor */` |
|   85932 |  8037 | `	pGen->pIn++;` |
|       - |  8038 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8039 | `		SyBlob sFQN;` |
|       - |  8040 | `		SyString sFQNStr;` |
|   85932 |  8041 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   85932 |  8042 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   85932 |  8043 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   85932 |  8044 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   85932 |  8045 | `		SyBlobRelease(&sFQN);` |
|       - |  8046 | `	}` |
|   85932 |  8047 | `	if( pClass == 0 ){` |
|     ! 0 |  8048 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8049 | `		return SXERR_ABORT;` |
|       - |  8050 | `	}` |
|       - |  8051 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   85932 |  8052 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   85932 |  8053 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8054 | `	/* Assume a standalone class */` |
|   85932 |  8055 | `	pBase = 0;` |
|   85932 |  8056 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   75804 |  8057 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   75804 |  8058 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8059 | `			SyBlob sResolved;` |
|       - |  8060 | `			SyString sBaseName;` |
|       - |  8061 | `			sxu32 nRefLine;` |
|   66254 |  8062 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   66254 |  8063 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   66254 |  8064 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   66254 |  8065 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8066 | `				SyBlobRelease(&sResolved);` |
|       4 |  8067 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8068 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8069 | `					pName);` |
|       3 |  8070 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8071 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8072 | `					return SXERR_ABORT;` |
|       - |  8073 | `				}` |
|       3 |  8074 | `				return SXRET_OK;` |
|       - |  8075 | `			}` |
|   99377 |  8076 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   66250 |  8077 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   66252 |  8078 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8079 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8080 | `			/* Interfaces are not allowed */` |
|   66252 |  8081 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8082 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8083 | `			}` |
|   66252 |  8084 | `			if( pBase == 0 ){` |
|     ! 0 |  8085 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8086 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8087 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8088 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8089 | `					return SXERR_ABORT;` |
|       - |  8090 | `				}` |
|     ! 0 |  8091 | `			}else{` |
|   66252 |  8092 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8093 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8094 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8095 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8096 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8097 | `						return SXERR_ABORT;` |
|       - |  8098 | `					}` |
|     ! 0 |  8099 | `				}` |
|       - |  8100 | `			}` |
|   66252 |  8101 | `			SyBlobRelease(&sResolved);` |
|   33125 |  8102 | `		}` |
|   75802 |  8103 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8104 | `			ph7_class *pInterface;` |
|       - |  8105 | `			/* Interface implementation */` |
|    9558 |  8106 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    4778 |  8107 | `			for(;;){` |
|       - |  8108 | `				SyBlob sResolved;` |
|       - |  8109 | `				SyString sIntName;` |
|       - |  8110 | `				sxu32 nRefLine;` |
|    9558 |  8111 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9558 |  8112 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9558 |  8113 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8114 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8115 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8116 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8117 | `						pName);` |
|     ! 0 |  8118 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8119 | `						return SXERR_ABORT;` |
|       - |  8120 | `					}` |
|     ! 0 |  8121 | `					break;` |
|       - |  8122 | `				}` |
|   19114 |  8123 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    9556 |  8124 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9558 |  8125 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8126 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8127 | `				/* Only interfaces are allowed */` |
|    9558 |  8128 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8129 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8130 | `				}` |
|    9558 |  8131 | `				if( pInterface == 0 ){` |
|     ! 0 |  8132 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8133 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8134 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8135 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8136 | `						return SXERR_ABORT;` |
|       - |  8137 | `					}` |
|     ! 0 |  8138 | `				}else{` |
|       - |  8139 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8140 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8141 | `					 * unless they already extend Exception or Error.` |
|       - |  8142 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8143 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8144 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    9558 |  8145 | `					SyString *pFqn = &pClass->sName;` |
|    9558 |  8146 | `					int bIsExceptionOrError =` |
|    7928 |  8147 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   15910 |  8148 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    7987 |  8149 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3158 |  8150 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   15860 |  8151 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    9459 |  8152 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3151 |  8153 | `						!bIsExceptionOrError ){` |
|      10 |  8154 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8155 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8156 | `							&pClass->sName);` |
|       7 |  8157 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8158 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8159 | `							return SXERR_ABORT;` |
|       - |  8160 | `						}` |
|       - |  8161 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8162 | `						 * check does not produce a duplicate fatal. */` |
|       4 |  8163 | `					}else{` |
|    9552 |  8164 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8165 | `					}` |
|       - |  8166 | `				}` |
|    9558 |  8167 | `				SyBlobRelease(&sResolved);` |
|    9558 |  8168 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    4780 |  8169 | `					break;` |
|       - |  8170 | `				}` |
|     ! 0 |  8171 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8172 | `			}` |
|    4778 |  8173 | `		}` |
|   37900 |  8174 | `	}` |
|   85930 |  8175 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8176 | `		/* Syntax error */` |
|     ! 0 |  8177 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8178 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8179 | `		if( rc == SXERR_ABORT ){` |
|       - |  8180 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8181 | `			return SXERR_ABORT;` |
|       - |  8182 | `		}` |
|     ! 0 |  8183 | `		return SXRET_OK;` |
|       - |  8184 | `	}` |
|   85930 |  8185 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   85930 |  8186 | `	pEnd = 0; /* cc warning */` |
|       - |  8187 | `	/* Delimit the class body */` |
|   85930 |  8188 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   85930 |  8189 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8190 | `		/* Syntax error */` |
|     ! 0 |  8191 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8192 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8193 | `		if( rc == SXERR_ABORT ){` |
|       - |  8194 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8195 | `			return SXERR_ABORT;` |
|       - |  8196 | `		}` |
|     ! 0 |  8197 | `		return SXRET_OK;` |
|       - |  8198 | `	}` |
|       - |  8199 | `	/* Swap token stream */` |
|   85930 |  8200 | `	pTmp = pGen->pEnd;` |
|   85930 |  8201 | `	pGen->pEnd = pEnd;` |
|       - |  8202 | `	/* Set the inherited flags */` |
|   85930 |  8203 | `	pClass->iFlags = iFlags;` |
|       - |  8204 | `	/* Start the parse process */` |
|  120477 |  8205 | `	for(;;){` |
|       - |  8206 | `		/* Jump leading/trailing semi-colons */` |
|  361654 |  8207 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   60370 |  8208 | `			pGen->pIn++;` |
|       2 |  8209 | `		}` |
|  301286 |  8210 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8211 | `			/* End of class body */` |
|   85916 |  8212 | `			break;` |
|       - |  8213 | `		}` |
|  215372 |  8214 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8215 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8216 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8217 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8218 | `			if( rc == SXERR_ABORT ){` |
|       - |  8219 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8220 | `				return SXERR_ABORT;` |
|       - |  8221 | `			}` |
|     ! 0 |  8222 | `			goto done;` |
|       - |  8223 | `		}` |
|       - |  8224 | `		/* Assume public visibility */` |
|  215372 |  8225 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  215372 |  8226 | `		iAttrflags = 0;` |
|  215372 |  8227 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  8228 | `			/* Extract the current keyword */` |
|  215372 |  8229 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  215372 |  8230 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8231 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8232 | `				TraitUseEntry sUse;` |
|      46 |  8233 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      46 |  8234 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      46 |  8235 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      30 |  8236 | `				for(;;){` |
|       - |  8237 | `					ph7_class *pTrait;` |
|       - |  8238 | `					SyString *pTraitName;` |
|      54 |  8239 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8240 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8241 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8242 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8243 | `							return SXERR_ABORT;` |
|       - |  8244 | `						}` |
|     ! 0 |  8245 | `						break;` |
|       - |  8246 | `					}` |
|      54 |  8247 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8248 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8249 | `						SyBlob sResolved;` |
|      54 |  8250 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      54 |  8251 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     106 |  8252 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      52 |  8253 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      54 |  8254 | `						SyBlobRelease(&sResolved);` |
|       - |  8255 | `					}` |
|       - |  8256 | `					/* Only traits are allowed */` |
|      54 |  8257 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8258 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8259 | `					}` |
|      54 |  8260 | `					if( pTrait == 0 ){` |
|     ! 0 |  8261 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8262 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8263 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8264 | `							return SXERR_ABORT;` |
|       - |  8265 | `						}` |
|     ! 0 |  8266 | `					}else{` |
|      54 |  8267 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8268 | `					}` |
|      54 |  8269 | `					pGen->pIn++; /* Advance past trait name */` |
|      54 |  8270 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      24 |  8271 | `						break;` |
|       - |  8272 | `					}` |
|       9 |  8273 | `					pGen->pIn++; /* Jump the comma */` |
|       1 |  8274 | `				}` |
|       - |  8275 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      46 |  8276 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8277 | `					SyToken *pBlock;` |
|       9 |  8278 | `					pGen->pIn++; /* Jump '{' */` |
|       9 |  8279 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 |  8280 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 |  8281 | `					sUse.pResolvEnd = pBlock;` |
|       9 |  8282 | `					if( pBlock < pGen->pEnd ){` |
|       9 |  8283 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 |  8284 | `					}else{` |
|     ! 0 |  8285 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8286 | `					}` |
|       4 |  8287 | `				}` |
|      46 |  8288 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8289 | `				/* The semicolon will be consumed by the outer loop */` |
|      46 |  8290 | `				continue;` |
|       - |  8291 | `			}` |
|  215328 |  8292 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  212056 |  8293 | `				iProtection = nKwrd;` |
|  212056 |  8294 | `				pGen->pIn++; /* Jump the visibility token */` |
|  212054 |  8295 | `				if( pGen->pIn >= pGen->pEnd` |
|  212056 |  8296 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8297 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8298 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8299 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8300 | `					if( rc == SXERR_ABORT ){` |
|       - |  8301 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8302 | `						return SXERR_ABORT;` |
|       - |  8303 | `					}` |
|     ! 0 |  8304 | `					goto done;` |
|       - |  8305 | `				}` |
|  212056 |  8306 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8307 | `					/* Attribute declaration (untyped) */` |
|   60160 |  8308 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   60160 |  8309 | `					if( rc != SXRET_OK ){` |
|       3 |  8310 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8311 | `							return SXERR_ABORT;` |
|       - |  8312 | `						}` |
|       3 |  8313 | `						goto done;` |
|       - |  8314 | `					}` |
|   60158 |  8315 | `					continue;` |
|       - |  8316 | `				}` |
|  151898 |  8317 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8318 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     120 |  8319 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     120 |  8320 | `					if( rc != SXRET_OK ){` |
|       3 |  8321 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8322 | `							return SXERR_ABORT;` |
|       - |  8323 | `						}` |
|       3 |  8324 | `						goto done;` |
|       - |  8325 | `					}` |
|     118 |  8326 | `					continue;` |
|       - |  8327 | `				}` |
|       - |  8328 | `				/* Extract the keyword */` |
|  151780 |  8329 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   75889 |  8330 | `			}` |
|  155052 |  8331 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8332 | `				/* Process constant declaration */` |
|      32 |  8333 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      32 |  8334 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8335 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8336 | `						return SXERR_ABORT;` |
|       - |  8337 | `					}` |
|     ! 0 |  8338 | `					goto done;` |
|       - |  8339 | `				}` |
|      17 |  8340 | `			}else{` |
|  155022 |  8341 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8342 | `					/* Static method or attribute,record that */` |
|    3190 |  8343 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3190 |  8344 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3190 |  8345 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8346 | `						/* Extract the keyword */` |
|    3184 |  8347 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3184 |  8348 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8349 | `							iProtection = nKwrd;` |
|     ! 0 |  8350 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8351 | `						}` |
|    1591 |  8352 | `					}` |
|    3188 |  8353 | `					if( pGen->pIn >= pGen->pEnd` |
|    3190 |  8354 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8355 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8356 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8357 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8358 | `						if( rc == SXERR_ABORT ){` |
|       - |  8359 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8360 | `							return SXERR_ABORT;` |
|       - |  8361 | `						}` |
|     ! 0 |  8362 | `						goto done;` |
|       - |  8363 | `					}` |
|    3190 |  8364 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8365 | `						/* Attribute declaration */` |
|       5 |  8366 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8367 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8368 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8369 | `								return SXERR_ABORT;` |
|       - |  8370 | `							}` |
|     ! 0 |  8371 | `							goto done;` |
|       - |  8372 | `						}` |
|       5 |  8373 | `						continue;` |
|       - |  8374 | `					}` |
|    3186 |  8375 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8376 | `						/* Typed static attribute declaration */` |
|      12 |  8377 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  8378 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8379 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8380 | `								return SXERR_ABORT;` |
|       - |  8381 | `							}` |
|     ! 0 |  8382 | `							goto done;` |
|       - |  8383 | `						}` |
|      12 |  8384 | `						continue;` |
|       - |  8385 | `					}` |
|       - |  8386 | `					/* Extract the keyword */` |
|    3176 |  8387 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  153421 |  8388 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8389 | `					/* Abstract method,record that */` |
|      12 |  8390 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8391 | `					/* Mark the whole class as abstract */` |
|      12 |  8392 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8393 | `					/* Advance the stream cursor */` |
|      12 |  8394 | `					pGen->pIn++;` |
|      12 |  8395 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8396 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8397 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8398 | `							iProtection = nKwrd;` |
|      10 |  8399 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8400 | `						}` |
|       5 |  8401 | `					}` |
|      12 |  8402 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8403 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8404 | `							/* Static method */` |
|     ! 0 |  8405 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8406 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8407 | `					}` |
|      12 |  8408 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8409 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8410 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8411 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8412 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8413 | `							if( rc == SXERR_ABORT ){` |
|       - |  8414 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8415 | `								return SXERR_ABORT;` |
|       - |  8416 | `							}` |
|     ! 0 |  8417 | `							goto done;` |
|       - |  8418 | `					}` |
|      12 |  8419 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  151829 |  8420 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8421 | `					/* final method ,record that */` |
|       5 |  8422 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 |  8423 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 |  8424 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8425 | `						/* Extract the keyword */` |
|       5 |  8426 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8427 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8428 | `							iProtection = nKwrd;` |
|       5 |  8429 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  8430 | `						}` |
|       2 |  8431 | `					}` |
|       5 |  8432 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8433 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8434 | `							/* Static method */` |
|     ! 0 |  8435 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8436 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8437 | `					}` |
|       5 |  8438 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8439 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8440 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8441 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8442 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8443 | `							if( rc == SXERR_ABORT ){` |
|       - |  8444 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8445 | `								return SXERR_ABORT;` |
|       - |  8446 | `							}` |
|     ! 0 |  8447 | `							goto done;` |
|       - |  8448 | `					}` |
|       5 |  8449 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8450 | `				}` |
|  155008 |  8451 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8452 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8453 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8454 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8455 | `						if( rc == SXERR_ABORT ){` |
|       - |  8456 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8457 | `							return SXERR_ABORT;` |
|       - |  8458 | `						}` |
|     ! 0 |  8459 | `						goto done;` |
|       - |  8460 | `				}` |
|  155008 |  8461 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8462 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8463 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8464 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8465 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8466 | `						if( rc == SXERR_ABORT ){` |
|       - |  8467 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8468 | `							return SXERR_ABORT;` |
|       - |  8469 | `						}` |
|     ! 0 |  8470 | `						goto done;` |
|       - |  8471 | `					}` |
|       - |  8472 | `					/* Attribute declaration */` |
|       7 |  8473 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8474 | `				}else{` |
|       - |  8475 | `					/* Process method declaration */` |
|  155002 |  8476 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8477 | `				}` |
|  155008 |  8478 | `				if( rc != SXRET_OK ){` |
|      11 |  8479 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8480 | `						return SXERR_ABORT;` |
|       - |  8481 | `					}` |
|      11 |  8482 | `					goto done;` |
|       - |  8483 | `				}` |
|       - |  8484 | `			}` |
|   77515 |  8485 | `		}else{` |
|       - |  8486 | `			/* Attribute declaration */` |
|     ! 0 |  8487 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8488 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8489 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8490 | `					return SXERR_ABORT;` |
|       - |  8491 | `				}` |
|     ! 0 |  8492 | `				goto done;` |
|       - |  8493 | `			}` |
|       - |  8494 | `		}` |
|       2 |  8495 | `	}` |
|       - |  8496 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8497 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8498 | `	 */` |
|       - |  8499 | `	{` |
|       - |  8500 | `		TraitUseEntry *apUse;` |
|       - |  8501 | `		sxu32 nU;` |
|   85916 |  8502 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   85960 |  8503 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      46 |  8504 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      46 |  8505 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      46 |  8506 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      46 |  8507 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8508 | `			sxu32 nT;` |
|      46 |  8509 | `			if( !hasResolution ){` |
|       - |  8510 | `				/* No conflict resolution block: use standard trait application */` |
|      80 |  8511 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      44 |  8512 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      44 |  8513 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8514 | `						break;` |
|       - |  8515 | `					}` |
|      23 |  8516 | `				}` |
|      20 |  8517 | `			}else{` |
|       - |  8518 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8519 | `				 * then use the block to resolve method conflicts.` |
|       - |  8520 | `				 */` |
|       - |  8521 | `				SyToken *pR;` |
|      19 |  8522 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 |  8523 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8524 | `					ph7_class_attr *pAR;` |
|       - |  8525 | `					SyHashEntry *pER;` |
|       - |  8526 | `					SyString *pNR;` |
|      11 |  8527 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 |  8528 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8529 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8530 | `						pNR = &pAR->sName;` |
|     ! 0 |  8531 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8532 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8533 | `						}` |
|     ! 0 |  8534 | `					}` |
|      11 |  8535 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 |  8536 | `				}` |
|       - |  8537 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 |  8538 | `				pR = pUse->pResolvStart;` |
|      21 |  8539 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8540 | `					SyString sTrait,sMethod;` |
|       - |  8541 | `					ph7_class *pSrcTrait;` |
|       - |  8542 | `					ph7_class_method *pMeth;` |
|       - |  8543 | `					sxi32 nRKwrd;` |
|      33 |  8544 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8545 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8546 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8547 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8548 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8549 | `					sMethod = pR->sData;` |
|      13 |  8550 | `					pR++;` |
|      13 |  8551 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8552 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8553 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8554 | `							sTrait = sMethod;` |
|       7 |  8555 | `							pR++;` |
|       7 |  8556 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8557 | `							sMethod = pR->sData;` |
|       7 |  8558 | `							pR++;` |
|       3 |  8559 | `						}` |
|       3 |  8560 | `					}` |
|      13 |  8561 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8562 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8563 | `						continue;` |
|       - |  8564 | `					}` |
|      13 |  8565 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8566 | `					pR++;` |
|      13 |  8567 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8568 | `						pSrcTrait = 0;` |
|       7 |  8569 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8570 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8571 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8572 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8573 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8574 | `								break;` |
|       - |  8575 | `							}` |
|       2 |  8576 | `						}` |
|       5 |  8577 | `						if( pSrcTrait ){` |
|       5 |  8578 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8579 | `							if( pMeth ){` |
|       5 |  8580 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8581 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8582 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8583 | `								}` |
|       2 |  8584 | `							}` |
|       2 |  8585 | `						}` |
|       2 |  8586 | `					}` |
|      29 |  8587 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8588 | `				}` |
|       - |  8589 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 |  8590 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8591 | `					ph7_class_method *pMR;` |
|       - |  8592 | `					SyHashEntry *pER;` |
|       - |  8593 | `					SyString *pNR;` |
|      11 |  8594 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 |  8595 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 |  8596 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 |  8597 | `						pNR = &pMR->sFunc.sName;` |
|      19 |  8598 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8599 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8600 | `						}` |
|       1 |  8601 | `					}` |
|       6 |  8602 | `				}` |
|       - |  8603 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 |  8604 | `				pR = pUse->pResolvStart;` |
|      21 |  8605 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8606 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8607 | `					ph7_class *pSrcTrait;` |
|       - |  8608 | `					ph7_class_method *pMeth;` |
|      21 |  8609 | `					int hasQual = 0;` |
|       - |  8610 | `					sxi32 nRKwrd;` |
|      33 |  8611 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8612 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8613 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8614 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8615 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 |  8616 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8617 | `					sMethod = pR->sData;` |
|      13 |  8618 | `					pR++;` |
|      13 |  8619 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8620 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8621 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8622 | `							sTrait = sMethod;` |
|       7 |  8623 | `							hasQual = 1;` |
|       7 |  8624 | `							pR++;` |
|       7 |  8625 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8626 | `							sMethod = pR->sData;` |
|       7 |  8627 | `							pR++;` |
|       3 |  8628 | `						}` |
|       3 |  8629 | `					}` |
|      13 |  8630 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8631 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8632 | `						continue;` |
|       - |  8633 | `					}` |
|      13 |  8634 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8635 | `					pR++;` |
|      13 |  8636 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 |  8637 | `						sxi32 iNewVis = -1;` |
|       9 |  8638 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8639 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8640 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8641 | `								iNewVis = nAK;` |
|       7 |  8642 | `								pR++;` |
|       3 |  8643 | `							}` |
|       3 |  8644 | `						}` |
|       9 |  8645 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 |  8646 | `							sAlias = pR->sData;` |
|       7 |  8647 | `							pR++;` |
|       3 |  8648 | `						}` |
|       9 |  8649 | `						pMeth = 0;` |
|       9 |  8650 | `						if( hasQual ){` |
|       3 |  8651 | `							pSrcTrait = 0;` |
|       5 |  8652 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8653 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8654 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8655 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8656 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8657 | `									break;` |
|       - |  8658 | `								}` |
|       2 |  8659 | `							}` |
|       3 |  8660 | `							if( pSrcTrait ){` |
|       3 |  8661 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8662 | `							}` |
|       2 |  8663 | `						}else{` |
|       7 |  8664 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8665 | `						}` |
|       9 |  8666 | `						if( pMeth ){` |
|       9 |  8667 | `							if( sAlias.nByte > 0 ){` |
|       - |  8668 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8669 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8670 | `								 */` |
|       - |  8671 | `								ph7_class_method *pAlias;` |
|       - |  8672 | `								char *zAliasDup;` |
|       7 |  8673 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 |  8674 | `								if( pAlias ){` |
|       7 |  8675 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 |  8676 | `									if( iNewVis >= 0 ){` |
|       5 |  8677 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8678 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8679 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8680 | `									}` |
|       7 |  8681 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 |  8682 | `									if( zAliasDup ){` |
|       7 |  8683 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8684 | `									}` |
|       4 |  8685 | `								}` |
|       6 |  8686 | `							}else if( iNewVis >= 0 ){` |
|       - |  8687 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8688 | `								ph7_class_method *pCopy;` |
|       3 |  8689 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8690 | `								if( pCopy ){` |
|       3 |  8691 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8692 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8693 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8694 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8695 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8696 | `									/* Replace the method in the class hash */` |
|       3 |  8697 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8698 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8699 | `								}` |
|       1 |  8700 | `							}` |
|       4 |  8701 | `						}` |
|       4 |  8702 | `						SXUNUSED(hasQual);` |
|       4 |  8703 | `					}` |
|      17 |  8704 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8705 | `				}` |
|       - |  8706 | `			}` |
|      46 |  8707 | `			SySetRelease(&pUse->aTraits);` |
|      24 |  8708 | `		}` |
|       - |  8709 | `	}` |
|       - |  8710 | `	/* Install the class */` |
|   85916 |  8711 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   85916 |  8712 | `	if( rc == SXRET_OK ){` |
|       - |  8713 | `		ph7_class **apInterface;` |
|       - |  8714 | `		sxu32 n;` |
|   85916 |  8715 | `		if( pBase ){` |
|       - |  8716 | `			/* Inherit from base class and mark as a subclass */` |
|   66252 |  8717 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   33125 |  8718 | `		}` |
|   85916 |  8719 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   95466 |  8720 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8721 | `			/* Implements one or more interface */` |
|    9552 |  8722 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    9552 |  8723 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8724 | `				break;` |
|       - |  8725 | `			}` |
|    4777 |  8726 | `		}` |
|       - |  8727 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  8728 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  128871 |  8729 | `		if( rc == SXRET_OK` |
|   85914 |  8730 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   85916 |  8731 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   75636 |  8732 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  8733 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   75636 |  8734 | `			if( pStringable ){` |
|   75636 |  8735 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   75636 |  8736 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  8737 | `				sxu32 i;` |
|   75636 |  8738 | `				int bAlready = 0;` |
|   81936 |  8739 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    6304 |  8740 | `					if( apImpl[i] == pStringable ){` |
|       3 |  8741 | `						bAlready = 1;` |
|       3 |  8742 | `						break;` |
|       - |  8743 | `					}` |
|    3152 |  8744 | `				}` |
|   75636 |  8745 | `				if( !bAlready ){` |
|   75634 |  8746 | `					PH7_ClassImplement(pClass,pStringable);` |
|   37816 |  8747 | `				}` |
|   37817 |  8748 | `			}` |
|   37817 |  8749 | `		}` |
|       - |  8750 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   85916 |  8751 | `		if( rc == SXRET_OK ){` |
|   85916 |  8752 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   85916 |  8753 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8754 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8755 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8756 | `				return SXERR_ABORT;` |
|       - |  8757 | `			}` |
|   42957 |  8758 | `		}` |
|       - |  8759 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   85916 |  8760 | `		if( rc == SXRET_OK ){` |
|   85916 |  8761 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   85916 |  8762 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8763 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8764 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8765 | `				return SXERR_ABORT;` |
|       - |  8766 | `			}` |
|   42957 |  8767 | `		}` |
|   42957 |  8768 | `	}` |
|   85916 |  8769 | `	SySetRelease(&aUseEntries);` |
|   85916 |  8770 | `	SySetRelease(&aInterfaces);` |
|   85916 |  8771 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8772 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8773 | `		return SXERR_ABORT;` |
|       - |  8774 | `	}` |
|   42957 |  8775 | `done:` |
|       - |  8776 | `	/* Point beyond the class body */` |
|   85930 |  8777 | `	pGen->pIn = &pEnd[1];` |
|   85930 |  8778 | `	pGen->pEnd = pTmp;` |
|   85930 |  8779 | `	return PH7_OK;` |
|   42967 |  8780 |  |
|       - |  8781 | `/*` |
|       - |  8782 | ` * Compile a user-defined abstract class.` |
|       - |  8783 | ` *  According to the PHP language reference manual` |
|       - |  8784 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8785 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8786 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8787 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8788 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8789 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8790 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8791 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8792 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8793 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8794 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8795 | ` *   could differ.` |
|       - |  8796 | ` */` |
|      20 |  8797 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 |  8798 |  |
|       - |  8799 | `	sxi32 rc;` |
|      22 |  8800 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      22 |  8801 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      22 |  8802 | `	return rc;` |
|       2 |  8803 |  |
|       - |  8804 | `/*` |
|       - |  8805 | ` * Compile a user-defined final class.` |
|       - |  8806 | ` *  According to the PHP language reference manual` |
|       - |  8807 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8808 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8809 | ` *    final then it cannot be extended.` |
|       - |  8810 | ` */` |
|       2 |  8811 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8812 |  |
|       - |  8813 | `	sxi32 rc;` |
|       3 |  8814 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8815 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8816 | `	return rc;` |
|       1 |  8817 |  |
|       - |  8818 | `/*` |
|       - |  8819 | ` * Compile a user-defined trait.` |
|       - |  8820 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8821 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8822 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8823 | ` */` |
|      56 |  8824 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 |  8825 |  |
|      58 |  8826 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8827 | `	ph7_class *pClass;` |
|       - |  8828 | `	SyToken *pEnd,*pTmp;` |
|       - |  8829 | `	sxi32 iProtection;` |
|       - |  8830 | `	sxi32 iAttrflags;` |
|       - |  8831 | `	SyString *pName;` |
|       - |  8832 | `	sxi32 nKwrd;` |
|       - |  8833 | `	sxi32 rc;` |
|       - |  8834 | `	/* Jump the 'trait' keyword */` |
|      58 |  8835 | `	pGen->pIn++;` |
|      58 |  8836 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8837 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8838 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8839 | `			return SXERR_ABORT;` |
|       - |  8840 | `		}` |
|     ! 0 |  8841 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8842 | `			pGen->pIn++;` |
|     ! 0 |  8843 | `		}` |
|     ! 0 |  8844 | `		return SXRET_OK;` |
|       - |  8845 | `	}` |
|       - |  8846 | `	/* Extract trait name */` |
|      58 |  8847 | `	pName = &pGen->pIn->sData;` |
|      58 |  8848 | `	pGen->pIn++;` |
|       - |  8849 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8850 | `		SyBlob sFQN;` |
|       - |  8851 | `		SyString sFQNStr;` |
|      58 |  8852 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      58 |  8853 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      58 |  8854 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      58 |  8855 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      58 |  8856 | `		SyBlobRelease(&sFQN);` |
|       - |  8857 | `	}` |
|      58 |  8858 | `	if( pClass == 0 ){` |
|     ! 0 |  8859 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8860 | `		return SXERR_ABORT;` |
|       - |  8861 | `	}` |
|       - |  8862 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      58 |  8863 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8864 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8865 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8866 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8867 | `			return SXERR_ABORT;` |
|       - |  8868 | `		}` |
|     ! 0 |  8869 | `		return SXRET_OK;` |
|       - |  8870 | `	}` |
|      58 |  8871 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      58 |  8872 | `	pEnd = 0;` |
|      58 |  8873 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      58 |  8874 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8875 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8876 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8877 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8878 | `			return SXERR_ABORT;` |
|       - |  8879 | `		}` |
|     ! 0 |  8880 | `		return SXRET_OK;` |
|       - |  8881 | `	}` |
|       - |  8882 | `	/* Swap token stream */` |
|      58 |  8883 | `	pTmp = pGen->pEnd;` |
|      58 |  8884 | `	pGen->pEnd = pEnd;` |
|       - |  8885 | `	/* Mark as trait */` |
|      58 |  8886 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8887 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      56 |  8888 | `	for(;;){` |
|     158 |  8889 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 |  8890 | `			pGen->pIn++;` |
|       2 |  8891 | `		}` |
|     134 |  8892 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      58 |  8893 | `			break;` |
|       - |  8894 | `		}` |
|      78 |  8895 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8896 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8897 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8898 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8899 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8900 | `				return SXERR_ABORT;` |
|       - |  8901 | `			}` |
|     ! 0 |  8902 | `			goto done;` |
|       - |  8903 | `		}` |
|      78 |  8904 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      78 |  8905 | `		iAttrflags = 0;` |
|      78 |  8906 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      78 |  8907 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      78 |  8908 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8909 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8910 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8911 | `				for(;;){` |
|       - |  8912 | `					ph7_class *pUsedTrait;` |
|       - |  8913 | `					SyString *pUsedName;` |
|       5 |  8914 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8915 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8916 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8917 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8918 | `							return SXERR_ABORT;` |
|       - |  8919 | `						}` |
|     ! 0 |  8920 | `						break;` |
|       - |  8921 | `					}` |
|       5 |  8922 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8923 | `					{` |
|       - |  8924 | `						SyBlob sResolved;` |
|       5 |  8925 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8926 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8927 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8928 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8929 | `						SyBlobRelease(&sResolved);` |
|       - |  8930 | `					}` |
|       5 |  8931 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8932 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8933 | `					}` |
|       5 |  8934 | `					if( pUsedTrait == 0 ){` |
|       4 |  8935 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8936 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8937 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8938 | `							return SXERR_ABORT;` |
|       - |  8939 | `						}` |
|       2 |  8940 | `					}else{` |
|       3 |  8941 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8942 | `					}` |
|       5 |  8943 | `					pGen->pIn++;` |
|       5 |  8944 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8945 | `						break;` |
|       - |  8946 | `					}` |
|     ! 0 |  8947 | `					pGen->pIn++;` |
|     ! 0 |  8948 | `				}` |
|       5 |  8949 | `				continue;` |
|       - |  8950 | `			}` |
|      74 |  8951 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      70 |  8952 | `				iProtection = nKwrd;` |
|      70 |  8953 | `				pGen->pIn++;` |
|      68 |  8954 | `				if( pGen->pIn >= pGen->pEnd` |
|      70 |  8955 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8956 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8957 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8958 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8959 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8960 | `						return SXERR_ABORT;` |
|       - |  8961 | `					}` |
|     ! 0 |  8962 | `					goto done;` |
|       - |  8963 | `				}` |
|      70 |  8964 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 |  8965 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  8966 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8967 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8968 | `							return SXERR_ABORT;` |
|       - |  8969 | `						}` |
|     ! 0 |  8970 | `						goto done;` |
|       - |  8971 | `					}` |
|      11 |  8972 | `					continue;` |
|       - |  8973 | `				}` |
|      60 |  8974 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8975 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8976 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8977 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8978 | `							return SXERR_ABORT;` |
|       - |  8979 | `						}` |
|     ! 0 |  8980 | `						goto done;` |
|       - |  8981 | `					}` |
|       5 |  8982 | `					continue;` |
|       - |  8983 | `				}` |
|      55 |  8984 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  8985 | `			}` |
|      59 |  8986 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8987 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8988 | `					"Traits cannot have constants");` |
|     ! 0 |  8989 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8990 | `					return SXERR_ABORT;` |
|       - |  8991 | `				}` |
|     ! 0 |  8992 | `				goto done;` |
|     ! 0 |  8993 | `			}else{` |
|      59 |  8994 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  8995 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  8996 | `					pGen->pIn++;` |
|       5 |  8997 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  8998 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  8999 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9000 | `							iProtection = nKwrd;` |
|     ! 0 |  9001 | `							pGen->pIn++;` |
|     ! 0 |  9002 | `						}` |
|       1 |  9003 | `					}` |
|       4 |  9004 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9005 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9006 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9007 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9008 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9009 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9010 | `							return SXERR_ABORT;` |
|       - |  9011 | `						}` |
|     ! 0 |  9012 | `						goto done;` |
|       - |  9013 | `					}` |
|       5 |  9014 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9015 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9016 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9017 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9018 | `								return SXERR_ABORT;` |
|       - |  9019 | `							}` |
|     ! 0 |  9020 | `							goto done;` |
|       - |  9021 | `						}` |
|       3 |  9022 | `						continue;` |
|       - |  9023 | `					}` |
|       3 |  9024 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9025 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9026 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9027 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9028 | `								return SXERR_ABORT;` |
|       - |  9029 | `							}` |
|     ! 0 |  9030 | `							goto done;` |
|       - |  9031 | `						}` |
|     ! 0 |  9032 | `						continue;` |
|       - |  9033 | `					}` |
|       3 |  9034 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      56 |  9035 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 |  9036 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 |  9037 | `					pGen->pIn++;` |
|       5 |  9038 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 |  9039 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  9040 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  9041 | `							iProtection = nKwrd;` |
|       5 |  9042 | `							pGen->pIn++;` |
|       2 |  9043 | `						}` |
|       2 |  9044 | `					}` |
|       5 |  9045 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9046 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9047 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9048 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9049 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9050 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9051 | `							return SXERR_ABORT;` |
|       - |  9052 | `						}` |
|     ! 0 |  9053 | `						goto done;` |
|       - |  9054 | `					}` |
|       5 |  9055 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9056 | `				}` |
|      57 |  9057 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9058 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9059 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9060 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9061 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9062 | `						return SXERR_ABORT;` |
|       - |  9063 | `					}` |
|     ! 0 |  9064 | `					goto done;` |
|       - |  9065 | `				}` |
|      57 |  9066 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9067 | `					pGen->pIn++;` |
|     ! 0 |  9068 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9069 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9070 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9071 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9072 | `							return SXERR_ABORT;` |
|       - |  9073 | `						}` |
|     ! 0 |  9074 | `						goto done;` |
|       - |  9075 | `					}` |
|     ! 0 |  9076 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9077 | `				}else{` |
|      57 |  9078 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9079 | `				}` |
|      57 |  9080 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9081 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9082 | `						return SXERR_ABORT;` |
|       - |  9083 | `					}` |
|     ! 0 |  9084 | `					goto done;` |
|       - |  9085 | `				}` |
|       - |  9086 | `			}` |
|      29 |  9087 | `		}else{` |
|     ! 0 |  9088 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9089 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9090 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9091 | `					return SXERR_ABORT;` |
|       - |  9092 | `				}` |
|     ! 0 |  9093 | `				goto done;` |
|       - |  9094 | `			}` |
|       - |  9095 | `		}` |
|       1 |  9096 | `	}` |
|       - |  9097 | `	/* Install the trait */` |
|      58 |  9098 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      58 |  9099 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9100 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9101 | `		return SXERR_ABORT;` |
|       - |  9102 | `	}` |
|      28 |  9103 | `done:` |
|       - |  9104 | `	/* Point beyond the trait body */` |
|      58 |  9105 | `	pGen->pIn = &pEnd[1];` |
|      58 |  9106 | `	pGen->pEnd = pTmp;` |
|      58 |  9107 | `	return PH7_OK;` |
|      30 |  9108 |  |
|       - |  9109 | `/*` |
|       - |  9110 | ` * Compile a user-defined class.` |
|       - |  9111 | ` *  According to the PHP language reference manual` |
|       - |  9112 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9113 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9114 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9115 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9116 | ` *   and functions (called "methods").` |
|       - |  9117 | ` */` |
|   85908 |  9118 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 |  9119 |  |
|       - |  9120 | `	sxi32 rc;` |
|   85910 |  9121 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   85910 |  9122 | `	return rc;` |
|       2 |  9123 |  |
|       - |  9124 | `/*` |
|       - |  9125 | ` * Exception handling.` |
|       - |  9126 | ` *  According to the PHP language reference manual` |
|       - |  9127 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9128 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9129 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9130 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9131 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9132 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9133 | ` *    (or re-thrown) within a catch block.` |
|       - |  9134 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9135 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9136 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9137 | ` *    been defined with set_exception_handler().` |
|       - |  9138 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9139 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9140 | ` */` |
|       - |  9141 | `/*` |
|       - |  9142 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9143 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9144 | ` * indicates failure.` |
|       - |  9145 | ` */` |
|    9602 |  9146 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  9147 |  |
|    9604 |  9148 | `	sxi32 rc = SXRET_OK;` |
|    9604 |  9149 | `	if( pRoot->pOp ){` |
|    9596 |  9150 | `		switch( pRoot->pOp->iOp ){` |
|    4797 |  9151 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9152 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9153 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9154 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9155 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9156 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    9596 |  9157 | `			break;` |
|     ! 0 |  9158 | `		default:` |
|       - |  9159 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9160 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9161 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9162 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9163 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9164 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9165 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9166 | `			}` |
|     ! 0 |  9167 | `			break;` |
|       - |  9168 | `		}` |
|    4807 |  9169 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9170 | `		/* Unexpected expression */` |
|     ! 0 |  9171 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9172 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9173 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9174 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9175 | `		}` |
|     ! 0 |  9176 | `	}` |
|    9604 |  9177 | `	return rc;` |
|       2 |  9178 |  |
|       - |  9179 | `/*` |
|       - |  9180 | ` * Compile a 'throw' statement.` |
|       - |  9181 | ` * throw: This is how you trigger an exception.` |
|       - |  9182 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9183 | ` */` |
|    9566 |  9184 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 |  9185 |  |
|    9568 |  9186 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9187 | `	GenBlock *pBlock;` |
|       - |  9188 | `	sxu32 nIdx;` |
|       - |  9189 | `	sxi32 rc;` |
|    9568 |  9190 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9191 | `	/* Compile the expression */` |
|    9568 |  9192 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    9568 |  9193 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9194 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9195 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9196 | `			return SXERR_ABORT;` |
|       - |  9197 | `		}` |
|     ! 0 |  9198 | `		return SXRET_OK;` |
|       - |  9199 | `	}` |
|    9568 |  9200 | `	pBlock = pGen->pCurrent;` |
|       - |  9201 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   44318 |  9202 | `	while(pBlock->pParent){` |
|   44314 |  9203 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    9564 |  9204 | `			break;` |
|       - |  9205 | `		}` |
|       - |  9206 | `		/* Point to the parent block */` |
|   34752 |  9207 | `		pBlock = pBlock->pParent;` |
|       2 |  9208 | `	}` |
|       - |  9209 | `	/* Emit the throw instruction */` |
|    9568 |  9210 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9211 | `	/* Emit the jump */` |
|    9568 |  9212 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    9568 |  9213 | `	return SXRET_OK;` |
|    4785 |  9214 |  |
|       - |  9215 | `/*` |
|       - |  9216 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9217 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9218 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9219 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9220 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9221 | ` */` |
|      36 |  9222 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9223 |  |
|      38 |  9224 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9225 | `	GenBlock *pBlock;` |
|       - |  9226 | `	sxu32 nIdx;` |
|       - |  9227 | `	sxi32 rc;` |
|      18 |  9228 | `	(void)iCompileFlag;` |
|      38 |  9229 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9230 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9231 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9232 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9233 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9234 | `			return SXERR_ABORT;` |
|       - |  9235 | `		}` |
|     ! 0 |  9236 | `		return SXRET_OK;` |
|       - |  9237 | `	}` |
|      38 |  9238 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9239 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9240 | `		return SXERR_ABORT;` |
|       - |  9241 | `	}` |
|      38 |  9242 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9243 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9244 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9245 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9246 | `			return SXERR_ABORT;` |
|       - |  9247 | `		}` |
|     ! 0 |  9248 | `		return SXRET_OK;` |
|       - |  9249 | `	}` |
|       - |  9250 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9251 | `	pBlock = pGen->pCurrent;` |
|      60 |  9252 | `	while( pBlock->pParent ){` |
|      49 |  9253 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9254 | `			break;` |
|       - |  9255 | `		}` |
|      23 |  9256 | `		pBlock = pBlock->pParent;` |
|       1 |  9257 | `	}` |
|      38 |  9258 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9259 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9260 | `	return SXRET_OK;` |
|      20 |  9261 |  |
|       - |  9262 | `/*` |
|       - |  9263 | ` * Compile a 'catch' block.` |
|       - |  9264 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9265 | ` * an object containing the exception information.` |
|       - |  9266 | ` */` |
|     342 |  9267 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 |  9268 |  |
|     344 |  9269 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9270 | `	ph7_exception_block sCatch;` |
|       - |  9271 | `	SySet *pInstrContainer;` |
|       - |  9272 | `	SyString sClassName;` |
|       - |  9273 | `	GenBlock *pCatch;` |
|       - |  9274 | `	SyToken *pToken;` |
|       - |  9275 | `	SyString *pName;` |
|       - |  9276 | `	char *zDup;` |
|       - |  9277 | `	sxi32 rc;` |
|     344 |  9278 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9279 | `	/* Zero the structure */` |
|     344 |  9280 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9281 | `	/* Initialize fields */` |
|     344 |  9282 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     344 |  9283 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     344 |  9284 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9285 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9286 | `			pToken = pGen->pIn;` |
|     ! 0 |  9287 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9288 | `				pToken--;` |
|     ! 0 |  9289 | `			}` |
|     ! 0 |  9290 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9291 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9292 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9293 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9294 | `				return SXERR_ABORT;` |
|       - |  9295 | `			}` |
|     ! 0 |  9296 | `			return SXERR_INVALID;` |
|       - |  9297 | `	}` |
|       - |  9298 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     344 |  9299 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     184 |  9300 | `	for(;;){` |
|       - |  9301 | `		SyBlob sResolved;` |
|     370 |  9302 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     370 |  9303 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       5 |  9304 | `			SyBlobRelease(&sResolved);` |
|       5 |  9305 | `			pToken = pGen->pIn;` |
|       5 |  9306 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9307 | `				pToken--;` |
|     ! 0 |  9308 | `			}` |
|       7 |  9309 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9310 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9311 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 |  9312 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9313 | `				return SXERR_ABORT;` |
|       - |  9314 | `			}` |
|       5 |  9315 | `			return SXERR_INVALID;` |
|       - |  9316 | `		}` |
|       - |  9317 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9318 | `		 * transient SyBlob allocation. */` |
|     548 |  9319 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     364 |  9320 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     366 |  9321 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     366 |  9322 | `		SyBlobRelease(&sResolved);` |
|     366 |  9323 | `		if( zDup == 0 ){` |
|     ! 0 |  9324 | `			goto Mem;` |
|       - |  9325 | `		}` |
|     366 |  9326 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     366 |  9327 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9328 | `			goto Mem;` |
|       - |  9329 | `		}` |
|       - |  9330 | `		/* Check for '\|' (multi-catch separator) */` |
|     377 |  9331 | `		if( pGen->pIn < pGen->pEnd &&` |
|     364 |  9332 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      28 |  9333 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9334 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9335 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9336 | `			continue;` |
|       - |  9337 | `		}` |
|     340 |  9338 | `		break;` |
|     ! 0 |  9339 | `	}` |
|     507 |  9340 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     340 |  9341 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9342 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9343 | `			pToken = pGen->pIn;` |
|     ! 0 |  9344 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9345 | `				pToken--;` |
|     ! 0 |  9346 | `			}` |
|     ! 0 |  9347 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9348 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9349 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9350 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9351 | `				return SXERR_ABORT;` |
|       - |  9352 | `			}` |
|     ! 0 |  9353 | `			return SXERR_INVALID;` |
|       - |  9354 | `	}` |
|     340 |  9355 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9356 | `	/* Duplicate instance name */` |
|     340 |  9357 | `	pName = &pGen->pIn->sData;` |
|     340 |  9358 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     340 |  9359 | `	if( zDup == 0 ){` |
|     ! 0 |  9360 | `		goto Mem;` |
|       - |  9361 | `	}` |
|     340 |  9362 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     340 |  9363 | `	pGen->pIn++;` |
|     340 |  9364 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9365 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9366 | `		pToken = pGen->pIn;` |
|     ! 0 |  9367 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9368 | `			pToken--;` |
|     ! 0 |  9369 | `		}` |
|     ! 0 |  9370 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9371 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9372 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9373 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9374 | `			return SXERR_ABORT;` |
|       - |  9375 | `		}` |
|     ! 0 |  9376 | `		return SXERR_INVALID;` |
|       - |  9377 | `	}` |
|       - |  9378 | `	/* Compile the block */` |
|     340 |  9379 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9380 | `	/* Create the catch block */` |
|     340 |  9381 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     340 |  9382 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9383 | `		return SXERR_ABORT;` |
|       - |  9384 | `	}` |
|       - |  9385 | `	/* Swap bytecode container */` |
|     340 |  9386 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     340 |  9387 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9388 | `	/* Compile the block */` |
|     340 |  9389 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9390 | `	/* Fix forward jumps now the destination is resolved  */` |
|     340 |  9391 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9392 | `	/* Emit the DONE instruction */` |
|     340 |  9393 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9394 | `	/* Leave the block */` |
|     340 |  9395 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9396 | `	/* Restore the default container */` |
|     340 |  9397 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9398 | `	/* Install the catch block */` |
|     340 |  9399 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     340 |  9400 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9401 | `		goto Mem;` |
|       - |  9402 | `	}` |
|     340 |  9403 | `	return SXRET_OK;` |
|     ! 0 |  9404 | `Mem:` |
|     ! 0 |  9405 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9406 | `	return SXERR_ABORT;` |
|     173 |  9407 |  |
|       - |  9408 | `/*` |
|       - |  9409 | ` * Compile a 'try' block.` |
|       - |  9410 | ` * A function using an exception should be in a "try" block.` |
|       - |  9411 | ` * If the exception does not trigger, the code will continue` |
|       - |  9412 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9413 | ` * is "thrown".` |
|       - |  9414 | ` */` |
|     346 |  9415 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 |  9416 |  |
|       - |  9417 | `	ph7_exception *pException;` |
|     348 |  9418 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9419 | `	GenBlock *pTry;` |
|       - |  9420 | `	sxu32 nJmpIdx;` |
|       - |  9421 | `	sxi32 rc;` |
|       - |  9422 | `	/* Create the exception container */` |
|     348 |  9423 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     348 |  9424 | `	if( pException == 0 ){` |
|     ! 0 |  9425 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9426 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9427 | `		return SXERR_ABORT;` |
|       - |  9428 | `	}` |
|       - |  9429 | `	/* Zero the structure */` |
|     348 |  9430 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9431 | `	/* Initialize fields */` |
|     348 |  9432 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     348 |  9433 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     348 |  9434 | `	pException->iHasFinally = 0;` |
|     348 |  9435 | `	pException->iFinallyDone = 0;` |
|     348 |  9436 | `	pException->pVm = pGen->pVm;` |
|       - |  9437 | `	/* Create the try block */` |
|     348 |  9438 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     348 |  9439 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9440 | `		return SXERR_ABORT;` |
|       - |  9441 | `	}` |
|       - |  9442 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     348 |  9443 | `	pTry->pUserData = pException;` |
|       - |  9444 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     348 |  9445 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9446 | `	/* Fix the jump later when the destination is resolved */` |
|     348 |  9447 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     348 |  9448 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9449 | `	/* Compile the block */` |
|     348 |  9450 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     348 |  9451 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9452 | `		return SXERR_ABORT;` |
|       - |  9453 | `	}` |
|       - |  9454 | `	/* Fix forward jumps now the destination is resolved */` |
|     348 |  9455 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9456 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     348 |  9457 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9458 | `	/* Leave the block */` |
|     348 |  9459 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9460 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     348 |  9461 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     344 |  9462 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9463 | `		/* Compile one or more catch blocks */` |
|     338 |  9464 | `		for(;;){` |
|     676 |  9465 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     526 |  9466 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     169 |  9467 | `					break;` |
|       - |  9468 | `			}` |
|     344 |  9469 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     344 |  9470 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9471 | `				return SXERR_ABORT;` |
|       - |  9472 | `			}` |
|       2 |  9473 | `		}` |
|     167 |  9474 | `	}` |
|       - |  9475 | `	/* Compile optional finally block */` |
|     348 |  9476 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     166 |  9477 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9478 | `		SySet *pInstrContainer;` |
|       - |  9479 | `		GenBlock *pFinBlock;` |
|      32 |  9480 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9481 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 |  9482 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 |  9483 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9484 | `			return SXERR_ABORT;` |
|       - |  9485 | `		}` |
|       - |  9486 | `		/* Swap bytecode container */` |
|      32 |  9487 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  9488 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9489 | `		/* Compile the finally body */` |
|      32 |  9490 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 |  9491 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9492 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9493 | `			return SXERR_ABORT;` |
|       - |  9494 | `		}` |
|       - |  9495 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 |  9496 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9497 | `		/* Emit DONE to terminate the finally block */` |
|      32 |  9498 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9499 | `		/* Leave the block */` |
|      32 |  9500 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9501 | `		/* Restore the default container */` |
|      32 |  9502 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  9503 | `		pException->iHasFinally = 1;` |
|      15 |  9504 | `	}` |
|       - |  9505 | `	/* Must have at least one catch or finally */` |
|     348 |  9506 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 |  9507 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9508 | `			"Cannot use try without catch or finally");` |
|       7 |  9509 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9510 | `			return SXERR_ABORT;` |
|       - |  9511 | `		}` |
|       3 |  9512 | `	}` |
|     348 |  9513 | `	return SXRET_OK;` |
|     175 |  9514 |  |
|       - |  9515 | `/*` |
|       - |  9516 | ` * Compile a switch block.` |
|       - |  9517 | ` *  (See block-comment below for more information)` |
|       - |  9518 | ` */` |
|     108 |  9519 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 |  9520 |  |
|     110 |  9521 | `	sxi32 rc = SXRET_OK;` |
|     110 |  9522 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9523 | `		/* Unexpected token */` |
|     ! 0 |  9524 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9525 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9526 | `			return SXERR_ABORT;` |
|       - |  9527 | `		}` |
|     ! 0 |  9528 | `		pGen->pIn++;` |
|     ! 0 |  9529 | `	}` |
|     110 |  9530 | `	pGen->pIn++;` |
|       - |  9531 | `	/* First instruction to execute in this block. */` |
|     110 |  9532 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9533 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9534 | `	 * or the '}' token */` |
|     188 |  9535 | `	for(;;){` |
|     378 |  9536 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9537 | `			/* No more input to process */` |
|     ! 0 |  9538 | `			break;` |
|       - |  9539 | `		}` |
|     378 |  9540 | `		rc = SXRET_OK;` |
|     378 |  9541 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 |  9542 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 |  9543 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9544 | `					/* Unexpected token */` |
|     ! 0 |  9545 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9546 | `						&pGen->pIn->sData);` |
|     ! 0 |  9547 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9548 | `						return SXERR_ABORT;` |
|       - |  9549 | `					}` |
|       - |  9550 | `					/* FALL THROUGH */` |
|     ! 0 |  9551 | `				}` |
|      28 |  9552 | `				rc = SXERR_EOF;` |
|      28 |  9553 | `				break;` |
|       - |  9554 | `			}` |
|      23 |  9555 | `		}else{` |
|       - |  9556 | `			sxi32 nKwrd;` |
|       - |  9557 | `			/* Extract the keyword */` |
|     310 |  9558 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     310 |  9559 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 |  9560 | `				break;` |
|       - |  9561 | `			}` |
|     230 |  9562 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9563 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9564 | `					/* Unexpected token */` |
|     ! 0 |  9565 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9566 | `						&pGen->pIn->sData);` |
|     ! 0 |  9567 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9568 | `						return SXERR_ABORT;` |
|       - |  9569 | `					}` |
|       - |  9570 | `					/* FALL THROUGH */` |
|     ! 0 |  9571 | `				}` |
|       - |  9572 | `				/* Block compiled */` |
|       3 |  9573 | `				break;` |
|       - |  9574 | `			}` |
|       - |  9575 | `		}` |
|       - |  9576 | `		/* Compile block */` |
|     270 |  9577 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     270 |  9578 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9579 | `			return SXERR_ABORT;` |
|       - |  9580 | `		}` |
|       2 |  9581 | `	}` |
|     110 |  9582 | `	return rc;` |
|      56 |  9583 |  |
|       - |  9584 | `/*` |
|       - |  9585 | ` * Compile a case eXpression.` |
|       - |  9586 | ` *  (See block-comment below for more information)` |
|       - |  9587 | ` */` |
|      88 |  9588 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 |  9589 |  |
|       - |  9590 | `	SySet *pInstrContainer;` |
|       - |  9591 | `	SyToken *pEnd,*pTmp;` |
|      90 |  9592 | `	sxi32 iNest = 0;` |
|       - |  9593 | `	sxi32 rc;` |
|       - |  9594 | `	/* Delimit the expression */` |
|      90 |  9595 | `	pEnd = pGen->pIn;` |
|     186 |  9596 | `	while( pEnd < pGen->pEnd ){` |
|     186 |  9597 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9598 | `			/* Increment nesting level */` |
|       3 |  9599 | `			iNest++;` |
|     185 |  9600 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9601 | `			/* Decrement nesting level */` |
|       3 |  9602 | `			iNest--;` |
|     183 |  9603 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 |  9604 | `			break;` |
|       - |  9605 | `		}` |
|      98 |  9606 | `		pEnd++;` |
|       2 |  9607 | `	}` |
|      90 |  9608 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9609 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9610 | `		if( rc == SXERR_ABORT ){` |
|       - |  9611 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9612 | `			return SXERR_ABORT;` |
|       - |  9613 | `		}` |
|     ! 0 |  9614 | `	}` |
|       - |  9615 | `	/* Swap token stream */` |
|      90 |  9616 | `	pTmp = pGen->pEnd;` |
|      90 |  9617 | `	pGen->pEnd = pEnd;` |
|      90 |  9618 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 |  9619 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 |  9620 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9621 | `	/* Emit the done instruction */` |
|      90 |  9622 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 |  9623 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9624 | `	/* Update token stream */` |
|      90 |  9625 | `	pGen->pIn  = pEnd;` |
|      90 |  9626 | `	pGen->pEnd = pTmp;` |
|      90 |  9627 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9628 | `		return SXERR_ABORT;` |
|       - |  9629 | `	}` |
|      90 |  9630 | `	return SXRET_OK;` |
|      46 |  9631 |  |
|       - |  9632 | `/*` |
|       - |  9633 | ` * Compile the smart switch statement.` |
|       - |  9634 | ` * According to the PHP language reference manual` |
|       - |  9635 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9636 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9637 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9638 | ` *  This is exactly what the switch statement is for.` |
|       - |  9639 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9640 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9641 | ` *  of the outer loop, use continue 2.` |
|       - |  9642 | ` *  Note that switch/case does loose comparision.` |
|       - |  9643 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9644 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9645 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9646 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9647 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9648 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9649 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9650 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9651 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9652 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9653 | ` *  list for the next case.` |
|       - |  9654 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9655 | ` *  or floating-point numbers and strings.` |
|       - |  9656 | ` */` |
|      28 |  9657 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 |  9658 |  |
|       - |  9659 | `	GenBlock *pSwitchBlock;` |
|       - |  9660 | `	SyToken *pTmp,*pEnd;` |
|       - |  9661 | `	ph7_switch *pSwitch;` |
|       - |  9662 | `	sxu32 nToken;` |
|       - |  9663 | `	sxu32 nLine;` |
|       - |  9664 | `	sxi32 rc;` |
|      30 |  9665 | `	nLine = pGen->pIn->nLine;` |
|       - |  9666 | `	/* Jump the 'switch' keyword */` |
|      30 |  9667 | `	pGen->pIn++;` |
|      30 |  9668 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9669 | `		/* Syntax error */` |
|     ! 0 |  9670 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9671 | `		if( rc == SXERR_ABORT ){` |
|       - |  9672 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9673 | `			return SXERR_ABORT;` |
|       - |  9674 | `		}` |
|     ! 0 |  9675 | `		goto Synchronize;` |
|       - |  9676 | `	}` |
|       - |  9677 | `	/* Jump the left parenthesis '(' */` |
|      30 |  9678 | `	pGen->pIn++;` |
|      30 |  9679 | `	pEnd = 0; /* cc warning */` |
|       - |  9680 | `	/* Create the loop block */` |
|      44 |  9681 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9682 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 |  9683 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9684 | `		return SXERR_ABORT;` |
|       - |  9685 | `	}` |
|       - |  9686 | `	/* Delimit the condition */` |
|      30 |  9687 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 |  9688 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9689 | `		/* Empty expression */` |
|     ! 0 |  9690 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9691 | `		if( rc == SXERR_ABORT ){` |
|       - |  9692 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9693 | `			return SXERR_ABORT;` |
|       - |  9694 | `		}` |
|     ! 0 |  9695 | `	}` |
|       - |  9696 | `	/* Swap token streams */` |
|      30 |  9697 | `	pTmp = pGen->pEnd;` |
|      30 |  9698 | `	pGen->pEnd = pEnd;` |
|       - |  9699 | `	/* Compile the expression */` |
|      30 |  9700 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  9701 | `	if( rc == SXERR_ABORT ){` |
|       - |  9702 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9703 | `		return SXERR_ABORT;` |
|       - |  9704 | `	}` |
|       - |  9705 | `	/* Update token stream */` |
|      30 |  9706 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9707 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9708 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9709 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9710 | `			return SXERR_ABORT;` |
|       - |  9711 | `		}` |
|     ! 0 |  9712 | `		pGen->pIn++;` |
|     ! 0 |  9713 | `	}` |
|      30 |  9714 | `	pGen->pIn  = &pEnd[1];` |
|      30 |  9715 | `	pGen->pEnd = pTmp;` |
|      30 |  9716 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9717 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9718 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9719 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9720 | `				pTmp--;` |
|     ! 0 |  9721 | `			}` |
|       - |  9722 | `			/* Unexpected token */` |
|     ! 0 |  9723 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9724 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9725 | `				return SXERR_ABORT;` |
|       - |  9726 | `			}` |
|     ! 0 |  9727 | `			goto Synchronize;` |
|       - |  9728 | `	}` |
|       - |  9729 | `	/* Set the delimiter token */` |
|      30 |  9730 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9731 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9732 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9733 | `	}else{` |
|      28 |  9734 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9735 | `	}` |
|      30 |  9736 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9737 | `	/* Create the switch blocks container */` |
|      30 |  9738 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 |  9739 | `	if( pSwitch == 0 ){` |
|       - |  9740 | `		/* Abort compilation */` |
|     ! 0 |  9741 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9742 | `		return SXERR_ABORT;` |
|       - |  9743 | `	}` |
|       - |  9744 | `	/* Zero the structure */` |
|      30 |  9745 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9746 | `	/* Initialize fields */` |
|      30 |  9747 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9748 | `	/* Emit the switch instruction */` |
|      30 |  9749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9750 | `	/* Compile case blocks */` |
|      96 |  9751 | `	for(;;){` |
|       - |  9752 | `		sxu32 nKwrd;` |
|     112 |  9753 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9754 | `			/* No more input to process */` |
|     ! 0 |  9755 | `			break;` |
|       - |  9756 | `		}` |
|     112 |  9757 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9758 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9759 | `				/* Unexpected token */` |
|     ! 0 |  9760 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9761 | `					&pGen->pIn->sData);` |
|     ! 0 |  9762 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9763 | `					return SXERR_ABORT;` |
|       - |  9764 | `				}` |
|       - |  9765 | `				/* FALL THROUGH */` |
|     ! 0 |  9766 | `			}` |
|       - |  9767 | `			/* Block compiled */` |
|     ! 0 |  9768 | `			break;` |
|       - |  9769 | `		}` |
|       - |  9770 | `		/* Extract the keyword */` |
|     112 |  9771 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 |  9772 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9773 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9774 | `				/* Unexpected token */` |
|     ! 0 |  9775 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9776 | `					&pGen->pIn->sData);` |
|     ! 0 |  9777 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9778 | `					return SXERR_ABORT;` |
|       - |  9779 | `				}` |
|       - |  9780 | `				/* FALL THROUGH */` |
|     ! 0 |  9781 | `			}` |
|       - |  9782 | `			/* Block compiled */` |
|       3 |  9783 | `			break;` |
|       - |  9784 | `		}` |
|     110 |  9785 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9786 | `			/*` |
|       - |  9787 | `			 * Accroding to the PHP language reference manual` |
|       - |  9788 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9789 | `			 *  that wasn't matched by the other cases.` |
|       - |  9790 | `			 */` |
|      22 |  9791 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9792 | `				/* Default case already compiled */` |
|     ! 0 |  9793 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9794 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9795 | `					return SXERR_ABORT;` |
|       - |  9796 | `				}` |
|     ! 0 |  9797 | `			}` |
|      22 |  9798 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9799 | `			/* Compile the default block */` |
|      22 |  9800 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 |  9801 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9802 | `				return SXERR_ABORT;` |
|      22 |  9803 | `			}else if( rc == SXERR_EOF ){` |
|      20 |  9804 | `				break;` |
|       1 |  9805 | `			}` |
|      91 |  9806 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9807 | `			ph7_case_expr sCase;` |
|       - |  9808 | `			/* Standard case block */` |
|      90 |  9809 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9810 | `			/* initialize the structure */` |
|      90 |  9811 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9812 | `			/* Compile the case expression */` |
|      90 |  9813 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 |  9814 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9815 | `				return SXERR_ABORT;` |
|       - |  9816 | `			}` |
|       - |  9817 | `			/* Compile the case block */` |
|      90 |  9818 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9819 | `			/* Insert in the switch container */` |
|      90 |  9820 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 |  9821 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9822 | `				return SXERR_ABORT;` |
|      90 |  9823 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9824 | `				break;` |
|       - |  9825 | `			}` |
|      42 |  9826 | `		}else{` |
|       - |  9827 | `			/* Unexpected token */` |
|     ! 0 |  9828 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9829 | `				&pGen->pIn->sData);` |
|     ! 0 |  9830 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9831 | `				return SXERR_ABORT;` |
|       - |  9832 | `			}` |
|     ! 0 |  9833 | `			break;` |
|       - |  9834 | `		}` |
|       2 |  9835 | `	}` |
|       - |  9836 | `	/* Fix all jumps now the destination is resolved */` |
|      30 |  9837 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 |  9838 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9839 | `	/* Release the loop block */` |
|      30 |  9840 | `	GenStateLeaveBlock(pGen,0);` |
|      30 |  9841 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9842 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 |  9843 | `		pGen->pIn++;` |
|      14 |  9844 | `	}` |
|       - |  9845 | `	/* Statement successfully compiled */` |
|      30 |  9846 | `	return SXRET_OK;` |
|     ! 0 |  9847 | `Synchronize:` |
|       - |  9848 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9849 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9850 | `		pGen->pIn++;` |
|     ! 0 |  9851 | `	}` |
|     ! 0 |  9852 | `	return SXRET_OK;` |
|      16 |  9853 |  |
|       - |  9854 | `/*` |
|       - |  9855 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9856 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9857 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9858 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9859 | ` */` |
|       - |  9860 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9861 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9862 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9863 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9864 |  |
|       - |  9865 | `/*` |
|       - |  9866 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9867 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9868 | ` * patched entries from the pending set.` |
|       - |  9869 | ` */` |
| 2301582 |  9870 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       2 |  9871 |  |
| 2301584 |  9872 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9873 | `	sxu32 nTarget;` |
|       - |  9874 | `	sxu32 *aIdx;` |
|       - |  9875 | `	sxu32 i;` |
| 2301584 |  9876 | `	if( nCur <= nBaseline ){` |
| 2301494 |  9877 | `		return;` |
|       - |  9878 | `	}` |
|      92 |  9879 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      92 |  9880 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     190 |  9881 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     100 |  9882 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     100 |  9883 | `		if( pInstr ){` |
|     100 |  9884 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 |  9885 | `		}` |
|      51 |  9886 | `	}` |
|      92 |  9887 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1150793 |  9888 |  |
|       - |  9889 |  |
|       - |  9890 | `/*` |
|       - |  9891 | ` * By-reference out-parameters of builtin functions.` |
|       - |  9892 | ` *` |
|       - |  9893 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - |  9894 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - |  9895 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - |  9896 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - |  9897 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - |  9898 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - |  9899 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - |  9900 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - |  9901 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - |  9902 | ` * creates it" behaviour).` |
|       - |  9903 | ` *` |
|       - |  9904 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - |  9905 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - |  9906 | ` */` |
|  373748 |  9907 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       2 |  9908 |  |
|       - |  9909 | `	static const struct {` |
|       - |  9910 | `		const char *zName;` |
|       - |  9911 | `		sxu32 nByte;` |
|       - |  9912 | `		sxu32 mask;` |
|       - |  9913 | `	} aByRef[] = {` |
|       - |  9914 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - |  9915 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - |  9916 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - |  9917 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - |  9918 | `	};` |
|       - |  9919 | `	sxu32 i;` |
|  373750 |  9920 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1128 |  9921 | `		return 0;` |
|       - |  9922 | `	}` |
| 1862984 |  9923 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1490402 |  9924 | `		if( pName->nByte == aByRef[i].nByte` |
|  764850 |  9925 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      44 |  9926 | `			return aByRef[i].mask;` |
|       - |  9927 | `		}` |
|  745182 |  9928 | `	}` |
|  372582 |  9929 | `	return 0;` |
|  186876 |  9930 |  |
|       - |  9931 | `/*` |
|       - |  9932 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - |  9933 | ` *` |
|       - |  9934 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - |  9935 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - |  9936 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - |  9937 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - |  9938 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - |  9939 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - |  9940 | ` */` |
|  373748 |  9941 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       2 |  9942 |  |
|       - |  9943 | `	SyToken *p, *pEnd;` |
|  373750 |  9944 | `	pOut->zString = 0;` |
|  373750 |  9945 | `	pOut->nByte = 0;` |
|  373750 |  9946 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 |  9947 | `		return;` |
|       - |  9948 | `	}` |
|  373750 |  9949 | `	p = pLeft->pStart;` |
|  373750 |  9950 | `	pEnd = pLeft->pEnd;` |
|       - |  9951 | `	/* Optional single leading namespace separator (absolute path). */` |
|  373750 |  9952 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      22 |  9953 | `		p++;` |
|      10 |  9954 | `	}` |
|  373750 |  9955 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1106 |  9956 | `		return;` |
|       - |  9957 | `	}` |
|       - |  9958 | `	/* Must be a single component: nothing follows the name token. */` |
|  372646 |  9959 | `	if( p + 1 != pEnd ){` |
|      24 |  9960 | `		return;` |
|       - |  9961 | `	}` |
|  372624 |  9962 | `	*pOut = p->sData;` |
|  186876 |  9963 |  |
|       - |  9964 | `/*` |
|       - |  9965 | ` * Generate bytecode for a given expression tree.` |
|       - |  9966 | ` * If something goes wrong while generating bytecode` |
|       - |  9967 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9968 | ` * this function takes care of generating the appropriate` |
|       - |  9969 | ` * error message.` |
|       - |  9970 | ` */` |
| 3100772 |  9971 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9972 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9973 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9974 | `	sxi32 iFlags /* Control flags */` |
|       - |  9975 | `	)` |
|       2 |  9976 |  |
|       - |  9977 | `	VmInstr *pInstr;` |
|       - |  9978 | `	sxu32 nJmpIdx;` |
| 3100774 |  9979 | `	sxi32 iP1 = 0;` |
| 3100774 |  9980 | `	sxu32 iP2 = 0;` |
| 3100774 |  9981 | `	void *p3  = 0;` |
|       - |  9982 | `	sxi32 iVmOp;` |
|       - |  9983 | `	sxi32 rc;` |
| 3100774 |  9984 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3100774 |  9985 | `	sxu32 nRhsNsBase = 0;` |
| 3100774 |  9986 | `	if( pNode->xCode ){` |
|       - |  9987 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9988 | `		/* Compile node */` |
| 1920778 |  9989 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1920778 |  9990 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1920778 |  9991 | `		RE_SWAP_DELIMITER(pGen);` |
| 1920778 |  9992 | `		return rc;` |
|       - |  9993 | `	}` |
| 1179998 |  9994 | `	if( pNode->pOp == 0 ){` |
|     ! 0 |  9995 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - |  9996 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 |  9997 | `		return SXERR_ABORT;` |
|       - |  9998 | `	}` |
| 1179998 |  9999 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1179998 | 10000 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      58 | 10001 | `		sxu32 nJmp = 0;` |
|       - | 10002 | `		sxu32 nNcNsBase;` |
|       - | 10003 | `		VmInstr *pInstrFix;` |
|       - | 10004 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10005 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10006 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10007 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10008 | `		 * stack slot carries a writable nIdx. */` |
|      58 | 10009 | `		if( pNode->pRight ){` |
|      58 | 10010 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      58 | 10011 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      58 | 10012 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10013 | `				return rc;` |
|       - | 10014 | `			}` |
|      58 | 10015 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10016 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10017 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10018 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10019 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10020 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10021 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10022 | `			 * cascade for the actual write path stays correct. */` |
|      58 | 10023 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      58 | 10024 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      28 | 10025 | `				pInstrFix->iP2 = 3;` |
|      13 | 10026 | `			}` |
|      28 | 10027 | `		}` |
|       - | 10028 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      58 | 10029 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10030 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      58 | 10031 | `		if( pNode->pLeft ){` |
|      58 | 10032 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      58 | 10033 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      58 | 10034 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10035 | `				return rc;` |
|       - | 10036 | `			}` |
|      58 | 10037 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10038 | `		}` |
|       - | 10039 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      58 | 10040 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10041 | `		/* Patch the short-circuit jump to land after the store. */` |
|      58 | 10042 | `		if( nJmp > 0 ){` |
|      58 | 10043 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      58 | 10044 | `			if( pInstrFix ){` |
|      58 | 10045 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10046 | `			}` |
|      28 | 10047 | `		}` |
|      58 | 10048 | `		return SXRET_OK;` |
|       - | 10049 | `	}` |
| 1179942 | 10050 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10051 | `		sxu32 nJz,nJmp;` |
|       - | 10052 | `		sxu32 nTernaryNsBase;` |
|       - | 10053 | `		/* Ternary operator require special handling */` |
|       - | 10054 | `		/* Phase#1: Compile the condition */` |
|    2500 | 10055 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2500 | 10056 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2500 | 10057 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10058 | `			return rc;` |
|       - | 10059 | `		}` |
|       - | 10060 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10061 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10062 | `		 * condition expression, not leak past the ternary. */` |
|    2500 | 10063 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2500 | 10064 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2500 | 10065 | `		if( pNode->pLeft ){` |
|       - | 10066 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10067 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2432 | 10068 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10069 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2432 | 10070 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2432 | 10071 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2432 | 10072 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10073 | `				return rc;` |
|       - | 10074 | `			}` |
|    2432 | 10075 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1217 | 10076 | `		}else{` |
|       - | 10077 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10078 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10079 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10080 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10081 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10082 | `		}` |
|       - | 10083 | `		/* Phase#4: Emit the unconditional jump */` |
|    2500 | 10084 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10085 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2500 | 10086 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2500 | 10087 | `		if( pInstr ){` |
|    2500 | 10088 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1249 | 10089 | `		}` |
|    2500 | 10090 | `		if( !pNode->pLeft ){` |
|       - | 10091 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10092 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10093 | `		}` |
|       - | 10094 | `		/* Phase#6: Compile the 'else' expression */` |
|    2500 | 10095 | `		if( pNode->pRight ){` |
|    2500 | 10096 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2500 | 10097 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2500 | 10098 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10099 | `				return rc;` |
|       - | 10100 | `			}` |
|    2500 | 10101 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1249 | 10102 | `		}` |
|    2500 | 10103 | `		if( nJmp > 0 ){` |
|       - | 10104 | `			/* Phase#7: Fix the unconditional jump */` |
|    2500 | 10105 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2500 | 10106 | `			if( pInstr ){` |
|    2500 | 10107 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1249 | 10108 | `			}` |
|    1249 | 10109 | `		}` |
|       - | 10110 | `		/* All done */` |
|    2500 | 10111 | `		return SXRET_OK;` |
|       - | 10112 | `	}` |
| 1177444 | 10113 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10114 | `	/* Generate code for the left tree */` |
| 1177444 | 10115 | `	if( pNode->pLeft ){` |
| 1177406 | 10116 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1177406 | 10117 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10118 | `			ph7_expr_node **apNode;` |
|  373868 | 10119 | `			int hasSpread = 0;` |
|  373868 | 10120 | `			int hasNamed = 0;` |
|  373868 | 10121 | `			int bAnySpread = 0;` |
|  373868 | 10122 | `			sxu32 byRefMask = 0;` |
|       - | 10123 | `			sxi32 nArgs;` |
|       - | 10124 | `			sxi32 n;` |
|       - | 10125 | `			/* Recurse and generate bytecodes for function arguments */` |
|  373868 | 10126 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  373868 | 10127 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10128 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10129 | `			{` |
|  373868 | 10130 | `				int seenNamed = 0;` |
|  740430 | 10131 | `				for( n = 0; n < nArgs; ++n ){` |
|  366566 | 10132 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     186 | 10133 | `						seenNamed = 1;` |
|     186 | 10134 | `						hasNamed = 1;` |
|  366474 | 10135 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      20 | 10136 | `						bAnySpread = 1;` |
|  366373 | 10137 | `					}else if( seenNamed ){` |
|       3 | 10138 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10139 | `							"Cannot use positional argument after named argument");` |
|       3 | 10140 | `						return SXERR_SYNTAX;` |
|       - | 10141 | `					}` |
|  183283 | 10142 | `				}` |
|       - | 10143 | `			}` |
|       - | 10144 | `			/* Read-only load */` |
|  373866 | 10145 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10146 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10147 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10148 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10149 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  373866 | 10150 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  373866 | 10151 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  373864 | 10152 | `				if( pCallName->nByte == 5` |
|  205193 | 10153 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   19182 | 10154 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  364276 | 10155 | `				}else if( pCallName->nByte == 5` |
|  186013 | 10156 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      80 | 10157 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10158 | `				}` |
|       - | 10159 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10160 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10161 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10162 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10163 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10164 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  373866 | 10165 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10166 | `					SyString sBuiltin;` |
|  373750 | 10167 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  373750 | 10168 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  186874 | 10169 | `				}` |
|  186932 | 10170 | `			}` |
|  740426 | 10171 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  366562 | 10172 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  366562 | 10173 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10174 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10175 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate). */` |
|  366562 | 10176 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      23 | 10177 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      11 | 10178 | `				}` |
|  366562 | 10179 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  366562 | 10180 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10181 | `					return rc;` |
|       - | 10182 | `				}` |
|       - | 10183 | `				/* Each argument is an independent nullsafe scope. */` |
|  366562 | 10184 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  366562 | 10185 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10186 | `					/* Emit spread opcode to unpack this array argument */` |
|      20 | 10187 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      20 | 10188 | `					hasSpread = 1;` |
|       9 | 10189 | `				}` |
|  183282 | 10190 | `			}` |
|       - | 10191 | `			/* Total number of given arguments */` |
|  373866 | 10192 | `			iP1 = nArgs;` |
|  373866 | 10193 | `			iP2 = hasSpread;` |
|       - | 10194 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10195 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  373866 | 10196 | `			if( hasNamed ){` |
|     100 | 10197 | `				sxu32 nStrBytes = 0;` |
|       - | 10198 | `				char *zBuf;` |
|     296 | 10199 | `				for( n = 0; n < nArgs; ++n ){` |
|     198 | 10200 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     184 | 10201 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10202 | `					}` |
|     100 | 10203 | `				}` |
|       - | 10204 | `				{` |
|     100 | 10205 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     100 | 10206 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10207 | `					&pGen->pVm->sAllocator, mapSize);` |
|     100 | 10208 | `				if( pMap ){` |
|     100 | 10209 | `					SyZero(pMap, mapSize);` |
|     100 | 10210 | `					pMap->bHasNamed = 1;` |
|     100 | 10211 | `					pMap->nTotal = (sxu32)nArgs;` |
|     100 | 10212 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     100 | 10213 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     296 | 10214 | `					for( n = 0; n < nArgs; ++n ){` |
|     198 | 10215 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     184 | 10216 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     184 | 10217 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     184 | 10218 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     184 | 10219 | `							zBuf += nb;` |
|      91 | 10220 | `						}` |
|       - | 10221 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     100 | 10222 | `					}` |
|     100 | 10223 | `					p3 = (void *)pMap;` |
|      49 | 10224 | `				}` |
|       - | 10225 | `				}` |
|      49 | 10226 | `			}` |
|       - | 10227 | `			/* Remove stale flags now */` |
|  373866 | 10228 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  186932 | 10229 | `		}` |
| 1177404 | 10230 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1177404 | 10231 | `		if( rc != SXRET_OK ){` |
|      31 | 10232 | `			return rc;` |
|       - | 10233 | `		}` |
| 1177374 | 10234 | `		if( !bIsChainOp ){` |
|       - | 10235 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10236 | `			 * target the end of that LHS chain, which is right here. */` |
|  550306 | 10237 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  275152 | 10238 | `		}` |
| 1177374 | 10239 | `		if( iVmOp == PH7_OP_CALL ){` |
|  373866 | 10240 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  373866 | 10241 | `			if( pInstr ){` |
|  373866 | 10242 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  372738 | 10243 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10244 | `					sxu32 nQual;` |
|  372738 | 10245 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10246 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10247 | `					 * so the later NEW handler (if any) can see it. */` |
|  372738 | 10248 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10249 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10250 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10251 | `					 * imports — class imports must NOT affect function` |
|       - | 10252 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10253 | `					 * before NEW; we store the original literal index in the` |
|       - | 10254 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10255 | `					 * the unqualified name and re-qualify with class imports. */` |
|  372738 | 10256 | `					if( bAbsolute ){` |
|      22 | 10257 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      12 | 10258 | `					}else{` |
|  372718 | 10259 | `						int fromImport = 0;` |
|  372718 | 10260 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  372718 | 10261 | `						pInstr->iP2 = (sxi32)nQual;` |
|  372718 | 10262 | `						if( nQual != nOrig ){` |
|       - | 10263 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10264 | `							 * NEW handler can recover the unqualified name. */` |
|      74 | 10265 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      74 | 10266 | `							if( !fromImport ){` |
|       - | 10267 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      64 | 10268 | `								if( p3 == 0 ){` |
|      64 | 10269 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10270 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      64 | 10271 | `									if( pMap ){` |
|      64 | 10272 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      64 | 10273 | `										p3 = (void *)pMap;` |
|      31 | 10274 | `									}` |
|      31 | 10275 | `								}` |
|      64 | 10276 | `								if( p3 ){` |
|      64 | 10277 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10278 | `								}` |
|      31 | 10279 | `							}` |
|      36 | 10280 | `						}` |
|       2 | 10281 | `					}` |
|  187498 | 10282 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10283 | `					/* Method call,flag that */` |
|     854 | 10284 | `					pInstr->iP2 = 1;` |
|     426 | 10285 | `				}` |
|  186934 | 10286 | `			}` |
|  990442 | 10287 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10288 | `			ph7_expr_node **apNode;` |
|       - | 10289 | `			sxi32 n;` |
|   81110 | 10290 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 10291 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 10292 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 10293 | `			/* Recurse and generate bytecodes for array index */` |
|   81110 | 10294 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  146388 | 10295 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   65280 | 10296 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   65280 | 10297 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   65280 | 10298 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10299 | `					return rc;` |
|       - | 10300 | `				}` |
|       - | 10301 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   65280 | 10302 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   32641 | 10303 | `			}` |
|   81110 | 10304 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   65280 | 10305 | `				iP1 = 1; /* Node have an index associated with it */` |
|   32639 | 10306 | `			}` |
|   81110 | 10307 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 10308 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     238 | 10309 | `				iP2 = 4;` |
|   80992 | 10310 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 10311 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 10312 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      52 | 10313 | `				iP2 = 5;` |
|   80849 | 10314 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 10315 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 10316 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 10317 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      26 | 10318 | `				iP2 = 6;` |
|   80812 | 10319 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10320 | `				/* Create an empty entry when the desired index is not found */` |
|   31936 | 10321 | `				iP2 = 1;` |
|   15969 | 10322 | `			}` |
|  762956 | 10323 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10324 | `			/* POP the left node */` |
|      32 | 10325 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10326 | `		}` |
|  588686 | 10327 | `	}` |
| 1177412 | 10328 | `	rc = SXRET_OK;` |
| 1177412 | 10329 | `	nJmpIdx = 0;` |
|       - | 10330 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10331 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10332 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1177412 | 10333 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     276 | 10334 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     276 | 10335 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     276 | 10336 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     276 | 10337 | `			int isSpecial = 0;` |
|     276 | 10338 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     188 | 10339 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     188 | 10340 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     201 | 10341 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     166 | 10342 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      86 | 10343 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      90 | 10344 | `					isSpecial = 1;` |
|      44 | 10345 | `				}` |
|     115 | 10346 | `			}` |
|     320 | 10347 | `			pInstr->iP1 = 0;` |
|     320 | 10348 | `			if( !isSpecial ){` |
|     144 | 10349 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      71 | 10350 | `			}` |
|       - | 10351 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10352 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     232 | 10353 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     144 | 10354 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     144 | 10355 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10356 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 10357 | `					return SXRET_OK;` |
|       - | 10358 | `				}` |
|      50 | 10359 | `			}` |
|      94 | 10360 | `		}` |
|     170 | 10361 | `	}` |
|       - | 10362 | `	/* Generate code for the right tree */` |
| 1177334 | 10363 | `	if( pNode->pRight ){` |
|  650186 | 10364 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10365 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9900 | 10366 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  645237 | 10367 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10368 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3316 | 10369 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  638631 | 10370 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10371 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     126 | 10372 | `			iVmOp = 0; /* No binary operator to emit */` |
|     126 | 10373 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  636961 | 10374 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10375 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10376 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10377 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10378 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10379 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10380 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     100 | 10381 | `			sxu32 nNsJmp = 0;` |
|     100 | 10382 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     100 | 10383 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  636801 | 10384 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  264032 | 10385 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  132015 | 10386 | `		}` |
|  650186 | 10387 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  650186 | 10388 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  650186 | 10389 | `		if( !bIsChainOp ){` |
|       - | 10390 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10391 | `			 * operator instruction is emitted. */` |
|  478132 | 10392 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  239065 | 10393 | `		}` |
|  650186 | 10394 | `		if( iVmOp == PH7_OP_STORE ){` |
|  260638 | 10395 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  260612 | 10396 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10397 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10398 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10399 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10400 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10401 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10402 | `				 */` |
|      54 | 10403 | `				iVmOp = 0;` |
|  260612 | 10404 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  260586 | 10405 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10406 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   72720 | 10407 | `					iP2 = 1;` |
|   36361 | 10408 | `				}else{` |
|  187868 | 10409 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10410 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   31890 | 10411 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   31890 | 10412 | `						iP1 = pInstr->iP1;` |
|   15946 | 10413 | `					}else{` |
|  155980 | 10414 | `						p3 = pInstr->p3;` |
|       - | 10415 | `					}` |
|       - | 10416 | `					/* POP the last dynamic load instruction */` |
|  187868 | 10417 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10418 | `				}` |
|  130294 | 10419 | `			}` |
|  519868 | 10420 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      52 | 10421 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      52 | 10422 | `			if( pInstr ){` |
|      52 | 10423 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10424 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10425 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10426 | `					 */` |
|      15 | 10427 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10428 | `					iP1 = pInstr->iP1;` |
|      15 | 10429 | `					iP2 = pInstr->iP2;` |
|      15 | 10430 | `					p3  = pInstr->p3;` |
|       8 | 10431 | `				}else{` |
|      38 | 10432 | `					p3 = pInstr->p3;` |
|       - | 10433 | `				}` |
|      25 | 10434 | `			}` |
|      25 | 10435 | `		}` |
|  325092 | 10436 | `	}` |
| 1177334 | 10437 | `	if( iVmOp > 0 ){` |
| 1177128 | 10438 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   12964 | 10439 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10440 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    9462 | 10441 | `				iP1 = 1;` |
|    4732 | 10442 | `			}` |
| 1170647 | 10443 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10444 | `			/* Namespace-qualify the class name for NEW */ {` |
|   16804 | 10445 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   16804 | 10446 | `				VmInstr *pCallInstr = 0;` |
|   16804 | 10447 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   16780 | 10448 | `					pCallInstr = pPeek;` |
|   16780 | 10449 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    8389 | 10450 | `				}` |
|   16804 | 10451 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   16802 | 10452 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10453 | `					sxu32 nLitForClass;` |
|       - | 10454 | `					/* If the CALL handler already qualified the name using` |
|       - | 10455 | `					 * function imports, recover the original unqualified` |
|       - | 10456 | `					 * literal so we can re-qualify with class imports. */` |
|   16802 | 10457 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 | 10458 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 | 10459 | `					}else{` |
|   16770 | 10460 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10461 | `					}` |
|   16802 | 10462 | `					pPeek->iP1 = 0;` |
|   16802 | 10463 | `					if( !bAbsolute ){` |
|   16786 | 10464 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    8394 | 10465 | `					}else{` |
|      18 | 10466 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10467 | `					}` |
|    8400 | 10468 | `				}` |
|       - | 10469 | `			}` |
|   16804 | 10470 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   16804 | 10471 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10472 | `				VmInstr *pPrev;` |
|   16780 | 10473 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   16780 | 10474 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10475 | `					/* Pop the call instruction, preserve named-arg map */` |
|   16780 | 10476 | `					iP1 = pInstr->iP1;` |
|   16780 | 10477 | `					if( pInstr->p3 ){` |
|      40 | 10478 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 10479 | `					}` |
|   16780 | 10480 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    8389 | 10481 | `				}` |
|    8391 | 10482 | `			}` |
| 1155765 | 10483 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10484 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10485 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     154 | 10486 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     154 | 10487 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     154 | 10488 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     154 | 10489 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     154 | 10490 | `				int isSpecialIs = 0;` |
|     154 | 10491 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     150 | 10492 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     150 | 10493 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     153 | 10494 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     145 | 10495 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      75 | 10496 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 10497 | `						isSpecialIs = 1;` |
|       5 | 10498 | `					}` |
|      75 | 10499 | `				}` |
|     156 | 10500 | `				pInstr->iP1 = 0;` |
|     156 | 10501 | `				if( !isSpecialIs && !bAbsolute ){` |
|     134 | 10502 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      66 | 10503 | `				}` |
|      77 | 10504 | `			}` |
| 1147291 | 10505 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10506 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10507 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10508 | `			 * should not trigger constant lookup. */` |
|  172056 | 10509 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  172056 | 10510 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  172014 | 10511 | `				pInstr->iP1 = 0;` |
|   86006 | 10512 | `			}` |
|  172056 | 10513 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10514 | `				/* Static member access,remember that */` |
|     198 | 10515 | `				iP1 = 1;` |
|     198 | 10516 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     198 | 10517 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      36 | 10518 | `					p3 = pInstr->p3;` |
|      36 | 10519 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      17 | 10520 | `				}` |
|      98 | 10521 | `			}` |
|   86027 | 10522 | `		}` |
|       - | 10523 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 10524 | `		 * This is the primary emit path for user-visible calls. */` |
| 1177126 | 10525 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  390668 | 10526 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  195333 | 10527 | `		}` |
|       - | 10528 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1177126 | 10529 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  588562 | 10530 | `	}` |
| 1177332 | 10531 | `	if( nJmpIdx > 0 ){` |
|       - | 10532 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   13338 | 10533 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   13338 | 10534 | `		if( pInstr ){` |
|   13338 | 10535 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6668 | 10536 | `		}` |
|    6668 | 10537 | `	}` |
| 1177332 | 10538 | `	return rc;` |
| 1550369 | 10539 |  |
|       - | 10540 | `/*` |
|       - | 10541 | ` * Compile a PHP expression.` |
|       - | 10542 | ` * According to the PHP language reference manual:` |
|       - | 10543 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10544 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10545 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10546 | ` *  is "anything that has a value".` |
|       - | 10547 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10548 | ` * function takes care of generating the appropriate error` |
|       - | 10549 | ` * message.` |
|       - | 10550 | ` */` |
|  833964 | 10551 | `static sxi32 PH7_CompileExpr(` |
|       - | 10552 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10553 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10554 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10555 | `	)` |
|       2 | 10556 |  |
|       - | 10557 | `	ph7_expr_node *pRoot;` |
|       - | 10558 | `	SySet sExprNode;` |
|       - | 10559 | `	SyToken *pEnd;` |
|       - | 10560 | `	sxi32 nExpr;` |
|       - | 10561 | `	sxi32 iNest;` |
|       - | 10562 | `	sxi32 rc;` |
|       - | 10563 | `	sxu32 nNullsafeBase;` |
|       - | 10564 | `	/* Initialize worker variables */` |
|  833966 | 10565 | `	nExpr = 0;` |
|  833966 | 10566 | `	pRoot = 0;` |
|       - | 10567 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10568 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  833966 | 10569 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  833966 | 10570 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  833966 | 10571 | `	SySetAlloc(&sExprNode,0x10);` |
|  833966 | 10572 | `	rc = SXRET_OK;` |
|       - | 10573 | `	/* Delimit the expression */` |
|  833966 | 10574 | `	pEnd = pGen->pIn;` |
|  833966 | 10575 | `	iNest = 0;` |
| 5580584 | 10576 | `	while( pEnd < pGen->pEnd ){` |
| 5296140 | 10577 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10578 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     392 | 10579 | `			iNest++;` |
| 5295945 | 10580 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     400 | 10581 | `			iNest--;` |
| 5295551 | 10582 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  549800 | 10583 | `			if( iNest <= 0 ){` |
|  549522 | 10584 | `				break;` |
|       - | 10585 | `			}` |
|     139 | 10586 | `		}` |
| 4746620 | 10587 | `		pEnd++;` |
|       2 | 10588 | `	}` |
|  833966 | 10589 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   19306 | 10590 | `		SyToken *pEnd2 = pGen->pIn;` |
|   19306 | 10591 | `		iNest = 0;` |
|       - | 10592 | `		/* Stop at the first comma */` |
|   38872 | 10593 | `		while( pEnd2 < pEnd ){` |
|   19572 | 10594 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      54 | 10595 | `				iNest++;` |
|   19546 | 10596 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      54 | 10597 | `				iNest--;` |
|   19494 | 10598 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      46 | 10599 | `				if( iNest <= 0 ){` |
|       5 | 10600 | `					break;` |
|       - | 10601 | `				}` |
|      20 | 10602 | `			}` |
|   19568 | 10603 | `			pEnd2++;` |
|       2 | 10604 | `		}` |
|   19306 | 10605 | `		if( pEnd2 <pEnd ){` |
|       5 | 10606 | `			pEnd = pEnd2;` |
|       2 | 10607 | `		}` |
|    9652 | 10608 | `	}` |
|  833966 | 10609 | `	if( pEnd > pGen->pIn ){` |
|  833956 | 10610 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10611 | `		/* Swap delimiter */` |
|  833956 | 10612 | `		pGen->pEnd = pEnd;` |
|       - | 10613 | `		/* Try to get an expression tree */` |
|  833956 | 10614 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  833956 | 10615 | `		if( rc == SXRET_OK && pRoot ){` |
|  833774 | 10616 | `			rc = SXRET_OK;` |
|  833774 | 10617 | `			if( xTreeValidator ){` |
|       - | 10618 | `				/* Call the upper layer validator callback */` |
|   23302 | 10619 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   11650 | 10620 | `			}` |
|  833774 | 10621 | `			if( rc != SXERR_ABORT ){` |
|       - | 10622 | `				/* Generate code for the given tree */` |
|  833774 | 10623 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10624 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10625 | `				 * expression so they short-circuit to its end. */` |
|  833774 | 10626 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  416886 | 10627 | `			}` |
|  833774 | 10628 | `			nExpr = 1;` |
|  416886 | 10629 | `		}` |
|       - | 10630 | `		/* Release the whole tree */` |
|  833956 | 10631 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10632 | `		/* Synchronize token stream */` |
|  833956 | 10633 | `		pGen->pEnd = pTmp;` |
|  833956 | 10634 | `		pGen->pIn  = pEnd;` |
|  833956 | 10635 | `		if( rc == SXERR_ABORT ){` |
|      11 | 10636 | `			SySetRelease(&sExprNode);` |
|      11 | 10637 | `			return SXERR_ABORT;` |
|       - | 10638 | `		}` |
|  416972 | 10639 | `	}` |
|  833956 | 10640 | `	SySetRelease(&sExprNode);` |
|  833956 | 10641 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  416984 | 10642 |  |
|       - | 10643 | `/*` |
|       - | 10644 | ` * Return a pointer to the node construct handler associated` |
|       - | 10645 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10646 | ` */` |
|  212056 | 10647 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 10648 |  |
|  212058 | 10649 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10650 | `		/* Numeric literal: Either real or integer */` |
|  111404 | 10651 | `		return PH7_CompileNumLiteral;` |
|  100656 | 10652 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10653 | `		/* Double quoted string */` |
|   20368 | 10654 | `		return PH7_CompileString;` |
|   80290 | 10655 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10656 | `		/* Single quoted string */` |
|   80176 | 10657 | `		return PH7_CompileSimpleString;` |
|     116 | 10658 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10659 | `		/* Heredoc */` |
|      66 | 10660 | `		return PH7_CompileHereDoc;` |
|      52 | 10661 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10662 | `		/* Nowdoc */` |
|      46 | 10663 | `		return PH7_CompileNowDoc;` |
|       7 | 10664 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10665 | `		/* Backtick quoted string */` |
|       5 | 10666 | `		return PH7_CompileBacktic;` |
|       - | 10667 | `	}` |
|       3 | 10668 | `	return 0;` |
|  106030 | 10669 |  |
|       - | 10670 | `/*` |
|       - | 10671 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10672 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10673 | ` * in write context" parse error.` |
|       - | 10674 | ` */` |
|    6744 | 10675 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       2 | 10676 |  |
|       - | 10677 | `	sxi32 rc;` |
|    6746 | 10678 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6744 | 10679 | `		return SXRET_OK;` |
|       - | 10680 | `	}` |
|       5 | 10681 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10682 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10683 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10684 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3374 | 10685 |  |
|       - | 10686 | `/*` |
|       - | 10687 | ` * Compile an unset() statement.` |
|       - | 10688 | ` * unset($var, $arr[$key], ...);` |
|       - | 10689 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10690 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10691 | ` * parent array before extracting the element to unset.` |
|       - | 10692 | ` */` |
|    2898 | 10693 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 10694 |  |
|    2900 | 10695 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2900 | 10696 | `	sxu32 nIdx = 0;` |
|       - | 10697 | `	SyString sName;` |
|       - | 10698 | `	sxi32 rc;` |
|       - | 10699 | `	/* Jump the 'unset' keyword */` |
|    2900 | 10700 | `	pGen->pIn++;` |
|       - | 10701 | `	/* Save delimiter */` |
|    2900 | 10702 | `	pTmp = pGen->pEnd;` |
|       - | 10703 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2900 | 10704 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2900 | 10705 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10706 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10707 | `		SyToken *pClose;` |
|    2900 | 10708 | `		pGen->pIn++;   /* Skip '(' */` |
|    2900 | 10709 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2900 | 10710 | `		pEnd = pClose; /* Stop at ')' */` |
|    1449 | 10711 | `	}` |
|    2900 | 10712 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10713 | `	/* Resolve the 'unset' builtin name once */` |
|    2900 | 10714 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     356 | 10715 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     356 | 10716 | `		if( pObj == 0 ){` |
|     ! 0 | 10717 | `			return SXERR_ABORT;` |
|       - | 10718 | `		}` |
|     356 | 10719 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     356 | 10720 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     177 | 10721 | `	}` |
|       - | 10722 | `	/* Compile each comma-separated argument */` |
|    9646 | 10723 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6748 | 10724 | `		if( pGen->pIn < pNext ){` |
|    6748 | 10725 | `			pGen->pEnd = pNext;` |
|    6748 | 10726 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10727 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 10728 | `				GenStateUnsetValidator);` |
|    6748 | 10729 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10730 | `				return SXERR_ABORT;` |
|       - | 10731 | `			}` |
|    6748 | 10732 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10733 | `				/* Emit call for this single argument */` |
|    6746 | 10734 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6746 | 10735 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6746 | 10736 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3372 | 10737 | `			}` |
|    3373 | 10738 | `		}` |
|       - | 10739 | `		/* Jump trailing commas */` |
|   10598 | 10740 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3852 | 10741 | `			pNext++;` |
|       2 | 10742 | `		}` |
|    6748 | 10743 | `		pGen->pIn = pNext;` |
|       2 | 10744 | `	}` |
|       - | 10745 | `	/* Skip past the closing ')' if present */` |
|    2900 | 10746 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2900 | 10747 | `		pGen->pIn++;` |
|    1449 | 10748 | `	}` |
|       - | 10749 | `	/* Restore token stream */` |
|    2900 | 10750 | `	pGen->pEnd = pTmp;` |
|    2900 | 10751 | `	return SXRET_OK;` |
|    1451 | 10752 |  |
|       - | 10753 | `/*` |
|       - | 10754 | ` * PHP Language construct table.` |
|       - | 10755 | ` */` |
|       - | 10756 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10757 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10758 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10759 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10760 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10761 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10762 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10763 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10764 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10765 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10766 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10767 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10768 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10769 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10770 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10771 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10772 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10773 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10774 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10775 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10776 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10777 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10778 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10779 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10780 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10781 | `};` |
|       - | 10782 | `/*` |
|       - | 10783 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10784 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10785 | ` */` |
|  562280 | 10786 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10787 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10788 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10789 | `	)` |
|       2 | 10790 |  |
|  562282 | 10791 | `	sxu32 n = 0;` |
| 2902328 | 10792 | `	for(;;){` |
| 5804658 | 10793 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  120810 | 10794 | `			break;` |
|       - | 10795 | `		}` |
| 5683850 | 10796 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  441474 | 10797 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10798 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10799 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10800 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10801 | `					return 0;` |
|       - | 10802 | `				}` |
|     ! 0 | 10803 | `			}` |
|  441472 | 10804 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       4 | 10805 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       4 | 10806 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10807 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10808 | `				return 0;` |
|       - | 10809 | `			}` |
|       - | 10810 | `			/* Return a pointer to the handler.` |
|       - | 10811 | `			*/` |
|  441474 | 10812 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10813 | `		}` |
| 5242378 | 10814 | `		n++;` |
|       2 | 10815 | `	}` |
|  120810 | 10816 | `	if( pLookahed ){` |
|  120810 | 10817 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   34674 | 10818 | `			return PH7_CompileClassInterface;` |
|   86138 | 10819 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   85910 | 10820 | `			return PH7_CompileClass;` |
|     230 | 10821 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      58 | 10822 | `			return PH7_CompileTrait;` |
|     172 | 10823 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      23 | 10824 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      22 | 10825 | `				return PH7_CompileAbstractClass;` |
|     152 | 10826 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 10827 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10828 | `				return PH7_CompileFinalClass;` |
|       - | 10829 | `		}` |
|      75 | 10830 | `	}` |
|       - | 10831 | `	/* Not a language construct */` |
|     152 | 10832 | `	return 0;` |
|  281142 | 10833 |  |
|       - | 10834 | `/*` |
|       - | 10835 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10836 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10837 | ` */` |
|     150 | 10838 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 10839 |  |
|       - | 10840 | `	int rc;` |
|     152 | 10841 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     152 | 10842 | `	if( rc == FALSE ){` |
|      44 | 10843 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      40 | 10844 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10845 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10846 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10847 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10848 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10849 | `			*/` |
|       - | 10850 | `			){` |
|      38 | 10851 | `				rc = TRUE;` |
|      18 | 10852 | `		}` |
|      22 | 10853 | `	}` |
|     152 | 10854 | `	return rc;` |
|       2 | 10855 |  |
|       - | 10856 | `/*` |
|       - | 10857 | ` * Compile a PHP chunk.` |
|       - | 10858 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10859 | ` * takes care of generating the appropriate error message.` |
|       - | 10860 | ` */` |
|  673520 | 10861 | `static sxi32 GenStateCompileChunk(` |
|       - | 10862 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10863 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10864 | `	)` |
|       2 | 10865 |  |
|       - | 10866 | `	ProcLangConstruct xCons;` |
|       - | 10867 | `	sxi32 rc;` |
|  673522 | 10868 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  525148 | 10869 | `	for(;;){` |
|  861910 | 10870 | `		int bStmtIsDeclare = 0;` |
|  861910 | 10871 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10872 | `			/* No more input to process */` |
|   13284 | 10873 | `			break;` |
|       - | 10874 | `		}` |
|       - | 10875 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 10876 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  848628 | 10877 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  562282 | 10878 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  562282 | 10879 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      42 | 10880 | `				bStmtIsDeclare = 1;` |
|      20 | 10881 | `			}` |
|  281140 | 10882 | `		}` |
|  848628 | 10883 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 10884 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 10885 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  188360 | 10886 | `			pGen->bStrictTypesLocked = 1;` |
|   94179 | 10887 | `		}` |
|  848628 | 10888 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10889 | `			/* Compile block */` |
|      18 | 10890 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      18 | 10891 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10892 | `				break;` |
|       - | 10893 | `			}` |
|      10 | 10894 | `		}else{` |
|  848612 | 10895 | `			xCons = 0;` |
|  848612 | 10896 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  562282 | 10897 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10898 | `				/* Try to extract a language construct handler */` |
|  562282 | 10899 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  562282 | 10900 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10901 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10902 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10903 | `						&pGen->pIn->sData);` |
|       9 | 10904 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10905 | `						break;` |
|       - | 10906 | `					}` |
|       - | 10907 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10908 | `					 * this erroneous statement.` |
|       - | 10909 | `					 */` |
|       9 | 10910 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10911 | `				}` |
|  567472 | 10912 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   46892 | 10913 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10914 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 10915 | `				xCons = PH7_CompileLabel;` |
|      56 | 10916 | `			}` |
|  848612 | 10917 | `			if( xCons == 0 ){` |
|       - | 10918 | `				/* Assume an expression an try to compile it */` |
|  286362 | 10919 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  286362 | 10920 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10921 | `					/* Pop l-value */` |
|  286212 | 10922 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  143105 | 10923 | `				}` |
|  143182 | 10924 | `			}else{` |
|       - | 10925 | `				/* Go compile the sucker */` |
|  562252 | 10926 | `				rc = xCons(&(*pGen));` |
|       - | 10927 | `			}` |
|  848612 | 10928 | `			if( rc == SXERR_ABORT ){` |
|       - | 10929 | `				/* Request to abort compilation */` |
|      11 | 10930 | `				break;` |
|       - | 10931 | `			}` |
|       - | 10932 | `		}` |
|       - | 10933 | `		/* Ignore trailing semi-colons ';' */` |
| 1373244 | 10934 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  524628 | 10935 | `			pGen->pIn++;` |
|       2 | 10936 | `		}` |
|  848618 | 10937 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10938 | `			/* Compile a single statement and return */` |
|  660230 | 10939 | `			break;` |
|       - | 10940 | `		}` |
|       - | 10941 | `		/* LOOP ONE */` |
|       - | 10942 | `		/* LOOP TWO */` |
|       - | 10943 | `		/* LOOP THREE */` |
|       - | 10944 | `		/* LOOP FOUR */` |
|       2 | 10945 | `	}` |
|       - | 10946 | `	/* Return compilation status */` |
|  673522 | 10947 | `	return rc;` |
|       2 | 10948 |  |
|       - | 10949 | `/*` |
|       - | 10950 | ` * Compile a Raw PHP chunk.` |
|       - | 10951 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10952 | ` * takes care of generating the appropriate error message.` |
|       - | 10953 | ` */` |
|   13294 | 10954 | `static sxi32 PH7_CompilePHP(` |
|       - | 10955 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10956 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10957 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10958 | `	)` |
|       2 | 10959 |  |
|   13296 | 10960 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10961 | `	sxi32 rc;` |
|       - | 10962 | `	/* Reset the token set */` |
|   13296 | 10963 | `	SySetReset(&(*pTokenSet));` |
|       - | 10964 | `	/* Mark as the default token set */` |
|   13296 | 10965 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10966 | `	/* Advance the stream cursor */` |
|   13296 | 10967 | `	pGen->pRawIn++;` |
|       - | 10968 | `	/* Tokenize the PHP chunk first */` |
|   13296 | 10969 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10970 | `	/* Point to the head and tail of the token stream. */` |
|   13296 | 10971 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   13296 | 10972 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   13296 | 10973 | `	if( is_expr ){` |
|     ! 0 | 10974 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10975 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10976 | `			/* A simple expression,compile it */` |
|     ! 0 | 10977 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10978 | `		}` |
|       - | 10979 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10980 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10981 | `		return SXRET_OK;` |
|       - | 10982 | `	}` |
|   13296 | 10983 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10984 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10985 | `		/*` |
|       - | 10986 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10987 | `		 * According to the PHP reference manual:` |
|       - | 10988 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10989 | `		 *  immediately follow` |
|       - | 10990 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 10991 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 10992 | `		 * Symisc extension:` |
|       - | 10993 | `		 *   This short syntax works with all PHP opening` |
|       - | 10994 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 10995 | `		 *   only short tag.` |
|       - | 10996 | `		 */` |
|       - | 10997 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 10998 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 10999 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11000 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11001 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11002 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11003 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11004 | `		}` |
|       3 | 11005 | `		return SXRET_OK;` |
|       - | 11006 | `	}` |
|       - | 11007 | `	/* Compile the PHP chunk */` |
|   13294 | 11008 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11009 | `	/* Fix exceptions jumps */` |
|   13294 | 11010 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11011 | `	/* Fix gotos now, the jump destination is resolved */` |
|   13294 | 11012 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11013 | `		rc = SXERR_ABORT;` |
|       1 | 11014 | `	}` |
|       - | 11015 | `	/* Reset container */` |
|   13294 | 11016 | `	SySetReset(&pGen->aGoto);` |
|   13294 | 11017 | `	SySetReset(&pGen->aLabel);` |
|   13294 | 11018 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11019 | `	/* Compilation result */` |
|   13294 | 11020 | `	return rc;` |
|    6649 | 11021 |  |
|       - | 11022 | `/*` |
|       - | 11023 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11024 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11025 | ` * This is the only compile interface exported from this file.` |
|       - | 11026 | ` */` |
|   15930 | 11027 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11028 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11029 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11030 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11031 | `	)` |
|       2 | 11032 |  |
|       - | 11033 | `	SySet aPhpToken,aRawToken;` |
|       - | 11034 | `	ph7_gen_state *pCodeGen;` |
|       - | 11035 | `	ph7_value *pRawObj;` |
|       - | 11036 | `	sxu32 nObjIdx;` |
|       - | 11037 | `	sxi32 nRawObj;` |
|       - | 11038 | `	int is_expr;` |
|       - | 11039 | `	sxi8 bSavedStrict;` |
|       - | 11040 | `	sxi8 bSavedStrictLocked;` |
|       - | 11041 | `	sxi32 rc;` |
|   15932 | 11042 | `	if( pScript->nByte < 1 ){` |
|       - | 11043 | `		/* Nothing to compile */` |
|     ! 0 | 11044 | `		return PH7_OK;` |
|       - | 11045 | `	}` |
|       - | 11046 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11047 | `	 * file's flags so include/require restore them on return. */` |
|   15932 | 11048 | `	pCodeGen = &pVm->sCodeGen;` |
|   15932 | 11049 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   15932 | 11050 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   15932 | 11051 | `	pCodeGen->bStrictTypes = 0;` |
|   15932 | 11052 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11053 | `	/* Initialize the tokens containers */` |
|   15932 | 11054 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   15932 | 11055 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   15932 | 11056 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   15932 | 11057 | `	is_expr = 0;` |
|   15932 | 11058 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11059 | `		SyToken sTmp;` |
|       - | 11060 | `		/* PHP only: -*/` |
|    3214 | 11061 | `		sTmp.nLine = 1;` |
|    3214 | 11062 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3214 | 11063 | `		sTmp.pUserData = 0;` |
|    3214 | 11064 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3214 | 11065 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3214 | 11066 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11067 | `			/* A simple PHP expression */` |
|     ! 0 | 11068 | `			is_expr = 1;` |
|     ! 0 | 11069 | `		}` |
|    1608 | 11070 | `	}else{` |
|       - | 11071 | `		/* Tokenize raw text */` |
|   12720 | 11072 | `		SySetAlloc(&aRawToken,32);` |
|   12720 | 11073 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11074 | `	}` |
|       - | 11075 | `	/* Process high-level tokens */` |
|   15932 | 11076 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   15932 | 11077 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   15932 | 11078 | `	rc = PH7_OK;` |
|   15932 | 11079 | `	if( is_expr ){` |
|       - | 11080 | `		/* Compile the expression */` |
|     ! 0 | 11081 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11082 | `		goto cleanup;` |
|       - | 11083 | `	}` |
|   15932 | 11084 | `	nObjIdx = 0;` |
|       - | 11085 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11086 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11087 | `	 * preventing namespace bleeding across include()d files. */` |
|   15932 | 11088 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11089 | `	/* Start the compilation process */` |
|   14330 | 11090 | `	for(;;){` |
|   41944 | 11091 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   15920 | 11092 | `			break; /* No more tokens to process */` |
|       - | 11093 | `		}` |
|   26026 | 11094 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11095 | `			/* Compile the PHP chunk */` |
|   13296 | 11096 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   13296 | 11097 | `			if( rc == SXERR_ABORT ){` |
|      13 | 11098 | `				break;` |
|       - | 11099 | `			}` |
|   13284 | 11100 | `			continue;` |
|       - | 11101 | `		}` |
|       - | 11102 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   12732 | 11103 | `		nRawObj = 0;` |
|   25504 | 11104 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11105 | `			/* Consume the raw chunk without any processing */` |
|   12774 | 11106 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   12774 | 11107 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11108 | `				rc = SXERR_MEM;` |
|     ! 0 | 11109 | `				break;` |
|       - | 11110 | `			}` |
|       - | 11111 | `			/* Mark as constant and emit the load constant instruction */` |
|   12774 | 11112 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   12774 | 11113 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   12774 | 11114 | `			++nRawObj;` |
|   12774 | 11115 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 11116 | `		}` |
|   12732 | 11117 | `		if( nRawObj > 0 ){` |
|       - | 11118 | `			/* Emit the consume instruction */` |
|   12732 | 11119 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6365 | 11120 | `		}` |
|    7967 | 11121 | `	}` |
|    7965 | 11122 | `cleanup:` |
|   15932 | 11123 | `	SySetRelease(&aRawToken);` |
|   15932 | 11124 | `	SySetRelease(&aPhpToken);` |
|       - | 11125 | `	/* Restore outer file's strict_types scope */` |
|   15932 | 11126 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   15932 | 11127 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   15932 | 11128 | `	return rc;` |
|    7967 | 11129 |  |
|       - | 11130 | `/*` |
|       - | 11131 | ` * Utility routines.Initialize the code generator.` |
|       - | 11132 | ` */` |
|    3148 | 11133 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11134 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11135 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11136 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11137 | `	)` |
|       2 | 11138 |  |
|    3150 | 11139 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11140 | `	/* Zero the structure */` |
|    3150 | 11141 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11142 | `	/* Initial state */` |
|    3150 | 11143 | `	pGen->pVm  = &(*pVm);` |
|    3150 | 11144 | `	pGen->xErr = xErr;` |
|    3150 | 11145 | `	pGen->pErrData = pErrData;` |
|    3150 | 11146 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3150 | 11147 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3150 | 11148 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3150 | 11149 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3150 | 11150 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11151 | `	/* Error log buffer */` |
|    3150 | 11152 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11153 | `	/* General purpose working buffer */` |
|    3150 | 11154 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11155 | `	/* Namespace state */` |
|    3150 | 11156 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3150 | 11157 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3150 | 11158 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3150 | 11159 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11160 | `	/* Create the global scope */` |
|    3150 | 11161 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11162 | `	/* Point to the global scope */` |
|    3150 | 11163 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3150 | 11164 | `	return SXRET_OK;` |
|       2 | 11165 |  |
|       - | 11166 | `/*` |
|       - | 11167 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11168 | ` */` |
|   18764 | 11169 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11170 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11171 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11172 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11173 | `	)` |
|       2 | 11174 |  |
|   18766 | 11175 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11176 | `	GenBlock *pBlock,*pParent;` |
|       - | 11177 | `	/* Reset state */` |
|   18766 | 11178 | `	SySetReset(&pGen->aLabel);` |
|   18766 | 11179 | `	SySetReset(&pGen->aGoto);` |
|   18766 | 11180 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   18766 | 11181 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   18766 | 11182 | `	SyBlobRelease(&pGen->sWorker);` |
|   18766 | 11183 | `	SyBlobRelease(&pGen->sNamespace);` |
|   18766 | 11184 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   18766 | 11185 | `	SyHashRelease(&pGen->hUseImports);` |
|   18766 | 11186 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   18766 | 11187 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   18766 | 11188 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   18766 | 11189 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   18766 | 11190 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11191 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11192 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11193 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11194 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11195 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11196 | `	 * number of unique names, which is acceptable. */` |
|       - | 11197 | `	/* Point to the global scope */` |
|   18766 | 11198 | `	pBlock = pGen->pCurrent;` |
|   18766 | 11199 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11200 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11201 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11202 | `		pBlock = pParent;` |
|     ! 0 | 11203 | `	}` |
|   18766 | 11204 | `	pGen->xErr = xErr;` |
|   18766 | 11205 | `	pGen->pErrData = pErrData;` |
|   18766 | 11206 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   18766 | 11207 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   18766 | 11208 | `	pGen->pIn = pGen->pEnd = 0;` |
|   18766 | 11209 | `	pGen->nErr = 0;` |
|   18766 | 11210 | `	return SXRET_OK;` |
|       2 | 11211 |  |
|       - | 11212 | `/*` |
|       - | 11213 | ` * Generate a compile-time error message.` |
|       - | 11214 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11215 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11216 | ` * abort compilation immediately.` |
|       - | 11217 | ` */` |
|     574 | 11218 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 11219 |  |
|     576 | 11220 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     576 | 11221 | `	const char *zErr = "Error";` |
|       - | 11222 | `	SyString *pFile;` |
|       - | 11223 | `	va_list ap;` |
|       - | 11224 | `	sxi32 rc;` |
|       - | 11225 | `	/* Reset the working buffer */` |
|     576 | 11226 | `	SyBlobReset(pWorker);` |
|       - | 11227 | `	/* Peek the processed file path if available */` |
|     576 | 11228 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     576 | 11229 | `	if( nErrType == E_ERROR ){` |
|       - | 11230 | `		/* Increment the error counter */` |
|     470 | 11231 | `		pGen->nErr++;` |
|     470 | 11232 | `		if( pGen->nErr > 15 ){` |
|       - | 11233 | `			/* Error count limit reached */` |
|       5 | 11234 | `			if( pGen->xErr ){` |
|       5 | 11235 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 11236 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 11237 | `				if( pFile ){` |
|       5 | 11238 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11239 | `				}` |
|       5 | 11240 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 11241 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 11242 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11243 | `				}` |
|       2 | 11244 | `			}` |
|       - | 11245 | `			/* Abort immediately */` |
|       5 | 11246 | `			return SXERR_ABORT;` |
|       - | 11247 | `		}` |
|     232 | 11248 | `	}` |
|     572 | 11249 | `	if( pGen->xErr == 0 ){` |
|       - | 11250 | `		/* No available error consumer,return immediately */` |
|       3 | 11251 | `		return SXRET_OK;` |
|       - | 11252 | `	}` |
|     569 | 11253 | `	switch(nErrType){` |
|     463 | 11254 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      27 | 11255 | `	case E_WARNING: zErr = "Warning";     break;` |
|      73 | 11256 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 11257 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11258 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11259 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11260 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11261 | `	default:` |
|     ! 0 | 11262 | `		break;` |
|       - | 11263 | `	}` |
|     569 | 11264 | `	rc = SXRET_OK;` |
|       - | 11265 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     569 | 11266 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     569 | 11267 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     569 | 11268 | `	va_start(ap,zFormat);` |
|     569 | 11269 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     569 | 11270 | `	va_end(ap);` |
|     569 | 11271 | `	if( pFile ){` |
|     569 | 11272 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     284 | 11273 | `	}` |
|       - | 11274 | `	/* Append a new line */` |
|     569 | 11275 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     569 | 11276 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11277 | `		/* Consume the generated error message */` |
|     569 | 11278 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     284 | 11279 | `	}` |
|     569 | 11280 | `	return rc;` |
|     289 | 11281 |  |
|       - | 11282 |  |
