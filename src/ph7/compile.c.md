# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7053/8730 lines (80.79%)

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
|       277 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   121 | `			/* Jump destination found */` |
|        97 |   122 | `			aLabel[n].bRef = TRUE;` |
|        97 |   123 | `			if( ppOut ){` |
|        97 |   124 | `				*ppOut = &aLabel[n];` |
|        46 |   125 | `			}` |
|        97 |   126 | `			return SXRET_OK;` |
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
|    118672 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   138 | `{` |
|    118677 |   139 | `	GenBlock *pBlock = pCurrent;` |
|    276497 |   140 | `	for(;;){` |
|    552999 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    118569 |   142 | `			iCount--; /* Decrement nesting level */` |
|    118569 |   143 | `			if( iCount < 1 ){` |
|         - |   144 | `				/* Block meet with the desired criteria */` |
|    118543 |   145 | `				return pBlock;` |
|         - |   146 | `			}` |
|        13 |   147 | `		}` |
|         - |   148 | `		/* Point to the upper block */` |
|    434461 |   149 | `		pBlock = pBlock->pParent;` |
|    434461 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   151 | `			/* Forbidden */` |
|        72 |   152 | `			break;` |
|         - |   153 | `		}` |
|         5 |   154 | `	}` |
|         - |   155 | `	/* No such block */` |
|       139 |   156 | `	return 0;` |
|     59341 |   157 | `}` |
|         - |   158 | `/*` |
|         - |   159 | ` * Initialize a freshly allocated block instance.` |
|         - |   160 | ` */` |
|  10363210 |   161 | `static void GenStateInitBlock(` |
|         - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   166 | `	void *pUserData      /* Upper layer private data */` |
|         - |   167 | `	)` |
|         5 |   168 | `{` |
|         - |   169 | `	/* Initialize block fields */` |
|  10363215 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  10363215 |   171 | `	pBlock->pUserData   = pUserData;` |
|  10363215 |   172 | `	pBlock->pGen        = pGen;` |
|  10363215 |   173 | `	pBlock->iFlags      = iType;` |
|  10363215 |   174 | `	pBlock->pParent     = 0;` |
|  10363215 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  10363215 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  10363215 |   177 | `}` |
|         - |   178 | `/*` |
|         - |   179 | ` * Allocate a new block instance.` |
|         - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   182 | ` * processing on failure.` |
|         - |   183 | ` */` |
|  10359264 |   184 | `static sxi32 GenStateEnterBlock(` |
|         - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   190 | `	)` |
|         5 |   191 | `{` |
|         - |   192 | `	GenBlock *pBlock;` |
|         - |   193 | `	/* Allocate a new block instance */` |
|  10359269 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  10359269 |   195 | `	if( pBlock == 0 ){` |
|         - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   198 | `		 */` |
|       ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   200 | `		/* Abort processing immediately */` |
|       ! 0 |   201 | `		return SXERR_ABORT;` |
|         - |   202 | `	}` |
|         - |   203 | `	/* Zero the structure */` |
|  10359269 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  10359269 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   206 | `	/* Link to the parent block */` |
|  10359269 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   208 | `	/* Mark as the current block */` |
|  10359269 |   209 | `	pGen->pCurrent = pBlock;` |
|  10359269 |   210 | `	if( ppBlock ){` |
|         - |   211 | `		/* Write a pointer to the new instance */` |
|   4986881 |   212 | `		*ppBlock = pBlock;` |
|   2493438 |   213 | `	}` |
|  10359269 |   214 | `	return SXRET_OK;` |
|   5179637 |   215 | `}` |
|         - |   216 | `/*` |
|         - |   217 | ` * Release block fields without freeing the whole instance.` |
|         - |   218 | ` */` |
|  10359248 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   220 | `{` |
|  10359253 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  10359253 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  10359253 |   223 | `}` |
|         - |   224 | `/*` |
|         - |   225 | ` * Release a block.` |
|         - |   226 | ` */` |
|  10359248 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   228 | `{` |
|  10359253 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  10359253 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   231 | `	/* Free the instance */` |
|  10359253 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  10359253 |   233 | `}` |
|         - |   234 | `/*` |
|         - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   236 | ` */` |
|  10359248 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   238 | `{` |
|  10359253 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  10359253 |   240 | `	if( pBlock == 0 ){` |
|         - |   241 | `		/* No more block to pop */` |
|       ! 0 |   242 | `		return SXERR_EMPTY;` |
|         - |   243 | `	}` |
|         - |   244 | `	/* Point to the upper block */` |
|  10359253 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  10359253 |   246 | `	if( ppBlock ){` |
|         - |   247 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   248 | `		*ppBlock = pBlock;` |
|       ! 0 |   249 | `	}else{` |
|         - |   250 | `		/* Safely release the block */` |
|  10359253 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   252 | `	}` |
|  10359253 |   253 | `	return SXRET_OK;` |
|   5179629 |   254 | `}` |
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
|   3704274 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   266 | `{` |
|         - |   267 | `	JumpFixup sJumpFix;` |
|         - |   268 | `	sxi32 rc;` |
|         - |   269 | `	/* Init the JumpFixup structure */` |
|   3704279 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|   3704279 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   272 | `	/* Insert in the jump fixup table */` |
|   3704279 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   3704279 |   274 | `	return rc;` |
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
|   7205768 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   288 | `{` |
|         - |   289 | `	JumpFixup *aFix;` |
|         - |   290 | `	VmInstr *pInstr;` |
|         - |   291 | `	sxu32 nFixed;` |
|         - |   292 | `	sxu32 n;` |
|         - |   293 | `	/* Point to the jump fixup table */` |
|   7205773 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   295 | `	/* Fix the desired jumps */` |
|  15216009 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   8010241 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   298 | `			/* Already fixed */` |
|   3038449 |   299 | `			continue;` |
|         - |   300 | `		}` |
|   4971797 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   302 | `			/* Not of our interest */` |
|   1267525 |   303 | `			continue;` |
|         - |   304 | `		}` |
|         - |   305 | `		/* Point to the instruction to fix */` |
|   3704277 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   3704277 |   307 | `		if( pInstr ){` |
|   3704277 |   308 | `			pInstr->iP2 = nJumpDest;` |
|   3704277 |   309 | `			nFixed++;` |
|         - |   310 | `			/* Mark as fixed */` |
|   3704277 |   311 | `			aFix[n].nJumpType = -1;` |
|   1852136 |   312 | `		}` |
|   1852141 |   313 | `	}` |
|         - |   314 | `	/* Total number of fixed jumps */` |
|   7205773 |   315 | `	return nFixed;` |
|         5 |   316 | `}` |
|         - |   317 | `/*` |
|         - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   319 | ` * The goto statement can be used to jump to another section` |
|         - |   320 | ` * in the program.` |
|         - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   322 | ` * statement for more information.` |
|         - |   323 | ` */` |
|   2701324 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   325 | `{` |
|         - |   326 | `	JumpFixup *pJump,*aJumps;` |
|         - |   327 | `	Label *pLabel,*aLabel;` |
|         - |   328 | `	VmInstr *pInstr;` |
|         - |   329 | `	sxi32 rc;` |
|         - |   330 | `	sxu32 n;` |
|         - |   331 | `	/* Point to the goto table */` |
|   2701329 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   333 | `	/* Fix */` |
|   2701475 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|        97 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|        11 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|        11 |   349 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |   350 | `				return SXERR_ABORT;` |
|         - |   351 | `			}` |
|         4 |   352 | `		}` |
|         - |   353 | `		/* Fix the jump now the destination is resolved */` |
|        97 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|        97 |   355 | `		if( pInstr ){` |
|        97 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|        46 |   357 | `		}` |
|        51 |   358 | `	}` |
|   2701327 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   2701459 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|       137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|         - |   362 | `			/* Emit a warning */` |
|        40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|        24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|        12 |   365 | `		}` |
|        71 |   366 | `	}` |
|   2701327 |   367 | `	return SXRET_OK;` |
|   1350667 |   368 | `}` |
|         - |   369 | `/*` |
|         - |   370 | ` * Check if a given token value is installed in the literal table.` |
|         - |   371 | ` */` |
|  13633598 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   373 | `{` |
|         - |   374 | `	SyHashEntry *pEntry;` |
|  13633603 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  13633603 |   376 | `	if( pEntry == 0 ){` |
|   3570099 |   377 | `		return SXERR_NOTFOUND;` |
|         - |   378 | `	}` |
|  10063509 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10063509 |   380 | `	return SXRET_OK;` |
|   6816804 |   381 | `}` |
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
|   3570094 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   393 | `{` |
|   3570099 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3570099 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1785047 |   396 | `	}` |
|   3570099 |   397 | `	return SXRET_OK;` |
|         5 |   398 | `}` |
|         - |   399 | `/*` |
|         - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   401 | ` * in the constant table.` |
|         - |   402 | ` */` |
|   2800798 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   404 | `{` |
|         - |   405 | `	ph7_value *pObj;` |
|   2800803 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   407 | `	/* Reserve a new constant */` |
|   2800803 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   2800803 |   409 | `	if( pObj == 0 ){` |
|       ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   411 | `		return 0;` |
|         - |   412 | `	}` |
|   2800803 |   413 | `	*pIdx = nIdx;` |
|         - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   416 | `	 */` |
|   2800803 |   417 | `	return pObj;` |
|   1400404 |   418 | `}` |
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
|   6368880 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   434 | `{` |
|         - |   435 | `	VmCallArgMap *pMap;` |
|   6368885 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   437 | `	if( p3 == 0 ){` |
|        35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   439 | `		if( pMap == 0 ) return 0;` |
|        35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   441 | `		p3 = (void *)pMap;` |
|        16 |   442 | `	}` |
|        39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   444 | `	return p3;` |
|   3184445 |   445 | `}` |
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
|   2801794 |   509 | `static int GenStateFindBadNumericSeparator(` |
|         - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   511 | `{` |
|   2801799 |   512 | `	const char *z = pRaw->zString;` |
|   2801799 |   513 | `	sxu32 n = pRaw->nByte;` |
|   2801799 |   514 | `	int base = 10;` |
|         - |   515 | `	sxu32 i, start;` |
|   2801799 |   516 | `	if( n < 2 ) return 0;` |
|    458071 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        80 |   518 | `		base = 16;` |
|    458032 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       284 |   520 | `		base = 2;` |
|       141 |   521 | `	}` |
|   1533073 |   522 | `	for( i = 0; i < n; ++i ){` |
|   1075021 |   523 | `		if( z[i] != '_' ) continue;` |
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
|    458057 |   540 | `	return 0;` |
|   1400902 |   541 | `}` |
|         - |   542 | `/*` |
|         - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   548 | ` * so callers can bail from the current construct).` |
|         - |   549 | ` */` |
|   2801794 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   551 | `{` |
|   2801799 |   552 | `	const char *zBad = 0;` |
|   2801799 |   553 | `	sxu32 nBad = 0;` |
|         - |   554 | `	SyString sBad;` |
|         - |   555 | `	sxi32 rc;` |
|   2801799 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   2801785 |   557 | `		return SXRET_OK;` |
|         - |   558 | `	}` |
|        18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   562 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   563 | `		return SXERR_ABORT;` |
|         - |   564 | `	}` |
|        18 |   565 | `	return SXERR_SYNTAX;` |
|   1400902 |   566 | `}` |
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
|   2801780 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   584 | `	SyMemBackend *pAlloc,` |
|         - |   585 | `	const SyString *pToken,` |
|         - |   586 | `	char *zScratch, sxu32 nScratch,` |
|         - |   587 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   588 | `{` |
|         - |   589 | `	sxu32 i, j;` |
|   2801785 |   590 | `	int hasUnderscore = 0;` |
|         - |   591 | `	char *zBuf;` |
|   2801785 |   592 | `	*pzAlloc = 0;` |
|   6218449 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   3416921 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   1708337 |   595 | `	}` |
|   2801785 |   596 | `	if( !hasUnderscore ){` |
|   2801533 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|   2801533 |   598 | `		return SXRET_OK;` |
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
|   1400895 |   615 | `}` |
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
|   2800832 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   652 | `{` |
|   2800837 |   653 | `	const char *z = pNum->zString;` |
|   2800837 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   655 | `	const char *p, *q;` |
|         - |   656 | `	int n;` |
|   2800837 |   657 | `	*pbDecimal = FALSE;` |
|   2800837 |   658 | `	if( z >= zEnd ){` |
|       ! 0 |   659 | `		return FALSE;` |
|         - |   660 | `	}` |
|   2800837 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
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
|   2800761 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|   2800481 |   691 | `	}else if( z[0] == '0' ){` |
|         - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1059811 |   695 | `		p = z;` |
|   2119619 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1060039 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1059811 |   698 | `		if( n <= 21 ){` |
|   1059809 |   699 | `			return FALSE;` |
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
|   1740675 |   712 | `	p = z;` |
|   1740675 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   4091045 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   1740675 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   716 | `		*pbDecimal = TRUE;` |
|        25 |   717 | `		return TRUE;` |
|         - |   718 | `	}` |
|   1740651 |   719 | `	return FALSE;` |
|   1400421 |   720 | `}` |
|   2801766 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   722 | `{` |
|   2801771 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   2801771 |   724 | `	sxu32 nIdx = 0;` |
|         - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   2801771 |   726 | `	char *zAlloc = 0;` |
|         - |   727 | `	SyString sNum;` |
|         - |   728 | `	sxi32 rc;` |
|   1400883 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   2801771 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   2801771 |   731 | `	if( rc != SXRET_OK ){` |
|        14 |   732 | `		return rc;` |
|         - |   733 | `	}` |
|   4202639 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1400878 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   2801761 |   736 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   737 | `		return SXERR_ABORT;` |
|         - |   738 | `	}` |
|   2801761 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   740 | `		ph7_value *pObj;` |
|         - |   741 | `		sxi64 iValue;` |
|   2800837 |   742 | `		ph7_real rOverflow = 0;` |
|   2800837 |   743 | `		int bDecimalOverflow = 0;` |
|   2800837 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|   2800803 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   2800803 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   2800803 |   763 | `			if( pObj == 0 ){` |
|       ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   765 | `				return SXERR_ABORT;` |
|         - |   766 | `			}` |
|   2800803 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   768 | `		}` |
|   1400421 |   769 | `	}else{` |
|         - |   770 | `		/* Real number */` |
|         - |   771 | `		ph7_value *pObj;` |
|         - |   772 | `		/* Reserve a new constant */` |
|       927 |   773 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       927 |   774 | `		if( pObj == 0 ){` |
|       ! 0 |   775 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   776 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   777 | `			return SXERR_ABORT;` |
|         - |   778 | `		}` |
|       927 |   779 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       927 |   780 | `		PH7_MemObjToReal(pObj);` |
|         - |   781 | `	}` |
|   2801761 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   783 | `	/* Emit the load constant instruction */` |
|   2801761 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   785 | `	/* Node successfully compiled */` |
|   2801761 |   786 | `	return SXRET_OK;` |
|   1400888 |   787 | `}` |
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
|   4452224 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   800 | `{` |
|   4452229 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   803 | `	ph7_value *pObj;` |
|         - |   804 | `	sxu32 nIdx;` |
|   4452229 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   806 | `	/* Delimit the string */` |
|   4452229 |   807 | `	zIn  = pStr->zString;` |
|   4452229 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|   4452229 |   809 | `	if( zIn >= zEnd ){` |
|         - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   811 | `		 * rather than reserving a new object each time. */` |
|    209347 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    209347 |   813 | `		return SXRET_OK;` |
|         - |   814 | `	}` |
|   4242887 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   816 | `		/* Already processed,emit the load constant instruction` |
|         - |   817 | `		 * and return.` |
|         - |   818 | `		 */` |
|   2474679 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2474679 |   820 | `		return SXRET_OK;` |
|         - |   821 | `	}` |
|         - |   822 | `	/* Reserve a new constant */` |
|   1768213 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1768213 |   824 | `	if( pObj == 0 ){` |
|       ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   827 | `		return SXERR_ABORT;` |
|         - |   828 | `	}` |
|   1768213 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   830 | `	/* Compile the node */` |
|   1811674 |   831 | `	for(;;){` |
|   3623353 |   832 | `		if( zIn >= zEnd ){` |
|         - |   833 | `			/* End of input */` |
|   1768213 |   834 | `			break;` |
|         - |   835 | `		}` |
|   1855145 |   836 | `		zCur = zIn;` |
|  38006077 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  36150937 |   838 | `			zIn++;` |
|         5 |   839 | `		}` |
|   1855145 |   840 | `		if( zIn > zCur ){` |
|         - |   841 | `			/* Append raw contents*/` |
|   1815655 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    907825 |   843 | `		}` |
|   1855145 |   844 | `		zIn++;` |
|   1855145 |   845 | `		if( zIn < zEnd ){` |
|    122477 |   846 | `			if( zIn[0] == '\\' ){` |
|         - |   847 | `				/* A literal backslash */` |
|     31607 |   848 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    106676 |   849 | `			}else if( zIn[0] == '\'' ){` |
|         - |   850 | `				/* A single quote */` |
|        11 |   851 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   852 | `			}else{` |
|         - |   853 | `				/* verbatim copy */` |
|     90865 |   854 | `				zIn--;` |
|     90865 |   855 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     90865 |   856 | `				zIn++;` |
|         - |   857 | `			}` |
|     61236 |   858 | `		}` |
|         - |   859 | `		/* Advance the stream cursor */` |
|   1855145 |   860 | `		zIn++;` |
|         5 |   861 | `	}` |
|         - |   862 | `	/* Emit the load constant instruction */` |
|   1768213 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1768213 |   864 | `	if( pStr->nByte < 1024 ){` |
|         - |   865 | `		/* Install in the literal table */` |
|   1768213 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    884104 |   867 | `	}` |
|         - |   868 | `	/* Node successfully compiled */` |
|   1768213 |   869 | `	return SXRET_OK;` |
|   2226117 |   870 | `}` |
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
|      2576 |  1036 | `static sxi32 GenStateProcessStringExpression(` |
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
|      2581 |  1047 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1048 | `	/* Preallocate some slots */` |
|      2581 |  1049 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1050 | `	/* Tokenize the text */` |
|      2581 |  1051 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1052 | `	/* Swap delimiter */` |
|      2581 |  1053 | `	pTmpIn  = pGen->pIn;` |
|      2581 |  1054 | `	pTmpEnd = pGen->pEnd;` |
|      2581 |  1055 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2581 |  1056 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1057 | `	/* Compile the expression */` |
|      2581 |  1058 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1059 | `	/* Restore token stream */` |
|      2581 |  1060 | `	pGen->pIn  = pTmpIn;` |
|      2581 |  1061 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1062 | `	/* Release the token set */` |
|      2581 |  1063 | `	SySetRelease(&sToken);` |
|         - |  1064 | `	/* Compilation result */` |
|      2581 |  1065 | `	return rc;` |
|         5 |  1066 | `}` |
|         - |  1067 | `/*` |
|         - |  1068 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1069 | ` */` |
|     83830 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1071 | `{` |
|         - |  1072 | `	ph7_value *pConstObj;` |
|     83835 |  1073 | `	sxu32 nIdx = 0;` |
|         - |  1074 | `	/* Reserve a new constant */` |
|     83835 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     83835 |  1076 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1078 | `		return 0;` |
|         - |  1079 | `	}` |
|     83835 |  1080 | `	(*pCount)++;` |
|     83835 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1082 | `	/* Emit the load constant instruction */` |
|     83835 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     83835 |  1084 | `	return pConstObj;` |
|     41920 |  1085 | `}` |
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
|     82266 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1149 | `{` |
|     82271 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|     82271 |  1152 | `	ph7_value *pObj = 0;` |
|         - |  1153 | `	sxi32 iCons;` |
|         - |  1154 | `	sxi32 rc;` |
|         - |  1155 | `	/* Delimit the string */` |
|     82271 |  1156 | `	zIn  = pStr->zString;` |
|     82271 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|     82271 |  1158 | `	if( zIn >= zEnd ){` |
|         - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1162 | `		 */` |
|       385 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       385 |  1164 | `		return SXRET_OK;` |
|         - |  1165 | `	}` |
|     81891 |  1166 | `	zCur = 0;` |
|         - |  1167 | `	/* Compile the node */` |
|     81891 |  1168 | `	iCons = 0;` |
|     42231 |  1169 | `	for(;;){` |
|    117265 |  1170 | `		zCur = zIn;` |
|   1588045 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1473361 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        72 |  1173 | `				break;` |
|   1473228 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2448 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1224 |  1176 | `					break;` |
|         - |  1177 | `			}` |
|   1470785 |  1178 | `			zIn++;` |
|         5 |  1179 | `		}` |
|    117265 |  1180 | `		if( zIn > zCur ){` |
|     56969 |  1181 | `			if( pObj == 0 ){` |
|     56375 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     56375 |  1183 | `				if( pObj == 0 ){` |
|       ! 0 |  1184 | `					return SXERR_ABORT;` |
|         - |  1185 | `				}` |
|     28185 |  1186 | `			}` |
|     56969 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     28482 |  1188 | `		}` |
|    117265 |  1189 | `		if( zIn >= zEnd ){` |
|     81889 |  1190 | `			break;` |
|         - |  1191 | `		}` |
|     35381 |  1192 | `		if( zIn[0] == '\\' ){` |
|     32805 |  1193 | `			const char *zPtr = 0;` |
|         - |  1194 | `			sxu32 n;` |
|     32805 |  1195 | `			zIn++;` |
|     32805 |  1196 | `			if( pObj == 0 ){` |
|     27465 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27465 |  1198 | `				if( pObj == 0 ){` |
|       ! 0 |  1199 | `					return SXERR_ABORT;` |
|         - |  1200 | `				}` |
|     13730 |  1201 | `			}` |
|     32805 |  1202 | `			if( zIn >= zEnd ){` |
|         - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1205 | `				break;` |
|         - |  1206 | `			}` |
|     32803 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|     32803 |  1208 | `			switch( zIn[0] ){` |
|        11 |  1209 | `			case '$':` |
|         - |  1210 | `				/* Dollar sign */` |
|        24 |  1211 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        24 |  1212 | `				break;` |
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
|     13833 |  1225 | `			case 'n':` |
|         - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     27671 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     27671 |  1228 | `				break;` |
|        27 |  1229 | `			case 'r':` |
|         - |  1230 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1231 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1232 | `				break;` |
|      2004 |  1233 | `			case 't':` |
|         - |  1234 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      4013 |  1235 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      4013 |  1236 | `				break;` |
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
|     32803 |  1351 | `			zIn += n;` |
|     32803 |  1352 | `			continue;` |
|         - |  1353 | `		}` |
|      2581 |  1354 | `		if( zIn[0] == '{' ){` |
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
|      2443 |  1388 | `			const char *zExpr = zIn;` |
|         - |  1389 | `			/* Assemble variable name */` |
|      1244 |  1390 | `			for(;;){` |
|         - |  1391 | `				/* Jump leading dollars */` |
|      4931 |  1392 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2443 |  1393 | `					zIn++;` |
|         5 |  1394 | `				}` |
|      1244 |  1395 | `				for(;;){` |
|     12877 |  1396 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9145 |  1397 | `						zIn++;` |
|         5 |  1398 | `					}` |
|      2493 |  1399 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1400 | `						/* UTF-8 stream */` |
|       ! 0 |  1401 | `						zIn++;` |
|       ! 0 |  1402 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1403 | `							zIn++;` |
|       ! 0 |  1404 | `						}` |
|       ! 0 |  1405 | `						continue;` |
|         - |  1406 | `					}` |
|      2493 |  1407 | `					break;` |
|       ! 0 |  1408 | `				}` |
|      2493 |  1409 | `				if( zIn >= zEnd ){` |
|       263 |  1410 | `					break;` |
|         - |  1411 | `				}` |
|      2235 |  1412 | `				if( zIn[0] == '[' ){` |
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
|      2225 |  1430 | `				}else if(zIn[0] == '{' ){` |
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
|      2221 |  1448 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1449 | `					/* Member access operator '->' */` |
|        53 |  1450 | `					zIn += 2;` |
|      2196 |  1451 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1452 | `					/* Static member access operator '::' */` |
|       ! 0 |  1453 | `					zIn += 2;` |
|       ! 0 |  1454 | `				}else{` |
|      1088 |  1455 | `					break;` |
|         - |  1456 | `				}` |
|         3 |  1457 | `			}` |
|         - |  1458 | `			/* Process the expression */` |
|      2443 |  1459 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2443 |  1460 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1461 | `				return SXERR_ABORT;` |
|         - |  1462 | `			}` |
|      2443 |  1463 | `			if( rc != SXERR_EMPTY ){` |
|      2441 |  1464 | `				++iCons;` |
|      1218 |  1465 | `			}` |
|         - |  1466 | `		}` |
|         - |  1467 | `		/* Invalidate the previously used constant */` |
|      2581 |  1468 | `		pObj = 0;` |
|         5 |  1469 | `	}/*for(;;)*/` |
|     81891 |  1470 | `	if( iCons > 1 ){` |
|         - |  1471 | `		/* Concatenate all compiled constants */` |
|      1869 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       932 |  1473 | `	}` |
|         - |  1474 | `	/* Node successfully compiled */` |
|     81891 |  1475 | `	return SXRET_OK;` |
|     41138 |  1476 | `}` |
|         - |  1477 | `/*` |
|         - |  1478 | ` * Compile a double quoted string.` |
|         - |  1479 | ` *  See the block-comment above for more information.` |
|         - |  1480 | ` */` |
|     82204 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1482 | `{` |
|         - |  1483 | `	sxi32 rc;` |
|     82209 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     41102 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1486 | `	/* Compilation result */` |
|     82209 |  1487 | `	return rc;` |
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
|   1027930 |  1531 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   1027935 |  1542 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1543 | `	/* Compile the expression*/` |
|   1027935 |  1544 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1545 | `	/* Restore token stream */` |
|   1027935 |  1546 | `	RE_SWAP_DELIMITER(pGen);` |
|   1027935 |  1547 | `	return rc;` |
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
|    967502 |  1588 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1589 | `{` |
|    967507 |  1590 | `	SyToken *pCur = pStart;` |
|    967507 |  1591 | `	sxi32 iNest = 0;` |
|   2668285 |  1592 | `	while( pCur < pEnd ){` |
|   2093747 |  1593 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    392965 |  1594 | `			return pCur;` |
|         - |  1595 | `		}` |
|         - |  1596 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1597 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1598 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1599 | `		 */` |
|   1700787 |  1600 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23783 |  1601 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23783 |  1602 | `			SyToken *pFn = pCur;` |
|     23778 |  1603 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1604 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1605 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1606 | `				pFn = &pCur[1];` |
|       ! 0 |  1607 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1608 | `			}` |
|     23783 |  1609 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|     23779 |  1640 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|     11886 |  1660 | `		}` |
|   1700781 |  1661 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     55695 |  1662 | `			iNest++;` |
|   1672936 |  1663 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1664 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1665 | `			 * parser will shortly detect any syntax error. */` |
|     55695 |  1666 | `			iNest--;` |
|     27845 |  1667 | `		}` |
|   1700781 |  1668 | `		pCur++;` |
|         5 |  1669 | `	}` |
|    574543 |  1670 | `	return pEnd;` |
|    483756 |  1671 | `}` |
|         - |  1672 | `/*` |
|         - |  1673 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1674 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1675 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1676 | ` */` |
|    516742 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1678 | `{` |
|         - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1680 | `	SyToken *pKey,*pCur;` |
|    516747 |  1681 | `	sxi32 iEmitRef = 0;` |
|    516747 |  1682 | `	sxi32 iSpread = 0;` |
|    516747 |  1683 | `	sxi32 nPair = 0;` |
|         - |  1684 | `	sxi32 rc;` |
|    516747 |  1685 | `	xValidator = 0;` |
|    627194 |  1686 | `	for(;;){` |
|         - |  1687 | `		/* Jump leading commas */` |
|   1767915 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    513527 |  1689 | `			pGen->pIn++;` |
|         5 |  1690 | `		}` |
|   1254393 |  1691 | `		pCur = pGen->pIn;` |
|   1254393 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1693 | `			/* No more entry to process */` |
|    516731 |  1694 | `			break;` |
|         - |  1695 | `		}` |
|    737667 |  1696 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1697 | `			continue;` |
|         - |  1698 | `		}` |
|         - |  1699 | `		/* Compile the key if available */` |
|    737667 |  1700 | `		pKey = pCur;` |
|    737667 |  1701 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|    737667 |  1702 | `		rc = SXERR_EMPTY;` |
|    737667 |  1703 | `		if( pCur < pGen->pIn ){` |
|    290015 |  1704 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1705 | `				/* Missing value */` |
|        13 |  1706 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|        13 |  1707 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1708 | `					return SXERR_ABORT;` |
|         - |  1709 | `				}` |
|        13 |  1710 | `				return SXRET_OK;` |
|         - |  1711 | `			}` |
|         - |  1712 | `			/* Compile the expression holding the key */` |
|    290005 |  1713 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1714 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    290005 |  1715 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1716 | `				return SXERR_ABORT;` |
|         - |  1717 | `			}` |
|    290005 |  1718 | `			pCur++; /* Jump the '=>' operator */` |
|    592657 |  1719 | `		}else if( pKey == pCur ){` |
|         - |  1720 | `			/* Key is omitted,emit a warning */` |
|       ! 0 |  1721 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|       ! 0 |  1722 | `			pCur++; /* Jump the '=>' operator */` |
|       ! 0 |  1723 | `		}else{` |
|         - |  1724 | `			/* Reset back the cursor and point to the entry value */` |
|    447657 |  1725 | `			pCur = pKey;` |
|         - |  1726 | `		}` |
|    737657 |  1727 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1728 | `			/* No available key,load NULL */` |
|    447659 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    223827 |  1730 | `		}` |
|    737657 |  1731 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|    737655 |  1750 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|    737655 |  1751 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|         - |  1763 | `		/* Compile indice value */` |
|    737651 |  1764 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|    737651 |  1765 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1766 | `			return SXERR_ABORT;` |
|         - |  1767 | `		}` |
|    737651 |  1768 | `		if( iSpread ){` |
|         - |  1769 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        69 |  1770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|    737618 |  1771 | `		}else if( iEmitRef ){` |
|         - |  1772 | `			/* Emit the load reference instruction */` |
|        40 |  1773 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1774 | `		}` |
|    737651 |  1775 | `		xValidator = 0;` |
|    737651 |  1776 | `		iEmitRef = 0;` |
|    737651 |  1777 | `		iSpread = 0;` |
|    737651 |  1778 | `		nPair++;` |
|         5 |  1779 | `	}` |
|         - |  1780 | `	/* Emit the load map instruction */` |
|    516731 |  1781 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1782 | `	/* Node successfully compiled */` |
|    516731 |  1783 | `	return SXRET_OK;` |
|    258376 |  1784 | `}` |
|         - |  1785 | `/*` |
|         - |  1786 | ` * Compile the 'array' language construct.` |
|         - |  1787 | ` *	 According to the PHP language reference manual` |
|         - |  1788 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1789 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1790 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1791 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1792 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1793 | ` */` |
|    293860 |  1794 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1795 | `{` |
|         - |  1796 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    293865 |  1797 | `	pGen->pIn += 2;` |
|    293865 |  1798 | `	pGen->pEnd--;` |
|    146930 |  1799 | `	SXUNUSED(iCompileFlag);` |
|    293865 |  1800 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1801 | `}` |
|         - |  1802 | `/*` |
|         - |  1803 | ` * Compile the PHP 8.5 clone(...) call form:` |
|         - |  1804 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|         - |  1805 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|         - |  1806 | ` *                                              property updates as scope-aware writes` |
|         - |  1807 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|         - |  1808 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|         - |  1809 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|         - |  1810 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|         - |  1811 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|         - |  1812 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|         - |  1813 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|         - |  1814 | ` */` |
|        22 |  1815 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  1816 | `{` |
|         - |  1817 | `	SyToken *pIn,*pEnd,*pNext;` |
|        24 |  1818 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|        24 |  1819 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|        24 |  1820 | `	int nArg = 0;` |
|         - |  1821 | `	sxi32 rc;` |
|        11 |  1822 | `	SXUNUSED(iCompileFlag);` |
|         - |  1823 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|        24 |  1824 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|        24 |  1825 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|         - |  1826 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|        24 |  1827 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       ! 0 |  1828 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  1829 | `			"clone(...) first-class callable form is not yet supported");` |
|         - |  1830 | `	}` |
|         - |  1831 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|        62 |  1832 | `	while( pIn < pEnd ){` |
|        40 |  1833 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|        40 |  1834 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|       ! 0 |  1835 | `			break;` |
|         - |  1836 | `		}` |
|        40 |  1837 | `		pArgStart = pIn;` |
|        40 |  1838 | `		pArgEnd   = pNext;` |
|         - |  1839 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|         - |  1840 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|        38 |  1841 | `		if( (pArgEnd - pArgStart) >= 2` |
|        37 |  1842 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        23 |  1843 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|         5 |  1844 | `			pName = pArgStart;` |
|         5 |  1845 | `			pArgStart += 2;` |
|         2 |  1846 | `		}` |
|        40 |  1847 | `		if( pName ){` |
|         - |  1848 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|         - |  1849 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|         4 |  1850 | `			if( pName->sData.nByte == sizeof("object")-1` |
|         4 |  1851 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|         3 |  1852 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|         4 |  1853 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|         3 |  1854 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|         3 |  1855 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         2 |  1856 | `			}else{` |
|       ! 0 |  1857 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|       ! 0 |  1858 | `					"Unknown named parameter $%z",&pName->sData);` |
|         1 |  1859 | `			}` |
|        38 |  1860 | `		}else if( nArg == 0 ){` |
|        22 |  1861 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        25 |  1862 | `		}else if( nArg == 1 ){` |
|        15 |  1863 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         8 |  1864 | `		}else{` |
|       ! 0 |  1865 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|         - |  1866 | `				"clone() expects at most 2 arguments");` |
|         - |  1867 | `		}` |
|        40 |  1868 | `		nArg++;` |
|        40 |  1869 | `		pIn = pNext;` |
|        40 |  1870 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 |  1871 | `			pIn++; /* step over the argument separator */` |
|         8 |  1872 | `		}` |
|         2 |  1873 | `	}` |
|        24 |  1874 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|       ! 0 |  1875 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  1876 | `			"clone() expects at least 1 argument, 0 given");` |
|         - |  1877 | `	}` |
|         - |  1878 | `	/* Object argument -> clone (+ __clone()). */` |
|        24 |  1879 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        24 |  1880 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  1881 | `		return SXERR_ABORT;` |
|         - |  1882 | `	}` |
|        24 |  1883 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|         - |  1884 | `	/* Property updates (evaluated after __clone runs). */` |
|        24 |  1885 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|        17 |  1886 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        17 |  1887 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1888 | `			return SXERR_ABORT;` |
|         - |  1889 | `		}` |
|        17 |  1890 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|         8 |  1891 | `	}` |
|        24 |  1892 | `	return SXRET_OK;` |
|        13 |  1893 | `}` |
|         - |  1894 | `/*` |
|         - |  1895 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|         - |  1896 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|         - |  1897 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|         - |  1898 | ` */` |
|    222882 |  1899 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1900 | `{` |
|         - |  1901 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    222887 |  1902 | `	pGen->pIn++;` |
|    222887 |  1903 | `	pGen->pEnd--;` |
|    111441 |  1904 | `	SXUNUSED(iCompileFlag);` |
|    222887 |  1905 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1906 | `}` |
|         - |  1907 | `/*` |
|         - |  1908 | ` * Expression tree validator callback for the 'list' language construct.` |
|         - |  1909 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1910 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1911 | ` * error message.` |
|         - |  1912 | ` * See the routine responible of compiling the list language construct` |
|         - |  1913 | ` * for more inforation.` |
|         - |  1914 | ` */` |
|       210 |  1915 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  1916 | `{` |
|       215 |  1917 | `	sxi32 rc = SXRET_OK;` |
|       215 |  1918 | `	if( pRoot->pOp ){` |
|         4 |  1919 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|         2 |  1920 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|         - |  1921 | `				/* Unexpected expression */` |
|       ! 0 |  1922 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1923 | `					"list(): Expecting a variable not an expression");` |
|       ! 0 |  1924 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 |  1925 | `					rc = SXERR_INVALID;` |
|       ! 0 |  1926 | `				}` |
|         1 |  1927 | `		}` |
|       213 |  1928 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1929 | `		/* Unexpected expression */` |
|         6 |  1930 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1931 | `			"list(): Expecting a variable not an expression");` |
|         6 |  1932 | `		if( rc != SXERR_ABORT ){` |
|         6 |  1933 | `			rc = SXERR_INVALID;` |
|         2 |  1934 | `		}` |
|         2 |  1935 | `	}` |
|       215 |  1936 | `	return rc;` |
|         5 |  1937 | `}` |
|         - |  1938 | `/*` |
|         - |  1939 | ` * Compile the 'list' language construct.` |
|         - |  1940 | ` *  According to the PHP language reference` |
|         - |  1941 | ` *  list(): Assign variables as if they were an array.` |
|         - |  1942 | ` *  list() is used to assign a list of variables in one operation.` |
|         - |  1943 | ` *  Description` |
|         - |  1944 | ` *   array list (mixed $varname [, mixed $... ] )` |
|         - |  1945 | ` *   Like array(), this is not really a function, but a language construct.` |
|         - |  1946 | ` *   list() is used to assign a list of variables in one operation.` |
|         - |  1947 | ` *  Parameters` |
|         - |  1948 | ` *   $varname: A variable.` |
|         - |  1949 | ` *  Return Values` |
|         - |  1950 | ` *   The assigned array.` |
|         - |  1951 | ` */` |
|         - |  1952 | `/* Nested list entry recorded during first pass of list body compilation */` |
|         - |  1953 | `struct NestedListEntry {` |
|         - |  1954 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|         - |  1955 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|         - |  1956 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|         - |  1957 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|         - |  1958 | `};` |
|         - |  1959 | `/*` |
|         - |  1960 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|         - |  1961 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|         - |  1962 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|         - |  1963 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|         - |  1964 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|         - |  1965 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|         - |  1966 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|         - |  1967 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|         - |  1968 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|         - |  1969 | ` */` |
|        22 |  1970 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|         1 |  1971 | `{` |
|         - |  1972 | `	SyToken *pNext;` |
|         - |  1973 | `	sxi32 rc;` |
|        53 |  1974 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|         - |  1975 | `		SyToken *pArrow,*pTarget;` |
|         - |  1976 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|        31 |  1977 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|        31 |  1978 | `		pTarget = &pArrow[1];` |
|        31 |  1979 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|         - |  1980 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|         - |  1981 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|       ! 0 |  1982 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  1983 | `				"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  1984 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  1985 | `		}` |
|         - |  1986 | `		/* DUP the source array (it is on the stack top) */` |
|        31 |  1987 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  1988 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|        31 |  1989 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|        31 |  1990 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1991 | `			return SXERR_ABORT;` |
|         - |  1992 | `		}` |
|         - |  1993 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|         - |  1994 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|         - |  1995 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|         - |  1996 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|         - |  1997 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|         - |  1998 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|        31 |  1999 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|        31 |  2000 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|        28 |  2001 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|        15 |  2002 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|         - |  2003 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|         - |  2004 | `			 * Treat source[key] as the inner body's source, then drop the` |
|         - |  2005 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|         5 |  2006 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|         5 |  2007 | `			SyToken *pSavedIn = pGen->pIn;` |
|         5 |  2008 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         5 |  2009 | `			pGen->pIn = pTarget;` |
|         5 |  2010 | `			pGen->pEnd = pNext;` |
|         5 |  2011 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|         2 |  2012 | `			             : PH7_CompileList(&(*pGen),0);` |
|         5 |  2013 | `			pGen->pIn = pSavedIn;` |
|         5 |  2014 | `			pGen->pEnd = pSavedEnd;` |
|         5 |  2015 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2016 | `				return SXERR_ABORT;` |
|         - |  2017 | `			}` |
|         5 |  2018 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         3 |  2019 | `		}else{` |
|         - |  2020 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|         - |  2021 | `			 * is already on the stack as the value; compiling the target appends` |
|         - |  2022 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|         - |  2023 | `			 * assignment does. */` |
|         - |  2024 | `			VmInstr *pInstr;` |
|        27 |  2025 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|        27 |  2026 | `			sxi32 iP1 = 0, iP2 = 0;` |
|        27 |  2027 | `			void *p3 = 0;` |
|        27 |  2028 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|         - |  2029 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|        27 |  2030 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2031 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2032 | `			}` |
|        27 |  2033 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|        27 |  2034 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|         3 |  2035 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|        26 |  2036 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         3 |  2037 | `					iVmOp = PH7_OP_STORE_IDX;` |
|         3 |  2038 | `					iP1 = pInstr->iP1;` |
|         3 |  2039 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         2 |  2040 | `				}else{` |
|        23 |  2041 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|        23 |  2042 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - |  2043 | `				}` |
|        13 |  2044 | `			}` |
|        27 |  2045 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|         - |  2046 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|         - |  2047 | `			 * source array is back on top for the next entry. */` |
|        27 |  2048 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         - |  2049 | `		}` |
|        31 |  2050 | `		pGen->pIn = &pNext[1];` |
|         1 |  2051 | `	}` |
|        23 |  2052 | `	return SXRET_OK;` |
|        12 |  2053 | `}` |
|         - |  2054 | `/*` |
|         - |  2055 | ` * Shared body for list() and short list [...] compilation.` |
|         - |  2056 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|         - |  2057 | ` * the opening delimiter and before the closing delimiter.` |
|         - |  2058 | ` */` |
|       122 |  2059 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|         5 |  2060 | `{` |
|         - |  2061 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|         - |  2062 | `	SyToken *pNext;` |
|         - |  2063 | `	SyToken *pClassifyIn;` |
|       127 |  2064 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|         - |  2065 | `	sxi32 nExpr;` |
|         - |  2066 | `	sxi32 rc;` |
|         - |  2067 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|         - |  2068 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|         - |  2069 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|         - |  2070 | `	 * list. */` |
|       127 |  2071 | `	pClassifyIn = pGen->pIn;` |
|       367 |  2072 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       245 |  2073 | `		if( pGen->pIn >= pNext ){` |
|        13 |  2074 | `			nEmpty++;` |
|       239 |  2075 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|        31 |  2076 | `			nKeyed++;` |
|        16 |  2077 | `		}else{` |
|       203 |  2078 | `			nPositional++;` |
|         - |  2079 | `		}` |
|       245 |  2080 | `		pGen->pIn = &pNext[1];` |
|         5 |  2081 | `	}` |
|       127 |  2082 | `	pGen->pIn = pClassifyIn;` |
|       127 |  2083 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|       ! 0 |  2084 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2085 | `			"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2086 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2087 | `	}` |
|       127 |  2088 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|       ! 0 |  2089 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2090 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|       ! 0 |  2091 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2092 | `	}` |
|       127 |  2093 | `	if( nKeyed > 0 ){` |
|        23 |  2094 | `		return GenStateCompileKeyedListBody(pGen);` |
|         - |  2095 | `	}` |
|       105 |  2096 | `	nExpr = 0;` |
|       105 |  2097 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       315 |  2098 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       215 |  2099 | `		if( pGen->pIn < pNext ){` |
|         - |  2100 | `			/* Check for nested list() */` |
|       203 |  2101 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         3 |  2102 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  2103 | `				/* Record this nested list for post-processing */` |
|         3 |  2104 | `				SyToken *pListEnd = 0;` |
|         3 |  2105 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|         3 |  2106 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         1 |  2107 | `				}` |
|         3 |  2108 | `				if( pListEnd ){` |
|         - |  2109 | `					struct NestedListEntry sEntry;` |
|         3 |  2110 | `					sEntry.nIndex = nExpr;` |
|         3 |  2111 | `					sEntry.pStart = pGen->pIn;` |
|         3 |  2112 | `					sEntry.pEnd = pListEnd + 1;` |
|         3 |  2113 | `					sEntry.isShort = 0;` |
|         3 |  2114 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         1 |  2115 | `				}` |
|         - |  2116 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|         3 |  2117 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       202 |  2118 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  2119 | `				/* Nested short destructuring [...] */` |
|        13 |  2120 | `				SyToken *pBracketEnd = 0;` |
|        13 |  2121 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|        13 |  2122 | `				if( pBracketEnd ){` |
|         - |  2123 | `					struct NestedListEntry sEntry;` |
|        13 |  2124 | `					sEntry.nIndex = nExpr;` |
|        13 |  2125 | `					sEntry.pStart = pGen->pIn;` |
|        13 |  2126 | `					sEntry.pEnd = pBracketEnd + 1;` |
|        13 |  2127 | `					sEntry.isShort = 1;` |
|        13 |  2128 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         6 |  2129 | `				}` |
|         - |  2130 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        13 |  2131 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |  2132 | `			}else{` |
|         - |  2133 | `				/* Compile the expression holding the variable */` |
|       189 |  2134 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       189 |  2135 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  2136 | `					SySetRelease(&sNested);` |
|       ! 0 |  2137 | `					return SXRET_OK;` |
|         - |  2138 | `				}` |
|         - |  2139 | `			}` |
|       104 |  2140 | `		}else{` |
|         - |  2141 | `			/* Empty entry,load NULL */` |
|        13 |  2142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|         - |  2143 | `		}` |
|       215 |  2144 | `		nExpr++;` |
|         - |  2145 | `		/* Advance the stream cursor */` |
|       215 |  2146 | `		pGen->pIn = &pNext[1];` |
|         5 |  2147 | `	}` |
|         - |  2148 | `	/* Emit the LOAD_LIST instruction */` |
|       105 |  2149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|         - |  2150 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|         - |  2151 | `	 * For each nested entry, emit code to extract the sub-array` |
|         - |  2152 | `	 * at the corresponding index and recursively destructure it.` |
|         - |  2153 | `	 */` |
|       105 |  2154 | `	if( SySetUsed(&sNested) > 0 ){` |
|        13 |  2155 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|         - |  2156 | `		sxu32 i;` |
|        27 |  2157 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|        15 |  2158 | `			SyToken *pSavedIn = pGen->pIn;` |
|        15 |  2159 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         - |  2160 | `			ph7_value *pIdx;` |
|         - |  2161 | `			sxu32 nConstIdx;` |
|         - |  2162 | `			/* DUP the source array (it's on stack top) */` |
|        15 |  2163 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2164 | `			/* Push the integer index for this nested entry */` |
|        15 |  2165 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|        15 |  2166 | `			if( pIdx == 0 ){` |
|       ! 0 |  2167 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2168 | `				SySetRelease(&sNested);` |
|       ! 0 |  2169 | `				return SXERR_ABORT;` |
|         - |  2170 | `			}` |
|        15 |  2171 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|        15 |  2172 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|         - |  2173 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|         - |  2174 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|         - |  2175 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|         - |  2176 | `			 */` |
|        15 |  2177 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|         - |  2178 | `			/* Recursively compile the inner list */` |
|        15 |  2179 | `			pGen->pIn = apNested[i].pStart;` |
|        15 |  2180 | `			pGen->pEnd = apNested[i].pEnd;` |
|        15 |  2181 | `			if( apNested[i].isShort ){` |
|        13 |  2182 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  2183 | `			}else{` |
|         3 |  2184 | `				rc = PH7_CompileList(&(*pGen),0);` |
|         - |  2185 | `			}` |
|        15 |  2186 | `			pGen->pIn = pSavedIn;` |
|        15 |  2187 | `			pGen->pEnd = pSavedEnd;` |
|        15 |  2188 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2189 | `				SySetRelease(&sNested);` |
|       ! 0 |  2190 | `				return SXERR_ABORT;` |
|         - |  2191 | `			}` |
|         - |  2192 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|        15 |  2193 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         8 |  2194 | `		}` |
|         6 |  2195 | `	}` |
|       105 |  2196 | `	SySetRelease(&sNested);` |
|         - |  2197 | `	/* Node successfully compiled */` |
|       105 |  2198 | `	return SXRET_OK;` |
|        66 |  2199 | `}` |
|        40 |  2200 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2201 | `{` |
|         - |  2202 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|        45 |  2203 | `	pGen->pIn += 2;` |
|        45 |  2204 | `	pGen->pEnd--;` |
|        20 |  2205 | `	SXUNUSED(iCompileFlag);` |
|        45 |  2206 | `	return GenStateCompileListBody(pGen);` |
|         5 |  2207 | `}` |
|        82 |  2208 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2209 | `{` |
|         - |  2210 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        84 |  2211 | `	pGen->pIn++;` |
|        84 |  2212 | `	pGen->pEnd--;` |
|        41 |  2213 | `	SXUNUSED(iCompileFlag);` |
|        84 |  2214 | `	return GenStateCompileListBody(pGen);` |
|         2 |  2215 | `}` |
|         - |  2216 | `/* Forward declarations */` |
|         - |  2217 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|         - |  2218 | `static int GenStateIsReservedConstant(SyString *pName);` |
|         - |  2219 | `static int GenStateIsReadonly(SyToken *pTok);` |
|         - |  2220 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|         - |  2221 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|         - |  2222 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - |  2223 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|         - |  2224 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|         - |  2225 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|         - |  2226 | `/*` |
|         - |  2227 | ` * Compile an annoynmous function or a closure.` |
|         - |  2228 | ` * According to the PHP language reference` |
|         - |  2229 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  2230 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  2231 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|         - |  2232 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|         - |  2233 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|         - |  2234 | ` *  Example Anonymous function variable assignment example` |
|         - |  2235 | ` * <?php` |
|         - |  2236 | ` * $greet = function($name)` |
|         - |  2237 | ` * {` |
|         - |  2238 | ` *    printf("Hello %s\r\n", $name);` |
|         - |  2239 | ` * };` |
|         - |  2240 | ` * $greet('World');` |
|         - |  2241 | ` * $greet('PHP');` |
|         - |  2242 | ` * ?>` |
|         - |  2243 | ` * Note that the implementation of annoynmous function and closure under` |
|         - |  2244 | ` * PH7 is completely different from the one used by the zend engine.` |
|         - |  2245 | ` */` |
|       466 |  2246 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2247 | `{` |
|       471 |  2248 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2249 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2250 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2251 | `							  * one thread is allowed to compile the script.` |
|         - |  2252 | `						      */` |
|         - |  2253 | `	SyString sName;` |
|       471 |  2254 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2255 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2256 | `	sxu32 nKwLine;` |
|       471 |  2257 | `	sxi32 iFlags = 0;` |
|         - |  2258 | `	sxu32 nLen;` |
|         - |  2259 | `	sxi32 rc;` |
|       233 |  2260 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2261 |  |
|       471 |  2262 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       466 |  2263 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       471 |  2264 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2265 | `		/* Static closure: no $this auto-capture, bind refused */` |
|         9 |  2266 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         9 |  2267 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         4 |  2268 | `	}` |
|       471 |  2269 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       471 |  2270 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2271 | `		pGen->pIn++;` |
|       ! 0 |  2272 | `	}` |
|         - |  2273 | `	/* Generate a unique name */` |
|       471 |  2274 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2275 | `	/* Make sure the generated name is unique */` |
|       471 |  2276 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2277 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2278 | `	}` |
|       471 |  2279 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2280 | `	/* Compile the lambda body */` |
|       471 |  2281 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       471 |  2282 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2283 | `		return SXERR_ABORT;` |
|         - |  2284 | `	}` |
|       471 |  2285 | `	if( pAnnonFunc ){` |
|       471 |  2286 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2287 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2288 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       471 |  2289 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2290 | `			return SXERR_ABORT;` |
|         - |  2291 | `		}` |
|       233 |  2292 | `	}` |
|         - |  2293 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2294 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2295 | `	 * the handler wraps either in a Closure instance. */` |
|       471 |  2296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2297 | `	/* Node successfully compiled */` |
|       471 |  2298 | `	return SXRET_OK;` |
|       238 |  2299 | `}` |
|         - |  2300 | `/*` |
|         - |  2301 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2302 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2303 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2304 | ` */` |
|       204 |  2305 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2306 | `	ph7_gen_state *pGen,` |
|         - |  2307 | `	ph7_vm_func *pFunc,` |
|         - |  2308 | `	const char *zName,` |
|         - |  2309 | `	sxu32 nByte,` |
|         - |  2310 | `	SyString *aShadow,` |
|         - |  2311 | `	sxu32 nShadow)` |
|         2 |  2312 | `{` |
|         - |  2313 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2314 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2315 | `	sxu32 n, nEnv;` |
|         - |  2316 | `	char *zDup;` |
|       206 |  2317 | `	if( nByte == 0 ){` |
|       ! 0 |  2318 | `		return SXRET_OK;` |
|         - |  2319 | `	}` |
|       204 |  2320 | `	if( nByte == sizeof("this")-1` |
|       110 |  2321 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         3 |  2322 | `		return SXRET_OK;` |
|         - |  2323 | `	}` |
|       256 |  2324 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       192 |  2325 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       185 |  2326 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       142 |  2327 | `			return SXRET_OK;` |
|         - |  2328 | `		}` |
|        28 |  2329 | `	}` |
|        63 |  2330 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        63 |  2331 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|        91 |  2332 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2333 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2334 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2335 | `			return SXRET_OK;` |
|         - |  2336 | `		}` |
|        15 |  2337 | `	}` |
|        61 |  2338 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        61 |  2339 | `	if( zDup == 0 ){` |
|       ! 0 |  2340 | `		return SXERR_ABORT;` |
|         - |  2341 | `	}` |
|        61 |  2342 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        61 |  2343 | `	sEnv.iFlags = 0;` |
|        61 |  2344 | `	sEnv.nIdx = SXU32_HIGH;` |
|        61 |  2345 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        61 |  2346 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        61 |  2347 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        61 |  2348 | `	return SXRET_OK;` |
|       104 |  2349 | `}` |
|         - |  2350 | `/*` |
|         - |  2351 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|         - |  2352 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|         - |  2353 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|         - |  2354 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|         - |  2355 | ` */` |
|        56 |  2356 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|         - |  2357 | `	ph7_gen_state *pGen,` |
|         - |  2358 | `	ph7_vm_func *pFunc,` |
|         - |  2359 | `	const char *zIn,` |
|         - |  2360 | `	const char *zEnd,` |
|         - |  2361 | `	SyString *aShadow,` |
|         - |  2362 | `	sxu32 nShadow)` |
|         2 |  2363 | `{` |
|         - |  2364 | `	sxi32 rc;` |
|       370 |  2365 | `	while( zIn < zEnd ){` |
|       314 |  2366 | `		if( zIn[0] == '\\' ){` |
|         5 |  2367 | `			zIn++;` |
|         5 |  2368 | `			if( zIn < zEnd ){` |
|         5 |  2369 | `				zIn++;` |
|         2 |  2370 | `			}` |
|         5 |  2371 | `			continue;` |
|         - |  2372 | `		}` |
|       308 |  2373 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        26 |  2374 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|        24 |  2375 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|         - |  2376 | `			const char *zName;` |
|        26 |  2377 | `			zIn++; /* skip '$' */` |
|        26 |  2378 | `			zName = zIn;` |
|        82 |  2379 | `			while( zIn < zEnd ){` |
|        76 |  2380 | `				unsigned char c = (unsigned char)zIn[0];` |
|        76 |  2381 | `				if( c >= 0xc0 ){` |
|       ! 0 |  2382 | `					zIn++;` |
|       ! 0 |  2383 | `					while( zIn < zEnd` |
|       ! 0 |  2384 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  2385 | `						zIn++;` |
|       ! 0 |  2386 | `					}` |
|       ! 0 |  2387 | `					continue;` |
|         - |  2388 | `				}` |
|        76 |  2389 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        20 |  2390 | `					break;` |
|         - |  2391 | `				}` |
|        58 |  2392 | `				zIn++;` |
|         2 |  2393 | `			}` |
|        26 |  2394 | `			if( zIn > zName ){` |
|        38 |  2395 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|        24 |  2396 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|        26 |  2397 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2398 | `					return SXERR_ABORT;` |
|         - |  2399 | `				}` |
|        12 |  2400 | `			}` |
|        26 |  2401 | `			continue;` |
|         - |  2402 | `		}` |
|       286 |  2403 | `		zIn++;` |
|         2 |  2404 | `	}` |
|        58 |  2405 | `	return SXRET_OK;` |
|        30 |  2406 | `}` |
|         - |  2407 | `/*` |
|         - |  2408 | ` * Scan the body token range of an arrow function for free-variable` |
|         - |  2409 | ` * references and record them in pFunc's closure environment. Handles:` |
|         - |  2410 | ` *   - plain $<id> pairs` |
|         - |  2411 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|         - |  2412 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|         - |  2413 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|         - |  2414 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|         - |  2415 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|         - |  2416 | ` *     are never mistakenly captured.` |
|         - |  2417 | ` */` |
|       304 |  2418 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2419 | `	ph7_gen_state *pGen,` |
|         - |  2420 | `	ph7_vm_func *pFunc,` |
|         - |  2421 | `	SyToken *pStart,` |
|         - |  2422 | `	SyToken *pEnd,` |
|         - |  2423 | `	SyString *aShadow,` |
|         - |  2424 | `	sxu32 nShadow)` |
|         4 |  2425 | `{` |
|       308 |  2426 | `	SyToken *pScan = pStart;` |
|         - |  2427 | `	sxi32 rc;` |
|      1740 |  2428 | `	while( pScan < pEnd ){` |
|      1436 |  2429 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|        86 |  2430 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        28 |  2431 | `				pScan->sData.zString,` |
|        56 |  2432 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        28 |  2433 | `				aShadow,nShadow);` |
|        58 |  2434 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2435 | `				return SXERR_ABORT;` |
|         - |  2436 | `			}` |
|        58 |  2437 | `			pScan++;` |
|        58 |  2438 | `			continue;` |
|         - |  2439 | `		}` |
|      1380 |  2440 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        30 |  2441 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        30 |  2442 | `			SyToken *pFnKw = pScan;` |
|        28 |  2443 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2444 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         2 |  2445 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2446 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2447 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2448 | `			}` |
|        30 |  2449 | `			if( nKw == PH7_TKWRD_FN ){` |
|         - |  2450 | `				SyToken *pInnerSigStart;` |
|         - |  2451 | `				SyToken *pInnerSigEnd;` |
|         - |  2452 | `				SyToken *pInnerBodyEnd;` |
|         - |  2453 | `				SyString *aInnerShadow;` |
|         - |  2454 | `				sxu32 nInnerShadow;` |
|         - |  2455 | `				sxu32 nInnerParamMax;` |
|         - |  2456 | `				SyToken *p;` |
|         - |  2457 | `				int iNestInner;` |
|        19 |  2458 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|        19 |  2459 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2460 | `					pScan++;` |
|       ! 0 |  2461 | `				}` |
|        19 |  2462 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2463 | `					pScan++;` |
|       ! 0 |  2464 | `					continue;` |
|         - |  2465 | `				}` |
|        19 |  2466 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|        19 |  2467 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|         - |  2468 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|        19 |  2469 | `				if( pInnerSigEnd >= pEnd ){` |
|       ! 0 |  2470 | `					pScan = pEnd;` |
|       ! 0 |  2471 | `					continue;` |
|         - |  2472 | `				}` |
|         - |  2473 | `				/* Build an augmented shadow list: inherited + inner params */` |
|        19 |  2474 | `				nInnerParamMax = 0;` |
|        57 |  2475 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2476 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|        13 |  2477 | `						nInnerParamMax++;` |
|         6 |  2478 | `					}` |
|        20 |  2479 | `				}` |
|        19 |  2480 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|        18 |  2481 | `					&pGen->pVm->sAllocator,` |
|        18 |  2482 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|        19 |  2483 | `				if( aInnerShadow == 0 ){` |
|       ! 0 |  2484 | `					return SXERR_ABORT;` |
|         - |  2485 | `				}` |
|        19 |  2486 | `				nInnerShadow = 0;` |
|        25 |  2487 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|         7 |  2488 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|         4 |  2489 | `				}` |
|        57 |  2490 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2491 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|        27 |  2492 | `						continue;` |
|         - |  2493 | `					}` |
|        13 |  2494 | `					if( &p[1] >= pInnerSigEnd ){` |
|       ! 0 |  2495 | `						break;` |
|         - |  2496 | `					}` |
|        13 |  2497 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2498 | `						continue;` |
|         - |  2499 | `					}` |
|        13 |  2500 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|         7 |  2501 | `				}` |
|        19 |  2502 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|        19 |  2503 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|       ! 0 |  2504 | `					pScan++;` |
|       ! 0 |  2505 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|       ! 0 |  2506 | `						&& pScan->sData.nByte == 1` |
|       ! 0 |  2507 | `						&& pScan->sData.zString[0] == '?' ){` |
|       ! 0 |  2508 | `						pScan++;` |
|       ! 0 |  2509 | `					}` |
|       ! 0 |  2510 | `					if( pScan < pEnd` |
|       ! 0 |  2511 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  2512 | `						pScan++;` |
|       ! 0 |  2513 | `					}` |
|       ! 0 |  2514 | `				}` |
|        19 |  2515 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|        19 |  2516 | `					pScan++; /* past '=>' */` |
|         9 |  2517 | `				}` |
|        19 |  2518 | `				pInnerBodyEnd = pScan;` |
|        19 |  2519 | `				iNestInner = 0;` |
|       131 |  2520 | `				while( pInnerBodyEnd < pEnd ){` |
|       113 |  2521 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|         - |  2522 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|         - |  2523 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       ! 0 |  2524 | `						break;` |
|         - |  2525 | `					}` |
|       113 |  2526 | `					if( pInnerBodyEnd->nType &` |
|         - |  2527 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         3 |  2528 | `						iNestInner++;` |
|       112 |  2529 | `					}else if( pInnerBodyEnd->nType &` |
|         - |  2530 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         3 |  2531 | `						iNestInner--;` |
|         1 |  2532 | `					}` |
|       113 |  2533 | `					pInnerBodyEnd++;` |
|         1 |  2534 | `				}` |
|         - |  2535 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|         - |  2536 | `				 * the outer's body: a default value is evaluated at call time` |
|         - |  2537 | `				 * in the outer frame, so any free variable it references is` |
|         - |  2538 | `				 * an outer capture. We must NOT scan the parameter-name` |
|         - |  2539 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|         - |  2540 | `				 * or those names leak into the outer's closure environment.` |
|         - |  2541 | `				 *` |
|         - |  2542 | `				 * Walk the signature argument-by-argument, splitting on` |
|         - |  2543 | `				 * top-level commas, and for each argument scan only the token` |
|         - |  2544 | `				 * range after the '=' sign. */` |
|         - |  2545 | `				{` |
|        19 |  2546 | `					SyToken *pArgStart = pInnerSigStart;` |
|        31 |  2547 | `					while( pArgStart < pInnerSigEnd ){` |
|        13 |  2548 | `						SyToken *pArgEnd = pArgStart;` |
|        13 |  2549 | `						SyToken *pEq = 0;` |
|        13 |  2550 | `						int iNestArg = 0;` |
|        49 |  2551 | `						while( pArgEnd < pInnerSigEnd ){` |
|        38 |  2552 | `							if( iNestArg == 0` |
|        39 |  2553 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|         3 |  2554 | `								break;` |
|         - |  2555 | `							}` |
|        37 |  2556 | `							if( pArgEnd->nType &` |
|         - |  2557 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  2558 | `								iNestArg++;` |
|        37 |  2559 | `							}else if( pArgEnd->nType &` |
|         - |  2560 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  2561 | `								iNestArg--;` |
|       ! 0 |  2562 | `							}` |
|        36 |  2563 | `							if( pEq == 0 && iNestArg == 0` |
|        31 |  2564 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|         7 |  2565 | `								pEq = pArgEnd;` |
|         3 |  2566 | `							}` |
|        37 |  2567 | `							pArgEnd++;` |
|         1 |  2568 | `						}` |
|        13 |  2569 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|        10 |  2570 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         3 |  2571 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|         7 |  2572 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  2573 | `								return SXERR_ABORT;` |
|         - |  2574 | `							}` |
|         3 |  2575 | `						}` |
|        13 |  2576 | `						pArgStart = pArgEnd;` |
|        12 |  2577 | `						if( pArgStart < pInnerSigEnd` |
|         8 |  2578 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|         3 |  2579 | `							pArgStart++;` |
|         1 |  2580 | `						}` |
|         1 |  2581 | `					}` |
|         - |  2582 | `				}` |
|        28 |  2583 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         9 |  2584 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|        19 |  2585 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2586 | `					return SXERR_ABORT;` |
|         - |  2587 | `				}` |
|        19 |  2588 | `				pScan = pInnerBodyEnd;` |
|        19 |  2589 | `				continue;` |
|         - |  2590 | `			}` |
|         5 |  2591 | `		}` |
|      1362 |  2592 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      1182 |  2593 | `			pScan++;` |
|      1182 |  2594 | `			continue;` |
|         - |  2595 | `		}` |
|         - |  2596 | `		{` |
|         - |  2597 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       182 |  2598 | `			SyToken *pDollar = pScan;` |
|       270 |  2599 | `			while( &pDollar[1] < pEnd` |
|       182 |  2600 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2601 | `				pDollar++;` |
|       ! 0 |  2602 | `			}` |
|       182 |  2603 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2604 | `				break;` |
|         - |  2605 | `			}` |
|       182 |  2606 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2607 | `				pScan = pDollar + 1;` |
|       ! 0 |  2608 | `				continue;` |
|         - |  2609 | `			}` |
|       272 |  2610 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       180 |  2611 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|        90 |  2612 | `				aShadow,nShadow);` |
|       182 |  2613 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2614 | `				return SXERR_ABORT;` |
|         - |  2615 | `			}` |
|       182 |  2616 | `			pScan = pDollar + 2;` |
|         - |  2617 | `		}` |
|         2 |  2618 | `	}` |
|       308 |  2619 | `	return SXRET_OK;` |
|       156 |  2620 | `}` |
|         - |  2621 | `/*` |
|         - |  2622 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2623 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2624 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2625 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2626 | ` * $this is also made available.` |
|         - |  2627 | ` */` |
|       286 |  2628 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2629 | `{` |
|         - |  2630 | `	ph7_vm_func *pFunc;` |
|         - |  2631 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2632 | `	GenBlock *pBlock;` |
|         - |  2633 | `	SySet *pInstrContainer;` |
|         - |  2634 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|         - |  2635 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|         - |  2636 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|         - |  2637 | `	SyToken *pSavedEnd;` |
|         - |  2638 | `	ph7_vm_func_arg *aArgs;` |
|         - |  2639 | `	char zName[512];` |
|         - |  2640 | `	static int iCnt = 1;` |
|         - |  2641 | `	char *zDup;` |
|         - |  2642 | `	SyToken *pTokKw;` |
|         - |  2643 | `	sxu32 nLen;` |
|         - |  2644 | `	sxu32 nLine;` |
|       291 |  2645 | `	sxi32 iFlags = 0;` |
|       291 |  2646 | `	int bStatic = 0;` |
|         - |  2647 | `	sxi32 rc;` |
|         - |  2648 | `	sxu32 n;` |
|       143 |  2649 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2650 |  |
|       291 |  2651 | `	nLine = pGen->pIn->nLine;` |
|         - |  2652 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       291 |  2653 | `	pTokKw = pGen->pIn;` |
|         - |  2654 | `	/* Optional 'static' prefix */` |
|       286 |  2655 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       291 |  2656 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  2657 | `		bStatic = 1;` |
|         7 |  2658 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         7 |  2659 | `		pGen->pIn++;` |
|         3 |  2660 | `	}` |
|         - |  2661 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       286 |  2662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       291 |  2663 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2664 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2665 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2666 | `		return SXERR_SYNTAX;` |
|         - |  2667 | `	}` |
|       291 |  2668 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2669 | `	/* Optional '&' — return by reference */` |
|       291 |  2670 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2671 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2672 | `		pGen->pIn++;` |
|       ! 0 |  2673 | `	}` |
|         - |  2674 | `	/* Expect '(' */` |
|       291 |  2675 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  2676 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2677 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2678 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|         2 |  2679 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2680 | `		}else{` |
|       ! 0 |  2681 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2682 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|         - |  2683 | `		}` |
|         3 |  2684 | `		return SXERR_SYNTAX;` |
|         - |  2685 | `	}` |
|       288 |  2686 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2687 | `	/* Delimit the parameter list */` |
|       288 |  2688 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       288 |  2689 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2690 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2691 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2692 | `		return SXERR_SYNTAX;` |
|         - |  2693 | `	}` |
|         - |  2694 | `	/* Allocate the function state */` |
|       286 |  2695 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       286 |  2696 | `	if( pFunc == 0 ){` |
|       ! 0 |  2697 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2698 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2699 | `		return SXERR_ABORT;` |
|         - |  2700 | `	}` |
|         - |  2701 | `	/* Generate a unique lambda name */` |
|       286 |  2702 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       286 |  2703 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2704 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2705 | `	}` |
|       286 |  2706 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       286 |  2707 | `	if( zDup == 0 ){` |
|       ! 0 |  2708 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2709 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2710 | `		return SXERR_ABORT;` |
|         - |  2711 | `	}` |
|       286 |  2712 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2713 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       286 |  2714 | `	pFunc->nLine = nLine;` |
|         - |  2715 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       286 |  2716 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2717 | `		return SXERR_ABORT;` |
|         - |  2718 | `	}` |
|         - |  2719 | `	/* Collect function arguments */` |
|       286 |  2720 | `	if( pGen->pIn < pSigEnd ){` |
|       115 |  2721 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       115 |  2722 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2723 | `			return SXERR_ABORT;` |
|         - |  2724 | `		}` |
|        56 |  2725 | `	}` |
|         - |  2726 | `	/* Point past ')' and parse optional return type */` |
|       286 |  2727 | `	pGen->pIn = &pSigEnd[1];` |
|       286 |  2728 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       286 |  2729 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2730 | `		return SXERR_ABORT;` |
|       286 |  2731 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2732 | `		return SXERR_SYNTAX;` |
|         - |  2733 | `	}` |
|         - |  2734 | `	/* Expect '=>' */` |
|       286 |  2735 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  2736 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2737 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2738 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|         2 |  2739 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2740 | `		}else{` |
|       ! 0 |  2741 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2742 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|         - |  2743 | `		}` |
|         3 |  2744 | `		return SXERR_SYNTAX;` |
|         - |  2745 | `	}` |
|       284 |  2746 | `	pGen->pIn++; /* Jump '=>' */` |
|       284 |  2747 | `	pBodyStart = pGen->pIn;` |
|       284 |  2748 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2749 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2750 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2751 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2752 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       284 |  2753 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2754 | `	{` |
|       284 |  2755 | `		SyString *aShadow = 0;` |
|       284 |  2756 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       284 |  2757 | `		if( nShadow > 0 ){` |
|       112 |  2758 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       110 |  2759 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       112 |  2760 | `			if( aShadow == 0 ){` |
|       ! 0 |  2761 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2762 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2763 | `				return SXERR_ABORT;` |
|         - |  2764 | `			}` |
|       256 |  2765 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       146 |  2766 | `				aShadow[n] = aArgs[n].sName;` |
|        74 |  2767 | `			}` |
|        55 |  2768 | `		}` |
|       424 |  2769 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       140 |  2770 | `			aShadow,nShadow);` |
|       284 |  2771 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2772 | `			return SXERR_ABORT;` |
|         - |  2773 | `		}` |
|         - |  2774 | `	}` |
|         - |  2775 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2776 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2777 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2778 | `	 * $this. */` |
|       284 |  2779 | `	if( !bStatic ){` |
|         - |  2780 | `		char *zThisDup;` |
|       278 |  2781 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       278 |  2782 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2783 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2784 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2785 | `			return SXERR_ABORT;` |
|         - |  2786 | `		}` |
|       278 |  2787 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       278 |  2788 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       278 |  2789 | `		sEnv.nIdx = SXU32_HIGH;` |
|       278 |  2790 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       278 |  2791 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       278 |  2792 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       137 |  2793 | `	}` |
|         - |  2794 | `	/* Arrow functions are always closures */` |
|       284 |  2795 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2796 | `	/* Compile the body expression as an implicit return */` |
|       424 |  2797 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       140 |  2798 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       284 |  2799 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2800 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2801 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2802 | `		return SXERR_ABORT;` |
|         - |  2803 | `	}` |
|       284 |  2804 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       284 |  2805 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       284 |  2806 | `	pSavedEnd = pGen->pEnd;` |
|       284 |  2807 | `	pGen->pIn = pBodyStart;` |
|       284 |  2808 | `	pGen->pEnd = pBodyEnd;` |
|       284 |  2809 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       284 |  2810 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2811 | `		return SXERR_ABORT;` |
|         - |  2812 | `	}` |
|         - |  2813 | `	/* The cursor stopped just past the body expression */` |
|       284 |  2814 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2815 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2816 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2817 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2818 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       284 |  2819 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       284 |  2820 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       284 |  2821 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       284 |  2822 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       284 |  2823 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2824 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       284 |  2825 | `	pGen->pIn = pBodyEnd;` |
|       284 |  2826 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2827 | `	/* Emit the load-closure instruction */` |
|       284 |  2828 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       284 |  2829 | `	return SXRET_OK;` |
|       148 |  2830 | `}` |
|         - |  2831 | `/*` |
|         - |  2832 | ` * Compile a single arm's expression range into a freshly-allocated` |
|         - |  2833 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|         - |  2834 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|         - |  2835 | ` * expression's value.` |
|         - |  2836 | ` */` |
|       354 |  2837 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|         - |  2838 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|         3 |  2839 | `{` |
|         - |  2840 | `	SySet *pInstrContainer;` |
|         - |  2841 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  2842 | `	GenBlock *pArmBlock;` |
|         - |  2843 | `	sxi32 rc;` |
|       357 |  2844 | `	pTmpIn  = pGen->pIn;` |
|       357 |  2845 | `	pTmpEnd = pGen->pEnd;` |
|       357 |  2846 | `	pGen->pIn  = pStart;` |
|       357 |  2847 | `	pGen->pEnd = pStop;` |
|       357 |  2848 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       357 |  2849 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|         - |  2850 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|         - |  2851 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|         - |  2852 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|         - |  2853 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|         - |  2854 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|       534 |  2855 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       177 |  2856 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|       357 |  2857 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2858 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  2859 | `		pGen->pIn  = pTmpIn;` |
|       ! 0 |  2860 | `		pGen->pEnd = pTmpEnd;` |
|       ! 0 |  2861 | `		return SXERR_ABORT;` |
|         - |  2862 | `	}` |
|       357 |  2863 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       357 |  2864 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       357 |  2865 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       357 |  2866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       357 |  2867 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       357 |  2868 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       357 |  2869 | `	pGen->pIn  = pTmpIn;` |
|       357 |  2870 | `	pGen->pEnd = pTmpEnd;` |
|       357 |  2871 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2872 | `		return SXERR_ABORT;` |
|         - |  2873 | `	}` |
|       357 |  2874 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 |  2875 | `		return SXERR_EMPTY;` |
|         - |  2876 | `	}` |
|       357 |  2877 | `	return SXRET_OK;` |
|       180 |  2878 | `}` |
|         - |  2879 | `/*` |
|         - |  2880 | ` * Compile a PHP 8.0 match expression:` |
|         - |  2881 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|         - |  2882 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|         - |  2883 | ` * Strict comparison (===) is used between the subject and each condition.` |
|         - |  2884 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|         - |  2885 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|         - |  2886 | ` */` |
|         - |  2887 | `/*` |
|         - |  2888 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|         - |  2889 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|         - |  2890 | ` * caller can bail out of the current expression.` |
|         - |  2891 | ` */` |
|         2 |  2892 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|         1 |  2893 | `{` |
|         - |  2894 | `	va_list ap;` |
|         - |  2895 | `	sxi32 rc;` |
|         - |  2896 | `	SyBlob sMsg;` |
|         3 |  2897 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|         3 |  2898 | `	va_start(ap,zFmt);` |
|         3 |  2899 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|         3 |  2900 | `	va_end(ap);` |
|         3 |  2901 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|         3 |  2902 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|         3 |  2903 | `	SyBlobRelease(&sMsg);` |
|         3 |  2904 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2905 | `		return SXERR_ABORT;` |
|         - |  2906 | `	}` |
|         3 |  2907 | `	return SXERR_SYNTAX;` |
|         2 |  2908 | `}` |
|         - |  2909 | `/*` |
|         - |  2910 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|         - |  2911 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|         - |  2912 | ` * Returns the stop token pointer (or pEnd if none found).` |
|         - |  2913 | ` */` |
|       356 |  2914 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|         4 |  2915 | `{` |
|       360 |  2916 | `	SyToken *pCur = pStart;` |
|       360 |  2917 | `	int iNest = 0;` |
|       838 |  2918 | `	while( pCur < pEnd ){` |
|       802 |  2919 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        13 |  2920 | `			iNest++;` |
|       796 |  2921 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        13 |  2922 | `			iNest--;` |
|       784 |  2923 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|       323 |  2924 | `			return pCur;` |
|         - |  2925 | `		}` |
|       482 |  2926 | `		pCur++;` |
|         4 |  2927 | `	}` |
|        39 |  2928 | `	return pEnd;` |
|       182 |  2929 | `}` |
|        72 |  2930 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2931 | `{` |
|         - |  2932 | `	ph7_match *pMatch;` |
|         - |  2933 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|        77 |  2934 | `	int bHasDefault = 0;` |
|         - |  2935 | `	sxu32 nLine;` |
|         - |  2936 | `	sxi32 rc;` |
|        36 |  2937 | `	SXUNUSED(iCompileFlag);` |
|        77 |  2938 | `	nLine = pGen->pIn->nLine;` |
|        77 |  2939 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|         - |  2940 | `	/* Expect '(' */` |
|        77 |  2941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2942 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2943 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|       ! 0 |  2944 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|         - |  2945 | `	}` |
|        77 |  2946 | `	pGen->pIn++; /* Jump '(' */` |
|        77 |  2947 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|        77 |  2948 | `	if( pSubjEnd >= pGen->pEnd ){` |
|       ! 0 |  2949 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2950 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         - |  2951 | `	}` |
|        77 |  2952 | `	if( pGen->pIn >= pSubjEnd ){` |
|       ! 0 |  2953 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2954 | `			"syntax error, unexpected \")\", expecting match subject");` |
|         - |  2955 | `	}` |
|         - |  2956 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|        77 |  2957 | `	pSavedEnd = pGen->pEnd;` |
|        77 |  2958 | `	pGen->pEnd = pSubjEnd;` |
|        77 |  2959 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        77 |  2960 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2961 | `		return SXERR_ABORT;` |
|         - |  2962 | `	}` |
|        77 |  2963 | `	pGen->pEnd = pSavedEnd;` |
|        77 |  2964 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|         - |  2965 | `	/* Expect '{' */` |
|        77 |  2966 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 |  2967 | `		return GenStateMatchError(pGen,` |
|       ! 0 |  2968 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  2969 | `			"syntax error, expecting \"{\" after match subject");` |
|         - |  2970 | `	}` |
|        77 |  2971 | `	pGen->pIn++; /* Jump '{' */` |
|        77 |  2972 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|        77 |  2973 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  2974 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2975 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|         - |  2976 | `	}` |
|         - |  2977 | `	/* Allocate ph7_match container */` |
|        77 |  2978 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|        77 |  2979 | `	if( pMatch == 0 ){` |
|       ! 0 |  2980 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2981 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2982 | `		return SXERR_ABORT;` |
|         - |  2983 | `	}` |
|        77 |  2984 | `	SyZero(pMatch,sizeof(ph7_match));` |
|        77 |  2985 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|         - |  2986 | `	/* Iterate arms */` |
|       259 |  2987 | `	while( pGen->pIn < pBodyEnd ){` |
|         - |  2988 | `		ph7_match_arm sArm;` |
|         - |  2989 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|       190 |  2990 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|       190 |  2991 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|       190 |  2992 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|       190 |  2993 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  2994 | `		/* 'default' arm? */` |
|       186 |  2995 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       107 |  2996 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|        22 |  2997 | `			if( bHasDefault ){` |
|         3 |  2998 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|         - |  2999 | `					"Match expressions may only contain one default arm");` |
|         4 |  3000 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  3001 | `			}` |
|        20 |  3002 | `			sArm.bDefault = 1;` |
|        20 |  3003 | `			bHasDefault = 1;` |
|        20 |  3004 | `			pGen->pIn++;` |
|        20 |  3005 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       ! 0 |  3006 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3007 | `					"syntax error, expecting \"=>\" after 'default'");` |
|         - |  3008 | `			}` |
|        20 |  3009 | `			pGen->pIn++; /* Jump '=>' */` |
|        11 |  3010 | `		}else{` |
|         - |  3011 | `			/* Condition list: cond (',' cond)* '=>' */` |
|       170 |  3012 | `			pCondStart = pGen->pIn;` |
|       170 |  3013 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|         - |  3014 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       178 |  3015 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|         - |  3016 | `				SySet sCondBc;` |
|         9 |  3017 | `				if( pCondStart >= pArrow ){` |
|       ! 0 |  3018 | `					return GenStateMatchError(pGen,nArmLine,` |
|         - |  3019 | `						"syntax error, empty match condition expression");` |
|         - |  3020 | `				}` |
|         9 |  3021 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         9 |  3022 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|         9 |  3023 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3024 | `					return SXERR_ABORT;` |
|         - |  3025 | `				}` |
|         9 |  3026 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         9 |  3027 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|         9 |  3028 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|         - |  3029 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|         1 |  3030 | `			}` |
|       170 |  3031 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  3032 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3033 | `					"syntax error, expecting \"=>\" in match arm");` |
|         - |  3034 | `			}` |
|       167 |  3035 | `			if( pCondStart >= pArrow ){` |
|       ! 0 |  3036 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3037 | `					"syntax error, empty match condition expression");` |
|         - |  3038 | `			}` |
|         - |  3039 | `			{` |
|         - |  3040 | `				SySet sCondBc;` |
|       167 |  3041 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       167 |  3042 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       167 |  3043 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3044 | `					return SXERR_ABORT;` |
|         - |  3045 | `				}` |
|       167 |  3046 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         - |  3047 | `			}` |
|       167 |  3048 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|         - |  3049 | `		}` |
|         - |  3050 | `		/* Compile result expression: up to top-level ',' or body end */` |
|       185 |  3051 | `		pResStart = pGen->pIn;` |
|       185 |  3052 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|       185 |  3053 | `		if( pResStart >= pResEnd ){` |
|       ! 0 |  3054 | `			return GenStateMatchError(pGen,nArmLine,` |
|         - |  3055 | `				"syntax error, expected expression after \"=>\"");` |
|         - |  3056 | `		}` |
|       185 |  3057 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|       185 |  3058 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3059 | `			return SXERR_ABORT;` |
|         - |  3060 | `		}` |
|       185 |  3061 | `		pGen->pIn = pResEnd;` |
|       185 |  3062 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       151 |  3063 | `			pGen->pIn++; /* Skip trailing ',' */` |
|        74 |  3064 | `		}` |
|       185 |  3065 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|         3 |  3066 | `	}` |
|        71 |  3067 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|        71 |  3068 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|        71 |  3069 | `	return SXRET_OK;` |
|        41 |  3070 | `}` |
|         - |  3071 | `/*` |
|         - |  3072 | ` * Compile a backtick quoted string.` |
|         - |  3073 | ` */` |
|         4 |  3074 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  3075 | `{` |
|         - |  3076 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|         - |  3077 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|         - |  3078 | `	 */` |
|         8 |  3079 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|         - |  3080 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|         2 |  3081 | `		ph7_lib_version()` |
|         - |  3082 | `		);` |
|         - |  3083 | `	/* Load NULL */` |
|         6 |  3084 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         2 |  3085 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  3086 | `	/* Node successfully compiled */` |
|         6 |  3087 | `	return SXRET_OK;` |
|         2 |  3088 | `}` |
|         - |  3089 | `/*` |
|         - |  3090 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|         - |  3091 | ` * construct.` |
|         - |  3092 | ` */` |
|        82 |  3093 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3094 | `{` |
|         - |  3095 | `	SyString *pName;` |
|         - |  3096 | `	sxu32 nKeyID;` |
|         - |  3097 | `	sxi32 rc;` |
|         - |  3098 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        87 |  3099 | `	pName = &pGen->pIn->sData;` |
|        87 |  3100 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        87 |  3101 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        87 |  3102 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|         9 |  3103 | `		SyToken *pTmp,*pNext = 0;` |
|         - |  3104 | `		/* Compile arguments one after one */` |
|         9 |  3105 | `		pTmp = pGen->pEnd;` |
|         - |  3106 | `		/* Symisc eXtension to the PHP programming language:` |
|         - |  3107 | `		 * 'echo' can be used in the context of a function which` |
|         - |  3108 | `		 *  mean that the following expression is valid:` |
|         - |  3109 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|         - |  3110 | `		 */` |
|         9 |  3111 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        17 |  3112 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|         9 |  3113 | `			if( pGen->pIn < pNext ){` |
|         9 |  3114 | `				pGen->pEnd = pNext;` |
|         9 |  3115 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|         9 |  3116 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3117 | `					return SXERR_ABORT;` |
|         - |  3118 | `				}` |
|         9 |  3119 | `				if( rc != SXERR_EMPTY ){` |
|         - |  3120 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|         - |  3121 | `					 * without the overhead of a function call.` |
|         - |  3122 | `					 * This is a very powerful optimization that improve` |
|         - |  3123 | `					 * performance greatly.` |
|         - |  3124 | `					 */` |
|         9 |  3125 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|         4 |  3126 | `				}` |
|         4 |  3127 | `			}` |
|         - |  3128 | `			/* Jump trailing commas */` |
|         9 |  3129 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|       ! 0 |  3130 | `				pNext++;` |
|       ! 0 |  3131 | `			}` |
|         9 |  3132 | `			pGen->pIn = pNext;` |
|         1 |  3133 | `		}` |
|         - |  3134 | `		/* Restore token stream */` |
|         9 |  3135 | `		pGen->pEnd = pTmp;` |
|         5 |  3136 | `	}else{` |
|        79 |  3137 | `		sxi32 nArg = 0;` |
|        79 |  3138 | `		sxu32 nIdx = 0;` |
|        79 |  3139 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        79 |  3140 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3141 | `			return SXERR_ABORT;` |
|        79 |  3142 | `		}else if(rc != SXERR_EMPTY ){` |
|        79 |  3143 | `			nArg = 1;` |
|        37 |  3144 | `		}` |
|        79 |  3145 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3146 | `			ph7_value *pObj;` |
|         - |  3147 | `			/* Emit the call instruction */` |
|        31 |  3148 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        31 |  3149 | `			if( pObj == 0 ){` |
|       ! 0 |  3150 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3151 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3152 | `				return SXERR_ABORT;` |
|         - |  3153 | `			}` |
|        31 |  3154 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3155 | `			/* Install in the literal table */` |
|        31 |  3156 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        13 |  3157 | `		}` |
|         - |  3158 | `		/* Emit the call instruction */` |
|        79 |  3159 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        79 |  3160 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3161 | `	}` |
|         - |  3162 | `	/* Node successfully compiled */` |
|        87 |  3163 | `	return SXRET_OK;` |
|        46 |  3164 | `}` |
|         - |  3165 | `/*` |
|         - |  3166 | ` * Compile a node holding a variable declaration.` |
|         - |  3167 | ` * According to the PHP language reference` |
|         - |  3168 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|         - |  3169 | ` *  The variable name is case-sensitive.` |
|         - |  3170 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|         - |  3171 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3172 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|         - |  3173 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|         - |  3174 | ` *  Note: $this is a special variable that can't be assigned.` |
|         - |  3175 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|         - |  3176 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|         - |  3177 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|         - |  3178 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|         - |  3179 | ` *  the chapter on Expressions.` |
|         - |  3180 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|         - |  3181 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|         - |  3182 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|         - |  3183 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|         - |  3184 | ` *  is being assigned (the source variable).` |
|         - |  3185 | ` */` |
|  16526720 |  3186 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3187 | `{` |
|  16526725 |  3188 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3189 | `	sxi32 iVv;` |
|         - |  3190 | `	sxi32 iP1;` |
|         - |  3191 | `	void *p3;` |
|         - |  3192 | `	sxi32 rc;` |
|  16526725 |  3193 | `	iVv = -1; /* Variable variable counter */` |
|  33053457 |  3194 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  16526737 |  3195 | `		pGen->pIn++;` |
|  16526737 |  3196 | `		iVv++;` |
|         5 |  3197 | `	}` |
|  16526725 |  3198 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3199 | `		/* Invalid variable name */` |
|       ! 0 |  3200 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|       ! 0 |  3201 | `		if( rc == SXERR_ABORT ){` |
|         - |  3202 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3203 | `			return SXERR_ABORT;` |
|         - |  3204 | `		}` |
|       ! 0 |  3205 | `		return SXRET_OK;` |
|         - |  3206 | `	}` |
|  16526725 |  3207 | `	p3  = 0;` |
|  16526725 |  3208 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|         - |  3209 | `		/* Dynamic variable creation */` |
|        21 |  3210 | `		pGen->pIn++;  /* Jump the open curly */` |
|        21 |  3211 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|        21 |  3212 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3213 | `			/* Empty expression */` |
|         3 |  3214 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|         3 |  3215 | `			return SXRET_OK;` |
|         - |  3216 | `		}` |
|         - |  3217 | `		/* Compile the expression holding the variable name */` |
|        18 |  3218 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        18 |  3219 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3220 | `			return SXERR_ABORT;` |
|        18 |  3221 | `		}else if( rc == SXERR_EMPTY ){` |
|         3 |  3222 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|         3 |  3223 | `			return SXRET_OK;` |
|         - |  3224 | `		}` |
|         8 |  3225 | `	}else{` |
|         - |  3226 | `		SyHashEntry *pEntry;` |
|         - |  3227 | `		SyString *pName;` |
|  16526707 |  3228 | `		char *zName = 0;` |
|         - |  3229 | `		/* Extract variable name */` |
|  16526707 |  3230 | `		pName = &pGen->pIn->sData;` |
|         - |  3231 | `		/* Advance the stream cursor */` |
|  16526707 |  3232 | `		pGen->pIn++;` |
|  16526707 |  3233 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  16526707 |  3234 | `		if( pEntry == 0 ){` |
|         - |  3235 | `			/* Duplicate name */` |
|    954317 |  3236 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    954317 |  3237 | `			if( zName == 0 ){` |
|       ! 0 |  3238 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3239 | `				return SXERR_ABORT;` |
|         - |  3240 | `			}` |
|         - |  3241 | `			/* Install in the hashtable */` |
|    954317 |  3242 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    477161 |  3243 | `		}else{` |
|         - |  3244 | `			/* Name already available */` |
|  15572395 |  3245 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3246 | `		}` |
|  16526707 |  3247 | `		p3 = (void *)zName;` |
|         - |  3248 | `	}` |
|  16526721 |  3249 | `	iP1 = 0;` |
|  16526721 |  3250 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   4920927 |  3251 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3252 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   4916953 |  3253 | `			iP1 = 1;` |
|   2458474 |  3254 | `		}` |
|   2460461 |  3255 | `	}` |
|         - |  3256 | `	/* Emit the load instruction */` |
|  16526721 |  3257 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  16526733 |  3258 | `	while( iVv > 0 ){` |
|        13 |  3259 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3260 | `		iVv--;` |
|         1 |  3261 | `	}` |
|         - |  3262 | `	/* Node successfully compiled */` |
|  16526721 |  3263 | `	return SXRET_OK;` |
|   8263365 |  3264 | `}` |
|         - |  3265 | `/*` |
|         - |  3266 | ` * Load a literal.` |
|         - |  3267 | ` */` |
|  11193502 |  3268 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3269 | `{` |
|  11193507 |  3270 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3271 | `	ph7_value *pObj;` |
|         - |  3272 | `	SyString *pStr;` |
|         - |  3273 | `	sxu32 nIdx;` |
|         - |  3274 | `	/* Extract token value */` |
|  11193507 |  3275 | `	pStr = &pToken->sData;` |
|         - |  3276 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  11193507 |  3277 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2158423 |  3278 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3279 | `			/* NULL constant are always indexed at 0 */` |
|    896683 |  3280 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    896683 |  3281 | `			return SXRET_OK;` |
|   1261745 |  3282 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3283 | `			/* TRUE constant are always indexed at 1 */` |
|    285211 |  3284 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    285211 |  3285 | `			return SXRET_OK;` |
|         5 |  3286 | `		}` |
|  10458583 |  3287 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1870454 |  3288 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3289 | `			/* FALSE constant are always indexed at 2 */` |
|    639907 |  3290 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    639907 |  3291 | `			return SXRET_OK;` |
|   8796603 |  3292 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    802832 |  3293 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3294 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     11849 |  3295 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     11849 |  3296 | `			if( pObj == 0 ){` |
|       ! 0 |  3297 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3298 | `				return SXERR_ABORT;` |
|         - |  3299 | `			}` |
|     11849 |  3300 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3301 | `			/* Emit the load constant instruction */` |
|     11849 |  3302 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     11849 |  3303 | `			return SXRET_OK;` |
|   8478457 |  3304 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    190228 |  3305 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3306 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         8 |  3307 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         8 |  3308 | `			if( pObj == 0 ){` |
|       ! 0 |  3309 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3310 | `				return SXERR_ABORT;` |
|         - |  3311 | `			}` |
|         8 |  3312 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3313 | `				SyString sNs;` |
|         8 |  3314 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  3315 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         5 |  3316 | `			}else{` |
|       ! 0 |  3317 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3318 | `			}` |
|         8 |  3319 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         8 |  3320 | `			return SXRET_OK;` |
|   8482597 |  3321 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    391505 |  3322 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   8576295 |  3323 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    385940 |  3324 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3325 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3326 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3327 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3328 | `				/* Point to the upper block */` |
|        11 |  3329 | `				pBlock = pBlock->pParent;` |
|         1 |  3330 | `			}` |
|        11 |  3331 | `			if( pBlock == 0 ){` |
|         - |  3332 | `				/* Called in the global scope,load NULL */` |
|         5 |  3333 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3334 | `			}else{` |
|         - |  3335 | `				/* Extract the target function/method */` |
|         7 |  3336 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3337 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|         - |  3338 | `					/* Not a class method,Load null */` |
|         3 |  3339 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         2 |  3340 | `				}else{` |
|         5 |  3341 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         5 |  3342 | `					if( pObj == 0 ){` |
|       ! 0 |  3343 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3344 | `						return SXERR_ABORT;` |
|         - |  3345 | `					}` |
|         5 |  3346 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3347 | `					/* Emit the load constant instruction */` |
|         5 |  3348 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3349 | `				}` |
|         - |  3350 | `			}` |
|        11 |  3351 | `			return SXRET_OK;` |
|         - |  3352 | `	}` |
|         - |  3353 | `	/* Query literal table */` |
|   9359861 |  3354 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3355 | `		ph7_value *pLitObj;` |
|         - |  3356 | `		/* Unknown literal,install it in the literal table */` |
|   1793881 |  3357 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1793881 |  3358 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3359 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3360 | `			return SXERR_ABORT;` |
|         - |  3361 | `		}` |
|   1793881 |  3362 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1793881 |  3363 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    896938 |  3364 | `	}` |
|         - |  3365 | `	/* Emit the load constant instruction */` |
|   9359861 |  3366 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9359861 |  3367 | `	return SXRET_OK;` |
|   5596756 |  3368 | `}` |
|         - |  3369 | `/*` |
|         - |  3370 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3371 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3372 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3373 | ` * Otherwise, load the simple literal directly.` |
|         - |  3374 | ` */` |
|  11197496 |  3375 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3376 | `{` |
|         - |  3377 | `	sxi32 rc;` |
|  11197501 |  3378 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3379 | `		return SXRET_OK;` |
|         - |  3380 | `	}` |
|         - |  3381 | `	/* Check if this is a multi-token namespace path */` |
|  11197501 |  3382 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3383 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3999 |  3384 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3999 |  3385 | `		int isAbsolute = 0;` |
|      3999 |  3386 | `		SyBlobReset(pWorker);` |
|         - |  3387 | `		/* Check for leading backslash (absolute path) */` |
|      3999 |  3388 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3997 |  3389 | `			isAbsolute = 1;` |
|      3997 |  3390 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1996 |  3391 | `		}` |
|         - |  3392 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3999 |  3393 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3394 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3395 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3396 | `		}` |
|         - |  3397 | `		/* Collect all path components */` |
|      4107 |  3398 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4107 |  3399 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        59 |  3400 | `				SyBlobAppend(pWorker,"\\",1);` |
|        32 |  3401 | `			}else{` |
|      4053 |  3402 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3403 | `			}` |
|      4107 |  3404 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3999 |  3405 | `				pGen->pIn++;` |
|      3999 |  3406 | `				break;` |
|         - |  3407 | `			}` |
|       113 |  3408 | `			pGen->pIn++;` |
|         5 |  3409 | `		}` |
|      3999 |  3410 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3411 | `			ph7_value *pObj;` |
|         - |  3412 | `			SyString sPath;` |
|         - |  3413 | `			sxu32 nIdx;` |
|      3999 |  3414 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3415 | `			/* Install in the literal table */` |
|      3999 |  3416 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3969 |  3417 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3969 |  3418 | `				if( pObj == 0 ){` |
|       ! 0 |  3419 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3420 | `					return SXERR_ABORT;` |
|         - |  3421 | `				}` |
|      3969 |  3422 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3969 |  3423 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1982 |  3424 | `			}` |
|         - |  3425 | `			/* Emit the load constant instruction.` |
|         - |  3426 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3427 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5996 |  3428 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1997 |  3429 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1997 |  3430 | `				nIdx,0,0);` |
|      3999 |  3431 | `			return SXRET_OK;` |
|         - |  3432 | `		}` |
|       ! 0 |  3433 | `	}` |
|         - |  3434 | `	/* Single-token literal: load directly */` |
|  11193507 |  3435 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11193507 |  3436 | `	return rc;` |
|   5598753 |  3437 | `}` |
|         - |  3438 | `/*` |
|         - |  3439 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3440 | ` */` |
|         - |  3441 | `/*` |
|         - |  3442 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3443 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3444 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3445 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3446 | ` */` |
|       ! 0 |  3447 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3448 | `{` |
|       ! 0 |  3449 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3450 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3451 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3452 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3453 | `}` |
|  11197496 |  3454 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3455 | `{` |
|         - |  3456 | `	sxi32 rc;` |
|  11197501 |  3457 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11197501 |  3458 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3459 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3460 | `		return rc;` |
|         - |  3461 | `	}` |
|         - |  3462 | `	/* Node successfully compiled */` |
|  11197501 |  3463 | `	return SXRET_OK;` |
|   5598753 |  3464 | `}` |
|         - |  3465 | `/*` |
|         - |  3466 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3467 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3468 | ` */` |
|         8 |  3469 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3470 | `{` |
|         - |  3471 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3472 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3473 | `		pGen->pIn++;` |
|         1 |  3474 | `	}` |
|         9 |  3475 | `	return SXRET_OK;` |
|         1 |  3476 | `}` |
|         - |  3477 | `/*` |
|         - |  3478 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3479 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3480 | ` */` |
|    300116 |  3481 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3482 | `{` |
|    300121 |  3483 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3995 |  3484 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3485 | `			return TRUE;` |
|      3993 |  3486 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3487 | `			return TRUE;` |
|         5 |  3488 | `		}` |
|    298123 |  3489 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7917 |  3490 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3491 | `			return TRUE;` |
|         - |  3492 | `		}` |
|      3955 |  3493 | `	}` |
|         - |  3494 | `	/* Not a reserved constant */` |
|    300113 |  3495 | `	return FALSE;` |
|    150063 |  3496 | `}` |
|         - |  3497 | `/*` |
|         - |  3498 | ` * Compile the 'const' statement.` |
|         - |  3499 | ` * According to the PHP language reference` |
|         - |  3500 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3501 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3502 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3503 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3504 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3505 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3506 | ` *  Syntax` |
|         - |  3507 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3508 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3509 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3510 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3511 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3512 | ` *  to get a list of all defined constants.` |
|         - |  3513 | ` *` |
|         - |  3514 | ` * Symisc eXtension.` |
|         - |  3515 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3516 | ` *  would allow only simple scalar value.` |
|         - |  3517 | ` *  Example` |
|         - |  3518 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3519 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3520 | ` */` |
|        48 |  3521 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3522 | `{` |
|         - |  3523 | `	SySet *pConsCode,*pInstrContainer;` |
|        53 |  3524 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3525 | `	SyString *pName;` |
|         - |  3526 | `	sxi32 rc;` |
|        53 |  3527 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        53 |  3528 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3529 | `		/* Invalid constant name */` |
|         8 |  3530 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|         8 |  3531 | `		if( rc == SXERR_ABORT ){` |
|         - |  3532 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3533 | `			return SXERR_ABORT;` |
|         - |  3534 | `		}` |
|         8 |  3535 | `		goto Synchronize;` |
|         - |  3536 | `	}` |
|         - |  3537 | `	/* Peek constant name */` |
|        47 |  3538 | `	pName = &pGen->pIn->sData;` |
|         - |  3539 | `	/* Make sure the constant name isn't reserved */` |
|        47 |  3540 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3541 | `		/* Reserved constant */` |
|        10 |  3542 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|        10 |  3543 | `		if( rc == SXERR_ABORT ){` |
|         - |  3544 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3545 | `			return SXERR_ABORT;` |
|         - |  3546 | `		}` |
|        10 |  3547 | `		goto Synchronize;` |
|         - |  3548 | `	}` |
|        38 |  3549 | `	pGen->pIn++;` |
|        38 |  3550 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3551 | `		/* Invalid statement*/` |
|         6 |  3552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|         6 |  3553 | `		if( rc == SXERR_ABORT ){` |
|         - |  3554 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3555 | `			return SXERR_ABORT;` |
|         - |  3556 | `		}` |
|         6 |  3557 | `		goto Synchronize;` |
|         - |  3558 | `	}` |
|        32 |  3559 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3560 | `	/* Allocate a new constant value container */` |
|        32 |  3561 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        32 |  3562 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3563 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3564 | `		return SXERR_ABORT;` |
|         - |  3565 | `	}` |
|        32 |  3566 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3567 | `	/* Swap bytecode container */` |
|        32 |  3568 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        32 |  3569 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3570 | `	/* Compile constant value */` |
|        32 |  3571 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3572 | `	/* Emit the done instruction */` |
|        32 |  3573 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        32 |  3574 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        32 |  3575 | `	if( rc == SXERR_ABORT ){` |
|         - |  3576 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3577 | `		return SXERR_ABORT;` |
|         - |  3578 | `	}` |
|        32 |  3579 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3580 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3581 | `	{` |
|         - |  3582 | `		SyBlob sFQN;` |
|         - |  3583 | `		SyString sFQNStr;` |
|        32 |  3584 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        32 |  3585 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        32 |  3586 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        47 |  3587 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        30 |  3588 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        32 |  3589 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3590 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3591 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3592 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3593 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3594 | `			if( pCEntry ){` |
|         5 |  3595 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3596 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3597 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3598 | `					return SXERR_ABORT;` |
|         - |  3599 | `				}` |
|         2 |  3600 | `			}` |
|         2 |  3601 | `		}` |
|        32 |  3602 | `		SyBlobRelease(&sFQN);` |
|         - |  3603 | `	}` |
|        32 |  3604 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3605 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3606 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3607 | `	}` |
|        32 |  3608 | `	return SXRET_OK;` |
|         9 |  3609 | `Synchronize:` |
|         - |  3610 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3611 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        42 |  3612 | `		pGen->pIn++;` |
|         4 |  3613 | `	}` |
|        22 |  3614 | `	return SXRET_OK;` |
|        29 |  3615 | `}` |
|         - |  3616 | `/*` |
|         - |  3617 | ` * Compile the 'continue' statement.` |
|         - |  3618 | ` * According to the PHP language reference` |
|         - |  3619 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3620 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3621 | ` *  iteration.` |
|         - |  3622 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3623 | ` *  the purposes of continue.` |
|         - |  3624 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3625 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3626 | ` *  Note:` |
|         - |  3627 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3628 | ` */` |
|         - |  3629 | `/*` |
|         - |  3630 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3631 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3632 | ` * break/continue crosses a try boundary.` |
|         - |  3633 | ` *` |
|         - |  3634 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3635 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3636 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3637 | ` */` |
|    118534 |  3638 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3639 | `{` |
|    118539 |  3640 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    118539 |  3641 | `	int nInlineTry = 0;` |
|    552845 |  3642 | `	while( pBlock && pBlock != pTarget ){` |
|    434311 |  3643 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3644 | `			if( pBlock->pUserData ){` |
|         - |  3645 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3646 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3647 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3648 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3649 | `				if( pGen->bInGenerator ){` |
|         3 |  3650 | `					nInlineTry++;` |
|         2 |  3651 | `				}else{` |
|         3 |  3652 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3653 | `				}` |
|         4 |  3654 | `			}else{` |
|         - |  3655 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3656 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3657 | `				break;` |
|         - |  3658 | `			}` |
|         2 |  3659 | `		}` |
|    434311 |  3660 | `		pBlock = pBlock->pParent;` |
|         5 |  3661 | `	}` |
|    118539 |  3662 | `	return nInlineTry;` |
|         5 |  3663 | `}` |
|     59240 |  3664 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3665 | `{` |
|         - |  3666 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3667 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3668 | `	sxu32 nLineLocal;` |
|         - |  3669 | `	sxi32 rc;` |
|     59245 |  3670 | `	nLineLocal = pGen->pIn->nLine;` |
|     59245 |  3671 | `	iLevel = 0;` |
|         - |  3672 | `	/* Jump the 'continue' keyword */` |
|     59245 |  3673 | `	pGen->pIn++;` |
|     59245 |  3674 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3675 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3676 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3677 | `		 */` |
|         - |  3678 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3679 | `		char *zAlloc = 0;` |
|         - |  3680 | `		SyString sNum;` |
|        17 |  3681 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3682 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3683 | `			return SXERR_ABORT;` |
|         - |  3684 | `		}` |
|        17 |  3685 | `		if( rc == SXRET_OK ){` |
|        20 |  3686 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3687 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3688 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3689 | `				return SXERR_ABORT;` |
|         - |  3690 | `			}` |
|        14 |  3691 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3692 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3693 | `		}` |
|        17 |  3694 | `		if( iLevel < 2 ){` |
|         3 |  3695 | `			iLevel = 0;` |
|         1 |  3696 | `		}` |
|        17 |  3697 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3698 | `	}` |
|         - |  3699 | `	/* Point to the target loop */` |
|     59245 |  3700 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     59245 |  3701 | `	if( pLoop == 0 ){` |
|         - |  3702 | `		/* Illegal continue */` |
|        12 |  3703 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|        12 |  3704 | `		if( rc == SXERR_ABORT ){` |
|         - |  3705 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3706 | `			return SXERR_ABORT;` |
|         - |  3707 | `		}` |
|         7 |  3708 | `	}else{` |
|     59235 |  3709 | `		sxu32 nInstrIdx = 0;` |
|         - |  3710 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     59235 |  3711 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3712 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3713 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     59235 |  3714 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     59235 |  3715 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3716 | `			/* According to the PHP language reference manual` |
|         - |  3717 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|         - |  3718 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|         - |  3719 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|         - |  3720 | `			 */` |
|         5 |  3721 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3722 | `			if( rc == SXRET_OK ){` |
|         5 |  3723 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3724 | `			}` |
|         3 |  3725 | `		}else{` |
|         - |  3726 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     59231 |  3727 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     59231 |  3728 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  3729 | `				JumpFixup sJumpFix;` |
|         - |  3730 | `				/* Post-continue */` |
|     19747 |  3731 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     19747 |  3732 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     19747 |  3733 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|      9871 |  3734 | `			}` |
|         - |  3735 | `		}` |
|         - |  3736 | `	}` |
|     59245 |  3737 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3738 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3739 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  3740 | `	}` |
|         - |  3741 | `	/* Statement successfully compiled */` |
|     59245 |  3742 | `	return SXRET_OK;` |
|     29625 |  3743 | `}` |
|         - |  3744 | `/*` |
|         - |  3745 | ` * Compile the 'break' statement.` |
|         - |  3746 | ` * According to the PHP language reference` |
|         - |  3747 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  3748 | ` *  structure.` |
|         - |  3749 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  3750 | ` *  enclosing structures are to be broken out of.` |
|         - |  3751 | ` */` |
|     59320 |  3752 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  3753 | `{` |
|         - |  3754 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3755 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3756 | `	sxi32 rc;` |
|     59325 |  3757 | `	iLevel = 0;` |
|         - |  3758 | `	/* Jump the 'break' keyword */` |
|     59325 |  3759 | `	pGen->pIn++;` |
|     59325 |  3760 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3761 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3762 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3763 | `		 */` |
|         - |  3764 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3765 | `		char *zAlloc = 0;` |
|         - |  3766 | `		SyString sNum;` |
|        17 |  3767 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3768 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3769 | `			return SXERR_ABORT;` |
|         - |  3770 | `		}` |
|        17 |  3771 | `		if( rc == SXRET_OK ){` |
|        21 |  3772 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3773 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  3774 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3775 | `				return SXERR_ABORT;` |
|         - |  3776 | `			}` |
|        15 |  3777 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  3778 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3779 | `		}` |
|        17 |  3780 | `		if( iLevel < 2 ){` |
|         3 |  3781 | `			iLevel = 0;` |
|         1 |  3782 | `		}` |
|        17 |  3783 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3784 | `	}` |
|         - |  3785 | `	/* Extract the target loop */` |
|     59325 |  3786 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     59325 |  3787 | `	if( pLoop == 0 ){` |
|         - |  3788 | `		/* Illegal break */` |
|        19 |  3789 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|        19 |  3790 | `		if( rc == SXERR_ABORT ){` |
|         - |  3791 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3792 | `			return SXERR_ABORT;` |
|         - |  3793 | `		}` |
|        11 |  3794 | `	}else{` |
|         - |  3795 | `		sxu32 nInstrIdx;` |
|         - |  3796 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     59309 |  3797 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3798 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     59309 |  3799 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     59309 |  3800 | `		if( rc == SXRET_OK ){` |
|         - |  3801 | `			/* Fix the jump later when the jump destination is resolved */` |
|     59309 |  3802 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     29652 |  3803 | `		}` |
|         - |  3804 | `	}` |
|     59325 |  3805 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3806 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3807 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  3808 | `	}` |
|         - |  3809 | `	/* Statement successfully compiled */` |
|     59325 |  3810 | `	return SXRET_OK;` |
|     29665 |  3811 | `}` |
|         - |  3812 | `/*` |
|         - |  3813 | ` * Compile or record a label.` |
|         - |  3814 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  3815 | ` * Example` |
|         - |  3816 | ` *  goto LABEL;` |
|         - |  3817 | ` *   echo 'Foo';` |
|         - |  3818 | ` *  LABEL:` |
|         - |  3819 | ` *   echo 'Bar';` |
|         - |  3820 | ` */` |
|       112 |  3821 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  3822 | `{` |
|         - |  3823 | `	GenBlock *pBlock;` |
|         - |  3824 | `	Label sLabel;` |
|         - |  3825 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|       117 |  3826 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|       117 |  3827 | `	if( pBlock ){` |
|         - |  3828 | `		sxi32 rc;` |
|         8 |  3829 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         4 |  3830 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|         6 |  3831 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3832 | `			return SXERR_ABORT;` |
|         - |  3833 | `		}` |
|         4 |  3834 | `	}else{` |
|       113 |  3835 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3836 | `		char *zDup;` |
|         - |  3837 | `		/* Initialize label fields */` |
|       113 |  3838 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  3839 | `		/* Duplicate label name */` |
|       113 |  3840 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       113 |  3841 | `		if( zDup == 0 ){` |
|       ! 0 |  3842 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3843 | `			return SXERR_ABORT;` |
|         - |  3844 | `		}` |
|       113 |  3845 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       113 |  3846 | `		sLabel.bRef  = FALSE;` |
|       113 |  3847 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       113 |  3848 | `		pBlock = pGen->pCurrent;` |
|       221 |  3849 | `		while( pBlock ){` |
|       133 |  3850 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        24 |  3851 | `				break;` |
|         - |  3852 | `			}` |
|         - |  3853 | `			/* Point to the upper block */` |
|       113 |  3854 | `			pBlock = pBlock->pParent;` |
|         5 |  3855 | `		}` |
|       113 |  3856 | `		if( pBlock ){` |
|        24 |  3857 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        14 |  3858 | `		}else{` |
|        93 |  3859 | `			sLabel.pFunc = 0;` |
|         - |  3860 | `		}` |
|         - |  3861 | `		/* Insert in label set */` |
|       113 |  3862 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  3863 | `	}` |
|       117 |  3864 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  3865 | `	return SXRET_OK;` |
|        61 |  3866 | `}` |
|         - |  3867 | `/*` |
|         - |  3868 | ` * Compile the so hated 'goto' statement.` |
|         - |  3869 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  3870 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  3871 | ` * a compiler it has to do this.` |
|         - |  3872 | ` * According to the PHP language reference manual` |
|         - |  3873 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  3874 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  3875 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  3876 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  3877 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  3878 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  3879 | ` *   of a multi-level break` |
|         - |  3880 | ` */` |
|       152 |  3881 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  3882 | `{` |
|         - |  3883 | `	JumpFixup sJump;` |
|         - |  3884 | `	sxi32 rc;` |
|       157 |  3885 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  3886 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3887 | `		/* Missing label */` |
|       ! 0 |  3888 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  3889 | `		if( rc == SXERR_ABORT ){` |
|         - |  3890 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3891 | `			return SXERR_ABORT;` |
|         - |  3892 | `		}` |
|       ! 0 |  3893 | `		return SXRET_OK;` |
|         - |  3894 | `	}` |
|       157 |  3895 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         6 |  3896 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|         6 |  3897 | `		if( rc == SXERR_ABORT ){` |
|         - |  3898 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3899 | `			return SXERR_ABORT;` |
|         - |  3900 | `		}` |
|         4 |  3901 | `	}else{` |
|       153 |  3902 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3903 | `		GenBlock *pBlock;` |
|         - |  3904 | `		char *zDup;` |
|         - |  3905 | `		/* Prepare the jump destination */` |
|       153 |  3906 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  3907 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  3908 | `		/* Duplicate label name */` |
|       153 |  3909 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  3910 | `		if( zDup == 0 ){` |
|       ! 0 |  3911 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3912 | `			return SXERR_ABORT;` |
|         - |  3913 | `		}` |
|       153 |  3914 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|       153 |  3915 | `		pBlock = pGen->pCurrent;` |
|       315 |  3916 | `		while( pBlock ){` |
|       199 |  3917 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        37 |  3918 | `				break;` |
|         - |  3919 | `			}` |
|         - |  3920 | `			/* Point to the upper block */` |
|       167 |  3921 | `			pBlock = pBlock->pParent;` |
|         5 |  3922 | `		}` |
|       153 |  3923 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         9 |  3924 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|         9 |  3925 | `			if( rc == SXERR_ABORT ){` |
|         - |  3926 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  3927 | `				return SXERR_ABORT;` |
|         - |  3928 | `			}` |
|         3 |  3929 | `		}` |
|       153 |  3930 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        30 |  3931 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        17 |  3932 | `		}else{` |
|       127 |  3933 | `			sJump.pFunc = 0;` |
|         - |  3934 | `		}` |
|         - |  3935 | `		/* Emit the unconditional jump */` |
|       153 |  3936 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  3937 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  3938 | `		}` |
|         - |  3939 | `	}` |
|       157 |  3940 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  3941 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  3942 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  3943 | `	}` |
|         - |  3944 | `	/* Statement successfully compiled */` |
|       157 |  3945 | `	return SXRET_OK;` |
|        81 |  3946 | `}` |
|         - |  3947 | `/*` |
|         - |  3948 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  3949 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  3950 | ` * failure.` |
|         - |  3951 | ` */` |
|        20 |  3952 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         2 |  3953 | `{` |
|         - |  3954 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  3955 | `	sxu32 nRawObj;` |
|        10 |  3956 | `	sxu32 nObjIdx;` |
|         - |  3957 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  3958 | `	 * a PHP block.` |
|         - |  3959 | `	 */` |
|        10 |  3960 | `Consume:` |
|        22 |  3961 | `	nRawObj = nObjIdx = 0;` |
|        22 |  3962 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  3963 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  3964 | `		if( pRawObj == 0 ){` |
|       ! 0 |  3965 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3966 | `			return SXERR_ABORT;` |
|         - |  3967 | `		}` |
|         - |  3968 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  3969 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  3970 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  3971 | `		++nRawObj;` |
|       ! 0 |  3972 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  3973 | `	}` |
|        22 |  3974 | `	if( nRawObj > 0 ){` |
|         - |  3975 | `		/* Emit the consume instruction */` |
|       ! 0 |  3976 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  3977 | `	}` |
|        22 |  3978 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  3979 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  3980 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  3981 | `		SySetReset(pTokenSet);` |
|       ! 0 |  3982 | `		SySetReset(&pGen->aTrivia);` |
|         - |  3983 | `		/* Tokenize input */` |
|       ! 0 |  3984 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  3985 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  3986 | `		/* Point to the fresh token stream */` |
|       ! 0 |  3987 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  3988 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  3989 | `		/* Advance the stream cursor */` |
|       ! 0 |  3990 | `		pGen->pRawIn++;` |
|         - |  3991 | `		/* TICKET 1433-011 */` |
|       ! 0 |  3992 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  3993 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  3994 | `			sxi32 rc;` |
|         - |  3995 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  3996 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  3997 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  3998 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  3999 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4000 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4001 | `				return SXERR_ABORT;` |
|       ! 0 |  4002 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4003 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4004 | `			}` |
|       ! 0 |  4005 | `			goto Consume;` |
|         - |  4006 | `		}` |
|       ! 0 |  4007 | `	}else{` |
|         - |  4008 | `		/* No more chunks to process */` |
|        22 |  4009 | `		pGen->pIn = pGen->pEnd;` |
|        22 |  4010 | `		return SXERR_EOF;` |
|         - |  4011 | `	}` |
|       ! 0 |  4012 | `	return SXRET_OK;` |
|        12 |  4013 | `}` |
|         - |  4014 | `/*` |
|         - |  4015 | ` * Compile a PHP block.` |
|         - |  4016 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4017 | ` * optionally delimited by braces {}.` |
|         - |  4018 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4019 | ` * and this function takes care of generating the appropriate error` |
|         - |  4020 | ` * message.` |
|         - |  4021 | ` */` |
|   5373754 |  4022 | `static sxi32 PH7_CompileBlock(` |
|         - |  4023 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4024 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4025 | `	)` |
|         5 |  4026 | `{` |
|         - |  4027 | `	sxi32 rc;` |
|         - |  4028 | `	sxu32 nLine;` |
|   5373759 |  4029 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5372393 |  4030 | `		nLine = pGen->pIn->nLine;` |
|   5372393 |  4031 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5372393 |  4032 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4033 | `			return SXERR_ABORT;` |
|         - |  4034 | `		}` |
|   5372393 |  4035 | `		pGen->pIn++;` |
|         - |  4036 | `		/* Compile until we hit the closing braces '}' */` |
|   7859403 |  4037 | `		for(;;){` |
|  15718811 |  4038 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        22 |  4039 | `				rc = GenStateNextChunk(&(*pGen));` |
|        22 |  4040 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4041 | `			 	   return SXERR_ABORT;` |
|         - |  4042 | `				}` |
|        22 |  4043 | `				if( rc == SXERR_EOF ){` |
|         - |  4044 | `					/* No more token to process. Missing closing braces */` |
|        22 |  4045 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|        22 |  4046 | `					break;` |
|         - |  4047 | `				}` |
|       ! 0 |  4048 | `			}` |
|  15718791 |  4049 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4050 | `				/* Closing braces found,break immediately*/` |
|   5372373 |  4051 | `				pGen->pIn++;` |
|   5372373 |  4052 | `				break;` |
|         - |  4053 | `			}` |
|         - |  4054 | `			/* Compile a single statement */` |
|  10346423 |  4055 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  10346423 |  4056 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4057 | `				return SXERR_ABORT;` |
|         - |  4058 | `			}` |
|         5 |  4059 | `		}` |
|   5372393 |  4060 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   2687565 |  4061 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4062 | `		pGen->pIn++;` |
|       ! 0 |  4063 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4064 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4065 | `			return SXERR_ABORT;` |
|         - |  4066 | `		}` |
|         - |  4067 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4068 | `		for(;;){` |
|       ! 0 |  4069 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4070 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4071 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4072 | `			 	   return SXERR_ABORT;` |
|         - |  4073 | `				}` |
|       ! 0 |  4074 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4075 | `					/* No more token to process */` |
|       ! 0 |  4076 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4077 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4078 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4079 | `					}` |
|       ! 0 |  4080 | `					break;` |
|         - |  4081 | `				}` |
|       ! 0 |  4082 | `			}` |
|       ! 0 |  4083 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4084 | `				sxi32 nKwrd;` |
|         - |  4085 | `				/* Keyword found */` |
|       ! 0 |  4086 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4087 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4088 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4089 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4090 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4091 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4092 | `						}` |
|       ! 0 |  4093 | `						break;` |
|         - |  4094 | `				}` |
|       ! 0 |  4095 | `			}` |
|         - |  4096 | `			/* Compile a single statement */` |
|       ! 0 |  4097 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4098 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4099 | `				return SXERR_ABORT;` |
|         - |  4100 | `			}` |
|       ! 0 |  4101 | `		}` |
|       ! 0 |  4102 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4103 | `	}else{` |
|         - |  4104 | `		/* Compile a single statement */` |
|      1371 |  4105 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      1371 |  4106 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4107 | `			return SXERR_ABORT;` |
|         - |  4108 | `		}` |
|         - |  4109 | `	}` |
|         - |  4110 | `	/* Jump trailing semi-colons ';' */` |
|   5373759 |  4111 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4112 | `		pGen->pIn++;` |
|       ! 0 |  4113 | `	}` |
|   5373759 |  4114 | `	return SXRET_OK;` |
|   2686882 |  4115 | `}` |
|         - |  4116 | `/*` |
|         - |  4117 | ` * Compile the gentle 'while' statement.` |
|         - |  4118 | ` * According to the PHP language reference` |
|         - |  4119 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4120 | ` *  The basic form of a while statement is:` |
|         - |  4121 | ` *  while (expr)` |
|         - |  4122 | ` *   statement` |
|         - |  4123 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4124 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4125 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4126 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4127 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4128 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4129 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4130 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4131 | ` *  while (expr):` |
|         - |  4132 | ` *    statement` |
|         - |  4133 | ` *   endwhile;` |
|         - |  4134 | ` */` |
|     55386 |  4135 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4136 | `{` |
|     55391 |  4137 | `	GenBlock *pWhileBlock = 0;` |
|     55391 |  4138 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4139 | `	sxu32 nFalseJump;` |
|         - |  4140 | `	sxu32 nLine;` |
|         - |  4141 | `	sxi32 rc;` |
|     55391 |  4142 | `	nLine = pGen->pIn->nLine;` |
|         - |  4143 | `	/* Jump the 'while' keyword */` |
|     55391 |  4144 | `	pGen->pIn++;` |
|     55391 |  4145 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4146 | `		/* Syntax error */` |
|       ! 0 |  4147 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4148 | `		if( rc == SXERR_ABORT ){` |
|         - |  4149 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4150 | `			return SXERR_ABORT;` |
|         - |  4151 | `		}` |
|       ! 0 |  4152 | `		goto Synchronize;` |
|         - |  4153 | `	}` |
|         - |  4154 | `	/* Jump the left parenthesis '(' */` |
|     55391 |  4155 | `	pGen->pIn++;` |
|         - |  4156 | `	/* Create the loop block */` |
|     55391 |  4157 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     55391 |  4158 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4159 | `		return SXERR_ABORT;` |
|         - |  4160 | `	}` |
|         - |  4161 | `	/* Delimit the condition */` |
|     55391 |  4162 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     55391 |  4163 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4164 | `		/* Empty expression */` |
|         3 |  4165 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4166 | `		if( rc == SXERR_ABORT ){` |
|         - |  4167 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4168 | `			return SXERR_ABORT;` |
|         - |  4169 | `		}` |
|         1 |  4170 | `	}` |
|         - |  4171 | `	/* Swap token streams */` |
|     55391 |  4172 | `	pTmp = pGen->pEnd;` |
|     55391 |  4173 | `	pGen->pEnd = pEnd;` |
|         - |  4174 | `	/* Compile the expression */` |
|     55391 |  4175 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     55391 |  4176 | `	if( rc == SXERR_ABORT ){` |
|         - |  4177 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4178 | `		return SXERR_ABORT;` |
|         - |  4179 | `	}` |
|         - |  4180 | `	/* Update token stream */` |
|     55391 |  4181 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4182 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4183 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4184 | `			return SXERR_ABORT;` |
|         - |  4185 | `		}` |
|       ! 0 |  4186 | `		pGen->pIn++;` |
|       ! 0 |  4187 | `	}` |
|         - |  4188 | `	/* Synchronize pointers */` |
|     55391 |  4189 | `	pGen->pIn  = &pEnd[1];` |
|     55391 |  4190 | `	pGen->pEnd = pTmp;` |
|         - |  4191 | `	/* Emit the false jump */` |
|     55391 |  4192 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4193 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     55391 |  4194 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4195 | `	/* Compile the loop body */` |
|     55391 |  4196 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     55391 |  4197 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4198 | `		return SXERR_ABORT;` |
|         - |  4199 | `	}` |
|         - |  4200 | `	/* Emit the unconditional jump to the start of the loop */` |
|     55391 |  4201 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4202 | `	/* Fix all jumps now the destination is resolved */` |
|     55391 |  4203 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4204 | `	/* Release the loop block */` |
|     55391 |  4205 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4206 | `	/* Statement successfully compiled */` |
|     55391 |  4207 | `	return SXRET_OK;` |
|       ! 0 |  4208 | `Synchronize:` |
|         - |  4209 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4210 | `	 * compiling this erroneous block.` |
|         - |  4211 | `	 */` |
|       ! 0 |  4212 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4213 | `		pGen->pIn++;` |
|       ! 0 |  4214 | `	}` |
|       ! 0 |  4215 | `	return SXRET_OK;` |
|     27698 |  4216 | `}` |
|         - |  4217 | `/*` |
|         - |  4218 | ` * Compile the ugly do..while() statement.` |
|         - |  4219 | ` * According to the PHP language reference` |
|         - |  4220 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4221 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4222 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4223 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4224 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4225 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4226 | ` *  would end immediately).` |
|         - |  4227 | ` *  There is just one syntax for do-while loops:` |
|         - |  4228 | ` *  <?php` |
|         - |  4229 | ` *  $i = 0;` |
|         - |  4230 | ` *  do {` |
|         - |  4231 | ` *   echo $i;` |
|         - |  4232 | ` *  } while ($i > 0);` |
|         - |  4233 | ` * ?>` |
|         - |  4234 | ` */` |
|         2 |  4235 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4236 | `{` |
|         3 |  4237 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4238 | `	GenBlock *pDoBlock = 0;` |
|         - |  4239 | `	sxu32 nLine;` |
|         - |  4240 | `	sxi32 rc;` |
|         3 |  4241 | `	nLine = pGen->pIn->nLine;` |
|         - |  4242 | `	/* Jump the 'do' keyword */` |
|         3 |  4243 | `	pGen->pIn++;` |
|         - |  4244 | `	/* Create the loop block */` |
|         3 |  4245 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4246 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4247 | `		return SXERR_ABORT;` |
|         - |  4248 | `	}` |
|         - |  4249 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4250 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4251 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4252 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4253 | `		return SXERR_ABORT;` |
|         - |  4254 | `	}` |
|         3 |  4255 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4256 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4257 | `	}` |
|         3 |  4258 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4259 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4260 | `			/* Missing 'while' statement */` |
|         3 |  4261 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4262 | `			if( rc == SXERR_ABORT ){` |
|         - |  4263 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4264 | `				return SXERR_ABORT;` |
|         - |  4265 | `			}` |
|         3 |  4266 | `			goto Synchronize;` |
|         - |  4267 | `	}` |
|         - |  4268 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4269 | `	pGen->pIn++;` |
|       ! 0 |  4270 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4271 | `		/* Syntax error */` |
|       ! 0 |  4272 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4273 | `		if( rc == SXERR_ABORT ){` |
|         - |  4274 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4275 | `			return SXERR_ABORT;` |
|         - |  4276 | `		}` |
|       ! 0 |  4277 | `		goto Synchronize;` |
|         - |  4278 | `	}` |
|         - |  4279 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4280 | `	pGen->pIn++;` |
|         - |  4281 | `	/* Delimit the condition */` |
|       ! 0 |  4282 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4283 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4284 | `		/* Empty expression */` |
|       ! 0 |  4285 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4286 | `		if( rc == SXERR_ABORT ){` |
|         - |  4287 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4288 | `			return SXERR_ABORT;` |
|         - |  4289 | `		}` |
|       ! 0 |  4290 | `		goto Synchronize;` |
|         - |  4291 | `	}` |
|         - |  4292 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4293 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4294 | `		JumpFixup *aPost;` |
|         - |  4295 | `		VmInstr *pInstr;` |
|         - |  4296 | `		sxu32 nJumpDest;` |
|         - |  4297 | `		sxu32 n;` |
|       ! 0 |  4298 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4299 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4300 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4301 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4302 | `			if( pInstr ){` |
|         - |  4303 | `				/* Fix */` |
|       ! 0 |  4304 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4305 | `			}` |
|       ! 0 |  4306 | `		}` |
|       ! 0 |  4307 | `	}` |
|         - |  4308 | `	/* Swap token streams */` |
|       ! 0 |  4309 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4310 | `	pGen->pEnd = pEnd;` |
|         - |  4311 | `	/* Compile the expression */` |
|       ! 0 |  4312 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4313 | `	if( rc == SXERR_ABORT ){` |
|         - |  4314 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4315 | `		return SXERR_ABORT;` |
|         - |  4316 | `	}` |
|         - |  4317 | `	/* Update token stream */` |
|       ! 0 |  4318 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4319 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4320 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4321 | `			return SXERR_ABORT;` |
|         - |  4322 | `		}` |
|       ! 0 |  4323 | `		pGen->pIn++;` |
|       ! 0 |  4324 | `	}` |
|       ! 0 |  4325 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4326 | `	pGen->pEnd = pTmp;` |
|         - |  4327 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4328 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4329 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4330 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4331 | `	/* Release the loop block */` |
|       ! 0 |  4332 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4333 | `	/* Statement successfully compiled */` |
|       ! 0 |  4334 | `	return SXRET_OK;` |
|         1 |  4335 | `Synchronize:` |
|         - |  4336 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4337 | `	 * compiling this erroneous block.` |
|         - |  4338 | `	 */` |
|         3 |  4339 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4340 | `		pGen->pIn++;` |
|       ! 0 |  4341 | `	}` |
|         3 |  4342 | `	return SXRET_OK;` |
|         2 |  4343 | `}` |
|         - |  4344 | `/*` |
|         - |  4345 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4346 | ` * According to the PHP language reference` |
|         - |  4347 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4348 | ` *  The syntax of a for loop is:` |
|         - |  4349 | ` *  for (expr1; expr2; expr3)` |
|         - |  4350 | ` *   statement` |
|         - |  4351 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4352 | ` *  the beginning of the loop.` |
|         - |  4353 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4354 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4355 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4356 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4357 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4358 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4359 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4360 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4361 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4362 | ` *  of using the for truth expression.` |
|         - |  4363 | ` */` |
|     90902 |  4364 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4365 | `{` |
|     90907 |  4366 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|     90907 |  4367 | `	GenBlock *pForBlock = 0;` |
|         - |  4368 | `	sxu32 nFalseJump;` |
|         - |  4369 | `	sxu32 nLine;` |
|         - |  4370 | `	sxi32 rc;` |
|     90907 |  4371 | `	nLine = pGen->pIn->nLine;` |
|         - |  4372 | `	/* Jump the 'for' keyword */` |
|     90907 |  4373 | `	pGen->pIn++;` |
|     90907 |  4374 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4375 | `		/* Syntax error */` |
|       ! 0 |  4376 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4377 | `		if( rc == SXERR_ABORT ){` |
|         - |  4378 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4379 | `			return SXERR_ABORT;` |
|         - |  4380 | `		}` |
|       ! 0 |  4381 | `		return SXRET_OK;` |
|         - |  4382 | `	}` |
|         - |  4383 | `	/* Jump the left parenthesis '(' */` |
|     90907 |  4384 | `	pGen->pIn++;` |
|         - |  4385 | `	/* Delimit the init-expr;condition;post-expr */` |
|     90907 |  4386 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     90907 |  4387 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4388 | `		/* Empty expression */` |
|       ! 0 |  4389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4390 | `		if( rc == SXERR_ABORT ){` |
|         - |  4391 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4392 | `			return SXERR_ABORT;` |
|         - |  4393 | `		}` |
|         - |  4394 | `		/* Synchronize */` |
|       ! 0 |  4395 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4396 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4397 | `			pGen->pIn++;` |
|       ! 0 |  4398 | `		}` |
|       ! 0 |  4399 | `		return SXRET_OK;` |
|         - |  4400 | `	}` |
|         - |  4401 | `	/* Swap token streams */` |
|     90907 |  4402 | `	pTmp = pGen->pEnd;` |
|     90907 |  4403 | `	pGen->pEnd = pEnd;` |
|         - |  4404 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4405 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4406 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4407 | `	 * compiled through this same window — recorded as a known leniency. */` |
|     90907 |  4408 | `	pGen->nCommaExprOk++;` |
|         - |  4409 | `	/* Compile initialization expressions if available */` |
|     90907 |  4410 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4411 | `	/* Pop operand lvalues */` |
|     90907 |  4412 | `	if( rc == SXERR_ABORT ){` |
|         - |  4413 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4414 | `		return SXERR_ABORT;` |
|     90907 |  4415 | `	}else if( rc != SXERR_EMPTY ){` |
|     79067 |  4416 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     39531 |  4417 | `	}` |
|     90907 |  4418 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4419 | `		/* Syntax error */` |
|       ! 0 |  4420 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  4421 | `			"for: Expected ';' after initialization expressions");` |
|       ! 0 |  4422 | `		if( rc == SXERR_ABORT ){` |
|         - |  4423 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4424 | `			return SXERR_ABORT;` |
|         - |  4425 | `		}` |
|       ! 0 |  4426 | `		return SXRET_OK;` |
|         - |  4427 | `	}` |
|         - |  4428 | `	/* Jump the trailing ';' */` |
|     90907 |  4429 | `	pGen->pIn++;` |
|         - |  4430 | `	/* Create the loop block */` |
|     90907 |  4431 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|     90907 |  4432 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4433 | `		return SXERR_ABORT;` |
|         - |  4434 | `	}` |
|         - |  4435 | `	/* Deffer continue jumps */` |
|     90907 |  4436 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4437 | `	/* Compile the condition */` |
|     90907 |  4438 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     90907 |  4439 | `	if( rc == SXERR_ABORT ){` |
|         - |  4440 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4441 | `		return SXERR_ABORT;` |
|     90907 |  4442 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4443 | `		/* Emit the false jump */` |
|     79067 |  4444 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4445 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     79067 |  4446 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     39531 |  4447 | `	}` |
|     90907 |  4448 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4449 | `		/* Syntax error */` |
|         6 |  4450 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  4451 | `			"for: Expected ';' after conditionals expressions");` |
|         6 |  4452 | `		if( rc == SXERR_ABORT ){` |
|         - |  4453 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4454 | `			return SXERR_ABORT;` |
|         - |  4455 | `		}` |
|         6 |  4456 | `		return SXRET_OK;` |
|         - |  4457 | `	}` |
|         - |  4458 | `	/* Jump the trailing ';' */` |
|     90903 |  4459 | `	pGen->pIn++;` |
|         - |  4460 | `	/* Save the post condition stream */` |
|     90903 |  4461 | `	pPostStart = pGen->pIn;` |
|         - |  4462 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4463 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|     90903 |  4464 | `	pGen->nCommaExprOk--;` |
|     90903 |  4465 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|     90903 |  4466 | `	pGen->pEnd = pTmp;` |
|     90903 |  4467 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|     90903 |  4468 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4469 | `		return SXERR_ABORT;` |
|         - |  4470 | `	}` |
|         - |  4471 | `	/* Fix post-continue jumps */` |
|     90903 |  4472 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4473 | `		JumpFixup *aPost;` |
|         - |  4474 | `		VmInstr *pInstr;` |
|         - |  4475 | `		sxu32 nJumpDest;` |
|         - |  4476 | `		sxu32 n;` |
|      7909 |  4477 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      7909 |  4478 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     27651 |  4479 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     19747 |  4480 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     19747 |  4481 | `			if( pInstr ){` |
|         - |  4482 | `				/* Fix jump */` |
|     19747 |  4483 | `				pInstr->iP2 = nJumpDest;` |
|      9871 |  4484 | `			}` |
|      9876 |  4485 | `		}` |
|      3952 |  4486 | `	}` |
|         - |  4487 | `	/* compile the post-expressions if available */` |
|     90903 |  4488 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4489 | `		pPostStart++;` |
|       ! 0 |  4490 | `	}` |
|     90903 |  4491 | `	if( pPostStart < pEnd ){` |
|         - |  4492 | `		SyToken *pTmpIn,*pTmpEnd;` |
|     79065 |  4493 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|     79065 |  4494 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|     79065 |  4495 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     79065 |  4496 | `		pGen->nCommaExprOk--;` |
|     79065 |  4497 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4498 | `			/* Syntax error */` |
|       ! 0 |  4499 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4500 | `			if( rc == SXERR_ABORT ){` |
|         - |  4501 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4502 | `				return SXERR_ABORT;` |
|         - |  4503 | `			}` |
|       ! 0 |  4504 | `			return SXRET_OK;` |
|         - |  4505 | `		}` |
|     79065 |  4506 | `		RE_SWAP_DELIMITER(pGen);` |
|     79065 |  4507 | `		if( rc == SXERR_ABORT ){` |
|         - |  4508 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4509 | `			return SXERR_ABORT;` |
|     79065 |  4510 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4511 | `			/* Pop operand lvalue */` |
|     79065 |  4512 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     39530 |  4513 | `		}` |
|     39530 |  4514 | `	}` |
|         - |  4515 | `	/* Emit the unconditional jump to the start of the loop */` |
|     90903 |  4516 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4517 | `	/* Fix all jumps now the destination is resolved */` |
|     90903 |  4518 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4519 | `	/* Release the loop block */` |
|     90903 |  4520 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4521 | `	/* Statement successfully compiled */` |
|     90903 |  4522 | `	return SXRET_OK;` |
|     45456 |  4523 | `}` |
|         - |  4524 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4525 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4526 | ` * are allowed.` |
|         - |  4527 | ` */` |
|    332452 |  4528 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4529 | `{` |
|    332457 |  4530 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    332457 |  4531 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4532 | `		/* Unexpected expression */` |
|       ! 0 |  4533 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4534 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4535 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4536 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4537 | `		}` |
|       ! 0 |  4538 | `	}` |
|    332457 |  4539 | `	return rc;` |
|         5 |  4540 | `}` |
|         - |  4541 | `/*` |
|         - |  4542 | ` * Compile the 'foreach' statement.` |
|         - |  4543 | ` * According to the PHP language reference` |
|         - |  4544 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4545 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4546 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4547 | ` *  is a minor but useful extension of the first:` |
|         - |  4548 | ` *  foreach (array_expression as $value)` |
|         - |  4549 | ` *    statement` |
|         - |  4550 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4551 | ` *   statement` |
|         - |  4552 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4553 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4554 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4555 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4556 | ` *  to the variable $key on each loop.` |
|         - |  4557 | ` *  Note:` |
|         - |  4558 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4559 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4560 | ` *  Note:` |
|         - |  4561 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4562 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4563 | ` *  or after the foreach without resetting it.` |
|         - |  4564 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4565 | ` *  of copying the value.` |
|         - |  4566 | ` */` |
|    229582 |  4567 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4568 | `{` |
|    229587 |  4569 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    229587 |  4570 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    229587 |  4571 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4572 | `	ph7_foreach_info *pInfo;` |
|         - |  4573 | `	sxu32 nFalseJump;` |
|         - |  4574 | `	VmInstr *pInstr;` |
|         - |  4575 | `	sxu32 nLine;` |
|         - |  4576 | `	sxi32 rc;` |
|    229587 |  4577 | `	nLine = pGen->pIn->nLine;` |
|         - |  4578 | `	/* Jump the 'foreach' keyword */` |
|    229587 |  4579 | `	pGen->pIn++;` |
|    229587 |  4580 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4581 | `		/* Syntax error */` |
|       ! 0 |  4582 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4583 | `		if( rc == SXERR_ABORT ){` |
|         - |  4584 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4585 | `			return SXERR_ABORT;` |
|         - |  4586 | `		}` |
|       ! 0 |  4587 | `		goto Synchronize;` |
|         - |  4588 | `	}` |
|         - |  4589 | `	/* Jump the left parenthesis '(' */` |
|    229587 |  4590 | `	pGen->pIn++;` |
|         - |  4591 | `	/* Create the loop block */` |
|    229587 |  4592 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    229587 |  4593 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4594 | `		return SXERR_ABORT;` |
|         - |  4595 | `	}` |
|         - |  4596 | `	/* Delimit the expression */` |
|    229587 |  4597 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    229587 |  4598 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4599 | `		/* Empty expression */` |
|       ! 0 |  4600 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4601 | `		if( rc == SXERR_ABORT ){` |
|         - |  4602 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4603 | `			return SXERR_ABORT;` |
|         - |  4604 | `		}` |
|         - |  4605 | `		/* Synchronize */` |
|       ! 0 |  4606 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4607 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4608 | `			pGen->pIn++;` |
|       ! 0 |  4609 | `		}` |
|       ! 0 |  4610 | `		return SXRET_OK;` |
|         - |  4611 | `	}` |
|         - |  4612 | `	/* Compile the array expression */` |
|    229587 |  4613 | `	pCur = pGen->pIn;` |
|   1239095 |  4614 | `	while( pCur < pEnd ){` |
|   1239095 |  4615 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    241439 |  4616 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    241439 |  4617 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4618 | `				/* Break with the first 'as' found */` |
|    229587 |  4619 | `				break;` |
|         - |  4620 | `			}` |
|      5926 |  4621 | `		}` |
|         - |  4622 | `		/* Advance the stream cursor */` |
|   1009513 |  4623 | `		pCur++;` |
|         5 |  4624 | `	}` |
|    229587 |  4625 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4626 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4627 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4628 | `		if( rc == SXERR_ABORT ){` |
|         - |  4629 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4630 | `			return SXERR_ABORT;` |
|         - |  4631 | `		}` |
|       ! 0 |  4632 | `		goto Synchronize;` |
|         - |  4633 | `	}` |
|         - |  4634 | `	/* Swap token streams */` |
|    229587 |  4635 | `	pTmp = pGen->pEnd;` |
|    229587 |  4636 | `	pGen->pEnd = pCur;` |
|    229587 |  4637 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    229587 |  4638 | `	if( rc == SXERR_ABORT ){` |
|         - |  4639 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4640 | `		return SXERR_ABORT;` |
|         - |  4641 | `	}` |
|         - |  4642 | `	/* Update token stream */` |
|    229587 |  4643 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4644 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4645 | `		if( rc == SXERR_ABORT ){` |
|         - |  4646 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4647 | `			return SXERR_ABORT;` |
|         - |  4648 | `		}` |
|       ! 0 |  4649 | `		pGen->pIn++;` |
|       ! 0 |  4650 | `	}` |
|    229587 |  4651 | `	pCur++; /* Jump the 'as' keyword */` |
|    229587 |  4652 | `	pGen->pIn = pCur;` |
|    229587 |  4653 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4654 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4655 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4656 | `			return SXERR_ABORT;` |
|         - |  4657 | `		}` |
|       ! 0 |  4658 | `	}` |
|         - |  4659 | `	/* Create the foreach context */` |
|    229587 |  4660 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    229587 |  4661 | `	if( pInfo == 0 ){` |
|       ! 0 |  4662 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4663 | `		return SXERR_ABORT;` |
|         - |  4664 | `	}` |
|         - |  4665 | `	/* Zero the structure */` |
|    229587 |  4666 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4667 | `	/* Initialize structure fields */` |
|    229587 |  4668 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4669 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4670 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4671 | `	 * '=>'. */` |
|    229587 |  4672 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    229587 |  4673 | `	if( pCur < pEnd ){` |
|         - |  4674 | `		/* Compile the expression holding the key name */` |
|    102895 |  4675 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4676 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4677 | `			if( rc == SXERR_ABORT ){` |
|         - |  4678 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4679 | `				return SXERR_ABORT;` |
|         - |  4680 | `			}` |
|       ! 0 |  4681 | `		}else{` |
|    102895 |  4682 | `			pGen->pEnd = pCur;` |
|    102895 |  4683 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    102895 |  4684 | `			if( rc == SXERR_ABORT ){` |
|         - |  4685 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4686 | `				return SXERR_ABORT;` |
|         - |  4687 | `			}` |
|    102895 |  4688 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    102895 |  4689 | `			if( pInstr->p3 ){` |
|         - |  4690 | `				/* Record key name */` |
|    102895 |  4691 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     51445 |  4692 | `			}` |
|    102895 |  4693 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4694 | `		}` |
|    102895 |  4695 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     51445 |  4696 | `	}` |
|    229587 |  4697 | `	pGen->pEnd = pEnd;` |
|    229587 |  4698 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4699 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4700 | `		if( rc == SXERR_ABORT ){` |
|         - |  4701 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4702 | `			return SXERR_ABORT;` |
|         - |  4703 | `		}` |
|       ! 0 |  4704 | `		goto Synchronize;` |
|         - |  4705 | `	}` |
|    229587 |  4706 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4707 | `		pGen->pIn++;` |
|         - |  4708 | `		/* Pass by reference  */` |
|        33 |  4709 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4710 | `	}` |
|         - |  4711 | `	/* Check if the value target is list() */` |
|    229587 |  4712 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4713 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4714 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4715 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4716 | `		 */` |
|         - |  4717 | `		static int iForeachListCnt = 0;` |
|         - |  4718 | `		char zTmp[128];` |
|         - |  4719 | `		sxu32 nLen;` |
|         - |  4720 | `		char *zDup;` |
|        10 |  4721 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4722 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4723 | `		if( zDup == 0 ){` |
|       ! 0 |  4724 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4725 | `			return SXERR_ABORT;` |
|         - |  4726 | `		}` |
|        10 |  4727 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4728 | `		/* Save list() token boundaries */` |
|        10 |  4729 | `		pListStart = pGen->pIn;` |
|         - |  4730 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4731 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4732 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4733 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  4734 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  4735 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4736 | `				return SXERR_ABORT;` |
|         - |  4737 | `			}` |
|         3 |  4738 | `			goto Synchronize;` |
|         - |  4739 | `		}` |
|         7 |  4740 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  4741 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  4742 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4743 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4744 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  4745 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4746 | `				return SXERR_ABORT;` |
|         - |  4747 | `			}` |
|       ! 0 |  4748 | `			goto Synchronize;` |
|         - |  4749 | `		}` |
|         7 |  4750 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  4751 | `		pListEnd = pGen->pIn;` |
|         7 |  4752 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    229582 |  4753 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  4754 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  4755 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  4756 | `		 */` |
|         - |  4757 | `		static int iForeachShortListCnt = 0;` |
|         - |  4758 | `		char zTmp[128];` |
|         - |  4759 | `		sxu32 nLen;` |
|         - |  4760 | `		char *zDup;` |
|        13 |  4761 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        13 |  4762 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        13 |  4763 | `		if( zDup == 0 ){` |
|       ! 0 |  4764 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4765 | `			return SXERR_ABORT;` |
|         - |  4766 | `		}` |
|        13 |  4767 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4768 | `		/* Save [...] token boundaries */` |
|        13 |  4769 | `		pListStart = pGen->pIn;` |
|         - |  4770 | `		/* Advance past [...] */` |
|        13 |  4771 | `		pGen->pIn++; /* Jump '[' */` |
|        13 |  4772 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        13 |  4773 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4774 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4775 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  4776 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4777 | `				return SXERR_ABORT;` |
|         - |  4778 | `			}` |
|       ! 0 |  4779 | `			goto Synchronize;` |
|         - |  4780 | `		}` |
|        13 |  4781 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        13 |  4782 | `		pListEnd = pGen->pIn;` |
|        13 |  4783 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         7 |  4784 | `	}else{` |
|         - |  4785 | `		/* Compile the expression holding the value name */` |
|    229567 |  4786 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    229567 |  4787 | `		if( rc == SXERR_ABORT ){` |
|         - |  4788 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4789 | `			return SXERR_ABORT;` |
|         - |  4790 | `		}` |
|    229567 |  4791 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    229567 |  4792 | `		if( pInstr->p3 ){` |
|         - |  4793 | `			/* Record value name */` |
|    229567 |  4794 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    114781 |  4795 | `		}` |
|         - |  4796 | `	}` |
|         - |  4797 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    229585 |  4798 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  4799 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    229585 |  4800 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  4801 | `	/* Record the first instruction to execute */` |
|    229585 |  4802 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4803 | `	/* Emit the FOREACH_STEP instruction */` |
|    229585 |  4804 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  4805 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    229585 |  4806 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  4807 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    229585 |  4808 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  4809 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  4810 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  4811 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  4812 | `		 */` |
|        19 |  4813 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  4814 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  4815 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  4816 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  4817 | `		 */` |
|        19 |  4818 | `		pSavedIn = pGen->pIn;` |
|        19 |  4819 | `		pSavedEnd = pGen->pEnd;` |
|        19 |  4820 | `		pGen->pIn = pListStart;` |
|        19 |  4821 | `		pGen->pEnd = pListEnd;` |
|        19 |  4822 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        13 |  4823 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  4824 | `		}else{` |
|         7 |  4825 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  4826 | `		}` |
|        19 |  4827 | `		pGen->pIn = pSavedIn;` |
|        19 |  4828 | `		pGen->pEnd = pSavedEnd;` |
|        19 |  4829 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4830 | `			return SXERR_ABORT;` |
|         - |  4831 | `		}` |
|         - |  4832 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        19 |  4833 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         9 |  4834 | `	}` |
|         - |  4835 | `	/* Compile the loop body */` |
|    229585 |  4836 | `	pGen->pIn = &pEnd[1];` |
|    229585 |  4837 | `	pGen->pEnd = pTmp;` |
|    229585 |  4838 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    229585 |  4839 | `	if( rc == SXERR_ABORT ){` |
|         - |  4840 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4841 | `		return SXERR_ABORT;` |
|         - |  4842 | `	}` |
|         - |  4843 | `	/* Emit the unconditional jump to the start of the loop */` |
|    229585 |  4844 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  4845 | `	/* Fix all jumps now the destination is resolved */` |
|    229585 |  4846 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4847 | `	/* Release the loop block */` |
|    229585 |  4848 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4849 | `	/* Statement successfully compiled */` |
|    229585 |  4850 | `	return SXRET_OK;` |
|         1 |  4851 | `Synchronize:` |
|         - |  4852 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4853 | `	 * compiling this erroneous block.` |
|         - |  4854 | `	 */` |
|         3 |  4855 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4856 | `		pGen->pIn++;` |
|       ! 0 |  4857 | `	}` |
|         3 |  4858 | `	return SXRET_OK;` |
|    114796 |  4859 | `}` |
|         - |  4860 | `/*` |
|         - |  4861 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  4862 | ` * According to the PHP language reference` |
|         - |  4863 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  4864 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  4865 | ` *  that is similar to that of C:` |
|         - |  4866 | ` *  if (expr)` |
|         - |  4867 | ` *   statement` |
|         - |  4868 | ` *  else construct:` |
|         - |  4869 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  4870 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  4871 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  4872 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  4873 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  4874 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  4875 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  4876 | ` *  elseif` |
|         - |  4877 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  4878 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  4879 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  4880 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  4881 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  4882 | ` *   <?php` |
|         - |  4883 | ` *    if ($a > $b) {` |
|         - |  4884 | ` *     echo "a is bigger than b";` |
|         - |  4885 | ` *    } elseif ($a == $b) {` |
|         - |  4886 | ` *     echo "a is equal to b";` |
|         - |  4887 | ` *    } else {` |
|         - |  4888 | ` *     echo "a is smaller than b";` |
|         - |  4889 | ` *    }` |
|         - |  4890 | ` *    ?>` |
|         - |  4891 | ` */` |
|   1928070 |  4892 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  4893 | `{` |
|   1928075 |  4894 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   1928075 |  4895 | `	GenBlock *pCondBlock = 0;` |
|         - |  4896 | `	sxu32 nJumpIdx;` |
|         - |  4897 | `	sxu32 nKeyID;` |
|         - |  4898 | `	sxi32 rc;` |
|         - |  4899 | `	/* Jump the 'if' keyword */` |
|   1928075 |  4900 | `	pGen->pIn++;` |
|   1928075 |  4901 | `	pToken = pGen->pIn;` |
|         - |  4902 | `	/* Create the conditional block */` |
|   1928075 |  4903 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   1928075 |  4904 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4905 | `		return SXERR_ABORT;` |
|         - |  4906 | `	}` |
|         - |  4907 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1074582 |  4908 | `	for(;;){` |
|   2149169 |  4909 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4910 | `			/* Syntax error */` |
|       ! 0 |  4911 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4912 | `				pToken--;` |
|       ! 0 |  4913 | `			}` |
|       ! 0 |  4914 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  4915 | `			if( rc == SXERR_ABORT ){` |
|         - |  4916 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4917 | `				return SXERR_ABORT;` |
|         - |  4918 | `			}` |
|       ! 0 |  4919 | `			goto Synchronize;` |
|         - |  4920 | `		}` |
|         - |  4921 | `		/* Jump the left parenthesis '(' */` |
|   2149169 |  4922 | `		pToken++;` |
|         - |  4923 | `		/* Delimit the condition */` |
|   2149169 |  4924 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2149169 |  4925 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  4926 | `			/* Syntax error */` |
|        11 |  4927 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4928 | `				pToken--;` |
|       ! 0 |  4929 | `			}` |
|        11 |  4930 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  4931 | `			if( rc == SXERR_ABORT ){` |
|         - |  4932 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4933 | `				return SXERR_ABORT;` |
|         - |  4934 | `			}` |
|        11 |  4935 | `			goto Synchronize;` |
|         - |  4936 | `		}` |
|         - |  4937 | `		/* Swap token streams */` |
|   2149161 |  4938 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  4939 | `		/* Compile the condition */` |
|   2149161 |  4940 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4941 | `		/* Update token stream */` |
|   2149161 |  4942 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  4943 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4944 | `			pGen->pIn++;` |
|       ! 0 |  4945 | `		}` |
|   2149161 |  4946 | `		pGen->pIn  = &pEnd[1];` |
|   2149161 |  4947 | `		pGen->pEnd = pTmp;` |
|   2149161 |  4948 | `		if( rc == SXERR_ABORT ){` |
|         - |  4949 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4950 | `			return SXERR_ABORT;` |
|         - |  4951 | `		}` |
|         - |  4952 | `		/* Emit the false jump */` |
|   2149161 |  4953 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  4954 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2149161 |  4955 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  4956 | `		/* Compile the body */` |
|   2149161 |  4957 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2149161 |  4958 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4959 | `			return SXERR_ABORT;` |
|         - |  4960 | `		}` |
|   2149161 |  4961 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    426968 |  4962 | `			break;` |
|         - |  4963 | `		}` |
|         - |  4964 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1295235 |  4965 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1295235 |  4966 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|    911883 |  4967 | `			break;` |
|         - |  4968 | `		}` |
|         - |  4969 | `		/* Emit the unconditional jump */` |
|    383357 |  4970 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  4971 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    383357 |  4972 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    383357 |  4973 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    249075 |  4974 | `			pToken = &pGen->pIn[1];` |
|    249075 |  4975 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     86850 |  4976 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|     81134 |  4977 | `					break;` |
|         - |  4978 | `			}` |
|     86817 |  4979 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     43406 |  4980 | `		}` |
|    221099 |  4981 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  4982 | `		/* Synchronize cursors */` |
|    221099 |  4983 | `		pToken = pGen->pIn;` |
|         - |  4984 | `		/* Fix the false jump */` |
|    221099 |  4985 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  4986 | `	} /* For(;;) */` |
|         - |  4987 | `	/* Fix the false jump */` |
|   1928067 |  4988 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   1928067 |  4989 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1074136 |  4990 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  4991 | `			/* Compile the else block */` |
|    162263 |  4992 | `			pGen->pIn++;` |
|    162263 |  4993 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    162263 |  4994 | `			if( rc == SXERR_ABORT ){` |
|         - |  4995 |  |
|       ! 0 |  4996 | `				return SXERR_ABORT;` |
|         - |  4997 | `			}` |
|     81129 |  4998 | `	}` |
|   1928067 |  4999 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5000 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   1928067 |  5001 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5002 | `	/* Release the conditional block */` |
|   1928067 |  5003 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5004 | `	/* Statement successfully compiled */` |
|   1928067 |  5005 | `	return SXRET_OK;` |
|         4 |  5006 | `Synchronize:` |
|         - |  5007 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5008 | `	 */` |
|        67 |  5009 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5010 | `		pGen->pIn++;` |
|         3 |  5011 | `	}` |
|        11 |  5012 | `	return SXRET_OK;` |
|    964040 |  5013 | `}` |
|         - |  5014 | `/*` |
|         - |  5015 | ` * Compile the global construct.` |
|         - |  5016 | ` * According to the PHP language reference` |
|         - |  5017 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5018 | ` *  to be used in that function.` |
|         - |  5019 | ` *  Example #1 Using global` |
|         - |  5020 | ` *  <?php` |
|         - |  5021 | ` *   $a = 1;` |
|         - |  5022 | ` *   $b = 2;` |
|         - |  5023 | ` *   function Sum()` |
|         - |  5024 | ` *   {` |
|         - |  5025 | ` *    global $a, $b;` |
|         - |  5026 | ` *    $b = $a + $b;` |
|         - |  5027 | ` *   }` |
|         - |  5028 | ` *   Sum();` |
|         - |  5029 | ` *   echo $b;` |
|         - |  5030 | ` *  ?>` |
|         - |  5031 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5032 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5033 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5034 | ` */` |
|        38 |  5035 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5036 | `{` |
|        43 |  5037 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5038 | `	sxi32 nExpr;` |
|         - |  5039 | `	sxi32 rc;` |
|         - |  5040 | `	/* Jump the 'global' keyword */` |
|        43 |  5041 | `	pGen->pIn++;` |
|        43 |  5042 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5043 | `		/* Nothing to process */` |
|       ! 0 |  5044 | `		return SXRET_OK;` |
|         - |  5045 | `	}` |
|        43 |  5046 | `	pTmp = pGen->pEnd;` |
|        43 |  5047 | `	nExpr = 0;` |
|        91 |  5048 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5049 | `		if( pGen->pIn < pNext ){` |
|        53 |  5050 | `			pGen->pEnd = pNext;` |
|        53 |  5051 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5052 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5053 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5054 | `					return SXERR_ABORT;` |
|         - |  5055 | `				}` |
|       ! 0 |  5056 | `			}else{` |
|        53 |  5057 | `				pGen->pIn++;` |
|        53 |  5058 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5059 | `					/* Emit a warning */` |
|       ! 0 |  5060 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5061 | `				}else{` |
|        53 |  5062 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5063 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5064 | `						return SXERR_ABORT;` |
|        53 |  5065 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5066 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5067 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5068 | `							/* Variable name, not a constant */` |
|        53 |  5069 | `							pLast->iP1 = 0;` |
|        24 |  5070 | `						}` |
|        53 |  5071 | `						nExpr++;` |
|        24 |  5072 | `					}` |
|         - |  5073 | `				}` |
|         - |  5074 | `			}` |
|        24 |  5075 | `		}` |
|         - |  5076 | `		/* Next expression in the stream */` |
|        53 |  5077 | `		pGen->pIn = pNext;` |
|         - |  5078 | `		/* Jump trailing commas */` |
|        63 |  5079 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5080 | `			pGen->pIn++;` |
|         5 |  5081 | `		}` |
|         5 |  5082 | `	}` |
|         - |  5083 | `	/* Restore token stream */` |
|        43 |  5084 | `	pGen->pEnd = pTmp;` |
|        43 |  5085 | `	if( nExpr > 0 ){` |
|         - |  5086 | `		/* Emit the uplink instruction */` |
|        43 |  5087 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5088 | `	}` |
|        43 |  5089 | `	return SXRET_OK;` |
|        24 |  5090 | `}` |
|         - |  5091 | `/*` |
|         - |  5092 | ` * Compile the return statement.` |
|         - |  5093 | ` * According to the PHP language reference` |
|         - |  5094 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5095 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5096 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5097 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5098 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5099 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5100 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5101 | ` *  from within the main script file, then script execution end.` |
|         - |  5102 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5103 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5104 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5105 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5106 | ` */` |
|   2748488 |  5107 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5108 | `{` |
|   2748493 |  5109 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5110 | `	sxi32 rc;` |
|   2748493 |  5111 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2748493 |  5112 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5113 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5114 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5115 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5116 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5117 | `	 * normally below so token processing stays consistent. */` |
|   7123531 |  5118 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4375043 |  5119 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5120 | `	}` |
|   2748488 |  5121 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2748461 |  5122 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5123 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5124 | `			"A never-returning function must not return");` |
|         3 |  5125 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5126 | `			return SXERR_ABORT;` |
|         - |  5127 | `		}` |
|         1 |  5128 | `	}` |
|         - |  5129 | `	/* Jump the 'return' keyword */` |
|   2748493 |  5130 | `	pGen->pIn++;` |
|   2748493 |  5131 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5132 | `		/* Compile the expression */` |
|   2661651 |  5133 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2661651 |  5134 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5135 | `			return SXERR_ABORT;` |
|   2661651 |  5136 | `		}else if(rc != SXERR_EMPTY ){` |
|   2661651 |  5137 | `			nRet = 1;` |
|   1330823 |  5138 | `		}` |
|   1330823 |  5139 | `	}` |
|         - |  5140 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5141 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5142 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5143 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2748493 |  5144 | `	if( pGen->bInGenerator ){` |
|      3979 |  5145 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3979 |  5146 | `		return SXRET_OK;` |
|         - |  5147 | `	}` |
|         - |  5148 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5149 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5150 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5151 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5152 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2744519 |  5153 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2744519 |  5154 | `	return SXRET_OK;` |
|   1374249 |  5155 | `}` |
|         - |  5156 | `/*` |
|         - |  5157 | ` * Compile a yield expression.` |
|         - |  5158 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5159 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5160 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5161 | ` */` |
|     16168 |  5162 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5163 | `{` |
|         - |  5164 | `	SyToken *pTmp, *pSplit;` |
|     16173 |  5165 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     16173 |  5166 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5167 | `	sxi32 rc;` |
|      8084 |  5168 | `	(void)iCompileFlag;` |
|         - |  5169 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     16173 |  5170 | `	pGen->pIn++;` |
|         - |  5171 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5172 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5173 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5174 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5175 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     16168 |  5176 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      8119 |  5177 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5178 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5179 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5180 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5181 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5182 | `			return SXERR_ABORT;` |
|         - |  5183 | `		}` |
|        67 |  5184 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5185 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5186 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5187 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5188 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5189 | `				return SXERR_ABORT;` |
|         - |  5190 | `			}` |
|       ! 0 |  5191 | `		}` |
|        67 |  5192 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5193 | `		return SXRET_OK;` |
|         - |  5194 | `	}` |
|     16111 |  5195 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5196 | `		/* Bare yield — no value */` |
|         3 |  5197 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5198 | `		return SXRET_OK;` |
|         - |  5199 | `	}` |
|         - |  5200 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     16109 |  5201 | `	pSplit = 0;` |
|         - |  5202 | `	{` |
|     16109 |  5203 | `		SyToken *pCur = pGen->pIn;` |
|     16109 |  5204 | `		sxi32 nNest = 0;` |
|     48133 |  5205 | `		while( pCur < pGen->pEnd ){` |
|     47827 |  5206 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5207 | `				nNest++;` |
|     47819 |  5208 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5209 | `				nNest--;` |
|     47803 |  5210 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15803 |  5211 | `				pSplit = pCur;` |
|     15803 |  5212 | `				break;` |
|         - |  5213 | `			}` |
|     32029 |  5214 | `			pCur++;` |
|         5 |  5215 | `		}` |
|         - |  5216 | `	}` |
|     16109 |  5217 | `	pTmp = pGen->pEnd;` |
|     16109 |  5218 | `	if( pSplit ){` |
|         - |  5219 | `		/* yield $key => $value */` |
|     15803 |  5220 | `		pGen->pEnd = pSplit;` |
|     15803 |  5221 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15803 |  5222 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15803 |  5223 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15803 |  5224 | `		pGen->pEnd = pTmp;` |
|     15803 |  5225 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15803 |  5226 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15803 |  5227 | `		iP1 = 1;` |
|     15803 |  5228 | `		iP2 = 1;` |
|      7904 |  5229 | `	}else{` |
|         - |  5230 | `		/* yield $value */` |
|       311 |  5231 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       311 |  5232 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       311 |  5233 | `		if( rc != SXERR_EMPTY ){` |
|       311 |  5234 | `			iP1 = 1;` |
|       153 |  5235 | `		}` |
|         - |  5236 | `	}` |
|     16109 |  5237 | `	pGen->pEnd = pTmp;` |
|     16109 |  5238 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     16109 |  5239 | `	return SXRET_OK;` |
|      8089 |  5240 | `}` |
|         - |  5241 | `/*` |
|         - |  5242 | ` * Compile the die/exit language construct.` |
|         - |  5243 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5244 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5245 | ` */` |
|       128 |  5246 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5247 | `{` |
|       133 |  5248 | `	sxi32 nExpr = 0;` |
|         - |  5249 | `	sxi32 rc;` |
|         - |  5250 | `	/* Jump the die/exit keyword */` |
|       133 |  5251 | `	pGen->pIn++;` |
|       133 |  5252 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5253 | `		/* Compile the expression */` |
|       133 |  5254 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5255 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5256 | `			return SXERR_ABORT;` |
|       133 |  5257 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5258 | `			nExpr = 1;` |
|        64 |  5259 | `		}` |
|        64 |  5260 | `	}` |
|         - |  5261 | `	/* Emit the HALT instruction */` |
|       133 |  5262 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5263 | `	return SXRET_OK;` |
|        69 |  5264 | `}` |
|         - |  5265 | `/*` |
|         - |  5266 | ` * Compile the 'echo' language construct.` |
|         - |  5267 | ` */` |
|     17874 |  5268 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5269 | `{` |
|     17879 |  5270 | `	SyToken *pTmp,*pNext = 0;` |
|     17879 |  5271 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17879 |  5272 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17879 |  5273 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5274 | `	sxi32 rc;` |
|         - |  5275 | `	/* Jump the 'echo' keyword */` |
|     17879 |  5276 | `	pGen->pIn++;` |
|         - |  5277 | `	/* Compile arguments one after one */` |
|     17879 |  5278 | `	pTmp = pGen->pEnd;` |
|     44629 |  5279 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     26757 |  5280 | `		if( pGen->pIn < pNext ){` |
|     26757 |  5281 | `			pGen->pEnd = pNext;` |
|     26757 |  5282 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     26757 |  5283 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5284 | `				return SXERR_ABORT;` |
|     26757 |  5285 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5286 | `				/* Emit the consume instruction */` |
|     26733 |  5287 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     26733 |  5288 | `				nExpr++;` |
|     26733 |  5289 | `				bExpectMore = 0;` |
|     13364 |  5290 | `			}` |
|     13376 |  5291 | `		}` |
|         - |  5292 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5293 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     35641 |  5294 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      8891 |  5295 | `			if( bExpectMore ){` |
|         - |  5296 | `				/* two commas in a row */` |
|         3 |  5297 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5298 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5299 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5300 | `			}` |
|      8889 |  5301 | `			bExpectMore = 1;` |
|      8889 |  5302 | `			pNext++;` |
|         5 |  5303 | `		}` |
|     26755 |  5304 | `		pGen->pIn = pNext;` |
|         5 |  5305 | `	}` |
|         - |  5306 | `	/* Restore token stream */` |
|     17877 |  5307 | `	pGen->pEnd = pTmp;` |
|     17877 |  5308 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5309 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        32 |  5310 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5311 | `			"syntax error, unexpected token \";\"");` |
|        32 |  5312 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5313 | `	}` |
|     17849 |  5314 | `	return SXRET_OK;` |
|      8942 |  5315 | `}` |
|         - |  5316 | `/*` |
|         - |  5317 | ` * Compile the static statement.` |
|         - |  5318 | ` * According to the PHP language reference` |
|         - |  5319 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5320 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5321 | ` *  when program execution leaves this scope.` |
|         - |  5322 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5323 | ` * Symisc eXtension.` |
|         - |  5324 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5325 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5326 | ` *  Example` |
|         - |  5327 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5328 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5329 | ` */` |
|        12 |  5330 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         3 |  5331 | `{` |
|         - |  5332 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5333 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5334 | `	GenBlock *pBlock;` |
|         - |  5335 | `	SyString *pName;` |
|         - |  5336 | `	char *zDup;` |
|         - |  5337 | `	sxu32 nLine;` |
|         - |  5338 | `	sxi32 rc;` |
|         - |  5339 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5340 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5341 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|        12 |  5342 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|        10 |  5343 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5344 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5345 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5346 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5347 | `			return SXERR_ABORT;` |
|         3 |  5348 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5349 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5350 | `		}` |
|         3 |  5351 | `		return SXRET_OK;` |
|         - |  5352 | `	}` |
|         - |  5353 | `	/* Jump the static keyword */` |
|        13 |  5354 | `	nLine = pGen->pIn->nLine;` |
|        13 |  5355 | `	pGen->pIn++;` |
|         - |  5356 | `	/* Extract the enclosing function if any */` |
|        13 |  5357 | `	pBlock = pGen->pCurrent;` |
|        23 |  5358 | `	while( pBlock ){` |
|        23 |  5359 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|        13 |  5360 | `			break;` |
|         - |  5361 | `		}` |
|         - |  5362 | `		/* Point to the upper block */` |
|        13 |  5363 | `		pBlock = pBlock->pParent;` |
|         3 |  5364 | `	}` |
|        13 |  5365 | `	if( pBlock == 0 ){` |
|         - |  5366 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5367 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5368 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5369 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5370 | `				return SXERR_ABORT;` |
|         - |  5371 | `			}` |
|       ! 0 |  5372 | `			goto Synchronize;` |
|         - |  5373 | `		}` |
|         - |  5374 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5375 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5376 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5377 | `			return SXERR_ABORT;` |
|       ! 0 |  5378 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5379 | `			/* Emit the POP instruction */` |
|       ! 0 |  5380 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5381 | `		}` |
|       ! 0 |  5382 | `		return SXRET_OK;` |
|         - |  5383 | `	}` |
|        13 |  5384 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5385 | `	/* Make sure we are dealing with a valid statement */` |
|        13 |  5386 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|         8 |  5387 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5388 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5389 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5390 | `				return SXERR_ABORT;` |
|         - |  5391 | `			}` |
|         3 |  5392 | `			goto Synchronize;` |
|         - |  5393 | `	}` |
|        10 |  5394 | `	pGen->pIn++;` |
|         - |  5395 | `	/* Extract variable name */` |
|        10 |  5396 | `	pName = &pGen->pIn->sData;` |
|        10 |  5397 | `	pGen->pIn++; /* Jump the var name */` |
|        10 |  5398 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5399 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5400 | `		goto Synchronize;` |
|         - |  5401 | `	}` |
|         - |  5402 | `	/* Initialize the structure describing the static variable */` |
|        10 |  5403 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        10 |  5404 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5405 | `	/* Duplicate variable name */` |
|        10 |  5406 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        10 |  5407 | `	if( zDup == 0 ){` |
|       ! 0 |  5408 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5409 | `		return SXERR_ABORT;` |
|         - |  5410 | `	}` |
|        10 |  5411 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5412 | `	/* Check if we have an expression to compile */` |
|        10 |  5413 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5414 | `		SySet *pInstrContainer;` |
|         - |  5415 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5416 | `		 * Static variable can take any complex expression including function` |
|         - |  5417 | `		 * call as their initialization value.` |
|         - |  5418 | `		 * Example:` |
|         - |  5419 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5420 | `		 */` |
|        10 |  5421 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5422 | `		/* Swap bytecode container */` |
|        10 |  5423 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        10 |  5424 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5425 | `		/* Compile the expression */` |
|        10 |  5426 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5427 | `		/* Emit the done instruction */` |
|        10 |  5428 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5429 | `		/* Restore default bytecode container */` |
|        10 |  5430 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         4 |  5431 | `	}` |
|         - |  5432 | `	/* Finally save the compiled static variable in the appropriate container */` |
|        10 |  5433 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|        10 |  5434 | `	return SXRET_OK;` |
|         1 |  5435 | `Synchronize:` |
|         - |  5436 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5437 | `	 * statement.` |
|         - |  5438 | `	 */` |
|         5 |  5439 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5440 | `		pGen->pIn++;` |
|         1 |  5441 | `	}` |
|         3 |  5442 | `	return SXRET_OK;` |
|         9 |  5443 | `}` |
|         - |  5444 | `/*` |
|         - |  5445 | ` * Compile the var statement.` |
|         - |  5446 | ` * Symisc Extension:` |
|         - |  5447 | ` *      var statement can be used outside of a class definition.` |
|         - |  5448 | ` */` |
|         4 |  5449 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5450 | `{` |
|         - |  5451 | `	sxu32 nLine;` |
|         - |  5452 | `	sxi32 rc;` |
|         5 |  5453 | `	nLine = pGen->pIn->nLine;` |
|         - |  5454 | `	/* Jump the 'var' keyword */` |
|         5 |  5455 | `	pGen->pIn++;` |
|         5 |  5456 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5457 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5458 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5459 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5460 | `			pGen->pIn++;` |
|       ! 0 |  5461 | `		}` |
|       ! 0 |  5462 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5463 | `			return SXERR_ABORT;` |
|         - |  5464 | `		}` |
|       ! 0 |  5465 | `	}else{` |
|         - |  5466 | `		/* Compile the expression */` |
|         5 |  5467 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5468 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5469 | `			return SXERR_ABORT;` |
|         5 |  5470 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5471 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5472 | `		}` |
|         - |  5473 | `	}` |
|         5 |  5474 | `	return SXRET_OK;` |
|         3 |  5475 | `}` |
|         - |  5476 | `/*` |
|         - |  5477 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5478 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5479 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5480 | ` */` |
|         - |  5481 | `/*` |
|         - |  5482 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5483 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5484 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5485 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5486 | ` *` |
|         - |  5487 | ` * Resolution order:` |
|         - |  5488 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5489 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5490 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5491 | ` *` |
|         - |  5492 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5493 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5494 | ` * Returns the (possibly new) literal index.` |
|         - |  5495 | ` */` |
|   4996540 |  5496 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5497 | `{` |
|         - |  5498 | `	ph7_value *pLit;` |
|         - |  5499 | `	const char *zLit;` |
|         - |  5500 | `	SyString sQualified;` |
|         - |  5501 | `	sxu32 nLit;` |
|         - |  5502 | `	sxu32 k;` |
|         - |  5503 | `	sxu32 nNewIdx;` |
|         - |  5504 | `	int hasNsSep;` |
|         - |  5505 | `	SyHashEntry *pImport;` |
|         - |  5506 | `	ph7_value *pNew;` |
|   4996545 |  5507 | `	if( pFromImport ){` |
|   3943125 |  5508 | `		*pFromImport = 0;` |
|   1971560 |  5509 | `	}` |
|   4996545 |  5510 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   4996545 |  5511 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5512 | `		return nOrigIdx;` |
|         - |  5513 | `	}` |
|   4996545 |  5514 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   4996545 |  5515 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5516 | `	/* Skip if already qualified (contains backslash) */` |
|   4996545 |  5517 | `	hasNsSep = 0;` |
|  61171163 |  5518 | `	for( k = 0; k < nLit; k++ ){` |
|  56174631 |  5519 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  28087314 |  5520 | `	}` |
|   4996545 |  5521 | `	if( hasNsSep ){` |
|        10 |  5522 | `		return nOrigIdx;` |
|         - |  5523 | `	}` |
|         - |  5524 | `	/* Check use imports first (works even outside namespaces) */` |
|   4996537 |  5525 | `	SyBlobReset(&pGen->sWorker);` |
|   4996537 |  5526 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   4996537 |  5527 | `	if( pImport ){` |
|        41 |  5528 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5529 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5530 | `		if( pFromImport ){` |
|        18 |  5531 | `			*pFromImport = 1;` |
|         8 |  5532 | `		}` |
|        23 |  5533 | `	}else{` |
|   4996501 |  5534 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   4996411 |  5535 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5536 | `		}` |
|         - |  5537 | `		/* Prepend current namespace */` |
|        95 |  5538 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        95 |  5539 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|        95 |  5540 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5541 | `	}` |
|         - |  5542 | `	/* Look up or create a new literal for the qualified name */` |
|       131 |  5543 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       131 |  5544 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        57 |  5545 | `		return nNewIdx; /* Already interned */` |
|         - |  5546 | `	}` |
|        79 |  5547 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        79 |  5548 | `	if( pNew == 0 ){` |
|       ! 0 |  5549 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5550 | `	}` |
|        79 |  5551 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        79 |  5552 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        79 |  5553 | `	return nNewIdx;` |
|   2498275 |  5554 | `}` |
|         - |  5555 | `/*` |
|         - |  5556 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5557 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5558 | ` */` |
|    423714 |  5559 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5560 | `{` |
|         - |  5561 | `	SyHashEntry *pImport;` |
|         - |  5562 | `	/* Check use imports first */` |
|    423719 |  5563 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    423719 |  5564 | `	if( pImport ){` |
|        20 |  5565 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        20 |  5566 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        20 |  5567 | `		return;` |
|         - |  5568 | `	}` |
|         - |  5569 | `	/* Prepend current namespace if active */` |
|    423703 |  5570 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         8 |  5571 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  5572 | `		SyBlobAppend(pOut,"\\",1);` |
|         3 |  5573 | `	}` |
|    423703 |  5574 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    211862 |  5575 | `}` |
|         - |  5576 | `/*` |
|         - |  5577 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5578 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5579 | ` * The caller must release pOut when done.` |
|         - |  5580 | ` */` |
|    443906 |  5581 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5582 | `{` |
|    443911 |  5583 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      4009 |  5584 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      4009 |  5585 | `		SyBlobAppend(pOut,"\\",1);` |
|      2002 |  5586 | `	}` |
|    443911 |  5587 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    443911 |  5588 | `}` |
|         - |  5589 | `/*` |
|         - |  5590 | ` * Compile a namespace statement` |
|         - |  5591 | ` * According to the PHP language reference manual` |
|         - |  5592 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5593 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5594 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5595 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5596 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5597 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5598 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5599 | ` *  programming world.` |
|         - |  5600 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5601 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5602 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5603 | ` *  classes/functions/constants.` |
|         - |  5604 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5605 | ` *  readability of source code.` |
|         - |  5606 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5607 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5608 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5609 | ` *       class MyClass {}` |
|         - |  5610 | ` *       function myfunction() {}` |
|         - |  5611 | ` *       const MYCONST = 1;` |
|         - |  5612 | ` *       $a = new MyClass;` |
|         - |  5613 | ` *       $c = new \my\name\MyClass;` |
|         - |  5614 | ` *       $a = strlen('hi');` |
|         - |  5615 | ` *       $d = namespace\MYCONST;` |
|         - |  5616 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5617 | ` *       echo constant($d);` |
|         - |  5618 | ` * NOTE` |
|         - |  5619 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5620 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5621 | ` */` |
|         - |  5622 | `/*` |
|         - |  5623 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5624 | ` */` |
|        14 |  5625 | `static const char * TokenTypeName(sxu32 nType)` |
|         3 |  5626 | `{` |
|        17 |  5627 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        10 |  5628 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        10 |  5629 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        10 |  5630 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        10 |  5631 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        10 |  5632 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5633 | `	return "token";` |
|        10 |  5634 | `}` |
|      4052 |  5635 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5636 | `{` |
|         - |  5637 | `	sxu32 nLine;` |
|         - |  5638 | `	sxi32 rc;` |
|      4057 |  5639 | `	nLine = pGen->pIn->nLine;` |
|      4057 |  5640 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5641 | `	/* Reset namespace and clear previous use imports */` |
|      4057 |  5642 | `	SyBlobReset(&pGen->sNamespace);` |
|      4057 |  5643 | `	SyHashRelease(&pGen->hUseImports);` |
|      4057 |  5644 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      4057 |  5645 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      4057 |  5646 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      4057 |  5647 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      4057 |  5648 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      4057 |  5649 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5650 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5651 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5652 | `		return SXRET_OK;` |
|         - |  5653 | `	}` |
|      4057 |  5654 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5655 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5656 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5657 | `		return SXRET_OK;` |
|         - |  5658 | `	}` |
|      4057 |  5659 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5660 | `		/* namespace { } — global namespace block */` |
|       ! 0 |  5661 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5662 | `		return SXRET_OK;` |
|         - |  5663 | `	}` |
|         - |  5664 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      8151 |  5665 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      4099 |  5666 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5667 | `			/* Append backslash separator */` |
|        27 |  5668 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        27 |  5669 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        11 |  5670 | `			}` |
|        16 |  5671 | `		}else{` |
|         - |  5672 | `			/* Append identifier */` |
|      4077 |  5673 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5674 | `		}` |
|      4099 |  5675 | `		pGen->pIn++;` |
|         5 |  5676 | `	}` |
|         - |  5677 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5678 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5679 | `	{` |
|      4057 |  5680 | `		char *zNsDup = 0;` |
|      4057 |  5681 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      6080 |  5682 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      4050 |  5683 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      2025 |  5684 | `		}` |
|      4057 |  5685 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5686 | `	}` |
|      4057 |  5687 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5688 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5689 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5690 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5691 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5692 | `			return SXERR_ABORT;` |
|         - |  5693 | `		}` |
|         2 |  5694 | `	}` |
|      4057 |  5695 | `	return SXRET_OK;` |
|      2031 |  5696 | `}` |
|         - |  5697 | `/*` |
|         - |  5698 | ` * Compile the 'use' statement` |
|         - |  5699 | ` * According to the PHP language reference manual` |
|         - |  5700 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5701 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5702 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5703 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5704 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5705 | ` *  a function or constant is not supported.` |
|         - |  5706 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5707 | ` * NOTE` |
|         - |  5708 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5709 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5710 | ` */` |
|        72 |  5711 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5712 | `{` |
|         - |  5713 | `	sxu32 nLine;` |
|         - |  5714 | `	sxi32 rc;` |
|         - |  5715 | `	SyBlob sPath;` |
|         - |  5716 | `	SyString sAlias;` |
|         - |  5717 | `	SyToken *pLast;` |
|         - |  5718 | `	char *zDup;` |
|         - |  5719 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5720 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5721 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        77 |  5722 | `	nLine = pGen->pIn->nLine;` |
|        77 |  5723 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5724 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        77 |  5725 | `	iUseType = 0;` |
|        77 |  5726 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5727 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5728 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5729 | `			iUseType = 1;` |
|        16 |  5730 | `			pGen->pIn++;` |
|        23 |  5731 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5732 | `			iUseType = 2;` |
|        16 |  5733 | `			pGen->pIn++;` |
|         7 |  5734 | `		}` |
|        14 |  5735 | `	}` |
|         - |  5736 | `	/* Select target hash tables based on import type */` |
|        77 |  5737 | `	switch( iUseType ){` |
|         7 |  5738 | `		case 1:` |
|        16 |  5739 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5740 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5741 | `			break;` |
|         7 |  5742 | `		case 2:` |
|        16 |  5743 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5744 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5745 | `			break;` |
|        22 |  5746 | `		default:` |
|        49 |  5747 | `			pGenHash = &pGen->hUseImports;` |
|        49 |  5748 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        44 |  5749 | `			break;` |
|         - |  5750 | `	}` |
|        77 |  5751 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5752 | `	/* Process one or more use declarations separated by commas */` |
|        37 |  5753 | `	for(;;){` |
|        79 |  5754 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5755 | `			break;` |
|         - |  5756 | `		}` |
|        79 |  5757 | `		SyBlobReset(&sPath);` |
|        79 |  5758 | `		pLast = 0;` |
|         - |  5759 | `		/* Collect the full namespace path */` |
|       269 |  5760 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       195 |  5761 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       135 |  5762 | `				pLast = pGen->pIn;` |
|       135 |  5763 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        65 |  5764 | `					SyBlobAppend(&sPath,"\\",1);` |
|        30 |  5765 | `				}` |
|       135 |  5766 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        65 |  5767 | `			}` |
|       195 |  5768 | `			pGen->pIn++;` |
|         5 |  5769 | `		}` |
|        79 |  5770 | `		if( pLast == 0 ){` |
|         - |  5771 | `			/* Empty path */` |
|         6 |  5772 | `			break;` |
|         - |  5773 | `		}` |
|         - |  5774 | `		/* Default alias is the last component of the path */` |
|        75 |  5775 | `		sAlias = pLast->sData;` |
|         - |  5776 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        70 |  5777 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        50 |  5778 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        23 |  5779 | `			pGen->pIn++; /* Jump 'as' */` |
|        23 |  5780 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        23 |  5781 | `				sAlias = pGen->pIn->sData;` |
|        23 |  5782 | `				pGen->pIn++;` |
|        10 |  5783 | `			}` |
|        10 |  5784 | `		}` |
|         - |  5785 | `		/* Check for duplicate import alias (per-type) */` |
|        75 |  5786 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  5787 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  5788 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  5789 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  5790 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5791 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  5792 | `				return SXERR_ABORT;` |
|         - |  5793 | `			}` |
|         2 |  5794 | `		}` |
|         - |  5795 | `		/* Register the import: alias -> FQN.` |
|         - |  5796 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  5797 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  5798 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       110 |  5799 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        70 |  5800 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        75 |  5801 | `		if( zDup ){` |
|        75 |  5802 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        75 |  5803 | `			if( pVmHash ){` |
|         - |  5804 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  5805 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        47 |  5806 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        47 |  5807 | `				if( zAliasDup ){` |
|        47 |  5808 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        21 |  5809 | `				}` |
|        21 |  5810 | `			}` |
|        75 |  5811 | `			if( iUseType == 2 ){` |
|         - |  5812 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  5813 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  5814 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  5815 | `				if( zAliasDup ){` |
|         - |  5816 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  5817 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  5818 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  5819 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  5820 | `					if( azPair ){` |
|        16 |  5821 | `						azPair[0] = zAliasDup;` |
|        16 |  5822 | `						azPair[1] = zDup;` |
|        16 |  5823 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  5824 | `					}` |
|         7 |  5825 | `				}` |
|         7 |  5826 | `			}` |
|        35 |  5827 | `		}` |
|         - |  5828 | `		/* Check for comma (multiple use declarations) */` |
|        75 |  5829 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  5830 | `			pGen->pIn++;` |
|         2 |  5831 | `		}else{` |
|        39 |  5832 | `			break;` |
|         - |  5833 | `		}` |
|         1 |  5834 | `	}` |
|        77 |  5835 | `	SyBlobRelease(&sPath);` |
|        77 |  5836 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  5837 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  5838 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  5839 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5840 | `			return SXERR_ABORT;` |
|         - |  5841 | `		}` |
|         1 |  5842 | `	}` |
|        77 |  5843 | `	return SXRET_OK;` |
|        41 |  5844 | `}` |
|         - |  5845 | `/*` |
|         - |  5846 | ` * Compile the stupid 'declare' language construct.` |
|         - |  5847 | ` *` |
|         - |  5848 | ` * According to the PHP language reference manual.` |
|         - |  5849 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  5850 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  5851 | ` *  declare (directive)` |
|         - |  5852 | ` *   statement` |
|         - |  5853 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  5854 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  5855 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  5856 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  5857 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  5858 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  5859 | ` * <?php` |
|         - |  5860 | ` * // these are the same:` |
|         - |  5861 | ` * // you can use this:` |
|         - |  5862 | ` * declare(ticks=1) {` |
|         - |  5863 | ` *   // entire script here` |
|         - |  5864 | ` * }` |
|         - |  5865 | ` * // or you can use this:` |
|         - |  5866 | ` * declare(ticks=1);` |
|         - |  5867 | ` * // entire script here` |
|         - |  5868 | ` * ?>` |
|         - |  5869 | ` *` |
|         - |  5870 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  5871 | ` */` |
|         - |  5872 | `/*` |
|         - |  5873 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  5874 | ` */` |
|        72 |  5875 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  5876 | `{` |
|       109 |  5877 | `	return SyStringLength(pName) == nWant` |
|        72 |  5878 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  5879 | `}` |
|         - |  5880 |  |
|        42 |  5881 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  5882 | `{` |
|        47 |  5883 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  5884 | `	SyToken *pBodyEnd = 0;` |
|         - |  5885 | `	SyToken *pBodyStart;` |
|         - |  5886 | `	SyToken *pCursor;` |
|         - |  5887 | `	int bHasStrictTypes;` |
|         - |  5888 | `	int bBlockForm;` |
|         - |  5889 | `	int bPlacementOk;` |
|         - |  5890 | `	sxi32 rc;` |
|        47 |  5891 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  5892 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  5893 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|         6 |  5894 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5895 | `			return SXERR_ABORT;` |
|         - |  5896 | `		}` |
|         6 |  5897 | `		goto Synchro;` |
|         - |  5898 | `	}` |
|        43 |  5899 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  5900 | `	pBodyStart = pGen->pIn;` |
|         - |  5901 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  5902 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  5903 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  5904 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  5905 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5906 | `			return SXERR_ABORT;` |
|         - |  5907 | `		}` |
|       ! 0 |  5908 | `		return SXRET_OK;` |
|         - |  5909 | `	}` |
|         - |  5910 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  5911 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  5912 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  5913 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  5914 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  5915 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5916 | `			return SXERR_ABORT;` |
|         - |  5917 | `		}` |
|       ! 0 |  5918 | `	}` |
|        43 |  5919 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  5920 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  5921 | `	bHasStrictTypes = 0;` |
|         - |  5922 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  5923 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  5924 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  5925 | `	pCursor = pBodyStart;` |
|        55 |  5926 | `	while( pCursor < pBodyEnd ){` |
|        51 |  5927 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  5928 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  5929 | `				bHasStrictTypes = 1;` |
|        39 |  5930 | `				break;` |
|         - |  5931 | `			}` |
|         2 |  5932 | `		}` |
|        14 |  5933 | `		pCursor++;` |
|         2 |  5934 | `	}` |
|        43 |  5935 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  5936 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5937 | `			"strict_types declaration must not use block mode");` |
|         3 |  5938 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  5939 | `		return SXRET_OK;` |
|         - |  5940 | `	}` |
|        41 |  5941 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  5942 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5943 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  5944 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  5945 | `		return SXRET_OK;` |
|         - |  5946 | `	}` |
|         - |  5947 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  5948 | `	pCursor = pBodyStart;` |
|        69 |  5949 | `	while( pCursor < pBodyEnd ){` |
|         - |  5950 | `		SyToken *pNameTok;` |
|         - |  5951 | `		SyToken *pEqTok;` |
|         - |  5952 | `		SyToken *pValTok;` |
|         - |  5953 | `		SyString *pDirName;` |
|         - |  5954 | `		int bIsStrict;` |
|         - |  5955 | `		int iStrictValue;` |
|        39 |  5956 | `		pNameTok = pCursor;` |
|        39 |  5957 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  5958 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5959 | `				"declare: Expecting a directive name");` |
|       ! 0 |  5960 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5961 | `			return SXRET_OK;` |
|         - |  5962 | `		}` |
|        39 |  5963 | `		pEqTok = pNameTok + 1;` |
|        39 |  5964 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  5965 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5966 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  5967 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5968 | `			return SXRET_OK;` |
|         - |  5969 | `		}` |
|        39 |  5970 | `		pValTok = pEqTok + 1;` |
|        39 |  5971 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  5972 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5973 | `				"declare: Expecting value after '='");` |
|       ! 0 |  5974 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5975 | `			return SXRET_OK;` |
|         - |  5976 | `		}` |
|        39 |  5977 | `		pDirName = &pNameTok->sData;` |
|        39 |  5978 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  5979 | `		if( bIsStrict ){` |
|         - |  5980 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  5981 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  5982 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  5983 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5984 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  5985 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5986 | `				return SXRET_OK;` |
|         - |  5987 | `			}` |
|        35 |  5988 | `			iStrictValue = -1;` |
|        35 |  5989 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  5990 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  5991 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  5992 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  5993 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  5994 | `			}` |
|        35 |  5995 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  5996 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5997 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  5998 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  5999 | `				return SXRET_OK;` |
|         - |  6000 | `			}` |
|        32 |  6001 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6002 | `		}else{` |
|         - |  6003 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6004 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6005 | `			 * behavior don't regress. */` |
|         8 |  6006 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6007 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6008 | `				ph7_lib_version()` |
|         - |  6009 | `				);` |
|         - |  6010 | `		}` |
|        36 |  6011 | `		pCursor = pValTok + 1;` |
|         - |  6012 | `		/* Consume separating comma (or end). */` |
|        36 |  6013 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6014 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6015 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6016 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6017 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6018 | `				return SXRET_OK;` |
|         - |  6019 | `			}` |
|         3 |  6020 | `			pCursor++;` |
|         1 |  6021 | `		}` |
|         4 |  6022 | `	}` |
|         - |  6023 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6024 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6025 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        34 |  6026 | `	return SXRET_OK;` |
|         2 |  6027 | `Synchro:` |
|         - |  6028 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6029 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6030 | `		pGen->pIn++;` |
|         2 |  6031 | `	}` |
|         6 |  6032 | `	return SXRET_OK;` |
|        26 |  6033 | `}` |
|         - |  6034 | `/*` |
|         - |  6035 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6036 | ` * as follows:` |
|         - |  6037 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6038 | ` * {` |
|         - |  6039 | ` *   return "Making a cup of $type.\n";` |
|         - |  6040 | ` * }` |
|         - |  6041 | ` * Symisc eXtension.` |
|         - |  6042 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6043 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6044 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6045 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6046 | ` *      {` |
|         - |  6047 | ` *       var_dump($a);` |
|         - |  6048 | ` *      }` |
|         - |  6049 | ` *     //call test without args` |
|         - |  6050 | ` *      test();` |
|         - |  6051 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6052 | ` *      Example:` |
|         - |  6053 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6054 | ` * 3 -) Function overloading!!` |
|         - |  6055 | ` *      Example:` |
|         - |  6056 | ` *      function foo($a) {` |
|         - |  6057 | ` *   	  return $a.PHP_EOL;` |
|         - |  6058 | ` *	    }` |
|         - |  6059 | ` *	    function foo($a, $b) {` |
|         - |  6060 | ` *   	  return $a + $b;` |
|         - |  6061 | ` *	    }` |
|         - |  6062 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6063 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6064 | ` *      // Same arg` |
|         - |  6065 | ` *	   function foo(string $a)` |
|         - |  6066 | ` *	   {` |
|         - |  6067 | ` *	     echo "a is a string\n";` |
|         - |  6068 | ` *	     var_dump($a);` |
|         - |  6069 | ` *	   }` |
|         - |  6070 | ` *	  function foo(int $a)` |
|         - |  6071 | ` *	  {` |
|         - |  6072 | ` *	    echo "a is integer\n";` |
|         - |  6073 | ` *	    var_dump($a);` |
|         - |  6074 | ` *	  }` |
|         - |  6075 | ` *	  function foo(array $a)` |
|         - |  6076 | ` *	  {` |
|         - |  6077 | ` * 	    echo "a is an array\n";` |
|         - |  6078 | ` * 	    var_dump($a);` |
|         - |  6079 | ` *	  }` |
|         - |  6080 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6081 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6082 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6083 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6084 | ` * introduced by the PH7 engine.` |
|         - |  6085 | ` */` |
|    469732 |  6086 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6087 | `{` |
|         - |  6088 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6089 | `	SySet *pInstrContainer;` |
|         - |  6090 | `	sxi32 rc;` |
|         - |  6091 | `	/* Swap token stream */` |
|    469737 |  6092 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    469737 |  6093 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    469737 |  6094 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6095 | `	/* Compile the expression holding the argument value */` |
|    469737 |  6096 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6097 | `	/* Emit the done instruction */` |
|    469737 |  6098 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    469737 |  6099 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    469737 |  6100 | `	RE_SWAP_DELIMITER(pGen);` |
|    469737 |  6101 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6102 | `		return SXERR_ABORT;` |
|         - |  6103 | `	}` |
|    469737 |  6104 | `	return SXRET_OK;` |
|    234871 |  6105 | `}` |
|         - |  6106 | `/*` |
|         - |  6107 | ` * Collect function arguments one after one.` |
|         - |  6108 | ` * According to the PHP language reference manual.` |
|         - |  6109 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6110 | ` * list of expressions.` |
|         - |  6111 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6112 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6113 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6114 | ` * for more information.` |
|         - |  6115 | ` * Example #1 Passing arrays to functions` |
|         - |  6116 | ` * <?php` |
|         - |  6117 | ` * function takes_array($input)` |
|         - |  6118 | ` * {` |
|         - |  6119 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6120 | ` * }` |
|         - |  6121 | ` * ?>` |
|         - |  6122 | ` * Making arguments be passed by reference` |
|         - |  6123 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6124 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6125 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6126 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6127 | ` * to the argument name in the function definition:` |
|         - |  6128 | ` * Example #2 Passing function parameters by reference` |
|         - |  6129 | ` * <?php` |
|         - |  6130 | ` * function add_some_extra(&$string)` |
|         - |  6131 | ` * {` |
|         - |  6132 | ` *   $string .= 'and something extra.';` |
|         - |  6133 | ` * }` |
|         - |  6134 | ` * $str = 'This is a string, ';` |
|         - |  6135 | ` * add_some_extra($str);` |
|         - |  6136 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6137 | ` * ?>` |
|         - |  6138 | ` *` |
|         - |  6139 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6140 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6141 | ` * on these extension.` |
|         - |  6142 | ` */` |
|   1134406 |  6143 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6144 | `{` |
|         - |  6145 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6146 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6147 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6148 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6149 | `	sxi32 rc;` |
|         - |  6150 |  |
|   1134411 |  6151 | `	pIn = pGen->pIn;` |
|   1134411 |  6152 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6153 | `	/* Process arguments one after one */` |
|   1422934 |  6154 | `	for(;;){` |
|   2845873 |  6155 | `		if( pIn >= pEnd ){` |
|         - |  6156 | `			/* No more arguments to process */` |
|   1134395 |  6157 | `			break;` |
|         - |  6158 | `		}` |
|   1711483 |  6159 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   1711483 |  6160 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   1711483 |  6161 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   1711483 |  6162 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   1711483 |  6163 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6164 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6165 | `		 * first token inside the main token stream */` |
|   1711483 |  6166 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6167 | `			return SXERR_ABORT;` |
|         - |  6168 | `		}` |
|         - |  6169 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6170 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6171 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6172 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6173 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6174 | `		{` |
|   1711483 |  6175 | `			int bReadonly = 0, bVisSeen = 0;` |
|   1711483 |  6176 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   1711483 |  6177 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6178 | `			int nSetTok;` |
|         - |  6179 | `			sxi32 nSetVis;` |
|   1711483 |  6180 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6181 | `				bReadonly = 1;` |
|         3 |  6182 | `				pIn++;` |
|         1 |  6183 | `			}` |
|   1711483 |  6184 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   1711483 |  6185 | `			if( nSetVis ){` |
|         - |  6186 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6187 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6188 | `				bVisSeen = 1;` |
|         3 |  6189 | `				pIn += nSetTok;` |
|         3 |  6190 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6191 | `					bReadonly = 1;` |
|       ! 0 |  6192 | `					pIn++;` |
|         1 |  6193 | `				}` |
|   1711482 |  6194 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     91157 |  6195 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     91157 |  6196 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6197 | `					bVisSeen = 1;` |
|        89 |  6198 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6199 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6200 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6201 | `					pIn++;` |
|        89 |  6202 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6203 | `					if( nSetVis ){` |
|         - |  6204 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6205 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6206 | `						pIn += nSetTok;` |
|         1 |  6207 | `					}` |
|        89 |  6208 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6209 | `						bReadonly = 1;` |
|        18 |  6210 | `						pIn++;` |
|         7 |  6211 | `					}` |
|        42 |  6212 | `				}` |
|     45576 |  6213 | `			}` |
|   1711483 |  6214 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6215 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   1711481 |  6216 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6217 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6218 | `			}` |
|   1711483 |  6219 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6220 | `				if( !bCtorCtx ){` |
|         6 |  6221 | `					if( bAbstractCtx ){` |
|         3 |  6222 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6223 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6224 | `					}else{` |
|         3 |  6225 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6226 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6227 | `					}` |
|         6 |  6228 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6229 | `						return SXERR_ABORT;` |
|         - |  6230 | `					}` |
|         6 |  6231 | `					return SXERR_SYNTAX;` |
|         - |  6232 | `				}` |
|        89 |  6233 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6234 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6235 | `				if( bReadonly ){` |
|        20 |  6236 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6237 | `				}` |
|        42 |  6238 | `			}` |
|         - |  6239 | `		}` |
|         - |  6240 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   1711474 |  6241 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|    931025 |  6242 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    144645 |  6243 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    118897 |  6244 | `			sxu32 nLineLocal = pIn->nLine;` |
|    118897 |  6245 | `			sxi32 iTFlags = 0;` |
|    118897 |  6246 | `			pGen->pIn = pIn;` |
|    118897 |  6247 | `			rc = GenStateParseUnionTypeDecl(` |
|     59446 |  6248 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     59446 |  6249 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6250 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6251 | `				/* bAllowVoid */ 0,` |
|     59446 |  6252 | `						nLineLocal);` |
|    118897 |  6253 | `			pIn = pGen->pIn;` |
|    118897 |  6254 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6255 | `				return SXERR_ABORT;` |
|    118897 |  6256 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6257 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6258 | `				return SXERR_SYNTAX;` |
|    118895 |  6259 | `			}else if( rc == SXERR_SYNTAX ){` |
|        12 |  6260 | `				if( pIn < pEnd ){` |
|        16 |  6261 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6262 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6263 | `						&pIn->sData);` |
|         8 |  6264 | `				}else{` |
|       ! 0 |  6265 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6266 | `						"syntax error, unexpected end of file");` |
|         - |  6267 | `				}` |
|        12 |  6268 | `				return SXERR_SYNTAX;` |
|         - |  6269 | `			}` |
|    118887 |  6270 | `			sArg.iFlags \|= iTFlags;` |
|     59441 |  6271 | `		}` |
|   1711469 |  6272 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6273 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6274 | `			return rc;` |
|         - |  6275 | `		}` |
|   1711469 |  6276 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6277 | `			/* Pass by reference,record that */` |
|     11885 |  6278 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     11885 |  6279 | `			pIn++;` |
|      5940 |  6280 | `		}` |
|   1711469 |  6281 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6282 | `			/* Variadic parameter: ...$args */` |
|     19839 |  6283 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     19839 |  6284 | `			pIn++;` |
|      9917 |  6285 | `		}` |
|   1711469 |  6286 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6287 | `			/* Invalid argument */` |
|       ! 0 |  6288 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6289 | `			return rc;` |
|         - |  6290 | `		}` |
|   1711469 |  6291 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6292 | `		/* Copy argument name */` |
|   1711469 |  6293 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   1711469 |  6294 | `		if( zDup == 0 ){` |
|       ! 0 |  6295 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6296 | `			return SXERR_ABORT;` |
|         - |  6297 | `		}` |
|   1711469 |  6298 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   1711469 |  6299 | `		pIn++;` |
|   1711469 |  6300 | `		if( pIn < pEnd ){` |
|    884981 |  6301 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6302 | `				SyToken *pDefend;` |
|    469739 |  6303 | `				sxi32 iNest = 0;` |
|    469739 |  6304 | `				pIn++; /* Jump the equal sign */` |
|    469739 |  6305 | `				pDefend = pIn;` |
|         - |  6306 | `				/* Process the default value associated with this argument */` |
|    990797 |  6307 | `				while( pDefend < pEnd ){` |
|    682899 |  6308 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    161841 |  6309 | `						break;` |
|         - |  6310 | `					}` |
|    521063 |  6311 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6312 | `						/* Increment nesting level */` |
|     27635 |  6313 | `						iNest++;` |
|    507248 |  6314 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6315 | `						/* Decrement nesting level */` |
|     27635 |  6316 | `						iNest--;` |
|     13815 |  6317 | `					}` |
|    521063 |  6318 | `					pDefend++;` |
|         5 |  6319 | `				}` |
|    469739 |  6320 | `				if( pIn >= pDefend ){` |
|         3 |  6321 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6322 | `					return rc;` |
|         - |  6323 | `				}` |
|         - |  6324 | `				/* Process default value */` |
|    469737 |  6325 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    469737 |  6326 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6327 | `					return rc;` |
|         - |  6328 | `				}` |
|         - |  6329 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6330 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6331 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6332 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6333 | `				 * arg-type check lets null through. */` |
|    469732 |  6334 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    260538 |  6335 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    260535 |  6336 | `					&& &pIn[1] == pDefend` |
|     47391 |  6337 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     35536 |  6338 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     21714 |  6339 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15795 |  6340 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6341 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6342 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6343 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6344 | `					 * already up at this point). */` |
|         - |  6345 | `					{` |
|     15795 |  6346 | `						const char *zSep = "";` |
|     15795 |  6347 | `						SyString sCls = { "", 0 };` |
|     15795 |  6348 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15789 |  6349 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15789 |  6350 | `							zSep = "::";` |
|      7892 |  6351 | `						}` |
|     23690 |  6352 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6353 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7895 |  6354 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6355 | `					}` |
|      7895 |  6356 | `				}` |
|         - |  6357 | `				/* Point beyond the default value */` |
|    469737 |  6358 | `				pIn = pDefend;` |
|    234866 |  6359 | `			}` |
|    884979 |  6360 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6361 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6362 | `				return rc;` |
|         - |  6363 | `			}` |
|    884979 |  6364 | `			pIn++; /* Jump the trailing comma */` |
|    442487 |  6365 | `		}` |
|         - |  6366 | `		/* Append argument signature */` |
|   1711467 |  6367 | `		if( sArg.nType > 0 ){` |
|    118825 |  6368 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6369 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     27707 |  6370 | `				int marker = 'o';` |
|     27707 |  6371 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     27707 |  6372 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13856 |  6373 | `			}else{` |
|         - |  6374 | `				int c;` |
|     91123 |  6375 | `				c = 'n'; /* cc warning */` |
|         - |  6376 | `				/* Type leading character */` |
|     91123 |  6377 | `				switch(sArg.nType){` |
|      5925 |  6378 | `				case MEMOBJ_HASHMAP:` |
|         - |  6379 | `					/* Hashmap aka 'array' */` |
|     11855 |  6380 | `					c = 'h';` |
|     11855 |  6381 | `					break;` |
|      9985 |  6382 | `				case MEMOBJ_INT:` |
|         - |  6383 | `					/* Integer */` |
|     19975 |  6384 | `					c = 'i';` |
|     19975 |  6385 | `					break;` |
|         2 |  6386 | `				case MEMOBJ_BOOL:` |
|         - |  6387 | `					/* Bool */` |
|         5 |  6388 | `					c = 'b';` |
|         5 |  6389 | `					break;` |
|         5 |  6390 | `				case MEMOBJ_REAL:` |
|         - |  6391 | `					/* Float */` |
|        12 |  6392 | `					c = 'f';` |
|        12 |  6393 | `					break;` |
|     29634 |  6394 | `				case MEMOBJ_STRING:` |
|         - |  6395 | `					/* String */` |
|     59273 |  6396 | `					c = 's';` |
|     59273 |  6397 | `					break;` |
|         7 |  6398 | `				case MEMOBJ_OBJ:` |
|         - |  6399 | `					/* Object */` |
|        16 |  6400 | `					c = 'o';` |
|        14 |  6401 | `					break;` |
|         1 |  6402 | `				default:` |
|         2 |  6403 | `					break;` |
|         - |  6404 | `				}` |
|     91123 |  6405 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6406 | `			}` |
|     59415 |  6407 | `		}else{` |
|         - |  6408 | `			/* No type is associated with this parameter which mean` |
|         - |  6409 | `			 * that this function is not condidate for overloading.` |
|         - |  6410 | `			 */` |
|   1592647 |  6411 | `			SyBlobRelease(&sSig);` |
|         - |  6412 | `		}` |
|         - |  6413 | `		/* Save in the argument set */` |
|   1711467 |  6414 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6415 | `	}` |
|   1134395 |  6416 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6417 | `		/* Save function signature */` |
|     87185 |  6418 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     43590 |  6419 | `	}` |
|   1134395 |  6420 | `	return SXRET_OK;` |
|    567208 |  6421 | `}` |
|         - |  6422 | `/*` |
|         - |  6423 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6424 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6425 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6426 | ` */` |
|     35556 |  6427 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6428 | `{` |
|     35561 |  6429 | `	sxi32 iParen = 0;` |
|     35561 |  6430 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6431 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6432 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6433 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    158073 |  6434 | `	while( pIn < pEnd ){` |
|    158073 |  6435 | `		sxu32 t = pIn->nType;` |
|    158073 |  6436 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    154073 |  6437 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    106667 |  6438 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     86895 |  6439 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    122517 |  6440 | `		pIn++;` |
|         5 |  6441 | `	}` |
|     19777 |  6442 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6443 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6444 | `	{` |
|     19777 |  6445 | `		sxi32 d = 0;` |
|    785679 |  6446 | `		while( pIn < pEnd ){` |
|    785679 |  6447 | `			sxu32 t = pIn->nType;` |
|    785679 |  6448 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    754065 |  6449 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    765907 |  6450 | `			pIn++;` |
|         5 |  6451 | `		}` |
|         - |  6452 | `	}` |
|     19777 |  6453 | `	return pIn;` |
|     17783 |  6454 | `}` |
|         - |  6455 | `/*` |
|         - |  6456 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6457 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6458 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6459 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6460 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6461 | ` * detached-mini-program path untouched.` |
|         - |  6462 | ` */` |
|         - |  6463 | `/*` |
|         - |  6464 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6465 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6466 | ` * mixed, object.` |
|         - |  6467 | ` */` |
|     11866 |  6468 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6469 | `{` |
|         - |  6470 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6471 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6472 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6473 | `	};` |
|         - |  6474 | `	sxu32 i;` |
|     11871 |  6475 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6476 | `		zName++;` |
|       ! 0 |  6477 | `		nName--;` |
|       ! 0 |  6478 | `	}` |
|     11903 |  6479 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11899 |  6480 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11867 |  6481 | `			return 1;` |
|         - |  6482 | `		}` |
|        17 |  6483 | `	}` |
|         5 |  6484 | `	return 0;` |
|      5938 |  6485 | `}` |
|         - |  6486 | `/*` |
|         - |  6487 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6488 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6489 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6490 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6491 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6492 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6493 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6494 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6495 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6496 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6497 | ` */` |
|     11864 |  6498 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6499 | `{` |
|     11869 |  6500 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6501 | ``		return 1; /* bare `object` */`` |
|         - |  6502 | `	}` |
|     11869 |  6503 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6504 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6505 | `	}` |
|     11867 |  6506 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11863 |  6507 | `		return 1;` |
|         - |  6508 | `	}` |
|         - |  6509 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6510 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6511 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6512 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6513 | `	{` |
|         - |  6514 | `		SyBlob sFQN;` |
|         - |  6515 | `		int bOk;` |
|         5 |  6516 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         5 |  6517 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|         5 |  6518 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|         5 |  6519 | `		SyBlobRelease(&sFQN);` |
|         5 |  6520 | `		return bOk;` |
|         - |  6521 | `	}` |
|      5937 |  6522 | `}` |
|         - |  6523 | `/*` |
|         - |  6524 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6525 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6526 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6527 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6528 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6529 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6530 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6531 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6532 | ` */` |
|     12102 |  6533 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6534 | `{` |
|     12107 |  6535 | `	int bOk = 0;` |
|         - |  6536 | `	sxu32 nLine;` |
|         - |  6537 | `	sxi32 rc;` |
|     12107 |  6538 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       243 |  6539 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6540 | `	}` |
|     11869 |  6541 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6542 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6543 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6544 | `		sxu32 i,j;` |
|       ! 0 |  6545 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6546 | `			int bGroupOk;` |
|       ! 0 |  6547 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6548 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6549 | `			}` |
|       ! 0 |  6550 | `			bGroupOk = 1;` |
|       ! 0 |  6551 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6552 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6553 | `					bGroupOk = 0;` |
|       ! 0 |  6554 | `					break;` |
|         - |  6555 | `				}` |
|       ! 0 |  6556 | `			}` |
|       ! 0 |  6557 | `			bOk = bGroupOk;` |
|       ! 0 |  6558 | `		}` |
|       ! 0 |  6559 | `	}else{` |
|     11869 |  6560 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6561 | `	}` |
|     11869 |  6562 | `	if( bOk ){` |
|     11867 |  6563 | `		return SXRET_OK;` |
|         - |  6564 | `	}` |
|         - |  6565 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6566 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6567 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6568 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6569 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6570 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6571 | `	{` |
|         3 |  6572 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6573 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6574 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6575 | `		}` |
|         3 |  6576 | `		if( sGiven.nByte < 1 ){` |
|         - |  6577 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6578 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6579 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6580 | `			const char *zScalar =` |
|       ! 0 |  6581 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6582 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6583 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6584 | `		}` |
|         3 |  6585 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6586 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6587 | `	}` |
|         3 |  6588 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      6056 |  6589 | `}` |
|   2631572 |  6590 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6591 | `{` |
|   2631577 |  6592 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2631577 |  6593 | `	SyToken *pEnd = pGen->pEnd;` |
|   2631577 |  6594 | `	sxi32 iDepth = 0;` |
|   2631577 |  6595 | `	int bStarted = 0;` |
| 116131531 |  6596 | `	while( pIn < pEnd ){` |
| 116131531 |  6597 | `		sxu32 t = pIn->nType;` |
| 116131531 |  6598 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 110803877 |  6599 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 105512115 |  6600 | `		if( t & PH7_TK_KEYWORD ){` |
|   7699975 |  6601 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   7699975 |  6602 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   7687873 |  6603 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6604 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   3826156 |  6605 | `		}` |
| 105464457 |  6606 | `		pIn++;` |
|         5 |  6607 | `	}` |
|   2619475 |  6608 | `	return FALSE;` |
|   1315791 |  6609 | `}` |
|         - |  6610 | `/*` |
|         - |  6611 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6612 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6613 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6614 | ` */` |
|   2631572 |  6615 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6616 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6617 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6618 | `	)` |
|         5 |  6619 | `{` |
|         - |  6620 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6621 | `	GenBlock *pBlock;` |
|         - |  6622 | `	sxu32 nGotoOfft;` |
|         - |  6623 | `	sxi32 rc;` |
|         - |  6624 | `	/* Attach the new function */` |
|   2631577 |  6625 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2631577 |  6626 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6627 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6628 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6629 | `		return SXERR_ABORT;` |
|         - |  6630 | `	}` |
|   2631577 |  6631 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6632 | `	/* Swap bytecode containers */` |
|   2631577 |  6633 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2631577 |  6634 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6635 | `	/* Emit constructor property promotion prologue:` |
|         - |  6636 | `	 *   $this->NAME = $NAME;` |
|         - |  6637 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6638 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6639 | `	{` |
|   2631577 |  6640 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6641 | `		sxu32 i;` |
|   4287669 |  6642 | `		for( i = 0; i < nArg; i++ ){` |
|   1656097 |  6643 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6644 | `			char *zSrc;` |
|         - |  6645 | `			sxu32 nSrc,nName;` |
|         - |  6646 | `			SySet sToken;` |
|         - |  6647 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6648 | `			sxi32 rcPromote;` |
|   1656097 |  6649 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1656023 |  6650 | `				continue;` |
|         - |  6651 | `			}` |
|         - |  6652 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6653 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6654 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6655 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6656 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6657 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6658 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6659 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6660 | `			if( zSrc == 0 ){` |
|       ! 0 |  6661 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6662 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6663 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6664 | `				return SXERR_ABORT;` |
|         - |  6665 | `			}` |
|         - |  6666 | `			{` |
|        79 |  6667 | `				char *z = zSrc;` |
|        79 |  6668 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6669 | `				z += sizeof("$this->")-1;` |
|        79 |  6670 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6671 | `				z += nName;` |
|        79 |  6672 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6673 | `				z += sizeof(" = $")-1;` |
|        79 |  6674 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6675 | `				z += nName;` |
|        79 |  6676 | `				*z = 0;` |
|         - |  6677 | `			}` |
|        79 |  6678 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6679 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6680 | `			pTmpIn = pGen->pIn;` |
|        79 |  6681 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6682 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6683 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6684 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6685 | `			pGen->pIn = pTmpIn;` |
|        79 |  6686 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6687 | `			SySetRelease(&sToken);` |
|        79 |  6688 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6689 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6690 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6691 | `				return SXERR_ABORT;` |
|         - |  6692 | `			}` |
|         - |  6693 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6694 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6695 | `		}` |
|         - |  6696 | `	}` |
|         - |  6697 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6698 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6699 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6700 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6701 | `	{` |
|   2631577 |  6702 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2631577 |  6703 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6704 | `		/* Compile the body */` |
|   2631577 |  6705 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2631577 |  6706 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6707 | `	}` |
|         - |  6708 | `	/* Fix exception jumps now the destination is resolved */` |
|   2631577 |  6709 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6710 | `	/* Emit the final return if not yet done */` |
|   2631577 |  6711 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6712 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2631577 |  6713 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6714 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6715 | `	}` |
|   2631577 |  6716 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6717 | `	/* Restore the default container */` |
|   2631577 |  6718 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6719 | `	/* Leave function block */` |
|   2631577 |  6720 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2631577 |  6721 | `	if( rc == SXERR_ABORT ){` |
|         - |  6722 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6723 | `		return SXERR_ABORT;` |
|         - |  6724 | `	}` |
|         - |  6725 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6726 | `	{` |
|   2631577 |  6727 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6728 | `		sxu32 i;` |
|  71910871 |  6729 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  69291401 |  6730 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     12107 |  6731 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     12107 |  6732 | `				break;` |
|         - |  6733 | `			}` |
|  34639652 |  6734 | `		}` |
|         - |  6735 | `	}` |
|   2631577 |  6736 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6737 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     12107 |  6738 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6739 | `			return SXERR_ABORT;` |
|         - |  6740 | `		}` |
|      6051 |  6741 | `	}` |
|         - |  6742 | `	/* All done, function body compiled */` |
|   2631577 |  6743 | `	return SXRET_OK;` |
|   1315791 |  6744 | `}` |
|         - |  6745 | `/*` |
|         - |  6746 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  6747 | ` * According to the PHP language reference manual.` |
|         - |  6748 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  6749 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  6750 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  6751 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  6752 | ` *  Functions need not be defined before they are referenced.` |
|         - |  6753 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  6754 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  6755 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  6756 | ` *  calls with over 32-64 recursion levels.` |
|         - |  6757 | ` *` |
|         - |  6758 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  6759 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  6760 | ` * on these extension.` |
|         - |  6761 | ` */` |
|         - |  6762 | `/*` |
|         - |  6763 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  6764 | ` */` |
|       570 |  6765 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  6766 | `{` |
|         - |  6767 | `	sxu32 i;` |
|      1611 |  6768 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  6769 | `		int a = zA[i], b = zB[i];` |
|      1381 |  6770 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  6771 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  6772 | `		if( a != b ) return a - b;` |
|       523 |  6773 | `	}` |
|       235 |  6774 | `	return 0;` |
|       290 |  6775 | `}` |
|         - |  6776 | `/*` |
|         - |  6777 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  6778 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  6779 | ` * (which are positive bit values stored in sxu32).` |
|         - |  6780 | ` */` |
|         - |  6781 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  6782 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  6783 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  6784 |  |
|         - |  6785 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  6786 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  6787 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  6788 |  |
|         - |  6789 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  6790 | `struct PhlTypeAtom {` |
|         - |  6791 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  6792 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  6793 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  6794 | `	sxu32 nCanon;` |
|         - |  6795 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  6796 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  6797 | `};` |
|         - |  6798 |  |
|         - |  6799 | `/*` |
|         - |  6800 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  6801 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  6802 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  6803 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  6804 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  6805 | ` * already be consumed by the caller.` |
|         - |  6806 | ` */` |
|    131910 |  6807 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  6808 | `{` |
|    131915 |  6809 | `	SyToken *pIn = pGen->pIn;` |
|    131915 |  6810 | `	SyZero(pOut, sizeof(*pOut));` |
|    131915 |  6811 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    131915 |  6812 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6813 | `		return SXERR_SYNTAX;` |
|         - |  6814 | `	}` |
|         - |  6815 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    131915 |  6816 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|         8 |  6817 | `		pIn++;` |
|         8 |  6818 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6819 | `			return SXERR_SYNTAX;` |
|         - |  6820 | `		}` |
|         3 |  6821 | `	}` |
|    131915 |  6822 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6823 | `		return SXERR_SYNTAX;` |
|         - |  6824 | `	}` |
|    131915 |  6825 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     91915 |  6826 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     91915 |  6827 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11887 |  6828 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     85974 |  6829 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        81 |  6830 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     79995 |  6831 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     20381 |  6832 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     69769 |  6833 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     59499 |  6834 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     29834 |  6835 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  6836 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        68 |  6837 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        27 |  6838 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        37 |  6839 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        14 |  6840 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        23 |  6841 | `			pOut->nType = SXU32_HIGH;` |
|        23 |  6842 | `			pOut->sClass = pIn->sData;` |
|        13 |  6843 | `		}else{` |
|         3 |  6844 | `			return SXERR_SYNTAX;` |
|         - |  6845 | `		}` |
|     91913 |  6846 | `		pIn++;` |
|     45959 |  6847 | `	}else{` |
|         - |  6848 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  6849 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     40005 |  6850 | `		SyString *pT = &pIn->sData;` |
|     40005 |  6851 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  6852 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  6853 | `			pIn++;` |
|     39990 |  6854 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  6855 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  6856 | `			pIn++;` |
|     39889 |  6857 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        26 |  6858 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        26 |  6859 | `			pIn++;` |
|        15 |  6860 | `		}else{` |
|         - |  6861 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     39781 |  6862 | `			SyToken *pFirst = pIn;` |
|     39781 |  6863 | `			SyToken *pLast = pIn;` |
|     39781 |  6864 | `			pOut->nType = SXU32_HIGH;` |
|     39781 |  6865 | `			pOut->sClass = pIn->sData;` |
|     39781 |  6866 | `			pIn++;` |
|     59667 |  6867 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     39784 |  6868 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  6869 | `				pLast = &pIn[1];` |
|         3 |  6870 | `				pIn += 2;` |
|         1 |  6871 | `			}` |
|     39781 |  6872 | `			if( pLast != pFirst ){` |
|         3 |  6873 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  6874 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  6875 | `				pOut->sClass.zString = zFirst;` |
|         3 |  6876 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  6877 | `			}` |
|         - |  6878 | `		}` |
|         - |  6879 | `	}` |
|    131913 |  6880 | `	pGen->pIn = pIn;` |
|    131913 |  6881 | `	return SXRET_OK;` |
|     65960 |  6882 | `}` |
|         - |  6883 |  |
|         - |  6884 | `/*` |
|         - |  6885 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  6886 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  6887 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  6888 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  6889 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  6890 | ` */` |
|    131732 |  6891 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  6892 | `{` |
|         - |  6893 | `	int i;` |
|    131737 |  6894 | `	int nNonNull = 0;` |
|    131737 |  6895 | `	int bAnyIntersection = 0;` |
|         - |  6896 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    131737 |  6897 | `	sxu32 nMaxGroup = 0;` |
|   4347161 |  6898 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    263621 |  6899 | `	for( i = 0; i < nAtoms; i++ ){` |
|    131889 |  6900 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    131859 |  6901 | `			nNonNull++;` |
|    131859 |  6902 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    131859 |  6903 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    131859 |  6904 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     65927 |  6905 | `			}` |
|     65927 |  6906 | `		}` |
|     65947 |  6907 | `	}` |
|    263569 |  6908 | `	for( i = 0; i < nAtoms; i++ ){` |
|    131861 |  6909 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  6910 | `			bAnyIntersection = 1;` |
|        29 |  6911 | `			break;` |
|         - |  6912 | `		}` |
|     65921 |  6913 | `	}` |
|    131737 |  6914 | `	if( bAnyIntersection ){` |
|         - |  6915 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  6916 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  6917 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  6918 | `		sxu32 g, nGroups = 0;` |
|        29 |  6919 | `		int bFirstGroup = 1;` |
|        59 |  6920 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  6921 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  6922 | `			int bFirstMember = 1;` |
|         - |  6923 | `			int bWrap;` |
|        35 |  6924 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  6925 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  6926 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  6927 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  6928 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  6929 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  6930 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  6931 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  6932 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  6933 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  6934 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  6935 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  6936 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  6937 | `				}else{` |
|         6 |  6938 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  6939 | `				}` |
|        59 |  6940 | `				bFirstMember = 0;` |
|        32 |  6941 | `			}` |
|        35 |  6942 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  6943 | `			bFirstGroup = 0;` |
|        20 |  6944 | `		}` |
|        29 |  6945 | `		if( bNullable ){` |
|       ! 0 |  6946 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  6947 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  6948 | `		}` |
|        83 |  6949 | `		return;` |
|         - |  6950 | `	}` |
|    131713 |  6951 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  6952 | `		/* Shorthand: ?T */` |
|       112 |  6953 | `		for( i = 0; i < nAtoms; i++ ){` |
|       112 |  6954 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       112 |  6955 | `			SyBlobAppend(pBlob, "?", 1);` |
|       112 |  6956 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        23 |  6957 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  6958 | `			}else{` |
|        92 |  6959 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  6960 | `			}` |
|       112 |  6961 | `			return;` |
|       ! 0 |  6962 | `		}` |
|       ! 0 |  6963 | `	}` |
|         - |  6964 | `	{` |
|    131605 |  6965 | `		int bFirst = 1;` |
|         - |  6966 | `		/* 1) Classes in declaration order */` |
|    263313 |  6967 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131713 |  6968 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     39731 |  6969 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     39731 |  6970 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     39731 |  6971 | `				bFirst = 0;` |
|     19863 |  6972 | `			}` |
|     65859 |  6973 | `		}` |
|         - |  6974 | `		/* 2) Built-ins in canonical order */` |
|         - |  6975 | `		{` |
|         - |  6976 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  6977 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  6978 | `			int k;` |
|    921205 |  6979 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1487961 |  6980 | `				for( i = 0; i < nAtoms; i++ ){` |
|    790141 |  6981 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     91785 |  6982 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     91785 |  6983 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     91785 |  6984 | `						bFirst = 0;` |
|     91785 |  6985 | `						break;` |
|         - |  6986 | `					}` |
|    349183 |  6987 | `				}` |
|    394805 |  6988 | `			}` |
|         - |  6989 | `		}` |
|         - |  6990 | `		/* 3) null suffix */` |
|    131605 |  6991 | `		if( bNullable ){` |
|        19 |  6992 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        19 |  6993 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  6994 | `		}` |
|         - |  6995 | `	}` |
|     65871 |  6996 | `}` |
|         - |  6997 |  |
|         - |  6998 | `/*` |
|         - |  6999 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7000 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7001 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7002 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7003 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7004 | ` * whether it was parenthesized.` |
|         - |  7005 | ` *` |
|         - |  7006 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7007 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7008 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7009 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7010 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7011 | ` */` |
|    131884 |  7012 | `static sxi32 GenStateParsePart(` |
|         - |  7013 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7014 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7015 | `{` |
|         - |  7016 | `	sxi32 rc;` |
|    131889 |  7017 | `	int nMembers = 0;` |
|    131889 |  7018 | `	int bParen = 0;` |
|    131889 |  7019 | `	*pnMembers = 0;` |
|    131889 |  7020 | `	*pbParen = 0;` |
|    131889 |  7021 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7022 | `		bParen = 1;` |
|         9 |  7023 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7024 | `	}` |
|     65942 |  7025 | `	for(;;){` |
|    131915 |  7026 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7027 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7028 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7029 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7030 | `		}` |
|    131915 |  7031 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    131915 |  7032 | `		if( rc != SXRET_OK ){` |
|         3 |  7033 | `			return rc;` |
|         - |  7034 | `		}` |
|    131913 |  7035 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    131913 |  7036 | `		(*pnAtoms)++;` |
|    131913 |  7037 | `		nMembers++;` |
|         - |  7038 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    131913 |  7039 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7040 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7041 | `			if( pNext < pGen->pEnd` |
|        39 |  7042 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7043 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7044 | `				continue;` |
|         - |  7045 | `			}` |
|         4 |  7046 | `		}` |
|    131887 |  7047 | `		break;` |
|       ! 0 |  7048 | `	}` |
|    131887 |  7049 | `	if( bParen ){` |
|         9 |  7050 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7051 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7052 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7053 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7054 | `		}` |
|         9 |  7055 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7056 | `		if( nMembers < 2 ){` |
|       ! 0 |  7057 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7058 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7059 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7060 | `		}` |
|         3 |  7061 | `	}` |
|    131887 |  7062 | `	*pnMembers = nMembers;` |
|    131887 |  7063 | `	*pbParen = bParen;` |
|    131887 |  7064 | `	return SXRET_OK;` |
|     65947 |  7065 | `}` |
|         - |  7066 |  |
|         - |  7067 | `/*` |
|         - |  7068 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7069 | ` *` |
|         - |  7070 | ` * Outputs:` |
|         - |  7071 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7072 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7073 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7074 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7075 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7076 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7077 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7078 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7079 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7080 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7081 | ` *` |
|         - |  7082 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7083 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7084 | ` */` |
|    131748 |  7085 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7086 | `	ph7_gen_state *pGen,` |
|         - |  7087 | `	sxu32 *pnType,` |
|         - |  7088 | `	SyString *pClass,` |
|         - |  7089 | `	SySet *pAlts,` |
|         - |  7090 | `	sxi32 *piTypeFlags,` |
|         - |  7091 | `	SyString *pTypeText,` |
|         - |  7092 | `	int iNullableFlag,` |
|         - |  7093 | `	int iUnionFlag,` |
|         - |  7094 | `	int bAllowVoid,` |
|         - |  7095 | `	sxu32 nLine` |
|         5 |  7096 | `){` |
|         - |  7097 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    131753 |  7098 | `	int nAtoms = 0;` |
|    131753 |  7099 | `	int bShortNullable = 0;` |
|    131753 |  7100 | `	int bExplicitNull = 0;` |
|         - |  7101 | `	sxi32 rc;` |
|    131753 |  7102 | `	*pnType = 0;` |
|    131753 |  7103 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    131753 |  7104 | `	*piTypeFlags = 0;` |
|    131753 |  7105 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7106 |  |
|    131753 |  7107 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7108 | `		return SXRET_OK;` |
|         - |  7109 | `	}` |
|         - |  7110 | ``	/* Optional `?` shorthand prefix */`` |
|    131748 |  7111 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       101 |  7112 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       100 |  7113 | `		bShortNullable = 1;` |
|       100 |  7114 | `		pGen->pIn++;` |
|       100 |  7115 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7116 | `			return SXERR_SYNTAX;` |
|         - |  7117 | `		}` |
|        48 |  7118 | `	}` |
|         - |  7119 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7120 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7121 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7122 | `	{` |
|         - |  7123 | `		int nMembers, bParen;` |
|    131753 |  7124 | `		sxu32 iGroup = 0;` |
|    131753 |  7125 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    131753 |  7126 | `		if( rc != SXRET_OK ){` |
|         4 |  7127 | `			return rc;` |
|         - |  7128 | `		}` |
|         - |  7129 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7130 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7131 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7132 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7133 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    197828 |  7134 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    131960 |  7135 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7136 | `			if( bShortNullable ){` |
|         - |  7137 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7138 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7139 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7140 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7141 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7142 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7143 | `			}` |
|       141 |  7144 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7145 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7146 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7147 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7148 | `			}` |
|       141 |  7149 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7150 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7151 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7152 | `				return rc;` |
|         - |  7153 | `			}` |
|         5 |  7154 | `		}` |
|    131749 |  7155 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7156 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7157 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7158 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7159 | `		}` |
|         - |  7160 | `	}` |
|         - |  7161 | `	/* Validation pass.` |
|         - |  7162 | `	 *` |
|         - |  7163 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7164 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7165 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7166 | `	 */` |
|         - |  7167 | `	{` |
|         - |  7168 | `		int i, j;` |
|    131749 |  7169 | `		int bHasNonNull = 0;` |
|    131749 |  7170 | `		int bAnyIntersection = 0;` |
|         - |  7171 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7172 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7173 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4347557 |  7174 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    263655 |  7175 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131911 |  7176 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     65958 |  7177 | `		}` |
|    263599 |  7178 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131881 |  7179 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     65930 |  7180 | `		}` |
|         - |  7181 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7182 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    131749 |  7183 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7184 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7185 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7186 | `			return SXERR_SYNTAX;` |
|         - |  7187 | `		}` |
|    263641 |  7188 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7189 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7190 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7191 | ``			 * `true`/`false` in an intersection). */`` |
|    131909 |  7192 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7193 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7194 | `				if( bClassLike ){` |
|        53 |  7195 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7196 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7197 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7198 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7199 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7200 | `						bClassLike = 0;` |
|       ! 0 |  7201 | `					}` |
|        24 |  7202 | `				}` |
|        55 |  7203 | `				if( !bClassLike ){` |
|         - |  7204 | `					const char *zName; sxu32 nName;` |
|         3 |  7205 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7206 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7207 | `					}else{` |
|         3 |  7208 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7209 | `					}` |
|         4 |  7210 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7211 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7212 | `						(int)nName, zName);` |
|         3 |  7213 | `					return SXERR_SYNTAX;` |
|         - |  7214 | `				}` |
|        24 |  7215 | `			}` |
|    131907 |  7216 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7217 | `				if( nAtoms > 1 ){` |
|         3 |  7218 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7219 | `						"Void can only be used as a standalone type");` |
|         3 |  7220 | `					return SXERR_SYNTAX;` |
|         - |  7221 | `				}` |
|       175 |  7222 | `				if( !bAllowVoid ){` |
|       ! 0 |  7223 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7224 | `						"void cannot be used here");` |
|       ! 0 |  7225 | `					return SXERR_SYNTAX;` |
|         - |  7226 | `				}` |
|       175 |  7227 | `				if( bShortNullable ){` |
|       ! 0 |  7228 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7229 | `						"Void type cannot be nullable");` |
|       ! 0 |  7230 | `					return SXERR_SYNTAX;` |
|         - |  7231 | `				}` |
|        85 |  7232 | `			}` |
|    131905 |  7233 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7234 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7235 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7236 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7237 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        26 |  7238 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7239 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7240 | `					 * same as any other non-standalone use. */` |
|         5 |  7241 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7242 | `						"never can only be used as a standalone type");` |
|         5 |  7243 | `					return SXERR_SYNTAX;` |
|         - |  7244 | `				}` |
|        21 |  7245 | `				if( !bAllowVoid ){` |
|         - |  7246 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7247 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7248 | `						"never cannot be used as a parameter type");` |
|         3 |  7249 | `					return SXERR_SYNTAX;` |
|         - |  7250 | `				}` |
|         8 |  7251 | `			}` |
|    131899 |  7252 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7253 | `				bExplicitNull = 1;` |
|        19 |  7254 | `			}else{` |
|    131869 |  7255 | `				bHasNonNull = 1;` |
|         - |  7256 | `			}` |
|         - |  7257 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7258 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7259 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7260 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7261 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    132099 |  7262 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7263 | `				int bDup = 0;` |
|       207 |  7264 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7265 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7266 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7267 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7268 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7269 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7270 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7271 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7272 | `								aAtoms[j].sClass.zString,` |
|        34 |  7273 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7274 | `							bDup = 1;` |
|       ! 0 |  7275 | `						}` |
|        27 |  7276 | `					}else{` |
|         3 |  7277 | `						bDup = 1;` |
|         - |  7278 | `					}` |
|        23 |  7279 | `				}` |
|       195 |  7280 | `				if( bDup ){` |
|         - |  7281 | `					const char *zName;` |
|         - |  7282 | `					sxu32 nName;` |
|         3 |  7283 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7284 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7285 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7286 | `					}else{` |
|         3 |  7287 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7288 | `						nName = aAtoms[i].nCanon;` |
|         - |  7289 | `					}` |
|         4 |  7290 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7291 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7292 | `					return SXERR_SYNTAX;` |
|         - |  7293 | `				}` |
|        99 |  7294 | `			}` |
|     65951 |  7295 | `		}` |
|    131737 |  7296 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7297 | `			if( bShortNullable ){` |
|         - |  7298 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7299 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7300 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7301 | `				return SXERR_SYNTAX;` |
|         - |  7302 | `			}` |
|         - |  7303 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7304 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7305 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7306 | `			 * atom, so set it here. */` |
|         7 |  7307 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7308 | `		}` |
|         - |  7309 | `	}` |
|         - |  7310 | `	/* Compute nullability flag */` |
|    131737 |  7311 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       128 |  7312 | `		*piTypeFlags \|= iNullableFlag;` |
|        62 |  7313 | `	}` |
|         - |  7314 | `	/* Build canonical type text */` |
|    131737 |  7315 | `	if( pTypeText ){` |
|         - |  7316 | `		SyBlob sBlob;` |
|    131737 |  7317 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    197556 |  7318 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     65866 |  7319 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    131737 |  7320 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    197324 |  7321 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    131546 |  7322 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    131551 |  7323 | `			if( zDup ){` |
|    131551 |  7324 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     65773 |  7325 | `			}` |
|     65773 |  7326 | `		}` |
|    131737 |  7327 | `		SyBlobRelease(&sBlob);` |
|     65866 |  7328 | `	}` |
|         - |  7329 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7330 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7331 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7332 | `	{` |
|    131737 |  7333 | `		int nNonNull = 0;` |
|    131737 |  7334 | `		int iNonNullIdx = -1;` |
|         - |  7335 | `		int i;` |
|    263621 |  7336 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131889 |  7337 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    131859 |  7338 | `				nNonNull++;` |
|    131859 |  7339 | `				iNonNullIdx = i;` |
|     65927 |  7340 | `			}` |
|     65947 |  7341 | `		}` |
|    131737 |  7342 | `		if( nNonNull <= 1 ){` |
|         - |  7343 | `			/* Fast path: store as single type. */` |
|    131631 |  7344 | `			if( iNonNullIdx >= 0 ){` |
|    131625 |  7345 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    131625 |  7346 | `				if( pA->nType == SXU32_HIGH ){` |
|     59558 |  7347 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19851 |  7348 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     39707 |  7349 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     39707 |  7350 | `					*pnType = SXU32_HIGH;` |
|     39707 |  7351 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    111774 |  7352 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7353 | `					*pnType = MEMOBJ_VOID;` |
|     91838 |  7354 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7355 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7356 | `				}else{` |
|     91737 |  7357 | `					*pnType = pA->nType;` |
|         - |  7358 | `				}` |
|     65810 |  7359 | `			}` |
|     65818 |  7360 | `		}else{` |
|         - |  7361 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7362 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7363 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7364 | `				ph7_type_alt sAlt;` |
|       249 |  7365 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7366 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7367 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7368 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7369 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7370 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7371 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7372 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7373 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7374 | `				}else{` |
|       145 |  7375 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7376 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7377 | `				}` |
|       239 |  7378 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7379 | `			}` |
|         - |  7380 | `		}` |
|         - |  7381 | `	}` |
|    131737 |  7382 | `	return SXRET_OK;` |
|     65879 |  7383 | `}` |
|         - |  7384 |  |
|         - |  7385 | `/*` |
|         - |  7386 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7387 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7388 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7389 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7390 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7391 | `` *          and union types `: T\|U`.`` |
|         - |  7392 | ` */` |
|   2773926 |  7393 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7394 | `{` |
|   2773931 |  7395 | `	sxi32 iFlags = 0;` |
|         - |  7396 | `	sxi32 rc;` |
|         - |  7397 | `	sxu32 nLine;` |
|   2773931 |  7398 | `	pFunc->nReturnType = 0;` |
|   2773931 |  7399 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2773931 |  7400 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7401 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7402 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7403 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7404 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7405 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2773931 |  7406 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2773931 |  7407 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2773931 |  7408 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2761435 |  7409 | `		return SXRET_OK;` |
|         - |  7410 | `	}` |
|     12501 |  7411 | `	pGen->pIn++; /* Skip ':' */` |
|     12501 |  7412 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7413 | `		return SXRET_OK;` |
|         - |  7414 | `	}` |
|     12501 |  7415 | `	nLine = pGen->pIn->nLine;` |
|     12501 |  7416 | `	rc = GenStateParseUnionTypeDecl(` |
|      6248 |  7417 | `		pGen,` |
|      6248 |  7418 | `		&pFunc->nReturnType,` |
|      6248 |  7419 | `		&pFunc->sReturnClass,` |
|      6248 |  7420 | `		&pFunc->aReturnUnion,` |
|         - |  7421 | `		&iFlags,` |
|      6248 |  7422 | `		&pFunc->sReturnTypeName,` |
|         - |  7423 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7424 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7425 | `		/* iUnionFlag */ 0,` |
|         - |  7426 | `		/* bAllowVoid */ 1,` |
|      6248 |  7427 | `		nLine);` |
|     12501 |  7428 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7429 | `		return SXERR_ABORT;` |
|         - |  7430 | `	}` |
|     12501 |  7431 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7432 | `		/* Error already reported */` |
|       ! 0 |  7433 | `		return SXERR_SYNTAX;` |
|         - |  7434 | `	}` |
|     12501 |  7435 | `	if( rc == SXERR_SYNTAX ){` |
|         8 |  7436 | `		if( pGen->pIn < pGen->pEnd ){` |
|        11 |  7437 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7438 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7439 | `				&pGen->pIn->sData);` |
|         5 |  7440 | `		}else{` |
|       ! 0 |  7441 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7442 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7443 | `		}` |
|         8 |  7444 | `		return SXERR_SYNTAX;` |
|         - |  7445 | `	}` |
|     12495 |  7446 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12495 |  7447 | `	return SXRET_OK;` |
|   1386968 |  7448 | `}` |
|         - |  7449 |  |
|    309732 |  7450 | `static sxi32 GenStateCompileFunc(` |
|         - |  7451 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7452 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7453 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7454 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7455 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7456 | `	)` |
|         5 |  7457 | `{` |
|         - |  7458 | `	ph7_vm_func *pFunc;` |
|         - |  7459 | `	SyToken *pEnd;` |
|         - |  7460 | `	sxu32 nLine;` |
|         - |  7461 | `	char *zName;` |
|         - |  7462 | `	sxi32 rc;` |
|         - |  7463 | `	/* Extract line number */` |
|    309737 |  7464 | `	nLine = pGen->pIn->nLine;` |
|         - |  7465 | `	/* Jump the left parenthesis '(' */` |
|    309737 |  7466 | `	pGen->pIn++;` |
|         - |  7467 | `	/* Delimit the function signature */` |
|    309737 |  7468 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    309737 |  7469 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7470 | `		/* Syntax error */` |
|         8 |  7471 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|         8 |  7472 | `		if( rc == SXERR_ABORT ){` |
|         - |  7473 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7474 | `			return SXERR_ABORT;` |
|         - |  7475 | `		}` |
|         8 |  7476 | `		pGen->pIn = pGen->pEnd;` |
|         8 |  7477 | `		return SXRET_OK;` |
|         - |  7478 | `	}` |
|         - |  7479 | `	/* Create the function state */` |
|    309731 |  7480 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    309731 |  7481 | `	if( pFunc == 0 ){` |
|       ! 0 |  7482 | `		goto OutOfMem;` |
|         - |  7483 | `	}` |
|         - |  7484 | `	/* Build the function name, prepending namespace if active */` |
|    309738 |  7485 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7486 | `		SyBlob sFQN;` |
|         - |  7487 | `		sxu32 nLen;` |
|        16 |  7488 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7489 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7490 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7491 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7492 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7493 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7494 | `		SyBlobRelease(&sFQN);` |
|        16 |  7495 | `		if( zName == 0 ){` |
|       ! 0 |  7496 | `			goto OutOfMem;` |
|         - |  7497 | `		}` |
|        16 |  7498 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7499 | `	}else{` |
|    309717 |  7500 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    309717 |  7501 | `		if( zName == 0 ){` |
|       ! 0 |  7502 | `			goto OutOfMem;` |
|         - |  7503 | `		}` |
|    309717 |  7504 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7505 | `	}` |
|         - |  7506 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7507 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    309731 |  7508 | `	pFunc->nLine = nLine;` |
|    309731 |  7509 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    309731 |  7510 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7511 | `		return SXERR_ABORT;` |
|         - |  7512 | `	}` |
|    309731 |  7513 | `	if( pGen->pIn < pEnd ){` |
|         - |  7514 | `		/* Collect function arguments */` |
|    249701 |  7515 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    249701 |  7516 | `		if( rc == SXERR_ABORT ){` |
|         - |  7517 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7518 | `			return SXERR_ABORT;` |
|         - |  7519 | `		}` |
|    124848 |  7520 | `	}` |
|         - |  7521 | `	/* Point past ')' and parse optional return type ': type' */` |
|    309731 |  7522 | `	pGen->pIn = &pEnd[1];` |
|         - |  7523 | `	{` |
|    309731 |  7524 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    309731 |  7525 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7526 | `			return SXERR_ABORT;` |
|    309731 |  7527 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         8 |  7528 | `			return SXERR_SYNTAX;` |
|         - |  7529 | `		}` |
|         - |  7530 | `	}` |
|    309725 |  7531 | `	if( bHandleClosure ){` |
|         - |  7532 | `		ph7_vm_func_closure_env sEnv;` |
|       471 |  7533 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       466 |  7534 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       281 |  7535 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        91 |  7536 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7537 | `				/* Closure,record environment variable */` |
|        91 |  7538 | `				pGen->pIn++;` |
|        91 |  7539 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7540 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7541 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7542 | `						return SXERR_ABORT;` |
|         - |  7543 | `					}` |
|       ! 0 |  7544 | `				}` |
|        91 |  7545 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7546 | `				/* Compile until we hit the first closing parenthesis */` |
|       187 |  7547 | `				while( pGen->pIn < pGen->pEnd ){` |
|       187 |  7548 | `					int iFlagsLocal = 0;` |
|       187 |  7549 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        91 |  7550 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        91 |  7551 | `						break;` |
|         - |  7552 | `					}` |
|       101 |  7553 | `					nLineLocal = pGen->pIn->nLine;` |
|       101 |  7554 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7555 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7556 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7557 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7558 | `						pGen->pIn++;` |
|        27 |  7559 | `					}` |
|        96 |  7560 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       101 |  7561 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7562 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7563 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7564 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7565 | `								return SXERR_ABORT;` |
|         - |  7566 | `							}` |
|         - |  7567 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7568 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7569 | `								pGen->pIn++;` |
|       ! 0 |  7570 | `							}` |
|       ! 0 |  7571 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7572 | `								pGen->pIn++;` |
|       ! 0 |  7573 | `							}` |
|       ! 0 |  7574 | `							break;` |
|         - |  7575 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7576 | `					}else{` |
|         - |  7577 | `						SyString *pNameLocal;` |
|         - |  7578 | `						char *zDup;` |
|         - |  7579 | `						/* Duplicate variable name */` |
|       101 |  7580 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       101 |  7581 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       101 |  7582 | `						if( zDup ){` |
|         - |  7583 | `							/* Zero the structure */` |
|       101 |  7584 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       101 |  7585 | `							sEnv.iFlags = iFlagsLocal;` |
|       101 |  7586 | `							sEnv.nIdx = SXU32_HIGH;` |
|       101 |  7587 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       101 |  7588 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       116 |  7589 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7590 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7591 | `									got_this = 1;` |
|       ! 0 |  7592 | `							}` |
|         - |  7593 | `							/* Save imported variable */` |
|       101 |  7594 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        53 |  7595 | `						}else{` |
|       ! 0 |  7596 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7597 | `							 return SXERR_ABORT;` |
|         - |  7598 | `						}` |
|         - |  7599 | `					}` |
|       101 |  7600 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       113 |  7601 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7602 | `						/* Ignore trailing commas */` |
|        13 |  7603 | `						pGen->pIn++;` |
|         1 |  7604 | `					}` |
|         5 |  7605 | `				}` |
|         - |  7606 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7607 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7608 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7609 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7610 | `				 * legacy pre-use position. */` |
|        91 |  7611 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7612 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7613 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7614 | `						return SXERR_ABORT;` |
|         7 |  7615 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7616 | `						return SXERR_SYNTAX;` |
|         - |  7617 | `					}` |
|         3 |  7618 | `				}` |
|        43 |  7619 | `		}` |
|       471 |  7620 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7621 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7622 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7623 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7624 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7625 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7626 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7627 | `			 * closure never binds $this (php). */` |
|       463 |  7628 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       463 |  7629 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       463 |  7630 | `			sEnv.nIdx = SXU32_HIGH;` |
|       463 |  7631 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       463 |  7632 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       463 |  7633 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       229 |  7634 | `		}` |
|       471 |  7635 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7636 | `			/* Mark as closure */` |
|       465 |  7637 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       230 |  7638 | `		}` |
|       233 |  7639 | `	}` |
|         - |  7640 | `	/* Compile the body */` |
|    309725 |  7641 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    309725 |  7642 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7643 | `		return SXERR_ABORT;` |
|         - |  7644 | `	}` |
|         - |  7645 | `	/* The cursor sits just past the body's closing brace */` |
|    309725 |  7646 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    309725 |  7647 | `	if( ppFunc ){` |
|    309725 |  7648 | `		*ppFunc = pFunc;` |
|    154860 |  7649 | `	}` |
|    309725 |  7650 | `	rc = SXRET_OK;` |
|    309725 |  7651 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7652 | `		/* Finally register the function */` |
|    309265 |  7653 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    154630 |  7654 | `	}` |
|    309725 |  7655 | `	if( rc == SXRET_OK ){` |
|    309725 |  7656 | `		return SXRET_OK;` |
|         - |  7657 | `	}` |
|         - |  7658 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7659 | `OutOfMem:` |
|         - |  7660 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7661 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7662 | `	 */` |
|       ! 0 |  7663 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7664 | `	return SXERR_ABORT;` |
|    154871 |  7665 | `}` |
|         - |  7666 | `/*` |
|         - |  7667 | ` * Compile a standard PHP function.` |
|         - |  7668 | ` *  Refer to the block-comment above for more information.` |
|         - |  7669 | ` */` |
|    309274 |  7670 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7671 | `{` |
|         - |  7672 | `	SyString *pName;` |
|         - |  7673 | `	sxi32 iFlags;` |
|         - |  7674 | `	sxu32 nKwLine;` |
|         - |  7675 | `	sxu32 nLine;` |
|         - |  7676 | `	sxi32 rc;` |
|         - |  7677 |  |
|    309279 |  7678 | `	nLine = pGen->pIn->nLine;` |
|    309279 |  7679 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    309279 |  7680 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    309279 |  7681 | `	iFlags = 0;` |
|    309279 |  7682 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7683 | `		/* Return by reference,remember that */` |
|        12 |  7684 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7685 | `		/* Jump the '&' token */` |
|        12 |  7686 | `		pGen->pIn++;` |
|         5 |  7687 | `	}` |
|    309279 |  7688 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7689 | `		/* Invalid function name */` |
|         8 |  7690 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|         8 |  7691 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7692 | `			return SXERR_ABORT;` |
|         - |  7693 | `		}` |
|         - |  7694 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  7695 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  7696 | `			pGen->pIn++;` |
|         2 |  7697 | `		}` |
|         8 |  7698 | `		return SXRET_OK;` |
|         - |  7699 | `	}` |
|    309273 |  7700 | `	pName = &pGen->pIn->sData;` |
|    309273 |  7701 | `	nLine = pGen->pIn->nLine;` |
|         - |  7702 | `	/* Jump the function name */` |
|    309273 |  7703 | `	pGen->pIn++;` |
|    309273 |  7704 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7705 | `		/* Syntax error */` |
|         3 |  7706 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|         3 |  7707 | `		if( rc == SXERR_ABORT ){` |
|         - |  7708 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7709 | `			return SXERR_ABORT;` |
|         - |  7710 | `		}` |
|         - |  7711 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  7712 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  7713 | `			pGen->pIn++;` |
|       ! 0 |  7714 | `		}` |
|         3 |  7715 | `		return SXRET_OK;` |
|         - |  7716 | `	}` |
|         - |  7717 | `	/* Compile function body */` |
|         - |  7718 | `	{` |
|    309271 |  7719 | `		ph7_vm_func *pFuncState = 0;` |
|    309271 |  7720 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    309271 |  7721 | `		if( pFuncState ){` |
|         - |  7722 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    309259 |  7723 | `			pFuncState->nLine = nKwLine;` |
|    154627 |  7724 | `		}` |
|         - |  7725 | `	}` |
|    309271 |  7726 | `	return rc;` |
|    154642 |  7727 | `}` |
|         - |  7728 | `/*` |
|         - |  7729 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  7730 | ` * According to the PHP language reference manual` |
|         - |  7731 | ` *  Visibility:` |
|         - |  7732 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  7733 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  7734 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  7735 | ` *  Members declared protected can be accessed only within the class` |
|         - |  7736 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  7737 | ` *  may only be accessed by the class that defines the member.` |
|         - |  7738 | ` */` |
|   3234538 |  7739 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  7740 | `{` |
|   3234543 |  7741 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    260629 |  7742 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2973919 |  7743 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    197387 |  7744 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  7745 | `	}` |
|         - |  7746 | `	/* Assume public by default */` |
|   2776537 |  7747 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1617274 |  7748 | `}` |
|         - |  7749 | `/*` |
|         - |  7750 | ` * Compile a class constant.` |
|         - |  7751 | ` * According to the PHP language reference manual` |
|         - |  7752 | ` *  Class Constants` |
|         - |  7753 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  7754 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  7755 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  7756 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  7757 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  7758 | ` *   It's also possible for interfaces to have constants.` |
|         - |  7759 | ` * Symisc eXtension.` |
|         - |  7760 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  7761 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  7762 | ` *  Example:` |
|         - |  7763 | ` *   class Test{` |
|         - |  7764 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  7765 | ` *   };` |
|         - |  7766 | ` *   var_dump(TEST::MyConst);` |
|         - |  7767 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  7768 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  7769 | ` */` |
|         - |  7770 | `/*` |
|         - |  7771 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  7772 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  7773 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  7774 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  7775 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  7776 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  7777 | ` */` |
|    300072 |  7778 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  7779 | `{` |
|         - |  7780 | `	SyToken *p0, *p1;` |
|    300077 |  7781 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7782 | `		return 0;` |
|         - |  7783 | `	}` |
|    300077 |  7784 | `	p0 = pGen->pIn;` |
|         - |  7785 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    300077 |  7786 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  7787 | `		return 1;` |
|         - |  7788 | `	}` |
|    300077 |  7789 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  7790 | `		return 1;` |
|         - |  7791 | `	}` |
|         - |  7792 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  7793 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  7794 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    300073 |  7795 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    300073 |  7796 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    300073 |  7797 | `		if( p1 ){` |
|    300073 |  7798 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  7799 | `				return 1;` |
|         - |  7800 | `			}` |
|    300043 |  7801 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  7802 | `				return 1;` |
|         - |  7803 | `			}` |
|    150017 |  7804 | `		}` |
|    150017 |  7805 | `	}` |
|    300039 |  7806 | `	return 0;` |
|    150041 |  7807 | `}` |
|         - |  7808 | `/*` |
|         - |  7809 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  7810 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  7811 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  7812 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  7813 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  7814 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  7815 | ` * Peek only; never consumes tokens.` |
|         - |  7816 | ` */` |
|        24 |  7817 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  7818 | `{` |
|        28 |  7819 | `	SyToken *p = pGen->pIn;` |
|        39 |  7820 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  7821 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  7822 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  7823 | `	}` |
|        28 |  7824 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  7825 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  7826 | `	}` |
|         6 |  7827 | `	p++;` |
|         - |  7828 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  7829 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  7830 | `}` |
|         - |  7831 | `/*` |
|         - |  7832 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  7833 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  7834 | `` * `$o->new`), not a `new` expression.`` |
|         - |  7835 | ` */` |
|       110 |  7836 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  7837 | `{` |
|         - |  7838 | `	sxi32 iOp;` |
|       114 |  7839 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  7840 | `		return 0;` |
|         - |  7841 | `	}` |
|       104 |  7842 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  7843 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  7844 | `}` |
|         - |  7845 | `/*` |
|         - |  7846 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  7847 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  7848 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  7849 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  7850 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  7851 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  7852 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  7853 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  7854 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  7855 | ` *` |
|         - |  7856 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  7857 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  7858 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  7859 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  7860 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  7861 | ` */` |
|    644056 |  7862 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  7863 | `{` |
|    644061 |  7864 | `	SyToken *p = pGen->pIn;` |
|    644061 |  7865 | `	int iDepth = 0;` |
|   1687757 |  7866 | `	while( p < pGen->pEnd ){` |
|   1687757 |  7867 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    644009 |  7868 | `			break; /* end of this initializer */` |
|         - |  7869 | `		}` |
|   1043748 |  7870 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    525841 |  7871 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7924 |  7872 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  7873 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  7874 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  7875 | `			 * expression. */` |
|         3 |  7876 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  7877 | `			p++;` |
|         3 |  7878 | `			if( bArrow ){` |
|         - |  7879 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  7880 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  7881 | `				int iBase = iDepth;` |
|        17 |  7882 | `				while( p < pGen->pEnd ){` |
|        17 |  7883 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  7884 | `						iDepth++;` |
|        15 |  7885 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  7886 | `						if( iDepth <= iBase ){` |
|       ! 0 |  7887 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  7888 | `						}` |
|         5 |  7889 | `						iDepth--;` |
|        11 |  7890 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  7891 | `						break;` |
|         - |  7892 | `					}` |
|        15 |  7893 | `					p++;` |
|         1 |  7894 | `				}` |
|         2 |  7895 | `			}else{` |
|         - |  7896 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  7897 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  7898 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  7899 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  7900 | `				int iLocal = 0;` |
|       ! 0 |  7901 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  7902 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  7903 | `						break; /* body brace */` |
|         - |  7904 | `					}` |
|       ! 0 |  7905 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  7906 | `						iLocal++;` |
|       ! 0 |  7907 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  7908 | `						if( iLocal > 0 ){` |
|       ! 0 |  7909 | `							iLocal--;` |
|       ! 0 |  7910 | `						}` |
|       ! 0 |  7911 | `					}` |
|       ! 0 |  7912 | `					p++;` |
|       ! 0 |  7913 | `				}` |
|       ! 0 |  7914 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  7915 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  7916 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  7917 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  7918 | `							iBrace++;` |
|       ! 0 |  7919 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  7920 | `							iBrace--;` |
|       ! 0 |  7921 | `							if( iBrace == 0 ){` |
|       ! 0 |  7922 | `								p++;` |
|       ! 0 |  7923 | `								break;` |
|         - |  7924 | `							}` |
|       ! 0 |  7925 | `						}` |
|       ! 0 |  7926 | `						p++;` |
|       ! 0 |  7927 | `					}` |
|       ! 0 |  7928 | `				}` |
|         - |  7929 | `			}` |
|         3 |  7930 | `			continue;` |
|         - |  7931 | `		}` |
|   1043751 |  7932 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  7933 | `			if( iDepth == 0 ){` |
|         - |  7934 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  7935 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  7936 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  7937 | `				 * is legal — don't scan into it. */` |
|        45 |  7938 | `				break;` |
|         - |  7939 | `			}` |
|       ! 0 |  7940 | `			iDepth++;` |
|   1043707 |  7941 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     43495 |  7942 | `			iDepth++;` |
|   1021962 |  7943 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     43493 |  7944 | `			if( iDepth > 0 ){` |
|     43493 |  7945 | `				iDepth--;` |
|     21744 |  7946 | `			}` |
|    978473 |  7947 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    348037 |  7948 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  7949 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  7950 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  7951 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  7952 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  7953 | `				return 1;` |
|         - |  7954 | `			}` |
|       ! 0 |  7955 | `		}` |
|   1043699 |  7956 | `		p++;` |
|         5 |  7957 | `	}` |
|    644053 |  7958 | `	return 0;` |
|    322033 |  7959 | `}` |
|         - |  7960 | `/*` |
|         - |  7961 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  7962 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  7963 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  7964 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  7965 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  7966 | ` * share the same backing.` |
|         - |  7967 | ` */` |
|       350 |  7968 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  7969 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  7970 | `{` |
|       355 |  7971 | `	pAttr->nType = nType;` |
|       355 |  7972 | `	pAttr->sClass = *pClass;` |
|       355 |  7973 | `	pAttr->sTypeName = *pTypeName;` |
|       355 |  7974 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  7975 | `		sxu32 i;` |
|        73 |  7976 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  7977 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  7978 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  7979 | `		}` |
|        11 |  7980 | `	}` |
|       355 |  7981 | `}` |
|    300072 |  7982 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  7983 | `{` |
|    300077 |  7984 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  7985 | `	SySet *pInstrContainer;` |
|         - |  7986 | `	ph7_class_attr *pCons;` |
|         - |  7987 | `	SyString *pName;` |
|         - |  7988 | `	sxi32 rc;` |
|    300077 |  7989 | `	sxu32 nType = 0;` |
|         - |  7990 | `	SyString sTypeClass;` |
|         - |  7991 | `	SyString sTypeText;` |
|         - |  7992 | `	SySet aUnionAlts;` |
|    300077 |  7993 | `	sxi32 iTypeFlags = 0;` |
|    300077 |  7994 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    300077 |  7995 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    300077 |  7996 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  7997 | `	/* Extract visibility level */` |
|    300077 |  7998 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  7999 | `	/* Mark as constant */` |
|    300077 |  8000 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    300077 |  8001 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8002 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8003 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    300096 |  8004 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8005 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8006 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8007 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8008 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8009 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8010 | `		 * and success paths release. */` |
|        42 |  8011 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8012 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8013 | `			goto Synchronize;` |
|        42 |  8014 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8015 | `			return SXERR_ABORT;` |
|        42 |  8016 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8017 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8018 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8019 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8020 | `				return SXERR_ABORT;` |
|         - |  8021 | `			}` |
|       ! 0 |  8022 | `			goto Synchronize;` |
|         - |  8023 | `		}` |
|        42 |  8024 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8025 | `	}` |
|    150036 |  8026 | `loop:` |
|    300079 |  8027 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8028 | `		/* Invalid constant name */` |
|       ! 0 |  8029 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8030 | `		if( rc == SXERR_ABORT ){` |
|         - |  8031 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8032 | `			return SXERR_ABORT;` |
|         - |  8033 | `		}` |
|       ! 0 |  8034 | `		goto Synchronize;` |
|         - |  8035 | `	}` |
|         - |  8036 | `	/* Peek constant name */` |
|    300079 |  8037 | `	pName = &pGen->pIn->sData;` |
|         - |  8038 | `	/* Make sure the constant name isn't reserved */` |
|    300079 |  8039 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8040 | `		/* Reserved constant name */` |
|       ! 0 |  8041 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8042 | `		if( rc == SXERR_ABORT ){` |
|         - |  8043 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8044 | `			return SXERR_ABORT;` |
|         - |  8045 | `		}` |
|       ! 0 |  8046 | `		goto Synchronize;` |
|         - |  8047 | `	}` |
|         - |  8048 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    300079 |  8049 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8050 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8051 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8052 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8053 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8054 | `			return SXERR_ABORT;` |
|        42 |  8055 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8056 | `			goto Synchronize;` |
|         - |  8057 | `		}` |
|        18 |  8058 | `	}` |
|         - |  8059 | `	/* Advance the stream cursor */` |
|    300077 |  8060 | `	pGen->pIn++;` |
|    300077 |  8061 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8062 | `		/* Invalid declaration */` |
|       ! 0 |  8063 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8064 | `		if( rc == SXERR_ABORT ){` |
|         - |  8065 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8066 | `			return SXERR_ABORT;` |
|         - |  8067 | `		}` |
|       ! 0 |  8068 | `		goto Synchronize;` |
|         - |  8069 | `	}` |
|    300077 |  8070 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8071 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8072 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8073 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8074 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    300072 |  8075 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8076 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8077 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8078 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8079 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8080 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8081 | `			return SXERR_ABORT;` |
|         - |  8082 | `		}` |
|         6 |  8083 | `		goto Synchronize;` |
|         - |  8084 | `	}` |
|         - |  8085 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8086 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8087 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    300073 |  8088 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8089 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8090 | `			"New expressions are not supported in this context");` |
|         5 |  8091 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8092 | `			return SXERR_ABORT;` |
|         - |  8093 | `		}` |
|         5 |  8094 | `		goto Synchronize;` |
|         - |  8095 | `	}` |
|         - |  8096 | `	/* Allocate a new class attribute */` |
|    300069 |  8097 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    300069 |  8098 | `	if( pCons ){` |
|    300069 |  8099 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    300069 |  8100 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8101 | `			return SXERR_ABORT;` |
|         - |  8102 | `		}` |
|    150032 |  8103 | `	}` |
|    300069 |  8104 | `	if( pCons == 0 ){` |
|       ! 0 |  8105 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8106 | `		return SXERR_ABORT;` |
|         - |  8107 | `	}` |
|    300069 |  8108 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8109 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8110 | `	}` |
|         - |  8111 | `	/* Swap bytecode container */` |
|    300069 |  8112 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    300069 |  8113 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8114 | `	/* Compile constant value.` |
|         - |  8115 | `	 */` |
|    300069 |  8116 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    300069 |  8117 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8118 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8119 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8120 | `			return SXERR_ABORT;` |
|         - |  8121 | `		}` |
|         1 |  8122 | `	}` |
|         - |  8123 | `	/* Emit the done instruction */` |
|    300069 |  8124 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    300069 |  8125 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    300069 |  8126 | `	if( rc == SXERR_ABORT ){` |
|         - |  8127 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8128 | `		return SXERR_ABORT;` |
|         - |  8129 | `	}` |
|         - |  8130 | `	/* All done,install the constant */` |
|    300069 |  8131 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    300069 |  8132 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8133 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8134 | `		return SXERR_ABORT;` |
|         - |  8135 | `	}` |
|    300069 |  8136 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8137 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8138 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8139 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8140 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8141 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8142 | `				pTok--;` |
|       ! 0 |  8143 | `			}` |
|       ! 0 |  8144 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8145 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8146 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8147 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8148 | `				return SXERR_ABORT;` |
|         - |  8149 | `			}` |
|       ! 0 |  8150 | `		}else{` |
|         3 |  8151 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8152 | `				goto loop;` |
|         - |  8153 | `			}` |
|         - |  8154 | `		}` |
|       ! 0 |  8155 | `	}` |
|    300067 |  8156 | `	SySetRelease(&aUnionAlts);` |
|    300067 |  8157 | `	return SXRET_OK;` |
|         5 |  8158 | `Synchronize:` |
|        13 |  8159 | `	SySetRelease(&aUnionAlts);` |
|         - |  8160 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8161 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8162 | `		pGen->pIn++;` |
|         3 |  8163 | `	}` |
|        13 |  8164 | `	return SXERR_CORRUPT;` |
|    150041 |  8165 | `}` |
|         - |  8166 | `/*` |
|         - |  8167 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8168 | ` * According to the PHP language reference manual` |
|         - |  8169 | ` *  Properties` |
|         - |  8170 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8171 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8172 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8173 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8174 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8175 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8176 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8177 | ` * Symisc eXtension.` |
|         - |  8178 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8179 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8180 | ` *  Example:` |
|         - |  8181 | ` *   class Test{` |
|         - |  8182 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8183 | ` *   };` |
|         - |  8184 | ` *   var_dump(TEST::myVar);` |
|         - |  8185 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8186 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8187 | ` */` |
|         - |  8188 | `/*` |
|         - |  8189 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8190 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8191 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8192 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8193 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8194 | ` */` |
|   2416596 |  8195 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8196 | `{` |
|   2416601 |  8197 | `	SyToken *p = pStart;` |
|   2416601 |  8198 | `	int bFirst = 1;` |
|   2416601 |  8199 | `	if( p >= pEnd ) return 0;` |
|         - |  8200 | ``	/* Optional nullable `?` shorthand. */`` |
|   2416601 |  8201 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        35 |  8202 | `		p++;` |
|        35 |  8203 | `		if( p >= pEnd ) return 0;` |
|        16 |  8204 | `	}` |
|         - |  8205 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8206 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8207 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8208 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1208298 |  8209 | `	for(;;){` |
|   2416621 |  8210 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8211 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8212 | `			p++;` |
|         9 |  8213 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8214 | `			if( p >= pEnd ) return 0;` |
|         3 |  8215 | `			p++; /* skip ')' */` |
|         2 |  8216 | `		}else{` |
|         - |  8217 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8218 | ``			 * then any `&`-joined intersection members. */`` |
|   2416619 |  8219 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2416619 |  8220 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8221 | `				return 0;` |
|         - |  8222 | `			}` |
|         - |  8223 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8224 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8225 | `			 * may still appear at the initial dispatch site). */` |
|   2416619 |  8226 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2416571 |  8227 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2416566 |  8228 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    106952 |  8229 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2416289 |  8230 | `					return 0;` |
|         - |  8231 | `				}` |
|       141 |  8232 | `			}` |
|       335 |  8233 | `			p++;` |
|       337 |  8234 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8235 | `				p += 2;` |
|         1 |  8236 | `			}` |
|       498 |  8237 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       338 |  8238 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8239 | `				p++; /* skip '&' */` |
|         3 |  8240 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8241 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8242 | `				p++;` |
|         3 |  8243 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8244 | `					p += 2;` |
|       ! 0 |  8245 | `				}` |
|         1 |  8246 | `			}` |
|         - |  8247 | `		}` |
|       337 |  8248 | `		bFirst = 0;` |
|       332 |  8249 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8250 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8251 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8252 | `			continue;` |
|         - |  8253 | `		}` |
|       317 |  8254 | `		break;` |
|       ! 0 |  8255 | `	}` |
|       317 |  8256 | `	if( p >= pEnd ) return 0;` |
|       317 |  8257 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1208303 |  8258 | `}` |
|         - |  8259 |  |
|         - |  8260 | `/*` |
|         - |  8261 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8262 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8263 | ` * if not). Recognized forms:` |
|         - |  8264 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8265 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8266 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8267 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8268 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8269 | ` * on unrecoverable error.` |
|         - |  8270 | ` *` |
|         - |  8271 | ` * When a type is parsed:` |
|         - |  8272 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8273 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8274 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8275 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8276 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8277 | ` */` |
|       322 |  8278 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8279 | `	ph7_gen_state *pGen,` |
|         - |  8280 | `	sxu32 *pnType,` |
|         - |  8281 | `	SyString *pClass,` |
|         - |  8282 | `	sxi32 *piTypeFlags,` |
|         - |  8283 | `	SyString *pTypeText,` |
|         - |  8284 | `	SySet *pAlts` |
|         5 |  8285 | `){` |
|       327 |  8286 | `	sxi32 iFlags = 0;` |
|         - |  8287 | `	sxi32 rc;` |
|       327 |  8288 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8289 | `		return SXRET_OK;` |
|         - |  8290 | `	}` |
|         - |  8291 | `	/* If the first token is '$', there's no type */` |
|       327 |  8292 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8293 | `		return SXRET_OK;` |
|         - |  8294 | `	}` |
|       327 |  8295 | `	rc = GenStateParseUnionTypeDecl(` |
|       161 |  8296 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8297 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8298 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8299 | `		/* bAllowVoid */ 0,` |
|       322 |  8300 | `		pGen->pIn->nLine);` |
|       327 |  8301 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8302 | `		return rc;` |
|         - |  8303 | `	}` |
|         - |  8304 | `	/* Verify next token is '$' (start of property name) */` |
|       327 |  8305 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8306 | `		return SXERR_SYNTAX;` |
|         - |  8307 | `	}` |
|       327 |  8308 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       327 |  8309 | `	return SXRET_OK;` |
|       166 |  8310 | `}` |
|         - |  8311 |  |
|         - |  8312 | `/*` |
|         - |  8313 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8314 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8315 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8316 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8317 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8318 | ` * by the type parser itself before reaching here.` |
|         - |  8319 | ` *` |
|         - |  8320 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8321 | ` * use in the error message.` |
|         - |  8322 | ` */` |
|       498 |  8323 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8324 | `	sxu32 nType,` |
|         - |  8325 | `	const SyString *pClass,` |
|         - |  8326 | `	const char **pzName,` |
|         - |  8327 | `	sxu32 *pnName)` |
|         5 |  8328 | `{` |
|         - |  8329 | `	const char *z;` |
|         - |  8330 | `	sxu32 n;` |
|       503 |  8331 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       449 |  8332 | `		return 0;` |
|         - |  8333 | `	}` |
|        59 |  8334 | `	z = pClass->zString;` |
|        59 |  8335 | `	n = pClass->nByte;` |
|        59 |  8336 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8337 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8338 | `	}` |
|         - |  8339 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8340 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8341 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        52 |  8342 | `	return 0;` |
|       254 |  8343 | `}` |
|         - |  8344 |  |
|         - |  8345 | `/*` |
|         - |  8346 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8347 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8348 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8349 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8350 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8351 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8352 | ` *` |
|         - |  8353 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8354 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8355 | ` */` |
|       436 |  8356 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8357 | `	ph7_gen_state *pGen,` |
|         - |  8358 | `	ph7_class *pClass,` |
|         - |  8359 | `	const SyString *pMemberName,` |
|         - |  8360 | `	sxu32 nType,` |
|         - |  8361 | `	const SyString *pTypeClass,` |
|         - |  8362 | `	const SyString *pTypeText,` |
|         - |  8363 | `	SySet *pUnionAlts,` |
|         - |  8364 | `	const char *zErrFmt,` |
|         - |  8365 | `	sxu32 nLine)` |
|         5 |  8366 | `{` |
|       441 |  8367 | `	const char *zBad = 0;` |
|       441 |  8368 | `	sxu32 nBad = 0;` |
|         - |  8369 | `	SyString sFallback;` |
|         - |  8370 | `	const SyString *pBad;` |
|         - |  8371 | `	sxi32 rc;` |
|       441 |  8372 | `	int bDisallowed = 0;` |
|       441 |  8373 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8374 | `		bDisallowed = 1;` |
|       439 |  8375 | `	}else if( pUnionAlts ){` |
|         - |  8376 | `		sxu32 i;` |
|        95 |  8377 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8378 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8379 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8380 | `				bDisallowed = 1;` |
|         3 |  8381 | `				break;` |
|         - |  8382 | `			}` |
|        35 |  8383 | `		}` |
|        15 |  8384 | `	}` |
|       441 |  8385 | `	if( !bDisallowed ){` |
|       435 |  8386 | `		return SXRET_OK;` |
|         - |  8387 | `	}` |
|         - |  8388 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8389 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8390 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8391 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8392 | `		pBad = pTypeText;` |
|         5 |  8393 | `	}else{` |
|       ! 0 |  8394 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8395 | `		pBad = &sFallback;` |
|         - |  8396 | `	}` |
|        11 |  8397 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8398 | `		zErrFmt,` |
|         3 |  8399 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8400 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8401 | `		return SXERR_ABORT;` |
|         - |  8402 | `	}` |
|         8 |  8403 | `	return SXERR_SYNTAX;` |
|       223 |  8404 | `}` |
|         - |  8405 | `/*` |
|         - |  8406 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8407 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8408 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8409 | ` * than promoted to a lexer keyword.` |
|         - |  8410 | ` */` |
|  18580982 |  8411 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8412 | `{` |
|  18782696 |  8413 | `	return (pTok->nType & PH7_TK_ID)` |
|   9492200 |  8414 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  18782691 |  8415 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8416 | `}` |
|         - |  8417 | `/*` |
|         - |  8418 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8419 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8420 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8421 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8422 | ` */` |
|   7098138 |  8423 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8424 | `{` |
|   7098143 |  8425 | `	*pnTok = 0;` |
|   7098138 |  8426 | `	if( &pTok[3] < pEnd` |
|   6684927 |  8427 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5641486 |  8428 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2505636 |  8429 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8430 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8431 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8432 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8433 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8434 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8435 | `			*pnTok = 4;` |
|        17 |  8436 | `			return nKw;` |
|         - |  8437 | `		}` |
|       ! 0 |  8438 | `	}` |
|   7098127 |  8439 | `	return 0;` |
|   3549074 |  8440 | `}` |
|         - |  8441 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8442 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8443 | `{` |
|        17 |  8444 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8445 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8446 | `	}` |
|         5 |  8447 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8448 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8449 | `	}` |
|         3 |  8450 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8451 | `}` |
|    470552 |  8452 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8453 | `{` |
|    470557 |  8454 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8455 | `	ph7_class_attr *pAttr;` |
|         - |  8456 | `	SyString *pName;` |
|         - |  8457 | `	sxi32 rc;` |
|    470557 |  8458 | `	sxu32 nType = 0;` |
|         - |  8459 | `	SyString sTypeClass;` |
|         - |  8460 | `	SyString sTypeText;` |
|         - |  8461 | `	SySet aUnionAlts;` |
|    470557 |  8462 | `	sxi32 iTypeFlags = 0;` |
|    470557 |  8463 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    470557 |  8464 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    470557 |  8465 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8466 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8467 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8468 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    470557 |  8469 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8470 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8471 | `	}` |
|         - |  8472 | `	/* Extract visibility level */` |
|    470557 |  8473 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8474 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    470718 |  8475 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       327 |  8476 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       327 |  8477 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8478 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8479 | `			goto Synchronize;` |
|       327 |  8480 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8481 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8482 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8483 | `				&pGen->pIn->sData);` |
|       ! 0 |  8484 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8485 | `				return SXERR_ABORT;` |
|         - |  8486 | `			}` |
|       ! 0 |  8487 | `			goto Synchronize;` |
|       327 |  8488 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8489 | `			return SXERR_ABORT;` |
|         - |  8490 | `		}` |
|       161 |  8491 | `	}` |
|       ! 0 |  8492 | `loop:` |
|    470561 |  8493 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8494 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8495 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8496 | `			return SXERR_ABORT;` |
|         - |  8497 | `		}` |
|       ! 0 |  8498 | `		goto Synchronize;` |
|         - |  8499 | `	}` |
|    470561 |  8500 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    470561 |  8501 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8502 | `		/* Invalid attribute name */` |
|       ! 0 |  8503 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8504 | `		if( rc == SXERR_ABORT ){` |
|         - |  8505 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8506 | `			return SXERR_ABORT;` |
|         - |  8507 | `		}` |
|       ! 0 |  8508 | `		goto Synchronize;` |
|         - |  8509 | `	}` |
|         - |  8510 | `	/* Peek attribute name */` |
|    470561 |  8511 | `	pName = &pGen->pIn->sData;` |
|         - |  8512 | `	/* Advance the stream cursor */` |
|    470561 |  8513 | `	pGen->pIn++;` |
|    470561 |  8514 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8515 | `		/* Invalid declaration */` |
|         3 |  8516 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8517 | `		if( rc == SXERR_ABORT ){` |
|         - |  8518 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8519 | `			return SXERR_ABORT;` |
|         - |  8520 | `		}` |
|         3 |  8521 | `		goto Synchronize;` |
|         - |  8522 | `	}` |
|         - |  8523 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8524 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    470559 |  8525 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8526 | `		const char *zAvErr = 0;` |
|        19 |  8527 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8528 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8529 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8530 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8531 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8532 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8533 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8534 | `		}` |
|        13 |  8535 | `		if( zAvErr ){` |
|       ! 0 |  8536 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8537 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8538 | `				return SXERR_ABORT;` |
|         - |  8539 | `			}` |
|       ! 0 |  8540 | `			goto Synchronize;` |
|         - |  8541 | `		}` |
|         6 |  8542 | `	}` |
|         - |  8543 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8544 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    470559 |  8545 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8546 | `		const char *zRoErr = 0;` |
|        43 |  8547 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8548 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8549 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8550 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8551 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8552 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8553 | `		}` |
|        43 |  8554 | `		if( zRoErr ){` |
|        13 |  8555 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8556 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8557 | `				return SXERR_ABORT;` |
|         - |  8558 | `			}` |
|        13 |  8559 | `			goto Synchronize;` |
|         - |  8560 | `		}` |
|        14 |  8561 | `	}` |
|         - |  8562 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8563 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8564 | `	 * by the type parser. */` |
|    470549 |  8565 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       485 |  8566 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8567 | `			&sTypeText,` |
|       320 |  8568 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       160 |  8569 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       325 |  8570 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8571 | `			return SXERR_ABORT;` |
|       325 |  8572 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8573 | `			goto Synchronize;` |
|         - |  8574 | `		}` |
|       160 |  8575 | `	}` |
|         - |  8576 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    470549 |  8577 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8578 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8579 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8580 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8581 | `			return SXERR_ABORT;` |
|         - |  8582 | `		}` |
|         3 |  8583 | `		goto Synchronize;` |
|         - |  8584 | `	}` |
|         - |  8585 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8586 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8587 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8588 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8589 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8590 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    470547 |  8591 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8592 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8593 | `			"New expressions are not supported in this context");` |
|         6 |  8594 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8595 | `			return SXERR_ABORT;` |
|         - |  8596 | `		}` |
|         6 |  8597 | `		goto Synchronize;` |
|         - |  8598 | `	}` |
|         - |  8599 | `	/* Allocate a new class attribute */` |
|    470543 |  8600 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    470543 |  8601 | `	if( pAttr ){` |
|    470543 |  8602 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    470543 |  8603 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8604 | `			return SXERR_ABORT;` |
|         - |  8605 | `		}` |
|    235269 |  8606 | `	}` |
|    470543 |  8607 | `	if( pAttr == 0 ){` |
|       ! 0 |  8608 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8609 | `		return SXERR_ABORT;` |
|         - |  8610 | `	}` |
|    470543 |  8611 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       323 |  8612 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       159 |  8613 | `	}` |
|    470543 |  8614 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8615 | `		SySet *pInstrContainer;` |
|    343989 |  8616 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    343989 |  8617 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8618 | `		{` |
|         - |  8619 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8620 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8621 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8622 | `			 * compiler would otherwise run into the hook tokens. */` |
|    343989 |  8623 | `			SyToken *pScan = pGen->pIn;` |
|    343989 |  8624 | `			sxi32 iNest = 0;` |
|    743563 |  8625 | `			while( pScan < pGen->pEnd ){` |
|    743563 |  8626 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     43493 |  8627 | `					iNest++;` |
|    721819 |  8628 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     43493 |  8629 | `					iNest--;` |
|    678331 |  8630 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    343989 |  8631 | `					break;` |
|         - |  8632 | `				}` |
|    399579 |  8633 | `				pScan++;` |
|         5 |  8634 | `			}` |
|    343989 |  8635 | `			pGen->pEnd = pScan;` |
|         - |  8636 | `		}` |
|         - |  8637 | `		/* Swap bytecode container */` |
|    343989 |  8638 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    343989 |  8639 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8640 | `		/* Compile attribute value.` |
|         - |  8641 | `		 */` |
|    343989 |  8642 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    343989 |  8643 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8644 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8645 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8646 | `				return SXERR_ABORT;` |
|         - |  8647 | `			}` |
|       ! 0 |  8648 | `		}` |
|         - |  8649 | `		/* Emit the done instruction */` |
|    343989 |  8650 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    343989 |  8651 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    343989 |  8652 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    343989 |  8653 | `		pGen->pEnd = pSavedDefEnd;` |
|    171992 |  8654 | `	}` |
|         - |  8655 | `	/* All done,install the attribute */` |
|    470543 |  8656 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    470543 |  8657 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8658 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8659 | `		return SXERR_ABORT;` |
|         - |  8660 | `	}` |
|    470543 |  8661 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8662 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8663 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8664 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8665 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8666 | `			return SXERR_ABORT;` |
|         - |  8667 | `		}` |
|        95 |  8668 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8669 | `			goto Synchronize;` |
|         - |  8670 | `		}` |
|        95 |  8671 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8672 | `		return SXRET_OK;` |
|         - |  8673 | `	}` |
|    470449 |  8674 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8675 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8676 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8677 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8678 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8679 | `				? "Interfaces may only include hooked properties"` |
|         - |  8680 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8681 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8682 | `			return SXERR_ABORT;` |
|         - |  8683 | `		}` |
|       ! 0 |  8684 | `		goto Synchronize;` |
|         - |  8685 | `	}` |
|    470449 |  8686 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8687 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8688 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8689 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8690 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8691 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8692 | `				pTok--;` |
|       ! 0 |  8693 | `			}` |
|       ! 0 |  8694 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8695 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8696 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8697 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8698 | `				return SXERR_ABORT;` |
|         - |  8699 | `			}` |
|       ! 0 |  8700 | `		}else{` |
|         5 |  8701 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8702 | `				goto loop;` |
|         - |  8703 | `			}` |
|         - |  8704 | `		}` |
|       ! 0 |  8705 | `	}` |
|    470445 |  8706 | `	SySetRelease(&aUnionAlts);` |
|    470445 |  8707 | `	return SXRET_OK;` |
|         9 |  8708 | `Synchronize:` |
|         - |  8709 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8710 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  8711 | `		pGen->pIn++;` |
|         3 |  8712 | `	}` |
|        22 |  8713 | `	SySetRelease(&aUnionAlts);` |
|        22 |  8714 | `	return SXERR_CORRUPT;` |
|    235281 |  8715 | `}` |
|         - |  8716 | `/*` |
|         - |  8717 | ` * Compile a class method.` |
|         - |  8718 | ` *` |
|         - |  8719 | ` * Refer to the official documentation for more information` |
|         - |  8720 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  8721 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  8722 | ` * overloading and many more.` |
|         - |  8723 | ` */` |
|   2463914 |  8724 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  8725 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  8726 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  8727 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  8728 | `	int doBody,          /* TRUE to process method body */` |
|         - |  8729 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  8730 | `	)` |
|         5 |  8731 | `{` |
|   2463919 |  8732 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2463919 |  8733 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  8734 | `	ph7_class_method *pMeth;` |
|         - |  8735 | `	sxi32 iFuncFlags;` |
|         - |  8736 | `	SyString *pName;` |
|         - |  8737 | `	SyToken *pEnd;` |
|         - |  8738 | `	sxi32 rc;` |
|         - |  8739 | `	/* Extract visibility level */` |
|   2463919 |  8740 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2463919 |  8741 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2463919 |  8742 | `	iFuncFlags = 0;` |
|   2463919 |  8743 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  8744 | `		/* Invalid method name */` |
|       ! 0 |  8745 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8746 | `		if( rc == SXERR_ABORT ){` |
|         - |  8747 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8748 | `			return SXERR_ABORT;` |
|         - |  8749 | `		}` |
|       ! 0 |  8750 | `		goto Synchronize;` |
|         - |  8751 | `	}` |
|   2463919 |  8752 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8753 | `		/* Return by reference,remember that */` |
|       ! 0 |  8754 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8755 | `		/* Jump the '&' token */` |
|       ! 0 |  8756 | `		pGen->pIn++;` |
|       ! 0 |  8757 | `	}` |
|   2463919 |  8758 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8759 | `		/* Invalid method name */` |
|       ! 0 |  8760 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8761 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8762 | `			return SXERR_ABORT;` |
|         - |  8763 | `		}` |
|       ! 0 |  8764 | `		goto Synchronize;` |
|         - |  8765 | `	}` |
|         - |  8766 | `	/* Peek method name */` |
|   2463919 |  8767 | `	pName = &pGen->pIn->sData;` |
|   2463919 |  8768 | `	nLine = pGen->pIn->nLine;` |
|         - |  8769 | `	/* Jump the method name */` |
|   2463919 |  8770 | `	pGen->pIn++;` |
|   2463919 |  8771 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8772 | `		/* Abstract method */` |
|    142123 |  8773 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  8774 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8775 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  8776 | `				&pClass->sName,pName);` |
|       ! 0 |  8777 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8778 | `				return SXERR_ABORT;` |
|         - |  8779 | `			}` |
|       ! 0 |  8780 | `		}` |
|         - |  8781 | `		/* Assemble method signature only */` |
|    142123 |  8782 | `		doBody = FALSE;` |
|     71059 |  8783 | `	}` |
|   2463919 |  8784 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8785 | `		/* Syntax error */` |
|       ! 0 |  8786 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  8787 | `		if( rc == SXERR_ABORT ){` |
|         - |  8788 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8789 | `			return SXERR_ABORT;` |
|         - |  8790 | `		}` |
|       ! 0 |  8791 | `		goto Synchronize;` |
|         - |  8792 | `	}` |
|         - |  8793 | `	/* Allocate a new class_method instance */` |
|   2463919 |  8794 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2463919 |  8795 | `	if( pMeth == 0 ){` |
|       ! 0 |  8796 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8797 | `		return SXERR_ABORT;` |
|         - |  8798 | `	}` |
|   2463919 |  8799 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2463919 |  8800 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2463919 |  8801 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8802 | `		return SXERR_ABORT;` |
|         - |  8803 | `	}` |
|         - |  8804 | `	/* Jump the left parenthesis '(' */` |
|   2463919 |  8805 | `	pGen->pIn++;` |
|   2463919 |  8806 | `	pEnd = 0; /* cc warning */` |
|         - |  8807 | `	/* Delimit the method signature */` |
|   2463919 |  8808 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2463919 |  8809 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  8810 | `		/* Syntax error */` |
|         3 |  8811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  8812 | `		if( rc == SXERR_ABORT ){` |
|         - |  8813 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8814 | `			return SXERR_ABORT;` |
|         - |  8815 | `		}` |
|         3 |  8816 | `		goto Synchronize;` |
|         - |  8817 | `	}` |
|         - |  8818 | `	{` |
|   2463917 |  8819 | `		int bIsCtor = 0;` |
|   2463917 |  8820 | `		int bAbstractCtor = 0;` |
|   2463912 |  8821 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1439281 |  8822 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2378974 |  8823 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    169891 |  8824 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  8825 | `				bAbstractCtor = 1;` |
|         2 |  8826 | `			}else{` |
|    169889 |  8827 | `				bIsCtor = 1;` |
|         - |  8828 | `			}` |
|     84943 |  8829 | `		}` |
|   2463917 |  8830 | `		if( pGen->pIn < pEnd ){` |
|         - |  8831 | `			/* Collect method arguments */` |
|    884587 |  8832 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    884587 |  8833 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8834 | `				return SXERR_ABORT;` |
|         - |  8835 | `			}` |
|    442291 |  8836 | `		}` |
|         - |  8837 | `	}` |
|         - |  8838 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2463917 |  8839 | `	pGen->pIn = &pEnd[1];` |
|         - |  8840 | `	{` |
|   2463917 |  8841 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2463917 |  8842 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  8843 | `			return SXERR_ABORT;` |
|   2463917 |  8844 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  8845 | `			goto Synchronize;` |
|         - |  8846 | `		}` |
|         - |  8847 | `	}` |
|         - |  8848 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  8849 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  8850 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  8851 | `	{` |
|   2463917 |  8852 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  8853 | `		sxu32 i;` |
|   3790629 |  8854 | `		for( i = 0; i < nArg; i++ ){` |
|   1326727 |  8855 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  8856 | `			ph7_class_attr *pAttr;` |
|   1326727 |  8857 | `			sxi32 iAttrFlags = 0;` |
|         - |  8858 | `			int bArgTyped;` |
|   1326727 |  8859 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1326643 |  8860 | `				continue;` |
|         - |  8861 | `			}` |
|         - |  8862 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  8863 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  8864 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  8865 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  8866 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  8867 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  8868 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8869 | `					"Cannot declare variadic promoted property");` |
|         3 |  8870 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8871 | `					return SXERR_ABORT;` |
|         - |  8872 | `				}` |
|         3 |  8873 | `				goto Synchronize;` |
|         - |  8874 | `			}` |
|         - |  8875 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  8876 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  8877 | `			 * appear as an alternative of a union type. */` |
|        87 |  8878 | `			if( bArgTyped ){` |
|       122 |  8879 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  8880 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  8881 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  8882 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  8883 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8884 | `					return SXERR_ABORT;` |
|        83 |  8885 | `				}else if( rc != SXRET_OK ){` |
|         6 |  8886 | `					goto Synchronize;` |
|         - |  8887 | `				}` |
|        37 |  8888 | `			}` |
|         - |  8889 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  8890 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  8891 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8892 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  8893 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8894 | `					return SXERR_ABORT;` |
|         - |  8895 | `				}` |
|         3 |  8896 | `				goto Synchronize;` |
|         - |  8897 | `			}` |
|        81 |  8898 | `			if( bArgTyped ){` |
|        77 |  8899 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  8900 | `			}` |
|        81 |  8901 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  8902 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  8903 | `			}` |
|        81 |  8904 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  8905 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  8906 | `			}` |
|        81 |  8907 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  8908 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  8909 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  8910 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  8911 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8912 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  8913 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8914 | `						return SXERR_ABORT;` |
|         - |  8915 | `					}` |
|         3 |  8916 | `					goto Synchronize;` |
|         - |  8917 | `				}` |
|        24 |  8918 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  8919 | `			}` |
|        79 |  8920 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  8921 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  8922 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8923 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8924 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  8925 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  8926 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8927 | `						return SXERR_ABORT;` |
|         - |  8928 | `					}` |
|       ! 0 |  8929 | `					goto Synchronize;` |
|         - |  8930 | `				}` |
|         5 |  8931 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  8932 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  8933 | `			}` |
|        79 |  8934 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  8935 | `			if( pAttr == 0 ){` |
|       ! 0 |  8936 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8937 | `				return SXERR_ABORT;` |
|         - |  8938 | `			}` |
|        79 |  8939 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  8940 | `				pAttr->nType = pArg->nType;` |
|        77 |  8941 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  8942 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  8943 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8944 | `					sxu32 k;` |
|        20 |  8945 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  8946 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  8947 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  8948 | `					}` |
|         3 |  8949 | `				}` |
|        36 |  8950 | `			}` |
|        79 |  8951 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  8952 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  8953 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8954 | `				return SXERR_ABORT;` |
|         - |  8955 | `			}` |
|        42 |  8956 | `		}` |
|         - |  8957 | `	}` |
|   2463907 |  8958 | `	if( doBody ){` |
|         - |  8959 | `		/* Compile method body */` |
|   2321789 |  8960 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2321789 |  8961 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8962 | `			return SXERR_ABORT;` |
|         - |  8963 | `		}` |
|         - |  8964 | `		/* The cursor sits just past the body's closing brace */` |
|   2321789 |  8965 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1160897 |  8966 | `	}else{` |
|         - |  8967 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    142123 |  8968 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    142123 |  8969 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     71059 |  8970 | `		}` |
|         - |  8971 | `		/* Only method signature is allowed */` |
|    142123 |  8972 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  8973 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  8974 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  8975 | `				if( rc == SXERR_ABORT ){` |
|         - |  8976 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  8977 | `					return SXERR_ABORT;` |
|         - |  8978 | `				}` |
|       ! 0 |  8979 | `				return SXERR_CORRUPT;` |
|         - |  8980 | `			}` |
|         - |  8981 | `	}` |
|         - |  8982 | `	/* All done,install the method */` |
|   2463907 |  8983 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2463907 |  8984 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8985 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8986 | `		return SXERR_ABORT;` |
|         - |  8987 | `	}` |
|   2463907 |  8988 | `	return SXRET_OK;` |
|         6 |  8989 | `Synchronize:` |
|         - |  8990 | `	/* Synchronize with the first semi-colon */` |
|        40 |  8991 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  8992 | `		pGen->pIn++;` |
|         4 |  8993 | `	}` |
|        16 |  8994 | `	return SXERR_CORRUPT;` |
|   1231962 |  8995 | `}` |
|         - |  8996 | `/*` |
|         - |  8997 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  8998 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  8999 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9000 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9001 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9002 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9003 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9004 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9005 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9006 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9007 | `` * implicit `$value` formal.`` |
|         - |  9008 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9009 | ` */` |
|         - |  9010 | `/*` |
|         - |  9011 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9012 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9013 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9014 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9015 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9016 | ` */` |
|        94 |  9017 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9018 | `{` |
|         - |  9019 | `	SyToken *p;` |
|       345 |  9020 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9021 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9022 | `			continue;` |
|         - |  9023 | `		}` |
|         - |  9024 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9025 | `		if( p + 3 < pEnd` |
|        80 |  9026 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9027 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9028 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9029 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9030 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9031 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9032 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9033 | `			return 1;` |
|         - |  9034 | `		}` |
|         - |  9035 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9036 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9037 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9038 | `		if( p > pStart` |
|        26 |  9039 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9040 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9041 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9042 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9043 | `			return 1;` |
|         - |  9044 | `		}` |
|        15 |  9045 | `	}` |
|        43 |  9046 | `	return 0;` |
|        48 |  9047 | `}` |
|         - |  9048 | `/*` |
|         - |  9049 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9050 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9051 | ` */` |
|       990 |  9052 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9053 | `{` |
|      1167 |  9054 | `	return p + 6 < pEnd` |
|       671 |  9055 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9056 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9057 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9058 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9059 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9060 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9061 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9062 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9063 | `	 && p[5].sData.nByte == 3` |
|         8 |  9064 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9065 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9066 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9067 | `}` |
|         - |  9068 | `/*` |
|         - |  9069 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9070 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9071 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9072 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9073 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9074 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9075 | ` * or SXERR_MEM.` |
|         - |  9076 | ` */` |
|         4 |  9077 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9078 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9079 | `{` |
|         5 |  9080 | `	SyToken *p = pStart;` |
|        35 |  9081 | `	while( p < pEnd ){` |
|        31 |  9082 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9083 | `			SyToken sTok;` |
|         - |  9084 | `			char zName[384];` |
|         - |  9085 | `			sxu32 nName;` |
|         - |  9086 | `			char *zDup;` |
|         - |  9087 | ``			/* `parent` `::` */`` |
|         5 |  9088 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9089 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9090 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9091 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9092 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9093 | `			if( zDup == 0 ){` |
|       ! 0 |  9094 | `				return SXERR_MEM;` |
|         - |  9095 | `			}` |
|         5 |  9096 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9097 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9098 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9099 | `			sTok.pUserData = 0;` |
|         5 |  9100 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9101 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9102 | `			continue;` |
|         - |  9103 | `		}` |
|        27 |  9104 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9105 | `		p++;` |
|         1 |  9106 | `	}` |
|         5 |  9107 | `	return SXRET_OK;` |
|         3 |  9108 | `}` |
|        94 |  9109 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9110 | `{` |
|        95 |  9111 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9112 | `	sxi32 rc;` |
|        95 |  9113 | `	int bRefsSelf = 0;` |
|        95 |  9114 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9115 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9116 | `		char zHook[384];` |
|         - |  9117 | `		SyString sHookName;` |
|         - |  9118 | `		ph7_class_method *pMeth;` |
|         - |  9119 | `		int bGet;` |
|       159 |  9120 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9121 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9122 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9123 | `			continue;` |
|         - |  9124 | `		}` |
|       145 |  9125 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9126 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9127 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9128 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9129 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9130 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9131 | `				return SXERR_ABORT;` |
|         - |  9132 | `			}` |
|       ! 0 |  9133 | `			return SXERR_CORRUPT;` |
|         - |  9134 | `		}` |
|       145 |  9135 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9136 | `			goto HookSyntax;` |
|         - |  9137 | `		}` |
|       144 |  9138 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9139 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9140 | `			bGet = 1;` |
|       106 |  9141 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9142 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9143 | `			bGet = 0;` |
|        34 |  9144 | `		}else{` |
|       ! 0 |  9145 | `			goto HookSyntax;` |
|         - |  9146 | `		}` |
|       145 |  9147 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9148 | `		sHookName.zString = zHook;` |
|       217 |  9149 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9150 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9151 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9152 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9153 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9154 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9155 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9156 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9157 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9158 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9159 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9160 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9161 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9162 | `					return SXERR_ABORT;` |
|         - |  9163 | `				}` |
|       ! 0 |  9164 | `				return SXERR_CORRUPT;` |
|         - |  9165 | `			}` |
|        15 |  9166 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9167 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9168 | `			if( pMeth == 0 ){` |
|       ! 0 |  9169 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9170 | `				return SXERR_ABORT;` |
|         - |  9171 | `			}` |
|        15 |  9172 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9173 | `			if( !bGet ){` |
|         - |  9174 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9175 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9176 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9177 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9178 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9179 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9180 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9181 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9182 | `				if( zVName == 0 ){` |
|       ! 0 |  9183 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9184 | `					return SXERR_ABORT;` |
|         - |  9185 | `				}` |
|         7 |  9186 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9187 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9188 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9189 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9190 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9191 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9192 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9193 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9194 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9195 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9196 | `				}` |
|         7 |  9197 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9198 | `			}` |
|        15 |  9199 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9200 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9201 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9202 | `				return SXERR_ABORT;` |
|         - |  9203 | `			}` |
|        15 |  9204 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9205 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9206 | `		}` |
|       130 |  9207 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9208 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9209 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9210 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9211 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9212 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9213 | `				return SXERR_ABORT;` |
|         - |  9214 | `			}` |
|       ! 0 |  9215 | `			return SXERR_CORRUPT;` |
|         - |  9216 | `		}` |
|       131 |  9217 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9218 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9219 | `		if( pMeth == 0 ){` |
|       ! 0 |  9220 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9221 | `			return SXERR_ABORT;` |
|         - |  9222 | `		}` |
|       131 |  9223 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9224 | `		if( !bGet ){` |
|         - |  9225 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9226 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9227 | `				SyToken *pRp = 0;` |
|        17 |  9228 | `				pGen->pIn++;` |
|        17 |  9229 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9230 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9231 | `					goto HookSyntax;` |
|         - |  9232 | `				}` |
|        17 |  9233 | `				if( pGen->pIn < pRp ){` |
|        17 |  9234 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9235 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9236 | `						return SXERR_ABORT;` |
|         - |  9237 | `					}` |
|         8 |  9238 | `				}` |
|        17 |  9239 | `				pGen->pIn = &pRp[1];` |
|         8 |  9240 | `			}` |
|        61 |  9241 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9242 | `				/* Implicit $value formal */` |
|         - |  9243 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9244 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9245 | `				if( zVName == 0 ){` |
|       ! 0 |  9246 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9247 | `					return SXERR_ABORT;` |
|         - |  9248 | `				}` |
|        45 |  9249 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9250 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9251 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9252 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9253 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9254 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9255 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9256 | `			}` |
|        30 |  9257 | `		}` |
|       165 |  9258 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9259 | `			/* Block body */` |
|        69 |  9260 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9261 | `			SyToken *pCloser = 0;` |
|        69 |  9262 | `			int bParentCall = 0;` |
|        69 |  9263 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9264 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9265 | `				SyToken *pScan;` |
|       753 |  9266 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9267 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9268 | `						bParentCall = 1;` |
|         3 |  9269 | `						break;` |
|         - |  9270 | `					}` |
|       343 |  9271 | `				}` |
|        34 |  9272 | `			}` |
|        69 |  9273 | `			if( bParentCall ){` |
|         - |  9274 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9275 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9276 | `				 * hook method), then continue past the original body. */` |
|         - |  9277 | `				SySet sBody;` |
|         3 |  9278 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9279 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9280 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9281 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9282 | `					SySetRelease(&sBody);` |
|       ! 0 |  9283 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9284 | `					return SXERR_ABORT;` |
|         - |  9285 | `				}` |
|         3 |  9286 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9287 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9288 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9289 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9290 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9291 | `				SySetRelease(&sBody);` |
|         3 |  9292 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9293 | `					return SXERR_ABORT;` |
|         - |  9294 | `				}` |
|         3 |  9295 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9296 | `			}else{` |
|        67 |  9297 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9298 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9299 | `					return SXERR_ABORT;` |
|         - |  9300 | `				}` |
|        67 |  9301 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9302 | `			}` |
|        69 |  9303 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9304 | `				bRefsSelf = 1;` |
|         9 |  9305 | `			}` |
|       128 |  9306 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9307 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9308 | `			GenBlock *pBlock;` |
|         - |  9309 | `			SySet *pInstrContainer;` |
|         - |  9310 | `			SyToken *pBodyStart;` |
|         - |  9311 | `			SyToken *pExprEnd;` |
|        63 |  9312 | `			SyToken *pSavedEnd = 0;` |
|         - |  9313 | `			SySet sBody;` |
|        63 |  9314 | `			int bParentCall = 0;` |
|        63 |  9315 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9316 | `			pBodyStart = pGen->pIn;` |
|         - |  9317 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9318 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9319 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9320 | `			 * method on a token copy. */` |
|         - |  9321 | `			{` |
|        63 |  9322 | `				sxi32 iNest = 0;` |
|        63 |  9323 | `				pExprEnd = pBodyStart;` |
|       355 |  9324 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9325 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9326 | `						iNest++;` |
|       351 |  9327 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9328 | `						if( iNest <= 0 ){` |
|       ! 0 |  9329 | `							break;` |
|         - |  9330 | `						}` |
|         9 |  9331 | `						iNest--;` |
|       343 |  9332 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9333 | `						break;` |
|         - |  9334 | `					}` |
|       293 |  9335 | `					pExprEnd++;` |
|         1 |  9336 | `				}` |
|         - |  9337 | `			}` |
|         - |  9338 | `			{` |
|         - |  9339 | `				SyToken *pScan;` |
|       335 |  9340 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9341 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9342 | `						bParentCall = 1;` |
|         3 |  9343 | `						break;` |
|         - |  9344 | `					}` |
|       137 |  9345 | `				}` |
|         - |  9346 | `			}` |
|        63 |  9347 | `			if( bParentCall ){` |
|         3 |  9348 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9349 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9350 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9351 | `					SySetRelease(&sBody);` |
|       ! 0 |  9352 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9353 | `					return SXERR_ABORT;` |
|         - |  9354 | `				}` |
|         3 |  9355 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9356 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9357 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9358 | `			}` |
|        94 |  9359 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9360 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9361 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9362 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9363 | `				return SXERR_ABORT;` |
|         - |  9364 | `			}` |
|        63 |  9365 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9366 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9367 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9368 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9369 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9370 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9371 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9372 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9373 | `			if( bParentCall ){` |
|         3 |  9374 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9375 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9376 | `				SySetRelease(&sBody);` |
|         1 |  9377 | `			}` |
|        63 |  9378 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9379 | `				return SXERR_ABORT;` |
|         - |  9380 | `			}` |
|        63 |  9381 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9382 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9383 | `				bRefsSelf = 1;` |
|        18 |  9384 | `			}` |
|        63 |  9385 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9386 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9387 | `			}` |
|        63 |  9388 | `			if( !bGet ){` |
|         - |  9389 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9390 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9391 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9392 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9393 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9394 | `				bRefsSelf = 1;` |
|         1 |  9395 | `			}` |
|        32 |  9396 | `		}else{` |
|       ! 0 |  9397 | `			goto HookSyntax;` |
|         - |  9398 | `		}` |
|       131 |  9399 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9400 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9401 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9402 | `			return SXERR_ABORT;` |
|         - |  9403 | `		}` |
|       131 |  9404 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9405 | `	}` |
|        95 |  9406 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9407 | `		goto HookSyntax;` |
|         - |  9408 | `	}` |
|        95 |  9409 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9410 | `	if( !bRefsSelf ){` |
|         - |  9411 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9412 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9413 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9414 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9415 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9416 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9417 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9418 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9419 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9420 | `				return SXERR_ABORT;` |
|         - |  9421 | `			}` |
|       ! 0 |  9422 | `			return SXERR_CORRUPT;` |
|         - |  9423 | `		}` |
|        20 |  9424 | `	}` |
|        95 |  9425 | `	return SXRET_OK;` |
|       ! 0 |  9426 | `HookSyntax:` |
|       ! 0 |  9427 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9428 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9429 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9430 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9431 | `		return SXERR_ABORT;` |
|         - |  9432 | `	}` |
|       ! 0 |  9433 | `	return SXERR_CORRUPT;` |
|        48 |  9434 | `}` |
|         - |  9435 | `/*` |
|         - |  9436 | ` * Compile an object interface.` |
|         - |  9437 | ` *  According to the PHP language reference manual` |
|         - |  9438 | ` *   Object Interfaces:` |
|         - |  9439 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9440 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9441 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9442 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9443 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9444 | ` */` |
|     71132 |  9445 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9446 | `{` |
|     71137 |  9447 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9448 | `	ph7_class *pClass,*pBase;` |
|         - |  9449 | `	SyToken *pEnd,*pTmp;` |
|         - |  9450 | `	SyString *pName;` |
|         - |  9451 | `	sxi32 nKwrd;` |
|         - |  9452 | `	sxi32 rc;` |
|         - |  9453 | `	/* Jump the 'interface' keyword */` |
|     71137 |  9454 | `	pGen->pIn++;` |
|         - |  9455 | `	/* Extract interface name */` |
|     71137 |  9456 | `	pName = &pGen->pIn->sData;` |
|         - |  9457 | `	/* Advance the stream cursor */` |
|     71137 |  9458 | `	pGen->pIn++;` |
|         - |  9459 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9460 | `		SyBlob sFQN;` |
|         - |  9461 | `		SyString sFQNStr;` |
|     71137 |  9462 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     71137 |  9463 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     71137 |  9464 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     71137 |  9465 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     71137 |  9466 | `		SyBlobRelease(&sFQN);` |
|         - |  9467 | `	}` |
|     71137 |  9468 | `	if( pClass == 0 ){` |
|       ! 0 |  9469 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9470 | `		return SXERR_ABORT;` |
|         - |  9471 | `	}` |
|     71137 |  9472 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     71137 |  9473 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9474 | `		return SXERR_ABORT;` |
|         - |  9475 | `	}` |
|         - |  9476 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     71137 |  9477 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9478 | `	/* Assume no base class is given */` |
|     71137 |  9479 | `	pBase = 0;` |
|     71137 |  9480 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     27637 |  9481 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     27637 |  9482 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9483 | `			SyBlob sResolved;` |
|         - |  9484 | `			SyString sBaseName;` |
|         - |  9485 | `			sxu32 nRefLine;` |
|         - |  9486 | `			/* Extract base interface */` |
|     27637 |  9487 | `			pGen->pIn++;` |
|     27637 |  9488 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     27637 |  9489 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     27637 |  9490 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9491 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9492 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9493 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9494 | `					pName);` |
|       ! 0 |  9495 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9496 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9497 | `					return SXERR_ABORT;` |
|         - |  9498 | `				}` |
|       ! 0 |  9499 | `				return SXRET_OK;` |
|         - |  9500 | `			}` |
|     41453 |  9501 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     27632 |  9502 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     27637 |  9503 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9504 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9505 | `			/* Only interfaces is allowed */` |
|     27637 |  9506 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9507 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9508 | `			}` |
|     27637 |  9509 | `			if( pBase == 0 ){` |
|       ! 0 |  9510 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9511 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9512 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9513 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9514 | `					return SXERR_ABORT;` |
|         - |  9515 | `				}` |
|       ! 0 |  9516 | `			}` |
|     27637 |  9517 | `			SyBlobRelease(&sResolved);` |
|     13816 |  9518 | `		}` |
|     13816 |  9519 | `	}` |
|     71137 |  9520 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9521 | `		/* Syntax error */` |
|       ! 0 |  9522 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9523 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9524 | `		if( rc == SXERR_ABORT ){` |
|         - |  9525 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9526 | `			return SXERR_ABORT;` |
|         - |  9527 | `		}` |
|       ! 0 |  9528 | `		return SXRET_OK;` |
|         - |  9529 | `	}` |
|     71137 |  9530 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     71137 |  9531 | `	pEnd = 0; /* cc warning */` |
|         - |  9532 | `	/* Delimit the interface body */` |
|     71137 |  9533 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     71137 |  9534 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9535 | `		/* Syntax error */` |
|       ! 0 |  9536 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9537 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9538 | `		if( rc == SXERR_ABORT ){` |
|         - |  9539 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9540 | `			return SXERR_ABORT;` |
|         - |  9541 | `		}` |
|       ! 0 |  9542 | `		return SXRET_OK;` |
|         - |  9543 | `	}` |
|         - |  9544 | `	/* The delimiter token is the interface body's closing brace */` |
|     71137 |  9545 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9546 | `	/* Swap token stream */` |
|     71137 |  9547 | `	pTmp = pGen->pEnd;` |
|     71137 |  9548 | `	pGen->pEnd = pEnd;` |
|         - |  9549 | `	/* Start the parse process` |
|         - |  9550 | `	 * Note (According to the PHP reference manual):` |
|         - |  9551 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9552 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9553 | `	 */` |
|    130299 |  9554 | `	for(;;){` |
|         - |  9555 | `		/* Jump leading/trailing semi-colons */` |
|    450073 |  9556 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    189471 |  9557 | `			pGen->pIn++;` |
|         5 |  9558 | `		}` |
|    260607 |  9559 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9560 | `			/* End of interface body */` |
|     71133 |  9561 | `			break;` |
|         - |  9562 | `		}` |
|         - |  9563 | `		/* Bind a directly-preceding docblock to this member */` |
|    189479 |  9564 | `		GenStateSetPendingDoc(&(*pGen));` |
|    189479 |  9565 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9566 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9567 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9568 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9569 | `			if( rc == SXERR_ABORT ){` |
|         - |  9570 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9571 | `				return SXERR_ABORT;` |
|         - |  9572 | `			}` |
|       ! 0 |  9573 | `			goto done;` |
|         - |  9574 | `		}` |
|         - |  9575 | `		/* Extract the current keyword */` |
|    189479 |  9576 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    189479 |  9577 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9578 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9579 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9580 | `			const char *zKind = "member";` |
|         3 |  9581 | `			SyString *pMemberName = 0;` |
|         3 |  9582 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9583 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9584 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9585 | `					zKind = "constant";` |
|         3 |  9586 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9587 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9588 | `					}` |
|         1 |  9589 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9590 | `					zKind = "method";` |
|       ! 0 |  9591 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9592 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9593 | `					}` |
|       ! 0 |  9594 | `				}` |
|         1 |  9595 | `			}` |
|         3 |  9596 | `			if( pMemberName ){` |
|         4 |  9597 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9598 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9599 | `			}else{` |
|       ! 0 |  9600 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9601 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9602 | `			}` |
|         3 |  9603 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9604 | `				return SXERR_ABORT;` |
|         - |  9605 | `			}` |
|         3 |  9606 | `			goto done;` |
|         - |  9607 | `		}` |
|    189477 |  9608 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9609 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9610 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9611 | `			if( rc == SXERR_ABORT ){` |
|         - |  9612 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9613 | `				return SXERR_ABORT;` |
|         - |  9614 | `			}` |
|       ! 0 |  9615 | `			goto done;` |
|         - |  9616 | `		}` |
|    189477 |  9617 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9618 | `			/* Advance the stream cursor */` |
|    134215 |  9619 | `			pGen->pIn++;` |
|    134210 |  9620 | `			if( pGen->pIn < pGen->pEnd` |
|    134215 |  9621 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    134210 |  9622 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9623 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9624 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9625 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9626 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9627 | `				 * hooked properties" error). */` |
|       ! 0 |  9628 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9629 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9630 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9631 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9632 | `						return SXERR_ABORT;` |
|         - |  9633 | `					}` |
|       ! 0 |  9634 | `					goto done;` |
|         - |  9635 | `				}` |
|       ! 0 |  9636 | `				continue;` |
|         - |  9637 | `			}` |
|    134215 |  9638 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9639 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9640 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9641 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9642 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9643 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9644 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9645 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9646 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9647 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9648 | `							return SXERR_ABORT;` |
|         - |  9649 | `						}` |
|       ! 0 |  9650 | `						goto done;` |
|         - |  9651 | `					}` |
|       ! 0 |  9652 | `					continue;` |
|         - |  9653 | `				}` |
|       ! 0 |  9654 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9655 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9656 | `				if( rc == SXERR_ABORT ){` |
|         - |  9657 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9658 | `					return SXERR_ABORT;` |
|         - |  9659 | `				}` |
|       ! 0 |  9660 | `				goto done;` |
|         - |  9661 | `			}` |
|    134215 |  9662 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    134215 |  9663 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9664 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9665 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9666 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9667 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9668 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9669 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9670 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9671 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9672 | `							return SXERR_ABORT;` |
|         - |  9673 | `						}` |
|       ! 0 |  9674 | `						goto done;` |
|         - |  9675 | `					}` |
|         5 |  9676 | `					continue;` |
|         - |  9677 | `				}` |
|       ! 0 |  9678 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9679 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9680 | `				if( rc == SXERR_ABORT ){` |
|         - |  9681 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9682 | `					return SXERR_ABORT;` |
|         - |  9683 | `				}` |
|       ! 0 |  9684 | `				goto done;` |
|         - |  9685 | `			}` |
|     67103 |  9686 | `		}` |
|    189473 |  9687 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9688 | `			/* Parse constant */` |
|     55263 |  9689 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     55263 |  9690 | `			if( rc != SXRET_OK ){` |
|         3 |  9691 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9692 | `					return SXERR_ABORT;` |
|         - |  9693 | `				}` |
|         3 |  9694 | `				goto done;` |
|         - |  9695 | `			}` |
|     27633 |  9696 | `		}else{` |
|    134215 |  9697 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    134215 |  9698 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9699 | `				/* Static method,record that */` |
|     11843 |  9700 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9701 | `				/* Advance the stream cursor */` |
|     11843 |  9702 | `				pGen->pIn++;` |
|     11838 |  9703 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11843 |  9704 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9705 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9706 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9707 | `						if( rc == SXERR_ABORT ){` |
|         - |  9708 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9709 | `							return SXERR_ABORT;` |
|         - |  9710 | `						}` |
|       ! 0 |  9711 | `						goto done;` |
|         - |  9712 | `				}` |
|      5919 |  9713 | `			}` |
|         - |  9714 | `			/* Process method signature (no body for interface methods) */` |
|    134215 |  9715 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    134215 |  9716 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9717 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9718 | `					return SXERR_ABORT;` |
|         - |  9719 | `				}` |
|       ! 0 |  9720 | `				goto done;` |
|         - |  9721 | `			}` |
|         - |  9722 | `		}` |
|         5 |  9723 | `	}` |
|         - |  9724 | `	/* Install the interface */` |
|     71133 |  9725 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     71133 |  9726 | `	if( rc == SXRET_OK && pBase ){` |
|         - |  9727 | `		/* Inherit from the base interface */` |
|     27637 |  9728 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13816 |  9729 | `	}` |
|     71133 |  9730 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9731 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9732 | `		return SXERR_ABORT;` |
|         - |  9733 | `	}` |
|     35564 |  9734 | `done:` |
|         - |  9735 | `	/* Point beyond the interface body */` |
|     71137 |  9736 | `	pGen->pIn  = &pEnd[1];` |
|     71137 |  9737 | `	pGen->pEnd = pTmp;` |
|     71137 |  9738 | `	return PH7_OK;` |
|     35571 |  9739 | `}` |
|         - |  9740 | `/*` |
|         - |  9741 | ` * Compile a user-defined class.` |
|         - |  9742 | ` * According to the PHP language reference manual` |
|         - |  9743 | ` *  class` |
|         - |  9744 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - |  9745 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - |  9746 | ` *  of the properties and methods belonging to the class.` |
|         - |  9747 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - |  9748 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - |  9749 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - |  9750 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  9751 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - |  9752 | ` *  (called "methods").` |
|         - |  9753 | ` */` |
|         - |  9754 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - |  9755 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - |  9756 | `struct TraitUseEntry {` |
|         - |  9757 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - |  9758 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - |  9759 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - |  9760 | `};` |
|         - |  9761 | `/*` |
|         - |  9762 | ` * Validate that methods implementing interface contracts have compatible` |
|         - |  9763 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - |  9764 | ` */` |
|    364758 |  9765 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9766 | `{` |
|         - |  9767 | `	ph7_class **apIface;` |
|         - |  9768 | `	sxu32 nIface,i;` |
|         - |  9769 | `	sxi32 rc;` |
|    364763 |  9770 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 |  9771 | `		return SXRET_OK;` |
|         - |  9772 | `	}` |
|    364763 |  9773 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    364763 |  9774 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    732073 |  9775 | `	for(i = 0; i < nIface; i++){` |
|    367315 |  9776 | `		ph7_class *pIface = apIface[i];` |
|         - |  9777 | `		SyHashEntry *pEntry;` |
|    367315 |  9778 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1058445 |  9779 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    691135 |  9780 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - |  9781 | `			ph7_class_method *pImplMeth;` |
|    691135 |  9782 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - |  9783 | `			/* Find the implementing method in the class */` |
|    691135 |  9784 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    691135 |  9785 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 |  9786 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - |  9787 | `			}` |
|         - |  9788 | `			/* Check visibility: interface methods must be implemented as public */` |
|    691117 |  9789 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 |  9790 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9791 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 |  9792 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 |  9793 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9794 | `					return SXERR_ABORT;` |
|         - |  9795 | `				}` |
|         1 |  9796 | `			}` |
|         - |  9797 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - |  9798 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - |  9799 | `			 */` |
|         - |  9800 | `			{` |
|    691117 |  9801 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    691117 |  9802 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    691117 |  9803 | `				int sigError = 0;` |
|    691117 |  9804 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 |  9805 | `					sigError = 1;` |
|    691116 |  9806 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - |  9807 | `					/* Extra parameters must all have default values */` |
|      3955 |  9808 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - |  9809 | `					sxu32 k;` |
|      7903 |  9810 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3955 |  9811 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 |  9812 | `							sigError = 1;` |
|         3 |  9813 | `							break;` |
|         - |  9814 | `						}` |
|      1979 |  9815 | `					}` |
|      1975 |  9816 | `				}` |
|    691117 |  9817 | `				if( sigError ){` |
|         - |  9818 | `					SyBlob sImplSig, sIfaceSig;` |
|         - |  9819 | `					ph7_vm_func_arg *aArgs;` |
|         - |  9820 | `					sxu32 j;` |
|         6 |  9821 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 |  9822 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - |  9823 | `					/* Build implementing method signature */` |
|         6 |  9824 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 |  9825 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 |  9826 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 |  9827 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 |  9828 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9829 | `					}` |
|         - |  9830 | `					/* Build interface method signature */` |
|         6 |  9831 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 |  9832 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 |  9833 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 |  9834 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 |  9835 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9836 | `					}` |
|         8 |  9837 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9838 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 |  9839 | `						&pClass->sName,pMName,` |
|         4 |  9840 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 |  9841 | `						&pIface->sName,pMName,` |
|         4 |  9842 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 |  9843 | `					SyBlobRelease(&sImplSig);` |
|         6 |  9844 | `					SyBlobRelease(&sIfaceSig);` |
|         6 |  9845 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9846 | `						return SXERR_ABORT;` |
|         - |  9847 | `					}` |
|         2 |  9848 | `				}` |
|         - |  9849 | `			}` |
|         5 |  9850 | `		}` |
|    183660 |  9851 | `	}` |
|    364763 |  9852 | `	return SXRET_OK;` |
|    182384 |  9853 | `}` |
|         - |  9854 | `/*` |
|         - |  9855 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - |  9856 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - |  9857 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - |  9858 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - |  9859 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - |  9860 | ` * means that specific hook is still missing.` |
|         - |  9861 | ` */` |
|        38 |  9862 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 |  9863 | `{` |
|         - |  9864 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - |  9865 | `	ph7_class_attr *pProp;` |
|        38 |  9866 | `	if( pMName->nByte <= nPfx` |
|        27 |  9867 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 |  9868 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 |  9869 | `		return 0; /* not a hook stub */` |
|         - |  9870 | `	}` |
|         7 |  9871 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 |  9872 | `	return pProp != 0` |
|         6 |  9873 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 |  9874 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 |  9875 | `}` |
|         - |  9876 | `/*` |
|         - |  9877 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - |  9878 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - |  9879 | ` */` |
|        16 |  9880 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 |  9881 | `{` |
|         - |  9882 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 |  9883 | `	if( pMName->nByte > nPfx` |
|        12 |  9884 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 |  9885 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 |  9886 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 |  9887 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 |  9888 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 |  9889 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 |  9890 | `		return;` |
|         - |  9891 | `	}` |
|        20 |  9892 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 |  9893 | `}` |
|         - |  9894 | `/*` |
|         - |  9895 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - |  9896 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - |  9897 | ` */` |
|    364758 |  9898 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9899 | `{` |
|         - |  9900 | `	ph7_class_method *pMeth;` |
|         - |  9901 | `	SyHashEntry *pEntry;` |
|         - |  9902 | `	sxu32 nAbstract;` |
|         - |  9903 | `	SyBlob sMsg;` |
|         - |  9904 | `	sxi32 rc;` |
|         - |  9905 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    364763 |  9906 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15833 |  9907 | `		return SXRET_OK;` |
|         - |  9908 | `	}` |
|         - |  9909 | `	/* Count abstract methods */` |
|    348935 |  9910 | `	nAbstract = 0;` |
|    348935 |  9911 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   5150588 |  9912 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4627193 |  9913 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4627193 |  9914 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 |  9915 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 |  9916 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - |  9917 | `			}` |
|        20 |  9918 | `			nAbstract++;` |
|         8 |  9919 | `		}` |
|         5 |  9920 | `	}` |
|    348935 |  9921 | `	if( nAbstract == 0 ){` |
|    348921 |  9922 | `		return SXRET_OK;` |
|         - |  9923 | `	}` |
|         - |  9924 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 |  9925 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 |  9926 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - |  9927 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 |  9928 | `		&pClass->sName,nAbstract,` |
|         7 |  9929 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 |  9930 | `		(nAbstract > 1 ? "s" : ""));` |
|         - |  9931 | `	/* Second pass: list methods with origins */` |
|         - |  9932 | `	{` |
|        18 |  9933 | `		sxu32 nListed = 0;` |
|        18 |  9934 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 |  9935 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 |  9936 | `			ph7_class *pOrigin = 0;` |
|         - |  9937 | `			SyString *pMName;` |
|        22 |  9938 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 |  9939 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 |  9940 | `				continue;` |
|         - |  9941 | `			}` |
|        20 |  9942 | `			pMName = &pMeth->sFunc.sName;` |
|        20 |  9943 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 |  9944 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - |  9945 | `			}` |
|        20 |  9946 | `			if( nListed > 0 ){` |
|         3 |  9947 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 |  9948 | `			}` |
|         - |  9949 | `			/* Find the origin of this abstract method.` |
|         - |  9950 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - |  9951 | `			 * inheritance chains) take precedence for interface-declared` |
|         - |  9952 | `			 * methods. Abstract class methods only win when the class` |
|         - |  9953 | `			 * itself declared the abstract method (not inherited from` |
|         - |  9954 | `			 * an interface). Trait methods are adopted into the using` |
|         - |  9955 | `			 * class's namespace.` |
|         - |  9956 | `			 */` |
|         - |  9957 | `			{` |
|         - |  9958 | `				ph7_class **apIface;` |
|         - |  9959 | `				ph7_class **apTrait;` |
|         - |  9960 | `				ph7_class *pWalk;` |
|         - |  9961 | `				sxu32 i;` |
|         - |  9962 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - |  9963 | `				 * (one that was written in the class body, not inherited from an` |
|         - |  9964 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - |  9965 | `				 */` |
|        20 |  9966 | `				if( pClass->pBase ){` |
|        11 |  9967 | `					pWalk = pClass->pBase;` |
|        19 |  9968 | `					while( pWalk ){` |
|         - |  9969 | `						ph7_class_method *pParentMeth;` |
|        13 |  9970 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 |  9971 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - |  9972 | `							/* Exclude methods that came from an interface anywhere` |
|         - |  9973 | `							 * in this class's ancestor chain.` |
|         - |  9974 | `							 */` |
|        13 |  9975 | `							int fromIface = 0;` |
|        13 |  9976 | `							ph7_class *pAnc = pWalk;` |
|        17 |  9977 | `							while( pAnc ){` |
|         - |  9978 | `								ph7_class **apPI;` |
|         - |  9979 | `								sxu32 j;` |
|        15 |  9980 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 |  9981 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 |  9982 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 |  9983 | `										fromIface = 1;` |
|        10 |  9984 | `										break;` |
|         - |  9985 | `									}` |
|       ! 0 |  9986 | `								}` |
|        15 |  9987 | `								if( fromIface ) break;` |
|         6 |  9988 | `								pAnc = pAnc->pBase;` |
|         2 |  9989 | `							}` |
|        13 |  9990 | `							if( !fromIface ){` |
|         3 |  9991 | `								pOrigin = pWalk;` |
|         3 |  9992 | `								break;` |
|         - |  9993 | `							}` |
|         4 |  9994 | `						}` |
|        10 |  9995 | `						pWalk = pWalk->pBase;` |
|         2 |  9996 | `					}` |
|         4 |  9997 | `				}` |
|         - |  9998 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - |  9999 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10000 | `				 */` |
|        20 | 10001 | `				if( !pOrigin ){` |
|        18 | 10002 | `					pWalk = pClass;` |
|        40 | 10003 | `					while( pWalk && !pOrigin ){` |
|        26 | 10004 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10005 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10006 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10007 | `							ph7_class *pDeepest = 0;` |
|        28 | 10008 | `							while( pIface ){` |
|        16 | 10009 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10010 | `									pDeepest = pIface;` |
|         6 | 10011 | `								}` |
|        16 | 10012 | `								pIface = pIface->pBase;` |
|         4 | 10013 | `							}` |
|        16 | 10014 | `							if( pDeepest ){` |
|        16 | 10015 | `								pOrigin = pDeepest;` |
|        16 | 10016 | `								break;` |
|         - | 10017 | `							}` |
|       ! 0 | 10018 | `						}` |
|        26 | 10019 | `						pWalk = pWalk->pBase;` |
|         4 | 10020 | `					}` |
|         7 | 10021 | `				}` |
|         - | 10022 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10023 | `				if( !pOrigin ){` |
|         3 | 10024 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10025 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10026 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10027 | `							pOrigin = pClass;` |
|         3 | 10028 | `							break;` |
|         - | 10029 | `						}` |
|       ! 0 | 10030 | `					}` |
|         1 | 10031 | `				}` |
|         - | 10032 | `			}` |
|        20 | 10033 | `			if( pOrigin ){` |
|        20 | 10034 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10035 | `			}else{` |
|         - | 10036 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10037 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10038 | `			}` |
|        20 | 10039 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10040 | `			nListed++;` |
|         4 | 10041 | `		}` |
|         - | 10042 | `	}` |
|        18 | 10043 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10044 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10045 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10046 | `	SyBlobRelease(&sMsg);` |
|        18 | 10047 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10048 | `		return SXERR_ABORT;` |
|         - | 10049 | `	}` |
|        18 | 10050 | `	return SXRET_OK;` |
|    182384 | 10051 | `}` |
|         - | 10052 | `/*` |
|         - | 10053 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10054 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10055 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10056 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10057 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10058 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10059 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10060 | ` */` |
|    412384 | 10061 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10062 | `{` |
|    412389 | 10063 | `	int isAbsolute = 0;` |
|    412389 | 10064 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10065 | `	SyBlob sName;` |
|    412389 | 10066 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4535 | 10067 | `		isAbsolute = 1;` |
|      4535 | 10068 | `		pGen->pIn++;` |
|      2265 | 10069 | `	}` |
|    412389 | 10070 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         8 | 10071 | `		pGen->pIn = pStart;` |
|         8 | 10072 | `		return SXERR_INVALID;` |
|         - | 10073 | `	}` |
|    412383 | 10074 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    412383 | 10075 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    412383 | 10076 | `	pGen->pIn++;` |
|    618588 | 10077 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    206215 | 10078 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10079 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10080 | `		pGen->pIn++;` |
|        16 | 10081 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10082 | `		pGen->pIn++;` |
|         2 | 10083 | `	}` |
|    412383 | 10084 | `	if( isAbsolute ){` |
|      4533 | 10085 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2269 | 10086 | `	}else{` |
|         - | 10087 | `		SyString sRaw;` |
|    407855 | 10088 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    407855 | 10089 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10090 | `	}` |
|    412383 | 10091 | `	SyBlobRelease(&sName);` |
|    412383 | 10092 | `	return SXRET_OK;` |
|    206197 | 10093 | `}` |
|         - | 10094 | `/*` |
|         - | 10095 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10096 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10097 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10098 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10099 | ` * either direction cannot run unbounded.` |
|         - | 10100 | ` */` |
|         - | 10101 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    169878 | 10102 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10103 | `{` |
|         - | 10104 | `	ph7_class **apParent;` |
|         - | 10105 | `	sxu32 n;` |
|    442395 | 10106 | `	while( pInterface ){` |
|    280419 | 10107 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10108 | `			return FALSE;` |
|         - | 10109 | `		}` |
|    315952 | 10110 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     71066 | 10111 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7907 | 10112 | `			return TRUE;` |
|         - | 10113 | `		}` |
|    272517 | 10114 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    272517 | 10115 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10116 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10117 | `				return TRUE;` |
|         - | 10118 | `			}` |
|       ! 0 | 10119 | `		}` |
|    272517 | 10120 | `		pInterface = pInterface->pBase;` |
|    272517 | 10121 | `		iDepth++;` |
|         5 | 10122 | `	}` |
|    161981 | 10123 | `	return FALSE;` |
|     84944 | 10124 | `}` |
|    169878 | 10125 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10126 | `{` |
|    169883 | 10127 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10128 | `}` |
|         - | 10129 | `/*` |
|         - | 10130 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10131 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10132 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10133 | ` */` |
|      7902 | 10134 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10135 | `{` |
|      7911 | 10136 | `	while( pBase ){` |
|        10 | 10137 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10138 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10139 | `			return TRUE;` |
|         - | 10140 | `		}` |
|        10 | 10141 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10142 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10143 | `			return TRUE;` |
|         - | 10144 | `		}` |
|         5 | 10145 | `		pBase = pBase->pBase;` |
|         1 | 10146 | `	}` |
|      7903 | 10147 | `	return FALSE;` |
|      3956 | 10148 | `}` |
|         - | 10149 | `/*` |
|         - | 10150 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10151 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10152 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10153 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10154 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10155 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10156 | ` * pClass->aEnumCases for cases().` |
|         - | 10157 | ` */` |
|      7934 | 10158 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10159 | `{` |
|      7939 | 10160 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10161 | `	SySet *pInstrContainer;` |
|         - | 10162 | `	ph7_class_attr *pCase;` |
|         - | 10163 | `	SyString *pName;` |
|         - | 10164 | `	sxi32 rc;` |
|      7939 | 10165 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7939 | 10166 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10167 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10168 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10169 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10170 | `			return SXERR_ABORT;` |
|         - | 10171 | `		}` |
|       ! 0 | 10172 | `		goto Synchronize;` |
|         - | 10173 | `	}` |
|      7939 | 10174 | `	pName = &pGen->pIn->sData;` |
|         - | 10175 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7939 | 10176 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10177 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10178 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10179 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10180 | `			return SXERR_ABORT;` |
|         - | 10181 | `		}` |
|       ! 0 | 10182 | `		goto Synchronize;` |
|         - | 10183 | `	}` |
|      7939 | 10184 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10185 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7939 | 10186 | `	if( pCase == 0 ){` |
|       ! 0 | 10187 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10188 | `		return SXERR_ABORT;` |
|         - | 10189 | `	}` |
|      7939 | 10190 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7939 | 10191 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10192 | `		return SXERR_ABORT;` |
|         - | 10193 | `	}` |
|      7939 | 10194 | `	pGen->pIn++; /* Jump the case name */` |
|      7939 | 10195 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7925 | 10196 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10197 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10198 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10199 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10200 | `				return SXERR_ABORT;` |
|         - | 10201 | `			}` |
|         6 | 10202 | `			goto Synchronize;` |
|         - | 10203 | `		}` |
|      7921 | 10204 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10205 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10206 | `		 * (same technique as class constants). */` |
|      7921 | 10207 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7921 | 10208 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7921 | 10209 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7921 | 10210 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10211 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10212 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10213 | `		}` |
|      7921 | 10214 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7921 | 10215 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7921 | 10216 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10217 | `			return SXERR_ABORT;` |
|         - | 10218 | `		}` |
|      3963 | 10219 | `	}else{` |
|        17 | 10220 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10221 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10222 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10223 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10224 | `				return SXERR_ABORT;` |
|         - | 10225 | `			}` |
|       ! 0 | 10226 | `			goto Synchronize;` |
|         - | 10227 | `		}` |
|         - | 10228 | `	}` |
|      7935 | 10229 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7935 | 10230 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10231 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10232 | `		return SXERR_ABORT;` |
|         - | 10233 | `	}` |
|      7935 | 10234 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7935 | 10235 | `	return SXRET_OK;` |
|         2 | 10236 | `Synchronize:` |
|         - | 10237 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10238 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10239 | `		pGen->pIn++;` |
|         2 | 10240 | `	}` |
|         6 | 10241 | `	return SXERR_CORRUPT;` |
|      3972 | 10242 | `}` |
|         - | 10243 | `/*` |
|         - | 10244 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10245 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10246 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10247 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10248 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10249 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10250 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10251 | ` */` |
|      3970 | 10252 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10253 | `{` |
|         - | 10254 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10255 | `	const char *zBack;` |
|         - | 10256 | `	SySet sToken;` |
|         - | 10257 | `	char *zSrc;` |
|         - | 10258 | `	sxu32 nSrc,nMax;` |
|      3975 | 10259 | `	sxi32 rc = SXRET_OK;` |
|      3975 | 10260 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3970 | 10261 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3975 | 10262 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3975 | 10263 | `	if( zSrc == 0 ){` |
|       ! 0 | 10264 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10265 | `		return SXERR_ABORT;` |
|         - | 10266 | `	}` |
|      3975 | 10267 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3975 | 10268 | `	if( pClass->nEnumBacking != 0 ){` |
|      5942 | 10269 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10270 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10271 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10272 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1979 | 10273 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1984 | 10274 | `	}else{` |
|        21 | 10275 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10276 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10277 | `	}` |
|      3975 | 10278 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3975 | 10279 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3975 | 10280 | `	pSaveIn = pGen->pIn;` |
|      3975 | 10281 | `	pSaveEnd = pGen->pEnd;` |
|      3975 | 10282 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3975 | 10283 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15861 | 10284 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11891 | 10285 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10286 | `	}` |
|      3975 | 10287 | `	pGen->pIn = pSaveIn;` |
|      3975 | 10288 | `	pGen->pEnd = pSaveEnd;` |
|      3975 | 10289 | `	SySetRelease(&sToken);` |
|      3975 | 10290 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1990 | 10291 | `}` |
|         - | 10292 | `/*` |
|         - | 10293 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10294 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10295 | ` */` |
|         - | 10296 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10297 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10298 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10299 | `};` |
|         - | 10300 | `/*` |
|         - | 10301 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10302 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10303 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10304 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10305 | ` * and before the class is installed.` |
|         - | 10306 | ` */` |
|      3970 | 10307 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10308 | `{` |
|         - | 10309 | `	SyHashEntry *pEntry;` |
|         - | 10310 | `	sxi32 rc;` |
|         - | 10311 | `	sxu32 n;` |
|         - | 10312 | `	/* php: "Enum %s cannot include properties" */` |
|      3975 | 10313 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11909 | 10314 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7941 | 10315 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7941 | 10316 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10317 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10318 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10319 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10320 | `				return SXERR_ABORT;` |
|         - | 10321 | `			}` |
|         3 | 10322 | `			break;` |
|         - | 10323 | `		}` |
|         5 | 10324 | `	}` |
|         - | 10325 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     55585 | 10326 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     77415 | 10327 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     51615 | 10328 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10329 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10330 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10331 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10332 | `				return SXERR_ABORT;` |
|         - | 10333 | `			}` |
|       ! 0 | 10334 | `		}` |
|     25810 | 10335 | `	}` |
|         - | 10336 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10337 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10338 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10339 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10340 | `	{` |
|         - | 10341 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10342 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10343 | `		ph7_class_attr *pAttr;` |
|      3975 | 10344 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10345 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3975 | 10346 | `		if( pAttr == 0 ){` |
|       ! 0 | 10347 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10348 | `			return SXERR_ABORT;` |
|         - | 10349 | `		}` |
|      3975 | 10350 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3975 | 10351 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3975 | 10352 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3975 | 10353 | `		if( pClass->nEnumBacking != 0 ){` |
|      3963 | 10354 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10355 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3963 | 10356 | `			if( pAttr == 0 ){` |
|       ! 0 | 10357 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10358 | `				return SXERR_ABORT;` |
|         - | 10359 | `			}` |
|      3963 | 10360 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3963 | 10361 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10362 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10363 | `			}else{` |
|      3957 | 10364 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10365 | `			}` |
|      3963 | 10366 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1979 | 10367 | `		}` |
|         - | 10368 | `	}` |
|      3975 | 10369 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1990 | 10370 | `}` |
|         - | 10371 | `/*` |
|         - | 10372 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10373 | ` *` |
|         - | 10374 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10375 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10376 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10377 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10378 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10379 | ` * implements, body, install) is shared by both paths.` |
|         - | 10380 | ` */` |
|    364802 | 10381 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10382 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10383 | `{` |
|    364807 | 10384 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10385 | `	ph7_class *pClass,*pBase;` |
|         - | 10386 | `	SyToken *pEnd,*pTmp;` |
|         - | 10387 | `	sxi32 iProtection;` |
|         - | 10388 | `	SySet aInterfaces;` |
|         - | 10389 | `	SySet aUseEntries;` |
|         - | 10390 | `	sxi32 iAttrflags;` |
|         - | 10391 | `	SyString *pName;` |
|         - | 10392 | `	sxi32 nKwrd;` |
|         - | 10393 | `	sxi32 rc;` |
|         - | 10394 | `	/* Jump the 'class' keyword */` |
|    364807 | 10395 | `	pGen->pIn++;` |
|    364807 | 10396 | `	if( pAnonName ){` |
|         - | 10397 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10398 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10399 | `		 * then use the synthesized name. */` |
|        32 | 10400 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10401 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10402 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10403 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10404 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10405 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10406 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10407 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10408 | `		}` |
|        32 | 10409 | `		pName = pAnonName;` |
|        32 | 10410 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10411 | `	}else{` |
|    364779 | 10412 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10413 | `			/* Syntax error */` |
|       ! 0 | 10414 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10415 | `			if( rc == SXERR_ABORT ){` |
|         - | 10416 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10417 | `				return SXERR_ABORT;` |
|         - | 10418 | `			}` |
|         - | 10419 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10420 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10421 | `				pGen->pIn++;` |
|       ! 0 | 10422 | `			}` |
|       ! 0 | 10423 | `			return SXRET_OK;` |
|         - | 10424 | `		}` |
|         - | 10425 | `		/* Extract class name */` |
|    364779 | 10426 | `		pName = &pGen->pIn->sData;` |
|         - | 10427 | `		/* Advance the stream cursor */` |
|    364779 | 10428 | `		pGen->pIn++;` |
|         - | 10429 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10430 | `			SyBlob sFQN;` |
|         - | 10431 | `			SyString sFQNStr;` |
|    364779 | 10432 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    364779 | 10433 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    364779 | 10434 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    364779 | 10435 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    364779 | 10436 | `			SyBlobRelease(&sFQN);` |
|         - | 10437 | `		}` |
|         - | 10438 | `	}` |
|    364807 | 10439 | `	if( pClass == 0 ){` |
|       ! 0 | 10440 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10441 | `		return SXERR_ABORT;` |
|         - | 10442 | `	}` |
|    364802 | 10443 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3979 | 10444 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10445 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3965 | 10446 | `		pGen->pIn++; /* Jump ':' */` |
|      3960 | 10447 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3965 | 10448 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10449 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10450 | `			pGen->pIn++;` |
|      3958 | 10451 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3959 | 10452 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3957 | 10453 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3957 | 10454 | `			pGen->pIn++;` |
|      1981 | 10455 | `		}else{` |
|         3 | 10456 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10457 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10458 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10459 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10460 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10461 | `				return SXERR_ABORT;` |
|         - | 10462 | `			}` |
|         3 | 10463 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10464 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10465 | `			}` |
|         - | 10466 | `		}` |
|      1980 | 10467 | `	}` |
|    364807 | 10468 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    364807 | 10469 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10470 | `		return SXERR_ABORT;` |
|         - | 10471 | `	}` |
|         - | 10472 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    364807 | 10473 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    364807 | 10474 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10475 | `	/* Assume a standalone class */` |
|    364807 | 10476 | `	pBase = 0;` |
|    364807 | 10477 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    296385 | 10478 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    296385 | 10479 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10480 | `			SyBlob sResolved;` |
|         - | 10481 | `			SyString sBaseName;` |
|         - | 10482 | `			sxu32 nRefLine;` |
|    189667 | 10483 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10484 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10485 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10486 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10487 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10488 | `					return SXERR_ABORT;` |
|         - | 10489 | `				}` |
|       ! 0 | 10490 | `			}` |
|    189667 | 10491 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    189667 | 10492 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    189667 | 10493 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    189667 | 10494 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10495 | `				SyBlobRelease(&sResolved);` |
|         4 | 10496 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10497 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10498 | `					pName);` |
|         3 | 10499 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10500 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10501 | `					return SXERR_ABORT;` |
|         - | 10502 | `				}` |
|         3 | 10503 | `				return SXRET_OK;` |
|         - | 10504 | `			}` |
|    284495 | 10505 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    189660 | 10506 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    189665 | 10507 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10508 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10509 | `			/* Interfaces are not allowed */` |
|    189665 | 10510 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10511 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10512 | `			}` |
|    189665 | 10513 | `			if( pBase == 0 ){` |
|       ! 0 | 10514 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10515 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10516 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10517 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10518 | `					return SXERR_ABORT;` |
|         - | 10519 | `				}` |
|       ! 0 | 10520 | `			}else{` |
|    189665 | 10521 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10522 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10523 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10524 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10525 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10526 | `						return SXERR_ABORT;` |
|         - | 10527 | `					}` |
|         3 | 10528 | `					pBase = 0; /* Never inherit from an enum */` |
|    189664 | 10529 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10530 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10531 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10532 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10533 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10534 | `						return SXERR_ABORT;` |
|         - | 10535 | `					}` |
|       ! 0 | 10536 | `				}` |
|         - | 10537 | `			}` |
|    189665 | 10538 | `			SyBlobRelease(&sResolved);` |
|    189665 | 10539 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10540 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10541 | `			}` |
|     94830 | 10542 | `		}` |
|    296383 | 10543 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10544 | `			ph7_class *pInterface;` |
|         - | 10545 | `			/* Interface implementation */` |
|    110681 | 10546 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    114540 | 10547 | `			for(;;){` |
|         - | 10548 | `				SyBlob sResolved;` |
|         - | 10549 | `				SyString sIntName;` |
|         - | 10550 | `				sxu32 nRefLine;` |
|    169883 | 10551 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    169883 | 10552 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    169883 | 10553 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10554 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10555 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10556 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10557 | `						pName);` |
|       ! 0 | 10558 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10559 | `						return SXERR_ABORT;` |
|         - | 10560 | `					}` |
|       ! 0 | 10561 | `					break;` |
|         - | 10562 | `				}` |
|    339761 | 10563 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    169878 | 10564 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    169883 | 10565 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10566 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10567 | `				/* Only interfaces are allowed */` |
|    169883 | 10568 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10569 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10570 | `				}` |
|    169883 | 10571 | `				if( pInterface == 0 ){` |
|       ! 0 | 10572 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10573 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10574 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10575 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10576 | `						return SXERR_ABORT;` |
|         - | 10577 | `					}` |
|       ! 0 | 10578 | `				}else{` |
|         - | 10579 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10580 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10581 | `					 * unless they already extend Exception or Error.` |
|         - | 10582 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10583 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10584 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    169883 | 10585 | `					SyString *pFqn = &pClass->sName;` |
|    169883 | 10586 | `					int bIsExceptionOrError =` |
|     88889 | 10587 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    256794 | 10588 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    167912 | 10589 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3960 | 10590 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    173829 | 10591 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11856 | 10592 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3949 | 10593 | `						!bIsExceptionOrError ){` |
|        12 | 10594 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10595 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10596 | `							&pClass->sName);` |
|         9 | 10597 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10598 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10599 | `							return SXERR_ABORT;` |
|         - | 10600 | `						}` |
|         - | 10601 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10602 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10603 | `					}else{` |
|    169877 | 10604 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10605 | `					}` |
|         - | 10606 | `				}` |
|    169883 | 10607 | `				SyBlobRelease(&sResolved);` |
|    169883 | 10608 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     55343 | 10609 | `					break;` |
|         - | 10610 | `				}` |
|     59207 | 10611 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10612 | `			}` |
|     55338 | 10613 | `		}` |
|    148189 | 10614 | `	}` |
|    364805 | 10615 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10616 | `		/* Syntax error */` |
|       ! 0 | 10617 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10618 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10619 | `		if( rc == SXERR_ABORT ){` |
|         - | 10620 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10621 | `			return SXERR_ABORT;` |
|         - | 10622 | `		}` |
|       ! 0 | 10623 | `		return SXRET_OK;` |
|         - | 10624 | `	}` |
|    364805 | 10625 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    364805 | 10626 | `	pEnd = 0; /* cc warning */` |
|         - | 10627 | `	/* Delimit the class body */` |
|    364805 | 10628 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    364805 | 10629 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10630 | `		/* Syntax error */` |
|       ! 0 | 10631 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10632 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10633 | `		if( rc == SXERR_ABORT ){` |
|         - | 10634 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10635 | `			return SXERR_ABORT;` |
|         - | 10636 | `		}` |
|       ! 0 | 10637 | `		return SXRET_OK;` |
|         - | 10638 | `	}` |
|         - | 10639 | `	/* The delimiter token is the class body's closing brace */` |
|    364805 | 10640 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10641 | `	/* Swap token stream */` |
|    364805 | 10642 | `	pTmp = pGen->pEnd;` |
|    364805 | 10643 | `	pGen->pEnd = pEnd;` |
|         - | 10644 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    364805 | 10645 | `	pClass->iFlags \|= iFlags;` |
|         - | 10646 | `	/* Start the parse process */` |
|   1412365 | 10647 | `	for(;;){` |
|         - | 10648 | `		/* Jump leading/trailing semi-colons */` |
|   4026471 | 10649 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    727175 | 10650 | `			pGen->pIn++;` |
|         5 | 10651 | `		}` |
|   3299301 | 10652 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10653 | `			/* End of class body */` |
|    364763 | 10654 | `			break;` |
|         - | 10655 | `		}` |
|         - | 10656 | `		/* Bind a directly-preceding docblock to this member */` |
|   2934543 | 10657 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2934538 | 10658 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1467274 | 10659 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10660 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10661 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10662 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10663 | `			if( rc == SXERR_ABORT ){` |
|         - | 10664 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10665 | `				return SXERR_ABORT;` |
|         - | 10666 | `			}` |
|       ! 0 | 10667 | `			goto done;` |
|         - | 10668 | `		}` |
|         - | 10669 | `		/* Assume public visibility */` |
|   2934543 | 10670 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2934543 | 10671 | `		iAttrflags = 0;` |
|         - | 10672 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10673 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10674 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10675 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2934543 | 10676 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10677 | `			int bMod = 0;` |
|       ! 0 | 10678 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10679 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10680 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10681 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10682 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10683 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10684 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10685 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10686 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10687 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10688 | `			}` |
|       ! 0 | 10689 | `			if( !bMod ){` |
|       ! 0 | 10690 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10691 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10692 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10693 | `						return SXERR_ABORT;` |
|         - | 10694 | `					}` |
|       ! 0 | 10695 | `					goto done;` |
|         - | 10696 | `				}` |
|       ! 0 | 10697 | `				continue;` |
|         - | 10698 | `			}` |
|       ! 0 | 10699 | `		}` |
|   2934543 | 10700 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10701 | `			/* Extract the current keyword */` |
|   2934543 | 10702 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2934543 | 10703 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10704 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7939 | 10705 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7939 | 10706 | `				if( rc != SXRET_OK ){` |
|         6 | 10707 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10708 | `						return SXERR_ABORT;` |
|         - | 10709 | `					}` |
|         6 | 10710 | `					goto done;` |
|         - | 10711 | `				}` |
|      7935 | 10712 | `				continue;` |
|         - | 10713 | `			}` |
|   2926609 | 10714 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 10715 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 10716 | `				TraitUseEntry sUse;` |
|     15853 | 10717 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15853 | 10718 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15853 | 10719 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7932 | 10720 | `				for(;;){` |
|         - | 10721 | `					ph7_class *pTrait;` |
|         - | 10722 | `					SyString *pTraitName;` |
|     15861 | 10723 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 10724 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10725 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 10726 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10727 | `							return SXERR_ABORT;` |
|         - | 10728 | `						}` |
|       ! 0 | 10729 | `						break;` |
|         - | 10730 | `					}` |
|     15861 | 10731 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 10732 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 10733 | `						SyBlob sResolved;` |
|     15861 | 10734 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15861 | 10735 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     31717 | 10736 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15856 | 10737 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15861 | 10738 | `						SyBlobRelease(&sResolved);` |
|         - | 10739 | `					}` |
|         - | 10740 | `					/* Only traits are allowed */` |
|     15861 | 10741 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 10742 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 10743 | `					}` |
|     15861 | 10744 | `					if( pTrait == 0 ){` |
|       ! 0 | 10745 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10746 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 10747 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10748 | `							return SXERR_ABORT;` |
|         - | 10749 | `						}` |
|       ! 0 | 10750 | `					}else{` |
|     15861 | 10751 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 10752 | `					}` |
|     15861 | 10753 | `					pGen->pIn++; /* Advance past trait name */` |
|     15861 | 10754 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7929 | 10755 | `						break;` |
|         - | 10756 | `					}` |
|        10 | 10757 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 10758 | `				}` |
|         - | 10759 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15853 | 10760 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 10761 | `					SyToken *pBlock;` |
|        13 | 10762 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 10763 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 10764 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 10765 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 10766 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 10767 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 10768 | `					}else{` |
|       ! 0 | 10769 | `						pGen->pIn = pGen->pEnd;` |
|         - | 10770 | `					}` |
|         5 | 10771 | `				}` |
|     15853 | 10772 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 10773 | `				/* The semicolon will be consumed by the outer loop */` |
|     15853 | 10774 | `				continue;` |
|         - | 10775 | `			}` |
|   2910761 | 10776 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 10777 | `				int nSetTok;` |
|   2657725 | 10778 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2657725 | 10779 | `				if( nSetVis ){` |
|         - | 10780 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 10781 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 10782 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10783 | `					pGen->pIn += nSetTok;` |
|         2 | 10784 | `				}else{` |
|   2657723 | 10785 | `					iProtection = nKwrd;` |
|   2657723 | 10786 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 10787 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 10788 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2657723 | 10789 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2657723 | 10790 | `					if( nSetVis ){` |
|         9 | 10791 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 10792 | `						pGen->pIn += nSetTok;` |
|         4 | 10793 | `					}` |
|         - | 10794 | `				}` |
|         - | 10795 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 10796 | ``				 * `public private(set) readonly int $x`. */`` |
|   2657725 | 10797 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 10798 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 10799 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 10800 | `				}` |
|   2657720 | 10801 | `				if( pGen->pIn >= pGen->pEnd` |
|   2657725 | 10802 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10803 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10804 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10805 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 10806 | `					if( rc == SXERR_ABORT ){` |
|         - | 10807 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 10808 | `						return SXERR_ABORT;` |
|         - | 10809 | `					}` |
|       ! 0 | 10810 | `					goto done;` |
|         - | 10811 | `				}` |
|   2657725 | 10812 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10813 | `					/* Attribute declaration (untyped) */` |
|    422835 | 10814 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    422835 | 10815 | `					if( rc != SXRET_OK ){` |
|        11 | 10816 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10817 | `							return SXERR_ABORT;` |
|         - | 10818 | `						}` |
|        11 | 10819 | `						goto done;` |
|         - | 10820 | `					}` |
|    422971 | 10821 | `					continue;` |
|         - | 10822 | `				}` |
|   2234895 | 10823 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10824 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       299 | 10825 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       299 | 10826 | `					if( rc != SXRET_OK ){` |
|         8 | 10827 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10828 | `							return SXERR_ABORT;` |
|         - | 10829 | `						}` |
|         8 | 10830 | `						goto done;` |
|         - | 10831 | `					}` |
|       293 | 10832 | `					continue;` |
|         - | 10833 | `				}` |
|         - | 10834 | `				/* Extract the keyword */` |
|   2234601 | 10835 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1117298 | 10836 | `			}` |
|   2487637 | 10837 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 10838 | `				/* Process constant declaration */` |
|    244807 | 10839 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    244807 | 10840 | `				if( rc != SXRET_OK ){` |
|        11 | 10841 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10842 | `						return SXERR_ABORT;` |
|         - | 10843 | `					}` |
|        11 | 10844 | `					goto done;` |
|         - | 10845 | `				}` |
|    122402 | 10846 | `			}else{` |
|   2242835 | 10847 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 10848 | `					/* Static method or attribute,record that */` |
|     98795 | 10849 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     98795 | 10850 | `					pGen->pIn++; /* Jump the static keyword */` |
|     98795 | 10851 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10852 | `						int nSetTok;` |
|     71143 | 10853 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     71143 | 10854 | `						if( nSetVis ){` |
|         - | 10855 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 10856 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10857 | `							pGen->pIn += nSetTok;` |
|         2 | 10858 | `						}else{` |
|         - | 10859 | `							/* Extract the keyword */` |
|     71141 | 10860 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     71141 | 10861 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 10862 | `								iProtection = nKwrd;` |
|       ! 0 | 10863 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 10864 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 10865 | `								if( nSetVis ){` |
|       ! 0 | 10866 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 10867 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 10868 | `								}` |
|       ! 0 | 10869 | `							}` |
|         - | 10870 | `						}` |
|     35569 | 10871 | `					}` |
|         - | 10872 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 10873 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 10874 | `					 * than a generic "expecting method" parse error. */` |
|     98795 | 10875 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10876 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10877 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 10878 | `					}` |
|     98790 | 10879 | `					if( pGen->pIn >= pGen->pEnd` |
|     98795 | 10880 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10881 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10882 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 10883 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 10884 | `						if( rc == SXERR_ABORT ){` |
|         - | 10885 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 10886 | `							return SXERR_ABORT;` |
|         - | 10887 | `						}` |
|       ! 0 | 10888 | `						goto done;` |
|         - | 10889 | `					}` |
|     98795 | 10890 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10891 | `						/* Attribute declaration */` |
|     27655 | 10892 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     27655 | 10893 | `						if( rc != SXRET_OK ){` |
|         3 | 10894 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10895 | `								return SXERR_ABORT;` |
|         - | 10896 | `							}` |
|         3 | 10897 | `							goto done;` |
|         - | 10898 | `						}` |
|     27653 | 10899 | `						continue;` |
|         - | 10900 | `					}` |
|     71145 | 10901 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10902 | `						/* Typed static attribute declaration */` |
|        17 | 10903 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        17 | 10904 | `						if( rc != SXRET_OK ){` |
|         3 | 10905 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10906 | `								return SXERR_ABORT;` |
|         - | 10907 | `							}` |
|         3 | 10908 | `							goto done;` |
|         - | 10909 | `						}` |
|        15 | 10910 | `						continue;` |
|         - | 10911 | `					}` |
|         - | 10912 | `					/* Extract the keyword */` |
|     71131 | 10913 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2179608 | 10914 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 10915 | `					/* Abstract method,record that */` |
|      7915 | 10916 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 10917 | `					/* Mark the whole class as abstract */` |
|      7915 | 10918 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 10919 | `					/* Advance the stream cursor */` |
|      7915 | 10920 | `					pGen->pIn++;` |
|      7915 | 10921 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7915 | 10922 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7915 | 10923 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7913 | 10924 | `							iProtection = nKwrd;` |
|      7913 | 10925 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3954 | 10926 | `						}` |
|      3955 | 10927 | `					}` |
|      7915 | 10928 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7910 | 10929 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 10930 | `							/* Static method */` |
|       ! 0 | 10931 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 10932 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 10933 | `					}` |
|      7915 | 10934 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7910 | 10935 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 10936 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 10937 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 10938 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 10939 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 10940 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 10941 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 10942 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 10943 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 10944 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 10945 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 10946 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 10947 | `										return SXERR_ABORT;` |
|         - | 10948 | `									}` |
|       ! 0 | 10949 | `									goto done;` |
|         - | 10950 | `								}` |
|         7 | 10951 | `								continue;` |
|         - | 10952 | `							}` |
|       ! 0 | 10953 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10954 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 10955 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 10956 | `							if( rc == SXERR_ABORT ){` |
|         - | 10957 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 10958 | `								return SXERR_ABORT;` |
|         - | 10959 | `							}` |
|       ! 0 | 10960 | `							goto done;` |
|         - | 10961 | `					}` |
|      7909 | 10962 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2140087 | 10963 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 10964 | `					/* final method ,record that */` |
|        21 | 10965 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        21 | 10966 | `					pGen->pIn++; /* Jump the final keyword */` |
|        21 | 10967 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10968 | `						/* Extract the keyword */` |
|        21 | 10969 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        21 | 10970 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        11 | 10971 | `							iProtection = nKwrd;` |
|        11 | 10972 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 10973 | `						}` |
|         9 | 10974 | `					}` |
|        21 | 10975 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 10976 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 10977 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 10978 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 10979 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 10980 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 10981 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 10982 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 10983 | `									return SXERR_ABORT;` |
|         - | 10984 | `								}` |
|       ! 0 | 10985 | `								goto done;` |
|         - | 10986 | `							}` |
|        14 | 10987 | `							continue;` |
|         - | 10988 | `					}` |
|         9 | 10989 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 10990 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 10991 | `							/* Static method */` |
|       ! 0 | 10992 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 10993 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 10994 | `					}` |
|         9 | 10995 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 10996 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 10997 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10998 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 10999 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11000 | `							if( rc == SXERR_ABORT ){` |
|         - | 11001 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11002 | `								return SXERR_ABORT;` |
|         - | 11003 | `							}` |
|       ! 0 | 11004 | `							goto done;` |
|         - | 11005 | `					}` |
|         9 | 11006 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11007 | `				}` |
|   2215153 | 11008 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11009 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11010 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11011 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11012 | `						if( rc == SXERR_ABORT ){` |
|         - | 11013 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11014 | `							return SXERR_ABORT;` |
|         - | 11015 | `						}` |
|       ! 0 | 11016 | `						goto done;` |
|         - | 11017 | `				}` |
|   2215153 | 11018 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11019 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11020 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11021 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11022 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11023 | `						if( rc == SXERR_ABORT ){` |
|         - | 11024 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11025 | `							return SXERR_ABORT;` |
|         - | 11026 | `						}` |
|       ! 0 | 11027 | `						goto done;` |
|         - | 11028 | `					}` |
|         - | 11029 | `					/* Attribute declaration */` |
|         7 | 11030 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11031 | `				}else{` |
|         - | 11032 | `					/* Process method declaration */` |
|   2215147 | 11033 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11034 | `				}` |
|   2215153 | 11035 | `				if( rc != SXRET_OK ){` |
|        16 | 11036 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11037 | `						return SXERR_ABORT;` |
|         - | 11038 | `					}` |
|        16 | 11039 | `					goto done;` |
|         - | 11040 | `				}` |
|         - | 11041 | `			}` |
|   1229970 | 11042 | `		}else{` |
|         - | 11043 | `			/* Attribute declaration */` |
|       ! 0 | 11044 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11045 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11046 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11047 | `					return SXERR_ABORT;` |
|         - | 11048 | `				}` |
|       ! 0 | 11049 | `				goto done;` |
|         - | 11050 | `			}` |
|         - | 11051 | `		}` |
|         5 | 11052 | `	}` |
|         - | 11053 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11054 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11055 | `	 */` |
|         - | 11056 | `	{` |
|         - | 11057 | `		TraitUseEntry *apUse;` |
|         - | 11058 | `		sxu32 nU;` |
|    364763 | 11059 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    380611 | 11060 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15853 | 11061 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15853 | 11062 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15853 | 11063 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15853 | 11064 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11065 | `			sxu32 nT;` |
|     15853 | 11066 | `			if( !hasResolution ){` |
|         - | 11067 | `				/* No conflict resolution block: use standard trait application */` |
|     31687 | 11068 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15849 | 11069 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15849 | 11070 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11071 | `						break;` |
|         - | 11072 | `					}` |
|      7927 | 11073 | `				}` |
|      7924 | 11074 | `			}else{` |
|         - | 11075 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11076 | `				 * then use the block to resolve method conflicts.` |
|         - | 11077 | `				 */` |
|         - | 11078 | `				SyToken *pR;` |
|        25 | 11079 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11080 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11081 | `					ph7_class_attr *pAR;` |
|         - | 11082 | `					SyHashEntry *pER;` |
|         - | 11083 | `					SyString *pNR;` |
|        15 | 11084 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11085 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11086 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11087 | `						pNR = &pAR->sName;` |
|       ! 0 | 11088 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11089 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11090 | `						}` |
|       ! 0 | 11091 | `					}` |
|        15 | 11092 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11093 | `				}` |
|         - | 11094 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11095 | `				pR = pUse->pResolvStart;` |
|        27 | 11096 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11097 | `					SyString sTrait,sMethod;` |
|         - | 11098 | `					ph7_class *pSrcTrait;` |
|         - | 11099 | `					ph7_class_method *pMeth;` |
|         - | 11100 | `					sxi32 nRKwrd;` |
|        41 | 11101 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11102 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11103 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11104 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11105 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11106 | `					sMethod = pR->sData;` |
|        17 | 11107 | `					pR++;` |
|        17 | 11108 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11109 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11110 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11111 | `							sTrait = sMethod;` |
|         7 | 11112 | `							pR++;` |
|         7 | 11113 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11114 | `							sMethod = pR->sData;` |
|         7 | 11115 | `							pR++;` |
|         3 | 11116 | `						}` |
|         3 | 11117 | `					}` |
|        17 | 11118 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11119 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11120 | `						continue;` |
|         - | 11121 | `					}` |
|        17 | 11122 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11123 | `					pR++;` |
|        17 | 11124 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11125 | `						pSrcTrait = 0;` |
|         7 | 11126 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11127 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11128 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11129 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11130 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11131 | `								break;` |
|         - | 11132 | `							}` |
|         2 | 11133 | `						}` |
|         5 | 11134 | `						if( pSrcTrait ){` |
|         5 | 11135 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11136 | `							if( pMeth ){` |
|         5 | 11137 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11138 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11139 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11140 | `								}` |
|         2 | 11141 | `							}` |
|         2 | 11142 | `						}` |
|         2 | 11143 | `					}` |
|        35 | 11144 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11145 | `				}` |
|         - | 11146 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11147 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11148 | `					ph7_class_method *pMR;` |
|         - | 11149 | `					SyHashEntry *pER;` |
|         - | 11150 | `					SyString *pNR;` |
|        15 | 11151 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11152 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11153 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11154 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11155 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11156 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11157 | `						}` |
|         3 | 11158 | `					}` |
|         9 | 11159 | `				}` |
|         - | 11160 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11161 | `				pR = pUse->pResolvStart;` |
|        27 | 11162 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11163 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11164 | `					ph7_class *pSrcTrait;` |
|         - | 11165 | `					ph7_class_method *pMeth;` |
|        27 | 11166 | `					int hasQual = 0;` |
|         - | 11167 | `					sxi32 nRKwrd;` |
|        41 | 11168 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11169 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11170 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11171 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11172 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11173 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11174 | `					sMethod = pR->sData;` |
|        17 | 11175 | `					pR++;` |
|        17 | 11176 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11177 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11178 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11179 | `							sTrait = sMethod;` |
|         7 | 11180 | `							hasQual = 1;` |
|         7 | 11181 | `							pR++;` |
|         7 | 11182 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11183 | `							sMethod = pR->sData;` |
|         7 | 11184 | `							pR++;` |
|         3 | 11185 | `						}` |
|         3 | 11186 | `					}` |
|        17 | 11187 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11188 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11189 | `						continue;` |
|         - | 11190 | `					}` |
|        17 | 11191 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11192 | `					pR++;` |
|        17 | 11193 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11194 | `						sxi32 iNewVis = -1;` |
|        13 | 11195 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11196 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11197 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11198 | `								iNewVis = nAK;` |
|         7 | 11199 | `								pR++;` |
|         3 | 11200 | `							}` |
|         3 | 11201 | `						}` |
|        13 | 11202 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11203 | `							sAlias = pR->sData;` |
|        11 | 11204 | `							pR++;` |
|         4 | 11205 | `						}` |
|        13 | 11206 | `						pMeth = 0;` |
|        13 | 11207 | `						if( hasQual ){` |
|         3 | 11208 | `							pSrcTrait = 0;` |
|         5 | 11209 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11210 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11211 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11212 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11213 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11214 | `									break;` |
|         - | 11215 | `								}` |
|         2 | 11216 | `							}` |
|         3 | 11217 | `							if( pSrcTrait ){` |
|         3 | 11218 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11219 | `							}` |
|         2 | 11220 | `						}else{` |
|        10 | 11221 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11222 | `						}` |
|        13 | 11223 | `						if( pMeth ){` |
|        13 | 11224 | `							if( sAlias.nByte > 0 ){` |
|         - | 11225 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11226 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11227 | `								 */` |
|         - | 11228 | `								ph7_class_method *pAlias;` |
|         - | 11229 | `								char *zAliasDup;` |
|        11 | 11230 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11231 | `								if( pAlias ){` |
|        11 | 11232 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11233 | `									if( iNewVis >= 0 ){` |
|         5 | 11234 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11235 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11236 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11237 | `									}` |
|        11 | 11238 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11239 | `									if( zAliasDup ){` |
|        11 | 11240 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11241 | `									}` |
|         7 | 11242 | `								}` |
|         7 | 11243 | `							}else if( iNewVis >= 0 ){` |
|         - | 11244 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11245 | `								ph7_class_method *pCopy;` |
|         3 | 11246 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11247 | `								if( pCopy ){` |
|         3 | 11248 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11249 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11250 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11251 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11252 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11253 | `									/* Replace the method in the class hash */` |
|         3 | 11254 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11255 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11256 | `								}` |
|         1 | 11257 | `							}` |
|         5 | 11258 | `						}` |
|         5 | 11259 | `						SXUNUSED(hasQual);` |
|         5 | 11260 | `					}` |
|        21 | 11261 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11262 | `				}` |
|         - | 11263 | `			}` |
|     15853 | 11264 | `			SySetRelease(&pUse->aTraits);` |
|      7929 | 11265 | `		}` |
|         - | 11266 | `	}` |
|    364763 | 11267 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11268 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11269 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3975 | 11270 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3975 | 11271 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11272 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11273 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11274 | `			return SXERR_ABORT;` |
|         - | 11275 | `		}` |
|      1985 | 11276 | `	}` |
|         - | 11277 | `	/* Install the class */` |
|    364763 | 11278 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    364763 | 11279 | `	if( rc == SXRET_OK ){` |
|         - | 11280 | `		ph7_class **apInterface;` |
|         - | 11281 | `		sxu32 n;` |
|    364763 | 11282 | `		if( pBase ){` |
|         - | 11283 | `			/* Inherit from base class and mark as a subclass */` |
|    189663 | 11284 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     94829 | 11285 | `		}` |
|    364763 | 11286 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    534635 | 11287 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11288 | `			/* Implements one or more interface */` |
|    169877 | 11289 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    169877 | 11290 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11291 | `				break;` |
|         - | 11292 | `			}` |
|     84941 | 11293 | `		}` |
|         - | 11294 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11295 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    364763 | 11296 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3975 | 11297 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3975 | 11298 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11299 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11300 | `			}` |
|      3975 | 11301 | `			if( pIntf ){` |
|      3975 | 11302 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1985 | 11303 | `			}` |
|      3975 | 11304 | `			if( pClass->nEnumBacking != 0 ){` |
|      3963 | 11305 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3963 | 11306 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11307 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11308 | `				}` |
|      3963 | 11309 | `				if( pIntf ){` |
|      3963 | 11310 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1979 | 11311 | `				}` |
|      1979 | 11312 | `			}` |
|      1985 | 11313 | `		}` |
|         - | 11314 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11315 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    364758 | 11316 | `		if( rc == SXRET_OK` |
|    364758 | 11317 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    364763 | 11318 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    193463 | 11319 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11320 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    193463 | 11321 | `			if( pStringable ){` |
|    193463 | 11322 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    193463 | 11323 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11324 | `				sxu32 i;` |
|    193463 | 11325 | `				int bAlready = 0;` |
|    232927 | 11326 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     43417 | 11327 | `					if( apImpl[i] == pStringable ){` |
|      3953 | 11328 | `						bAlready = 1;` |
|      3953 | 11329 | `						break;` |
|         - | 11330 | `					}` |
|     19737 | 11331 | `				}` |
|    193463 | 11332 | `				if( !bAlready ){` |
|    189515 | 11333 | `					PH7_ClassImplement(pClass,pStringable);` |
|     94755 | 11334 | `				}` |
|     96729 | 11335 | `			}` |
|     96729 | 11336 | `		}` |
|         - | 11337 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    364763 | 11338 | `		if( rc == SXRET_OK ){` |
|    364763 | 11339 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    364763 | 11340 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11341 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11342 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11343 | `				return SXERR_ABORT;` |
|         - | 11344 | `			}` |
|    182379 | 11345 | `		}` |
|         - | 11346 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    364763 | 11347 | `		if( rc == SXRET_OK ){` |
|    364763 | 11348 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    364763 | 11349 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11350 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11351 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11352 | `				return SXERR_ABORT;` |
|         - | 11353 | `			}` |
|    182379 | 11354 | `		}` |
|    182379 | 11355 | `	}` |
|    364763 | 11356 | `	SySetRelease(&aUseEntries);` |
|    364763 | 11357 | `	SySetRelease(&aInterfaces);` |
|    364763 | 11358 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11359 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11360 | `		return SXERR_ABORT;` |
|         - | 11361 | `	}` |
|    182379 | 11362 | `done:` |
|         - | 11363 | `	/* Point beyond the class body */` |
|    364805 | 11364 | `	pGen->pIn = &pEnd[1];` |
|    364805 | 11365 | `	pGen->pEnd = pTmp;` |
|    364805 | 11366 | `	return PH7_OK;` |
|    182406 | 11367 | `}` |
|         - | 11368 | `/* Compile a named class declaration (the common case). */` |
|    364774 | 11369 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11370 | `{` |
|    364779 | 11371 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11372 | `}` |
|         - | 11373 | `/*` |
|         - | 11374 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11375 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11376 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11377 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11378 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11379 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11380 | ` */` |
|        28 | 11381 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11382 | `{` |
|         - | 11383 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11384 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11385 | `	SyString sName;` |
|         - | 11386 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11387 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11388 | `	                              * is keyed to this 'class' token */` |
|         - | 11389 | `	ph7_value *pObj;` |
|        32 | 11390 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11391 | `	sxu32 nIdx,nLen;` |
|         - | 11392 | `	sxi32 nArg,rc;` |
|        14 | 11393 | `	SXUNUSED(iCompileFlag);` |
|         - | 11394 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11395 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11396 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11397 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11398 | `	}` |
|        32 | 11399 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11400 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11401 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11402 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11403 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11404 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11405 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11406 | `		return rc;` |
|         - | 11407 | `	}` |
|         - | 11408 | `	{` |
|         - | 11409 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11410 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11411 | `		if( pAnonClass` |
|        32 | 11412 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11413 | `			return SXERR_ABORT;` |
|         - | 11414 | `		}` |
|         - | 11415 | `	}` |
|         - | 11416 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11417 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11418 | `	nArg = 0;` |
|        32 | 11419 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11420 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11421 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11422 | `		SyToken *pArgNext;` |
|         7 | 11423 | `		pGen->pIn = pArgStart;` |
|         7 | 11424 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11425 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11426 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11427 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11428 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11429 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11430 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11431 | `					return SXERR_ABORT;` |
|         - | 11432 | `				}` |
|         7 | 11433 | `				nArg++;` |
|         3 | 11434 | `			}` |
|         7 | 11435 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11436 | `		}` |
|         7 | 11437 | `		pGen->pIn = pSavedIn;` |
|         7 | 11438 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11439 | `	}` |
|         - | 11440 | `	/* Load the synthesized class name */` |
|        32 | 11441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11442 | `	if( pObj == 0 ){` |
|       ! 0 | 11443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11444 | `		return SXERR_ABORT;` |
|         - | 11445 | `	}` |
|        32 | 11446 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11447 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11448 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11449 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11450 | `	return SXRET_OK;` |
|        18 | 11451 | `}` |
|         - | 11452 | `/*` |
|         - | 11453 | ` * Compile a user-defined abstract class.` |
|         - | 11454 | ` *  According to the PHP language reference manual` |
|         - | 11455 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11456 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11457 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11458 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11459 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11460 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11461 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11462 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11463 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11464 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11465 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11466 | ` *   could differ.` |
|         - | 11467 | ` */` |
|         - | 11468 | `/*` |
|         - | 11469 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11470 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11471 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11472 | ` */` |
|  11257402 | 11473 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11474 | `{` |
|  11257407 | 11475 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   6650717 | 11476 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   6650717 | 11477 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   6603339 | 11478 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3285840 | 11479 | `	}` |
|  11178375 | 11480 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  11178315 | 11481 | `	return FALSE;` |
|   5628706 | 11482 | `}` |
|         - | 11483 | `/*` |
|         - | 11484 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11485 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11486 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11487 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11488 | ` */` |
|  11178310 | 11489 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11490 | `{` |
|  11178315 | 11491 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  11178315 | 11492 | `	sxi32 iFlags = 0,iFlag;` |
|  11257407 | 11493 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     79097 | 11494 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11495 | `			pDup = pIn;` |
|         2 | 11496 | `		}` |
|     79097 | 11497 | `		iFlags \|= iFlag;` |
|     79097 | 11498 | `		pIn++;` |
|         5 | 11499 | `	}` |
|  11178315 | 11500 | `	*ppIn = pIn;` |
|  11178315 | 11501 | `	if( ppDup ){ *ppDup = pDup; }` |
|  11178315 | 11502 | `	return iFlags;` |
|         5 | 11503 | `}` |
|         - | 11504 | `/*` |
|         - | 11505 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11506 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11507 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11508 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11509 | `` * `readonly`) to their existing handlers.`` |
|         - | 11510 | ` */` |
|  11142720 | 11511 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11512 | `{` |
|  11142725 | 11513 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   5614849 | 11514 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  11164463 | 11515 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11516 | `}` |
|         - | 11517 | `/*` |
|         - | 11518 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11519 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11520 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11521 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11522 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11523 | ` */` |
|     35590 | 11524 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11525 | `{` |
|         - | 11526 | `	SyToken *pDup;` |
|     35595 | 11527 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11528 | `	sxi32 rc;` |
|     35595 | 11529 | `	if( pDup ){` |
|         4 | 11530 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11531 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11532 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11533 | `			return SXERR_ABORT;` |
|         - | 11534 | `		}` |
|         1 | 11535 | `	}` |
|     35590 | 11536 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17800 | 11537 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11538 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11539 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11540 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11541 | `			return SXERR_ABORT;` |
|         - | 11542 | `		}` |
|         1 | 11543 | `	}` |
|     35595 | 11544 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17800 | 11545 | `}` |
|         - | 11546 | `/*` |
|         - | 11547 | ` * Compile a user-defined trait.` |
|         - | 11548 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11549 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11550 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11551 | ` */` |
|      7970 | 11552 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11553 | `{` |
|      7975 | 11554 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11555 | `	ph7_class *pClass;` |
|         - | 11556 | `	SyToken *pEnd,*pTmp;` |
|         - | 11557 | `	sxi32 iProtection;` |
|         - | 11558 | `	sxi32 iAttrflags;` |
|         - | 11559 | `	SyString *pName;` |
|         - | 11560 | `	sxi32 nKwrd;` |
|         - | 11561 | `	sxi32 rc;` |
|         - | 11562 | `	/* Jump the 'trait' keyword */` |
|      7975 | 11563 | `	pGen->pIn++;` |
|      7975 | 11564 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11565 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11566 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11567 | `			return SXERR_ABORT;` |
|         - | 11568 | `		}` |
|       ! 0 | 11569 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11570 | `			pGen->pIn++;` |
|       ! 0 | 11571 | `		}` |
|       ! 0 | 11572 | `		return SXRET_OK;` |
|         - | 11573 | `	}` |
|         - | 11574 | `	/* Extract trait name */` |
|      7975 | 11575 | `	pName = &pGen->pIn->sData;` |
|      7975 | 11576 | `	pGen->pIn++;` |
|         - | 11577 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11578 | `		SyBlob sFQN;` |
|         - | 11579 | `		SyString sFQNStr;` |
|      7975 | 11580 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7975 | 11581 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7975 | 11582 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7975 | 11583 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7975 | 11584 | `		SyBlobRelease(&sFQN);` |
|         - | 11585 | `	}` |
|      7975 | 11586 | `	if( pClass == 0 ){` |
|       ! 0 | 11587 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11588 | `		return SXERR_ABORT;` |
|         - | 11589 | `	}` |
|      7975 | 11590 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7975 | 11591 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11592 | `		return SXERR_ABORT;` |
|         - | 11593 | `	}` |
|         - | 11594 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7975 | 11595 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11596 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11597 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11598 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11599 | `			return SXERR_ABORT;` |
|         - | 11600 | `		}` |
|       ! 0 | 11601 | `		return SXRET_OK;` |
|         - | 11602 | `	}` |
|      7975 | 11603 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7975 | 11604 | `	pEnd = 0;` |
|      7975 | 11605 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7975 | 11606 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11607 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11608 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11609 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11610 | `			return SXERR_ABORT;` |
|         - | 11611 | `		}` |
|       ! 0 | 11612 | `		return SXRET_OK;` |
|         - | 11613 | `	}` |
|         - | 11614 | `	/* The delimiter token is the trait body's closing brace */` |
|      7975 | 11615 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11616 | `	/* Swap token stream */` |
|      7975 | 11617 | `	pTmp = pGen->pEnd;` |
|      7975 | 11618 | `	pGen->pEnd = pEnd;` |
|         - | 11619 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7975 | 11620 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11621 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     55323 | 11622 | `	for(;;){` |
|    150159 | 11623 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     19761 | 11624 | `			pGen->pIn++;` |
|         5 | 11625 | `		}` |
|    130403 | 11626 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7975 | 11627 | `			break;` |
|         - | 11628 | `		}` |
|         - | 11629 | `		/* Bind a directly-preceding docblock to this member */` |
|    122433 | 11630 | `		GenStateSetPendingDoc(&(*pGen));` |
|    122433 | 11631 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11632 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11633 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11634 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11635 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11636 | `				return SXERR_ABORT;` |
|         - | 11637 | `			}` |
|       ! 0 | 11638 | `			goto done;` |
|         - | 11639 | `		}` |
|    122433 | 11640 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    122433 | 11641 | `		iAttrflags = 0;` |
|    122433 | 11642 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    122433 | 11643 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    122433 | 11644 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11645 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11646 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11647 | `				for(;;){` |
|         - | 11648 | `					ph7_class *pUsedTrait;` |
|         - | 11649 | `					SyString *pUsedName;` |
|         5 | 11650 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11651 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11652 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11653 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11654 | `							return SXERR_ABORT;` |
|         - | 11655 | `						}` |
|       ! 0 | 11656 | `						break;` |
|         - | 11657 | `					}` |
|         5 | 11658 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11659 | `					{` |
|         - | 11660 | `						SyBlob sResolved;` |
|         5 | 11661 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11662 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11663 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11664 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11665 | `						SyBlobRelease(&sResolved);` |
|         - | 11666 | `					}` |
|         5 | 11667 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11668 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11669 | `					}` |
|         5 | 11670 | `					if( pUsedTrait == 0 ){` |
|         4 | 11671 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11672 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11673 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11674 | `							return SXERR_ABORT;` |
|         - | 11675 | `						}` |
|         2 | 11676 | `					}else{` |
|         3 | 11677 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11678 | `					}` |
|         5 | 11679 | `					pGen->pIn++;` |
|         5 | 11680 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11681 | `						break;` |
|         - | 11682 | `					}` |
|       ! 0 | 11683 | `					pGen->pIn++;` |
|       ! 0 | 11684 | `				}` |
|         5 | 11685 | `				continue;` |
|         - | 11686 | `			}` |
|    122429 | 11687 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    122413 | 11688 | `				iProtection = nKwrd;` |
|    122413 | 11689 | `				pGen->pIn++;` |
|    122408 | 11690 | `				if( pGen->pIn >= pGen->pEnd` |
|    122413 | 11691 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11692 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11693 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11694 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11695 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11696 | `						return SXERR_ABORT;` |
|         - | 11697 | `					}` |
|       ! 0 | 11698 | `					goto done;` |
|         - | 11699 | `				}` |
|    122413 | 11700 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     19747 | 11701 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     19747 | 11702 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11703 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11704 | `							return SXERR_ABORT;` |
|         - | 11705 | `						}` |
|       ! 0 | 11706 | `						goto done;` |
|         - | 11707 | `					}` |
|     19747 | 11708 | `					continue;` |
|         - | 11709 | `				}` |
|    102671 | 11710 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 11711 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 11712 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11713 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11714 | `							return SXERR_ABORT;` |
|         - | 11715 | `						}` |
|       ! 0 | 11716 | `						goto done;` |
|         - | 11717 | `					}` |
|         5 | 11718 | `					continue;` |
|         - | 11719 | `				}` |
|    102667 | 11720 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     51331 | 11721 | `			}` |
|    102683 | 11722 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 11723 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11724 | `					"Traits cannot have constants");` |
|       ! 0 | 11725 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11726 | `					return SXERR_ABORT;` |
|         - | 11727 | `				}` |
|       ! 0 | 11728 | `				goto done;` |
|       ! 0 | 11729 | `			}else{` |
|    102683 | 11730 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7907 | 11731 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7907 | 11732 | `					pGen->pIn++;` |
|      7907 | 11733 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7905 | 11734 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7905 | 11735 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11736 | `							iProtection = nKwrd;` |
|       ! 0 | 11737 | `							pGen->pIn++;` |
|       ! 0 | 11738 | `						}` |
|      3950 | 11739 | `					}` |
|      7902 | 11740 | `					if( pGen->pIn >= pGen->pEnd` |
|      7907 | 11741 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11742 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11743 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 11744 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11745 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11746 | `							return SXERR_ABORT;` |
|         - | 11747 | `						}` |
|       ! 0 | 11748 | `						goto done;` |
|         - | 11749 | `					}` |
|      7907 | 11750 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 11751 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 11752 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11753 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11754 | `								return SXERR_ABORT;` |
|         - | 11755 | `							}` |
|       ! 0 | 11756 | `							goto done;` |
|         - | 11757 | `						}` |
|         3 | 11758 | `						continue;` |
|         - | 11759 | `					}` |
|      7905 | 11760 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 11761 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11762 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11763 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11764 | `								return SXERR_ABORT;` |
|         - | 11765 | `							}` |
|       ! 0 | 11766 | `							goto done;` |
|         - | 11767 | `						}` |
|       ! 0 | 11768 | `						continue;` |
|         - | 11769 | `					}` |
|      7905 | 11770 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     98731 | 11771 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 11772 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 11773 | `					pGen->pIn++;` |
|         6 | 11774 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 11775 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 11776 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 11777 | `							iProtection = nKwrd;` |
|         6 | 11778 | `							pGen->pIn++;` |
|         2 | 11779 | `						}` |
|         2 | 11780 | `					}` |
|         6 | 11781 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 11782 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11783 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11784 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 11785 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11786 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11787 | `							return SXERR_ABORT;` |
|         - | 11788 | `						}` |
|       ! 0 | 11789 | `						goto done;` |
|         - | 11790 | `					}` |
|         6 | 11791 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 11792 | `				}` |
|    102681 | 11793 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11794 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11795 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 11796 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11797 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11798 | `						return SXERR_ABORT;` |
|         - | 11799 | `					}` |
|       ! 0 | 11800 | `					goto done;` |
|         - | 11801 | `				}` |
|    102681 | 11802 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 11803 | `					pGen->pIn++;` |
|       ! 0 | 11804 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 11805 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11806 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11807 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11808 | `							return SXERR_ABORT;` |
|         - | 11809 | `						}` |
|       ! 0 | 11810 | `						goto done;` |
|         - | 11811 | `					}` |
|       ! 0 | 11812 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11813 | `				}else{` |
|    102681 | 11814 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11815 | `				}` |
|    102681 | 11816 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11817 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11818 | `						return SXERR_ABORT;` |
|         - | 11819 | `					}` |
|       ! 0 | 11820 | `					goto done;` |
|         - | 11821 | `				}` |
|         - | 11822 | `			}` |
|     51343 | 11823 | `		}else{` |
|       ! 0 | 11824 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11825 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11826 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11827 | `					return SXERR_ABORT;` |
|         - | 11828 | `				}` |
|       ! 0 | 11829 | `				goto done;` |
|         - | 11830 | `			}` |
|         - | 11831 | `		}` |
|         5 | 11832 | `	}` |
|         - | 11833 | `	/* Install the trait */` |
|      7975 | 11834 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7975 | 11835 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11836 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11837 | `		return SXERR_ABORT;` |
|         - | 11838 | `	}` |
|      3985 | 11839 | `done:` |
|         - | 11840 | `	/* Point beyond the trait body */` |
|      7975 | 11841 | `	pGen->pIn = &pEnd[1];` |
|      7975 | 11842 | `	pGen->pEnd = pTmp;` |
|      7975 | 11843 | `	return PH7_OK;` |
|      3990 | 11844 | `}` |
|         - | 11845 | `/*` |
|         - | 11846 | ` * Compile a user-defined class.` |
|         - | 11847 | ` *  According to the PHP language reference manual` |
|         - | 11848 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 11849 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 11850 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 11851 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 11852 | ` *   and functions (called "methods").` |
|         - | 11853 | ` */` |
|    325210 | 11854 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 11855 | `{` |
|         - | 11856 | `	sxi32 rc;` |
|    325215 | 11857 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    325215 | 11858 | `	return rc;` |
|         5 | 11859 | `}` |
|         - | 11860 | `/*` |
|         - | 11861 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 11862 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 11863 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 11864 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 11865 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 11866 | ` */` |
|  11099238 | 11867 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11868 | `{` |
|  11287039 | 11869 | `	return (pIn->nType & PH7_TK_ID)` |
|   5737415 | 11870 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    197784 | 11871 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  11287034 | 11872 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 11873 | `}` |
|         - | 11874 | `/*` |
|         - | 11875 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 11876 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 11877 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 11878 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 11879 | ` */` |
|      3974 | 11880 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 11881 | `{` |
|      3979 | 11882 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 11883 | `}` |
|         - | 11884 | `/*` |
|         - | 11885 | ` * Exception handling.` |
|         - | 11886 | ` *  According to the PHP language reference manual` |
|         - | 11887 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 11888 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 11889 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 11890 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 11891 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 11892 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 11893 | ` *    (or re-thrown) within a catch block.` |
|         - | 11894 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 11895 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 11896 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 11897 | ` *    been defined with set_exception_handler().` |
|         - | 11898 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 11899 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 11900 | ` */` |
|         - | 11901 | `/*` |
|         - | 11902 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 11903 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 11904 | ` * indicates failure.` |
|         - | 11905 | ` */` |
|    493662 | 11906 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 11907 | `{` |
|    493667 | 11908 | `	sxi32 rc = SXRET_OK;` |
|    493667 | 11909 | `	if( pRoot->pOp ){` |
|    493655 | 11910 | `		switch( pRoot->pOp->iOp ){` |
|    246825 | 11911 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 11912 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 11913 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 11914 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 11915 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 11916 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    493655 | 11917 | `			break;` |
|       ! 0 | 11918 | `		default:` |
|         - | 11919 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 11920 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 11921 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 11922 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11923 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 11924 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 11925 | `				rc = SXERR_INVALID;` |
|       ! 0 | 11926 | `			}` |
|       ! 0 | 11927 | `			break;` |
|         - | 11928 | `		}` |
|    246842 | 11929 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 11930 | `		/* Unexpected expression */` |
|       ! 0 | 11931 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11932 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 11933 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 11934 | `			rc = SXERR_INVALID;` |
|       ! 0 | 11935 | `		}` |
|       ! 0 | 11936 | `	}` |
|    493667 | 11937 | `	return rc;` |
|         5 | 11938 | `}` |
|         - | 11939 | `/*` |
|         - | 11940 | ` * Compile a 'throw' statement.` |
|         - | 11941 | ` * throw: This is how you trigger an exception.` |
|         - | 11942 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 11943 | ` */` |
|    493626 | 11944 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 11945 | `{` |
|    493631 | 11946 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11947 | `	GenBlock *pBlock;` |
|         - | 11948 | `	sxu32 nIdx;` |
|         - | 11949 | `	sxi32 rc;` |
|    493631 | 11950 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 11951 | `	/* Compile the expression */` |
|    493631 | 11952 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    493631 | 11953 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 11954 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 11955 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11956 | `			return SXERR_ABORT;` |
|         - | 11957 | `		}` |
|       ! 0 | 11958 | `		return SXRET_OK;` |
|         - | 11959 | `	}` |
|    493631 | 11960 | `	pBlock = pGen->pCurrent;` |
|         - | 11961 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   1950101 | 11962 | `	while(pBlock->pParent){` |
|   1950097 | 11963 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    493627 | 11964 | `			break;` |
|         - | 11965 | `		}` |
|         - | 11966 | `		/* Point to the parent block */` |
|   1456475 | 11967 | `		pBlock = pBlock->pParent;` |
|         5 | 11968 | `	}` |
|         - | 11969 | `	/* Emit the throw instruction */` |
|    493631 | 11970 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 11971 | `	/* Emit the jump */` |
|    493631 | 11972 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    493631 | 11973 | `	return SXRET_OK;` |
|    246818 | 11974 | `}` |
|         - | 11975 | `/*` |
|         - | 11976 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 11977 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 11978 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 11979 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 11980 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 11981 | ` */` |
|        36 | 11982 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 11983 | `{` |
|        38 | 11984 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11985 | `	GenBlock *pBlock;` |
|         - | 11986 | `	sxu32 nIdx;` |
|         - | 11987 | `	sxi32 rc;` |
|        18 | 11988 | `	(void)iCompileFlag;` |
|        38 | 11989 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 11990 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 11991 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 11992 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 11993 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11994 | `			return SXERR_ABORT;` |
|         - | 11995 | `		}` |
|       ! 0 | 11996 | `		return SXRET_OK;` |
|         - | 11997 | `	}` |
|        38 | 11998 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 11999 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12000 | `		return SXERR_ABORT;` |
|         - | 12001 | `	}` |
|        38 | 12002 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12003 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12004 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12005 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12006 | `			return SXERR_ABORT;` |
|         - | 12007 | `		}` |
|       ! 0 | 12008 | `		return SXRET_OK;` |
|         - | 12009 | `	}` |
|         - | 12010 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12011 | `	pBlock = pGen->pCurrent;` |
|        60 | 12012 | `	while( pBlock->pParent ){` |
|        49 | 12013 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12014 | `			break;` |
|         - | 12015 | `		}` |
|        23 | 12016 | `		pBlock = pBlock->pParent;` |
|         1 | 12017 | `	}` |
|        38 | 12018 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12019 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12020 | `	return SXRET_OK;` |
|        20 | 12021 | `}` |
|         - | 12022 | `/*` |
|         - | 12023 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12024 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12025 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12026 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12027 | ` * compile error propagated from the parser.` |
|         - | 12028 | ` */` |
|        54 | 12029 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12030 | `{` |
|         - | 12031 | `	SyString sClassName;` |
|         - | 12032 | `	SyToken *pToken;` |
|         - | 12033 | `	SyString *pName;` |
|         - | 12034 | `	char *zDup;` |
|         - | 12035 | `	sxi32 rc;` |
|        59 | 12036 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        59 | 12037 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        59 | 12038 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        59 | 12039 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        59 | 12040 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12041 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12042 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12043 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12044 | `		return SXERR_INVALID;` |
|         - | 12045 | `	}` |
|        59 | 12046 | `	pGen->pIn++; /* '(' */` |
|        27 | 12047 | `	for(;;){` |
|         - | 12048 | `		SyBlob sResolved;` |
|        59 | 12049 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        59 | 12050 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12051 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12052 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12053 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12054 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12055 | `			return SXERR_INVALID;` |
|         - | 12056 | `		}` |
|        86 | 12057 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        54 | 12058 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        59 | 12059 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        59 | 12060 | `		SyBlobRelease(&sResolved);` |
|        59 | 12061 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12062 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        59 | 12063 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        54 | 12064 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12065 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12066 | `			pGen->pIn++; continue;` |
|         - | 12067 | `		}` |
|        59 | 12068 | `		break;` |
|       ! 0 | 12069 | `	}` |
|        54 | 12070 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12071 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12072 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12073 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12074 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12075 | `		return SXERR_INVALID;` |
|         - | 12076 | `	}` |
|        59 | 12077 | `	pGen->pIn++; /* '$' */` |
|        59 | 12078 | `	pName = &pGen->pIn->sData;` |
|        59 | 12079 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12080 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12081 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12082 | `	pGen->pIn++;` |
|        59 | 12083 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12084 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12085 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12086 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12087 | `		return SXERR_INVALID;` |
|         - | 12088 | `	}` |
|        59 | 12089 | `	pGen->pIn++; /* ')' */` |
|        59 | 12090 | `	return SXRET_OK;` |
|        32 | 12091 | `}` |
|         - | 12092 | `/*` |
|         - | 12093 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12094 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12095 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12096 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12097 | ` * VmThrowException):` |
|         - | 12098 | ` *` |
|         - | 12099 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12100 | ` *    <try body>` |
|         - | 12101 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12102 | ` *    JMP  -> finally\|end` |
|         - | 12103 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12104 | ` *    <catch body>` |
|         - | 12105 | ` *    JMP  -> finally\|end` |
|         - | 12106 | ` *    ... more catches ...` |
|         - | 12107 | ` *  Lfin: <finally body>` |
|         - | 12108 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12109 | ` *  Lend:` |
|         - | 12110 | ` */` |
|        98 | 12111 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12112 | `{` |
|       103 | 12113 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12114 | `	GenBlock *pTry;` |
|         - | 12115 | `	VmInstr *pInstr;` |
|       103 | 12116 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12117 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12118 | `	sxi32 rc;` |
|       103 | 12119 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12120 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       103 | 12121 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       103 | 12122 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       103 | 12123 | `	pTry->pUserData = pException;` |
|       103 | 12124 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       103 | 12125 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       103 | 12126 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       103 | 12127 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       103 | 12128 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       103 | 12129 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12130 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       103 | 12131 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       103 | 12132 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       103 | 12133 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       103 | 12134 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12135 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       103 | 12136 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12137 | `	/* Catch clauses (inline) */` |
|       103 | 12138 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        98 | 12139 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        59 | 12140 | `		sxu32 k = 0;` |
|        81 | 12141 | `		for(;;){` |
|         - | 12142 | `			ph7_exception_block sCatch;` |
|         - | 12143 | `			GenBlock *pCatchBlk;` |
|       113 | 12144 | `			sxu32 idxJmp = 0;` |
|       108 | 12145 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       104 | 12146 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        32 | 12147 | `				break;` |
|         - | 12148 | `			}` |
|        59 | 12149 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        59 | 12150 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12151 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        59 | 12152 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        59 | 12153 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        59 | 12154 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        59 | 12155 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12156 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12157 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12158 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        59 | 12159 | `			pCatchBlk->pUserData = pException;` |
|        59 | 12160 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        59 | 12161 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12162 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        59 | 12163 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12164 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12165 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        59 | 12166 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        59 | 12167 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        59 | 12168 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        59 | 12169 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        59 | 12170 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        59 | 12171 | `			k++;` |
|         5 | 12172 | `		}` |
|        27 | 12173 | `	}` |
|         - | 12174 | `	/* Finally (inline) */` |
|       103 | 12175 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12176 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12177 | `		GenBlock *pFinBlk;` |
|        52 | 12178 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12179 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12180 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12181 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12182 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12183 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12184 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12185 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12186 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12187 | `		pException->iHasFinally = 1;` |
|        24 | 12188 | `	}` |
|       103 | 12189 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       103 | 12190 | `	pException->iInlined = 1;` |
|         - | 12191 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12192 | `	{` |
|       103 | 12193 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12194 | `		sxu32 *aJ; sxu32 n;` |
|       103 | 12195 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       103 | 12196 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       103 | 12197 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       157 | 12198 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        59 | 12199 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        59 | 12200 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        32 | 12201 | `		}` |
|         - | 12202 | `	}` |
|       103 | 12203 | `	SySetRelease(&aCatchJmp);` |
|       103 | 12204 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12205 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12206 | `	}` |
|       103 | 12207 | `	return SXRET_OK;` |
|        54 | 12208 | `}` |
|         - | 12209 | `/*` |
|         - | 12210 | ` * Compile a 'catch' block.` |
|         - | 12211 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12212 | ` * an object containing the exception information.` |
|         - | 12213 | ` */` |
|     25130 | 12214 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12215 | `{` |
|     25135 | 12216 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12217 | `	ph7_exception_block sCatch;` |
|         - | 12218 | `	SySet *pInstrContainer;` |
|         - | 12219 | `	SyString sClassName;` |
|         - | 12220 | `	GenBlock *pCatch;` |
|         - | 12221 | `	SyToken *pToken;` |
|         - | 12222 | `	SyString *pName;` |
|         - | 12223 | `	char *zDup;` |
|         - | 12224 | `	sxi32 rc;` |
|     25135 | 12225 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12226 | `	/* Zero the structure */` |
|     25135 | 12227 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12228 | `	/* Initialize fields */` |
|     25135 | 12229 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     25135 | 12230 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     25135 | 12231 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12232 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12233 | `			pToken = pGen->pIn;` |
|       ! 0 | 12234 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12235 | `				pToken--;` |
|       ! 0 | 12236 | `			}` |
|       ! 0 | 12237 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12238 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12239 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12240 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12241 | `				return SXERR_ABORT;` |
|         - | 12242 | `			}` |
|       ! 0 | 12243 | `			return SXERR_INVALID;` |
|         - | 12244 | `	}` |
|         - | 12245 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     25135 | 12246 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12579 | 12247 | `	for(;;){` |
|         - | 12248 | `		SyBlob sResolved;` |
|     25163 | 12249 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     25163 | 12250 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12251 | `			SyBlobRelease(&sResolved);` |
|         6 | 12252 | `			pToken = pGen->pIn;` |
|         6 | 12253 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12254 | `				pToken--;` |
|       ! 0 | 12255 | `			}` |
|         8 | 12256 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12257 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12258 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12259 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12260 | `				return SXERR_ABORT;` |
|         - | 12261 | `			}` |
|         6 | 12262 | `			return SXERR_INVALID;` |
|         - | 12263 | `		}` |
|         - | 12264 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12265 | `		 * transient SyBlob allocation. */` |
|     37736 | 12266 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     25154 | 12267 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     25159 | 12268 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     25159 | 12269 | `		SyBlobRelease(&sResolved);` |
|     25159 | 12270 | `		if( zDup == 0 ){` |
|       ! 0 | 12271 | `			goto Mem;` |
|         - | 12272 | `		}` |
|     25159 | 12273 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     25159 | 12274 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12275 | `			goto Mem;` |
|         - | 12276 | `		}` |
|         - | 12277 | `		/* Check for '\|' (multi-catch separator) */` |
|     25154 | 12278 | `		if( pGen->pIn < pGen->pEnd &&` |
|     25154 | 12279 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        33 | 12280 | `			pGen->pIn->sData.nByte == 1 &&` |
|        28 | 12281 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        30 | 12282 | `			pGen->pIn++; /* Consume the '\|' */` |
|        30 | 12283 | `			continue;` |
|         - | 12284 | `		}` |
|     25131 | 12285 | `		break;` |
|       ! 0 | 12286 | `	}` |
|     25126 | 12287 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     25131 | 12288 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12289 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12290 | `			pToken = pGen->pIn;` |
|       ! 0 | 12291 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12292 | `				pToken--;` |
|       ! 0 | 12293 | `			}` |
|       ! 0 | 12294 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12295 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12296 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12297 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12298 | `				return SXERR_ABORT;` |
|         - | 12299 | `			}` |
|       ! 0 | 12300 | `			return SXERR_INVALID;` |
|         - | 12301 | `	}` |
|     25131 | 12302 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12303 | `	/* Duplicate instance name */` |
|     25131 | 12304 | `	pName = &pGen->pIn->sData;` |
|     25131 | 12305 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     25131 | 12306 | `	if( zDup == 0 ){` |
|       ! 0 | 12307 | `		goto Mem;` |
|         - | 12308 | `	}` |
|     25131 | 12309 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     25131 | 12310 | `	pGen->pIn++;` |
|     25131 | 12311 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12312 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12313 | `		pToken = pGen->pIn;` |
|       ! 0 | 12314 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12315 | `			pToken--;` |
|       ! 0 | 12316 | `		}` |
|       ! 0 | 12317 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12318 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12319 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12320 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12321 | `			return SXERR_ABORT;` |
|         - | 12322 | `		}` |
|       ! 0 | 12323 | `		return SXERR_INVALID;` |
|         - | 12324 | `	}` |
|         - | 12325 | `	/* Compile the block */` |
|     25131 | 12326 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12327 | `	/* Create the catch block */` |
|     25131 | 12328 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     25131 | 12329 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12330 | `		return SXERR_ABORT;` |
|         - | 12331 | `	}` |
|         - | 12332 | `	/* Swap bytecode container */` |
|     25131 | 12333 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     25131 | 12334 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12335 | `	/* Compile the block */` |
|     25131 | 12336 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12337 | `	/* Fix forward jumps now the destination is resolved  */` |
|     25131 | 12338 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12339 | `	/* Emit the DONE instruction */` |
|     25131 | 12340 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12341 | `	/* Leave the block */` |
|     25131 | 12342 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12343 | `	/* Restore the default container */` |
|     25131 | 12344 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12345 | `	/* Install the catch block */` |
|     25131 | 12346 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     25131 | 12347 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12348 | `		goto Mem;` |
|         - | 12349 | `	}` |
|     25131 | 12350 | `	return SXRET_OK;` |
|       ! 0 | 12351 | `Mem:` |
|       ! 0 | 12352 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12353 | `	return SXERR_ABORT;` |
|     12570 | 12354 | `}` |
|         - | 12355 | `/*` |
|         - | 12356 | ` * Compile a 'try' block.` |
|         - | 12357 | ` * A function using an exception should be in a "try" block.` |
|         - | 12358 | ` * If the exception does not trigger, the code will continue` |
|         - | 12359 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12360 | ` * is "thrown".` |
|         - | 12361 | ` */` |
|     25286 | 12362 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12363 | `{` |
|         - | 12364 | `	ph7_exception *pException;` |
|     25291 | 12365 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12366 | `	GenBlock *pTry;` |
|         - | 12367 | `	sxu32 nJmpIdx;` |
|         - | 12368 | `	sxi32 rc;` |
|         - | 12369 | `	/* Create the exception container */` |
|     25291 | 12370 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     25291 | 12371 | `	if( pException == 0 ){` |
|       ! 0 | 12372 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12373 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12374 | `		return SXERR_ABORT;` |
|         - | 12375 | `	}` |
|         - | 12376 | `	/* Zero the structure */` |
|     25291 | 12377 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12378 | `	/* Initialize fields */` |
|     25291 | 12379 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     25291 | 12380 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     25291 | 12381 | `	pException->iHasFinally = 0;` |
|     25291 | 12382 | `	pException->iFinallyDone = 0;` |
|     25291 | 12383 | `	pException->pVm = pGen->pVm;` |
|         - | 12384 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12385 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12386 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12387 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12388 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12389 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     25291 | 12390 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       103 | 12391 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12392 | `	}` |
|         - | 12393 | `	/* Create the try block */` |
|     25193 | 12394 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     25193 | 12395 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12396 | `		return SXERR_ABORT;` |
|         - | 12397 | `	}` |
|         - | 12398 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     25193 | 12399 | `	pTry->pUserData = pException;` |
|         - | 12400 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     25193 | 12401 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12402 | `	/* Fix the jump later when the destination is resolved */` |
|     25193 | 12403 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     25193 | 12404 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12405 | `	/* Compile the block */` |
|     25193 | 12406 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     25193 | 12407 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12408 | `		return SXERR_ABORT;` |
|         - | 12409 | `	}` |
|         - | 12410 | `	/* Fix forward jumps now the destination is resolved */` |
|     25193 | 12411 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12412 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     25193 | 12413 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12414 | `	/* Leave the block */` |
|     25193 | 12415 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12416 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     25193 | 12417 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     25186 | 12418 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12419 | `		/* Compile one or more catch blocks */` |
|     25126 | 12420 | `		for(;;){` |
|     50252 | 12421 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     37760 | 12422 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12566 | 12423 | `					break;` |
|         - | 12424 | `			}` |
|     25135 | 12425 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     25135 | 12426 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12427 | `				return SXERR_ABORT;` |
|         - | 12428 | `			}` |
|         5 | 12429 | `		}` |
|     12561 | 12430 | `	}` |
|         - | 12431 | `	/* Compile optional finally block */` |
|     25193 | 12432 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       726 | 12433 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12434 | `		SySet *pInstrContainer;` |
|         - | 12435 | `		GenBlock *pFinBlock;` |
|       129 | 12436 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12437 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12438 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12439 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12440 | `			return SXERR_ABORT;` |
|         - | 12441 | `		}` |
|         - | 12442 | `		/* Swap bytecode container */` |
|       129 | 12443 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12444 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12445 | `		/* Compile the finally body */` |
|       129 | 12446 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12447 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12448 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12449 | `			return SXERR_ABORT;` |
|         - | 12450 | `		}` |
|         - | 12451 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12452 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12453 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12454 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12455 | `		/* Leave the block */` |
|       129 | 12456 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12457 | `		/* Restore the default container */` |
|       129 | 12458 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12459 | `		pException->iHasFinally = 1;` |
|        62 | 12460 | `	}` |
|         - | 12461 | `	/* Must have at least one catch or finally */` |
|     25193 | 12462 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12463 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12464 | `			"Cannot use try without catch or finally");` |
|         8 | 12465 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12466 | `			return SXERR_ABORT;` |
|         - | 12467 | `		}` |
|         3 | 12468 | `	}` |
|     25193 | 12469 | `	return SXRET_OK;` |
|     12648 | 12470 | `}` |
|         - | 12471 | `/*` |
|         - | 12472 | ` * Compile a switch block.` |
|         - | 12473 | ` *  (See block-comment below for more information)` |
|         - | 12474 | ` */` |
|       112 | 12475 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12476 | `{` |
|       117 | 12477 | `	sxi32 rc = SXRET_OK;` |
|       117 | 12478 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12479 | `		/* Unexpected token */` |
|       ! 0 | 12480 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12481 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12482 | `			return SXERR_ABORT;` |
|         - | 12483 | `		}` |
|       ! 0 | 12484 | `		pGen->pIn++;` |
|       ! 0 | 12485 | `	}` |
|       117 | 12486 | `	pGen->pIn++;` |
|         - | 12487 | `	/* First instruction to execute in this block. */` |
|       117 | 12488 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12489 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12490 | `	 * or the '}' token */` |
|       206 | 12491 | `	for(;;){` |
|       417 | 12492 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12493 | `			/* No more input to process */` |
|       ! 0 | 12494 | `			break;` |
|         - | 12495 | `		}` |
|       417 | 12496 | `		rc = SXRET_OK;` |
|       417 | 12497 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        85 | 12498 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|        31 | 12499 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12500 | `					/* Unexpected token */` |
|       ! 0 | 12501 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12502 | `						&pGen->pIn->sData);` |
|       ! 0 | 12503 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12504 | `						return SXERR_ABORT;` |
|         - | 12505 | `					}` |
|         - | 12506 | `					/* FALL THROUGH */` |
|       ! 0 | 12507 | `				}` |
|        31 | 12508 | `				rc = SXERR_EOF;` |
|        31 | 12509 | `				break;` |
|         - | 12510 | `			}` |
|        32 | 12511 | `		}else{` |
|         - | 12512 | `			sxi32 nKwrd;` |
|         - | 12513 | `			/* Extract the keyword */` |
|       337 | 12514 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       337 | 12515 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|        47 | 12516 | `				break;` |
|         - | 12517 | `			}` |
|       253 | 12518 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12519 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12520 | `					/* Unexpected token */` |
|       ! 0 | 12521 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12522 | `						&pGen->pIn->sData);` |
|       ! 0 | 12523 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12524 | `						return SXERR_ABORT;` |
|         - | 12525 | `					}` |
|         - | 12526 | `					/* FALL THROUGH */` |
|       ! 0 | 12527 | `				}` |
|         - | 12528 | `				/* Block compiled */` |
|         3 | 12529 | `				break;` |
|         - | 12530 | `			}` |
|         - | 12531 | `		}` |
|         - | 12532 | `		/* Compile block */` |
|       305 | 12533 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       305 | 12534 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12535 | `			return SXERR_ABORT;` |
|         - | 12536 | `		}` |
|         5 | 12537 | `	}` |
|       117 | 12538 | `	return rc;` |
|        61 | 12539 | `}` |
|         - | 12540 | `/*` |
|         - | 12541 | ` * Compile a case eXpression.` |
|         - | 12542 | ` *  (See block-comment below for more information)` |
|         - | 12543 | ` */` |
|        92 | 12544 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12545 | `{` |
|         - | 12546 | `	SySet *pInstrContainer;` |
|         - | 12547 | `	SyToken *pEnd,*pTmp;` |
|        97 | 12548 | `	sxi32 iNest = 0;` |
|         - | 12549 | `	sxi32 rc;` |
|         - | 12550 | `	/* Delimit the expression */` |
|        97 | 12551 | `	pEnd = pGen->pIn;` |
|       197 | 12552 | `	while( pEnd < pGen->pEnd ){` |
|       197 | 12553 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12554 | `			/* Increment nesting level */` |
|         3 | 12555 | `			iNest++;` |
|       196 | 12556 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12557 | `			/* Decrement nesting level */` |
|         3 | 12558 | `			iNest--;` |
|       194 | 12559 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|        97 | 12560 | `			break;` |
|         - | 12561 | `		}` |
|       105 | 12562 | `		pEnd++;` |
|         5 | 12563 | `	}` |
|        97 | 12564 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12565 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12566 | `		if( rc == SXERR_ABORT ){` |
|         - | 12567 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12568 | `			return SXERR_ABORT;` |
|         - | 12569 | `		}` |
|       ! 0 | 12570 | `	}` |
|         - | 12571 | `	/* Swap token stream */` |
|        97 | 12572 | `	pTmp = pGen->pEnd;` |
|        97 | 12573 | `	pGen->pEnd = pEnd;` |
|        97 | 12574 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        97 | 12575 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|        97 | 12576 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12577 | `	/* Emit the done instruction */` |
|        97 | 12578 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        97 | 12579 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12580 | `	/* Update token stream */` |
|        97 | 12581 | `	pGen->pIn  = pEnd;` |
|        97 | 12582 | `	pGen->pEnd = pTmp;` |
|        97 | 12583 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12584 | `		return SXERR_ABORT;` |
|         - | 12585 | `	}` |
|        97 | 12586 | `	return SXRET_OK;` |
|        51 | 12587 | `}` |
|         - | 12588 | `/*` |
|         - | 12589 | ` * Compile the smart switch statement.` |
|         - | 12590 | ` * According to the PHP language reference manual` |
|         - | 12591 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12592 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12593 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12594 | ` *  This is exactly what the switch statement is for.` |
|         - | 12595 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12596 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12597 | ` *  of the outer loop, use continue 2.` |
|         - | 12598 | ` *  Note that switch/case does loose comparision.` |
|         - | 12599 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12600 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12601 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12602 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12603 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12604 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12605 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12606 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12607 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12608 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12609 | ` *  list for the next case.` |
|         - | 12610 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12611 | ` *  or floating-point numbers and strings.` |
|         - | 12612 | ` */` |
|        28 | 12613 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12614 | `{` |
|         - | 12615 | `	GenBlock *pSwitchBlock;` |
|         - | 12616 | `	SyToken *pTmp,*pEnd;` |
|         - | 12617 | `	ph7_switch *pSwitch;` |
|         - | 12618 | `	sxu32 nToken;` |
|         - | 12619 | `	sxu32 nLine;` |
|         - | 12620 | `	sxi32 rc;` |
|        33 | 12621 | `	nLine = pGen->pIn->nLine;` |
|         - | 12622 | `	/* Jump the 'switch' keyword */` |
|        33 | 12623 | `	pGen->pIn++;` |
|        33 | 12624 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12625 | `		/* Syntax error */` |
|       ! 0 | 12626 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12627 | `		if( rc == SXERR_ABORT ){` |
|         - | 12628 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12629 | `			return SXERR_ABORT;` |
|         - | 12630 | `		}` |
|       ! 0 | 12631 | `		goto Synchronize;` |
|         - | 12632 | `	}` |
|         - | 12633 | `	/* Jump the left parenthesis '(' */` |
|        33 | 12634 | `	pGen->pIn++;` |
|        33 | 12635 | `	pEnd = 0; /* cc warning */` |
|         - | 12636 | `	/* Create the loop block */` |
|        47 | 12637 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|        14 | 12638 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|        33 | 12639 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12640 | `		return SXERR_ABORT;` |
|         - | 12641 | `	}` |
|         - | 12642 | `	/* Delimit the condition */` |
|        33 | 12643 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|        33 | 12644 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12645 | `		/* Empty expression */` |
|       ! 0 | 12646 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12647 | `		if( rc == SXERR_ABORT ){` |
|         - | 12648 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12649 | `			return SXERR_ABORT;` |
|         - | 12650 | `		}` |
|       ! 0 | 12651 | `	}` |
|         - | 12652 | `	/* Swap token streams */` |
|        33 | 12653 | `	pTmp = pGen->pEnd;` |
|        33 | 12654 | `	pGen->pEnd = pEnd;` |
|         - | 12655 | `	/* Compile the expression */` |
|        33 | 12656 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        33 | 12657 | `	if( rc == SXERR_ABORT ){` |
|         - | 12658 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12659 | `		return SXERR_ABORT;` |
|         - | 12660 | `	}` |
|         - | 12661 | `	/* Update token stream */` |
|        33 | 12662 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12663 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12664 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12665 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12666 | `			return SXERR_ABORT;` |
|         - | 12667 | `		}` |
|       ! 0 | 12668 | `		pGen->pIn++;` |
|       ! 0 | 12669 | `	}` |
|        33 | 12670 | `	pGen->pIn  = &pEnd[1];` |
|        33 | 12671 | `	pGen->pEnd = pTmp;` |
|        33 | 12672 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        28 | 12673 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12674 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12675 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12676 | `				pTmp--;` |
|       ! 0 | 12677 | `			}` |
|         - | 12678 | `			/* Unexpected token */` |
|       ! 0 | 12679 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12680 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12681 | `				return SXERR_ABORT;` |
|         - | 12682 | `			}` |
|       ! 0 | 12683 | `			goto Synchronize;` |
|         - | 12684 | `	}` |
|         - | 12685 | `	/* Set the delimiter token */` |
|        33 | 12686 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12687 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12688 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12689 | `	}else{` |
|        31 | 12690 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12691 | `	}` |
|        33 | 12692 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12693 | `	/* Create the switch blocks container */` |
|        33 | 12694 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|        33 | 12695 | `	if( pSwitch == 0 ){` |
|         - | 12696 | `		/* Abort compilation */` |
|       ! 0 | 12697 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12698 | `		return SXERR_ABORT;` |
|         - | 12699 | `	}` |
|         - | 12700 | `	/* Zero the structure */` |
|        33 | 12701 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 12702 | `	/* Initialize fields */` |
|        33 | 12703 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 12704 | `	/* Emit the switch instruction */` |
|        33 | 12705 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 12706 | `	/* Compile case blocks */` |
|       100 | 12707 | `	for(;;){` |
|         - | 12708 | `		sxu32 nKwrd;` |
|       119 | 12709 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12710 | `			/* No more input to process */` |
|       ! 0 | 12711 | `			break;` |
|         - | 12712 | `		}` |
|       119 | 12713 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 12714 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 12715 | `				/* Unexpected token */` |
|       ! 0 | 12716 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12717 | `					&pGen->pIn->sData);` |
|       ! 0 | 12718 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12719 | `					return SXERR_ABORT;` |
|         - | 12720 | `				}` |
|         - | 12721 | `				/* FALL THROUGH */` |
|       ! 0 | 12722 | `			}` |
|         - | 12723 | `			/* Block compiled */` |
|       ! 0 | 12724 | `			break;` |
|         - | 12725 | `		}` |
|         - | 12726 | `		/* Extract the keyword */` |
|       119 | 12727 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       119 | 12728 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12729 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 12730 | `				/* Unexpected token */` |
|       ! 0 | 12731 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12732 | `					&pGen->pIn->sData);` |
|       ! 0 | 12733 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12734 | `					return SXERR_ABORT;` |
|         - | 12735 | `				}` |
|         - | 12736 | `				/* FALL THROUGH */` |
|       ! 0 | 12737 | `			}` |
|         - | 12738 | `			/* Block compiled */` |
|         3 | 12739 | `			break;` |
|         - | 12740 | `		}` |
|       117 | 12741 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 12742 | `			/*` |
|         - | 12743 | `			 * Accroding to the PHP language reference manual` |
|         - | 12744 | `			 *  A special case is the default case. This case matches anything` |
|         - | 12745 | `			 *  that wasn't matched by the other cases.` |
|         - | 12746 | `			 */` |
|        25 | 12747 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 12748 | `				/* Default case already compiled */` |
|       ! 0 | 12749 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 12750 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12751 | `					return SXERR_ABORT;` |
|         - | 12752 | `				}` |
|       ! 0 | 12753 | `			}` |
|        25 | 12754 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 12755 | `			/* Compile the default block */` |
|        25 | 12756 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 12757 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12758 | `				return SXERR_ABORT;` |
|        25 | 12759 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 12760 | `				break;` |
|         1 | 12761 | `			}` |
|        98 | 12762 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 12763 | `			ph7_case_expr sCase;` |
|         - | 12764 | `			/* Standard case block */` |
|        97 | 12765 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 12766 | `			/* initialize the structure */` |
|        97 | 12767 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 12768 | `			/* Compile the case expression */` |
|        97 | 12769 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|        97 | 12770 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12771 | `				return SXERR_ABORT;` |
|         - | 12772 | `			}` |
|         - | 12773 | `			/* Compile the case block */` |
|        97 | 12774 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 12775 | `			/* Insert in the switch container */` |
|        97 | 12776 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|        97 | 12777 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12778 | `				return SXERR_ABORT;` |
|        97 | 12779 | `			}else if( rc == SXERR_EOF ){` |
|         9 | 12780 | `				break;` |
|         - | 12781 | `			}` |
|        47 | 12782 | `		}else{` |
|         - | 12783 | `			/* Unexpected token */` |
|       ! 0 | 12784 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12785 | `				&pGen->pIn->sData);` |
|       ! 0 | 12786 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12787 | `				return SXERR_ABORT;` |
|         - | 12788 | `			}` |
|       ! 0 | 12789 | `			break;` |
|         - | 12790 | `		}` |
|         5 | 12791 | `	}` |
|         - | 12792 | `	/* Fix all jumps now the destination is resolved */` |
|        33 | 12793 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|        33 | 12794 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12795 | `	/* Release the loop block */` |
|        33 | 12796 | `	GenStateLeaveBlock(pGen,0);` |
|        33 | 12797 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 12798 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|        33 | 12799 | `		pGen->pIn++;` |
|        14 | 12800 | `	}` |
|         - | 12801 | `	/* Statement successfully compiled */` |
|        33 | 12802 | `	return SXRET_OK;` |
|       ! 0 | 12803 | `Synchronize:` |
|         - | 12804 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 12805 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 12806 | `		pGen->pIn++;` |
|       ! 0 | 12807 | `	}` |
|       ! 0 | 12808 | `	return SXRET_OK;` |
|        19 | 12809 | `}` |
|         - | 12810 | `/*` |
|         - | 12811 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 12812 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 12813 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 12814 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 12815 | ` */` |
|         - | 12816 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 12817 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 12818 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 12819 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 12820 |  |
|         - | 12821 | `/*` |
|         - | 12822 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 12823 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 12824 | ` * patched entries from the pending set.` |
|         - | 12825 | ` */` |
|  41189624 | 12826 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 12827 | `{` |
|  41189629 | 12828 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 12829 | `	sxu32 nTarget;` |
|         - | 12830 | `	sxu32 *aIdx;` |
|         - | 12831 | `	sxu32 i;` |
|  41189629 | 12832 | `	if( nCur <= nBaseline ){` |
|  41189533 | 12833 | `		return;` |
|         - | 12834 | `	}` |
|       100 | 12835 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 12836 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 12837 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 12838 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 12839 | `		if( pInstr ){` |
|       108 | 12840 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 12841 | `		}` |
|        56 | 12842 | `	}` |
|       100 | 12843 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  20594817 | 12844 | `}` |
|         - | 12845 |  |
|         - | 12846 | `/*` |
|         - | 12847 | ` * By-reference out-parameters of builtin functions.` |
|         - | 12848 | ` *` |
|         - | 12849 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 12850 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 12851 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 12852 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 12853 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 12854 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 12855 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 12856 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 12857 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 12858 | ` * creates it" behaviour).` |
|         - | 12859 | ` *` |
|         - | 12860 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 12861 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 12862 | ` */` |
|   5596872 | 12863 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 12864 | `{` |
|         - | 12865 | `	static const struct {` |
|         - | 12866 | `		const char *zName;` |
|         - | 12867 | `		sxu32 nByte;` |
|         - | 12868 | `		sxu32 mask;` |
|         - | 12869 | `	} aByRef[] = {` |
|         - | 12870 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12871 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12872 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12873 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12874 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 12875 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 12876 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 12877 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 12878 | `	};` |
|         - | 12879 | `	sxu32 i;` |
|   5596877 | 12880 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1650059 | 12881 | `		return 0;` |
|         - | 12882 | `	}` |
|  35216735 | 12883 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  31309502 | 12884 | `		if( pName->nByte == aByRef[i].nByte` |
|  16360763 | 12885 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     39595 | 12886 | `			return aByRef[i].mask;` |
|         - | 12887 | `		}` |
|  15634961 | 12888 | `	}` |
|   3907233 | 12889 | `	return 0;` |
|   2798441 | 12890 | `}` |
|         - | 12891 | `/*` |
|         - | 12892 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 12893 | ` *` |
|         - | 12894 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 12895 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 12896 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 12897 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 12898 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 12899 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 12900 | ` */` |
|   5596872 | 12901 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 12902 | `{` |
|         - | 12903 | `	SyToken *p, *pEnd;` |
|   5596877 | 12904 | `	pOut->zString = 0;` |
|   5596877 | 12905 | `	pOut->nByte = 0;` |
|   5596877 | 12906 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 12907 | `		return;` |
|         - | 12908 | `	}` |
|   5596877 | 12909 | `	p = pLeft->pStart;` |
|   5596877 | 12910 | `	pEnd = pLeft->pEnd;` |
|         - | 12911 | `	/* Optional single leading namespace separator (absolute path). */` |
|   5596877 | 12912 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3979 | 12913 | `		p++;` |
|      1987 | 12914 | `	}` |
|   5596877 | 12915 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1650023 | 12916 | `		return;` |
|         - | 12917 | `	}` |
|         - | 12918 | `	/* Must be a single component: nothing follows the name token. */` |
|   3946859 | 12919 | `	if( p + 1 != pEnd ){` |
|        41 | 12920 | `		return;` |
|         - | 12921 | `	}` |
|   3946823 | 12922 | `	*pOut = p->sData;` |
|   2798441 | 12923 | `}` |
|         - | 12924 | `/*` |
|         - | 12925 | ` * Generate bytecode for a given expression tree.` |
|         - | 12926 | ` * If something goes wrong while generating bytecode` |
|         - | 12927 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 12928 | ` * this function takes care of generating the appropriate` |
|         - | 12929 | ` * error message.` |
|         - | 12930 | ` */` |
|  59509946 | 12931 | `static sxi32 GenStateEmitExprCode(` |
|         - | 12932 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 12933 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 12934 | `	sxi32 iFlags /* Control flags */` |
|         - | 12935 | `	)` |
|         5 | 12936 | `{` |
|         - | 12937 | `	VmInstr *pInstr;` |
|         - | 12938 | `	sxu32 nJmpIdx;` |
|  59509951 | 12939 | `	sxi32 iP1 = 0;` |
|  59509951 | 12940 | `	sxu32 iP2 = 0;` |
|  59509951 | 12941 | `	void *p3  = 0;` |
|         - | 12942 | `	sxi32 iVmOp;` |
|         - | 12943 | `	sxi32 rc;` |
|  59509951 | 12944 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  59509951 | 12945 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  59509951 | 12946 | `	sxu32 nRhsNsBase = 0;` |
|  59509951 | 12947 | `	if( pNode->xCode ){` |
|         - | 12948 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 12949 | `		/* Compile node */` |
|  35594521 | 12950 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  35594521 | 12951 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  35594521 | 12952 | `		RE_SWAP_DELIMITER(pGen);` |
|  35594521 | 12953 | `		return rc;` |
|         - | 12954 | `	}` |
|  23915435 | 12955 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 12956 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 12957 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 12958 | `		return SXERR_ABORT;` |
|         - | 12959 | `	}` |
|  23915435 | 12960 | `	iVmOp = pNode->pOp->iVmOp;` |
|  23915435 | 12961 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 12962 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 12963 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 12964 | `		 * and later errors are still reported. */` |
|         3 | 12965 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 12966 | `			"The (unset) cast is no longer supported");` |
|         3 | 12967 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12968 | `			return SXERR_ABORT;` |
|         - | 12969 | `		}` |
|         1 | 12970 | `	}` |
|  23915435 | 12971 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        91 | 12972 | `		sxu32 nJmp = 0;` |
|         - | 12973 | `		sxu32 nNcNsBase;` |
|         - | 12974 | `		VmInstr *pInstrFix;` |
|         - | 12975 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 12976 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 12977 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 12978 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 12979 | `		 * stack slot carries a writable nIdx. */` |
|        91 | 12980 | `		if( pNode->pRight ){` |
|        91 | 12981 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 12982 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        91 | 12983 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12984 | `				return rc;` |
|         - | 12985 | `			}` |
|        91 | 12986 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 12987 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 12988 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 12989 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 12990 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 12991 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 12992 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 12993 | `			 * cascade for the actual write path stays correct. */` |
|        91 | 12994 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        91 | 12995 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 12996 | `				pInstrFix->iP2 = 3;` |
|        15 | 12997 | `			}` |
|        44 | 12998 | `		}` |
|         - | 12999 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        91 | 13000 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13001 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        91 | 13002 | `		if( pNode->pLeft ){` |
|        91 | 13003 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13004 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        91 | 13005 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13006 | `				return rc;` |
|         - | 13007 | `			}` |
|        91 | 13008 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        44 | 13009 | `		}` |
|         - | 13010 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        91 | 13011 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13012 | `		/* Patch the short-circuit jump to land after the store. */` |
|        91 | 13013 | `		if( nJmp > 0 ){` |
|        91 | 13014 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        91 | 13015 | `			if( pInstrFix ){` |
|        91 | 13016 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        44 | 13017 | `			}` |
|        44 | 13018 | `		}` |
|        91 | 13019 | `		return SXRET_OK;` |
|         - | 13020 | `	}` |
|  23915347 | 13021 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13022 | `		sxu32 nJz,nJmp;` |
|         - | 13023 | `		sxu32 nTernaryNsBase;` |
|         - | 13024 | `		/* Ternary operator require special handling */` |
|         - | 13025 | `		/* Phase#1: Compile the condition */` |
|    382129 | 13026 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    382129 | 13027 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    382129 | 13028 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13029 | `			return rc;` |
|         - | 13030 | `		}` |
|         - | 13031 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13032 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13033 | `		 * condition expression, not leak past the ternary. */` |
|    382129 | 13034 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    382129 | 13035 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    382129 | 13036 | `		if( pNode->pLeft ){` |
|         - | 13037 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13038 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    378115 | 13039 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13040 | `			/* Phase#3: Compile the 'then' expression  */` |
|    378115 | 13041 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    378115 | 13042 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    378115 | 13043 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13044 | `				return rc;` |
|         - | 13045 | `			}` |
|    378115 | 13046 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    189060 | 13047 | `		}else{` |
|         - | 13048 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13049 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13050 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      4019 | 13051 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      4019 | 13052 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13053 | `		}` |
|         - | 13054 | `		/* Phase#4: Emit the unconditional jump */` |
|    382129 | 13055 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13056 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    382129 | 13057 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    382129 | 13058 | `		if( pInstr ){` |
|    382129 | 13059 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    191062 | 13060 | `		}` |
|    382129 | 13061 | `		if( !pNode->pLeft ){` |
|         - | 13062 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      4019 | 13063 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      2007 | 13064 | `		}` |
|         - | 13065 | `		/* Phase#6: Compile the 'else' expression */` |
|    382129 | 13066 | `		if( pNode->pRight ){` |
|    382129 | 13067 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    382129 | 13068 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    382129 | 13069 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13070 | `				return rc;` |
|         - | 13071 | `			}` |
|    382129 | 13072 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    191062 | 13073 | `		}` |
|    382129 | 13074 | `		if( nJmp > 0 ){` |
|         - | 13075 | `			/* Phase#7: Fix the unconditional jump */` |
|    382129 | 13076 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    382129 | 13077 | `			if( pInstr ){` |
|    382129 | 13078 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    191062 | 13079 | `			}` |
|    191062 | 13080 | `		}` |
|         - | 13081 | `		/* All done */` |
|    382129 | 13082 | `		return SXRET_OK;` |
|         - | 13083 | `	}` |
|  23533223 | 13084 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13085 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13086 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13087 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13088 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13089 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13090 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13091 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13092 | `		sxu32 nPipeNsBase;` |
|        27 | 13093 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13094 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13095 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13096 | `				"'\|>': Missing operand");` |
|       ! 0 | 13097 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13098 | `		}` |
|         - | 13099 | `		/* Argument: the LHS value. */` |
|        27 | 13100 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13101 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13102 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13103 | `			return rc;` |
|         - | 13104 | `		}` |
|        27 | 13105 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13106 | `		/* Callable: the RHS. */` |
|        27 | 13107 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13108 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13109 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13110 | `			return rc;` |
|         - | 13111 | `		}` |
|        27 | 13112 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13113 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13114 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13115 | `		return SXRET_OK;` |
|         - | 13116 | `	}` |
|  23533197 | 13117 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13118 | `	/* Generate code for the left tree */` |
|  23533197 | 13119 | `	if( pNode->pLeft ){` |
|  23509519 | 13120 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  23509519 | 13121 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13122 | `			ph7_expr_node **apNode;` |
|   5601139 | 13123 | `			int hasSpread = 0;` |
|   5601139 | 13124 | `			int hasNamed = 0;` |
|   5601139 | 13125 | `			int bAnySpread = 0;` |
|   5601139 | 13126 | `			sxu32 byRefMask = 0;` |
|         - | 13127 | `			sxi32 nArgs;` |
|         - | 13128 | `			sxi32 n;` |
|         - | 13129 | `			/* Recurse and generate bytecodes for function arguments */` |
|   5601139 | 13130 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5601139 | 13131 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13132 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13133 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13134 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   5601139 | 13135 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13136 | `				bFcc = 1;` |
|        81 | 13137 | `				nArgs = 0;` |
|        40 | 13138 | `			}` |
|         - | 13139 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13140 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13141 | `			{` |
|   5601139 | 13142 | `				int seenNamed = 0;` |
|   5601139 | 13143 | `				int seenSpread = 0;` |
|  11437353 | 13144 | `				for( n = 0; n < nArgs; ++n ){` |
|   5836221 | 13145 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4135 | 13146 | `						bAnySpread = 1;` |
|      4135 | 13147 | `						seenSpread = 1;` |
|      4135 | 13148 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13149 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13150 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13151 | `							return SXERR_SYNTAX;` |
|         5 | 13152 | `						}` |
|   5834156 | 13153 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13154 | `						seenNamed = 1;` |
|       289 | 13155 | `						hasNamed = 1;` |
|   5831949 | 13156 | `					}else if( seenNamed ){` |
|         3 | 13157 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13158 | `							"Cannot use positional argument after named argument");` |
|         3 | 13159 | `						return SXERR_SYNTAX;` |
|   5831805 | 13160 | `					}else if( seenSpread ){` |
|       ! 0 | 13161 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13162 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13163 | `						return SXERR_SYNTAX;` |
|         - | 13164 | `					}` |
|   2918112 | 13165 | `				}` |
|         - | 13166 | `			}` |
|         - | 13167 | `			/* Read-only load */` |
|   5601137 | 13168 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13169 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13170 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13171 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13172 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   5601137 | 13173 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   5601137 | 13174 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   5601132 | 13175 | `				if( pCallName->nByte == 5` |
|   3149151 | 13176 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    284497 | 13177 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5458891 | 13178 | `				}else if( pCallName->nByte == 5` |
|   2864659 | 13179 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       107 | 13180 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        51 | 13181 | `				}` |
|         - | 13182 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13183 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13184 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13185 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13186 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13187 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   5601137 | 13188 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13189 | `					SyString sBuiltin;` |
|   5596877 | 13190 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   5596877 | 13191 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   2798436 | 13192 | `				}` |
|   2800566 | 13193 | `			}` |
|  11437349 | 13194 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   5836217 | 13195 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   5836217 | 13196 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13197 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13198 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13199 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13200 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13201 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13202 | `				 * (iP1=0 either way). */` |
|   5836217 | 13203 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     27695 | 13204 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     27695 | 13205 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     13845 | 13206 | `				}` |
|   5836217 | 13207 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   5836217 | 13208 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13209 | `					return rc;` |
|         - | 13210 | `				}` |
|         - | 13211 | `				/* Each argument is an independent nullsafe scope. */` |
|   5836217 | 13212 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   5836217 | 13213 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13214 | `					/* Emit spread opcode to unpack this array argument */` |
|      4135 | 13215 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4135 | 13216 | `					hasSpread = 1;` |
|      2065 | 13217 | `				}` |
|   2918111 | 13218 | `			}` |
|         - | 13219 | `			/* Total number of given arguments */` |
|   5601137 | 13220 | `			iP1 = nArgs;` |
|   5601137 | 13221 | `			iP2 = hasSpread;` |
|         - | 13222 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13223 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   5601137 | 13224 | `			if( hasNamed ){` |
|       178 | 13225 | `				sxu32 nStrBytes = 0;` |
|         - | 13226 | `				char *zBuf;` |
|       534 | 13227 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13228 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13229 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13230 | `					}` |
|       182 | 13231 | `				}` |
|         - | 13232 | `				{` |
|       178 | 13233 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13234 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13235 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13236 | `				if( pMap ){` |
|       178 | 13237 | `					SyZero(pMap, mapSize);` |
|       178 | 13238 | `					pMap->bHasNamed = 1;` |
|       178 | 13239 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13240 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13241 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13242 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13243 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13244 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13245 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13246 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13247 | `							zBuf += nb;` |
|       141 | 13248 | `						}` |
|         - | 13249 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13250 | `					}` |
|       178 | 13251 | `					p3 = (void *)pMap;` |
|        87 | 13252 | `				}` |
|         - | 13253 | `				}` |
|        87 | 13254 | `			}` |
|         - | 13255 | `			/* Remove stale flags now */` |
|   5601137 | 13256 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   2800566 | 13257 | `		}` |
|         - | 13258 | `		{` |
|         - | 13259 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13260 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13261 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13262 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13263 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13264 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13265 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13266 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  23509517 | 13267 | `			sxi32 iLeftFlags = iFlags;` |
|  23509512 | 13268 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  19339487 | 13269 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   7584757 | 13270 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   6434397 | 13271 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2478791 | 13272 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1239393 | 13273 | `			}` |
|         - | 13274 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13275 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13276 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13277 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13278 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13279 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13280 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  23509512 | 13281 | `			if( pNode->pOp` |
|  32925199 | 13282 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  21170490 | 13283 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  18831416 | 13284 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   5049959 | 13285 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2524977 | 13286 | `			}` |
|         - | 13287 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13288 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13289 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13290 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13291 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13292 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  23509512 | 13293 | `			if( pNode->pOp` |
|  23509517 | 13294 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    150439 | 13295 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     75217 | 13296 | `			}` |
|  23509517 | 13297 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13298 | `		}` |
|  23509517 | 13299 | `		if( rc != SXRET_OK ){` |
|        34 | 13300 | `			return rc;` |
|         - | 13301 | `		}` |
|  23509487 | 13302 | `		if( !bIsChainOp ){` |
|         - | 13303 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13304 | `			 * target the end of that LHS chain, which is right here. */` |
|  10262769 | 13305 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   5131382 | 13306 | `		}` |
|  23509487 | 13307 | `		if( iVmOp == PH7_OP_CALL ){` |
|   5601137 | 13308 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5601137 | 13309 | `			if( pInstr ){` |
|   5601137 | 13310 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   3947099 | 13311 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13312 | `					sxu32 nQual;` |
|   3947099 | 13313 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13314 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13315 | `					 * so the later NEW handler (if any) can see it. */` |
|   3947099 | 13316 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13317 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13318 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13319 | `					 * imports — class imports must NOT affect function` |
|         - | 13320 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13321 | `					 * before NEW; we store the original literal index in the` |
|         - | 13322 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13323 | `					 * the unqualified name and re-qualify with class imports. */` |
|   3947099 | 13324 | `					if( bAbsolute ){` |
|      3979 | 13325 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1992 | 13326 | `					}else{` |
|   3943125 | 13327 | `						int fromImport = 0;` |
|   3943125 | 13328 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   3943125 | 13329 | `						pInstr->iP2 = (sxi32)nQual;` |
|   3943125 | 13330 | `						if( nQual != nOrig ){` |
|         - | 13331 | `							/* Record the original literal index in the arg map` |
|         - | 13332 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13333 | `							 * flag) so the NEW handler can recover the` |
|         - | 13334 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13335 | `							 * imports. */` |
|        77 | 13336 | `							if( p3 == 0 ){` |
|        77 | 13337 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        72 | 13338 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        77 | 13339 | `								if( pMap ){` |
|        77 | 13340 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        77 | 13341 | `									p3 = (void *)pMap;` |
|        36 | 13342 | `								}` |
|        36 | 13343 | `							}` |
|        77 | 13344 | `							if( p3 ){` |
|        77 | 13345 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        77 | 13346 | `								if( !fromImport ){` |
|         - | 13347 | `									/* Mark as namespace-qualified */` |
|        67 | 13348 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        31 | 13349 | `								}` |
|        36 | 13350 | `							}` |
|        36 | 13351 | `						}` |
|         5 | 13352 | `					}` |
|   3627590 | 13353 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13354 | `					/* Method call,flag that */` |
|   1633701 | 13355 | `					pInstr->iP2 = 1;` |
|    816848 | 13356 | `				}` |
|   2800571 | 13357 | `			}` |
|  20708921 | 13358 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13359 | `			ph7_expr_node **apNode;` |
|         - | 13360 | `			sxi32 n;` |
|   2595637 | 13361 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13362 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13363 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13364 | `			/* Recurse and generate bytecodes for array index */` |
|   2595637 | 13365 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5029267 | 13366 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2433635 | 13367 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2433635 | 13368 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2433635 | 13369 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13370 | `					return rc;` |
|         - | 13371 | `				}` |
|         - | 13372 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2433635 | 13373 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1216820 | 13374 | `			}` |
|   2595637 | 13375 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2433635 | 13376 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1216815 | 13377 | `			}` |
|   2595637 | 13378 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13379 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    323841 | 13380 | `				iP2 = 4;` |
|   2433719 | 13381 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13382 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13383 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23757 | 13384 | `				iP2 = 5;` |
|   2259925 | 13385 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13386 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13387 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13388 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 13389 | `				iP2 = 6;` |
|   2248037 | 13390 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13391 | `				/* Create an empty entry when the desired index is not found */` |
|    379555 | 13392 | `				iP2 = 1;` |
|    189780 | 13393 | `			}` |
|  16610539 | 13394 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13395 | `			/* POP the left node */` |
|         5 | 13396 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13397 | `		}` |
|  11754741 | 13398 | `	}` |
|  23533165 | 13399 | `	rc = SXRET_OK;` |
|  23533165 | 13400 | `	nJmpIdx = 0;` |
|         - | 13401 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13402 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13403 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  23533165 | 13404 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    395489 | 13405 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    395489 | 13406 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    395489 | 13407 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    395489 | 13408 | `			int isSpecial = 0;` |
|    395489 | 13409 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    348105 | 13410 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    348105 | 13411 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    348100 | 13412 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    316472 | 13413 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    172049 | 13414 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    106691 | 13415 | `					isSpecial = 1;` |
|     53343 | 13416 | `				}` |
|    185896 | 13417 | `			}` |
|    419181 | 13418 | `			pInstr->iP1 = 0;` |
|    419181 | 13419 | `			if( !isSpecial ){` |
|    265111 | 13420 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    132553 | 13421 | `			}` |
|         - | 13422 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13423 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    371797 | 13424 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    265111 | 13425 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    265111 | 13426 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        60 | 13427 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        62 | 13428 | `					return SXRET_OK;` |
|         - | 13429 | `				}` |
|    132524 | 13430 | `			}` |
|    185867 | 13431 | `		}` |
|    233230 | 13432 | `	}` |
|         - | 13433 | `	/* Generate code for the right tree */` |
|  23509429 | 13434 | `	if( pNode->pRight ){` |
|  13511925 | 13435 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13436 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    324093 | 13437 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  13349881 | 13438 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13439 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    221157 | 13440 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  13077261 | 13441 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13442 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     51453 | 13443 | `			iVmOp = 0; /* No binary operator to emit */` |
|     51453 | 13444 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  12941013 | 13445 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13446 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13447 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13448 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13449 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13450 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13451 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13452 | `			sxu32 nNsJmp = 0;` |
|       108 | 13453 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13454 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  12915185 | 13455 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13456 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13457 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13458 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4103645 | 13459 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2051820 | 13460 | `		}` |
|  13511925 | 13461 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  13511925 | 13462 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  13511925 | 13463 | `		if( !bIsChainOp ){` |
|         - | 13464 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13465 | `			 * operator instruction is emitted. */` |
|   8462029 | 13466 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   4231012 | 13467 | `		}` |
|  13511925 | 13468 | `		if( iVmOp == PH7_OP_STORE ){` |
|   3677151 | 13469 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   3677114 | 13470 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13471 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13472 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13473 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13474 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13475 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13476 | `				 */` |
|        91 | 13477 | `				iVmOp = 0;` |
|   3677108 | 13478 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   3677065 | 13479 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13480 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    793857 | 13481 | `					iP2 = 1;` |
|    396931 | 13482 | `				}else{` |
|   2883213 | 13483 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13484 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    359729 | 13485 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    359729 | 13486 | `						iP1 = pInstr->iP1;` |
|    179867 | 13487 | `					}else{` |
|   2523489 | 13488 | `						p3 = pInstr->p3;` |
|         - | 13489 | `					}` |
|         - | 13490 | `					/* POP the last dynamic load instruction */` |
|   2883213 | 13491 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13492 | `				}` |
|   1838535 | 13493 | `			}` |
|  11673352 | 13494 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13495 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13496 | `			if( pInstr ){` |
|        63 | 13497 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13498 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13499 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13500 | `					 */` |
|        19 | 13501 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13502 | `					iP1 = pInstr->iP1;` |
|        19 | 13503 | `					iP2 = pInstr->iP2;` |
|        19 | 13504 | `					p3  = pInstr->p3;` |
|        10 | 13505 | `				}else{` |
|        45 | 13506 | `					p3 = pInstr->p3;` |
|         - | 13507 | `				}` |
|        30 | 13508 | `			}` |
|        30 | 13509 | `		}` |
|   6755960 | 13510 | `	}` |
|  23509424 | 13511 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    368587 | 13512 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13513 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13514 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13515 | `		iVmOp = 0;` |
|        14 | 13516 | `	}` |
|  23509429 | 13517 | `	if( iVmOp > 0 ){` |
|  23457863 | 13518 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    150439 | 13519 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13520 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     11875 | 13521 | `				iP1 = 1;` |
|      5940 | 13522 | `			}` |
|  23382646 | 13523 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13524 | `			/* Namespace-qualify the class name for NEW */ {` |
|    736817 | 13525 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    736817 | 13526 | `				VmInstr *pCallInstr = 0;` |
|    736817 | 13527 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    736521 | 13528 | `					pCallInstr = pPeek;` |
|    736521 | 13529 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    368258 | 13530 | `				}` |
|    736817 | 13531 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    721029 | 13532 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13533 | `					sxu32 nLitForClass;` |
|    721029 | 13534 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13535 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13536 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13537 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13538 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13539 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13540 | `					 * with class imports. */` |
|    721029 | 13541 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        37 | 13542 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        21 | 13543 | `					}else{` |
|    720997 | 13544 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13545 | `					}` |
|    721029 | 13546 | `					pPeek->iP1 = 0;` |
|    721029 | 13547 | `					if( !bAbsolute ){` |
|    717059 | 13548 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    358532 | 13549 | `					}else{` |
|      3975 | 13550 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13551 | `					}` |
|    360512 | 13552 | `				}` |
|         - | 13553 | `			}` |
|    736817 | 13554 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    736817 | 13555 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13556 | `				VmInstr *pPrev;` |
|    736521 | 13557 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    736521 | 13558 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13559 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13560 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13561 | `					 * accumulator exactly like OP_CALL would have). */` |
|    736521 | 13562 | `					iP1 = pInstr->iP1;` |
|    736521 | 13563 | `					iP2 = pInstr->iP2;` |
|    736521 | 13564 | `					if( pInstr->p3 ){` |
|        47 | 13565 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        21 | 13566 | `					}` |
|    736521 | 13567 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    368258 | 13568 | `				}` |
|    368263 | 13569 | `			}` |
|  22939023 | 13570 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13571 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13572 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     71285 | 13573 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     71285 | 13574 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     71285 | 13575 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     71285 | 13576 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     71285 | 13577 | `				int isSpecialIs = 0;` |
|     71285 | 13578 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     71285 | 13579 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     71285 | 13580 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     71280 | 13581 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     71283 | 13582 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     35640 | 13583 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13584 | `						isSpecialIs = 1;` |
|         5 | 13585 | `					}` |
|     35640 | 13586 | `				}` |
|     71285 | 13587 | `				pInstr->iP1 = 0;` |
|     71285 | 13588 | `				if( !isSpecialIs && !bAbsolute ){` |
|     71265 | 13589 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     35630 | 13590 | `				}` |
|     35645 | 13591 | `			}` |
|  22534977 | 13592 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13593 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13594 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13595 | `			 * should not trigger constant lookup. */` |
|   5049901 | 13596 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5049901 | 13597 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4809117 | 13598 | `				pInstr->iP1 = 0;` |
|   2404556 | 13599 | `			}` |
|   5049901 | 13600 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13601 | `				/* Static member access,remember that */` |
|    371753 | 13602 | `				iP1 = 1;` |
|    371753 | 13603 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    371753 | 13604 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    236827 | 13605 | `					p3 = pInstr->p3;` |
|    236827 | 13606 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    118411 | 13607 | `				}` |
|    185874 | 13608 | `			}` |
|         - | 13609 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13610 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13611 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13612 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   5049901 | 13613 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   5049901 | 13614 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13615 | `					iP2 = PH7_MEMBER_UNSET;` |
|   5049881 | 13616 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     63239 | 13617 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5018244 | 13618 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13619 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4986619 | 13620 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13621 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    967665 | 13622 | `					iP2 = PH7_MEMBER_WRITE;` |
|    483830 | 13623 | `				}` |
|   2524948 | 13624 | `			}` |
|   2524948 | 13625 | `		}` |
|         - | 13626 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13627 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13628 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13629 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13630 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  23457863 | 13631 | `		if( bFcc ){` |
|        81 | 13632 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13633 | `			iP2 = 0;` |
|        81 | 13634 | `			p3 = 0;` |
|        81 | 13635 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13636 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13637 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13638 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13639 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13640 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13641 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13642 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13643 | `				if( pMemberName ){` |
|         3 | 13644 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13645 | `				}` |
|        37 | 13646 | `				iP1 = 2;` |
|        19 | 13647 | `			}else{` |
|        45 | 13648 | `				iP1 = 1;` |
|         - | 13649 | `			}` |
|        40 | 13650 | `		}` |
|         - | 13651 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13652 | `		 * This is the primary emit path for user-visible calls. */` |
|  23457863 | 13653 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6337869 | 13654 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3168932 | 13655 | `		}` |
|         - | 13656 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  23457863 | 13657 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  11728929 | 13658 | `	}` |
|  23509429 | 13659 | `	if( nJmpIdx > 0 ){` |
|         - | 13660 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    596693 | 13661 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    596693 | 13662 | `		if( pInstr ){` |
|    596693 | 13663 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    298344 | 13664 | `		}` |
|    298344 | 13665 | `	}` |
|  23509429 | 13666 | `	return rc;` |
|  29743139 | 13667 | `}` |
|         - | 13668 | `/*` |
|         - | 13669 | ` * Compile a PHP expression.` |
|         - | 13670 | ` * According to the PHP language reference manual:` |
|         - | 13671 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 13672 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 13673 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 13674 | ` *  is "anything that has a value".` |
|         - | 13675 | ` * If something goes wrong while compiling the expression,this` |
|         - | 13676 | ` * function takes care of generating the appropriate error` |
|         - | 13677 | ` * message.` |
|         - | 13678 | ` */` |
|         - | 13679 | `/*` |
|         - | 13680 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 13681 | ` *` |
|         - | 13682 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 13683 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 13684 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 13685 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 13686 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 13687 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 13688 | ` * except for() now reports php's parse error.` |
|         - | 13689 | ` */` |
| 197311456 | 13690 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 13691 | `{` |
|         - | 13692 | `	ph7_expr_node **apArg;` |
|         - | 13693 | `	sxu32 n;` |
| 197311461 | 13694 | `	if( pNode == 0 ){` |
| 138564091 | 13695 | `		return 0;` |
|         - | 13696 | `	}` |
|  58747375 | 13697 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 13698 | `		return 1;` |
|         - | 13699 | `	}` |
|  58747366 | 13700 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  58747367 | 13701 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 13702 | `		return 1;` |
|         - | 13703 | `	}` |
|  58747367 | 13704 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  67001505 | 13705 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   8254143 | 13706 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 13707 | `			return 1;` |
|         - | 13708 | `		}` |
|   4127074 | 13709 | `	}` |
|  58747367 | 13710 | `	return 0;` |
|  98655733 | 13711 | `}` |
|  13076280 | 13712 | `static sxi32 PH7_CompileExpr(` |
|         - | 13713 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 13714 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 13715 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 13716 | `	)` |
|         5 | 13717 | `{` |
|         - | 13718 | `	ph7_expr_node *pRoot;` |
|         - | 13719 | `	SySet sExprNode;` |
|         - | 13720 | `	SyToken *pEnd;` |
|         - | 13721 | `	sxi32 nExpr;` |
|         - | 13722 | `	sxi32 iNest;` |
|         - | 13723 | `	sxi32 rc;` |
|         - | 13724 | `	sxu32 nNullsafeBase;` |
|         - | 13725 | `	/* Initialize worker variables */` |
|  13076285 | 13726 | `	nExpr = 0;` |
|  13076285 | 13727 | `	pRoot = 0;` |
|         - | 13728 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 13729 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  13076285 | 13730 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  13076285 | 13731 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  13076285 | 13732 | `	SySetAlloc(&sExprNode,0x10);` |
|  13076285 | 13733 | `	rc = SXRET_OK;` |
|         - | 13734 | `	/* Delimit the expression */` |
|  13076285 | 13735 | `	pEnd = pGen->pIn;` |
|  13076285 | 13736 | `	iNest = 0;` |
| 103847241 | 13737 | `	while( pEnd < pGen->pEnd ){` |
|  99058715 | 13738 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 13739 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4667 | 13740 | `			iNest++;` |
|  99056384 | 13741 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4675 | 13742 | `			iNest--;` |
|  99051718 | 13743 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   8288377 | 13744 | `			if( iNest <= 0 ){` |
|   8287759 | 13745 | `				break;` |
|         - | 13746 | `			}` |
|       309 | 13747 | `		}` |
|  90770961 | 13748 | `		pEnd++;` |
|         5 | 13749 | `	}` |
|  13076285 | 13750 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    659991 | 13751 | `		SyToken *pEnd2 = pGen->pIn;` |
|    659991 | 13752 | `		iNest = 0;` |
|         - | 13753 | `		/* Stop at the first comma */` |
|   1439085 | 13754 | `		while( pEnd2 < pEnd ){` |
|    779101 | 13755 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     43515 | 13756 | `				iNest++;` |
|    757346 | 13757 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     43515 | 13758 | `				iNest--;` |
|    713836 | 13759 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|        63 | 13760 | `				if( iNest <= 0 ){` |
|         3 | 13761 | `					break;` |
|         - | 13762 | `				}` |
|        28 | 13763 | `			}` |
|    779099 | 13764 | `			pEnd2++;` |
|         5 | 13765 | `		}` |
|    659991 | 13766 | `		if( pEnd2 <pEnd ){` |
|         3 | 13767 | `			pEnd = pEnd2;` |
|         1 | 13768 | `		}` |
|    329993 | 13769 | `	}` |
|  13076285 | 13770 | `	if( pEnd > pGen->pIn ){` |
|  13052599 | 13771 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 13772 | `		/* Swap delimiter */` |
|  13052599 | 13773 | `		pGen->pEnd = pEnd;` |
|         - | 13774 | `		/* Try to get an expression tree */` |
|  13052599 | 13775 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  13052594 | 13776 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  12933825 | 13777 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 13778 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 13779 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 13780 | `				"syntax error, unexpected token \",\"");` |
|         6 | 13781 | `			pGen->pEnd = pTmp;` |
|         6 | 13782 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13783 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 13784 | `				return SXERR_ABORT;` |
|         - | 13785 | `			}` |
|         6 | 13786 | `			pGen->pIn = pEnd;` |
|         6 | 13787 | `			SySetRelease(&sExprNode);` |
|         6 | 13788 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 13789 | `			return SXRET_OK;` |
|         - | 13790 | `		}` |
|  13052595 | 13791 | `		if( rc == SXRET_OK && pRoot ){` |
|  13052413 | 13792 | `			rc = SXRET_OK;` |
|  13052413 | 13793 | `			if( xTreeValidator ){` |
|         - | 13794 | `				/* Call the upper layer validator callback */` |
|    857253 | 13795 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    428624 | 13796 | `			}` |
|  13052413 | 13797 | `			if( rc != SXERR_ABORT ){` |
|         - | 13798 | `				/* Generate code for the given tree */` |
|  13052413 | 13799 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 13800 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 13801 | `				 * expression so they short-circuit to its end. */` |
|  13052413 | 13802 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   6526204 | 13803 | `			}` |
|  13052413 | 13804 | `			nExpr = 1;` |
|   6526204 | 13805 | `		}` |
|         - | 13806 | `		/* Release the whole tree */` |
|  13052595 | 13807 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 13808 | `		/* Synchronize token stream */` |
|  13052595 | 13809 | `		pGen->pEnd = pTmp;` |
|  13052595 | 13810 | `		pGen->pIn  = pEnd;` |
|  13052595 | 13811 | `		if( rc == SXERR_ABORT ){` |
|        13 | 13812 | `			SySetRelease(&sExprNode);` |
|        13 | 13813 | `			return SXERR_ABORT;` |
|         - | 13814 | `		}` |
|   6526290 | 13815 | `	}` |
|  13076271 | 13816 | `	SySetRelease(&sExprNode);` |
|  13076271 | 13817 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   6538145 | 13818 | `}` |
|         - | 13819 | `/*` |
|         - | 13820 | ` * Return a pointer to the node construct handler associated` |
|         - | 13821 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 13822 | ` */` |
|   7336414 | 13823 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 13824 | `{` |
|   7336419 | 13825 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 13826 | `		/* Numeric literal: Either real or integer */` |
|   2801865 | 13827 | `		return PH7_CompileNumLiteral;` |
|   4534559 | 13828 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 13829 | `		/* Double quoted string */` |
|     82215 | 13830 | `		return PH7_CompileString;` |
|   4452349 | 13831 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 13832 | `		/* Single quoted string */` |
|   4452229 | 13833 | `		return PH7_CompileSimpleString;` |
|       125 | 13834 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 13835 | `		/* Heredoc */` |
|        70 | 13836 | `		return PH7_CompileHereDoc;` |
|        59 | 13837 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 13838 | `		/* Nowdoc */` |
|        51 | 13839 | `		return PH7_CompileNowDoc;` |
|         9 | 13840 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 13841 | `		/* Backtick quoted string */` |
|         6 | 13842 | `		return PH7_CompileBacktic;` |
|         - | 13843 | `	}` |
|         3 | 13844 | `	return 0;` |
|   3668212 | 13845 | `}` |
|         - | 13846 | `/*` |
|         - | 13847 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 13848 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 13849 | ` * in write context" parse error.` |
|         - | 13850 | ` */` |
|     30888 | 13851 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 13852 | `{` |
|         - | 13853 | `	sxi32 rc;` |
|     30893 | 13854 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     30891 | 13855 | `		return SXRET_OK;` |
|         - | 13856 | `	}` |
|         5 | 13857 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 13858 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 13859 | `		"Can't use nullsafe operator in write context");` |
|         3 | 13860 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     15449 | 13861 | `}` |
|         - | 13862 | `/*` |
|         - | 13863 | ` * Compile an unset() statement.` |
|         - | 13864 | ` * unset($var, $arr[$key], ...);` |
|         - | 13865 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 13866 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 13867 | ` * parent array before extracting the element to unset.` |
|         - | 13868 | ` */` |
|     26666 | 13869 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 13870 | `{` |
|     26671 | 13871 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26671 | 13872 | `	sxu32 nIdx = 0;` |
|         - | 13873 | `	SyString sName;` |
|         - | 13874 | `	sxi32 rc;` |
|         - | 13875 | `	/* Jump the 'unset' keyword */` |
|     26671 | 13876 | `	pGen->pIn++;` |
|         - | 13877 | `	/* Save delimiter */` |
|     26671 | 13878 | `	pTmp = pGen->pEnd;` |
|         - | 13879 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26671 | 13880 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26671 | 13881 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 13882 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 13883 | `		SyToken *pClose;` |
|     26671 | 13884 | `		pGen->pIn++;   /* Skip '(' */` |
|     26671 | 13885 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26671 | 13886 | `		pEnd = pClose; /* Stop at ')' */` |
|     13333 | 13887 | `	}` |
|     26671 | 13888 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 13889 | `	/* Resolve the 'unset' builtin name once */` |
|     26671 | 13890 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3951 | 13891 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3951 | 13892 | `		if( pObj == 0 ){` |
|       ! 0 | 13893 | `			return SXERR_ABORT;` |
|         - | 13894 | `		}` |
|      3951 | 13895 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3951 | 13896 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1973 | 13897 | `	}` |
|         - | 13898 | `	/* Compile each comma-separated argument */` |
|     57561 | 13899 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30895 | 13900 | `		if( pGen->pIn < pNext ){` |
|     30895 | 13901 | `			pGen->pEnd = pNext;` |
|     30895 | 13902 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 13903 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 13904 | `				GenStateUnsetValidator);` |
|     30895 | 13905 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13906 | `				return SXERR_ABORT;` |
|         - | 13907 | `			}` |
|     30895 | 13908 | `			if( rc != SXERR_EMPTY ){` |
|         - | 13909 | `				/* Emit call for this single argument */` |
|     30893 | 13910 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     30893 | 13911 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     30893 | 13912 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     15444 | 13913 | `			}` |
|     15445 | 13914 | `		}` |
|         - | 13915 | `		/* Jump trailing commas */` |
|     35121 | 13916 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|      4231 | 13917 | `			pNext++;` |
|         5 | 13918 | `		}` |
|     30895 | 13919 | `		pGen->pIn = pNext;` |
|         5 | 13920 | `	}` |
|         - | 13921 | `	/* Skip past the closing ')' if present */` |
|     26671 | 13922 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26671 | 13923 | `		pGen->pIn++;` |
|     13333 | 13924 | `	}` |
|         - | 13925 | `	/* Restore token stream */` |
|     26671 | 13926 | `	pGen->pEnd = pTmp;` |
|     26671 | 13927 | `	return SXRET_OK;` |
|     13338 | 13928 | `}` |
|         - | 13929 | `/*` |
|         - | 13930 | ` * PHP Language construct table.` |
|         - | 13931 | ` */` |
|         - | 13932 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 13933 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 13934 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 13935 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 13936 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 13937 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 13938 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 13939 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 13940 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 13941 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 13942 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 13943 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 13944 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 13945 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 13946 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 13947 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 13948 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 13949 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 13950 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 13951 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 13952 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 13953 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 13954 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 13955 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 13956 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 13957 | `};` |
|         - | 13958 | `/*` |
|         - | 13959 | ` * Return a pointer to the statement handler routine associated` |
|         - | 13960 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 13961 | ` */` |
|   6492554 | 13962 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 13963 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 13964 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 13965 | `	)` |
|         5 | 13966 | `{` |
|   6492559 | 13967 | `	sxu32 n = 0;` |
|  26817170 | 13968 | `	for(;;){` |
|  53634345 | 13969 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    444265 | 13970 | `			break;` |
|         - | 13971 | `		}` |
|  53190085 | 13972 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6048299 | 13973 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 13974 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 13975 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 13976 | `					/* 'static' (class context),return null */` |
|       ! 0 | 13977 | `					return 0;` |
|         - | 13978 | `				}` |
|       ! 0 | 13979 | `			}` |
|   6048294 | 13980 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|        14 | 13981 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|        14 | 13982 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 13983 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 13984 | `				return 0;` |
|         - | 13985 | `			}` |
|         - | 13986 | `			/* Return a pointer to the handler.` |
|         - | 13987 | `			*/` |
|   6048297 | 13988 | `			return aLangConstruct[n].xConstruct;` |
|         - | 13989 | `		}` |
|  47141791 | 13990 | `		n++;` |
|         5 | 13991 | `	}` |
|    444265 | 13992 | `	if( pLookahed ){` |
|    444265 | 13993 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     71137 | 13994 | `			return PH7_CompileClassInterface;` |
|    373133 | 13995 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    325215 | 13996 | `			return PH7_CompileClass;` |
|     47923 | 13997 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7975 | 13998 | `			return PH7_CompileTrait;` |
|         - | 13999 | `		}` |
|         - | 14000 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14001 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14002 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14003 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19974 | 14004 | `	}` |
|         - | 14005 | `	/* Not a language construct */` |
|     39953 | 14006 | `	return 0;` |
|   3246282 | 14007 | `}` |
|         - | 14008 | `/*` |
|         - | 14009 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14010 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14011 | ` */` |
|     39950 | 14012 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14013 | `{` |
|         - | 14014 | `	int rc;` |
|     39955 | 14015 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     39955 | 14016 | `	if( rc == FALSE ){` |
|     39834 | 14017 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     16150 | 14018 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14019 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14020 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14021 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14022 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14023 | `			*/` |
|         - | 14024 | `			){` |
|     39831 | 14025 | `				rc = TRUE;` |
|     19913 | 14026 | `		}` |
|     19917 | 14027 | `	}` |
|     39955 | 14028 | `	return rc;` |
|         5 | 14029 | `}` |
|         - | 14030 | `/*` |
|         - | 14031 | ` * Compile a PHP chunk.` |
|         - | 14032 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14033 | ` * takes care of generating the appropriate error message.` |
|         - | 14034 | ` */` |
|         - | 14035 | `/*` |
|         - | 14036 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14037 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14038 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14039 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14040 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14041 | ` * intervening non-declaration statements.` |
|         - | 14042 | ` */` |
|  14385174 | 14043 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14044 | `{` |
|  14385179 | 14045 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  14385179 | 14046 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  14385179 | 14047 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14048 | `	sxu32 nIdx, n;` |
|  14385174 | 14049 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   1561579 | 14050 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14051 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14052 | `		 * indexes do not map to the sidecar */` |
|  12823607 | 14053 | `		return;` |
|         - | 14054 | `	}` |
|   1561577 | 14055 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14056 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14057 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   1561577 | 14058 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4686215 | 14059 | `	for( n = 0 ; n < nT ; n++ ){` |
|   3124643 | 14060 | `		if( aT[n].nTokIdx != nIdx ){` |
|   3116587 | 14061 | `			continue;` |
|         - | 14062 | `		}` |
|      8061 | 14063 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14064 | `			pGen->sPendingDoc = aT[n].sText;` |
|      8049 | 14065 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      8037 | 14066 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      4016 | 14067 | `		}` |
|      4033 | 14068 | `	}` |
|   7192592 | 14069 | `}` |
|         - | 14070 | `/*` |
|         - | 14071 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14072 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14073 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14074 | ` */` |
|   3996080 | 14075 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14076 | `{` |
|         - | 14077 | `	char *zDup;` |
|   3996085 | 14078 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   3996065 | 14079 | `		return;` |
|         - | 14080 | `	}` |
|        35 | 14081 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14082 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14083 | `	if( zDup ){` |
|        25 | 14084 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14085 | `	}` |
|        25 | 14086 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   1998045 | 14087 | `}` |
|         - | 14088 | `/*` |
|         - | 14089 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14090 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14091 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14092 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14093 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14094 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14095 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14096 | ` */` |
|      8044 | 14097 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14098 | `{` |
|         - | 14099 | `	SySet *pToken;` |
|         - | 14100 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14101 | `	char *zSpan;` |
|      8049 | 14102 | `	sxi32 rc = SXRET_OK;` |
|      8049 | 14103 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14104 | `		return SXRET_OK;` |
|         - | 14105 | `	}` |
|     12071 | 14106 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      4022 | 14107 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      8049 | 14108 | `	if( zSpan == 0 ){` |
|       ! 0 | 14109 | `		return SXRET_OK;` |
|         - | 14110 | `	}` |
|         - | 14111 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14112 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14113 | `	 * the number of attribute declarations in the program. */` |
|      8049 | 14114 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      8049 | 14115 | `	if( pToken == 0 ){` |
|       ! 0 | 14116 | `		return SXRET_OK;` |
|         - | 14117 | `	}` |
|      8049 | 14118 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      8049 | 14119 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      8049 | 14120 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      8049 | 14121 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      8049 | 14122 | `	pSavedIn = pGen->pIn;` |
|      8049 | 14123 | `	pSavedEnd = pGen->pEnd;` |
|      8053 | 14124 | `	while( pIn < pEnd ){` |
|         - | 14125 | `		ph7_attribute sAttr;` |
|         - | 14126 | `		SyBlob sFQN;` |
|      8053 | 14127 | `		int bAbsolute = 0;` |
|      8053 | 14128 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      8053 | 14129 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      8053 | 14130 | `		sAttr.nLine = pIn->nLine;` |
|      8053 | 14131 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14132 | `			bAbsolute = 1;` |
|        75 | 14133 | `			pIn++;` |
|        35 | 14134 | `		}` |
|      8053 | 14135 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      8053 | 14136 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      8053 | 14137 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      8053 | 14138 | `			pIn++;` |
|      8053 | 14139 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14140 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14141 | `				pIn++;` |
|       ! 0 | 14142 | `				continue;` |
|         - | 14143 | `			}` |
|      8053 | 14144 | `			break;` |
|       ! 0 | 14145 | `		}` |
|      8053 | 14146 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14147 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14148 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14149 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14150 | `			break;` |
|         - | 14151 | `		}` |
|         - | 14152 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14153 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14154 | `		{` |
|      8053 | 14155 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      8053 | 14156 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      8053 | 14157 | `			char *zDup = 0;` |
|      8053 | 14158 | `			if( !bAbsolute ){` |
|      7983 | 14159 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7983 | 14160 | `				if( pImp ){` |
|       ! 0 | 14161 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14162 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14163 | `					if( zDup ){` |
|       ! 0 | 14164 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14165 | `					}` |
|      7983 | 14166 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14167 | `					SyBlob sTmp;` |
|       ! 0 | 14168 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14169 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14170 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14171 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14172 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14173 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14174 | `					if( zDup ){` |
|       ! 0 | 14175 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14176 | `					}` |
|       ! 0 | 14177 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14178 | `				}` |
|      3989 | 14179 | `			}` |
|      8053 | 14180 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      8053 | 14181 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      8053 | 14182 | `				if( zDup ){` |
|      8053 | 14183 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      4024 | 14184 | `				}` |
|      4024 | 14185 | `			}` |
|         - | 14186 | `		}` |
|      8053 | 14187 | `		SyBlobRelease(&sFQN);` |
|      8053 | 14188 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14189 | `			SyToken *pArgsEnd;` |
|      7951 | 14190 | `			pIn++;` |
|      7951 | 14191 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15911 | 14192 | `			while( pIn < pArgsEnd ){` |
|      7965 | 14193 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7965 | 14194 | `				sxi32 iDepth = 0;` |
|         - | 14195 | `				ph7_attr_arg sArgRec;` |
|     79165 | 14196 | `				while( pArgStop < pArgsEnd ){` |
|     71221 | 14197 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14198 | `						iDepth++;` |
|     71216 | 14199 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14200 | `						iDepth--;` |
|     71206 | 14201 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14202 | `						break;` |
|         - | 14203 | `					}` |
|     71205 | 14204 | `					pArgStop++;` |
|         5 | 14205 | `				}` |
|      7965 | 14206 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7965 | 14207 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7960 | 14208 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7944 | 14209 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14210 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14211 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14212 | `					if( zN ){` |
|        19 | 14213 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14214 | `					}` |
|        19 | 14215 | `					pArgStart += 2;` |
|         9 | 14216 | `				}` |
|      7965 | 14217 | `				if( pArgStart < pArgStop ){` |
|         - | 14218 | `					SySet *pInstrContainer;` |
|      7965 | 14219 | `					pGen->pIn = pArgStart;` |
|      7965 | 14220 | `					pGen->pEnd = pArgStop;` |
|      7965 | 14221 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7965 | 14222 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7965 | 14223 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7965 | 14224 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7965 | 14225 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7965 | 14226 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14227 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14228 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14229 | `						return SXERR_ABORT;` |
|         - | 14230 | `					}` |
|      7965 | 14231 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3980 | 14232 | `				}` |
|      7965 | 14233 | `				pIn = pArgStop;` |
|      7965 | 14234 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14235 | `					pIn++;` |
|         8 | 14236 | `				}` |
|         5 | 14237 | `			}` |
|      7951 | 14238 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3973 | 14239 | `		}` |
|      8053 | 14240 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      8053 | 14241 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14242 | `			pIn++;` |
|         5 | 14243 | `			continue;` |
|         - | 14244 | `		}` |
|      8049 | 14245 | `		break;` |
|       ! 0 | 14246 | `	}` |
|      8049 | 14247 | `	pGen->pIn = pSavedIn;` |
|      8049 | 14248 | `	pGen->pEnd = pSavedEnd;` |
|      8049 | 14249 | `	return SXRET_OK;` |
|      4027 | 14250 | `}` |
|         - | 14251 | `/*` |
|         - | 14252 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14253 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14254 | ` */` |
|   3996084 | 14255 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14256 | `{` |
|   3996089 | 14257 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14258 | `	sxu32 n;` |
|         - | 14259 | `	sxi32 rc;` |
|   4004121 | 14260 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      8037 | 14261 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      8037 | 14262 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14263 | `			return SXERR_ABORT;` |
|         - | 14264 | `		}` |
|      4021 | 14265 | `	}` |
|   3996089 | 14266 | `	SySetReset(&pGen->aPendingAttrs);` |
|   3996089 | 14267 | `	return SXRET_OK;` |
|   1998047 | 14268 | `}` |
|         - | 14269 | `/*` |
|         - | 14270 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14271 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14272 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14273 | ` */` |
|   1712254 | 14274 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14275 | `{` |
|   1712259 | 14276 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   1712259 | 14277 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   1712259 | 14278 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14279 | `	sxu32 nIdx, n;` |
|         - | 14280 | `	sxi32 rc;` |
|   1712254 | 14281 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    197635 | 14282 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1514629 | 14283 | `		return SXRET_OK;` |
|         - | 14284 | `	}` |
|    197635 | 14285 | `	nIdx = (sxu32)(pTok - pBase);` |
|    592893 | 14286 | `	for( n = 0 ; n < nT ; n++ ){` |
|    395263 | 14287 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14288 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14289 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14290 | `				return SXERR_ABORT;` |
|         - | 14291 | `			}` |
|         6 | 14292 | `		}` |
|    197634 | 14293 | `	}` |
|    197635 | 14294 | `	return SXRET_OK;` |
|    856132 | 14295 | `}` |
|  10417536 | 14296 | `static sxi32 GenStateCompileChunk(` |
|         - | 14297 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14298 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14299 | `	)` |
|         5 | 14300 | `{` |
|         - | 14301 | `	ProcLangConstruct xCons;` |
|         - | 14302 | `	sxi32 rc;` |
|  10417541 | 14303 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   5999708 | 14304 | `	for(;;){` |
|  11208481 | 14305 | `		int bStmtIsDeclare = 0;` |
|  11208481 | 14306 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14307 | `			/* No more input to process */` |
|     69747 | 14308 | `			break;` |
|         - | 14309 | `		}` |
|         - | 14310 | `		/* Bind a directly-preceding docblock to this statement */` |
|  11138739 | 14311 | `		GenStateSetPendingDoc(&(*pGen));` |
|  11138739 | 14312 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14313 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14314 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14315 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14316 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14317 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7955 | 14318 | `			int bAttrTarget = 0;` |
|      7950 | 14319 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      4009 | 14320 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7897 | 14321 | `				bAttrTarget = 1;` |
|      4005 | 14322 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14323 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14324 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14325 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14326 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14327 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14328 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14329 | `					bAttrTarget = 1;` |
|        29 | 14330 | `				}` |
|        29 | 14331 | `			}` |
|      7955 | 14332 | `			if( !bAttrTarget ){` |
|       ! 0 | 14333 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14334 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14335 | `					&pGen->pIn->sData);` |
|       ! 0 | 14336 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14337 | `					break;` |
|         - | 14338 | `				}` |
|       ! 0 | 14339 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14340 | `			}` |
|      3975 | 14341 | `		}` |
|         - | 14342 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14343 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  11138739 | 14344 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6528123 | 14345 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   6528123 | 14346 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14347 | `				bStmtIsDeclare = 1;` |
|        21 | 14348 | `			}` |
|   3264059 | 14349 | `		}` |
|  11138739 | 14350 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14351 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14352 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    790913 | 14353 | `			pGen->bStrictTypesLocked = 1;` |
|    395454 | 14354 | `		}` |
|  11138739 | 14355 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14356 | `			/* Compile block */` |
|      3969 | 14357 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3969 | 14358 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14359 | `				break;` |
|         - | 14360 | `			}` |
|      1987 | 14361 | `		}else{` |
|  11134775 | 14362 | `			xCons = 0;` |
|  11134775 | 14363 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14364 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14365 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14366 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     35595 | 14367 | `				xCons = PH7_CompileClassModifiers;` |
|  11116980 | 14368 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14369 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14370 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3979 | 14371 | `				xCons = PH7_CompileEnum;` |
|  11097198 | 14372 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6492559 | 14373 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14374 | `				/* Try to extract a language construct handler */` |
|   6492559 | 14375 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   6492559 | 14376 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14377 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14378 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14379 | `						&pGen->pIn->sData);` |
|         9 | 14380 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14381 | `						break;` |
|         - | 14382 | `					}` |
|         - | 14383 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14384 | `					 * this erroneous statement.` |
|         - | 14385 | `					 */` |
|         9 | 14386 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14387 | `				}` |
|   7848934 | 14388 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    371623 | 14389 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14390 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14391 | `				xCons = PH7_CompileLabel;` |
|        56 | 14392 | `			}` |
|  11134775 | 14393 | `			if( xCons == 0 ){` |
|         - | 14394 | `				/* Assume an expression an try to compile it */` |
|   4642487 | 14395 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   4642487 | 14396 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14397 | `					/* Pop l-value */` |
|   4642337 | 14398 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2321166 | 14399 | `				}` |
|   2321246 | 14400 | `			}else{` |
|         - | 14401 | `				/* Go compile the sucker */` |
|   6492293 | 14402 | `				rc = xCons(&(*pGen));` |
|         - | 14403 | `			}` |
|  11134775 | 14404 | `			if( rc == SXERR_ABORT ){` |
|         - | 14405 | `				/* Request to abort compilation */` |
|        13 | 14406 | `				break;` |
|         - | 14407 | `			}` |
|         - | 14408 | `		}` |
|         - | 14409 | `		/* Ignore trailing semi-colons ';' */` |
|  19186991 | 14410 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   8048267 | 14411 | `			pGen->pIn++;` |
|         5 | 14412 | `		}` |
|  11138729 | 14413 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14414 | `			/* Compile a single statement and return */` |
|  10347789 | 14415 | `			break;` |
|         - | 14416 | `		}` |
|         - | 14417 | `		/* LOOP ONE */` |
|         - | 14418 | `		/* LOOP TWO */` |
|         - | 14419 | `		/* LOOP THREE */` |
|         - | 14420 | `		/* LOOP FOUR */` |
|         5 | 14421 | `	}` |
|         - | 14422 | `	/* Return compilation status */` |
|  10417541 | 14423 | `	return rc;` |
|         5 | 14424 | `}` |
|         - | 14425 | `/*` |
|         - | 14426 | ` * Compile a Raw PHP chunk.` |
|         - | 14427 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14428 | ` * takes care of generating the appropriate error message.` |
|         - | 14429 | ` */` |
|     69754 | 14430 | `static sxi32 PH7_CompilePHP(` |
|         - | 14431 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14432 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14433 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14434 | `	)` |
|         5 | 14435 | `{` |
|     69759 | 14436 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14437 | `	sxi32 rc;` |
|         - | 14438 | `	/* Reset the token set (and its trivia sidecar) */` |
|     69759 | 14439 | `	SySetReset(&(*pTokenSet));` |
|     69759 | 14440 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14441 | `	/* Mark as the default token set */` |
|     69759 | 14442 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14443 | `	/* Advance the stream cursor */` |
|     69759 | 14444 | `	pGen->pRawIn++;` |
|         - | 14445 | `	/* Tokenize the PHP chunk first */` |
|     69759 | 14446 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14447 | `	/* Point to the head and tail of the token stream. */` |
|     69759 | 14448 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     69759 | 14449 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     69759 | 14450 | `	if( is_expr ){` |
|       ! 0 | 14451 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14452 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14453 | `			/* A simple expression,compile it */` |
|       ! 0 | 14454 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14455 | `		}` |
|         - | 14456 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14457 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14458 | `		return SXRET_OK;` |
|         - | 14459 | `	}` |
|     69759 | 14460 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14461 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14462 | `		/*` |
|         - | 14463 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14464 | `		 * According to the PHP reference manual:` |
|         - | 14465 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14466 | `		 *  immediately follow` |
|         - | 14467 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14468 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14469 | `		 * Symisc extension:` |
|         - | 14470 | `		 *   This short syntax works with all PHP opening` |
|         - | 14471 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14472 | `		 *   only short tag.` |
|         - | 14473 | `		 */` |
|         - | 14474 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14475 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14476 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14477 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14478 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14479 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14480 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14481 | `		}` |
|         3 | 14482 | `		return SXRET_OK;` |
|         - | 14483 | `	}` |
|         - | 14484 | `	/* Compile the PHP chunk */` |
|     69757 | 14485 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14486 | `	/* Fix exceptions jumps */` |
|     69757 | 14487 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14488 | `	/* Fix gotos now, the jump destination is resolved */` |
|     69757 | 14489 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14490 | `		rc = SXERR_ABORT;` |
|         1 | 14491 | `	}` |
|         - | 14492 | `	/* Reset container */` |
|     69757 | 14493 | `	SySetReset(&pGen->aGoto);` |
|     69757 | 14494 | `	SySetReset(&pGen->aLabel);` |
|     69757 | 14495 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14496 | `	/* Compilation result */` |
|     69757 | 14497 | `	return rc;` |
|     34882 | 14498 | `}` |
|         - | 14499 | `/*` |
|         - | 14500 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14501 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14502 | ` * This is the only compile interface exported from this file.` |
|         - | 14503 | ` */` |
|     72826 | 14504 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14505 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14506 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14507 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14508 | `	)` |
|         5 | 14509 | `{` |
|         - | 14510 | `	SySet aPhpToken,aRawToken;` |
|         - | 14511 | `	ph7_gen_state *pCodeGen;` |
|         - | 14512 | `	ph7_value *pRawObj;` |
|         - | 14513 | `	sxu32 nObjIdx;` |
|         - | 14514 | `	sxi32 nRawObj;` |
|         - | 14515 | `	int is_expr;` |
|         - | 14516 | `	sxi8 bSavedStrict;` |
|         - | 14517 | `	sxi8 bSavedStrictLocked;` |
|         - | 14518 | `	sxi32 rc;` |
|     72831 | 14519 | `	if( pScript->nByte < 1 ){` |
|         - | 14520 | `		/* Nothing to compile */` |
|       ! 0 | 14521 | `		return PH7_OK;` |
|         - | 14522 | `	}` |
|         - | 14523 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14524 | `	 * file's flags so include/require restore them on return. */` |
|     72831 | 14525 | `	pCodeGen = &pVm->sCodeGen;` |
|     72831 | 14526 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     72831 | 14527 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     72831 | 14528 | `	pCodeGen->bStrictTypes = 0;` |
|     72831 | 14529 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14530 | `	/* Initialize the tokens containers */` |
|     72831 | 14531 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     72831 | 14532 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     72831 | 14533 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     72831 | 14534 | `	is_expr = 0;` |
|     72831 | 14535 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14536 | `		SyToken sTmp;` |
|         - | 14537 | `		/* PHP only: -*/` |
|     59307 | 14538 | `		sTmp.nLine = 1;` |
|     59307 | 14539 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     59307 | 14540 | `		sTmp.pUserData = 0;` |
|     59307 | 14541 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     59307 | 14542 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     59307 | 14543 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14544 | `			/* A simple PHP expression */` |
|       ! 0 | 14545 | `			is_expr = 1;` |
|       ! 0 | 14546 | `		}` |
|     29656 | 14547 | `	}else{` |
|         - | 14548 | `		/* Tokenize raw text */` |
|     13529 | 14549 | `		SySetAlloc(&aRawToken,32);` |
|     13529 | 14550 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|         - | 14551 | `	}` |
|         - | 14552 | `	/* Process high-level tokens */` |
|     72831 | 14553 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     72831 | 14554 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     72831 | 14555 | `	rc = PH7_OK;` |
|     72831 | 14556 | `	if( is_expr ){` |
|         - | 14557 | `		/* Compile the expression */` |
|       ! 0 | 14558 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14559 | `		goto cleanup;` |
|         - | 14560 | `	}` |
|     72831 | 14561 | `	nObjIdx = 0;` |
|         - | 14562 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14563 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14564 | `	 * preventing namespace bleeding across include()d files. */` |
|     72831 | 14565 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14566 | `	/* Start the compilation process */` |
|     43180 | 14567 | `	for(;;){` |
|    156107 | 14568 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     72819 | 14569 | `			break; /* No more tokens to process */` |
|         - | 14570 | `		}` |
|     83293 | 14571 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14572 | `			/* Compile the PHP chunk */` |
|     69759 | 14573 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     69759 | 14574 | `			if( rc == SXERR_ABORT ){` |
|        16 | 14575 | `				break;` |
|         - | 14576 | `			}` |
|     69747 | 14577 | `			continue;` |
|         - | 14578 | `		}` |
|         - | 14579 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13539 | 14580 | `		nRawObj = 0;` |
|     27073 | 14581 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14582 | `			/* Consume the raw chunk without any processing */` |
|     13539 | 14583 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13539 | 14584 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14585 | `				rc = SXERR_MEM;` |
|       ! 0 | 14586 | `				break;` |
|         - | 14587 | `			}` |
|         - | 14588 | `			/* Mark as constant and emit the load constant instruction */` |
|     13539 | 14589 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13539 | 14590 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13539 | 14591 | `			++nRawObj;` |
|     13539 | 14592 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14593 | `		}` |
|     13539 | 14594 | `		if( nRawObj > 0 ){` |
|         - | 14595 | `			/* Emit the consume instruction */` |
|     13539 | 14596 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6767 | 14597 | `		}` |
|     36418 | 14598 | `	}` |
|     36413 | 14599 | `cleanup:` |
|     72831 | 14600 | `	SySetRelease(&aRawToken);` |
|     72831 | 14601 | `	SySetRelease(&aPhpToken);` |
|         - | 14602 | `	/* Restore outer file's strict_types scope */` |
|     72831 | 14603 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     72831 | 14604 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     72831 | 14605 | `	return rc;` |
|     36418 | 14606 | `}` |
|         - | 14607 | `/*` |
|         - | 14608 | ` * Utility routines.Initialize the code generator.` |
|         - | 14609 | ` */` |
|      3946 | 14610 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 14611 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14612 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14613 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14614 | `	)` |
|         5 | 14615 | `{` |
|      3951 | 14616 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14617 | `	/* Zero the structure */` |
|      3951 | 14618 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 14619 | `	/* Initial state */` |
|      3951 | 14620 | `	pGen->pVm  = &(*pVm);` |
|      3951 | 14621 | `	pGen->xErr = xErr;` |
|      3951 | 14622 | `	pGen->pErrData = pErrData;` |
|      3951 | 14623 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3951 | 14624 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3951 | 14625 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3951 | 14626 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3951 | 14627 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3951 | 14628 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3951 | 14629 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 14630 | `	/* Error log buffer */` |
|      3951 | 14631 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 14632 | `	/* General purpose working buffer */` |
|      3951 | 14633 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 14634 | `	/* Namespace state */` |
|      3951 | 14635 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3951 | 14636 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3951 | 14637 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3951 | 14638 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14639 | `	/* Create the global scope */` |
|      3951 | 14640 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 14641 | `	/* Point to the global scope */` |
|      3951 | 14642 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3951 | 14643 | `	return SXRET_OK;` |
|         5 | 14644 | `}` |
|         - | 14645 | `/*` |
|         - | 14646 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 14647 | ` */` |
|     76384 | 14648 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 14649 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14650 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14651 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14652 | `	)` |
|         5 | 14653 | `{` |
|     76389 | 14654 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14655 | `	GenBlock *pBlock,*pParent;` |
|         - | 14656 | `	/* Reset state */` |
|     76389 | 14657 | `	SySetReset(&pGen->aLabel);` |
|     76389 | 14658 | `	SySetReset(&pGen->aGoto);` |
|     76389 | 14659 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     76389 | 14660 | `	SySetReset(&pGen->aTrivia);` |
|     76389 | 14661 | `	SySetReset(&pGen->aPendingAttrs);` |
|     76389 | 14662 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     76389 | 14663 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     76389 | 14664 | `	SyBlobRelease(&pGen->sWorker);` |
|     76389 | 14665 | `	SyBlobRelease(&pGen->sNamespace);` |
|     76389 | 14666 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     76389 | 14667 | `	SyHashRelease(&pGen->hUseImports);` |
|     76389 | 14668 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     76389 | 14669 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     76389 | 14670 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     76389 | 14671 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     76389 | 14672 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14673 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 14674 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 14675 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 14676 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 14677 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 14678 | `	 * number of unique names, which is acceptable. */` |
|         - | 14679 | `	/* Point to the global scope */` |
|     76389 | 14680 | `	pBlock = pGen->pCurrent;` |
|     76389 | 14681 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 14682 | `		pParent = pBlock->pParent;` |
|       ! 0 | 14683 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 14684 | `		pBlock = pParent;` |
|       ! 0 | 14685 | `	}` |
|     76389 | 14686 | `	pGen->xErr = xErr;` |
|     76389 | 14687 | `	pGen->pErrData = pErrData;` |
|     76389 | 14688 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     76389 | 14689 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     76389 | 14690 | `	pGen->pIn = pGen->pEnd = 0;` |
|     76389 | 14691 | `	pGen->nErr = 0;` |
|     76389 | 14692 | `	return SXRET_OK;` |
|         5 | 14693 | `}` |
|         - | 14694 | `/*` |
|         - | 14695 | ` * Generate a compile-time error message.` |
|         - | 14696 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 14697 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 14698 | ` * abort compilation immediately.` |
|         - | 14699 | ` */` |
|     16484 | 14700 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 14701 | `{` |
|     16489 | 14702 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     16489 | 14703 | `	const char *zErr = "Error";` |
|         - | 14704 | `	SyString *pFile;` |
|         - | 14705 | `	va_list ap;` |
|         - | 14706 | `	sxi32 rc;` |
|         - | 14707 | `	/* Reset the working buffer */` |
|     16489 | 14708 | `	SyBlobReset(pWorker);` |
|         - | 14709 | `	/* Peek the processed file path if available */` |
|     16489 | 14710 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     16489 | 14711 | `	if( nErrType == E_ERROR ){` |
|         - | 14712 | `		/* Increment the error counter */` |
|       551 | 14713 | `		pGen->nErr++;` |
|       551 | 14714 | `		if( pGen->nErr > 15 ){` |
|         - | 14715 | `			/* Error count limit reached */` |
|         6 | 14716 | `			if( pGen->xErr ){` |
|         6 | 14717 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 14718 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 14719 | `				if( pFile ){` |
|         6 | 14720 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 14721 | `				}` |
|         6 | 14722 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 14723 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 14724 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 14725 | `				}` |
|         2 | 14726 | `			}` |
|         - | 14727 | `			/* Abort immediately */` |
|         6 | 14728 | `			return SXERR_ABORT;` |
|         - | 14729 | `		}` |
|       271 | 14730 | `	}` |
|     16485 | 14731 | `	if( pGen->xErr == 0 ){` |
|         - | 14732 | `		/* No available error consumer,return immediately */` |
|     15791 | 14733 | `		return SXRET_OK;` |
|         - | 14734 | `	}` |
|       698 | 14735 | `	switch(nErrType){` |
|       544 | 14736 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        32 | 14737 | `	case E_WARNING: zErr = "Warning";     break;` |
|       116 | 14738 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|        11 | 14739 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 14740 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 14741 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 14742 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|         7 | 14743 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 14744 | `	default:` |
|       ! 0 | 14745 | `		break;` |
|         - | 14746 | `	}` |
|       698 | 14747 | `	rc = SXRET_OK;` |
|         - | 14748 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       698 | 14749 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       698 | 14750 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       698 | 14751 | `	va_start(ap,zFormat);` |
|       698 | 14752 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       698 | 14753 | `	va_end(ap);` |
|       698 | 14754 | `	if( pFile ){` |
|       698 | 14755 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       347 | 14756 | `	}` |
|         - | 14757 | `	/* Append a new line */` |
|       698 | 14758 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       698 | 14759 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 14760 | `		/* Consume the generated error message */` |
|       698 | 14761 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       347 | 14762 | `	}` |
|       698 | 14763 | `	return rc;` |
|      8247 | 14764 | `}` |
|         - | 14765 |  |
