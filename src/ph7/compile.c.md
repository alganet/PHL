# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7058/8735 lines (80.80%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits |  Line | Source |
| --------: | ----: | :--- |
|         - |     1 | `/**` |
|         - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |     5 | ` */` |
|         - |     6 | `#include "ph7int.h"` |
|         - |     7 | `/*` |
|         - |     8 | ` * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.` |
|         - |     9 | ` * That is, routines defined in this file takes a stream of tokens and output` |
|         - |    10 | ` * PH7 bytecode instructions.` |
|         - |    11 | ` */` |
|         - |    12 | `/* Forward declaration */` |
|         - |    13 | `typedef struct LangConstruct LangConstruct;` |
|         - |    14 | `typedef struct JumpFixup     JumpFixup;` |
|         - |    15 | `typedef struct Label         Label;` |
|         - |    16 | `/* Block [i.e: set of statements] control flags */` |
|         - |    17 | `#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */` |
|         - |    18 | `#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */` |
|         - |    19 | `#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/` |
|         - |    20 | `#define GEN_BLOCK_FUNC        0x008    /* Function body */` |
|         - |    21 | `#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/` |
|         - |    22 | `#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */` |
|         - |    23 | `#define GEN_BLOCK_EXPR        0x040    /* Expression */` |
|         - |    24 | `#define GEN_BLOCK_STD         0x080    /* Standard block */` |
|         - |    25 | `#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/` |
|         - |    26 | `#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */` |
|         - |    27 | `/*` |
|         - |    28 | ` * Each label seen in the input is recorded in an instance` |
|         - |    29 | ` * of the following structure.` |
|         - |    30 | ` * A label is a target point [i.e: a jump destination] that is specified` |
|         - |    31 | ` * by an identifier followed by a colon.` |
|         - |    32 | ` * Example` |
|         - |    33 | ` *  LABEL:` |
|         - |    34 | ` *		echo "hello\n";` |
|         - |    35 | ` */` |
|         - |    36 | `struct Label` |
|         - |    37 | `{` |
|         - |    38 | `	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */` |
|         - |    39 | `	sxu32 nJumpDest;     /* Jump destination */` |
|         - |    40 | `	SyString sName;      /* Label name */` |
|         - |    41 | `	sxu32 nLine;         /* Line number this label occurs */` |
|         - |    42 | `	sxu8 bRef;           /* True if the label was referenced */` |
|         - |    43 | `};` |
|         - |    44 | `/*` |
|         - |    45 | ` * Compilation of some PHP constructs such as if, for, while, the logical or` |
|         - |    46 | ` * (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |    47 | ` * generation of forward jumps.` |
|         - |    48 | ` * Since the destination PC target of these jumps isn't known when the jumps` |
|         - |    49 | ` * are emitted, we record each forward jump in an instance of the following` |
|         - |    50 | ` * structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |    51 | ` */` |
|         - |    52 | `struct JumpFixup` |
|         - |    53 | `{` |
|         - |    54 | `	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */` |
|         - |    55 | `	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */` |
|         - |    56 | `	/* The following fields are only used by the goto statement */` |
|         - |    57 | `	SyString sLabel;    /* Label name */` |
|         - |    58 | `	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */` |
|         - |    59 | `	sxu32 nLine;        /* Track line number */` |
|         - |    60 | `};` |
|         - |    61 | `/*` |
|         - |    62 | ` * Each language construct is represented by an instance` |
|         - |    63 | ` * of the following structure.` |
|         - |    64 | ` */` |
|         - |    65 | `struct LangConstruct` |
|         - |    66 | `{` |
|         - |    67 | `	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */` |
|         - |    68 | `	ProcLangConstruct xConstruct;  /* C function implementing the language construct */` |
|         - |    69 | `};` |
|         - |    70 | `/* Compilation flags */` |
|         - |    71 | `#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */` |
|         - |    72 | `/* Token stream synchronization macros */` |
|         - |    73 | `#define SWAP_TOKEN_STREAM(GEN,START,END)\` |
|         - |    74 | `	pTmp  = GEN->pEnd;\` |
|         - |    75 | `	pGen->pIn  = START;\` |
|         - |    76 | `	pGen->pEnd = END` |
|         - |    77 | `#define UPDATE_TOKEN_STREAM(GEN)\` |
|         - |    78 | `	if( GEN->pIn < pTmp ){\` |
|         - |    79 | `	    GEN->pIn++;\` |
|         - |    80 | `	}\` |
|         - |    81 | `	GEN->pEnd = pTmp` |
|         - |    82 | `#define SWAP_DELIMITER(GEN,START,END)\` |
|         - |    83 | `	pTmpIn  = GEN->pIn;\` |
|         - |    84 | `	pTmpEnd = GEN->pEnd;\` |
|         - |    85 | `	GEN->pIn = START;\` |
|         - |    86 | `	GEN->pEnd = END` |
|         - |    87 | `#define RE_SWAP_DELIMITER(GEN)\` |
|         - |    88 | `	GEN->pIn  = pTmpIn;\` |
|         - |    89 | `	GEN->pEnd = pTmpEnd` |
|         - |    90 | `/* Flags related to expression compilation */` |
|         - |    91 | `#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */` |
|         - |    92 | `#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */` |
|         - |    93 | `#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */` |
|         - |    94 | `#define EXPR_FLAG_LOAD_IDX_ISSET    0x008 /* LOAD_IDX argument is the LHS of isset() — emit iP2=4 (offsetExists) */` |
|         - |    95 | `#define EXPR_FLAG_LOAD_IDX_UNSET    0x010 /* LOAD_IDX argument is the LHS of unset() — emit iP2=5 (offsetUnset) */` |
|         - |    96 | `#define EXPR_FLAG_LOAD_IDX_EMPTY    0x020 /* LOAD_IDX argument is the LHS of empty() — emit iP2=6 (offsetExists+offsetGet) */` |
|         - |    97 | `#define EXPR_FLAG_MEMBER_WRITE      0x040 /* Sub-tree is the write lvalue of an assignment: tag a target` |
|         - |    98 | `                                           * OP_MEMBER iP2=PH7_MEMBER_WRITE so the VM auto-creates a missing` |
|         - |    99 | ``                                           * property (e.g. `$o->arr[$k] = v`, `$o->p ??= v`). Propagated`` |
|         - |   100 | `                                           * from the precedence-18 lvalue through SUBSCRIPT to the base` |
|         - |   101 | ``                                            * member; stripped when descending into an intermediate `->` `` |
|         - |   102 | `                                           * container (the container is read, not the write target). */` |
|         - |   103 | `/* Forward declaration */` |
|         - |   104 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|         - |   105 | `/*` |
|         - |   106 | ` * Local utility routines used in the code generation phase.` |
|         - |   107 | ` */` |
|         - |   108 | `/*` |
|         - |   109 | ` * Check if the given name refer to a valid label.` |
|         - |   110 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|         - |   111 | ` * Any other return value indicates no such label.` |
|         - |   112 | ` */` |
|       148 |   113 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|         5 |   114 | `{` |
|         - |   115 | `	Label *aLabel;` |
|         - |   116 | `	sxu32 n;` |
|         - |   117 | `	/* Perform a linear scan on the label table */` |
|       153 |   118 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|       333 |   119 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|       276 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   121 | `			/* Jump destination found */` |
|        96 |   122 | `			aLabel[n].bRef = TRUE;` |
|        96 |   123 | `			if( ppOut ){` |
|        96 |   124 | `				*ppOut = &aLabel[n];` |
|        46 |   125 | `			}` |
|        96 |   126 | `			return SXRET_OK;` |
|         - |   127 | `		}` |
|        93 |   128 | `	}` |
|         - |   129 | `	/* No such destination */` |
|        60 |   130 | `	return SXERR_NOTFOUND;` |
|        79 |   131 | `}` |
|         - |   132 | `/*` |
|         - |   133 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   134 | ` * compiled blocks.` |
|         - |   135 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   136 | ` */` |
|    118492 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   138 | `{` |
|    118497 |   139 | `	GenBlock *pBlock = pCurrent;` |
|    276077 |   140 | `	for(;;){` |
|    552159 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    118389 |   142 | `			iCount--; /* Decrement nesting level */` |
|    118389 |   143 | `			if( iCount < 1 ){` |
|         - |   144 | `				/* Block meet with the desired criteria */` |
|    118363 |   145 | `				return pBlock;` |
|         - |   146 | `			}` |
|        13 |   147 | `		}` |
|         - |   148 | `		/* Point to the upper block */` |
|    433801 |   149 | `		pBlock = pBlock->pParent;` |
|    433801 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   151 | `			/* Forbidden */` |
|        72 |   152 | `			break;` |
|         - |   153 | `		}` |
|         5 |   154 | `	}` |
|         - |   155 | `	/* No such block */` |
|       139 |   156 | `	return 0;` |
|     59251 |   157 | `}` |
|         - |   158 | `/*` |
|         - |   159 | ` * Initialize a freshly allocated block instance.` |
|         - |   160 | ` */` |
|  10347434 |   161 | `static void GenStateInitBlock(` |
|         - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   166 | `	void *pUserData      /* Upper layer private data */` |
|         - |   167 | `	)` |
|         5 |   168 | `{` |
|         - |   169 | `	/* Initialize block fields */` |
|  10347439 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  10347439 |   171 | `	pBlock->pUserData   = pUserData;` |
|  10347439 |   172 | `	pBlock->pGen        = pGen;` |
|  10347439 |   173 | `	pBlock->iFlags      = iType;` |
|  10347439 |   174 | `	pBlock->pParent     = 0;` |
|  10347439 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  10347439 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  10347439 |   177 | `}` |
|         - |   178 | `/*` |
|         - |   179 | ` * Allocate a new block instance.` |
|         - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   182 | ` * processing on failure.` |
|         - |   183 | ` */` |
|  10343494 |   184 | `static sxi32 GenStateEnterBlock(` |
|         - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   190 | `	)` |
|         5 |   191 | `{` |
|         - |   192 | `	GenBlock *pBlock;` |
|         - |   193 | `	/* Allocate a new block instance */` |
|  10343499 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  10343499 |   195 | `	if( pBlock == 0 ){` |
|         - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   198 | `		 */` |
|       ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   200 | `		/* Abort processing immediately */` |
|       ! 0 |   201 | `		return SXERR_ABORT;` |
|         - |   202 | `	}` |
|         - |   203 | `	/* Zero the structure */` |
|  10343499 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  10343499 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   206 | `	/* Link to the parent block */` |
|  10343499 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   208 | `	/* Mark as the current block */` |
|  10343499 |   209 | `	pGen->pCurrent = pBlock;` |
|  10343499 |   210 | `	if( ppBlock ){` |
|         - |   211 | `		/* Write a pointer to the new instance */` |
|   4979265 |   212 | `		*ppBlock = pBlock;` |
|   2489630 |   213 | `	}` |
|  10343499 |   214 | `	return SXRET_OK;` |
|   5171752 |   215 | `}` |
|         - |   216 | `/*` |
|         - |   217 | ` * Release block fields without freeing the whole instance.` |
|         - |   218 | ` */` |
|  10343478 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   220 | `{` |
|  10343483 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  10343483 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  10343483 |   223 | `}` |
|         - |   224 | `/*` |
|         - |   225 | ` * Release a block.` |
|         - |   226 | ` */` |
|  10343478 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   228 | `{` |
|  10343483 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  10343483 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   231 | `	/* Free the instance */` |
|  10343483 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  10343483 |   233 | `}` |
|         - |   234 | `/*` |
|         - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   236 | ` */` |
|  10343478 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   238 | `{` |
|  10343483 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  10343483 |   240 | `	if( pBlock == 0 ){` |
|         - |   241 | `		/* No more block to pop */` |
|       ! 0 |   242 | `		return SXERR_EMPTY;` |
|         - |   243 | `	}` |
|         - |   244 | `	/* Point to the upper block */` |
|  10343483 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  10343483 |   246 | `	if( ppBlock ){` |
|         - |   247 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   248 | `		*ppBlock = pBlock;` |
|       ! 0 |   249 | `	}else{` |
|         - |   250 | `		/* Safely release the block */` |
|  10343483 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   252 | `	}` |
|  10343483 |   253 | `	return SXRET_OK;` |
|   5171744 |   254 | `}` |
|         - |   255 | `/*` |
|         - |   256 | ` * Emit a forward jump.` |
|         - |   257 | ` * Notes on forward jumps` |
|         - |   258 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |   259 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |   260 | ` *  generation of forward jumps.` |
|         - |   261 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |   262 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |   263 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |   264 | ` */` |
|   3698572 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   266 | `{` |
|         - |   267 | `	JumpFixup sJumpFix;` |
|         - |   268 | `	sxi32 rc;` |
|         - |   269 | `	/* Init the JumpFixup structure */` |
|   3698577 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|   3698577 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   272 | `	/* Insert in the jump fixup table */` |
|   3698577 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   3698577 |   274 | `	return rc;` |
|         5 |   275 | `}` |
|         - |   276 | `/*` |
|         - |   277 | ` * Fix a forward jump now the jump destination is resolved.` |
|         - |   278 | ` * Return the total number of fixed jumps.` |
|         - |   279 | ` * Notes on forward jumps:` |
|         - |   280 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |   281 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |   282 | ` *  generation of forward jumps.` |
|         - |   283 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |   284 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |   285 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|         - |   286 | ` */` |
|   7194614 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   288 | `{` |
|         - |   289 | `	JumpFixup *aFix;` |
|         - |   290 | `	VmInstr *pInstr;` |
|         - |   291 | `	sxu32 nFixed;` |
|         - |   292 | `	sxu32 n;` |
|         - |   293 | `	/* Point to the jump fixup table */` |
|   7194619 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   295 | `	/* Fix the desired jumps */` |
|  15192491 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   7997877 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   298 | `			/* Already fixed */` |
|   3033721 |   299 | `			continue;` |
|         - |   300 | `		}` |
|   4964161 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   302 | `			/* Not of our interest */` |
|   1265591 |   303 | `			continue;` |
|         - |   304 | `		}` |
|         - |   305 | `		/* Point to the instruction to fix */` |
|   3698575 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   3698575 |   307 | `		if( pInstr ){` |
|   3698575 |   308 | `			pInstr->iP2 = nJumpDest;` |
|   3698575 |   309 | `			nFixed++;` |
|         - |   310 | `			/* Mark as fixed */` |
|   3698575 |   311 | `			aFix[n].nJumpType = -1;` |
|   1849285 |   312 | `		}` |
|   1849290 |   313 | `	}` |
|         - |   314 | `	/* Total number of fixed jumps */` |
|   7194619 |   315 | `	return nFixed;` |
|         5 |   316 | `}` |
|         - |   317 | `/*` |
|         - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   319 | ` * The goto statement can be used to jump to another section` |
|         - |   320 | ` * in the program.` |
|         - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   322 | ` * statement for more information.` |
|         - |   323 | ` */` |
|   2697148 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   325 | `{` |
|         - |   326 | `	JumpFixup *pJump,*aJumps;` |
|         - |   327 | `	Label *pLabel,*aLabel;` |
|         - |   328 | `	VmInstr *pInstr;` |
|         - |   329 | `	sxi32 rc;` |
|         - |   330 | `	sxu32 n;` |
|         - |   331 | `	/* Point to the goto table */` |
|   2697153 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   333 | `	/* Fix */` |
|   2697299 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|       153 |   335 | `		pJump = &aJumps[n];` |
|         - |   336 | `		/* Extract the target label */` |
|       153 |   337 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|       153 |   338 | `		if( rc != SXRET_OK ){` |
|         - |   339 | `			/* No such label */` |
|        60 |   340 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|        60 |   341 | `			if( rc == SXERR_ABORT ){` |
|         3 |   342 | `				return SXERR_ABORT;` |
|         - |   343 | `			}` |
|        58 |   344 | `			continue;` |
|         - |   345 | `		}` |
|         - |   346 | `		/* Make sure the target label is reachable */` |
|        96 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|        11 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|        11 |   349 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |   350 | `				return SXERR_ABORT;` |
|         - |   351 | `			}` |
|         4 |   352 | `		}` |
|         - |   353 | `		/* Fix the jump now the destination is resolved */` |
|        96 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|        96 |   355 | `		if( pInstr ){` |
|        96 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|        46 |   357 | `		}` |
|        50 |   358 | `	}` |
|   2697151 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   2697283 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|       137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|         - |   362 | `			/* Emit a warning */` |
|        40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|        24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|        12 |   365 | `		}` |
|        71 |   366 | `	}` |
|   2697151 |   367 | `	return SXRET_OK;` |
|   1348579 |   368 | `}` |
|         - |   369 | `/*` |
|         - |   370 | ` * Check if a given token value is installed in the literal table.` |
|         - |   371 | ` */` |
|  13612784 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   373 | `{` |
|         - |   374 | `	SyHashEntry *pEntry;` |
|  13612789 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  13612789 |   376 | `	if( pEntry == 0 ){` |
|   3564731 |   377 | `		return SXERR_NOTFOUND;` |
|         - |   378 | `	}` |
|  10048063 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10048063 |   380 | `	return SXRET_OK;` |
|   6806397 |   381 | `}` |
|         - |   382 | `/*` |
|         - |   383 | ` * Install a given constant index in the literal table.` |
|         - |   384 | ` * In order to be installed, the ph7_value must be of type string.` |
|         - |   385 | ` *` |
|         - |   386 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|         - |   387 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|         - |   388 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|         - |   389 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|         - |   390 | ` * many "" literals appear in user code.` |
|         - |   391 | ` */` |
|   3564726 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   393 | `{` |
|   3564731 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3564731 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1782363 |   396 | `	}` |
|   3564731 |   397 | `	return SXRET_OK;` |
|         5 |   398 | `}` |
|         - |   399 | `/*` |
|         - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   401 | ` * in the constant table.` |
|         - |   402 | ` */` |
|   2796574 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   404 | `{` |
|         - |   405 | `	ph7_value *pObj;` |
|   2796579 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   407 | `	/* Reserve a new constant */` |
|   2796579 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   2796579 |   409 | `	if( pObj == 0 ){` |
|       ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   411 | `		return 0;` |
|         - |   412 | `	}` |
|   2796579 |   413 | `	*pIdx = nIdx;` |
|         - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   416 | `	 */` |
|   2796579 |   417 | `	return pObj;` |
|   1398292 |   418 | `}` |
|         - |   419 | `/*` |
|         - |   420 | ` * Implementation of the PHP language constructs.` |
|         - |   421 | ` */` |
|         - |   422 | `/*` |
|         - |   423 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|         - |   424 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|         - |   425 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|         - |   426 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|         - |   427 | ` *` |
|         - |   428 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|         - |   429 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|         - |   430 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|         - |   431 | ` * surrounding callsites' zero-check fallback pattern.` |
|         - |   432 | ` */` |
|   6359218 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   434 | `{` |
|         - |   435 | `	VmCallArgMap *pMap;` |
|   6359223 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   437 | `	if( p3 == 0 ){` |
|        35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   439 | `		if( pMap == 0 ) return 0;` |
|        35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   441 | `		p3 = (void *)pMap;` |
|        16 |   442 | `	}` |
|        39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   444 | `	return p3;` |
|   3179614 |   445 | `}` |
|         - |   446 | `/* Forward declaration */` |
|         - |   447 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|         - |   448 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen);` |
|         - |   449 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut);` |
|         - |   450 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|         - |   451 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut);` |
|         - |   452 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut);` |
|         - |   453 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|         - |   454 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|         - |   455 | `/* Forward decl: union type parser is defined later in this file. */` |
|         - |   456 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |   457 | `	ph7_gen_state *pGen,` |
|         - |   458 | `	sxu32 *pnType,` |
|         - |   459 | `	SyString *pClass,` |
|         - |   460 | `	SySet *pAlts,` |
|         - |   461 | `	sxi32 *piTypeFlags,` |
|         - |   462 | `	SyString *pTypeText,` |
|         - |   463 | `	int iNullableFlag,` |
|         - |   464 | `	int iUnionFlag,` |
|         - |   465 | `	int bAllowVoid,` |
|         - |   466 | `	sxu32 nLine` |
|         - |   467 | `);` |
|         - |   468 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|         - |   469 | `static const char * TokenTypeName(sxu32 nType);` |
|         - |   470 | `/*` |
|         - |   471 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|         - |   472 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|         - |   473 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|         - |   474 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|         - |   475 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|         - |   476 | ` * for anything larger, so correctness is preserved even for pathological` |
|         - |   477 | ` * inputs like a thousand-digit number.` |
|         - |   478 | ` */` |
|         - |   479 | `#define GEN_NUM_SCRATCH 128` |
|         - |   480 | `/*` |
|         - |   481 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|         - |   482 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|         - |   483 | ` *   base  2 => 0 or 1` |
|         - |   484 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|         - |   485 | ` *              decimal scan in the lexer)` |
|         - |   486 | ` */` |
|      1076 |   487 | `static int GenStateIsBaseDigit(int c, int base)` |
|         5 |   488 | `{` |
|      1081 |   489 | `	if( base == 16 ){ return SyisHex(c); }` |
|       982 |   490 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|       703 |   491 | `	return SyisDigit(c);` |
|       543 |   492 | `}` |
|         - |   493 | `/*` |
|         - |   494 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|         - |   495 | ` * underscore separator so the caller can report the malformed portion with` |
|         - |   496 | ` * the exact wording PHP uses:` |
|         - |   497 | ` *` |
|         - |   498 | ` *   syntax error, unexpected identifier "X"` |
|         - |   499 | ` *` |
|         - |   500 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|         - |   501 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|         - |   502 | ` * absorbed by the lexer specifically to let this validator see and report` |
|         - |   503 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|         - |   504 | ` * no forward rescan needed.` |
|         - |   505 | ` *` |
|         - |   506 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|         - |   507 | ` * returns 0 when it is well-formed.` |
|         - |   508 | ` */` |
|   2797566 |   509 | `static int GenStateFindBadNumericSeparator(` |
|         - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   511 | `{` |
|   2797571 |   512 | `	const char *z = pRaw->zString;` |
|   2797571 |   513 | `	sxu32 n = pRaw->nByte;` |
|   2797571 |   514 | `	int base = 10;` |
|         - |   515 | `	sxu32 i, start;` |
|   2797571 |   516 | `	if( n < 2 ) return 0;` |
|    457377 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        80 |   518 | `		base = 16;` |
|    457338 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       284 |   520 | `		base = 2;` |
|       141 |   521 | `	}` |
|   1530755 |   522 | `	for( i = 0; i < n; ++i ){` |
|   1073397 |   523 | `		if( z[i] != '_' ) continue;` |
|       546 |   524 | `		if( i > 0 && i + 1 < n` |
|       543 |   525 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|       543 |   526 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|       533 |   527 | `			continue; /* well-placed separator */` |
|         - |   528 | `		}` |
|         - |   529 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|         - |   530 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|        18 |   531 | `		start = i;` |
|        23 |   532 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|        12 |   533 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|         6 |   534 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|         2 |   535 | `		}` |
|        18 |   536 | `		*pBadStart = &z[start];` |
|        18 |   537 | `		*pBadLen = n - start;` |
|        18 |   538 | `		return 1;` |
|       ! 0 |   539 | `	}` |
|    457363 |   540 | `	return 0;` |
|   1398788 |   541 | `}` |
|         - |   542 | `/*` |
|         - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   548 | ` * so callers can bail from the current construct).` |
|         - |   549 | ` */` |
|   2797566 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   551 | `{` |
|   2797571 |   552 | `	const char *zBad = 0;` |
|   2797571 |   553 | `	sxu32 nBad = 0;` |
|         - |   554 | `	SyString sBad;` |
|         - |   555 | `	sxi32 rc;` |
|   2797571 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   2797557 |   557 | `		return SXRET_OK;` |
|         - |   558 | `	}` |
|        18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   562 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   563 | `		return SXERR_ABORT;` |
|         - |   564 | `	}` |
|        18 |   565 | `	return SXERR_SYNTAX;` |
|   1398788 |   566 | `}` |
|         - |   567 | `/*` |
|         - |   568 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|         - |   569 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|         - |   570 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|         - |   571 | ` *` |
|         - |   572 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|         - |   573 | ` * and *pzAlloc is set to NULL.` |
|         - |   574 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|         - |   575 | ` * and *pzAlloc is set to NULL.` |
|         - |   576 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|         - |   577 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|         - |   578 | ` * caller with SyMemBackendFree once the converter is done.` |
|         - |   579 | ` *` |
|         - |   580 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|         - |   581 | ` * case *pOut is left untouched and the caller must not read it).` |
|         - |   582 | ` */` |
|   2797552 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   584 | `	SyMemBackend *pAlloc,` |
|         - |   585 | `	const SyString *pToken,` |
|         - |   586 | `	char *zScratch, sxu32 nScratch,` |
|         - |   587 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   588 | `{` |
|         - |   589 | `	sxu32 i, j;` |
|   2797557 |   590 | `	int hasUnderscore = 0;` |
|         - |   591 | `	char *zBuf;` |
|   2797557 |   592 | `	*pzAlloc = 0;` |
|   6209063 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   3411763 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   1705758 |   595 | `	}` |
|   2797557 |   596 | `	if( !hasUnderscore ){` |
|   2797305 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|   2797305 |   598 | `		return SXRET_OK;` |
|         - |   599 | `	}` |
|       253 |   600 | `	if( pToken->nByte <= nScratch ){` |
|       251 |   601 | `		zBuf = zScratch;` |
|       126 |   602 | `	}else{` |
|         3 |   603 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|         3 |   604 | `		if( zBuf == 0 ){` |
|       ! 0 |   605 | `			return SXERR_ABORT;` |
|         - |   606 | `		}` |
|         3 |   607 | `		*pzAlloc = zBuf;` |
|         - |   608 | `	}` |
|       253 |   609 | `	j = 0;` |
|      2895 |   610 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|      2643 |   611 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|      1322 |   612 | `	}` |
|       253 |   613 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|       253 |   614 | `	return SXRET_OK;` |
|   1398781 |   615 | `}` |
|         - |   616 | `/*` |
|         - |   617 | ` * Compile a numeric [i.e: integer or real] literal.` |
|         - |   618 | ` * Notes on the integer type.` |
|         - |   619 | ` *  According to the PHP language reference manual` |
|         - |   620 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|         - |   621 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|         - |   622 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|         - |   623 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|         - |   624 | ` * Symisc eXtension to the integer type.` |
|         - |   625 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|         - |   626 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|         - |   627 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|         - |   628 | ` *  [i.e: either 32bit or 64bit].` |
|         - |   629 | ` *  For more information on this powerfull extension please refer to the official` |
|         - |   630 | ` *  documentation.` |
|         - |   631 | ` */` |
|         - |   632 | `/*` |
|         - |   633 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|         - |   634 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|         - |   635 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|         - |   636 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|         - |   637 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|         - |   638 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|         - |   639 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|         - |   640 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|         - |   641 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|         - |   642 | ` * confidently classify, so the int path stays in charge.` |
|         - |   643 | ` *` |
|         - |   644 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|         - |   645 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|         - |   646 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|         - |   647 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|         - |   648 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|         - |   649 | ` * residual; matching php exactly would need a port of those functions.` |
|         - |   650 | ` */` |
|   2796608 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   652 | `{` |
|   2796613 |   653 | `	const char *z = pNum->zString;` |
|   2796613 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   655 | `	const char *p, *q;` |
|         - |   656 | `	int n;` |
|   2796613 |   657 | `	*pbDecimal = FALSE;` |
|   2796613 |   658 | `	if( z >= zEnd ){` |
|       ! 0 |   659 | `		return FALSE;` |
|         - |   660 | `	}` |
|   2796613 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|         - |   662 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|        77 |   663 | `		p = z + 2;` |
|        85 |   664 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|       493 |   665 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|        77 |   666 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|        71 |   667 | `			return FALSE;` |
|         - |   668 | `		}` |
|         7 |   669 | `		{ ph7_real dv = 0;` |
|       103 |   670 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|        97 |   671 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|        49 |   672 | `		  }` |
|         7 |   673 | `		  *pReal = dv;` |
|         - |   674 | `		}` |
|         7 |   675 | `		return TRUE;` |
|   2796537 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|         - |   677 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|       281 |   678 | `		p = z + 2;` |
|       329 |   679 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|      2149 |   680 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|       281 |   681 | `		if( n <= 63 ){` |
|       279 |   682 | `			return FALSE;` |
|         - |   683 | `		}` |
|         3 |   684 | `		{ ph7_real dv = 0;` |
|       195 |   685 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|       129 |   686 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|        65 |   687 | `		  }` |
|         3 |   688 | `		  *pReal = dv;` |
|         - |   689 | `		}` |
|         3 |   690 | `		return TRUE;` |
|   2796257 |   691 | `	}else if( z[0] == '0' ){` |
|         - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1058203 |   695 | `		p = z;` |
|   2116403 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1058431 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1058203 |   698 | `		if( n <= 21 ){` |
|   1058201 |   699 | `			return FALSE;` |
|         - |   700 | `		}` |
|         3 |   701 | `		{ ph7_real dv = 0;` |
|        47 |   702 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|        45 |   703 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|        23 |   704 | `		  }` |
|         3 |   705 | `		  *pReal = dv;` |
|         - |   706 | `		}` |
|         3 |   707 | `		return TRUE;` |
|         - |   708 | `	}` |
|         - |   709 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|         - |   710 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|         - |   711 | `	 * for php-exact rounding. */` |
|   1738059 |   712 | `	p = z;` |
|   1738059 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   4084895 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   1738059 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   716 | `		*pbDecimal = TRUE;` |
|        25 |   717 | `		return TRUE;` |
|         - |   718 | `	}` |
|   1738035 |   719 | `	return FALSE;` |
|   1398309 |   720 | `}` |
|   2797538 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   722 | `{` |
|   2797543 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   2797543 |   724 | `	sxu32 nIdx = 0;` |
|         - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   2797543 |   726 | `	char *zAlloc = 0;` |
|         - |   727 | `	SyString sNum;` |
|         - |   728 | `	sxi32 rc;` |
|   1398769 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   2797543 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   2797543 |   731 | `	if( rc != SXRET_OK ){` |
|        14 |   732 | `		return rc;` |
|         - |   733 | `	}` |
|   4196297 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1398764 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   2797533 |   736 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   737 | `		return SXERR_ABORT;` |
|         - |   738 | `	}` |
|   2797533 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   740 | `		ph7_value *pObj;` |
|         - |   741 | `		sxi64 iValue;` |
|   2796613 |   742 | `		ph7_real rOverflow = 0;` |
|   2796613 |   743 | `		int bDecimalOverflow = 0;` |
|   2796613 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|         - |   745 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|         - |   746 | `			 * float instead of wrapping/dropping digits. */` |
|        35 |   747 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        35 |   748 | `			if( pObj == 0 ){` |
|       ! 0 |   749 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   750 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   751 | `				return SXERR_ABORT;` |
|         - |   752 | `			}` |
|        35 |   753 | `			if( bDecimalOverflow ){` |
|         - |   754 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|        25 |   755 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|        25 |   756 | `				PH7_MemObjToReal(pObj);` |
|        13 |   757 | `			}else{` |
|        11 |   758 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|         - |   759 | `			}` |
|        18 |   760 | `		}else{` |
|   2796579 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   2796579 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   2796579 |   763 | `			if( pObj == 0 ){` |
|       ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   765 | `				return SXERR_ABORT;` |
|         - |   766 | `			}` |
|   2796579 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   768 | `		}` |
|   1398309 |   769 | `	}else{` |
|         - |   770 | `		/* Real number */` |
|         - |   771 | `		ph7_value *pObj;` |
|         - |   772 | `		/* Reserve a new constant */` |
|       924 |   773 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       924 |   774 | `		if( pObj == 0 ){` |
|       ! 0 |   775 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   776 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   777 | `			return SXERR_ABORT;` |
|         - |   778 | `		}` |
|       924 |   779 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       924 |   780 | `		PH7_MemObjToReal(pObj);` |
|         - |   781 | `	}` |
|   2797533 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   783 | `	/* Emit the load constant instruction */` |
|   2797533 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   785 | `	/* Node successfully compiled */` |
|   2797533 |   786 | `	return SXRET_OK;` |
|   1398774 |   787 | `}` |
|         - |   788 | `/*` |
|         - |   789 | ` * Compile a single quoted string.` |
|         - |   790 | ` * According to the PHP language reference manual:` |
|         - |   791 | ` *` |
|         - |   792 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|         - |   793 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|         - |   794 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|         - |   795 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|         - |   796 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|         - |   797 | ` *` |
|         - |   798 | ` */` |
|   4445364 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   800 | `{` |
|   4445369 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   803 | `	ph7_value *pObj;` |
|         - |   804 | `	sxu32 nIdx;` |
|   4445369 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   806 | `	/* Delimit the string */` |
|   4445369 |   807 | `	zIn  = pStr->zString;` |
|   4445369 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|   4445369 |   809 | `	if( zIn >= zEnd ){` |
|         - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   811 | `		 * rather than reserving a new object each time. */` |
|    209033 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    209033 |   813 | `		return SXRET_OK;` |
|         - |   814 | `	}` |
|   4236341 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   816 | `		/* Already processed,emit the load constant instruction` |
|         - |   817 | `		 * and return.` |
|         - |   818 | `		 */` |
|   2470789 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2470789 |   820 | `		return SXRET_OK;` |
|         - |   821 | `	}` |
|         - |   822 | `	/* Reserve a new constant */` |
|   1765557 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1765557 |   824 | `	if( pObj == 0 ){` |
|       ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   827 | `		return SXERR_ABORT;` |
|         - |   828 | `	}` |
|   1765557 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   830 | `	/* Compile the node */` |
|   1808952 |   831 | `	for(;;){` |
|   3617909 |   832 | `		if( zIn >= zEnd ){` |
|         - |   833 | `			/* End of input */` |
|   1765557 |   834 | `			break;` |
|         - |   835 | `		}` |
|   1852357 |   836 | `		zCur = zIn;` |
|  37950467 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  36098115 |   838 | `			zIn++;` |
|         5 |   839 | `		}` |
|   1852357 |   840 | `		if( zIn > zCur ){` |
|         - |   841 | `			/* Append raw contents*/` |
|   1812927 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    906461 |   843 | `		}` |
|   1852357 |   844 | `		zIn++;` |
|   1852357 |   845 | `		if( zIn < zEnd ){` |
|    122291 |   846 | `			if( zIn[0] == '\\' ){` |
|         - |   847 | `				/* A literal backslash */` |
|     31559 |   848 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    106514 |   849 | `			}else if( zIn[0] == '\'' ){` |
|         - |   850 | `				/* A single quote */` |
|        11 |   851 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   852 | `			}else{` |
|         - |   853 | `				/* verbatim copy */` |
|     90727 |   854 | `				zIn--;` |
|     90727 |   855 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     90727 |   856 | `				zIn++;` |
|         - |   857 | `			}` |
|     61143 |   858 | `		}` |
|         - |   859 | `		/* Advance the stream cursor */` |
|   1852357 |   860 | `		zIn++;` |
|         5 |   861 | `	}` |
|         - |   862 | `	/* Emit the load constant instruction */` |
|   1765557 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1765557 |   864 | `	if( pStr->nByte < 1024 ){` |
|         - |   865 | `		/* Install in the literal table */` |
|   1765557 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    882776 |   867 | `	}` |
|         - |   868 | `	/* Node successfully compiled */` |
|   1765557 |   869 | `	return SXRET_OK;` |
|   2222687 |   870 | `}` |
|         - |   871 | `/*` |
|         - |   872 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|         - |   873 | ` *` |
|         - |   874 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|         - |   875 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|         - |   876 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|         - |   877 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|         - |   878 | ` * original source buffer — the buffer is stable through compilation.` |
|         - |   879 | ` *` |
|         - |   880 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|         - |   881 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|         - |   882 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|         - |   883 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|         - |   884 | ` *     at least N)" — line too short, or first differing byte is not` |
|         - |   885 | ` *     whitespace.` |
|         - |   886 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|         - |   887 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|         - |   888 | ` */` |
|       114 |   889 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|         4 |   890 | `{` |
|       118 |   891 | `	SyString *pIn = &pGen->pIn->sData;` |
|       118 |   892 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - |   893 | `	const char *zPrefix;` |
|         - |   894 | `	const char *z, *zEnd;` |
|         - |   895 | `	char *zBuf, *zDst;` |
|       118 |   896 | `	if( nIndent == 0 ){` |
|         - |   897 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|        73 |   898 | `		*pOut = *pIn;` |
|        73 |   899 | `		return SXRET_OK;` |
|         - |   900 | `	}` |
|         - |   901 | `	/* Recover the marker indent prefix from the original source buffer.` |
|         - |   902 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|         - |   903 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|         - |   904 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|         - |   905 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|         - |   906 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|        47 |   907 | `	zPrefix = pIn->zString + pIn->nByte;` |
|        47 |   908 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|       ! 0 |   909 | `		zPrefix += 2;` |
|       ! 0 |   910 | `	}else{` |
|        47 |   911 | `		zPrefix += 1;` |
|         - |   912 | `	}` |
|         - |   913 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|        47 |   914 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|        47 |   915 | `	if( zBuf == 0 ){` |
|       ! 0 |   916 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |   917 | `		return SXERR_ABORT;` |
|         - |   918 | `	}` |
|        47 |   919 | `	zDst = zBuf;` |
|        47 |   920 | `	z = pIn->zString;` |
|        47 |   921 | `	zEnd = z + pIn->nByte;` |
|       129 |   922 | `	while( z < zEnd ){` |
|        71 |   923 | `		const char *zLine = z;` |
|         - |   924 | `		sxu32 nLine;` |
|         - |   925 | `		int bEmpty;` |
|       799 |   926 | `		while( z < zEnd && z[0] != '\n' ){` |
|       731 |   927 | `			z++;` |
|         3 |   928 | `		}` |
|        71 |   929 | `		nLine = (sxu32)(z - zLine);` |
|        71 |   930 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|        71 |   931 | `		if( !bEmpty ){` |
|         - |   932 | `			sxu32 i;` |
|        67 |   933 | `			if( nLine < nIndent ){` |
|       ! 0 |   934 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   935 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       ! 0 |   936 | `					nIndent);` |
|       ! 0 |   937 | `				return SXERR_ABORT;` |
|         - |   938 | `			}` |
|       269 |   939 | `			for( i = 0; i < nIndent; i++ ){` |
|       213 |   940 | `				if( zLine[i] != zPrefix[i] ){` |
|        10 |   941 | `					unsigned char c = (unsigned char)zLine[i];` |
|        10 |   942 | `					if( c == ' ' \|\| c == '\t' ){` |
|         5 |   943 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   944 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|         3 |   945 | `					}else{` |
|         7 |   946 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   947 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|         2 |   948 | `							nIndent);` |
|         - |   949 | `					}` |
|        10 |   950 | `					return SXERR_ABORT;` |
|         - |   951 | `				}` |
|       103 |   952 | `			}` |
|        57 |   953 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|        57 |   954 | `			zDst += nLine - nIndent;` |
|        33 |   955 | `		}else if( nLine == 1 ){` |
|         - |   956 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|       ! 0 |   957 | `			*zDst++ = '\r';` |
|       ! 0 |   958 | `		}` |
|        61 |   959 | `		if( z < zEnd ){` |
|        25 |   960 | `			*zDst++ = '\n';` |
|        25 |   961 | `			z++;` |
|        12 |   962 | `		}` |
|         1 |   963 | `	}` |
|        37 |   964 | `	pOut->zString = zBuf;` |
|        37 |   965 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|        37 |   966 | `	return SXRET_OK;` |
|        61 |   967 | `}` |
|         - |   968 | `/*` |
|         - |   969 | ` * Compile a nowdoc string.` |
|         - |   970 | ` * According to the PHP language reference manual:` |
|         - |   971 | ` *` |
|         - |   972 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - |   973 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - |   974 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|         - |   975 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|         - |   976 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|         - |   977 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|         - |   978 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|         - |   979 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|         - |   980 | ` *  of the closing identifier.` |
|         - |   981 | ` */` |
|        48 |   982 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         3 |   983 | `{` |
|         - |   984 | `	SyString sStripped;` |
|         - |   985 | `	SyString *pStr;` |
|         - |   986 | `	ph7_value *pObj;` |
|         - |   987 | `	sxu32 nIdx;` |
|         - |   988 | `	sxi32 rc;` |
|        51 |   989 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        51 |   990 | `	if( rc != SXRET_OK ){` |
|         6 |   991 | `		return rc;` |
|         - |   992 | `	}` |
|        46 |   993 | `	pStr = &sStripped;` |
|        46 |   994 | `	nIdx = 0; /* Prevent compiler warning */` |
|        46 |   995 | `	if( pStr->nByte <= 0 ){` |
|         - |   996 | `		/* Empty string,load NULL */` |
|         7 |   997 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |   998 | `		return SXRET_OK;` |
|         - |   999 | `	}` |
|         - |  1000 | `	/* Reserve a new constant */` |
|        40 |  1001 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        40 |  1002 | `	if( pObj == 0 ){` |
|       ! 0 |  1003 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1004 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  1005 | `		return SXERR_ABORT;` |
|         - |  1006 | `	}` |
|         - |  1007 | `	/* No processing is done here, simply a memcpy() operation */` |
|        40 |  1008 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|         - |  1009 | `	/* Emit the load constant instruction */` |
|        40 |  1010 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  1011 | `	/* Node successfully compiled */` |
|        40 |  1012 | `	return SXRET_OK;` |
|        27 |  1013 | `}` |
|         - |  1014 | `/*` |
|         - |  1015 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|         - |  1016 | ` * According to the PHP language reference manual` |
|         - |  1017 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|         - |  1018 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|         - |  1019 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|         - |  1020 | ` *  property in a string with a minimum of effort.` |
|         - |  1021 | ` *  Simple syntax` |
|         - |  1022 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|         - |  1023 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|         - |  1024 | ` *   the end of the name.` |
|         - |  1025 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|         - |  1026 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|         - |  1027 | ` *   as to simple variables.` |
|         - |  1028 | ` *  Complex (curly) syntax` |
|         - |  1029 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|         - |  1030 | ` *   of complex expressions.` |
|         - |  1031 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|         - |  1032 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|         - |  1033 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|         - |  1034 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|         - |  1035 | ` */` |
|      2568 |  1036 | `static sxi32 GenStateProcessStringExpression(` |
|         - |  1037 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1038 | `	sxu32 nLine,         /* Line number */` |
|         - |  1039 | `	const char *zIn,     /* Raw expression */` |
|         - |  1040 | `	const char *zEnd     /* End of the expression */` |
|         - |  1041 | `	)` |
|         5 |  1042 | `{` |
|         - |  1043 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1044 | `	SySet sToken;` |
|         - |  1045 | `	sxi32 rc;` |
|         - |  1046 | `	/* Initialize the token set */` |
|      2573 |  1047 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1048 | `	/* Preallocate some slots */` |
|      2573 |  1049 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1050 | `	/* Tokenize the text */` |
|      2573 |  1051 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1052 | `	/* Swap delimiter */` |
|      2573 |  1053 | `	pTmpIn  = pGen->pIn;` |
|      2573 |  1054 | `	pTmpEnd = pGen->pEnd;` |
|      2573 |  1055 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2573 |  1056 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1057 | `	/* Compile the expression */` |
|      2573 |  1058 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1059 | `	/* Restore token stream */` |
|      2573 |  1060 | `	pGen->pIn  = pTmpIn;` |
|      2573 |  1061 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1062 | `	/* Release the token set */` |
|      2573 |  1063 | `	SySetRelease(&sToken);` |
|         - |  1064 | `	/* Compilation result */` |
|      2573 |  1065 | `	return rc;` |
|         5 |  1066 | `}` |
|         - |  1067 | `/*` |
|         - |  1068 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1069 | ` */` |
|     83692 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1071 | `{` |
|         - |  1072 | `	ph7_value *pConstObj;` |
|     83697 |  1073 | `	sxu32 nIdx = 0;` |
|         - |  1074 | `	/* Reserve a new constant */` |
|     83697 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     83697 |  1076 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1078 | `		return 0;` |
|         - |  1079 | `	}` |
|     83697 |  1080 | `	(*pCount)++;` |
|     83697 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1082 | `	/* Emit the load constant instruction */` |
|     83697 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     83697 |  1084 | `	return pConstObj;` |
|     41851 |  1085 | `}` |
|         - |  1086 | `/*` |
|         - |  1087 | ` * Compile a double quoted/heredoc string.` |
|         - |  1088 | ` * According to the PHP language reference manual` |
|         - |  1089 | ` * Heredoc` |
|         - |  1090 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - |  1091 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - |  1092 | ` *  to close the quotation.` |
|         - |  1093 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - |  1094 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - |  1095 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - |  1096 | ` *  Warning` |
|         - |  1097 | ` *  It is very important to note that the line with the closing identifier must contain` |
|         - |  1098 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|         - |  1099 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|         - |  1100 | ` *  It's also important to realize that the first character before the closing identifier must` |
|         - |  1101 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|         - |  1102 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|         - |  1103 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|         - |  1104 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|         - |  1105 | ` *  the end of the current file, a parse error will result at the last line.` |
|         - |  1106 | ` *  Heredocs can not be used for initializing class properties.` |
|         - |  1107 | ` * Double quoted` |
|         - |  1108 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|         - |  1109 | ` *  Escaped characters Sequence 	Meaning` |
|         - |  1110 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|         - |  1111 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|         - |  1112 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|         - |  1113 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|         - |  1114 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|         - |  1115 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|         - |  1116 | ` *  \\ backslash` |
|         - |  1117 | ` *  \$ dollar sign` |
|         - |  1118 | ` *  \" double-quote` |
|         - |  1119 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|         - |  1120 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|         - |  1121 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|         - |  1122 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|         - |  1123 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|         - |  1124 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|         - |  1125 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|         - |  1126 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|         - |  1127 | ` * See string parsing for details.` |
|         - |  1128 | ` */` |
|         - |  1129 | `/*` |
|         - |  1130 | ` * Line number of an escape sequence inside the string body being compiled:` |
|         - |  1131 | ` * the token's line plus every newline before the escape (php reports the` |
|         - |  1132 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|         - |  1133 | ` * on the line after the '<<<' marker, hence the +1.` |
|         - |  1134 | ` */` |
|         6 |  1135 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|         3 |  1136 | `{` |
|         9 |  1137 | `	const char *z = pGen->pIn->sData.zString;` |
|         9 |  1138 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|        15 |  1139 | `	for( ; z < zPos ; z++ ){` |
|         9 |  1140 | `		if( z[0] == '\n' ){` |
|       ! 0 |  1141 | `			nLine++;` |
|       ! 0 |  1142 | `		}` |
|         6 |  1143 | `	}` |
|         9 |  1144 | `	return nLine;` |
|         3 |  1145 | `}` |
|         - |  1146 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|         - |  1147 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|     82132 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1149 | `{` |
|     82137 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|     82137 |  1152 | `	ph7_value *pObj = 0;` |
|         - |  1153 | `	sxi32 iCons;` |
|         - |  1154 | `	sxi32 rc;` |
|         - |  1155 | `	/* Delimit the string */` |
|     82137 |  1156 | `	zIn  = pStr->zString;` |
|     82137 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|     82137 |  1158 | `	if( zIn >= zEnd ){` |
|         - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1162 | `		 */` |
|       381 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       381 |  1164 | `		return SXRET_OK;` |
|         - |  1165 | `	}` |
|     81761 |  1166 | `	zCur = 0;` |
|         - |  1167 | `	/* Compile the node */` |
|     81761 |  1168 | `	iCons = 0;` |
|     42162 |  1169 | `	for(;;){` |
|    117115 |  1170 | `		zCur = zIn;` |
|   1585081 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1470539 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        72 |  1173 | `				break;` |
|   1470406 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2440 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1220 |  1176 | `					break;` |
|         - |  1177 | `			}` |
|   1467971 |  1178 | `			zIn++;` |
|         5 |  1179 | `		}` |
|    117115 |  1180 | `		if( zIn > zCur ){` |
|     56813 |  1181 | `			if( pObj == 0 ){` |
|     56219 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     56219 |  1183 | `				if( pObj == 0 ){` |
|       ! 0 |  1184 | `					return SXERR_ABORT;` |
|         - |  1185 | `				}` |
|     28107 |  1186 | `			}` |
|     56813 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     28404 |  1188 | `		}` |
|    117115 |  1189 | `		if( zIn >= zEnd ){` |
|     81759 |  1190 | `			break;` |
|         - |  1191 | `		}` |
|     35361 |  1192 | `		if( zIn[0] == '\\' ){` |
|     32793 |  1193 | `			const char *zPtr = 0;` |
|         - |  1194 | `			sxu32 n;` |
|     32793 |  1195 | `			zIn++;` |
|     32793 |  1196 | `			if( pObj == 0 ){` |
|     27483 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27483 |  1198 | `				if( pObj == 0 ){` |
|       ! 0 |  1199 | `					return SXERR_ABORT;` |
|         - |  1200 | `				}` |
|     13739 |  1201 | `			}` |
|     32793 |  1202 | `			if( zIn >= zEnd ){` |
|         - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1205 | `				break;` |
|         - |  1206 | `			}` |
|     32791 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|     32791 |  1208 | `			switch( zIn[0] ){` |
|        11 |  1209 | `			case '$':` |
|         - |  1210 | `				/* Dollar sign */` |
|        25 |  1211 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        25 |  1212 | `				break;` |
|        52 |  1213 | `			case '\\':` |
|         - |  1214 | `				/* A literal backslash */` |
|       109 |  1215 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       109 |  1216 | `				break;` |
|         1 |  1217 | `			case 'e':` |
|         - |  1218 | `				/* Escape (ESC) ASCII code 27 */` |
|         3 |  1219 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|         3 |  1220 | `				break;` |
|         4 |  1221 | `			case 'f':` |
|         - |  1222 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|         9 |  1223 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|         9 |  1224 | `				break;` |
|     13830 |  1225 | `			case 'n':` |
|         - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     27665 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     27665 |  1228 | `				break;` |
|        27 |  1229 | `			case 'r':` |
|         - |  1230 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1231 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1232 | `				break;` |
|      2001 |  1233 | `			case 't':` |
|         - |  1234 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      4007 |  1235 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      4007 |  1236 | `				break;` |
|         3 |  1237 | `			case 'v':` |
|         - |  1238 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|         7 |  1239 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|         7 |  1240 | `				break;` |
|       141 |  1241 | `			case '"':` |
|       287 |  1242 | `				if( bHeredoc ){` |
|         - |  1243 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|         5 |  1244 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|         3 |  1245 | `				}else{` |
|         - |  1246 | `					/* Double quote */` |
|       283 |  1247 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|         - |  1248 | `				}` |
|       287 |  1249 | `				break;` |
|        25 |  1250 | `			case '0': case '1': case '2': case '3':` |
|         - |  1251 | `			case '4': case '5': case '6': case '7': {` |
|         - |  1252 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|         - |  1253 | `				 * warns and wraps to the low byte, matching php 8. */` |
|        52 |  1254 | `				int c = 0;` |
|         - |  1255 | `				char cOut;` |
|       148 |  1256 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|       126 |  1257 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|        15 |  1258 | `						break;` |
|         - |  1259 | `					}` |
|        98 |  1260 | `					c = c * 8 + (zPtr[0] - '0');` |
|        50 |  1261 | `				}` |
|        52 |  1262 | `				if( c > 0xFF ){` |
|         - |  1263 | `					SyString sSeq;` |
|         3 |  1264 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|         3 |  1265 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1266 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|         3 |  1267 | `					c &= 0xFF;` |
|         1 |  1268 | `				}` |
|        52 |  1269 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|        52 |  1270 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|        52 |  1271 | `				n = (sxu32)(zPtr-zIn);` |
|        52 |  1272 | `				break;` |
|         - |  1273 | `			}` |
|       273 |  1274 | `			case 'x':` |
|       818 |  1275 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|         - |  1276 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|       543 |  1277 | `					int c = SyHexToint(zIn[1]);` |
|         - |  1278 | `					char cOut;` |
|       543 |  1279 | `					n += sizeof(char);` |
|       543 |  1280 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|       539 |  1281 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|       539 |  1282 | `						n += sizeof(char);` |
|       269 |  1283 | `					}` |
|       543 |  1284 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|       543 |  1285 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       272 |  1286 | `				}else{` |
|         - |  1287 | `					/* Not an escape: keep the backslash, as php does */` |
|         5 |  1288 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|         - |  1289 | `				}` |
|       547 |  1290 | `				break;` |
|         9 |  1291 | `			case 'u':` |
|        18 |  1292 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|        22 |  1293 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|         - |  1294 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|         - |  1295 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|         - |  1296 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|         - |  1297 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|         - |  1298 | `					 * followed by {$...} curly interpolation. */` |
|        15 |  1299 | `					sxu32 nCp = 0;` |
|        15 |  1300 | `					zPtr = &zIn[2];` |
|        59 |  1301 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|        46 |  1302 | `						if( nCp <= 0x10FFFF ){` |
|         - |  1303 | `							/* stop accumulating once out of range: keeps a long` |
|         - |  1304 | `							 * digit run from wrapping sxu32 */` |
|        46 |  1305 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|        22 |  1306 | `						}` |
|        46 |  1307 | `						zPtr++;` |
|         2 |  1308 | `					}` |
|        15 |  1309 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|         - |  1310 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|         - |  1311 | `						 * malformed sequence so later errors are still reported. */` |
|         3 |  1312 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1313 | `							"Invalid UTF-8 codepoint escape sequence");` |
|         3 |  1314 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1315 | `							return SXERR_ABORT;` |
|         - |  1316 | `						}` |
|         3 |  1317 | `						n = (sxu32)(zPtr-zIn);` |
|         3 |  1318 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|         3 |  1319 | `							n += sizeof(char);` |
|         1 |  1320 | `						}` |
|         3 |  1321 | `						break;` |
|         - |  1322 | `					}` |
|        12 |  1323 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|        12 |  1324 | `					if( nCp > 0x10FFFF ){` |
|         3 |  1325 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1326 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|         3 |  1327 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1328 | `							return SXERR_ABORT;` |
|         - |  1329 | `						}` |
|         3 |  1330 | `						break;` |
|         - |  1331 | `					}` |
|         - |  1332 | `					{` |
|         - |  1333 | `						char zUtf[4];` |
|         9 |  1334 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|         9 |  1335 | `						SX_WRITE_UTF8(zOut,nCp);` |
|         9 |  1336 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|         - |  1337 | `					}` |
|         5 |  1338 | `				}else{` |
|         - |  1339 | `					/* Not an escape: keep the backslash, as php does */` |
|         7 |  1340 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|         - |  1341 | `				}` |
|        15 |  1342 | `				break;` |
|        16 |  1343 | `			default:` |
|         - |  1344 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|         - |  1345 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|         - |  1346 | `				 * in the source buffer — one batched append. */` |
|        33 |  1347 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|        32 |  1348 | `				break;` |
|         - |  1349 | `			}` |
|         - |  1350 | `			/* Advance the stream cursor */` |
|     32791 |  1351 | `			zIn += n;` |
|     32791 |  1352 | `			continue;` |
|         - |  1353 | `		}` |
|      2573 |  1354 | `		if( zIn[0] == '{' ){` |
|         - |  1355 | `			/* Curly syntax */` |
|         - |  1356 | `			const char *zExpr;` |
|       141 |  1357 | `			sxi32 iNest = 1;` |
|       141 |  1358 | `			zIn++;` |
|       141 |  1359 | `			zExpr = zIn;` |
|         - |  1360 | `			/* Synchronize with the next closing curly braces */` |
|      1419 |  1361 | `			while( zIn < zEnd ){` |
|      1419 |  1362 | `				if( zIn[0] == '{' ){` |
|         - |  1363 | `					/* Increment nesting level */` |
|         9 |  1364 | `					iNest++;` |
|      1415 |  1365 | `				}else if(zIn[0] == '}' ){` |
|         - |  1366 | `					/* Decrement nesting level */` |
|       149 |  1367 | `					iNest--;` |
|       149 |  1368 | `					if( iNest <= 0 ){` |
|       141 |  1369 | `						break;` |
|         - |  1370 | `					}` |
|         4 |  1371 | `				}` |
|      1281 |  1372 | `				zIn++;` |
|         3 |  1373 | `			}` |
|         - |  1374 | `			/* Process the expression */` |
|       141 |  1375 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|       141 |  1376 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1377 | `				return SXERR_ABORT;` |
|         - |  1378 | `			}` |
|       141 |  1379 | `			if( rc != SXERR_EMPTY ){` |
|       141 |  1380 | `				++iCons;` |
|        69 |  1381 | `			}` |
|       141 |  1382 | `			if( zIn < zEnd ){` |
|         - |  1383 | `				/* Jump the trailing curly */` |
|       141 |  1384 | `				zIn++;` |
|        69 |  1385 | `			}` |
|        72 |  1386 | `		}else{` |
|         - |  1387 | `			/* Simple syntax */` |
|      2435 |  1388 | `			const char *zExpr = zIn;` |
|         - |  1389 | `			/* Assemble variable name */` |
|      1240 |  1390 | `			for(;;){` |
|         - |  1391 | `				/* Jump leading dollars */` |
|      4915 |  1392 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2435 |  1393 | `					zIn++;` |
|         5 |  1394 | `				}` |
|      1240 |  1395 | `				for(;;){` |
|     12817 |  1396 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9097 |  1397 | `						zIn++;` |
|         5 |  1398 | `					}` |
|      2485 |  1399 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1400 | `						/* UTF-8 stream */` |
|       ! 0 |  1401 | `						zIn++;` |
|       ! 0 |  1402 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1403 | `							zIn++;` |
|       ! 0 |  1404 | `						}` |
|       ! 0 |  1405 | `						continue;` |
|         - |  1406 | `					}` |
|      2485 |  1407 | `					break;` |
|       ! 0 |  1408 | `				}` |
|      2485 |  1409 | `				if( zIn >= zEnd ){` |
|       263 |  1410 | `					break;` |
|         - |  1411 | `				}` |
|      2227 |  1412 | `				if( zIn[0] == '[' ){` |
|        12 |  1413 | `					sxi32 iSquare = 1;` |
|        12 |  1414 | `					zIn++;` |
|        28 |  1415 | `					while( zIn < zEnd ){` |
|        28 |  1416 | `						if( zIn[0] == '[' ){` |
|       ! 0 |  1417 | `							iSquare++;` |
|        28 |  1418 | `						}else if (zIn[0] == ']' ){` |
|        12 |  1419 | `							iSquare--;` |
|        12 |  1420 | `							if( iSquare <= 0 ){` |
|        12 |  1421 | `								break;` |
|         - |  1422 | `							}` |
|       ! 0 |  1423 | `						}` |
|        18 |  1424 | `						zIn++;` |
|         2 |  1425 | `					}` |
|        12 |  1426 | `					if( zIn < zEnd ){` |
|        12 |  1427 | `						zIn++;` |
|         5 |  1428 | `					}` |
|        12 |  1429 | `					break;` |
|      2217 |  1430 | `				}else if(zIn[0] == '{' ){` |
|         6 |  1431 | `					sxi32 iCurly = 1;` |
|         6 |  1432 | `					zIn++;` |
|        18 |  1433 | `					while( zIn < zEnd ){` |
|        16 |  1434 | `						if( zIn[0] == '{' ){` |
|       ! 0 |  1435 | `							iCurly++;` |
|        16 |  1436 | `						}else if (zIn[0] == '}' ){` |
|         3 |  1437 | `							iCurly--;` |
|         3 |  1438 | `							if( iCurly <= 0 ){` |
|         3 |  1439 | `								break;` |
|         - |  1440 | `							}` |
|       ! 0 |  1441 | `						}` |
|        14 |  1442 | `						zIn++;` |
|         2 |  1443 | `					}` |
|         6 |  1444 | `					if( zIn < zEnd ){` |
|         3 |  1445 | `						zIn++;` |
|         1 |  1446 | `					}` |
|         6 |  1447 | `					break;` |
|      2213 |  1448 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1449 | `					/* Member access operator '->' */` |
|        53 |  1450 | `					zIn += 2;` |
|      2188 |  1451 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1452 | `					/* Static member access operator '::' */` |
|       ! 0 |  1453 | `					zIn += 2;` |
|       ! 0 |  1454 | `				}else{` |
|      1084 |  1455 | `					break;` |
|         - |  1456 | `				}` |
|         3 |  1457 | `			}` |
|         - |  1458 | `			/* Process the expression */` |
|      2435 |  1459 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2435 |  1460 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1461 | `				return SXERR_ABORT;` |
|         - |  1462 | `			}` |
|      2435 |  1463 | `			if( rc != SXERR_EMPTY ){` |
|      2433 |  1464 | `				++iCons;` |
|      1214 |  1465 | `			}` |
|         - |  1466 | `		}` |
|         - |  1467 | `		/* Invalidate the previously used constant */` |
|      2573 |  1468 | `		pObj = 0;` |
|         5 |  1469 | `	}/*for(;;)*/` |
|     81761 |  1470 | `	if( iCons > 1 ){` |
|         - |  1471 | `		/* Concatenate all compiled constants */` |
|      1861 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       928 |  1473 | `	}` |
|         - |  1474 | `	/* Node successfully compiled */` |
|     81761 |  1475 | `	return SXRET_OK;` |
|     41071 |  1476 | `}` |
|         - |  1477 | `/*` |
|         - |  1478 | ` * Compile a double quoted string.` |
|         - |  1479 | ` *  See the block-comment above for more information.` |
|         - |  1480 | ` */` |
|     82070 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1482 | `{` |
|         - |  1483 | `	sxi32 rc;` |
|     82075 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     41035 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1486 | `	/* Compilation result */` |
|     82075 |  1487 | `	return rc;` |
|         5 |  1488 | `}` |
|         - |  1489 | `/*` |
|         - |  1490 | ` * Compile a Heredoc string.` |
|         - |  1491 | ` *  See the block-comment above for more information.` |
|         - |  1492 | ` */` |
|        66 |  1493 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 |  1494 | `{` |
|         - |  1495 | `	SyString sOrig, sStripped;` |
|         - |  1496 | `	sxi32 rc;` |
|        70 |  1497 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        70 |  1498 | `	if( rc != SXRET_OK ){` |
|         6 |  1499 | `		return rc;` |
|         - |  1500 | `	}` |
|         - |  1501 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|         - |  1502 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|         - |  1503 | `	 * Restore before returning so downstream code that references pIn is` |
|         - |  1504 | `	 * unaffected, including on the error path. */` |
|        65 |  1505 | `	sOrig = pGen->pIn->sData;` |
|        65 |  1506 | `	pGen->pIn->sData = sStripped;` |
|        65 |  1507 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|        65 |  1508 | `	pGen->pIn->sData = sOrig;` |
|        31 |  1509 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        65 |  1510 | `	return rc;` |
|        37 |  1511 | `}` |
|         - |  1512 | `/*` |
|         - |  1513 | ` * Compile an array entry whether it is a key or a value.` |
|         - |  1514 | ` *  Notes on array entries.` |
|         - |  1515 | ` *  According to the PHP language reference manual` |
|         - |  1516 | ` *  An array can be created by the array() language construct.` |
|         - |  1517 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|         - |  1518 | ` *  array(  key =>  value` |
|         - |  1519 | ` *    , ...` |
|         - |  1520 | ` *    )` |
|         - |  1521 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|         - |  1522 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|         - |  1523 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|         - |  1524 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|         - |  1525 | ` *  contain integer and string indices.` |
|         - |  1526 | ` *  A value can be any PHP type.` |
|         - |  1527 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|         - |  1528 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|         - |  1529 | ` *  is specified, that value will be overwritten.` |
|         - |  1530 | ` */` |
|   1026406 |  1531 | `static sxi32 GenStateCompileArrayEntry(` |
|         - |  1532 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1533 | `	SyToken *pIn,        /* Token stream */` |
|         - |  1534 | `	SyToken *pEnd,       /* End of the token stream */` |
|         - |  1535 | `	sxi32 iFlags,        /* Compilation flags */` |
|         - |  1536 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|         - |  1537 | `	)` |
|         5 |  1538 | `{` |
|         - |  1539 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1540 | `	sxi32 rc;` |
|         - |  1541 | `	/* Swap token stream */` |
|   1026411 |  1542 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1543 | `	/* Compile the expression*/` |
|   1026411 |  1544 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1545 | `	/* Restore token stream */` |
|   1026411 |  1546 | `	RE_SWAP_DELIMITER(pGen);` |
|   1026411 |  1547 | `	return rc;` |
|         5 |  1548 | `}` |
|         - |  1549 | `/*` |
|         - |  1550 | ` * Expression tree validator callback for the 'array' language construct.` |
|         - |  1551 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1552 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1553 | ` * error message.` |
|         - |  1554 | ` * See the routine responible of compiling the array language construct` |
|         - |  1555 | ` * for more inforation.` |
|         - |  1556 | ` */` |
|        36 |  1557 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         4 |  1558 | `{` |
|        40 |  1559 | `	sxi32 rc = SXRET_OK;` |
|        40 |  1560 | `	if( pRoot->pOp ){` |
|        14 |  1561 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|        12 |  1562 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|        16 |  1563 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1564 | `			/* Unexpected expression */` |
|        13 |  1565 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1566 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|        13 |  1567 | `			if( rc != SXERR_ABORT ){` |
|        13 |  1568 | `				rc = SXERR_INVALID;` |
|         5 |  1569 | `			}` |
|         9 |  1570 | `		}` |
|        31 |  1571 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1572 | `		/* Unexpected expression */` |
|         3 |  1573 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1574 | `			"array(): Expecting a variable after reference operator '&'");` |
|         3 |  1575 | `		if( rc != SXERR_ABORT ){` |
|         3 |  1576 | `			rc = SXERR_INVALID;` |
|         1 |  1577 | `		}` |
|         1 |  1578 | `	}` |
|        40 |  1579 | `	return rc;` |
|         4 |  1580 | `}` |
|         - |  1581 | `/*` |
|         - |  1582 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|         - |  1583 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|         - |  1584 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|         - |  1585 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|         - |  1586 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|         - |  1587 | ` */` |
|    966068 |  1588 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1589 | `{` |
|    966073 |  1590 | `	SyToken *pCur = pStart;` |
|    966073 |  1591 | `	sxi32 iNest = 0;` |
|   2664317 |  1592 | `	while( pCur < pEnd ){` |
|   2090619 |  1593 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    392371 |  1594 | `			return pCur;` |
|         - |  1595 | `		}` |
|         - |  1596 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1597 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1598 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1599 | `		 */` |
|   1698253 |  1600 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23747 |  1601 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23747 |  1602 | `			SyToken *pFn = pCur;` |
|     23742 |  1603 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1604 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1605 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1606 | `				pFn = &pCur[1];` |
|       ! 0 |  1607 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1608 | `			}` |
|     23747 |  1609 | `			if( nKw == PH7_TKWRD_FN ){` |
|         5 |  1610 | `				pCur = pFn + 1; /* past 'fn' */` |
|         5 |  1611 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  1612 | `					pCur++;` |
|       ! 0 |  1613 | `				}` |
|         5 |  1614 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         5 |  1615 | `					pCur++;` |
|         5 |  1616 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1617 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         5 |  1618 | `					if( pCur < pEnd ){` |
|         5 |  1619 | `						pCur++;` |
|         2 |  1620 | `					}` |
|         2 |  1621 | `				}` |
|         5 |  1622 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|       ! 0 |  1623 | `					pCur++;` |
|       ! 0 |  1624 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|       ! 0 |  1625 | `						&& pCur->sData.nByte == 1` |
|       ! 0 |  1626 | `						&& pCur->sData.zString[0] == '?' ){` |
|       ! 0 |  1627 | `						pCur++;` |
|       ! 0 |  1628 | `					}` |
|       ! 0 |  1629 | `					if( pCur < pEnd` |
|       ! 0 |  1630 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  1631 | `						pCur++;` |
|       ! 0 |  1632 | `					}` |
|       ! 0 |  1633 | `				}` |
|         - |  1634 | `				/* The rest of the entry is the arrow-function body — no outer` |
|         - |  1635 | `				 * key to extract. */` |
|         5 |  1636 | `				return pEnd;` |
|         - |  1637 | `			}` |
|         - |  1638 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|         - |  1639 | `			 * entry separator. Skip past the full match span. */` |
|     23743 |  1640 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|         3 |  1641 | `				pCur++; /* past 'match' */` |
|         3 |  1642 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         3 |  1643 | `					pCur++;` |
|         3 |  1644 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1645 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         3 |  1646 | `					if( pCur < pEnd ){` |
|         3 |  1647 | `						pCur++;` |
|         1 |  1648 | `					}` |
|         1 |  1649 | `				}` |
|         3 |  1650 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|         3 |  1651 | `					pCur++;` |
|         3 |  1652 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1653 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|         3 |  1654 | `					if( pCur < pEnd ){` |
|         3 |  1655 | `						pCur++;` |
|         1 |  1656 | `					}` |
|         1 |  1657 | `				}` |
|         3 |  1658 | `				continue;` |
|         - |  1659 | `			}` |
|     11868 |  1660 | `		}` |
|   1698247 |  1661 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     55615 |  1662 | `			iNest++;` |
|   1670442 |  1663 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1664 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1665 | `			 * parser will shortly detect any syntax error. */` |
|     55615 |  1666 | `			iNest--;` |
|     27805 |  1667 | `		}` |
|   1698247 |  1668 | `		pCur++;` |
|         5 |  1669 | `	}` |
|    573703 |  1670 | `	return pEnd;` |
|    483039 |  1671 | `}` |
|         - |  1672 | `/*` |
|         - |  1673 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1674 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1675 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1676 | ` */` |
|    515972 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1678 | `{` |
|         - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1680 | `	SyToken *pKey,*pCur;` |
|    515977 |  1681 | `	sxi32 iEmitRef = 0;` |
|    515977 |  1682 | `	sxi32 iSpread = 0;` |
|    515977 |  1683 | `	sxi32 nPair = 0;` |
|         - |  1684 | `	sxi32 rc;` |
|    515977 |  1685 | `	xValidator = 0;` |
|    626266 |  1686 | `	for(;;){` |
|         - |  1687 | `		/* Jump leading commas */` |
|   1765299 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    512767 |  1689 | `			pGen->pIn++;` |
|         5 |  1690 | `		}` |
|   1252537 |  1691 | `		pCur = pGen->pIn;` |
|   1252537 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1693 | `			/* No more entry to process */` |
|    515961 |  1694 | `			break;` |
|         - |  1695 | `		}` |
|    736581 |  1696 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1697 | `			continue;` |
|         - |  1698 | `		}` |
|         - |  1699 | `		/* Compile the key if available */` |
|    736581 |  1700 | `		pKey = pCur;` |
|    736581 |  1701 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|    736581 |  1702 | `		rc = SXERR_EMPTY;` |
|    736581 |  1703 | `		if( pCur < pGen->pIn ){` |
|    289577 |  1704 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1705 | `				/* Missing value */` |
|        13 |  1706 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|        13 |  1707 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1708 | `					return SXERR_ABORT;` |
|         - |  1709 | `				}` |
|        13 |  1710 | `				return SXRET_OK;` |
|         - |  1711 | `			}` |
|         - |  1712 | `			/* Compile the expression holding the key */` |
|    289567 |  1713 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1714 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    289567 |  1715 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1716 | `				return SXERR_ABORT;` |
|         - |  1717 | `			}` |
|    289567 |  1718 | `			pCur++; /* Jump the '=>' operator */` |
|    591790 |  1719 | `		}else if( pKey == pCur ){` |
|         - |  1720 | `			/* Key is omitted,emit a warning */` |
|       ! 0 |  1721 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|       ! 0 |  1722 | `			pCur++; /* Jump the '=>' operator */` |
|       ! 0 |  1723 | `		}else{` |
|         - |  1724 | `			/* Reset back the cursor and point to the entry value */` |
|    447009 |  1725 | `			pCur = pKey;` |
|         - |  1726 | `		}` |
|    736571 |  1727 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1728 | `			/* No available key,load NULL */` |
|    447011 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    223503 |  1730 | `		}` |
|    736571 |  1731 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|         - |  1732 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|        45 |  1733 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|        45 |  1734 | `			iEmitRef = 1;` |
|        45 |  1735 | `			pCur++; /* Jump the '&' token */` |
|        45 |  1736 | `			if( pCur >= pGen->pIn ){` |
|         - |  1737 | `				/* Missing value */` |
|         3 |  1738 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|         3 |  1739 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1740 | `					return SXERR_ABORT;` |
|         - |  1741 | `				}` |
|         3 |  1742 | `				return SXRET_OK;` |
|         - |  1743 | `			}` |
|        19 |  1744 | `		}` |
|         - |  1745 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|         - |  1746 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|         - |  1747 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|         - |  1748 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|         - |  1749 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|    736569 |  1750 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|    736569 |  1751 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|         - |  1752 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|         - |  1753 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|         - |  1754 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|         - |  1755 | `			 * output is engine-portable. */` |
|         6 |  1756 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|         - |  1757 | `				"syntax error, unexpected token \"...\"");` |
|         6 |  1758 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1759 | `				return SXERR_ABORT;` |
|         - |  1760 | `			}` |
|         6 |  1761 | `			return SXRET_OK;` |
|         - |  1762 | `		}` |
|         - |  1763 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|         - |  1764 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|         - |  1765 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|         - |  1766 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|         - |  1767 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|   1104845 |  1768 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    368280 |  1769 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1770 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    368280 |  1771 | `			xValidator);` |
|    736565 |  1772 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1773 | `			return SXERR_ABORT;` |
|         - |  1774 | `		}` |
|    736565 |  1775 | `		if( iSpread ){` |
|         - |  1776 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        69 |  1777 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|    736532 |  1778 | `		}else if( iEmitRef ){` |
|         - |  1779 | `			/* Emit the load reference instruction */` |
|        40 |  1780 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1781 | `		}` |
|    736565 |  1782 | `		xValidator = 0;` |
|    736565 |  1783 | `		iEmitRef = 0;` |
|    736565 |  1784 | `		iSpread = 0;` |
|    736565 |  1785 | `		nPair++;` |
|         5 |  1786 | `	}` |
|         - |  1787 | `	/* Emit the load map instruction */` |
|    515961 |  1788 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1789 | `	/* Node successfully compiled */` |
|    515961 |  1790 | `	return SXRET_OK;` |
|    257991 |  1791 | `}` |
|         - |  1792 | `/*` |
|         - |  1793 | ` * Compile the 'array' language construct.` |
|         - |  1794 | ` *	 According to the PHP language reference manual` |
|         - |  1795 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1796 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1797 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1798 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1799 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1800 | ` */` |
|    293412 |  1801 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1802 | `{` |
|         - |  1803 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    293417 |  1804 | `	pGen->pIn += 2;` |
|    293417 |  1805 | `	pGen->pEnd--;` |
|    146706 |  1806 | `	SXUNUSED(iCompileFlag);` |
|    293417 |  1807 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1808 | `}` |
|         - |  1809 | `/*` |
|         - |  1810 | ` * Compile the PHP 8.5 clone(...) call form:` |
|         - |  1811 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|         - |  1812 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|         - |  1813 | ` *                                              property updates as scope-aware writes` |
|         - |  1814 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|         - |  1815 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|         - |  1816 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|         - |  1817 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|         - |  1818 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|         - |  1819 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|         - |  1820 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|         - |  1821 | ` */` |
|        22 |  1822 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  1823 | `{` |
|         - |  1824 | `	SyToken *pIn,*pEnd,*pNext;` |
|        24 |  1825 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|        24 |  1826 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|        24 |  1827 | `	int nArg = 0;` |
|         - |  1828 | `	sxi32 rc;` |
|        11 |  1829 | `	SXUNUSED(iCompileFlag);` |
|         - |  1830 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|        24 |  1831 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|        24 |  1832 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|         - |  1833 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|        24 |  1834 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       ! 0 |  1835 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  1836 | `			"clone(...) first-class callable form is not yet supported");` |
|         - |  1837 | `	}` |
|         - |  1838 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|        62 |  1839 | `	while( pIn < pEnd ){` |
|        40 |  1840 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|        40 |  1841 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|       ! 0 |  1842 | `			break;` |
|         - |  1843 | `		}` |
|        40 |  1844 | `		pArgStart = pIn;` |
|        40 |  1845 | `		pArgEnd   = pNext;` |
|         - |  1846 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|         - |  1847 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|        38 |  1848 | `		if( (pArgEnd - pArgStart) >= 2` |
|        37 |  1849 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        23 |  1850 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|         5 |  1851 | `			pName = pArgStart;` |
|         5 |  1852 | `			pArgStart += 2;` |
|         2 |  1853 | `		}` |
|        40 |  1854 | `		if( pName ){` |
|         - |  1855 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|         - |  1856 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|         4 |  1857 | `			if( pName->sData.nByte == sizeof("object")-1` |
|         4 |  1858 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|         3 |  1859 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|         4 |  1860 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|         3 |  1861 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|         3 |  1862 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         2 |  1863 | `			}else{` |
|       ! 0 |  1864 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|       ! 0 |  1865 | `					"Unknown named parameter $%z",&pName->sData);` |
|         1 |  1866 | `			}` |
|        38 |  1867 | `		}else if( nArg == 0 ){` |
|        22 |  1868 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        25 |  1869 | `		}else if( nArg == 1 ){` |
|        15 |  1870 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         8 |  1871 | `		}else{` |
|       ! 0 |  1872 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|         - |  1873 | `				"clone() expects at most 2 arguments");` |
|         - |  1874 | `		}` |
|        40 |  1875 | `		nArg++;` |
|        40 |  1876 | `		pIn = pNext;` |
|        40 |  1877 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 |  1878 | `			pIn++; /* step over the argument separator */` |
|         8 |  1879 | `		}` |
|         2 |  1880 | `	}` |
|        24 |  1881 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|       ! 0 |  1882 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  1883 | `			"clone() expects at least 1 argument, 0 given");` |
|         - |  1884 | `	}` |
|         - |  1885 | `	/* Object argument -> clone (+ __clone()). */` |
|        24 |  1886 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        24 |  1887 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  1888 | `		return SXERR_ABORT;` |
|         - |  1889 | `	}` |
|        24 |  1890 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|         - |  1891 | `	/* Property updates (evaluated after __clone runs). */` |
|        24 |  1892 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|        17 |  1893 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        17 |  1894 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1895 | `			return SXERR_ABORT;` |
|         - |  1896 | `		}` |
|        17 |  1897 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|         8 |  1898 | `	}` |
|        24 |  1899 | `	return SXRET_OK;` |
|        13 |  1900 | `}` |
|         - |  1901 | `/*` |
|         - |  1902 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|         - |  1903 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|         - |  1904 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|         - |  1905 | ` */` |
|    222560 |  1906 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1907 | `{` |
|         - |  1908 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    222565 |  1909 | `	pGen->pIn++;` |
|    222565 |  1910 | `	pGen->pEnd--;` |
|    111280 |  1911 | `	SXUNUSED(iCompileFlag);` |
|    222565 |  1912 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1913 | `}` |
|         - |  1914 | `/*` |
|         - |  1915 | ` * Expression tree validator callback for the 'list' language construct.` |
|         - |  1916 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1917 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1918 | ` * error message.` |
|         - |  1919 | ` * See the routine responible of compiling the list language construct` |
|         - |  1920 | ` * for more inforation.` |
|         - |  1921 | ` */` |
|       210 |  1922 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  1923 | `{` |
|       215 |  1924 | `	sxi32 rc = SXRET_OK;` |
|       215 |  1925 | `	if( pRoot->pOp ){` |
|         4 |  1926 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|         2 |  1927 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|         - |  1928 | `				/* Unexpected expression */` |
|       ! 0 |  1929 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1930 | `					"list(): Expecting a variable not an expression");` |
|       ! 0 |  1931 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 |  1932 | `					rc = SXERR_INVALID;` |
|       ! 0 |  1933 | `				}` |
|         1 |  1934 | `		}` |
|       213 |  1935 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1936 | `		/* Unexpected expression */` |
|         6 |  1937 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1938 | `			"list(): Expecting a variable not an expression");` |
|         6 |  1939 | `		if( rc != SXERR_ABORT ){` |
|         6 |  1940 | `			rc = SXERR_INVALID;` |
|         2 |  1941 | `		}` |
|         2 |  1942 | `	}` |
|       215 |  1943 | `	return rc;` |
|         5 |  1944 | `}` |
|         - |  1945 | `/*` |
|         - |  1946 | ` * Compile the 'list' language construct.` |
|         - |  1947 | ` *  According to the PHP language reference` |
|         - |  1948 | ` *  list(): Assign variables as if they were an array.` |
|         - |  1949 | ` *  list() is used to assign a list of variables in one operation.` |
|         - |  1950 | ` *  Description` |
|         - |  1951 | ` *   array list (mixed $varname [, mixed $... ] )` |
|         - |  1952 | ` *   Like array(), this is not really a function, but a language construct.` |
|         - |  1953 | ` *   list() is used to assign a list of variables in one operation.` |
|         - |  1954 | ` *  Parameters` |
|         - |  1955 | ` *   $varname: A variable.` |
|         - |  1956 | ` *  Return Values` |
|         - |  1957 | ` *   The assigned array.` |
|         - |  1958 | ` */` |
|         - |  1959 | `/* Nested list entry recorded during first pass of list body compilation */` |
|         - |  1960 | `struct NestedListEntry {` |
|         - |  1961 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|         - |  1962 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|         - |  1963 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|         - |  1964 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|         - |  1965 | `};` |
|         - |  1966 | `/*` |
|         - |  1967 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|         - |  1968 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|         - |  1969 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|         - |  1970 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|         - |  1971 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|         - |  1972 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|         - |  1973 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|         - |  1974 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|         - |  1975 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|         - |  1976 | ` */` |
|        22 |  1977 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|         1 |  1978 | `{` |
|         - |  1979 | `	SyToken *pNext;` |
|         - |  1980 | `	sxi32 rc;` |
|        53 |  1981 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|         - |  1982 | `		SyToken *pArrow,*pTarget;` |
|         - |  1983 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|        31 |  1984 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|        31 |  1985 | `		pTarget = &pArrow[1];` |
|        31 |  1986 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|         - |  1987 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|         - |  1988 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|       ! 0 |  1989 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  1990 | `				"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  1991 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  1992 | `		}` |
|         - |  1993 | `		/* DUP the source array (it is on the stack top) */` |
|        31 |  1994 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  1995 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|        31 |  1996 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|        31 |  1997 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1998 | `			return SXERR_ABORT;` |
|         - |  1999 | `		}` |
|         - |  2000 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|         - |  2001 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|         - |  2002 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|         - |  2003 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|         - |  2004 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|         - |  2005 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|        31 |  2006 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|        31 |  2007 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|        28 |  2008 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|        15 |  2009 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|         - |  2010 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|         - |  2011 | `			 * Treat source[key] as the inner body's source, then drop the` |
|         - |  2012 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|         5 |  2013 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|         5 |  2014 | `			SyToken *pSavedIn = pGen->pIn;` |
|         5 |  2015 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         5 |  2016 | `			pGen->pIn = pTarget;` |
|         5 |  2017 | `			pGen->pEnd = pNext;` |
|         5 |  2018 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|         2 |  2019 | `			             : PH7_CompileList(&(*pGen),0);` |
|         5 |  2020 | `			pGen->pIn = pSavedIn;` |
|         5 |  2021 | `			pGen->pEnd = pSavedEnd;` |
|         5 |  2022 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2023 | `				return SXERR_ABORT;` |
|         - |  2024 | `			}` |
|         5 |  2025 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         3 |  2026 | `		}else{` |
|         - |  2027 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|         - |  2028 | `			 * is already on the stack as the value; compiling the target appends` |
|         - |  2029 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|         - |  2030 | `			 * assignment does. */` |
|         - |  2031 | `			VmInstr *pInstr;` |
|        27 |  2032 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|        27 |  2033 | `			sxi32 iP1 = 0, iP2 = 0;` |
|        27 |  2034 | `			void *p3 = 0;` |
|        27 |  2035 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|         - |  2036 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|        27 |  2037 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2038 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2039 | `			}` |
|        27 |  2040 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|        27 |  2041 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|         3 |  2042 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|        26 |  2043 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         3 |  2044 | `					iVmOp = PH7_OP_STORE_IDX;` |
|         3 |  2045 | `					iP1 = pInstr->iP1;` |
|         3 |  2046 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         2 |  2047 | `				}else{` |
|        23 |  2048 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|        23 |  2049 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - |  2050 | `				}` |
|        13 |  2051 | `			}` |
|        27 |  2052 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|         - |  2053 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|         - |  2054 | `			 * source array is back on top for the next entry. */` |
|        27 |  2055 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         - |  2056 | `		}` |
|        31 |  2057 | `		pGen->pIn = &pNext[1];` |
|         1 |  2058 | `	}` |
|        23 |  2059 | `	return SXRET_OK;` |
|        12 |  2060 | `}` |
|         - |  2061 | `/*` |
|         - |  2062 | ` * Shared body for list() and short list [...] compilation.` |
|         - |  2063 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|         - |  2064 | ` * the opening delimiter and before the closing delimiter.` |
|         - |  2065 | ` */` |
|       122 |  2066 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|         5 |  2067 | `{` |
|         - |  2068 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|         - |  2069 | `	SyToken *pNext;` |
|         - |  2070 | `	SyToken *pClassifyIn;` |
|       127 |  2071 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|         - |  2072 | `	sxi32 nExpr;` |
|         - |  2073 | `	sxi32 rc;` |
|         - |  2074 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|         - |  2075 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|         - |  2076 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|         - |  2077 | `	 * list. */` |
|       127 |  2078 | `	pClassifyIn = pGen->pIn;` |
|       367 |  2079 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       245 |  2080 | `		if( pGen->pIn >= pNext ){` |
|        13 |  2081 | `			nEmpty++;` |
|       239 |  2082 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|        31 |  2083 | `			nKeyed++;` |
|        16 |  2084 | `		}else{` |
|       203 |  2085 | `			nPositional++;` |
|         - |  2086 | `		}` |
|       245 |  2087 | `		pGen->pIn = &pNext[1];` |
|         5 |  2088 | `	}` |
|       127 |  2089 | `	pGen->pIn = pClassifyIn;` |
|       127 |  2090 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|       ! 0 |  2091 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2092 | `			"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2093 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2094 | `	}` |
|       127 |  2095 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|       ! 0 |  2096 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2097 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|       ! 0 |  2098 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2099 | `	}` |
|       127 |  2100 | `	if( nKeyed > 0 ){` |
|        23 |  2101 | `		return GenStateCompileKeyedListBody(pGen);` |
|         - |  2102 | `	}` |
|       105 |  2103 | `	nExpr = 0;` |
|       105 |  2104 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       315 |  2105 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       215 |  2106 | `		if( pGen->pIn < pNext ){` |
|         - |  2107 | `			/* Check for nested list() */` |
|       203 |  2108 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         3 |  2109 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  2110 | `				/* Record this nested list for post-processing */` |
|         3 |  2111 | `				SyToken *pListEnd = 0;` |
|         3 |  2112 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|         3 |  2113 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         1 |  2114 | `				}` |
|         3 |  2115 | `				if( pListEnd ){` |
|         - |  2116 | `					struct NestedListEntry sEntry;` |
|         3 |  2117 | `					sEntry.nIndex = nExpr;` |
|         3 |  2118 | `					sEntry.pStart = pGen->pIn;` |
|         3 |  2119 | `					sEntry.pEnd = pListEnd + 1;` |
|         3 |  2120 | `					sEntry.isShort = 0;` |
|         3 |  2121 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         1 |  2122 | `				}` |
|         - |  2123 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|         3 |  2124 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       202 |  2125 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  2126 | `				/* Nested short destructuring [...] */` |
|        13 |  2127 | `				SyToken *pBracketEnd = 0;` |
|        13 |  2128 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|        13 |  2129 | `				if( pBracketEnd ){` |
|         - |  2130 | `					struct NestedListEntry sEntry;` |
|        13 |  2131 | `					sEntry.nIndex = nExpr;` |
|        13 |  2132 | `					sEntry.pStart = pGen->pIn;` |
|        13 |  2133 | `					sEntry.pEnd = pBracketEnd + 1;` |
|        13 |  2134 | `					sEntry.isShort = 1;` |
|        13 |  2135 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         6 |  2136 | `				}` |
|         - |  2137 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        13 |  2138 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |  2139 | `			}else{` |
|         - |  2140 | `				/* Compile the expression holding the variable */` |
|       189 |  2141 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       189 |  2142 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  2143 | `					SySetRelease(&sNested);` |
|       ! 0 |  2144 | `					return SXRET_OK;` |
|         - |  2145 | `				}` |
|         - |  2146 | `			}` |
|       104 |  2147 | `		}else{` |
|         - |  2148 | `			/* Empty entry,load NULL */` |
|        13 |  2149 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|         - |  2150 | `		}` |
|       215 |  2151 | `		nExpr++;` |
|         - |  2152 | `		/* Advance the stream cursor */` |
|       215 |  2153 | `		pGen->pIn = &pNext[1];` |
|         5 |  2154 | `	}` |
|         - |  2155 | `	/* Emit the LOAD_LIST instruction */` |
|       105 |  2156 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|         - |  2157 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|         - |  2158 | `	 * For each nested entry, emit code to extract the sub-array` |
|         - |  2159 | `	 * at the corresponding index and recursively destructure it.` |
|         - |  2160 | `	 */` |
|       105 |  2161 | `	if( SySetUsed(&sNested) > 0 ){` |
|        13 |  2162 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|         - |  2163 | `		sxu32 i;` |
|        27 |  2164 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|        15 |  2165 | `			SyToken *pSavedIn = pGen->pIn;` |
|        15 |  2166 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         - |  2167 | `			ph7_value *pIdx;` |
|         - |  2168 | `			sxu32 nConstIdx;` |
|         - |  2169 | `			/* DUP the source array (it's on stack top) */` |
|        15 |  2170 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2171 | `			/* Push the integer index for this nested entry */` |
|        15 |  2172 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|        15 |  2173 | `			if( pIdx == 0 ){` |
|       ! 0 |  2174 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2175 | `				SySetRelease(&sNested);` |
|       ! 0 |  2176 | `				return SXERR_ABORT;` |
|         - |  2177 | `			}` |
|        15 |  2178 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|        15 |  2179 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|         - |  2180 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|         - |  2181 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|         - |  2182 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|         - |  2183 | `			 */` |
|        15 |  2184 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|         - |  2185 | `			/* Recursively compile the inner list */` |
|        15 |  2186 | `			pGen->pIn = apNested[i].pStart;` |
|        15 |  2187 | `			pGen->pEnd = apNested[i].pEnd;` |
|        15 |  2188 | `			if( apNested[i].isShort ){` |
|        13 |  2189 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  2190 | `			}else{` |
|         3 |  2191 | `				rc = PH7_CompileList(&(*pGen),0);` |
|         - |  2192 | `			}` |
|        15 |  2193 | `			pGen->pIn = pSavedIn;` |
|        15 |  2194 | `			pGen->pEnd = pSavedEnd;` |
|        15 |  2195 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2196 | `				SySetRelease(&sNested);` |
|       ! 0 |  2197 | `				return SXERR_ABORT;` |
|         - |  2198 | `			}` |
|         - |  2199 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|        15 |  2200 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         8 |  2201 | `		}` |
|         6 |  2202 | `	}` |
|       105 |  2203 | `	SySetRelease(&sNested);` |
|         - |  2204 | `	/* Node successfully compiled */` |
|       105 |  2205 | `	return SXRET_OK;` |
|        66 |  2206 | `}` |
|        40 |  2207 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2208 | `{` |
|         - |  2209 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|        45 |  2210 | `	pGen->pIn += 2;` |
|        45 |  2211 | `	pGen->pEnd--;` |
|        20 |  2212 | `	SXUNUSED(iCompileFlag);` |
|        45 |  2213 | `	return GenStateCompileListBody(pGen);` |
|         5 |  2214 | `}` |
|        82 |  2215 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2216 | `{` |
|         - |  2217 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        84 |  2218 | `	pGen->pIn++;` |
|        84 |  2219 | `	pGen->pEnd--;` |
|        41 |  2220 | `	SXUNUSED(iCompileFlag);` |
|        84 |  2221 | `	return GenStateCompileListBody(pGen);` |
|         2 |  2222 | `}` |
|         - |  2223 | `/* Forward declarations */` |
|         - |  2224 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|         - |  2225 | `static int GenStateIsReservedConstant(SyString *pName);` |
|         - |  2226 | `static int GenStateIsReadonly(SyToken *pTok);` |
|         - |  2227 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|         - |  2228 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|         - |  2229 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - |  2230 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|         - |  2231 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|         - |  2232 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|         - |  2233 | `/*` |
|         - |  2234 | ` * Compile an annoynmous function or a closure.` |
|         - |  2235 | ` * According to the PHP language reference` |
|         - |  2236 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  2237 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  2238 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|         - |  2239 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|         - |  2240 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|         - |  2241 | ` *  Example Anonymous function variable assignment example` |
|         - |  2242 | ` * <?php` |
|         - |  2243 | ` * $greet = function($name)` |
|         - |  2244 | ` * {` |
|         - |  2245 | ` *    printf("Hello %s\r\n", $name);` |
|         - |  2246 | ` * };` |
|         - |  2247 | ` * $greet('World');` |
|         - |  2248 | ` * $greet('PHP');` |
|         - |  2249 | ` * ?>` |
|         - |  2250 | ` * Note that the implementation of annoynmous function and closure under` |
|         - |  2251 | ` * PH7 is completely different from the one used by the zend engine.` |
|         - |  2252 | ` */` |
|       464 |  2253 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2254 | `{` |
|       469 |  2255 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2256 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2257 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2258 | `							  * one thread is allowed to compile the script.` |
|         - |  2259 | `						      */` |
|         - |  2260 | `	SyString sName;` |
|       469 |  2261 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2262 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2263 | `	sxu32 nKwLine;` |
|       469 |  2264 | `	sxi32 iFlags = 0;` |
|         - |  2265 | `	sxu32 nLen;` |
|         - |  2266 | `	sxi32 rc;` |
|       232 |  2267 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2268 |  |
|       469 |  2269 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       464 |  2270 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       469 |  2271 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2272 | `		/* Static closure: no $this auto-capture, bind refused */` |
|         9 |  2273 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         9 |  2274 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         4 |  2275 | `	}` |
|       469 |  2276 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       469 |  2277 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2278 | `		pGen->pIn++;` |
|       ! 0 |  2279 | `	}` |
|         - |  2280 | `	/* Generate a unique name */` |
|       469 |  2281 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2282 | `	/* Make sure the generated name is unique */` |
|       469 |  2283 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2284 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2285 | `	}` |
|       469 |  2286 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2287 | `	/* Compile the lambda body */` |
|       469 |  2288 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       469 |  2289 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2290 | `		return SXERR_ABORT;` |
|         - |  2291 | `	}` |
|       469 |  2292 | `	if( pAnnonFunc ){` |
|       469 |  2293 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2294 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2295 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       469 |  2296 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2297 | `			return SXERR_ABORT;` |
|         - |  2298 | `		}` |
|       232 |  2299 | `	}` |
|         - |  2300 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2301 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2302 | `	 * the handler wraps either in a Closure instance. */` |
|       469 |  2303 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2304 | `	/* Node successfully compiled */` |
|       469 |  2305 | `	return SXRET_OK;` |
|       237 |  2306 | `}` |
|         - |  2307 | `/*` |
|         - |  2308 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2309 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2310 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2311 | ` */` |
|       204 |  2312 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2313 | `	ph7_gen_state *pGen,` |
|         - |  2314 | `	ph7_vm_func *pFunc,` |
|         - |  2315 | `	const char *zName,` |
|         - |  2316 | `	sxu32 nByte,` |
|         - |  2317 | `	SyString *aShadow,` |
|         - |  2318 | `	sxu32 nShadow)` |
|         3 |  2319 | `{` |
|         - |  2320 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2321 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2322 | `	sxu32 n, nEnv;` |
|         - |  2323 | `	char *zDup;` |
|       207 |  2324 | `	if( nByte == 0 ){` |
|       ! 0 |  2325 | `		return SXRET_OK;` |
|         - |  2326 | `	}` |
|       204 |  2327 | `	if( nByte == sizeof("this")-1` |
|       111 |  2328 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         3 |  2329 | `		return SXRET_OK;` |
|         - |  2330 | `	}` |
|       257 |  2331 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       192 |  2332 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       186 |  2333 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       143 |  2334 | `			return SXRET_OK;` |
|         - |  2335 | `		}` |
|        28 |  2336 | `	}` |
|        63 |  2337 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        63 |  2338 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|        91 |  2339 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2340 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2341 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2342 | `			return SXRET_OK;` |
|         - |  2343 | `		}` |
|        15 |  2344 | `	}` |
|        61 |  2345 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        61 |  2346 | `	if( zDup == 0 ){` |
|       ! 0 |  2347 | `		return SXERR_ABORT;` |
|         - |  2348 | `	}` |
|        61 |  2349 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        61 |  2350 | `	sEnv.iFlags = 0;` |
|        61 |  2351 | `	sEnv.nIdx = SXU32_HIGH;` |
|        61 |  2352 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        61 |  2353 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        61 |  2354 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        61 |  2355 | `	return SXRET_OK;` |
|       105 |  2356 | `}` |
|         - |  2357 | `/*` |
|         - |  2358 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|         - |  2359 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|         - |  2360 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|         - |  2361 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|         - |  2362 | ` */` |
|        56 |  2363 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|         - |  2364 | `	ph7_gen_state *pGen,` |
|         - |  2365 | `	ph7_vm_func *pFunc,` |
|         - |  2366 | `	const char *zIn,` |
|         - |  2367 | `	const char *zEnd,` |
|         - |  2368 | `	SyString *aShadow,` |
|         - |  2369 | `	sxu32 nShadow)` |
|         2 |  2370 | `{` |
|         - |  2371 | `	sxi32 rc;` |
|       370 |  2372 | `	while( zIn < zEnd ){` |
|       314 |  2373 | `		if( zIn[0] == '\\' ){` |
|         5 |  2374 | `			zIn++;` |
|         5 |  2375 | `			if( zIn < zEnd ){` |
|         5 |  2376 | `				zIn++;` |
|         2 |  2377 | `			}` |
|         5 |  2378 | `			continue;` |
|         - |  2379 | `		}` |
|       308 |  2380 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        26 |  2381 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|        24 |  2382 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|         - |  2383 | `			const char *zName;` |
|        26 |  2384 | `			zIn++; /* skip '$' */` |
|        26 |  2385 | `			zName = zIn;` |
|        82 |  2386 | `			while( zIn < zEnd ){` |
|        76 |  2387 | `				unsigned char c = (unsigned char)zIn[0];` |
|        76 |  2388 | `				if( c >= 0xc0 ){` |
|       ! 0 |  2389 | `					zIn++;` |
|       ! 0 |  2390 | `					while( zIn < zEnd` |
|       ! 0 |  2391 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  2392 | `						zIn++;` |
|       ! 0 |  2393 | `					}` |
|       ! 0 |  2394 | `					continue;` |
|         - |  2395 | `				}` |
|        76 |  2396 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        20 |  2397 | `					break;` |
|         - |  2398 | `				}` |
|        58 |  2399 | `				zIn++;` |
|         2 |  2400 | `			}` |
|        26 |  2401 | `			if( zIn > zName ){` |
|        38 |  2402 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|        24 |  2403 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|        26 |  2404 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2405 | `					return SXERR_ABORT;` |
|         - |  2406 | `				}` |
|        12 |  2407 | `			}` |
|        26 |  2408 | `			continue;` |
|         - |  2409 | `		}` |
|       286 |  2410 | `		zIn++;` |
|         2 |  2411 | `	}` |
|        58 |  2412 | `	return SXRET_OK;` |
|        30 |  2413 | `}` |
|         - |  2414 | `/*` |
|         - |  2415 | ` * Scan the body token range of an arrow function for free-variable` |
|         - |  2416 | ` * references and record them in pFunc's closure environment. Handles:` |
|         - |  2417 | ` *   - plain $<id> pairs` |
|         - |  2418 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|         - |  2419 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|         - |  2420 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|         - |  2421 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|         - |  2422 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|         - |  2423 | ` *     are never mistakenly captured.` |
|         - |  2424 | ` */` |
|       304 |  2425 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2426 | `	ph7_gen_state *pGen,` |
|         - |  2427 | `	ph7_vm_func *pFunc,` |
|         - |  2428 | `	SyToken *pStart,` |
|         - |  2429 | `	SyToken *pEnd,` |
|         - |  2430 | `	SyString *aShadow,` |
|         - |  2431 | `	sxu32 nShadow)` |
|         5 |  2432 | `{` |
|       309 |  2433 | `	SyToken *pScan = pStart;` |
|         - |  2434 | `	sxi32 rc;` |
|      1741 |  2435 | `	while( pScan < pEnd ){` |
|      1437 |  2436 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|        86 |  2437 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        28 |  2438 | `				pScan->sData.zString,` |
|        56 |  2439 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        28 |  2440 | `				aShadow,nShadow);` |
|        58 |  2441 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2442 | `				return SXERR_ABORT;` |
|         - |  2443 | `			}` |
|        58 |  2444 | `			pScan++;` |
|        58 |  2445 | `			continue;` |
|         - |  2446 | `		}` |
|      1381 |  2447 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        30 |  2448 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        30 |  2449 | `			SyToken *pFnKw = pScan;` |
|        28 |  2450 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2451 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         2 |  2452 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2453 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2454 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2455 | `			}` |
|        30 |  2456 | `			if( nKw == PH7_TKWRD_FN ){` |
|         - |  2457 | `				SyToken *pInnerSigStart;` |
|         - |  2458 | `				SyToken *pInnerSigEnd;` |
|         - |  2459 | `				SyToken *pInnerBodyEnd;` |
|         - |  2460 | `				SyString *aInnerShadow;` |
|         - |  2461 | `				sxu32 nInnerShadow;` |
|         - |  2462 | `				sxu32 nInnerParamMax;` |
|         - |  2463 | `				SyToken *p;` |
|         - |  2464 | `				int iNestInner;` |
|        19 |  2465 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|        19 |  2466 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2467 | `					pScan++;` |
|       ! 0 |  2468 | `				}` |
|        19 |  2469 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2470 | `					pScan++;` |
|       ! 0 |  2471 | `					continue;` |
|         - |  2472 | `				}` |
|        19 |  2473 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|        19 |  2474 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|         - |  2475 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|        19 |  2476 | `				if( pInnerSigEnd >= pEnd ){` |
|       ! 0 |  2477 | `					pScan = pEnd;` |
|       ! 0 |  2478 | `					continue;` |
|         - |  2479 | `				}` |
|         - |  2480 | `				/* Build an augmented shadow list: inherited + inner params */` |
|        19 |  2481 | `				nInnerParamMax = 0;` |
|        57 |  2482 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2483 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|        13 |  2484 | `						nInnerParamMax++;` |
|         6 |  2485 | `					}` |
|        20 |  2486 | `				}` |
|        19 |  2487 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|        18 |  2488 | `					&pGen->pVm->sAllocator,` |
|        18 |  2489 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|        19 |  2490 | `				if( aInnerShadow == 0 ){` |
|       ! 0 |  2491 | `					return SXERR_ABORT;` |
|         - |  2492 | `				}` |
|        19 |  2493 | `				nInnerShadow = 0;` |
|        25 |  2494 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|         7 |  2495 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|         4 |  2496 | `				}` |
|        57 |  2497 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2498 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|        27 |  2499 | `						continue;` |
|         - |  2500 | `					}` |
|        13 |  2501 | `					if( &p[1] >= pInnerSigEnd ){` |
|       ! 0 |  2502 | `						break;` |
|         - |  2503 | `					}` |
|        13 |  2504 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2505 | `						continue;` |
|         - |  2506 | `					}` |
|        13 |  2507 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|         7 |  2508 | `				}` |
|        19 |  2509 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|        19 |  2510 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|       ! 0 |  2511 | `					pScan++;` |
|       ! 0 |  2512 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|       ! 0 |  2513 | `						&& pScan->sData.nByte == 1` |
|       ! 0 |  2514 | `						&& pScan->sData.zString[0] == '?' ){` |
|       ! 0 |  2515 | `						pScan++;` |
|       ! 0 |  2516 | `					}` |
|       ! 0 |  2517 | `					if( pScan < pEnd` |
|       ! 0 |  2518 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  2519 | `						pScan++;` |
|       ! 0 |  2520 | `					}` |
|       ! 0 |  2521 | `				}` |
|        19 |  2522 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|        19 |  2523 | `					pScan++; /* past '=>' */` |
|         9 |  2524 | `				}` |
|        19 |  2525 | `				pInnerBodyEnd = pScan;` |
|        19 |  2526 | `				iNestInner = 0;` |
|       131 |  2527 | `				while( pInnerBodyEnd < pEnd ){` |
|       113 |  2528 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|         - |  2529 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|         - |  2530 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       ! 0 |  2531 | `						break;` |
|         - |  2532 | `					}` |
|       113 |  2533 | `					if( pInnerBodyEnd->nType &` |
|         - |  2534 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         3 |  2535 | `						iNestInner++;` |
|       112 |  2536 | `					}else if( pInnerBodyEnd->nType &` |
|         - |  2537 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         3 |  2538 | `						iNestInner--;` |
|         1 |  2539 | `					}` |
|       113 |  2540 | `					pInnerBodyEnd++;` |
|         1 |  2541 | `				}` |
|         - |  2542 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|         - |  2543 | `				 * the outer's body: a default value is evaluated at call time` |
|         - |  2544 | `				 * in the outer frame, so any free variable it references is` |
|         - |  2545 | `				 * an outer capture. We must NOT scan the parameter-name` |
|         - |  2546 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|         - |  2547 | `				 * or those names leak into the outer's closure environment.` |
|         - |  2548 | `				 *` |
|         - |  2549 | `				 * Walk the signature argument-by-argument, splitting on` |
|         - |  2550 | `				 * top-level commas, and for each argument scan only the token` |
|         - |  2551 | `				 * range after the '=' sign. */` |
|         - |  2552 | `				{` |
|        19 |  2553 | `					SyToken *pArgStart = pInnerSigStart;` |
|        31 |  2554 | `					while( pArgStart < pInnerSigEnd ){` |
|        13 |  2555 | `						SyToken *pArgEnd = pArgStart;` |
|        13 |  2556 | `						SyToken *pEq = 0;` |
|        13 |  2557 | `						int iNestArg = 0;` |
|        49 |  2558 | `						while( pArgEnd < pInnerSigEnd ){` |
|        38 |  2559 | `							if( iNestArg == 0` |
|        39 |  2560 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|         3 |  2561 | `								break;` |
|         - |  2562 | `							}` |
|        37 |  2563 | `							if( pArgEnd->nType &` |
|         - |  2564 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  2565 | `								iNestArg++;` |
|        37 |  2566 | `							}else if( pArgEnd->nType &` |
|         - |  2567 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  2568 | `								iNestArg--;` |
|       ! 0 |  2569 | `							}` |
|        36 |  2570 | `							if( pEq == 0 && iNestArg == 0` |
|        31 |  2571 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|         7 |  2572 | `								pEq = pArgEnd;` |
|         3 |  2573 | `							}` |
|        37 |  2574 | `							pArgEnd++;` |
|         1 |  2575 | `						}` |
|        13 |  2576 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|        10 |  2577 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         3 |  2578 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|         7 |  2579 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  2580 | `								return SXERR_ABORT;` |
|         - |  2581 | `							}` |
|         3 |  2582 | `						}` |
|        13 |  2583 | `						pArgStart = pArgEnd;` |
|        12 |  2584 | `						if( pArgStart < pInnerSigEnd` |
|         8 |  2585 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|         3 |  2586 | `							pArgStart++;` |
|         1 |  2587 | `						}` |
|         1 |  2588 | `					}` |
|         - |  2589 | `				}` |
|        28 |  2590 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         9 |  2591 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|        19 |  2592 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2593 | `					return SXERR_ABORT;` |
|         - |  2594 | `				}` |
|        19 |  2595 | `				pScan = pInnerBodyEnd;` |
|        19 |  2596 | `				continue;` |
|         - |  2597 | `			}` |
|         5 |  2598 | `		}` |
|      1363 |  2599 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      1183 |  2600 | `			pScan++;` |
|      1183 |  2601 | `			continue;` |
|         - |  2602 | `		}` |
|         - |  2603 | `		{` |
|         - |  2604 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       183 |  2605 | `			SyToken *pDollar = pScan;` |
|       270 |  2606 | `			while( &pDollar[1] < pEnd` |
|       183 |  2607 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2608 | `				pDollar++;` |
|       ! 0 |  2609 | `			}` |
|       183 |  2610 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2611 | `				break;` |
|         - |  2612 | `			}` |
|       183 |  2613 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2614 | `				pScan = pDollar + 1;` |
|       ! 0 |  2615 | `				continue;` |
|         - |  2616 | `			}` |
|       273 |  2617 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       180 |  2618 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|        90 |  2619 | `				aShadow,nShadow);` |
|       183 |  2620 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2621 | `				return SXERR_ABORT;` |
|         - |  2622 | `			}` |
|       183 |  2623 | `			pScan = pDollar + 2;` |
|         - |  2624 | `		}` |
|         3 |  2625 | `	}` |
|       309 |  2626 | `	return SXRET_OK;` |
|       157 |  2627 | `}` |
|         - |  2628 | `/*` |
|         - |  2629 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2630 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2631 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2632 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2633 | ` * $this is also made available.` |
|         - |  2634 | ` */` |
|       286 |  2635 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2636 | `{` |
|         - |  2637 | `	ph7_vm_func *pFunc;` |
|         - |  2638 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2639 | `	GenBlock *pBlock;` |
|         - |  2640 | `	SySet *pInstrContainer;` |
|         - |  2641 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|         - |  2642 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|         - |  2643 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|         - |  2644 | `	SyToken *pSavedEnd;` |
|         - |  2645 | `	ph7_vm_func_arg *aArgs;` |
|         - |  2646 | `	char zName[512];` |
|         - |  2647 | `	static int iCnt = 1;` |
|         - |  2648 | `	char *zDup;` |
|         - |  2649 | `	SyToken *pTokKw;` |
|         - |  2650 | `	sxu32 nLen;` |
|         - |  2651 | `	sxu32 nLine;` |
|       291 |  2652 | `	sxi32 iFlags = 0;` |
|       291 |  2653 | `	int bStatic = 0;` |
|         - |  2654 | `	sxi32 rc;` |
|         - |  2655 | `	sxu32 n;` |
|       143 |  2656 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2657 |  |
|       291 |  2658 | `	nLine = pGen->pIn->nLine;` |
|         - |  2659 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       291 |  2660 | `	pTokKw = pGen->pIn;` |
|         - |  2661 | `	/* Optional 'static' prefix */` |
|       286 |  2662 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       291 |  2663 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  2664 | `		bStatic = 1;` |
|         7 |  2665 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         7 |  2666 | `		pGen->pIn++;` |
|         3 |  2667 | `	}` |
|         - |  2668 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       286 |  2669 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       291 |  2670 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2671 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2672 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2673 | `		return SXERR_SYNTAX;` |
|         - |  2674 | `	}` |
|       291 |  2675 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2676 | `	/* Optional '&' — return by reference */` |
|       291 |  2677 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2678 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2679 | `		pGen->pIn++;` |
|       ! 0 |  2680 | `	}` |
|         - |  2681 | `	/* Expect '(' */` |
|       291 |  2682 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  2683 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2684 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2685 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|         2 |  2686 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2687 | `		}else{` |
|       ! 0 |  2688 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2689 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|         - |  2690 | `		}` |
|         3 |  2691 | `		return SXERR_SYNTAX;` |
|         - |  2692 | `	}` |
|       289 |  2693 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2694 | `	/* Delimit the parameter list */` |
|       289 |  2695 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       289 |  2696 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2697 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2698 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2699 | `		return SXERR_SYNTAX;` |
|         - |  2700 | `	}` |
|         - |  2701 | `	/* Allocate the function state */` |
|       287 |  2702 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       287 |  2703 | `	if( pFunc == 0 ){` |
|       ! 0 |  2704 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2705 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2706 | `		return SXERR_ABORT;` |
|         - |  2707 | `	}` |
|         - |  2708 | `	/* Generate a unique lambda name */` |
|       287 |  2709 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       287 |  2710 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2711 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2712 | `	}` |
|       287 |  2713 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       287 |  2714 | `	if( zDup == 0 ){` |
|       ! 0 |  2715 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2716 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2717 | `		return SXERR_ABORT;` |
|         - |  2718 | `	}` |
|       287 |  2719 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2720 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       287 |  2721 | `	pFunc->nLine = nLine;` |
|         - |  2722 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       287 |  2723 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2724 | `		return SXERR_ABORT;` |
|         - |  2725 | `	}` |
|         - |  2726 | `	/* Collect function arguments */` |
|       287 |  2727 | `	if( pGen->pIn < pSigEnd ){` |
|       116 |  2728 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       116 |  2729 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2730 | `			return SXERR_ABORT;` |
|         - |  2731 | `		}` |
|        56 |  2732 | `	}` |
|         - |  2733 | `	/* Point past ')' and parse optional return type */` |
|       287 |  2734 | `	pGen->pIn = &pSigEnd[1];` |
|       287 |  2735 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       287 |  2736 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2737 | `		return SXERR_ABORT;` |
|       287 |  2738 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2739 | `		return SXERR_SYNTAX;` |
|         - |  2740 | `	}` |
|         - |  2741 | `	/* Expect '=>' */` |
|       287 |  2742 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  2743 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2744 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2745 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|         2 |  2746 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2747 | `		}else{` |
|       ! 0 |  2748 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2749 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|         - |  2750 | `		}` |
|         3 |  2751 | `		return SXERR_SYNTAX;` |
|         - |  2752 | `	}` |
|       285 |  2753 | `	pGen->pIn++; /* Jump '=>' */` |
|       285 |  2754 | `	pBodyStart = pGen->pIn;` |
|       285 |  2755 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2756 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2757 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2758 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2759 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       285 |  2760 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2761 | `	{` |
|       285 |  2762 | `		SyString *aShadow = 0;` |
|       285 |  2763 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       285 |  2764 | `		if( nShadow > 0 ){` |
|       113 |  2765 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       110 |  2766 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       113 |  2767 | `			if( aShadow == 0 ){` |
|       ! 0 |  2768 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2769 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2770 | `				return SXERR_ABORT;` |
|         - |  2771 | `			}` |
|       257 |  2772 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       147 |  2773 | `				aShadow[n] = aArgs[n].sName;` |
|        75 |  2774 | `			}` |
|        55 |  2775 | `		}` |
|       425 |  2776 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       140 |  2777 | `			aShadow,nShadow);` |
|       285 |  2778 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2779 | `			return SXERR_ABORT;` |
|         - |  2780 | `		}` |
|         - |  2781 | `	}` |
|         - |  2782 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2783 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2784 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2785 | `	 * $this. */` |
|       285 |  2786 | `	if( !bStatic ){` |
|         - |  2787 | `		char *zThisDup;` |
|       279 |  2788 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       279 |  2789 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2790 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2791 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2792 | `			return SXERR_ABORT;` |
|         - |  2793 | `		}` |
|       279 |  2794 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       279 |  2795 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       279 |  2796 | `		sEnv.nIdx = SXU32_HIGH;` |
|       279 |  2797 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       279 |  2798 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       279 |  2799 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       137 |  2800 | `	}` |
|         - |  2801 | `	/* Arrow functions are always closures */` |
|       285 |  2802 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2803 | `	/* Compile the body expression as an implicit return */` |
|       425 |  2804 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       140 |  2805 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       285 |  2806 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2807 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2808 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2809 | `		return SXERR_ABORT;` |
|         - |  2810 | `	}` |
|       285 |  2811 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       285 |  2812 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       285 |  2813 | `	pSavedEnd = pGen->pEnd;` |
|       285 |  2814 | `	pGen->pIn = pBodyStart;` |
|       285 |  2815 | `	pGen->pEnd = pBodyEnd;` |
|       285 |  2816 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       285 |  2817 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2818 | `		return SXERR_ABORT;` |
|         - |  2819 | `	}` |
|         - |  2820 | `	/* The cursor stopped just past the body expression */` |
|       285 |  2821 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2822 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2823 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2824 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2825 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       285 |  2826 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       285 |  2827 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       285 |  2828 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       285 |  2829 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       285 |  2830 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2831 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       285 |  2832 | `	pGen->pIn = pBodyEnd;` |
|       285 |  2833 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2834 | `	/* Emit the load-closure instruction */` |
|       285 |  2835 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       285 |  2836 | `	return SXRET_OK;` |
|       148 |  2837 | `}` |
|         - |  2838 | `/*` |
|         - |  2839 | ` * Compile a single arm's expression range into a freshly-allocated` |
|         - |  2840 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|         - |  2841 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|         - |  2842 | ` * expression's value.` |
|         - |  2843 | ` */` |
|       354 |  2844 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|         - |  2845 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|         3 |  2846 | `{` |
|         - |  2847 | `	SySet *pInstrContainer;` |
|         - |  2848 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  2849 | `	GenBlock *pArmBlock;` |
|         - |  2850 | `	sxi32 rc;` |
|       357 |  2851 | `	pTmpIn  = pGen->pIn;` |
|       357 |  2852 | `	pTmpEnd = pGen->pEnd;` |
|       357 |  2853 | `	pGen->pIn  = pStart;` |
|       357 |  2854 | `	pGen->pEnd = pStop;` |
|       357 |  2855 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       357 |  2856 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|         - |  2857 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|         - |  2858 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|         - |  2859 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|         - |  2860 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|         - |  2861 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|       534 |  2862 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       177 |  2863 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|       357 |  2864 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2865 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  2866 | `		pGen->pIn  = pTmpIn;` |
|       ! 0 |  2867 | `		pGen->pEnd = pTmpEnd;` |
|       ! 0 |  2868 | `		return SXERR_ABORT;` |
|         - |  2869 | `	}` |
|       357 |  2870 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       357 |  2871 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       357 |  2872 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       357 |  2873 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       357 |  2874 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       357 |  2875 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       357 |  2876 | `	pGen->pIn  = pTmpIn;` |
|       357 |  2877 | `	pGen->pEnd = pTmpEnd;` |
|       357 |  2878 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2879 | `		return SXERR_ABORT;` |
|         - |  2880 | `	}` |
|       357 |  2881 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 |  2882 | `		return SXERR_EMPTY;` |
|         - |  2883 | `	}` |
|       357 |  2884 | `	return SXRET_OK;` |
|       180 |  2885 | `}` |
|         - |  2886 | `/*` |
|         - |  2887 | ` * Compile a PHP 8.0 match expression:` |
|         - |  2888 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|         - |  2889 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|         - |  2890 | ` * Strict comparison (===) is used between the subject and each condition.` |
|         - |  2891 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|         - |  2892 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|         - |  2893 | ` */` |
|         - |  2894 | `/*` |
|         - |  2895 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|         - |  2896 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|         - |  2897 | ` * caller can bail out of the current expression.` |
|         - |  2898 | ` */` |
|         2 |  2899 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|         1 |  2900 | `{` |
|         - |  2901 | `	va_list ap;` |
|         - |  2902 | `	sxi32 rc;` |
|         - |  2903 | `	SyBlob sMsg;` |
|         3 |  2904 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|         3 |  2905 | `	va_start(ap,zFmt);` |
|         3 |  2906 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|         3 |  2907 | `	va_end(ap);` |
|         3 |  2908 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|         3 |  2909 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|         3 |  2910 | `	SyBlobRelease(&sMsg);` |
|         3 |  2911 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2912 | `		return SXERR_ABORT;` |
|         - |  2913 | `	}` |
|         3 |  2914 | `	return SXERR_SYNTAX;` |
|         2 |  2915 | `}` |
|         - |  2916 | `/*` |
|         - |  2917 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|         - |  2918 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|         - |  2919 | ` * Returns the stop token pointer (or pEnd if none found).` |
|         - |  2920 | ` */` |
|       356 |  2921 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|         4 |  2922 | `{` |
|       360 |  2923 | `	SyToken *pCur = pStart;` |
|       360 |  2924 | `	int iNest = 0;` |
|       838 |  2925 | `	while( pCur < pEnd ){` |
|       802 |  2926 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        13 |  2927 | `			iNest++;` |
|       796 |  2928 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        13 |  2929 | `			iNest--;` |
|       784 |  2930 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|       323 |  2931 | `			return pCur;` |
|         - |  2932 | `		}` |
|       482 |  2933 | `		pCur++;` |
|         4 |  2934 | `	}` |
|        39 |  2935 | `	return pEnd;` |
|       182 |  2936 | `}` |
|        72 |  2937 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2938 | `{` |
|         - |  2939 | `	ph7_match *pMatch;` |
|         - |  2940 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|        77 |  2941 | `	int bHasDefault = 0;` |
|         - |  2942 | `	sxu32 nLine;` |
|         - |  2943 | `	sxi32 rc;` |
|        36 |  2944 | `	SXUNUSED(iCompileFlag);` |
|        77 |  2945 | `	nLine = pGen->pIn->nLine;` |
|        77 |  2946 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|         - |  2947 | `	/* Expect '(' */` |
|        77 |  2948 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2949 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2950 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|       ! 0 |  2951 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|         - |  2952 | `	}` |
|        77 |  2953 | `	pGen->pIn++; /* Jump '(' */` |
|        77 |  2954 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|        77 |  2955 | `	if( pSubjEnd >= pGen->pEnd ){` |
|       ! 0 |  2956 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2957 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         - |  2958 | `	}` |
|        77 |  2959 | `	if( pGen->pIn >= pSubjEnd ){` |
|       ! 0 |  2960 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2961 | `			"syntax error, unexpected \")\", expecting match subject");` |
|         - |  2962 | `	}` |
|         - |  2963 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|        77 |  2964 | `	pSavedEnd = pGen->pEnd;` |
|        77 |  2965 | `	pGen->pEnd = pSubjEnd;` |
|        77 |  2966 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        77 |  2967 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2968 | `		return SXERR_ABORT;` |
|         - |  2969 | `	}` |
|        77 |  2970 | `	pGen->pEnd = pSavedEnd;` |
|        77 |  2971 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|         - |  2972 | `	/* Expect '{' */` |
|        77 |  2973 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 |  2974 | `		return GenStateMatchError(pGen,` |
|       ! 0 |  2975 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  2976 | `			"syntax error, expecting \"{\" after match subject");` |
|         - |  2977 | `	}` |
|        77 |  2978 | `	pGen->pIn++; /* Jump '{' */` |
|        77 |  2979 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|        77 |  2980 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  2981 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2982 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|         - |  2983 | `	}` |
|         - |  2984 | `	/* Allocate ph7_match container */` |
|        77 |  2985 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|        77 |  2986 | `	if( pMatch == 0 ){` |
|       ! 0 |  2987 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2988 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2989 | `		return SXERR_ABORT;` |
|         - |  2990 | `	}` |
|        77 |  2991 | `	SyZero(pMatch,sizeof(ph7_match));` |
|        77 |  2992 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|         - |  2993 | `	/* Iterate arms */` |
|       259 |  2994 | `	while( pGen->pIn < pBodyEnd ){` |
|         - |  2995 | `		ph7_match_arm sArm;` |
|         - |  2996 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|       190 |  2997 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|       190 |  2998 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|       190 |  2999 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|       190 |  3000 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3001 | `		/* 'default' arm? */` |
|       186 |  3002 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       107 |  3003 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|        22 |  3004 | `			if( bHasDefault ){` |
|         3 |  3005 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|         - |  3006 | `					"Match expressions may only contain one default arm");` |
|         4 |  3007 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  3008 | `			}` |
|        20 |  3009 | `			sArm.bDefault = 1;` |
|        20 |  3010 | `			bHasDefault = 1;` |
|        20 |  3011 | `			pGen->pIn++;` |
|        20 |  3012 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       ! 0 |  3013 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3014 | `					"syntax error, expecting \"=>\" after 'default'");` |
|         - |  3015 | `			}` |
|        20 |  3016 | `			pGen->pIn++; /* Jump '=>' */` |
|        11 |  3017 | `		}else{` |
|         - |  3018 | `			/* Condition list: cond (',' cond)* '=>' */` |
|       170 |  3019 | `			pCondStart = pGen->pIn;` |
|       170 |  3020 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|         - |  3021 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       178 |  3022 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|         - |  3023 | `				SySet sCondBc;` |
|         9 |  3024 | `				if( pCondStart >= pArrow ){` |
|       ! 0 |  3025 | `					return GenStateMatchError(pGen,nArmLine,` |
|         - |  3026 | `						"syntax error, empty match condition expression");` |
|         - |  3027 | `				}` |
|         9 |  3028 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         9 |  3029 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|         9 |  3030 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3031 | `					return SXERR_ABORT;` |
|         - |  3032 | `				}` |
|         9 |  3033 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         9 |  3034 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|         9 |  3035 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|         - |  3036 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|         1 |  3037 | `			}` |
|       170 |  3038 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  3039 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3040 | `					"syntax error, expecting \"=>\" in match arm");` |
|         - |  3041 | `			}` |
|       167 |  3042 | `			if( pCondStart >= pArrow ){` |
|       ! 0 |  3043 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3044 | `					"syntax error, empty match condition expression");` |
|         - |  3045 | `			}` |
|         - |  3046 | `			{` |
|         - |  3047 | `				SySet sCondBc;` |
|       167 |  3048 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       167 |  3049 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       167 |  3050 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3051 | `					return SXERR_ABORT;` |
|         - |  3052 | `				}` |
|       167 |  3053 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         - |  3054 | `			}` |
|       167 |  3055 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|         - |  3056 | `		}` |
|         - |  3057 | `		/* Compile result expression: up to top-level ',' or body end */` |
|       185 |  3058 | `		pResStart = pGen->pIn;` |
|       185 |  3059 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|       185 |  3060 | `		if( pResStart >= pResEnd ){` |
|       ! 0 |  3061 | `			return GenStateMatchError(pGen,nArmLine,` |
|         - |  3062 | `				"syntax error, expected expression after \"=>\"");` |
|         - |  3063 | `		}` |
|       185 |  3064 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|       185 |  3065 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3066 | `			return SXERR_ABORT;` |
|         - |  3067 | `		}` |
|       185 |  3068 | `		pGen->pIn = pResEnd;` |
|       185 |  3069 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       151 |  3070 | `			pGen->pIn++; /* Skip trailing ',' */` |
|        74 |  3071 | `		}` |
|       185 |  3072 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|         3 |  3073 | `	}` |
|        71 |  3074 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|        71 |  3075 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|        71 |  3076 | `	return SXRET_OK;` |
|        41 |  3077 | `}` |
|         - |  3078 | `/*` |
|         - |  3079 | ` * Compile a backtick quoted string.` |
|         - |  3080 | ` */` |
|         4 |  3081 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  3082 | `{` |
|         - |  3083 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|         - |  3084 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|         - |  3085 | `	 */` |
|         8 |  3086 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|         - |  3087 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|         2 |  3088 | `		ph7_lib_version()` |
|         - |  3089 | `		);` |
|         - |  3090 | `	/* Load NULL */` |
|         6 |  3091 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         2 |  3092 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  3093 | `	/* Node successfully compiled */` |
|         6 |  3094 | `	return SXRET_OK;` |
|         2 |  3095 | `}` |
|         - |  3096 | `/*` |
|         - |  3097 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|         - |  3098 | ` * construct.` |
|         - |  3099 | ` */` |
|        82 |  3100 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3101 | `{` |
|         - |  3102 | `	SyString *pName;` |
|         - |  3103 | `	sxu32 nKeyID;` |
|         - |  3104 | `	sxi32 rc;` |
|         - |  3105 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        87 |  3106 | `	pName = &pGen->pIn->sData;` |
|        87 |  3107 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        87 |  3108 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        87 |  3109 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|         9 |  3110 | `		SyToken *pTmp,*pNext = 0;` |
|         - |  3111 | `		/* Compile arguments one after one */` |
|         9 |  3112 | `		pTmp = pGen->pEnd;` |
|         - |  3113 | `		/* Symisc eXtension to the PHP programming language:` |
|         - |  3114 | `		 * 'echo' can be used in the context of a function which` |
|         - |  3115 | `		 *  mean that the following expression is valid:` |
|         - |  3116 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|         - |  3117 | `		 */` |
|         9 |  3118 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        17 |  3119 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|         9 |  3120 | `			if( pGen->pIn < pNext ){` |
|         9 |  3121 | `				pGen->pEnd = pNext;` |
|         9 |  3122 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|         9 |  3123 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3124 | `					return SXERR_ABORT;` |
|         - |  3125 | `				}` |
|         9 |  3126 | `				if( rc != SXERR_EMPTY ){` |
|         - |  3127 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|         - |  3128 | `					 * without the overhead of a function call.` |
|         - |  3129 | `					 * This is a very powerful optimization that improve` |
|         - |  3130 | `					 * performance greatly.` |
|         - |  3131 | `					 */` |
|         9 |  3132 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|         4 |  3133 | `				}` |
|         4 |  3134 | `			}` |
|         - |  3135 | `			/* Jump trailing commas */` |
|         9 |  3136 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|       ! 0 |  3137 | `				pNext++;` |
|       ! 0 |  3138 | `			}` |
|         9 |  3139 | `			pGen->pIn = pNext;` |
|         1 |  3140 | `		}` |
|         - |  3141 | `		/* Restore token stream */` |
|         9 |  3142 | `		pGen->pEnd = pTmp;` |
|         5 |  3143 | `	}else{` |
|        79 |  3144 | `		sxi32 nArg = 0;` |
|        79 |  3145 | `		sxu32 nIdx = 0;` |
|        79 |  3146 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        79 |  3147 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3148 | `			return SXERR_ABORT;` |
|        79 |  3149 | `		}else if(rc != SXERR_EMPTY ){` |
|        79 |  3150 | `			nArg = 1;` |
|        37 |  3151 | `		}` |
|        79 |  3152 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3153 | `			ph7_value *pObj;` |
|         - |  3154 | `			/* Emit the call instruction */` |
|        31 |  3155 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        31 |  3156 | `			if( pObj == 0 ){` |
|       ! 0 |  3157 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3158 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3159 | `				return SXERR_ABORT;` |
|         - |  3160 | `			}` |
|        31 |  3161 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3162 | `			/* Install in the literal table */` |
|        31 |  3163 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        13 |  3164 | `		}` |
|         - |  3165 | `		/* Emit the call instruction */` |
|        79 |  3166 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        79 |  3167 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3168 | `	}` |
|         - |  3169 | `	/* Node successfully compiled */` |
|        87 |  3170 | `	return SXRET_OK;` |
|        46 |  3171 | `}` |
|         - |  3172 | `/*` |
|         - |  3173 | ` * Compile a node holding a variable declaration.` |
|         - |  3174 | ` * According to the PHP language reference` |
|         - |  3175 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|         - |  3176 | ` *  The variable name is case-sensitive.` |
|         - |  3177 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|         - |  3178 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3179 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|         - |  3180 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|         - |  3181 | ` *  Note: $this is a special variable that can't be assigned.` |
|         - |  3182 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|         - |  3183 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|         - |  3184 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|         - |  3185 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|         - |  3186 | ` *  the chapter on Expressions.` |
|         - |  3187 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|         - |  3188 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|         - |  3189 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|         - |  3190 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|         - |  3191 | ` *  is being assigned (the source variable).` |
|         - |  3192 | ` */` |
|  16501660 |  3193 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3194 | `{` |
|  16501665 |  3195 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3196 | `	sxi32 iVv;` |
|         - |  3197 | `	sxi32 iP1;` |
|         - |  3198 | `	void *p3;` |
|         - |  3199 | `	sxi32 rc;` |
|  16501665 |  3200 | `	iVv = -1; /* Variable variable counter */` |
|  33003337 |  3201 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  16501677 |  3202 | `		pGen->pIn++;` |
|  16501677 |  3203 | `		iVv++;` |
|         5 |  3204 | `	}` |
|  16501665 |  3205 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3206 | `		/* Invalid variable name */` |
|       ! 0 |  3207 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|       ! 0 |  3208 | `		if( rc == SXERR_ABORT ){` |
|         - |  3209 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3210 | `			return SXERR_ABORT;` |
|         - |  3211 | `		}` |
|       ! 0 |  3212 | `		return SXRET_OK;` |
|         - |  3213 | `	}` |
|  16501665 |  3214 | `	p3  = 0;` |
|  16501665 |  3215 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|         - |  3216 | `		/* Dynamic variable creation */` |
|        21 |  3217 | `		pGen->pIn++;  /* Jump the open curly */` |
|        21 |  3218 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|        21 |  3219 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3220 | `			/* Empty expression */` |
|         3 |  3221 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|         3 |  3222 | `			return SXRET_OK;` |
|         - |  3223 | `		}` |
|         - |  3224 | `		/* Compile the expression holding the variable name */` |
|        18 |  3225 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        18 |  3226 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3227 | `			return SXERR_ABORT;` |
|        18 |  3228 | `		}else if( rc == SXERR_EMPTY ){` |
|         3 |  3229 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|         3 |  3230 | `			return SXRET_OK;` |
|         - |  3231 | `		}` |
|         8 |  3232 | `	}else{` |
|         - |  3233 | `		SyHashEntry *pEntry;` |
|         - |  3234 | `		SyString *pName;` |
|  16501647 |  3235 | `		char *zName = 0;` |
|         - |  3236 | `		/* Extract variable name */` |
|  16501647 |  3237 | `		pName = &pGen->pIn->sData;` |
|         - |  3238 | `		/* Advance the stream cursor */` |
|  16501647 |  3239 | `		pGen->pIn++;` |
|  16501647 |  3240 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  16501647 |  3241 | `		if( pEntry == 0 ){` |
|         - |  3242 | `			/* Duplicate name */` |
|    952871 |  3243 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    952871 |  3244 | `			if( zName == 0 ){` |
|       ! 0 |  3245 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3246 | `				return SXERR_ABORT;` |
|         - |  3247 | `			}` |
|         - |  3248 | `			/* Install in the hashtable */` |
|    952871 |  3249 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    476438 |  3250 | `		}else{` |
|         - |  3251 | `			/* Name already available */` |
|  15548781 |  3252 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3253 | `		}` |
|  16501647 |  3254 | `		p3 = (void *)zName;` |
|         - |  3255 | `	}` |
|  16501661 |  3256 | `	iP1 = 0;` |
|  16501661 |  3257 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   4913431 |  3258 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3259 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   4909467 |  3260 | `			iP1 = 1;` |
|   2454731 |  3261 | `		}` |
|   2456713 |  3262 | `	}` |
|         - |  3263 | `	/* Emit the load instruction */` |
|  16501661 |  3264 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  16501673 |  3265 | `	while( iVv > 0 ){` |
|        13 |  3266 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3267 | `		iVv--;` |
|         1 |  3268 | `	}` |
|         - |  3269 | `	/* Node successfully compiled */` |
|  16501661 |  3270 | `	return SXRET_OK;` |
|   8250835 |  3271 | `}` |
|         - |  3272 | `/*` |
|         - |  3273 | ` * Load a literal.` |
|         - |  3274 | ` */` |
|  11180450 |  3275 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3276 | `{` |
|  11180455 |  3277 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3278 | `	ph7_value *pObj;` |
|         - |  3279 | `	SyString *pStr;` |
|         - |  3280 | `	sxu32 nIdx;` |
|         - |  3281 | `	/* Extract token value */` |
|  11180455 |  3282 | `	pStr = &pToken->sData;` |
|         - |  3283 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  11180455 |  3284 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2159105 |  3285 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3286 | `			/* NULL constant are always indexed at 0 */` |
|    899257 |  3287 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    899257 |  3288 | `			return SXRET_OK;` |
|   1259853 |  3289 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3290 | `			/* TRUE constant are always indexed at 1 */` |
|    284801 |  3291 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    284801 |  3292 | `			return SXRET_OK;` |
|         5 |  3293 | `		}` |
|  10442686 |  3294 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1867610 |  3295 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3296 | `			/* FALSE constant are always indexed at 2 */` |
|    638929 |  3297 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    638929 |  3298 | `			return SXRET_OK;` |
|   8783239 |  3299 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    801616 |  3300 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3301 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     11831 |  3302 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     11831 |  3303 | `			if( pObj == 0 ){` |
|       ! 0 |  3304 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3305 | `				return SXERR_ABORT;` |
|         - |  3306 | `			}` |
|     11831 |  3307 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3308 | `			/* Emit the load constant instruction */` |
|     11831 |  3309 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     11831 |  3310 | `			return SXRET_OK;` |
|   8465575 |  3311 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    189940 |  3312 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3313 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         8 |  3314 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         8 |  3315 | `			if( pObj == 0 ){` |
|       ! 0 |  3316 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3317 | `				return SXERR_ABORT;` |
|         - |  3318 | `			}` |
|         8 |  3319 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3320 | `				SyString sNs;` |
|         8 |  3321 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  3322 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         5 |  3323 | `			}else{` |
|       ! 0 |  3324 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3325 | `			}` |
|         8 |  3326 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         8 |  3327 | `			return SXRET_OK;` |
|   8469710 |  3328 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    390939 |  3329 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   8563289 |  3330 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    385404 |  3331 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3332 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3333 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3334 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3335 | `				/* Point to the upper block */` |
|        11 |  3336 | `				pBlock = pBlock->pParent;` |
|         1 |  3337 | `			}` |
|        11 |  3338 | `			if( pBlock == 0 ){` |
|         - |  3339 | `				/* Called in the global scope,load NULL */` |
|         5 |  3340 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3341 | `			}else{` |
|         - |  3342 | `				/* Extract the target function/method */` |
|         7 |  3343 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3344 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|         - |  3345 | `					/* Not a class method,Load null */` |
|         3 |  3346 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         2 |  3347 | `				}else{` |
|         5 |  3348 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         5 |  3349 | `					if( pObj == 0 ){` |
|       ! 0 |  3350 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3351 | `						return SXERR_ABORT;` |
|         - |  3352 | `					}` |
|         5 |  3353 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3354 | `					/* Emit the load constant instruction */` |
|         5 |  3355 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3356 | `				}` |
|         - |  3357 | `			}` |
|        11 |  3358 | `			return SXRET_OK;` |
|         - |  3359 | `	}` |
|         - |  3360 | `	/* Query literal table */` |
|   9345641 |  3361 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3362 | `		ph7_value *pLitObj;` |
|         - |  3363 | `		/* Unknown literal,install it in the literal table */` |
|   1791181 |  3364 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1791181 |  3365 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3366 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3367 | `			return SXERR_ABORT;` |
|         - |  3368 | `		}` |
|   1791181 |  3369 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1791181 |  3370 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    895588 |  3371 | `	}` |
|         - |  3372 | `	/* Emit the load constant instruction */` |
|   9345641 |  3373 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9345641 |  3374 | `	return SXRET_OK;` |
|   5590230 |  3375 | `}` |
|         - |  3376 | `/*` |
|         - |  3377 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3378 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3379 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3380 | ` * Otherwise, load the simple literal directly.` |
|         - |  3381 | ` */` |
|  11184438 |  3382 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3383 | `{` |
|         - |  3384 | `	sxi32 rc;` |
|  11184443 |  3385 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3386 | `		return SXRET_OK;` |
|         - |  3387 | `	}` |
|         - |  3388 | `	/* Check if this is a multi-token namespace path */` |
|  11184443 |  3389 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3390 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3993 |  3391 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3993 |  3392 | `		int isAbsolute = 0;` |
|      3993 |  3393 | `		SyBlobReset(pWorker);` |
|         - |  3394 | `		/* Check for leading backslash (absolute path) */` |
|      3993 |  3395 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3991 |  3396 | `			isAbsolute = 1;` |
|      3991 |  3397 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1993 |  3398 | `		}` |
|         - |  3399 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3993 |  3400 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3401 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3402 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3403 | `		}` |
|         - |  3404 | `		/* Collect all path components */` |
|      4101 |  3405 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4101 |  3406 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        59 |  3407 | `				SyBlobAppend(pWorker,"\\",1);` |
|        32 |  3408 | `			}else{` |
|      4047 |  3409 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3410 | `			}` |
|      4101 |  3411 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3993 |  3412 | `				pGen->pIn++;` |
|      3993 |  3413 | `				break;` |
|         - |  3414 | `			}` |
|       113 |  3415 | `			pGen->pIn++;` |
|         5 |  3416 | `		}` |
|      3993 |  3417 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3418 | `			ph7_value *pObj;` |
|         - |  3419 | `			SyString sPath;` |
|         - |  3420 | `			sxu32 nIdx;` |
|      3993 |  3421 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3422 | `			/* Install in the literal table */` |
|      3993 |  3423 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3963 |  3424 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3963 |  3425 | `				if( pObj == 0 ){` |
|       ! 0 |  3426 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3427 | `					return SXERR_ABORT;` |
|         - |  3428 | `				}` |
|      3963 |  3429 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3963 |  3430 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1979 |  3431 | `			}` |
|         - |  3432 | `			/* Emit the load constant instruction.` |
|         - |  3433 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3434 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5987 |  3435 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1994 |  3436 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1994 |  3437 | `				nIdx,0,0);` |
|      3993 |  3438 | `			return SXRET_OK;` |
|         - |  3439 | `		}` |
|       ! 0 |  3440 | `	}` |
|         - |  3441 | `	/* Single-token literal: load directly */` |
|  11180455 |  3442 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11180455 |  3443 | `	return rc;` |
|   5592224 |  3444 | `}` |
|         - |  3445 | `/*` |
|         - |  3446 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3447 | ` */` |
|         - |  3448 | `/*` |
|         - |  3449 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3450 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3451 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3452 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3453 | ` */` |
|       ! 0 |  3454 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3455 | `{` |
|       ! 0 |  3456 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3457 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3458 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3459 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3460 | `}` |
|  11184438 |  3461 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3462 | `{` |
|         - |  3463 | `	sxi32 rc;` |
|  11184443 |  3464 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11184443 |  3465 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3466 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3467 | `		return rc;` |
|         - |  3468 | `	}` |
|         - |  3469 | `	/* Node successfully compiled */` |
|  11184443 |  3470 | `	return SXRET_OK;` |
|   5592224 |  3471 | `}` |
|         - |  3472 | `/*` |
|         - |  3473 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3474 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3475 | ` */` |
|         8 |  3476 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3477 | `{` |
|         - |  3478 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3479 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3480 | `		pGen->pIn++;` |
|         1 |  3481 | `	}` |
|         9 |  3482 | `	return SXRET_OK;` |
|         1 |  3483 | `}` |
|         - |  3484 | `/*` |
|         - |  3485 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3486 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3487 | ` */` |
|    299660 |  3488 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3489 | `{` |
|    299665 |  3490 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3989 |  3491 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3492 | `			return TRUE;` |
|      3987 |  3493 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3494 | `			return TRUE;` |
|         5 |  3495 | `		}` |
|    297670 |  3496 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7905 |  3497 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3498 | `			return TRUE;` |
|         - |  3499 | `		}` |
|      3949 |  3500 | `	}` |
|         - |  3501 | `	/* Not a reserved constant */` |
|    299657 |  3502 | `	return FALSE;` |
|    149835 |  3503 | `}` |
|         - |  3504 | `/*` |
|         - |  3505 | ` * Compile the 'const' statement.` |
|         - |  3506 | ` * According to the PHP language reference` |
|         - |  3507 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3508 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3509 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3510 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3511 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3512 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3513 | ` *  Syntax` |
|         - |  3514 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3515 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3516 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3517 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3518 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3519 | ` *  to get a list of all defined constants.` |
|         - |  3520 | ` *` |
|         - |  3521 | ` * Symisc eXtension.` |
|         - |  3522 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3523 | ` *  would allow only simple scalar value.` |
|         - |  3524 | ` *  Example` |
|         - |  3525 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3526 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3527 | ` */` |
|        48 |  3528 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3529 | `{` |
|         - |  3530 | `	SySet *pConsCode,*pInstrContainer;` |
|        53 |  3531 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3532 | `	SyString *pName;` |
|         - |  3533 | `	sxi32 rc;` |
|        53 |  3534 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        53 |  3535 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3536 | `		/* Invalid constant name */` |
|         8 |  3537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|         8 |  3538 | `		if( rc == SXERR_ABORT ){` |
|         - |  3539 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3540 | `			return SXERR_ABORT;` |
|         - |  3541 | `		}` |
|         8 |  3542 | `		goto Synchronize;` |
|         - |  3543 | `	}` |
|         - |  3544 | `	/* Peek constant name */` |
|        47 |  3545 | `	pName = &pGen->pIn->sData;` |
|         - |  3546 | `	/* Make sure the constant name isn't reserved */` |
|        47 |  3547 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3548 | `		/* Reserved constant */` |
|        10 |  3549 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|        10 |  3550 | `		if( rc == SXERR_ABORT ){` |
|         - |  3551 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3552 | `			return SXERR_ABORT;` |
|         - |  3553 | `		}` |
|        10 |  3554 | `		goto Synchronize;` |
|         - |  3555 | `	}` |
|        38 |  3556 | `	pGen->pIn++;` |
|        38 |  3557 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3558 | `		/* Invalid statement*/` |
|         6 |  3559 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|         6 |  3560 | `		if( rc == SXERR_ABORT ){` |
|         - |  3561 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3562 | `			return SXERR_ABORT;` |
|         - |  3563 | `		}` |
|         6 |  3564 | `		goto Synchronize;` |
|         - |  3565 | `	}` |
|        32 |  3566 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3567 | `	/* Allocate a new constant value container */` |
|        32 |  3568 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        32 |  3569 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3570 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3571 | `		return SXERR_ABORT;` |
|         - |  3572 | `	}` |
|        32 |  3573 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3574 | `	/* Swap bytecode container */` |
|        32 |  3575 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        32 |  3576 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3577 | `	/* Compile constant value */` |
|        32 |  3578 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3579 | `	/* Emit the done instruction */` |
|        32 |  3580 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        32 |  3581 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        32 |  3582 | `	if( rc == SXERR_ABORT ){` |
|         - |  3583 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3584 | `		return SXERR_ABORT;` |
|         - |  3585 | `	}` |
|        32 |  3586 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3587 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3588 | `	{` |
|         - |  3589 | `		SyBlob sFQN;` |
|         - |  3590 | `		SyString sFQNStr;` |
|        32 |  3591 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        32 |  3592 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        32 |  3593 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        47 |  3594 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        30 |  3595 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        32 |  3596 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3597 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3598 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3599 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3600 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3601 | `			if( pCEntry ){` |
|         5 |  3602 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3603 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3604 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3605 | `					return SXERR_ABORT;` |
|         - |  3606 | `				}` |
|         2 |  3607 | `			}` |
|         2 |  3608 | `		}` |
|        32 |  3609 | `		SyBlobRelease(&sFQN);` |
|         - |  3610 | `	}` |
|        32 |  3611 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3612 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3613 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3614 | `	}` |
|        32 |  3615 | `	return SXRET_OK;` |
|         9 |  3616 | `Synchronize:` |
|         - |  3617 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3618 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        42 |  3619 | `		pGen->pIn++;` |
|         4 |  3620 | `	}` |
|        22 |  3621 | `	return SXRET_OK;` |
|        29 |  3622 | `}` |
|         - |  3623 | `/*` |
|         - |  3624 | ` * Compile the 'continue' statement.` |
|         - |  3625 | ` * According to the PHP language reference` |
|         - |  3626 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3627 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3628 | ` *  iteration.` |
|         - |  3629 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3630 | ` *  the purposes of continue.` |
|         - |  3631 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3632 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3633 | ` *  Note:` |
|         - |  3634 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3635 | ` */` |
|         - |  3636 | `/*` |
|         - |  3637 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3638 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3639 | ` * break/continue crosses a try boundary.` |
|         - |  3640 | ` *` |
|         - |  3641 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3642 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3643 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3644 | ` */` |
|    118354 |  3645 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3646 | `{` |
|    118359 |  3647 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    118359 |  3648 | `	int nInlineTry = 0;` |
|    552005 |  3649 | `	while( pBlock && pBlock != pTarget ){` |
|    433651 |  3650 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3651 | `			if( pBlock->pUserData ){` |
|         - |  3652 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3653 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3654 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3655 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3656 | `				if( pGen->bInGenerator ){` |
|         3 |  3657 | `					nInlineTry++;` |
|         2 |  3658 | `				}else{` |
|         3 |  3659 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3660 | `				}` |
|         4 |  3661 | `			}else{` |
|         - |  3662 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3663 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3664 | `				break;` |
|         - |  3665 | `			}` |
|         2 |  3666 | `		}` |
|    433651 |  3667 | `		pBlock = pBlock->pParent;` |
|         5 |  3668 | `	}` |
|    118359 |  3669 | `	return nInlineTry;` |
|         5 |  3670 | `}` |
|     59150 |  3671 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3672 | `{` |
|         - |  3673 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3674 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3675 | `	sxu32 nLineLocal;` |
|         - |  3676 | `	sxi32 rc;` |
|     59155 |  3677 | `	nLineLocal = pGen->pIn->nLine;` |
|     59155 |  3678 | `	iLevel = 0;` |
|         - |  3679 | `	/* Jump the 'continue' keyword */` |
|     59155 |  3680 | `	pGen->pIn++;` |
|     59155 |  3681 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3682 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3683 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3684 | `		 */` |
|         - |  3685 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3686 | `		char *zAlloc = 0;` |
|         - |  3687 | `		SyString sNum;` |
|        17 |  3688 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3689 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3690 | `			return SXERR_ABORT;` |
|         - |  3691 | `		}` |
|        17 |  3692 | `		if( rc == SXRET_OK ){` |
|        20 |  3693 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3694 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3695 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3696 | `				return SXERR_ABORT;` |
|         - |  3697 | `			}` |
|        14 |  3698 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3699 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3700 | `		}` |
|        17 |  3701 | `		if( iLevel < 2 ){` |
|         3 |  3702 | `			iLevel = 0;` |
|         1 |  3703 | `		}` |
|        17 |  3704 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3705 | `	}` |
|         - |  3706 | `	/* Point to the target loop */` |
|     59155 |  3707 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     59155 |  3708 | `	if( pLoop == 0 ){` |
|         - |  3709 | `		/* Illegal continue */` |
|        12 |  3710 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|        12 |  3711 | `		if( rc == SXERR_ABORT ){` |
|         - |  3712 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3713 | `			return SXERR_ABORT;` |
|         - |  3714 | `		}` |
|         7 |  3715 | `	}else{` |
|     59145 |  3716 | `		sxu32 nInstrIdx = 0;` |
|         - |  3717 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     59145 |  3718 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3719 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3720 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     59145 |  3721 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     59145 |  3722 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3723 | `			/* According to the PHP language reference manual` |
|         - |  3724 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|         - |  3725 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|         - |  3726 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|         - |  3727 | `			 */` |
|         5 |  3728 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3729 | `			if( rc == SXRET_OK ){` |
|         5 |  3730 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3731 | `			}` |
|         3 |  3732 | `		}else{` |
|         - |  3733 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     59141 |  3734 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     59141 |  3735 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  3736 | `				JumpFixup sJumpFix;` |
|         - |  3737 | `				/* Post-continue */` |
|     19717 |  3738 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     19717 |  3739 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     19717 |  3740 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|      9856 |  3741 | `			}` |
|         - |  3742 | `		}` |
|         - |  3743 | `	}` |
|     59155 |  3744 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3745 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3746 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  3747 | `	}` |
|         - |  3748 | `	/* Statement successfully compiled */` |
|     59155 |  3749 | `	return SXRET_OK;` |
|     29580 |  3750 | `}` |
|         - |  3751 | `/*` |
|         - |  3752 | ` * Compile the 'break' statement.` |
|         - |  3753 | ` * According to the PHP language reference` |
|         - |  3754 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  3755 | ` *  structure.` |
|         - |  3756 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  3757 | ` *  enclosing structures are to be broken out of.` |
|         - |  3758 | ` */` |
|     59230 |  3759 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  3760 | `{` |
|         - |  3761 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3762 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3763 | `	sxi32 rc;` |
|     59235 |  3764 | `	iLevel = 0;` |
|         - |  3765 | `	/* Jump the 'break' keyword */` |
|     59235 |  3766 | `	pGen->pIn++;` |
|     59235 |  3767 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3768 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3769 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3770 | `		 */` |
|         - |  3771 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3772 | `		char *zAlloc = 0;` |
|         - |  3773 | `		SyString sNum;` |
|        17 |  3774 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3775 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3776 | `			return SXERR_ABORT;` |
|         - |  3777 | `		}` |
|        17 |  3778 | `		if( rc == SXRET_OK ){` |
|        21 |  3779 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3780 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  3781 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3782 | `				return SXERR_ABORT;` |
|         - |  3783 | `			}` |
|        15 |  3784 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  3785 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3786 | `		}` |
|        17 |  3787 | `		if( iLevel < 2 ){` |
|         3 |  3788 | `			iLevel = 0;` |
|         1 |  3789 | `		}` |
|        17 |  3790 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3791 | `	}` |
|         - |  3792 | `	/* Extract the target loop */` |
|     59235 |  3793 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     59235 |  3794 | `	if( pLoop == 0 ){` |
|         - |  3795 | `		/* Illegal break */` |
|        19 |  3796 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|        19 |  3797 | `		if( rc == SXERR_ABORT ){` |
|         - |  3798 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3799 | `			return SXERR_ABORT;` |
|         - |  3800 | `		}` |
|        11 |  3801 | `	}else{` |
|         - |  3802 | `		sxu32 nInstrIdx;` |
|         - |  3803 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     59219 |  3804 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3805 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     59219 |  3806 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     59219 |  3807 | `		if( rc == SXRET_OK ){` |
|         - |  3808 | `			/* Fix the jump later when the jump destination is resolved */` |
|     59219 |  3809 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     29607 |  3810 | `		}` |
|         - |  3811 | `	}` |
|     59235 |  3812 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3813 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3814 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  3815 | `	}` |
|         - |  3816 | `	/* Statement successfully compiled */` |
|     59235 |  3817 | `	return SXRET_OK;` |
|     29620 |  3818 | `}` |
|         - |  3819 | `/*` |
|         - |  3820 | ` * Compile or record a label.` |
|         - |  3821 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  3822 | ` * Example` |
|         - |  3823 | ` *  goto LABEL;` |
|         - |  3824 | ` *   echo 'Foo';` |
|         - |  3825 | ` *  LABEL:` |
|         - |  3826 | ` *   echo 'Bar';` |
|         - |  3827 | ` */` |
|       112 |  3828 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  3829 | `{` |
|         - |  3830 | `	GenBlock *pBlock;` |
|         - |  3831 | `	Label sLabel;` |
|         - |  3832 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|       117 |  3833 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|       117 |  3834 | `	if( pBlock ){` |
|         - |  3835 | `		sxi32 rc;` |
|         8 |  3836 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         4 |  3837 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|         6 |  3838 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3839 | `			return SXERR_ABORT;` |
|         - |  3840 | `		}` |
|         4 |  3841 | `	}else{` |
|       113 |  3842 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3843 | `		char *zDup;` |
|         - |  3844 | `		/* Initialize label fields */` |
|       113 |  3845 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  3846 | `		/* Duplicate label name */` |
|       113 |  3847 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       113 |  3848 | `		if( zDup == 0 ){` |
|       ! 0 |  3849 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3850 | `			return SXERR_ABORT;` |
|         - |  3851 | `		}` |
|       113 |  3852 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       113 |  3853 | `		sLabel.bRef  = FALSE;` |
|       113 |  3854 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       113 |  3855 | `		pBlock = pGen->pCurrent;` |
|       221 |  3856 | `		while( pBlock ){` |
|       133 |  3857 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        24 |  3858 | `				break;` |
|         - |  3859 | `			}` |
|         - |  3860 | `			/* Point to the upper block */` |
|       113 |  3861 | `			pBlock = pBlock->pParent;` |
|         5 |  3862 | `		}` |
|       113 |  3863 | `		if( pBlock ){` |
|        24 |  3864 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        14 |  3865 | `		}else{` |
|        93 |  3866 | `			sLabel.pFunc = 0;` |
|         - |  3867 | `		}` |
|         - |  3868 | `		/* Insert in label set */` |
|       113 |  3869 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  3870 | `	}` |
|       117 |  3871 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  3872 | `	return SXRET_OK;` |
|        61 |  3873 | `}` |
|         - |  3874 | `/*` |
|         - |  3875 | ` * Compile the so hated 'goto' statement.` |
|         - |  3876 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  3877 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  3878 | ` * a compiler it has to do this.` |
|         - |  3879 | ` * According to the PHP language reference manual` |
|         - |  3880 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  3881 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  3882 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  3883 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  3884 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  3885 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  3886 | ` *   of a multi-level break` |
|         - |  3887 | ` */` |
|       152 |  3888 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  3889 | `{` |
|         - |  3890 | `	JumpFixup sJump;` |
|         - |  3891 | `	sxi32 rc;` |
|       157 |  3892 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  3893 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3894 | `		/* Missing label */` |
|       ! 0 |  3895 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  3896 | `		if( rc == SXERR_ABORT ){` |
|         - |  3897 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3898 | `			return SXERR_ABORT;` |
|         - |  3899 | `		}` |
|       ! 0 |  3900 | `		return SXRET_OK;` |
|         - |  3901 | `	}` |
|       157 |  3902 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         5 |  3903 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|         5 |  3904 | `		if( rc == SXERR_ABORT ){` |
|         - |  3905 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3906 | `			return SXERR_ABORT;` |
|         - |  3907 | `		}` |
|         3 |  3908 | `	}else{` |
|       153 |  3909 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3910 | `		GenBlock *pBlock;` |
|         - |  3911 | `		char *zDup;` |
|         - |  3912 | `		/* Prepare the jump destination */` |
|       153 |  3913 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  3914 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  3915 | `		/* Duplicate label name */` |
|       153 |  3916 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  3917 | `		if( zDup == 0 ){` |
|       ! 0 |  3918 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3919 | `			return SXERR_ABORT;` |
|         - |  3920 | `		}` |
|       153 |  3921 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|       153 |  3922 | `		pBlock = pGen->pCurrent;` |
|       315 |  3923 | `		while( pBlock ){` |
|       199 |  3924 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        36 |  3925 | `				break;` |
|         - |  3926 | `			}` |
|         - |  3927 | `			/* Point to the upper block */` |
|       167 |  3928 | `			pBlock = pBlock->pParent;` |
|         5 |  3929 | `		}` |
|       153 |  3930 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         8 |  3931 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|         8 |  3932 | `			if( rc == SXERR_ABORT ){` |
|         - |  3933 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  3934 | `				return SXERR_ABORT;` |
|         - |  3935 | `			}` |
|         3 |  3936 | `		}` |
|       153 |  3937 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        29 |  3938 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        16 |  3939 | `		}else{` |
|       127 |  3940 | `			sJump.pFunc = 0;` |
|         - |  3941 | `		}` |
|         - |  3942 | `		/* Emit the unconditional jump */` |
|       153 |  3943 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  3944 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  3945 | `		}` |
|         - |  3946 | `	}` |
|       157 |  3947 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  3948 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  3949 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  3950 | `	}` |
|         - |  3951 | `	/* Statement successfully compiled */` |
|       157 |  3952 | `	return SXRET_OK;` |
|        81 |  3953 | `}` |
|         - |  3954 | `/*` |
|         - |  3955 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  3956 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  3957 | ` * failure.` |
|         - |  3958 | ` */` |
|        20 |  3959 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         2 |  3960 | `{` |
|         - |  3961 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  3962 | `	sxu32 nRawObj;` |
|        10 |  3963 | `	sxu32 nObjIdx;` |
|         - |  3964 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  3965 | `	 * a PHP block.` |
|         - |  3966 | `	 */` |
|        10 |  3967 | `Consume:` |
|        22 |  3968 | `	nRawObj = nObjIdx = 0;` |
|        22 |  3969 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  3970 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  3971 | `		if( pRawObj == 0 ){` |
|       ! 0 |  3972 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3973 | `			return SXERR_ABORT;` |
|         - |  3974 | `		}` |
|         - |  3975 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  3976 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  3977 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  3978 | `		++nRawObj;` |
|       ! 0 |  3979 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  3980 | `	}` |
|        22 |  3981 | `	if( nRawObj > 0 ){` |
|         - |  3982 | `		/* Emit the consume instruction */` |
|       ! 0 |  3983 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  3984 | `	}` |
|        22 |  3985 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  3986 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  3987 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  3988 | `		SySetReset(pTokenSet);` |
|       ! 0 |  3989 | `		SySetReset(&pGen->aTrivia);` |
|         - |  3990 | `		/* Tokenize input */` |
|       ! 0 |  3991 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  3992 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  3993 | `		/* Point to the fresh token stream */` |
|       ! 0 |  3994 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  3995 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  3996 | `		/* Advance the stream cursor */` |
|       ! 0 |  3997 | `		pGen->pRawIn++;` |
|         - |  3998 | `		/* TICKET 1433-011 */` |
|       ! 0 |  3999 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4000 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4001 | `			sxi32 rc;` |
|         - |  4002 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4003 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4004 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4005 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4006 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4007 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4008 | `				return SXERR_ABORT;` |
|       ! 0 |  4009 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4010 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4011 | `			}` |
|       ! 0 |  4012 | `			goto Consume;` |
|         - |  4013 | `		}` |
|       ! 0 |  4014 | `	}else{` |
|         - |  4015 | `		/* No more chunks to process */` |
|        22 |  4016 | `		pGen->pIn = pGen->pEnd;` |
|        22 |  4017 | `		return SXERR_EOF;` |
|         - |  4018 | `	}` |
|       ! 0 |  4019 | `	return SXRET_OK;` |
|        12 |  4020 | `}` |
|         - |  4021 | `/*` |
|         - |  4022 | ` * Compile a PHP block.` |
|         - |  4023 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4024 | ` * optionally delimited by braces {}.` |
|         - |  4025 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4026 | ` * and this function takes care of generating the appropriate error` |
|         - |  4027 | ` * message.` |
|         - |  4028 | ` */` |
|   5365542 |  4029 | `static sxi32 PH7_CompileBlock(` |
|         - |  4030 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4031 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4032 | `	)` |
|         5 |  4033 | `{` |
|         - |  4034 | `	sxi32 rc;` |
|         - |  4035 | `	sxu32 nLine;` |
|   5365547 |  4036 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5364239 |  4037 | `		nLine = pGen->pIn->nLine;` |
|   5364239 |  4038 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5364239 |  4039 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4040 | `			return SXERR_ABORT;` |
|         - |  4041 | `		}` |
|   5364239 |  4042 | `		pGen->pIn++;` |
|         - |  4043 | `		/* Compile until we hit the closing braces '}' */` |
|   7847469 |  4044 | `		for(;;){` |
|  15694943 |  4045 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        22 |  4046 | `				rc = GenStateNextChunk(&(*pGen));` |
|        22 |  4047 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4048 | `			 	   return SXERR_ABORT;` |
|         - |  4049 | `				}` |
|        22 |  4050 | `				if( rc == SXERR_EOF ){` |
|         - |  4051 | `					/* No more token to process. Missing closing braces */` |
|        22 |  4052 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|        22 |  4053 | `					break;` |
|         - |  4054 | `				}` |
|       ! 0 |  4055 | `			}` |
|  15694923 |  4056 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4057 | `				/* Closing braces found,break immediately*/` |
|   5364219 |  4058 | `				pGen->pIn++;` |
|   5364219 |  4059 | `				break;` |
|         - |  4060 | `			}` |
|         - |  4061 | `			/* Compile a single statement */` |
|  10330709 |  4062 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  10330709 |  4063 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4064 | `				return SXERR_ABORT;` |
|         - |  4065 | `			}` |
|         5 |  4066 | `		}` |
|   5364239 |  4067 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   2683430 |  4068 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4069 | `		pGen->pIn++;` |
|       ! 0 |  4070 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4071 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4072 | `			return SXERR_ABORT;` |
|         - |  4073 | `		}` |
|         - |  4074 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4075 | `		for(;;){` |
|       ! 0 |  4076 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4077 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4078 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4079 | `			 	   return SXERR_ABORT;` |
|         - |  4080 | `				}` |
|       ! 0 |  4081 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4082 | `					/* No more token to process */` |
|       ! 0 |  4083 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4084 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4085 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4086 | `					}` |
|       ! 0 |  4087 | `					break;` |
|         - |  4088 | `				}` |
|       ! 0 |  4089 | `			}` |
|       ! 0 |  4090 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4091 | `				sxi32 nKwrd;` |
|         - |  4092 | `				/* Keyword found */` |
|       ! 0 |  4093 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4094 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4095 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4096 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4097 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4098 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4099 | `						}` |
|       ! 0 |  4100 | `						break;` |
|         - |  4101 | `				}` |
|       ! 0 |  4102 | `			}` |
|         - |  4103 | `			/* Compile a single statement */` |
|       ! 0 |  4104 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4105 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4106 | `				return SXERR_ABORT;` |
|         - |  4107 | `			}` |
|       ! 0 |  4108 | `		}` |
|       ! 0 |  4109 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4110 | `	}else{` |
|         - |  4111 | `		/* Compile a single statement */` |
|      1313 |  4112 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      1313 |  4113 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4114 | `			return SXERR_ABORT;` |
|         - |  4115 | `		}` |
|         - |  4116 | `	}` |
|         - |  4117 | `	/* Jump trailing semi-colons ';' */` |
|   5365547 |  4118 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4119 | `		pGen->pIn++;` |
|       ! 0 |  4120 | `	}` |
|   5365547 |  4121 | `	return SXRET_OK;` |
|   2682776 |  4122 | `}` |
|         - |  4123 | `/*` |
|         - |  4124 | ` * Compile the gentle 'while' statement.` |
|         - |  4125 | ` * According to the PHP language reference` |
|         - |  4126 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4127 | ` *  The basic form of a while statement is:` |
|         - |  4128 | ` *  while (expr)` |
|         - |  4129 | ` *   statement` |
|         - |  4130 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4131 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4132 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4133 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4134 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4135 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4136 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4137 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4138 | ` *  while (expr):` |
|         - |  4139 | ` *    statement` |
|         - |  4140 | ` *   endwhile;` |
|         - |  4141 | ` */` |
|     55302 |  4142 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4143 | `{` |
|     55307 |  4144 | `	GenBlock *pWhileBlock = 0;` |
|     55307 |  4145 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4146 | `	sxu32 nFalseJump;` |
|         - |  4147 | `	sxu32 nLine;` |
|         - |  4148 | `	sxi32 rc;` |
|     55307 |  4149 | `	nLine = pGen->pIn->nLine;` |
|         - |  4150 | `	/* Jump the 'while' keyword */` |
|     55307 |  4151 | `	pGen->pIn++;` |
|     55307 |  4152 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4153 | `		/* Syntax error */` |
|       ! 0 |  4154 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4155 | `		if( rc == SXERR_ABORT ){` |
|         - |  4156 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4157 | `			return SXERR_ABORT;` |
|         - |  4158 | `		}` |
|       ! 0 |  4159 | `		goto Synchronize;` |
|         - |  4160 | `	}` |
|         - |  4161 | `	/* Jump the left parenthesis '(' */` |
|     55307 |  4162 | `	pGen->pIn++;` |
|         - |  4163 | `	/* Create the loop block */` |
|     55307 |  4164 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     55307 |  4165 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4166 | `		return SXERR_ABORT;` |
|         - |  4167 | `	}` |
|         - |  4168 | `	/* Delimit the condition */` |
|     55307 |  4169 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     55307 |  4170 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4171 | `		/* Empty expression */` |
|         3 |  4172 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4173 | `		if( rc == SXERR_ABORT ){` |
|         - |  4174 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4175 | `			return SXERR_ABORT;` |
|         - |  4176 | `		}` |
|         1 |  4177 | `	}` |
|         - |  4178 | `	/* Swap token streams */` |
|     55307 |  4179 | `	pTmp = pGen->pEnd;` |
|     55307 |  4180 | `	pGen->pEnd = pEnd;` |
|         - |  4181 | `	/* Compile the expression */` |
|     55307 |  4182 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     55307 |  4183 | `	if( rc == SXERR_ABORT ){` |
|         - |  4184 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4185 | `		return SXERR_ABORT;` |
|         - |  4186 | `	}` |
|         - |  4187 | `	/* Update token stream */` |
|     55307 |  4188 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4189 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4190 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4191 | `			return SXERR_ABORT;` |
|         - |  4192 | `		}` |
|       ! 0 |  4193 | `		pGen->pIn++;` |
|       ! 0 |  4194 | `	}` |
|         - |  4195 | `	/* Synchronize pointers */` |
|     55307 |  4196 | `	pGen->pIn  = &pEnd[1];` |
|     55307 |  4197 | `	pGen->pEnd = pTmp;` |
|         - |  4198 | `	/* Emit the false jump */` |
|     55307 |  4199 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4200 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     55307 |  4201 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4202 | `	/* Compile the loop body */` |
|     55307 |  4203 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     55307 |  4204 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4205 | `		return SXERR_ABORT;` |
|         - |  4206 | `	}` |
|         - |  4207 | `	/* Emit the unconditional jump to the start of the loop */` |
|     55307 |  4208 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4209 | `	/* Fix all jumps now the destination is resolved */` |
|     55307 |  4210 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4211 | `	/* Release the loop block */` |
|     55307 |  4212 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4213 | `	/* Statement successfully compiled */` |
|     55307 |  4214 | `	return SXRET_OK;` |
|       ! 0 |  4215 | `Synchronize:` |
|         - |  4216 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4217 | `	 * compiling this erroneous block.` |
|         - |  4218 | `	 */` |
|       ! 0 |  4219 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4220 | `		pGen->pIn++;` |
|       ! 0 |  4221 | `	}` |
|       ! 0 |  4222 | `	return SXRET_OK;` |
|     27656 |  4223 | `}` |
|         - |  4224 | `/*` |
|         - |  4225 | ` * Compile the ugly do..while() statement.` |
|         - |  4226 | ` * According to the PHP language reference` |
|         - |  4227 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4228 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4229 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4230 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4231 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4232 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4233 | ` *  would end immediately).` |
|         - |  4234 | ` *  There is just one syntax for do-while loops:` |
|         - |  4235 | ` *  <?php` |
|         - |  4236 | ` *  $i = 0;` |
|         - |  4237 | ` *  do {` |
|         - |  4238 | ` *   echo $i;` |
|         - |  4239 | ` *  } while ($i > 0);` |
|         - |  4240 | ` * ?>` |
|         - |  4241 | ` */` |
|         2 |  4242 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4243 | `{` |
|         3 |  4244 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4245 | `	GenBlock *pDoBlock = 0;` |
|         - |  4246 | `	sxu32 nLine;` |
|         - |  4247 | `	sxi32 rc;` |
|         3 |  4248 | `	nLine = pGen->pIn->nLine;` |
|         - |  4249 | `	/* Jump the 'do' keyword */` |
|         3 |  4250 | `	pGen->pIn++;` |
|         - |  4251 | `	/* Create the loop block */` |
|         3 |  4252 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4253 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4254 | `		return SXERR_ABORT;` |
|         - |  4255 | `	}` |
|         - |  4256 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4257 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4258 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4259 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4260 | `		return SXERR_ABORT;` |
|         - |  4261 | `	}` |
|         3 |  4262 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4263 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4264 | `	}` |
|         3 |  4265 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4266 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4267 | `			/* Missing 'while' statement */` |
|         3 |  4268 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4269 | `			if( rc == SXERR_ABORT ){` |
|         - |  4270 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4271 | `				return SXERR_ABORT;` |
|         - |  4272 | `			}` |
|         3 |  4273 | `			goto Synchronize;` |
|         - |  4274 | `	}` |
|         - |  4275 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4276 | `	pGen->pIn++;` |
|       ! 0 |  4277 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4278 | `		/* Syntax error */` |
|       ! 0 |  4279 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4280 | `		if( rc == SXERR_ABORT ){` |
|         - |  4281 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4282 | `			return SXERR_ABORT;` |
|         - |  4283 | `		}` |
|       ! 0 |  4284 | `		goto Synchronize;` |
|         - |  4285 | `	}` |
|         - |  4286 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4287 | `	pGen->pIn++;` |
|         - |  4288 | `	/* Delimit the condition */` |
|       ! 0 |  4289 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4290 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4291 | `		/* Empty expression */` |
|       ! 0 |  4292 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4293 | `		if( rc == SXERR_ABORT ){` |
|         - |  4294 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4295 | `			return SXERR_ABORT;` |
|         - |  4296 | `		}` |
|       ! 0 |  4297 | `		goto Synchronize;` |
|         - |  4298 | `	}` |
|         - |  4299 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4300 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4301 | `		JumpFixup *aPost;` |
|         - |  4302 | `		VmInstr *pInstr;` |
|         - |  4303 | `		sxu32 nJumpDest;` |
|         - |  4304 | `		sxu32 n;` |
|       ! 0 |  4305 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4306 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4307 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4308 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4309 | `			if( pInstr ){` |
|         - |  4310 | `				/* Fix */` |
|       ! 0 |  4311 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4312 | `			}` |
|       ! 0 |  4313 | `		}` |
|       ! 0 |  4314 | `	}` |
|         - |  4315 | `	/* Swap token streams */` |
|       ! 0 |  4316 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4317 | `	pGen->pEnd = pEnd;` |
|         - |  4318 | `	/* Compile the expression */` |
|       ! 0 |  4319 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4320 | `	if( rc == SXERR_ABORT ){` |
|         - |  4321 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4322 | `		return SXERR_ABORT;` |
|         - |  4323 | `	}` |
|         - |  4324 | `	/* Update token stream */` |
|       ! 0 |  4325 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4326 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4327 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4328 | `			return SXERR_ABORT;` |
|         - |  4329 | `		}` |
|       ! 0 |  4330 | `		pGen->pIn++;` |
|       ! 0 |  4331 | `	}` |
|       ! 0 |  4332 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4333 | `	pGen->pEnd = pTmp;` |
|         - |  4334 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4336 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4337 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4338 | `	/* Release the loop block */` |
|       ! 0 |  4339 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4340 | `	/* Statement successfully compiled */` |
|       ! 0 |  4341 | `	return SXRET_OK;` |
|         1 |  4342 | `Synchronize:` |
|         - |  4343 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4344 | `	 * compiling this erroneous block.` |
|         - |  4345 | `	 */` |
|         3 |  4346 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4347 | `		pGen->pIn++;` |
|       ! 0 |  4348 | `	}` |
|         3 |  4349 | `	return SXRET_OK;` |
|         2 |  4350 | `}` |
|         - |  4351 | `/*` |
|         - |  4352 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4353 | ` * According to the PHP language reference` |
|         - |  4354 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4355 | ` *  The syntax of a for loop is:` |
|         - |  4356 | ` *  for (expr1; expr2; expr3)` |
|         - |  4357 | ` *   statement` |
|         - |  4358 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4359 | ` *  the beginning of the loop.` |
|         - |  4360 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4361 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4362 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4363 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4364 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4365 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4366 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4367 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4368 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4369 | ` *  of using the for truth expression.` |
|         - |  4370 | ` */` |
|     90764 |  4371 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4372 | `{` |
|     90769 |  4373 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|     90769 |  4374 | `	GenBlock *pForBlock = 0;` |
|         - |  4375 | `	sxu32 nFalseJump;` |
|         - |  4376 | `	sxu32 nLine;` |
|         - |  4377 | `	sxi32 rc;` |
|     90769 |  4378 | `	nLine = pGen->pIn->nLine;` |
|         - |  4379 | `	/* Jump the 'for' keyword */` |
|     90769 |  4380 | `	pGen->pIn++;` |
|     90769 |  4381 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4382 | `		/* Syntax error */` |
|       ! 0 |  4383 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4384 | `		if( rc == SXERR_ABORT ){` |
|         - |  4385 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4386 | `			return SXERR_ABORT;` |
|         - |  4387 | `		}` |
|       ! 0 |  4388 | `		return SXRET_OK;` |
|         - |  4389 | `	}` |
|         - |  4390 | `	/* Jump the left parenthesis '(' */` |
|     90769 |  4391 | `	pGen->pIn++;` |
|         - |  4392 | `	/* Delimit the init-expr;condition;post-expr */` |
|     90769 |  4393 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     90769 |  4394 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4395 | `		/* Empty expression */` |
|       ! 0 |  4396 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4397 | `		if( rc == SXERR_ABORT ){` |
|         - |  4398 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4399 | `			return SXERR_ABORT;` |
|         - |  4400 | `		}` |
|         - |  4401 | `		/* Synchronize */` |
|       ! 0 |  4402 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4403 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4404 | `			pGen->pIn++;` |
|       ! 0 |  4405 | `		}` |
|       ! 0 |  4406 | `		return SXRET_OK;` |
|         - |  4407 | `	}` |
|         - |  4408 | `	/* Swap token streams */` |
|     90769 |  4409 | `	pTmp = pGen->pEnd;` |
|     90769 |  4410 | `	pGen->pEnd = pEnd;` |
|         - |  4411 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4412 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4413 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4414 | `	 * compiled through this same window — recorded as a known leniency. */` |
|     90769 |  4415 | `	pGen->nCommaExprOk++;` |
|         - |  4416 | `	/* Compile initialization expressions if available */` |
|     90769 |  4417 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4418 | `	/* Pop operand lvalues */` |
|     90769 |  4419 | `	if( rc == SXERR_ABORT ){` |
|         - |  4420 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4421 | `		return SXERR_ABORT;` |
|     90769 |  4422 | `	}else if( rc != SXERR_EMPTY ){` |
|     78947 |  4423 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     39471 |  4424 | `	}` |
|     90769 |  4425 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4426 | `		/* Syntax error */` |
|       ! 0 |  4427 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  4428 | `			"for: Expected ';' after initialization expressions");` |
|       ! 0 |  4429 | `		if( rc == SXERR_ABORT ){` |
|         - |  4430 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4431 | `			return SXERR_ABORT;` |
|         - |  4432 | `		}` |
|       ! 0 |  4433 | `		return SXRET_OK;` |
|         - |  4434 | `	}` |
|         - |  4435 | `	/* Jump the trailing ';' */` |
|     90769 |  4436 | `	pGen->pIn++;` |
|         - |  4437 | `	/* Create the loop block */` |
|     90769 |  4438 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|     90769 |  4439 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4440 | `		return SXERR_ABORT;` |
|         - |  4441 | `	}` |
|         - |  4442 | `	/* Deffer continue jumps */` |
|     90769 |  4443 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4444 | `	/* Compile the condition */` |
|     90769 |  4445 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     90769 |  4446 | `	if( rc == SXERR_ABORT ){` |
|         - |  4447 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4448 | `		return SXERR_ABORT;` |
|     90769 |  4449 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4450 | `		/* Emit the false jump */` |
|     78947 |  4451 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4452 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     78947 |  4453 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     39471 |  4454 | `	}` |
|     90769 |  4455 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4456 | `		/* Syntax error */` |
|         6 |  4457 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  4458 | `			"for: Expected ';' after conditionals expressions");` |
|         6 |  4459 | `		if( rc == SXERR_ABORT ){` |
|         - |  4460 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4461 | `			return SXERR_ABORT;` |
|         - |  4462 | `		}` |
|         6 |  4463 | `		return SXRET_OK;` |
|         - |  4464 | `	}` |
|         - |  4465 | `	/* Jump the trailing ';' */` |
|     90765 |  4466 | `	pGen->pIn++;` |
|         - |  4467 | `	/* Save the post condition stream */` |
|     90765 |  4468 | `	pPostStart = pGen->pIn;` |
|         - |  4469 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4470 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|     90765 |  4471 | `	pGen->nCommaExprOk--;` |
|     90765 |  4472 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|     90765 |  4473 | `	pGen->pEnd = pTmp;` |
|     90765 |  4474 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|     90765 |  4475 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4476 | `		return SXERR_ABORT;` |
|         - |  4477 | `	}` |
|         - |  4478 | `	/* Fix post-continue jumps */` |
|     90765 |  4479 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4480 | `		JumpFixup *aPost;` |
|         - |  4481 | `		VmInstr *pInstr;` |
|         - |  4482 | `		sxu32 nJumpDest;` |
|         - |  4483 | `		sxu32 n;` |
|      7897 |  4484 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      7897 |  4485 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     27609 |  4486 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     19717 |  4487 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     19717 |  4488 | `			if( pInstr ){` |
|         - |  4489 | `				/* Fix jump */` |
|     19717 |  4490 | `				pInstr->iP2 = nJumpDest;` |
|      9856 |  4491 | `			}` |
|      9861 |  4492 | `		}` |
|      3946 |  4493 | `	}` |
|         - |  4494 | `	/* compile the post-expressions if available */` |
|     90765 |  4495 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4496 | `		pPostStart++;` |
|       ! 0 |  4497 | `	}` |
|     90765 |  4498 | `	if( pPostStart < pEnd ){` |
|         - |  4499 | `		SyToken *pTmpIn,*pTmpEnd;` |
|     78945 |  4500 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|     78945 |  4501 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|     78945 |  4502 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     78945 |  4503 | `		pGen->nCommaExprOk--;` |
|     78945 |  4504 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4505 | `			/* Syntax error */` |
|       ! 0 |  4506 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4507 | `			if( rc == SXERR_ABORT ){` |
|         - |  4508 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4509 | `				return SXERR_ABORT;` |
|         - |  4510 | `			}` |
|       ! 0 |  4511 | `			return SXRET_OK;` |
|         - |  4512 | `		}` |
|     78945 |  4513 | `		RE_SWAP_DELIMITER(pGen);` |
|     78945 |  4514 | `		if( rc == SXERR_ABORT ){` |
|         - |  4515 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4516 | `			return SXERR_ABORT;` |
|     78945 |  4517 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4518 | `			/* Pop operand lvalue */` |
|     78945 |  4519 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     39470 |  4520 | `		}` |
|     39470 |  4521 | `	}` |
|         - |  4522 | `	/* Emit the unconditional jump to the start of the loop */` |
|     90765 |  4523 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4524 | `	/* Fix all jumps now the destination is resolved */` |
|     90765 |  4525 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4526 | `	/* Release the loop block */` |
|     90765 |  4527 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4528 | `	/* Statement successfully compiled */` |
|     90765 |  4529 | `	return SXRET_OK;` |
|     45387 |  4530 | `}` |
|         - |  4531 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4532 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4533 | ` * are allowed.` |
|         - |  4534 | ` */` |
|    331948 |  4535 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4536 | `{` |
|    331953 |  4537 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    331953 |  4538 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4539 | `		/* Unexpected expression */` |
|       ! 0 |  4540 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4541 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4542 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4543 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4544 | `		}` |
|       ! 0 |  4545 | `	}` |
|    331953 |  4546 | `	return rc;` |
|         5 |  4547 | `}` |
|         - |  4548 | `/*` |
|         - |  4549 | ` * Compile the 'foreach' statement.` |
|         - |  4550 | ` * According to the PHP language reference` |
|         - |  4551 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4552 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4553 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4554 | ` *  is a minor but useful extension of the first:` |
|         - |  4555 | ` *  foreach (array_expression as $value)` |
|         - |  4556 | ` *    statement` |
|         - |  4557 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4558 | ` *   statement` |
|         - |  4559 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4560 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4561 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4562 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4563 | ` *  to the variable $key on each loop.` |
|         - |  4564 | ` *  Note:` |
|         - |  4565 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4566 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4567 | ` *  Note:` |
|         - |  4568 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4569 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4570 | ` *  or after the foreach without resetting it.` |
|         - |  4571 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4572 | ` *  of copying the value.` |
|         - |  4573 | ` */` |
|    229234 |  4574 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4575 | `{` |
|    229239 |  4576 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    229239 |  4577 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    229239 |  4578 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4579 | `	ph7_foreach_info *pInfo;` |
|         - |  4580 | `	sxu32 nFalseJump;` |
|         - |  4581 | `	VmInstr *pInstr;` |
|         - |  4582 | `	sxu32 nLine;` |
|         - |  4583 | `	sxi32 rc;` |
|    229239 |  4584 | `	nLine = pGen->pIn->nLine;` |
|         - |  4585 | `	/* Jump the 'foreach' keyword */` |
|    229239 |  4586 | `	pGen->pIn++;` |
|    229239 |  4587 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4588 | `		/* Syntax error */` |
|       ! 0 |  4589 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4590 | `		if( rc == SXERR_ABORT ){` |
|         - |  4591 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4592 | `			return SXERR_ABORT;` |
|         - |  4593 | `		}` |
|       ! 0 |  4594 | `		goto Synchronize;` |
|         - |  4595 | `	}` |
|         - |  4596 | `	/* Jump the left parenthesis '(' */` |
|    229239 |  4597 | `	pGen->pIn++;` |
|         - |  4598 | `	/* Create the loop block */` |
|    229239 |  4599 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    229239 |  4600 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4601 | `		return SXERR_ABORT;` |
|         - |  4602 | `	}` |
|         - |  4603 | `	/* Delimit the expression */` |
|    229239 |  4604 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    229239 |  4605 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4606 | `		/* Empty expression */` |
|       ! 0 |  4607 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4608 | `		if( rc == SXERR_ABORT ){` |
|         - |  4609 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4610 | `			return SXERR_ABORT;` |
|         - |  4611 | `		}` |
|         - |  4612 | `		/* Synchronize */` |
|       ! 0 |  4613 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4614 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4615 | `			pGen->pIn++;` |
|       ! 0 |  4616 | `		}` |
|       ! 0 |  4617 | `		return SXRET_OK;` |
|         - |  4618 | `	}` |
|         - |  4619 | `	/* Compile the array expression */` |
|    229239 |  4620 | `	pCur = pGen->pIn;` |
|   1237217 |  4621 | `	while( pCur < pEnd ){` |
|   1237217 |  4622 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    241073 |  4623 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    241073 |  4624 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4625 | `				/* Break with the first 'as' found */` |
|    229239 |  4626 | `				break;` |
|         - |  4627 | `			}` |
|      5917 |  4628 | `		}` |
|         - |  4629 | `		/* Advance the stream cursor */` |
|   1007983 |  4630 | `		pCur++;` |
|         5 |  4631 | `	}` |
|    229239 |  4632 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4633 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4634 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4635 | `		if( rc == SXERR_ABORT ){` |
|         - |  4636 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4637 | `			return SXERR_ABORT;` |
|         - |  4638 | `		}` |
|       ! 0 |  4639 | `		goto Synchronize;` |
|         - |  4640 | `	}` |
|         - |  4641 | `	/* Swap token streams */` |
|    229239 |  4642 | `	pTmp = pGen->pEnd;` |
|    229239 |  4643 | `	pGen->pEnd = pCur;` |
|    229239 |  4644 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    229239 |  4645 | `	if( rc == SXERR_ABORT ){` |
|         - |  4646 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4647 | `		return SXERR_ABORT;` |
|         - |  4648 | `	}` |
|         - |  4649 | `	/* Update token stream */` |
|    229239 |  4650 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4651 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4652 | `		if( rc == SXERR_ABORT ){` |
|         - |  4653 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4654 | `			return SXERR_ABORT;` |
|         - |  4655 | `		}` |
|       ! 0 |  4656 | `		pGen->pIn++;` |
|       ! 0 |  4657 | `	}` |
|    229239 |  4658 | `	pCur++; /* Jump the 'as' keyword */` |
|    229239 |  4659 | `	pGen->pIn = pCur;` |
|    229239 |  4660 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4661 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4662 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4663 | `			return SXERR_ABORT;` |
|         - |  4664 | `		}` |
|       ! 0 |  4665 | `	}` |
|         - |  4666 | `	/* Create the foreach context */` |
|    229239 |  4667 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    229239 |  4668 | `	if( pInfo == 0 ){` |
|       ! 0 |  4669 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4670 | `		return SXERR_ABORT;` |
|         - |  4671 | `	}` |
|         - |  4672 | `	/* Zero the structure */` |
|    229239 |  4673 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4674 | `	/* Initialize structure fields */` |
|    229239 |  4675 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4676 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4677 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4678 | `	 * '=>'. */` |
|    229239 |  4679 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    229239 |  4680 | `	if( pCur < pEnd ){` |
|         - |  4681 | `		/* Compile the expression holding the key name */` |
|    102739 |  4682 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4683 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4684 | `			if( rc == SXERR_ABORT ){` |
|         - |  4685 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4686 | `				return SXERR_ABORT;` |
|         - |  4687 | `			}` |
|       ! 0 |  4688 | `		}else{` |
|    102739 |  4689 | `			pGen->pEnd = pCur;` |
|    102739 |  4690 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    102739 |  4691 | `			if( rc == SXERR_ABORT ){` |
|         - |  4692 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4693 | `				return SXERR_ABORT;` |
|         - |  4694 | `			}` |
|    102739 |  4695 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    102739 |  4696 | `			if( pInstr->p3 ){` |
|         - |  4697 | `				/* Record key name */` |
|    102739 |  4698 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     51367 |  4699 | `			}` |
|    102739 |  4700 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4701 | `		}` |
|    102739 |  4702 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     51367 |  4703 | `	}` |
|    229239 |  4704 | `	pGen->pEnd = pEnd;` |
|    229239 |  4705 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4706 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4707 | `		if( rc == SXERR_ABORT ){` |
|         - |  4708 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4709 | `			return SXERR_ABORT;` |
|         - |  4710 | `		}` |
|       ! 0 |  4711 | `		goto Synchronize;` |
|         - |  4712 | `	}` |
|    229239 |  4713 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4714 | `		pGen->pIn++;` |
|         - |  4715 | `		/* Pass by reference  */` |
|        33 |  4716 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4717 | `	}` |
|         - |  4718 | `	/* Check if the value target is list() */` |
|    229239 |  4719 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4720 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4721 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4722 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4723 | `		 */` |
|         - |  4724 | `		static int iForeachListCnt = 0;` |
|         - |  4725 | `		char zTmp[128];` |
|         - |  4726 | `		sxu32 nLen;` |
|         - |  4727 | `		char *zDup;` |
|        10 |  4728 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4729 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4730 | `		if( zDup == 0 ){` |
|       ! 0 |  4731 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4732 | `			return SXERR_ABORT;` |
|         - |  4733 | `		}` |
|        10 |  4734 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4735 | `		/* Save list() token boundaries */` |
|        10 |  4736 | `		pListStart = pGen->pIn;` |
|         - |  4737 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4738 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4739 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4740 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  4741 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  4742 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4743 | `				return SXERR_ABORT;` |
|         - |  4744 | `			}` |
|         3 |  4745 | `			goto Synchronize;` |
|         - |  4746 | `		}` |
|         7 |  4747 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  4748 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  4749 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4750 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4751 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  4752 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4753 | `				return SXERR_ABORT;` |
|         - |  4754 | `			}` |
|       ! 0 |  4755 | `			goto Synchronize;` |
|         - |  4756 | `		}` |
|         7 |  4757 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  4758 | `		pListEnd = pGen->pIn;` |
|         7 |  4759 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    229234 |  4760 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  4761 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  4762 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  4763 | `		 */` |
|         - |  4764 | `		static int iForeachShortListCnt = 0;` |
|         - |  4765 | `		char zTmp[128];` |
|         - |  4766 | `		sxu32 nLen;` |
|         - |  4767 | `		char *zDup;` |
|        13 |  4768 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        13 |  4769 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        13 |  4770 | `		if( zDup == 0 ){` |
|       ! 0 |  4771 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4772 | `			return SXERR_ABORT;` |
|         - |  4773 | `		}` |
|        13 |  4774 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4775 | `		/* Save [...] token boundaries */` |
|        13 |  4776 | `		pListStart = pGen->pIn;` |
|         - |  4777 | `		/* Advance past [...] */` |
|        13 |  4778 | `		pGen->pIn++; /* Jump '[' */` |
|        13 |  4779 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        13 |  4780 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4781 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4782 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  4783 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4784 | `				return SXERR_ABORT;` |
|         - |  4785 | `			}` |
|       ! 0 |  4786 | `			goto Synchronize;` |
|         - |  4787 | `		}` |
|        13 |  4788 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        13 |  4789 | `		pListEnd = pGen->pIn;` |
|        13 |  4790 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         7 |  4791 | `	}else{` |
|         - |  4792 | `		/* Compile the expression holding the value name */` |
|    229219 |  4793 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    229219 |  4794 | `		if( rc == SXERR_ABORT ){` |
|         - |  4795 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4796 | `			return SXERR_ABORT;` |
|         - |  4797 | `		}` |
|    229219 |  4798 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    229219 |  4799 | `		if( pInstr->p3 ){` |
|         - |  4800 | `			/* Record value name */` |
|    229219 |  4801 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    114607 |  4802 | `		}` |
|         - |  4803 | `	}` |
|         - |  4804 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    229237 |  4805 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  4806 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    229237 |  4807 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  4808 | `	/* Record the first instruction to execute */` |
|    229237 |  4809 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4810 | `	/* Emit the FOREACH_STEP instruction */` |
|    229237 |  4811 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  4812 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    229237 |  4813 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  4814 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    229237 |  4815 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  4816 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  4817 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  4818 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  4819 | `		 */` |
|        19 |  4820 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  4821 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  4822 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  4823 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  4824 | `		 */` |
|        19 |  4825 | `		pSavedIn = pGen->pIn;` |
|        19 |  4826 | `		pSavedEnd = pGen->pEnd;` |
|        19 |  4827 | `		pGen->pIn = pListStart;` |
|        19 |  4828 | `		pGen->pEnd = pListEnd;` |
|        19 |  4829 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        13 |  4830 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  4831 | `		}else{` |
|         7 |  4832 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  4833 | `		}` |
|        19 |  4834 | `		pGen->pIn = pSavedIn;` |
|        19 |  4835 | `		pGen->pEnd = pSavedEnd;` |
|        19 |  4836 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4837 | `			return SXERR_ABORT;` |
|         - |  4838 | `		}` |
|         - |  4839 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        19 |  4840 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         9 |  4841 | `	}` |
|         - |  4842 | `	/* Compile the loop body */` |
|    229237 |  4843 | `	pGen->pIn = &pEnd[1];` |
|    229237 |  4844 | `	pGen->pEnd = pTmp;` |
|    229237 |  4845 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    229237 |  4846 | `	if( rc == SXERR_ABORT ){` |
|         - |  4847 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4848 | `		return SXERR_ABORT;` |
|         - |  4849 | `	}` |
|         - |  4850 | `	/* Emit the unconditional jump to the start of the loop */` |
|    229237 |  4851 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  4852 | `	/* Fix all jumps now the destination is resolved */` |
|    229237 |  4853 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4854 | `	/* Release the loop block */` |
|    229237 |  4855 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4856 | `	/* Statement successfully compiled */` |
|    229237 |  4857 | `	return SXRET_OK;` |
|         1 |  4858 | `Synchronize:` |
|         - |  4859 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4860 | `	 * compiling this erroneous block.` |
|         - |  4861 | `	 */` |
|         3 |  4862 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4863 | `		pGen->pIn++;` |
|       ! 0 |  4864 | `	}` |
|         3 |  4865 | `	return SXRET_OK;` |
|    114622 |  4866 | `}` |
|         - |  4867 | `/*` |
|         - |  4868 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  4869 | ` * According to the PHP language reference` |
|         - |  4870 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  4871 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  4872 | ` *  that is similar to that of C:` |
|         - |  4873 | ` *  if (expr)` |
|         - |  4874 | ` *   statement` |
|         - |  4875 | ` *  else construct:` |
|         - |  4876 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  4877 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  4878 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  4879 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  4880 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  4881 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  4882 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  4883 | ` *  elseif` |
|         - |  4884 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  4885 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  4886 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  4887 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  4888 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  4889 | ` *   <?php` |
|         - |  4890 | ` *    if ($a > $b) {` |
|         - |  4891 | ` *     echo "a is bigger than b";` |
|         - |  4892 | ` *    } elseif ($a == $b) {` |
|         - |  4893 | ` *     echo "a is equal to b";` |
|         - |  4894 | ` *    } else {` |
|         - |  4895 | ` *     echo "a is smaller than b";` |
|         - |  4896 | ` *    }` |
|         - |  4897 | ` *    ?>` |
|         - |  4898 | ` */` |
|   1925046 |  4899 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  4900 | `{` |
|   1925051 |  4901 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   1925051 |  4902 | `	GenBlock *pCondBlock = 0;` |
|         - |  4903 | `	sxu32 nJumpIdx;` |
|         - |  4904 | `	sxu32 nKeyID;` |
|         - |  4905 | `	sxi32 rc;` |
|         - |  4906 | `	/* Jump the 'if' keyword */` |
|   1925051 |  4907 | `	pGen->pIn++;` |
|   1925051 |  4908 | `	pToken = pGen->pIn;` |
|         - |  4909 | `	/* Create the conditional block */` |
|   1925051 |  4910 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   1925051 |  4911 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4912 | `		return SXERR_ABORT;` |
|         - |  4913 | `	}` |
|         - |  4914 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1072902 |  4915 | `	for(;;){` |
|   2145809 |  4916 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4917 | `			/* Syntax error */` |
|       ! 0 |  4918 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4919 | `				pToken--;` |
|       ! 0 |  4920 | `			}` |
|       ! 0 |  4921 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  4922 | `			if( rc == SXERR_ABORT ){` |
|         - |  4923 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4924 | `				return SXERR_ABORT;` |
|         - |  4925 | `			}` |
|       ! 0 |  4926 | `			goto Synchronize;` |
|         - |  4927 | `		}` |
|         - |  4928 | `		/* Jump the left parenthesis '(' */` |
|   2145809 |  4929 | `		pToken++;` |
|         - |  4930 | `		/* Delimit the condition */` |
|   2145809 |  4931 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2145809 |  4932 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  4933 | `			/* Syntax error */` |
|        10 |  4934 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4935 | `				pToken--;` |
|       ! 0 |  4936 | `			}` |
|        10 |  4937 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        10 |  4938 | `			if( rc == SXERR_ABORT ){` |
|         - |  4939 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4940 | `				return SXERR_ABORT;` |
|         - |  4941 | `			}` |
|        10 |  4942 | `			goto Synchronize;` |
|         - |  4943 | `		}` |
|         - |  4944 | `		/* Swap token streams */` |
|   2145801 |  4945 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  4946 | `		/* Compile the condition */` |
|   2145801 |  4947 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4948 | `		/* Update token stream */` |
|   2145801 |  4949 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  4950 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4951 | `			pGen->pIn++;` |
|       ! 0 |  4952 | `		}` |
|   2145801 |  4953 | `		pGen->pIn  = &pEnd[1];` |
|   2145801 |  4954 | `		pGen->pEnd = pTmp;` |
|   2145801 |  4955 | `		if( rc == SXERR_ABORT ){` |
|         - |  4956 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4957 | `			return SXERR_ABORT;` |
|         - |  4958 | `		}` |
|         - |  4959 | `		/* Emit the false jump */` |
|   2145801 |  4960 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  4961 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2145801 |  4962 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  4963 | `		/* Compile the body */` |
|   2145801 |  4964 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2145801 |  4965 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4966 | `			return SXERR_ABORT;` |
|         - |  4967 | `		}` |
|   2145801 |  4968 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    426280 |  4969 | `			break;` |
|         - |  4970 | `		}` |
|         - |  4971 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1293251 |  4972 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1293251 |  4973 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|    910489 |  4974 | `			break;` |
|         - |  4975 | `		}` |
|         - |  4976 | `		/* Emit the unconditional jump */` |
|    382767 |  4977 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  4978 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    382767 |  4979 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    382767 |  4980 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    248689 |  4981 | `			pToken = &pGen->pIn[1];` |
|    248689 |  4982 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     86718 |  4983 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|     81007 |  4984 | `					break;` |
|         - |  4985 | `			}` |
|     86685 |  4986 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     43340 |  4987 | `		}` |
|    220763 |  4988 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  4989 | `		/* Synchronize cursors */` |
|    220763 |  4990 | `		pToken = pGen->pIn;` |
|         - |  4991 | `		/* Fix the false jump */` |
|    220763 |  4992 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  4993 | `	} /* For(;;) */` |
|         - |  4994 | `	/* Fix the false jump */` |
|   1925043 |  4995 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   1925043 |  4996 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1072488 |  4997 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  4998 | `			/* Compile the else block */` |
|    162009 |  4999 | `			pGen->pIn++;` |
|    162009 |  5000 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    162009 |  5001 | `			if( rc == SXERR_ABORT ){` |
|         - |  5002 |  |
|       ! 0 |  5003 | `				return SXERR_ABORT;` |
|         - |  5004 | `			}` |
|     81002 |  5005 | `	}` |
|   1925043 |  5006 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5007 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   1925043 |  5008 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5009 | `	/* Release the conditional block */` |
|   1925043 |  5010 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5011 | `	/* Statement successfully compiled */` |
|   1925043 |  5012 | `	return SXRET_OK;` |
|         4 |  5013 | `Synchronize:` |
|         - |  5014 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5015 | `	 */` |
|        66 |  5016 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        58 |  5017 | `		pGen->pIn++;` |
|         2 |  5018 | `	}` |
|        10 |  5019 | `	return SXRET_OK;` |
|    962528 |  5020 | `}` |
|         - |  5021 | `/*` |
|         - |  5022 | ` * Compile the global construct.` |
|         - |  5023 | ` * According to the PHP language reference` |
|         - |  5024 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5025 | ` *  to be used in that function.` |
|         - |  5026 | ` *  Example #1 Using global` |
|         - |  5027 | ` *  <?php` |
|         - |  5028 | ` *   $a = 1;` |
|         - |  5029 | ` *   $b = 2;` |
|         - |  5030 | ` *   function Sum()` |
|         - |  5031 | ` *   {` |
|         - |  5032 | ` *    global $a, $b;` |
|         - |  5033 | ` *    $b = $a + $b;` |
|         - |  5034 | ` *   }` |
|         - |  5035 | ` *   Sum();` |
|         - |  5036 | ` *   echo $b;` |
|         - |  5037 | ` *  ?>` |
|         - |  5038 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5039 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5040 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5041 | ` */` |
|        38 |  5042 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5043 | `{` |
|        43 |  5044 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5045 | `	sxi32 nExpr;` |
|         - |  5046 | `	sxi32 rc;` |
|         - |  5047 | `	/* Jump the 'global' keyword */` |
|        43 |  5048 | `	pGen->pIn++;` |
|        43 |  5049 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5050 | `		/* Nothing to process */` |
|       ! 0 |  5051 | `		return SXRET_OK;` |
|         - |  5052 | `	}` |
|        43 |  5053 | `	pTmp = pGen->pEnd;` |
|        43 |  5054 | `	nExpr = 0;` |
|        91 |  5055 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5056 | `		if( pGen->pIn < pNext ){` |
|        53 |  5057 | `			pGen->pEnd = pNext;` |
|        53 |  5058 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5059 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5060 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5061 | `					return SXERR_ABORT;` |
|         - |  5062 | `				}` |
|       ! 0 |  5063 | `			}else{` |
|        53 |  5064 | `				pGen->pIn++;` |
|        53 |  5065 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5066 | `					/* Emit a warning */` |
|       ! 0 |  5067 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5068 | `				}else{` |
|        53 |  5069 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5070 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5071 | `						return SXERR_ABORT;` |
|        53 |  5072 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5073 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5074 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5075 | `							/* Variable name, not a constant */` |
|        53 |  5076 | `							pLast->iP1 = 0;` |
|        24 |  5077 | `						}` |
|        53 |  5078 | `						nExpr++;` |
|        24 |  5079 | `					}` |
|         - |  5080 | `				}` |
|         - |  5081 | `			}` |
|        24 |  5082 | `		}` |
|         - |  5083 | `		/* Next expression in the stream */` |
|        53 |  5084 | `		pGen->pIn = pNext;` |
|         - |  5085 | `		/* Jump trailing commas */` |
|        63 |  5086 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5087 | `			pGen->pIn++;` |
|         5 |  5088 | `		}` |
|         5 |  5089 | `	}` |
|         - |  5090 | `	/* Restore token stream */` |
|        43 |  5091 | `	pGen->pEnd = pTmp;` |
|        43 |  5092 | `	if( nExpr > 0 ){` |
|         - |  5093 | `		/* Emit the uplink instruction */` |
|        43 |  5094 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5095 | `	}` |
|        43 |  5096 | `	return SXRET_OK;` |
|        24 |  5097 | `}` |
|         - |  5098 | `/*` |
|         - |  5099 | ` * Compile the return statement.` |
|         - |  5100 | ` * According to the PHP language reference` |
|         - |  5101 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5102 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5103 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5104 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5105 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5106 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5107 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5108 | ` *  from within the main script file, then script execution end.` |
|         - |  5109 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5110 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5111 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5112 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5113 | ` */` |
|   2744310 |  5114 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5115 | `{` |
|   2744315 |  5116 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5117 | `	sxi32 rc;` |
|   2744315 |  5118 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2744315 |  5119 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5120 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5121 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5122 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5123 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5124 | `	 * normally below so token processing stays consistent. */` |
|   7112703 |  5125 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4368393 |  5126 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5127 | `	}` |
|   2744310 |  5128 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2744283 |  5129 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5130 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5131 | `			"A never-returning function must not return");` |
|         3 |  5132 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5133 | `			return SXERR_ABORT;` |
|         - |  5134 | `		}` |
|         1 |  5135 | `	}` |
|         - |  5136 | `	/* Jump the 'return' keyword */` |
|   2744315 |  5137 | `	pGen->pIn++;` |
|   2744315 |  5138 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5139 | `		/* Compile the expression */` |
|   2657605 |  5140 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2657605 |  5141 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5142 | `			return SXERR_ABORT;` |
|   2657605 |  5143 | `		}else if(rc != SXERR_EMPTY ){` |
|   2657605 |  5144 | `			nRet = 1;` |
|   1328800 |  5145 | `		}` |
|   1328800 |  5146 | `	}` |
|         - |  5147 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5148 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5149 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5150 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2744315 |  5151 | `	if( pGen->bInGenerator ){` |
|      3973 |  5152 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3973 |  5153 | `		return SXRET_OK;` |
|         - |  5154 | `	}` |
|         - |  5155 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5156 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5157 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5158 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5159 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2740347 |  5160 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2740347 |  5161 | `	return SXRET_OK;` |
|   1372160 |  5162 | `}` |
|         - |  5163 | `/*` |
|         - |  5164 | ` * Compile a yield expression.` |
|         - |  5165 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5166 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5167 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5168 | ` */` |
|     16144 |  5169 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5170 | `{` |
|         - |  5171 | `	SyToken *pTmp, *pSplit;` |
|     16149 |  5172 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     16149 |  5173 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5174 | `	sxi32 rc;` |
|      8072 |  5175 | `	(void)iCompileFlag;` |
|         - |  5176 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     16149 |  5177 | `	pGen->pIn++;` |
|         - |  5178 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5179 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5180 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5181 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5182 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     16144 |  5183 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      8107 |  5184 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5185 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5186 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5187 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5188 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5189 | `			return SXERR_ABORT;` |
|         - |  5190 | `		}` |
|        67 |  5191 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5192 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5193 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5194 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5195 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5196 | `				return SXERR_ABORT;` |
|         - |  5197 | `			}` |
|       ! 0 |  5198 | `		}` |
|        67 |  5199 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5200 | `		return SXRET_OK;` |
|         - |  5201 | `	}` |
|     16087 |  5202 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5203 | `		/* Bare yield — no value */` |
|         3 |  5204 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5205 | `		return SXRET_OK;` |
|         - |  5206 | `	}` |
|         - |  5207 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     16085 |  5208 | `	pSplit = 0;` |
|         - |  5209 | `	{` |
|     16085 |  5210 | `		SyToken *pCur = pGen->pIn;` |
|     16085 |  5211 | `		sxi32 nNest = 0;` |
|     48061 |  5212 | `		while( pCur < pGen->pEnd ){` |
|     47755 |  5213 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5214 | `				nNest++;` |
|     47747 |  5215 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5216 | `				nNest--;` |
|     47731 |  5217 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15779 |  5218 | `				pSplit = pCur;` |
|     15779 |  5219 | `				break;` |
|         - |  5220 | `			}` |
|     31981 |  5221 | `			pCur++;` |
|         5 |  5222 | `		}` |
|         - |  5223 | `	}` |
|     16085 |  5224 | `	pTmp = pGen->pEnd;` |
|     16085 |  5225 | `	if( pSplit ){` |
|         - |  5226 | `		/* yield $key => $value */` |
|     15779 |  5227 | `		pGen->pEnd = pSplit;` |
|     15779 |  5228 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15779 |  5229 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15779 |  5230 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15779 |  5231 | `		pGen->pEnd = pTmp;` |
|     15779 |  5232 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15779 |  5233 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15779 |  5234 | `		iP1 = 1;` |
|     15779 |  5235 | `		iP2 = 1;` |
|      7892 |  5236 | `	}else{` |
|         - |  5237 | `		/* yield $value */` |
|       311 |  5238 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       311 |  5239 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       311 |  5240 | `		if( rc != SXERR_EMPTY ){` |
|       311 |  5241 | `			iP1 = 1;` |
|       153 |  5242 | `		}` |
|         - |  5243 | `	}` |
|     16085 |  5244 | `	pGen->pEnd = pTmp;` |
|     16085 |  5245 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     16085 |  5246 | `	return SXRET_OK;` |
|      8077 |  5247 | `}` |
|         - |  5248 | `/*` |
|         - |  5249 | ` * Compile the die/exit language construct.` |
|         - |  5250 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5251 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5252 | ` */` |
|       128 |  5253 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5254 | `{` |
|       133 |  5255 | `	sxi32 nExpr = 0;` |
|         - |  5256 | `	sxi32 rc;` |
|         - |  5257 | `	/* Jump the die/exit keyword */` |
|       133 |  5258 | `	pGen->pIn++;` |
|       133 |  5259 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5260 | `		/* Compile the expression */` |
|       133 |  5261 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5262 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5263 | `			return SXERR_ABORT;` |
|       133 |  5264 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5265 | `			nExpr = 1;` |
|        64 |  5266 | `		}` |
|        64 |  5267 | `	}` |
|         - |  5268 | `	/* Emit the HALT instruction */` |
|       133 |  5269 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5270 | `	return SXRET_OK;` |
|        69 |  5271 | `}` |
|         - |  5272 | `/*` |
|         - |  5273 | ` * Compile the 'echo' language construct.` |
|         - |  5274 | ` */` |
|     17798 |  5275 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5276 | `{` |
|     17803 |  5277 | `	SyToken *pTmp,*pNext = 0;` |
|     17803 |  5278 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17803 |  5279 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17803 |  5280 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5281 | `	sxi32 rc;` |
|         - |  5282 | `	/* Jump the 'echo' keyword */` |
|     17803 |  5283 | `	pGen->pIn++;` |
|         - |  5284 | `	/* Compile arguments one after one */` |
|     17803 |  5285 | `	pTmp = pGen->pEnd;` |
|     44541 |  5286 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     26745 |  5287 | `		if( pGen->pIn < pNext ){` |
|     26745 |  5288 | `			pGen->pEnd = pNext;` |
|     26745 |  5289 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     26745 |  5290 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5291 | `				return SXERR_ABORT;` |
|     26745 |  5292 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5293 | `				/* Emit the consume instruction */` |
|     26721 |  5294 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     26721 |  5295 | `				nExpr++;` |
|     26721 |  5296 | `				bExpectMore = 0;` |
|     13358 |  5297 | `			}` |
|     13370 |  5298 | `		}` |
|         - |  5299 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5300 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     35693 |  5301 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      8955 |  5302 | `			if( bExpectMore ){` |
|         - |  5303 | `				/* two commas in a row */` |
|         3 |  5304 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5305 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5306 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5307 | `			}` |
|      8953 |  5308 | `			bExpectMore = 1;` |
|      8953 |  5309 | `			pNext++;` |
|         5 |  5310 | `		}` |
|     26743 |  5311 | `		pGen->pIn = pNext;` |
|         5 |  5312 | `	}` |
|         - |  5313 | `	/* Restore token stream */` |
|     17801 |  5314 | `	pGen->pEnd = pTmp;` |
|     17801 |  5315 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5316 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        32 |  5317 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5318 | `			"syntax error, unexpected token \";\"");` |
|        32 |  5319 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5320 | `	}` |
|     17773 |  5321 | `	return SXRET_OK;` |
|      8904 |  5322 | `}` |
|         - |  5323 | `/*` |
|         - |  5324 | ` * Compile the static statement.` |
|         - |  5325 | ` * According to the PHP language reference` |
|         - |  5326 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5327 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5328 | ` *  when program execution leaves this scope.` |
|         - |  5329 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5330 | ` * Symisc eXtension.` |
|         - |  5331 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5332 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5333 | ` *  Example` |
|         - |  5334 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5335 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5336 | ` */` |
|        12 |  5337 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         3 |  5338 | `{` |
|         - |  5339 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5340 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5341 | `	GenBlock *pBlock;` |
|         - |  5342 | `	SyString *pName;` |
|         - |  5343 | `	char *zDup;` |
|         - |  5344 | `	sxu32 nLine;` |
|         - |  5345 | `	sxi32 rc;` |
|         - |  5346 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5347 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5348 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|        12 |  5349 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|        10 |  5350 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5351 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5352 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5353 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5354 | `			return SXERR_ABORT;` |
|         3 |  5355 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5356 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5357 | `		}` |
|         3 |  5358 | `		return SXRET_OK;` |
|         - |  5359 | `	}` |
|         - |  5360 | `	/* Jump the static keyword */` |
|        13 |  5361 | `	nLine = pGen->pIn->nLine;` |
|        13 |  5362 | `	pGen->pIn++;` |
|         - |  5363 | `	/* Extract the enclosing function if any */` |
|        13 |  5364 | `	pBlock = pGen->pCurrent;` |
|        23 |  5365 | `	while( pBlock ){` |
|        23 |  5366 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|        13 |  5367 | `			break;` |
|         - |  5368 | `		}` |
|         - |  5369 | `		/* Point to the upper block */` |
|        13 |  5370 | `		pBlock = pBlock->pParent;` |
|         3 |  5371 | `	}` |
|        13 |  5372 | `	if( pBlock == 0 ){` |
|         - |  5373 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5374 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5375 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5376 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5377 | `				return SXERR_ABORT;` |
|         - |  5378 | `			}` |
|       ! 0 |  5379 | `			goto Synchronize;` |
|         - |  5380 | `		}` |
|         - |  5381 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5382 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5383 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5384 | `			return SXERR_ABORT;` |
|       ! 0 |  5385 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5386 | `			/* Emit the POP instruction */` |
|       ! 0 |  5387 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5388 | `		}` |
|       ! 0 |  5389 | `		return SXRET_OK;` |
|         - |  5390 | `	}` |
|        13 |  5391 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5392 | `	/* Make sure we are dealing with a valid statement */` |
|        13 |  5393 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|         8 |  5394 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5395 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5396 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5397 | `				return SXERR_ABORT;` |
|         - |  5398 | `			}` |
|         3 |  5399 | `			goto Synchronize;` |
|         - |  5400 | `	}` |
|        10 |  5401 | `	pGen->pIn++;` |
|         - |  5402 | `	/* Extract variable name */` |
|        10 |  5403 | `	pName = &pGen->pIn->sData;` |
|        10 |  5404 | `	pGen->pIn++; /* Jump the var name */` |
|        10 |  5405 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5406 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5407 | `		goto Synchronize;` |
|         - |  5408 | `	}` |
|         - |  5409 | `	/* Initialize the structure describing the static variable */` |
|        10 |  5410 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        10 |  5411 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5412 | `	/* Duplicate variable name */` |
|        10 |  5413 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        10 |  5414 | `	if( zDup == 0 ){` |
|       ! 0 |  5415 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5416 | `		return SXERR_ABORT;` |
|         - |  5417 | `	}` |
|        10 |  5418 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5419 | `	/* Check if we have an expression to compile */` |
|        10 |  5420 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5421 | `		SySet *pInstrContainer;` |
|         - |  5422 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5423 | `		 * Static variable can take any complex expression including function` |
|         - |  5424 | `		 * call as their initialization value.` |
|         - |  5425 | `		 * Example:` |
|         - |  5426 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5427 | `		 */` |
|        10 |  5428 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5429 | `		/* Swap bytecode container */` |
|        10 |  5430 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        10 |  5431 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5432 | `		/* Compile the expression */` |
|        10 |  5433 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5434 | `		/* Emit the done instruction */` |
|        10 |  5435 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5436 | `		/* Restore default bytecode container */` |
|        10 |  5437 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         4 |  5438 | `	}` |
|         - |  5439 | `	/* Finally save the compiled static variable in the appropriate container */` |
|        10 |  5440 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|        10 |  5441 | `	return SXRET_OK;` |
|         1 |  5442 | `Synchronize:` |
|         - |  5443 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5444 | `	 * statement.` |
|         - |  5445 | `	 */` |
|         5 |  5446 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5447 | `		pGen->pIn++;` |
|         1 |  5448 | `	}` |
|         3 |  5449 | `	return SXRET_OK;` |
|         9 |  5450 | `}` |
|         - |  5451 | `/*` |
|         - |  5452 | ` * Compile the var statement.` |
|         - |  5453 | ` * Symisc Extension:` |
|         - |  5454 | ` *      var statement can be used outside of a class definition.` |
|         - |  5455 | ` */` |
|         4 |  5456 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5457 | `{` |
|         - |  5458 | `	sxu32 nLine;` |
|         - |  5459 | `	sxi32 rc;` |
|         5 |  5460 | `	nLine = pGen->pIn->nLine;` |
|         - |  5461 | `	/* Jump the 'var' keyword */` |
|         5 |  5462 | `	pGen->pIn++;` |
|         5 |  5463 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5464 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5465 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5466 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5467 | `			pGen->pIn++;` |
|       ! 0 |  5468 | `		}` |
|       ! 0 |  5469 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5470 | `			return SXERR_ABORT;` |
|         - |  5471 | `		}` |
|       ! 0 |  5472 | `	}else{` |
|         - |  5473 | `		/* Compile the expression */` |
|         5 |  5474 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5475 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5476 | `			return SXERR_ABORT;` |
|         5 |  5477 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5478 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5479 | `		}` |
|         - |  5480 | `	}` |
|         5 |  5481 | `	return SXRET_OK;` |
|         3 |  5482 | `}` |
|         - |  5483 | `/*` |
|         - |  5484 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5485 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5486 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5487 | ` */` |
|         - |  5488 | `/*` |
|         - |  5489 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5490 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5491 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5492 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5493 | ` *` |
|         - |  5494 | ` * Resolution order:` |
|         - |  5495 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5496 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5497 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5498 | ` *` |
|         - |  5499 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5500 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5501 | ` * Returns the (possibly new) literal index.` |
|         - |  5502 | ` */` |
|   4988926 |  5503 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5504 | `{` |
|         - |  5505 | `	ph7_value *pLit;` |
|         - |  5506 | `	const char *zLit;` |
|         - |  5507 | `	SyString sQualified;` |
|         - |  5508 | `	sxu32 nLit;` |
|         - |  5509 | `	sxu32 k;` |
|         - |  5510 | `	sxu32 nNewIdx;` |
|         - |  5511 | `	int hasNsSep;` |
|         - |  5512 | `	SyHashEntry *pImport;` |
|         - |  5513 | `	ph7_value *pNew;` |
|   4988931 |  5514 | `	if( pFromImport ){` |
|   3937103 |  5515 | `		*pFromImport = 0;` |
|   1968549 |  5516 | `	}` |
|   4988931 |  5517 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   4988931 |  5518 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5519 | `		return nOrigIdx;` |
|         - |  5520 | `	}` |
|   4988931 |  5521 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   4988931 |  5522 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5523 | `	/* Skip if already qualified (contains backslash) */` |
|   4988931 |  5524 | `	hasNsSep = 0;` |
|  61077477 |  5525 | `	for( k = 0; k < nLit; k++ ){` |
|  56088559 |  5526 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  28044278 |  5527 | `	}` |
|   4988931 |  5528 | `	if( hasNsSep ){` |
|        10 |  5529 | `		return nOrigIdx;` |
|         - |  5530 | `	}` |
|         - |  5531 | `	/* Check use imports first (works even outside namespaces) */` |
|   4988923 |  5532 | `	SyBlobReset(&pGen->sWorker);` |
|   4988923 |  5533 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   4988923 |  5534 | `	if( pImport ){` |
|        41 |  5535 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5536 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5537 | `		if( pFromImport ){` |
|        18 |  5538 | `			*pFromImport = 1;` |
|         8 |  5539 | `		}` |
|        23 |  5540 | `	}else{` |
|   4988887 |  5541 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   4988797 |  5542 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5543 | `		}` |
|         - |  5544 | `		/* Prepend current namespace */` |
|        95 |  5545 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        95 |  5546 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|        95 |  5547 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5548 | `	}` |
|         - |  5549 | `	/* Look up or create a new literal for the qualified name */` |
|       131 |  5550 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       131 |  5551 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        57 |  5552 | `		return nNewIdx; /* Already interned */` |
|         - |  5553 | `	}` |
|        79 |  5554 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        79 |  5555 | `	if( pNew == 0 ){` |
|       ! 0 |  5556 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5557 | `	}` |
|        79 |  5558 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        79 |  5559 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        79 |  5560 | `	return nNewIdx;` |
|   2494468 |  5561 | `}` |
|         - |  5562 | `/*` |
|         - |  5563 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5564 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5565 | ` */` |
|    423096 |  5566 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5567 | `{` |
|         - |  5568 | `	SyHashEntry *pImport;` |
|         - |  5569 | `	/* Check use imports first */` |
|    423101 |  5570 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    423101 |  5571 | `	if( pImport ){` |
|        20 |  5572 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        20 |  5573 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        20 |  5574 | `		return;` |
|         - |  5575 | `	}` |
|         - |  5576 | `	/* Prepend current namespace if active */` |
|    423085 |  5577 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         8 |  5578 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  5579 | `		SyBlobAppend(pOut,"\\",1);` |
|         3 |  5580 | `	}` |
|    423085 |  5581 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    211553 |  5582 | `}` |
|         - |  5583 | `/*` |
|         - |  5584 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5585 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5586 | ` * The caller must release pOut when done.` |
|         - |  5587 | ` */` |
|    443234 |  5588 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5589 | `{` |
|    443239 |  5590 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      4003 |  5591 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      4003 |  5592 | `		SyBlobAppend(pOut,"\\",1);` |
|      1999 |  5593 | `	}` |
|    443239 |  5594 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    443239 |  5595 | `}` |
|         - |  5596 | `/*` |
|         - |  5597 | ` * Compile a namespace statement` |
|         - |  5598 | ` * According to the PHP language reference manual` |
|         - |  5599 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5600 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5601 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5602 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5603 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5604 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5605 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5606 | ` *  programming world.` |
|         - |  5607 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5608 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5609 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5610 | ` *  classes/functions/constants.` |
|         - |  5611 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5612 | ` *  readability of source code.` |
|         - |  5613 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5614 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5615 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5616 | ` *       class MyClass {}` |
|         - |  5617 | ` *       function myfunction() {}` |
|         - |  5618 | ` *       const MYCONST = 1;` |
|         - |  5619 | ` *       $a = new MyClass;` |
|         - |  5620 | ` *       $c = new \my\name\MyClass;` |
|         - |  5621 | ` *       $a = strlen('hi');` |
|         - |  5622 | ` *       $d = namespace\MYCONST;` |
|         - |  5623 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5624 | ` *       echo constant($d);` |
|         - |  5625 | ` * NOTE` |
|         - |  5626 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5627 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5628 | ` */` |
|         - |  5629 | `/*` |
|         - |  5630 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5631 | ` */` |
|        14 |  5632 | `static const char * TokenTypeName(sxu32 nType)` |
|         3 |  5633 | `{` |
|        17 |  5634 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        10 |  5635 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        10 |  5636 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        10 |  5637 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        10 |  5638 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        10 |  5639 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5640 | `	return "token";` |
|        10 |  5641 | `}` |
|      4046 |  5642 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5643 | `{` |
|         - |  5644 | `	sxu32 nLine;` |
|         - |  5645 | `	sxi32 rc;` |
|      4051 |  5646 | `	nLine = pGen->pIn->nLine;` |
|      4051 |  5647 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5648 | `	/* Reset namespace and clear previous use imports */` |
|      4051 |  5649 | `	SyBlobReset(&pGen->sNamespace);` |
|      4051 |  5650 | `	SyHashRelease(&pGen->hUseImports);` |
|      4051 |  5651 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      4051 |  5652 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      4051 |  5653 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      4051 |  5654 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      4051 |  5655 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      4051 |  5656 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5657 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5658 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5659 | `		return SXRET_OK;` |
|         - |  5660 | `	}` |
|      4051 |  5661 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5662 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5663 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5664 | `		return SXRET_OK;` |
|         - |  5665 | `	}` |
|      4051 |  5666 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5667 | `		/* namespace { } — global namespace block */` |
|       ! 0 |  5668 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5669 | `		return SXRET_OK;` |
|         - |  5670 | `	}` |
|         - |  5671 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      8139 |  5672 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      4093 |  5673 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5674 | `			/* Append backslash separator */` |
|        27 |  5675 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        27 |  5676 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        11 |  5677 | `			}` |
|        16 |  5678 | `		}else{` |
|         - |  5679 | `			/* Append identifier */` |
|      4071 |  5680 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5681 | `		}` |
|      4093 |  5682 | `		pGen->pIn++;` |
|         5 |  5683 | `	}` |
|         - |  5684 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5685 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5686 | `	{` |
|      4051 |  5687 | `		char *zNsDup = 0;` |
|      4051 |  5688 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      6071 |  5689 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      4044 |  5690 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      2022 |  5691 | `		}` |
|      4051 |  5692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5693 | `	}` |
|      4051 |  5694 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5695 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5696 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5697 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5698 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5699 | `			return SXERR_ABORT;` |
|         - |  5700 | `		}` |
|         2 |  5701 | `	}` |
|      4051 |  5702 | `	return SXRET_OK;` |
|      2028 |  5703 | `}` |
|         - |  5704 | `/*` |
|         - |  5705 | ` * Compile the 'use' statement` |
|         - |  5706 | ` * According to the PHP language reference manual` |
|         - |  5707 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5708 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5709 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5710 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5711 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5712 | ` *  a function or constant is not supported.` |
|         - |  5713 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5714 | ` * NOTE` |
|         - |  5715 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5716 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5717 | ` */` |
|        72 |  5718 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5719 | `{` |
|         - |  5720 | `	sxu32 nLine;` |
|         - |  5721 | `	sxi32 rc;` |
|         - |  5722 | `	SyBlob sPath;` |
|         - |  5723 | `	SyString sAlias;` |
|         - |  5724 | `	SyToken *pLast;` |
|         - |  5725 | `	char *zDup;` |
|         - |  5726 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5727 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5728 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        77 |  5729 | `	nLine = pGen->pIn->nLine;` |
|        77 |  5730 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5731 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        77 |  5732 | `	iUseType = 0;` |
|        77 |  5733 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5734 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5735 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5736 | `			iUseType = 1;` |
|        16 |  5737 | `			pGen->pIn++;` |
|        23 |  5738 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5739 | `			iUseType = 2;` |
|        16 |  5740 | `			pGen->pIn++;` |
|         7 |  5741 | `		}` |
|        14 |  5742 | `	}` |
|         - |  5743 | `	/* Select target hash tables based on import type */` |
|        77 |  5744 | `	switch( iUseType ){` |
|         7 |  5745 | `		case 1:` |
|        16 |  5746 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5747 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5748 | `			break;` |
|         7 |  5749 | `		case 2:` |
|        16 |  5750 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5751 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5752 | `			break;` |
|        22 |  5753 | `		default:` |
|        49 |  5754 | `			pGenHash = &pGen->hUseImports;` |
|        49 |  5755 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        44 |  5756 | `			break;` |
|         - |  5757 | `	}` |
|        77 |  5758 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5759 | `	/* Process one or more use declarations separated by commas */` |
|        37 |  5760 | `	for(;;){` |
|        79 |  5761 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5762 | `			break;` |
|         - |  5763 | `		}` |
|        79 |  5764 | `		SyBlobReset(&sPath);` |
|        79 |  5765 | `		pLast = 0;` |
|         - |  5766 | `		/* Collect the full namespace path */` |
|       269 |  5767 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       195 |  5768 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       135 |  5769 | `				pLast = pGen->pIn;` |
|       135 |  5770 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        65 |  5771 | `					SyBlobAppend(&sPath,"\\",1);` |
|        30 |  5772 | `				}` |
|       135 |  5773 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        65 |  5774 | `			}` |
|       195 |  5775 | `			pGen->pIn++;` |
|         5 |  5776 | `		}` |
|        79 |  5777 | `		if( pLast == 0 ){` |
|         - |  5778 | `			/* Empty path */` |
|         6 |  5779 | `			break;` |
|         - |  5780 | `		}` |
|         - |  5781 | `		/* Default alias is the last component of the path */` |
|        75 |  5782 | `		sAlias = pLast->sData;` |
|         - |  5783 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        70 |  5784 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        50 |  5785 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        23 |  5786 | `			pGen->pIn++; /* Jump 'as' */` |
|        23 |  5787 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        23 |  5788 | `				sAlias = pGen->pIn->sData;` |
|        23 |  5789 | `				pGen->pIn++;` |
|        10 |  5790 | `			}` |
|        10 |  5791 | `		}` |
|         - |  5792 | `		/* Check for duplicate import alias (per-type) */` |
|        75 |  5793 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  5794 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  5795 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  5796 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  5797 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5798 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  5799 | `				return SXERR_ABORT;` |
|         - |  5800 | `			}` |
|         2 |  5801 | `		}` |
|         - |  5802 | `		/* Register the import: alias -> FQN.` |
|         - |  5803 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  5804 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  5805 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       110 |  5806 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        70 |  5807 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        75 |  5808 | `		if( zDup ){` |
|        75 |  5809 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        75 |  5810 | `			if( pVmHash ){` |
|         - |  5811 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  5812 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        47 |  5813 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        47 |  5814 | `				if( zAliasDup ){` |
|        47 |  5815 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        21 |  5816 | `				}` |
|        21 |  5817 | `			}` |
|        75 |  5818 | `			if( iUseType == 2 ){` |
|         - |  5819 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  5820 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  5821 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  5822 | `				if( zAliasDup ){` |
|         - |  5823 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  5824 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  5825 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  5826 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  5827 | `					if( azPair ){` |
|        16 |  5828 | `						azPair[0] = zAliasDup;` |
|        16 |  5829 | `						azPair[1] = zDup;` |
|        16 |  5830 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  5831 | `					}` |
|         7 |  5832 | `				}` |
|         7 |  5833 | `			}` |
|        35 |  5834 | `		}` |
|         - |  5835 | `		/* Check for comma (multiple use declarations) */` |
|        75 |  5836 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  5837 | `			pGen->pIn++;` |
|         2 |  5838 | `		}else{` |
|        39 |  5839 | `			break;` |
|         - |  5840 | `		}` |
|         1 |  5841 | `	}` |
|        77 |  5842 | `	SyBlobRelease(&sPath);` |
|        77 |  5843 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  5844 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  5845 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  5846 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5847 | `			return SXERR_ABORT;` |
|         - |  5848 | `		}` |
|         1 |  5849 | `	}` |
|        77 |  5850 | `	return SXRET_OK;` |
|        41 |  5851 | `}` |
|         - |  5852 | `/*` |
|         - |  5853 | ` * Compile the stupid 'declare' language construct.` |
|         - |  5854 | ` *` |
|         - |  5855 | ` * According to the PHP language reference manual.` |
|         - |  5856 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  5857 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  5858 | ` *  declare (directive)` |
|         - |  5859 | ` *   statement` |
|         - |  5860 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  5861 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  5862 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  5863 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  5864 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  5865 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  5866 | ` * <?php` |
|         - |  5867 | ` * // these are the same:` |
|         - |  5868 | ` * // you can use this:` |
|         - |  5869 | ` * declare(ticks=1) {` |
|         - |  5870 | ` *   // entire script here` |
|         - |  5871 | ` * }` |
|         - |  5872 | ` * // or you can use this:` |
|         - |  5873 | ` * declare(ticks=1);` |
|         - |  5874 | ` * // entire script here` |
|         - |  5875 | ` * ?>` |
|         - |  5876 | ` *` |
|         - |  5877 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  5878 | ` */` |
|         - |  5879 | `/*` |
|         - |  5880 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  5881 | ` */` |
|        72 |  5882 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  5883 | `{` |
|       109 |  5884 | `	return SyStringLength(pName) == nWant` |
|        72 |  5885 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  5886 | `}` |
|         - |  5887 |  |
|        42 |  5888 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  5889 | `{` |
|        47 |  5890 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  5891 | `	SyToken *pBodyEnd = 0;` |
|         - |  5892 | `	SyToken *pBodyStart;` |
|         - |  5893 | `	SyToken *pCursor;` |
|         - |  5894 | `	int bHasStrictTypes;` |
|         - |  5895 | `	int bBlockForm;` |
|         - |  5896 | `	int bPlacementOk;` |
|         - |  5897 | `	sxi32 rc;` |
|        47 |  5898 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  5899 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         5 |  5900 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|         5 |  5901 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5902 | `			return SXERR_ABORT;` |
|         - |  5903 | `		}` |
|         5 |  5904 | `		goto Synchro;` |
|         - |  5905 | `	}` |
|        43 |  5906 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  5907 | `	pBodyStart = pGen->pIn;` |
|         - |  5908 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  5909 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  5910 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  5911 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  5912 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5913 | `			return SXERR_ABORT;` |
|         - |  5914 | `		}` |
|       ! 0 |  5915 | `		return SXRET_OK;` |
|         - |  5916 | `	}` |
|         - |  5917 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  5918 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  5919 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  5920 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  5921 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  5922 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5923 | `			return SXERR_ABORT;` |
|         - |  5924 | `		}` |
|       ! 0 |  5925 | `	}` |
|        43 |  5926 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  5927 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  5928 | `	bHasStrictTypes = 0;` |
|         - |  5929 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  5930 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  5931 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  5932 | `	pCursor = pBodyStart;` |
|        55 |  5933 | `	while( pCursor < pBodyEnd ){` |
|        51 |  5934 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  5935 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  5936 | `				bHasStrictTypes = 1;` |
|        39 |  5937 | `				break;` |
|         - |  5938 | `			}` |
|         2 |  5939 | `		}` |
|        14 |  5940 | `		pCursor++;` |
|         2 |  5941 | `	}` |
|        43 |  5942 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  5943 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5944 | `			"strict_types declaration must not use block mode");` |
|         3 |  5945 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  5946 | `		return SXRET_OK;` |
|         - |  5947 | `	}` |
|        41 |  5948 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  5949 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5950 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  5951 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  5952 | `		return SXRET_OK;` |
|         - |  5953 | `	}` |
|         - |  5954 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  5955 | `	pCursor = pBodyStart;` |
|        69 |  5956 | `	while( pCursor < pBodyEnd ){` |
|         - |  5957 | `		SyToken *pNameTok;` |
|         - |  5958 | `		SyToken *pEqTok;` |
|         - |  5959 | `		SyToken *pValTok;` |
|         - |  5960 | `		SyString *pDirName;` |
|         - |  5961 | `		int bIsStrict;` |
|         - |  5962 | `		int iStrictValue;` |
|        39 |  5963 | `		pNameTok = pCursor;` |
|        39 |  5964 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  5965 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5966 | `				"declare: Expecting a directive name");` |
|       ! 0 |  5967 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5968 | `			return SXRET_OK;` |
|         - |  5969 | `		}` |
|        39 |  5970 | `		pEqTok = pNameTok + 1;` |
|        39 |  5971 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  5972 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5973 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  5974 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5975 | `			return SXRET_OK;` |
|         - |  5976 | `		}` |
|        39 |  5977 | `		pValTok = pEqTok + 1;` |
|        39 |  5978 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  5979 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5980 | `				"declare: Expecting value after '='");` |
|       ! 0 |  5981 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5982 | `			return SXRET_OK;` |
|         - |  5983 | `		}` |
|        39 |  5984 | `		pDirName = &pNameTok->sData;` |
|        39 |  5985 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  5986 | `		if( bIsStrict ){` |
|         - |  5987 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  5988 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  5989 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  5990 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5991 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  5992 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5993 | `				return SXRET_OK;` |
|         - |  5994 | `			}` |
|        35 |  5995 | `			iStrictValue = -1;` |
|        35 |  5996 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  5997 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  5998 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  5999 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6000 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6001 | `			}` |
|        35 |  6002 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6003 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6004 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6005 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6006 | `				return SXRET_OK;` |
|         - |  6007 | `			}` |
|        32 |  6008 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6009 | `		}else{` |
|         - |  6010 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6011 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6012 | `			 * behavior don't regress. */` |
|         8 |  6013 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6014 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6015 | `				ph7_lib_version()` |
|         - |  6016 | `				);` |
|         - |  6017 | `		}` |
|        36 |  6018 | `		pCursor = pValTok + 1;` |
|         - |  6019 | `		/* Consume separating comma (or end). */` |
|        36 |  6020 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6021 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6022 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6023 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6024 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6025 | `				return SXRET_OK;` |
|         - |  6026 | `			}` |
|         3 |  6027 | `			pCursor++;` |
|         1 |  6028 | `		}` |
|         4 |  6029 | `	}` |
|         - |  6030 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6031 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6032 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        34 |  6033 | `	return SXRET_OK;` |
|         2 |  6034 | `Synchro:` |
|         - |  6035 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        15 |  6036 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        11 |  6037 | `		pGen->pIn++;` |
|         1 |  6038 | `	}` |
|         5 |  6039 | `	return SXRET_OK;` |
|        26 |  6040 | `}` |
|         - |  6041 | `/*` |
|         - |  6042 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6043 | ` * as follows:` |
|         - |  6044 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6045 | ` * {` |
|         - |  6046 | ` *   return "Making a cup of $type.\n";` |
|         - |  6047 | ` * }` |
|         - |  6048 | ` * Symisc eXtension.` |
|         - |  6049 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6050 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6051 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6052 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6053 | ` *      {` |
|         - |  6054 | ` *       var_dump($a);` |
|         - |  6055 | ` *      }` |
|         - |  6056 | ` *     //call test without args` |
|         - |  6057 | ` *      test();` |
|         - |  6058 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6059 | ` *      Example:` |
|         - |  6060 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6061 | ` * 3 -) Function overloading!!` |
|         - |  6062 | ` *      Example:` |
|         - |  6063 | ` *      function foo($a) {` |
|         - |  6064 | ` *   	  return $a.PHP_EOL;` |
|         - |  6065 | ` *	    }` |
|         - |  6066 | ` *	    function foo($a, $b) {` |
|         - |  6067 | ` *   	  return $a + $b;` |
|         - |  6068 | ` *	    }` |
|         - |  6069 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6070 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6071 | ` *      // Same arg` |
|         - |  6072 | ` *	   function foo(string $a)` |
|         - |  6073 | ` *	   {` |
|         - |  6074 | ` *	     echo "a is a string\n";` |
|         - |  6075 | ` *	     var_dump($a);` |
|         - |  6076 | ` *	   }` |
|         - |  6077 | ` *	  function foo(int $a)` |
|         - |  6078 | ` *	  {` |
|         - |  6079 | ` *	    echo "a is integer\n";` |
|         - |  6080 | ` *	    var_dump($a);` |
|         - |  6081 | ` *	  }` |
|         - |  6082 | ` *	  function foo(array $a)` |
|         - |  6083 | ` *	  {` |
|         - |  6084 | ` * 	    echo "a is an array\n";` |
|         - |  6085 | ` * 	    var_dump($a);` |
|         - |  6086 | ` *	  }` |
|         - |  6087 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6088 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6089 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6090 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6091 | ` * introduced by the PH7 engine.` |
|         - |  6092 | ` */` |
|    469018 |  6093 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6094 | `{` |
|         - |  6095 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6096 | `	SySet *pInstrContainer;` |
|         - |  6097 | `	sxi32 rc;` |
|         - |  6098 | `	/* Swap token stream */` |
|    469023 |  6099 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    469023 |  6100 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    469023 |  6101 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6102 | `	/* Compile the expression holding the argument value */` |
|    469023 |  6103 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6104 | `	/* Emit the done instruction */` |
|    469023 |  6105 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    469023 |  6106 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    469023 |  6107 | `	RE_SWAP_DELIMITER(pGen);` |
|    469023 |  6108 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6109 | `		return SXERR_ABORT;` |
|         - |  6110 | `	}` |
|    469023 |  6111 | `	return SXRET_OK;` |
|    234514 |  6112 | `}` |
|         - |  6113 | `/*` |
|         - |  6114 | ` * Collect function arguments one after one.` |
|         - |  6115 | ` * According to the PHP language reference manual.` |
|         - |  6116 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6117 | ` * list of expressions.` |
|         - |  6118 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6119 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6120 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6121 | ` * for more information.` |
|         - |  6122 | ` * Example #1 Passing arrays to functions` |
|         - |  6123 | ` * <?php` |
|         - |  6124 | ` * function takes_array($input)` |
|         - |  6125 | ` * {` |
|         - |  6126 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6127 | ` * }` |
|         - |  6128 | ` * ?>` |
|         - |  6129 | ` * Making arguments be passed by reference` |
|         - |  6130 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6131 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6132 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6133 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6134 | ` * to the argument name in the function definition:` |
|         - |  6135 | ` * Example #2 Passing function parameters by reference` |
|         - |  6136 | ` * <?php` |
|         - |  6137 | ` * function add_some_extra(&$string)` |
|         - |  6138 | ` * {` |
|         - |  6139 | ` *   $string .= 'and something extra.';` |
|         - |  6140 | ` * }` |
|         - |  6141 | ` * $str = 'This is a string, ';` |
|         - |  6142 | ` * add_some_extra($str);` |
|         - |  6143 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6144 | ` * ?>` |
|         - |  6145 | ` *` |
|         - |  6146 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6147 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6148 | ` * on these extension.` |
|         - |  6149 | ` */` |
|   1132684 |  6150 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6151 | `{` |
|         - |  6152 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6153 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6154 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6155 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6156 | `	sxi32 rc;` |
|         - |  6157 |  |
|   1132689 |  6158 | `	pIn = pGen->pIn;` |
|   1132689 |  6159 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6160 | `	/* Process arguments one after one */` |
|   1420774 |  6161 | `	for(;;){` |
|   2841553 |  6162 | `		if( pIn >= pEnd ){` |
|         - |  6163 | `			/* No more arguments to process */` |
|   1132673 |  6164 | `			break;` |
|         - |  6165 | `		}` |
|   1708885 |  6166 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   1708885 |  6167 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   1708885 |  6168 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   1708885 |  6169 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   1708885 |  6170 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6171 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6172 | `		 * first token inside the main token stream */` |
|   1708885 |  6173 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6174 | `			return SXERR_ABORT;` |
|         - |  6175 | `		}` |
|         - |  6176 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6177 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6178 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6179 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6180 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6181 | `		{` |
|   1708885 |  6182 | `			int bReadonly = 0, bVisSeen = 0;` |
|   1708885 |  6183 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   1708885 |  6184 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6185 | `			int nSetTok;` |
|         - |  6186 | `			sxi32 nSetVis;` |
|   1708885 |  6187 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6188 | `				bReadonly = 1;` |
|         3 |  6189 | `				pIn++;` |
|         1 |  6190 | `			}` |
|   1708885 |  6191 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   1708885 |  6192 | `			if( nSetVis ){` |
|         - |  6193 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6194 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6195 | `				bVisSeen = 1;` |
|         3 |  6196 | `				pIn += nSetTok;` |
|         3 |  6197 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6198 | `					bReadonly = 1;` |
|       ! 0 |  6199 | `					pIn++;` |
|         1 |  6200 | `				}` |
|   1708884 |  6201 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     91019 |  6202 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     91019 |  6203 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6204 | `					bVisSeen = 1;` |
|        89 |  6205 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6206 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6207 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6208 | `					pIn++;` |
|        89 |  6209 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6210 | `					if( nSetVis ){` |
|         - |  6211 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6212 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6213 | `						pIn += nSetTok;` |
|         1 |  6214 | `					}` |
|        89 |  6215 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6216 | `						bReadonly = 1;` |
|        18 |  6217 | `						pIn++;` |
|         7 |  6218 | `					}` |
|        42 |  6219 | `				}` |
|     45507 |  6220 | `			}` |
|   1708885 |  6221 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6222 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   1708883 |  6223 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6224 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6225 | `			}` |
|   1708885 |  6226 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6227 | `				if( !bCtorCtx ){` |
|         6 |  6228 | `					if( bAbstractCtx ){` |
|         3 |  6229 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6230 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6231 | `					}else{` |
|         3 |  6232 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6233 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6234 | `					}` |
|         6 |  6235 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6236 | `						return SXERR_ABORT;` |
|         - |  6237 | `					}` |
|         6 |  6238 | `					return SXERR_SYNTAX;` |
|         - |  6239 | `				}` |
|        89 |  6240 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6241 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6242 | `				if( bReadonly ){` |
|        20 |  6243 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6244 | `				}` |
|        42 |  6245 | `			}` |
|         - |  6246 | `		}` |
|         - |  6247 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   1708876 |  6248 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|    929612 |  6249 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    144426 |  6250 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    118717 |  6251 | `			sxu32 nLineLocal = pIn->nLine;` |
|    118717 |  6252 | `			sxi32 iTFlags = 0;` |
|    118717 |  6253 | `			pGen->pIn = pIn;` |
|    118717 |  6254 | `			rc = GenStateParseUnionTypeDecl(` |
|     59356 |  6255 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     59356 |  6256 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6257 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6258 | `				/* bAllowVoid */ 0,` |
|     59356 |  6259 | `						nLineLocal);` |
|    118717 |  6260 | `			pIn = pGen->pIn;` |
|    118717 |  6261 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6262 | `				return SXERR_ABORT;` |
|    118717 |  6263 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6264 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6265 | `				return SXERR_SYNTAX;` |
|    118715 |  6266 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6267 | `				if( pIn < pEnd ){` |
|        15 |  6268 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6269 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6270 | `						&pIn->sData);` |
|         7 |  6271 | `				}else{` |
|       ! 0 |  6272 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6273 | `						"syntax error, unexpected end of file");` |
|         - |  6274 | `				}` |
|        11 |  6275 | `				return SXERR_SYNTAX;` |
|         - |  6276 | `			}` |
|    118707 |  6277 | `			sArg.iFlags \|= iTFlags;` |
|     59351 |  6278 | `		}` |
|   1708871 |  6279 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6280 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6281 | `			return rc;` |
|         - |  6282 | `		}` |
|   1708871 |  6283 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6284 | `			/* Pass by reference,record that */` |
|     11867 |  6285 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     11867 |  6286 | `			pIn++;` |
|      5931 |  6287 | `		}` |
|   1708871 |  6288 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6289 | `			/* Variadic parameter: ...$args */` |
|     19809 |  6290 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     19809 |  6291 | `			pIn++;` |
|      9902 |  6292 | `		}` |
|   1708871 |  6293 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6294 | `			/* Invalid argument */` |
|       ! 0 |  6295 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6296 | `			return rc;` |
|         - |  6297 | `		}` |
|   1708871 |  6298 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6299 | `		/* Copy argument name */` |
|   1708871 |  6300 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   1708871 |  6301 | `		if( zDup == 0 ){` |
|       ! 0 |  6302 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6303 | `			return SXERR_ABORT;` |
|         - |  6304 | `		}` |
|   1708871 |  6305 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   1708871 |  6306 | `		pIn++;` |
|   1708871 |  6307 | `		if( pIn < pEnd ){` |
|    883637 |  6308 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6309 | `				SyToken *pDefend;` |
|    469025 |  6310 | `				sxi32 iNest = 0;` |
|    469025 |  6311 | `				pIn++; /* Jump the equal sign */` |
|    469025 |  6312 | `				pDefend = pIn;` |
|         - |  6313 | `				/* Process the default value associated with this argument */` |
|    989291 |  6314 | `				while( pDefend < pEnd ){` |
|    681861 |  6315 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    161595 |  6316 | `						break;` |
|         - |  6317 | `					}` |
|    520271 |  6318 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6319 | `						/* Increment nesting level */` |
|     27593 |  6320 | `						iNest++;` |
|    506477 |  6321 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6322 | `						/* Decrement nesting level */` |
|     27593 |  6323 | `						iNest--;` |
|     13794 |  6324 | `					}` |
|    520271 |  6325 | `					pDefend++;` |
|         5 |  6326 | `				}` |
|    469025 |  6327 | `				if( pIn >= pDefend ){` |
|         3 |  6328 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6329 | `					return rc;` |
|         - |  6330 | `				}` |
|         - |  6331 | `				/* Process default value */` |
|    469023 |  6332 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    469023 |  6333 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6334 | `					return rc;` |
|         - |  6335 | `				}` |
|         - |  6336 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6337 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6338 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6339 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6340 | `				 * arg-type check lets null through. */` |
|    469018 |  6341 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    260142 |  6342 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    260139 |  6343 | `					&& &pIn[1] == pDefend` |
|     47319 |  6344 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     35482 |  6345 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     21681 |  6346 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15771 |  6347 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6348 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6349 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6350 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6351 | `					 * already up at this point). */` |
|         - |  6352 | `					{` |
|     15771 |  6353 | `						const char *zSep = "";` |
|     15771 |  6354 | `						SyString sCls = { "", 0 };` |
|     15771 |  6355 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15765 |  6356 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15765 |  6357 | `							zSep = "::";` |
|      7880 |  6358 | `						}` |
|     23654 |  6359 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6360 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7883 |  6361 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6362 | `					}` |
|      7883 |  6363 | `				}` |
|         - |  6364 | `				/* Point beyond the default value */` |
|    469023 |  6365 | `				pIn = pDefend;` |
|    234509 |  6366 | `			}` |
|    883635 |  6367 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6368 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6369 | `				return rc;` |
|         - |  6370 | `			}` |
|    883635 |  6371 | `			pIn++; /* Jump the trailing comma */` |
|    441815 |  6372 | `		}` |
|         - |  6373 | `		/* Append argument signature */` |
|   1708869 |  6374 | `		if( sArg.nType > 0 ){` |
|    118645 |  6375 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6376 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     27665 |  6377 | `				int marker = 'o';` |
|     27665 |  6378 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     27665 |  6379 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13835 |  6380 | `			}else{` |
|         - |  6381 | `				int c;` |
|     90985 |  6382 | `				c = 'n'; /* cc warning */` |
|         - |  6383 | `				/* Type leading character */` |
|     90985 |  6384 | `				switch(sArg.nType){` |
|      5916 |  6385 | `				case MEMOBJ_HASHMAP:` |
|         - |  6386 | `					/* Hashmap aka 'array' */` |
|     11837 |  6387 | `					c = 'h';` |
|     11837 |  6388 | `					break;` |
|      9970 |  6389 | `				case MEMOBJ_INT:` |
|         - |  6390 | `					/* Integer */` |
|     19945 |  6391 | `					c = 'i';` |
|     19945 |  6392 | `					break;` |
|         2 |  6393 | `				case MEMOBJ_BOOL:` |
|         - |  6394 | `					/* Bool */` |
|         5 |  6395 | `					c = 'b';` |
|         5 |  6396 | `					break;` |
|         5 |  6397 | `				case MEMOBJ_REAL:` |
|         - |  6398 | `					/* Float */` |
|        12 |  6399 | `					c = 'f';` |
|        12 |  6400 | `					break;` |
|     29589 |  6401 | `				case MEMOBJ_STRING:` |
|         - |  6402 | `					/* String */` |
|     59183 |  6403 | `					c = 's';` |
|     59183 |  6404 | `					break;` |
|         7 |  6405 | `				case MEMOBJ_OBJ:` |
|         - |  6406 | `					/* Object */` |
|        16 |  6407 | `					c = 'o';` |
|        14 |  6408 | `					break;` |
|         1 |  6409 | `				default:` |
|         2 |  6410 | `					break;` |
|         - |  6411 | `				}` |
|     90985 |  6412 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6413 | `			}` |
|     59325 |  6414 | `		}else{` |
|         - |  6415 | `			/* No type is associated with this parameter which mean` |
|         - |  6416 | `			 * that this function is not condidate for overloading.` |
|         - |  6417 | `			 */` |
|   1590229 |  6418 | `			SyBlobRelease(&sSig);` |
|         - |  6419 | `		}` |
|         - |  6420 | `		/* Save in the argument set */` |
|   1708869 |  6421 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6422 | `	}` |
|   1132673 |  6423 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6424 | `		/* Save function signature */` |
|     87053 |  6425 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     43524 |  6426 | `	}` |
|   1132673 |  6427 | `	return SXRET_OK;` |
|    566347 |  6428 | `}` |
|         - |  6429 | `/*` |
|         - |  6430 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6431 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6432 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6433 | ` */` |
|     35502 |  6434 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6435 | `{` |
|     35507 |  6436 | `	sxi32 iParen = 0;` |
|     35507 |  6437 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6438 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6439 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6440 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    157833 |  6441 | `	while( pIn < pEnd ){` |
|    157833 |  6442 | `		sxu32 t = pIn->nType;` |
|    157833 |  6443 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    153839 |  6444 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    106505 |  6445 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     86763 |  6446 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    122331 |  6447 | `		pIn++;` |
|         5 |  6448 | `	}` |
|     19747 |  6449 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6450 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6451 | `	{` |
|     19747 |  6452 | `		sxi32 d = 0;` |
|    784485 |  6453 | `		while( pIn < pEnd ){` |
|    784485 |  6454 | `			sxu32 t = pIn->nType;` |
|    784485 |  6455 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    752919 |  6456 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    764743 |  6457 | `			pIn++;` |
|         5 |  6458 | `		}` |
|         - |  6459 | `	}` |
|     19747 |  6460 | `	return pIn;` |
|     17756 |  6461 | `}` |
|         - |  6462 | `/*` |
|         - |  6463 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6464 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6465 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6466 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6467 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6468 | ` * detached-mini-program path untouched.` |
|         - |  6469 | ` */` |
|         - |  6470 | `/*` |
|         - |  6471 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6472 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6473 | ` * mixed, object.` |
|         - |  6474 | ` */` |
|     11848 |  6475 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6476 | `{` |
|         - |  6477 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6478 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6479 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6480 | `	};` |
|         - |  6481 | `	sxu32 i;` |
|     11853 |  6482 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6483 | `		zName++;` |
|       ! 0 |  6484 | `		nName--;` |
|       ! 0 |  6485 | `	}` |
|     11885 |  6486 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11881 |  6487 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11849 |  6488 | `			return 1;` |
|         - |  6489 | `		}` |
|        17 |  6490 | `	}` |
|         5 |  6491 | `	return 0;` |
|      5929 |  6492 | `}` |
|         - |  6493 | `/*` |
|         - |  6494 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6495 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6496 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6497 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6498 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6499 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6500 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6501 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6502 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6503 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6504 | ` */` |
|     11846 |  6505 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6506 | `{` |
|     11851 |  6507 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6508 | ``		return 1; /* bare `object` */`` |
|         - |  6509 | `	}` |
|     11851 |  6510 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6511 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6512 | `	}` |
|     11849 |  6513 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11845 |  6514 | `		return 1;` |
|         - |  6515 | `	}` |
|         - |  6516 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6517 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6518 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6519 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6520 | `	{` |
|         - |  6521 | `		SyBlob sFQN;` |
|         - |  6522 | `		int bOk;` |
|         5 |  6523 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         5 |  6524 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|         5 |  6525 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|         5 |  6526 | `		SyBlobRelease(&sFQN);` |
|         5 |  6527 | `		return bOk;` |
|         - |  6528 | `	}` |
|      5928 |  6529 | `}` |
|         - |  6530 | `/*` |
|         - |  6531 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6532 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6533 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6534 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6535 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6536 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6537 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6538 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6539 | ` */` |
|     12084 |  6540 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6541 | `{` |
|     12089 |  6542 | `	int bOk = 0;` |
|         - |  6543 | `	sxu32 nLine;` |
|         - |  6544 | `	sxi32 rc;` |
|     12089 |  6545 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       243 |  6546 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6547 | `	}` |
|     11851 |  6548 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6549 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6550 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6551 | `		sxu32 i,j;` |
|       ! 0 |  6552 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6553 | `			int bGroupOk;` |
|       ! 0 |  6554 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6555 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6556 | `			}` |
|       ! 0 |  6557 | `			bGroupOk = 1;` |
|       ! 0 |  6558 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6559 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6560 | `					bGroupOk = 0;` |
|       ! 0 |  6561 | `					break;` |
|         - |  6562 | `				}` |
|       ! 0 |  6563 | `			}` |
|       ! 0 |  6564 | `			bOk = bGroupOk;` |
|       ! 0 |  6565 | `		}` |
|       ! 0 |  6566 | `	}else{` |
|     11851 |  6567 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6568 | `	}` |
|     11851 |  6569 | `	if( bOk ){` |
|     11849 |  6570 | `		return SXRET_OK;` |
|         - |  6571 | `	}` |
|         - |  6572 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6573 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6574 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6575 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6576 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6577 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6578 | `	{` |
|         3 |  6579 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6580 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6581 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6582 | `		}` |
|         3 |  6583 | `		if( sGiven.nByte < 1 ){` |
|         - |  6584 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6585 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6586 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6587 | `			const char *zScalar =` |
|       ! 0 |  6588 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6589 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6590 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6591 | `		}` |
|         3 |  6592 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6593 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6594 | `	}` |
|         3 |  6595 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      6047 |  6596 | `}` |
|   2627574 |  6597 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6598 | `{` |
|   2627579 |  6599 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2627579 |  6600 | `	SyToken *pEnd = pGen->pEnd;` |
|   2627579 |  6601 | `	sxi32 iDepth = 0;` |
|   2627579 |  6602 | `	int bStarted = 0;` |
| 115962887 |  6603 | `	while( pIn < pEnd ){` |
| 115962887 |  6604 | `		sxu32 t = pIn->nType;` |
| 115962887 |  6605 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 110643329 |  6606 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 105359609 |  6607 | `		if( t & PH7_TK_KEYWORD ){` |
|   7688273 |  6608 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   7688273 |  6609 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   7676189 |  6610 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6611 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   3820341 |  6612 | `		}` |
| 105312023 |  6613 | `		pIn++;` |
|         5 |  6614 | `	}` |
|   2615495 |  6615 | `	return FALSE;` |
|   1313792 |  6616 | `}` |
|         - |  6617 | `/*` |
|         - |  6618 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6619 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6620 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6621 | ` */` |
|   2627574 |  6622 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6623 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6624 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6625 | `	)` |
|         5 |  6626 | `{` |
|         - |  6627 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6628 | `	GenBlock *pBlock;` |
|         - |  6629 | `	sxu32 nGotoOfft;` |
|         - |  6630 | `	sxi32 rc;` |
|         - |  6631 | `	/* Attach the new function */` |
|   2627579 |  6632 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2627579 |  6633 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6634 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6635 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6636 | `		return SXERR_ABORT;` |
|         - |  6637 | `	}` |
|   2627579 |  6638 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6639 | `	/* Swap bytecode containers */` |
|   2627579 |  6640 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2627579 |  6641 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6642 | `	/* Emit constructor property promotion prologue:` |
|         - |  6643 | `	 *   $this->NAME = $NAME;` |
|         - |  6644 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6645 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6646 | `	{` |
|   2627579 |  6647 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6648 | `		sxu32 i;` |
|   4281157 |  6649 | `		for( i = 0; i < nArg; i++ ){` |
|   1653583 |  6650 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6651 | `			char *zSrc;` |
|         - |  6652 | `			sxu32 nSrc,nName;` |
|         - |  6653 | `			SySet sToken;` |
|         - |  6654 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6655 | `			sxi32 rcPromote;` |
|   1653583 |  6656 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1653509 |  6657 | `				continue;` |
|         - |  6658 | `			}` |
|         - |  6659 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6660 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6661 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6662 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6663 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6664 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6665 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6666 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6667 | `			if( zSrc == 0 ){` |
|       ! 0 |  6668 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6669 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6670 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6671 | `				return SXERR_ABORT;` |
|         - |  6672 | `			}` |
|         - |  6673 | `			{` |
|        79 |  6674 | `				char *z = zSrc;` |
|        79 |  6675 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6676 | `				z += sizeof("$this->")-1;` |
|        79 |  6677 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6678 | `				z += nName;` |
|        79 |  6679 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6680 | `				z += sizeof(" = $")-1;` |
|        79 |  6681 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6682 | `				z += nName;` |
|        79 |  6683 | `				*z = 0;` |
|         - |  6684 | `			}` |
|        79 |  6685 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6686 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6687 | `			pTmpIn = pGen->pIn;` |
|        79 |  6688 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6689 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6690 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6691 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6692 | `			pGen->pIn = pTmpIn;` |
|        79 |  6693 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6694 | `			SySetRelease(&sToken);` |
|        79 |  6695 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6696 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6697 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6698 | `				return SXERR_ABORT;` |
|         - |  6699 | `			}` |
|         - |  6700 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6701 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6702 | `		}` |
|         - |  6703 | `	}` |
|         - |  6704 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6705 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6706 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6707 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6708 | `	{` |
|   2627579 |  6709 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2627579 |  6710 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6711 | `		/* Compile the body */` |
|   2627579 |  6712 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2627579 |  6713 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6714 | `	}` |
|         - |  6715 | `	/* Fix exception jumps now the destination is resolved */` |
|   2627579 |  6716 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6717 | `	/* Emit the final return if not yet done */` |
|   2627579 |  6718 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6719 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2627579 |  6720 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6721 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6722 | `	}` |
|   2627579 |  6723 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6724 | `	/* Restore the default container */` |
|   2627579 |  6725 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6726 | `	/* Leave function block */` |
|   2627579 |  6727 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2627579 |  6728 | `	if( rc == SXERR_ABORT ){` |
|         - |  6729 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6730 | `		return SXERR_ABORT;` |
|         - |  6731 | `	}` |
|         - |  6732 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6733 | `	{` |
|   2627579 |  6734 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6735 | `		sxu32 i;` |
|  71809447 |  6736 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  69193957 |  6737 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     12089 |  6738 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     12089 |  6739 | `				break;` |
|         - |  6740 | `			}` |
|  34590939 |  6741 | `		}` |
|         - |  6742 | `	}` |
|   2627579 |  6743 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6744 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     12089 |  6745 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6746 | `			return SXERR_ABORT;` |
|         - |  6747 | `		}` |
|      6042 |  6748 | `	}` |
|         - |  6749 | `	/* All done, function body compiled */` |
|   2627579 |  6750 | `	return SXRET_OK;` |
|   1313792 |  6751 | `}` |
|         - |  6752 | `/*` |
|         - |  6753 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  6754 | ` * According to the PHP language reference manual.` |
|         - |  6755 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  6756 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  6757 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  6758 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  6759 | ` *  Functions need not be defined before they are referenced.` |
|         - |  6760 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  6761 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  6762 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  6763 | ` *  calls with over 32-64 recursion levels.` |
|         - |  6764 | ` *` |
|         - |  6765 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  6766 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  6767 | ` * on these extension.` |
|         - |  6768 | ` */` |
|         - |  6769 | `/*` |
|         - |  6770 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  6771 | ` */` |
|       570 |  6772 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  6773 | `{` |
|         - |  6774 | `	sxu32 i;` |
|      1611 |  6775 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  6776 | `		int a = zA[i], b = zB[i];` |
|      1381 |  6777 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  6778 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  6779 | `		if( a != b ) return a - b;` |
|       523 |  6780 | `	}` |
|       235 |  6781 | `	return 0;` |
|       290 |  6782 | `}` |
|         - |  6783 | `/*` |
|         - |  6784 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  6785 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  6786 | ` * (which are positive bit values stored in sxu32).` |
|         - |  6787 | ` */` |
|         - |  6788 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  6789 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  6790 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  6791 |  |
|         - |  6792 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  6793 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  6794 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  6795 |  |
|         - |  6796 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  6797 | `struct PhlTypeAtom {` |
|         - |  6798 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  6799 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  6800 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  6801 | `	sxu32 nCanon;` |
|         - |  6802 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  6803 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  6804 | `};` |
|         - |  6805 |  |
|         - |  6806 | `/*` |
|         - |  6807 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  6808 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  6809 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  6810 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  6811 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  6812 | ` * already be consumed by the caller.` |
|         - |  6813 | ` */` |
|    131712 |  6814 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  6815 | `{` |
|    131717 |  6816 | `	SyToken *pIn = pGen->pIn;` |
|    131717 |  6817 | `	SyZero(pOut, sizeof(*pOut));` |
|    131717 |  6818 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    131717 |  6819 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6820 | `		return SXERR_SYNTAX;` |
|         - |  6821 | `	}` |
|         - |  6822 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    131717 |  6823 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|         8 |  6824 | `		pIn++;` |
|         8 |  6825 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6826 | `			return SXERR_SYNTAX;` |
|         - |  6827 | `		}` |
|         3 |  6828 | `	}` |
|    131717 |  6829 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6830 | `		return SXERR_SYNTAX;` |
|         - |  6831 | `	}` |
|    131717 |  6832 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     91777 |  6833 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     91777 |  6834 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11869 |  6835 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     85845 |  6836 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        81 |  6837 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     79875 |  6838 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     20351 |  6839 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     69664 |  6840 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     59409 |  6841 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     29789 |  6842 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  6843 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        67 |  6844 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        27 |  6845 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        37 |  6846 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        13 |  6847 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        23 |  6848 | `			pOut->nType = SXU32_HIGH;` |
|        23 |  6849 | `			pOut->sClass = pIn->sData;` |
|        13 |  6850 | `		}else{` |
|         3 |  6851 | `			return SXERR_SYNTAX;` |
|         - |  6852 | `		}` |
|     91775 |  6853 | `		pIn++;` |
|     45890 |  6854 | `	}else{` |
|         - |  6855 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  6856 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     39945 |  6857 | `		SyString *pT = &pIn->sData;` |
|     39945 |  6858 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  6859 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  6860 | `			pIn++;` |
|     39930 |  6861 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  6862 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  6863 | `			pIn++;` |
|     39829 |  6864 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        26 |  6865 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        26 |  6866 | `			pIn++;` |
|        15 |  6867 | `		}else{` |
|         - |  6868 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     39721 |  6869 | `			SyToken *pFirst = pIn;` |
|     39721 |  6870 | `			SyToken *pLast = pIn;` |
|     39721 |  6871 | `			pOut->nType = SXU32_HIGH;` |
|     39721 |  6872 | `			pOut->sClass = pIn->sData;` |
|     39721 |  6873 | `			pIn++;` |
|     59577 |  6874 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     39724 |  6875 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  6876 | `				pLast = &pIn[1];` |
|         3 |  6877 | `				pIn += 2;` |
|         1 |  6878 | `			}` |
|     39721 |  6879 | `			if( pLast != pFirst ){` |
|         3 |  6880 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  6881 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  6882 | `				pOut->sClass.zString = zFirst;` |
|         3 |  6883 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  6884 | `			}` |
|         - |  6885 | `		}` |
|         - |  6886 | `	}` |
|    131715 |  6887 | `	pGen->pIn = pIn;` |
|    131715 |  6888 | `	return SXRET_OK;` |
|     65861 |  6889 | `}` |
|         - |  6890 |  |
|         - |  6891 | `/*` |
|         - |  6892 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  6893 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  6894 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  6895 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  6896 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  6897 | ` */` |
|    131534 |  6898 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  6899 | `{` |
|         - |  6900 | `	int i;` |
|    131539 |  6901 | `	int nNonNull = 0;` |
|    131539 |  6902 | `	int bAnyIntersection = 0;` |
|         - |  6903 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    131539 |  6904 | `	sxu32 nMaxGroup = 0;` |
|   4340627 |  6905 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    263225 |  6906 | `	for( i = 0; i < nAtoms; i++ ){` |
|    131691 |  6907 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    131661 |  6908 | `			nNonNull++;` |
|    131661 |  6909 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    131661 |  6910 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    131661 |  6911 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     65828 |  6912 | `			}` |
|     65828 |  6913 | `		}` |
|     65848 |  6914 | `	}` |
|    263173 |  6915 | `	for( i = 0; i < nAtoms; i++ ){` |
|    131663 |  6916 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  6917 | `			bAnyIntersection = 1;` |
|        29 |  6918 | `			break;` |
|         - |  6919 | `		}` |
|     65822 |  6920 | `	}` |
|    131539 |  6921 | `	if( bAnyIntersection ){` |
|         - |  6922 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  6923 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  6924 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  6925 | `		sxu32 g, nGroups = 0;` |
|        29 |  6926 | `		int bFirstGroup = 1;` |
|        59 |  6927 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  6928 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  6929 | `			int bFirstMember = 1;` |
|         - |  6930 | `			int bWrap;` |
|        35 |  6931 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  6932 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  6933 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  6934 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  6935 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  6936 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  6937 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  6938 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  6939 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  6940 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  6941 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  6942 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  6943 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  6944 | `				}else{` |
|         6 |  6945 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  6946 | `				}` |
|        59 |  6947 | `				bFirstMember = 0;` |
|        32 |  6948 | `			}` |
|        35 |  6949 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  6950 | `			bFirstGroup = 0;` |
|        20 |  6951 | `		}` |
|        29 |  6952 | `		if( bNullable ){` |
|       ! 0 |  6953 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  6954 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  6955 | `		}` |
|        83 |  6956 | `		return;` |
|         - |  6957 | `	}` |
|    131515 |  6958 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  6959 | `		/* Shorthand: ?T */` |
|       112 |  6960 | `		for( i = 0; i < nAtoms; i++ ){` |
|       112 |  6961 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       112 |  6962 | `			SyBlobAppend(pBlob, "?", 1);` |
|       112 |  6963 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        23 |  6964 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  6965 | `			}else{` |
|        92 |  6966 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  6967 | `			}` |
|       112 |  6968 | `			return;` |
|       ! 0 |  6969 | `		}` |
|       ! 0 |  6970 | `	}` |
|         - |  6971 | `	{` |
|    131407 |  6972 | `		int bFirst = 1;` |
|         - |  6973 | `		/* 1) Classes in declaration order */` |
|    262917 |  6974 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131515 |  6975 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     39671 |  6976 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     39671 |  6977 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     39671 |  6978 | `				bFirst = 0;` |
|     19833 |  6979 | `			}` |
|     65760 |  6980 | `		}` |
|         - |  6981 | `		/* 2) Built-ins in canonical order */` |
|         - |  6982 | `		{` |
|         - |  6983 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  6984 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  6985 | `			int k;` |
|    919819 |  6986 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1485723 |  6987 | `				for( i = 0; i < nAtoms; i++ ){` |
|    788953 |  6988 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     91647 |  6989 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     91647 |  6990 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     91647 |  6991 | `						bFirst = 0;` |
|     91647 |  6992 | `						break;` |
|         - |  6993 | `					}` |
|    348658 |  6994 | `				}` |
|    394211 |  6995 | `			}` |
|         - |  6996 | `		}` |
|         - |  6997 | `		/* 3) null suffix */` |
|    131407 |  6998 | `		if( bNullable ){` |
|        19 |  6999 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        19 |  7000 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7001 | `		}` |
|         - |  7002 | `	}` |
|     65772 |  7003 | `}` |
|         - |  7004 |  |
|         - |  7005 | `/*` |
|         - |  7006 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7007 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7008 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7009 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7010 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7011 | ` * whether it was parenthesized.` |
|         - |  7012 | ` *` |
|         - |  7013 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7014 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7015 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7016 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7017 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7018 | ` */` |
|    131686 |  7019 | `static sxi32 GenStateParsePart(` |
|         - |  7020 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7021 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7022 | `{` |
|         - |  7023 | `	sxi32 rc;` |
|    131691 |  7024 | `	int nMembers = 0;` |
|    131691 |  7025 | `	int bParen = 0;` |
|    131691 |  7026 | `	*pnMembers = 0;` |
|    131691 |  7027 | `	*pbParen = 0;` |
|    131691 |  7028 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7029 | `		bParen = 1;` |
|         9 |  7030 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7031 | `	}` |
|     65843 |  7032 | `	for(;;){` |
|    131717 |  7033 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7034 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7035 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7036 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7037 | `		}` |
|    131717 |  7038 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    131717 |  7039 | `		if( rc != SXRET_OK ){` |
|         3 |  7040 | `			return rc;` |
|         - |  7041 | `		}` |
|    131715 |  7042 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    131715 |  7043 | `		(*pnAtoms)++;` |
|    131715 |  7044 | `		nMembers++;` |
|         - |  7045 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    131715 |  7046 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7047 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7048 | `			if( pNext < pGen->pEnd` |
|        39 |  7049 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7050 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7051 | `				continue;` |
|         - |  7052 | `			}` |
|         4 |  7053 | `		}` |
|    131689 |  7054 | `		break;` |
|       ! 0 |  7055 | `	}` |
|    131689 |  7056 | `	if( bParen ){` |
|         9 |  7057 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7058 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7059 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7060 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7061 | `		}` |
|         9 |  7062 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7063 | `		if( nMembers < 2 ){` |
|       ! 0 |  7064 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7065 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7066 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7067 | `		}` |
|         3 |  7068 | `	}` |
|    131689 |  7069 | `	*pnMembers = nMembers;` |
|    131689 |  7070 | `	*pbParen = bParen;` |
|    131689 |  7071 | `	return SXRET_OK;` |
|     65848 |  7072 | `}` |
|         - |  7073 |  |
|         - |  7074 | `/*` |
|         - |  7075 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7076 | ` *` |
|         - |  7077 | ` * Outputs:` |
|         - |  7078 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7079 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7080 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7081 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7082 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7083 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7084 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7085 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7086 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7087 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7088 | ` *` |
|         - |  7089 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7090 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7091 | ` */` |
|    131550 |  7092 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7093 | `	ph7_gen_state *pGen,` |
|         - |  7094 | `	sxu32 *pnType,` |
|         - |  7095 | `	SyString *pClass,` |
|         - |  7096 | `	SySet *pAlts,` |
|         - |  7097 | `	sxi32 *piTypeFlags,` |
|         - |  7098 | `	SyString *pTypeText,` |
|         - |  7099 | `	int iNullableFlag,` |
|         - |  7100 | `	int iUnionFlag,` |
|         - |  7101 | `	int bAllowVoid,` |
|         - |  7102 | `	sxu32 nLine` |
|         5 |  7103 | `){` |
|         - |  7104 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    131555 |  7105 | `	int nAtoms = 0;` |
|    131555 |  7106 | `	int bShortNullable = 0;` |
|    131555 |  7107 | `	int bExplicitNull = 0;` |
|         - |  7108 | `	sxi32 rc;` |
|    131555 |  7109 | `	*pnType = 0;` |
|    131555 |  7110 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    131555 |  7111 | `	*piTypeFlags = 0;` |
|    131555 |  7112 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7113 |  |
|    131555 |  7114 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7115 | `		return SXRET_OK;` |
|         - |  7116 | `	}` |
|         - |  7117 | ``	/* Optional `?` shorthand prefix */`` |
|    131550 |  7118 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       101 |  7119 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       100 |  7120 | `		bShortNullable = 1;` |
|       100 |  7121 | `		pGen->pIn++;` |
|       100 |  7122 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7123 | `			return SXERR_SYNTAX;` |
|         - |  7124 | `		}` |
|        48 |  7125 | `	}` |
|         - |  7126 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7127 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7128 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7129 | `	{` |
|         - |  7130 | `		int nMembers, bParen;` |
|    131555 |  7131 | `		sxu32 iGroup = 0;` |
|    131555 |  7132 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    131555 |  7133 | `		if( rc != SXRET_OK ){` |
|         4 |  7134 | `			return rc;` |
|         - |  7135 | `		}` |
|         - |  7136 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7137 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7138 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7139 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7140 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    197531 |  7141 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    131762 |  7142 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7143 | `			if( bShortNullable ){` |
|         - |  7144 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7145 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7146 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7147 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7148 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7149 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7150 | `			}` |
|       141 |  7151 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7152 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7153 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7154 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7155 | `			}` |
|       141 |  7156 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7157 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7158 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7159 | `				return rc;` |
|         - |  7160 | `			}` |
|         5 |  7161 | `		}` |
|    131551 |  7162 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7163 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7164 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7165 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7166 | `		}` |
|         - |  7167 | `	}` |
|         - |  7168 | `	/* Validation pass.` |
|         - |  7169 | `	 *` |
|         - |  7170 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7171 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7172 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7173 | `	 */` |
|         - |  7174 | `	{` |
|         - |  7175 | `		int i, j;` |
|    131551 |  7176 | `		int bHasNonNull = 0;` |
|    131551 |  7177 | `		int bAnyIntersection = 0;` |
|         - |  7178 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7179 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7180 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4341023 |  7181 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    263259 |  7182 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131713 |  7183 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     65859 |  7184 | `		}` |
|    263203 |  7185 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131683 |  7186 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     65831 |  7187 | `		}` |
|         - |  7188 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7189 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    131551 |  7190 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7191 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7192 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7193 | `			return SXERR_SYNTAX;` |
|         - |  7194 | `		}` |
|    263245 |  7195 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7196 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7197 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7198 | ``			 * `true`/`false` in an intersection). */`` |
|    131711 |  7199 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7200 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7201 | `				if( bClassLike ){` |
|        53 |  7202 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7203 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7204 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7205 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7206 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7207 | `						bClassLike = 0;` |
|       ! 0 |  7208 | `					}` |
|        24 |  7209 | `				}` |
|        55 |  7210 | `				if( !bClassLike ){` |
|         - |  7211 | `					const char *zName; sxu32 nName;` |
|         3 |  7212 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7213 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7214 | `					}else{` |
|         3 |  7215 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7216 | `					}` |
|         4 |  7217 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7218 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7219 | `						(int)nName, zName);` |
|         3 |  7220 | `					return SXERR_SYNTAX;` |
|         - |  7221 | `				}` |
|        24 |  7222 | `			}` |
|    131709 |  7223 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7224 | `				if( nAtoms > 1 ){` |
|         3 |  7225 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7226 | `						"Void can only be used as a standalone type");` |
|         3 |  7227 | `					return SXERR_SYNTAX;` |
|         - |  7228 | `				}` |
|       175 |  7229 | `				if( !bAllowVoid ){` |
|       ! 0 |  7230 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7231 | `						"void cannot be used here");` |
|       ! 0 |  7232 | `					return SXERR_SYNTAX;` |
|         - |  7233 | `				}` |
|       175 |  7234 | `				if( bShortNullable ){` |
|       ! 0 |  7235 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7236 | `						"Void type cannot be nullable");` |
|       ! 0 |  7237 | `					return SXERR_SYNTAX;` |
|         - |  7238 | `				}` |
|        85 |  7239 | `			}` |
|    131707 |  7240 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7241 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7242 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7243 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7244 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        26 |  7245 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7246 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7247 | `					 * same as any other non-standalone use. */` |
|         5 |  7248 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7249 | `						"never can only be used as a standalone type");` |
|         5 |  7250 | `					return SXERR_SYNTAX;` |
|         - |  7251 | `				}` |
|        21 |  7252 | `				if( !bAllowVoid ){` |
|         - |  7253 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7254 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7255 | `						"never cannot be used as a parameter type");` |
|         3 |  7256 | `					return SXERR_SYNTAX;` |
|         - |  7257 | `				}` |
|         8 |  7258 | `			}` |
|    131701 |  7259 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7260 | `				bExplicitNull = 1;` |
|        19 |  7261 | `			}else{` |
|    131671 |  7262 | `				bHasNonNull = 1;` |
|         - |  7263 | `			}` |
|         - |  7264 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7265 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7266 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7267 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7268 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    131901 |  7269 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7270 | `				int bDup = 0;` |
|       207 |  7271 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7272 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7273 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7274 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7275 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7276 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7277 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7278 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7279 | `								aAtoms[j].sClass.zString,` |
|        34 |  7280 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7281 | `							bDup = 1;` |
|       ! 0 |  7282 | `						}` |
|        27 |  7283 | `					}else{` |
|         3 |  7284 | `						bDup = 1;` |
|         - |  7285 | `					}` |
|        23 |  7286 | `				}` |
|       195 |  7287 | `				if( bDup ){` |
|         - |  7288 | `					const char *zName;` |
|         - |  7289 | `					sxu32 nName;` |
|         3 |  7290 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7291 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7292 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7293 | `					}else{` |
|         3 |  7294 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7295 | `						nName = aAtoms[i].nCanon;` |
|         - |  7296 | `					}` |
|         4 |  7297 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7298 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7299 | `					return SXERR_SYNTAX;` |
|         - |  7300 | `				}` |
|        99 |  7301 | `			}` |
|     65852 |  7302 | `		}` |
|    131539 |  7303 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7304 | `			if( bShortNullable ){` |
|         - |  7305 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7306 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7307 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7308 | `				return SXERR_SYNTAX;` |
|         - |  7309 | `			}` |
|         - |  7310 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7311 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7312 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7313 | `			 * atom, so set it here. */` |
|         7 |  7314 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7315 | `		}` |
|         - |  7316 | `	}` |
|         - |  7317 | `	/* Compute nullability flag */` |
|    131539 |  7318 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       128 |  7319 | `		*piTypeFlags \|= iNullableFlag;` |
|        62 |  7320 | `	}` |
|         - |  7321 | `	/* Build canonical type text */` |
|    131539 |  7322 | `	if( pTypeText ){` |
|         - |  7323 | `		SyBlob sBlob;` |
|    131539 |  7324 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    197259 |  7325 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     65767 |  7326 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    131539 |  7327 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    197027 |  7328 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    131348 |  7329 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    131353 |  7330 | `			if( zDup ){` |
|    131353 |  7331 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     65674 |  7332 | `			}` |
|     65674 |  7333 | `		}` |
|    131539 |  7334 | `		SyBlobRelease(&sBlob);` |
|     65767 |  7335 | `	}` |
|         - |  7336 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7337 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7338 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7339 | `	{` |
|    131539 |  7340 | `		int nNonNull = 0;` |
|    131539 |  7341 | `		int iNonNullIdx = -1;` |
|         - |  7342 | `		int i;` |
|    263225 |  7343 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131691 |  7344 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    131661 |  7345 | `				nNonNull++;` |
|    131661 |  7346 | `				iNonNullIdx = i;` |
|     65828 |  7347 | `			}` |
|     65848 |  7348 | `		}` |
|    131539 |  7349 | `		if( nNonNull <= 1 ){` |
|         - |  7350 | `			/* Fast path: store as single type. */` |
|    131433 |  7351 | `			if( iNonNullIdx >= 0 ){` |
|    131427 |  7352 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    131427 |  7353 | `				if( pA->nType == SXU32_HIGH ){` |
|     59468 |  7354 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19821 |  7355 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     39647 |  7356 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     39647 |  7357 | `					*pnType = SXU32_HIGH;` |
|     39647 |  7358 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    111606 |  7359 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7360 | `					*pnType = MEMOBJ_VOID;` |
|     91700 |  7361 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7362 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7363 | `				}else{` |
|     91599 |  7364 | `					*pnType = pA->nType;` |
|         - |  7365 | `				}` |
|     65711 |  7366 | `			}` |
|     65719 |  7367 | `		}else{` |
|         - |  7368 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7369 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7370 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7371 | `				ph7_type_alt sAlt;` |
|       249 |  7372 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7373 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7374 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7375 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7376 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7377 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7378 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7379 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7380 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7381 | `				}else{` |
|       145 |  7382 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7383 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7384 | `				}` |
|       239 |  7385 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7386 | `			}` |
|         - |  7387 | `		}` |
|         - |  7388 | `	}` |
|    131539 |  7389 | `	return SXRET_OK;` |
|     65780 |  7390 | `}` |
|         - |  7391 |  |
|         - |  7392 | `/*` |
|         - |  7393 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7394 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7395 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7396 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7397 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7398 | `` *          and union types `: T\|U`.`` |
|         - |  7399 | ` */` |
|   2769712 |  7400 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7401 | `{` |
|   2769717 |  7402 | `	sxi32 iFlags = 0;` |
|         - |  7403 | `	sxi32 rc;` |
|         - |  7404 | `	sxu32 nLine;` |
|   2769717 |  7405 | `	pFunc->nReturnType = 0;` |
|   2769717 |  7406 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2769717 |  7407 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7408 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7409 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7410 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7411 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7412 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2769717 |  7413 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2769717 |  7414 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2769717 |  7415 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2757239 |  7416 | `		return SXRET_OK;` |
|         - |  7417 | `	}` |
|     12483 |  7418 | `	pGen->pIn++; /* Skip ':' */` |
|     12483 |  7419 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7420 | `		return SXRET_OK;` |
|         - |  7421 | `	}` |
|     12483 |  7422 | `	nLine = pGen->pIn->nLine;` |
|     12483 |  7423 | `	rc = GenStateParseUnionTypeDecl(` |
|      6239 |  7424 | `		pGen,` |
|      6239 |  7425 | `		&pFunc->nReturnType,` |
|      6239 |  7426 | `		&pFunc->sReturnClass,` |
|      6239 |  7427 | `		&pFunc->aReturnUnion,` |
|         - |  7428 | `		&iFlags,` |
|      6239 |  7429 | `		&pFunc->sReturnTypeName,` |
|         - |  7430 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7431 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7432 | `		/* iUnionFlag */ 0,` |
|         - |  7433 | `		/* bAllowVoid */ 1,` |
|      6239 |  7434 | `		nLine);` |
|     12483 |  7435 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7436 | `		return SXERR_ABORT;` |
|         - |  7437 | `	}` |
|     12483 |  7438 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7439 | `		/* Error already reported */` |
|       ! 0 |  7440 | `		return SXERR_SYNTAX;` |
|         - |  7441 | `	}` |
|     12483 |  7442 | `	if( rc == SXERR_SYNTAX ){` |
|         8 |  7443 | `		if( pGen->pIn < pGen->pEnd ){` |
|        11 |  7444 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7445 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7446 | `				&pGen->pIn->sData);` |
|         5 |  7447 | `		}else{` |
|       ! 0 |  7448 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7449 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7450 | `		}` |
|         8 |  7451 | `		return SXERR_SYNTAX;` |
|         - |  7452 | `	}` |
|     12477 |  7453 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12477 |  7454 | `	return SXRET_OK;` |
|   1384861 |  7455 | `}` |
|         - |  7456 |  |
|    309262 |  7457 | `static sxi32 GenStateCompileFunc(` |
|         - |  7458 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7459 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7460 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7461 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7462 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7463 | `	)` |
|         5 |  7464 | `{` |
|         - |  7465 | `	ph7_vm_func *pFunc;` |
|         - |  7466 | `	SyToken *pEnd;` |
|         - |  7467 | `	sxu32 nLine;` |
|         - |  7468 | `	char *zName;` |
|         - |  7469 | `	sxi32 rc;` |
|         - |  7470 | `	/* Extract line number */` |
|    309267 |  7471 | `	nLine = pGen->pIn->nLine;` |
|         - |  7472 | `	/* Jump the left parenthesis '(' */` |
|    309267 |  7473 | `	pGen->pIn++;` |
|         - |  7474 | `	/* Delimit the function signature */` |
|    309267 |  7475 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    309267 |  7476 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7477 | `		/* Syntax error */` |
|         8 |  7478 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|         8 |  7479 | `		if( rc == SXERR_ABORT ){` |
|         - |  7480 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7481 | `			return SXERR_ABORT;` |
|         - |  7482 | `		}` |
|         8 |  7483 | `		pGen->pIn = pGen->pEnd;` |
|         8 |  7484 | `		return SXRET_OK;` |
|         - |  7485 | `	}` |
|         - |  7486 | `	/* Create the function state */` |
|    309261 |  7487 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    309261 |  7488 | `	if( pFunc == 0 ){` |
|       ! 0 |  7489 | `		goto OutOfMem;` |
|         - |  7490 | `	}` |
|         - |  7491 | `	/* Build the function name, prepending namespace if active */` |
|    309268 |  7492 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7493 | `		SyBlob sFQN;` |
|         - |  7494 | `		sxu32 nLen;` |
|        16 |  7495 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7496 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7497 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7498 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7499 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7500 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7501 | `		SyBlobRelease(&sFQN);` |
|        16 |  7502 | `		if( zName == 0 ){` |
|       ! 0 |  7503 | `			goto OutOfMem;` |
|         - |  7504 | `		}` |
|        16 |  7505 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7506 | `	}else{` |
|    309247 |  7507 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    309247 |  7508 | `		if( zName == 0 ){` |
|       ! 0 |  7509 | `			goto OutOfMem;` |
|         - |  7510 | `		}` |
|    309247 |  7511 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7512 | `	}` |
|         - |  7513 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7514 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    309261 |  7515 | `	pFunc->nLine = nLine;` |
|    309261 |  7516 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    309261 |  7517 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7518 | `		return SXERR_ABORT;` |
|         - |  7519 | `	}` |
|    309261 |  7520 | `	if( pGen->pIn < pEnd ){` |
|         - |  7521 | `		/* Collect function arguments */` |
|    249323 |  7522 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    249323 |  7523 | `		if( rc == SXERR_ABORT ){` |
|         - |  7524 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7525 | `			return SXERR_ABORT;` |
|         - |  7526 | `		}` |
|    124659 |  7527 | `	}` |
|         - |  7528 | `	/* Point past ')' and parse optional return type ': type' */` |
|    309261 |  7529 | `	pGen->pIn = &pEnd[1];` |
|         - |  7530 | `	{` |
|    309261 |  7531 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    309261 |  7532 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7533 | `			return SXERR_ABORT;` |
|    309261 |  7534 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         8 |  7535 | `			return SXERR_SYNTAX;` |
|         - |  7536 | `		}` |
|         - |  7537 | `	}` |
|    309255 |  7538 | `	if( bHandleClosure ){` |
|         - |  7539 | `		ph7_vm_func_closure_env sEnv;` |
|       469 |  7540 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       464 |  7541 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       280 |  7542 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        91 |  7543 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7544 | `				/* Closure,record environment variable */` |
|        91 |  7545 | `				pGen->pIn++;` |
|        91 |  7546 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7547 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7548 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7549 | `						return SXERR_ABORT;` |
|         - |  7550 | `					}` |
|       ! 0 |  7551 | `				}` |
|        91 |  7552 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7553 | `				/* Compile until we hit the first closing parenthesis */` |
|       187 |  7554 | `				while( pGen->pIn < pGen->pEnd ){` |
|       187 |  7555 | `					int iFlagsLocal = 0;` |
|       187 |  7556 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        91 |  7557 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        91 |  7558 | `						break;` |
|         - |  7559 | `					}` |
|       101 |  7560 | `					nLineLocal = pGen->pIn->nLine;` |
|       101 |  7561 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7562 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7563 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7564 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7565 | `						pGen->pIn++;` |
|        27 |  7566 | `					}` |
|        96 |  7567 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       101 |  7568 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7569 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7570 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7571 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7572 | `								return SXERR_ABORT;` |
|         - |  7573 | `							}` |
|         - |  7574 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7575 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7576 | `								pGen->pIn++;` |
|       ! 0 |  7577 | `							}` |
|       ! 0 |  7578 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7579 | `								pGen->pIn++;` |
|       ! 0 |  7580 | `							}` |
|       ! 0 |  7581 | `							break;` |
|         - |  7582 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7583 | `					}else{` |
|         - |  7584 | `						SyString *pNameLocal;` |
|         - |  7585 | `						char *zDup;` |
|         - |  7586 | `						/* Duplicate variable name */` |
|       101 |  7587 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       101 |  7588 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       101 |  7589 | `						if( zDup ){` |
|         - |  7590 | `							/* Zero the structure */` |
|       101 |  7591 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       101 |  7592 | `							sEnv.iFlags = iFlagsLocal;` |
|       101 |  7593 | `							sEnv.nIdx = SXU32_HIGH;` |
|       101 |  7594 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       101 |  7595 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       116 |  7596 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7597 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7598 | `									got_this = 1;` |
|       ! 0 |  7599 | `							}` |
|         - |  7600 | `							/* Save imported variable */` |
|       101 |  7601 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        53 |  7602 | `						}else{` |
|       ! 0 |  7603 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7604 | `							 return SXERR_ABORT;` |
|         - |  7605 | `						}` |
|         - |  7606 | `					}` |
|       101 |  7607 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       113 |  7608 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7609 | `						/* Ignore trailing commas */` |
|        13 |  7610 | `						pGen->pIn++;` |
|         1 |  7611 | `					}` |
|         5 |  7612 | `				}` |
|         - |  7613 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7614 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7615 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7616 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7617 | `				 * legacy pre-use position. */` |
|        91 |  7618 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7619 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7620 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7621 | `						return SXERR_ABORT;` |
|         7 |  7622 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7623 | `						return SXERR_SYNTAX;` |
|         - |  7624 | `					}` |
|         3 |  7625 | `				}` |
|        43 |  7626 | `		}` |
|       469 |  7627 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7628 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7629 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7630 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7631 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7632 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7633 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7634 | `			 * closure never binds $this (php). */` |
|       461 |  7635 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       461 |  7636 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       461 |  7637 | `			sEnv.nIdx = SXU32_HIGH;` |
|       461 |  7638 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       461 |  7639 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       461 |  7640 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       228 |  7641 | `		}` |
|       469 |  7642 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7643 | `			/* Mark as closure */` |
|       463 |  7644 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       229 |  7645 | `		}` |
|       232 |  7646 | `	}` |
|         - |  7647 | `	/* Compile the body */` |
|    309255 |  7648 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    309255 |  7649 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7650 | `		return SXERR_ABORT;` |
|         - |  7651 | `	}` |
|         - |  7652 | `	/* The cursor sits just past the body's closing brace */` |
|    309255 |  7653 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    309255 |  7654 | `	if( ppFunc ){` |
|    309255 |  7655 | `		*ppFunc = pFunc;` |
|    154625 |  7656 | `	}` |
|    309255 |  7657 | `	rc = SXRET_OK;` |
|    309255 |  7658 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7659 | `		/* Finally register the function */` |
|    308797 |  7660 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    154396 |  7661 | `	}` |
|    309255 |  7662 | `	if( rc == SXRET_OK ){` |
|    309255 |  7663 | `		return SXRET_OK;` |
|         - |  7664 | `	}` |
|         - |  7665 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7666 | `OutOfMem:` |
|         - |  7667 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7668 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7669 | `	 */` |
|       ! 0 |  7670 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7671 | `	return SXERR_ABORT;` |
|    154636 |  7672 | `}` |
|         - |  7673 | `/*` |
|         - |  7674 | ` * Compile a standard PHP function.` |
|         - |  7675 | ` *  Refer to the block-comment above for more information.` |
|         - |  7676 | ` */` |
|    308806 |  7677 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7678 | `{` |
|         - |  7679 | `	SyString *pName;` |
|         - |  7680 | `	sxi32 iFlags;` |
|         - |  7681 | `	sxu32 nKwLine;` |
|         - |  7682 | `	sxu32 nLine;` |
|         - |  7683 | `	sxi32 rc;` |
|         - |  7684 |  |
|    308811 |  7685 | `	nLine = pGen->pIn->nLine;` |
|    308811 |  7686 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    308811 |  7687 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    308811 |  7688 | `	iFlags = 0;` |
|    308811 |  7689 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7690 | `		/* Return by reference,remember that */` |
|        12 |  7691 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7692 | `		/* Jump the '&' token */` |
|        12 |  7693 | `		pGen->pIn++;` |
|         5 |  7694 | `	}` |
|    308811 |  7695 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7696 | `		/* Invalid function name */` |
|         8 |  7697 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|         8 |  7698 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7699 | `			return SXERR_ABORT;` |
|         - |  7700 | `		}` |
|         - |  7701 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  7702 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  7703 | `			pGen->pIn++;` |
|         2 |  7704 | `		}` |
|         8 |  7705 | `		return SXRET_OK;` |
|         - |  7706 | `	}` |
|    308805 |  7707 | `	pName = &pGen->pIn->sData;` |
|    308805 |  7708 | `	nLine = pGen->pIn->nLine;` |
|         - |  7709 | `	/* Jump the function name */` |
|    308805 |  7710 | `	pGen->pIn++;` |
|    308805 |  7711 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7712 | `		/* Syntax error */` |
|         3 |  7713 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|         3 |  7714 | `		if( rc == SXERR_ABORT ){` |
|         - |  7715 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7716 | `			return SXERR_ABORT;` |
|         - |  7717 | `		}` |
|         - |  7718 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  7719 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  7720 | `			pGen->pIn++;` |
|       ! 0 |  7721 | `		}` |
|         3 |  7722 | `		return SXRET_OK;` |
|         - |  7723 | `	}` |
|         - |  7724 | `	/* Compile function body */` |
|         - |  7725 | `	{` |
|    308803 |  7726 | `		ph7_vm_func *pFuncState = 0;` |
|    308803 |  7727 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    308803 |  7728 | `		if( pFuncState ){` |
|         - |  7729 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    308791 |  7730 | `			pFuncState->nLine = nKwLine;` |
|    154393 |  7731 | `		}` |
|         - |  7732 | `	}` |
|    308803 |  7733 | `	return rc;` |
|    154408 |  7734 | `}` |
|         - |  7735 | `/*` |
|         - |  7736 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  7737 | ` * According to the PHP language reference manual` |
|         - |  7738 | ` *  Visibility:` |
|         - |  7739 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  7740 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  7741 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  7742 | ` *  Members declared protected can be accessed only within the class` |
|         - |  7743 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  7744 | ` *  may only be accessed by the class that defines the member.` |
|         - |  7745 | ` */` |
|   3229624 |  7746 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  7747 | `{` |
|   3229629 |  7748 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    260233 |  7749 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2969401 |  7750 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    197087 |  7751 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  7752 | `	}` |
|         - |  7753 | `	/* Assume public by default */` |
|   2772319 |  7754 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1614817 |  7755 | `}` |
|         - |  7756 | `/*` |
|         - |  7757 | ` * Compile a class constant.` |
|         - |  7758 | ` * According to the PHP language reference manual` |
|         - |  7759 | ` *  Class Constants` |
|         - |  7760 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  7761 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  7762 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  7763 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  7764 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  7765 | ` *   It's also possible for interfaces to have constants.` |
|         - |  7766 | ` * Symisc eXtension.` |
|         - |  7767 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  7768 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  7769 | ` *  Example:` |
|         - |  7770 | ` *   class Test{` |
|         - |  7771 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  7772 | ` *   };` |
|         - |  7773 | ` *   var_dump(TEST::MyConst);` |
|         - |  7774 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  7775 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  7776 | ` */` |
|         - |  7777 | `/*` |
|         - |  7778 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  7779 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  7780 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  7781 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  7782 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  7783 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  7784 | ` */` |
|    299616 |  7785 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  7786 | `{` |
|         - |  7787 | `	SyToken *p0, *p1;` |
|    299621 |  7788 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7789 | `		return 0;` |
|         - |  7790 | `	}` |
|    299621 |  7791 | `	p0 = pGen->pIn;` |
|         - |  7792 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    299621 |  7793 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  7794 | `		return 1;` |
|         - |  7795 | `	}` |
|    299621 |  7796 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  7797 | `		return 1;` |
|         - |  7798 | `	}` |
|         - |  7799 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  7800 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  7801 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    299617 |  7802 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    299617 |  7803 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    299617 |  7804 | `		if( p1 ){` |
|    299617 |  7805 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  7806 | `				return 1;` |
|         - |  7807 | `			}` |
|    299587 |  7808 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  7809 | `				return 1;` |
|         - |  7810 | `			}` |
|    149789 |  7811 | `		}` |
|    149789 |  7812 | `	}` |
|    299583 |  7813 | `	return 0;` |
|    149813 |  7814 | `}` |
|         - |  7815 | `/*` |
|         - |  7816 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  7817 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  7818 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  7819 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  7820 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  7821 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  7822 | ` * Peek only; never consumes tokens.` |
|         - |  7823 | ` */` |
|        24 |  7824 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  7825 | `{` |
|        28 |  7826 | `	SyToken *p = pGen->pIn;` |
|        39 |  7827 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  7828 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  7829 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  7830 | `	}` |
|        28 |  7831 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  7832 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  7833 | `	}` |
|         6 |  7834 | `	p++;` |
|         - |  7835 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  7836 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  7837 | `}` |
|         - |  7838 | `/*` |
|         - |  7839 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  7840 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  7841 | `` * `$o->new`), not a `new` expression.`` |
|         - |  7842 | ` */` |
|       110 |  7843 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  7844 | `{` |
|         - |  7845 | `	sxi32 iOp;` |
|       114 |  7846 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  7847 | `		return 0;` |
|         - |  7848 | `	}` |
|       104 |  7849 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  7850 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  7851 | `}` |
|         - |  7852 | `/*` |
|         - |  7853 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  7854 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  7855 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  7856 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  7857 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  7858 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  7859 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  7860 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  7861 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  7862 | ` *` |
|         - |  7863 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  7864 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  7865 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  7866 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  7867 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  7868 | ` */` |
|    643078 |  7869 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  7870 | `{` |
|    643083 |  7871 | `	SyToken *p = pGen->pIn;` |
|    643083 |  7872 | `	int iDepth = 0;` |
|   1685195 |  7873 | `	while( p < pGen->pEnd ){` |
|   1685195 |  7874 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    643031 |  7875 | `			break; /* end of this initializer */` |
|         - |  7876 | `		}` |
|   1042164 |  7877 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    525043 |  7878 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7912 |  7879 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  7880 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  7881 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  7882 | `			 * expression. */` |
|         3 |  7883 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  7884 | `			p++;` |
|         3 |  7885 | `			if( bArrow ){` |
|         - |  7886 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  7887 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  7888 | `				int iBase = iDepth;` |
|        17 |  7889 | `				while( p < pGen->pEnd ){` |
|        17 |  7890 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  7891 | `						iDepth++;` |
|        15 |  7892 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  7893 | `						if( iDepth <= iBase ){` |
|       ! 0 |  7894 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  7895 | `						}` |
|         5 |  7896 | `						iDepth--;` |
|        11 |  7897 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  7898 | `						break;` |
|         - |  7899 | `					}` |
|        15 |  7900 | `					p++;` |
|         1 |  7901 | `				}` |
|         2 |  7902 | `			}else{` |
|         - |  7903 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  7904 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  7905 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  7906 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  7907 | `				int iLocal = 0;` |
|       ! 0 |  7908 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  7909 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  7910 | `						break; /* body brace */` |
|         - |  7911 | `					}` |
|       ! 0 |  7912 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  7913 | `						iLocal++;` |
|       ! 0 |  7914 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  7915 | `						if( iLocal > 0 ){` |
|       ! 0 |  7916 | `							iLocal--;` |
|       ! 0 |  7917 | `						}` |
|       ! 0 |  7918 | `					}` |
|       ! 0 |  7919 | `					p++;` |
|       ! 0 |  7920 | `				}` |
|       ! 0 |  7921 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  7922 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  7923 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  7924 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  7925 | `							iBrace++;` |
|       ! 0 |  7926 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  7927 | `							iBrace--;` |
|       ! 0 |  7928 | `							if( iBrace == 0 ){` |
|       ! 0 |  7929 | `								p++;` |
|       ! 0 |  7930 | `								break;` |
|         - |  7931 | `							}` |
|       ! 0 |  7932 | `						}` |
|       ! 0 |  7933 | `						p++;` |
|       ! 0 |  7934 | `					}` |
|       ! 0 |  7935 | `				}` |
|         - |  7936 | `			}` |
|         3 |  7937 | `			continue;` |
|         - |  7938 | `		}` |
|   1042167 |  7939 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  7940 | `			if( iDepth == 0 ){` |
|         - |  7941 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  7942 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  7943 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  7944 | `				 * is legal — don't scan into it. */` |
|        45 |  7945 | `				break;` |
|         - |  7946 | `			}` |
|       ! 0 |  7947 | `			iDepth++;` |
|   1042123 |  7948 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     43429 |  7949 | `			iDepth++;` |
|   1020411 |  7950 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     43427 |  7951 | `			if( iDepth > 0 ){` |
|     43427 |  7952 | `				iDepth--;` |
|     21711 |  7953 | `			}` |
|    976988 |  7954 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    347509 |  7955 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  7956 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  7957 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  7958 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  7959 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  7960 | `				return 1;` |
|         - |  7961 | `			}` |
|       ! 0 |  7962 | `		}` |
|   1042115 |  7963 | `		p++;` |
|         5 |  7964 | `	}` |
|    643075 |  7965 | `	return 0;` |
|    321544 |  7966 | `}` |
|         - |  7967 | `/*` |
|         - |  7968 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  7969 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  7970 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  7971 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  7972 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  7973 | ` * share the same backing.` |
|         - |  7974 | ` */` |
|       350 |  7975 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  7976 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  7977 | `{` |
|       355 |  7978 | `	pAttr->nType = nType;` |
|       355 |  7979 | `	pAttr->sClass = *pClass;` |
|       355 |  7980 | `	pAttr->sTypeName = *pTypeName;` |
|       355 |  7981 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  7982 | `		sxu32 i;` |
|        73 |  7983 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  7984 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  7985 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  7986 | `		}` |
|        11 |  7987 | `	}` |
|       355 |  7988 | `}` |
|    299616 |  7989 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  7990 | `{` |
|    299621 |  7991 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  7992 | `	SySet *pInstrContainer;` |
|         - |  7993 | `	ph7_class_attr *pCons;` |
|         - |  7994 | `	SyString *pName;` |
|         - |  7995 | `	sxi32 rc;` |
|    299621 |  7996 | `	sxu32 nType = 0;` |
|         - |  7997 | `	SyString sTypeClass;` |
|         - |  7998 | `	SyString sTypeText;` |
|         - |  7999 | `	SySet aUnionAlts;` |
|    299621 |  8000 | `	sxi32 iTypeFlags = 0;` |
|    299621 |  8001 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    299621 |  8002 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    299621 |  8003 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8004 | `	/* Extract visibility level */` |
|    299621 |  8005 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8006 | `	/* Mark as constant */` |
|    299621 |  8007 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    299621 |  8008 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8009 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8010 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    299640 |  8011 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8012 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8013 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8014 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8015 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8016 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8017 | `		 * and success paths release. */` |
|        42 |  8018 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8019 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8020 | `			goto Synchronize;` |
|        42 |  8021 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8022 | `			return SXERR_ABORT;` |
|        42 |  8023 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8024 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8025 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8026 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8027 | `				return SXERR_ABORT;` |
|         - |  8028 | `			}` |
|       ! 0 |  8029 | `			goto Synchronize;` |
|         - |  8030 | `		}` |
|        42 |  8031 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8032 | `	}` |
|    149808 |  8033 | `loop:` |
|    299623 |  8034 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8035 | `		/* Invalid constant name */` |
|       ! 0 |  8036 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8037 | `		if( rc == SXERR_ABORT ){` |
|         - |  8038 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8039 | `			return SXERR_ABORT;` |
|         - |  8040 | `		}` |
|       ! 0 |  8041 | `		goto Synchronize;` |
|         - |  8042 | `	}` |
|         - |  8043 | `	/* Peek constant name */` |
|    299623 |  8044 | `	pName = &pGen->pIn->sData;` |
|         - |  8045 | `	/* Make sure the constant name isn't reserved */` |
|    299623 |  8046 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8047 | `		/* Reserved constant name */` |
|       ! 0 |  8048 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8049 | `		if( rc == SXERR_ABORT ){` |
|         - |  8050 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8051 | `			return SXERR_ABORT;` |
|         - |  8052 | `		}` |
|       ! 0 |  8053 | `		goto Synchronize;` |
|         - |  8054 | `	}` |
|         - |  8055 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    299623 |  8056 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8057 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8058 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8059 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8060 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8061 | `			return SXERR_ABORT;` |
|        42 |  8062 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8063 | `			goto Synchronize;` |
|         - |  8064 | `		}` |
|        18 |  8065 | `	}` |
|         - |  8066 | `	/* Advance the stream cursor */` |
|    299621 |  8067 | `	pGen->pIn++;` |
|    299621 |  8068 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8069 | `		/* Invalid declaration */` |
|       ! 0 |  8070 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8071 | `		if( rc == SXERR_ABORT ){` |
|         - |  8072 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8073 | `			return SXERR_ABORT;` |
|         - |  8074 | `		}` |
|       ! 0 |  8075 | `		goto Synchronize;` |
|         - |  8076 | `	}` |
|    299621 |  8077 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8078 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8079 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8080 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8081 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    299616 |  8082 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8083 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8084 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8085 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8086 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8087 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8088 | `			return SXERR_ABORT;` |
|         - |  8089 | `		}` |
|         6 |  8090 | `		goto Synchronize;` |
|         - |  8091 | `	}` |
|         - |  8092 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8093 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8094 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    299617 |  8095 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8096 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8097 | `			"New expressions are not supported in this context");` |
|         5 |  8098 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8099 | `			return SXERR_ABORT;` |
|         - |  8100 | `		}` |
|         5 |  8101 | `		goto Synchronize;` |
|         - |  8102 | `	}` |
|         - |  8103 | `	/* Allocate a new class attribute */` |
|    299613 |  8104 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    299613 |  8105 | `	if( pCons ){` |
|    299613 |  8106 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    299613 |  8107 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8108 | `			return SXERR_ABORT;` |
|         - |  8109 | `		}` |
|    149804 |  8110 | `	}` |
|    299613 |  8111 | `	if( pCons == 0 ){` |
|       ! 0 |  8112 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8113 | `		return SXERR_ABORT;` |
|         - |  8114 | `	}` |
|    299613 |  8115 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8116 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8117 | `	}` |
|         - |  8118 | `	/* Swap bytecode container */` |
|    299613 |  8119 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    299613 |  8120 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8121 | `	/* Compile constant value.` |
|         - |  8122 | `	 */` |
|    299613 |  8123 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    299613 |  8124 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8125 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8126 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8127 | `			return SXERR_ABORT;` |
|         - |  8128 | `		}` |
|         1 |  8129 | `	}` |
|         - |  8130 | `	/* Emit the done instruction */` |
|    299613 |  8131 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    299613 |  8132 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    299613 |  8133 | `	if( rc == SXERR_ABORT ){` |
|         - |  8134 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8135 | `		return SXERR_ABORT;` |
|         - |  8136 | `	}` |
|         - |  8137 | `	/* All done,install the constant */` |
|    299613 |  8138 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    299613 |  8139 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8140 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8141 | `		return SXERR_ABORT;` |
|         - |  8142 | `	}` |
|    299613 |  8143 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8144 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8145 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8146 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8147 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8148 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8149 | `				pTok--;` |
|       ! 0 |  8150 | `			}` |
|       ! 0 |  8151 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8152 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8153 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8154 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8155 | `				return SXERR_ABORT;` |
|         - |  8156 | `			}` |
|       ! 0 |  8157 | `		}else{` |
|         3 |  8158 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8159 | `				goto loop;` |
|         - |  8160 | `			}` |
|         - |  8161 | `		}` |
|       ! 0 |  8162 | `	}` |
|    299611 |  8163 | `	SySetRelease(&aUnionAlts);` |
|    299611 |  8164 | `	return SXRET_OK;` |
|         5 |  8165 | `Synchronize:` |
|        13 |  8166 | `	SySetRelease(&aUnionAlts);` |
|         - |  8167 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8168 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8169 | `		pGen->pIn++;` |
|         3 |  8170 | `	}` |
|        13 |  8171 | `	return SXERR_CORRUPT;` |
|    149813 |  8172 | `}` |
|         - |  8173 | `/*` |
|         - |  8174 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8175 | ` * According to the PHP language reference manual` |
|         - |  8176 | ` *  Properties` |
|         - |  8177 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8178 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8179 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8180 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8181 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8182 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8183 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8184 | ` * Symisc eXtension.` |
|         - |  8185 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8186 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8187 | ` *  Example:` |
|         - |  8188 | ` *   class Test{` |
|         - |  8189 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8190 | ` *   };` |
|         - |  8191 | ` *   var_dump(TEST::myVar);` |
|         - |  8192 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8193 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8194 | ` */` |
|         - |  8195 | `/*` |
|         - |  8196 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8197 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8198 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8199 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8200 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8201 | ` */` |
|   2412924 |  8202 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8203 | `{` |
|   2412929 |  8204 | `	SyToken *p = pStart;` |
|   2412929 |  8205 | `	int bFirst = 1;` |
|   2412929 |  8206 | `	if( p >= pEnd ) return 0;` |
|         - |  8207 | ``	/* Optional nullable `?` shorthand. */`` |
|   2412929 |  8208 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        35 |  8209 | `		p++;` |
|        35 |  8210 | `		if( p >= pEnd ) return 0;` |
|        16 |  8211 | `	}` |
|         - |  8212 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8213 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8214 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8215 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1206462 |  8216 | `	for(;;){` |
|   2412949 |  8217 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8218 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8219 | `			p++;` |
|         9 |  8220 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8221 | `			if( p >= pEnd ) return 0;` |
|         3 |  8222 | `			p++; /* skip ')' */` |
|         2 |  8223 | `		}else{` |
|         - |  8224 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8225 | ``			 * then any `&`-joined intersection members. */`` |
|   2412947 |  8226 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2412947 |  8227 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8228 | `				return 0;` |
|         - |  8229 | `			}` |
|         - |  8230 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8231 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8232 | `			 * may still appear at the initial dispatch site). */` |
|   2412947 |  8233 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2412899 |  8234 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2412894 |  8235 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    106790 |  8236 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2412617 |  8237 | `					return 0;` |
|         - |  8238 | `				}` |
|       141 |  8239 | `			}` |
|       335 |  8240 | `			p++;` |
|       337 |  8241 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8242 | `				p += 2;` |
|         1 |  8243 | `			}` |
|       498 |  8244 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       338 |  8245 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8246 | `				p++; /* skip '&' */` |
|         3 |  8247 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8248 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8249 | `				p++;` |
|         3 |  8250 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8251 | `					p += 2;` |
|       ! 0 |  8252 | `				}` |
|         1 |  8253 | `			}` |
|         - |  8254 | `		}` |
|       337 |  8255 | `		bFirst = 0;` |
|       332 |  8256 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8257 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8258 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8259 | `			continue;` |
|         - |  8260 | `		}` |
|       317 |  8261 | `		break;` |
|       ! 0 |  8262 | `	}` |
|       317 |  8263 | `	if( p >= pEnd ) return 0;` |
|       317 |  8264 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1206467 |  8265 | `}` |
|         - |  8266 |  |
|         - |  8267 | `/*` |
|         - |  8268 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8269 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8270 | ` * if not). Recognized forms:` |
|         - |  8271 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8272 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8273 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8274 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8275 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8276 | ` * on unrecoverable error.` |
|         - |  8277 | ` *` |
|         - |  8278 | ` * When a type is parsed:` |
|         - |  8279 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8280 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8281 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8282 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8283 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8284 | ` */` |
|       322 |  8285 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8286 | `	ph7_gen_state *pGen,` |
|         - |  8287 | `	sxu32 *pnType,` |
|         - |  8288 | `	SyString *pClass,` |
|         - |  8289 | `	sxi32 *piTypeFlags,` |
|         - |  8290 | `	SyString *pTypeText,` |
|         - |  8291 | `	SySet *pAlts` |
|         5 |  8292 | `){` |
|       327 |  8293 | `	sxi32 iFlags = 0;` |
|         - |  8294 | `	sxi32 rc;` |
|       327 |  8295 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8296 | `		return SXRET_OK;` |
|         - |  8297 | `	}` |
|         - |  8298 | `	/* If the first token is '$', there's no type */` |
|       327 |  8299 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8300 | `		return SXRET_OK;` |
|         - |  8301 | `	}` |
|       327 |  8302 | `	rc = GenStateParseUnionTypeDecl(` |
|       161 |  8303 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8304 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8305 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8306 | `		/* bAllowVoid */ 0,` |
|       322 |  8307 | `		pGen->pIn->nLine);` |
|       327 |  8308 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8309 | `		return rc;` |
|         - |  8310 | `	}` |
|         - |  8311 | `	/* Verify next token is '$' (start of property name) */` |
|       327 |  8312 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8313 | `		return SXERR_SYNTAX;` |
|         - |  8314 | `	}` |
|       327 |  8315 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       327 |  8316 | `	return SXRET_OK;` |
|       166 |  8317 | `}` |
|         - |  8318 |  |
|         - |  8319 | `/*` |
|         - |  8320 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8321 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8322 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8323 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8324 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8325 | ` * by the type parser itself before reaching here.` |
|         - |  8326 | ` *` |
|         - |  8327 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8328 | ` * use in the error message.` |
|         - |  8329 | ` */` |
|       498 |  8330 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8331 | `	sxu32 nType,` |
|         - |  8332 | `	const SyString *pClass,` |
|         - |  8333 | `	const char **pzName,` |
|         - |  8334 | `	sxu32 *pnName)` |
|         5 |  8335 | `{` |
|         - |  8336 | `	const char *z;` |
|         - |  8337 | `	sxu32 n;` |
|       503 |  8338 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       449 |  8339 | `		return 0;` |
|         - |  8340 | `	}` |
|        59 |  8341 | `	z = pClass->zString;` |
|        59 |  8342 | `	n = pClass->nByte;` |
|        59 |  8343 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8344 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8345 | `	}` |
|         - |  8346 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8347 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8348 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        52 |  8349 | `	return 0;` |
|       254 |  8350 | `}` |
|         - |  8351 |  |
|         - |  8352 | `/*` |
|         - |  8353 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8354 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8355 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8356 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8357 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8358 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8359 | ` *` |
|         - |  8360 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8361 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8362 | ` */` |
|       436 |  8363 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8364 | `	ph7_gen_state *pGen,` |
|         - |  8365 | `	ph7_class *pClass,` |
|         - |  8366 | `	const SyString *pMemberName,` |
|         - |  8367 | `	sxu32 nType,` |
|         - |  8368 | `	const SyString *pTypeClass,` |
|         - |  8369 | `	const SyString *pTypeText,` |
|         - |  8370 | `	SySet *pUnionAlts,` |
|         - |  8371 | `	const char *zErrFmt,` |
|         - |  8372 | `	sxu32 nLine)` |
|         5 |  8373 | `{` |
|       441 |  8374 | `	const char *zBad = 0;` |
|       441 |  8375 | `	sxu32 nBad = 0;` |
|         - |  8376 | `	SyString sFallback;` |
|         - |  8377 | `	const SyString *pBad;` |
|         - |  8378 | `	sxi32 rc;` |
|       441 |  8379 | `	int bDisallowed = 0;` |
|       441 |  8380 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8381 | `		bDisallowed = 1;` |
|       439 |  8382 | `	}else if( pUnionAlts ){` |
|         - |  8383 | `		sxu32 i;` |
|        95 |  8384 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8385 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8386 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8387 | `				bDisallowed = 1;` |
|         3 |  8388 | `				break;` |
|         - |  8389 | `			}` |
|        35 |  8390 | `		}` |
|        15 |  8391 | `	}` |
|       441 |  8392 | `	if( !bDisallowed ){` |
|       435 |  8393 | `		return SXRET_OK;` |
|         - |  8394 | `	}` |
|         - |  8395 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8396 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8397 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8398 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8399 | `		pBad = pTypeText;` |
|         5 |  8400 | `	}else{` |
|       ! 0 |  8401 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8402 | `		pBad = &sFallback;` |
|         - |  8403 | `	}` |
|        11 |  8404 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8405 | `		zErrFmt,` |
|         3 |  8406 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8407 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8408 | `		return SXERR_ABORT;` |
|         - |  8409 | `	}` |
|         8 |  8410 | `	return SXERR_SYNTAX;` |
|       223 |  8411 | `}` |
|         - |  8412 | `/*` |
|         - |  8413 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8414 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8415 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8416 | ` * than promoted to a lexer keyword.` |
|         - |  8417 | ` */` |
|  18552682 |  8418 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8419 | `{` |
|  18754105 |  8420 | `	return (pTok->nType & PH7_TK_ID)` |
|   9477759 |  8421 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  18754100 |  8422 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8423 | `}` |
|         - |  8424 | `/*` |
|         - |  8425 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8426 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8427 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8428 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8429 | ` */` |
|   7087356 |  8430 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8431 | `{` |
|   7087361 |  8432 | `	*pnTok = 0;` |
|   7087356 |  8433 | `	if( &pTok[3] < pEnd` |
|   6674772 |  8434 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5632915 |  8435 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2501829 |  8436 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8437 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8438 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8439 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8440 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8441 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8442 | `			*pnTok = 4;` |
|        17 |  8443 | `			return nKw;` |
|         - |  8444 | `		}` |
|       ! 0 |  8445 | `	}` |
|   7087345 |  8446 | `	return 0;` |
|   3543683 |  8447 | `}` |
|         - |  8448 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8449 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8450 | `{` |
|        17 |  8451 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8452 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8453 | `	}` |
|         5 |  8454 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8455 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8456 | `	}` |
|         3 |  8457 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8458 | `}` |
|    469838 |  8459 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8460 | `{` |
|    469843 |  8461 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8462 | `	ph7_class_attr *pAttr;` |
|         - |  8463 | `	SyString *pName;` |
|         - |  8464 | `	sxi32 rc;` |
|    469843 |  8465 | `	sxu32 nType = 0;` |
|         - |  8466 | `	SyString sTypeClass;` |
|         - |  8467 | `	SyString sTypeText;` |
|         - |  8468 | `	SySet aUnionAlts;` |
|    469843 |  8469 | `	sxi32 iTypeFlags = 0;` |
|    469843 |  8470 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    469843 |  8471 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    469843 |  8472 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8473 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8474 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8475 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    469843 |  8476 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8477 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8478 | `	}` |
|         - |  8479 | `	/* Extract visibility level */` |
|    469843 |  8480 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8481 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    470004 |  8482 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       327 |  8483 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       327 |  8484 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8485 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8486 | `			goto Synchronize;` |
|       327 |  8487 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8488 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8489 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8490 | `				&pGen->pIn->sData);` |
|       ! 0 |  8491 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8492 | `				return SXERR_ABORT;` |
|         - |  8493 | `			}` |
|       ! 0 |  8494 | `			goto Synchronize;` |
|       327 |  8495 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8496 | `			return SXERR_ABORT;` |
|         - |  8497 | `		}` |
|       161 |  8498 | `	}` |
|       ! 0 |  8499 | `loop:` |
|    469847 |  8500 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8501 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8502 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8503 | `			return SXERR_ABORT;` |
|         - |  8504 | `		}` |
|       ! 0 |  8505 | `		goto Synchronize;` |
|         - |  8506 | `	}` |
|    469847 |  8507 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    469847 |  8508 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8509 | `		/* Invalid attribute name */` |
|       ! 0 |  8510 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8511 | `		if( rc == SXERR_ABORT ){` |
|         - |  8512 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8513 | `			return SXERR_ABORT;` |
|         - |  8514 | `		}` |
|       ! 0 |  8515 | `		goto Synchronize;` |
|         - |  8516 | `	}` |
|         - |  8517 | `	/* Peek attribute name */` |
|    469847 |  8518 | `	pName = &pGen->pIn->sData;` |
|         - |  8519 | `	/* Advance the stream cursor */` |
|    469847 |  8520 | `	pGen->pIn++;` |
|    469847 |  8521 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8522 | `		/* Invalid declaration */` |
|         3 |  8523 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8524 | `		if( rc == SXERR_ABORT ){` |
|         - |  8525 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8526 | `			return SXERR_ABORT;` |
|         - |  8527 | `		}` |
|         3 |  8528 | `		goto Synchronize;` |
|         - |  8529 | `	}` |
|         - |  8530 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8531 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    469845 |  8532 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8533 | `		const char *zAvErr = 0;` |
|        19 |  8534 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8535 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8536 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8537 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8538 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8539 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8540 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8541 | `		}` |
|        13 |  8542 | `		if( zAvErr ){` |
|       ! 0 |  8543 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8544 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8545 | `				return SXERR_ABORT;` |
|         - |  8546 | `			}` |
|       ! 0 |  8547 | `			goto Synchronize;` |
|         - |  8548 | `		}` |
|         6 |  8549 | `	}` |
|         - |  8550 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8551 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    469845 |  8552 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8553 | `		const char *zRoErr = 0;` |
|        43 |  8554 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8555 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8556 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8557 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8558 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8559 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8560 | `		}` |
|        43 |  8561 | `		if( zRoErr ){` |
|        13 |  8562 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8563 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8564 | `				return SXERR_ABORT;` |
|         - |  8565 | `			}` |
|        13 |  8566 | `			goto Synchronize;` |
|         - |  8567 | `		}` |
|        14 |  8568 | `	}` |
|         - |  8569 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8570 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8571 | `	 * by the type parser. */` |
|    469835 |  8572 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       485 |  8573 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8574 | `			&sTypeText,` |
|       320 |  8575 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       160 |  8576 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       325 |  8577 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8578 | `			return SXERR_ABORT;` |
|       325 |  8579 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8580 | `			goto Synchronize;` |
|         - |  8581 | `		}` |
|       160 |  8582 | `	}` |
|         - |  8583 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    469835 |  8584 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8585 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8586 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8587 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8588 | `			return SXERR_ABORT;` |
|         - |  8589 | `		}` |
|         3 |  8590 | `		goto Synchronize;` |
|         - |  8591 | `	}` |
|         - |  8592 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8593 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8594 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8595 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8596 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8597 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    469833 |  8598 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8599 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8600 | `			"New expressions are not supported in this context");` |
|         6 |  8601 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8602 | `			return SXERR_ABORT;` |
|         - |  8603 | `		}` |
|         6 |  8604 | `		goto Synchronize;` |
|         - |  8605 | `	}` |
|         - |  8606 | `	/* Allocate a new class attribute */` |
|    469829 |  8607 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    469829 |  8608 | `	if( pAttr ){` |
|    469829 |  8609 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    469829 |  8610 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8611 | `			return SXERR_ABORT;` |
|         - |  8612 | `		}` |
|    234912 |  8613 | `	}` |
|    469829 |  8614 | `	if( pAttr == 0 ){` |
|       ! 0 |  8615 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8616 | `		return SXERR_ABORT;` |
|         - |  8617 | `	}` |
|    469829 |  8618 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       323 |  8619 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       159 |  8620 | `	}` |
|    469829 |  8621 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8622 | `		SySet *pInstrContainer;` |
|    343467 |  8623 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    343467 |  8624 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8625 | `		{` |
|         - |  8626 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8627 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8628 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8629 | `			 * compiler would otherwise run into the hook tokens. */` |
|    343467 |  8630 | `			SyToken *pScan = pGen->pIn;` |
|    343467 |  8631 | `			sxi32 iNest = 0;` |
|    742435 |  8632 | `			while( pScan < pGen->pEnd ){` |
|    742435 |  8633 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     43427 |  8634 | `					iNest++;` |
|    720724 |  8635 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     43427 |  8636 | `					iNest--;` |
|    677302 |  8637 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    343467 |  8638 | `					break;` |
|         - |  8639 | `				}` |
|    398973 |  8640 | `				pScan++;` |
|         5 |  8641 | `			}` |
|    343467 |  8642 | `			pGen->pEnd = pScan;` |
|         - |  8643 | `		}` |
|         - |  8644 | `		/* Swap bytecode container */` |
|    343467 |  8645 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    343467 |  8646 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8647 | `		/* Compile attribute value.` |
|         - |  8648 | `		 */` |
|    343467 |  8649 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    343467 |  8650 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8651 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8652 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8653 | `				return SXERR_ABORT;` |
|         - |  8654 | `			}` |
|       ! 0 |  8655 | `		}` |
|         - |  8656 | `		/* Emit the done instruction */` |
|    343467 |  8657 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    343467 |  8658 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    343467 |  8659 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    343467 |  8660 | `		pGen->pEnd = pSavedDefEnd;` |
|    171731 |  8661 | `	}` |
|         - |  8662 | `	/* All done,install the attribute */` |
|    469829 |  8663 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    469829 |  8664 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8665 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8666 | `		return SXERR_ABORT;` |
|         - |  8667 | `	}` |
|    469829 |  8668 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8669 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8670 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8671 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8672 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8673 | `			return SXERR_ABORT;` |
|         - |  8674 | `		}` |
|        95 |  8675 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8676 | `			goto Synchronize;` |
|         - |  8677 | `		}` |
|        95 |  8678 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8679 | `		return SXRET_OK;` |
|         - |  8680 | `	}` |
|    469735 |  8681 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8682 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8683 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8684 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8685 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8686 | `				? "Interfaces may only include hooked properties"` |
|         - |  8687 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8688 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8689 | `			return SXERR_ABORT;` |
|         - |  8690 | `		}` |
|       ! 0 |  8691 | `		goto Synchronize;` |
|         - |  8692 | `	}` |
|    469735 |  8693 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8694 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8695 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8696 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8697 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8698 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8699 | `				pTok--;` |
|       ! 0 |  8700 | `			}` |
|       ! 0 |  8701 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8702 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8703 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8704 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8705 | `				return SXERR_ABORT;` |
|         - |  8706 | `			}` |
|       ! 0 |  8707 | `		}else{` |
|         5 |  8708 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8709 | `				goto loop;` |
|         - |  8710 | `			}` |
|         - |  8711 | `		}` |
|       ! 0 |  8712 | `	}` |
|    469731 |  8713 | `	SySetRelease(&aUnionAlts);` |
|    469731 |  8714 | `	return SXRET_OK;` |
|         9 |  8715 | `Synchronize:` |
|         - |  8716 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8717 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  8718 | `		pGen->pIn++;` |
|         3 |  8719 | `	}` |
|        22 |  8720 | `	SySetRelease(&aUnionAlts);` |
|        22 |  8721 | `	return SXERR_CORRUPT;` |
|    234924 |  8722 | `}` |
|         - |  8723 | `/*` |
|         - |  8724 | ` * Compile a class method.` |
|         - |  8725 | ` *` |
|         - |  8726 | ` * Refer to the official documentation for more information` |
|         - |  8727 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  8728 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  8729 | ` * overloading and many more.` |
|         - |  8730 | ` */` |
|   2460170 |  8731 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  8732 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  8733 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  8734 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  8735 | `	int doBody,          /* TRUE to process method body */` |
|         - |  8736 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  8737 | `	)` |
|         5 |  8738 | `{` |
|   2460175 |  8739 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2460175 |  8740 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  8741 | `	ph7_class_method *pMeth;` |
|         - |  8742 | `	sxi32 iFuncFlags;` |
|         - |  8743 | `	SyString *pName;` |
|         - |  8744 | `	SyToken *pEnd;` |
|         - |  8745 | `	sxi32 rc;` |
|         - |  8746 | `	/* Extract visibility level */` |
|   2460175 |  8747 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2460175 |  8748 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2460175 |  8749 | `	iFuncFlags = 0;` |
|   2460175 |  8750 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  8751 | `		/* Invalid method name */` |
|       ! 0 |  8752 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8753 | `		if( rc == SXERR_ABORT ){` |
|         - |  8754 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8755 | `			return SXERR_ABORT;` |
|         - |  8756 | `		}` |
|       ! 0 |  8757 | `		goto Synchronize;` |
|         - |  8758 | `	}` |
|   2460175 |  8759 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8760 | `		/* Return by reference,remember that */` |
|       ! 0 |  8761 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8762 | `		/* Jump the '&' token */` |
|       ! 0 |  8763 | `		pGen->pIn++;` |
|       ! 0 |  8764 | `	}` |
|   2460175 |  8765 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8766 | `		/* Invalid method name */` |
|       ! 0 |  8767 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8768 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8769 | `			return SXERR_ABORT;` |
|         - |  8770 | `		}` |
|       ! 0 |  8771 | `		goto Synchronize;` |
|         - |  8772 | `	}` |
|         - |  8773 | `	/* Peek method name */` |
|   2460175 |  8774 | `	pName = &pGen->pIn->sData;` |
|   2460175 |  8775 | `	nLine = pGen->pIn->nLine;` |
|         - |  8776 | `	/* Jump the method name */` |
|   2460175 |  8777 | `	pGen->pIn++;` |
|   2460175 |  8778 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8779 | `		/* Abstract method */` |
|    141907 |  8780 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  8781 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8782 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  8783 | `				&pClass->sName,pName);` |
|       ! 0 |  8784 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8785 | `				return SXERR_ABORT;` |
|         - |  8786 | `			}` |
|       ! 0 |  8787 | `		}` |
|         - |  8788 | `		/* Assemble method signature only */` |
|    141907 |  8789 | `		doBody = FALSE;` |
|     70951 |  8790 | `	}` |
|   2460175 |  8791 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8792 | `		/* Syntax error */` |
|       ! 0 |  8793 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  8794 | `		if( rc == SXERR_ABORT ){` |
|         - |  8795 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8796 | `			return SXERR_ABORT;` |
|         - |  8797 | `		}` |
|       ! 0 |  8798 | `		goto Synchronize;` |
|         - |  8799 | `	}` |
|         - |  8800 | `	/* Allocate a new class_method instance */` |
|   2460175 |  8801 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2460175 |  8802 | `	if( pMeth == 0 ){` |
|       ! 0 |  8803 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8804 | `		return SXERR_ABORT;` |
|         - |  8805 | `	}` |
|   2460175 |  8806 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2460175 |  8807 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2460175 |  8808 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8809 | `		return SXERR_ABORT;` |
|         - |  8810 | `	}` |
|         - |  8811 | `	/* Jump the left parenthesis '(' */` |
|   2460175 |  8812 | `	pGen->pIn++;` |
|   2460175 |  8813 | `	pEnd = 0; /* cc warning */` |
|         - |  8814 | `	/* Delimit the method signature */` |
|   2460175 |  8815 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2460175 |  8816 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  8817 | `		/* Syntax error */` |
|         3 |  8818 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  8819 | `		if( rc == SXERR_ABORT ){` |
|         - |  8820 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8821 | `			return SXERR_ABORT;` |
|         - |  8822 | `		}` |
|         3 |  8823 | `		goto Synchronize;` |
|         - |  8824 | `	}` |
|         - |  8825 | `	{` |
|   2460173 |  8826 | `		int bIsCtor = 0;` |
|   2460173 |  8827 | `		int bAbstractCtor = 0;` |
|   2460168 |  8828 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1437094 |  8829 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2375359 |  8830 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    169633 |  8831 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  8832 | `				bAbstractCtor = 1;` |
|         2 |  8833 | `			}else{` |
|    169631 |  8834 | `				bIsCtor = 1;` |
|         - |  8835 | `			}` |
|     84814 |  8836 | `		}` |
|   2460173 |  8837 | `		if( pGen->pIn < pEnd ){` |
|         - |  8838 | `			/* Collect method arguments */` |
|    883243 |  8839 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    883243 |  8840 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8841 | `				return SXERR_ABORT;` |
|         - |  8842 | `			}` |
|    441619 |  8843 | `		}` |
|         - |  8844 | `	}` |
|         - |  8845 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2460173 |  8846 | `	pGen->pIn = &pEnd[1];` |
|         - |  8847 | `	{` |
|   2460173 |  8848 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2460173 |  8849 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  8850 | `			return SXERR_ABORT;` |
|   2460173 |  8851 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  8852 | `			goto Synchronize;` |
|         - |  8853 | `		}` |
|         - |  8854 | `	}` |
|         - |  8855 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  8856 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  8857 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  8858 | `	{` |
|   2460173 |  8859 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  8860 | `		sxu32 i;` |
|   3784869 |  8861 | `		for( i = 0; i < nArg; i++ ){` |
|   1324711 |  8862 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  8863 | `			ph7_class_attr *pAttr;` |
|   1324711 |  8864 | `			sxi32 iAttrFlags = 0;` |
|         - |  8865 | `			int bArgTyped;` |
|   1324711 |  8866 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1324627 |  8867 | `				continue;` |
|         - |  8868 | `			}` |
|         - |  8869 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  8870 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  8871 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  8872 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  8873 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  8874 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  8875 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8876 | `					"Cannot declare variadic promoted property");` |
|         3 |  8877 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8878 | `					return SXERR_ABORT;` |
|         - |  8879 | `				}` |
|         3 |  8880 | `				goto Synchronize;` |
|         - |  8881 | `			}` |
|         - |  8882 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  8883 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  8884 | `			 * appear as an alternative of a union type. */` |
|        87 |  8885 | `			if( bArgTyped ){` |
|       122 |  8886 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  8887 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  8888 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  8889 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  8890 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8891 | `					return SXERR_ABORT;` |
|        83 |  8892 | `				}else if( rc != SXRET_OK ){` |
|         6 |  8893 | `					goto Synchronize;` |
|         - |  8894 | `				}` |
|        37 |  8895 | `			}` |
|         - |  8896 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  8897 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  8898 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8899 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  8900 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8901 | `					return SXERR_ABORT;` |
|         - |  8902 | `				}` |
|         3 |  8903 | `				goto Synchronize;` |
|         - |  8904 | `			}` |
|        81 |  8905 | `			if( bArgTyped ){` |
|        77 |  8906 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  8907 | `			}` |
|        81 |  8908 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  8909 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  8910 | `			}` |
|        81 |  8911 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  8912 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  8913 | `			}` |
|        81 |  8914 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  8915 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  8916 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  8917 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  8918 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8919 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  8920 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8921 | `						return SXERR_ABORT;` |
|         - |  8922 | `					}` |
|         3 |  8923 | `					goto Synchronize;` |
|         - |  8924 | `				}` |
|        24 |  8925 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  8926 | `			}` |
|        79 |  8927 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  8928 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  8929 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8930 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8931 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  8932 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  8933 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8934 | `						return SXERR_ABORT;` |
|         - |  8935 | `					}` |
|       ! 0 |  8936 | `					goto Synchronize;` |
|         - |  8937 | `				}` |
|         5 |  8938 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  8939 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  8940 | `			}` |
|        79 |  8941 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  8942 | `			if( pAttr == 0 ){` |
|       ! 0 |  8943 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8944 | `				return SXERR_ABORT;` |
|         - |  8945 | `			}` |
|        79 |  8946 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  8947 | `				pAttr->nType = pArg->nType;` |
|        77 |  8948 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  8949 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  8950 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8951 | `					sxu32 k;` |
|        20 |  8952 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  8953 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  8954 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  8955 | `					}` |
|         3 |  8956 | `				}` |
|        36 |  8957 | `			}` |
|        79 |  8958 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  8959 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  8960 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8961 | `				return SXERR_ABORT;` |
|         - |  8962 | `			}` |
|        42 |  8963 | `		}` |
|         - |  8964 | `	}` |
|   2460163 |  8965 | `	if( doBody ){` |
|         - |  8966 | `		/* Compile method body */` |
|   2318261 |  8967 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2318261 |  8968 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8969 | `			return SXERR_ABORT;` |
|         - |  8970 | `		}` |
|         - |  8971 | `		/* The cursor sits just past the body's closing brace */` |
|   2318261 |  8972 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1159133 |  8973 | `	}else{` |
|         - |  8974 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    141907 |  8975 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    141907 |  8976 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     70951 |  8977 | `		}` |
|         - |  8978 | `		/* Only method signature is allowed */` |
|    141907 |  8979 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  8980 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  8981 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  8982 | `				if( rc == SXERR_ABORT ){` |
|         - |  8983 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  8984 | `					return SXERR_ABORT;` |
|         - |  8985 | `				}` |
|       ! 0 |  8986 | `				return SXERR_CORRUPT;` |
|         - |  8987 | `			}` |
|         - |  8988 | `	}` |
|         - |  8989 | `	/* All done,install the method */` |
|   2460163 |  8990 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2460163 |  8991 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8992 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8993 | `		return SXERR_ABORT;` |
|         - |  8994 | `	}` |
|   2460163 |  8995 | `	return SXRET_OK;` |
|         6 |  8996 | `Synchronize:` |
|         - |  8997 | `	/* Synchronize with the first semi-colon */` |
|        40 |  8998 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  8999 | `		pGen->pIn++;` |
|         4 |  9000 | `	}` |
|        16 |  9001 | `	return SXERR_CORRUPT;` |
|   1230090 |  9002 | `}` |
|         - |  9003 | `/*` |
|         - |  9004 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9005 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9006 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9007 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9008 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9009 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9010 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9011 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9012 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9013 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9014 | `` * implicit `$value` formal.`` |
|         - |  9015 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9016 | ` */` |
|         - |  9017 | `/*` |
|         - |  9018 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9019 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9020 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9021 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9022 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9023 | ` */` |
|        94 |  9024 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9025 | `{` |
|         - |  9026 | `	SyToken *p;` |
|       345 |  9027 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9028 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9029 | `			continue;` |
|         - |  9030 | `		}` |
|         - |  9031 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9032 | `		if( p + 3 < pEnd` |
|        80 |  9033 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9034 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9035 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9036 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9037 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9038 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9039 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9040 | `			return 1;` |
|         - |  9041 | `		}` |
|         - |  9042 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9043 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9044 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9045 | `		if( p > pStart` |
|        26 |  9046 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9047 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9048 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9049 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9050 | `			return 1;` |
|         - |  9051 | `		}` |
|        15 |  9052 | `	}` |
|        43 |  9053 | `	return 0;` |
|        48 |  9054 | `}` |
|         - |  9055 | `/*` |
|         - |  9056 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9057 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9058 | ` */` |
|       990 |  9059 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9060 | `{` |
|      1167 |  9061 | `	return p + 6 < pEnd` |
|       671 |  9062 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9063 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9064 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9065 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9066 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9067 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9068 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9069 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9070 | `	 && p[5].sData.nByte == 3` |
|         8 |  9071 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9072 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9073 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9074 | `}` |
|         - |  9075 | `/*` |
|         - |  9076 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9077 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9078 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9079 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9080 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9081 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9082 | ` * or SXERR_MEM.` |
|         - |  9083 | ` */` |
|         4 |  9084 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9085 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9086 | `{` |
|         5 |  9087 | `	SyToken *p = pStart;` |
|        35 |  9088 | `	while( p < pEnd ){` |
|        31 |  9089 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9090 | `			SyToken sTok;` |
|         - |  9091 | `			char zName[384];` |
|         - |  9092 | `			sxu32 nName;` |
|         - |  9093 | `			char *zDup;` |
|         - |  9094 | ``			/* `parent` `::` */`` |
|         5 |  9095 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9096 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9097 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9098 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9099 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9100 | `			if( zDup == 0 ){` |
|       ! 0 |  9101 | `				return SXERR_MEM;` |
|         - |  9102 | `			}` |
|         5 |  9103 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9104 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9105 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9106 | `			sTok.pUserData = 0;` |
|         5 |  9107 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9108 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9109 | `			continue;` |
|         - |  9110 | `		}` |
|        27 |  9111 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9112 | `		p++;` |
|         1 |  9113 | `	}` |
|         5 |  9114 | `	return SXRET_OK;` |
|         3 |  9115 | `}` |
|        94 |  9116 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9117 | `{` |
|        95 |  9118 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9119 | `	sxi32 rc;` |
|        95 |  9120 | `	int bRefsSelf = 0;` |
|        95 |  9121 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9122 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9123 | `		char zHook[384];` |
|         - |  9124 | `		SyString sHookName;` |
|         - |  9125 | `		ph7_class_method *pMeth;` |
|         - |  9126 | `		int bGet;` |
|       159 |  9127 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9128 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9129 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9130 | `			continue;` |
|         - |  9131 | `		}` |
|       145 |  9132 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9133 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9134 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9135 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9136 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9137 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9138 | `				return SXERR_ABORT;` |
|         - |  9139 | `			}` |
|       ! 0 |  9140 | `			return SXERR_CORRUPT;` |
|         - |  9141 | `		}` |
|       145 |  9142 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9143 | `			goto HookSyntax;` |
|         - |  9144 | `		}` |
|       144 |  9145 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9146 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9147 | `			bGet = 1;` |
|       106 |  9148 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9149 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9150 | `			bGet = 0;` |
|        34 |  9151 | `		}else{` |
|       ! 0 |  9152 | `			goto HookSyntax;` |
|         - |  9153 | `		}` |
|       145 |  9154 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9155 | `		sHookName.zString = zHook;` |
|       217 |  9156 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9157 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9158 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9159 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9160 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9161 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9162 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9163 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9164 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9165 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9166 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9167 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9168 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9169 | `					return SXERR_ABORT;` |
|         - |  9170 | `				}` |
|       ! 0 |  9171 | `				return SXERR_CORRUPT;` |
|         - |  9172 | `			}` |
|        15 |  9173 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9174 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9175 | `			if( pMeth == 0 ){` |
|       ! 0 |  9176 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9177 | `				return SXERR_ABORT;` |
|         - |  9178 | `			}` |
|        15 |  9179 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9180 | `			if( !bGet ){` |
|         - |  9181 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9182 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9183 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9184 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9185 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9186 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9187 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9188 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9189 | `				if( zVName == 0 ){` |
|       ! 0 |  9190 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9191 | `					return SXERR_ABORT;` |
|         - |  9192 | `				}` |
|         7 |  9193 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9194 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9195 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9196 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9197 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9198 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9199 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9200 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9201 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9202 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9203 | `				}` |
|         7 |  9204 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9205 | `			}` |
|        15 |  9206 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9207 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9208 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9209 | `				return SXERR_ABORT;` |
|         - |  9210 | `			}` |
|        15 |  9211 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9212 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9213 | `		}` |
|       130 |  9214 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9215 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9216 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9217 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9218 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9219 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9220 | `				return SXERR_ABORT;` |
|         - |  9221 | `			}` |
|       ! 0 |  9222 | `			return SXERR_CORRUPT;` |
|         - |  9223 | `		}` |
|       131 |  9224 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9225 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9226 | `		if( pMeth == 0 ){` |
|       ! 0 |  9227 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9228 | `			return SXERR_ABORT;` |
|         - |  9229 | `		}` |
|       131 |  9230 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9231 | `		if( !bGet ){` |
|         - |  9232 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9233 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9234 | `				SyToken *pRp = 0;` |
|        17 |  9235 | `				pGen->pIn++;` |
|        17 |  9236 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9237 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9238 | `					goto HookSyntax;` |
|         - |  9239 | `				}` |
|        17 |  9240 | `				if( pGen->pIn < pRp ){` |
|        17 |  9241 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9242 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9243 | `						return SXERR_ABORT;` |
|         - |  9244 | `					}` |
|         8 |  9245 | `				}` |
|        17 |  9246 | `				pGen->pIn = &pRp[1];` |
|         8 |  9247 | `			}` |
|        61 |  9248 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9249 | `				/* Implicit $value formal */` |
|         - |  9250 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9251 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9252 | `				if( zVName == 0 ){` |
|       ! 0 |  9253 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9254 | `					return SXERR_ABORT;` |
|         - |  9255 | `				}` |
|        45 |  9256 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9257 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9258 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9259 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9260 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9261 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9262 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9263 | `			}` |
|        30 |  9264 | `		}` |
|       165 |  9265 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9266 | `			/* Block body */` |
|        69 |  9267 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9268 | `			SyToken *pCloser = 0;` |
|        69 |  9269 | `			int bParentCall = 0;` |
|        69 |  9270 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9271 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9272 | `				SyToken *pScan;` |
|       753 |  9273 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9274 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9275 | `						bParentCall = 1;` |
|         3 |  9276 | `						break;` |
|         - |  9277 | `					}` |
|       343 |  9278 | `				}` |
|        34 |  9279 | `			}` |
|        69 |  9280 | `			if( bParentCall ){` |
|         - |  9281 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9282 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9283 | `				 * hook method), then continue past the original body. */` |
|         - |  9284 | `				SySet sBody;` |
|         3 |  9285 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9286 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9287 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9288 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9289 | `					SySetRelease(&sBody);` |
|       ! 0 |  9290 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9291 | `					return SXERR_ABORT;` |
|         - |  9292 | `				}` |
|         3 |  9293 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9294 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9295 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9296 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9297 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9298 | `				SySetRelease(&sBody);` |
|         3 |  9299 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9300 | `					return SXERR_ABORT;` |
|         - |  9301 | `				}` |
|         3 |  9302 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9303 | `			}else{` |
|        67 |  9304 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9305 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9306 | `					return SXERR_ABORT;` |
|         - |  9307 | `				}` |
|        67 |  9308 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9309 | `			}` |
|        69 |  9310 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9311 | `				bRefsSelf = 1;` |
|         9 |  9312 | `			}` |
|       128 |  9313 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9314 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9315 | `			GenBlock *pBlock;` |
|         - |  9316 | `			SySet *pInstrContainer;` |
|         - |  9317 | `			SyToken *pBodyStart;` |
|         - |  9318 | `			SyToken *pExprEnd;` |
|        63 |  9319 | `			SyToken *pSavedEnd = 0;` |
|         - |  9320 | `			SySet sBody;` |
|        63 |  9321 | `			int bParentCall = 0;` |
|        63 |  9322 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9323 | `			pBodyStart = pGen->pIn;` |
|         - |  9324 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9325 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9326 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9327 | `			 * method on a token copy. */` |
|         - |  9328 | `			{` |
|        63 |  9329 | `				sxi32 iNest = 0;` |
|        63 |  9330 | `				pExprEnd = pBodyStart;` |
|       355 |  9331 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9332 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9333 | `						iNest++;` |
|       351 |  9334 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9335 | `						if( iNest <= 0 ){` |
|       ! 0 |  9336 | `							break;` |
|         - |  9337 | `						}` |
|         9 |  9338 | `						iNest--;` |
|       343 |  9339 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9340 | `						break;` |
|         - |  9341 | `					}` |
|       293 |  9342 | `					pExprEnd++;` |
|         1 |  9343 | `				}` |
|         - |  9344 | `			}` |
|         - |  9345 | `			{` |
|         - |  9346 | `				SyToken *pScan;` |
|       335 |  9347 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9348 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9349 | `						bParentCall = 1;` |
|         3 |  9350 | `						break;` |
|         - |  9351 | `					}` |
|       137 |  9352 | `				}` |
|         - |  9353 | `			}` |
|        63 |  9354 | `			if( bParentCall ){` |
|         3 |  9355 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9356 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9357 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9358 | `					SySetRelease(&sBody);` |
|       ! 0 |  9359 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9360 | `					return SXERR_ABORT;` |
|         - |  9361 | `				}` |
|         3 |  9362 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9363 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9364 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9365 | `			}` |
|        94 |  9366 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9367 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9368 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9369 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9370 | `				return SXERR_ABORT;` |
|         - |  9371 | `			}` |
|        63 |  9372 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9373 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9374 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9375 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9376 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9377 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9378 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9379 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9380 | `			if( bParentCall ){` |
|         3 |  9381 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9382 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9383 | `				SySetRelease(&sBody);` |
|         1 |  9384 | `			}` |
|        63 |  9385 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9386 | `				return SXERR_ABORT;` |
|         - |  9387 | `			}` |
|        63 |  9388 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9389 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9390 | `				bRefsSelf = 1;` |
|        18 |  9391 | `			}` |
|        63 |  9392 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9393 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9394 | `			}` |
|        63 |  9395 | `			if( !bGet ){` |
|         - |  9396 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9397 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9398 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9399 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9400 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9401 | `				bRefsSelf = 1;` |
|         1 |  9402 | `			}` |
|        32 |  9403 | `		}else{` |
|       ! 0 |  9404 | `			goto HookSyntax;` |
|         - |  9405 | `		}` |
|       131 |  9406 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9407 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9408 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9409 | `			return SXERR_ABORT;` |
|         - |  9410 | `		}` |
|       131 |  9411 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9412 | `	}` |
|        95 |  9413 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9414 | `		goto HookSyntax;` |
|         - |  9415 | `	}` |
|        95 |  9416 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9417 | `	if( !bRefsSelf ){` |
|         - |  9418 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9419 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9420 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9421 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9422 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9423 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9424 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9425 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9426 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9427 | `				return SXERR_ABORT;` |
|         - |  9428 | `			}` |
|       ! 0 |  9429 | `			return SXERR_CORRUPT;` |
|         - |  9430 | `		}` |
|        20 |  9431 | `	}` |
|        95 |  9432 | `	return SXRET_OK;` |
|       ! 0 |  9433 | `HookSyntax:` |
|       ! 0 |  9434 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9435 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9436 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9437 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9438 | `		return SXERR_ABORT;` |
|         - |  9439 | `	}` |
|       ! 0 |  9440 | `	return SXERR_CORRUPT;` |
|        48 |  9441 | `}` |
|         - |  9442 | `/*` |
|         - |  9443 | ` * Compile an object interface.` |
|         - |  9444 | ` *  According to the PHP language reference manual` |
|         - |  9445 | ` *   Object Interfaces:` |
|         - |  9446 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9447 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9448 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9449 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9450 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9451 | ` */` |
|     71024 |  9452 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9453 | `{` |
|     71029 |  9454 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9455 | `	ph7_class *pClass,*pBase;` |
|         - |  9456 | `	SyToken *pEnd,*pTmp;` |
|         - |  9457 | `	SyString *pName;` |
|         - |  9458 | `	sxi32 nKwrd;` |
|         - |  9459 | `	sxi32 rc;` |
|         - |  9460 | `	/* Jump the 'interface' keyword */` |
|     71029 |  9461 | `	pGen->pIn++;` |
|         - |  9462 | `	/* Extract interface name */` |
|     71029 |  9463 | `	pName = &pGen->pIn->sData;` |
|         - |  9464 | `	/* Advance the stream cursor */` |
|     71029 |  9465 | `	pGen->pIn++;` |
|         - |  9466 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9467 | `		SyBlob sFQN;` |
|         - |  9468 | `		SyString sFQNStr;` |
|     71029 |  9469 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     71029 |  9470 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     71029 |  9471 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     71029 |  9472 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     71029 |  9473 | `		SyBlobRelease(&sFQN);` |
|         - |  9474 | `	}` |
|     71029 |  9475 | `	if( pClass == 0 ){` |
|       ! 0 |  9476 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9477 | `		return SXERR_ABORT;` |
|         - |  9478 | `	}` |
|     71029 |  9479 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     71029 |  9480 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9481 | `		return SXERR_ABORT;` |
|         - |  9482 | `	}` |
|         - |  9483 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     71029 |  9484 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9485 | `	/* Assume no base class is given */` |
|     71029 |  9486 | `	pBase = 0;` |
|     71029 |  9487 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     27595 |  9488 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     27595 |  9489 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9490 | `			SyBlob sResolved;` |
|         - |  9491 | `			SyString sBaseName;` |
|         - |  9492 | `			sxu32 nRefLine;` |
|         - |  9493 | `			/* Extract base interface */` |
|     27595 |  9494 | `			pGen->pIn++;` |
|     27595 |  9495 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     27595 |  9496 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     27595 |  9497 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9498 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9499 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9500 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9501 | `					pName);` |
|       ! 0 |  9502 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9503 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9504 | `					return SXERR_ABORT;` |
|         - |  9505 | `				}` |
|       ! 0 |  9506 | `				return SXRET_OK;` |
|         - |  9507 | `			}` |
|     41390 |  9508 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     27590 |  9509 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     27595 |  9510 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9511 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9512 | `			/* Only interfaces is allowed */` |
|     27595 |  9513 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9514 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9515 | `			}` |
|     27595 |  9516 | `			if( pBase == 0 ){` |
|       ! 0 |  9517 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9518 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9519 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9520 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9521 | `					return SXERR_ABORT;` |
|         - |  9522 | `				}` |
|       ! 0 |  9523 | `			}` |
|     27595 |  9524 | `			SyBlobRelease(&sResolved);` |
|     13795 |  9525 | `		}` |
|     13795 |  9526 | `	}` |
|     71029 |  9527 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9528 | `		/* Syntax error */` |
|       ! 0 |  9529 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9530 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9531 | `		if( rc == SXERR_ABORT ){` |
|         - |  9532 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9533 | `			return SXERR_ABORT;` |
|         - |  9534 | `		}` |
|       ! 0 |  9535 | `		return SXRET_OK;` |
|         - |  9536 | `	}` |
|     71029 |  9537 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     71029 |  9538 | `	pEnd = 0; /* cc warning */` |
|         - |  9539 | `	/* Delimit the interface body */` |
|     71029 |  9540 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     71029 |  9541 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9542 | `		/* Syntax error */` |
|       ! 0 |  9543 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9544 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9545 | `		if( rc == SXERR_ABORT ){` |
|         - |  9546 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9547 | `			return SXERR_ABORT;` |
|         - |  9548 | `		}` |
|       ! 0 |  9549 | `		return SXRET_OK;` |
|         - |  9550 | `	}` |
|         - |  9551 | `	/* The delimiter token is the interface body's closing brace */` |
|     71029 |  9552 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9553 | `	/* Swap token stream */` |
|     71029 |  9554 | `	pTmp = pGen->pEnd;` |
|     71029 |  9555 | `	pGen->pEnd = pEnd;` |
|         - |  9556 | `	/* Start the parse process` |
|         - |  9557 | `	 * Note (According to the PHP reference manual):` |
|         - |  9558 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9559 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9560 | `	 */` |
|    130101 |  9561 | `	for(;;){` |
|         - |  9562 | `		/* Jump leading/trailing semi-colons */` |
|    449389 |  9563 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    189183 |  9564 | `			pGen->pIn++;` |
|         5 |  9565 | `		}` |
|    260211 |  9566 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9567 | `			/* End of interface body */` |
|     71025 |  9568 | `			break;` |
|         - |  9569 | `		}` |
|         - |  9570 | `		/* Bind a directly-preceding docblock to this member */` |
|    189191 |  9571 | `		GenStateSetPendingDoc(&(*pGen));` |
|    189191 |  9572 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9573 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9574 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9575 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9576 | `			if( rc == SXERR_ABORT ){` |
|         - |  9577 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9578 | `				return SXERR_ABORT;` |
|         - |  9579 | `			}` |
|       ! 0 |  9580 | `			goto done;` |
|         - |  9581 | `		}` |
|         - |  9582 | `		/* Extract the current keyword */` |
|    189191 |  9583 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    189191 |  9584 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9585 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9586 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9587 | `			const char *zKind = "member";` |
|         3 |  9588 | `			SyString *pMemberName = 0;` |
|         3 |  9589 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9590 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9591 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9592 | `					zKind = "constant";` |
|         3 |  9593 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9594 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9595 | `					}` |
|         1 |  9596 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9597 | `					zKind = "method";` |
|       ! 0 |  9598 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9599 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9600 | `					}` |
|       ! 0 |  9601 | `				}` |
|         1 |  9602 | `			}` |
|         3 |  9603 | `			if( pMemberName ){` |
|         4 |  9604 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9605 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9606 | `			}else{` |
|       ! 0 |  9607 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9608 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9609 | `			}` |
|         3 |  9610 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9611 | `				return SXERR_ABORT;` |
|         - |  9612 | `			}` |
|         3 |  9613 | `			goto done;` |
|         - |  9614 | `		}` |
|    189189 |  9615 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9616 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9617 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9618 | `			if( rc == SXERR_ABORT ){` |
|         - |  9619 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9620 | `				return SXERR_ABORT;` |
|         - |  9621 | `			}` |
|       ! 0 |  9622 | `			goto done;` |
|         - |  9623 | `		}` |
|    189189 |  9624 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9625 | `			/* Advance the stream cursor */` |
|    134011 |  9626 | `			pGen->pIn++;` |
|    134006 |  9627 | `			if( pGen->pIn < pGen->pEnd` |
|    134011 |  9628 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    134006 |  9629 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9630 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9631 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9632 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9633 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9634 | `				 * hooked properties" error). */` |
|       ! 0 |  9635 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9636 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9637 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9638 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9639 | `						return SXERR_ABORT;` |
|         - |  9640 | `					}` |
|       ! 0 |  9641 | `					goto done;` |
|         - |  9642 | `				}` |
|       ! 0 |  9643 | `				continue;` |
|         - |  9644 | `			}` |
|    134011 |  9645 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9646 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9647 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9648 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9649 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9650 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9651 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9652 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9653 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9654 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9655 | `							return SXERR_ABORT;` |
|         - |  9656 | `						}` |
|       ! 0 |  9657 | `						goto done;` |
|         - |  9658 | `					}` |
|       ! 0 |  9659 | `					continue;` |
|         - |  9660 | `				}` |
|       ! 0 |  9661 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9662 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9663 | `				if( rc == SXERR_ABORT ){` |
|         - |  9664 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9665 | `					return SXERR_ABORT;` |
|         - |  9666 | `				}` |
|       ! 0 |  9667 | `				goto done;` |
|         - |  9668 | `			}` |
|    134011 |  9669 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    134011 |  9670 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9671 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9672 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9673 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9674 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9675 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9676 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9677 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9678 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9679 | `							return SXERR_ABORT;` |
|         - |  9680 | `						}` |
|       ! 0 |  9681 | `						goto done;` |
|         - |  9682 | `					}` |
|         5 |  9683 | `					continue;` |
|         - |  9684 | `				}` |
|       ! 0 |  9685 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9686 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9687 | `				if( rc == SXERR_ABORT ){` |
|         - |  9688 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9689 | `					return SXERR_ABORT;` |
|         - |  9690 | `				}` |
|       ! 0 |  9691 | `				goto done;` |
|         - |  9692 | `			}` |
|     67001 |  9693 | `		}` |
|    189185 |  9694 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9695 | `			/* Parse constant */` |
|     55179 |  9696 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     55179 |  9697 | `			if( rc != SXRET_OK ){` |
|         3 |  9698 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9699 | `					return SXERR_ABORT;` |
|         - |  9700 | `				}` |
|         3 |  9701 | `				goto done;` |
|         - |  9702 | `			}` |
|     27591 |  9703 | `		}else{` |
|    134011 |  9704 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    134011 |  9705 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9706 | `				/* Static method,record that */` |
|     11825 |  9707 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9708 | `				/* Advance the stream cursor */` |
|     11825 |  9709 | `				pGen->pIn++;` |
|     11820 |  9710 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11825 |  9711 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9712 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9713 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9714 | `						if( rc == SXERR_ABORT ){` |
|         - |  9715 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9716 | `							return SXERR_ABORT;` |
|         - |  9717 | `						}` |
|       ! 0 |  9718 | `						goto done;` |
|         - |  9719 | `				}` |
|      5910 |  9720 | `			}` |
|         - |  9721 | `			/* Process method signature (no body for interface methods) */` |
|    134011 |  9722 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    134011 |  9723 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9724 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9725 | `					return SXERR_ABORT;` |
|         - |  9726 | `				}` |
|       ! 0 |  9727 | `				goto done;` |
|         - |  9728 | `			}` |
|         - |  9729 | `		}` |
|         5 |  9730 | `	}` |
|         - |  9731 | `	/* Install the interface */` |
|     71025 |  9732 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     71025 |  9733 | `	if( rc == SXRET_OK && pBase ){` |
|         - |  9734 | `		/* Inherit from the base interface */` |
|     27595 |  9735 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13795 |  9736 | `	}` |
|     71025 |  9737 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9738 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9739 | `		return SXERR_ABORT;` |
|         - |  9740 | `	}` |
|     35510 |  9741 | `done:` |
|         - |  9742 | `	/* Point beyond the interface body */` |
|     71029 |  9743 | `	pGen->pIn  = &pEnd[1];` |
|     71029 |  9744 | `	pGen->pEnd = pTmp;` |
|     71029 |  9745 | `	return PH7_OK;` |
|     35517 |  9746 | `}` |
|         - |  9747 | `/*` |
|         - |  9748 | ` * Compile a user-defined class.` |
|         - |  9749 | ` * According to the PHP language reference manual` |
|         - |  9750 | ` *  class` |
|         - |  9751 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - |  9752 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - |  9753 | ` *  of the properties and methods belonging to the class.` |
|         - |  9754 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - |  9755 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - |  9756 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - |  9757 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  9758 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - |  9759 | ` *  (called "methods").` |
|         - |  9760 | ` */` |
|         - |  9761 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - |  9762 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - |  9763 | `struct TraitUseEntry {` |
|         - |  9764 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - |  9765 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - |  9766 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - |  9767 | `};` |
|         - |  9768 | `/*` |
|         - |  9769 | ` * Validate that methods implementing interface contracts have compatible` |
|         - |  9770 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - |  9771 | ` */` |
|    364206 |  9772 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9773 | `{` |
|         - |  9774 | `	ph7_class **apIface;` |
|         - |  9775 | `	sxu32 nIface,i;` |
|         - |  9776 | `	sxi32 rc;` |
|    364211 |  9777 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 |  9778 | `		return SXRET_OK;` |
|         - |  9779 | `	}` |
|    364211 |  9780 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    364211 |  9781 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    730963 |  9782 | `	for(i = 0; i < nIface; i++){` |
|    366757 |  9783 | `		ph7_class *pIface = apIface[i];` |
|         - |  9784 | `		SyHashEntry *pEntry;` |
|    366757 |  9785 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1056837 |  9786 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    690085 |  9787 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - |  9788 | `			ph7_class_method *pImplMeth;` |
|    690085 |  9789 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - |  9790 | `			/* Find the implementing method in the class */` |
|    690085 |  9791 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    690085 |  9792 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 |  9793 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - |  9794 | `			}` |
|         - |  9795 | `			/* Check visibility: interface methods must be implemented as public */` |
|    690067 |  9796 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 |  9797 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9798 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 |  9799 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 |  9800 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9801 | `					return SXERR_ABORT;` |
|         - |  9802 | `				}` |
|         1 |  9803 | `			}` |
|         - |  9804 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - |  9805 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - |  9806 | `			 */` |
|         - |  9807 | `			{` |
|    690067 |  9808 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    690067 |  9809 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    690067 |  9810 | `				int sigError = 0;` |
|    690067 |  9811 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 |  9812 | `					sigError = 1;` |
|    690066 |  9813 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - |  9814 | `					/* Extra parameters must all have default values */` |
|      3949 |  9815 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - |  9816 | `					sxu32 k;` |
|      7891 |  9817 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3949 |  9818 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 |  9819 | `							sigError = 1;` |
|         3 |  9820 | `							break;` |
|         - |  9821 | `						}` |
|      1976 |  9822 | `					}` |
|      1972 |  9823 | `				}` |
|    690067 |  9824 | `				if( sigError ){` |
|         - |  9825 | `					SyBlob sImplSig, sIfaceSig;` |
|         - |  9826 | `					ph7_vm_func_arg *aArgs;` |
|         - |  9827 | `					sxu32 j;` |
|         6 |  9828 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 |  9829 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - |  9830 | `					/* Build implementing method signature */` |
|         6 |  9831 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 |  9832 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 |  9833 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 |  9834 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 |  9835 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9836 | `					}` |
|         - |  9837 | `					/* Build interface method signature */` |
|         6 |  9838 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 |  9839 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 |  9840 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 |  9841 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 |  9842 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9843 | `					}` |
|         8 |  9844 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9845 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 |  9846 | `						&pClass->sName,pMName,` |
|         4 |  9847 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 |  9848 | `						&pIface->sName,pMName,` |
|         4 |  9849 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 |  9850 | `					SyBlobRelease(&sImplSig);` |
|         6 |  9851 | `					SyBlobRelease(&sIfaceSig);` |
|         6 |  9852 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9853 | `						return SXERR_ABORT;` |
|         - |  9854 | `					}` |
|         2 |  9855 | `				}` |
|         - |  9856 | `			}` |
|         5 |  9857 | `		}` |
|    183381 |  9858 | `	}` |
|    364211 |  9859 | `	return SXRET_OK;` |
|    182108 |  9860 | `}` |
|         - |  9861 | `/*` |
|         - |  9862 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - |  9863 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - |  9864 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - |  9865 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - |  9866 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - |  9867 | ` * means that specific hook is still missing.` |
|         - |  9868 | ` */` |
|        38 |  9869 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 |  9870 | `{` |
|         - |  9871 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - |  9872 | `	ph7_class_attr *pProp;` |
|        38 |  9873 | `	if( pMName->nByte <= nPfx` |
|        27 |  9874 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 |  9875 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 |  9876 | `		return 0; /* not a hook stub */` |
|         - |  9877 | `	}` |
|         7 |  9878 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 |  9879 | `	return pProp != 0` |
|         6 |  9880 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 |  9881 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 |  9882 | `}` |
|         - |  9883 | `/*` |
|         - |  9884 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - |  9885 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - |  9886 | ` */` |
|        16 |  9887 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 |  9888 | `{` |
|         - |  9889 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 |  9890 | `	if( pMName->nByte > nPfx` |
|        12 |  9891 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 |  9892 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 |  9893 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 |  9894 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 |  9895 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 |  9896 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 |  9897 | `		return;` |
|         - |  9898 | `	}` |
|        20 |  9899 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 |  9900 | `}` |
|         - |  9901 | `/*` |
|         - |  9902 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - |  9903 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - |  9904 | ` */` |
|    364206 |  9905 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9906 | `{` |
|         - |  9907 | `	ph7_class_method *pMeth;` |
|         - |  9908 | `	SyHashEntry *pEntry;` |
|         - |  9909 | `	sxu32 nAbstract;` |
|         - |  9910 | `	SyBlob sMsg;` |
|         - |  9911 | `	sxi32 rc;` |
|         - |  9912 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    364211 |  9913 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15809 |  9914 | `		return SXRET_OK;` |
|         - |  9915 | `	}` |
|         - |  9916 | `	/* Count abstract methods */` |
|    348407 |  9917 | `	nAbstract = 0;` |
|    348407 |  9918 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   5142764 |  9919 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4620161 |  9920 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4620161 |  9921 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 |  9922 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 |  9923 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - |  9924 | `			}` |
|        20 |  9925 | `			nAbstract++;` |
|         8 |  9926 | `		}` |
|         5 |  9927 | `	}` |
|    348407 |  9928 | `	if( nAbstract == 0 ){` |
|    348393 |  9929 | `		return SXRET_OK;` |
|         - |  9930 | `	}` |
|         - |  9931 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 |  9932 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 |  9933 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - |  9934 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 |  9935 | `		&pClass->sName,nAbstract,` |
|         7 |  9936 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 |  9937 | `		(nAbstract > 1 ? "s" : ""));` |
|         - |  9938 | `	/* Second pass: list methods with origins */` |
|         - |  9939 | `	{` |
|        18 |  9940 | `		sxu32 nListed = 0;` |
|        18 |  9941 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 |  9942 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 |  9943 | `			ph7_class *pOrigin = 0;` |
|         - |  9944 | `			SyString *pMName;` |
|        22 |  9945 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 |  9946 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 |  9947 | `				continue;` |
|         - |  9948 | `			}` |
|        20 |  9949 | `			pMName = &pMeth->sFunc.sName;` |
|        20 |  9950 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 |  9951 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - |  9952 | `			}` |
|        20 |  9953 | `			if( nListed > 0 ){` |
|         3 |  9954 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 |  9955 | `			}` |
|         - |  9956 | `			/* Find the origin of this abstract method.` |
|         - |  9957 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - |  9958 | `			 * inheritance chains) take precedence for interface-declared` |
|         - |  9959 | `			 * methods. Abstract class methods only win when the class` |
|         - |  9960 | `			 * itself declared the abstract method (not inherited from` |
|         - |  9961 | `			 * an interface). Trait methods are adopted into the using` |
|         - |  9962 | `			 * class's namespace.` |
|         - |  9963 | `			 */` |
|         - |  9964 | `			{` |
|         - |  9965 | `				ph7_class **apIface;` |
|         - |  9966 | `				ph7_class **apTrait;` |
|         - |  9967 | `				ph7_class *pWalk;` |
|         - |  9968 | `				sxu32 i;` |
|         - |  9969 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - |  9970 | `				 * (one that was written in the class body, not inherited from an` |
|         - |  9971 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - |  9972 | `				 */` |
|        20 |  9973 | `				if( pClass->pBase ){` |
|        11 |  9974 | `					pWalk = pClass->pBase;` |
|        19 |  9975 | `					while( pWalk ){` |
|         - |  9976 | `						ph7_class_method *pParentMeth;` |
|        13 |  9977 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 |  9978 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - |  9979 | `							/* Exclude methods that came from an interface anywhere` |
|         - |  9980 | `							 * in this class's ancestor chain.` |
|         - |  9981 | `							 */` |
|        13 |  9982 | `							int fromIface = 0;` |
|        13 |  9983 | `							ph7_class *pAnc = pWalk;` |
|        17 |  9984 | `							while( pAnc ){` |
|         - |  9985 | `								ph7_class **apPI;` |
|         - |  9986 | `								sxu32 j;` |
|        15 |  9987 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 |  9988 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 |  9989 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 |  9990 | `										fromIface = 1;` |
|        10 |  9991 | `										break;` |
|         - |  9992 | `									}` |
|       ! 0 |  9993 | `								}` |
|        15 |  9994 | `								if( fromIface ) break;` |
|         6 |  9995 | `								pAnc = pAnc->pBase;` |
|         2 |  9996 | `							}` |
|        13 |  9997 | `							if( !fromIface ){` |
|         3 |  9998 | `								pOrigin = pWalk;` |
|         3 |  9999 | `								break;` |
|         - | 10000 | `							}` |
|         4 | 10001 | `						}` |
|        10 | 10002 | `						pWalk = pWalk->pBase;` |
|         2 | 10003 | `					}` |
|         4 | 10004 | `				}` |
|         - | 10005 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10006 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10007 | `				 */` |
|        20 | 10008 | `				if( !pOrigin ){` |
|        18 | 10009 | `					pWalk = pClass;` |
|        40 | 10010 | `					while( pWalk && !pOrigin ){` |
|        26 | 10011 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10012 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10013 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10014 | `							ph7_class *pDeepest = 0;` |
|        28 | 10015 | `							while( pIface ){` |
|        16 | 10016 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10017 | `									pDeepest = pIface;` |
|         6 | 10018 | `								}` |
|        16 | 10019 | `								pIface = pIface->pBase;` |
|         4 | 10020 | `							}` |
|        16 | 10021 | `							if( pDeepest ){` |
|        16 | 10022 | `								pOrigin = pDeepest;` |
|        16 | 10023 | `								break;` |
|         - | 10024 | `							}` |
|       ! 0 | 10025 | `						}` |
|        26 | 10026 | `						pWalk = pWalk->pBase;` |
|         4 | 10027 | `					}` |
|         7 | 10028 | `				}` |
|         - | 10029 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10030 | `				if( !pOrigin ){` |
|         3 | 10031 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10032 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10033 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10034 | `							pOrigin = pClass;` |
|         3 | 10035 | `							break;` |
|         - | 10036 | `						}` |
|       ! 0 | 10037 | `					}` |
|         1 | 10038 | `				}` |
|         - | 10039 | `			}` |
|        20 | 10040 | `			if( pOrigin ){` |
|        20 | 10041 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10042 | `			}else{` |
|         - | 10043 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10044 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10045 | `			}` |
|        20 | 10046 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10047 | `			nListed++;` |
|         4 | 10048 | `		}` |
|         - | 10049 | `	}` |
|        18 | 10050 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10051 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10052 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10053 | `	SyBlobRelease(&sMsg);` |
|        18 | 10054 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10055 | `		return SXERR_ABORT;` |
|         - | 10056 | `	}` |
|        18 | 10057 | `	return SXRET_OK;` |
|    182108 | 10058 | `}` |
|         - | 10059 | `/*` |
|         - | 10060 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10061 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10062 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10063 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10064 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10065 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10066 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10067 | ` */` |
|    411784 | 10068 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10069 | `{` |
|    411789 | 10070 | `	int isAbsolute = 0;` |
|    411789 | 10071 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10072 | `	SyBlob sName;` |
|    411789 | 10073 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4529 | 10074 | `		isAbsolute = 1;` |
|      4529 | 10075 | `		pGen->pIn++;` |
|      2262 | 10076 | `	}` |
|    411789 | 10077 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         8 | 10078 | `		pGen->pIn = pStart;` |
|         8 | 10079 | `		return SXERR_INVALID;` |
|         - | 10080 | `	}` |
|    411783 | 10081 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    411783 | 10082 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    411783 | 10083 | `	pGen->pIn++;` |
|    617688 | 10084 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    205915 | 10085 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10086 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10087 | `		pGen->pIn++;` |
|        16 | 10088 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10089 | `		pGen->pIn++;` |
|         2 | 10090 | `	}` |
|    411783 | 10091 | `	if( isAbsolute ){` |
|      4527 | 10092 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2266 | 10093 | `	}else{` |
|         - | 10094 | `		SyString sRaw;` |
|    407261 | 10095 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    407261 | 10096 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10097 | `	}` |
|    411783 | 10098 | `	SyBlobRelease(&sName);` |
|    411783 | 10099 | `	return SXRET_OK;` |
|    205897 | 10100 | `}` |
|         - | 10101 | `/*` |
|         - | 10102 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10103 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10104 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10105 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10106 | ` * either direction cannot run unbounded.` |
|         - | 10107 | ` */` |
|         - | 10108 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    169620 | 10109 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10110 | `{` |
|         - | 10111 | `	ph7_class **apParent;` |
|         - | 10112 | `	sxu32 n;` |
|    441723 | 10113 | `	while( pInterface ){` |
|    279993 | 10114 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10115 | `			return FALSE;` |
|         - | 10116 | `		}` |
|    315472 | 10117 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     70958 | 10118 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7895 | 10119 | `			return TRUE;` |
|         - | 10120 | `		}` |
|    272103 | 10121 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    272103 | 10122 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10123 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10124 | `				return TRUE;` |
|         - | 10125 | `			}` |
|       ! 0 | 10126 | `		}` |
|    272103 | 10127 | `		pInterface = pInterface->pBase;` |
|    272103 | 10128 | `		iDepth++;` |
|         5 | 10129 | `	}` |
|    161735 | 10130 | `	return FALSE;` |
|     84815 | 10131 | `}` |
|    169620 | 10132 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10133 | `{` |
|    169625 | 10134 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10135 | `}` |
|         - | 10136 | `/*` |
|         - | 10137 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10138 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10139 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10140 | ` */` |
|      7890 | 10141 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10142 | `{` |
|      7899 | 10143 | `	while( pBase ){` |
|        10 | 10144 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10145 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10146 | `			return TRUE;` |
|         - | 10147 | `		}` |
|        10 | 10148 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10149 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10150 | `			return TRUE;` |
|         - | 10151 | `		}` |
|         5 | 10152 | `		pBase = pBase->pBase;` |
|         1 | 10153 | `	}` |
|      7891 | 10154 | `	return FALSE;` |
|      3950 | 10155 | `}` |
|         - | 10156 | `/*` |
|         - | 10157 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10158 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10159 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10160 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10161 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10162 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10163 | ` * pClass->aEnumCases for cases().` |
|         - | 10164 | ` */` |
|      7922 | 10165 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10166 | `{` |
|      7927 | 10167 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10168 | `	SySet *pInstrContainer;` |
|         - | 10169 | `	ph7_class_attr *pCase;` |
|         - | 10170 | `	SyString *pName;` |
|         - | 10171 | `	sxi32 rc;` |
|      7927 | 10172 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7927 | 10173 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10174 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10175 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10176 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10177 | `			return SXERR_ABORT;` |
|         - | 10178 | `		}` |
|       ! 0 | 10179 | `		goto Synchronize;` |
|         - | 10180 | `	}` |
|      7927 | 10181 | `	pName = &pGen->pIn->sData;` |
|         - | 10182 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7927 | 10183 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10184 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10185 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10186 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10187 | `			return SXERR_ABORT;` |
|         - | 10188 | `		}` |
|       ! 0 | 10189 | `		goto Synchronize;` |
|         - | 10190 | `	}` |
|      7927 | 10191 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10192 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7927 | 10193 | `	if( pCase == 0 ){` |
|       ! 0 | 10194 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10195 | `		return SXERR_ABORT;` |
|         - | 10196 | `	}` |
|      7927 | 10197 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7927 | 10198 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10199 | `		return SXERR_ABORT;` |
|         - | 10200 | `	}` |
|      7927 | 10201 | `	pGen->pIn++; /* Jump the case name */` |
|      7927 | 10202 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7913 | 10203 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10204 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10205 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10206 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10207 | `				return SXERR_ABORT;` |
|         - | 10208 | `			}` |
|         6 | 10209 | `			goto Synchronize;` |
|         - | 10210 | `		}` |
|      7909 | 10211 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10212 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10213 | `		 * (same technique as class constants). */` |
|      7909 | 10214 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7909 | 10215 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7909 | 10216 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7909 | 10217 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10218 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10219 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10220 | `		}` |
|      7909 | 10221 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7909 | 10222 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7909 | 10223 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10224 | `			return SXERR_ABORT;` |
|         - | 10225 | `		}` |
|      3957 | 10226 | `	}else{` |
|        17 | 10227 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10228 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10229 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10230 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10231 | `				return SXERR_ABORT;` |
|         - | 10232 | `			}` |
|       ! 0 | 10233 | `			goto Synchronize;` |
|         - | 10234 | `		}` |
|         - | 10235 | `	}` |
|      7923 | 10236 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7923 | 10237 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10238 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10239 | `		return SXERR_ABORT;` |
|         - | 10240 | `	}` |
|      7923 | 10241 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7923 | 10242 | `	return SXRET_OK;` |
|         2 | 10243 | `Synchronize:` |
|         - | 10244 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10245 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10246 | `		pGen->pIn++;` |
|         2 | 10247 | `	}` |
|         6 | 10248 | `	return SXERR_CORRUPT;` |
|      3966 | 10249 | `}` |
|         - | 10250 | `/*` |
|         - | 10251 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10252 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10253 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10254 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10255 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10256 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10257 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10258 | ` */` |
|      3964 | 10259 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10260 | `{` |
|         - | 10261 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10262 | `	const char *zBack;` |
|         - | 10263 | `	SySet sToken;` |
|         - | 10264 | `	char *zSrc;` |
|         - | 10265 | `	sxu32 nSrc,nMax;` |
|      3969 | 10266 | `	sxi32 rc = SXRET_OK;` |
|      3969 | 10267 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3964 | 10268 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3969 | 10269 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3969 | 10270 | `	if( zSrc == 0 ){` |
|       ! 0 | 10271 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10272 | `		return SXERR_ABORT;` |
|         - | 10273 | `	}` |
|      3969 | 10274 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3969 | 10275 | `	if( pClass->nEnumBacking != 0 ){` |
|      5933 | 10276 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10277 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10278 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10279 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1976 | 10280 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1981 | 10281 | `	}else{` |
|        21 | 10282 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10283 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10284 | `	}` |
|      3969 | 10285 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3969 | 10286 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3969 | 10287 | `	pSaveIn = pGen->pIn;` |
|      3969 | 10288 | `	pSaveEnd = pGen->pEnd;` |
|      3969 | 10289 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3969 | 10290 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15837 | 10291 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11873 | 10292 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10293 | `	}` |
|      3969 | 10294 | `	pGen->pIn = pSaveIn;` |
|      3969 | 10295 | `	pGen->pEnd = pSaveEnd;` |
|      3969 | 10296 | `	SySetRelease(&sToken);` |
|      3969 | 10297 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1987 | 10298 | `}` |
|         - | 10299 | `/*` |
|         - | 10300 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10301 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10302 | ` */` |
|         - | 10303 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10304 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10305 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10306 | `};` |
|         - | 10307 | `/*` |
|         - | 10308 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10309 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10310 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10311 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10312 | ` * and before the class is installed.` |
|         - | 10313 | ` */` |
|      3964 | 10314 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10315 | `{` |
|         - | 10316 | `	SyHashEntry *pEntry;` |
|         - | 10317 | `	sxi32 rc;` |
|         - | 10318 | `	sxu32 n;` |
|         - | 10319 | `	/* php: "Enum %s cannot include properties" */` |
|      3969 | 10320 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11891 | 10321 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7929 | 10322 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7929 | 10323 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10324 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10325 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10326 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10327 | `				return SXERR_ABORT;` |
|         - | 10328 | `			}` |
|         3 | 10329 | `			break;` |
|         - | 10330 | `		}` |
|         5 | 10331 | `	}` |
|         - | 10332 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     55501 | 10333 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     77298 | 10334 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     51537 | 10335 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10336 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10337 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10338 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10339 | `				return SXERR_ABORT;` |
|         - | 10340 | `			}` |
|       ! 0 | 10341 | `		}` |
|     25771 | 10342 | `	}` |
|         - | 10343 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10344 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10345 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10346 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10347 | `	{` |
|         - | 10348 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10349 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10350 | `		ph7_class_attr *pAttr;` |
|      3969 | 10351 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10352 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3969 | 10353 | `		if( pAttr == 0 ){` |
|       ! 0 | 10354 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10355 | `			return SXERR_ABORT;` |
|         - | 10356 | `		}` |
|      3969 | 10357 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3969 | 10358 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3969 | 10359 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3969 | 10360 | `		if( pClass->nEnumBacking != 0 ){` |
|      3957 | 10361 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10362 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3957 | 10363 | `			if( pAttr == 0 ){` |
|       ! 0 | 10364 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10365 | `				return SXERR_ABORT;` |
|         - | 10366 | `			}` |
|      3957 | 10367 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3957 | 10368 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10369 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10370 | `			}else{` |
|      3951 | 10371 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10372 | `			}` |
|      3957 | 10373 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1976 | 10374 | `		}` |
|         - | 10375 | `	}` |
|      3969 | 10376 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1987 | 10377 | `}` |
|         - | 10378 | `/*` |
|         - | 10379 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10380 | ` *` |
|         - | 10381 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10382 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10383 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10384 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10385 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10386 | ` * implements, body, install) is shared by both paths.` |
|         - | 10387 | ` */` |
|    364250 | 10388 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10389 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10390 | `{` |
|    364255 | 10391 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10392 | `	ph7_class *pClass,*pBase;` |
|         - | 10393 | `	SyToken *pEnd,*pTmp;` |
|         - | 10394 | `	sxi32 iProtection;` |
|         - | 10395 | `	SySet aInterfaces;` |
|         - | 10396 | `	SySet aUseEntries;` |
|         - | 10397 | `	sxi32 iAttrflags;` |
|         - | 10398 | `	SyString *pName;` |
|         - | 10399 | `	sxi32 nKwrd;` |
|         - | 10400 | `	sxi32 rc;` |
|         - | 10401 | `	/* Jump the 'class' keyword */` |
|    364255 | 10402 | `	pGen->pIn++;` |
|    364255 | 10403 | `	if( pAnonName ){` |
|         - | 10404 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10405 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10406 | `		 * then use the synthesized name. */` |
|        32 | 10407 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10408 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10409 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10410 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10411 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10412 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10413 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10414 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10415 | `		}` |
|        32 | 10416 | `		pName = pAnonName;` |
|        32 | 10417 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10418 | `	}else{` |
|    364227 | 10419 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10420 | `			/* Syntax error */` |
|       ! 0 | 10421 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10422 | `			if( rc == SXERR_ABORT ){` |
|         - | 10423 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10424 | `				return SXERR_ABORT;` |
|         - | 10425 | `			}` |
|         - | 10426 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10427 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10428 | `				pGen->pIn++;` |
|       ! 0 | 10429 | `			}` |
|       ! 0 | 10430 | `			return SXRET_OK;` |
|         - | 10431 | `		}` |
|         - | 10432 | `		/* Extract class name */` |
|    364227 | 10433 | `		pName = &pGen->pIn->sData;` |
|         - | 10434 | `		/* Advance the stream cursor */` |
|    364227 | 10435 | `		pGen->pIn++;` |
|         - | 10436 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10437 | `			SyBlob sFQN;` |
|         - | 10438 | `			SyString sFQNStr;` |
|    364227 | 10439 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    364227 | 10440 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    364227 | 10441 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    364227 | 10442 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    364227 | 10443 | `			SyBlobRelease(&sFQN);` |
|         - | 10444 | `		}` |
|         - | 10445 | `	}` |
|    364255 | 10446 | `	if( pClass == 0 ){` |
|       ! 0 | 10447 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10448 | `		return SXERR_ABORT;` |
|         - | 10449 | `	}` |
|    364250 | 10450 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3973 | 10451 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10452 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3959 | 10453 | `		pGen->pIn++; /* Jump ':' */` |
|      3954 | 10454 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3959 | 10455 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10456 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10457 | `			pGen->pIn++;` |
|      3952 | 10458 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3953 | 10459 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3951 | 10460 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3951 | 10461 | `			pGen->pIn++;` |
|      1978 | 10462 | `		}else{` |
|         3 | 10463 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10464 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10465 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10466 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10467 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10468 | `				return SXERR_ABORT;` |
|         - | 10469 | `			}` |
|         3 | 10470 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10471 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10472 | `			}` |
|         - | 10473 | `		}` |
|      1977 | 10474 | `	}` |
|    364255 | 10475 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    364255 | 10476 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10477 | `		return SXERR_ABORT;` |
|         - | 10478 | `	}` |
|         - | 10479 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    364255 | 10480 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    364255 | 10481 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10482 | `	/* Assume a standalone class */` |
|    364255 | 10483 | `	pBase = 0;` |
|    364255 | 10484 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    295935 | 10485 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    295935 | 10486 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10487 | `			SyBlob sResolved;` |
|         - | 10488 | `			SyString sBaseName;` |
|         - | 10489 | `			sxu32 nRefLine;` |
|    189379 | 10490 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10491 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10492 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10493 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10494 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10495 | `					return SXERR_ABORT;` |
|         - | 10496 | `				}` |
|       ! 0 | 10497 | `			}` |
|    189379 | 10498 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    189379 | 10499 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    189379 | 10500 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    189379 | 10501 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10502 | `				SyBlobRelease(&sResolved);` |
|         4 | 10503 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10504 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10505 | `					pName);` |
|         3 | 10506 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10507 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10508 | `					return SXERR_ABORT;` |
|         - | 10509 | `				}` |
|         3 | 10510 | `				return SXRET_OK;` |
|         - | 10511 | `			}` |
|    284063 | 10512 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    189372 | 10513 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    189377 | 10514 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10515 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10516 | `			/* Interfaces are not allowed */` |
|    189377 | 10517 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10518 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10519 | `			}` |
|    189377 | 10520 | `			if( pBase == 0 ){` |
|       ! 0 | 10521 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10522 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10523 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10524 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10525 | `					return SXERR_ABORT;` |
|         - | 10526 | `				}` |
|       ! 0 | 10527 | `			}else{` |
|    189377 | 10528 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10529 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10530 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10531 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10532 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10533 | `						return SXERR_ABORT;` |
|         - | 10534 | `					}` |
|         3 | 10535 | `					pBase = 0; /* Never inherit from an enum */` |
|    189376 | 10536 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10537 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10538 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10539 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10540 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10541 | `						return SXERR_ABORT;` |
|         - | 10542 | `					}` |
|       ! 0 | 10543 | `				}` |
|         - | 10544 | `			}` |
|    189377 | 10545 | `			SyBlobRelease(&sResolved);` |
|    189377 | 10546 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10547 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10548 | `			}` |
|     94686 | 10549 | `		}` |
|    295933 | 10550 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10551 | `			ph7_class *pInterface;` |
|         - | 10552 | `			/* Interface implementation */` |
|    110513 | 10553 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    114366 | 10554 | `			for(;;){` |
|         - | 10555 | `				SyBlob sResolved;` |
|         - | 10556 | `				SyString sIntName;` |
|         - | 10557 | `				sxu32 nRefLine;` |
|    169625 | 10558 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    169625 | 10559 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    169625 | 10560 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10561 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10562 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10563 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10564 | `						pName);` |
|       ! 0 | 10565 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10566 | `						return SXERR_ABORT;` |
|         - | 10567 | `					}` |
|       ! 0 | 10568 | `					break;` |
|         - | 10569 | `				}` |
|    339245 | 10570 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    169620 | 10571 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    169625 | 10572 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10573 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10574 | `				/* Only interfaces are allowed */` |
|    169625 | 10575 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10576 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10577 | `				}` |
|    169625 | 10578 | `				if( pInterface == 0 ){` |
|       ! 0 | 10579 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10580 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10581 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10582 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10583 | `						return SXERR_ABORT;` |
|         - | 10584 | `					}` |
|       ! 0 | 10585 | `				}else{` |
|         - | 10586 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10587 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10588 | `					 * unless they already extend Exception or Error.` |
|         - | 10589 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10590 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10591 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    169625 | 10592 | `					SyString *pFqn = &pClass->sName;` |
|    169625 | 10593 | `					int bIsExceptionOrError =` |
|     88754 | 10594 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    256404 | 10595 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    167657 | 10596 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3954 | 10597 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    173565 | 10598 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11838 | 10599 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3943 | 10600 | `						!bIsExceptionOrError ){` |
|        12 | 10601 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10602 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10603 | `							&pClass->sName);` |
|         9 | 10604 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10605 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10606 | `							return SXERR_ABORT;` |
|         - | 10607 | `						}` |
|         - | 10608 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10609 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10610 | `					}else{` |
|    169619 | 10611 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10612 | `					}` |
|         - | 10613 | `				}` |
|    169625 | 10614 | `				SyBlobRelease(&sResolved);` |
|    169625 | 10615 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     55259 | 10616 | `					break;` |
|         - | 10617 | `				}` |
|     59117 | 10618 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10619 | `			}` |
|     55254 | 10620 | `		}` |
|    147964 | 10621 | `	}` |
|    364253 | 10622 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10623 | `		/* Syntax error */` |
|       ! 0 | 10624 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10625 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10626 | `		if( rc == SXERR_ABORT ){` |
|         - | 10627 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10628 | `			return SXERR_ABORT;` |
|         - | 10629 | `		}` |
|       ! 0 | 10630 | `		return SXRET_OK;` |
|         - | 10631 | `	}` |
|    364253 | 10632 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    364253 | 10633 | `	pEnd = 0; /* cc warning */` |
|         - | 10634 | `	/* Delimit the class body */` |
|    364253 | 10635 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    364253 | 10636 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10637 | `		/* Syntax error */` |
|       ! 0 | 10638 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10639 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10640 | `		if( rc == SXERR_ABORT ){` |
|         - | 10641 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10642 | `			return SXERR_ABORT;` |
|         - | 10643 | `		}` |
|       ! 0 | 10644 | `		return SXRET_OK;` |
|         - | 10645 | `	}` |
|         - | 10646 | `	/* The delimiter token is the class body's closing brace */` |
|    364253 | 10647 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10648 | `	/* Swap token stream */` |
|    364253 | 10649 | `	pTmp = pGen->pEnd;` |
|    364253 | 10650 | `	pGen->pEnd = pEnd;` |
|         - | 10651 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    364253 | 10652 | `	pClass->iFlags \|= iFlags;` |
|         - | 10653 | `	/* Start the parse process */` |
|   1410220 | 10654 | `	for(;;){` |
|         - | 10655 | `		/* Jump leading/trailing semi-colons */` |
|   4020357 | 10656 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    726071 | 10657 | `			pGen->pIn++;` |
|         5 | 10658 | `		}` |
|   3294291 | 10659 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10660 | `			/* End of class body */` |
|    364211 | 10661 | `			break;` |
|         - | 10662 | `		}` |
|         - | 10663 | `		/* Bind a directly-preceding docblock to this member */` |
|   2930085 | 10664 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2930080 | 10665 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1465045 | 10666 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10667 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10668 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10669 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10670 | `			if( rc == SXERR_ABORT ){` |
|         - | 10671 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10672 | `				return SXERR_ABORT;` |
|         - | 10673 | `			}` |
|       ! 0 | 10674 | `			goto done;` |
|         - | 10675 | `		}` |
|         - | 10676 | `		/* Assume public visibility */` |
|   2930085 | 10677 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2930085 | 10678 | `		iAttrflags = 0;` |
|         - | 10679 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10680 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10681 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10682 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2930085 | 10683 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10684 | `			int bMod = 0;` |
|       ! 0 | 10685 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10686 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10687 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10688 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10689 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10690 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10691 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10692 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10693 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10694 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10695 | `			}` |
|       ! 0 | 10696 | `			if( !bMod ){` |
|       ! 0 | 10697 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10698 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10699 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10700 | `						return SXERR_ABORT;` |
|         - | 10701 | `					}` |
|       ! 0 | 10702 | `					goto done;` |
|         - | 10703 | `				}` |
|       ! 0 | 10704 | `				continue;` |
|         - | 10705 | `			}` |
|       ! 0 | 10706 | `		}` |
|   2930085 | 10707 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10708 | `			/* Extract the current keyword */` |
|   2930085 | 10709 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2930085 | 10710 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10711 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7927 | 10712 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7927 | 10713 | `				if( rc != SXRET_OK ){` |
|         6 | 10714 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10715 | `						return SXERR_ABORT;` |
|         - | 10716 | `					}` |
|         6 | 10717 | `					goto done;` |
|         - | 10718 | `				}` |
|      7923 | 10719 | `				continue;` |
|         - | 10720 | `			}` |
|   2922163 | 10721 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 10722 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 10723 | `				TraitUseEntry sUse;` |
|     15829 | 10724 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15829 | 10725 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15829 | 10726 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7920 | 10727 | `				for(;;){` |
|         - | 10728 | `					ph7_class *pTrait;` |
|         - | 10729 | `					SyString *pTraitName;` |
|     15837 | 10730 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 10731 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10732 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 10733 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10734 | `							return SXERR_ABORT;` |
|         - | 10735 | `						}` |
|       ! 0 | 10736 | `						break;` |
|         - | 10737 | `					}` |
|     15837 | 10738 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 10739 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 10740 | `						SyBlob sResolved;` |
|     15837 | 10741 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15837 | 10742 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     31669 | 10743 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15832 | 10744 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15837 | 10745 | `						SyBlobRelease(&sResolved);` |
|         - | 10746 | `					}` |
|         - | 10747 | `					/* Only traits are allowed */` |
|     15837 | 10748 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 10749 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 10750 | `					}` |
|     15837 | 10751 | `					if( pTrait == 0 ){` |
|       ! 0 | 10752 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10753 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 10754 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10755 | `							return SXERR_ABORT;` |
|         - | 10756 | `						}` |
|       ! 0 | 10757 | `					}else{` |
|     15837 | 10758 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 10759 | `					}` |
|     15837 | 10760 | `					pGen->pIn++; /* Advance past trait name */` |
|     15837 | 10761 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7917 | 10762 | `						break;` |
|         - | 10763 | `					}` |
|        10 | 10764 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 10765 | `				}` |
|         - | 10766 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15829 | 10767 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 10768 | `					SyToken *pBlock;` |
|        13 | 10769 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 10770 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 10771 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 10772 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 10773 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 10774 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 10775 | `					}else{` |
|       ! 0 | 10776 | `						pGen->pIn = pGen->pEnd;` |
|         - | 10777 | `					}` |
|         5 | 10778 | `				}` |
|     15829 | 10779 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 10780 | `				/* The semicolon will be consumed by the outer loop */` |
|     15829 | 10781 | `				continue;` |
|         - | 10782 | `			}` |
|   2906339 | 10783 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 10784 | `				int nSetTok;` |
|   2653687 | 10785 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2653687 | 10786 | `				if( nSetVis ){` |
|         - | 10787 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 10788 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 10789 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10790 | `					pGen->pIn += nSetTok;` |
|         2 | 10791 | `				}else{` |
|   2653685 | 10792 | `					iProtection = nKwrd;` |
|   2653685 | 10793 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 10794 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 10795 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2653685 | 10796 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2653685 | 10797 | `					if( nSetVis ){` |
|         9 | 10798 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 10799 | `						pGen->pIn += nSetTok;` |
|         4 | 10800 | `					}` |
|         - | 10801 | `				}` |
|         - | 10802 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 10803 | ``				 * `public private(set) readonly int $x`. */`` |
|   2653687 | 10804 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 10805 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 10806 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 10807 | `				}` |
|   2653682 | 10808 | `				if( pGen->pIn >= pGen->pEnd` |
|   2653687 | 10809 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10810 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10811 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10812 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 10813 | `					if( rc == SXERR_ABORT ){` |
|         - | 10814 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 10815 | `						return SXERR_ABORT;` |
|         - | 10816 | `					}` |
|       ! 0 | 10817 | `					goto done;` |
|         - | 10818 | `				}` |
|   2653687 | 10819 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10820 | `					/* Attribute declaration (untyped) */` |
|    422193 | 10821 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    422193 | 10822 | `					if( rc != SXRET_OK ){` |
|        11 | 10823 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10824 | `							return SXERR_ABORT;` |
|         - | 10825 | `						}` |
|        11 | 10826 | `						goto done;` |
|         - | 10827 | `					}` |
|    422329 | 10828 | `					continue;` |
|         - | 10829 | `				}` |
|   2231499 | 10830 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10831 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       299 | 10832 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       299 | 10833 | `					if( rc != SXRET_OK ){` |
|         8 | 10834 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10835 | `							return SXERR_ABORT;` |
|         - | 10836 | `						}` |
|         8 | 10837 | `						goto done;` |
|         - | 10838 | `					}` |
|       293 | 10839 | `					continue;` |
|         - | 10840 | `				}` |
|         - | 10841 | `				/* Extract the keyword */` |
|   2231205 | 10842 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1115600 | 10843 | `			}` |
|   2483857 | 10844 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 10845 | `				/* Process constant declaration */` |
|    244435 | 10846 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    244435 | 10847 | `				if( rc != SXRET_OK ){` |
|        11 | 10848 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10849 | `						return SXERR_ABORT;` |
|         - | 10850 | `					}` |
|        11 | 10851 | `					goto done;` |
|         - | 10852 | `				}` |
|    122216 | 10853 | `			}else{` |
|   2239427 | 10854 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 10855 | `					/* Static method or attribute,record that */` |
|     98645 | 10856 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     98645 | 10857 | `					pGen->pIn++; /* Jump the static keyword */` |
|     98645 | 10858 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10859 | `						int nSetTok;` |
|     71035 | 10860 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     71035 | 10861 | `						if( nSetVis ){` |
|         - | 10862 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 10863 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10864 | `							pGen->pIn += nSetTok;` |
|         2 | 10865 | `						}else{` |
|         - | 10866 | `							/* Extract the keyword */` |
|     71033 | 10867 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     71033 | 10868 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 10869 | `								iProtection = nKwrd;` |
|       ! 0 | 10870 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 10871 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 10872 | `								if( nSetVis ){` |
|       ! 0 | 10873 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 10874 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 10875 | `								}` |
|       ! 0 | 10876 | `							}` |
|         - | 10877 | `						}` |
|     35515 | 10878 | `					}` |
|         - | 10879 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 10880 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 10881 | `					 * than a generic "expecting method" parse error. */` |
|     98645 | 10882 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10883 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10884 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 10885 | `					}` |
|     98640 | 10886 | `					if( pGen->pIn >= pGen->pEnd` |
|     98645 | 10887 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10888 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10889 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 10890 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 10891 | `						if( rc == SXERR_ABORT ){` |
|         - | 10892 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 10893 | `							return SXERR_ABORT;` |
|         - | 10894 | `						}` |
|       ! 0 | 10895 | `						goto done;` |
|         - | 10896 | `					}` |
|     98645 | 10897 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10898 | `						/* Attribute declaration */` |
|     27613 | 10899 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     27613 | 10900 | `						if( rc != SXRET_OK ){` |
|         3 | 10901 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10902 | `								return SXERR_ABORT;` |
|         - | 10903 | `							}` |
|         3 | 10904 | `							goto done;` |
|         - | 10905 | `						}` |
|     27611 | 10906 | `						continue;` |
|         - | 10907 | `					}` |
|     71037 | 10908 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10909 | `						/* Typed static attribute declaration */` |
|        17 | 10910 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        17 | 10911 | `						if( rc != SXRET_OK ){` |
|         3 | 10912 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10913 | `								return SXERR_ABORT;` |
|         - | 10914 | `							}` |
|         3 | 10915 | `							goto done;` |
|         - | 10916 | `						}` |
|        15 | 10917 | `						continue;` |
|         - | 10918 | `					}` |
|         - | 10919 | `					/* Extract the keyword */` |
|     71023 | 10920 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2176296 | 10921 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 10922 | `					/* Abstract method,record that */` |
|      7903 | 10923 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 10924 | `					/* Mark the whole class as abstract */` |
|      7903 | 10925 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 10926 | `					/* Advance the stream cursor */` |
|      7903 | 10927 | `					pGen->pIn++;` |
|      7903 | 10928 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7903 | 10929 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7903 | 10930 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7901 | 10931 | `							iProtection = nKwrd;` |
|      7901 | 10932 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3948 | 10933 | `						}` |
|      3949 | 10934 | `					}` |
|      7903 | 10935 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7898 | 10936 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 10937 | `							/* Static method */` |
|       ! 0 | 10938 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 10939 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 10940 | `					}` |
|      7903 | 10941 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7898 | 10942 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 10943 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 10944 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 10945 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 10946 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 10947 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 10948 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 10949 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 10950 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 10951 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 10952 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 10953 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 10954 | `										return SXERR_ABORT;` |
|         - | 10955 | `									}` |
|       ! 0 | 10956 | `									goto done;` |
|         - | 10957 | `								}` |
|         7 | 10958 | `								continue;` |
|         - | 10959 | `							}` |
|       ! 0 | 10960 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10961 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 10962 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 10963 | `							if( rc == SXERR_ABORT ){` |
|         - | 10964 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 10965 | `								return SXERR_ABORT;` |
|         - | 10966 | `							}` |
|       ! 0 | 10967 | `							goto done;` |
|         - | 10968 | `					}` |
|      7897 | 10969 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2136835 | 10970 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 10971 | `					/* final method ,record that */` |
|        20 | 10972 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        20 | 10973 | `					pGen->pIn++; /* Jump the final keyword */` |
|        20 | 10974 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10975 | `						/* Extract the keyword */` |
|        20 | 10976 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        20 | 10977 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        10 | 10978 | `							iProtection = nKwrd;` |
|        10 | 10979 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 10980 | `						}` |
|         9 | 10981 | `					}` |
|        20 | 10982 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 10983 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 10984 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 10985 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 10986 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 10987 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 10988 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 10989 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 10990 | `									return SXERR_ABORT;` |
|         - | 10991 | `								}` |
|       ! 0 | 10992 | `								goto done;` |
|         - | 10993 | `							}` |
|        14 | 10994 | `							continue;` |
|         - | 10995 | `					}` |
|         8 | 10996 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 10997 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 10998 | `							/* Static method */` |
|       ! 0 | 10999 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11000 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11001 | `					}` |
|         8 | 11002 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11003 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11004 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11005 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11006 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11007 | `							if( rc == SXERR_ABORT ){` |
|         - | 11008 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11009 | `								return SXERR_ABORT;` |
|         - | 11010 | `							}` |
|       ! 0 | 11011 | `							goto done;` |
|         - | 11012 | `					}` |
|         8 | 11013 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11014 | `				}` |
|   2211787 | 11015 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11016 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11017 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11018 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11019 | `						if( rc == SXERR_ABORT ){` |
|         - | 11020 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11021 | `							return SXERR_ABORT;` |
|         - | 11022 | `						}` |
|       ! 0 | 11023 | `						goto done;` |
|         - | 11024 | `				}` |
|   2211787 | 11025 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11026 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11027 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11028 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11029 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11030 | `						if( rc == SXERR_ABORT ){` |
|         - | 11031 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11032 | `							return SXERR_ABORT;` |
|         - | 11033 | `						}` |
|       ! 0 | 11034 | `						goto done;` |
|         - | 11035 | `					}` |
|         - | 11036 | `					/* Attribute declaration */` |
|         7 | 11037 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11038 | `				}else{` |
|         - | 11039 | `					/* Process method declaration */` |
|   2211781 | 11040 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11041 | `				}` |
|   2211787 | 11042 | `				if( rc != SXRET_OK ){` |
|        16 | 11043 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11044 | `						return SXERR_ABORT;` |
|         - | 11045 | `					}` |
|        16 | 11046 | `					goto done;` |
|         - | 11047 | `				}` |
|         - | 11048 | `			}` |
|   1228101 | 11049 | `		}else{` |
|         - | 11050 | `			/* Attribute declaration */` |
|       ! 0 | 11051 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11052 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11053 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11054 | `					return SXERR_ABORT;` |
|         - | 11055 | `				}` |
|       ! 0 | 11056 | `				goto done;` |
|         - | 11057 | `			}` |
|         - | 11058 | `		}` |
|         5 | 11059 | `	}` |
|         - | 11060 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11061 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11062 | `	 */` |
|         - | 11063 | `	{` |
|         - | 11064 | `		TraitUseEntry *apUse;` |
|         - | 11065 | `		sxu32 nU;` |
|    364211 | 11066 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    380035 | 11067 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15829 | 11068 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15829 | 11069 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15829 | 11070 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15829 | 11071 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11072 | `			sxu32 nT;` |
|     15829 | 11073 | `			if( !hasResolution ){` |
|         - | 11074 | `				/* No conflict resolution block: use standard trait application */` |
|     31639 | 11075 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15825 | 11076 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15825 | 11077 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11078 | `						break;` |
|         - | 11079 | `					}` |
|      7915 | 11080 | `				}` |
|      7912 | 11081 | `			}else{` |
|         - | 11082 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11083 | `				 * then use the block to resolve method conflicts.` |
|         - | 11084 | `				 */` |
|         - | 11085 | `				SyToken *pR;` |
|        25 | 11086 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11087 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11088 | `					ph7_class_attr *pAR;` |
|         - | 11089 | `					SyHashEntry *pER;` |
|         - | 11090 | `					SyString *pNR;` |
|        15 | 11091 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11092 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11093 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11094 | `						pNR = &pAR->sName;` |
|       ! 0 | 11095 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11096 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11097 | `						}` |
|       ! 0 | 11098 | `					}` |
|        15 | 11099 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11100 | `				}` |
|         - | 11101 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11102 | `				pR = pUse->pResolvStart;` |
|        27 | 11103 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11104 | `					SyString sTrait,sMethod;` |
|         - | 11105 | `					ph7_class *pSrcTrait;` |
|         - | 11106 | `					ph7_class_method *pMeth;` |
|         - | 11107 | `					sxi32 nRKwrd;` |
|        41 | 11108 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11109 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11110 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11111 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11112 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11113 | `					sMethod = pR->sData;` |
|        17 | 11114 | `					pR++;` |
|        17 | 11115 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11116 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11117 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11118 | `							sTrait = sMethod;` |
|         7 | 11119 | `							pR++;` |
|         7 | 11120 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11121 | `							sMethod = pR->sData;` |
|         7 | 11122 | `							pR++;` |
|         3 | 11123 | `						}` |
|         3 | 11124 | `					}` |
|        17 | 11125 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11126 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11127 | `						continue;` |
|         - | 11128 | `					}` |
|        17 | 11129 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11130 | `					pR++;` |
|        17 | 11131 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11132 | `						pSrcTrait = 0;` |
|         7 | 11133 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11134 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11135 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11136 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11137 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11138 | `								break;` |
|         - | 11139 | `							}` |
|         2 | 11140 | `						}` |
|         5 | 11141 | `						if( pSrcTrait ){` |
|         5 | 11142 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11143 | `							if( pMeth ){` |
|         5 | 11144 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11145 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11146 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11147 | `								}` |
|         2 | 11148 | `							}` |
|         2 | 11149 | `						}` |
|         2 | 11150 | `					}` |
|        35 | 11151 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11152 | `				}` |
|         - | 11153 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11154 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11155 | `					ph7_class_method *pMR;` |
|         - | 11156 | `					SyHashEntry *pER;` |
|         - | 11157 | `					SyString *pNR;` |
|        15 | 11158 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11159 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11160 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11161 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11162 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11163 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11164 | `						}` |
|         3 | 11165 | `					}` |
|         9 | 11166 | `				}` |
|         - | 11167 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11168 | `				pR = pUse->pResolvStart;` |
|        27 | 11169 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11170 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11171 | `					ph7_class *pSrcTrait;` |
|         - | 11172 | `					ph7_class_method *pMeth;` |
|        27 | 11173 | `					int hasQual = 0;` |
|         - | 11174 | `					sxi32 nRKwrd;` |
|        41 | 11175 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11176 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11177 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11178 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11179 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11180 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11181 | `					sMethod = pR->sData;` |
|        17 | 11182 | `					pR++;` |
|        17 | 11183 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11184 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11185 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11186 | `							sTrait = sMethod;` |
|         7 | 11187 | `							hasQual = 1;` |
|         7 | 11188 | `							pR++;` |
|         7 | 11189 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11190 | `							sMethod = pR->sData;` |
|         7 | 11191 | `							pR++;` |
|         3 | 11192 | `						}` |
|         3 | 11193 | `					}` |
|        17 | 11194 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11195 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11196 | `						continue;` |
|         - | 11197 | `					}` |
|        17 | 11198 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11199 | `					pR++;` |
|        17 | 11200 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11201 | `						sxi32 iNewVis = -1;` |
|        13 | 11202 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11203 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11204 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11205 | `								iNewVis = nAK;` |
|         7 | 11206 | `								pR++;` |
|         3 | 11207 | `							}` |
|         3 | 11208 | `						}` |
|        13 | 11209 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11210 | `							sAlias = pR->sData;` |
|        11 | 11211 | `							pR++;` |
|         4 | 11212 | `						}` |
|        13 | 11213 | `						pMeth = 0;` |
|        13 | 11214 | `						if( hasQual ){` |
|         3 | 11215 | `							pSrcTrait = 0;` |
|         5 | 11216 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11217 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11218 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11219 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11220 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11221 | `									break;` |
|         - | 11222 | `								}` |
|         2 | 11223 | `							}` |
|         3 | 11224 | `							if( pSrcTrait ){` |
|         3 | 11225 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11226 | `							}` |
|         2 | 11227 | `						}else{` |
|        10 | 11228 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11229 | `						}` |
|        13 | 11230 | `						if( pMeth ){` |
|        13 | 11231 | `							if( sAlias.nByte > 0 ){` |
|         - | 11232 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11233 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11234 | `								 */` |
|         - | 11235 | `								ph7_class_method *pAlias;` |
|         - | 11236 | `								char *zAliasDup;` |
|        11 | 11237 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11238 | `								if( pAlias ){` |
|        11 | 11239 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11240 | `									if( iNewVis >= 0 ){` |
|         5 | 11241 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11242 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11243 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11244 | `									}` |
|        11 | 11245 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11246 | `									if( zAliasDup ){` |
|        11 | 11247 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11248 | `									}` |
|         7 | 11249 | `								}` |
|         7 | 11250 | `							}else if( iNewVis >= 0 ){` |
|         - | 11251 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11252 | `								ph7_class_method *pCopy;` |
|         3 | 11253 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11254 | `								if( pCopy ){` |
|         3 | 11255 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11256 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11257 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11258 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11259 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11260 | `									/* Replace the method in the class hash */` |
|         3 | 11261 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11262 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11263 | `								}` |
|         1 | 11264 | `							}` |
|         5 | 11265 | `						}` |
|         5 | 11266 | `						SXUNUSED(hasQual);` |
|         5 | 11267 | `					}` |
|        21 | 11268 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11269 | `				}` |
|         - | 11270 | `			}` |
|     15829 | 11271 | `			SySetRelease(&pUse->aTraits);` |
|      7917 | 11272 | `		}` |
|         - | 11273 | `	}` |
|    364211 | 11274 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11275 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11276 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3969 | 11277 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3969 | 11278 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11279 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11280 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11281 | `			return SXERR_ABORT;` |
|         - | 11282 | `		}` |
|      1982 | 11283 | `	}` |
|         - | 11284 | `	/* Install the class */` |
|    364211 | 11285 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    364211 | 11286 | `	if( rc == SXRET_OK ){` |
|         - | 11287 | `		ph7_class **apInterface;` |
|         - | 11288 | `		sxu32 n;` |
|    364211 | 11289 | `		if( pBase ){` |
|         - | 11290 | `			/* Inherit from base class and mark as a subclass */` |
|    189375 | 11291 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     94685 | 11292 | `		}` |
|    364211 | 11293 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    533825 | 11294 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11295 | `			/* Implements one or more interface */` |
|    169619 | 11296 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    169619 | 11297 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11298 | `				break;` |
|         - | 11299 | `			}` |
|     84812 | 11300 | `		}` |
|         - | 11301 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11302 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    364211 | 11303 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3969 | 11304 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3969 | 11305 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11306 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11307 | `			}` |
|      3969 | 11308 | `			if( pIntf ){` |
|      3969 | 11309 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1982 | 11310 | `			}` |
|      3969 | 11311 | `			if( pClass->nEnumBacking != 0 ){` |
|      3957 | 11312 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3957 | 11313 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11314 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11315 | `				}` |
|      3957 | 11316 | `				if( pIntf ){` |
|      3957 | 11317 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1976 | 11318 | `				}` |
|      1976 | 11319 | `			}` |
|      1982 | 11320 | `		}` |
|         - | 11321 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11322 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    364206 | 11323 | `		if( rc == SXRET_OK` |
|    364206 | 11324 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    364211 | 11325 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    193169 | 11326 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11327 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    193169 | 11328 | `			if( pStringable ){` |
|    193169 | 11329 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    193169 | 11330 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11331 | `				sxu32 i;` |
|    193169 | 11332 | `				int bAlready = 0;` |
|    232573 | 11333 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     43351 | 11334 | `					if( apImpl[i] == pStringable ){` |
|      3947 | 11335 | `						bAlready = 1;` |
|      3947 | 11336 | `						break;` |
|         - | 11337 | `					}` |
|     19707 | 11338 | `				}` |
|    193169 | 11339 | `				if( !bAlready ){` |
|    189227 | 11340 | `					PH7_ClassImplement(pClass,pStringable);` |
|     94611 | 11341 | `				}` |
|     96582 | 11342 | `			}` |
|     96582 | 11343 | `		}` |
|         - | 11344 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    364211 | 11345 | `		if( rc == SXRET_OK ){` |
|    364211 | 11346 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    364211 | 11347 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11348 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11349 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11350 | `				return SXERR_ABORT;` |
|         - | 11351 | `			}` |
|    182103 | 11352 | `		}` |
|         - | 11353 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    364211 | 11354 | `		if( rc == SXRET_OK ){` |
|    364211 | 11355 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    364211 | 11356 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11357 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11358 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11359 | `				return SXERR_ABORT;` |
|         - | 11360 | `			}` |
|    182103 | 11361 | `		}` |
|    182103 | 11362 | `	}` |
|    364211 | 11363 | `	SySetRelease(&aUseEntries);` |
|    364211 | 11364 | `	SySetRelease(&aInterfaces);` |
|    364211 | 11365 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11366 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11367 | `		return SXERR_ABORT;` |
|         - | 11368 | `	}` |
|    182103 | 11369 | `done:` |
|         - | 11370 | `	/* Point beyond the class body */` |
|    364253 | 11371 | `	pGen->pIn = &pEnd[1];` |
|    364253 | 11372 | `	pGen->pEnd = pTmp;` |
|    364253 | 11373 | `	return PH7_OK;` |
|    182130 | 11374 | `}` |
|         - | 11375 | `/* Compile a named class declaration (the common case). */` |
|    364222 | 11376 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11377 | `{` |
|    364227 | 11378 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11379 | `}` |
|         - | 11380 | `/*` |
|         - | 11381 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11382 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11383 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11384 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11385 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11386 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11387 | ` */` |
|        28 | 11388 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11389 | `{` |
|         - | 11390 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11391 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11392 | `	SyString sName;` |
|         - | 11393 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11394 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11395 | `	                              * is keyed to this 'class' token */` |
|         - | 11396 | `	ph7_value *pObj;` |
|        32 | 11397 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11398 | `	sxu32 nIdx,nLen;` |
|         - | 11399 | `	sxi32 nArg,rc;` |
|        14 | 11400 | `	SXUNUSED(iCompileFlag);` |
|         - | 11401 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11402 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11403 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11404 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11405 | `	}` |
|        32 | 11406 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11407 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11408 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11409 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11410 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11411 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11412 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11413 | `		return rc;` |
|         - | 11414 | `	}` |
|         - | 11415 | `	{` |
|         - | 11416 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11417 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11418 | `		if( pAnonClass` |
|        32 | 11419 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11420 | `			return SXERR_ABORT;` |
|         - | 11421 | `		}` |
|         - | 11422 | `	}` |
|         - | 11423 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11424 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11425 | `	nArg = 0;` |
|        32 | 11426 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11427 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11428 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11429 | `		SyToken *pArgNext;` |
|         7 | 11430 | `		pGen->pIn = pArgStart;` |
|         7 | 11431 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11432 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11433 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11434 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11435 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11436 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11437 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11438 | `					return SXERR_ABORT;` |
|         - | 11439 | `				}` |
|         7 | 11440 | `				nArg++;` |
|         3 | 11441 | `			}` |
|         7 | 11442 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11443 | `		}` |
|         7 | 11444 | `		pGen->pIn = pSavedIn;` |
|         7 | 11445 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11446 | `	}` |
|         - | 11447 | `	/* Load the synthesized class name */` |
|        32 | 11448 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11449 | `	if( pObj == 0 ){` |
|       ! 0 | 11450 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11451 | `		return SXERR_ABORT;` |
|         - | 11452 | `	}` |
|        32 | 11453 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11454 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11455 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11456 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11457 | `	return SXRET_OK;` |
|        18 | 11458 | `}` |
|         - | 11459 | `/*` |
|         - | 11460 | ` * Compile a user-defined abstract class.` |
|         - | 11461 | ` *  According to the PHP language reference manual` |
|         - | 11462 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11463 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11464 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11465 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11466 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11467 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11468 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11469 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11470 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11471 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11472 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11473 | ` *   could differ.` |
|         - | 11474 | ` */` |
|         - | 11475 | `/*` |
|         - | 11476 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11477 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11478 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11479 | ` */` |
|  11240226 | 11480 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11481 | `{` |
|  11240231 | 11482 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   6640499 | 11483 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   6640499 | 11484 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   6593193 | 11485 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3280791 | 11486 | `	}` |
|  11161319 | 11487 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  11161259 | 11488 | `	return FALSE;` |
|   5620118 | 11489 | `}` |
|         - | 11490 | `/*` |
|         - | 11491 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11492 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11493 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11494 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11495 | ` */` |
|  11161254 | 11496 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11497 | `{` |
|  11161259 | 11498 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  11161259 | 11499 | `	sxi32 iFlags = 0,iFlag;` |
|  11240231 | 11500 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     78977 | 11501 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11502 | `			pDup = pIn;` |
|         2 | 11503 | `		}` |
|     78977 | 11504 | `		iFlags \|= iFlag;` |
|     78977 | 11505 | `		pIn++;` |
|         5 | 11506 | `	}` |
|  11161259 | 11507 | `	*ppIn = pIn;` |
|  11161259 | 11508 | `	if( ppDup ){ *ppDup = pDup; }` |
|  11161259 | 11509 | `	return iFlags;` |
|         5 | 11510 | `}` |
|         - | 11511 | `/*` |
|         - | 11512 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11513 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11514 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11515 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11516 | `` * `readonly`) to their existing handlers.`` |
|         - | 11517 | ` */` |
|  11125718 | 11518 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11519 | `{` |
|  11125723 | 11520 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   5606282 | 11521 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  11147428 | 11522 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11523 | `}` |
|         - | 11524 | `/*` |
|         - | 11525 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11526 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11527 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11528 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11529 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11530 | ` */` |
|     35536 | 11531 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11532 | `{` |
|         - | 11533 | `	SyToken *pDup;` |
|     35541 | 11534 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11535 | `	sxi32 rc;` |
|     35541 | 11536 | `	if( pDup ){` |
|         4 | 11537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11538 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11539 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11540 | `			return SXERR_ABORT;` |
|         - | 11541 | `		}` |
|         1 | 11542 | `	}` |
|     35536 | 11543 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17773 | 11544 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11545 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11546 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11547 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11548 | `			return SXERR_ABORT;` |
|         - | 11549 | `		}` |
|         1 | 11550 | `	}` |
|     35541 | 11551 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17773 | 11552 | `}` |
|         - | 11553 | `/*` |
|         - | 11554 | ` * Compile a user-defined trait.` |
|         - | 11555 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11556 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11557 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11558 | ` */` |
|      7958 | 11559 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11560 | `{` |
|      7963 | 11561 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11562 | `	ph7_class *pClass;` |
|         - | 11563 | `	SyToken *pEnd,*pTmp;` |
|         - | 11564 | `	sxi32 iProtection;` |
|         - | 11565 | `	sxi32 iAttrflags;` |
|         - | 11566 | `	SyString *pName;` |
|         - | 11567 | `	sxi32 nKwrd;` |
|         - | 11568 | `	sxi32 rc;` |
|         - | 11569 | `	/* Jump the 'trait' keyword */` |
|      7963 | 11570 | `	pGen->pIn++;` |
|      7963 | 11571 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11572 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11573 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11574 | `			return SXERR_ABORT;` |
|         - | 11575 | `		}` |
|       ! 0 | 11576 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11577 | `			pGen->pIn++;` |
|       ! 0 | 11578 | `		}` |
|       ! 0 | 11579 | `		return SXRET_OK;` |
|         - | 11580 | `	}` |
|         - | 11581 | `	/* Extract trait name */` |
|      7963 | 11582 | `	pName = &pGen->pIn->sData;` |
|      7963 | 11583 | `	pGen->pIn++;` |
|         - | 11584 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11585 | `		SyBlob sFQN;` |
|         - | 11586 | `		SyString sFQNStr;` |
|      7963 | 11587 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7963 | 11588 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7963 | 11589 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7963 | 11590 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7963 | 11591 | `		SyBlobRelease(&sFQN);` |
|         - | 11592 | `	}` |
|      7963 | 11593 | `	if( pClass == 0 ){` |
|       ! 0 | 11594 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11595 | `		return SXERR_ABORT;` |
|         - | 11596 | `	}` |
|      7963 | 11597 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7963 | 11598 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11599 | `		return SXERR_ABORT;` |
|         - | 11600 | `	}` |
|         - | 11601 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7963 | 11602 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11603 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11604 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11605 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11606 | `			return SXERR_ABORT;` |
|         - | 11607 | `		}` |
|       ! 0 | 11608 | `		return SXRET_OK;` |
|         - | 11609 | `	}` |
|      7963 | 11610 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7963 | 11611 | `	pEnd = 0;` |
|      7963 | 11612 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7963 | 11613 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11614 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11615 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11616 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11617 | `			return SXERR_ABORT;` |
|         - | 11618 | `		}` |
|       ! 0 | 11619 | `		return SXRET_OK;` |
|         - | 11620 | `	}` |
|         - | 11621 | `	/* The delimiter token is the trait body's closing brace */` |
|      7963 | 11622 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11623 | `	/* Swap token stream */` |
|      7963 | 11624 | `	pTmp = pGen->pEnd;` |
|      7963 | 11625 | `	pGen->pEnd = pEnd;` |
|         - | 11626 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7963 | 11627 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11628 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     55239 | 11629 | `	for(;;){` |
|    149931 | 11630 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     19731 | 11631 | `			pGen->pIn++;` |
|         5 | 11632 | `		}` |
|    130205 | 11633 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7963 | 11634 | `			break;` |
|         - | 11635 | `		}` |
|         - | 11636 | `		/* Bind a directly-preceding docblock to this member */` |
|    122247 | 11637 | `		GenStateSetPendingDoc(&(*pGen));` |
|    122247 | 11638 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11639 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11640 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11641 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11642 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11643 | `				return SXERR_ABORT;` |
|         - | 11644 | `			}` |
|       ! 0 | 11645 | `			goto done;` |
|         - | 11646 | `		}` |
|    122247 | 11647 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    122247 | 11648 | `		iAttrflags = 0;` |
|    122247 | 11649 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    122247 | 11650 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    122247 | 11651 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11652 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11653 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11654 | `				for(;;){` |
|         - | 11655 | `					ph7_class *pUsedTrait;` |
|         - | 11656 | `					SyString *pUsedName;` |
|         5 | 11657 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11658 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11659 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11660 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11661 | `							return SXERR_ABORT;` |
|         - | 11662 | `						}` |
|       ! 0 | 11663 | `						break;` |
|         - | 11664 | `					}` |
|         5 | 11665 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11666 | `					{` |
|         - | 11667 | `						SyBlob sResolved;` |
|         5 | 11668 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11669 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11670 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11671 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11672 | `						SyBlobRelease(&sResolved);` |
|         - | 11673 | `					}` |
|         5 | 11674 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11675 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11676 | `					}` |
|         5 | 11677 | `					if( pUsedTrait == 0 ){` |
|         4 | 11678 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11679 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11680 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11681 | `							return SXERR_ABORT;` |
|         - | 11682 | `						}` |
|         2 | 11683 | `					}else{` |
|         3 | 11684 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11685 | `					}` |
|         5 | 11686 | `					pGen->pIn++;` |
|         5 | 11687 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11688 | `						break;` |
|         - | 11689 | `					}` |
|       ! 0 | 11690 | `					pGen->pIn++;` |
|       ! 0 | 11691 | `				}` |
|         5 | 11692 | `				continue;` |
|         - | 11693 | `			}` |
|    122243 | 11694 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    122227 | 11695 | `				iProtection = nKwrd;` |
|    122227 | 11696 | `				pGen->pIn++;` |
|    122222 | 11697 | `				if( pGen->pIn >= pGen->pEnd` |
|    122227 | 11698 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11699 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11700 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11701 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11702 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11703 | `						return SXERR_ABORT;` |
|         - | 11704 | `					}` |
|       ! 0 | 11705 | `					goto done;` |
|         - | 11706 | `				}` |
|    122227 | 11707 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     19717 | 11708 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     19717 | 11709 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11710 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11711 | `							return SXERR_ABORT;` |
|         - | 11712 | `						}` |
|       ! 0 | 11713 | `						goto done;` |
|         - | 11714 | `					}` |
|     19717 | 11715 | `					continue;` |
|         - | 11716 | `				}` |
|    102515 | 11717 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 11718 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 11719 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11720 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11721 | `							return SXERR_ABORT;` |
|         - | 11722 | `						}` |
|       ! 0 | 11723 | `						goto done;` |
|         - | 11724 | `					}` |
|         5 | 11725 | `					continue;` |
|         - | 11726 | `				}` |
|    102511 | 11727 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     51253 | 11728 | `			}` |
|    102527 | 11729 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 11730 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11731 | `					"Traits cannot have constants");` |
|       ! 0 | 11732 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11733 | `					return SXERR_ABORT;` |
|         - | 11734 | `				}` |
|       ! 0 | 11735 | `				goto done;` |
|       ! 0 | 11736 | `			}else{` |
|    102527 | 11737 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7895 | 11738 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7895 | 11739 | `					pGen->pIn++;` |
|      7895 | 11740 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7893 | 11741 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7893 | 11742 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11743 | `							iProtection = nKwrd;` |
|       ! 0 | 11744 | `							pGen->pIn++;` |
|       ! 0 | 11745 | `						}` |
|      3944 | 11746 | `					}` |
|      7890 | 11747 | `					if( pGen->pIn >= pGen->pEnd` |
|      7895 | 11748 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11749 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11750 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 11751 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11752 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11753 | `							return SXERR_ABORT;` |
|         - | 11754 | `						}` |
|       ! 0 | 11755 | `						goto done;` |
|         - | 11756 | `					}` |
|      7895 | 11757 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 11758 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 11759 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11760 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11761 | `								return SXERR_ABORT;` |
|         - | 11762 | `							}` |
|       ! 0 | 11763 | `							goto done;` |
|         - | 11764 | `						}` |
|         3 | 11765 | `						continue;` |
|         - | 11766 | `					}` |
|      7893 | 11767 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 11768 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11769 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11770 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11771 | `								return SXERR_ABORT;` |
|         - | 11772 | `							}` |
|       ! 0 | 11773 | `							goto done;` |
|         - | 11774 | `						}` |
|       ! 0 | 11775 | `						continue;` |
|         - | 11776 | `					}` |
|      7893 | 11777 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     98581 | 11778 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 11779 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 11780 | `					pGen->pIn++;` |
|         6 | 11781 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 11782 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 11783 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 11784 | `							iProtection = nKwrd;` |
|         6 | 11785 | `							pGen->pIn++;` |
|         2 | 11786 | `						}` |
|         2 | 11787 | `					}` |
|         6 | 11788 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 11789 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11790 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11791 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 11792 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11793 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11794 | `							return SXERR_ABORT;` |
|         - | 11795 | `						}` |
|       ! 0 | 11796 | `						goto done;` |
|         - | 11797 | `					}` |
|         6 | 11798 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 11799 | `				}` |
|    102525 | 11800 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11801 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11802 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 11803 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11804 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11805 | `						return SXERR_ABORT;` |
|         - | 11806 | `					}` |
|       ! 0 | 11807 | `					goto done;` |
|         - | 11808 | `				}` |
|    102525 | 11809 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 11810 | `					pGen->pIn++;` |
|       ! 0 | 11811 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 11812 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11813 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11814 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11815 | `							return SXERR_ABORT;` |
|         - | 11816 | `						}` |
|       ! 0 | 11817 | `						goto done;` |
|         - | 11818 | `					}` |
|       ! 0 | 11819 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11820 | `				}else{` |
|    102525 | 11821 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11822 | `				}` |
|    102525 | 11823 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11824 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11825 | `						return SXERR_ABORT;` |
|         - | 11826 | `					}` |
|       ! 0 | 11827 | `					goto done;` |
|         - | 11828 | `				}` |
|         - | 11829 | `			}` |
|     51265 | 11830 | `		}else{` |
|       ! 0 | 11831 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11832 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11833 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11834 | `					return SXERR_ABORT;` |
|         - | 11835 | `				}` |
|       ! 0 | 11836 | `				goto done;` |
|         - | 11837 | `			}` |
|         - | 11838 | `		}` |
|         5 | 11839 | `	}` |
|         - | 11840 | `	/* Install the trait */` |
|      7963 | 11841 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7963 | 11842 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11843 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11844 | `		return SXERR_ABORT;` |
|         - | 11845 | `	}` |
|      3979 | 11846 | `done:` |
|         - | 11847 | `	/* Point beyond the trait body */` |
|      7963 | 11848 | `	pGen->pIn = &pEnd[1];` |
|      7963 | 11849 | `	pGen->pEnd = pTmp;` |
|      7963 | 11850 | `	return PH7_OK;` |
|      3984 | 11851 | `}` |
|         - | 11852 | `/*` |
|         - | 11853 | ` * Compile a user-defined class.` |
|         - | 11854 | ` *  According to the PHP language reference manual` |
|         - | 11855 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 11856 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 11857 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 11858 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 11859 | ` *   and functions (called "methods").` |
|         - | 11860 | ` */` |
|    324718 | 11861 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 11862 | `{` |
|         - | 11863 | `	sxi32 rc;` |
|    324723 | 11864 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    324723 | 11865 | `	return rc;` |
|         5 | 11866 | `}` |
|         - | 11867 | `/*` |
|         - | 11868 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 11869 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 11870 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 11871 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 11872 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 11873 | ` */` |
|  11082302 | 11874 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11875 | `{` |
|  11269833 | 11876 | `	return (pIn->nType & PH7_TK_ID)` |
|   5728677 | 11877 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    197503 | 11878 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  11269828 | 11879 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 11880 | `}` |
|         - | 11881 | `/*` |
|         - | 11882 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 11883 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 11884 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 11885 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 11886 | ` */` |
|      3968 | 11887 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 11888 | `{` |
|      3973 | 11889 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 11890 | `}` |
|         - | 11891 | `/*` |
|         - | 11892 | ` * Exception handling.` |
|         - | 11893 | ` *  According to the PHP language reference manual` |
|         - | 11894 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 11895 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 11896 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 11897 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 11898 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 11899 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 11900 | ` *    (or re-thrown) within a catch block.` |
|         - | 11901 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 11902 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 11903 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 11904 | ` *    been defined with set_exception_handler().` |
|         - | 11905 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 11906 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 11907 | ` */` |
|         - | 11908 | `/*` |
|         - | 11909 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 11910 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 11911 | ` * indicates failure.` |
|         - | 11912 | ` */` |
|    492912 | 11913 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 11914 | `{` |
|    492917 | 11915 | `	sxi32 rc = SXRET_OK;` |
|    492917 | 11916 | `	if( pRoot->pOp ){` |
|    492905 | 11917 | `		switch( pRoot->pOp->iOp ){` |
|    246450 | 11918 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 11919 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 11920 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 11921 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 11922 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 11923 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    492905 | 11924 | `			break;` |
|       ! 0 | 11925 | `		default:` |
|         - | 11926 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 11927 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 11928 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 11929 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11930 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 11931 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 11932 | `				rc = SXERR_INVALID;` |
|       ! 0 | 11933 | `			}` |
|       ! 0 | 11934 | `			break;` |
|         - | 11935 | `		}` |
|    246467 | 11936 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 11937 | `		/* Unexpected expression */` |
|       ! 0 | 11938 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11939 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 11940 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 11941 | `			rc = SXERR_INVALID;` |
|       ! 0 | 11942 | `		}` |
|       ! 0 | 11943 | `	}` |
|    492917 | 11944 | `	return rc;` |
|         5 | 11945 | `}` |
|         - | 11946 | `/*` |
|         - | 11947 | ` * Compile a 'throw' statement.` |
|         - | 11948 | ` * throw: This is how you trigger an exception.` |
|         - | 11949 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 11950 | ` */` |
|    492876 | 11951 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 11952 | `{` |
|    492881 | 11953 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11954 | `	GenBlock *pBlock;` |
|         - | 11955 | `	sxu32 nIdx;` |
|         - | 11956 | `	sxi32 rc;` |
|    492881 | 11957 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 11958 | `	/* Compile the expression */` |
|    492881 | 11959 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    492881 | 11960 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 11961 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 11962 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11963 | `			return SXERR_ABORT;` |
|         - | 11964 | `		}` |
|       ! 0 | 11965 | `		return SXRET_OK;` |
|         - | 11966 | `	}` |
|    492881 | 11967 | `	pBlock = pGen->pCurrent;` |
|         - | 11968 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   1947137 | 11969 | `	while(pBlock->pParent){` |
|   1947133 | 11970 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    492877 | 11971 | `			break;` |
|         - | 11972 | `		}` |
|         - | 11973 | `		/* Point to the parent block */` |
|   1454261 | 11974 | `		pBlock = pBlock->pParent;` |
|         5 | 11975 | `	}` |
|         - | 11976 | `	/* Emit the throw instruction */` |
|    492881 | 11977 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 11978 | `	/* Emit the jump */` |
|    492881 | 11979 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    492881 | 11980 | `	return SXRET_OK;` |
|    246443 | 11981 | `}` |
|         - | 11982 | `/*` |
|         - | 11983 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 11984 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 11985 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 11986 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 11987 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 11988 | ` */` |
|        36 | 11989 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 11990 | `{` |
|        38 | 11991 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11992 | `	GenBlock *pBlock;` |
|         - | 11993 | `	sxu32 nIdx;` |
|         - | 11994 | `	sxi32 rc;` |
|        18 | 11995 | `	(void)iCompileFlag;` |
|        38 | 11996 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 11997 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 11998 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 11999 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12000 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12001 | `			return SXERR_ABORT;` |
|         - | 12002 | `		}` |
|       ! 0 | 12003 | `		return SXRET_OK;` |
|         - | 12004 | `	}` |
|        38 | 12005 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12006 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12007 | `		return SXERR_ABORT;` |
|         - | 12008 | `	}` |
|        38 | 12009 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12010 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12011 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12012 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12013 | `			return SXERR_ABORT;` |
|         - | 12014 | `		}` |
|       ! 0 | 12015 | `		return SXRET_OK;` |
|         - | 12016 | `	}` |
|         - | 12017 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12018 | `	pBlock = pGen->pCurrent;` |
|        60 | 12019 | `	while( pBlock->pParent ){` |
|        49 | 12020 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12021 | `			break;` |
|         - | 12022 | `		}` |
|        23 | 12023 | `		pBlock = pBlock->pParent;` |
|         1 | 12024 | `	}` |
|        38 | 12025 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12026 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12027 | `	return SXRET_OK;` |
|        20 | 12028 | `}` |
|         - | 12029 | `/*` |
|         - | 12030 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12031 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12032 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12033 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12034 | ` * compile error propagated from the parser.` |
|         - | 12035 | ` */` |
|        54 | 12036 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12037 | `{` |
|         - | 12038 | `	SyString sClassName;` |
|         - | 12039 | `	SyToken *pToken;` |
|         - | 12040 | `	SyString *pName;` |
|         - | 12041 | `	char *zDup;` |
|         - | 12042 | `	sxi32 rc;` |
|        59 | 12043 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        59 | 12044 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        59 | 12045 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        59 | 12046 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        59 | 12047 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12048 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12049 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12050 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12051 | `		return SXERR_INVALID;` |
|         - | 12052 | `	}` |
|        59 | 12053 | `	pGen->pIn++; /* '(' */` |
|        27 | 12054 | `	for(;;){` |
|         - | 12055 | `		SyBlob sResolved;` |
|        59 | 12056 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        59 | 12057 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12058 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12059 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12060 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12061 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12062 | `			return SXERR_INVALID;` |
|         - | 12063 | `		}` |
|        86 | 12064 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        54 | 12065 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        59 | 12066 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        59 | 12067 | `		SyBlobRelease(&sResolved);` |
|        59 | 12068 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12069 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        59 | 12070 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        54 | 12071 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12072 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12073 | `			pGen->pIn++; continue;` |
|         - | 12074 | `		}` |
|        59 | 12075 | `		break;` |
|       ! 0 | 12076 | `	}` |
|        54 | 12077 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12078 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12079 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12080 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12081 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12082 | `		return SXERR_INVALID;` |
|         - | 12083 | `	}` |
|        59 | 12084 | `	pGen->pIn++; /* '$' */` |
|        59 | 12085 | `	pName = &pGen->pIn->sData;` |
|        59 | 12086 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12087 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12088 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12089 | `	pGen->pIn++;` |
|        59 | 12090 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12091 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12092 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12093 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12094 | `		return SXERR_INVALID;` |
|         - | 12095 | `	}` |
|        59 | 12096 | `	pGen->pIn++; /* ')' */` |
|        59 | 12097 | `	return SXRET_OK;` |
|        32 | 12098 | `}` |
|         - | 12099 | `/*` |
|         - | 12100 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12101 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12102 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12103 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12104 | ` * VmThrowException):` |
|         - | 12105 | ` *` |
|         - | 12106 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12107 | ` *    <try body>` |
|         - | 12108 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12109 | ` *    JMP  -> finally\|end` |
|         - | 12110 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12111 | ` *    <catch body>` |
|         - | 12112 | ` *    JMP  -> finally\|end` |
|         - | 12113 | ` *    ... more catches ...` |
|         - | 12114 | ` *  Lfin: <finally body>` |
|         - | 12115 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12116 | ` *  Lend:` |
|         - | 12117 | ` */` |
|        98 | 12118 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12119 | `{` |
|       103 | 12120 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12121 | `	GenBlock *pTry;` |
|         - | 12122 | `	VmInstr *pInstr;` |
|       103 | 12123 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12124 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12125 | `	sxi32 rc;` |
|       103 | 12126 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12127 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       103 | 12128 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       103 | 12129 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       103 | 12130 | `	pTry->pUserData = pException;` |
|       103 | 12131 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       103 | 12132 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       103 | 12133 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       103 | 12134 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       103 | 12135 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       103 | 12136 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12137 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       103 | 12138 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       103 | 12139 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       103 | 12140 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       103 | 12141 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12142 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       103 | 12143 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12144 | `	/* Catch clauses (inline) */` |
|       103 | 12145 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        98 | 12146 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        59 | 12147 | `		sxu32 k = 0;` |
|        81 | 12148 | `		for(;;){` |
|         - | 12149 | `			ph7_exception_block sCatch;` |
|         - | 12150 | `			GenBlock *pCatchBlk;` |
|       113 | 12151 | `			sxu32 idxJmp = 0;` |
|       108 | 12152 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       104 | 12153 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        32 | 12154 | `				break;` |
|         - | 12155 | `			}` |
|        59 | 12156 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        59 | 12157 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12158 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        59 | 12159 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        59 | 12160 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        59 | 12161 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        59 | 12162 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12163 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12164 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12165 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        59 | 12166 | `			pCatchBlk->pUserData = pException;` |
|        59 | 12167 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        59 | 12168 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12169 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        59 | 12170 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12171 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12172 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        59 | 12173 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        59 | 12174 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        59 | 12175 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        59 | 12176 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        59 | 12177 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        59 | 12178 | `			k++;` |
|         5 | 12179 | `		}` |
|        27 | 12180 | `	}` |
|         - | 12181 | `	/* Finally (inline) */` |
|       103 | 12182 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12183 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12184 | `		GenBlock *pFinBlk;` |
|        52 | 12185 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12186 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12187 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12188 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12189 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12190 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12191 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12192 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12193 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12194 | `		pException->iHasFinally = 1;` |
|        24 | 12195 | `	}` |
|       103 | 12196 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       103 | 12197 | `	pException->iInlined = 1;` |
|         - | 12198 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12199 | `	{` |
|       103 | 12200 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12201 | `		sxu32 *aJ; sxu32 n;` |
|       103 | 12202 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       103 | 12203 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       103 | 12204 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       157 | 12205 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        59 | 12206 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        59 | 12207 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        32 | 12208 | `		}` |
|         - | 12209 | `	}` |
|       103 | 12210 | `	SySetRelease(&aCatchJmp);` |
|       103 | 12211 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12212 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12213 | `	}` |
|       103 | 12214 | `	return SXRET_OK;` |
|        54 | 12215 | `}` |
|         - | 12216 | `/*` |
|         - | 12217 | ` * Compile a 'catch' block.` |
|         - | 12218 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12219 | ` * an object containing the exception information.` |
|         - | 12220 | ` */` |
|     25118 | 12221 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12222 | `{` |
|     25123 | 12223 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12224 | `	ph7_exception_block sCatch;` |
|         - | 12225 | `	SySet *pInstrContainer;` |
|         - | 12226 | `	SyString sClassName;` |
|         - | 12227 | `	GenBlock *pCatch;` |
|         - | 12228 | `	SyToken *pToken;` |
|         - | 12229 | `	SyString *pName;` |
|         - | 12230 | `	char *zDup;` |
|         - | 12231 | `	sxi32 rc;` |
|     25123 | 12232 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12233 | `	/* Zero the structure */` |
|     25123 | 12234 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12235 | `	/* Initialize fields */` |
|     25123 | 12236 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     25123 | 12237 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     25123 | 12238 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12239 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12240 | `			pToken = pGen->pIn;` |
|       ! 0 | 12241 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12242 | `				pToken--;` |
|       ! 0 | 12243 | `			}` |
|       ! 0 | 12244 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12245 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12246 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12247 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12248 | `				return SXERR_ABORT;` |
|         - | 12249 | `			}` |
|       ! 0 | 12250 | `			return SXERR_INVALID;` |
|         - | 12251 | `	}` |
|         - | 12252 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     25123 | 12253 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12573 | 12254 | `	for(;;){` |
|         - | 12255 | `		SyBlob sResolved;` |
|     25151 | 12256 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     25151 | 12257 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12258 | `			SyBlobRelease(&sResolved);` |
|         6 | 12259 | `			pToken = pGen->pIn;` |
|         6 | 12260 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12261 | `				pToken--;` |
|       ! 0 | 12262 | `			}` |
|         8 | 12263 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12264 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12265 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12266 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12267 | `				return SXERR_ABORT;` |
|         - | 12268 | `			}` |
|         6 | 12269 | `			return SXERR_INVALID;` |
|         - | 12270 | `		}` |
|         - | 12271 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12272 | `		 * transient SyBlob allocation. */` |
|     37718 | 12273 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     25142 | 12274 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     25147 | 12275 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     25147 | 12276 | `		SyBlobRelease(&sResolved);` |
|     25147 | 12277 | `		if( zDup == 0 ){` |
|       ! 0 | 12278 | `			goto Mem;` |
|         - | 12279 | `		}` |
|     25147 | 12280 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     25147 | 12281 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12282 | `			goto Mem;` |
|         - | 12283 | `		}` |
|         - | 12284 | `		/* Check for '\|' (multi-catch separator) */` |
|     25142 | 12285 | `		if( pGen->pIn < pGen->pEnd &&` |
|     25142 | 12286 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        33 | 12287 | `			pGen->pIn->sData.nByte == 1 &&` |
|        28 | 12288 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        30 | 12289 | `			pGen->pIn++; /* Consume the '\|' */` |
|        30 | 12290 | `			continue;` |
|         - | 12291 | `		}` |
|     25119 | 12292 | `		break;` |
|       ! 0 | 12293 | `	}` |
|     25114 | 12294 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     25119 | 12295 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12296 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12297 | `			pToken = pGen->pIn;` |
|       ! 0 | 12298 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12299 | `				pToken--;` |
|       ! 0 | 12300 | `			}` |
|       ! 0 | 12301 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12302 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12303 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12304 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12305 | `				return SXERR_ABORT;` |
|         - | 12306 | `			}` |
|       ! 0 | 12307 | `			return SXERR_INVALID;` |
|         - | 12308 | `	}` |
|     25119 | 12309 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12310 | `	/* Duplicate instance name */` |
|     25119 | 12311 | `	pName = &pGen->pIn->sData;` |
|     25119 | 12312 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     25119 | 12313 | `	if( zDup == 0 ){` |
|       ! 0 | 12314 | `		goto Mem;` |
|         - | 12315 | `	}` |
|     25119 | 12316 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     25119 | 12317 | `	pGen->pIn++;` |
|     25119 | 12318 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12319 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12320 | `		pToken = pGen->pIn;` |
|       ! 0 | 12321 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12322 | `			pToken--;` |
|       ! 0 | 12323 | `		}` |
|       ! 0 | 12324 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12325 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12326 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12327 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12328 | `			return SXERR_ABORT;` |
|         - | 12329 | `		}` |
|       ! 0 | 12330 | `		return SXERR_INVALID;` |
|         - | 12331 | `	}` |
|         - | 12332 | `	/* Compile the block */` |
|     25119 | 12333 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12334 | `	/* Create the catch block */` |
|     25119 | 12335 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     25119 | 12336 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12337 | `		return SXERR_ABORT;` |
|         - | 12338 | `	}` |
|         - | 12339 | `	/* Swap bytecode container */` |
|     25119 | 12340 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     25119 | 12341 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12342 | `	/* Compile the block */` |
|     25119 | 12343 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12344 | `	/* Fix forward jumps now the destination is resolved  */` |
|     25119 | 12345 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12346 | `	/* Emit the DONE instruction */` |
|     25119 | 12347 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12348 | `	/* Leave the block */` |
|     25119 | 12349 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12350 | `	/* Restore the default container */` |
|     25119 | 12351 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12352 | `	/* Install the catch block */` |
|     25119 | 12353 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     25119 | 12354 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12355 | `		goto Mem;` |
|         - | 12356 | `	}` |
|     25119 | 12357 | `	return SXRET_OK;` |
|       ! 0 | 12358 | `Mem:` |
|       ! 0 | 12359 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12360 | `	return SXERR_ABORT;` |
|     12564 | 12361 | `}` |
|         - | 12362 | `/*` |
|         - | 12363 | ` * Compile a 'try' block.` |
|         - | 12364 | ` * A function using an exception should be in a "try" block.` |
|         - | 12365 | ` * If the exception does not trigger, the code will continue` |
|         - | 12366 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12367 | ` * is "thrown".` |
|         - | 12368 | ` */` |
|     25274 | 12369 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12370 | `{` |
|         - | 12371 | `	ph7_exception *pException;` |
|     25279 | 12372 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12373 | `	GenBlock *pTry;` |
|         - | 12374 | `	sxu32 nJmpIdx;` |
|         - | 12375 | `	sxi32 rc;` |
|         - | 12376 | `	/* Create the exception container */` |
|     25279 | 12377 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     25279 | 12378 | `	if( pException == 0 ){` |
|       ! 0 | 12379 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12380 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12381 | `		return SXERR_ABORT;` |
|         - | 12382 | `	}` |
|         - | 12383 | `	/* Zero the structure */` |
|     25279 | 12384 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12385 | `	/* Initialize fields */` |
|     25279 | 12386 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     25279 | 12387 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     25279 | 12388 | `	pException->iHasFinally = 0;` |
|     25279 | 12389 | `	pException->iFinallyDone = 0;` |
|     25279 | 12390 | `	pException->pVm = pGen->pVm;` |
|         - | 12391 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12392 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12393 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12394 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12395 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12396 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     25279 | 12397 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       103 | 12398 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12399 | `	}` |
|         - | 12400 | `	/* Create the try block */` |
|     25181 | 12401 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     25181 | 12402 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12403 | `		return SXERR_ABORT;` |
|         - | 12404 | `	}` |
|         - | 12405 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     25181 | 12406 | `	pTry->pUserData = pException;` |
|         - | 12407 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     25181 | 12408 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12409 | `	/* Fix the jump later when the destination is resolved */` |
|     25181 | 12410 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     25181 | 12411 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12412 | `	/* Compile the block */` |
|     25181 | 12413 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     25181 | 12414 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12415 | `		return SXERR_ABORT;` |
|         - | 12416 | `	}` |
|         - | 12417 | `	/* Fix forward jumps now the destination is resolved */` |
|     25181 | 12418 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12419 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     25181 | 12420 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12421 | `	/* Leave the block */` |
|     25181 | 12422 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12423 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     25181 | 12424 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     25174 | 12425 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12426 | `		/* Compile one or more catch blocks */` |
|     25114 | 12427 | `		for(;;){` |
|     50228 | 12428 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     37733 | 12429 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12560 | 12430 | `					break;` |
|         - | 12431 | `			}` |
|     25123 | 12432 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     25123 | 12433 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12434 | `				return SXERR_ABORT;` |
|         - | 12435 | `			}` |
|         5 | 12436 | `		}` |
|     12555 | 12437 | `	}` |
|         - | 12438 | `	/* Compile optional finally block */` |
|     25181 | 12439 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       728 | 12440 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12441 | `		SySet *pInstrContainer;` |
|         - | 12442 | `		GenBlock *pFinBlock;` |
|       129 | 12443 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12444 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12445 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12446 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12447 | `			return SXERR_ABORT;` |
|         - | 12448 | `		}` |
|         - | 12449 | `		/* Swap bytecode container */` |
|       129 | 12450 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12451 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12452 | `		/* Compile the finally body */` |
|       129 | 12453 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12454 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12455 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12456 | `			return SXERR_ABORT;` |
|         - | 12457 | `		}` |
|         - | 12458 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12459 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12460 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12461 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12462 | `		/* Leave the block */` |
|       129 | 12463 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12464 | `		/* Restore the default container */` |
|       129 | 12465 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12466 | `		pException->iHasFinally = 1;` |
|        62 | 12467 | `	}` |
|         - | 12468 | `	/* Must have at least one catch or finally */` |
|     25181 | 12469 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         9 | 12470 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12471 | `			"Cannot use try without catch or finally");` |
|         9 | 12472 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12473 | `			return SXERR_ABORT;` |
|         - | 12474 | `		}` |
|         3 | 12475 | `	}` |
|     25181 | 12476 | `	return SXRET_OK;` |
|     12642 | 12477 | `}` |
|         - | 12478 | `/*` |
|         - | 12479 | ` * Compile a switch block.` |
|         - | 12480 | ` *  (See block-comment below for more information)` |
|         - | 12481 | ` */` |
|       112 | 12482 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12483 | `{` |
|       117 | 12484 | `	sxi32 rc = SXRET_OK;` |
|       117 | 12485 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12486 | `		/* Unexpected token */` |
|       ! 0 | 12487 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12488 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12489 | `			return SXERR_ABORT;` |
|         - | 12490 | `		}` |
|       ! 0 | 12491 | `		pGen->pIn++;` |
|       ! 0 | 12492 | `	}` |
|       117 | 12493 | `	pGen->pIn++;` |
|         - | 12494 | `	/* First instruction to execute in this block. */` |
|       117 | 12495 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12496 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12497 | `	 * or the '}' token */` |
|       206 | 12498 | `	for(;;){` |
|       417 | 12499 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12500 | `			/* No more input to process */` |
|       ! 0 | 12501 | `			break;` |
|         - | 12502 | `		}` |
|       417 | 12503 | `		rc = SXRET_OK;` |
|       417 | 12504 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        85 | 12505 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|        31 | 12506 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12507 | `					/* Unexpected token */` |
|       ! 0 | 12508 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12509 | `						&pGen->pIn->sData);` |
|       ! 0 | 12510 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12511 | `						return SXERR_ABORT;` |
|         - | 12512 | `					}` |
|         - | 12513 | `					/* FALL THROUGH */` |
|       ! 0 | 12514 | `				}` |
|        31 | 12515 | `				rc = SXERR_EOF;` |
|        31 | 12516 | `				break;` |
|         - | 12517 | `			}` |
|        32 | 12518 | `		}else{` |
|         - | 12519 | `			sxi32 nKwrd;` |
|         - | 12520 | `			/* Extract the keyword */` |
|       337 | 12521 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       337 | 12522 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|        47 | 12523 | `				break;` |
|         - | 12524 | `			}` |
|       253 | 12525 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12526 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12527 | `					/* Unexpected token */` |
|       ! 0 | 12528 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12529 | `						&pGen->pIn->sData);` |
|       ! 0 | 12530 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12531 | `						return SXERR_ABORT;` |
|         - | 12532 | `					}` |
|         - | 12533 | `					/* FALL THROUGH */` |
|       ! 0 | 12534 | `				}` |
|         - | 12535 | `				/* Block compiled */` |
|         3 | 12536 | `				break;` |
|         - | 12537 | `			}` |
|         - | 12538 | `		}` |
|         - | 12539 | `		/* Compile block */` |
|       305 | 12540 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       305 | 12541 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12542 | `			return SXERR_ABORT;` |
|         - | 12543 | `		}` |
|         5 | 12544 | `	}` |
|       117 | 12545 | `	return rc;` |
|        61 | 12546 | `}` |
|         - | 12547 | `/*` |
|         - | 12548 | ` * Compile a case eXpression.` |
|         - | 12549 | ` *  (See block-comment below for more information)` |
|         - | 12550 | ` */` |
|        92 | 12551 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12552 | `{` |
|         - | 12553 | `	SySet *pInstrContainer;` |
|         - | 12554 | `	SyToken *pEnd,*pTmp;` |
|        97 | 12555 | `	sxi32 iNest = 0;` |
|         - | 12556 | `	sxi32 rc;` |
|         - | 12557 | `	/* Delimit the expression */` |
|        97 | 12558 | `	pEnd = pGen->pIn;` |
|       197 | 12559 | `	while( pEnd < pGen->pEnd ){` |
|       197 | 12560 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12561 | `			/* Increment nesting level */` |
|         3 | 12562 | `			iNest++;` |
|       196 | 12563 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12564 | `			/* Decrement nesting level */` |
|         3 | 12565 | `			iNest--;` |
|       194 | 12566 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|        97 | 12567 | `			break;` |
|         - | 12568 | `		}` |
|       105 | 12569 | `		pEnd++;` |
|         5 | 12570 | `	}` |
|        97 | 12571 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12572 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12573 | `		if( rc == SXERR_ABORT ){` |
|         - | 12574 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12575 | `			return SXERR_ABORT;` |
|         - | 12576 | `		}` |
|       ! 0 | 12577 | `	}` |
|         - | 12578 | `	/* Swap token stream */` |
|        97 | 12579 | `	pTmp = pGen->pEnd;` |
|        97 | 12580 | `	pGen->pEnd = pEnd;` |
|        97 | 12581 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        97 | 12582 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|        97 | 12583 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12584 | `	/* Emit the done instruction */` |
|        97 | 12585 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        97 | 12586 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12587 | `	/* Update token stream */` |
|        97 | 12588 | `	pGen->pIn  = pEnd;` |
|        97 | 12589 | `	pGen->pEnd = pTmp;` |
|        97 | 12590 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12591 | `		return SXERR_ABORT;` |
|         - | 12592 | `	}` |
|        97 | 12593 | `	return SXRET_OK;` |
|        51 | 12594 | `}` |
|         - | 12595 | `/*` |
|         - | 12596 | ` * Compile the smart switch statement.` |
|         - | 12597 | ` * According to the PHP language reference manual` |
|         - | 12598 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12599 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12600 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12601 | ` *  This is exactly what the switch statement is for.` |
|         - | 12602 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12603 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12604 | ` *  of the outer loop, use continue 2.` |
|         - | 12605 | ` *  Note that switch/case does loose comparision.` |
|         - | 12606 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12607 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12608 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12609 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12610 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12611 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12612 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12613 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12614 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12615 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12616 | ` *  list for the next case.` |
|         - | 12617 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12618 | ` *  or floating-point numbers and strings.` |
|         - | 12619 | ` */` |
|        28 | 12620 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12621 | `{` |
|         - | 12622 | `	GenBlock *pSwitchBlock;` |
|         - | 12623 | `	SyToken *pTmp,*pEnd;` |
|         - | 12624 | `	ph7_switch *pSwitch;` |
|         - | 12625 | `	sxu32 nToken;` |
|         - | 12626 | `	sxu32 nLine;` |
|         - | 12627 | `	sxi32 rc;` |
|        33 | 12628 | `	nLine = pGen->pIn->nLine;` |
|         - | 12629 | `	/* Jump the 'switch' keyword */` |
|        33 | 12630 | `	pGen->pIn++;` |
|        33 | 12631 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12632 | `		/* Syntax error */` |
|       ! 0 | 12633 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12634 | `		if( rc == SXERR_ABORT ){` |
|         - | 12635 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12636 | `			return SXERR_ABORT;` |
|         - | 12637 | `		}` |
|       ! 0 | 12638 | `		goto Synchronize;` |
|         - | 12639 | `	}` |
|         - | 12640 | `	/* Jump the left parenthesis '(' */` |
|        33 | 12641 | `	pGen->pIn++;` |
|        33 | 12642 | `	pEnd = 0; /* cc warning */` |
|         - | 12643 | `	/* Create the loop block */` |
|        47 | 12644 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|        14 | 12645 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|        33 | 12646 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12647 | `		return SXERR_ABORT;` |
|         - | 12648 | `	}` |
|         - | 12649 | `	/* Delimit the condition */` |
|        33 | 12650 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|        33 | 12651 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12652 | `		/* Empty expression */` |
|       ! 0 | 12653 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12654 | `		if( rc == SXERR_ABORT ){` |
|         - | 12655 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12656 | `			return SXERR_ABORT;` |
|         - | 12657 | `		}` |
|       ! 0 | 12658 | `	}` |
|         - | 12659 | `	/* Swap token streams */` |
|        33 | 12660 | `	pTmp = pGen->pEnd;` |
|        33 | 12661 | `	pGen->pEnd = pEnd;` |
|         - | 12662 | `	/* Compile the expression */` |
|        33 | 12663 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        33 | 12664 | `	if( rc == SXERR_ABORT ){` |
|         - | 12665 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12666 | `		return SXERR_ABORT;` |
|         - | 12667 | `	}` |
|         - | 12668 | `	/* Update token stream */` |
|        33 | 12669 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12670 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12671 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12672 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12673 | `			return SXERR_ABORT;` |
|         - | 12674 | `		}` |
|       ! 0 | 12675 | `		pGen->pIn++;` |
|       ! 0 | 12676 | `	}` |
|        33 | 12677 | `	pGen->pIn  = &pEnd[1];` |
|        33 | 12678 | `	pGen->pEnd = pTmp;` |
|        33 | 12679 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        28 | 12680 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12681 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12682 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12683 | `				pTmp--;` |
|       ! 0 | 12684 | `			}` |
|         - | 12685 | `			/* Unexpected token */` |
|       ! 0 | 12686 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12687 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12688 | `				return SXERR_ABORT;` |
|         - | 12689 | `			}` |
|       ! 0 | 12690 | `			goto Synchronize;` |
|         - | 12691 | `	}` |
|         - | 12692 | `	/* Set the delimiter token */` |
|        33 | 12693 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12694 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12695 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12696 | `	}else{` |
|        31 | 12697 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12698 | `	}` |
|        33 | 12699 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12700 | `	/* Create the switch blocks container */` |
|        33 | 12701 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|        33 | 12702 | `	if( pSwitch == 0 ){` |
|         - | 12703 | `		/* Abort compilation */` |
|       ! 0 | 12704 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12705 | `		return SXERR_ABORT;` |
|         - | 12706 | `	}` |
|         - | 12707 | `	/* Zero the structure */` |
|        33 | 12708 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 12709 | `	/* Initialize fields */` |
|        33 | 12710 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 12711 | `	/* Emit the switch instruction */` |
|        33 | 12712 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 12713 | `	/* Compile case blocks */` |
|       100 | 12714 | `	for(;;){` |
|         - | 12715 | `		sxu32 nKwrd;` |
|       119 | 12716 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12717 | `			/* No more input to process */` |
|       ! 0 | 12718 | `			break;` |
|         - | 12719 | `		}` |
|       119 | 12720 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 12721 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 12722 | `				/* Unexpected token */` |
|       ! 0 | 12723 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12724 | `					&pGen->pIn->sData);` |
|       ! 0 | 12725 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12726 | `					return SXERR_ABORT;` |
|         - | 12727 | `				}` |
|         - | 12728 | `				/* FALL THROUGH */` |
|       ! 0 | 12729 | `			}` |
|         - | 12730 | `			/* Block compiled */` |
|       ! 0 | 12731 | `			break;` |
|         - | 12732 | `		}` |
|         - | 12733 | `		/* Extract the keyword */` |
|       119 | 12734 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       119 | 12735 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12736 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 12737 | `				/* Unexpected token */` |
|       ! 0 | 12738 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12739 | `					&pGen->pIn->sData);` |
|       ! 0 | 12740 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12741 | `					return SXERR_ABORT;` |
|         - | 12742 | `				}` |
|         - | 12743 | `				/* FALL THROUGH */` |
|       ! 0 | 12744 | `			}` |
|         - | 12745 | `			/* Block compiled */` |
|         3 | 12746 | `			break;` |
|         - | 12747 | `		}` |
|       117 | 12748 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 12749 | `			/*` |
|         - | 12750 | `			 * Accroding to the PHP language reference manual` |
|         - | 12751 | `			 *  A special case is the default case. This case matches anything` |
|         - | 12752 | `			 *  that wasn't matched by the other cases.` |
|         - | 12753 | `			 */` |
|        25 | 12754 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 12755 | `				/* Default case already compiled */` |
|       ! 0 | 12756 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 12757 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12758 | `					return SXERR_ABORT;` |
|         - | 12759 | `				}` |
|       ! 0 | 12760 | `			}` |
|        25 | 12761 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 12762 | `			/* Compile the default block */` |
|        25 | 12763 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 12764 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12765 | `				return SXERR_ABORT;` |
|        25 | 12766 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 12767 | `				break;` |
|         1 | 12768 | `			}` |
|        98 | 12769 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 12770 | `			ph7_case_expr sCase;` |
|         - | 12771 | `			/* Standard case block */` |
|        97 | 12772 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 12773 | `			/* initialize the structure */` |
|        97 | 12774 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 12775 | `			/* Compile the case expression */` |
|        97 | 12776 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|        97 | 12777 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12778 | `				return SXERR_ABORT;` |
|         - | 12779 | `			}` |
|         - | 12780 | `			/* Compile the case block */` |
|        97 | 12781 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 12782 | `			/* Insert in the switch container */` |
|        97 | 12783 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|        97 | 12784 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12785 | `				return SXERR_ABORT;` |
|        97 | 12786 | `			}else if( rc == SXERR_EOF ){` |
|         9 | 12787 | `				break;` |
|         - | 12788 | `			}` |
|        47 | 12789 | `		}else{` |
|         - | 12790 | `			/* Unexpected token */` |
|       ! 0 | 12791 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12792 | `				&pGen->pIn->sData);` |
|       ! 0 | 12793 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12794 | `				return SXERR_ABORT;` |
|         - | 12795 | `			}` |
|       ! 0 | 12796 | `			break;` |
|         - | 12797 | `		}` |
|         5 | 12798 | `	}` |
|         - | 12799 | `	/* Fix all jumps now the destination is resolved */` |
|        33 | 12800 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|        33 | 12801 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12802 | `	/* Release the loop block */` |
|        33 | 12803 | `	GenStateLeaveBlock(pGen,0);` |
|        33 | 12804 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 12805 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|        33 | 12806 | `		pGen->pIn++;` |
|        14 | 12807 | `	}` |
|         - | 12808 | `	/* Statement successfully compiled */` |
|        33 | 12809 | `	return SXRET_OK;` |
|       ! 0 | 12810 | `Synchronize:` |
|         - | 12811 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 12812 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 12813 | `		pGen->pIn++;` |
|       ! 0 | 12814 | `	}` |
|       ! 0 | 12815 | `	return SXRET_OK;` |
|        19 | 12816 | `}` |
|         - | 12817 | `/*` |
|         - | 12818 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 12819 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 12820 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 12821 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 12822 | ` */` |
|         - | 12823 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 12824 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 12825 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 12826 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 12827 |  |
|         - | 12828 | `/*` |
|         - | 12829 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 12830 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 12831 | ` * patched entries from the pending set.` |
|         - | 12832 | ` */` |
|  41134878 | 12833 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 12834 | `{` |
|  41134883 | 12835 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 12836 | `	sxu32 nTarget;` |
|         - | 12837 | `	sxu32 *aIdx;` |
|         - | 12838 | `	sxu32 i;` |
|  41134883 | 12839 | `	if( nCur <= nBaseline ){` |
|  41134787 | 12840 | `		return;` |
|         - | 12841 | `	}` |
|       100 | 12842 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 12843 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 12844 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 12845 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 12846 | `		if( pInstr ){` |
|       108 | 12847 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 12848 | `		}` |
|        56 | 12849 | `	}` |
|       100 | 12850 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  20567444 | 12851 | `}` |
|         - | 12852 |  |
|         - | 12853 | `/*` |
|         - | 12854 | ` * By-reference out-parameters of builtin functions.` |
|         - | 12855 | ` *` |
|         - | 12856 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 12857 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 12858 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 12859 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 12860 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 12861 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 12862 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 12863 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 12864 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 12865 | ` * creates it" behaviour).` |
|         - | 12866 | ` *` |
|         - | 12867 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 12868 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 12869 | ` */` |
|   5588366 | 12870 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 12871 | `{` |
|         - | 12872 | `	static const struct {` |
|         - | 12873 | `		const char *zName;` |
|         - | 12874 | `		sxu32 nByte;` |
|         - | 12875 | `		sxu32 mask;` |
|         - | 12876 | `	} aByRef[] = {` |
|         - | 12877 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12878 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12879 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12880 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12881 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 12882 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 12883 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 12884 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 12885 | `	};` |
|         - | 12886 | `	sxu32 i;` |
|   5588371 | 12887 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1647581 | 12888 | `		return 0;` |
|         - | 12889 | `	}` |
|  35162945 | 12890 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  31261680 | 12891 | `		if( pName->nByte == aByRef[i].nByte` |
|  16335812 | 12892 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     39535 | 12893 | `			return aByRef[i].mask;` |
|         - | 12894 | `		}` |
|  15611080 | 12895 | `	}` |
|   3901265 | 12896 | `	return 0;` |
|   2794188 | 12897 | `}` |
|         - | 12898 | `/*` |
|         - | 12899 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 12900 | ` *` |
|         - | 12901 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 12902 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 12903 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 12904 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 12905 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 12906 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 12907 | ` */` |
|   5588366 | 12908 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 12909 | `{` |
|         - | 12910 | `	SyToken *p, *pEnd;` |
|   5588371 | 12911 | `	pOut->zString = 0;` |
|   5588371 | 12912 | `	pOut->nByte = 0;` |
|   5588371 | 12913 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 12914 | `		return;` |
|         - | 12915 | `	}` |
|   5588371 | 12916 | `	p = pLeft->pStart;` |
|   5588371 | 12917 | `	pEnd = pLeft->pEnd;` |
|         - | 12918 | `	/* Optional single leading namespace separator (absolute path). */` |
|   5588371 | 12919 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3973 | 12920 | `		p++;` |
|      1984 | 12921 | `	}` |
|   5588371 | 12922 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1647545 | 12923 | `		return;` |
|         - | 12924 | `	}` |
|         - | 12925 | `	/* Must be a single component: nothing follows the name token. */` |
|   3940831 | 12926 | `	if( p + 1 != pEnd ){` |
|        40 | 12927 | `		return;` |
|         - | 12928 | `	}` |
|   3940795 | 12929 | `	*pOut = p->sData;` |
|   2794188 | 12930 | `}` |
|         - | 12931 | `/*` |
|         - | 12932 | ` * Generate bytecode for a given expression tree.` |
|         - | 12933 | ` * If something goes wrong while generating bytecode` |
|         - | 12934 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 12935 | ` * this function takes care of generating the appropriate` |
|         - | 12936 | ` * error message.` |
|         - | 12937 | ` */` |
|  59427422 | 12938 | `static sxi32 GenStateEmitExprCode(` |
|         - | 12939 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 12940 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 12941 | `	sxi32 iFlags /* Control flags */` |
|         - | 12942 | `	)` |
|         5 | 12943 | `{` |
|         - | 12944 | `	VmInstr *pInstr;` |
|         - | 12945 | `	sxu32 nJmpIdx;` |
|  59427427 | 12946 | `	sxi32 iP1 = 0;` |
|  59427427 | 12947 | `	sxu32 iP2 = 0;` |
|  59427427 | 12948 | `	void *p3  = 0;` |
|         - | 12949 | `	sxi32 iVmOp;` |
|         - | 12950 | `	sxi32 rc;` |
|  59427427 | 12951 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  59427427 | 12952 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  59427427 | 12953 | `	sxu32 nRhsNsBase = 0;` |
|  59427427 | 12954 | `	if( pNode->xCode ){` |
|         - | 12955 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 12956 | `		/* Compile node */` |
|  35544385 | 12957 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  35544385 | 12958 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  35544385 | 12959 | `		RE_SWAP_DELIMITER(pGen);` |
|  35544385 | 12960 | `		return rc;` |
|         - | 12961 | `	}` |
|  23883047 | 12962 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 12963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 12964 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 12965 | `		return SXERR_ABORT;` |
|         - | 12966 | `	}` |
|  23883047 | 12967 | `	iVmOp = pNode->pOp->iVmOp;` |
|  23883047 | 12968 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 12969 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 12970 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 12971 | `		 * and later errors are still reported. */` |
|         3 | 12972 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 12973 | `			"The (unset) cast is no longer supported");` |
|         3 | 12974 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12975 | `			return SXERR_ABORT;` |
|         - | 12976 | `		}` |
|         1 | 12977 | `	}` |
|  23883047 | 12978 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        91 | 12979 | `		sxu32 nJmp = 0;` |
|         - | 12980 | `		sxu32 nNcNsBase;` |
|         - | 12981 | `		VmInstr *pInstrFix;` |
|         - | 12982 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 12983 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 12984 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 12985 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 12986 | `		 * stack slot carries a writable nIdx. */` |
|        91 | 12987 | `		if( pNode->pRight ){` |
|        91 | 12988 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 12989 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        91 | 12990 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12991 | `				return rc;` |
|         - | 12992 | `			}` |
|        91 | 12993 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 12994 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 12995 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 12996 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 12997 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 12998 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 12999 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13000 | `			 * cascade for the actual write path stays correct. */` |
|        91 | 13001 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        91 | 13002 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13003 | `				pInstrFix->iP2 = 3;` |
|        15 | 13004 | `			}` |
|        44 | 13005 | `		}` |
|         - | 13006 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        91 | 13007 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13008 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        91 | 13009 | `		if( pNode->pLeft ){` |
|        91 | 13010 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13011 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        91 | 13012 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13013 | `				return rc;` |
|         - | 13014 | `			}` |
|        91 | 13015 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        44 | 13016 | `		}` |
|         - | 13017 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        91 | 13018 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13019 | `		/* Patch the short-circuit jump to land after the store. */` |
|        91 | 13020 | `		if( nJmp > 0 ){` |
|        91 | 13021 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        91 | 13022 | `			if( pInstrFix ){` |
|        91 | 13023 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        44 | 13024 | `			}` |
|        44 | 13025 | `		}` |
|        91 | 13026 | `		return SXRET_OK;` |
|         - | 13027 | `	}` |
|  23882959 | 13028 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13029 | `		sxu32 nJz,nJmp;` |
|         - | 13030 | `		sxu32 nTernaryNsBase;` |
|         - | 13031 | `		/* Ternary operator require special handling */` |
|         - | 13032 | `		/* Phase#1: Compile the condition */` |
|    381535 | 13033 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    381535 | 13034 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    381535 | 13035 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13036 | `			return rc;` |
|         - | 13037 | `		}` |
|         - | 13038 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13039 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13040 | `		 * condition expression, not leak past the ternary. */` |
|    381535 | 13041 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    381535 | 13042 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    381535 | 13043 | `		if( pNode->pLeft ){` |
|         - | 13044 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13045 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    377527 | 13046 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13047 | `			/* Phase#3: Compile the 'then' expression  */` |
|    377527 | 13048 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    377527 | 13049 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    377527 | 13050 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13051 | `				return rc;` |
|         - | 13052 | `			}` |
|    377527 | 13053 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    188766 | 13054 | `		}else{` |
|         - | 13055 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13056 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13057 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      4013 | 13058 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      4013 | 13059 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13060 | `		}` |
|         - | 13061 | `		/* Phase#4: Emit the unconditional jump */` |
|    381535 | 13062 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13063 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    381535 | 13064 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    381535 | 13065 | `		if( pInstr ){` |
|    381535 | 13066 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    190765 | 13067 | `		}` |
|    381535 | 13068 | `		if( !pNode->pLeft ){` |
|         - | 13069 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      4013 | 13070 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      2004 | 13071 | `		}` |
|         - | 13072 | `		/* Phase#6: Compile the 'else' expression */` |
|    381535 | 13073 | `		if( pNode->pRight ){` |
|    381535 | 13074 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    381535 | 13075 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    381535 | 13076 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13077 | `				return rc;` |
|         - | 13078 | `			}` |
|    381535 | 13079 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    190765 | 13080 | `		}` |
|    381535 | 13081 | `		if( nJmp > 0 ){` |
|         - | 13082 | `			/* Phase#7: Fix the unconditional jump */` |
|    381535 | 13083 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    381535 | 13084 | `			if( pInstr ){` |
|    381535 | 13085 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    190765 | 13086 | `			}` |
|    190765 | 13087 | `		}` |
|         - | 13088 | `		/* All done */` |
|    381535 | 13089 | `		return SXRET_OK;` |
|         - | 13090 | `	}` |
|  23501429 | 13091 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13092 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13093 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13094 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13095 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13096 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13097 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13098 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13099 | `		sxu32 nPipeNsBase;` |
|        27 | 13100 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13101 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13102 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13103 | `				"'\|>': Missing operand");` |
|       ! 0 | 13104 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13105 | `		}` |
|         - | 13106 | `		/* Argument: the LHS value. */` |
|        27 | 13107 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13108 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13109 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13110 | `			return rc;` |
|         - | 13111 | `		}` |
|        27 | 13112 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13113 | `		/* Callable: the RHS. */` |
|        27 | 13114 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13115 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13116 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13117 | `			return rc;` |
|         - | 13118 | `		}` |
|        27 | 13119 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13120 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13121 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13122 | `		return SXRET_OK;` |
|         - | 13123 | `	}` |
|  23501403 | 13124 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13125 | `	/* Generate code for the left tree */` |
|  23501403 | 13126 | `	if( pNode->pLeft ){` |
|  23477761 | 13127 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  23477761 | 13128 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13129 | `			ph7_expr_node **apNode;` |
|   5592627 | 13130 | `			int hasSpread = 0;` |
|   5592627 | 13131 | `			int hasNamed = 0;` |
|   5592627 | 13132 | `			int bAnySpread = 0;` |
|   5592627 | 13133 | `			sxu32 byRefMask = 0;` |
|         - | 13134 | `			sxi32 nArgs;` |
|         - | 13135 | `			sxi32 n;` |
|         - | 13136 | `			/* Recurse and generate bytecodes for function arguments */` |
|   5592627 | 13137 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5592627 | 13138 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13139 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13140 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13141 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   5592627 | 13142 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13143 | `				bFcc = 1;` |
|        81 | 13144 | `				nArgs = 0;` |
|        40 | 13145 | `			}` |
|         - | 13146 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13147 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13148 | `			{` |
|   5592627 | 13149 | `				int seenNamed = 0;` |
|   5592627 | 13150 | `				int seenSpread = 0;` |
|  11419967 | 13151 | `				for( n = 0; n < nArgs; ++n ){` |
|   5827347 | 13152 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4129 | 13153 | `						bAnySpread = 1;` |
|      4129 | 13154 | `						seenSpread = 1;` |
|      4129 | 13155 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13156 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13157 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13158 | `							return SXERR_SYNTAX;` |
|         5 | 13159 | `						}` |
|   5825285 | 13160 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13161 | `						seenNamed = 1;` |
|       289 | 13162 | `						hasNamed = 1;` |
|   5823081 | 13163 | `					}else if( seenNamed ){` |
|         3 | 13164 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13165 | `							"Cannot use positional argument after named argument");` |
|         3 | 13166 | `						return SXERR_SYNTAX;` |
|   5822937 | 13167 | `					}else if( seenSpread ){` |
|       ! 0 | 13168 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13169 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13170 | `						return SXERR_SYNTAX;` |
|         - | 13171 | `					}` |
|   2913675 | 13172 | `				}` |
|         - | 13173 | `			}` |
|         - | 13174 | `			/* Read-only load */` |
|   5592625 | 13175 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13176 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13177 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13178 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13179 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   5592625 | 13180 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   5592625 | 13181 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   5592620 | 13182 | `				if( pCallName->nByte == 5` |
|   3144367 | 13183 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    284065 | 13184 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5450595 | 13185 | `				}else if( pCallName->nByte == 5` |
|   2860307 | 13186 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       107 | 13187 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        51 | 13188 | `				}` |
|         - | 13189 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13190 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13191 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13192 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13193 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13194 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   5592625 | 13195 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13196 | `					SyString sBuiltin;` |
|   5588371 | 13197 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   5588371 | 13198 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   2794183 | 13199 | `				}` |
|   2796310 | 13200 | `			}` |
|  11419963 | 13201 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   5827343 | 13202 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   5827343 | 13203 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13204 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13205 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13206 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13207 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13208 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13209 | `				 * (iP1=0 either way). */` |
|   5827343 | 13210 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     27653 | 13211 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     27653 | 13212 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     13824 | 13213 | `				}` |
|   5827343 | 13214 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   5827343 | 13215 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13216 | `					return rc;` |
|         - | 13217 | `				}` |
|         - | 13218 | `				/* Each argument is an independent nullsafe scope. */` |
|   5827343 | 13219 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   5827343 | 13220 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13221 | `					/* Emit spread opcode to unpack this array argument */` |
|      4129 | 13222 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4129 | 13223 | `					hasSpread = 1;` |
|      2062 | 13224 | `				}` |
|   2913674 | 13225 | `			}` |
|         - | 13226 | `			/* Total number of given arguments */` |
|   5592625 | 13227 | `			iP1 = nArgs;` |
|   5592625 | 13228 | `			iP2 = hasSpread;` |
|         - | 13229 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13230 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   5592625 | 13231 | `			if( hasNamed ){` |
|       178 | 13232 | `				sxu32 nStrBytes = 0;` |
|         - | 13233 | `				char *zBuf;` |
|       534 | 13234 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13235 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13236 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13237 | `					}` |
|       182 | 13238 | `				}` |
|         - | 13239 | `				{` |
|       178 | 13240 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13241 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13242 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13243 | `				if( pMap ){` |
|       178 | 13244 | `					SyZero(pMap, mapSize);` |
|       178 | 13245 | `					pMap->bHasNamed = 1;` |
|       178 | 13246 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13247 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13248 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13249 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13250 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13251 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13252 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13253 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13254 | `							zBuf += nb;` |
|       141 | 13255 | `						}` |
|         - | 13256 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13257 | `					}` |
|       178 | 13258 | `					p3 = (void *)pMap;` |
|        87 | 13259 | `				}` |
|         - | 13260 | `				}` |
|        87 | 13261 | `			}` |
|         - | 13262 | `			/* Remove stale flags now */` |
|   5592625 | 13263 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   2796310 | 13264 | `		}` |
|         - | 13265 | `		{` |
|         - | 13266 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13267 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13268 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13269 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13270 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13271 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13272 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13273 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  23477759 | 13274 | `			sxi32 iLeftFlags = iFlags;` |
|  23477754 | 13275 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  19316020 | 13276 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   7577169 | 13277 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   6428543 | 13278 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2475053 | 13279 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1237524 | 13280 | `			}` |
|         - | 13281 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13282 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13283 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13284 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13285 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13286 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13287 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  23477754 | 13288 | `			if( pNode->pOp` |
|  32881102 | 13289 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  21142272 | 13290 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  18806738 | 13291 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   5042315 | 13292 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2521155 | 13293 | `			}` |
|         - | 13294 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13295 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13296 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13297 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13298 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13299 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  23477754 | 13300 | `			if( pNode->pOp` |
|  23477759 | 13301 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    150217 | 13302 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     75106 | 13303 | `			}` |
|  23477759 | 13304 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13305 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13306 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|       211 | 13307 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|       103 | 13308 | `			}` |
|  23477759 | 13309 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13310 | `		}` |
|  23477759 | 13311 | `		if( rc != SXRET_OK ){` |
|        34 | 13312 | `			return rc;` |
|         - | 13313 | `		}` |
|  23477729 | 13314 | `		if( !bIsChainOp ){` |
|         - | 13315 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13316 | `			 * target the end of that LHS chain, which is right here. */` |
|  10251109 | 13317 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   5125552 | 13318 | `		}` |
|  23477729 | 13319 | `		if( iVmOp == PH7_OP_CALL ){` |
|   5592625 | 13320 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5592625 | 13321 | `			if( pInstr ){` |
|   5592625 | 13322 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   3941071 | 13323 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13324 | `					sxu32 nQual;` |
|   3941071 | 13325 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13326 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13327 | `					 * so the later NEW handler (if any) can see it. */` |
|   3941071 | 13328 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13329 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13330 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13331 | `					 * imports — class imports must NOT affect function` |
|         - | 13332 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13333 | `					 * before NEW; we store the original literal index in the` |
|         - | 13334 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13335 | `					 * the unqualified name and re-qualify with class imports. */` |
|   3941071 | 13336 | `					if( bAbsolute ){` |
|      3973 | 13337 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1989 | 13338 | `					}else{` |
|   3937103 | 13339 | `						int fromImport = 0;` |
|   3937103 | 13340 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   3937103 | 13341 | `						pInstr->iP2 = (sxi32)nQual;` |
|   3937103 | 13342 | `						if( nQual != nOrig ){` |
|         - | 13343 | `							/* Record the original literal index in the arg map` |
|         - | 13344 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13345 | `							 * flag) so the NEW handler can recover the` |
|         - | 13346 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13347 | `							 * imports. */` |
|        77 | 13348 | `							if( p3 == 0 ){` |
|        77 | 13349 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        72 | 13350 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        77 | 13351 | `								if( pMap ){` |
|        77 | 13352 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        77 | 13353 | `									p3 = (void *)pMap;` |
|        36 | 13354 | `								}` |
|        36 | 13355 | `							}` |
|        77 | 13356 | `							if( p3 ){` |
|        77 | 13357 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        77 | 13358 | `								if( !fromImport ){` |
|         - | 13359 | `									/* Mark as namespace-qualified */` |
|        67 | 13360 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        31 | 13361 | `								}` |
|        36 | 13362 | `							}` |
|        36 | 13363 | `						}` |
|         5 | 13364 | `					}` |
|   3622092 | 13365 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13366 | `					/* Method call,flag that */` |
|   1631247 | 13367 | `					pInstr->iP2 = 1;` |
|    815621 | 13368 | `				}` |
|   2796315 | 13369 | `			}` |
|  20681419 | 13370 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13371 | `			ph7_expr_node **apNode;` |
|         - | 13372 | `			sxi32 n;` |
|   2591695 | 13373 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13374 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13375 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13376 | `			/* Recurse and generate bytecodes for array index */` |
|   2591695 | 13377 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5021629 | 13378 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2429939 | 13379 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2429939 | 13380 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2429939 | 13381 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13382 | `					return rc;` |
|         - | 13383 | `				}` |
|         - | 13384 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2429939 | 13385 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1214972 | 13386 | `			}` |
|   2591695 | 13387 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2429939 | 13388 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1214967 | 13389 | `			}` |
|   2591695 | 13390 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13391 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    323349 | 13392 | `				iP2 = 4;` |
|   2430023 | 13393 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13394 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13395 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23721 | 13396 | `				iP2 = 5;` |
|   2256493 | 13397 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13398 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13399 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13400 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 13401 | `				iP2 = 6;` |
|   2244623 | 13402 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13403 | `				/* Create an empty entry when the desired index is not found */` |
|    378981 | 13404 | `				iP2 = 1;` |
|    189493 | 13405 | `			}` |
|  16589264 | 13406 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13407 | `			/* POP the left node */` |
|         5 | 13408 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13409 | `		}` |
|  11738862 | 13410 | `	}` |
|  23501371 | 13411 | `	rc = SXRET_OK;` |
|  23501371 | 13412 | `	nJmpIdx = 0;` |
|         - | 13413 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13414 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13415 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  23501371 | 13416 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    394889 | 13417 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    394889 | 13418 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    394889 | 13419 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    394889 | 13420 | `			int isSpecial = 0;` |
|    394889 | 13421 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    347577 | 13422 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    347577 | 13423 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    347572 | 13424 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    315992 | 13425 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    171788 | 13426 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    106529 | 13427 | `					isSpecial = 1;` |
|     53262 | 13428 | `				}` |
|    185614 | 13429 | `			}` |
|    418545 | 13430 | `			pInstr->iP1 = 0;` |
|    418545 | 13431 | `			if( !isSpecial ){` |
|    264709 | 13432 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    132352 | 13433 | `			}` |
|         - | 13434 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13435 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    371233 | 13436 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    264709 | 13437 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    264709 | 13438 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        60 | 13439 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        62 | 13440 | `					return SXRET_OK;` |
|         - | 13441 | `				}` |
|    132323 | 13442 | `			}` |
|    185585 | 13443 | `		}` |
|    232876 | 13444 | `	}` |
|         - | 13445 | `	/* Generate code for the right tree */` |
|  23477671 | 13446 | `	if( pNode->pRight ){` |
|  13495351 | 13447 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13448 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    323601 | 13449 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  13333553 | 13450 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13451 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    220815 | 13452 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  13061350 | 13453 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13454 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     55315 | 13455 | `			iVmOp = 0; /* No binary operator to emit */` |
|     55315 | 13456 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  12923342 | 13457 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13458 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13459 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13460 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13461 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13462 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13463 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13464 | `			sxu32 nNsJmp = 0;` |
|       108 | 13465 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13466 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  12895583 | 13467 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13468 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13469 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13470 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4097413 | 13471 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2048704 | 13472 | `		}` |
|  13495351 | 13473 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  13495351 | 13474 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  13495351 | 13475 | `		if( !bIsChainOp ){` |
|         - | 13476 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13477 | `			 * operator instruction is emitted. */` |
|   8453099 | 13478 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   4226547 | 13479 | `		}` |
|  13495351 | 13480 | `		if( iVmOp == PH7_OP_STORE ){` |
|   3671567 | 13481 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   3671530 | 13482 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13483 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13484 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13485 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13486 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13487 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13488 | `				 */` |
|        91 | 13489 | `				iVmOp = 0;` |
|   3671524 | 13490 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   3671481 | 13491 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13492 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    792651 | 13493 | `					iP2 = 1;` |
|    396328 | 13494 | `				}else{` |
|   2878835 | 13495 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13496 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    359183 | 13497 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    359183 | 13498 | `						iP1 = pInstr->iP1;` |
|    179594 | 13499 | `					}else{` |
|   2519657 | 13500 | `						p3 = pInstr->p3;` |
|         - | 13501 | `					}` |
|         - | 13502 | `					/* POP the last dynamic load instruction */` |
|   2878835 | 13503 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13504 | `				}` |
|   1835743 | 13505 | `			}` |
|  11659570 | 13506 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        62 | 13507 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        62 | 13508 | `			if( pInstr ){` |
|        62 | 13509 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13510 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13511 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13512 | `					 */` |
|        19 | 13513 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13514 | `					iP1 = pInstr->iP1;` |
|        19 | 13515 | `					iP2 = pInstr->iP2;` |
|        19 | 13516 | `					p3  = pInstr->p3;` |
|        10 | 13517 | `				}else{` |
|        44 | 13518 | `					p3 = pInstr->p3;` |
|         - | 13519 | `				}` |
|        30 | 13520 | `			}` |
|        30 | 13521 | `		}` |
|   6747673 | 13522 | `	}` |
|  23477666 | 13523 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    368031 | 13524 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13525 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13526 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13527 | `		iVmOp = 0;` |
|        14 | 13528 | `	}` |
|  23477671 | 13529 | `	if( iVmOp > 0 ){` |
|  23422243 | 13530 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    150217 | 13531 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13532 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     11857 | 13533 | `				iP1 = 1;` |
|      5931 | 13534 | `			}` |
|  23347137 | 13535 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13536 | `			/* Namespace-qualify the class name for NEW */ {` |
|    735705 | 13537 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    735705 | 13538 | `				VmInstr *pCallInstr = 0;` |
|    735705 | 13539 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    735409 | 13540 | `					pCallInstr = pPeek;` |
|    735409 | 13541 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    367702 | 13542 | `				}` |
|    735705 | 13543 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    719941 | 13544 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13545 | `					sxu32 nLitForClass;` |
|    719941 | 13546 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13547 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13548 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13549 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13550 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13551 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13552 | `					 * with class imports. */` |
|    719941 | 13553 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        37 | 13554 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        21 | 13555 | `					}else{` |
|    719909 | 13556 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13557 | `					}` |
|    719941 | 13558 | `					pPeek->iP1 = 0;` |
|    719941 | 13559 | `					if( !bAbsolute ){` |
|    715977 | 13560 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    357991 | 13561 | `					}else{` |
|      3969 | 13562 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13563 | `					}` |
|    359968 | 13564 | `				}` |
|         - | 13565 | `			}` |
|    735705 | 13566 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    735705 | 13567 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13568 | `				VmInstr *pPrev;` |
|    735409 | 13569 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    735409 | 13570 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13571 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13572 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13573 | `					 * accumulator exactly like OP_CALL would have). */` |
|    735409 | 13574 | `					iP1 = pInstr->iP1;` |
|    735409 | 13575 | `					iP2 = pInstr->iP2;` |
|    735409 | 13576 | `					if( pInstr->p3 ){` |
|        47 | 13577 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        21 | 13578 | `					}` |
|    735409 | 13579 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    367702 | 13580 | `				}` |
|    367707 | 13581 | `			}` |
|  22904181 | 13582 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13583 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13584 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     71177 | 13585 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     71177 | 13586 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     71177 | 13587 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     71177 | 13588 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     71177 | 13589 | `				int isSpecialIs = 0;` |
|     71177 | 13590 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     71177 | 13591 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     71177 | 13592 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     71172 | 13593 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     71175 | 13594 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     35586 | 13595 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13596 | `						isSpecialIs = 1;` |
|         5 | 13597 | `					}` |
|     35586 | 13598 | `				}` |
|     71177 | 13599 | `				pInstr->iP1 = 0;` |
|     71177 | 13600 | `				if( !isSpecialIs && !bAbsolute ){` |
|     71157 | 13601 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     35576 | 13602 | `				}` |
|     35591 | 13603 | `			}` |
|  22500745 | 13604 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13605 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13606 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13607 | `			 * should not trigger constant lookup. */` |
|   5042257 | 13608 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5042257 | 13609 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4801839 | 13610 | `				pInstr->iP1 = 0;` |
|   2400917 | 13611 | `			}` |
|   5042257 | 13612 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13613 | `				/* Static member access,remember that */` |
|    371189 | 13614 | `				iP1 = 1;` |
|    371189 | 13615 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    371189 | 13616 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    236467 | 13617 | `					p3 = pInstr->p3;` |
|    236467 | 13618 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    118231 | 13619 | `				}` |
|    185592 | 13620 | `			}` |
|         - | 13621 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13622 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13623 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13624 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   5042257 | 13625 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   5042257 | 13626 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13627 | `					iP2 = PH7_MEMBER_UNSET;` |
|   5042237 | 13628 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     63143 | 13629 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5010648 | 13630 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13631 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4979071 | 13632 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13633 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    966195 | 13634 | `					iP2 = PH7_MEMBER_WRITE;` |
|    483095 | 13635 | `				}` |
|   2521126 | 13636 | `			}` |
|   2521126 | 13637 | `		}` |
|         - | 13638 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13639 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13640 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13641 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13642 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  23422243 | 13643 | `		if( bFcc ){` |
|        81 | 13644 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13645 | `			iP2 = 0;` |
|        81 | 13646 | `			p3 = 0;` |
|        81 | 13647 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13648 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13649 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13650 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13651 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13652 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13653 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13654 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13655 | `				if( pMemberName ){` |
|         3 | 13656 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13657 | `				}` |
|        37 | 13658 | `				iP1 = 2;` |
|        19 | 13659 | `			}else{` |
|        45 | 13660 | `				iP1 = 1;` |
|         - | 13661 | `			}` |
|        40 | 13662 | `		}` |
|         - | 13663 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13664 | `		 * This is the primary emit path for user-visible calls. */` |
|  23422243 | 13665 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6328245 | 13666 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3164120 | 13667 | `		}` |
|         - | 13668 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  23422243 | 13669 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  11711119 | 13670 | `	}` |
|  23477671 | 13671 | `	if( nJmpIdx > 0 ){` |
|         - | 13672 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    599721 | 13673 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    599721 | 13674 | `		if( pInstr ){` |
|    599721 | 13675 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    299858 | 13676 | `		}` |
|    299858 | 13677 | `	}` |
|  23477671 | 13678 | `	return rc;` |
|  29701895 | 13679 | `}` |
|         - | 13680 | `/*` |
|         - | 13681 | ` * Compile a PHP expression.` |
|         - | 13682 | ` * According to the PHP language reference manual:` |
|         - | 13683 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 13684 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 13685 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 13686 | ` *  is "anything that has a value".` |
|         - | 13687 | ` * If something goes wrong while compiling the expression,this` |
|         - | 13688 | ` * function takes care of generating the appropriate error` |
|         - | 13689 | ` * message.` |
|         - | 13690 | ` */` |
|         - | 13691 | `/*` |
|         - | 13692 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 13693 | ` *` |
|         - | 13694 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 13695 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 13696 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 13697 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 13698 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 13699 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 13700 | ` * except for() now reports php's parse error.` |
|         - | 13701 | ` */` |
| 197035362 | 13702 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 13703 | `{` |
|         - | 13704 | `	ph7_expr_node **apArg;` |
|         - | 13705 | `	sxu32 n;` |
| 197035367 | 13706 | `	if( pNode == 0 ){` |
| 138369363 | 13707 | `		return 0;` |
|         - | 13708 | `	}` |
|  58666009 | 13709 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 13710 | `		return 1;` |
|         - | 13711 | `	}` |
|  58666000 | 13712 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  58666001 | 13713 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 13714 | `		return 1;` |
|         - | 13715 | `	}` |
|  58666001 | 13716 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  66907593 | 13717 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   8241597 | 13718 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 13719 | `			return 1;` |
|         - | 13720 | `		}` |
|   4120801 | 13721 | `	}` |
|  58666001 | 13722 | `	return 0;` |
|  98517686 | 13723 | `}` |
|  13056434 | 13724 | `static sxi32 PH7_CompileExpr(` |
|         - | 13725 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 13726 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 13727 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 13728 | `	)` |
|         5 | 13729 | `{` |
|         - | 13730 | `	ph7_expr_node *pRoot;` |
|         - | 13731 | `	SySet sExprNode;` |
|         - | 13732 | `	SyToken *pEnd;` |
|         - | 13733 | `	sxi32 nExpr;` |
|         - | 13734 | `	sxi32 iNest;` |
|         - | 13735 | `	sxi32 rc;` |
|         - | 13736 | `	sxu32 nNullsafeBase;` |
|         - | 13737 | `	/* Initialize worker variables */` |
|  13056439 | 13738 | `	nExpr = 0;` |
|  13056439 | 13739 | `	pRoot = 0;` |
|         - | 13740 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 13741 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  13056439 | 13742 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  13056439 | 13743 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  13056439 | 13744 | `	SySetAlloc(&sExprNode,0x10);` |
|  13056439 | 13745 | `	rc = SXRET_OK;` |
|         - | 13746 | `	/* Delimit the expression */` |
|  13056439 | 13747 | `	pEnd = pGen->pIn;` |
|  13056439 | 13748 | `	iNest = 0;` |
| 103697545 | 13749 | `	while( pEnd < pGen->pEnd ){` |
|  98916313 | 13750 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 13751 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4659 | 13752 | `			iNest++;` |
|  98913986 | 13753 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4667 | 13754 | `			iNest--;` |
|  98909328 | 13755 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   8275823 | 13756 | `			if( iNest <= 0 ){` |
|   8275207 | 13757 | `				break;` |
|         - | 13758 | `			}` |
|       308 | 13759 | `		}` |
|  90641111 | 13760 | `		pEnd++;` |
|         5 | 13761 | `	}` |
|  13056439 | 13762 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    658989 | 13763 | `		SyToken *pEnd2 = pGen->pIn;` |
|    658989 | 13764 | `		iNest = 0;` |
|         - | 13765 | `		/* Stop at the first comma */` |
|   1436901 | 13766 | `		while( pEnd2 < pEnd ){` |
|    777919 | 13767 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     43449 | 13768 | `				iNest++;` |
|    756197 | 13769 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     43449 | 13770 | `				iNest--;` |
|    712753 | 13771 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|        63 | 13772 | `				if( iNest <= 0 ){` |
|         3 | 13773 | `					break;` |
|         - | 13774 | `				}` |
|        28 | 13775 | `			}` |
|    777917 | 13776 | `			pEnd2++;` |
|         5 | 13777 | `		}` |
|    658989 | 13778 | `		if( pEnd2 <pEnd ){` |
|         3 | 13779 | `			pEnd = pEnd2;` |
|         1 | 13780 | `		}` |
|    329492 | 13781 | `	}` |
|  13056439 | 13782 | `	if( pEnd > pGen->pIn ){` |
|  13032789 | 13783 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 13784 | `		/* Swap delimiter */` |
|  13032789 | 13785 | `		pGen->pEnd = pEnd;` |
|         - | 13786 | `		/* Try to get an expression tree */` |
|  13032789 | 13787 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  13032784 | 13788 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  12914195 | 13789 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 13790 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 13791 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 13792 | `				"syntax error, unexpected token \",\"");` |
|         6 | 13793 | `			pGen->pEnd = pTmp;` |
|         6 | 13794 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13795 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 13796 | `				return SXERR_ABORT;` |
|         - | 13797 | `			}` |
|         6 | 13798 | `			pGen->pIn = pEnd;` |
|         6 | 13799 | `			SySetRelease(&sExprNode);` |
|         6 | 13800 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 13801 | `			return SXRET_OK;` |
|         - | 13802 | `		}` |
|  13032785 | 13803 | `		if( rc == SXRET_OK && pRoot ){` |
|  13032603 | 13804 | `			rc = SXRET_OK;` |
|  13032603 | 13805 | `			if( xTreeValidator ){` |
|         - | 13806 | `				/* Call the upper layer validator callback */` |
|    855961 | 13807 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    427978 | 13808 | `			}` |
|  13032603 | 13809 | `			if( rc != SXERR_ABORT ){` |
|         - | 13810 | `				/* Generate code for the given tree */` |
|  13032603 | 13811 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 13812 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 13813 | `				 * expression so they short-circuit to its end. */` |
|  13032603 | 13814 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   6516299 | 13815 | `			}` |
|  13032603 | 13816 | `			nExpr = 1;` |
|   6516299 | 13817 | `		}` |
|         - | 13818 | `		/* Release the whole tree */` |
|  13032785 | 13819 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 13820 | `		/* Synchronize token stream */` |
|  13032785 | 13821 | `		pGen->pEnd = pTmp;` |
|  13032785 | 13822 | `		pGen->pIn  = pEnd;` |
|  13032785 | 13823 | `		if( rc == SXERR_ABORT ){` |
|        13 | 13824 | `			SySetRelease(&sExprNode);` |
|        13 | 13825 | `			return SXERR_ABORT;` |
|         - | 13826 | `		}` |
|   6516385 | 13827 | `	}` |
|  13056425 | 13828 | `	SySetRelease(&sExprNode);` |
|  13056425 | 13829 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   6528222 | 13830 | `}` |
|         - | 13831 | `/*` |
|         - | 13832 | ` * Return a pointer to the node construct handler associated` |
|         - | 13833 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 13834 | ` */` |
|   7325192 | 13835 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 13836 | `{` |
|   7325197 | 13837 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 13838 | `		/* Numeric literal: Either real or integer */` |
|   2797637 | 13839 | `		return PH7_CompileNumLiteral;` |
|   4527565 | 13840 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 13841 | `		/* Double quoted string */` |
|     82081 | 13842 | `		return PH7_CompileString;` |
|   4445489 | 13843 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 13844 | `		/* Single quoted string */` |
|   4445369 | 13845 | `		return PH7_CompileSimpleString;` |
|       125 | 13846 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 13847 | `		/* Heredoc */` |
|        70 | 13848 | `		return PH7_CompileHereDoc;` |
|        59 | 13849 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 13850 | `		/* Nowdoc */` |
|        51 | 13851 | `		return PH7_CompileNowDoc;` |
|         9 | 13852 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 13853 | `		/* Backtick quoted string */` |
|         6 | 13854 | `		return PH7_CompileBacktic;` |
|         - | 13855 | `	}` |
|         3 | 13856 | `	return 0;` |
|   3662601 | 13857 | `}` |
|         - | 13858 | `/*` |
|         - | 13859 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 13860 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 13861 | ` * in write context" parse error.` |
|         - | 13862 | ` */` |
|     30850 | 13863 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 13864 | `{` |
|         - | 13865 | `	sxi32 rc;` |
|     30855 | 13866 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     30853 | 13867 | `		return SXRET_OK;` |
|         - | 13868 | `	}` |
|         5 | 13869 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 13870 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 13871 | `		"Can't use nullsafe operator in write context");` |
|         3 | 13872 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     15430 | 13873 | `}` |
|         - | 13874 | `/*` |
|         - | 13875 | ` * Compile an unset() statement.` |
|         - | 13876 | ` * unset($var, $arr[$key], ...);` |
|         - | 13877 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 13878 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 13879 | ` * parent array before extracting the element to unset.` |
|         - | 13880 | ` */` |
|     26624 | 13881 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 13882 | `{` |
|     26629 | 13883 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26629 | 13884 | `	sxu32 nIdx = 0;` |
|         - | 13885 | `	SyString sName;` |
|         - | 13886 | `	sxi32 rc;` |
|         - | 13887 | `	/* Jump the 'unset' keyword */` |
|     26629 | 13888 | `	pGen->pIn++;` |
|         - | 13889 | `	/* Save delimiter */` |
|     26629 | 13890 | `	pTmp = pGen->pEnd;` |
|         - | 13891 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26629 | 13892 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26629 | 13893 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 13894 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 13895 | `		SyToken *pClose;` |
|     26629 | 13896 | `		pGen->pIn++;   /* Skip '(' */` |
|     26629 | 13897 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26629 | 13898 | `		pEnd = pClose; /* Stop at ')' */` |
|     13312 | 13899 | `	}` |
|     26629 | 13900 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 13901 | `	/* Resolve the 'unset' builtin name once */` |
|     26629 | 13902 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3945 | 13903 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3945 | 13904 | `		if( pObj == 0 ){` |
|       ! 0 | 13905 | `			return SXERR_ABORT;` |
|         - | 13906 | `		}` |
|      3945 | 13907 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3945 | 13908 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1970 | 13909 | `	}` |
|         - | 13910 | `	/* Compile each comma-separated argument */` |
|     57481 | 13911 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30857 | 13912 | `		if( pGen->pIn < pNext ){` |
|     30857 | 13913 | `			pGen->pEnd = pNext;` |
|     30857 | 13914 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 13915 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 13916 | `				GenStateUnsetValidator);` |
|     30857 | 13917 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13918 | `				return SXERR_ABORT;` |
|         - | 13919 | `			}` |
|     30857 | 13920 | `			if( rc != SXERR_EMPTY ){` |
|         - | 13921 | `				/* Emit call for this single argument */` |
|     30855 | 13922 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     30855 | 13923 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     30855 | 13924 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     15425 | 13925 | `			}` |
|     15426 | 13926 | `		}` |
|         - | 13927 | `		/* Jump trailing commas */` |
|     35087 | 13928 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|      4235 | 13929 | `			pNext++;` |
|         5 | 13930 | `		}` |
|     30857 | 13931 | `		pGen->pIn = pNext;` |
|         5 | 13932 | `	}` |
|         - | 13933 | `	/* Skip past the closing ')' if present */` |
|     26629 | 13934 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26629 | 13935 | `		pGen->pIn++;` |
|     13312 | 13936 | `	}` |
|         - | 13937 | `	/* Restore token stream */` |
|     26629 | 13938 | `	pGen->pEnd = pTmp;` |
|     26629 | 13939 | `	return SXRET_OK;` |
|     13317 | 13940 | `}` |
|         - | 13941 | `/*` |
|         - | 13942 | ` * PHP Language construct table.` |
|         - | 13943 | ` */` |
|         - | 13944 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 13945 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 13946 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 13947 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 13948 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 13949 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 13950 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 13951 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 13952 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 13953 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 13954 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 13955 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 13956 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 13957 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 13958 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 13959 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 13960 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 13961 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 13962 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 13963 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 13964 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 13965 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 13966 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 13967 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 13968 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 13969 | `};` |
|         - | 13970 | `/*` |
|         - | 13971 | ` * Return a pointer to the statement handler routine associated` |
|         - | 13972 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 13973 | ` */` |
|   6482576 | 13974 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 13975 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 13976 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 13977 | `	)` |
|         5 | 13978 | `{` |
|   6482581 | 13979 | `	sxu32 n = 0;` |
|  26776545 | 13980 | `	for(;;){` |
|  53553095 | 13981 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    443593 | 13982 | `			break;` |
|         - | 13983 | `		}` |
|  53109507 | 13984 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6038993 | 13985 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 13986 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 13987 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 13988 | `					/* 'static' (class context),return null */` |
|       ! 0 | 13989 | `					return 0;` |
|         - | 13990 | `				}` |
|       ! 0 | 13991 | `			}` |
|   6038988 | 13992 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|        14 | 13993 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|        14 | 13994 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 13995 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 13996 | `				return 0;` |
|         - | 13997 | `			}` |
|         - | 13998 | `			/* Return a pointer to the handler.` |
|         - | 13999 | `			*/` |
|   6038991 | 14000 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14001 | `		}` |
|  47070519 | 14002 | `		n++;` |
|         5 | 14003 | `	}` |
|    443593 | 14004 | `	if( pLookahed ){` |
|    443593 | 14005 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     71029 | 14006 | `			return PH7_CompileClassInterface;` |
|    372569 | 14007 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    324723 | 14008 | `			return PH7_CompileClass;` |
|     47851 | 14009 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7963 | 14010 | `			return PH7_CompileTrait;` |
|         - | 14011 | `		}` |
|         - | 14012 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14013 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14014 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14015 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19944 | 14016 | `	}` |
|         - | 14017 | `	/* Not a language construct */` |
|     39893 | 14018 | `	return 0;` |
|   3241293 | 14019 | `}` |
|         - | 14020 | `/*` |
|         - | 14021 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14022 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14023 | ` */` |
|     39890 | 14024 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14025 | `{` |
|         - | 14026 | `	int rc;` |
|     39895 | 14027 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     39895 | 14028 | `	if( rc == FALSE ){` |
|     39774 | 14029 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     16126 | 14030 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14031 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14032 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14033 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14034 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14035 | `			*/` |
|         - | 14036 | `			){` |
|     39771 | 14037 | `				rc = TRUE;` |
|     19883 | 14038 | `		}` |
|     19887 | 14039 | `	}` |
|     39895 | 14040 | `	return rc;` |
|         5 | 14041 | `}` |
|         - | 14042 | `/*` |
|         - | 14043 | ` * Compile a PHP chunk.` |
|         - | 14044 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14045 | ` * takes care of generating the appropriate error message.` |
|         - | 14046 | ` */` |
|         - | 14047 | `/*` |
|         - | 14048 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14049 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14050 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14051 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14052 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14053 | ` * intervening non-declaration statements.` |
|         - | 14054 | ` */` |
|  14363246 | 14055 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14056 | `{` |
|  14363251 | 14057 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  14363251 | 14058 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  14363251 | 14059 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14060 | `	sxu32 nIdx, n;` |
|  14363246 | 14061 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   1559209 | 14062 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14063 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14064 | `		 * indexes do not map to the sidecar */` |
|  12804049 | 14065 | `		return;` |
|         - | 14066 | `	}` |
|   1559207 | 14067 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14068 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14069 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   1559207 | 14070 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4679105 | 14071 | `	for( n = 0 ; n < nT ; n++ ){` |
|   3119903 | 14072 | `		if( aT[n].nTokIdx != nIdx ){` |
|   3111859 | 14073 | `			continue;` |
|         - | 14074 | `		}` |
|      8049 | 14075 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14076 | `			pGen->sPendingDoc = aT[n].sText;` |
|      8037 | 14077 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      8025 | 14078 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      4010 | 14079 | `		}` |
|      4027 | 14080 | `	}` |
|   7181628 | 14081 | `}` |
|         - | 14082 | `/*` |
|         - | 14083 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14084 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14085 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14086 | ` */` |
|   3990012 | 14087 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14088 | `{` |
|         - | 14089 | `	char *zDup;` |
|   3990017 | 14090 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   3989997 | 14091 | `		return;` |
|         - | 14092 | `	}` |
|        35 | 14093 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14094 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14095 | `	if( zDup ){` |
|        25 | 14096 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14097 | `	}` |
|        25 | 14098 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   1995011 | 14099 | `}` |
|         - | 14100 | `/*` |
|         - | 14101 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14102 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14103 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14104 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14105 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14106 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14107 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14108 | ` */` |
|      8032 | 14109 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14110 | `{` |
|         - | 14111 | `	SySet *pToken;` |
|         - | 14112 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14113 | `	char *zSpan;` |
|      8037 | 14114 | `	sxi32 rc = SXRET_OK;` |
|      8037 | 14115 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14116 | `		return SXRET_OK;` |
|         - | 14117 | `	}` |
|     12053 | 14118 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      4016 | 14119 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      8037 | 14120 | `	if( zSpan == 0 ){` |
|       ! 0 | 14121 | `		return SXRET_OK;` |
|         - | 14122 | `	}` |
|         - | 14123 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14124 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14125 | `	 * the number of attribute declarations in the program. */` |
|      8037 | 14126 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      8037 | 14127 | `	if( pToken == 0 ){` |
|       ! 0 | 14128 | `		return SXRET_OK;` |
|         - | 14129 | `	}` |
|      8037 | 14130 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      8037 | 14131 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      8037 | 14132 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      8037 | 14133 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      8037 | 14134 | `	pSavedIn = pGen->pIn;` |
|      8037 | 14135 | `	pSavedEnd = pGen->pEnd;` |
|      8041 | 14136 | `	while( pIn < pEnd ){` |
|         - | 14137 | `		ph7_attribute sAttr;` |
|         - | 14138 | `		SyBlob sFQN;` |
|      8041 | 14139 | `		int bAbsolute = 0;` |
|      8041 | 14140 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      8041 | 14141 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      8041 | 14142 | `		sAttr.nLine = pIn->nLine;` |
|      8041 | 14143 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14144 | `			bAbsolute = 1;` |
|        75 | 14145 | `			pIn++;` |
|        35 | 14146 | `		}` |
|      8041 | 14147 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      8041 | 14148 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      8041 | 14149 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      8041 | 14150 | `			pIn++;` |
|      8041 | 14151 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14152 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14153 | `				pIn++;` |
|       ! 0 | 14154 | `				continue;` |
|         - | 14155 | `			}` |
|      8041 | 14156 | `			break;` |
|       ! 0 | 14157 | `		}` |
|      8041 | 14158 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14159 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14160 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14161 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14162 | `			break;` |
|         - | 14163 | `		}` |
|         - | 14164 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14165 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14166 | `		{` |
|      8041 | 14167 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      8041 | 14168 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      8041 | 14169 | `			char *zDup = 0;` |
|      8041 | 14170 | `			if( !bAbsolute ){` |
|      7971 | 14171 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7971 | 14172 | `				if( pImp ){` |
|       ! 0 | 14173 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14174 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14175 | `					if( zDup ){` |
|       ! 0 | 14176 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14177 | `					}` |
|      7971 | 14178 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14179 | `					SyBlob sTmp;` |
|       ! 0 | 14180 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14181 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14182 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14183 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14184 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14185 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14186 | `					if( zDup ){` |
|       ! 0 | 14187 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14188 | `					}` |
|       ! 0 | 14189 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14190 | `				}` |
|      3983 | 14191 | `			}` |
|      8041 | 14192 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      8041 | 14193 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      8041 | 14194 | `				if( zDup ){` |
|      8041 | 14195 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      4018 | 14196 | `				}` |
|      4018 | 14197 | `			}` |
|         - | 14198 | `		}` |
|      8041 | 14199 | `		SyBlobRelease(&sFQN);` |
|      8041 | 14200 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14201 | `			SyToken *pArgsEnd;` |
|      7939 | 14202 | `			pIn++;` |
|      7939 | 14203 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15887 | 14204 | `			while( pIn < pArgsEnd ){` |
|      7953 | 14205 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7953 | 14206 | `				sxi32 iDepth = 0;` |
|         - | 14207 | `				ph7_attr_arg sArgRec;` |
|     79045 | 14208 | `				while( pArgStop < pArgsEnd ){` |
|     71113 | 14209 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14210 | `						iDepth++;` |
|     71108 | 14211 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14212 | `						iDepth--;` |
|     71098 | 14213 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14214 | `						break;` |
|         - | 14215 | `					}` |
|     71097 | 14216 | `					pArgStop++;` |
|         5 | 14217 | `				}` |
|      7953 | 14218 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7953 | 14219 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7948 | 14220 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7932 | 14221 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14222 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14223 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14224 | `					if( zN ){` |
|        19 | 14225 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14226 | `					}` |
|        19 | 14227 | `					pArgStart += 2;` |
|         9 | 14228 | `				}` |
|      7953 | 14229 | `				if( pArgStart < pArgStop ){` |
|         - | 14230 | `					SySet *pInstrContainer;` |
|      7953 | 14231 | `					pGen->pIn = pArgStart;` |
|      7953 | 14232 | `					pGen->pEnd = pArgStop;` |
|      7953 | 14233 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7953 | 14234 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7953 | 14235 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7953 | 14236 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7953 | 14237 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7953 | 14238 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14239 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14240 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14241 | `						return SXERR_ABORT;` |
|         - | 14242 | `					}` |
|      7953 | 14243 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3974 | 14244 | `				}` |
|      7953 | 14245 | `				pIn = pArgStop;` |
|      7953 | 14246 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14247 | `					pIn++;` |
|         8 | 14248 | `				}` |
|         5 | 14249 | `			}` |
|      7939 | 14250 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3967 | 14251 | `		}` |
|      8041 | 14252 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      8041 | 14253 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14254 | `			pIn++;` |
|         5 | 14255 | `			continue;` |
|         - | 14256 | `		}` |
|      8037 | 14257 | `		break;` |
|       ! 0 | 14258 | `	}` |
|      8037 | 14259 | `	pGen->pIn = pSavedIn;` |
|      8037 | 14260 | `	pGen->pEnd = pSavedEnd;` |
|      8037 | 14261 | `	return SXRET_OK;` |
|      4021 | 14262 | `}` |
|         - | 14263 | `/*` |
|         - | 14264 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14265 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14266 | ` */` |
|   3990016 | 14267 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14268 | `{` |
|   3990021 | 14269 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14270 | `	sxu32 n;` |
|         - | 14271 | `	sxi32 rc;` |
|   3998041 | 14272 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      8025 | 14273 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      8025 | 14274 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14275 | `			return SXERR_ABORT;` |
|         - | 14276 | `		}` |
|      4015 | 14277 | `	}` |
|   3990021 | 14278 | `	SySetReset(&pGen->aPendingAttrs);` |
|   3990021 | 14279 | `	return SXRET_OK;` |
|   1995013 | 14280 | `}` |
|         - | 14281 | `/*` |
|         - | 14282 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14283 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14284 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14285 | ` */` |
|   1709654 | 14286 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14287 | `{` |
|   1709659 | 14288 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   1709659 | 14289 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   1709659 | 14290 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14291 | `	sxu32 nIdx, n;` |
|         - | 14292 | `	sxi32 rc;` |
|   1709654 | 14293 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    197335 | 14294 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1512329 | 14295 | `		return SXRET_OK;` |
|         - | 14296 | `	}` |
|    197335 | 14297 | `	nIdx = (sxu32)(pTok - pBase);` |
|    591993 | 14298 | `	for( n = 0 ; n < nT ; n++ ){` |
|    394663 | 14299 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14300 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14301 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14302 | `				return SXERR_ABORT;` |
|         - | 14303 | `			}` |
|         6 | 14304 | `		}` |
|    197334 | 14305 | `	}` |
|    197335 | 14306 | `	return SXRET_OK;` |
|    854832 | 14307 | `}` |
|  10401586 | 14308 | `static sxi32 GenStateCompileChunk(` |
|         - | 14309 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14310 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14311 | `	)` |
|         5 | 14312 | `{` |
|         - | 14313 | `	ProcLangConstruct xCons;` |
|         - | 14314 | `	sxi32 rc;` |
|  10401591 | 14315 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   5990509 | 14316 | `	for(;;){` |
|  11191307 | 14317 | `		int bStmtIsDeclare = 0;` |
|  11191307 | 14318 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14319 | `			/* No more input to process */` |
|     69569 | 14320 | `			break;` |
|         - | 14321 | `		}` |
|         - | 14322 | `		/* Bind a directly-preceding docblock to this statement */` |
|  11121743 | 14323 | `		GenStateSetPendingDoc(&(*pGen));` |
|  11121743 | 14324 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14325 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14326 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14327 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14328 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14329 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7943 | 14330 | `			int bAttrTarget = 0;` |
|      7938 | 14331 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      4003 | 14332 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7885 | 14333 | `				bAttrTarget = 1;` |
|      3999 | 14334 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14335 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14336 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14337 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14338 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14339 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14340 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14341 | `					bAttrTarget = 1;` |
|        29 | 14342 | `				}` |
|        29 | 14343 | `			}` |
|      7943 | 14344 | `			if( !bAttrTarget ){` |
|       ! 0 | 14345 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14346 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14347 | `					&pGen->pIn->sData);` |
|       ! 0 | 14348 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14349 | `					break;` |
|         - | 14350 | `				}` |
|       ! 0 | 14351 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14352 | `			}` |
|      3969 | 14353 | `		}` |
|         - | 14354 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14355 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  11121743 | 14356 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6518091 | 14357 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   6518091 | 14358 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14359 | `				bStmtIsDeclare = 1;` |
|        21 | 14360 | `			}` |
|   3259043 | 14361 | `		}` |
|  11121743 | 14362 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14363 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14364 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    789689 | 14365 | `			pGen->bStrictTypesLocked = 1;` |
|    394842 | 14366 | `		}` |
|  11121743 | 14367 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14368 | `			/* Compile block */` |
|      3963 | 14369 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3963 | 14370 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14371 | `				break;` |
|         - | 14372 | `			}` |
|      1984 | 14373 | `		}else{` |
|  11117785 | 14374 | `			xCons = 0;` |
|  11117785 | 14375 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14376 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14377 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14378 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     35541 | 14379 | `				xCons = PH7_CompileClassModifiers;` |
|  11100017 | 14380 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14381 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14382 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3973 | 14383 | `				xCons = PH7_CompileEnum;` |
|  11080265 | 14384 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6482581 | 14385 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14386 | `				/* Try to extract a language construct handler */` |
|   6482581 | 14387 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   6482581 | 14388 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14389 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14390 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14391 | `						&pGen->pIn->sData);` |
|         9 | 14392 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14393 | `						break;` |
|         - | 14394 | `					}` |
|         - | 14395 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14396 | `					 * this erroneous statement.` |
|         - | 14397 | `					 */` |
|         9 | 14398 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14399 | `				}` |
|   7836993 | 14400 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    371089 | 14401 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14402 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14403 | `				xCons = PH7_CompileLabel;` |
|        56 | 14404 | `			}` |
|  11117785 | 14405 | `			if( xCons == 0 ){` |
|         - | 14406 | `				/* Assume an expression an try to compile it */` |
|   4635475 | 14407 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   4635475 | 14408 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14409 | `					/* Pop l-value */` |
|   4635325 | 14410 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2317660 | 14411 | `				}` |
|   2317740 | 14412 | `			}else{` |
|         - | 14413 | `				/* Go compile the sucker */` |
|   6482315 | 14414 | `				rc = xCons(&(*pGen));` |
|         - | 14415 | `			}` |
|  11117785 | 14416 | `			if( rc == SXERR_ABORT ){` |
|         - | 14417 | `				/* Request to abort compilation */` |
|        13 | 14418 | `				break;` |
|         - | 14419 | `			}` |
|         - | 14420 | `		}` |
|         - | 14421 | `		/* Ignore trailing semi-colons ';' */` |
|  19157757 | 14422 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   8036029 | 14423 | `			pGen->pIn++;` |
|         5 | 14424 | `		}` |
|  11121733 | 14425 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14426 | `			/* Compile a single statement and return */` |
|  10332017 | 14427 | `			break;` |
|         - | 14428 | `		}` |
|         - | 14429 | `		/* LOOP ONE */` |
|         - | 14430 | `		/* LOOP TWO */` |
|         - | 14431 | `		/* LOOP THREE */` |
|         - | 14432 | `		/* LOOP FOUR */` |
|         5 | 14433 | `	}` |
|         - | 14434 | `	/* Return compilation status */` |
|  10401591 | 14435 | `	return rc;` |
|         5 | 14436 | `}` |
|         - | 14437 | `/*` |
|         - | 14438 | ` * Compile a Raw PHP chunk.` |
|         - | 14439 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14440 | ` * takes care of generating the appropriate error message.` |
|         - | 14441 | ` */` |
|     69576 | 14442 | `static sxi32 PH7_CompilePHP(` |
|         - | 14443 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14444 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14445 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14446 | `	)` |
|         5 | 14447 | `{` |
|     69581 | 14448 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14449 | `	sxi32 rc;` |
|         - | 14450 | `	/* Reset the token set (and its trivia sidecar) */` |
|     69581 | 14451 | `	SySetReset(&(*pTokenSet));` |
|     69581 | 14452 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14453 | `	/* Mark as the default token set */` |
|     69581 | 14454 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14455 | `	/* Advance the stream cursor */` |
|     69581 | 14456 | `	pGen->pRawIn++;` |
|         - | 14457 | `	/* Tokenize the PHP chunk first */` |
|     69581 | 14458 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14459 | `	/* Point to the head and tail of the token stream. */` |
|     69581 | 14460 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     69581 | 14461 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     69581 | 14462 | `	if( is_expr ){` |
|       ! 0 | 14463 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14464 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14465 | `			/* A simple expression,compile it */` |
|       ! 0 | 14466 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14467 | `		}` |
|         - | 14468 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14469 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14470 | `		return SXRET_OK;` |
|         - | 14471 | `	}` |
|     69581 | 14472 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14473 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14474 | `		/*` |
|         - | 14475 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14476 | `		 * According to the PHP reference manual:` |
|         - | 14477 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14478 | `		 *  immediately follow` |
|         - | 14479 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14480 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14481 | `		 * Symisc extension:` |
|         - | 14482 | `		 *   This short syntax works with all PHP opening` |
|         - | 14483 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14484 | `		 *   only short tag.` |
|         - | 14485 | `		 */` |
|         - | 14486 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14487 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14488 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14489 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14490 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14491 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14492 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14493 | `		}` |
|         3 | 14494 | `		return SXRET_OK;` |
|         - | 14495 | `	}` |
|         - | 14496 | `	/* Compile the PHP chunk */` |
|     69579 | 14497 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14498 | `	/* Fix exceptions jumps */` |
|     69579 | 14499 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14500 | `	/* Fix gotos now, the jump destination is resolved */` |
|     69579 | 14501 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14502 | `		rc = SXERR_ABORT;` |
|         1 | 14503 | `	}` |
|         - | 14504 | `	/* Reset container */` |
|     69579 | 14505 | `	SySetReset(&pGen->aGoto);` |
|     69579 | 14506 | `	SySetReset(&pGen->aLabel);` |
|     69579 | 14507 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14508 | `	/* Compilation result */` |
|     69579 | 14509 | `	return rc;` |
|     34793 | 14510 | `}` |
|         - | 14511 | `/*` |
|         - | 14512 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14513 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14514 | ` * This is the only compile interface exported from this file.` |
|         - | 14515 | ` */` |
|     72654 | 14516 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14517 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14518 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14519 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14520 | `	)` |
|         5 | 14521 | `{` |
|         - | 14522 | `	SySet aPhpToken,aRawToken;` |
|         - | 14523 | `	ph7_gen_state *pCodeGen;` |
|         - | 14524 | `	ph7_value *pRawObj;` |
|         - | 14525 | `	sxu32 nObjIdx;` |
|         - | 14526 | `	sxi32 nRawObj;` |
|         - | 14527 | `	int is_expr;` |
|         - | 14528 | `	sxi8 bSavedStrict;` |
|         - | 14529 | `	sxi8 bSavedStrictLocked;` |
|         - | 14530 | `	sxi32 rc;` |
|     72659 | 14531 | `	if( pScript->nByte < 1 ){` |
|         - | 14532 | `		/* Nothing to compile */` |
|       ! 0 | 14533 | `		return PH7_OK;` |
|         - | 14534 | `	}` |
|         - | 14535 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14536 | `	 * file's flags so include/require restore them on return. */` |
|     72659 | 14537 | `	pCodeGen = &pVm->sCodeGen;` |
|     72659 | 14538 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     72659 | 14539 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     72659 | 14540 | `	pCodeGen->bStrictTypes = 0;` |
|     72659 | 14541 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14542 | `	/* Initialize the tokens containers */` |
|     72659 | 14543 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     72659 | 14544 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     72659 | 14545 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     72659 | 14546 | `	is_expr = 0;` |
|     72659 | 14547 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14548 | `		SyToken sTmp;` |
|         - | 14549 | `		/* PHP only: -*/` |
|     59217 | 14550 | `		sTmp.nLine = 1;` |
|     59217 | 14551 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     59217 | 14552 | `		sTmp.pUserData = 0;` |
|     59217 | 14553 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     59217 | 14554 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     59217 | 14555 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14556 | `			/* A simple PHP expression */` |
|       ! 0 | 14557 | `			is_expr = 1;` |
|       ! 0 | 14558 | `		}` |
|     29611 | 14559 | `	}else{` |
|         - | 14560 | `		/* Tokenize raw text */` |
|     13447 | 14561 | `		SySetAlloc(&aRawToken,32);` |
|     13447 | 14562 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|         - | 14563 | `	}` |
|         - | 14564 | `	/* Process high-level tokens */` |
|     72659 | 14565 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     72659 | 14566 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     72659 | 14567 | `	rc = PH7_OK;` |
|     72659 | 14568 | `	if( is_expr ){` |
|         - | 14569 | `		/* Compile the expression */` |
|       ! 0 | 14570 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14571 | `		goto cleanup;` |
|         - | 14572 | `	}` |
|     72659 | 14573 | `	nObjIdx = 0;` |
|         - | 14574 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14575 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14576 | `	 * preventing namespace bleeding across include()d files. */` |
|     72659 | 14577 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14578 | `	/* Start the compilation process */` |
|     43053 | 14579 | `	for(;;){` |
|    155675 | 14580 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     72647 | 14581 | `			break; /* No more tokens to process */` |
|         - | 14582 | `		}` |
|     83033 | 14583 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14584 | `			/* Compile the PHP chunk */` |
|     69581 | 14585 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     69581 | 14586 | `			if( rc == SXERR_ABORT ){` |
|        16 | 14587 | `				break;` |
|         - | 14588 | `			}` |
|     69569 | 14589 | `			continue;` |
|         - | 14590 | `		}` |
|         - | 14591 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13457 | 14592 | `		nRawObj = 0;` |
|     26909 | 14593 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14594 | `			/* Consume the raw chunk without any processing */` |
|     13457 | 14595 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13457 | 14596 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14597 | `				rc = SXERR_MEM;` |
|       ! 0 | 14598 | `				break;` |
|         - | 14599 | `			}` |
|         - | 14600 | `			/* Mark as constant and emit the load constant instruction */` |
|     13457 | 14601 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13457 | 14602 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13457 | 14603 | `			++nRawObj;` |
|     13457 | 14604 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14605 | `		}` |
|     13457 | 14606 | `		if( nRawObj > 0 ){` |
|         - | 14607 | `			/* Emit the consume instruction */` |
|     13457 | 14608 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6726 | 14609 | `		}` |
|     36332 | 14610 | `	}` |
|     36327 | 14611 | `cleanup:` |
|     72659 | 14612 | `	SySetRelease(&aRawToken);` |
|     72659 | 14613 | `	SySetRelease(&aPhpToken);` |
|         - | 14614 | `	/* Restore outer file's strict_types scope */` |
|     72659 | 14615 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     72659 | 14616 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     72659 | 14617 | `	return rc;` |
|     36332 | 14618 | `}` |
|         - | 14619 | `/*` |
|         - | 14620 | ` * Utility routines.Initialize the code generator.` |
|         - | 14621 | ` */` |
|      3940 | 14622 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 14623 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14624 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14625 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14626 | `	)` |
|         5 | 14627 | `{` |
|      3945 | 14628 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14629 | `	/* Zero the structure */` |
|      3945 | 14630 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 14631 | `	/* Initial state */` |
|      3945 | 14632 | `	pGen->pVm  = &(*pVm);` |
|      3945 | 14633 | `	pGen->xErr = xErr;` |
|      3945 | 14634 | `	pGen->pErrData = pErrData;` |
|      3945 | 14635 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3945 | 14636 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3945 | 14637 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3945 | 14638 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3945 | 14639 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3945 | 14640 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3945 | 14641 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 14642 | `	/* Error log buffer */` |
|      3945 | 14643 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 14644 | `	/* General purpose working buffer */` |
|      3945 | 14645 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 14646 | `	/* Namespace state */` |
|      3945 | 14647 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3945 | 14648 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3945 | 14649 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3945 | 14650 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14651 | `	/* Create the global scope */` |
|      3945 | 14652 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 14653 | `	/* Point to the global scope */` |
|      3945 | 14654 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3945 | 14655 | `	return SXRET_OK;` |
|         5 | 14656 | `}` |
|         - | 14657 | `/*` |
|         - | 14658 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 14659 | ` */` |
|     76206 | 14660 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 14661 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14662 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14663 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14664 | `	)` |
|         5 | 14665 | `{` |
|     76211 | 14666 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14667 | `	GenBlock *pBlock,*pParent;` |
|         - | 14668 | `	/* Reset state */` |
|     76211 | 14669 | `	SySetReset(&pGen->aLabel);` |
|     76211 | 14670 | `	SySetReset(&pGen->aGoto);` |
|     76211 | 14671 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     76211 | 14672 | `	SySetReset(&pGen->aTrivia);` |
|     76211 | 14673 | `	SySetReset(&pGen->aPendingAttrs);` |
|     76211 | 14674 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     76211 | 14675 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     76211 | 14676 | `	SyBlobRelease(&pGen->sWorker);` |
|     76211 | 14677 | `	SyBlobRelease(&pGen->sNamespace);` |
|     76211 | 14678 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     76211 | 14679 | `	SyHashRelease(&pGen->hUseImports);` |
|     76211 | 14680 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     76211 | 14681 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     76211 | 14682 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     76211 | 14683 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     76211 | 14684 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14685 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 14686 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 14687 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 14688 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 14689 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 14690 | `	 * number of unique names, which is acceptable. */` |
|         - | 14691 | `	/* Point to the global scope */` |
|     76211 | 14692 | `	pBlock = pGen->pCurrent;` |
|     76211 | 14693 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 14694 | `		pParent = pBlock->pParent;` |
|       ! 0 | 14695 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 14696 | `		pBlock = pParent;` |
|       ! 0 | 14697 | `	}` |
|     76211 | 14698 | `	pGen->xErr = xErr;` |
|     76211 | 14699 | `	pGen->pErrData = pErrData;` |
|     76211 | 14700 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     76211 | 14701 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     76211 | 14702 | `	pGen->pIn = pGen->pEnd = 0;` |
|     76211 | 14703 | `	pGen->nErr = 0;` |
|     76211 | 14704 | `	return SXRET_OK;` |
|         5 | 14705 | `}` |
|         - | 14706 | `/*` |
|         - | 14707 | ` * Generate a compile-time error message.` |
|         - | 14708 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 14709 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 14710 | ` * abort compilation immediately.` |
|         - | 14711 | ` */` |
|     16460 | 14712 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 14713 | `{` |
|     16465 | 14714 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     16465 | 14715 | `	const char *zErr = "Error";` |
|         - | 14716 | `	SyString *pFile;` |
|         - | 14717 | `	va_list ap;` |
|         - | 14718 | `	sxi32 rc;` |
|         - | 14719 | `	/* Reset the working buffer */` |
|     16465 | 14720 | `	SyBlobReset(pWorker);` |
|         - | 14721 | `	/* Peek the processed file path if available */` |
|     16465 | 14722 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     16465 | 14723 | `	if( nErrType == E_ERROR ){` |
|         - | 14724 | `		/* Increment the error counter */` |
|       551 | 14725 | `		pGen->nErr++;` |
|       551 | 14726 | `		if( pGen->nErr > 15 ){` |
|         - | 14727 | `			/* Error count limit reached */` |
|         6 | 14728 | `			if( pGen->xErr ){` |
|         6 | 14729 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 14730 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 14731 | `				if( pFile ){` |
|         6 | 14732 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 14733 | `				}` |
|         6 | 14734 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 14735 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 14736 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 14737 | `				}` |
|         2 | 14738 | `			}` |
|         - | 14739 | `			/* Abort immediately */` |
|         6 | 14740 | `			return SXERR_ABORT;` |
|         - | 14741 | `		}` |
|       271 | 14742 | `	}` |
|     16461 | 14743 | `	if( pGen->xErr == 0 ){` |
|         - | 14744 | `		/* No available error consumer,return immediately */` |
|     15767 | 14745 | `		return SXRET_OK;` |
|         - | 14746 | `	}` |
|       698 | 14747 | `	switch(nErrType){` |
|       544 | 14748 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        32 | 14749 | `	case E_WARNING: zErr = "Warning";     break;` |
|       116 | 14750 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|        11 | 14751 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 14752 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 14753 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 14754 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|         7 | 14755 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 14756 | `	default:` |
|       ! 0 | 14757 | `		break;` |
|         - | 14758 | `	}` |
|       698 | 14759 | `	rc = SXRET_OK;` |
|         - | 14760 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       698 | 14761 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       698 | 14762 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       698 | 14763 | `	va_start(ap,zFormat);` |
|       698 | 14764 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       698 | 14765 | `	va_end(ap);` |
|       698 | 14766 | `	if( pFile ){` |
|       698 | 14767 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       347 | 14768 | `	}` |
|         - | 14769 | `	/* Append a new line */` |
|       698 | 14770 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       698 | 14771 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 14772 | `		/* Consume the generated error message */` |
|       698 | 14773 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       347 | 14774 | `	}` |
|       698 | 14775 | `	return rc;` |
|      8235 | 14776 | `}` |
|         - | 14777 |  |
