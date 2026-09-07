# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7158/8861 lines (80.78%)

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
|         - |    42 | `	sxu32 nLoopId;       /* Innermost loop/switch enclosing this label (0 = none) */` |
|         - |    43 | `	sxu8 bRef;           /* True if the label was referenced */` |
|         - |    44 | `};` |
|         - |    45 | `/*` |
|         - |    46 | ` * Compilation of some PHP constructs such as if, for, while, the logical or` |
|         - |    47 | ` * (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |    48 | ` * generation of forward jumps.` |
|         - |    49 | ` * Since the destination PC target of these jumps isn't known when the jumps` |
|         - |    50 | ` * are emitted, we record each forward jump in an instance of the following` |
|         - |    51 | ` * structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |    52 | ` */` |
|         - |    53 | `struct JumpFixup` |
|         - |    54 | `{` |
|         - |    55 | `	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */` |
|         - |    56 | `	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */` |
|         - |    57 | `	/* The following fields are only used by the goto statement */` |
|         - |    58 | `	SyString sLabel;    /* Label name */` |
|         - |    59 | `	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */` |
|         - |    60 | `	sxu32 nLine;        /* Track line number */` |
|         - |    61 | `	sxu32 nLoopId;      /* Innermost loop/switch enclosing this goto (0 = none) */` |
|         - |    62 | `};` |
|         - |    63 | `/*` |
|         - |    64 | ` * Each language construct is represented by an instance` |
|         - |    65 | ` * of the following structure.` |
|         - |    66 | ` */` |
|         - |    67 | `struct LangConstruct` |
|         - |    68 | `{` |
|         - |    69 | `	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */` |
|         - |    70 | `	ProcLangConstruct xConstruct;  /* C function implementing the language construct */` |
|         - |    71 | `};` |
|         - |    72 | `/* Compilation flags */` |
|         - |    73 | `#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */` |
|         - |    74 | `/* Token stream synchronization macros */` |
|         - |    75 | `#define SWAP_TOKEN_STREAM(GEN,START,END)\` |
|         - |    76 | `	pTmp  = GEN->pEnd;\` |
|         - |    77 | `	pGen->pIn  = START;\` |
|         - |    78 | `	pGen->pEnd = END` |
|         - |    79 | `#define UPDATE_TOKEN_STREAM(GEN)\` |
|         - |    80 | `	if( GEN->pIn < pTmp ){\` |
|         - |    81 | `	    GEN->pIn++;\` |
|         - |    82 | `	}\` |
|         - |    83 | `	GEN->pEnd = pTmp` |
|         - |    84 | `#define SWAP_DELIMITER(GEN,START,END)\` |
|         - |    85 | `	pTmpIn  = GEN->pIn;\` |
|         - |    86 | `	pTmpEnd = GEN->pEnd;\` |
|         - |    87 | `	GEN->pIn = START;\` |
|         - |    88 | `	GEN->pEnd = END` |
|         - |    89 | `#define RE_SWAP_DELIMITER(GEN)\` |
|         - |    90 | `	GEN->pIn  = pTmpIn;\` |
|         - |    91 | `	GEN->pEnd = pTmpEnd` |
|         - |    92 | `/* Flags related to expression compilation */` |
|         - |    93 | `#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */` |
|         - |    94 | `#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */` |
|         - |    95 | `#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */` |
|         - |    96 | `#define EXPR_FLAG_LOAD_IDX_ISSET    0x008 /* LOAD_IDX argument is the LHS of isset() — emit iP2=4 (offsetExists) */` |
|         - |    97 | `#define EXPR_FLAG_LOAD_IDX_UNSET    0x010 /* LOAD_IDX argument is the LHS of unset() — emit iP2=5 (offsetUnset) */` |
|         - |    98 | `#define EXPR_FLAG_LOAD_IDX_EMPTY    0x020 /* LOAD_IDX argument is the LHS of empty() — emit iP2=6 (offsetExists+offsetGet) */` |
|         - |    99 | `#define EXPR_FLAG_MEMBER_WRITE      0x040 /* Sub-tree is the write lvalue of an assignment: tag a target` |
|         - |   100 | `                                           * OP_MEMBER iP2=PH7_MEMBER_WRITE so the VM auto-creates a missing` |
|         - |   101 | ``                                           * property (e.g. `$o->arr[$k] = v`, `$o->p ??= v`). Propagated`` |
|         - |   102 | `                                           * from the precedence-18 lvalue through SUBSCRIPT to the base` |
|         - |   103 | ``                                            * member; stripped when descending into an intermediate `->` `` |
|         - |   104 | `                                           * container (the container is read, not the write target). */` |
|         - |   105 | `/* Forward declaration */` |
|         - |   106 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|         - |   107 | `/*` |
|         - |   108 | ` * Local utility routines used in the code generation phase.` |
|         - |   109 | ` */` |
|         - |   110 | `/*` |
|         - |   111 | ` * Check if the given name refer to a valid label.` |
|         - |   112 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|         - |   113 | ` * Any other return value indicates no such label.` |
|         - |   114 | ` */` |
|       148 |   115 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|         5 |   116 | `{` |
|         - |   117 | `	Label *aLabel;` |
|         - |   118 | `	sxu32 n;` |
|         - |   119 | `	/* Perform a linear scan on the label table */` |
|       153 |   120 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|       333 |   121 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|       277 |   122 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   123 | `			/* Jump destination found */` |
|        97 |   124 | `			aLabel[n].bRef = TRUE;` |
|        97 |   125 | `			if( ppOut ){` |
|        97 |   126 | `				*ppOut = &aLabel[n];` |
|        46 |   127 | `			}` |
|        97 |   128 | `			return SXRET_OK;` |
|         - |   129 | `		}` |
|        92 |   130 | `	}` |
|         - |   131 | `	/* No such destination */` |
|        60 |   132 | `	return SXERR_NOTFOUND;` |
|        79 |   133 | `}` |
|         - |   134 | `/*` |
|         - |   135 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   136 | ` * compiled blocks.` |
|         - |   137 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   138 | ` */` |
|    114060 |   139 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   140 | `{` |
|    114065 |   141 | `	GenBlock *pBlock = pCurrent;` |
|    265939 |   142 | `	for(;;){` |
|    531883 |   143 | `		if( pBlock->iFlags & iBlockType ){` |
|    114065 |   144 | `			iCount--; /* Decrement nesting level */` |
|    114065 |   145 | `			if( iCount < 1 ){` |
|         - |   146 | `				/* Block meet with the desired criteria */` |
|    114039 |   147 | `				return pBlock;` |
|         - |   148 | `			}` |
|        13 |   149 | `		}` |
|         - |   150 | `		/* Point to the upper block */` |
|    417849 |   151 | `		pBlock = pBlock->pParent;` |
|    417849 |   152 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   153 | `			/* Forbidden */` |
|        17 |   154 | `			break;` |
|         - |   155 | `		}` |
|         5 |   156 | `	}` |
|         - |   157 | `	/* No such block */` |
|        30 |   158 | `	return 0;` |
|     57035 |   159 | `}` |
|         - |   160 | `/*` |
|         - |   161 | ` * Initialize a freshly allocated block instance.` |
|         - |   162 | ` */` |
|   9969738 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|   9969743 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|   9969743 |   173 | `	pBlock->pUserData   = pUserData;` |
|   9969743 |   174 | `	pBlock->pGen        = pGen;` |
|   9969743 |   175 | `	pBlock->iFlags      = iType;` |
|   9969743 |   176 | `	pBlock->pParent     = 0;` |
|   9969743 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   9969743 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   9969743 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|   9965942 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|   9965947 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|   9965947 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|   9965947 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|   9965947 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|   9965947 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|   9965947 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    361655 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    361655 |   214 | `		pGen->nLoopId++;` |
|    361655 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    361655 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    361655 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    361655 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    180825 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|   9965947 |   221 | `	pGen->pCurrent = pBlock;` |
|   9965947 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   4797459 |   224 | `		*ppBlock = pBlock;` |
|   2398727 |   225 | `	}` |
|   9965947 |   226 | `	return SXRET_OK;` |
|   4982976 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|   9965926 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|   9965931 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|   9965931 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|   9965931 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|   9965926 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|   9965931 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|   9965931 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|   9965931 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|   9965931 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|   9965926 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|   9965931 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   9965931 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|   9965931 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    361647 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    180821 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|   9965931 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|   9965931 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|   9965931 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|   9965931 |   268 | `	return SXRET_OK;` |
|   4982968 |   269 | `}` |
|         - |   270 | `/*` |
|         - |   271 | ` * Emit a forward jump.` |
|         - |   272 | ` * Notes on forward jumps` |
|         - |   273 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |   274 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |   275 | ` *  generation of forward jumps.` |
|         - |   276 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |   277 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |   278 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |   279 | ` */` |
|   3563414 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   3563419 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   3563419 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   3563419 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   3563419 |   289 | `	return rc;` |
|         5 |   290 | `}` |
|         - |   291 | `/*` |
|         - |   292 | ` * Fix a forward jump now the jump destination is resolved.` |
|         - |   293 | ` * Return the total number of fixed jumps.` |
|         - |   294 | ` * Notes on forward jumps:` |
|         - |   295 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |   296 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |   297 | ` *  generation of forward jumps.` |
|         - |   298 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |   299 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |   300 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|         - |   301 | ` */` |
|   6931864 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   6931869 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  14637533 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   7705669 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   2922895 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   4782779 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1219367 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   3563417 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   3563417 |   322 | `		if( pInstr ){` |
|   3563417 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   3563417 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   3563417 |   326 | `			aFix[n].nJumpType = -1;` |
|   1781706 |   327 | `		}` |
|   1781711 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   6931869 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2598874 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2598879 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2599025 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|       153 |   350 | `		pJump = &aJumps[n];` |
|         - |   351 | `		/* Extract the target label */` |
|       153 |   352 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|       153 |   353 | `		if( rc != SXRET_OK ){` |
|         - |   354 | `			/* No such label */` |
|        60 |   355 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        60 |   356 | `			if( rc == SXERR_ABORT ){` |
|         3 |   357 | `				return SXERR_ABORT;` |
|         - |   358 | `			}` |
|        58 |   359 | `			continue;` |
|         - |   360 | `		}` |
|         - |   361 | `		/* php's one goto restriction: you may not jump INTO a loop or a switch. The label` |
|         - |   362 | `		 * is inside one exactly when it carries a loop id; that is legal only if the same` |
|         - |   363 | `		 * loop also encloses the goto, i.e. the label's loop is the goto's loop or one of` |
|         - |   364 | `		 * its ancestors. Walk up from the goto's loop looking for the label's. */` |
|        97 |   365 | `		if( pLabel->nLoopId != 0 ){` |
|       ! 0 |   366 | `			sxu32 *aParent = (sxu32 *)SySetBasePtr(&pGen->aLoopParent);` |
|       ! 0 |   367 | `			sxu32 nCur = pJump->nLoopId;` |
|       ! 0 |   368 | `			int bInside = 0;` |
|       ! 0 |   369 | `			while( nCur != 0 ){` |
|       ! 0 |   370 | `				if( nCur == pLabel->nLoopId ){` |
|       ! 0 |   371 | `					bInside = 1;` |
|       ! 0 |   372 | `					break;` |
|         - |   373 | `				}` |
|       ! 0 |   374 | `				nCur = (nCur <= SySetUsed(&pGen->aLoopParent)) ? aParent[nCur - 1] : 0;` |
|       ! 0 |   375 | `			}` |
|       ! 0 |   376 | `			if( !bInside ){` |
|       ! 0 |   377 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |   378 | `					"'goto' into loop or switch statement is disallowed");` |
|       ! 0 |   379 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |   380 | `					return SXERR_ABORT;` |
|         - |   381 | `				}` |
|       ! 0 |   382 | `				continue;` |
|         - |   383 | `			}` |
|       ! 0 |   384 | `		}` |
|         - |   385 | `		/* Make sure the target label is reachable */` |
|        97 |   386 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|        12 |   387 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        12 |   388 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |   389 | `				return SXERR_ABORT;` |
|         - |   390 | `			}` |
|         4 |   391 | `		}` |
|         - |   392 | `		/* Fix the jump now the destination is resolved */` |
|        97 |   393 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|        97 |   394 | `		if( pInstr ){` |
|        97 |   395 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|        46 |   396 | `		}` |
|        51 |   397 | `	}` |
|         - |   398 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|         - |   399 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|   2598877 |   400 | `	return SXRET_OK;` |
|   1299442 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  13116346 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  13116351 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  13116351 |   409 | `	if( pEntry == 0 ){` |
|   3434543 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|   9681813 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   9681813 |   413 | `	return SXRET_OK;` |
|   6558178 |   414 | `}` |
|         - |   415 | `/*` |
|         - |   416 | ` * Install a given constant index in the literal table.` |
|         - |   417 | ` * In order to be installed, the ph7_value must be of type string.` |
|         - |   418 | ` *` |
|         - |   419 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|         - |   420 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|         - |   421 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|         - |   422 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|         - |   423 | ` * many "" literals appear in user code.` |
|         - |   424 | ` */` |
|   3434538 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3434543 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3434543 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1717269 |   429 | `	}` |
|   3434543 |   430 | `	return SXRET_OK;` |
|         5 |   431 | `}` |
|         - |   432 | `/*` |
|         - |   433 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   434 | ` * in the constant table.` |
|         - |   435 | ` */` |
|   2694920 |   436 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   437 | `{` |
|         - |   438 | `	ph7_value *pObj;` |
|   2694925 |   439 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   440 | `	/* Reserve a new constant */` |
|   2694925 |   441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   2694925 |   442 | `	if( pObj == 0 ){` |
|       ! 0 |   443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   444 | `		return 0;` |
|         - |   445 | `	}` |
|   2694925 |   446 | `	*pIdx = nIdx;` |
|         - |   447 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   448 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   449 | `	 */` |
|   2694925 |   450 | `	return pObj;` |
|   1347465 |   451 | `}` |
|         - |   452 | `/*` |
|         - |   453 | ` * Implementation of the PHP language constructs.` |
|         - |   454 | ` */` |
|         - |   455 | `/*` |
|         - |   456 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|         - |   457 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|         - |   458 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|         - |   459 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|         - |   460 | ` *` |
|         - |   461 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|         - |   462 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|         - |   463 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|         - |   464 | ` * surrounding callsites' zero-check fallback pattern.` |
|         - |   465 | ` */` |
|   6127882 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6127887 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3063946 |   478 | `}` |
|         - |   479 | `/* Forward declaration */` |
|         - |   480 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|         - |   481 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen);` |
|         - |   482 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut);` |
|         - |   483 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|         - |   484 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut);` |
|         - |   485 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut);` |
|         - |   486 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|         - |   487 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|         - |   488 | `/* Forward decl: union type parser is defined later in this file. */` |
|         - |   489 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |   490 | `	ph7_gen_state *pGen,` |
|         - |   491 | `	sxu32 *pnType,` |
|         - |   492 | `	SyString *pClass,` |
|         - |   493 | `	SySet *pAlts,` |
|         - |   494 | `	sxi32 *piTypeFlags,` |
|         - |   495 | `	SyString *pTypeText,` |
|         - |   496 | `	int iNullableFlag,` |
|         - |   497 | `	int iUnionFlag,` |
|         - |   498 | `	int bAllowVoid,` |
|         - |   499 | `	sxu32 nLine` |
|         - |   500 | `);` |
|         - |   501 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|         - |   502 | `static const char * TokenTypeName(sxu32 nType);` |
|         - |   503 | `/*` |
|         - |   504 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|         - |   505 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|         - |   506 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|         - |   507 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|         - |   508 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|         - |   509 | ` * for anything larger, so correctness is preserved even for pathological` |
|         - |   510 | ` * inputs like a thousand-digit number.` |
|         - |   511 | ` */` |
|         - |   512 | `#define GEN_NUM_SCRATCH 128` |
|         - |   513 | `/*` |
|         - |   514 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|         - |   515 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|         - |   516 | ` *   base  2 => 0 or 1` |
|         - |   517 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|         - |   518 | ` *              decimal scan in the lexer)` |
|         - |   519 | ` */` |
|      1076 |   520 | `static int GenStateIsBaseDigit(int c, int base)` |
|         5 |   521 | `{` |
|      1081 |   522 | `	if( base == 16 ){ return SyisHex(c); }` |
|       982 |   523 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|       703 |   524 | `	return SyisDigit(c);` |
|       543 |   525 | `}` |
|         - |   526 | `/*` |
|         - |   527 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|         - |   528 | ` * underscore separator so the caller can report the malformed portion with` |
|         - |   529 | ` * the exact wording PHP uses:` |
|         - |   530 | ` *` |
|         - |   531 | ` *   syntax error, unexpected identifier "X"` |
|         - |   532 | ` *` |
|         - |   533 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|         - |   534 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|         - |   535 | ` * absorbed by the lexer specifically to let this validator see and report` |
|         - |   536 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|         - |   537 | ` * no forward rescan needed.` |
|         - |   538 | ` *` |
|         - |   539 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|         - |   540 | ` * returns 0 when it is well-formed.` |
|         - |   541 | ` */` |
|   2695912 |   542 | `static int GenStateFindBadNumericSeparator(` |
|         - |   543 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   544 | `{` |
|   2695917 |   545 | `	const char *z = pRaw->zString;` |
|   2695917 |   546 | `	sxu32 n = pRaw->nByte;` |
|   2695917 |   547 | `	int base = 10;` |
|         - |   548 | `	sxu32 i, start;` |
|   2695917 |   549 | `	if( n < 2 ) return 0;` |
|    440815 |   550 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        80 |   551 | `		base = 16;` |
|    440776 |   552 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       284 |   553 | `		base = 2;` |
|       141 |   554 | `	}` |
|   1475597 |   555 | `	for( i = 0; i < n; ++i ){` |
|   1034801 |   556 | `		if( z[i] != '_' ) continue;` |
|       546 |   557 | `		if( i > 0 && i + 1 < n` |
|       543 |   558 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|       543 |   559 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|       533 |   560 | `			continue; /* well-placed separator */` |
|         - |   561 | `		}` |
|         - |   562 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|         - |   563 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|        18 |   564 | `		start = i;` |
|        23 |   565 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|        12 |   566 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|         6 |   567 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|         2 |   568 | `		}` |
|        18 |   569 | `		*pBadStart = &z[start];` |
|        18 |   570 | `		*pBadLen = n - start;` |
|        18 |   571 | `		return 1;` |
|       ! 0 |   572 | `	}` |
|    440801 |   573 | `	return 0;` |
|   1347961 |   574 | `}` |
|         - |   575 | `/*` |
|         - |   576 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   577 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   578 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   579 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   580 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   581 | ` * so callers can bail from the current construct).` |
|         - |   582 | ` */` |
|   2695912 |   583 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   584 | `{` |
|   2695917 |   585 | `	const char *zBad = 0;` |
|   2695917 |   586 | `	sxu32 nBad = 0;` |
|         - |   587 | `	SyString sBad;` |
|         - |   588 | `	sxi32 rc;` |
|   2695917 |   589 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   2695903 |   590 | `		return SXRET_OK;` |
|         - |   591 | `	}` |
|        18 |   592 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   593 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   594 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   595 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   596 | `		return SXERR_ABORT;` |
|         - |   597 | `	}` |
|        18 |   598 | `	return SXERR_SYNTAX;` |
|   1347961 |   599 | `}` |
|         - |   600 | `/*` |
|         - |   601 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|         - |   602 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|         - |   603 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|         - |   604 | ` *` |
|         - |   605 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|         - |   606 | ` * and *pzAlloc is set to NULL.` |
|         - |   607 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|         - |   608 | ` * and *pzAlloc is set to NULL.` |
|         - |   609 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|         - |   610 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|         - |   611 | ` * caller with SyMemBackendFree once the converter is done.` |
|         - |   612 | ` *` |
|         - |   613 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|         - |   614 | ` * case *pOut is left untouched and the caller must not read it).` |
|         - |   615 | ` */` |
|   2695898 |   616 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   617 | `	SyMemBackend *pAlloc,` |
|         - |   618 | `	const SyString *pToken,` |
|         - |   619 | `	char *zScratch, sxu32 nScratch,` |
|         - |   620 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   621 | `{` |
|         - |   622 | `	sxu32 i, j;` |
|   2695903 |   623 | `	int hasUnderscore = 0;` |
|         - |   624 | `	char *zBuf;` |
|   2695903 |   625 | `	*pzAlloc = 0;` |
|   5983721 |   626 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   3288075 |   627 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   1643914 |   628 | `	}` |
|   2695903 |   629 | `	if( !hasUnderscore ){` |
|   2695651 |   630 | `		SyStringDupPtr(pOut, pToken);` |
|   2695651 |   631 | `		return SXRET_OK;` |
|         - |   632 | `	}` |
|       253 |   633 | `	if( pToken->nByte <= nScratch ){` |
|       251 |   634 | `		zBuf = zScratch;` |
|       126 |   635 | `	}else{` |
|         3 |   636 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|         3 |   637 | `		if( zBuf == 0 ){` |
|       ! 0 |   638 | `			return SXERR_ABORT;` |
|         - |   639 | `		}` |
|         3 |   640 | `		*pzAlloc = zBuf;` |
|         - |   641 | `	}` |
|       253 |   642 | `	j = 0;` |
|      2895 |   643 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|      2643 |   644 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|      1322 |   645 | `	}` |
|       253 |   646 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|       253 |   647 | `	return SXRET_OK;` |
|   1347954 |   648 | `}` |
|         - |   649 | `/*` |
|         - |   650 | ` * Compile a numeric [i.e: integer or real] literal.` |
|         - |   651 | ` * Notes on the integer type.` |
|         - |   652 | ` *  According to the PHP language reference manual` |
|         - |   653 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|         - |   654 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|         - |   655 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|         - |   656 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|         - |   657 | ` * Symisc eXtension to the integer type.` |
|         - |   658 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|         - |   659 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|         - |   660 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|         - |   661 | ` *  [i.e: either 32bit or 64bit].` |
|         - |   662 | ` *  For more information on this powerfull extension please refer to the official` |
|         - |   663 | ` *  documentation.` |
|         - |   664 | ` */` |
|         - |   665 | `/*` |
|         - |   666 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|         - |   667 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|         - |   668 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|         - |   669 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|         - |   670 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|         - |   671 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|         - |   672 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|         - |   673 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|         - |   674 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|         - |   675 | ` * confidently classify, so the int path stays in charge.` |
|         - |   676 | ` *` |
|         - |   677 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|         - |   678 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|         - |   679 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|         - |   680 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|         - |   681 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|         - |   682 | ` * residual; matching php exactly would need a port of those functions.` |
|         - |   683 | ` */` |
|   2694954 |   684 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   685 | `{` |
|   2694959 |   686 | `	const char *z = pNum->zString;` |
|   2694959 |   687 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   688 | `	const char *p, *q;` |
|         - |   689 | `	int n;` |
|   2694959 |   690 | `	*pbDecimal = FALSE;` |
|   2694959 |   691 | `	if( z >= zEnd ){` |
|       ! 0 |   692 | `		return FALSE;` |
|         - |   693 | `	}` |
|   2694959 |   694 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|         - |   695 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|        77 |   696 | `		p = z + 2;` |
|        85 |   697 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|       493 |   698 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|        77 |   699 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|        71 |   700 | `			return FALSE;` |
|         - |   701 | `		}` |
|         7 |   702 | `		{ ph7_real dv = 0;` |
|       103 |   703 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|        97 |   704 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|        49 |   705 | `		  }` |
|         7 |   706 | `		  *pReal = dv;` |
|         - |   707 | `		}` |
|         7 |   708 | `		return TRUE;` |
|   2694883 |   709 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|         - |   710 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|       281 |   711 | `		p = z + 2;` |
|       329 |   712 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|      2149 |   713 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|       281 |   714 | `		if( n <= 63 ){` |
|       279 |   715 | `			return FALSE;` |
|         - |   716 | `		}` |
|         3 |   717 | `		{ ph7_real dv = 0;` |
|       195 |   718 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|       129 |   719 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|        65 |   720 | `		  }` |
|         3 |   721 | `		  *pReal = dv;` |
|         - |   722 | `		}` |
|         3 |   723 | `		return TRUE;` |
|   2694603 |   724 | `	}else if( z[0] == '0' ){` |
|         - |   725 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   726 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   727 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1019613 |   728 | `		p = z;` |
|   2039223 |   729 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1019841 |   730 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1019613 |   731 | `		if( n <= 21 ){` |
|   1019611 |   732 | `			return FALSE;` |
|         - |   733 | `		}` |
|         3 |   734 | `		{ ph7_real dv = 0;` |
|        47 |   735 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|        45 |   736 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|        23 |   737 | `		  }` |
|         3 |   738 | `		  *pReal = dv;` |
|         - |   739 | `		}` |
|         3 |   740 | `		return TRUE;` |
|         - |   741 | `	}` |
|         - |   742 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|         - |   743 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|         - |   744 | `	 * for php-exact rounding. */` |
|   1674995 |   745 | `	p = z;` |
|   1674995 |   746 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   3936733 |   747 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   1674995 |   748 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   749 | `		*pbDecimal = TRUE;` |
|        25 |   750 | `		return TRUE;` |
|         - |   751 | `	}` |
|   1674971 |   752 | `	return FALSE;` |
|   1347482 |   753 | `}` |
|   2695884 |   754 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   755 | `{` |
|   2695889 |   756 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   2695889 |   757 | `	sxu32 nIdx = 0;` |
|         - |   758 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   2695889 |   759 | `	char *zAlloc = 0;` |
|         - |   760 | `	SyString sNum;` |
|         - |   761 | `	sxi32 rc;` |
|   1347942 |   762 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   2695889 |   763 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   2695889 |   764 | `	if( rc != SXRET_OK ){` |
|        14 |   765 | `		return rc;` |
|         - |   766 | `	}` |
|   4043816 |   767 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1347937 |   768 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   2695879 |   769 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   770 | `		return SXERR_ABORT;` |
|         - |   771 | `	}` |
|   2695879 |   772 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   773 | `		ph7_value *pObj;` |
|         - |   774 | `		sxi64 iValue;` |
|   2694959 |   775 | `		ph7_real rOverflow = 0;` |
|   2694959 |   776 | `		int bDecimalOverflow = 0;` |
|   2694959 |   777 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|         - |   778 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|         - |   779 | `			 * float instead of wrapping/dropping digits. */` |
|        35 |   780 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        35 |   781 | `			if( pObj == 0 ){` |
|       ! 0 |   782 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   783 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   784 | `				return SXERR_ABORT;` |
|         - |   785 | `			}` |
|        35 |   786 | `			if( bDecimalOverflow ){` |
|         - |   787 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|        25 |   788 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|        25 |   789 | `				PH7_MemObjToReal(pObj);` |
|        13 |   790 | `			}else{` |
|        11 |   791 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|         - |   792 | `			}` |
|        18 |   793 | `		}else{` |
|   2694925 |   794 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   2694925 |   795 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   2694925 |   796 | `			if( pObj == 0 ){` |
|       ! 0 |   797 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   798 | `				return SXERR_ABORT;` |
|         - |   799 | `			}` |
|   2694925 |   800 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   801 | `		}` |
|   1347482 |   802 | `	}else{` |
|         - |   803 | `		/* Real number */` |
|         - |   804 | `		ph7_value *pObj;` |
|         - |   805 | `		/* Reserve a new constant */` |
|       925 |   806 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       925 |   807 | `		if( pObj == 0 ){` |
|       ! 0 |   808 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   809 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   810 | `			return SXERR_ABORT;` |
|         - |   811 | `		}` |
|       925 |   812 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       925 |   813 | `		PH7_MemObjToReal(pObj);` |
|         - |   814 | `	}` |
|   2695879 |   815 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   816 | `	/* Emit the load constant instruction */` |
|   2695879 |   817 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   818 | `	/* Node successfully compiled */` |
|   2695879 |   819 | `	return SXRET_OK;` |
|   1347947 |   820 | `}` |
|         - |   821 | `/*` |
|         - |   822 | ` * Compile a single quoted string.` |
|         - |   823 | ` * According to the PHP language reference manual:` |
|         - |   824 | ` *` |
|         - |   825 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|         - |   826 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|         - |   827 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|         - |   828 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|         - |   829 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|         - |   830 | ` *` |
|         - |   831 | ` */` |
|   4282938 |   832 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   833 | `{` |
|   4282943 |   834 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   835 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   836 | `	ph7_value *pObj;` |
|         - |   837 | `	sxu32 nIdx;` |
|   4282943 |   838 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   839 | `	/* Delimit the string */` |
|   4282943 |   840 | `	zIn  = pStr->zString;` |
|   4282943 |   841 | `	zEnd = &zIn[pStr->nByte];` |
|   4282943 |   842 | `	if( zIn >= zEnd ){` |
|         - |   843 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   844 | `		 * rather than reserving a new object each time. */` |
|    201401 |   845 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    201401 |   846 | `		return SXRET_OK;` |
|         - |   847 | `	}` |
|   4081547 |   848 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   849 | `		/* Already processed,emit the load constant instruction` |
|         - |   850 | `		 * and return.` |
|         - |   851 | `		 */` |
|   2380685 |   852 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2380685 |   853 | `		return SXRET_OK;` |
|         - |   854 | `	}` |
|         - |   855 | `	/* Reserve a new constant */` |
|   1700867 |   856 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1700867 |   857 | `	if( pObj == 0 ){` |
|       ! 0 |   858 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   859 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   860 | `		return SXERR_ABORT;` |
|         - |   861 | `	}` |
|   1700867 |   862 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   863 | `	/* Compile the node */` |
|   1742678 |   864 | `	for(;;){` |
|   3485361 |   865 | `		if( zIn >= zEnd ){` |
|         - |   866 | `			/* End of input */` |
|   1700867 |   867 | `			break;` |
|         - |   868 | `		}` |
|   1784499 |   869 | `		zCur = zIn;` |
|  36562203 |   870 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  34777709 |   871 | `			zIn++;` |
|         5 |   872 | `		}` |
|   1784499 |   873 | `		if( zIn > zCur ){` |
|         - |   874 | `			/* Append raw contents*/` |
|   1746509 |   875 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    873252 |   876 | `		}` |
|   1784499 |   877 | `		zIn++;` |
|   1784499 |   878 | `		if( zIn < zEnd ){` |
|    117827 |   879 | `			if( zIn[0] == '\\' ){` |
|         - |   880 | `				/* A literal backslash */` |
|     30407 |   881 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    102626 |   882 | `			}else if( zIn[0] == '\'' ){` |
|         - |   883 | `				/* A single quote */` |
|        11 |   884 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   885 | `			}else{` |
|         - |   886 | `				/* verbatim copy */` |
|     87415 |   887 | `				zIn--;` |
|     87415 |   888 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     87415 |   889 | `				zIn++;` |
|         - |   890 | `			}` |
|     58911 |   891 | `		}` |
|         - |   892 | `		/* Advance the stream cursor */` |
|   1784499 |   893 | `		zIn++;` |
|         5 |   894 | `	}` |
|         - |   895 | `	/* Emit the load constant instruction */` |
|   1700867 |   896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1700867 |   897 | `	if( pStr->nByte < 1024 ){` |
|         - |   898 | `		/* Install in the literal table */` |
|   1700867 |   899 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    850431 |   900 | `	}` |
|         - |   901 | `	/* Node successfully compiled */` |
|   1700867 |   902 | `	return SXRET_OK;` |
|   2141474 |   903 | `}` |
|         - |   904 | `/*` |
|         - |   905 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|         - |   906 | ` *` |
|         - |   907 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|         - |   908 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|         - |   909 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|         - |   910 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|         - |   911 | ` * original source buffer — the buffer is stable through compilation.` |
|         - |   912 | ` *` |
|         - |   913 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|         - |   914 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|         - |   915 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|         - |   916 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|         - |   917 | ` *     at least N)" — line too short, or first differing byte is not` |
|         - |   918 | ` *     whitespace.` |
|         - |   919 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|         - |   920 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|         - |   921 | ` */` |
|       114 |   922 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|         4 |   923 | `{` |
|       118 |   924 | `	SyString *pIn = &pGen->pIn->sData;` |
|       118 |   925 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - |   926 | `	const char *zPrefix;` |
|         - |   927 | `	const char *z, *zEnd;` |
|         - |   928 | `	char *zBuf, *zDst;` |
|       118 |   929 | `	if( nIndent == 0 ){` |
|         - |   930 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|        73 |   931 | `		*pOut = *pIn;` |
|        73 |   932 | `		return SXRET_OK;` |
|         - |   933 | `	}` |
|         - |   934 | `	/* Recover the marker indent prefix from the original source buffer.` |
|         - |   935 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|         - |   936 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|         - |   937 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|         - |   938 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|         - |   939 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|        47 |   940 | `	zPrefix = pIn->zString + pIn->nByte;` |
|        47 |   941 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|       ! 0 |   942 | `		zPrefix += 2;` |
|       ! 0 |   943 | `	}else{` |
|        47 |   944 | `		zPrefix += 1;` |
|         - |   945 | `	}` |
|         - |   946 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|        47 |   947 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|        47 |   948 | `	if( zBuf == 0 ){` |
|       ! 0 |   949 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |   950 | `		return SXERR_ABORT;` |
|         - |   951 | `	}` |
|        47 |   952 | `	zDst = zBuf;` |
|        47 |   953 | `	z = pIn->zString;` |
|        47 |   954 | `	zEnd = z + pIn->nByte;` |
|       129 |   955 | `	while( z < zEnd ){` |
|        71 |   956 | `		const char *zLine = z;` |
|         - |   957 | `		sxu32 nLine;` |
|         - |   958 | `		int bEmpty;` |
|       799 |   959 | `		while( z < zEnd && z[0] != '\n' ){` |
|       731 |   960 | `			z++;` |
|         3 |   961 | `		}` |
|        71 |   962 | `		nLine = (sxu32)(z - zLine);` |
|        71 |   963 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|        71 |   964 | `		if( !bEmpty ){` |
|         - |   965 | `			sxu32 i;` |
|        67 |   966 | `			if( nLine < nIndent ){` |
|       ! 0 |   967 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   968 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       ! 0 |   969 | `					nIndent);` |
|       ! 0 |   970 | `				return SXERR_ABORT;` |
|         - |   971 | `			}` |
|       269 |   972 | `			for( i = 0; i < nIndent; i++ ){` |
|       213 |   973 | `				if( zLine[i] != zPrefix[i] ){` |
|        10 |   974 | `					unsigned char c = (unsigned char)zLine[i];` |
|        10 |   975 | `					if( c == ' ' \|\| c == '\t' ){` |
|         5 |   976 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   977 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|         3 |   978 | `					}else{` |
|         7 |   979 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   980 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|         2 |   981 | `							nIndent);` |
|         - |   982 | `					}` |
|        10 |   983 | `					return SXERR_ABORT;` |
|         - |   984 | `				}` |
|       103 |   985 | `			}` |
|        57 |   986 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|        57 |   987 | `			zDst += nLine - nIndent;` |
|        33 |   988 | `		}else if( nLine == 1 ){` |
|         - |   989 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|       ! 0 |   990 | `			*zDst++ = '\r';` |
|       ! 0 |   991 | `		}` |
|        61 |   992 | `		if( z < zEnd ){` |
|        25 |   993 | `			*zDst++ = '\n';` |
|        25 |   994 | `			z++;` |
|        12 |   995 | `		}` |
|         1 |   996 | `	}` |
|        37 |   997 | `	pOut->zString = zBuf;` |
|        37 |   998 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|        37 |   999 | `	return SXRET_OK;` |
|        61 |  1000 | `}` |
|         - |  1001 | `/*` |
|         - |  1002 | ` * Compile a nowdoc string.` |
|         - |  1003 | ` * According to the PHP language reference manual:` |
|         - |  1004 | ` *` |
|         - |  1005 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - |  1006 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - |  1007 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|         - |  1008 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|         - |  1009 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|         - |  1010 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|         - |  1011 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|         - |  1012 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|         - |  1013 | ` *  of the closing identifier.` |
|         - |  1014 | ` */` |
|        48 |  1015 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         3 |  1016 | `{` |
|         - |  1017 | `	SyString sStripped;` |
|         - |  1018 | `	SyString *pStr;` |
|         - |  1019 | `	ph7_value *pObj;` |
|         - |  1020 | `	sxu32 nIdx;` |
|         - |  1021 | `	sxi32 rc;` |
|        51 |  1022 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        51 |  1023 | `	if( rc != SXRET_OK ){` |
|         6 |  1024 | `		return rc;` |
|         - |  1025 | `	}` |
|        46 |  1026 | `	pStr = &sStripped;` |
|        46 |  1027 | `	nIdx = 0; /* Prevent compiler warning */` |
|        46 |  1028 | `	if( pStr->nByte <= 0 ){` |
|         - |  1029 | `		/* Empty string,load NULL */` |
|         7 |  1030 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |  1031 | `		return SXRET_OK;` |
|         - |  1032 | `	}` |
|         - |  1033 | `	/* Reserve a new constant */` |
|        40 |  1034 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        40 |  1035 | `	if( pObj == 0 ){` |
|       ! 0 |  1036 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1037 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  1038 | `		return SXERR_ABORT;` |
|         - |  1039 | `	}` |
|         - |  1040 | `	/* No processing is done here, simply a memcpy() operation */` |
|        40 |  1041 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|         - |  1042 | `	/* Emit the load constant instruction */` |
|        40 |  1043 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  1044 | `	/* Node successfully compiled */` |
|        40 |  1045 | `	return SXRET_OK;` |
|        27 |  1046 | `}` |
|         - |  1047 | `/*` |
|         - |  1048 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|         - |  1049 | ` * According to the PHP language reference manual` |
|         - |  1050 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|         - |  1051 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|         - |  1052 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|         - |  1053 | ` *  property in a string with a minimum of effort.` |
|         - |  1054 | ` *  Simple syntax` |
|         - |  1055 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|         - |  1056 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|         - |  1057 | ` *   the end of the name.` |
|         - |  1058 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|         - |  1059 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|         - |  1060 | ` *   as to simple variables.` |
|         - |  1061 | ` *  Complex (curly) syntax` |
|         - |  1062 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|         - |  1063 | ` *   of complex expressions.` |
|         - |  1064 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|         - |  1065 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|         - |  1066 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|         - |  1067 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|         - |  1068 | ` */` |
|      2568 |  1069 | `static sxi32 GenStateProcessStringExpression(` |
|         - |  1070 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1071 | `	sxu32 nLine,         /* Line number */` |
|         - |  1072 | `	const char *zIn,     /* Raw expression */` |
|         - |  1073 | `	const char *zEnd     /* End of the expression */` |
|         - |  1074 | `	)` |
|         5 |  1075 | `{` |
|         - |  1076 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1077 | `	SySet sToken;` |
|         - |  1078 | `	sxi32 rc;` |
|         - |  1079 | `	/* Initialize the token set */` |
|      2573 |  1080 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1081 | `	/* Preallocate some slots */` |
|      2573 |  1082 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1083 | `	/* Tokenize the text */` |
|      2573 |  1084 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1085 | `	/* Swap delimiter */` |
|      2573 |  1086 | `	pTmpIn  = pGen->pIn;` |
|      2573 |  1087 | `	pTmpEnd = pGen->pEnd;` |
|      2573 |  1088 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2573 |  1089 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1090 | `	/* Compile the expression */` |
|      2573 |  1091 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1092 | `	/* Restore token stream */` |
|      2573 |  1093 | `	pGen->pIn  = pTmpIn;` |
|      2573 |  1094 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1095 | `	/* Release the token set */` |
|      2573 |  1096 | `	SySetRelease(&sToken);` |
|         - |  1097 | `	/* Compilation result */` |
|      2573 |  1098 | `	return rc;` |
|         5 |  1099 | `}` |
|         - |  1100 | `/*` |
|         - |  1101 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1102 | ` */` |
|     81772 |  1103 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1104 | `{` |
|         - |  1105 | `	ph7_value *pConstObj;` |
|     81777 |  1106 | `	sxu32 nIdx = 0;` |
|         - |  1107 | `	/* Reserve a new constant */` |
|     81777 |  1108 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     81777 |  1109 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1110 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1111 | `		return 0;` |
|         - |  1112 | `	}` |
|     81777 |  1113 | `	(*pCount)++;` |
|     81777 |  1114 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1115 | `	/* Emit the load constant instruction */` |
|     81777 |  1116 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     81777 |  1117 | `	return pConstObj;` |
|     40891 |  1118 | `}` |
|         - |  1119 | `/*` |
|         - |  1120 | ` * Compile a double quoted/heredoc string.` |
|         - |  1121 | ` * According to the PHP language reference manual` |
|         - |  1122 | ` * Heredoc` |
|         - |  1123 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - |  1124 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - |  1125 | ` *  to close the quotation.` |
|         - |  1126 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - |  1127 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - |  1128 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - |  1129 | ` *  Warning` |
|         - |  1130 | ` *  It is very important to note that the line with the closing identifier must contain` |
|         - |  1131 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|         - |  1132 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|         - |  1133 | ` *  It's also important to realize that the first character before the closing identifier must` |
|         - |  1134 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|         - |  1135 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|         - |  1136 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|         - |  1137 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|         - |  1138 | ` *  the end of the current file, a parse error will result at the last line.` |
|         - |  1139 | ` *  Heredocs can not be used for initializing class properties.` |
|         - |  1140 | ` * Double quoted` |
|         - |  1141 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|         - |  1142 | ` *  Escaped characters Sequence 	Meaning` |
|         - |  1143 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|         - |  1144 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|         - |  1145 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|         - |  1146 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|         - |  1147 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|         - |  1148 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|         - |  1149 | ` *  \\ backslash` |
|         - |  1150 | ` *  \$ dollar sign` |
|         - |  1151 | ` *  \" double-quote` |
|         - |  1152 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|         - |  1153 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|         - |  1154 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|         - |  1155 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|         - |  1156 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|         - |  1157 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|         - |  1158 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|         - |  1159 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|         - |  1160 | ` * See string parsing for details.` |
|         - |  1161 | ` */` |
|         - |  1162 | `/*` |
|         - |  1163 | ` * Line number of an escape sequence inside the string body being compiled:` |
|         - |  1164 | ` * the token's line plus every newline before the escape (php reports the` |
|         - |  1165 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|         - |  1166 | ` * on the line after the '<<<' marker, hence the +1.` |
|         - |  1167 | ` */` |
|         6 |  1168 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|         3 |  1169 | `{` |
|         9 |  1170 | `	const char *z = pGen->pIn->sData.zString;` |
|         9 |  1171 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|        15 |  1172 | `	for( ; z < zPos ; z++ ){` |
|         9 |  1173 | `		if( z[0] == '\n' ){` |
|       ! 0 |  1174 | `			nLine++;` |
|       ! 0 |  1175 | `		}` |
|         6 |  1176 | `	}` |
|         9 |  1177 | `	return nLine;` |
|         3 |  1178 | `}` |
|         - |  1179 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|         - |  1180 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|     80212 |  1181 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1182 | `{` |
|     80217 |  1183 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1184 | `	const char *zIn,*zCur,*zEnd;` |
|     80217 |  1185 | `	ph7_value *pObj = 0;` |
|         - |  1186 | `	sxi32 iCons;` |
|         - |  1187 | `	sxi32 rc;` |
|         - |  1188 | `	/* Delimit the string */` |
|     80217 |  1189 | `	zIn  = pStr->zString;` |
|     80217 |  1190 | `	zEnd = &zIn[pStr->nByte];` |
|     80217 |  1191 | `	if( zIn >= zEnd ){` |
|         - |  1192 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1193 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1194 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1195 | `		 */` |
|       381 |  1196 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       381 |  1197 | `		return SXRET_OK;` |
|         - |  1198 | `	}` |
|     79841 |  1199 | `	zCur = 0;` |
|         - |  1200 | `	/* Compile the node */` |
|     79841 |  1201 | `	iCons = 0;` |
|     41200 |  1202 | `	for(;;){` |
|    114603 |  1203 | `		zCur = zIn;` |
|   1534035 |  1204 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1422005 |  1205 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        72 |  1206 | `				break;` |
|   1421872 |  1207 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2440 |  1208 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1220 |  1209 | `					break;` |
|         - |  1210 | `			}` |
|   1419437 |  1211 | `			zIn++;` |
|         5 |  1212 | `		}` |
|    114603 |  1213 | `		if( zIn > zCur ){` |
|     55471 |  1214 | `			if( pObj == 0 ){` |
|     54877 |  1215 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     54877 |  1216 | `				if( pObj == 0 ){` |
|       ! 0 |  1217 | `					return SXERR_ABORT;` |
|         - |  1218 | `				}` |
|     27436 |  1219 | `			}` |
|     55471 |  1220 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     27733 |  1221 | `		}` |
|    114603 |  1222 | `		if( zIn >= zEnd ){` |
|     79839 |  1223 | `			break;` |
|         - |  1224 | `		}` |
|     34769 |  1225 | `		if( zIn[0] == '\\' ){` |
|     32201 |  1226 | `			const char *zPtr = 0;` |
|         - |  1227 | `			sxu32 n;` |
|     32201 |  1228 | `			zIn++;` |
|     32201 |  1229 | `			if( pObj == 0 ){` |
|     26905 |  1230 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     26905 |  1231 | `				if( pObj == 0 ){` |
|       ! 0 |  1232 | `					return SXERR_ABORT;` |
|         - |  1233 | `				}` |
|     13450 |  1234 | `			}` |
|     32201 |  1235 | `			if( zIn >= zEnd ){` |
|         - |  1236 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1237 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1238 | `				break;` |
|         - |  1239 | `			}` |
|     32199 |  1240 | `			n = sizeof(char); /* size of conversion */` |
|     32199 |  1241 | `			switch( zIn[0] ){` |
|        11 |  1242 | `			case '$':` |
|         - |  1243 | `				/* Dollar sign */` |
|        25 |  1244 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        25 |  1245 | `				break;` |
|        52 |  1246 | `			case '\\':` |
|         - |  1247 | `				/* A literal backslash */` |
|       109 |  1248 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       109 |  1249 | `				break;` |
|         1 |  1250 | `			case 'e':` |
|         - |  1251 | `				/* Escape (ESC) ASCII code 27 */` |
|         3 |  1252 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|         3 |  1253 | `				break;` |
|         4 |  1254 | `			case 'f':` |
|         - |  1255 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|         9 |  1256 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|         9 |  1257 | `				break;` |
|     13606 |  1258 | `			case 'n':` |
|         - |  1259 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     27217 |  1260 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     27217 |  1261 | `				break;` |
|        27 |  1262 | `			case 'r':` |
|         - |  1263 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1264 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1265 | `				break;` |
|      1929 |  1266 | `			case 't':` |
|         - |  1267 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      3863 |  1268 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      3863 |  1269 | `				break;` |
|         3 |  1270 | `			case 'v':` |
|         - |  1271 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|         7 |  1272 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|         7 |  1273 | `				break;` |
|       141 |  1274 | `			case '"':` |
|       287 |  1275 | `				if( bHeredoc ){` |
|         - |  1276 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|         5 |  1277 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|         3 |  1278 | `				}else{` |
|         - |  1279 | `					/* Double quote */` |
|       283 |  1280 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|         - |  1281 | `				}` |
|       287 |  1282 | `				break;` |
|        25 |  1283 | `			case '0': case '1': case '2': case '3':` |
|         - |  1284 | `			case '4': case '5': case '6': case '7': {` |
|         - |  1285 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|         - |  1286 | `				 * warns and wraps to the low byte, matching php 8. */` |
|        52 |  1287 | `				int c = 0;` |
|         - |  1288 | `				char cOut;` |
|       148 |  1289 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|       126 |  1290 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|        15 |  1291 | `						break;` |
|         - |  1292 | `					}` |
|        98 |  1293 | `					c = c * 8 + (zPtr[0] - '0');` |
|        50 |  1294 | `				}` |
|        52 |  1295 | `				if( c > 0xFF ){` |
|         - |  1296 | `					SyString sSeq;` |
|         3 |  1297 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|         3 |  1298 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1299 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|         3 |  1300 | `					c &= 0xFF;` |
|         1 |  1301 | `				}` |
|        52 |  1302 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|        52 |  1303 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|        52 |  1304 | `				n = (sxu32)(zPtr-zIn);` |
|        52 |  1305 | `				break;` |
|         - |  1306 | `			}` |
|       273 |  1307 | `			case 'x':` |
|       818 |  1308 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|         - |  1309 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|       543 |  1310 | `					int c = SyHexToint(zIn[1]);` |
|         - |  1311 | `					char cOut;` |
|       543 |  1312 | `					n += sizeof(char);` |
|       543 |  1313 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|       539 |  1314 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|       539 |  1315 | `						n += sizeof(char);` |
|       269 |  1316 | `					}` |
|       543 |  1317 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|       543 |  1318 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       272 |  1319 | `				}else{` |
|         - |  1320 | `					/* Not an escape: keep the backslash, as php does */` |
|         5 |  1321 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|         - |  1322 | `				}` |
|       547 |  1323 | `				break;` |
|         9 |  1324 | `			case 'u':` |
|        18 |  1325 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|        22 |  1326 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|         - |  1327 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|         - |  1328 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|         - |  1329 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|         - |  1330 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|         - |  1331 | `					 * followed by {$...} curly interpolation. */` |
|        15 |  1332 | `					sxu32 nCp = 0;` |
|        15 |  1333 | `					zPtr = &zIn[2];` |
|        59 |  1334 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|        46 |  1335 | `						if( nCp <= 0x10FFFF ){` |
|         - |  1336 | `							/* stop accumulating once out of range: keeps a long` |
|         - |  1337 | `							 * digit run from wrapping sxu32 */` |
|        46 |  1338 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|        22 |  1339 | `						}` |
|        46 |  1340 | `						zPtr++;` |
|         2 |  1341 | `					}` |
|        15 |  1342 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|         - |  1343 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|         - |  1344 | `						 * malformed sequence so later errors are still reported. */` |
|         3 |  1345 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1346 | `							"Invalid UTF-8 codepoint escape sequence");` |
|         3 |  1347 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1348 | `							return SXERR_ABORT;` |
|         - |  1349 | `						}` |
|         3 |  1350 | `						n = (sxu32)(zPtr-zIn);` |
|         3 |  1351 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|         3 |  1352 | `							n += sizeof(char);` |
|         1 |  1353 | `						}` |
|         3 |  1354 | `						break;` |
|         - |  1355 | `					}` |
|        12 |  1356 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|        12 |  1357 | `					if( nCp > 0x10FFFF ){` |
|         3 |  1358 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1359 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|         3 |  1360 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1361 | `							return SXERR_ABORT;` |
|         - |  1362 | `						}` |
|         3 |  1363 | `						break;` |
|         - |  1364 | `					}` |
|         - |  1365 | `					{` |
|         - |  1366 | `						char zUtf[4];` |
|         9 |  1367 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|         9 |  1368 | `						SX_WRITE_UTF8(zOut,nCp);` |
|         9 |  1369 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|         - |  1370 | `					}` |
|         5 |  1371 | `				}else{` |
|         - |  1372 | `					/* Not an escape: keep the backslash, as php does */` |
|         7 |  1373 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|         - |  1374 | `				}` |
|        15 |  1375 | `				break;` |
|        16 |  1376 | `			default:` |
|         - |  1377 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|         - |  1378 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|         - |  1379 | `				 * in the source buffer — one batched append. */` |
|        33 |  1380 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|        32 |  1381 | `				break;` |
|         - |  1382 | `			}` |
|         - |  1383 | `			/* Advance the stream cursor */` |
|     32199 |  1384 | `			zIn += n;` |
|     32199 |  1385 | `			continue;` |
|         - |  1386 | `		}` |
|      2573 |  1387 | `		if( zIn[0] == '{' ){` |
|         - |  1388 | `			/* Curly syntax */` |
|         - |  1389 | `			const char *zExpr;` |
|       141 |  1390 | `			sxi32 iNest = 1;` |
|       141 |  1391 | `			zIn++;` |
|       141 |  1392 | `			zExpr = zIn;` |
|         - |  1393 | `			/* Synchronize with the next closing curly braces */` |
|      1419 |  1394 | `			while( zIn < zEnd ){` |
|      1419 |  1395 | `				if( zIn[0] == '{' ){` |
|         - |  1396 | `					/* Increment nesting level */` |
|         3 |  1397 | `					iNest++;` |
|      1418 |  1398 | `				}else if(zIn[0] == '}' ){` |
|         - |  1399 | `					/* Decrement nesting level */` |
|       143 |  1400 | `					iNest--;` |
|       143 |  1401 | `					if( iNest <= 0 ){` |
|       141 |  1402 | `						break;` |
|         - |  1403 | `					}` |
|         1 |  1404 | `				}` |
|      1281 |  1405 | `				zIn++;` |
|         3 |  1406 | `			}` |
|         - |  1407 | `			/* Process the expression */` |
|       141 |  1408 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|       141 |  1409 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1410 | `				return SXERR_ABORT;` |
|         - |  1411 | `			}` |
|       141 |  1412 | `			if( rc != SXERR_EMPTY ){` |
|       141 |  1413 | `				++iCons;` |
|        69 |  1414 | `			}` |
|       141 |  1415 | `			if( zIn < zEnd ){` |
|         - |  1416 | `				/* Jump the trailing curly */` |
|       141 |  1417 | `				zIn++;` |
|        69 |  1418 | `			}` |
|        72 |  1419 | `		}else{` |
|         - |  1420 | `			/* Simple syntax */` |
|      2435 |  1421 | `			const char *zExpr = zIn;` |
|         - |  1422 | `			/* Assemble variable name */` |
|      1240 |  1423 | `			for(;;){` |
|         - |  1424 | `				/* Jump leading dollars */` |
|      4915 |  1425 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2435 |  1426 | `					zIn++;` |
|         5 |  1427 | `				}` |
|      1240 |  1428 | `				for(;;){` |
|     12817 |  1429 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9097 |  1430 | `						zIn++;` |
|         5 |  1431 | `					}` |
|      2485 |  1432 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1433 | `						/* UTF-8 stream */` |
|       ! 0 |  1434 | `						zIn++;` |
|       ! 0 |  1435 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1436 | `							zIn++;` |
|       ! 0 |  1437 | `						}` |
|       ! 0 |  1438 | `						continue;` |
|         - |  1439 | `					}` |
|      2485 |  1440 | `					break;` |
|       ! 0 |  1441 | `				}` |
|      2485 |  1442 | `				if( zIn >= zEnd ){` |
|       263 |  1443 | `					break;` |
|         - |  1444 | `				}` |
|      2227 |  1445 | `				if( zIn[0] == '[' ){` |
|        12 |  1446 | `					sxi32 iSquare = 1;` |
|        12 |  1447 | `					zIn++;` |
|        28 |  1448 | `					while( zIn < zEnd ){` |
|        28 |  1449 | `						if( zIn[0] == '[' ){` |
|       ! 0 |  1450 | `							iSquare++;` |
|        28 |  1451 | `						}else if (zIn[0] == ']' ){` |
|        12 |  1452 | `							iSquare--;` |
|        12 |  1453 | `							if( iSquare <= 0 ){` |
|        12 |  1454 | `								break;` |
|         - |  1455 | `							}` |
|       ! 0 |  1456 | `						}` |
|        18 |  1457 | `						zIn++;` |
|         2 |  1458 | `					}` |
|        12 |  1459 | `					if( zIn < zEnd ){` |
|        12 |  1460 | `						zIn++;` |
|         5 |  1461 | `					}` |
|        12 |  1462 | `					break;` |
|      2217 |  1463 | `				}else if(zIn[0] == '{' ){` |
|         6 |  1464 | `					sxi32 iCurly = 1;` |
|         6 |  1465 | `					zIn++;` |
|        18 |  1466 | `					while( zIn < zEnd ){` |
|        16 |  1467 | `						if( zIn[0] == '{' ){` |
|       ! 0 |  1468 | `							iCurly++;` |
|        16 |  1469 | `						}else if (zIn[0] == '}' ){` |
|         3 |  1470 | `							iCurly--;` |
|         3 |  1471 | `							if( iCurly <= 0 ){` |
|         3 |  1472 | `								break;` |
|         - |  1473 | `							}` |
|       ! 0 |  1474 | `						}` |
|        14 |  1475 | `						zIn++;` |
|         2 |  1476 | `					}` |
|         6 |  1477 | `					if( zIn < zEnd ){` |
|         3 |  1478 | `						zIn++;` |
|         1 |  1479 | `					}` |
|         6 |  1480 | `					break;` |
|      2213 |  1481 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1482 | `					/* Member access operator '->' */` |
|        53 |  1483 | `					zIn += 2;` |
|      2188 |  1484 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1485 | `					/* Static member access operator '::' */` |
|       ! 0 |  1486 | `					zIn += 2;` |
|       ! 0 |  1487 | `				}else{` |
|      1084 |  1488 | `					break;` |
|         - |  1489 | `				}` |
|         3 |  1490 | `			}` |
|         - |  1491 | `			/*` |
|         - |  1492 | `			 * "$a[name]" — php's SIMPLE syntax takes an unquoted subscript as the string key` |
|         - |  1493 | `			 * 'name', never as a constant. PH7 handed "$a[name]" straight to the expression` |
|         - |  1494 | `			 * compiler, where the bare word only resolved because an unknown constant used to` |
|         - |  1495 | `			 * fall back to its own name as a string. With undefined constants now a real` |
|         - |  1496 | `			 * Error, quote the key here so the simple syntax keeps meaning what php means.` |
|         - |  1497 | `			 * A numeric ($a[0]) or variable ($a[$k]) subscript is already unambiguous.` |
|         - |  1498 | `			 */` |
|         - |  1499 | `			{` |
|      2435 |  1500 | `				const char *zBr = zExpr;` |
|     14075 |  1501 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     11645 |  1502 | `					zBr++;` |
|         5 |  1503 | `				}` |
|      2435 |  1504 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
|        12 |  1505 | `					const char *zKey = &zBr[1];` |
|        12 |  1506 | `					const char *zKeyEnd = &zIn[-1];` |
|        12 |  1507 | `					const char *zScan = zKey;` |
|        12 |  1508 | `					int bBare = (zKey < zKeyEnd) && !SyisDigit(zKey[0]);` |
|        20 |  1509 | `					while( bBare && zScan < zKeyEnd ){` |
|         9 |  1510 | `						if( !SyisAlphaNum(zScan[0]) && zScan[0] != '_' ){` |
|       ! 0 |  1511 | `							bBare = 0;` |
|       ! 0 |  1512 | `						}` |
|         9 |  1513 | `						zScan++;` |
|         1 |  1514 | `					}` |
|        12 |  1515 | `					if( bBare ){` |
|         - |  1516 | `						SyBlob sSub;` |
|         3 |  1517 | `						SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|         3 |  1518 | `						SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|         3 |  1519 | `						SyBlobAppend(&sSub,"['",2);` |
|         3 |  1520 | `						SyBlobAppend(&sSub,zKey,(sxu32)(zKeyEnd - zKey));` |
|         3 |  1521 | `						SyBlobAppend(&sSub,"']",2);` |
|         4 |  1522 | `						rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|         2 |  1523 | `							(const char *)SyBlobData(&sSub),` |
|         2 |  1524 | `							(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|         3 |  1525 | `						SyBlobRelease(&sSub);` |
|         3 |  1526 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1527 | `							return SXERR_ABORT;` |
|         - |  1528 | `						}` |
|         3 |  1529 | `						if( rc != SXERR_EMPTY ){` |
|         3 |  1530 | `							++iCons;` |
|         1 |  1531 | `						}` |
|         3 |  1532 | `						pObj = 0;` |
|         3 |  1533 | `						continue;` |
|         - |  1534 | `					}` |
|         4 |  1535 | `				}` |
|         - |  1536 | `			}` |
|         - |  1537 | `			/*` |
|         - |  1538 | `			 * "${name}" is php's DEPRECATED (8.2) spelling of the variable $name — NOT an` |
|         - |  1539 | `			 * expression. PH7 handed the whole "${name}" to the expression compiler, whose` |
|         - |  1540 | ``			 * `${expr}` (variable-variable) rule evaluated the bare word `name`; that only`` |
|         - |  1541 | `			 * appeared to work while an unknown bare word fell back to its own name as a` |
|         - |  1542 | `			 * string. Now that an undefined constant is a real Error, rewrite the simple` |
|         - |  1543 | `			 * form to the variable it means. "${$x}" keeps the variable-variable meaning.` |
|         - |  1544 | `			 */` |
|      2428 |  1545 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' && zIn[-1] == '}'` |
|         8 |  1546 | `				&& zExpr[2] != '$' ){` |
|         3 |  1547 | `				const char *zName = &zExpr[2];` |
|         3 |  1548 | `				const char *zStop = &zIn[-1];` |
|         3 |  1549 | `				const char *zScan = zName;` |
|        12 |  1550 | `				while( zScan < zStop && (SyisAlphaNum(zScan[0]) \|\| zScan[0] == '_') ){` |
|         9 |  1551 | `					zScan++;` |
|         1 |  1552 | `				}` |
|         3 |  1553 | `				if( zScan == zStop && zName < zStop ){` |
|         - |  1554 | `					SyBlob sVar;` |
|         3 |  1555 | `					PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pGen->pIn->nLine,` |
|         - |  1556 | `						"Using ${var} in strings is deprecated, use {$var} instead");` |
|         3 |  1557 | `					SyBlobInit(&sVar,&pGen->pVm->sAllocator);` |
|         3 |  1558 | `					SyBlobAppend(&sVar,"$",1);` |
|         3 |  1559 | `					SyBlobAppend(&sVar,zName,(sxu32)(zStop - zName));` |
|         - |  1560 | `					/* The scanner reads one byte PAST the length it is given, so the rewritten` |
|         - |  1561 | `					 * source has to be NUL-terminated: in the ordinary path the byte after the` |
|         - |  1562 | `					 * expression is the string's own closing quote, which stops an identifier,` |
|         - |  1563 | `					 * but here it is whatever the allocator left after the blob -- and an` |
|         - |  1564 | `					 * identifier byte there silently EXTENDS the variable name. */` |
|         3 |  1565 | `					SyBlobNullAppend(&sVar);` |
|         4 |  1566 | `					rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|         2 |  1567 | `						(const char *)SyBlobData(&sVar),` |
|         2 |  1568 | `						(const char *)SyBlobData(&sVar) + SyBlobLength(&sVar));` |
|         3 |  1569 | `					SyBlobRelease(&sVar);` |
|         3 |  1570 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  1571 | `						return SXERR_ABORT;` |
|         - |  1572 | `					}` |
|         3 |  1573 | `					if( rc != SXERR_EMPTY ){` |
|         3 |  1574 | `						++iCons;` |
|         1 |  1575 | `					}` |
|         3 |  1576 | `					pObj = 0;` |
|         3 |  1577 | `					continue;` |
|         - |  1578 | `				}` |
|       ! 0 |  1579 | `			}` |
|         - |  1580 | `			/* Process the expression */` |
|      2431 |  1581 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2431 |  1582 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1583 | `				return SXERR_ABORT;` |
|         - |  1584 | `			}` |
|      2431 |  1585 | `			if( rc != SXERR_EMPTY ){` |
|      2429 |  1586 | `				++iCons;` |
|      1212 |  1587 | `			}` |
|         - |  1588 | `		}` |
|         - |  1589 | `		/* Invalidate the previously used constant */` |
|      2569 |  1590 | `		pObj = 0;` |
|         5 |  1591 | `	}/*for(;;)*/` |
|     79841 |  1592 | `	if( iCons > 1 ){` |
|         - |  1593 | `		/* Concatenate all compiled constants */` |
|      1861 |  1594 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       928 |  1595 | `	}` |
|         - |  1596 | `	/* Node successfully compiled */` |
|     79841 |  1597 | `	return SXRET_OK;` |
|     40111 |  1598 | `}` |
|         - |  1599 | `/*` |
|         - |  1600 | ` * Compile a double quoted string.` |
|         - |  1601 | ` *  See the block-comment above for more information.` |
|         - |  1602 | ` */` |
|     80150 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1604 | `{` |
|         - |  1605 | `	sxi32 rc;` |
|     80155 |  1606 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     40075 |  1607 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1608 | `	/* Compilation result */` |
|     80155 |  1609 | `	return rc;` |
|         5 |  1610 | `}` |
|         - |  1611 | `/*` |
|         - |  1612 | ` * Compile a Heredoc string.` |
|         - |  1613 | ` *  See the block-comment above for more information.` |
|         - |  1614 | ` */` |
|        66 |  1615 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 |  1616 | `{` |
|         - |  1617 | `	SyString sOrig, sStripped;` |
|         - |  1618 | `	sxi32 rc;` |
|        70 |  1619 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        70 |  1620 | `	if( rc != SXRET_OK ){` |
|         6 |  1621 | `		return rc;` |
|         - |  1622 | `	}` |
|         - |  1623 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|         - |  1624 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|         - |  1625 | `	 * Restore before returning so downstream code that references pIn is` |
|         - |  1626 | `	 * unaffected, including on the error path. */` |
|        65 |  1627 | `	sOrig = pGen->pIn->sData;` |
|        65 |  1628 | `	pGen->pIn->sData = sStripped;` |
|        65 |  1629 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|        65 |  1630 | `	pGen->pIn->sData = sOrig;` |
|        31 |  1631 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        65 |  1632 | `	return rc;` |
|        37 |  1633 | `}` |
|         - |  1634 | `/*` |
|         - |  1635 | ` * Compile an array entry whether it is a key or a value.` |
|         - |  1636 | ` *  Notes on array entries.` |
|         - |  1637 | ` *  According to the PHP language reference manual` |
|         - |  1638 | ` *  An array can be created by the array() language construct.` |
|         - |  1639 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|         - |  1640 | ` *  array(  key =>  value` |
|         - |  1641 | ` *    , ...` |
|         - |  1642 | ` *    )` |
|         - |  1643 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|         - |  1644 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|         - |  1645 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|         - |  1646 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|         - |  1647 | ` *  contain integer and string indices.` |
|         - |  1648 | ` *  A value can be any PHP type.` |
|         - |  1649 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|         - |  1650 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|         - |  1651 | ` *  is specified, that value will be overwritten.` |
|         - |  1652 | ` */` |
|    989250 |  1653 | `static sxi32 GenStateCompileArrayEntry(` |
|         - |  1654 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1655 | `	SyToken *pIn,        /* Token stream */` |
|         - |  1656 | `	SyToken *pEnd,       /* End of the token stream */` |
|         - |  1657 | `	sxi32 iFlags,        /* Compilation flags */` |
|         - |  1658 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|         - |  1659 | `	)` |
|         5 |  1660 | `{` |
|         - |  1661 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1662 | `	sxi32 rc;` |
|         - |  1663 | `	/* Swap token stream */` |
|    989255 |  1664 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1665 | `	/* Compile the expression*/` |
|    989255 |  1666 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1667 | `	/* Restore token stream */` |
|    989255 |  1668 | `	RE_SWAP_DELIMITER(pGen);` |
|    989255 |  1669 | `	return rc;` |
|         5 |  1670 | `}` |
|         - |  1671 | `/*` |
|         - |  1672 | ` * Expression tree validator callback for the 'array' language construct.` |
|         - |  1673 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1674 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1675 | ` * error message.` |
|         - |  1676 | ` * See the routine responible of compiling the array language construct` |
|         - |  1677 | ` * for more inforation.` |
|         - |  1678 | ` */` |
|        36 |  1679 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         4 |  1680 | `{` |
|        40 |  1681 | `	sxi32 rc = SXRET_OK;` |
|        40 |  1682 | `	if( pRoot->pOp ){` |
|        14 |  1683 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|        12 |  1684 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|        16 |  1685 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1686 | `			/* Unexpected expression */` |
|        13 |  1687 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|        13 |  1688 | `			if( rc != SXERR_ABORT ){` |
|        13 |  1689 | `				rc = SXERR_INVALID;` |
|         5 |  1690 | `			}` |
|         9 |  1691 | `		}` |
|        31 |  1692 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1693 | `		/* Unexpected expression */` |
|         3 |  1694 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|         3 |  1695 | `		if( rc != SXERR_ABORT ){` |
|         3 |  1696 | `			rc = SXERR_INVALID;` |
|         1 |  1697 | `		}` |
|         1 |  1698 | `	}` |
|        40 |  1699 | `	return rc;` |
|         4 |  1700 | `}` |
|         - |  1701 | `/*` |
|         - |  1702 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|         - |  1703 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|         - |  1704 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|         - |  1705 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|         - |  1706 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|         - |  1707 | ` */` |
|    931076 |  1708 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1709 | `{` |
|    931081 |  1710 | `	SyToken *pCur = pStart;` |
|    931081 |  1711 | `	sxi32 iNest = 0;` |
|   2567693 |  1712 | `	while( pCur < pEnd ){` |
|   2014727 |  1713 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    378111 |  1714 | `			return pCur;` |
|         - |  1715 | `		}` |
|         - |  1716 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1717 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1718 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1719 | `		 */` |
|   1636621 |  1720 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     22883 |  1721 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     22883 |  1722 | `			SyToken *pFn = pCur;` |
|     22878 |  1723 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1724 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1725 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1726 | `				pFn = &pCur[1];` |
|       ! 0 |  1727 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1728 | `			}` |
|     22883 |  1729 | `			if( nKw == PH7_TKWRD_FN ){` |
|         5 |  1730 | `				pCur = pFn + 1; /* past 'fn' */` |
|         5 |  1731 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  1732 | `					pCur++;` |
|       ! 0 |  1733 | `				}` |
|         5 |  1734 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         5 |  1735 | `					pCur++;` |
|         5 |  1736 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1737 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         5 |  1738 | `					if( pCur < pEnd ){` |
|         5 |  1739 | `						pCur++;` |
|         2 |  1740 | `					}` |
|         2 |  1741 | `				}` |
|         5 |  1742 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|       ! 0 |  1743 | `					pCur++;` |
|       ! 0 |  1744 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|       ! 0 |  1745 | `						&& pCur->sData.nByte == 1` |
|       ! 0 |  1746 | `						&& pCur->sData.zString[0] == '?' ){` |
|       ! 0 |  1747 | `						pCur++;` |
|       ! 0 |  1748 | `					}` |
|       ! 0 |  1749 | `					if( pCur < pEnd` |
|       ! 0 |  1750 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  1751 | `						pCur++;` |
|       ! 0 |  1752 | `					}` |
|       ! 0 |  1753 | `				}` |
|         - |  1754 | `				/* The rest of the entry is the arrow-function body — no outer` |
|         - |  1755 | `				 * key to extract. */` |
|         5 |  1756 | `				return pEnd;` |
|         - |  1757 | `			}` |
|         - |  1758 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|         - |  1759 | `			 * entry separator. Skip past the full match span. */` |
|     22879 |  1760 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|         3 |  1761 | `				pCur++; /* past 'match' */` |
|         3 |  1762 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         3 |  1763 | `					pCur++;` |
|         3 |  1764 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1765 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         3 |  1766 | `					if( pCur < pEnd ){` |
|         3 |  1767 | `						pCur++;` |
|         1 |  1768 | `					}` |
|         1 |  1769 | `				}` |
|         3 |  1770 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|         3 |  1771 | `					pCur++;` |
|         3 |  1772 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1773 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|         3 |  1774 | `					if( pCur < pEnd ){` |
|         3 |  1775 | `						pCur++;` |
|         1 |  1776 | `					}` |
|         1 |  1777 | `				}` |
|         3 |  1778 | `				continue;` |
|         - |  1779 | `			}` |
|     11436 |  1780 | `		}` |
|   1636615 |  1781 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     53599 |  1782 | `			iNest++;` |
|   1609818 |  1783 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1784 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1785 | `			 * parser will shortly detect any syntax error. */` |
|     53599 |  1786 | `			iNest--;` |
|     26797 |  1787 | `		}` |
|   1636615 |  1788 | `		pCur++;` |
|         5 |  1789 | `	}` |
|    552971 |  1790 | `	return pEnd;` |
|    465543 |  1791 | `}` |
|         - |  1792 | `/*` |
|         - |  1793 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1794 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1795 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1796 | ` */` |
|    497252 |  1797 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1798 | `{` |
|         - |  1799 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1800 | `	SyToken *pKey,*pCur;` |
|    497257 |  1801 | `	sxi32 iEmitRef = 0;` |
|    497257 |  1802 | `	sxi32 iSpread = 0;` |
|    497257 |  1803 | `	sxi32 nPair = 0;` |
|         - |  1804 | `	sxi32 rc;` |
|    497257 |  1805 | `	xValidator = 0;` |
|    603586 |  1806 | `	for(;;){` |
|         - |  1807 | `		/* Jump leading commas */` |
|   1701365 |  1808 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    494193 |  1809 | `			pGen->pIn++;` |
|         5 |  1810 | `		}` |
|   1207177 |  1811 | `		pCur = pGen->pIn;` |
|   1207177 |  1812 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1813 | `			/* No more entry to process */` |
|    497241 |  1814 | `			break;` |
|         - |  1815 | `		}` |
|    709941 |  1816 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1817 | `			continue;` |
|         - |  1818 | `		}` |
|         - |  1819 | `		/* Compile the key if available */` |
|    709941 |  1820 | `		pKey = pCur;` |
|    709941 |  1821 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|    709941 |  1822 | `		rc = SXERR_EMPTY;` |
|    709941 |  1823 | `		if( pCur < pGen->pIn ){` |
|    279061 |  1824 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1825 | `				/* Missing value */` |
|        13 |  1826 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,0);` |
|        13 |  1827 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1828 | `					return SXERR_ABORT;` |
|         - |  1829 | `				}` |
|        13 |  1830 | `				return SXRET_OK;` |
|         - |  1831 | `			}` |
|         - |  1832 | `			/* Compile the expression holding the key */` |
|    279051 |  1833 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1834 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    279051 |  1835 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1836 | `				return SXERR_ABORT;` |
|         - |  1837 | `			}` |
|    279051 |  1838 | `			pCur++; /* Jump the '=>' operator */` |
|    570408 |  1839 | `		}else if( pKey == pCur ){` |
|         - |  1840 | `			/* Key is omitted,emit a warning */` |
|       ! 0 |  1841 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|       ! 0 |  1842 | `			pCur++; /* Jump the '=>' operator */` |
|       ! 0 |  1843 | `		}else{` |
|         - |  1844 | `			/* Reset back the cursor and point to the entry value */` |
|    430885 |  1845 | `			pCur = pKey;` |
|         - |  1846 | `		}` |
|    709931 |  1847 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1848 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|         - |  1849 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|    430887 |  1850 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|    215441 |  1851 | `		}` |
|    709931 |  1852 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|         - |  1853 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|        45 |  1854 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|        45 |  1855 | `			iEmitRef = 1;` |
|        45 |  1856 | `			pCur++; /* Jump the '&' token */` |
|        45 |  1857 | `			if( pCur >= pGen->pIn ){` |
|         - |  1858 | `				/* Missing value */` |
|         3 |  1859 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|         3 |  1860 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1861 | `					return SXERR_ABORT;` |
|         - |  1862 | `				}` |
|         3 |  1863 | `				return SXRET_OK;` |
|         - |  1864 | `			}` |
|        19 |  1865 | `		}` |
|         - |  1866 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|         - |  1867 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|         - |  1868 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|         - |  1869 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|         - |  1870 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|    709929 |  1871 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|    709929 |  1872 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|         - |  1873 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|         - |  1874 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|         - |  1875 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|         - |  1876 | `			 * output is engine-portable. */` |
|         6 |  1877 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|         - |  1878 | `				"syntax error, unexpected token \"...\"");` |
|         6 |  1879 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1880 | `				return SXERR_ABORT;` |
|         - |  1881 | `			}` |
|         6 |  1882 | `			return SXRET_OK;` |
|         - |  1883 | `		}` |
|         - |  1884 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|         - |  1885 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|         - |  1886 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|         - |  1887 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|         - |  1888 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|   1064885 |  1889 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    354960 |  1890 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1891 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    354960 |  1892 | `			xValidator);` |
|    709925 |  1893 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1894 | `			return SXERR_ABORT;` |
|         - |  1895 | `		}` |
|    709925 |  1896 | `		if( iSpread ){` |
|         - |  1897 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        69 |  1898 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|    709892 |  1899 | `		}else if( iEmitRef ){` |
|         - |  1900 | `			/* Emit the load reference instruction */` |
|        40 |  1901 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1902 | `		}` |
|    709925 |  1903 | `		xValidator = 0;` |
|    709925 |  1904 | `		iEmitRef = 0;` |
|    709925 |  1905 | `		iSpread = 0;` |
|    709925 |  1906 | `		nPair++;` |
|         5 |  1907 | `	}` |
|         - |  1908 | `	/* Emit the load map instruction */` |
|    497241 |  1909 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1910 | `	/* Node successfully compiled */` |
|    497241 |  1911 | `	return SXRET_OK;` |
|    248631 |  1912 | `}` |
|         - |  1913 | `/*` |
|         - |  1914 | ` * Compile the 'array' language construct.` |
|         - |  1915 | ` *	 According to the PHP language reference manual` |
|         - |  1916 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1917 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1918 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1919 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1920 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1921 | ` */` |
|    282752 |  1922 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1923 | `{` |
|         - |  1924 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    282757 |  1925 | `	pGen->pIn += 2;` |
|    282757 |  1926 | `	pGen->pEnd--;` |
|    141376 |  1927 | `	SXUNUSED(iCompileFlag);` |
|    282757 |  1928 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1929 | `}` |
|         - |  1930 | `/*` |
|         - |  1931 | ` * Compile the PHP 8.5 clone(...) call form:` |
|         - |  1932 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|         - |  1933 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|         - |  1934 | ` *                                              property updates as scope-aware writes` |
|         - |  1935 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|         - |  1936 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|         - |  1937 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|         - |  1938 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|         - |  1939 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|         - |  1940 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|         - |  1941 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|         - |  1942 | ` */` |
|        22 |  1943 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  1944 | `{` |
|         - |  1945 | `	SyToken *pIn,*pEnd,*pNext;` |
|        24 |  1946 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|        24 |  1947 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|        24 |  1948 | `	int nArg = 0;` |
|         - |  1949 | `	sxi32 rc;` |
|        11 |  1950 | `	SXUNUSED(iCompileFlag);` |
|         - |  1951 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|        24 |  1952 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|        24 |  1953 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|         - |  1954 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|        24 |  1955 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       ! 0 |  1956 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  1957 | `			"clone(...) first-class callable form is not yet supported");` |
|         - |  1958 | `	}` |
|         - |  1959 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|        62 |  1960 | `	while( pIn < pEnd ){` |
|        40 |  1961 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|        40 |  1962 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|       ! 0 |  1963 | `			break;` |
|         - |  1964 | `		}` |
|        40 |  1965 | `		pArgStart = pIn;` |
|        40 |  1966 | `		pArgEnd   = pNext;` |
|         - |  1967 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|         - |  1968 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|        38 |  1969 | `		if( (pArgEnd - pArgStart) >= 2` |
|        37 |  1970 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        23 |  1971 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|         5 |  1972 | `			pName = pArgStart;` |
|         5 |  1973 | `			pArgStart += 2;` |
|         2 |  1974 | `		}` |
|        40 |  1975 | `		if( pName ){` |
|         - |  1976 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|         - |  1977 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|         4 |  1978 | `			if( pName->sData.nByte == sizeof("object")-1` |
|         4 |  1979 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|         3 |  1980 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|         4 |  1981 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|         3 |  1982 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|         3 |  1983 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         2 |  1984 | `			}else{` |
|       ! 0 |  1985 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|       ! 0 |  1986 | `					"Unknown named parameter $%z",&pName->sData);` |
|         1 |  1987 | `			}` |
|        38 |  1988 | `		}else if( nArg == 0 ){` |
|        22 |  1989 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        25 |  1990 | `		}else if( nArg == 1 ){` |
|        15 |  1991 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         8 |  1992 | `		}else{` |
|       ! 0 |  1993 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|         - |  1994 | `				"clone() expects at most 2 arguments");` |
|         - |  1995 | `		}` |
|        40 |  1996 | `		nArg++;` |
|        40 |  1997 | `		pIn = pNext;` |
|        40 |  1998 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 |  1999 | `			pIn++; /* step over the argument separator */` |
|         8 |  2000 | `		}` |
|         2 |  2001 | `	}` |
|        24 |  2002 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|       ! 0 |  2003 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  2004 | `			"clone() expects at least 1 argument, 0 given");` |
|         - |  2005 | `	}` |
|         - |  2006 | `	/* Object argument -> clone (+ __clone()). */` |
|        24 |  2007 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        24 |  2008 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2009 | `		return SXERR_ABORT;` |
|         - |  2010 | `	}` |
|        24 |  2011 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|         - |  2012 | `	/* Property updates (evaluated after __clone runs). */` |
|        24 |  2013 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|        17 |  2014 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        17 |  2015 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2016 | `			return SXERR_ABORT;` |
|         - |  2017 | `		}` |
|        17 |  2018 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|         8 |  2019 | `	}` |
|        24 |  2020 | `	return SXRET_OK;` |
|        13 |  2021 | `}` |
|         - |  2022 | `/*` |
|         - |  2023 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|         - |  2024 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|         - |  2025 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|         - |  2026 | ` */` |
|    214500 |  2027 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2028 | `{` |
|         - |  2029 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    214505 |  2030 | `	pGen->pIn++;` |
|    214505 |  2031 | `	pGen->pEnd--;` |
|    107250 |  2032 | `	SXUNUSED(iCompileFlag);` |
|    214505 |  2033 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  2034 | `}` |
|         - |  2035 | `/*` |
|         - |  2036 | ` * Expression tree validator callback for the 'list' language construct.` |
|         - |  2037 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  2038 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  2039 | ` * error message.` |
|         - |  2040 | ` * See the routine responible of compiling the list language construct` |
|         - |  2041 | ` * for more inforation.` |
|         - |  2042 | ` */` |
|       210 |  2043 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  2044 | `{` |
|       215 |  2045 | `	sxi32 rc = SXRET_OK;` |
|       215 |  2046 | `	if( pRoot->pOp ){` |
|         4 |  2047 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|         2 |  2048 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|         - |  2049 | `				/* Unexpected expression */` |
|       ! 0 |  2050 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2051 | `					"Assignments can only happen to writable values");` |
|       ! 0 |  2052 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 |  2053 | `					rc = SXERR_INVALID;` |
|       ! 0 |  2054 | `				}` |
|         1 |  2055 | `		}` |
|       213 |  2056 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  2057 | `		/* Unexpected expression */` |
|         6 |  2058 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2059 | `			"Assignments can only happen to writable values");` |
|         6 |  2060 | `		if( rc != SXERR_ABORT ){` |
|         6 |  2061 | `			rc = SXERR_INVALID;` |
|         2 |  2062 | `		}` |
|         2 |  2063 | `	}` |
|       215 |  2064 | `	return rc;` |
|         5 |  2065 | `}` |
|         - |  2066 | `/*` |
|         - |  2067 | ` * Compile the 'list' language construct.` |
|         - |  2068 | ` *  According to the PHP language reference` |
|         - |  2069 | ` *  list(): Assign variables as if they were an array.` |
|         - |  2070 | ` *  list() is used to assign a list of variables in one operation.` |
|         - |  2071 | ` *  Description` |
|         - |  2072 | ` *   array list (mixed $varname [, mixed $... ] )` |
|         - |  2073 | ` *   Like array(), this is not really a function, but a language construct.` |
|         - |  2074 | ` *   list() is used to assign a list of variables in one operation.` |
|         - |  2075 | ` *  Parameters` |
|         - |  2076 | ` *   $varname: A variable.` |
|         - |  2077 | ` *  Return Values` |
|         - |  2078 | ` *   The assigned array.` |
|         - |  2079 | ` */` |
|         - |  2080 | `/* Nested list entry recorded during first pass of list body compilation */` |
|         - |  2081 | `struct NestedListEntry {` |
|         - |  2082 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|         - |  2083 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|         - |  2084 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|         - |  2085 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|         - |  2086 | `};` |
|         - |  2087 | `/*` |
|         - |  2088 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|         - |  2089 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|         - |  2090 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|         - |  2091 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|         - |  2092 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|         - |  2093 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|         - |  2094 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|         - |  2095 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|         - |  2096 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|         - |  2097 | ` */` |
|        22 |  2098 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|         1 |  2099 | `{` |
|         - |  2100 | `	SyToken *pNext;` |
|         - |  2101 | `	sxi32 rc;` |
|        53 |  2102 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|         - |  2103 | `		SyToken *pArrow,*pTarget;` |
|         - |  2104 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|        31 |  2105 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|        31 |  2106 | `		pTarget = &pArrow[1];` |
|        31 |  2107 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|         - |  2108 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|         - |  2109 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|       ! 0 |  2110 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2111 | `				"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2112 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2113 | `		}` |
|         - |  2114 | `		/* DUP the source array (it is on the stack top) */` |
|        31 |  2115 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2116 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|        31 |  2117 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|        31 |  2118 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2119 | `			return SXERR_ABORT;` |
|         - |  2120 | `		}` |
|         - |  2121 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|         - |  2122 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|         - |  2123 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|         - |  2124 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|         - |  2125 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|         - |  2126 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|        31 |  2127 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|        31 |  2128 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|        28 |  2129 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|        15 |  2130 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|         - |  2131 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|         - |  2132 | `			 * Treat source[key] as the inner body's source, then drop the` |
|         - |  2133 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|         5 |  2134 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|         5 |  2135 | `			SyToken *pSavedIn = pGen->pIn;` |
|         5 |  2136 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         5 |  2137 | `			pGen->pIn = pTarget;` |
|         5 |  2138 | `			pGen->pEnd = pNext;` |
|         5 |  2139 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|         2 |  2140 | `			             : PH7_CompileList(&(*pGen),0);` |
|         5 |  2141 | `			pGen->pIn = pSavedIn;` |
|         5 |  2142 | `			pGen->pEnd = pSavedEnd;` |
|         5 |  2143 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2144 | `				return SXERR_ABORT;` |
|         - |  2145 | `			}` |
|         5 |  2146 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         3 |  2147 | `		}else{` |
|         - |  2148 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|         - |  2149 | `			 * is already on the stack as the value; compiling the target appends` |
|         - |  2150 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|         - |  2151 | `			 * assignment does. */` |
|         - |  2152 | `			VmInstr *pInstr;` |
|        27 |  2153 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|        27 |  2154 | `			sxi32 iP1 = 0, iP2 = 0;` |
|        27 |  2155 | `			void *p3 = 0;` |
|        27 |  2156 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|         - |  2157 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|        27 |  2158 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2159 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2160 | `			}` |
|        27 |  2161 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|        27 |  2162 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|         3 |  2163 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|        26 |  2164 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         3 |  2165 | `					iVmOp = PH7_OP_STORE_IDX;` |
|         3 |  2166 | `					iP1 = pInstr->iP1;` |
|         3 |  2167 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         2 |  2168 | `				}else{` |
|        23 |  2169 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|        23 |  2170 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - |  2171 | `				}` |
|        13 |  2172 | `			}` |
|        27 |  2173 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|         - |  2174 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|         - |  2175 | `			 * source array is back on top for the next entry. */` |
|        27 |  2176 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         - |  2177 | `		}` |
|        31 |  2178 | `		pGen->pIn = &pNext[1];` |
|         1 |  2179 | `	}` |
|        23 |  2180 | `	return SXRET_OK;` |
|        12 |  2181 | `}` |
|         - |  2182 | `/*` |
|         - |  2183 | ` * Shared body for list() and short list [...] compilation.` |
|         - |  2184 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|         - |  2185 | ` * the opening delimiter and before the closing delimiter.` |
|         - |  2186 | ` */` |
|       122 |  2187 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|         5 |  2188 | `{` |
|         - |  2189 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|         - |  2190 | `	SyToken *pNext;` |
|         - |  2191 | `	SyToken *pClassifyIn;` |
|       127 |  2192 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|         - |  2193 | `	sxi32 nExpr;` |
|         - |  2194 | `	sxi32 rc;` |
|         - |  2195 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|         - |  2196 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|         - |  2197 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|         - |  2198 | `	 * list. */` |
|       127 |  2199 | `	pClassifyIn = pGen->pIn;` |
|       367 |  2200 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       245 |  2201 | `		if( pGen->pIn >= pNext ){` |
|        13 |  2202 | `			nEmpty++;` |
|       239 |  2203 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|        31 |  2204 | `			nKeyed++;` |
|        16 |  2205 | `		}else{` |
|       203 |  2206 | `			nPositional++;` |
|         - |  2207 | `		}` |
|       245 |  2208 | `		pGen->pIn = &pNext[1];` |
|         5 |  2209 | `	}` |
|       127 |  2210 | `	pGen->pIn = pClassifyIn;` |
|       127 |  2211 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|       ! 0 |  2212 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2213 | `			"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2214 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2215 | `	}` |
|       127 |  2216 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|       ! 0 |  2217 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2218 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|       ! 0 |  2219 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2220 | `	}` |
|       127 |  2221 | `	if( nKeyed > 0 ){` |
|        23 |  2222 | `		return GenStateCompileKeyedListBody(pGen);` |
|         - |  2223 | `	}` |
|       105 |  2224 | `	nExpr = 0;` |
|       105 |  2225 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       315 |  2226 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       215 |  2227 | `		if( pGen->pIn < pNext ){` |
|         - |  2228 | `			/* Check for nested list() */` |
|       203 |  2229 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         3 |  2230 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  2231 | `				/* Record this nested list for post-processing */` |
|         3 |  2232 | `				SyToken *pListEnd = 0;` |
|         3 |  2233 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|         3 |  2234 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         1 |  2235 | `				}` |
|         3 |  2236 | `				if( pListEnd ){` |
|         - |  2237 | `					struct NestedListEntry sEntry;` |
|         3 |  2238 | `					sEntry.nIndex = nExpr;` |
|         3 |  2239 | `					sEntry.pStart = pGen->pIn;` |
|         3 |  2240 | `					sEntry.pEnd = pListEnd + 1;` |
|         3 |  2241 | `					sEntry.isShort = 0;` |
|         3 |  2242 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         1 |  2243 | `				}` |
|         - |  2244 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|         3 |  2245 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       202 |  2246 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  2247 | `				/* Nested short destructuring [...] */` |
|        13 |  2248 | `				SyToken *pBracketEnd = 0;` |
|        13 |  2249 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|        13 |  2250 | `				if( pBracketEnd ){` |
|         - |  2251 | `					struct NestedListEntry sEntry;` |
|        13 |  2252 | `					sEntry.nIndex = nExpr;` |
|        13 |  2253 | `					sEntry.pStart = pGen->pIn;` |
|        13 |  2254 | `					sEntry.pEnd = pBracketEnd + 1;` |
|        13 |  2255 | `					sEntry.isShort = 1;` |
|        13 |  2256 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         6 |  2257 | `				}` |
|         - |  2258 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        13 |  2259 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |  2260 | `			}else{` |
|         - |  2261 | `				/* Compile the expression holding the variable */` |
|       189 |  2262 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       189 |  2263 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  2264 | `					SySetRelease(&sNested);` |
|       ! 0 |  2265 | `					return SXRET_OK;` |
|         - |  2266 | `				}` |
|         - |  2267 | `			}` |
|       104 |  2268 | `		}else{` |
|         - |  2269 | `			/* Empty entry,load NULL */` |
|        13 |  2270 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|         - |  2271 | `		}` |
|       215 |  2272 | `		nExpr++;` |
|         - |  2273 | `		/* Advance the stream cursor */` |
|       215 |  2274 | `		pGen->pIn = &pNext[1];` |
|         5 |  2275 | `	}` |
|         - |  2276 | `	/* Emit the LOAD_LIST instruction */` |
|       105 |  2277 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|         - |  2278 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|         - |  2279 | `	 * For each nested entry, emit code to extract the sub-array` |
|         - |  2280 | `	 * at the corresponding index and recursively destructure it.` |
|         - |  2281 | `	 */` |
|       105 |  2282 | `	if( SySetUsed(&sNested) > 0 ){` |
|        13 |  2283 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|         - |  2284 | `		sxu32 i;` |
|        27 |  2285 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|        15 |  2286 | `			SyToken *pSavedIn = pGen->pIn;` |
|        15 |  2287 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         - |  2288 | `			ph7_value *pIdx;` |
|         - |  2289 | `			sxu32 nConstIdx;` |
|         - |  2290 | `			/* DUP the source array (it's on stack top) */` |
|        15 |  2291 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2292 | `			/* Push the integer index for this nested entry */` |
|        15 |  2293 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|        15 |  2294 | `			if( pIdx == 0 ){` |
|       ! 0 |  2295 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2296 | `				SySetRelease(&sNested);` |
|       ! 0 |  2297 | `				return SXERR_ABORT;` |
|         - |  2298 | `			}` |
|        15 |  2299 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|        15 |  2300 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|         - |  2301 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|         - |  2302 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|         - |  2303 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|         - |  2304 | `			 */` |
|        15 |  2305 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|         - |  2306 | `			/* Recursively compile the inner list */` |
|        15 |  2307 | `			pGen->pIn = apNested[i].pStart;` |
|        15 |  2308 | `			pGen->pEnd = apNested[i].pEnd;` |
|        15 |  2309 | `			if( apNested[i].isShort ){` |
|        13 |  2310 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  2311 | `			}else{` |
|         3 |  2312 | `				rc = PH7_CompileList(&(*pGen),0);` |
|         - |  2313 | `			}` |
|        15 |  2314 | `			pGen->pIn = pSavedIn;` |
|        15 |  2315 | `			pGen->pEnd = pSavedEnd;` |
|        15 |  2316 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2317 | `				SySetRelease(&sNested);` |
|       ! 0 |  2318 | `				return SXERR_ABORT;` |
|         - |  2319 | `			}` |
|         - |  2320 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|        15 |  2321 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         8 |  2322 | `		}` |
|         6 |  2323 | `	}` |
|       105 |  2324 | `	SySetRelease(&sNested);` |
|         - |  2325 | `	/* Node successfully compiled */` |
|       105 |  2326 | `	return SXRET_OK;` |
|        66 |  2327 | `}` |
|        40 |  2328 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2329 | `{` |
|         - |  2330 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|        45 |  2331 | `	pGen->pIn += 2;` |
|        45 |  2332 | `	pGen->pEnd--;` |
|        20 |  2333 | `	SXUNUSED(iCompileFlag);` |
|        45 |  2334 | `	return GenStateCompileListBody(pGen);` |
|         5 |  2335 | `}` |
|        82 |  2336 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2337 | `{` |
|         - |  2338 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        84 |  2339 | `	pGen->pIn++;` |
|        84 |  2340 | `	pGen->pEnd--;` |
|        41 |  2341 | `	SXUNUSED(iCompileFlag);` |
|        84 |  2342 | `	return GenStateCompileListBody(pGen);` |
|         2 |  2343 | `}` |
|         - |  2344 | `/* Forward declarations */` |
|         - |  2345 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|         - |  2346 | `static int GenStateIsReservedConstant(SyString *pName);` |
|         - |  2347 | `static int GenStateIsReadonly(SyToken *pTok);` |
|         - |  2348 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|         - |  2349 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|         - |  2350 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - |  2351 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|         - |  2352 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|         - |  2353 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|         - |  2354 | `/*` |
|         - |  2355 | ` * Compile an annoynmous function or a closure.` |
|         - |  2356 | ` * According to the PHP language reference` |
|         - |  2357 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  2358 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  2359 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|         - |  2360 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|         - |  2361 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|         - |  2362 | ` *  Example Anonymous function variable assignment example` |
|         - |  2363 | ` * <?php` |
|         - |  2364 | ` * $greet = function($name)` |
|         - |  2365 | ` * {` |
|         - |  2366 | ` *    printf("Hello %s\r\n", $name);` |
|         - |  2367 | ` * };` |
|         - |  2368 | ` * $greet('World');` |
|         - |  2369 | ` * $greet('PHP');` |
|         - |  2370 | ` * ?>` |
|         - |  2371 | ` * Note that the implementation of annoynmous function and closure under` |
|         - |  2372 | ` * PH7 is completely different from the one used by the zend engine.` |
|         - |  2373 | ` */` |
|       464 |  2374 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2375 | `{` |
|       469 |  2376 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2377 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2378 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2379 | `							  * one thread is allowed to compile the script.` |
|         - |  2380 | `						      */` |
|         - |  2381 | `	SyString sName;` |
|       469 |  2382 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2383 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2384 | `	sxu32 nKwLine;` |
|       469 |  2385 | `	sxi32 iFlags = 0;` |
|         - |  2386 | `	sxu32 nLen;` |
|         - |  2387 | `	sxi32 rc;` |
|       232 |  2388 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2389 |  |
|       469 |  2390 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       464 |  2391 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       469 |  2392 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2393 | `		/* Static closure: no $this auto-capture, bind refused */` |
|         9 |  2394 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         9 |  2395 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         4 |  2396 | `	}` |
|       469 |  2397 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       469 |  2398 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2399 | `		pGen->pIn++;` |
|       ! 0 |  2400 | `	}` |
|         - |  2401 | `	/* Generate a unique name */` |
|       469 |  2402 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2403 | `	/* Make sure the generated name is unique */` |
|       469 |  2404 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2405 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2406 | `	}` |
|       469 |  2407 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2408 | `	/* Compile the lambda body */` |
|       469 |  2409 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       469 |  2410 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2411 | `		return SXERR_ABORT;` |
|         - |  2412 | `	}` |
|       469 |  2413 | `	if( pAnnonFunc ){` |
|       469 |  2414 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2415 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2416 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       469 |  2417 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2418 | `			return SXERR_ABORT;` |
|         - |  2419 | `		}` |
|       232 |  2420 | `	}` |
|         - |  2421 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2422 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2423 | `	 * the handler wraps either in a Closure instance. */` |
|       469 |  2424 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2425 | `	/* Node successfully compiled */` |
|       469 |  2426 | `	return SXRET_OK;` |
|       237 |  2427 | `}` |
|         - |  2428 | `/*` |
|         - |  2429 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2430 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2431 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2432 | ` */` |
|       204 |  2433 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2434 | `	ph7_gen_state *pGen,` |
|         - |  2435 | `	ph7_vm_func *pFunc,` |
|         - |  2436 | `	const char *zName,` |
|         - |  2437 | `	sxu32 nByte,` |
|         - |  2438 | `	SyString *aShadow,` |
|         - |  2439 | `	sxu32 nShadow)` |
|         3 |  2440 | `{` |
|         - |  2441 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2442 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2443 | `	sxu32 n, nEnv;` |
|         - |  2444 | `	char *zDup;` |
|       207 |  2445 | `	if( nByte == 0 ){` |
|       ! 0 |  2446 | `		return SXRET_OK;` |
|         - |  2447 | `	}` |
|       204 |  2448 | `	if( nByte == sizeof("this")-1` |
|       111 |  2449 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         3 |  2450 | `		return SXRET_OK;` |
|         - |  2451 | `	}` |
|       257 |  2452 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       192 |  2453 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       186 |  2454 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       143 |  2455 | `			return SXRET_OK;` |
|         - |  2456 | `		}` |
|        28 |  2457 | `	}` |
|        63 |  2458 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        63 |  2459 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|        91 |  2460 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2461 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2462 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2463 | `			return SXRET_OK;` |
|         - |  2464 | `		}` |
|        15 |  2465 | `	}` |
|        61 |  2466 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        61 |  2467 | `	if( zDup == 0 ){` |
|       ! 0 |  2468 | `		return SXERR_ABORT;` |
|         - |  2469 | `	}` |
|        61 |  2470 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        61 |  2471 | `	sEnv.iFlags = 0;` |
|        61 |  2472 | `	sEnv.nIdx = SXU32_HIGH;` |
|        61 |  2473 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        61 |  2474 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        61 |  2475 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        61 |  2476 | `	return SXRET_OK;` |
|       105 |  2477 | `}` |
|         - |  2478 | `/*` |
|         - |  2479 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|         - |  2480 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|         - |  2481 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|         - |  2482 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|         - |  2483 | ` */` |
|        56 |  2484 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|         - |  2485 | `	ph7_gen_state *pGen,` |
|         - |  2486 | `	ph7_vm_func *pFunc,` |
|         - |  2487 | `	const char *zIn,` |
|         - |  2488 | `	const char *zEnd,` |
|         - |  2489 | `	SyString *aShadow,` |
|         - |  2490 | `	sxu32 nShadow)` |
|         2 |  2491 | `{` |
|         - |  2492 | `	sxi32 rc;` |
|       370 |  2493 | `	while( zIn < zEnd ){` |
|       314 |  2494 | `		if( zIn[0] == '\\' ){` |
|         5 |  2495 | `			zIn++;` |
|         5 |  2496 | `			if( zIn < zEnd ){` |
|         5 |  2497 | `				zIn++;` |
|         2 |  2498 | `			}` |
|         5 |  2499 | `			continue;` |
|         - |  2500 | `		}` |
|       308 |  2501 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        26 |  2502 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|        24 |  2503 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|         - |  2504 | `			const char *zName;` |
|        26 |  2505 | `			zIn++; /* skip '$' */` |
|        26 |  2506 | `			zName = zIn;` |
|        82 |  2507 | `			while( zIn < zEnd ){` |
|        76 |  2508 | `				unsigned char c = (unsigned char)zIn[0];` |
|        76 |  2509 | `				if( c >= 0xc0 ){` |
|       ! 0 |  2510 | `					zIn++;` |
|       ! 0 |  2511 | `					while( zIn < zEnd` |
|       ! 0 |  2512 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  2513 | `						zIn++;` |
|       ! 0 |  2514 | `					}` |
|       ! 0 |  2515 | `					continue;` |
|         - |  2516 | `				}` |
|        76 |  2517 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        20 |  2518 | `					break;` |
|         - |  2519 | `				}` |
|        58 |  2520 | `				zIn++;` |
|         2 |  2521 | `			}` |
|        26 |  2522 | `			if( zIn > zName ){` |
|        38 |  2523 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|        24 |  2524 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|        26 |  2525 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2526 | `					return SXERR_ABORT;` |
|         - |  2527 | `				}` |
|        12 |  2528 | `			}` |
|        26 |  2529 | `			continue;` |
|         - |  2530 | `		}` |
|       286 |  2531 | `		zIn++;` |
|         2 |  2532 | `	}` |
|        58 |  2533 | `	return SXRET_OK;` |
|        30 |  2534 | `}` |
|         - |  2535 | `/*` |
|         - |  2536 | ` * Scan the body token range of an arrow function for free-variable` |
|         - |  2537 | ` * references and record them in pFunc's closure environment. Handles:` |
|         - |  2538 | ` *   - plain $<id> pairs` |
|         - |  2539 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|         - |  2540 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|         - |  2541 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|         - |  2542 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|         - |  2543 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|         - |  2544 | ` *     are never mistakenly captured.` |
|         - |  2545 | ` */` |
|       304 |  2546 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2547 | `	ph7_gen_state *pGen,` |
|         - |  2548 | `	ph7_vm_func *pFunc,` |
|         - |  2549 | `	SyToken *pStart,` |
|         - |  2550 | `	SyToken *pEnd,` |
|         - |  2551 | `	SyString *aShadow,` |
|         - |  2552 | `	sxu32 nShadow)` |
|         3 |  2553 | `{` |
|       307 |  2554 | `	SyToken *pScan = pStart;` |
|         - |  2555 | `	sxi32 rc;` |
|      1739 |  2556 | `	while( pScan < pEnd ){` |
|      1435 |  2557 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|        86 |  2558 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        28 |  2559 | `				pScan->sData.zString,` |
|        56 |  2560 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        28 |  2561 | `				aShadow,nShadow);` |
|        58 |  2562 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2563 | `				return SXERR_ABORT;` |
|         - |  2564 | `			}` |
|        58 |  2565 | `			pScan++;` |
|        58 |  2566 | `			continue;` |
|         - |  2567 | `		}` |
|      1379 |  2568 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        30 |  2569 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        30 |  2570 | `			SyToken *pFnKw = pScan;` |
|        28 |  2571 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2572 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         2 |  2573 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2574 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2575 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2576 | `			}` |
|        30 |  2577 | `			if( nKw == PH7_TKWRD_FN ){` |
|         - |  2578 | `				SyToken *pInnerSigStart;` |
|         - |  2579 | `				SyToken *pInnerSigEnd;` |
|         - |  2580 | `				SyToken *pInnerBodyEnd;` |
|         - |  2581 | `				SyString *aInnerShadow;` |
|         - |  2582 | `				sxu32 nInnerShadow;` |
|         - |  2583 | `				sxu32 nInnerParamMax;` |
|         - |  2584 | `				SyToken *p;` |
|         - |  2585 | `				int iNestInner;` |
|        19 |  2586 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|        19 |  2587 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2588 | `					pScan++;` |
|       ! 0 |  2589 | `				}` |
|        19 |  2590 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2591 | `					pScan++;` |
|       ! 0 |  2592 | `					continue;` |
|         - |  2593 | `				}` |
|        19 |  2594 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|        19 |  2595 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|         - |  2596 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|        19 |  2597 | `				if( pInnerSigEnd >= pEnd ){` |
|       ! 0 |  2598 | `					pScan = pEnd;` |
|       ! 0 |  2599 | `					continue;` |
|         - |  2600 | `				}` |
|         - |  2601 | `				/* Build an augmented shadow list: inherited + inner params */` |
|        19 |  2602 | `				nInnerParamMax = 0;` |
|        57 |  2603 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2604 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|        13 |  2605 | `						nInnerParamMax++;` |
|         6 |  2606 | `					}` |
|        20 |  2607 | `				}` |
|        19 |  2608 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|        18 |  2609 | `					&pGen->pVm->sAllocator,` |
|        18 |  2610 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|        19 |  2611 | `				if( aInnerShadow == 0 ){` |
|       ! 0 |  2612 | `					return SXERR_ABORT;` |
|         - |  2613 | `				}` |
|        19 |  2614 | `				nInnerShadow = 0;` |
|        25 |  2615 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|         7 |  2616 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|         4 |  2617 | `				}` |
|        57 |  2618 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2619 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|        27 |  2620 | `						continue;` |
|         - |  2621 | `					}` |
|        13 |  2622 | `					if( &p[1] >= pInnerSigEnd ){` |
|       ! 0 |  2623 | `						break;` |
|         - |  2624 | `					}` |
|        13 |  2625 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2626 | `						continue;` |
|         - |  2627 | `					}` |
|        13 |  2628 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|         7 |  2629 | `				}` |
|        19 |  2630 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|        19 |  2631 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|       ! 0 |  2632 | `					pScan++;` |
|       ! 0 |  2633 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|       ! 0 |  2634 | `						&& pScan->sData.nByte == 1` |
|       ! 0 |  2635 | `						&& pScan->sData.zString[0] == '?' ){` |
|       ! 0 |  2636 | `						pScan++;` |
|       ! 0 |  2637 | `					}` |
|       ! 0 |  2638 | `					if( pScan < pEnd` |
|       ! 0 |  2639 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  2640 | `						pScan++;` |
|       ! 0 |  2641 | `					}` |
|       ! 0 |  2642 | `				}` |
|        19 |  2643 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|        19 |  2644 | `					pScan++; /* past '=>' */` |
|         9 |  2645 | `				}` |
|        19 |  2646 | `				pInnerBodyEnd = pScan;` |
|        19 |  2647 | `				iNestInner = 0;` |
|       131 |  2648 | `				while( pInnerBodyEnd < pEnd ){` |
|       113 |  2649 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|         - |  2650 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|         - |  2651 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       ! 0 |  2652 | `						break;` |
|         - |  2653 | `					}` |
|       113 |  2654 | `					if( pInnerBodyEnd->nType &` |
|         - |  2655 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         3 |  2656 | `						iNestInner++;` |
|       112 |  2657 | `					}else if( pInnerBodyEnd->nType &` |
|         - |  2658 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         3 |  2659 | `						iNestInner--;` |
|         1 |  2660 | `					}` |
|       113 |  2661 | `					pInnerBodyEnd++;` |
|         1 |  2662 | `				}` |
|         - |  2663 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|         - |  2664 | `				 * the outer's body: a default value is evaluated at call time` |
|         - |  2665 | `				 * in the outer frame, so any free variable it references is` |
|         - |  2666 | `				 * an outer capture. We must NOT scan the parameter-name` |
|         - |  2667 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|         - |  2668 | `				 * or those names leak into the outer's closure environment.` |
|         - |  2669 | `				 *` |
|         - |  2670 | `				 * Walk the signature argument-by-argument, splitting on` |
|         - |  2671 | `				 * top-level commas, and for each argument scan only the token` |
|         - |  2672 | `				 * range after the '=' sign. */` |
|         - |  2673 | `				{` |
|        19 |  2674 | `					SyToken *pArgStart = pInnerSigStart;` |
|        31 |  2675 | `					while( pArgStart < pInnerSigEnd ){` |
|        13 |  2676 | `						SyToken *pArgEnd = pArgStart;` |
|        13 |  2677 | `						SyToken *pEq = 0;` |
|        13 |  2678 | `						int iNestArg = 0;` |
|        49 |  2679 | `						while( pArgEnd < pInnerSigEnd ){` |
|        38 |  2680 | `							if( iNestArg == 0` |
|        39 |  2681 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|         3 |  2682 | `								break;` |
|         - |  2683 | `							}` |
|        37 |  2684 | `							if( pArgEnd->nType &` |
|         - |  2685 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  2686 | `								iNestArg++;` |
|        37 |  2687 | `							}else if( pArgEnd->nType &` |
|         - |  2688 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  2689 | `								iNestArg--;` |
|       ! 0 |  2690 | `							}` |
|        36 |  2691 | `							if( pEq == 0 && iNestArg == 0` |
|        31 |  2692 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|         7 |  2693 | `								pEq = pArgEnd;` |
|         3 |  2694 | `							}` |
|        37 |  2695 | `							pArgEnd++;` |
|         1 |  2696 | `						}` |
|        13 |  2697 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|        10 |  2698 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         3 |  2699 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|         7 |  2700 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  2701 | `								return SXERR_ABORT;` |
|         - |  2702 | `							}` |
|         3 |  2703 | `						}` |
|        13 |  2704 | `						pArgStart = pArgEnd;` |
|        12 |  2705 | `						if( pArgStart < pInnerSigEnd` |
|         8 |  2706 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|         3 |  2707 | `							pArgStart++;` |
|         1 |  2708 | `						}` |
|         1 |  2709 | `					}` |
|         - |  2710 | `				}` |
|        28 |  2711 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         9 |  2712 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|        19 |  2713 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2714 | `					return SXERR_ABORT;` |
|         - |  2715 | `				}` |
|        19 |  2716 | `				pScan = pInnerBodyEnd;` |
|        19 |  2717 | `				continue;` |
|         - |  2718 | `			}` |
|         5 |  2719 | `		}` |
|      1361 |  2720 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      1181 |  2721 | `			pScan++;` |
|      1181 |  2722 | `			continue;` |
|         - |  2723 | `		}` |
|         - |  2724 | `		{` |
|         - |  2725 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       183 |  2726 | `			SyToken *pDollar = pScan;` |
|       270 |  2727 | `			while( &pDollar[1] < pEnd` |
|       183 |  2728 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2729 | `				pDollar++;` |
|       ! 0 |  2730 | `			}` |
|       183 |  2731 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2732 | `				break;` |
|         - |  2733 | `			}` |
|       183 |  2734 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2735 | `				pScan = pDollar + 1;` |
|       ! 0 |  2736 | `				continue;` |
|         - |  2737 | `			}` |
|       273 |  2738 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       180 |  2739 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|        90 |  2740 | `				aShadow,nShadow);` |
|       183 |  2741 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2742 | `				return SXERR_ABORT;` |
|         - |  2743 | `			}` |
|       183 |  2744 | `			pScan = pDollar + 2;` |
|         - |  2745 | `		}` |
|         3 |  2746 | `	}` |
|       307 |  2747 | `	return SXRET_OK;` |
|       155 |  2748 | `}` |
|         - |  2749 | `/*` |
|         - |  2750 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2751 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2752 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2753 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2754 | ` * $this is also made available.` |
|         - |  2755 | ` */` |
|       286 |  2756 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2757 | `{` |
|         - |  2758 | `	ph7_vm_func *pFunc;` |
|         - |  2759 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2760 | `	GenBlock *pBlock;` |
|         - |  2761 | `	SySet *pInstrContainer;` |
|         - |  2762 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|         - |  2763 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|         - |  2764 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|         - |  2765 | `	SyToken *pSavedEnd;` |
|         - |  2766 | `	ph7_vm_func_arg *aArgs;` |
|         - |  2767 | `	char zName[512];` |
|         - |  2768 | `	static int iCnt = 1;` |
|         - |  2769 | `	char *zDup;` |
|         - |  2770 | `	SyToken *pTokKw;` |
|         - |  2771 | `	sxu32 nLen;` |
|         - |  2772 | `	sxu32 nLine;` |
|       291 |  2773 | `	sxi32 iFlags = 0;` |
|       291 |  2774 | `	int bStatic = 0;` |
|         - |  2775 | `	sxi32 rc;` |
|         - |  2776 | `	sxu32 n;` |
|       143 |  2777 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2778 |  |
|       291 |  2779 | `	nLine = pGen->pIn->nLine;` |
|         - |  2780 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       291 |  2781 | `	pTokKw = pGen->pIn;` |
|         - |  2782 | `	/* Optional 'static' prefix */` |
|       286 |  2783 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       291 |  2784 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  2785 | `		bStatic = 1;` |
|         7 |  2786 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         7 |  2787 | `		pGen->pIn++;` |
|         3 |  2788 | `	}` |
|         - |  2789 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       286 |  2790 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       291 |  2791 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2792 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2793 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2794 | `		return SXERR_SYNTAX;` |
|         - |  2795 | `	}` |
|       291 |  2796 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2797 | `	/* Optional '&' — return by reference */` |
|       291 |  2798 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2799 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2800 | `		pGen->pIn++;` |
|       ! 0 |  2801 | `	}` |
|         - |  2802 | `	/* Expect '(' */` |
|       291 |  2803 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  2804 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2805 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2806 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|         2 |  2807 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2808 | `		}else{` |
|       ! 0 |  2809 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2810 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|         - |  2811 | `		}` |
|         3 |  2812 | `		return SXERR_SYNTAX;` |
|         - |  2813 | `	}` |
|       289 |  2814 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2815 | `	/* Delimit the parameter list */` |
|       289 |  2816 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       289 |  2817 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2818 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2819 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2820 | `		return SXERR_SYNTAX;` |
|         - |  2821 | `	}` |
|         - |  2822 | `	/* Allocate the function state */` |
|       286 |  2823 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       286 |  2824 | `	if( pFunc == 0 ){` |
|       ! 0 |  2825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2826 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2827 | `		return SXERR_ABORT;` |
|         - |  2828 | `	}` |
|         - |  2829 | `	/* Generate a unique lambda name */` |
|       286 |  2830 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       286 |  2831 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2832 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2833 | `	}` |
|       286 |  2834 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       286 |  2835 | `	if( zDup == 0 ){` |
|       ! 0 |  2836 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2837 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2838 | `		return SXERR_ABORT;` |
|         - |  2839 | `	}` |
|       286 |  2840 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2841 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       286 |  2842 | `	pFunc->nLine = nLine;` |
|         - |  2843 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       286 |  2844 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2845 | `		return SXERR_ABORT;` |
|         - |  2846 | `	}` |
|         - |  2847 | `	/* Collect function arguments */` |
|       286 |  2848 | `	if( pGen->pIn < pSigEnd ){` |
|       116 |  2849 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       116 |  2850 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2851 | `			return SXERR_ABORT;` |
|         - |  2852 | `		}` |
|        56 |  2853 | `	}` |
|         - |  2854 | `	/* Point past ')' and parse optional return type */` |
|       286 |  2855 | `	pGen->pIn = &pSigEnd[1];` |
|       286 |  2856 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       286 |  2857 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2858 | `		return SXERR_ABORT;` |
|       286 |  2859 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2860 | `		return SXERR_SYNTAX;` |
|         - |  2861 | `	}` |
|         - |  2862 | `	/* Expect '=>' */` |
|       286 |  2863 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  2864 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2865 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2866 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|         2 |  2867 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2868 | `		}else{` |
|       ! 0 |  2869 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2870 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|         - |  2871 | `		}` |
|         3 |  2872 | `		return SXERR_SYNTAX;` |
|         - |  2873 | `	}` |
|       283 |  2874 | `	pGen->pIn++; /* Jump '=>' */` |
|       283 |  2875 | `	pBodyStart = pGen->pIn;` |
|       283 |  2876 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2877 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2878 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2879 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2880 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       283 |  2881 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2882 | `	{` |
|       283 |  2883 | `		SyString *aShadow = 0;` |
|       283 |  2884 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       283 |  2885 | `		if( nShadow > 0 ){` |
|       113 |  2886 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       110 |  2887 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       113 |  2888 | `			if( aShadow == 0 ){` |
|       ! 0 |  2889 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2890 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2891 | `				return SXERR_ABORT;` |
|         - |  2892 | `			}` |
|       257 |  2893 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       147 |  2894 | `				aShadow[n] = aArgs[n].sName;` |
|        75 |  2895 | `			}` |
|        55 |  2896 | `		}` |
|       423 |  2897 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       140 |  2898 | `			aShadow,nShadow);` |
|       283 |  2899 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2900 | `			return SXERR_ABORT;` |
|         - |  2901 | `		}` |
|         - |  2902 | `	}` |
|         - |  2903 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2904 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2905 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2906 | `	 * $this. */` |
|       283 |  2907 | `	if( !bStatic ){` |
|         - |  2908 | `		char *zThisDup;` |
|       277 |  2909 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       277 |  2910 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2911 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2912 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2913 | `			return SXERR_ABORT;` |
|         - |  2914 | `		}` |
|       277 |  2915 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       277 |  2916 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       277 |  2917 | `		sEnv.nIdx = SXU32_HIGH;` |
|       277 |  2918 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       277 |  2919 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       277 |  2920 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       137 |  2921 | `	}` |
|         - |  2922 | `	/* Arrow functions are always closures */` |
|       283 |  2923 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2924 | `	/* Compile the body expression as an implicit return */` |
|       423 |  2925 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       140 |  2926 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       283 |  2927 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2928 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2929 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2930 | `		return SXERR_ABORT;` |
|         - |  2931 | `	}` |
|       283 |  2932 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       283 |  2933 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       283 |  2934 | `	pSavedEnd = pGen->pEnd;` |
|       283 |  2935 | `	pGen->pIn = pBodyStart;` |
|       283 |  2936 | `	pGen->pEnd = pBodyEnd;` |
|       283 |  2937 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       283 |  2938 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2939 | `		return SXERR_ABORT;` |
|         - |  2940 | `	}` |
|         - |  2941 | `	/* The cursor stopped just past the body expression */` |
|       283 |  2942 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2943 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2944 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2945 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2946 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       283 |  2947 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       283 |  2948 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       283 |  2949 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       283 |  2950 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       283 |  2951 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2952 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       283 |  2953 | `	pGen->pIn = pBodyEnd;` |
|       283 |  2954 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2955 | `	/* Emit the load-closure instruction */` |
|       283 |  2956 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       283 |  2957 | `	return SXRET_OK;` |
|       148 |  2958 | `}` |
|         - |  2959 | `/*` |
|         - |  2960 | ` * Compile a single arm's expression range into a freshly-allocated` |
|         - |  2961 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|         - |  2962 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|         - |  2963 | ` * expression's value.` |
|         - |  2964 | ` */` |
|       354 |  2965 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|         - |  2966 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|         3 |  2967 | `{` |
|         - |  2968 | `	SySet *pInstrContainer;` |
|         - |  2969 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  2970 | `	GenBlock *pArmBlock;` |
|         - |  2971 | `	sxi32 rc;` |
|       357 |  2972 | `	pTmpIn  = pGen->pIn;` |
|       357 |  2973 | `	pTmpEnd = pGen->pEnd;` |
|       357 |  2974 | `	pGen->pIn  = pStart;` |
|       357 |  2975 | `	pGen->pEnd = pStop;` |
|       357 |  2976 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       357 |  2977 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|         - |  2978 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|         - |  2979 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|         - |  2980 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|         - |  2981 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|         - |  2982 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|       534 |  2983 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       177 |  2984 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|       357 |  2985 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2986 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  2987 | `		pGen->pIn  = pTmpIn;` |
|       ! 0 |  2988 | `		pGen->pEnd = pTmpEnd;` |
|       ! 0 |  2989 | `		return SXERR_ABORT;` |
|         - |  2990 | `	}` |
|       357 |  2991 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       357 |  2992 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       357 |  2993 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       357 |  2994 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       357 |  2995 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       357 |  2996 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       357 |  2997 | `	pGen->pIn  = pTmpIn;` |
|       357 |  2998 | `	pGen->pEnd = pTmpEnd;` |
|       357 |  2999 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3000 | `		return SXERR_ABORT;` |
|         - |  3001 | `	}` |
|       357 |  3002 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 |  3003 | `		return SXERR_EMPTY;` |
|         - |  3004 | `	}` |
|       357 |  3005 | `	return SXRET_OK;` |
|       180 |  3006 | `}` |
|         - |  3007 | `/*` |
|         - |  3008 | ` * Compile a PHP 8.0 match expression:` |
|         - |  3009 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|         - |  3010 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|         - |  3011 | ` * Strict comparison (===) is used between the subject and each condition.` |
|         - |  3012 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|         - |  3013 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|         - |  3014 | ` */` |
|         - |  3015 | `/*` |
|         - |  3016 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|         - |  3017 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|         - |  3018 | ` * caller can bail out of the current expression.` |
|         - |  3019 | ` */` |
|         2 |  3020 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|         1 |  3021 | `{` |
|         - |  3022 | `	va_list ap;` |
|         - |  3023 | `	sxi32 rc;` |
|         - |  3024 | `	SyBlob sMsg;` |
|         3 |  3025 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|         3 |  3026 | `	va_start(ap,zFmt);` |
|         3 |  3027 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|         3 |  3028 | `	va_end(ap);` |
|         3 |  3029 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|         3 |  3030 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|         3 |  3031 | `	SyBlobRelease(&sMsg);` |
|         3 |  3032 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3033 | `		return SXERR_ABORT;` |
|         - |  3034 | `	}` |
|         3 |  3035 | `	return SXERR_SYNTAX;` |
|         2 |  3036 | `}` |
|         - |  3037 | `/*` |
|         - |  3038 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|         - |  3039 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|         - |  3040 | ` * Returns the stop token pointer (or pEnd if none found).` |
|         - |  3041 | ` */` |
|       356 |  3042 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|         4 |  3043 | `{` |
|       360 |  3044 | `	SyToken *pCur = pStart;` |
|       360 |  3045 | `	int iNest = 0;` |
|       838 |  3046 | `	while( pCur < pEnd ){` |
|       802 |  3047 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        13 |  3048 | `			iNest++;` |
|       796 |  3049 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        13 |  3050 | `			iNest--;` |
|       784 |  3051 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|       323 |  3052 | `			return pCur;` |
|         - |  3053 | `		}` |
|       482 |  3054 | `		pCur++;` |
|         4 |  3055 | `	}` |
|        39 |  3056 | `	return pEnd;` |
|       182 |  3057 | `}` |
|        72 |  3058 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3059 | `{` |
|         - |  3060 | `	ph7_match *pMatch;` |
|         - |  3061 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|        77 |  3062 | `	int bHasDefault = 0;` |
|         - |  3063 | `	sxu32 nLine;` |
|         - |  3064 | `	sxi32 rc;` |
|        36 |  3065 | `	SXUNUSED(iCompileFlag);` |
|        77 |  3066 | `	nLine = pGen->pIn->nLine;` |
|        77 |  3067 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|         - |  3068 | `	/* Expect '(' */` |
|        77 |  3069 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  3070 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3071 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|       ! 0 |  3072 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|         - |  3073 | `	}` |
|        77 |  3074 | `	pGen->pIn++; /* Jump '(' */` |
|        77 |  3075 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|        77 |  3076 | `	if( pSubjEnd >= pGen->pEnd ){` |
|       ! 0 |  3077 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3078 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         - |  3079 | `	}` |
|        77 |  3080 | `	if( pGen->pIn >= pSubjEnd ){` |
|       ! 0 |  3081 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3082 | `			"syntax error, unexpected \")\", expecting match subject");` |
|         - |  3083 | `	}` |
|         - |  3084 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|        77 |  3085 | `	pSavedEnd = pGen->pEnd;` |
|        77 |  3086 | `	pGen->pEnd = pSubjEnd;` |
|        77 |  3087 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        77 |  3088 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3089 | `		return SXERR_ABORT;` |
|         - |  3090 | `	}` |
|        77 |  3091 | `	pGen->pEnd = pSavedEnd;` |
|        77 |  3092 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|         - |  3093 | `	/* Expect '{' */` |
|        77 |  3094 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 |  3095 | `		return GenStateMatchError(pGen,` |
|       ! 0 |  3096 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  3097 | `			"syntax error, expecting \"{\" after match subject");` |
|         - |  3098 | `	}` |
|        77 |  3099 | `	pGen->pIn++; /* Jump '{' */` |
|        77 |  3100 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|        77 |  3101 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  3102 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3103 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|         - |  3104 | `	}` |
|         - |  3105 | `	/* Allocate ph7_match container */` |
|        77 |  3106 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|        77 |  3107 | `	if( pMatch == 0 ){` |
|       ! 0 |  3108 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  3109 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3110 | `		return SXERR_ABORT;` |
|         - |  3111 | `	}` |
|        77 |  3112 | `	SyZero(pMatch,sizeof(ph7_match));` |
|        77 |  3113 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|         - |  3114 | `	/* Iterate arms */` |
|       259 |  3115 | `	while( pGen->pIn < pBodyEnd ){` |
|         - |  3116 | `		ph7_match_arm sArm;` |
|         - |  3117 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|       190 |  3118 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|       190 |  3119 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|       190 |  3120 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|       190 |  3121 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3122 | `		/* 'default' arm? */` |
|       186 |  3123 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       107 |  3124 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|        22 |  3125 | `			if( bHasDefault ){` |
|         3 |  3126 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|         - |  3127 | `					"Match expressions may only contain one default arm");` |
|         4 |  3128 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  3129 | `			}` |
|        20 |  3130 | `			sArm.bDefault = 1;` |
|        20 |  3131 | `			bHasDefault = 1;` |
|        20 |  3132 | `			pGen->pIn++;` |
|        20 |  3133 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       ! 0 |  3134 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3135 | `					"syntax error, expecting \"=>\" after 'default'");` |
|         - |  3136 | `			}` |
|        20 |  3137 | `			pGen->pIn++; /* Jump '=>' */` |
|        11 |  3138 | `		}else{` |
|         - |  3139 | `			/* Condition list: cond (',' cond)* '=>' */` |
|       170 |  3140 | `			pCondStart = pGen->pIn;` |
|       170 |  3141 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|         - |  3142 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       178 |  3143 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|         - |  3144 | `				SySet sCondBc;` |
|         9 |  3145 | `				if( pCondStart >= pArrow ){` |
|       ! 0 |  3146 | `					return GenStateMatchError(pGen,nArmLine,` |
|         - |  3147 | `						"syntax error, empty match condition expression");` |
|         - |  3148 | `				}` |
|         9 |  3149 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         9 |  3150 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|         9 |  3151 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3152 | `					return SXERR_ABORT;` |
|         - |  3153 | `				}` |
|         9 |  3154 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         9 |  3155 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|         9 |  3156 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|         - |  3157 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|         1 |  3158 | `			}` |
|       170 |  3159 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  3160 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3161 | `					"syntax error, expecting \"=>\" in match arm");` |
|         - |  3162 | `			}` |
|       167 |  3163 | `			if( pCondStart >= pArrow ){` |
|       ! 0 |  3164 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3165 | `					"syntax error, empty match condition expression");` |
|         - |  3166 | `			}` |
|         - |  3167 | `			{` |
|         - |  3168 | `				SySet sCondBc;` |
|       167 |  3169 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       167 |  3170 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       167 |  3171 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3172 | `					return SXERR_ABORT;` |
|         - |  3173 | `				}` |
|       167 |  3174 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         - |  3175 | `			}` |
|       167 |  3176 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|         - |  3177 | `		}` |
|         - |  3178 | `		/* Compile result expression: up to top-level ',' or body end */` |
|       185 |  3179 | `		pResStart = pGen->pIn;` |
|       185 |  3180 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|       185 |  3181 | `		if( pResStart >= pResEnd ){` |
|       ! 0 |  3182 | `			return GenStateMatchError(pGen,nArmLine,` |
|         - |  3183 | `				"syntax error, expected expression after \"=>\"");` |
|         - |  3184 | `		}` |
|       185 |  3185 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|       185 |  3186 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3187 | `			return SXERR_ABORT;` |
|         - |  3188 | `		}` |
|       185 |  3189 | `		pGen->pIn = pResEnd;` |
|       185 |  3190 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       151 |  3191 | `			pGen->pIn++; /* Skip trailing ',' */` |
|        74 |  3192 | `		}` |
|       185 |  3193 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|         3 |  3194 | `	}` |
|        71 |  3195 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|        71 |  3196 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|        71 |  3197 | `	return SXRET_OK;` |
|        41 |  3198 | `}` |
|         - |  3199 | `/*` |
|         - |  3200 | ` * Compile a backtick quoted string.` |
|         - |  3201 | ` */` |
|         4 |  3202 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  3203 | `{` |
|         - |  3204 | `	static const SyString sName = { "shell_exec", sizeof("shell_exec")-1 };` |
|         6 |  3205 | `	sxu32 nIdx = 0;` |
|         - |  3206 | `	sxi32 rc;` |
|         - |  3207 | `	/*` |
|         - |  3208 | ``	 * `cmd` IS shell_exec("cmd") in php — it interpolates like a double-quoted string,`` |
|         - |  3209 | `	 * runs the command and yields its output. PH7 refused to run it at all (TICKET` |
|         - |  3210 | `	 * 1433-40) and quietly evaluated to NULL. php 8.5 deprecates the syntax but still` |
|         - |  3211 | `	 * executes it, so compile it to the real call and say what php says.` |
|         - |  3212 | `	 */` |
|         6 |  3213 | `	PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pGen->pIn->nLine,` |
|         - |  3214 | ``		"The backtick (`) operator is deprecated, use shell_exec() instead");`` |
|         - |  3215 | `	/* The body interpolates exactly like a double-quoted string */` |
|         6 |  3216 | `	pGen->pIn->nType &= ~PH7_TK_BSTR;` |
|         6 |  3217 | `	pGen->pIn->nType \|= PH7_TK_DSTR;` |
|         6 |  3218 | `	rc = PH7_CompileString(&(*pGen),iCompileFlag);` |
|         6 |  3219 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3220 | `		return rc;` |
|         - |  3221 | `	}` |
|         - |  3222 | `	/* ... and the command string is then handed to shell_exec() */` |
|         6 |  3223 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|         6 |  3224 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         6 |  3225 | `		if( pObj == 0 ){` |
|       ! 0 |  3226 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  3227 | `			return SXERR_ABORT;` |
|         - |  3228 | `		}` |
|         6 |  3229 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|         6 |  3230 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|         2 |  3231 | `	}` |
|         6 |  3232 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         6 |  3233 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         6 |  3234 | `	return SXRET_OK;` |
|         4 |  3235 | `}` |
|         - |  3236 | `/*` |
|         - |  3237 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|         - |  3238 | ` * construct.` |
|         - |  3239 | ` */` |
|        70 |  3240 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3241 | `{` |
|         - |  3242 | `	SyString *pName;` |
|         - |  3243 | `	sxu32 nKeyID;` |
|         - |  3244 | `	sxi32 rc;` |
|         - |  3245 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        75 |  3246 | `	pName = &pGen->pIn->sData;` |
|        75 |  3247 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        75 |  3248 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        75 |  3249 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|         9 |  3250 | `		SyToken *pTmp,*pNext = 0;` |
|         - |  3251 | `		/* Compile arguments one after one */` |
|         9 |  3252 | `		pTmp = pGen->pEnd;` |
|         - |  3253 | `		/* Symisc eXtension to the PHP programming language:` |
|         - |  3254 | `		 * 'echo' can be used in the context of a function which` |
|         - |  3255 | `		 *  mean that the following expression is valid:` |
|         - |  3256 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|         - |  3257 | `		 */` |
|         9 |  3258 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        17 |  3259 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|         9 |  3260 | `			if( pGen->pIn < pNext ){` |
|         9 |  3261 | `				pGen->pEnd = pNext;` |
|         9 |  3262 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|         9 |  3263 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3264 | `					return SXERR_ABORT;` |
|         - |  3265 | `				}` |
|         9 |  3266 | `				if( rc != SXERR_EMPTY ){` |
|         - |  3267 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|         - |  3268 | `					 * without the overhead of a function call.` |
|         - |  3269 | `					 * This is a very powerful optimization that improve` |
|         - |  3270 | `					 * performance greatly.` |
|         - |  3271 | `					 */` |
|         9 |  3272 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|         4 |  3273 | `				}` |
|         4 |  3274 | `			}` |
|         - |  3275 | `			/* Jump trailing commas */` |
|         9 |  3276 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|       ! 0 |  3277 | `				pNext++;` |
|       ! 0 |  3278 | `			}` |
|         9 |  3279 | `			pGen->pIn = pNext;` |
|         1 |  3280 | `		}` |
|         - |  3281 | `		/* Restore token stream */` |
|         9 |  3282 | `		pGen->pEnd = pTmp;` |
|         5 |  3283 | `	}else{` |
|        67 |  3284 | `		sxi32 nArg = 0;` |
|        67 |  3285 | `		sxu32 nIdx = 0;` |
|        67 |  3286 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        67 |  3287 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3288 | `			return SXERR_ABORT;` |
|        67 |  3289 | `		}else if(rc != SXERR_EMPTY ){` |
|        67 |  3290 | `			nArg = 1;` |
|        31 |  3291 | `		}` |
|        67 |  3292 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3293 | `			ph7_value *pObj;` |
|         - |  3294 | `			/* Emit the call instruction */` |
|        31 |  3295 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        31 |  3296 | `			if( pObj == 0 ){` |
|       ! 0 |  3297 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3298 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3299 | `				return SXERR_ABORT;` |
|         - |  3300 | `			}` |
|        31 |  3301 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3302 | `			/* Install in the literal table */` |
|        31 |  3303 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        13 |  3304 | `		}` |
|         - |  3305 | `		/* Emit the call instruction */` |
|        67 |  3306 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        67 |  3307 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3308 | `	}` |
|         - |  3309 | `	/* Node successfully compiled */` |
|        75 |  3310 | `	return SXRET_OK;` |
|        40 |  3311 | `}` |
|         - |  3312 | `/*` |
|         - |  3313 | ` * Compile a node holding a variable declaration.` |
|         - |  3314 | ` * According to the PHP language reference` |
|         - |  3315 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|         - |  3316 | ` *  The variable name is case-sensitive.` |
|         - |  3317 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|         - |  3318 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3319 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|         - |  3320 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|         - |  3321 | ` *  Note: $this is a special variable that can't be assigned.` |
|         - |  3322 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|         - |  3323 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|         - |  3324 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|         - |  3325 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|         - |  3326 | ` *  the chapter on Expressions.` |
|         - |  3327 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|         - |  3328 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|         - |  3329 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|         - |  3330 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|         - |  3331 | ` *  is being assigned (the source variable).` |
|         - |  3332 | ` */` |
|  15900198 |  3333 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3334 | `{` |
|  15900203 |  3335 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3336 | `	sxi32 iVv;` |
|         - |  3337 | `	sxi32 iP1;` |
|         - |  3338 | `	void *p3;` |
|         - |  3339 | `	sxi32 rc;` |
|  15900203 |  3340 | `	iVv = -1; /* Variable variable counter */` |
|  31800413 |  3341 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  15900215 |  3342 | `		pGen->pIn++;` |
|  15900215 |  3343 | `		iVv++;` |
|         5 |  3344 | `	}` |
|  15900203 |  3345 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3346 | `		/* Invalid variable name */` |
|       ! 0 |  3347 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3348 | `		if( rc == SXERR_ABORT ){` |
|         - |  3349 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3350 | `			return SXERR_ABORT;` |
|         - |  3351 | `		}` |
|       ! 0 |  3352 | `		return SXRET_OK;` |
|         - |  3353 | `	}` |
|  15900203 |  3354 | `	p3  = 0;` |
|  15900203 |  3355 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|         - |  3356 | `		/* Dynamic variable creation */` |
|        19 |  3357 | `		pGen->pIn++;  /* Jump the open curly */` |
|        19 |  3358 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|        19 |  3359 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3360 | `			/* Empty expression */` |
|         - |  3361 | `			{` |
|         - |  3362 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|         - |  3363 | `			 * the "expecting" tail only appears when something could still follow. */` |
|         3 |  3364 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|         3 |  3365 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|         1 |  3366 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|         - |  3367 | `			}` |
|         3 |  3368 | `			return SXRET_OK;` |
|         - |  3369 | `		}` |
|         - |  3370 | `		/* Compile the expression holding the variable name */` |
|        16 |  3371 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        16 |  3372 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3373 | `			return SXERR_ABORT;` |
|        16 |  3374 | `		}else if( rc == SXERR_EMPTY ){` |
|         3 |  3375 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|         3 |  3376 | `			return SXRET_OK;` |
|         - |  3377 | `		}` |
|         7 |  3378 | `	}else{` |
|         - |  3379 | `		SyHashEntry *pEntry;` |
|         - |  3380 | `		SyString *pName;` |
|  15900187 |  3381 | `		char *zName = 0;` |
|         - |  3382 | `		/* Extract variable name */` |
|  15900187 |  3383 | `		pName = &pGen->pIn->sData;` |
|         - |  3384 | `		/* Advance the stream cursor */` |
|  15900187 |  3385 | `		pGen->pIn++;` |
|  15900187 |  3386 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  15900187 |  3387 | `		if( pEntry == 0 ){` |
|         - |  3388 | `			/* Duplicate name */` |
|    918167 |  3389 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    918167 |  3390 | `			if( zName == 0 ){` |
|       ! 0 |  3391 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3392 | `				return SXERR_ABORT;` |
|         - |  3393 | `			}` |
|         - |  3394 | `			/* Install in the hashtable */` |
|    918167 |  3395 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    459086 |  3396 | `		}else{` |
|         - |  3397 | `			/* Name already available */` |
|  14982025 |  3398 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3399 | `		}` |
|  15900187 |  3400 | `		p3 = (void *)zName;` |
|         - |  3401 | `	}` |
|  15900199 |  3402 | `	iP1 = 0;` |
|  15900199 |  3403 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   4734587 |  3404 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3405 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   4730767 |  3406 | `			iP1 = 1;` |
|   2365381 |  3407 | `		}` |
|   2367291 |  3408 | `	}` |
|         - |  3409 | `	/* Emit the load instruction */` |
|  15900199 |  3410 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  15900211 |  3411 | `	while( iVv > 0 ){` |
|        13 |  3412 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3413 | `		iVv--;` |
|         1 |  3414 | `	}` |
|         - |  3415 | `	/* Node successfully compiled */` |
|  15900199 |  3416 | `	return SXRET_OK;` |
|   7950104 |  3417 | `}` |
|         - |  3418 | `/*` |
|         - |  3419 | ` * Load a literal.` |
|         - |  3420 | ` */` |
|  10772862 |  3421 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3422 | `{` |
|  10772867 |  3423 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3424 | `	ph7_value *pObj;` |
|         - |  3425 | `	SyString *pStr;` |
|         - |  3426 | `	sxu32 nIdx;` |
|         - |  3427 | `	/* Extract token value */` |
|  10772867 |  3428 | `	pStr = &pToken->sData;` |
|         - |  3429 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  10772867 |  3430 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2080337 |  3431 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3432 | `			/* NULL constant are always indexed at 0 */` |
|    866425 |  3433 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    866425 |  3434 | `			return SXRET_OK;` |
|   1213917 |  3435 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3436 | `			/* TRUE constant are always indexed at 1 */` |
|    274433 |  3437 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    274433 |  3438 | `			return SXRET_OK;` |
|         5 |  3439 | `		}` |
|  10062028 |  3440 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1799502 |  3441 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3442 | `			/* FALSE constant are always indexed at 2 */` |
|    615601 |  3443 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    615601 |  3444 | `			return SXRET_OK;` |
|   8463131 |  3445 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    772384 |  3446 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3447 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     11399 |  3448 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     11399 |  3449 | `			if( pObj == 0 ){` |
|       ! 0 |  3450 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3451 | `				return SXERR_ABORT;` |
|         - |  3452 | `			}` |
|     11399 |  3453 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3454 | `			/* Emit the load constant instruction */` |
|     11399 |  3455 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     11399 |  3456 | `			return SXRET_OK;` |
|   8157060 |  3457 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    183030 |  3458 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3459 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         7 |  3460 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3461 | `			if( pObj == 0 ){` |
|       ! 0 |  3462 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3463 | `				return SXERR_ABORT;` |
|         - |  3464 | `			}` |
|         7 |  3465 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3466 | `				SyString sNs;` |
|         7 |  3467 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         7 |  3468 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         4 |  3469 | `			}else{` |
|       ! 0 |  3470 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3471 | `			}` |
|         7 |  3472 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         7 |  3473 | `			return SXRET_OK;` |
|   8161050 |  3474 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    376759 |  3475 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   8251249 |  3476 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    371444 |  3477 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3478 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3479 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3480 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3481 | `				/* Point to the upper block */` |
|        11 |  3482 | `				pBlock = pBlock->pParent;` |
|         1 |  3483 | `			}` |
|        11 |  3484 | `			if( pBlock == 0 ){` |
|         - |  3485 | `				/* Called in the global scope,load NULL */` |
|         5 |  3486 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3487 | `			}else{` |
|         - |  3488 | `				/* Extract the target function/method */` |
|         7 |  3489 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3490 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|         7 |  3491 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3492 | `				if( pObj == 0 ){` |
|       ! 0 |  3493 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3494 | `					return SXERR_ABORT;` |
|         - |  3495 | `				}` |
|         - |  3496 | `				/*` |
|         - |  3497 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|         - |  3498 | `				 * function name inside a plain function (php does not answer "" there —` |
|         - |  3499 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|         - |  3500 | `				 * unqualified in every method).` |
|         - |  3501 | `				 */` |
|         8 |  3502 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 |  3503 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         - |  3504 | `					SyBlob sQual;` |
|         - |  3505 | `					SyString sOut;` |
|         3 |  3506 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|         3 |  3507 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|         3 |  3508 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|         3 |  3509 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|         3 |  3510 | `					SyBlobRelease(&sQual);` |
|         2 |  3511 | `				}else{` |
|         5 |  3512 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3513 | `				}` |
|         - |  3514 | `				/* Emit the load constant instruction */` |
|         7 |  3515 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3516 | `			}` |
|        11 |  3517 | `			return SXRET_OK;` |
|         - |  3518 | `	}` |
|         - |  3519 | `	/* Query literal table */` |
|   9005013 |  3520 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3521 | `		ph7_value *pLitObj;` |
|         - |  3522 | `		/* Unknown literal,install it in the literal table */` |
|   1725967 |  3523 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1725967 |  3524 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3525 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3526 | `			return SXERR_ABORT;` |
|         - |  3527 | `		}` |
|   1725967 |  3528 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1725967 |  3529 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    862981 |  3530 | `	}` |
|         - |  3531 | `	/* Emit the load constant instruction */` |
|   9005013 |  3532 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9005013 |  3533 | `	return SXRET_OK;` |
|   5386436 |  3534 | `}` |
|         - |  3535 | `/*` |
|         - |  3536 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3537 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3538 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3539 | ` * Otherwise, load the simple literal directly.` |
|         - |  3540 | ` */` |
|  10776706 |  3541 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3542 | `{` |
|         - |  3543 | `	sxi32 rc;` |
|  10776711 |  3544 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3545 | `		return SXRET_OK;` |
|         - |  3546 | `	}` |
|         - |  3547 | `	/* Check if this is a multi-token namespace path */` |
|  10776711 |  3548 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3549 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3849 |  3550 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3849 |  3551 | `		int isAbsolute = 0;` |
|      3849 |  3552 | `		SyBlobReset(pWorker);` |
|         - |  3553 | `		/* Check for leading backslash (absolute path) */` |
|      3849 |  3554 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3847 |  3555 | `			isAbsolute = 1;` |
|      3847 |  3556 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1921 |  3557 | `		}` |
|         - |  3558 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3849 |  3559 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3560 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3561 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3562 | `		}` |
|         - |  3563 | `		/* Collect all path components */` |
|      3957 |  3564 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      3957 |  3565 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        58 |  3566 | `				SyBlobAppend(pWorker,"\\",1);` |
|        31 |  3567 | `			}else{` |
|      3903 |  3568 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3569 | `			}` |
|      3957 |  3570 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3849 |  3571 | `				pGen->pIn++;` |
|      3849 |  3572 | `				break;` |
|         - |  3573 | `			}` |
|       112 |  3574 | `			pGen->pIn++;` |
|         4 |  3575 | `		}` |
|      3849 |  3576 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3577 | `			ph7_value *pObj;` |
|         - |  3578 | `			SyString sPath;` |
|         - |  3579 | `			sxu32 nIdx;` |
|      3849 |  3580 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3581 | `			/* Install in the literal table */` |
|      3849 |  3582 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3819 |  3583 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3819 |  3584 | `				if( pObj == 0 ){` |
|       ! 0 |  3585 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3586 | `					return SXERR_ABORT;` |
|         - |  3587 | `				}` |
|      3819 |  3588 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3819 |  3589 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1907 |  3590 | `			}` |
|         - |  3591 | `			/* Emit the load constant instruction.` |
|         - |  3592 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3593 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5771 |  3594 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1922 |  3595 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1922 |  3596 | `				nIdx,0,0);` |
|      3849 |  3597 | `			return SXRET_OK;` |
|         - |  3598 | `		}` |
|       ! 0 |  3599 | `	}` |
|         - |  3600 | `	/* Single-token literal: load directly */` |
|  10772867 |  3601 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  10772867 |  3602 | `	return rc;` |
|   5388358 |  3603 | `}` |
|         - |  3604 | `/*` |
|         - |  3605 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3606 | ` */` |
|         - |  3607 | `/*` |
|         - |  3608 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3609 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3610 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3611 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3612 | ` */` |
|       ! 0 |  3613 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3614 | `{` |
|       ! 0 |  3615 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3616 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3617 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3618 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3619 | `}` |
|  10776706 |  3620 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3621 | `{` |
|         - |  3622 | `	sxi32 rc;` |
|  10776711 |  3623 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  10776711 |  3624 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3625 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3626 | `		return rc;` |
|         - |  3627 | `	}` |
|         - |  3628 | `	/* Node successfully compiled */` |
|  10776711 |  3629 | `	return SXRET_OK;` |
|   5388358 |  3630 | `}` |
|         - |  3631 | `/*` |
|         - |  3632 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3633 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3634 | ` */` |
|         8 |  3635 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3636 | `{` |
|         - |  3637 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3638 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3639 | `		pGen->pIn++;` |
|         1 |  3640 | `	}` |
|         9 |  3641 | `	return SXRET_OK;` |
|         1 |  3642 | `}` |
|         - |  3643 | `/*` |
|         - |  3644 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3645 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3646 | ` */` |
|    288716 |  3647 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3648 | `{` |
|    288721 |  3649 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3845 |  3650 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3651 | `			return TRUE;` |
|      3843 |  3652 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3653 | `			return TRUE;` |
|         5 |  3654 | `		}` |
|    286798 |  3655 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7617 |  3656 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3657 | `			return TRUE;` |
|         - |  3658 | `		}` |
|      3805 |  3659 | `	}` |
|         - |  3660 | `	/* Not a reserved constant */` |
|    288713 |  3661 | `	return FALSE;` |
|    144363 |  3662 | `}` |
|         - |  3663 | `/*` |
|         - |  3664 | ` * Compile the 'const' statement.` |
|         - |  3665 | ` * According to the PHP language reference` |
|         - |  3666 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3667 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3668 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3669 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3670 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3671 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3672 | ` *  Syntax` |
|         - |  3673 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3674 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3675 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3676 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3677 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3678 | ` *  to get a list of all defined constants.` |
|         - |  3679 | ` *` |
|         - |  3680 | ` * Symisc eXtension.` |
|         - |  3681 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3682 | ` *  would allow only simple scalar value.` |
|         - |  3683 | ` *  Example` |
|         - |  3684 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3685 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3686 | ` */` |
|        48 |  3687 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3688 | `{` |
|         - |  3689 | `	SySet *pConsCode,*pInstrContainer;` |
|        53 |  3690 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3691 | `	SyString *pName;` |
|         - |  3692 | `	sxi32 rc;` |
|        53 |  3693 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        53 |  3694 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3695 | `		/* Invalid constant name */` |
|         9 |  3696 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|         9 |  3697 | `		if( rc == SXERR_ABORT ){` |
|         - |  3698 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3699 | `			return SXERR_ABORT;` |
|         - |  3700 | `		}` |
|         9 |  3701 | `		goto Synchronize;` |
|         - |  3702 | `	}` |
|         - |  3703 | `	/* Peek constant name */` |
|        46 |  3704 | `	pName = &pGen->pIn->sData;` |
|         - |  3705 | `	/* Make sure the constant name isn't reserved */` |
|        46 |  3706 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3707 | `		/* Reserved constant */` |
|        10 |  3708 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        10 |  3709 | `		if( rc == SXERR_ABORT ){` |
|         - |  3710 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3711 | `			return SXERR_ABORT;` |
|         - |  3712 | `		}` |
|        10 |  3713 | `		goto Synchronize;` |
|         - |  3714 | `	}` |
|        37 |  3715 | `	pGen->pIn++;` |
|        37 |  3716 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3717 | `		/* Invalid statement*/` |
|         6 |  3718 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|         6 |  3719 | `		if( rc == SXERR_ABORT ){` |
|         - |  3720 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3721 | `			return SXERR_ABORT;` |
|         - |  3722 | `		}` |
|         6 |  3723 | `		goto Synchronize;` |
|         - |  3724 | `	}` |
|        32 |  3725 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3726 | `	/* Allocate a new constant value container */` |
|        32 |  3727 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        32 |  3728 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3729 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3730 | `		return SXERR_ABORT;` |
|         - |  3731 | `	}` |
|        32 |  3732 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3733 | `	/* Swap bytecode container */` |
|        32 |  3734 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        32 |  3735 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3736 | `	/* Compile constant value */` |
|        32 |  3737 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3738 | `	/* Emit the done instruction */` |
|        32 |  3739 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        32 |  3740 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        32 |  3741 | `	if( rc == SXERR_ABORT ){` |
|         - |  3742 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3743 | `		return SXERR_ABORT;` |
|         - |  3744 | `	}` |
|        32 |  3745 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3746 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3747 | `	{` |
|         - |  3748 | `		SyBlob sFQN;` |
|         - |  3749 | `		SyString sFQNStr;` |
|        32 |  3750 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        32 |  3751 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        32 |  3752 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        47 |  3753 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        30 |  3754 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        32 |  3755 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3756 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3757 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3758 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3759 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3760 | `			if( pCEntry ){` |
|         5 |  3761 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3762 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3763 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3764 | `					return SXERR_ABORT;` |
|         - |  3765 | `				}` |
|         2 |  3766 | `			}` |
|         2 |  3767 | `		}` |
|        32 |  3768 | `		SyBlobRelease(&sFQN);` |
|         - |  3769 | `	}` |
|        32 |  3770 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3771 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3772 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3773 | `	}` |
|        32 |  3774 | `	return SXRET_OK;` |
|         9 |  3775 | `Synchronize:` |
|         - |  3776 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3777 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        41 |  3778 | `		pGen->pIn++;` |
|         3 |  3779 | `	}` |
|        22 |  3780 | `	return SXRET_OK;` |
|        29 |  3781 | `}` |
|         - |  3782 | `/*` |
|         - |  3783 | ` * Compile the 'continue' statement.` |
|         - |  3784 | ` * According to the PHP language reference` |
|         - |  3785 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3786 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3787 | ` *  iteration.` |
|         - |  3788 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3789 | ` *  the purposes of continue.` |
|         - |  3790 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3791 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3792 | ` *  Note:` |
|         - |  3793 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3794 | ` */` |
|         - |  3795 | `/*` |
|         - |  3796 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3797 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3798 | ` * break/continue crosses a try boundary.` |
|         - |  3799 | ` *` |
|         - |  3800 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3801 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3802 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3803 | ` */` |
|    114034 |  3804 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3805 | `{` |
|    114039 |  3806 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    114039 |  3807 | `	int nInlineTry = 0;` |
|    531845 |  3808 | `	while( pBlock && pBlock != pTarget ){` |
|    417811 |  3809 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3810 | `			if( pBlock->pUserData ){` |
|         - |  3811 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3812 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3813 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3814 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3815 | `				if( pGen->bInGenerator ){` |
|         3 |  3816 | `					nInlineTry++;` |
|         2 |  3817 | `				}else{` |
|         3 |  3818 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3819 | `				}` |
|         4 |  3820 | `			}else{` |
|         - |  3821 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3822 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3823 | `				break;` |
|         - |  3824 | `			}` |
|         2 |  3825 | `		}` |
|    417811 |  3826 | `		pBlock = pBlock->pParent;` |
|         5 |  3827 | `	}` |
|    114039 |  3828 | `	return nInlineTry;` |
|         5 |  3829 | `}` |
|     56990 |  3830 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3831 | `{` |
|         - |  3832 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3833 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3834 | `	sxu32 nLineLocal;` |
|         - |  3835 | `	sxi32 rc;` |
|     56995 |  3836 | `	nLineLocal = pGen->pIn->nLine;` |
|     56995 |  3837 | `	iLevel = 0;` |
|         - |  3838 | `	/* Jump the 'continue' keyword */` |
|     56995 |  3839 | `	pGen->pIn++;` |
|     56995 |  3840 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3841 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3842 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3843 | `		 */` |
|         - |  3844 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3845 | `		char *zAlloc = 0;` |
|         - |  3846 | `		SyString sNum;` |
|        17 |  3847 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3848 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3849 | `			return SXERR_ABORT;` |
|         - |  3850 | `		}` |
|        17 |  3851 | `		if( rc == SXRET_OK ){` |
|        20 |  3852 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3853 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3854 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3855 | `				return SXERR_ABORT;` |
|         - |  3856 | `			}` |
|        14 |  3857 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3858 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3859 | `		}` |
|        17 |  3860 | `		if( iLevel < 2 ){` |
|         3 |  3861 | `			iLevel = 0;` |
|         1 |  3862 | `		}` |
|        17 |  3863 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3864 | `	}` |
|         - |  3865 | `	/* Point to the target loop */` |
|     56995 |  3866 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     56995 |  3867 | `	if( pLoop == 0 ){` |
|         - |  3868 | `		/* Illegal continue */` |
|        12 |  3869 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        12 |  3870 | `		if( rc == SXERR_ABORT ){` |
|         - |  3871 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3872 | `			return SXERR_ABORT;` |
|         - |  3873 | `		}` |
|         7 |  3874 | `	}else{` |
|     56985 |  3875 | `		sxu32 nInstrIdx = 0;` |
|         - |  3876 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     56985 |  3877 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3878 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3879 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     56985 |  3880 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     56985 |  3881 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3882 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|         - |  3883 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|         - |  3884 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|         - |  3885 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|         5 |  3886 | `			if( iLevel < 1 ){` |
|         5 |  3887 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|         - |  3888 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|         - |  3889 | `					" Did you mean to use \"continue 2\"?");` |
|         2 |  3890 | `			}` |
|         5 |  3891 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3892 | `			if( rc == SXRET_OK ){` |
|         5 |  3893 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3894 | `			}` |
|         3 |  3895 | `		}else{` |
|         - |  3896 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     56981 |  3897 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     56981 |  3898 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  3899 | `				JumpFixup sJumpFix;` |
|         - |  3900 | `				/* Post-continue */` |
|     18997 |  3901 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     18997 |  3902 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     18997 |  3903 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|      9496 |  3904 | `			}` |
|         - |  3905 | `		}` |
|         - |  3906 | `	}` |
|     56995 |  3907 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3908 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3909 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  3910 | `	}` |
|         - |  3911 | `	/* Statement successfully compiled */` |
|     56995 |  3912 | `	return SXRET_OK;` |
|     28500 |  3913 | `}` |
|         - |  3914 | `/*` |
|         - |  3915 | ` * Compile the 'break' statement.` |
|         - |  3916 | ` * According to the PHP language reference` |
|         - |  3917 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  3918 | ` *  structure.` |
|         - |  3919 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  3920 | ` *  enclosing structures are to be broken out of.` |
|         - |  3921 | ` */` |
|     57070 |  3922 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  3923 | `{` |
|         - |  3924 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3925 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3926 | `	sxi32 rc;` |
|     57075 |  3927 | `	iLevel = 0;` |
|         - |  3928 | `	/* Jump the 'break' keyword */` |
|     57075 |  3929 | `	pGen->pIn++;` |
|     57075 |  3930 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3931 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3932 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3933 | `		 */` |
|         - |  3934 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        18 |  3935 | `		char *zAlloc = 0;` |
|         - |  3936 | `		SyString sNum;` |
|        18 |  3937 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        18 |  3938 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3939 | `			return SXERR_ABORT;` |
|         - |  3940 | `		}` |
|        18 |  3941 | `		if( rc == SXRET_OK ){` |
|        21 |  3942 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3943 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  3944 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3945 | `				return SXERR_ABORT;` |
|         - |  3946 | `			}` |
|        15 |  3947 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  3948 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3949 | `		}` |
|        18 |  3950 | `		if( iLevel < 2 ){` |
|         3 |  3951 | `			iLevel = 0;` |
|         1 |  3952 | `		}` |
|        18 |  3953 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3954 | `	}` |
|         - |  3955 | `	/* Extract the target loop */` |
|     57075 |  3956 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     57075 |  3957 | `	if( pLoop == 0 ){` |
|         - |  3958 | `		/* Illegal break */` |
|        20 |  3959 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        20 |  3960 | `		if( rc == SXERR_ABORT ){` |
|         - |  3961 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3962 | `			return SXERR_ABORT;` |
|         - |  3963 | `		}` |
|        12 |  3964 | `	}else{` |
|         - |  3965 | `		sxu32 nInstrIdx;` |
|         - |  3966 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     57059 |  3967 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3968 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     57059 |  3969 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     57059 |  3970 | `		if( rc == SXRET_OK ){` |
|         - |  3971 | `			/* Fix the jump later when the jump destination is resolved */` |
|     57059 |  3972 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     28527 |  3973 | `		}` |
|         - |  3974 | `	}` |
|     57075 |  3975 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3976 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3977 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  3978 | `	}` |
|         - |  3979 | `	/* Statement successfully compiled */` |
|     57075 |  3980 | `	return SXRET_OK;` |
|     28540 |  3981 | `}` |
|         - |  3982 | `/*` |
|         - |  3983 | ` * Compile or record a label.` |
|         - |  3984 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  3985 | ` * Example` |
|         - |  3986 | ` *  goto LABEL;` |
|         - |  3987 | ` *   echo 'Foo';` |
|         - |  3988 | ` *  LABEL:` |
|         - |  3989 | ` *   echo 'Bar';` |
|         - |  3990 | ` */` |
|       112 |  3991 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  3992 | `{` |
|         - |  3993 | `	GenBlock *pBlock;` |
|         - |  3994 | `	Label sLabel;` |
|         - |  3995 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|         - |  3996 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|         - |  3997 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|         - |  3998 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|         - |  3999 | `	{` |
|       117 |  4000 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4001 | `		char *zDup;` |
|         - |  4002 | `		/* Initialize label fields */` |
|       117 |  4003 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4004 | `		/* Duplicate label name */` |
|       117 |  4005 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       117 |  4006 | `		if( zDup == 0 ){` |
|       ! 0 |  4007 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4008 | `			return SXERR_ABORT;` |
|         - |  4009 | `		}` |
|       117 |  4010 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       117 |  4011 | `		sLabel.bRef  = FALSE;` |
|       117 |  4012 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       117 |  4013 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       117 |  4014 | `		pBlock = pGen->pCurrent;` |
|       233 |  4015 | `		while( pBlock ){` |
|       143 |  4016 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        27 |  4017 | `				break;` |
|         - |  4018 | `			}` |
|         - |  4019 | `			/* Point to the upper block */` |
|       121 |  4020 | `			pBlock = pBlock->pParent;` |
|         5 |  4021 | `		}` |
|       117 |  4022 | `		if( pBlock ){` |
|        27 |  4023 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        16 |  4024 | `		}else{` |
|        95 |  4025 | `			sLabel.pFunc = 0;` |
|         - |  4026 | `		}` |
|         - |  4027 | `		/* Insert in label set */` |
|       117 |  4028 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  4029 | `	}` |
|       117 |  4030 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  4031 | `	return SXRET_OK;` |
|        61 |  4032 | `}` |
|         - |  4033 | `/*` |
|         - |  4034 | ` * Compile the so hated 'goto' statement.` |
|         - |  4035 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  4036 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  4037 | ` * a compiler it has to do this.` |
|         - |  4038 | ` * According to the PHP language reference manual` |
|         - |  4039 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  4040 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  4041 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  4042 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  4043 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  4044 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  4045 | ` *   of a multi-level break` |
|         - |  4046 | ` */` |
|       152 |  4047 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  4048 | `{` |
|         - |  4049 | `	JumpFixup sJump;` |
|         - |  4050 | `	sxi32 rc;` |
|       157 |  4051 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  4052 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  4053 | `		/* Missing label */` |
|       ! 0 |  4054 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  4055 | `		if( rc == SXERR_ABORT ){` |
|         - |  4056 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4057 | `			return SXERR_ABORT;` |
|         - |  4058 | `		}` |
|       ! 0 |  4059 | `		return SXRET_OK;` |
|         - |  4060 | `	}` |
|       157 |  4061 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         6 |  4062 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|         6 |  4063 | `		if( rc == SXERR_ABORT ){` |
|         - |  4064 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4065 | `			return SXERR_ABORT;` |
|         - |  4066 | `		}` |
|         4 |  4067 | `	}else{` |
|       153 |  4068 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4069 | `		GenBlock *pBlock;` |
|         - |  4070 | `		char *zDup;` |
|         - |  4071 | `		/* Prepare the jump destination */` |
|       153 |  4072 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  4073 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  4074 | `		/* Duplicate label name */` |
|       153 |  4075 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  4076 | `		if( zDup == 0 ){` |
|       ! 0 |  4077 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4078 | `			return SXERR_ABORT;` |
|         - |  4079 | `		}` |
|       153 |  4080 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|         - |  4081 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|       153 |  4082 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|         - |  4083 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|         - |  4084 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|       153 |  4085 | `		pBlock = pGen->pCurrent;` |
|       327 |  4086 | `		while( pBlock ){` |
|       205 |  4087 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|        30 |  4088 | `				break;` |
|         - |  4089 | `			}` |
|         - |  4090 | `			/* Point to the upper block */` |
|       179 |  4091 | `			pBlock = pBlock->pParent;` |
|         5 |  4092 | `		}` |
|       153 |  4093 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        30 |  4094 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        17 |  4095 | `		}else{` |
|       127 |  4096 | `			sJump.pFunc = 0;` |
|         - |  4097 | `		}` |
|         - |  4098 | `		/* Emit the unconditional jump */` |
|       153 |  4099 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  4100 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  4101 | `		}` |
|         - |  4102 | `	}` |
|       157 |  4103 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  4104 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  4105 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  4106 | `	}` |
|         - |  4107 | `	/* Statement successfully compiled */` |
|       157 |  4108 | `	return SXRET_OK;` |
|        81 |  4109 | `}` |
|         - |  4110 | `/*` |
|         - |  4111 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  4112 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  4113 | ` * failure.` |
|         - |  4114 | ` */` |
|        20 |  4115 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         2 |  4116 | `{` |
|         - |  4117 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  4118 | `	sxu32 nRawObj;` |
|        10 |  4119 | `	sxu32 nObjIdx;` |
|         - |  4120 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  4121 | `	 * a PHP block.` |
|         - |  4122 | `	 */` |
|        10 |  4123 | `Consume:` |
|        22 |  4124 | `	nRawObj = nObjIdx = 0;` |
|        22 |  4125 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  4126 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  4127 | `		if( pRawObj == 0 ){` |
|       ! 0 |  4128 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4129 | `			return SXERR_ABORT;` |
|         - |  4130 | `		}` |
|         - |  4131 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  4132 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  4133 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  4134 | `		++nRawObj;` |
|       ! 0 |  4135 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  4136 | `	}` |
|        22 |  4137 | `	if( nRawObj > 0 ){` |
|         - |  4138 | `		/* Emit the consume instruction */` |
|       ! 0 |  4139 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4140 | `	}` |
|        22 |  4141 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  4142 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  4143 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  4144 | `		SySetReset(pTokenSet);` |
|       ! 0 |  4145 | `		SySetReset(&pGen->aTrivia);` |
|         - |  4146 | `		/* Tokenize input */` |
|       ! 0 |  4147 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  4148 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  4149 | `		/* Point to the fresh token stream */` |
|       ! 0 |  4150 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  4151 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  4152 | `		/* Advance the stream cursor */` |
|       ! 0 |  4153 | `		pGen->pRawIn++;` |
|         - |  4154 | `		/* TICKET 1433-011 */` |
|       ! 0 |  4155 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4156 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4157 | `			sxi32 rc;` |
|         - |  4158 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4159 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4160 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4161 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4162 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4163 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4164 | `				return SXERR_ABORT;` |
|       ! 0 |  4165 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4166 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4167 | `			}` |
|       ! 0 |  4168 | `			goto Consume;` |
|         - |  4169 | `		}` |
|       ! 0 |  4170 | `	}else{` |
|         - |  4171 | `		/* No more chunks to process */` |
|        22 |  4172 | `		pGen->pIn = pGen->pEnd;` |
|        22 |  4173 | `		return SXERR_EOF;` |
|         - |  4174 | `	}` |
|       ! 0 |  4175 | `	return SXRET_OK;` |
|        12 |  4176 | `}` |
|         - |  4177 | `/*` |
|         - |  4178 | ` * Compile a PHP block.` |
|         - |  4179 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4180 | ` * optionally delimited by braces {}.` |
|         - |  4181 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4182 | ` * and this function takes care of generating the appropriate error` |
|         - |  4183 | ` * message.` |
|         - |  4184 | ` */` |
|   5169624 |  4185 | `static sxi32 PH7_CompileBlock(` |
|         - |  4186 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4187 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4188 | `	)` |
|         5 |  4189 | `{` |
|         - |  4190 | `	sxi32 rc;` |
|         - |  4191 | `	sxu32 nLine;` |
|   5169629 |  4192 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5168493 |  4193 | `		nLine = pGen->pIn->nLine;` |
|   5168493 |  4194 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5168493 |  4195 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4196 | `			return SXERR_ABORT;` |
|         - |  4197 | `		}` |
|   5168493 |  4198 | `		pGen->pIn++;` |
|         - |  4199 | `		/* Compile until we hit the closing braces '}' */` |
|   7561005 |  4200 | `		for(;;){` |
|  15122015 |  4201 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        22 |  4202 | `				rc = GenStateNextChunk(&(*pGen));` |
|        22 |  4203 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4204 | `			 	   return SXERR_ABORT;` |
|         - |  4205 | `				}` |
|        22 |  4206 | `				if( rc == SXERR_EOF ){` |
|         - |  4207 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4208 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        22 |  4209 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        22 |  4210 | `					break;` |
|         - |  4211 | `				}` |
|       ! 0 |  4212 | `			}` |
|  15121995 |  4213 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4214 | `				/* Closing braces found,break immediately*/` |
|   5168473 |  4215 | `				pGen->pIn++;` |
|   5168473 |  4216 | `				break;` |
|         - |  4217 | `			}` |
|         - |  4218 | `			/* Compile a single statement */` |
|   9953527 |  4219 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   9953527 |  4220 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4221 | `				return SXERR_ABORT;` |
|         - |  4222 | `			}` |
|         5 |  4223 | `		}` |
|   5168493 |  4224 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   2585385 |  4225 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4226 | `		pGen->pIn++;` |
|       ! 0 |  4227 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4228 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4229 | `			return SXERR_ABORT;` |
|         - |  4230 | `		}` |
|         - |  4231 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4232 | `		for(;;){` |
|       ! 0 |  4233 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4234 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4235 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4236 | `			 	   return SXERR_ABORT;` |
|         - |  4237 | `				}` |
|       ! 0 |  4238 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4239 | `					/* No more token to process */` |
|       ! 0 |  4240 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4241 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4242 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4243 | `					}` |
|       ! 0 |  4244 | `					break;` |
|         - |  4245 | `				}` |
|       ! 0 |  4246 | `			}` |
|       ! 0 |  4247 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4248 | `				sxi32 nKwrd;` |
|         - |  4249 | `				/* Keyword found */` |
|       ! 0 |  4250 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4251 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4252 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4253 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4254 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4255 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4256 | `						}` |
|       ! 0 |  4257 | `						break;` |
|         - |  4258 | `				}` |
|       ! 0 |  4259 | `			}` |
|         - |  4260 | `			/* Compile a single statement */` |
|       ! 0 |  4261 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4262 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4263 | `				return SXERR_ABORT;` |
|         - |  4264 | `			}` |
|       ! 0 |  4265 | `		}` |
|       ! 0 |  4266 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4267 | `	}else{` |
|         - |  4268 | `		/* Compile a single statement */` |
|      1141 |  4269 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      1141 |  4270 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4271 | `			return SXERR_ABORT;` |
|         - |  4272 | `		}` |
|         - |  4273 | `	}` |
|         - |  4274 | `	/* Jump trailing semi-colons ';' */` |
|   5169629 |  4275 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4276 | `		pGen->pIn++;` |
|       ! 0 |  4277 | `	}` |
|   5169629 |  4278 | `	return SXRET_OK;` |
|   2584817 |  4279 | `}` |
|         - |  4280 | `/*` |
|         - |  4281 | ` * Compile the gentle 'while' statement.` |
|         - |  4282 | ` * According to the PHP language reference` |
|         - |  4283 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4284 | ` *  The basic form of a while statement is:` |
|         - |  4285 | ` *  while (expr)` |
|         - |  4286 | ` *   statement` |
|         - |  4287 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4288 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4289 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4290 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4291 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4292 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4293 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4294 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4295 | ` *  while (expr):` |
|         - |  4296 | ` *    statement` |
|         - |  4297 | ` *   endwhile;` |
|         - |  4298 | ` */` |
|     53286 |  4299 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4300 | `{` |
|     53291 |  4301 | `	GenBlock *pWhileBlock = 0;` |
|     53291 |  4302 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4303 | `	sxu32 nFalseJump;` |
|         - |  4304 | `	sxu32 nLine;` |
|         - |  4305 | `	sxi32 rc;` |
|     53291 |  4306 | `	nLine = pGen->pIn->nLine;` |
|         - |  4307 | `	/* Jump the 'while' keyword */` |
|     53291 |  4308 | `	pGen->pIn++;` |
|     53291 |  4309 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4310 | `		/* Syntax error */` |
|       ! 0 |  4311 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4312 | `		if( rc == SXERR_ABORT ){` |
|         - |  4313 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4314 | `			return SXERR_ABORT;` |
|         - |  4315 | `		}` |
|       ! 0 |  4316 | `		goto Synchronize;` |
|         - |  4317 | `	}` |
|         - |  4318 | `	/* Jump the left parenthesis '(' */` |
|     53291 |  4319 | `	pGen->pIn++;` |
|         - |  4320 | `	/* Create the loop block */` |
|     53291 |  4321 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     53291 |  4322 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4323 | `		return SXERR_ABORT;` |
|         - |  4324 | `	}` |
|         - |  4325 | `	/* Delimit the condition */` |
|     53291 |  4326 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     53291 |  4327 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4328 | `		/* Empty expression */` |
|         3 |  4329 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4330 | `		if( rc == SXERR_ABORT ){` |
|         - |  4331 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4332 | `			return SXERR_ABORT;` |
|         - |  4333 | `		}` |
|         1 |  4334 | `	}` |
|         - |  4335 | `	/* Swap token streams */` |
|     53291 |  4336 | `	pTmp = pGen->pEnd;` |
|     53291 |  4337 | `	pGen->pEnd = pEnd;` |
|         - |  4338 | `	/* Compile the expression */` |
|     53291 |  4339 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     53291 |  4340 | `	if( rc == SXERR_ABORT ){` |
|         - |  4341 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4342 | `		return SXERR_ABORT;` |
|         - |  4343 | `	}` |
|         - |  4344 | `	/* Update token stream */` |
|     53291 |  4345 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4346 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4347 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4348 | `			return SXERR_ABORT;` |
|         - |  4349 | `		}` |
|       ! 0 |  4350 | `		pGen->pIn++;` |
|       ! 0 |  4351 | `	}` |
|         - |  4352 | `	/* Synchronize pointers */` |
|     53291 |  4353 | `	pGen->pIn  = &pEnd[1];` |
|     53291 |  4354 | `	pGen->pEnd = pTmp;` |
|         - |  4355 | `	/* Emit the false jump */` |
|     53291 |  4356 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4357 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     53291 |  4358 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4359 | `	/* Compile the loop body */` |
|     53291 |  4360 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     53291 |  4361 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4362 | `		return SXERR_ABORT;` |
|         - |  4363 | `	}` |
|         - |  4364 | `	/* Emit the unconditional jump to the start of the loop */` |
|     53291 |  4365 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4366 | `	/* Fix all jumps now the destination is resolved */` |
|     53291 |  4367 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4368 | `	/* Release the loop block */` |
|     53291 |  4369 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4370 | `	/* Statement successfully compiled */` |
|     53291 |  4371 | `	return SXRET_OK;` |
|       ! 0 |  4372 | `Synchronize:` |
|         - |  4373 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4374 | `	 * compiling this erroneous block.` |
|         - |  4375 | `	 */` |
|       ! 0 |  4376 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4377 | `		pGen->pIn++;` |
|       ! 0 |  4378 | `	}` |
|       ! 0 |  4379 | `	return SXRET_OK;` |
|     26648 |  4380 | `}` |
|         - |  4381 | `/*` |
|         - |  4382 | ` * Compile the ugly do..while() statement.` |
|         - |  4383 | ` * According to the PHP language reference` |
|         - |  4384 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4385 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4386 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4387 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4388 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4389 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4390 | ` *  would end immediately).` |
|         - |  4391 | ` *  There is just one syntax for do-while loops:` |
|         - |  4392 | ` *  <?php` |
|         - |  4393 | ` *  $i = 0;` |
|         - |  4394 | ` *  do {` |
|         - |  4395 | ` *   echo $i;` |
|         - |  4396 | ` *  } while ($i > 0);` |
|         - |  4397 | ` * ?>` |
|         - |  4398 | ` */` |
|         2 |  4399 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4400 | `{` |
|         3 |  4401 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4402 | `	GenBlock *pDoBlock = 0;` |
|         - |  4403 | `	sxu32 nLine;` |
|         - |  4404 | `	sxi32 rc;` |
|         3 |  4405 | `	nLine = pGen->pIn->nLine;` |
|         - |  4406 | `	/* Jump the 'do' keyword */` |
|         3 |  4407 | `	pGen->pIn++;` |
|         - |  4408 | `	/* Create the loop block */` |
|         3 |  4409 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4410 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4411 | `		return SXERR_ABORT;` |
|         - |  4412 | `	}` |
|         - |  4413 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4414 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4415 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4416 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4417 | `		return SXERR_ABORT;` |
|         - |  4418 | `	}` |
|         3 |  4419 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4420 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4421 | `	}` |
|         3 |  4422 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4423 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4424 | `			/* Missing 'while' statement */` |
|         3 |  4425 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4426 | `			if( rc == SXERR_ABORT ){` |
|         - |  4427 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4428 | `				return SXERR_ABORT;` |
|         - |  4429 | `			}` |
|         3 |  4430 | `			goto Synchronize;` |
|         - |  4431 | `	}` |
|         - |  4432 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4433 | `	pGen->pIn++;` |
|       ! 0 |  4434 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4435 | `		/* Syntax error */` |
|       ! 0 |  4436 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4437 | `		if( rc == SXERR_ABORT ){` |
|         - |  4438 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4439 | `			return SXERR_ABORT;` |
|         - |  4440 | `		}` |
|       ! 0 |  4441 | `		goto Synchronize;` |
|         - |  4442 | `	}` |
|         - |  4443 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4444 | `	pGen->pIn++;` |
|         - |  4445 | `	/* Delimit the condition */` |
|       ! 0 |  4446 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4447 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4448 | `		/* Empty expression */` |
|       ! 0 |  4449 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4450 | `		if( rc == SXERR_ABORT ){` |
|         - |  4451 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4452 | `			return SXERR_ABORT;` |
|         - |  4453 | `		}` |
|       ! 0 |  4454 | `		goto Synchronize;` |
|         - |  4455 | `	}` |
|         - |  4456 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4457 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4458 | `		JumpFixup *aPost;` |
|         - |  4459 | `		VmInstr *pInstr;` |
|         - |  4460 | `		sxu32 nJumpDest;` |
|         - |  4461 | `		sxu32 n;` |
|       ! 0 |  4462 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4463 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4464 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4465 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4466 | `			if( pInstr ){` |
|         - |  4467 | `				/* Fix */` |
|       ! 0 |  4468 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4469 | `			}` |
|       ! 0 |  4470 | `		}` |
|       ! 0 |  4471 | `	}` |
|         - |  4472 | `	/* Swap token streams */` |
|       ! 0 |  4473 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4474 | `	pGen->pEnd = pEnd;` |
|         - |  4475 | `	/* Compile the expression */` |
|       ! 0 |  4476 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4477 | `	if( rc == SXERR_ABORT ){` |
|         - |  4478 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4479 | `		return SXERR_ABORT;` |
|         - |  4480 | `	}` |
|         - |  4481 | `	/* Update token stream */` |
|       ! 0 |  4482 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4483 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4484 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4485 | `			return SXERR_ABORT;` |
|         - |  4486 | `		}` |
|       ! 0 |  4487 | `		pGen->pIn++;` |
|       ! 0 |  4488 | `	}` |
|       ! 0 |  4489 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4490 | `	pGen->pEnd = pTmp;` |
|         - |  4491 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4492 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4493 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4494 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4495 | `	/* Release the loop block */` |
|       ! 0 |  4496 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4497 | `	/* Statement successfully compiled */` |
|       ! 0 |  4498 | `	return SXRET_OK;` |
|         1 |  4499 | `Synchronize:` |
|         - |  4500 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4501 | `	 * compiling this erroneous block.` |
|         - |  4502 | `	 */` |
|         3 |  4503 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4504 | `		pGen->pIn++;` |
|       ! 0 |  4505 | `	}` |
|         3 |  4506 | `	return SXRET_OK;` |
|         2 |  4507 | `}` |
|         - |  4508 | `/*` |
|         - |  4509 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4510 | ` * According to the PHP language reference` |
|         - |  4511 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4512 | ` *  The syntax of a for loop is:` |
|         - |  4513 | ` *  for (expr1; expr2; expr3)` |
|         - |  4514 | ` *   statement` |
|         - |  4515 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4516 | ` *  the beginning of the loop.` |
|         - |  4517 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4518 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4519 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4520 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4521 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4522 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4523 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4524 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4525 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4526 | ` *  of using the for truth expression.` |
|         - |  4527 | ` */` |
|     87452 |  4528 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4529 | `{` |
|     87457 |  4530 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|     87457 |  4531 | `	GenBlock *pForBlock = 0;` |
|         - |  4532 | `	sxu32 nFalseJump;` |
|         - |  4533 | `	sxu32 nLine;` |
|         - |  4534 | `	sxi32 rc;` |
|     87457 |  4535 | `	nLine = pGen->pIn->nLine;` |
|         - |  4536 | `	/* Jump the 'for' keyword */` |
|     87457 |  4537 | `	pGen->pIn++;` |
|     87457 |  4538 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4539 | `		/* Syntax error */` |
|       ! 0 |  4540 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4541 | `		if( rc == SXERR_ABORT ){` |
|         - |  4542 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4543 | `			return SXERR_ABORT;` |
|         - |  4544 | `		}` |
|       ! 0 |  4545 | `		return SXRET_OK;` |
|         - |  4546 | `	}` |
|         - |  4547 | `	/* Jump the left parenthesis '(' */` |
|     87457 |  4548 | `	pGen->pIn++;` |
|         - |  4549 | `	/* Delimit the init-expr;condition;post-expr */` |
|     87457 |  4550 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     87457 |  4551 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4552 | `		/* Empty expression */` |
|       ! 0 |  4553 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4554 | `		if( rc == SXERR_ABORT ){` |
|         - |  4555 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4556 | `			return SXERR_ABORT;` |
|         - |  4557 | `		}` |
|         - |  4558 | `		/* Synchronize */` |
|       ! 0 |  4559 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4560 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4561 | `			pGen->pIn++;` |
|       ! 0 |  4562 | `		}` |
|       ! 0 |  4563 | `		return SXRET_OK;` |
|         - |  4564 | `	}` |
|         - |  4565 | `	/* Swap token streams */` |
|     87457 |  4566 | `	pTmp = pGen->pEnd;` |
|     87457 |  4567 | `	pGen->pEnd = pEnd;` |
|         - |  4568 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4569 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4570 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4571 | `	 * compiled through this same window — recorded as a known leniency. */` |
|     87457 |  4572 | `	pGen->nCommaExprOk++;` |
|         - |  4573 | `	/* Compile initialization expressions if available */` |
|     87457 |  4574 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4575 | `	/* Pop operand lvalues */` |
|     87457 |  4576 | `	if( rc == SXERR_ABORT ){` |
|         - |  4577 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4578 | `		return SXERR_ABORT;` |
|     87457 |  4579 | `	}else if( rc != SXERR_EMPTY ){` |
|     76067 |  4580 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     38031 |  4581 | `	}` |
|     87457 |  4582 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4583 | `		/* Syntax error */` |
|       ! 0 |  4584 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4585 | `		if( rc == SXERR_ABORT ){` |
|         - |  4586 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4587 | `			return SXERR_ABORT;` |
|         - |  4588 | `		}` |
|       ! 0 |  4589 | `		return SXRET_OK;` |
|         - |  4590 | `	}` |
|         - |  4591 | `	/* Jump the trailing ';' */` |
|     87457 |  4592 | `	pGen->pIn++;` |
|         - |  4593 | `	/* Create the loop block */` |
|     87457 |  4594 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|     87457 |  4595 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4596 | `		return SXERR_ABORT;` |
|         - |  4597 | `	}` |
|         - |  4598 | `	/* Deffer continue jumps */` |
|     87457 |  4599 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4600 | `	/* Compile the condition */` |
|     87457 |  4601 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     87457 |  4602 | `	if( rc == SXERR_ABORT ){` |
|         - |  4603 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4604 | `		return SXERR_ABORT;` |
|     87457 |  4605 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4606 | `		/* Emit the false jump */` |
|     76067 |  4607 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4608 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     76067 |  4609 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     38031 |  4610 | `	}` |
|     87457 |  4611 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4612 | `		/* Syntax error */` |
|         6 |  4613 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4614 | `		if( rc == SXERR_ABORT ){` |
|         - |  4615 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4616 | `			return SXERR_ABORT;` |
|         - |  4617 | `		}` |
|         6 |  4618 | `		return SXRET_OK;` |
|         - |  4619 | `	}` |
|         - |  4620 | `	/* Jump the trailing ';' */` |
|     87453 |  4621 | `	pGen->pIn++;` |
|         - |  4622 | `	/* Save the post condition stream */` |
|     87453 |  4623 | `	pPostStart = pGen->pIn;` |
|         - |  4624 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4625 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|     87453 |  4626 | `	pGen->nCommaExprOk--;` |
|     87453 |  4627 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|     87453 |  4628 | `	pGen->pEnd = pTmp;` |
|     87453 |  4629 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|     87453 |  4630 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4631 | `		return SXERR_ABORT;` |
|         - |  4632 | `	}` |
|         - |  4633 | `	/* Fix post-continue jumps */` |
|     87453 |  4634 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4635 | `		JumpFixup *aPost;` |
|         - |  4636 | `		VmInstr *pInstr;` |
|         - |  4637 | `		sxu32 nJumpDest;` |
|         - |  4638 | `		sxu32 n;` |
|      7609 |  4639 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      7609 |  4640 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     26601 |  4641 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     18997 |  4642 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     18997 |  4643 | `			if( pInstr ){` |
|         - |  4644 | `				/* Fix jump */` |
|     18997 |  4645 | `				pInstr->iP2 = nJumpDest;` |
|      9496 |  4646 | `			}` |
|      9501 |  4647 | `		}` |
|      3802 |  4648 | `	}` |
|         - |  4649 | `	/* compile the post-expressions if available */` |
|     87453 |  4650 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4651 | `		pPostStart++;` |
|       ! 0 |  4652 | `	}` |
|     87453 |  4653 | `	if( pPostStart < pEnd ){` |
|         - |  4654 | `		SyToken *pTmpIn,*pTmpEnd;` |
|     76065 |  4655 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|     76065 |  4656 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|     76065 |  4657 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     76065 |  4658 | `		pGen->nCommaExprOk--;` |
|     76065 |  4659 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4660 | `			/* Syntax error */` |
|       ! 0 |  4661 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4662 | `			if( rc == SXERR_ABORT ){` |
|         - |  4663 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4664 | `				return SXERR_ABORT;` |
|         - |  4665 | `			}` |
|       ! 0 |  4666 | `			return SXRET_OK;` |
|         - |  4667 | `		}` |
|     76065 |  4668 | `		RE_SWAP_DELIMITER(pGen);` |
|     76065 |  4669 | `		if( rc == SXERR_ABORT ){` |
|         - |  4670 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4671 | `			return SXERR_ABORT;` |
|     76065 |  4672 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4673 | `			/* Pop operand lvalue */` |
|     76065 |  4674 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     38030 |  4675 | `		}` |
|     38030 |  4676 | `	}` |
|         - |  4677 | `	/* Emit the unconditional jump to the start of the loop */` |
|     87453 |  4678 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4679 | `	/* Fix all jumps now the destination is resolved */` |
|     87453 |  4680 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4681 | `	/* Release the loop block */` |
|     87453 |  4682 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4683 | `	/* Statement successfully compiled */` |
|     87453 |  4684 | `	return SXRET_OK;` |
|     43731 |  4685 | `}` |
|         - |  4686 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4687 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4688 | ` * are allowed.` |
|         - |  4689 | ` */` |
|    319852 |  4690 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4691 | `{` |
|    319857 |  4692 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    319857 |  4693 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4694 | `		/* Unexpected expression */` |
|       ! 0 |  4695 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4696 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4697 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4698 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4699 | `		}` |
|       ! 0 |  4700 | `	}` |
|    319857 |  4701 | `	return rc;` |
|         5 |  4702 | `}` |
|         - |  4703 | `/*` |
|         - |  4704 | ` * Compile the 'foreach' statement.` |
|         - |  4705 | ` * According to the PHP language reference` |
|         - |  4706 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4707 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4708 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4709 | ` *  is a minor but useful extension of the first:` |
|         - |  4710 | ` *  foreach (array_expression as $value)` |
|         - |  4711 | ` *    statement` |
|         - |  4712 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4713 | ` *   statement` |
|         - |  4714 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4715 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4716 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4717 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4718 | ` *  to the variable $key on each loop.` |
|         - |  4719 | ` *  Note:` |
|         - |  4720 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4721 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4722 | ` *  Note:` |
|         - |  4723 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4724 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4725 | ` *  or after the foreach without resetting it.` |
|         - |  4726 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4727 | ` *  of copying the value.` |
|         - |  4728 | ` */` |
|    220882 |  4729 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4730 | `{` |
|    220887 |  4731 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    220887 |  4732 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    220887 |  4733 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4734 | `	ph7_foreach_info *pInfo;` |
|         - |  4735 | `	sxu32 nFalseJump;` |
|         - |  4736 | `	VmInstr *pInstr;` |
|         - |  4737 | `	sxu32 nLine;` |
|         - |  4738 | `	sxi32 rc;` |
|    220887 |  4739 | `	nLine = pGen->pIn->nLine;` |
|         - |  4740 | `	/* Jump the 'foreach' keyword */` |
|    220887 |  4741 | `	pGen->pIn++;` |
|    220887 |  4742 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4743 | `		/* Syntax error */` |
|       ! 0 |  4744 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4745 | `		if( rc == SXERR_ABORT ){` |
|         - |  4746 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4747 | `			return SXERR_ABORT;` |
|         - |  4748 | `		}` |
|       ! 0 |  4749 | `		goto Synchronize;` |
|         - |  4750 | `	}` |
|         - |  4751 | `	/* Jump the left parenthesis '(' */` |
|    220887 |  4752 | `	pGen->pIn++;` |
|         - |  4753 | `	/* Create the loop block */` |
|    220887 |  4754 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    220887 |  4755 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4756 | `		return SXERR_ABORT;` |
|         - |  4757 | `	}` |
|         - |  4758 | `	/* Delimit the expression */` |
|    220887 |  4759 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    220887 |  4760 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4761 | `		/* Empty expression */` |
|       ! 0 |  4762 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4763 | `		if( rc == SXERR_ABORT ){` |
|         - |  4764 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4765 | `			return SXERR_ABORT;` |
|         - |  4766 | `		}` |
|         - |  4767 | `		/* Synchronize */` |
|       ! 0 |  4768 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4769 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4770 | `			pGen->pIn++;` |
|       ! 0 |  4771 | `		}` |
|       ! 0 |  4772 | `		return SXRET_OK;` |
|         - |  4773 | `	}` |
|         - |  4774 | `	/* Compile the array expression */` |
|    220887 |  4775 | `	pCur = pGen->pIn;` |
|   1192145 |  4776 | `	while( pCur < pEnd ){` |
|   1192145 |  4777 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    232289 |  4778 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    232289 |  4779 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4780 | `				/* Break with the first 'as' found */` |
|    220887 |  4781 | `				break;` |
|         - |  4782 | `			}` |
|      5701 |  4783 | `		}` |
|         - |  4784 | `		/* Advance the stream cursor */` |
|    971263 |  4785 | `		pCur++;` |
|         5 |  4786 | `	}` |
|    220887 |  4787 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4788 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4789 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4790 | `		if( rc == SXERR_ABORT ){` |
|         - |  4791 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4792 | `			return SXERR_ABORT;` |
|         - |  4793 | `		}` |
|       ! 0 |  4794 | `		goto Synchronize;` |
|         - |  4795 | `	}` |
|         - |  4796 | `	/* Swap token streams */` |
|    220887 |  4797 | `	pTmp = pGen->pEnd;` |
|    220887 |  4798 | `	pGen->pEnd = pCur;` |
|    220887 |  4799 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    220887 |  4800 | `	if( rc == SXERR_ABORT ){` |
|         - |  4801 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4802 | `		return SXERR_ABORT;` |
|         - |  4803 | `	}` |
|         - |  4804 | `	/* Update token stream */` |
|    220887 |  4805 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4806 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4807 | `		if( rc == SXERR_ABORT ){` |
|         - |  4808 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4809 | `			return SXERR_ABORT;` |
|         - |  4810 | `		}` |
|       ! 0 |  4811 | `		pGen->pIn++;` |
|       ! 0 |  4812 | `	}` |
|    220887 |  4813 | `	pCur++; /* Jump the 'as' keyword */` |
|    220887 |  4814 | `	pGen->pIn = pCur;` |
|    220887 |  4815 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4816 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4817 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4818 | `			return SXERR_ABORT;` |
|         - |  4819 | `		}` |
|       ! 0 |  4820 | `	}` |
|         - |  4821 | `	/* Create the foreach context */` |
|    220887 |  4822 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    220887 |  4823 | `	if( pInfo == 0 ){` |
|       ! 0 |  4824 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4825 | `		return SXERR_ABORT;` |
|         - |  4826 | `	}` |
|         - |  4827 | `	/* Zero the structure */` |
|    220887 |  4828 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4829 | `	/* Initialize structure fields */` |
|    220887 |  4830 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4831 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4832 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4833 | `	 * '=>'. */` |
|    220887 |  4834 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    220887 |  4835 | `	if( pCur < pEnd ){` |
|         - |  4836 | `		/* Compile the expression holding the key name */` |
|     98995 |  4837 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4838 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4839 | `			if( rc == SXERR_ABORT ){` |
|         - |  4840 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4841 | `				return SXERR_ABORT;` |
|         - |  4842 | `			}` |
|       ! 0 |  4843 | `		}else{` |
|     98995 |  4844 | `			pGen->pEnd = pCur;` |
|     98995 |  4845 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|     98995 |  4846 | `			if( rc == SXERR_ABORT ){` |
|         - |  4847 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4848 | `				return SXERR_ABORT;` |
|         - |  4849 | `			}` |
|     98995 |  4850 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|     98995 |  4851 | `			if( pInstr->p3 ){` |
|         - |  4852 | `				/* Record key name */` |
|     98995 |  4853 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     49495 |  4854 | `			}` |
|     98995 |  4855 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4856 | `		}` |
|     98995 |  4857 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     49495 |  4858 | `	}` |
|    220887 |  4859 | `	pGen->pEnd = pEnd;` |
|    220887 |  4860 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4861 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4862 | `		if( rc == SXERR_ABORT ){` |
|         - |  4863 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4864 | `			return SXERR_ABORT;` |
|         - |  4865 | `		}` |
|       ! 0 |  4866 | `		goto Synchronize;` |
|         - |  4867 | `	}` |
|    220887 |  4868 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4869 | `		pGen->pIn++;` |
|         - |  4870 | `		/* Pass by reference  */` |
|        33 |  4871 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4872 | `	}` |
|         - |  4873 | `	/* Check if the value target is list() */` |
|    220887 |  4874 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4875 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4876 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4877 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4878 | `		 */` |
|         - |  4879 | `		static int iForeachListCnt = 0;` |
|         - |  4880 | `		char zTmp[128];` |
|         - |  4881 | `		sxu32 nLen;` |
|         - |  4882 | `		char *zDup;` |
|        10 |  4883 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4884 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4885 | `		if( zDup == 0 ){` |
|       ! 0 |  4886 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4887 | `			return SXERR_ABORT;` |
|         - |  4888 | `		}` |
|        10 |  4889 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4890 | `		/* Save list() token boundaries */` |
|        10 |  4891 | `		pListStart = pGen->pIn;` |
|         - |  4892 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4893 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4894 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4895 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  4896 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  4897 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4898 | `				return SXERR_ABORT;` |
|         - |  4899 | `			}` |
|         3 |  4900 | `			goto Synchronize;` |
|         - |  4901 | `		}` |
|         7 |  4902 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  4903 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  4904 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4906 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  4907 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4908 | `				return SXERR_ABORT;` |
|         - |  4909 | `			}` |
|       ! 0 |  4910 | `			goto Synchronize;` |
|         - |  4911 | `		}` |
|         7 |  4912 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  4913 | `		pListEnd = pGen->pIn;` |
|         7 |  4914 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    220882 |  4915 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  4916 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  4917 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  4918 | `		 */` |
|         - |  4919 | `		static int iForeachShortListCnt = 0;` |
|         - |  4920 | `		char zTmp[128];` |
|         - |  4921 | `		sxu32 nLen;` |
|         - |  4922 | `		char *zDup;` |
|        13 |  4923 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        13 |  4924 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        13 |  4925 | `		if( zDup == 0 ){` |
|       ! 0 |  4926 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4927 | `			return SXERR_ABORT;` |
|         - |  4928 | `		}` |
|        13 |  4929 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4930 | `		/* Save [...] token boundaries */` |
|        13 |  4931 | `		pListStart = pGen->pIn;` |
|         - |  4932 | `		/* Advance past [...] */` |
|        13 |  4933 | `		pGen->pIn++; /* Jump '[' */` |
|        13 |  4934 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        13 |  4935 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4936 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4937 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  4938 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4939 | `				return SXERR_ABORT;` |
|         - |  4940 | `			}` |
|       ! 0 |  4941 | `			goto Synchronize;` |
|         - |  4942 | `		}` |
|        13 |  4943 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        13 |  4944 | `		pListEnd = pGen->pIn;` |
|        13 |  4945 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         7 |  4946 | `	}else{` |
|         - |  4947 | `		/* Compile the expression holding the value name */` |
|    220867 |  4948 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    220867 |  4949 | `		if( rc == SXERR_ABORT ){` |
|         - |  4950 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4951 | `			return SXERR_ABORT;` |
|         - |  4952 | `		}` |
|    220867 |  4953 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    220867 |  4954 | `		if( pInstr->p3 ){` |
|         - |  4955 | `			/* Record value name */` |
|    220867 |  4956 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    110431 |  4957 | `		}` |
|         - |  4958 | `	}` |
|         - |  4959 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    220885 |  4960 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  4961 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    220885 |  4962 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  4963 | `	/* Record the first instruction to execute */` |
|    220885 |  4964 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4965 | `	/* Emit the FOREACH_STEP instruction */` |
|    220885 |  4966 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  4967 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    220885 |  4968 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  4969 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    220885 |  4970 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  4971 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  4972 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  4973 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  4974 | `		 */` |
|        19 |  4975 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  4976 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  4977 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  4978 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  4979 | `		 */` |
|        19 |  4980 | `		pSavedIn = pGen->pIn;` |
|        19 |  4981 | `		pSavedEnd = pGen->pEnd;` |
|        19 |  4982 | `		pGen->pIn = pListStart;` |
|        19 |  4983 | `		pGen->pEnd = pListEnd;` |
|        19 |  4984 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        13 |  4985 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  4986 | `		}else{` |
|         7 |  4987 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  4988 | `		}` |
|        19 |  4989 | `		pGen->pIn = pSavedIn;` |
|        19 |  4990 | `		pGen->pEnd = pSavedEnd;` |
|        19 |  4991 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4992 | `			return SXERR_ABORT;` |
|         - |  4993 | `		}` |
|         - |  4994 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        19 |  4995 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         9 |  4996 | `	}` |
|         - |  4997 | `	/* Compile the loop body */` |
|    220885 |  4998 | `	pGen->pIn = &pEnd[1];` |
|    220885 |  4999 | `	pGen->pEnd = pTmp;` |
|    220885 |  5000 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    220885 |  5001 | `	if( rc == SXERR_ABORT ){` |
|         - |  5002 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5003 | `		return SXERR_ABORT;` |
|         - |  5004 | `	}` |
|         - |  5005 | `	/* Emit the unconditional jump to the start of the loop */` |
|    220885 |  5006 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  5007 | `	/* Fix all jumps now the destination is resolved */` |
|    220885 |  5008 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  5009 | `	/* Release the loop block */` |
|    220885 |  5010 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5011 | `	/* Statement successfully compiled */` |
|    220885 |  5012 | `	return SXRET_OK;` |
|         1 |  5013 | `Synchronize:` |
|         - |  5014 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  5015 | `	 * compiling this erroneous block.` |
|         - |  5016 | `	 */` |
|         3 |  5017 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  5018 | `		pGen->pIn++;` |
|       ! 0 |  5019 | `	}` |
|         3 |  5020 | `	return SXRET_OK;` |
|    110446 |  5021 | `}` |
|         - |  5022 | `/*` |
|         - |  5023 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  5024 | ` * According to the PHP language reference` |
|         - |  5025 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  5026 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  5027 | ` *  that is similar to that of C:` |
|         - |  5028 | ` *  if (expr)` |
|         - |  5029 | ` *   statement` |
|         - |  5030 | ` *  else construct:` |
|         - |  5031 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  5032 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  5033 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  5034 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  5035 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  5036 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  5037 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  5038 | ` *  elseif` |
|         - |  5039 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  5040 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  5041 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  5042 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  5043 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  5044 | ` *   <?php` |
|         - |  5045 | ` *    if ($a > $b) {` |
|         - |  5046 | ` *     echo "a is bigger than b";` |
|         - |  5047 | ` *    } elseif ($a == $b) {` |
|         - |  5048 | ` *     echo "a is equal to b";` |
|         - |  5049 | ` *    } else {` |
|         - |  5050 | ` *     echo "a is smaller than b";` |
|         - |  5051 | ` *    }` |
|         - |  5052 | ` *    ?>` |
|         - |  5053 | ` */` |
|   1854540 |  5054 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  5055 | `{` |
|   1854545 |  5056 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   1854545 |  5057 | `	GenBlock *pCondBlock = 0;` |
|         - |  5058 | `	sxu32 nJumpIdx;` |
|         - |  5059 | `	sxu32 nKeyID;` |
|         - |  5060 | `	sxi32 rc;` |
|         - |  5061 | `	/* Jump the 'if' keyword */` |
|   1854545 |  5062 | `	pGen->pIn++;` |
|   1854545 |  5063 | `	pToken = pGen->pIn;` |
|         - |  5064 | `	/* Create the conditional block */` |
|   1854545 |  5065 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   1854545 |  5066 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5067 | `		return SXERR_ABORT;` |
|         - |  5068 | `	}` |
|         - |  5069 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1033617 |  5070 | `	for(;;){` |
|   2067239 |  5071 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  5072 | `			/* Syntax error */` |
|       ! 0 |  5073 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5074 | `				pToken--;` |
|       ! 0 |  5075 | `			}` |
|       ! 0 |  5076 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  5077 | `			if( rc == SXERR_ABORT ){` |
|         - |  5078 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5079 | `				return SXERR_ABORT;` |
|         - |  5080 | `			}` |
|       ! 0 |  5081 | `			goto Synchronize;` |
|         - |  5082 | `		}` |
|         - |  5083 | `		/* Jump the left parenthesis '(' */` |
|   2067239 |  5084 | `		pToken++;` |
|         - |  5085 | `		/* Delimit the condition */` |
|   2067239 |  5086 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2067239 |  5087 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  5088 | `			/* Syntax error */` |
|        11 |  5089 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5090 | `				pToken--;` |
|       ! 0 |  5091 | `			}` |
|        11 |  5092 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  5093 | `			if( rc == SXERR_ABORT ){` |
|         - |  5094 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5095 | `				return SXERR_ABORT;` |
|         - |  5096 | `			}` |
|        11 |  5097 | `			goto Synchronize;` |
|         - |  5098 | `		}` |
|         - |  5099 | `		/* Swap token streams */` |
|   2067231 |  5100 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  5101 | `		/* Compile the condition */` |
|   2067231 |  5102 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5103 | `		/* Update token stream */` |
|   2067231 |  5104 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5105 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5106 | `			pGen->pIn++;` |
|       ! 0 |  5107 | `		}` |
|   2067231 |  5108 | `		pGen->pIn  = &pEnd[1];` |
|   2067231 |  5109 | `		pGen->pEnd = pTmp;` |
|   2067231 |  5110 | `		if( rc == SXERR_ABORT ){` |
|         - |  5111 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5112 | `			return SXERR_ABORT;` |
|         - |  5113 | `		}` |
|         - |  5114 | `		/* Emit the false jump */` |
|   2067231 |  5115 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5116 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2067231 |  5117 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5118 | `		/* Compile the body */` |
|   2067231 |  5119 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2067231 |  5120 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5121 | `			return SXERR_ABORT;` |
|         - |  5122 | `		}` |
|   2067231 |  5123 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    410619 |  5124 | `			break;` |
|         - |  5125 | `		}` |
|         - |  5126 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1246003 |  5127 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1246003 |  5128 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|    877209 |  5129 | `			break;` |
|         - |  5130 | `		}` |
|         - |  5131 | `		/* Emit the unconditional jump */` |
|    368799 |  5132 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5133 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    368799 |  5134 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    368799 |  5135 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    239617 |  5136 | `			pToken = &pGen->pIn[1];` |
|    239617 |  5137 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     83550 |  5138 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|     78055 |  5139 | `					break;` |
|         - |  5140 | `			}` |
|     83517 |  5141 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     41756 |  5142 | `		}` |
|    212699 |  5143 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5144 | `		/* Synchronize cursors */` |
|    212699 |  5145 | `		pToken = pGen->pIn;` |
|         - |  5146 | `		/* Fix the false jump */` |
|    212699 |  5147 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5148 | `	} /* For(;;) */` |
|         - |  5149 | `	/* Fix the false jump */` |
|   1854537 |  5150 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   1854537 |  5151 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1033304 |  5152 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5153 | `			/* Compile the else block */` |
|    156105 |  5154 | `			pGen->pIn++;` |
|    156105 |  5155 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    156105 |  5156 | `			if( rc == SXERR_ABORT ){` |
|         - |  5157 |  |
|       ! 0 |  5158 | `				return SXERR_ABORT;` |
|         - |  5159 | `			}` |
|     78050 |  5160 | `	}` |
|   1854537 |  5161 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5162 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   1854537 |  5163 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5164 | `	/* Release the conditional block */` |
|   1854537 |  5165 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5166 | `	/* Statement successfully compiled */` |
|   1854537 |  5167 | `	return SXRET_OK;` |
|         4 |  5168 | `Synchronize:` |
|         - |  5169 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5170 | `	 */` |
|        67 |  5171 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5172 | `		pGen->pIn++;` |
|         3 |  5173 | `	}` |
|        11 |  5174 | `	return SXRET_OK;` |
|    927275 |  5175 | `}` |
|         - |  5176 | `/*` |
|         - |  5177 | ` * Compile the global construct.` |
|         - |  5178 | ` * According to the PHP language reference` |
|         - |  5179 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5180 | ` *  to be used in that function.` |
|         - |  5181 | ` *  Example #1 Using global` |
|         - |  5182 | ` *  <?php` |
|         - |  5183 | ` *   $a = 1;` |
|         - |  5184 | ` *   $b = 2;` |
|         - |  5185 | ` *   function Sum()` |
|         - |  5186 | ` *   {` |
|         - |  5187 | ` *    global $a, $b;` |
|         - |  5188 | ` *    $b = $a + $b;` |
|         - |  5189 | ` *   }` |
|         - |  5190 | ` *   Sum();` |
|         - |  5191 | ` *   echo $b;` |
|         - |  5192 | ` *  ?>` |
|         - |  5193 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5194 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5195 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5196 | ` */` |
|        38 |  5197 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5198 | `{` |
|        43 |  5199 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5200 | `	sxi32 nExpr;` |
|         - |  5201 | `	sxi32 rc;` |
|         - |  5202 | `	/* Jump the 'global' keyword */` |
|        43 |  5203 | `	pGen->pIn++;` |
|        43 |  5204 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5205 | `		/* Nothing to process */` |
|       ! 0 |  5206 | `		return SXRET_OK;` |
|         - |  5207 | `	}` |
|        43 |  5208 | `	pTmp = pGen->pEnd;` |
|        43 |  5209 | `	nExpr = 0;` |
|        91 |  5210 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5211 | `		if( pGen->pIn < pNext ){` |
|        53 |  5212 | `			pGen->pEnd = pNext;` |
|        53 |  5213 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5214 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5215 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5216 | `					return SXERR_ABORT;` |
|         - |  5217 | `				}` |
|       ! 0 |  5218 | `			}else{` |
|        53 |  5219 | `				pGen->pIn++;` |
|        53 |  5220 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5221 | `					/* Emit a warning */` |
|       ! 0 |  5222 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5223 | `				}else{` |
|        53 |  5224 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5225 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5226 | `						return SXERR_ABORT;` |
|        53 |  5227 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5228 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5229 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5230 | `							/* Variable name, not a constant */` |
|        53 |  5231 | `							pLast->iP1 = 0;` |
|        24 |  5232 | `						}` |
|        53 |  5233 | `						nExpr++;` |
|        24 |  5234 | `					}` |
|         - |  5235 | `				}` |
|         - |  5236 | `			}` |
|        24 |  5237 | `		}` |
|         - |  5238 | `		/* Next expression in the stream */` |
|        53 |  5239 | `		pGen->pIn = pNext;` |
|         - |  5240 | `		/* Jump trailing commas */` |
|        63 |  5241 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5242 | `			pGen->pIn++;` |
|         5 |  5243 | `		}` |
|         5 |  5244 | `	}` |
|         - |  5245 | `	/* Restore token stream */` |
|        43 |  5246 | `	pGen->pEnd = pTmp;` |
|        43 |  5247 | `	if( nExpr > 0 ){` |
|         - |  5248 | `		/* Emit the uplink instruction */` |
|        43 |  5249 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5250 | `	}` |
|        43 |  5251 | `	return SXRET_OK;` |
|        24 |  5252 | `}` |
|         - |  5253 | `/*` |
|         - |  5254 | ` * Compile the return statement.` |
|         - |  5255 | ` * According to the PHP language reference` |
|         - |  5256 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5257 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5258 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5259 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5260 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5261 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5262 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5263 | ` *  from within the main script file, then script execution end.` |
|         - |  5264 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5265 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5266 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5267 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5268 | ` */` |
|   2644088 |  5269 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5270 | `{` |
|   2644093 |  5271 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5272 | `	sxi32 rc;` |
|   2644093 |  5273 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2644093 |  5274 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5275 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5276 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5277 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5278 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5279 | `	 * normally below so token processing stays consistent. */` |
|   6852931 |  5280 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4208843 |  5281 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5282 | `	}` |
|   2644088 |  5283 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2644061 |  5284 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5285 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5286 | `			"A never-returning function must not return");` |
|         3 |  5287 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5288 | `			return SXERR_ABORT;` |
|         - |  5289 | `		}` |
|         1 |  5290 | `	}` |
|         - |  5291 | `	/* Jump the 'return' keyword */` |
|   2644093 |  5292 | `	pGen->pIn++;` |
|   2644093 |  5293 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5294 | `		/* Compile the expression */` |
|   2560551 |  5295 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2560551 |  5296 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5297 | `			return SXERR_ABORT;` |
|   2560551 |  5298 | `		}else if(rc != SXERR_EMPTY ){` |
|   2560551 |  5299 | `			nRet = 1;` |
|   1280273 |  5300 | `		}` |
|   1280273 |  5301 | `	}` |
|         - |  5302 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5303 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5304 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5305 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2644093 |  5306 | `	if( pGen->bInGenerator ){` |
|      3829 |  5307 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3829 |  5308 | `		return SXRET_OK;` |
|         - |  5309 | `	}` |
|         - |  5310 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5311 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5312 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5313 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5314 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2640269 |  5315 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2640269 |  5316 | `	return SXRET_OK;` |
|   1322049 |  5317 | `}` |
|         - |  5318 | `/*` |
|         - |  5319 | ` * Compile a yield expression.` |
|         - |  5320 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5321 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5322 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5323 | ` */` |
|     15568 |  5324 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5325 | `{` |
|         - |  5326 | `	SyToken *pTmp, *pSplit;` |
|     15573 |  5327 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15573 |  5328 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5329 | `	sxi32 rc;` |
|      7784 |  5330 | `	(void)iCompileFlag;` |
|         - |  5331 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15573 |  5332 | `	pGen->pIn++;` |
|         - |  5333 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5334 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5335 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5336 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5337 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15568 |  5338 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7819 |  5339 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5340 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5341 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5342 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5343 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5344 | `			return SXERR_ABORT;` |
|         - |  5345 | `		}` |
|        67 |  5346 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5347 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5348 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5349 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5350 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5351 | `				return SXERR_ABORT;` |
|         - |  5352 | `			}` |
|       ! 0 |  5353 | `		}` |
|        67 |  5354 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5355 | `		return SXRET_OK;` |
|         - |  5356 | `	}` |
|     15511 |  5357 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5358 | `		/* Bare yield — no value */` |
|         3 |  5359 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5360 | `		return SXRET_OK;` |
|         - |  5361 | `	}` |
|         - |  5362 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15509 |  5363 | `	pSplit = 0;` |
|         - |  5364 | `	{` |
|     15509 |  5365 | `		SyToken *pCur = pGen->pIn;` |
|     15509 |  5366 | `		sxi32 nNest = 0;` |
|     46333 |  5367 | `		while( pCur < pGen->pEnd ){` |
|     46027 |  5368 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5369 | `				nNest++;` |
|     46019 |  5370 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5371 | `				nNest--;` |
|     46003 |  5372 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15203 |  5373 | `				pSplit = pCur;` |
|     15203 |  5374 | `				break;` |
|         - |  5375 | `			}` |
|     30829 |  5376 | `			pCur++;` |
|         5 |  5377 | `		}` |
|         - |  5378 | `	}` |
|     15509 |  5379 | `	pTmp = pGen->pEnd;` |
|     15509 |  5380 | `	if( pSplit ){` |
|         - |  5381 | `		/* yield $key => $value */` |
|     15203 |  5382 | `		pGen->pEnd = pSplit;` |
|     15203 |  5383 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15203 |  5384 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15203 |  5385 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15203 |  5386 | `		pGen->pEnd = pTmp;` |
|     15203 |  5387 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15203 |  5388 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15203 |  5389 | `		iP1 = 1;` |
|     15203 |  5390 | `		iP2 = 1;` |
|      7604 |  5391 | `	}else{` |
|         - |  5392 | `		/* yield $value */` |
|       311 |  5393 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       311 |  5394 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       311 |  5395 | `		if( rc != SXERR_EMPTY ){` |
|       311 |  5396 | `			iP1 = 1;` |
|       153 |  5397 | `		}` |
|         - |  5398 | `	}` |
|     15509 |  5399 | `	pGen->pEnd = pTmp;` |
|     15509 |  5400 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15509 |  5401 | `	return SXRET_OK;` |
|      7789 |  5402 | `}` |
|         - |  5403 | `/*` |
|         - |  5404 | ` * Compile the die/exit language construct.` |
|         - |  5405 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5406 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5407 | ` */` |
|       128 |  5408 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5409 | `{` |
|       133 |  5410 | `	sxi32 nExpr = 0;` |
|         - |  5411 | `	sxi32 rc;` |
|         - |  5412 | `	/* Jump the die/exit keyword */` |
|       133 |  5413 | `	pGen->pIn++;` |
|       133 |  5414 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5415 | `		/* Compile the expression */` |
|       133 |  5416 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5417 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5418 | `			return SXERR_ABORT;` |
|       133 |  5419 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5420 | `			nExpr = 1;` |
|        64 |  5421 | `		}` |
|        64 |  5422 | `	}` |
|         - |  5423 | `	/* Emit the HALT instruction */` |
|       133 |  5424 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5425 | `	return SXRET_OK;` |
|        69 |  5426 | `}` |
|         - |  5427 | `/*` |
|         - |  5428 | ` * Compile the 'echo' language construct.` |
|         - |  5429 | ` */` |
|     17576 |  5430 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5431 | `{` |
|     17581 |  5432 | `	SyToken *pTmp,*pNext = 0;` |
|     17581 |  5433 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17581 |  5434 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17581 |  5435 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5436 | `	sxi32 rc;` |
|         - |  5437 | `	/* Jump the 'echo' keyword */` |
|     17581 |  5438 | `	pGen->pIn++;` |
|         - |  5439 | `	/* Compile arguments one after one */` |
|     17581 |  5440 | `	pTmp = pGen->pEnd;` |
|     44099 |  5441 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     26525 |  5442 | `		if( pGen->pIn < pNext ){` |
|     26525 |  5443 | `			pGen->pEnd = pNext;` |
|     26525 |  5444 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     26525 |  5445 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5446 | `				return SXERR_ABORT;` |
|     26525 |  5447 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5448 | `				/* Emit the consume instruction */` |
|     26499 |  5449 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     26499 |  5450 | `				nExpr++;` |
|     26499 |  5451 | `				bExpectMore = 0;` |
|     13247 |  5452 | `			}` |
|     13260 |  5453 | `		}` |
|         - |  5454 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5455 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     35475 |  5456 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      8957 |  5457 | `			if( bExpectMore ){` |
|         - |  5458 | `				/* two commas in a row */` |
|         3 |  5459 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5460 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5461 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5462 | `			}` |
|      8955 |  5463 | `			bExpectMore = 1;` |
|      8955 |  5464 | `			pNext++;` |
|         5 |  5465 | `		}` |
|     26523 |  5466 | `		pGen->pIn = pNext;` |
|         5 |  5467 | `	}` |
|         - |  5468 | `	/* Restore token stream */` |
|     17579 |  5469 | `	pGen->pEnd = pTmp;` |
|     17579 |  5470 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5471 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5472 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5473 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5474 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5475 | `	}` |
|     17549 |  5476 | `	return SXRET_OK;` |
|      8793 |  5477 | `}` |
|         - |  5478 | `/*` |
|         - |  5479 | ` * Compile the static statement.` |
|         - |  5480 | ` * According to the PHP language reference` |
|         - |  5481 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5482 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5483 | ` *  when program execution leaves this scope.` |
|         - |  5484 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5485 | ` * Symisc eXtension.` |
|         - |  5486 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5487 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5488 | ` *  Example` |
|         - |  5489 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5490 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5491 | ` */` |
|        12 |  5492 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         3 |  5493 | `{` |
|         - |  5494 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5495 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5496 | `	GenBlock *pBlock;` |
|         - |  5497 | `	SyString *pName;` |
|         - |  5498 | `	char *zDup;` |
|         - |  5499 | `	sxu32 nLine;` |
|         - |  5500 | `	sxi32 rc;` |
|         - |  5501 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5502 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5503 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|        12 |  5504 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|        10 |  5505 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5506 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5507 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5508 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5509 | `			return SXERR_ABORT;` |
|         3 |  5510 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5511 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5512 | `		}` |
|         3 |  5513 | `		return SXRET_OK;` |
|         - |  5514 | `	}` |
|         - |  5515 | `	/* Jump the static keyword */` |
|        13 |  5516 | `	nLine = pGen->pIn->nLine;` |
|        13 |  5517 | `	pGen->pIn++;` |
|         - |  5518 | `	/* Extract the enclosing function if any */` |
|        13 |  5519 | `	pBlock = pGen->pCurrent;` |
|        23 |  5520 | `	while( pBlock ){` |
|        23 |  5521 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|        13 |  5522 | `			break;` |
|         - |  5523 | `		}` |
|         - |  5524 | `		/* Point to the upper block */` |
|        13 |  5525 | `		pBlock = pBlock->pParent;` |
|         3 |  5526 | `	}` |
|        13 |  5527 | `	if( pBlock == 0 ){` |
|         - |  5528 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5529 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5530 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5531 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5532 | `				return SXERR_ABORT;` |
|         - |  5533 | `			}` |
|       ! 0 |  5534 | `			goto Synchronize;` |
|         - |  5535 | `		}` |
|         - |  5536 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5537 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5538 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5539 | `			return SXERR_ABORT;` |
|       ! 0 |  5540 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5541 | `			/* Emit the POP instruction */` |
|       ! 0 |  5542 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5543 | `		}` |
|       ! 0 |  5544 | `		return SXRET_OK;` |
|         - |  5545 | `	}` |
|        13 |  5546 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5547 | `	/* Make sure we are dealing with a valid statement */` |
|        13 |  5548 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|         8 |  5549 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5550 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5551 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5552 | `				return SXERR_ABORT;` |
|         - |  5553 | `			}` |
|         3 |  5554 | `			goto Synchronize;` |
|         - |  5555 | `	}` |
|        10 |  5556 | `	pGen->pIn++;` |
|         - |  5557 | `	/* Extract variable name */` |
|        10 |  5558 | `	pName = &pGen->pIn->sData;` |
|        10 |  5559 | `	pGen->pIn++; /* Jump the var name */` |
|        10 |  5560 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5561 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5562 | `		goto Synchronize;` |
|         - |  5563 | `	}` |
|         - |  5564 | `	/* Initialize the structure describing the static variable */` |
|        10 |  5565 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        10 |  5566 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5567 | `	/* Duplicate variable name */` |
|        10 |  5568 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        10 |  5569 | `	if( zDup == 0 ){` |
|       ! 0 |  5570 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5571 | `		return SXERR_ABORT;` |
|         - |  5572 | `	}` |
|        10 |  5573 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5574 | `	/* Check if we have an expression to compile */` |
|        10 |  5575 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5576 | `		SySet *pInstrContainer;` |
|         - |  5577 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5578 | `		 * Static variable can take any complex expression including function` |
|         - |  5579 | `		 * call as their initialization value.` |
|         - |  5580 | `		 * Example:` |
|         - |  5581 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5582 | `		 */` |
|        10 |  5583 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5584 | `		/* Swap bytecode container */` |
|        10 |  5585 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        10 |  5586 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5587 | `		/* Compile the expression */` |
|        10 |  5588 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5589 | `		/* Emit the done instruction */` |
|        10 |  5590 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5591 | `		/* Restore default bytecode container */` |
|        10 |  5592 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         4 |  5593 | `	}` |
|         - |  5594 | `	/* Finally save the compiled static variable in the appropriate container */` |
|        10 |  5595 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|        10 |  5596 | `	return SXRET_OK;` |
|         1 |  5597 | `Synchronize:` |
|         - |  5598 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5599 | `	 * statement.` |
|         - |  5600 | `	 */` |
|         5 |  5601 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5602 | `		pGen->pIn++;` |
|         1 |  5603 | `	}` |
|         3 |  5604 | `	return SXRET_OK;` |
|         9 |  5605 | `}` |
|         - |  5606 | `/*` |
|         - |  5607 | ` * Compile the var statement.` |
|         - |  5608 | ` * Symisc Extension:` |
|         - |  5609 | ` *      var statement can be used outside of a class definition.` |
|         - |  5610 | ` */` |
|         4 |  5611 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5612 | `{` |
|         - |  5613 | `	sxu32 nLine;` |
|         - |  5614 | `	sxi32 rc;` |
|         5 |  5615 | `	nLine = pGen->pIn->nLine;` |
|         - |  5616 | `	/* Jump the 'var' keyword */` |
|         5 |  5617 | `	pGen->pIn++;` |
|         5 |  5618 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5619 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5620 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5621 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5622 | `			pGen->pIn++;` |
|       ! 0 |  5623 | `		}` |
|       ! 0 |  5624 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5625 | `			return SXERR_ABORT;` |
|         - |  5626 | `		}` |
|       ! 0 |  5627 | `	}else{` |
|         - |  5628 | `		/* Compile the expression */` |
|         5 |  5629 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5630 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5631 | `			return SXERR_ABORT;` |
|         5 |  5632 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5633 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5634 | `		}` |
|         - |  5635 | `	}` |
|         5 |  5636 | `	return SXRET_OK;` |
|         3 |  5637 | `}` |
|         - |  5638 | `/*` |
|         - |  5639 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5640 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5641 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5642 | ` */` |
|         - |  5643 | `/*` |
|         - |  5644 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5645 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5646 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5647 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5648 | ` *` |
|         - |  5649 | ` * Resolution order:` |
|         - |  5650 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5651 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5652 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5653 | ` *` |
|         - |  5654 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5655 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5656 | ` * Returns the (possibly new) literal index.` |
|         - |  5657 | ` */` |
|   4807270 |  5658 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5659 | `{` |
|         - |  5660 | `	ph7_value *pLit;` |
|         - |  5661 | `	const char *zLit;` |
|         - |  5662 | `	SyString sQualified;` |
|         - |  5663 | `	sxu32 nLit;` |
|         - |  5664 | `	sxu32 k;` |
|         - |  5665 | `	sxu32 nNewIdx;` |
|         - |  5666 | `	int hasNsSep;` |
|         - |  5667 | `	SyHashEntry *pImport;` |
|         - |  5668 | `	ph7_value *pNew;` |
|   4807275 |  5669 | `	if( pFromImport ){` |
|   3793749 |  5670 | `		*pFromImport = 0;` |
|   1896872 |  5671 | `	}` |
|   4807275 |  5672 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   4807275 |  5673 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5674 | `		return nOrigIdx;` |
|         - |  5675 | `	}` |
|   4807275 |  5676 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   4807275 |  5677 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5678 | `	/* Skip if already qualified (contains backslash) */` |
|   4807275 |  5679 | `	hasNsSep = 0;` |
|  58851161 |  5680 | `	for( k = 0; k < nLit; k++ ){` |
|  54043899 |  5681 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  27021948 |  5682 | `	}` |
|   4807275 |  5683 | `	if( hasNsSep ){` |
|        10 |  5684 | `		return nOrigIdx;` |
|         - |  5685 | `	}` |
|         - |  5686 | `	/* Check use imports first (works even outside namespaces) */` |
|   4807267 |  5687 | `	SyBlobReset(&pGen->sWorker);` |
|   4807267 |  5688 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   4807267 |  5689 | `	if( pImport ){` |
|        41 |  5690 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5691 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5692 | `		if( pFromImport ){` |
|        18 |  5693 | `			*pFromImport = 1;` |
|         8 |  5694 | `		}` |
|        23 |  5695 | `	}else{` |
|   4807231 |  5696 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   4807141 |  5697 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5698 | `		}` |
|         - |  5699 | `		/* Prepend current namespace */` |
|        95 |  5700 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        95 |  5701 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|        95 |  5702 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5703 | `	}` |
|         - |  5704 | `	/* Look up or create a new literal for the qualified name */` |
|       131 |  5705 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       131 |  5706 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        57 |  5707 | `		return nNewIdx; /* Already interned */` |
|         - |  5708 | `	}` |
|        79 |  5709 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        79 |  5710 | `	if( pNew == 0 ){` |
|       ! 0 |  5711 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5712 | `	}` |
|        79 |  5713 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        79 |  5714 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        79 |  5715 | `	return nNewIdx;` |
|   2403640 |  5716 | `}` |
|         - |  5717 | `/*` |
|         - |  5718 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5719 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5720 | ` */` |
|    407692 |  5721 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5722 | `{` |
|         - |  5723 | `	SyHashEntry *pImport;` |
|         - |  5724 | `	/* Check use imports first */` |
|    407697 |  5725 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    407697 |  5726 | `	if( pImport ){` |
|        20 |  5727 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        20 |  5728 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        20 |  5729 | `		return;` |
|         - |  5730 | `	}` |
|         - |  5731 | `	/* Prepend current namespace if active */` |
|    407681 |  5732 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         8 |  5733 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  5734 | `		SyBlobAppend(pOut,"\\",1);` |
|         3 |  5735 | `	}` |
|    407681 |  5736 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    203851 |  5737 | `}` |
|         - |  5738 | `/*` |
|         - |  5739 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5740 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5741 | ` * The caller must release pOut when done.` |
|         - |  5742 | ` */` |
|    427108 |  5743 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5744 | `{` |
|    427113 |  5745 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3859 |  5746 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3859 |  5747 | `		SyBlobAppend(pOut,"\\",1);` |
|      1927 |  5748 | `	}` |
|    427113 |  5749 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    427113 |  5750 | `}` |
|         - |  5751 | `/*` |
|         - |  5752 | ` * Compile a namespace statement` |
|         - |  5753 | ` * According to the PHP language reference manual` |
|         - |  5754 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5755 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5756 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5757 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5758 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5759 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5760 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5761 | ` *  programming world.` |
|         - |  5762 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5763 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5764 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5765 | ` *  classes/functions/constants.` |
|         - |  5766 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5767 | ` *  readability of source code.` |
|         - |  5768 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5769 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5770 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5771 | ` *       class MyClass {}` |
|         - |  5772 | ` *       function myfunction() {}` |
|         - |  5773 | ` *       const MYCONST = 1;` |
|         - |  5774 | ` *       $a = new MyClass;` |
|         - |  5775 | ` *       $c = new \my\name\MyClass;` |
|         - |  5776 | ` *       $a = strlen('hi');` |
|         - |  5777 | ` *       $d = namespace\MYCONST;` |
|         - |  5778 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5779 | ` *       echo constant($d);` |
|         - |  5780 | ` * NOTE` |
|         - |  5781 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5782 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5783 | ` */` |
|         - |  5784 | `/*` |
|         - |  5785 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5786 | ` */` |
|        14 |  5787 | `static const char * TokenTypeName(sxu32 nType)` |
|         3 |  5788 | `{` |
|        17 |  5789 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        11 |  5790 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        11 |  5791 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        11 |  5792 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        11 |  5793 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        11 |  5794 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5795 | `	return "token";` |
|        10 |  5796 | `}` |
|      3902 |  5797 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5798 | `{` |
|         - |  5799 | `	sxu32 nLine;` |
|         - |  5800 | `	sxi32 rc;` |
|      3907 |  5801 | `	nLine = pGen->pIn->nLine;` |
|      3907 |  5802 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5803 | `	/* Reset namespace and clear previous use imports */` |
|      3907 |  5804 | `	SyBlobReset(&pGen->sNamespace);` |
|      3907 |  5805 | `	SyHashRelease(&pGen->hUseImports);` |
|      3907 |  5806 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3907 |  5807 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3907 |  5808 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3907 |  5809 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3907 |  5810 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3907 |  5811 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5812 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5813 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5814 | `		return SXRET_OK;` |
|         - |  5815 | `	}` |
|      3907 |  5816 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5817 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5818 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5819 | `		return SXRET_OK;` |
|         - |  5820 | `	}` |
|      3907 |  5821 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5822 | `		/* namespace { } — global namespace block */` |
|       ! 0 |  5823 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5824 | `		return SXRET_OK;` |
|         - |  5825 | `	}` |
|         - |  5826 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7851 |  5827 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3949 |  5828 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5829 | `			/* Append backslash separator */` |
|        26 |  5830 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        26 |  5831 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        11 |  5832 | `			}` |
|        15 |  5833 | `		}else{` |
|         - |  5834 | `			/* Append identifier */` |
|      3927 |  5835 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5836 | `		}` |
|      3949 |  5837 | `		pGen->pIn++;` |
|         5 |  5838 | `	}` |
|         - |  5839 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5840 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5841 | `	{` |
|      3907 |  5842 | `		char *zNsDup = 0;` |
|      3907 |  5843 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5855 |  5844 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3900 |  5845 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1950 |  5846 | `		}` |
|      3907 |  5847 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5848 | `	}` |
|      3907 |  5849 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5850 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5851 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5852 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5853 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5854 | `			return SXERR_ABORT;` |
|         - |  5855 | `		}` |
|         2 |  5856 | `	}` |
|      3907 |  5857 | `	return SXRET_OK;` |
|      1956 |  5858 | `}` |
|         - |  5859 | `/*` |
|         - |  5860 | ` * Compile the 'use' statement` |
|         - |  5861 | ` * According to the PHP language reference manual` |
|         - |  5862 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5863 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5864 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5865 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5866 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5867 | ` *  a function or constant is not supported.` |
|         - |  5868 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5869 | ` * NOTE` |
|         - |  5870 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5871 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5872 | ` */` |
|        72 |  5873 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5874 | `{` |
|         - |  5875 | `	sxu32 nLine;` |
|         - |  5876 | `	sxi32 rc;` |
|         - |  5877 | `	SyBlob sPath;` |
|         - |  5878 | `	SyString sAlias;` |
|         - |  5879 | `	SyToken *pLast;` |
|         - |  5880 | `	char *zDup;` |
|         - |  5881 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5882 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5883 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        77 |  5884 | `	nLine = pGen->pIn->nLine;` |
|        77 |  5885 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5886 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        77 |  5887 | `	iUseType = 0;` |
|        77 |  5888 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5889 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5890 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5891 | `			iUseType = 1;` |
|        16 |  5892 | `			pGen->pIn++;` |
|        23 |  5893 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5894 | `			iUseType = 2;` |
|        16 |  5895 | `			pGen->pIn++;` |
|         7 |  5896 | `		}` |
|        14 |  5897 | `	}` |
|         - |  5898 | `	/* Select target hash tables based on import type */` |
|        77 |  5899 | `	switch( iUseType ){` |
|         7 |  5900 | `		case 1:` |
|        16 |  5901 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5902 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5903 | `			break;` |
|         7 |  5904 | `		case 2:` |
|        16 |  5905 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5906 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5907 | `			break;` |
|        22 |  5908 | `		default:` |
|        49 |  5909 | `			pGenHash = &pGen->hUseImports;` |
|        49 |  5910 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        44 |  5911 | `			break;` |
|         - |  5912 | `	}` |
|        77 |  5913 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5914 | `	/* Process one or more use declarations separated by commas */` |
|        37 |  5915 | `	for(;;){` |
|        79 |  5916 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5917 | `			break;` |
|         - |  5918 | `		}` |
|        79 |  5919 | `		SyBlobReset(&sPath);` |
|        79 |  5920 | `		pLast = 0;` |
|         - |  5921 | `		/* Collect the full namespace path */` |
|       269 |  5922 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       195 |  5923 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       135 |  5924 | `				pLast = pGen->pIn;` |
|       135 |  5925 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        65 |  5926 | `					SyBlobAppend(&sPath,"\\",1);` |
|        30 |  5927 | `				}` |
|       135 |  5928 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        65 |  5929 | `			}` |
|       195 |  5930 | `			pGen->pIn++;` |
|         5 |  5931 | `		}` |
|        79 |  5932 | `		if( pLast == 0 ){` |
|         - |  5933 | `			/* Empty path */` |
|         6 |  5934 | `			break;` |
|         - |  5935 | `		}` |
|         - |  5936 | `		/* Default alias is the last component of the path */` |
|        75 |  5937 | `		sAlias = pLast->sData;` |
|         - |  5938 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        70 |  5939 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        50 |  5940 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        23 |  5941 | `			pGen->pIn++; /* Jump 'as' */` |
|        23 |  5942 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        23 |  5943 | `				sAlias = pGen->pIn->sData;` |
|        23 |  5944 | `				pGen->pIn++;` |
|        10 |  5945 | `			}` |
|        10 |  5946 | `		}` |
|         - |  5947 | `		/* Check for duplicate import alias (per-type) */` |
|        75 |  5948 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  5949 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  5950 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  5951 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  5952 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5953 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  5954 | `				return SXERR_ABORT;` |
|         - |  5955 | `			}` |
|         2 |  5956 | `		}` |
|         - |  5957 | `		/* Register the import: alias -> FQN.` |
|         - |  5958 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  5959 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  5960 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       110 |  5961 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        70 |  5962 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        75 |  5963 | `		if( zDup ){` |
|        75 |  5964 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        75 |  5965 | `			if( pVmHash ){` |
|         - |  5966 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  5967 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        47 |  5968 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        47 |  5969 | `				if( zAliasDup ){` |
|        47 |  5970 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        21 |  5971 | `				}` |
|        21 |  5972 | `			}` |
|        75 |  5973 | `			if( iUseType == 2 ){` |
|         - |  5974 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  5975 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  5976 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  5977 | `				if( zAliasDup ){` |
|         - |  5978 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  5979 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  5980 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  5981 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  5982 | `					if( azPair ){` |
|        16 |  5983 | `						azPair[0] = zAliasDup;` |
|        16 |  5984 | `						azPair[1] = zDup;` |
|        16 |  5985 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  5986 | `					}` |
|         7 |  5987 | `				}` |
|         7 |  5988 | `			}` |
|        35 |  5989 | `		}` |
|         - |  5990 | `		/* Check for comma (multiple use declarations) */` |
|        75 |  5991 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  5992 | `			pGen->pIn++;` |
|         2 |  5993 | `		}else{` |
|        39 |  5994 | `			break;` |
|         - |  5995 | `		}` |
|         1 |  5996 | `	}` |
|        77 |  5997 | `	SyBlobRelease(&sPath);` |
|        77 |  5998 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  5999 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  6000 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  6001 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6002 | `			return SXERR_ABORT;` |
|         - |  6003 | `		}` |
|         1 |  6004 | `	}` |
|        77 |  6005 | `	return SXRET_OK;` |
|        41 |  6006 | `}` |
|         - |  6007 | `/*` |
|         - |  6008 | ` * Compile the stupid 'declare' language construct.` |
|         - |  6009 | ` *` |
|         - |  6010 | ` * According to the PHP language reference manual.` |
|         - |  6011 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  6012 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  6013 | ` *  declare (directive)` |
|         - |  6014 | ` *   statement` |
|         - |  6015 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  6016 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  6017 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  6018 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  6019 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  6020 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  6021 | ` * <?php` |
|         - |  6022 | ` * // these are the same:` |
|         - |  6023 | ` * // you can use this:` |
|         - |  6024 | ` * declare(ticks=1) {` |
|         - |  6025 | ` *   // entire script here` |
|         - |  6026 | ` * }` |
|         - |  6027 | ` * // or you can use this:` |
|         - |  6028 | ` * declare(ticks=1);` |
|         - |  6029 | ` * // entire script here` |
|         - |  6030 | ` * ?>` |
|         - |  6031 | ` *` |
|         - |  6032 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  6033 | ` */` |
|         - |  6034 | `/*` |
|         - |  6035 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  6036 | ` */` |
|        72 |  6037 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  6038 | `{` |
|       109 |  6039 | `	return SyStringLength(pName) == nWant` |
|        72 |  6040 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  6041 | `}` |
|         - |  6042 |  |
|        42 |  6043 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  6044 | `{` |
|        47 |  6045 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  6046 | `	SyToken *pBodyEnd = 0;` |
|         - |  6047 | `	SyToken *pBodyStart;` |
|         - |  6048 | `	SyToken *pCursor;` |
|         - |  6049 | `	int bHasStrictTypes;` |
|         - |  6050 | `	int bBlockForm;` |
|         - |  6051 | `	int bPlacementOk;` |
|         - |  6052 | `	sxi32 rc;` |
|        47 |  6053 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  6054 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  6055 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         6 |  6056 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6057 | `			return SXERR_ABORT;` |
|         - |  6058 | `		}` |
|         6 |  6059 | `		goto Synchro;` |
|         - |  6060 | `	}` |
|        43 |  6061 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  6062 | `	pBodyStart = pGen->pIn;` |
|         - |  6063 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  6064 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  6065 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  6066 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  6067 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6068 | `			return SXERR_ABORT;` |
|         - |  6069 | `		}` |
|       ! 0 |  6070 | `		return SXRET_OK;` |
|         - |  6071 | `	}` |
|         - |  6072 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  6073 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  6074 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  6075 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  6076 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  6077 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6078 | `			return SXERR_ABORT;` |
|         - |  6079 | `		}` |
|       ! 0 |  6080 | `	}` |
|        43 |  6081 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  6082 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  6083 | `	bHasStrictTypes = 0;` |
|         - |  6084 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  6085 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  6086 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  6087 | `	pCursor = pBodyStart;` |
|        55 |  6088 | `	while( pCursor < pBodyEnd ){` |
|        51 |  6089 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  6090 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  6091 | `				bHasStrictTypes = 1;` |
|        39 |  6092 | `				break;` |
|         - |  6093 | `			}` |
|         2 |  6094 | `		}` |
|        14 |  6095 | `		pCursor++;` |
|         2 |  6096 | `	}` |
|        43 |  6097 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  6098 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6099 | `			"strict_types declaration must not use block mode");` |
|         3 |  6100 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6101 | `		return SXRET_OK;` |
|         - |  6102 | `	}` |
|        41 |  6103 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  6104 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6105 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  6106 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  6107 | `		return SXRET_OK;` |
|         - |  6108 | `	}` |
|         - |  6109 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  6110 | `	pCursor = pBodyStart;` |
|        69 |  6111 | `	while( pCursor < pBodyEnd ){` |
|         - |  6112 | `		SyToken *pNameTok;` |
|         - |  6113 | `		SyToken *pEqTok;` |
|         - |  6114 | `		SyToken *pValTok;` |
|         - |  6115 | `		SyString *pDirName;` |
|         - |  6116 | `		int bIsStrict;` |
|         - |  6117 | `		int iStrictValue;` |
|        39 |  6118 | `		pNameTok = pCursor;` |
|        39 |  6119 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6120 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6121 | `				"declare: Expecting a directive name");` |
|       ! 0 |  6122 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6123 | `			return SXRET_OK;` |
|         - |  6124 | `		}` |
|        39 |  6125 | `		pEqTok = pNameTok + 1;` |
|        39 |  6126 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6127 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6128 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6129 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6130 | `			return SXRET_OK;` |
|         - |  6131 | `		}` |
|        39 |  6132 | `		pValTok = pEqTok + 1;` |
|        39 |  6133 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6134 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6135 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6136 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6137 | `			return SXRET_OK;` |
|         - |  6138 | `		}` |
|        39 |  6139 | `		pDirName = &pNameTok->sData;` |
|        39 |  6140 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6141 | `		if( bIsStrict ){` |
|         - |  6142 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6143 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6144 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6145 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6146 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6147 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6148 | `				return SXRET_OK;` |
|         - |  6149 | `			}` |
|        35 |  6150 | `			iStrictValue = -1;` |
|        35 |  6151 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6152 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6153 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6154 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6155 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6156 | `			}` |
|        35 |  6157 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6158 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6159 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6160 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6161 | `				return SXRET_OK;` |
|         - |  6162 | `			}` |
|        32 |  6163 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6164 | `		}else{` |
|         - |  6165 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6166 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6167 | `			 * behavior don't regress. */` |
|         8 |  6168 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6169 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6170 | `				ph7_lib_version()` |
|         - |  6171 | `				);` |
|         - |  6172 | `		}` |
|        36 |  6173 | `		pCursor = pValTok + 1;` |
|         - |  6174 | `		/* Consume separating comma (or end). */` |
|        36 |  6175 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6176 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6177 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6178 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6179 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6180 | `				return SXRET_OK;` |
|         - |  6181 | `			}` |
|         3 |  6182 | `			pCursor++;` |
|         1 |  6183 | `		}` |
|         4 |  6184 | `	}` |
|         - |  6185 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6186 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6187 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        34 |  6188 | `	return SXRET_OK;` |
|         2 |  6189 | `Synchro:` |
|         - |  6190 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6191 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6192 | `		pGen->pIn++;` |
|         2 |  6193 | `	}` |
|         6 |  6194 | `	return SXRET_OK;` |
|        26 |  6195 | `}` |
|         - |  6196 | `/*` |
|         - |  6197 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6198 | ` * as follows:` |
|         - |  6199 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6200 | ` * {` |
|         - |  6201 | ` *   return "Making a cup of $type.\n";` |
|         - |  6202 | ` * }` |
|         - |  6203 | ` * Symisc eXtension.` |
|         - |  6204 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6205 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6206 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6207 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6208 | ` *      {` |
|         - |  6209 | ` *       var_dump($a);` |
|         - |  6210 | ` *      }` |
|         - |  6211 | ` *     //call test without args` |
|         - |  6212 | ` *      test();` |
|         - |  6213 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6214 | ` *      Example:` |
|         - |  6215 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6216 | ` * 3 -) Function overloading!!` |
|         - |  6217 | ` *      Example:` |
|         - |  6218 | ` *      function foo($a) {` |
|         - |  6219 | ` *   	  return $a.PHP_EOL;` |
|         - |  6220 | ` *	    }` |
|         - |  6221 | ` *	    function foo($a, $b) {` |
|         - |  6222 | ` *   	  return $a + $b;` |
|         - |  6223 | ` *	    }` |
|         - |  6224 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6225 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6226 | ` *      // Same arg` |
|         - |  6227 | ` *	   function foo(string $a)` |
|         - |  6228 | ` *	   {` |
|         - |  6229 | ` *	     echo "a is a string\n";` |
|         - |  6230 | ` *	     var_dump($a);` |
|         - |  6231 | ` *	   }` |
|         - |  6232 | ` *	  function foo(int $a)` |
|         - |  6233 | ` *	  {` |
|         - |  6234 | ` *	    echo "a is integer\n";` |
|         - |  6235 | ` *	    var_dump($a);` |
|         - |  6236 | ` *	  }` |
|         - |  6237 | ` *	  function foo(array $a)` |
|         - |  6238 | ` *	  {` |
|         - |  6239 | ` * 	    echo "a is an array\n";` |
|         - |  6240 | ` * 	    var_dump($a);` |
|         - |  6241 | ` *	  }` |
|         - |  6242 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6243 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6244 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6245 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6246 | ` * introduced by the PH7 engine.` |
|         - |  6247 | ` */` |
|    451882 |  6248 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6249 | `{` |
|         - |  6250 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6251 | `	SySet *pInstrContainer;` |
|         - |  6252 | `	sxi32 rc;` |
|         - |  6253 | `	/* Swap token stream */` |
|    451887 |  6254 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    451887 |  6255 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    451887 |  6256 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6257 | `	/* Compile the expression holding the argument value */` |
|    451887 |  6258 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6259 | `	/* Emit the done instruction */` |
|    451887 |  6260 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    451887 |  6261 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    451887 |  6262 | `	RE_SWAP_DELIMITER(pGen);` |
|    451887 |  6263 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6264 | `		return SXERR_ABORT;` |
|         - |  6265 | `	}` |
|    451887 |  6266 | `	return SXRET_OK;` |
|    225946 |  6267 | `}` |
|         - |  6268 | `/*` |
|         - |  6269 | ` * Collect function arguments one after one.` |
|         - |  6270 | ` * According to the PHP language reference manual.` |
|         - |  6271 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6272 | ` * list of expressions.` |
|         - |  6273 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6274 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6275 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6276 | ` * for more information.` |
|         - |  6277 | ` * Example #1 Passing arrays to functions` |
|         - |  6278 | ` * <?php` |
|         - |  6279 | ` * function takes_array($input)` |
|         - |  6280 | ` * {` |
|         - |  6281 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6282 | ` * }` |
|         - |  6283 | ` * ?>` |
|         - |  6284 | ` * Making arguments be passed by reference` |
|         - |  6285 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6286 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6287 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6288 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6289 | ` * to the argument name in the function definition:` |
|         - |  6290 | ` * Example #2 Passing function parameters by reference` |
|         - |  6291 | ` * <?php` |
|         - |  6292 | ` * function add_some_extra(&$string)` |
|         - |  6293 | ` * {` |
|         - |  6294 | ` *   $string .= 'and something extra.';` |
|         - |  6295 | ` * }` |
|         - |  6296 | ` * $str = 'This is a string, ';` |
|         - |  6297 | ` * add_some_extra($str);` |
|         - |  6298 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6299 | ` * ?>` |
|         - |  6300 | ` *` |
|         - |  6301 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6302 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6303 | ` * on these extension.` |
|         - |  6304 | ` */` |
|   1091360 |  6305 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6306 | `{` |
|         - |  6307 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6308 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6309 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6310 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6311 | `	sxi32 rc;` |
|         - |  6312 |  |
|   1091365 |  6313 | `	pIn = pGen->pIn;` |
|   1091365 |  6314 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6315 | `	/* Process arguments one after one */` |
|   1368938 |  6316 | `	for(;;){` |
|   2737881 |  6317 | `		if( pIn >= pEnd ){` |
|         - |  6318 | `			/* No more arguments to process */` |
|   1091349 |  6319 | `			break;` |
|         - |  6320 | `		}` |
|   1646537 |  6321 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   1646537 |  6322 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   1646537 |  6323 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   1646537 |  6324 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   1646537 |  6325 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6326 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6327 | `		 * first token inside the main token stream */` |
|   1646537 |  6328 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6329 | `			return SXERR_ABORT;` |
|         - |  6330 | `		}` |
|         - |  6331 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6332 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6333 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6334 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6335 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6336 | `		{` |
|   1646537 |  6337 | `			int bReadonly = 0, bVisSeen = 0;` |
|   1646537 |  6338 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   1646537 |  6339 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6340 | `			int nSetTok;` |
|         - |  6341 | `			sxi32 nSetVis;` |
|   1646537 |  6342 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6343 | `				bReadonly = 1;` |
|         3 |  6344 | `				pIn++;` |
|         1 |  6345 | `			}` |
|   1646537 |  6346 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   1646537 |  6347 | `			if( nSetVis ){` |
|         - |  6348 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6349 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6350 | `				bVisSeen = 1;` |
|         3 |  6351 | `				pIn += nSetTok;` |
|         3 |  6352 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6353 | `					bReadonly = 1;` |
|       ! 0 |  6354 | `					pIn++;` |
|         1 |  6355 | `				}` |
|   1646536 |  6356 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     87707 |  6357 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     87707 |  6358 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6359 | `					bVisSeen = 1;` |
|        89 |  6360 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6361 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6362 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6363 | `					pIn++;` |
|        89 |  6364 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6365 | `					if( nSetVis ){` |
|         - |  6366 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6367 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6368 | `						pIn += nSetTok;` |
|         1 |  6369 | `					}` |
|        89 |  6370 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6371 | `						bReadonly = 1;` |
|        18 |  6372 | `						pIn++;` |
|         7 |  6373 | `					}` |
|        42 |  6374 | `				}` |
|     43851 |  6375 | `			}` |
|   1646537 |  6376 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6377 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   1646535 |  6378 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6379 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6380 | `			}` |
|   1646537 |  6381 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6382 | `				if( !bCtorCtx ){` |
|         6 |  6383 | `					if( bAbstractCtx ){` |
|         3 |  6384 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6385 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6386 | `					}else{` |
|         3 |  6387 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6388 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6389 | `					}` |
|         6 |  6390 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6391 | `						return SXERR_ABORT;` |
|         - |  6392 | `					}` |
|         6 |  6393 | `					return SXERR_SYNTAX;` |
|         - |  6394 | `				}` |
|        89 |  6395 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6396 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6397 | `				if( bReadonly ){` |
|        20 |  6398 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6399 | `				}` |
|        42 |  6400 | `			}` |
|         - |  6401 | `		}` |
|         - |  6402 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   1646528 |  6403 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|    895702 |  6404 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    139170 |  6405 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    114397 |  6406 | `			sxu32 nLineLocal = pIn->nLine;` |
|    114397 |  6407 | `			sxi32 iTFlags = 0;` |
|    114397 |  6408 | `			pGen->pIn = pIn;` |
|    114397 |  6409 | `			rc = GenStateParseUnionTypeDecl(` |
|     57196 |  6410 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57196 |  6411 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6412 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6413 | `				/* bAllowVoid */ 0,` |
|     57196 |  6414 | `						nLineLocal);` |
|    114397 |  6415 | `			pIn = pGen->pIn;` |
|    114397 |  6416 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6417 | `				return SXERR_ABORT;` |
|    114397 |  6418 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6419 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6420 | `				return SXERR_SYNTAX;` |
|    114395 |  6421 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6422 | `				if( pIn < pEnd ){` |
|        15 |  6423 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6424 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6425 | `						&pIn->sData);` |
|         7 |  6426 | `				}else{` |
|       ! 0 |  6427 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6428 | `						"syntax error, unexpected end of file");` |
|         - |  6429 | `				}` |
|        11 |  6430 | `				return SXERR_SYNTAX;` |
|         - |  6431 | `			}` |
|    114387 |  6432 | `			sArg.iFlags \|= iTFlags;` |
|     57191 |  6433 | `		}` |
|   1646523 |  6434 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6435 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6436 | `			return rc;` |
|         - |  6437 | `		}` |
|   1646523 |  6438 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6439 | `			/* Pass by reference,record that */` |
|     11435 |  6440 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     11435 |  6441 | `			pIn++;` |
|      5715 |  6442 | `		}` |
|   1646523 |  6443 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6444 | `			/* Variadic parameter: ...$args */` |
|     19089 |  6445 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     19089 |  6446 | `			pIn++;` |
|      9542 |  6447 | `		}` |
|   1646523 |  6448 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6449 | `			/* Invalid argument */` |
|       ! 0 |  6450 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6451 | `			return rc;` |
|         - |  6452 | `		}` |
|   1646523 |  6453 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6454 | `		/* Copy argument name */` |
|   1646523 |  6455 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   1646523 |  6456 | `		if( zDup == 0 ){` |
|       ! 0 |  6457 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6458 | `			return SXERR_ABORT;` |
|         - |  6459 | `		}` |
|   1646523 |  6460 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   1646523 |  6461 | `		pIn++;` |
|   1646523 |  6462 | `		if( pIn < pEnd ){` |
|    851381 |  6463 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6464 | `				SyToken *pDefend;` |
|    451889 |  6465 | `				sxi32 iNest = 0;` |
|    451889 |  6466 | `				pIn++; /* Jump the equal sign */` |
|    451889 |  6467 | `				pDefend = pIn;` |
|         - |  6468 | `				/* Process the default value associated with this argument */` |
|    953147 |  6469 | `				while( pDefend < pEnd ){` |
|    656949 |  6470 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    155691 |  6471 | `						break;` |
|         - |  6472 | `					}` |
|    501263 |  6473 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6474 | `						/* Increment nesting level */` |
|     26585 |  6475 | `						iNest++;` |
|    487973 |  6476 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6477 | `						/* Decrement nesting level */` |
|     26585 |  6478 | `						iNest--;` |
|     13290 |  6479 | `					}` |
|    501263 |  6480 | `					pDefend++;` |
|         5 |  6481 | `				}` |
|    451889 |  6482 | `				if( pIn >= pDefend ){` |
|         3 |  6483 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6484 | `					return rc;` |
|         - |  6485 | `				}` |
|         - |  6486 | `				/* Process default value */` |
|    451887 |  6487 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    451887 |  6488 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6489 | `					return rc;` |
|         - |  6490 | `				}` |
|         - |  6491 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6492 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6493 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6494 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6495 | `				 * arg-type check lets null through. */` |
|    451882 |  6496 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    250638 |  6497 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    250635 |  6498 | `					&& &pIn[1] == pDefend` |
|     45591 |  6499 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     34186 |  6500 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     20889 |  6501 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15195 |  6502 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6503 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6504 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6505 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6506 | `					 * already up at this point). */` |
|         - |  6507 | `					{` |
|     15195 |  6508 | `						const char *zSep = "";` |
|     15195 |  6509 | `						SyString sCls = { "", 0 };` |
|     15195 |  6510 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15189 |  6511 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15189 |  6512 | `							zSep = "::";` |
|      7592 |  6513 | `						}` |
|     22790 |  6514 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6515 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7595 |  6516 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6517 | `					}` |
|      7595 |  6518 | `				}` |
|         - |  6519 | `				/* Point beyond the default value */` |
|    451887 |  6520 | `				pIn = pDefend;` |
|    225941 |  6521 | `			}` |
|    851379 |  6522 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6523 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6524 | `				return rc;` |
|         - |  6525 | `			}` |
|    851379 |  6526 | `			pIn++; /* Jump the trailing comma */` |
|    425687 |  6527 | `		}` |
|         - |  6528 | `		/* Append argument signature */` |
|   1646521 |  6529 | `		if( sArg.nType > 0 ){` |
|    114325 |  6530 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6531 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26657 |  6532 | `				int marker = 'o';` |
|     26657 |  6533 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26657 |  6534 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13331 |  6535 | `			}else{` |
|         - |  6536 | `				int c;` |
|     87673 |  6537 | `				c = 'n'; /* cc warning */` |
|         - |  6538 | `				/* Type leading character */` |
|     87673 |  6539 | `				switch(sArg.nType){` |
|      5700 |  6540 | `				case MEMOBJ_HASHMAP:` |
|         - |  6541 | `					/* Hashmap aka 'array' */` |
|     11405 |  6542 | `					c = 'h';` |
|     11405 |  6543 | `					break;` |
|      9610 |  6544 | `				case MEMOBJ_INT:` |
|         - |  6545 | `					/* Integer */` |
|     19225 |  6546 | `					c = 'i';` |
|     19225 |  6547 | `					break;` |
|         2 |  6548 | `				case MEMOBJ_BOOL:` |
|         - |  6549 | `					/* Bool */` |
|         5 |  6550 | `					c = 'b';` |
|         5 |  6551 | `					break;` |
|         5 |  6552 | `				case MEMOBJ_REAL:` |
|         - |  6553 | `					/* Float */` |
|        12 |  6554 | `					c = 'f';` |
|        12 |  6555 | `					break;` |
|     28509 |  6556 | `				case MEMOBJ_STRING:` |
|         - |  6557 | `					/* String */` |
|     57023 |  6558 | `					c = 's';` |
|     57023 |  6559 | `					break;` |
|         7 |  6560 | `				case MEMOBJ_OBJ:` |
|         - |  6561 | `					/* Object */` |
|        16 |  6562 | `					c = 'o';` |
|        14 |  6563 | `					break;` |
|         1 |  6564 | `				default:` |
|         2 |  6565 | `					break;` |
|         - |  6566 | `				}` |
|     87673 |  6567 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6568 | `			}` |
|     57165 |  6569 | `		}else{` |
|         - |  6570 | `			/* No type is associated with this parameter which mean` |
|         - |  6571 | `			 * that this function is not condidate for overloading.` |
|         - |  6572 | `			 */` |
|   1532201 |  6573 | `			SyBlobRelease(&sSig);` |
|         - |  6574 | `		}` |
|         - |  6575 | `		/* Save in the argument set */` |
|   1646521 |  6576 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6577 | `	}` |
|   1091349 |  6578 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6579 | `		/* Save function signature */` |
|     83885 |  6580 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     41940 |  6581 | `	}` |
|   1091349 |  6582 | `	return SXRET_OK;` |
|    545685 |  6583 | `}` |
|         - |  6584 | `/*` |
|         - |  6585 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6586 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6587 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6588 | ` */` |
|     34206 |  6589 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6590 | `{` |
|     34211 |  6591 | `	sxi32 iParen = 0;` |
|     34211 |  6592 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6593 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6594 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6595 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    152073 |  6596 | `	while( pIn < pEnd ){` |
|    152073 |  6597 | `		sxu32 t = pIn->nType;` |
|    152073 |  6598 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    148223 |  6599 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    102617 |  6600 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     83595 |  6601 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    117867 |  6602 | `		pIn++;` |
|         5 |  6603 | `	}` |
|     19027 |  6604 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6605 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6606 | `	{` |
|     19027 |  6607 | `		sxi32 d = 0;` |
|    755829 |  6608 | `		while( pIn < pEnd ){` |
|    755829 |  6609 | `			sxu32 t = pIn->nType;` |
|    755829 |  6610 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    725415 |  6611 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    736807 |  6612 | `			pIn++;` |
|         5 |  6613 | `		}` |
|         - |  6614 | `	}` |
|     19027 |  6615 | `	return pIn;` |
|     17108 |  6616 | `}` |
|         - |  6617 | `/*` |
|         - |  6618 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6619 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6620 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6621 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6622 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6623 | ` * detached-mini-program path untouched.` |
|         - |  6624 | ` */` |
|         - |  6625 | `/*` |
|         - |  6626 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6627 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6628 | ` * mixed, object.` |
|         - |  6629 | ` */` |
|     11416 |  6630 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6631 | `{` |
|         - |  6632 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6633 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6634 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6635 | `	};` |
|         - |  6636 | `	sxu32 i;` |
|     11421 |  6637 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6638 | `		zName++;` |
|       ! 0 |  6639 | `		nName--;` |
|       ! 0 |  6640 | `	}` |
|     11453 |  6641 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11449 |  6642 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11417 |  6643 | `			return 1;` |
|         - |  6644 | `		}` |
|        17 |  6645 | `	}` |
|         5 |  6646 | `	return 0;` |
|      5713 |  6647 | `}` |
|         - |  6648 | `/*` |
|         - |  6649 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6650 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6651 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6652 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6653 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6654 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6655 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6656 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6657 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6658 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6659 | ` */` |
|     11414 |  6660 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6661 | `{` |
|     11419 |  6662 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6663 | ``		return 1; /* bare `object` */`` |
|         - |  6664 | `	}` |
|     11419 |  6665 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6666 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6667 | `	}` |
|     11417 |  6668 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11413 |  6669 | `		return 1;` |
|         - |  6670 | `	}` |
|         - |  6671 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6672 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6673 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6674 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6675 | `	{` |
|         - |  6676 | `		SyBlob sFQN;` |
|         - |  6677 | `		int bOk;` |
|         5 |  6678 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         5 |  6679 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|         5 |  6680 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|         5 |  6681 | `		SyBlobRelease(&sFQN);` |
|         5 |  6682 | `		return bOk;` |
|         - |  6683 | `	}` |
|      5712 |  6684 | `}` |
|         - |  6685 | `/*` |
|         - |  6686 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6687 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6688 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6689 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6690 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6691 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6692 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6693 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6694 | ` */` |
|     11652 |  6695 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6696 | `{` |
|     11657 |  6697 | `	int bOk = 0;` |
|         - |  6698 | `	sxu32 nLine;` |
|         - |  6699 | `	sxi32 rc;` |
|     11657 |  6700 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       243 |  6701 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6702 | `	}` |
|     11419 |  6703 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6704 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6705 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6706 | `		sxu32 i,j;` |
|       ! 0 |  6707 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6708 | `			int bGroupOk;` |
|       ! 0 |  6709 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6710 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6711 | `			}` |
|       ! 0 |  6712 | `			bGroupOk = 1;` |
|       ! 0 |  6713 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6714 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6715 | `					bGroupOk = 0;` |
|       ! 0 |  6716 | `					break;` |
|         - |  6717 | `				}` |
|       ! 0 |  6718 | `			}` |
|       ! 0 |  6719 | `			bOk = bGroupOk;` |
|       ! 0 |  6720 | `		}` |
|       ! 0 |  6721 | `	}else{` |
|     11419 |  6722 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6723 | `	}` |
|     11419 |  6724 | `	if( bOk ){` |
|     11417 |  6725 | `		return SXRET_OK;` |
|         - |  6726 | `	}` |
|         - |  6727 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6728 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6729 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6730 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6731 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6732 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6733 | `	{` |
|         3 |  6734 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6735 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6736 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6737 | `		}` |
|         3 |  6738 | `		if( sGiven.nByte < 1 ){` |
|         - |  6739 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6740 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6741 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6742 | `			const char *zScalar =` |
|       ! 0 |  6743 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6744 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6745 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6746 | `		}` |
|         3 |  6747 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6748 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6749 | `	}` |
|         3 |  6750 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5831 |  6751 | `}` |
|   2531674 |  6752 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6753 | `{` |
|   2531679 |  6754 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2531679 |  6755 | `	SyToken *pEnd = pGen->pEnd;` |
|   2531679 |  6756 | `	sxi32 iDepth = 0;` |
|   2531679 |  6757 | `	int bStarted = 0;` |
| 111726339 |  6758 | `	while( pIn < pEnd ){` |
| 111726339 |  6759 | `		sxu32 t = pIn->nType;` |
| 111726339 |  6760 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 106601031 |  6761 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 101510265 |  6762 | `		if( t & PH7_TK_KEYWORD ){` |
|   7407479 |  6763 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   7407479 |  6764 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   7395827 |  6765 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6766 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   3680808 |  6767 | `		}` |
| 101464407 |  6768 | `		pIn++;` |
|         5 |  6769 | `	}` |
|   2520027 |  6770 | `	return FALSE;` |
|   1265842 |  6771 | `}` |
|         - |  6772 | `/*` |
|         - |  6773 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6774 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6775 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6776 | ` */` |
|   2531674 |  6777 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6778 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6779 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6780 | `	)` |
|         5 |  6781 | `{` |
|         - |  6782 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6783 | `	GenBlock *pBlock;` |
|         - |  6784 | `	sxu32 nGotoOfft;` |
|         - |  6785 | `	sxi32 rc;` |
|         - |  6786 | `	/* Attach the new function */` |
|   2531679 |  6787 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2531679 |  6788 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6789 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6790 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6791 | `		return SXERR_ABORT;` |
|         - |  6792 | `	}` |
|   2531679 |  6793 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6794 | `	/* Swap bytecode containers */` |
|   2531679 |  6795 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2531679 |  6796 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6797 | `	/* Emit constructor property promotion prologue:` |
|         - |  6798 | `	 *   $this->NAME = $NAME;` |
|         - |  6799 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6800 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6801 | `	{` |
|   2531679 |  6802 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6803 | `		sxu32 i;` |
|   4124925 |  6804 | `		for( i = 0; i < nArg; i++ ){` |
|   1593251 |  6805 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6806 | `			char *zSrc;` |
|         - |  6807 | `			sxu32 nSrc,nName;` |
|         - |  6808 | `			SySet sToken;` |
|         - |  6809 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6810 | `			sxi32 rcPromote;` |
|   1593251 |  6811 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1593177 |  6812 | `				continue;` |
|         - |  6813 | `			}` |
|         - |  6814 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6815 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6816 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6817 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6818 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6819 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6820 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6821 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6822 | `			if( zSrc == 0 ){` |
|       ! 0 |  6823 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6824 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6825 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6826 | `				return SXERR_ABORT;` |
|         - |  6827 | `			}` |
|         - |  6828 | `			{` |
|        79 |  6829 | `				char *z = zSrc;` |
|        79 |  6830 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6831 | `				z += sizeof("$this->")-1;` |
|        79 |  6832 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6833 | `				z += nName;` |
|        79 |  6834 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6835 | `				z += sizeof(" = $")-1;` |
|        79 |  6836 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6837 | `				z += nName;` |
|        79 |  6838 | `				*z = 0;` |
|         - |  6839 | `			}` |
|        79 |  6840 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6841 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6842 | `			pTmpIn = pGen->pIn;` |
|        79 |  6843 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6844 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6845 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6846 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6847 | `			pGen->pIn = pTmpIn;` |
|        79 |  6848 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6849 | `			SySetRelease(&sToken);` |
|        79 |  6850 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6851 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6852 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6853 | `				return SXERR_ABORT;` |
|         - |  6854 | `			}` |
|         - |  6855 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6856 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6857 | `		}` |
|         - |  6858 | `	}` |
|         - |  6859 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6860 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6861 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6862 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6863 | `	{` |
|   2531679 |  6864 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2531679 |  6865 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6866 | `		/* Compile the body */` |
|   2531679 |  6867 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2531679 |  6868 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6869 | `	}` |
|         - |  6870 | `	/* Fix exception jumps now the destination is resolved */` |
|   2531679 |  6871 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6872 | `	/* Emit the final return if not yet done */` |
|   2531679 |  6873 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6874 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2531679 |  6875 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6876 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6877 | `	}` |
|   2531679 |  6878 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6879 | `	/* Restore the default container */` |
|   2531679 |  6880 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6881 | `	/* Leave function block */` |
|   2531679 |  6882 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2531679 |  6883 | `	if( rc == SXERR_ABORT ){` |
|         - |  6884 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6885 | `		return SXERR_ABORT;` |
|         - |  6886 | `	}` |
|         - |  6887 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6888 | `	{` |
|   2531679 |  6889 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6890 | `		sxu32 i;` |
|  69186103 |  6891 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  66666081 |  6892 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11657 |  6893 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11657 |  6894 | `				break;` |
|         - |  6895 | `			}` |
|  33327217 |  6896 | `		}` |
|         - |  6897 | `	}` |
|   2531679 |  6898 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6899 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11657 |  6900 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6901 | `			return SXERR_ABORT;` |
|         - |  6902 | `		}` |
|      5826 |  6903 | `	}` |
|         - |  6904 | `	/* All done, function body compiled */` |
|   2531679 |  6905 | `	return SXRET_OK;` |
|   1265842 |  6906 | `}` |
|         - |  6907 | `/*` |
|         - |  6908 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  6909 | ` * According to the PHP language reference manual.` |
|         - |  6910 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  6911 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  6912 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  6913 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  6914 | ` *  Functions need not be defined before they are referenced.` |
|         - |  6915 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  6916 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  6917 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  6918 | ` *  calls with over 32-64 recursion levels.` |
|         - |  6919 | ` *` |
|         - |  6920 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  6921 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  6922 | ` * on these extension.` |
|         - |  6923 | ` */` |
|         - |  6924 | `/*` |
|         - |  6925 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  6926 | ` */` |
|       570 |  6927 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  6928 | `{` |
|         - |  6929 | `	sxu32 i;` |
|      1611 |  6930 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  6931 | `		int a = zA[i], b = zB[i];` |
|      1381 |  6932 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  6933 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  6934 | `		if( a != b ) return a - b;` |
|       523 |  6935 | `	}` |
|       235 |  6936 | `	return 0;` |
|       290 |  6937 | `}` |
|         - |  6938 | `/*` |
|         - |  6939 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  6940 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  6941 | ` * (which are positive bit values stored in sxu32).` |
|         - |  6942 | ` */` |
|         - |  6943 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  6944 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  6945 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  6946 |  |
|         - |  6947 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  6948 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  6949 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  6950 |  |
|         - |  6951 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  6952 | `struct PhlTypeAtom {` |
|         - |  6953 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  6954 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  6955 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  6956 | `	sxu32 nCanon;` |
|         - |  6957 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  6958 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  6959 | `};` |
|         - |  6960 |  |
|         - |  6961 | `/*` |
|         - |  6962 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  6963 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  6964 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  6965 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  6966 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  6967 | ` * already be consumed by the caller.` |
|         - |  6968 | ` */` |
|    126960 |  6969 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  6970 | `{` |
|    126965 |  6971 | `	SyToken *pIn = pGen->pIn;` |
|    126965 |  6972 | `	SyZero(pOut, sizeof(*pOut));` |
|    126965 |  6973 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    126965 |  6974 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6975 | `		return SXERR_SYNTAX;` |
|         - |  6976 | `	}` |
|         - |  6977 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    126965 |  6978 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|         8 |  6979 | `		pIn++;` |
|         8 |  6980 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6981 | `			return SXERR_SYNTAX;` |
|         - |  6982 | `		}` |
|         3 |  6983 | `	}` |
|    126965 |  6984 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6985 | `		return SXERR_SYNTAX;` |
|         - |  6986 | `	}` |
|    126965 |  6987 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     88465 |  6988 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     88465 |  6989 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11437 |  6990 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     82749 |  6991 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        81 |  6992 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     76995 |  6993 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19631 |  6994 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67144 |  6995 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57249 |  6996 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     28709 |  6997 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  6998 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        67 |  6999 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        27 |  7000 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        37 |  7001 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        13 |  7002 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        23 |  7003 | `			pOut->nType = SXU32_HIGH;` |
|        23 |  7004 | `			pOut->sClass = pIn->sData;` |
|        13 |  7005 | `		}else{` |
|         3 |  7006 | `			return SXERR_SYNTAX;` |
|         - |  7007 | `		}` |
|     88463 |  7008 | `		pIn++;` |
|     44234 |  7009 | `	}else{` |
|         - |  7010 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  7011 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38505 |  7012 | `		SyString *pT = &pIn->sData;` |
|     38505 |  7013 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  7014 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  7015 | `			pIn++;` |
|     38490 |  7016 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  7017 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  7018 | `			pIn++;` |
|     38389 |  7019 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        26 |  7020 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        26 |  7021 | `			pIn++;` |
|        15 |  7022 | `		}else{` |
|         - |  7023 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38281 |  7024 | `			SyToken *pFirst = pIn;` |
|     38281 |  7025 | `			SyToken *pLast = pIn;` |
|     38281 |  7026 | `			pOut->nType = SXU32_HIGH;` |
|     38281 |  7027 | `			pOut->sClass = pIn->sData;` |
|     38281 |  7028 | `			pIn++;` |
|     57417 |  7029 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38284 |  7030 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  7031 | `				pLast = &pIn[1];` |
|         3 |  7032 | `				pIn += 2;` |
|         1 |  7033 | `			}` |
|     38281 |  7034 | `			if( pLast != pFirst ){` |
|         3 |  7035 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  7036 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  7037 | `				pOut->sClass.zString = zFirst;` |
|         3 |  7038 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  7039 | `			}` |
|         - |  7040 | `		}` |
|         - |  7041 | `	}` |
|    126963 |  7042 | `	pGen->pIn = pIn;` |
|    126963 |  7043 | `	return SXRET_OK;` |
|     63485 |  7044 | `}` |
|         - |  7045 |  |
|         - |  7046 | `/*` |
|         - |  7047 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  7048 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  7049 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  7050 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  7051 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  7052 | ` */` |
|    126782 |  7053 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  7054 | `{` |
|         - |  7055 | `	int i;` |
|    126787 |  7056 | `	int nNonNull = 0;` |
|    126787 |  7057 | `	int bAnyIntersection = 0;` |
|         - |  7058 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    126787 |  7059 | `	sxu32 nMaxGroup = 0;` |
|   4183811 |  7060 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    253721 |  7061 | `	for( i = 0; i < nAtoms; i++ ){` |
|    126939 |  7062 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    126909 |  7063 | `			nNonNull++;` |
|    126909 |  7064 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    126909 |  7065 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    126909 |  7066 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     63452 |  7067 | `			}` |
|     63452 |  7068 | `		}` |
|     63472 |  7069 | `	}` |
|    253669 |  7070 | `	for( i = 0; i < nAtoms; i++ ){` |
|    126911 |  7071 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  7072 | `			bAnyIntersection = 1;` |
|        29 |  7073 | `			break;` |
|         - |  7074 | `		}` |
|     63446 |  7075 | `	}` |
|    126787 |  7076 | `	if( bAnyIntersection ){` |
|         - |  7077 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  7078 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  7079 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  7080 | `		sxu32 g, nGroups = 0;` |
|        29 |  7081 | `		int bFirstGroup = 1;` |
|        59 |  7082 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  7083 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  7084 | `			int bFirstMember = 1;` |
|         - |  7085 | `			int bWrap;` |
|        35 |  7086 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  7087 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  7088 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  7089 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  7090 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  7091 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  7092 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  7093 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  7094 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  7095 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  7096 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  7097 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  7098 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  7099 | `				}else{` |
|         6 |  7100 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7101 | `				}` |
|        59 |  7102 | `				bFirstMember = 0;` |
|        32 |  7103 | `			}` |
|        35 |  7104 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7105 | `			bFirstGroup = 0;` |
|        20 |  7106 | `		}` |
|        29 |  7107 | `		if( bNullable ){` |
|       ! 0 |  7108 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7109 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7110 | `		}` |
|        83 |  7111 | `		return;` |
|         - |  7112 | `	}` |
|    126763 |  7113 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7114 | `		/* Shorthand: ?T */` |
|       113 |  7115 | `		for( i = 0; i < nAtoms; i++ ){` |
|       113 |  7116 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       113 |  7117 | `			SyBlobAppend(pBlob, "?", 1);` |
|       113 |  7118 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        23 |  7119 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  7120 | `			}else{` |
|        93 |  7121 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7122 | `			}` |
|       113 |  7123 | `			return;` |
|       ! 0 |  7124 | `		}` |
|       ! 0 |  7125 | `	}` |
|         - |  7126 | `	{` |
|    126655 |  7127 | `		int bFirst = 1;` |
|         - |  7128 | `		/* 1) Classes in declaration order */` |
|    253413 |  7129 | `		for( i = 0; i < nAtoms; i++ ){` |
|    126763 |  7130 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38231 |  7131 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38231 |  7132 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38231 |  7133 | `				bFirst = 0;` |
|     19113 |  7134 | `			}` |
|     63384 |  7135 | `		}` |
|         - |  7136 | `		/* 2) Built-ins in canonical order */` |
|         - |  7137 | `		{` |
|         - |  7138 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7139 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7140 | `			int k;` |
|    886555 |  7141 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1432011 |  7142 | `				for( i = 0; i < nAtoms; i++ ){` |
|    760441 |  7143 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     88335 |  7144 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     88335 |  7145 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     88335 |  7146 | `						bFirst = 0;` |
|     88335 |  7147 | `						break;` |
|         - |  7148 | `					}` |
|    336058 |  7149 | `				}` |
|    379955 |  7150 | `			}` |
|         - |  7151 | `		}` |
|         - |  7152 | `		/* 3) null suffix */` |
|    126655 |  7153 | `		if( bNullable ){` |
|        19 |  7154 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        19 |  7155 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7156 | `		}` |
|         - |  7157 | `	}` |
|     63396 |  7158 | `}` |
|         - |  7159 |  |
|         - |  7160 | `/*` |
|         - |  7161 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7162 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7163 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7164 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7165 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7166 | ` * whether it was parenthesized.` |
|         - |  7167 | ` *` |
|         - |  7168 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7169 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7170 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7171 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7172 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7173 | ` */` |
|    126934 |  7174 | `static sxi32 GenStateParsePart(` |
|         - |  7175 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7176 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7177 | `{` |
|         - |  7178 | `	sxi32 rc;` |
|    126939 |  7179 | `	int nMembers = 0;` |
|    126939 |  7180 | `	int bParen = 0;` |
|    126939 |  7181 | `	*pnMembers = 0;` |
|    126939 |  7182 | `	*pbParen = 0;` |
|    126939 |  7183 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7184 | `		bParen = 1;` |
|         9 |  7185 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7186 | `	}` |
|     63467 |  7187 | `	for(;;){` |
|    126965 |  7188 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7189 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7190 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7191 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7192 | `		}` |
|    126965 |  7193 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    126965 |  7194 | `		if( rc != SXRET_OK ){` |
|         3 |  7195 | `			return rc;` |
|         - |  7196 | `		}` |
|    126963 |  7197 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    126963 |  7198 | `		(*pnAtoms)++;` |
|    126963 |  7199 | `		nMembers++;` |
|         - |  7200 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    126963 |  7201 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7202 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7203 | `			if( pNext < pGen->pEnd` |
|        39 |  7204 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7205 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7206 | `				continue;` |
|         - |  7207 | `			}` |
|         4 |  7208 | `		}` |
|    126937 |  7209 | `		break;` |
|       ! 0 |  7210 | `	}` |
|    126937 |  7211 | `	if( bParen ){` |
|         9 |  7212 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7213 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7214 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7215 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7216 | `		}` |
|         9 |  7217 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7218 | `		if( nMembers < 2 ){` |
|       ! 0 |  7219 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7220 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7221 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7222 | `		}` |
|         3 |  7223 | `	}` |
|    126937 |  7224 | `	*pnMembers = nMembers;` |
|    126937 |  7225 | `	*pbParen = bParen;` |
|    126937 |  7226 | `	return SXRET_OK;` |
|     63472 |  7227 | `}` |
|         - |  7228 |  |
|         - |  7229 | `/*` |
|         - |  7230 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7231 | ` *` |
|         - |  7232 | ` * Outputs:` |
|         - |  7233 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7234 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7235 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7236 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7237 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7238 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7239 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7240 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7241 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7242 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7243 | ` *` |
|         - |  7244 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7245 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7246 | ` */` |
|    126798 |  7247 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7248 | `	ph7_gen_state *pGen,` |
|         - |  7249 | `	sxu32 *pnType,` |
|         - |  7250 | `	SyString *pClass,` |
|         - |  7251 | `	SySet *pAlts,` |
|         - |  7252 | `	sxi32 *piTypeFlags,` |
|         - |  7253 | `	SyString *pTypeText,` |
|         - |  7254 | `	int iNullableFlag,` |
|         - |  7255 | `	int iUnionFlag,` |
|         - |  7256 | `	int bAllowVoid,` |
|         - |  7257 | `	sxu32 nLine` |
|         5 |  7258 | `){` |
|         - |  7259 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    126803 |  7260 | `	int nAtoms = 0;` |
|    126803 |  7261 | `	int bShortNullable = 0;` |
|    126803 |  7262 | `	int bExplicitNull = 0;` |
|         - |  7263 | `	sxi32 rc;` |
|    126803 |  7264 | `	*pnType = 0;` |
|    126803 |  7265 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    126803 |  7266 | `	*piTypeFlags = 0;` |
|    126803 |  7267 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7268 |  |
|    126803 |  7269 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7270 | `		return SXRET_OK;` |
|         - |  7271 | `	}` |
|         - |  7272 | ``	/* Optional `?` shorthand prefix */`` |
|    126798 |  7273 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       101 |  7274 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       101 |  7275 | `		bShortNullable = 1;` |
|       101 |  7276 | `		pGen->pIn++;` |
|       101 |  7277 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7278 | `			return SXERR_SYNTAX;` |
|         - |  7279 | `		}` |
|        48 |  7280 | `	}` |
|         - |  7281 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7282 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7283 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7284 | `	{` |
|         - |  7285 | `		int nMembers, bParen;` |
|    126803 |  7286 | `		sxu32 iGroup = 0;` |
|    126803 |  7287 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    126803 |  7288 | `		if( rc != SXRET_OK ){` |
|         4 |  7289 | `			return rc;` |
|         - |  7290 | `		}` |
|         - |  7291 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7292 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7293 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7294 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7295 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    190403 |  7296 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    127010 |  7297 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7298 | `			if( bShortNullable ){` |
|         - |  7299 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7300 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7301 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7302 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7303 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7304 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7305 | `			}` |
|       141 |  7306 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7307 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7308 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7309 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7310 | `			}` |
|       141 |  7311 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7312 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7313 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7314 | `				return rc;` |
|         - |  7315 | `			}` |
|         5 |  7316 | `		}` |
|    126799 |  7317 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7318 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7319 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7320 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7321 | `		}` |
|         - |  7322 | `	}` |
|         - |  7323 | `	/* Validation pass.` |
|         - |  7324 | `	 *` |
|         - |  7325 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7326 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7327 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7328 | `	 */` |
|         - |  7329 | `	{` |
|         - |  7330 | `		int i, j;` |
|    126799 |  7331 | `		int bHasNonNull = 0;` |
|    126799 |  7332 | `		int bAnyIntersection = 0;` |
|         - |  7333 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7334 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7335 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4184207 |  7336 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    253755 |  7337 | `		for( i = 0; i < nAtoms; i++ ){` |
|    126961 |  7338 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     63483 |  7339 | `		}` |
|    253699 |  7340 | `		for( i = 0; i < nAtoms; i++ ){` |
|    126931 |  7341 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     63455 |  7342 | `		}` |
|         - |  7343 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7344 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    126799 |  7345 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7346 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7347 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7348 | `			return SXERR_SYNTAX;` |
|         - |  7349 | `		}` |
|    253741 |  7350 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7351 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7352 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7353 | ``			 * `true`/`false` in an intersection). */`` |
|    126959 |  7354 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7355 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7356 | `				if( bClassLike ){` |
|        53 |  7357 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7358 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7359 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7360 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7361 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7362 | `						bClassLike = 0;` |
|       ! 0 |  7363 | `					}` |
|        24 |  7364 | `				}` |
|        55 |  7365 | `				if( !bClassLike ){` |
|         - |  7366 | `					const char *zName; sxu32 nName;` |
|         3 |  7367 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7368 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7369 | `					}else{` |
|         3 |  7370 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7371 | `					}` |
|         4 |  7372 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7373 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7374 | `						(int)nName, zName);` |
|         3 |  7375 | `					return SXERR_SYNTAX;` |
|         - |  7376 | `				}` |
|        24 |  7377 | `			}` |
|    126957 |  7378 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7379 | `				if( nAtoms > 1 ){` |
|         3 |  7380 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7381 | `						"Void can only be used as a standalone type");` |
|         3 |  7382 | `					return SXERR_SYNTAX;` |
|         - |  7383 | `				}` |
|       175 |  7384 | `				if( !bAllowVoid ){` |
|       ! 0 |  7385 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7386 | `						"void cannot be used here");` |
|       ! 0 |  7387 | `					return SXERR_SYNTAX;` |
|         - |  7388 | `				}` |
|       175 |  7389 | `				if( bShortNullable ){` |
|       ! 0 |  7390 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7391 | `						"Void type cannot be nullable");` |
|       ! 0 |  7392 | `					return SXERR_SYNTAX;` |
|         - |  7393 | `				}` |
|        85 |  7394 | `			}` |
|    126955 |  7395 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7396 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7397 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7398 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7399 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        26 |  7400 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7401 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7402 | `					 * same as any other non-standalone use. */` |
|         5 |  7403 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7404 | `						"never can only be used as a standalone type");` |
|         5 |  7405 | `					return SXERR_SYNTAX;` |
|         - |  7406 | `				}` |
|        21 |  7407 | `				if( !bAllowVoid ){` |
|         - |  7408 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7409 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7410 | `						"never cannot be used as a parameter type");` |
|         3 |  7411 | `					return SXERR_SYNTAX;` |
|         - |  7412 | `				}` |
|         8 |  7413 | `			}` |
|    126949 |  7414 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7415 | `				bExplicitNull = 1;` |
|        19 |  7416 | `			}else{` |
|    126919 |  7417 | `				bHasNonNull = 1;` |
|         - |  7418 | `			}` |
|         - |  7419 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7420 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7421 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7422 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7423 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    127149 |  7424 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7425 | `				int bDup = 0;` |
|       207 |  7426 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7427 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7428 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7429 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7430 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7431 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7432 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7433 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7434 | `								aAtoms[j].sClass.zString,` |
|        34 |  7435 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7436 | `							bDup = 1;` |
|       ! 0 |  7437 | `						}` |
|        27 |  7438 | `					}else{` |
|         3 |  7439 | `						bDup = 1;` |
|         - |  7440 | `					}` |
|        23 |  7441 | `				}` |
|       195 |  7442 | `				if( bDup ){` |
|         - |  7443 | `					const char *zName;` |
|         - |  7444 | `					sxu32 nName;` |
|         3 |  7445 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7446 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7447 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7448 | `					}else{` |
|         3 |  7449 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7450 | `						nName = aAtoms[i].nCanon;` |
|         - |  7451 | `					}` |
|         4 |  7452 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7453 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7454 | `					return SXERR_SYNTAX;` |
|         - |  7455 | `				}` |
|        99 |  7456 | `			}` |
|     63476 |  7457 | `		}` |
|    126787 |  7458 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7459 | `			if( bShortNullable ){` |
|         - |  7460 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7461 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7462 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7463 | `				return SXERR_SYNTAX;` |
|         - |  7464 | `			}` |
|         - |  7465 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7466 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7467 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7468 | `			 * atom, so set it here. */` |
|         7 |  7469 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7470 | `		}` |
|         - |  7471 | `	}` |
|         - |  7472 | `	/* Compute nullability flag */` |
|    126787 |  7473 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       129 |  7474 | `		*piTypeFlags \|= iNullableFlag;` |
|        62 |  7475 | `	}` |
|         - |  7476 | `	/* Build canonical type text */` |
|    126787 |  7477 | `	if( pTypeText ){` |
|         - |  7478 | `		SyBlob sBlob;` |
|    126787 |  7479 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    190131 |  7480 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     63391 |  7481 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    126787 |  7482 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    189899 |  7483 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    126596 |  7484 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    126601 |  7485 | `			if( zDup ){` |
|    126601 |  7486 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     63298 |  7487 | `			}` |
|     63298 |  7488 | `		}` |
|    126787 |  7489 | `		SyBlobRelease(&sBlob);` |
|     63391 |  7490 | `	}` |
|         - |  7491 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7492 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7493 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7494 | `	{` |
|    126787 |  7495 | `		int nNonNull = 0;` |
|    126787 |  7496 | `		int iNonNullIdx = -1;` |
|         - |  7497 | `		int i;` |
|    253721 |  7498 | `		for( i = 0; i < nAtoms; i++ ){` |
|    126939 |  7499 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    126909 |  7500 | `				nNonNull++;` |
|    126909 |  7501 | `				iNonNullIdx = i;` |
|     63452 |  7502 | `			}` |
|     63472 |  7503 | `		}` |
|    126787 |  7504 | `		if( nNonNull <= 1 ){` |
|         - |  7505 | `			/* Fast path: store as single type. */` |
|    126681 |  7506 | `			if( iNonNullIdx >= 0 ){` |
|    126675 |  7507 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    126675 |  7508 | `				if( pA->nType == SXU32_HIGH ){` |
|     57308 |  7509 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19101 |  7510 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38207 |  7511 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38207 |  7512 | `					*pnType = SXU32_HIGH;` |
|     38207 |  7513 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    107574 |  7514 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7515 | `					*pnType = MEMOBJ_VOID;` |
|     88388 |  7516 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7517 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7518 | `				}else{` |
|     88287 |  7519 | `					*pnType = pA->nType;` |
|         - |  7520 | `				}` |
|     63335 |  7521 | `			}` |
|     63343 |  7522 | `		}else{` |
|         - |  7523 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7524 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7525 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7526 | `				ph7_type_alt sAlt;` |
|       249 |  7527 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7528 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7529 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7530 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7531 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7532 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7533 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7534 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7535 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7536 | `				}else{` |
|       145 |  7537 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7538 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7539 | `				}` |
|       239 |  7540 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7541 | `			}` |
|         - |  7542 | `		}` |
|         - |  7543 | `	}` |
|    126787 |  7544 | `	return SXRET_OK;` |
|     63404 |  7545 | `}` |
|         - |  7546 |  |
|         - |  7547 | `/*` |
|         - |  7548 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7549 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7550 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7551 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7552 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7553 | `` *          and union types `: T\|U`.`` |
|         - |  7554 | ` */` |
|   2668628 |  7555 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7556 | `{` |
|   2668633 |  7557 | `	sxi32 iFlags = 0;` |
|         - |  7558 | `	sxi32 rc;` |
|         - |  7559 | `	sxu32 nLine;` |
|   2668633 |  7560 | `	pFunc->nReturnType = 0;` |
|   2668633 |  7561 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2668633 |  7562 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7563 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7564 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7565 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7566 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7567 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2668633 |  7568 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2668633 |  7569 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2668633 |  7570 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2656587 |  7571 | `		return SXRET_OK;` |
|         - |  7572 | `	}` |
|     12051 |  7573 | `	pGen->pIn++; /* Skip ':' */` |
|     12051 |  7574 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7575 | `		return SXRET_OK;` |
|         - |  7576 | `	}` |
|     12051 |  7577 | `	nLine = pGen->pIn->nLine;` |
|     12051 |  7578 | `	rc = GenStateParseUnionTypeDecl(` |
|      6023 |  7579 | `		pGen,` |
|      6023 |  7580 | `		&pFunc->nReturnType,` |
|      6023 |  7581 | `		&pFunc->sReturnClass,` |
|      6023 |  7582 | `		&pFunc->aReturnUnion,` |
|         - |  7583 | `		&iFlags,` |
|      6023 |  7584 | `		&pFunc->sReturnTypeName,` |
|         - |  7585 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7586 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7587 | `		/* iUnionFlag */ 0,` |
|         - |  7588 | `		/* bAllowVoid */ 1,` |
|      6023 |  7589 | `		nLine);` |
|     12051 |  7590 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7591 | `		return SXERR_ABORT;` |
|         - |  7592 | `	}` |
|     12051 |  7593 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7594 | `		/* Error already reported */` |
|       ! 0 |  7595 | `		return SXERR_SYNTAX;` |
|         - |  7596 | `	}` |
|     12051 |  7597 | `	if( rc == SXERR_SYNTAX ){` |
|         8 |  7598 | `		if( pGen->pIn < pGen->pEnd ){` |
|        11 |  7599 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7600 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7601 | `				&pGen->pIn->sData);` |
|         5 |  7602 | `		}else{` |
|       ! 0 |  7603 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7604 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7605 | `		}` |
|         8 |  7606 | `		return SXERR_SYNTAX;` |
|         - |  7607 | `	}` |
|     12045 |  7608 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12045 |  7609 | `	return SXRET_OK;` |
|   1334319 |  7610 | `}` |
|         - |  7611 |  |
|    298034 |  7612 | `static sxi32 GenStateCompileFunc(` |
|         - |  7613 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7614 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7615 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7616 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7617 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7618 | `	)` |
|         5 |  7619 | `{` |
|         - |  7620 | `	ph7_vm_func *pFunc;` |
|         - |  7621 | `	SyToken *pEnd;` |
|         - |  7622 | `	sxu32 nLine;` |
|         - |  7623 | `	char *zName;` |
|         - |  7624 | `	sxi32 rc;` |
|         - |  7625 | `	/* Extract line number */` |
|    298039 |  7626 | `	nLine = pGen->pIn->nLine;` |
|         - |  7627 | `	/* Jump the left parenthesis '(' */` |
|    298039 |  7628 | `	pGen->pIn++;` |
|         - |  7629 | `	/* Delimit the function signature */` |
|    298039 |  7630 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    298039 |  7631 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7632 | `		/* Syntax error */` |
|         9 |  7633 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7634 | `		(void)pName;` |
|         9 |  7635 | `		if( rc == SXERR_ABORT ){` |
|         - |  7636 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7637 | `			return SXERR_ABORT;` |
|         - |  7638 | `		}` |
|         9 |  7639 | `		pGen->pIn = pGen->pEnd;` |
|         9 |  7640 | `		return SXRET_OK;` |
|         - |  7641 | `	}` |
|         - |  7642 | `	/* Create the function state */` |
|    298033 |  7643 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    298033 |  7644 | `	if( pFunc == 0 ){` |
|       ! 0 |  7645 | `		goto OutOfMem;` |
|         - |  7646 | `	}` |
|         - |  7647 | `	/* Build the function name, prepending namespace if active */` |
|    298040 |  7648 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7649 | `		SyBlob sFQN;` |
|         - |  7650 | `		sxu32 nLen;` |
|        16 |  7651 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7652 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7653 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7654 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7655 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7656 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7657 | `		SyBlobRelease(&sFQN);` |
|        16 |  7658 | `		if( zName == 0 ){` |
|       ! 0 |  7659 | `			goto OutOfMem;` |
|         - |  7660 | `		}` |
|        16 |  7661 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7662 | `	}else{` |
|    298019 |  7663 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    298019 |  7664 | `		if( zName == 0 ){` |
|       ! 0 |  7665 | `			goto OutOfMem;` |
|         - |  7666 | `		}` |
|    298019 |  7667 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7668 | `	}` |
|         - |  7669 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7670 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    298033 |  7671 | `	pFunc->nLine = nLine;` |
|    298033 |  7672 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    298033 |  7673 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7674 | `		return SXERR_ABORT;` |
|         - |  7675 | `	}` |
|    298033 |  7676 | `	if( pGen->pIn < pEnd ){` |
|         - |  7677 | `		/* Collect function arguments */` |
|    240255 |  7678 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    240255 |  7679 | `		if( rc == SXERR_ABORT ){` |
|         - |  7680 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7681 | `			return SXERR_ABORT;` |
|         - |  7682 | `		}` |
|    120125 |  7683 | `	}` |
|         - |  7684 | `	/* Point past ')' and parse optional return type ': type' */` |
|    298033 |  7685 | `	pGen->pIn = &pEnd[1];` |
|         - |  7686 | `	{` |
|    298033 |  7687 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    298033 |  7688 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7689 | `			return SXERR_ABORT;` |
|    298033 |  7690 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         8 |  7691 | `			return SXERR_SYNTAX;` |
|         - |  7692 | `		}` |
|         - |  7693 | `	}` |
|    298027 |  7694 | `	if( bHandleClosure ){` |
|         - |  7695 | `		ph7_vm_func_closure_env sEnv;` |
|       469 |  7696 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       464 |  7697 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       280 |  7698 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        91 |  7699 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7700 | `				/* Closure,record environment variable */` |
|        91 |  7701 | `				pGen->pIn++;` |
|        91 |  7702 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7703 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7704 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7705 | `						return SXERR_ABORT;` |
|         - |  7706 | `					}` |
|       ! 0 |  7707 | `				}` |
|        91 |  7708 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7709 | `				/* Compile until we hit the first closing parenthesis */` |
|       187 |  7710 | `				while( pGen->pIn < pGen->pEnd ){` |
|       187 |  7711 | `					int iFlagsLocal = 0;` |
|       187 |  7712 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        91 |  7713 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        91 |  7714 | `						break;` |
|         - |  7715 | `					}` |
|       101 |  7716 | `					nLineLocal = pGen->pIn->nLine;` |
|       101 |  7717 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7718 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7719 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7720 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7721 | `						pGen->pIn++;` |
|        27 |  7722 | `					}` |
|        96 |  7723 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       101 |  7724 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7725 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7726 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7727 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7728 | `								return SXERR_ABORT;` |
|         - |  7729 | `							}` |
|         - |  7730 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7731 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7732 | `								pGen->pIn++;` |
|       ! 0 |  7733 | `							}` |
|       ! 0 |  7734 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7735 | `								pGen->pIn++;` |
|       ! 0 |  7736 | `							}` |
|       ! 0 |  7737 | `							break;` |
|         - |  7738 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7739 | `					}else{` |
|         - |  7740 | `						SyString *pNameLocal;` |
|         - |  7741 | `						char *zDup;` |
|         - |  7742 | `						/* Duplicate variable name */` |
|       101 |  7743 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       101 |  7744 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       101 |  7745 | `						if( zDup ){` |
|         - |  7746 | `							/* Zero the structure */` |
|       101 |  7747 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       101 |  7748 | `							sEnv.iFlags = iFlagsLocal;` |
|       101 |  7749 | `							sEnv.nIdx = SXU32_HIGH;` |
|       101 |  7750 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       101 |  7751 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       116 |  7752 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7753 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7754 | `									got_this = 1;` |
|       ! 0 |  7755 | `							}` |
|         - |  7756 | `							/* Save imported variable */` |
|       101 |  7757 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        53 |  7758 | `						}else{` |
|       ! 0 |  7759 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7760 | `							 return SXERR_ABORT;` |
|         - |  7761 | `						}` |
|         - |  7762 | `					}` |
|       101 |  7763 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       113 |  7764 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7765 | `						/* Ignore trailing commas */` |
|        13 |  7766 | `						pGen->pIn++;` |
|         1 |  7767 | `					}` |
|         5 |  7768 | `				}` |
|         - |  7769 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7770 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7771 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7772 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7773 | `				 * legacy pre-use position. */` |
|        91 |  7774 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7775 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7776 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7777 | `						return SXERR_ABORT;` |
|         7 |  7778 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7779 | `						return SXERR_SYNTAX;` |
|         - |  7780 | `					}` |
|         3 |  7781 | `				}` |
|        43 |  7782 | `		}` |
|       469 |  7783 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7784 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7785 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7786 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7787 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7788 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7789 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7790 | `			 * closure never binds $this (php). */` |
|       461 |  7791 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       461 |  7792 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       461 |  7793 | `			sEnv.nIdx = SXU32_HIGH;` |
|       461 |  7794 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       461 |  7795 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       461 |  7796 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       228 |  7797 | `		}` |
|       469 |  7798 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7799 | `			/* Mark as closure */` |
|       463 |  7800 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       229 |  7801 | `		}` |
|       232 |  7802 | `	}` |
|         - |  7803 | `	/* Compile the body */` |
|    298027 |  7804 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    298027 |  7805 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7806 | `		return SXERR_ABORT;` |
|         - |  7807 | `	}` |
|         - |  7808 | `	/* The cursor sits just past the body's closing brace */` |
|    298027 |  7809 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    298027 |  7810 | `	if( ppFunc ){` |
|    298027 |  7811 | `		*ppFunc = pFunc;` |
|    149011 |  7812 | `	}` |
|    298027 |  7813 | `	rc = SXRET_OK;` |
|    298027 |  7814 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7815 | `		/* Finally register the function */` |
|    297569 |  7816 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    148782 |  7817 | `	}` |
|    298027 |  7818 | `	if( rc == SXRET_OK ){` |
|    298027 |  7819 | `		return SXRET_OK;` |
|         - |  7820 | `	}` |
|         - |  7821 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7822 | `OutOfMem:` |
|         - |  7823 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7824 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7825 | `	 */` |
|       ! 0 |  7826 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7827 | `	return SXERR_ABORT;` |
|    149022 |  7828 | `}` |
|         - |  7829 | `/*` |
|         - |  7830 | ` * Compile a standard PHP function.` |
|         - |  7831 | ` *  Refer to the block-comment above for more information.` |
|         - |  7832 | ` */` |
|    297578 |  7833 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7834 | `{` |
|         - |  7835 | `	SyString *pName;` |
|         - |  7836 | `	sxi32 iFlags;` |
|         - |  7837 | `	sxu32 nKwLine;` |
|         - |  7838 | `	sxu32 nLine;` |
|         - |  7839 | `	sxi32 rc;` |
|         - |  7840 |  |
|    297583 |  7841 | `	nLine = pGen->pIn->nLine;` |
|    297583 |  7842 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    297583 |  7843 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    297583 |  7844 | `	iFlags = 0;` |
|    297583 |  7845 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7846 | `		/* Return by reference,remember that */` |
|        12 |  7847 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7848 | `		/* Jump the '&' token */` |
|        12 |  7849 | `		pGen->pIn++;` |
|         5 |  7850 | `	}` |
|    297583 |  7851 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7852 | `		/* Invalid function name */` |
|         8 |  7853 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         8 |  7854 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7855 | `			return SXERR_ABORT;` |
|         - |  7856 | `		}` |
|         - |  7857 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  7858 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  7859 | `			pGen->pIn++;` |
|         2 |  7860 | `		}` |
|         8 |  7861 | `		return SXRET_OK;` |
|         - |  7862 | `	}` |
|    297577 |  7863 | `	pName = &pGen->pIn->sData;` |
|    297577 |  7864 | `	nLine = pGen->pIn->nLine;` |
|         - |  7865 | `	/* Jump the function name */` |
|    297577 |  7866 | `	pGen->pIn++;` |
|    297577 |  7867 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7868 | `		/* Syntax error */` |
|         3 |  7869 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  7870 | `		if( rc == SXERR_ABORT ){` |
|         - |  7871 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7872 | `			return SXERR_ABORT;` |
|         - |  7873 | `		}` |
|         - |  7874 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  7875 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  7876 | `			pGen->pIn++;` |
|       ! 0 |  7877 | `		}` |
|         3 |  7878 | `		return SXRET_OK;` |
|         - |  7879 | `	}` |
|         - |  7880 | `	/* Compile function body */` |
|         - |  7881 | `	{` |
|    297575 |  7882 | `		ph7_vm_func *pFuncState = 0;` |
|    297575 |  7883 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    297575 |  7884 | `		if( pFuncState ){` |
|         - |  7885 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    297563 |  7886 | `			pFuncState->nLine = nKwLine;` |
|    148779 |  7887 | `		}` |
|         - |  7888 | `	}` |
|    297575 |  7889 | `	return rc;` |
|    148794 |  7890 | `}` |
|         - |  7891 | `/*` |
|         - |  7892 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  7893 | ` * According to the PHP language reference manual` |
|         - |  7894 | ` *  Visibility:` |
|         - |  7895 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  7896 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  7897 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  7898 | ` *  Members declared protected can be accessed only within the class` |
|         - |  7899 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  7900 | ` *  may only be accessed by the class that defines the member.` |
|         - |  7901 | ` */` |
|   3111690 |  7902 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  7903 | `{` |
|   3111695 |  7904 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    250729 |  7905 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2860971 |  7906 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    189887 |  7907 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  7908 | `	}` |
|         - |  7909 | `	/* Assume public by default */` |
|   2671089 |  7910 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1555850 |  7911 | `}` |
|         - |  7912 | `/*` |
|         - |  7913 | ` * Compile a class constant.` |
|         - |  7914 | ` * According to the PHP language reference manual` |
|         - |  7915 | ` *  Class Constants` |
|         - |  7916 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  7917 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  7918 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  7919 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  7920 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  7921 | ` *   It's also possible for interfaces to have constants.` |
|         - |  7922 | ` * Symisc eXtension.` |
|         - |  7923 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  7924 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  7925 | ` *  Example:` |
|         - |  7926 | ` *   class Test{` |
|         - |  7927 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  7928 | ` *   };` |
|         - |  7929 | ` *   var_dump(TEST::MyConst);` |
|         - |  7930 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  7931 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  7932 | ` */` |
|         - |  7933 | `/*` |
|         - |  7934 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  7935 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  7936 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  7937 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  7938 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  7939 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  7940 | ` */` |
|    288672 |  7941 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  7942 | `{` |
|         - |  7943 | `	SyToken *p0, *p1;` |
|    288677 |  7944 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7945 | `		return 0;` |
|         - |  7946 | `	}` |
|    288677 |  7947 | `	p0 = pGen->pIn;` |
|         - |  7948 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    288677 |  7949 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  7950 | `		return 1;` |
|         - |  7951 | `	}` |
|    288677 |  7952 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  7953 | `		return 1;` |
|         - |  7954 | `	}` |
|         - |  7955 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  7956 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  7957 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    288673 |  7958 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    288673 |  7959 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    288673 |  7960 | `		if( p1 ){` |
|    288673 |  7961 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  7962 | `				return 1;` |
|         - |  7963 | `			}` |
|    288643 |  7964 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  7965 | `				return 1;` |
|         - |  7966 | `			}` |
|    144317 |  7967 | `		}` |
|    144317 |  7968 | `	}` |
|    288639 |  7969 | `	return 0;` |
|    144341 |  7970 | `}` |
|         - |  7971 | `/*` |
|         - |  7972 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  7973 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  7974 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  7975 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  7976 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  7977 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  7978 | ` * Peek only; never consumes tokens.` |
|         - |  7979 | ` */` |
|        24 |  7980 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  7981 | `{` |
|        28 |  7982 | `	SyToken *p = pGen->pIn;` |
|        39 |  7983 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  7984 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  7985 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  7986 | `	}` |
|        28 |  7987 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  7988 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  7989 | `	}` |
|         6 |  7990 | `	p++;` |
|         - |  7991 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  7992 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  7993 | `}` |
|         - |  7994 | `/*` |
|         - |  7995 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  7996 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  7997 | `` * `$o->new`), not a `new` expression.`` |
|         - |  7998 | ` */` |
|       110 |  7999 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  8000 | `{` |
|         - |  8001 | `	sxi32 iOp;` |
|       114 |  8002 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  8003 | `		return 0;` |
|         - |  8004 | `	}` |
|       104 |  8005 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  8006 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  8007 | `}` |
|         - |  8008 | `/*` |
|         - |  8009 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  8010 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  8011 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  8012 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  8013 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  8014 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  8015 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  8016 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  8017 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  8018 | ` *` |
|         - |  8019 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  8020 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  8021 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  8022 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  8023 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  8024 | ` */` |
|    619606 |  8025 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  8026 | `{` |
|    619611 |  8027 | `	SyToken *p = pGen->pIn;` |
|    619611 |  8028 | `	int iDepth = 0;` |
|   1623707 |  8029 | `	while( p < pGen->pEnd ){` |
|   1623707 |  8030 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    619559 |  8031 | `			break; /* end of this initializer */` |
|         - |  8032 | `		}` |
|   1004148 |  8033 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    505891 |  8034 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7624 |  8035 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  8036 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  8037 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  8038 | `			 * expression. */` |
|         3 |  8039 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  8040 | `			p++;` |
|         3 |  8041 | `			if( bArrow ){` |
|         - |  8042 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  8043 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  8044 | `				int iBase = iDepth;` |
|        17 |  8045 | `				while( p < pGen->pEnd ){` |
|        17 |  8046 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  8047 | `						iDepth++;` |
|        15 |  8048 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  8049 | `						if( iDepth <= iBase ){` |
|       ! 0 |  8050 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  8051 | `						}` |
|         5 |  8052 | `						iDepth--;` |
|        11 |  8053 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  8054 | `						break;` |
|         - |  8055 | `					}` |
|        15 |  8056 | `					p++;` |
|         1 |  8057 | `				}` |
|         2 |  8058 | `			}else{` |
|         - |  8059 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  8060 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  8061 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  8062 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  8063 | `				int iLocal = 0;` |
|       ! 0 |  8064 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  8065 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  8066 | `						break; /* body brace */` |
|         - |  8067 | `					}` |
|       ! 0 |  8068 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  8069 | `						iLocal++;` |
|       ! 0 |  8070 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  8071 | `						if( iLocal > 0 ){` |
|       ! 0 |  8072 | `							iLocal--;` |
|       ! 0 |  8073 | `						}` |
|       ! 0 |  8074 | `					}` |
|       ! 0 |  8075 | `					p++;` |
|       ! 0 |  8076 | `				}` |
|       ! 0 |  8077 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  8078 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  8079 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  8080 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  8081 | `							iBrace++;` |
|       ! 0 |  8082 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  8083 | `							iBrace--;` |
|       ! 0 |  8084 | `							if( iBrace == 0 ){` |
|       ! 0 |  8085 | `								p++;` |
|       ! 0 |  8086 | `								break;` |
|         - |  8087 | `							}` |
|       ! 0 |  8088 | `						}` |
|       ! 0 |  8089 | `						p++;` |
|       ! 0 |  8090 | `					}` |
|       ! 0 |  8091 | `				}` |
|         - |  8092 | `			}` |
|         3 |  8093 | `			continue;` |
|         - |  8094 | `		}` |
|   1004151 |  8095 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  8096 | `			if( iDepth == 0 ){` |
|         - |  8097 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  8098 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  8099 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  8100 | `				 * is legal — don't scan into it. */` |
|        45 |  8101 | `				break;` |
|         - |  8102 | `			}` |
|       ! 0 |  8103 | `			iDepth++;` |
|   1004107 |  8104 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     41845 |  8105 | `			iDepth++;` |
|    983187 |  8106 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     41843 |  8107 | `			if( iDepth > 0 ){` |
|     41843 |  8108 | `				iDepth--;` |
|     20919 |  8109 | `			}` |
|    941348 |  8110 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    334837 |  8111 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8112 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8113 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8114 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  8115 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  8116 | `				return 1;` |
|         - |  8117 | `			}` |
|       ! 0 |  8118 | `		}` |
|   1004099 |  8119 | `		p++;` |
|         5 |  8120 | `	}` |
|    619603 |  8121 | `	return 0;` |
|    309808 |  8122 | `}` |
|         - |  8123 | `/*` |
|         - |  8124 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8125 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8126 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8127 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8128 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8129 | ` * share the same backing.` |
|         - |  8130 | ` */` |
|       350 |  8131 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8132 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8133 | `{` |
|       355 |  8134 | `	pAttr->nType = nType;` |
|       355 |  8135 | `	pAttr->sClass = *pClass;` |
|       355 |  8136 | `	pAttr->sTypeName = *pTypeName;` |
|       355 |  8137 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8138 | `		sxu32 i;` |
|        73 |  8139 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8140 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8141 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8142 | `		}` |
|        11 |  8143 | `	}` |
|       355 |  8144 | `}` |
|    288672 |  8145 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8146 | `{` |
|    288677 |  8147 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8148 | `	SySet *pInstrContainer;` |
|         - |  8149 | `	ph7_class_attr *pCons;` |
|         - |  8150 | `	SyString *pName;` |
|         - |  8151 | `	sxi32 rc;` |
|    288677 |  8152 | `	sxu32 nType = 0;` |
|         - |  8153 | `	SyString sTypeClass;` |
|         - |  8154 | `	SyString sTypeText;` |
|         - |  8155 | `	SySet aUnionAlts;` |
|    288677 |  8156 | `	sxi32 iTypeFlags = 0;` |
|    288677 |  8157 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    288677 |  8158 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    288677 |  8159 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8160 | `	/* Extract visibility level */` |
|    288677 |  8161 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8162 | `	/* Mark as constant */` |
|    288677 |  8163 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    288677 |  8164 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8165 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8166 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    288696 |  8167 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8168 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8169 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8170 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8171 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8172 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8173 | `		 * and success paths release. */` |
|        42 |  8174 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8175 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8176 | `			goto Synchronize;` |
|        42 |  8177 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8178 | `			return SXERR_ABORT;` |
|        42 |  8179 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8180 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8181 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8182 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8183 | `				return SXERR_ABORT;` |
|         - |  8184 | `			}` |
|       ! 0 |  8185 | `			goto Synchronize;` |
|         - |  8186 | `		}` |
|        42 |  8187 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8188 | `	}` |
|    144336 |  8189 | `loop:` |
|    288679 |  8190 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8191 | `		/* Invalid constant name */` |
|       ! 0 |  8192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8193 | `		if( rc == SXERR_ABORT ){` |
|         - |  8194 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8195 | `			return SXERR_ABORT;` |
|         - |  8196 | `		}` |
|       ! 0 |  8197 | `		goto Synchronize;` |
|         - |  8198 | `	}` |
|         - |  8199 | `	/* Peek constant name */` |
|    288679 |  8200 | `	pName = &pGen->pIn->sData;` |
|         - |  8201 | `	/* Make sure the constant name isn't reserved */` |
|    288679 |  8202 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8203 | `		/* Reserved constant name */` |
|       ! 0 |  8204 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8205 | `		if( rc == SXERR_ABORT ){` |
|         - |  8206 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8207 | `			return SXERR_ABORT;` |
|         - |  8208 | `		}` |
|       ! 0 |  8209 | `		goto Synchronize;` |
|         - |  8210 | `	}` |
|         - |  8211 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    288679 |  8212 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8213 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8214 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8215 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8216 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8217 | `			return SXERR_ABORT;` |
|        42 |  8218 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8219 | `			goto Synchronize;` |
|         - |  8220 | `		}` |
|        18 |  8221 | `	}` |
|         - |  8222 | `	/* Advance the stream cursor */` |
|    288677 |  8223 | `	pGen->pIn++;` |
|    288677 |  8224 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8225 | `		/* Invalid declaration */` |
|       ! 0 |  8226 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8227 | `		if( rc == SXERR_ABORT ){` |
|         - |  8228 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8229 | `			return SXERR_ABORT;` |
|         - |  8230 | `		}` |
|       ! 0 |  8231 | `		goto Synchronize;` |
|         - |  8232 | `	}` |
|    288677 |  8233 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8234 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8235 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8236 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8237 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    288672 |  8238 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8239 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8240 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8241 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8242 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8243 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8244 | `			return SXERR_ABORT;` |
|         - |  8245 | `		}` |
|         6 |  8246 | `		goto Synchronize;` |
|         - |  8247 | `	}` |
|         - |  8248 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8249 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8250 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    288673 |  8251 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8252 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8253 | `			"New expressions are not supported in this context");` |
|         5 |  8254 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8255 | `			return SXERR_ABORT;` |
|         - |  8256 | `		}` |
|         5 |  8257 | `		goto Synchronize;` |
|         - |  8258 | `	}` |
|         - |  8259 | `	/* Allocate a new class attribute */` |
|    288669 |  8260 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    288669 |  8261 | `	if( pCons ){` |
|    288669 |  8262 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    288669 |  8263 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8264 | `			return SXERR_ABORT;` |
|         - |  8265 | `		}` |
|    144332 |  8266 | `	}` |
|    288669 |  8267 | `	if( pCons == 0 ){` |
|       ! 0 |  8268 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8269 | `		return SXERR_ABORT;` |
|         - |  8270 | `	}` |
|    288669 |  8271 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8272 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8273 | `	}` |
|         - |  8274 | `	/* Swap bytecode container */` |
|    288669 |  8275 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    288669 |  8276 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8277 | `	/* Compile constant value.` |
|         - |  8278 | `	 */` |
|    288669 |  8279 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    288669 |  8280 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8281 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8282 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8283 | `			return SXERR_ABORT;` |
|         - |  8284 | `		}` |
|         1 |  8285 | `	}` |
|         - |  8286 | `	/* Emit the done instruction */` |
|    288669 |  8287 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    288669 |  8288 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    288669 |  8289 | `	if( rc == SXERR_ABORT ){` |
|         - |  8290 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8291 | `		return SXERR_ABORT;` |
|         - |  8292 | `	}` |
|         - |  8293 | `	/* All done,install the constant */` |
|    288669 |  8294 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    288669 |  8295 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8296 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8297 | `		return SXERR_ABORT;` |
|         - |  8298 | `	}` |
|    288669 |  8299 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8300 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8301 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8302 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8303 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8304 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8305 | `				pTok--;` |
|       ! 0 |  8306 | `			}` |
|       ! 0 |  8307 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8308 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8309 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8310 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8311 | `				return SXERR_ABORT;` |
|         - |  8312 | `			}` |
|       ! 0 |  8313 | `		}else{` |
|         3 |  8314 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8315 | `				goto loop;` |
|         - |  8316 | `			}` |
|         - |  8317 | `		}` |
|       ! 0 |  8318 | `	}` |
|    288667 |  8319 | `	SySetRelease(&aUnionAlts);` |
|    288667 |  8320 | `	return SXRET_OK;` |
|         5 |  8321 | `Synchronize:` |
|        13 |  8322 | `	SySetRelease(&aUnionAlts);` |
|         - |  8323 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8324 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8325 | `		pGen->pIn++;` |
|         3 |  8326 | `	}` |
|        13 |  8327 | `	return SXERR_CORRUPT;` |
|    144341 |  8328 | `}` |
|         - |  8329 | `/*` |
|         - |  8330 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8331 | ` * According to the PHP language reference manual` |
|         - |  8332 | ` *  Properties` |
|         - |  8333 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8334 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8335 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8336 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8337 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8338 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8339 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8340 | ` * Symisc eXtension.` |
|         - |  8341 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8342 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8343 | ` *  Example:` |
|         - |  8344 | ` *   class Test{` |
|         - |  8345 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8346 | ` *   };` |
|         - |  8347 | ` *   var_dump(TEST::myVar);` |
|         - |  8348 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8349 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8350 | ` */` |
|         - |  8351 | `/*` |
|         - |  8352 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8353 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8354 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8355 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8356 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8357 | ` */` |
|   2324796 |  8358 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8359 | `{` |
|   2324801 |  8360 | `	SyToken *p = pStart;` |
|   2324801 |  8361 | `	int bFirst = 1;` |
|   2324801 |  8362 | `	if( p >= pEnd ) return 0;` |
|         - |  8363 | ``	/* Optional nullable `?` shorthand. */`` |
|   2324801 |  8364 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        35 |  8365 | `		p++;` |
|        35 |  8366 | `		if( p >= pEnd ) return 0;` |
|        16 |  8367 | `	}` |
|         - |  8368 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8369 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8370 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8371 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1162398 |  8372 | `	for(;;){` |
|   2324821 |  8373 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8374 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8375 | `			p++;` |
|         9 |  8376 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8377 | `			if( p >= pEnd ) return 0;` |
|         3 |  8378 | `			p++; /* skip ')' */` |
|         2 |  8379 | `		}else{` |
|         - |  8380 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8381 | ``			 * then any `&`-joined intersection members. */`` |
|   2324819 |  8382 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2324819 |  8383 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8384 | `				return 0;` |
|         - |  8385 | `			}` |
|         - |  8386 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8387 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8388 | `			 * may still appear at the initial dispatch site). */` |
|   2324819 |  8389 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2324771 |  8390 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2324766 |  8391 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    102902 |  8392 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2324489 |  8393 | `					return 0;` |
|         - |  8394 | `				}` |
|       141 |  8395 | `			}` |
|       335 |  8396 | `			p++;` |
|       337 |  8397 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8398 | `				p += 2;` |
|         1 |  8399 | `			}` |
|       498 |  8400 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       338 |  8401 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8402 | `				p++; /* skip '&' */` |
|         3 |  8403 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8404 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8405 | `				p++;` |
|         3 |  8406 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8407 | `					p += 2;` |
|       ! 0 |  8408 | `				}` |
|         1 |  8409 | `			}` |
|         - |  8410 | `		}` |
|       337 |  8411 | `		bFirst = 0;` |
|       332 |  8412 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8413 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8414 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8415 | `			continue;` |
|         - |  8416 | `		}` |
|       317 |  8417 | `		break;` |
|       ! 0 |  8418 | `	}` |
|       317 |  8419 | `	if( p >= pEnd ) return 0;` |
|       317 |  8420 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1162403 |  8421 | `}` |
|         - |  8422 |  |
|         - |  8423 | `/*` |
|         - |  8424 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8425 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8426 | ` * if not). Recognized forms:` |
|         - |  8427 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8428 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8429 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8430 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8431 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8432 | ` * on unrecoverable error.` |
|         - |  8433 | ` *` |
|         - |  8434 | ` * When a type is parsed:` |
|         - |  8435 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8436 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8437 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8438 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8439 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8440 | ` */` |
|       322 |  8441 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8442 | `	ph7_gen_state *pGen,` |
|         - |  8443 | `	sxu32 *pnType,` |
|         - |  8444 | `	SyString *pClass,` |
|         - |  8445 | `	sxi32 *piTypeFlags,` |
|         - |  8446 | `	SyString *pTypeText,` |
|         - |  8447 | `	SySet *pAlts` |
|         5 |  8448 | `){` |
|       327 |  8449 | `	sxi32 iFlags = 0;` |
|         - |  8450 | `	sxi32 rc;` |
|       327 |  8451 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8452 | `		return SXRET_OK;` |
|         - |  8453 | `	}` |
|         - |  8454 | `	/* If the first token is '$', there's no type */` |
|       327 |  8455 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8456 | `		return SXRET_OK;` |
|         - |  8457 | `	}` |
|       327 |  8458 | `	rc = GenStateParseUnionTypeDecl(` |
|       161 |  8459 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8460 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8461 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8462 | `		/* bAllowVoid */ 0,` |
|       322 |  8463 | `		pGen->pIn->nLine);` |
|       327 |  8464 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8465 | `		return rc;` |
|         - |  8466 | `	}` |
|         - |  8467 | `	/* Verify next token is '$' (start of property name) */` |
|       327 |  8468 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8469 | `		return SXERR_SYNTAX;` |
|         - |  8470 | `	}` |
|       327 |  8471 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       327 |  8472 | `	return SXRET_OK;` |
|       166 |  8473 | `}` |
|         - |  8474 |  |
|         - |  8475 | `/*` |
|         - |  8476 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8477 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8478 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8479 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8480 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8481 | ` * by the type parser itself before reaching here.` |
|         - |  8482 | ` *` |
|         - |  8483 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8484 | ` * use in the error message.` |
|         - |  8485 | ` */` |
|       498 |  8486 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8487 | `	sxu32 nType,` |
|         - |  8488 | `	const SyString *pClass,` |
|         - |  8489 | `	const char **pzName,` |
|         - |  8490 | `	sxu32 *pnName)` |
|         5 |  8491 | `{` |
|         - |  8492 | `	const char *z;` |
|         - |  8493 | `	sxu32 n;` |
|       503 |  8494 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       449 |  8495 | `		return 0;` |
|         - |  8496 | `	}` |
|        59 |  8497 | `	z = pClass->zString;` |
|        59 |  8498 | `	n = pClass->nByte;` |
|        59 |  8499 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8500 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8501 | `	}` |
|         - |  8502 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8503 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8504 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        52 |  8505 | `	return 0;` |
|       254 |  8506 | `}` |
|         - |  8507 |  |
|         - |  8508 | `/*` |
|         - |  8509 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8510 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8511 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8512 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8513 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8514 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8515 | ` *` |
|         - |  8516 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8517 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8518 | ` */` |
|       436 |  8519 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8520 | `	ph7_gen_state *pGen,` |
|         - |  8521 | `	ph7_class *pClass,` |
|         - |  8522 | `	const SyString *pMemberName,` |
|         - |  8523 | `	sxu32 nType,` |
|         - |  8524 | `	const SyString *pTypeClass,` |
|         - |  8525 | `	const SyString *pTypeText,` |
|         - |  8526 | `	SySet *pUnionAlts,` |
|         - |  8527 | `	const char *zErrFmt,` |
|         - |  8528 | `	sxu32 nLine)` |
|         5 |  8529 | `{` |
|       441 |  8530 | `	const char *zBad = 0;` |
|       441 |  8531 | `	sxu32 nBad = 0;` |
|         - |  8532 | `	SyString sFallback;` |
|         - |  8533 | `	const SyString *pBad;` |
|         - |  8534 | `	sxi32 rc;` |
|       441 |  8535 | `	int bDisallowed = 0;` |
|       441 |  8536 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8537 | `		bDisallowed = 1;` |
|       439 |  8538 | `	}else if( pUnionAlts ){` |
|         - |  8539 | `		sxu32 i;` |
|        95 |  8540 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8541 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8542 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8543 | `				bDisallowed = 1;` |
|         3 |  8544 | `				break;` |
|         - |  8545 | `			}` |
|        35 |  8546 | `		}` |
|        15 |  8547 | `	}` |
|       441 |  8548 | `	if( !bDisallowed ){` |
|       435 |  8549 | `		return SXRET_OK;` |
|         - |  8550 | `	}` |
|         - |  8551 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8552 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8553 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8554 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8555 | `		pBad = pTypeText;` |
|         5 |  8556 | `	}else{` |
|       ! 0 |  8557 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8558 | `		pBad = &sFallback;` |
|         - |  8559 | `	}` |
|        11 |  8560 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8561 | `		zErrFmt,` |
|         3 |  8562 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8563 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8564 | `		return SXERR_ABORT;` |
|         - |  8565 | `	}` |
|         8 |  8566 | `	return SXERR_SYNTAX;` |
|       223 |  8567 | `}` |
|         - |  8568 | `/*` |
|         - |  8569 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8570 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8571 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8572 | ` * than promoted to a lexer keyword.` |
|         - |  8573 | ` */` |
|  17876320 |  8574 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8575 | `{` |
|  18070476 |  8576 | `	return (pTok->nType & PH7_TK_ID)` |
|   9132311 |  8577 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  18070471 |  8578 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8579 | `}` |
|         - |  8580 | `/*` |
|         - |  8581 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8582 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8583 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8584 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8585 | ` */` |
|   6828596 |  8586 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8587 | `{` |
|   6828601 |  8588 | `	*pnTok = 0;` |
|   6828596 |  8589 | `	if( &pTok[3] < pEnd` |
|   6431057 |  8590 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5427213 |  8591 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2410462 |  8592 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8593 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8594 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8595 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8596 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8597 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8598 | `			*pnTok = 4;` |
|        17 |  8599 | `			return nKw;` |
|         - |  8600 | `		}` |
|       ! 0 |  8601 | `	}` |
|   6828585 |  8602 | `	return 0;` |
|   3414303 |  8603 | `}` |
|         - |  8604 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8605 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8606 | `{` |
|        17 |  8607 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8608 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8609 | `	}` |
|         5 |  8610 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8611 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8612 | `	}` |
|         3 |  8613 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8614 | `}` |
|    452704 |  8615 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8616 | `{` |
|    452709 |  8617 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8618 | `	ph7_class_attr *pAttr;` |
|         - |  8619 | `	SyString *pName;` |
|         - |  8620 | `	sxi32 rc;` |
|    452709 |  8621 | `	sxu32 nType = 0;` |
|         - |  8622 | `	SyString sTypeClass;` |
|         - |  8623 | `	SyString sTypeText;` |
|         - |  8624 | `	SySet aUnionAlts;` |
|    452709 |  8625 | `	sxi32 iTypeFlags = 0;` |
|    452709 |  8626 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    452709 |  8627 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    452709 |  8628 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8629 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8630 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8631 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    452709 |  8632 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8633 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8634 | `	}` |
|         - |  8635 | `	/* Extract visibility level */` |
|    452709 |  8636 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8637 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    452870 |  8638 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       327 |  8639 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       327 |  8640 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8641 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8642 | `			goto Synchronize;` |
|       327 |  8643 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8644 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8645 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8646 | `				&pGen->pIn->sData);` |
|       ! 0 |  8647 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8648 | `				return SXERR_ABORT;` |
|         - |  8649 | `			}` |
|       ! 0 |  8650 | `			goto Synchronize;` |
|       327 |  8651 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8652 | `			return SXERR_ABORT;` |
|         - |  8653 | `		}` |
|       161 |  8654 | `	}` |
|       ! 0 |  8655 | `loop:` |
|    452713 |  8656 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8657 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8658 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8659 | `			return SXERR_ABORT;` |
|         - |  8660 | `		}` |
|       ! 0 |  8661 | `		goto Synchronize;` |
|         - |  8662 | `	}` |
|    452713 |  8663 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    452713 |  8664 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8665 | `		/* Invalid attribute name */` |
|       ! 0 |  8666 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8667 | `		if( rc == SXERR_ABORT ){` |
|         - |  8668 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8669 | `			return SXERR_ABORT;` |
|         - |  8670 | `		}` |
|       ! 0 |  8671 | `		goto Synchronize;` |
|         - |  8672 | `	}` |
|         - |  8673 | `	/* Peek attribute name */` |
|    452713 |  8674 | `	pName = &pGen->pIn->sData;` |
|         - |  8675 | `	/* Advance the stream cursor */` |
|    452713 |  8676 | `	pGen->pIn++;` |
|    452713 |  8677 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8678 | `		/* Invalid declaration */` |
|         3 |  8679 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8680 | `		if( rc == SXERR_ABORT ){` |
|         - |  8681 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8682 | `			return SXERR_ABORT;` |
|         - |  8683 | `		}` |
|         3 |  8684 | `		goto Synchronize;` |
|         - |  8685 | `	}` |
|         - |  8686 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8687 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    452711 |  8688 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8689 | `		const char *zAvErr = 0;` |
|        19 |  8690 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8691 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8692 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8693 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8694 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8695 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8696 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8697 | `		}` |
|        13 |  8698 | `		if( zAvErr ){` |
|       ! 0 |  8699 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8700 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8701 | `				return SXERR_ABORT;` |
|         - |  8702 | `			}` |
|       ! 0 |  8703 | `			goto Synchronize;` |
|         - |  8704 | `		}` |
|         6 |  8705 | `	}` |
|         - |  8706 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8707 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    452711 |  8708 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8709 | `		const char *zRoErr = 0;` |
|        43 |  8710 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8711 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8712 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8713 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8714 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8715 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8716 | `		}` |
|        43 |  8717 | `		if( zRoErr ){` |
|        13 |  8718 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8719 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8720 | `				return SXERR_ABORT;` |
|         - |  8721 | `			}` |
|        13 |  8722 | `			goto Synchronize;` |
|         - |  8723 | `		}` |
|        14 |  8724 | `	}` |
|         - |  8725 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8726 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8727 | `	 * by the type parser. */` |
|    452701 |  8728 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       485 |  8729 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8730 | `			&sTypeText,` |
|       320 |  8731 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       160 |  8732 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       325 |  8733 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8734 | `			return SXERR_ABORT;` |
|       325 |  8735 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8736 | `			goto Synchronize;` |
|         - |  8737 | `		}` |
|       160 |  8738 | `	}` |
|         - |  8739 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    452701 |  8740 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8741 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8742 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8743 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8744 | `			return SXERR_ABORT;` |
|         - |  8745 | `		}` |
|         3 |  8746 | `		goto Synchronize;` |
|         - |  8747 | `	}` |
|         - |  8748 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8749 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8750 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8751 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8752 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8753 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    452699 |  8754 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8755 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8756 | `			"New expressions are not supported in this context");` |
|         6 |  8757 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8758 | `			return SXERR_ABORT;` |
|         - |  8759 | `		}` |
|         6 |  8760 | `		goto Synchronize;` |
|         - |  8761 | `	}` |
|         - |  8762 | `	/* Allocate a new class attribute */` |
|    452695 |  8763 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    452695 |  8764 | `	if( pAttr ){` |
|    452695 |  8765 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    452695 |  8766 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8767 | `			return SXERR_ABORT;` |
|         - |  8768 | `		}` |
|    226345 |  8769 | `	}` |
|    452695 |  8770 | `	if( pAttr == 0 ){` |
|       ! 0 |  8771 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8772 | `		return SXERR_ABORT;` |
|         - |  8773 | `	}` |
|    452695 |  8774 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       323 |  8775 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       159 |  8776 | `	}` |
|    452695 |  8777 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8778 | `		SySet *pInstrContainer;` |
|    330939 |  8779 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    330939 |  8780 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8781 | `		{` |
|         - |  8782 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8783 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8784 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8785 | `			 * compiler would otherwise run into the hook tokens. */` |
|    330939 |  8786 | `			SyToken *pScan = pGen->pIn;` |
|    330939 |  8787 | `			sxi32 iNest = 0;` |
|    715363 |  8788 | `			while( pScan < pGen->pEnd ){` |
|    715363 |  8789 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     41843 |  8790 | `					iNest++;` |
|    694444 |  8791 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     41843 |  8792 | `					iNest--;` |
|    652606 |  8793 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    330939 |  8794 | `					break;` |
|         - |  8795 | `				}` |
|    384429 |  8796 | `				pScan++;` |
|         5 |  8797 | `			}` |
|    330939 |  8798 | `			pGen->pEnd = pScan;` |
|         - |  8799 | `		}` |
|         - |  8800 | `		/* Swap bytecode container */` |
|    330939 |  8801 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    330939 |  8802 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8803 | `		/* Compile attribute value.` |
|         - |  8804 | `		 */` |
|    330939 |  8805 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    330939 |  8806 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8807 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8808 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8809 | `				return SXERR_ABORT;` |
|         - |  8810 | `			}` |
|       ! 0 |  8811 | `		}` |
|         - |  8812 | `		/* Emit the done instruction */` |
|    330939 |  8813 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    330939 |  8814 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    330939 |  8815 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    330939 |  8816 | `		pGen->pEnd = pSavedDefEnd;` |
|    165467 |  8817 | `	}` |
|         - |  8818 | `	/* All done,install the attribute */` |
|    452695 |  8819 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    452695 |  8820 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8821 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8822 | `		return SXERR_ABORT;` |
|         - |  8823 | `	}` |
|    452695 |  8824 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8825 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8826 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8827 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8828 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8829 | `			return SXERR_ABORT;` |
|         - |  8830 | `		}` |
|        95 |  8831 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8832 | `			goto Synchronize;` |
|         - |  8833 | `		}` |
|        95 |  8834 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8835 | `		return SXRET_OK;` |
|         - |  8836 | `	}` |
|    452601 |  8837 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8838 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8839 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8840 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8841 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8842 | `				? "Interfaces may only include hooked properties"` |
|         - |  8843 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8844 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8845 | `			return SXERR_ABORT;` |
|         - |  8846 | `		}` |
|       ! 0 |  8847 | `		goto Synchronize;` |
|         - |  8848 | `	}` |
|    452601 |  8849 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8850 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8851 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8852 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8853 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8854 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8855 | `				pTok--;` |
|       ! 0 |  8856 | `			}` |
|       ! 0 |  8857 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8858 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8859 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8860 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8861 | `				return SXERR_ABORT;` |
|         - |  8862 | `			}` |
|       ! 0 |  8863 | `		}else{` |
|         5 |  8864 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8865 | `				goto loop;` |
|         - |  8866 | `			}` |
|         - |  8867 | `		}` |
|       ! 0 |  8868 | `	}` |
|    452597 |  8869 | `	SySetRelease(&aUnionAlts);` |
|    452597 |  8870 | `	return SXRET_OK;` |
|         9 |  8871 | `Synchronize:` |
|         - |  8872 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8873 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  8874 | `		pGen->pIn++;` |
|         3 |  8875 | `	}` |
|        22 |  8876 | `	SySetRelease(&aUnionAlts);` |
|        22 |  8877 | `	return SXERR_CORRUPT;` |
|    226357 |  8878 | `}` |
|         - |  8879 | `/*` |
|         - |  8880 | ` * Compile a class method.` |
|         - |  8881 | ` *` |
|         - |  8882 | ` * Refer to the official documentation for more information` |
|         - |  8883 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  8884 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  8885 | ` * overloading and many more.` |
|         - |  8886 | ` */` |
|   2370314 |  8887 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  8888 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  8889 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  8890 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  8891 | `	int doBody,          /* TRUE to process method body */` |
|         - |  8892 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  8893 | `	)` |
|         5 |  8894 | `{` |
|   2370319 |  8895 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2370319 |  8896 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  8897 | `	ph7_class_method *pMeth;` |
|         - |  8898 | `	sxi32 iFuncFlags;` |
|         - |  8899 | `	SyString *pName;` |
|         - |  8900 | `	SyToken *pEnd;` |
|         - |  8901 | `	sxi32 rc;` |
|         - |  8902 | `	/* Extract visibility level */` |
|   2370319 |  8903 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2370319 |  8904 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2370319 |  8905 | `	iFuncFlags = 0;` |
|   2370319 |  8906 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  8907 | `		/* Invalid method name */` |
|       ! 0 |  8908 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8909 | `		if( rc == SXERR_ABORT ){` |
|         - |  8910 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8911 | `			return SXERR_ABORT;` |
|         - |  8912 | `		}` |
|       ! 0 |  8913 | `		goto Synchronize;` |
|         - |  8914 | `	}` |
|   2370319 |  8915 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8916 | `		/* Return by reference,remember that */` |
|       ! 0 |  8917 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8918 | `		/* Jump the '&' token */` |
|       ! 0 |  8919 | `		pGen->pIn++;` |
|       ! 0 |  8920 | `	}` |
|   2370319 |  8921 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8922 | `		/* Invalid method name */` |
|       ! 0 |  8923 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8924 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8925 | `			return SXERR_ABORT;` |
|         - |  8926 | `		}` |
|       ! 0 |  8927 | `		goto Synchronize;` |
|         - |  8928 | `	}` |
|         - |  8929 | `	/* Peek method name */` |
|   2370319 |  8930 | `	pName = &pGen->pIn->sData;` |
|   2370319 |  8931 | `	nLine = pGen->pIn->nLine;` |
|         - |  8932 | `	/* Jump the method name */` |
|   2370319 |  8933 | `	pGen->pIn++;` |
|   2370319 |  8934 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8935 | `		/* Abstract method */` |
|    136723 |  8936 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  8937 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8938 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  8939 | `				&pClass->sName,pName);` |
|       ! 0 |  8940 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8941 | `				return SXERR_ABORT;` |
|         - |  8942 | `			}` |
|       ! 0 |  8943 | `		}` |
|         - |  8944 | `		/* Assemble method signature only */` |
|    136723 |  8945 | `		doBody = FALSE;` |
|     68359 |  8946 | `	}` |
|   2370319 |  8947 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8948 | `		/* Syntax error */` |
|       ! 0 |  8949 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  8950 | `		if( rc == SXERR_ABORT ){` |
|         - |  8951 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8952 | `			return SXERR_ABORT;` |
|         - |  8953 | `		}` |
|       ! 0 |  8954 | `		goto Synchronize;` |
|         - |  8955 | `	}` |
|         - |  8956 | `	/* Allocate a new class_method instance */` |
|   2370319 |  8957 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2370319 |  8958 | `	if( pMeth == 0 ){` |
|       ! 0 |  8959 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8960 | `		return SXERR_ABORT;` |
|         - |  8961 | `	}` |
|   2370319 |  8962 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2370319 |  8963 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2370319 |  8964 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8965 | `		return SXERR_ABORT;` |
|         - |  8966 | `	}` |
|         - |  8967 | `	/* Jump the left parenthesis '(' */` |
|   2370319 |  8968 | `	pGen->pIn++;` |
|   2370319 |  8969 | `	pEnd = 0; /* cc warning */` |
|         - |  8970 | `	/* Delimit the method signature */` |
|   2370319 |  8971 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2370319 |  8972 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  8973 | `		/* Syntax error */` |
|         3 |  8974 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  8975 | `		if( rc == SXERR_ABORT ){` |
|         - |  8976 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8977 | `			return SXERR_ABORT;` |
|         - |  8978 | `		}` |
|         3 |  8979 | `		goto Synchronize;` |
|         - |  8980 | `	}` |
|         - |  8981 | `	{` |
|   2370317 |  8982 | `		int bIsCtor = 0;` |
|   2370317 |  8983 | `		int bAbstractCtor = 0;` |
|   2370312 |  8984 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1384605 |  8985 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2288599 |  8986 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    163441 |  8987 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  8988 | `				bAbstractCtor = 1;` |
|         2 |  8989 | `			}else{` |
|    163439 |  8990 | `				bIsCtor = 1;` |
|         - |  8991 | `			}` |
|     81718 |  8992 | `		}` |
|   2370317 |  8993 | `		if( pGen->pIn < pEnd ){` |
|         - |  8994 | `			/* Collect method arguments */` |
|    850987 |  8995 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    850987 |  8996 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8997 | `				return SXERR_ABORT;` |
|         - |  8998 | `			}` |
|    425491 |  8999 | `		}` |
|         - |  9000 | `	}` |
|         - |  9001 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2370317 |  9002 | `	pGen->pIn = &pEnd[1];` |
|         - |  9003 | `	{` |
|   2370317 |  9004 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2370317 |  9005 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  9006 | `			return SXERR_ABORT;` |
|   2370317 |  9007 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  9008 | `			goto Synchronize;` |
|         - |  9009 | `		}` |
|         - |  9010 | `	}` |
|         - |  9011 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  9012 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  9013 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  9014 | `	{` |
|   2370317 |  9015 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  9016 | `		sxu32 i;` |
|   3646629 |  9017 | `		for( i = 0; i < nArg; i++ ){` |
|   1276327 |  9018 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  9019 | `			ph7_class_attr *pAttr;` |
|   1276327 |  9020 | `			sxi32 iAttrFlags = 0;` |
|         - |  9021 | `			int bArgTyped;` |
|   1276327 |  9022 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1276243 |  9023 | `				continue;` |
|         - |  9024 | `			}` |
|         - |  9025 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  9026 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  9027 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  9028 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  9029 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  9030 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  9031 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9032 | `					"Cannot declare variadic promoted property");` |
|         3 |  9033 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9034 | `					return SXERR_ABORT;` |
|         - |  9035 | `				}` |
|         3 |  9036 | `				goto Synchronize;` |
|         - |  9037 | `			}` |
|         - |  9038 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  9039 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  9040 | `			 * appear as an alternative of a union type. */` |
|        87 |  9041 | `			if( bArgTyped ){` |
|       122 |  9042 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  9043 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  9044 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  9045 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  9046 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9047 | `					return SXERR_ABORT;` |
|        83 |  9048 | `				}else if( rc != SXRET_OK ){` |
|         6 |  9049 | `					goto Synchronize;` |
|         - |  9050 | `				}` |
|        37 |  9051 | `			}` |
|         - |  9052 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  9053 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  9054 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9055 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  9056 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9057 | `					return SXERR_ABORT;` |
|         - |  9058 | `				}` |
|         3 |  9059 | `				goto Synchronize;` |
|         - |  9060 | `			}` |
|        81 |  9061 | `			if( bArgTyped ){` |
|        77 |  9062 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  9063 | `			}` |
|        81 |  9064 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  9065 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  9066 | `			}` |
|        81 |  9067 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  9068 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  9069 | `			}` |
|        81 |  9070 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  9071 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  9072 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  9073 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  9074 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9075 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  9076 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9077 | `						return SXERR_ABORT;` |
|         - |  9078 | `					}` |
|         3 |  9079 | `					goto Synchronize;` |
|         - |  9080 | `				}` |
|        24 |  9081 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  9082 | `			}` |
|        79 |  9083 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  9084 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  9085 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  9086 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9087 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  9088 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  9089 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9090 | `						return SXERR_ABORT;` |
|         - |  9091 | `					}` |
|       ! 0 |  9092 | `					goto Synchronize;` |
|         - |  9093 | `				}` |
|         5 |  9094 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  9095 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  9096 | `			}` |
|        79 |  9097 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  9098 | `			if( pAttr == 0 ){` |
|       ! 0 |  9099 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9100 | `				return SXERR_ABORT;` |
|         - |  9101 | `			}` |
|        79 |  9102 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  9103 | `				pAttr->nType = pArg->nType;` |
|        77 |  9104 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  9105 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  9106 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9107 | `					sxu32 k;` |
|        20 |  9108 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9109 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9110 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9111 | `					}` |
|         3 |  9112 | `				}` |
|        36 |  9113 | `			}` |
|        79 |  9114 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  9115 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9116 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9117 | `				return SXERR_ABORT;` |
|         - |  9118 | `			}` |
|        42 |  9119 | `		}` |
|         - |  9120 | `	}` |
|   2370307 |  9121 | `	if( doBody ){` |
|         - |  9122 | `		/* Compile method body */` |
|   2233589 |  9123 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2233589 |  9124 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9125 | `			return SXERR_ABORT;` |
|         - |  9126 | `		}` |
|         - |  9127 | `		/* The cursor sits just past the body's closing brace */` |
|   2233589 |  9128 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1116797 |  9129 | `	}else{` |
|         - |  9130 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    136723 |  9131 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    136723 |  9132 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     68359 |  9133 | `		}` |
|         - |  9134 | `		/* Only method signature is allowed */` |
|    136723 |  9135 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9136 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9137 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9138 | `				if( rc == SXERR_ABORT ){` |
|         - |  9139 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9140 | `					return SXERR_ABORT;` |
|         - |  9141 | `				}` |
|       ! 0 |  9142 | `				return SXERR_CORRUPT;` |
|         - |  9143 | `			}` |
|         - |  9144 | `	}` |
|         - |  9145 | `	/* All done,install the method */` |
|   2370307 |  9146 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2370307 |  9147 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9148 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9149 | `		return SXERR_ABORT;` |
|         - |  9150 | `	}` |
|   2370307 |  9151 | `	return SXRET_OK;` |
|         6 |  9152 | `Synchronize:` |
|         - |  9153 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9154 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9155 | `		pGen->pIn++;` |
|         4 |  9156 | `	}` |
|        16 |  9157 | `	return SXERR_CORRUPT;` |
|   1185162 |  9158 | `}` |
|         - |  9159 | `/*` |
|         - |  9160 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9161 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9162 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9163 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9164 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9165 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9166 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9167 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9168 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9169 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9170 | `` * implicit `$value` formal.`` |
|         - |  9171 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9172 | ` */` |
|         - |  9173 | `/*` |
|         - |  9174 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9175 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9176 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9177 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9178 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9179 | ` */` |
|        94 |  9180 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9181 | `{` |
|         - |  9182 | `	SyToken *p;` |
|       345 |  9183 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9184 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9185 | `			continue;` |
|         - |  9186 | `		}` |
|         - |  9187 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9188 | `		if( p + 3 < pEnd` |
|        80 |  9189 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9190 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9191 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9192 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9193 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9194 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9195 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9196 | `			return 1;` |
|         - |  9197 | `		}` |
|         - |  9198 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9199 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9200 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9201 | `		if( p > pStart` |
|        26 |  9202 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9203 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9204 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9205 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9206 | `			return 1;` |
|         - |  9207 | `		}` |
|        15 |  9208 | `	}` |
|        43 |  9209 | `	return 0;` |
|        48 |  9210 | `}` |
|         - |  9211 | `/*` |
|         - |  9212 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9213 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9214 | ` */` |
|       990 |  9215 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9216 | `{` |
|      1167 |  9217 | `	return p + 6 < pEnd` |
|       671 |  9218 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9219 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9220 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9221 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9222 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9223 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9224 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9225 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9226 | `	 && p[5].sData.nByte == 3` |
|         8 |  9227 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9228 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9229 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9230 | `}` |
|         - |  9231 | `/*` |
|         - |  9232 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9233 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9234 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9235 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9236 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9237 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9238 | ` * or SXERR_MEM.` |
|         - |  9239 | ` */` |
|         4 |  9240 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9241 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9242 | `{` |
|         5 |  9243 | `	SyToken *p = pStart;` |
|        35 |  9244 | `	while( p < pEnd ){` |
|        31 |  9245 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9246 | `			SyToken sTok;` |
|         - |  9247 | `			char zName[384];` |
|         - |  9248 | `			sxu32 nName;` |
|         - |  9249 | `			char *zDup;` |
|         - |  9250 | ``			/* `parent` `::` */`` |
|         5 |  9251 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9252 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9253 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9254 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9255 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9256 | `			if( zDup == 0 ){` |
|       ! 0 |  9257 | `				return SXERR_MEM;` |
|         - |  9258 | `			}` |
|         5 |  9259 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9260 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9261 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9262 | `			sTok.pUserData = 0;` |
|         5 |  9263 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9264 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9265 | `			continue;` |
|         - |  9266 | `		}` |
|        27 |  9267 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9268 | `		p++;` |
|         1 |  9269 | `	}` |
|         5 |  9270 | `	return SXRET_OK;` |
|         3 |  9271 | `}` |
|        94 |  9272 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9273 | `{` |
|        95 |  9274 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9275 | `	sxi32 rc;` |
|        95 |  9276 | `	int bRefsSelf = 0;` |
|        95 |  9277 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9278 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9279 | `		char zHook[384];` |
|         - |  9280 | `		SyString sHookName;` |
|         - |  9281 | `		ph7_class_method *pMeth;` |
|         - |  9282 | `		int bGet;` |
|       159 |  9283 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9284 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9285 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9286 | `			continue;` |
|         - |  9287 | `		}` |
|       145 |  9288 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9289 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9290 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9291 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9292 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9293 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9294 | `				return SXERR_ABORT;` |
|         - |  9295 | `			}` |
|       ! 0 |  9296 | `			return SXERR_CORRUPT;` |
|         - |  9297 | `		}` |
|       145 |  9298 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9299 | `			goto HookSyntax;` |
|         - |  9300 | `		}` |
|       144 |  9301 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9302 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9303 | `			bGet = 1;` |
|       106 |  9304 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9305 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9306 | `			bGet = 0;` |
|        34 |  9307 | `		}else{` |
|       ! 0 |  9308 | `			goto HookSyntax;` |
|         - |  9309 | `		}` |
|       145 |  9310 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9311 | `		sHookName.zString = zHook;` |
|       217 |  9312 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9313 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9314 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9315 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9316 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9317 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9318 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9319 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9320 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9321 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9322 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9323 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9324 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9325 | `					return SXERR_ABORT;` |
|         - |  9326 | `				}` |
|       ! 0 |  9327 | `				return SXERR_CORRUPT;` |
|         - |  9328 | `			}` |
|        15 |  9329 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9330 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9331 | `			if( pMeth == 0 ){` |
|       ! 0 |  9332 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9333 | `				return SXERR_ABORT;` |
|         - |  9334 | `			}` |
|        15 |  9335 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9336 | `			if( !bGet ){` |
|         - |  9337 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9338 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9339 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9340 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9341 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9342 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9343 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9344 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9345 | `				if( zVName == 0 ){` |
|       ! 0 |  9346 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9347 | `					return SXERR_ABORT;` |
|         - |  9348 | `				}` |
|         7 |  9349 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9350 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9351 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9352 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9353 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9354 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9355 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9356 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9357 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9358 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9359 | `				}` |
|         7 |  9360 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9361 | `			}` |
|        15 |  9362 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9363 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9364 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9365 | `				return SXERR_ABORT;` |
|         - |  9366 | `			}` |
|        15 |  9367 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9368 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9369 | `		}` |
|       130 |  9370 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9371 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9372 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9373 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9374 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9375 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9376 | `				return SXERR_ABORT;` |
|         - |  9377 | `			}` |
|       ! 0 |  9378 | `			return SXERR_CORRUPT;` |
|         - |  9379 | `		}` |
|       131 |  9380 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9381 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9382 | `		if( pMeth == 0 ){` |
|       ! 0 |  9383 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9384 | `			return SXERR_ABORT;` |
|         - |  9385 | `		}` |
|       131 |  9386 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9387 | `		if( !bGet ){` |
|         - |  9388 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9389 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9390 | `				SyToken *pRp = 0;` |
|        17 |  9391 | `				pGen->pIn++;` |
|        17 |  9392 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9393 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9394 | `					goto HookSyntax;` |
|         - |  9395 | `				}` |
|        17 |  9396 | `				if( pGen->pIn < pRp ){` |
|        17 |  9397 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9398 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9399 | `						return SXERR_ABORT;` |
|         - |  9400 | `					}` |
|         8 |  9401 | `				}` |
|        17 |  9402 | `				pGen->pIn = &pRp[1];` |
|         8 |  9403 | `			}` |
|        61 |  9404 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9405 | `				/* Implicit $value formal */` |
|         - |  9406 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9407 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9408 | `				if( zVName == 0 ){` |
|       ! 0 |  9409 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9410 | `					return SXERR_ABORT;` |
|         - |  9411 | `				}` |
|        45 |  9412 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9413 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9414 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9415 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9416 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9417 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9418 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9419 | `			}` |
|        30 |  9420 | `		}` |
|       165 |  9421 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9422 | `			/* Block body */` |
|        69 |  9423 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9424 | `			SyToken *pCloser = 0;` |
|        69 |  9425 | `			int bParentCall = 0;` |
|        69 |  9426 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9427 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9428 | `				SyToken *pScan;` |
|       753 |  9429 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9430 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9431 | `						bParentCall = 1;` |
|         3 |  9432 | `						break;` |
|         - |  9433 | `					}` |
|       343 |  9434 | `				}` |
|        34 |  9435 | `			}` |
|        69 |  9436 | `			if( bParentCall ){` |
|         - |  9437 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9438 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9439 | `				 * hook method), then continue past the original body. */` |
|         - |  9440 | `				SySet sBody;` |
|         3 |  9441 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9442 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9443 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9444 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9445 | `					SySetRelease(&sBody);` |
|       ! 0 |  9446 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9447 | `					return SXERR_ABORT;` |
|         - |  9448 | `				}` |
|         3 |  9449 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9450 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9451 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9452 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9453 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9454 | `				SySetRelease(&sBody);` |
|         3 |  9455 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9456 | `					return SXERR_ABORT;` |
|         - |  9457 | `				}` |
|         3 |  9458 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9459 | `			}else{` |
|        67 |  9460 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9461 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9462 | `					return SXERR_ABORT;` |
|         - |  9463 | `				}` |
|        67 |  9464 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9465 | `			}` |
|        69 |  9466 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9467 | `				bRefsSelf = 1;` |
|         9 |  9468 | `			}` |
|       128 |  9469 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9470 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9471 | `			GenBlock *pBlock;` |
|         - |  9472 | `			SySet *pInstrContainer;` |
|         - |  9473 | `			SyToken *pBodyStart;` |
|         - |  9474 | `			SyToken *pExprEnd;` |
|        63 |  9475 | `			SyToken *pSavedEnd = 0;` |
|         - |  9476 | `			SySet sBody;` |
|        63 |  9477 | `			int bParentCall = 0;` |
|        63 |  9478 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9479 | `			pBodyStart = pGen->pIn;` |
|         - |  9480 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9481 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9482 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9483 | `			 * method on a token copy. */` |
|         - |  9484 | `			{` |
|        63 |  9485 | `				sxi32 iNest = 0;` |
|        63 |  9486 | `				pExprEnd = pBodyStart;` |
|       355 |  9487 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9488 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9489 | `						iNest++;` |
|       351 |  9490 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9491 | `						if( iNest <= 0 ){` |
|       ! 0 |  9492 | `							break;` |
|         - |  9493 | `						}` |
|         9 |  9494 | `						iNest--;` |
|       343 |  9495 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9496 | `						break;` |
|         - |  9497 | `					}` |
|       293 |  9498 | `					pExprEnd++;` |
|         1 |  9499 | `				}` |
|         - |  9500 | `			}` |
|         - |  9501 | `			{` |
|         - |  9502 | `				SyToken *pScan;` |
|       335 |  9503 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9504 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9505 | `						bParentCall = 1;` |
|         3 |  9506 | `						break;` |
|         - |  9507 | `					}` |
|       137 |  9508 | `				}` |
|         - |  9509 | `			}` |
|        63 |  9510 | `			if( bParentCall ){` |
|         3 |  9511 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9512 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9513 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9514 | `					SySetRelease(&sBody);` |
|       ! 0 |  9515 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9516 | `					return SXERR_ABORT;` |
|         - |  9517 | `				}` |
|         3 |  9518 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9519 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9520 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9521 | `			}` |
|        94 |  9522 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9523 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9524 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9525 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9526 | `				return SXERR_ABORT;` |
|         - |  9527 | `			}` |
|        63 |  9528 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9529 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9530 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9531 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9532 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9533 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9534 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9535 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9536 | `			if( bParentCall ){` |
|         3 |  9537 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9538 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9539 | `				SySetRelease(&sBody);` |
|         1 |  9540 | `			}` |
|        63 |  9541 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9542 | `				return SXERR_ABORT;` |
|         - |  9543 | `			}` |
|        63 |  9544 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9545 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9546 | `				bRefsSelf = 1;` |
|        18 |  9547 | `			}` |
|        63 |  9548 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9549 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9550 | `			}` |
|        63 |  9551 | `			if( !bGet ){` |
|         - |  9552 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9553 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9554 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9555 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9556 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9557 | `				bRefsSelf = 1;` |
|         1 |  9558 | `			}` |
|        32 |  9559 | `		}else{` |
|       ! 0 |  9560 | `			goto HookSyntax;` |
|         - |  9561 | `		}` |
|       131 |  9562 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9563 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9564 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9565 | `			return SXERR_ABORT;` |
|         - |  9566 | `		}` |
|       131 |  9567 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9568 | `	}` |
|        95 |  9569 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9570 | `		goto HookSyntax;` |
|         - |  9571 | `	}` |
|        95 |  9572 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9573 | `	if( !bRefsSelf ){` |
|         - |  9574 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9575 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9576 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9577 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9578 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9579 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9580 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9581 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9582 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9583 | `				return SXERR_ABORT;` |
|         - |  9584 | `			}` |
|       ! 0 |  9585 | `			return SXERR_CORRUPT;` |
|         - |  9586 | `		}` |
|        20 |  9587 | `	}` |
|        95 |  9588 | `	return SXRET_OK;` |
|       ! 0 |  9589 | `HookSyntax:` |
|       ! 0 |  9590 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9591 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9592 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9593 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9594 | `		return SXERR_ABORT;` |
|         - |  9595 | `	}` |
|       ! 0 |  9596 | `	return SXERR_CORRUPT;` |
|        48 |  9597 | `}` |
|         - |  9598 | `/*` |
|         - |  9599 | ` * Compile an object interface.` |
|         - |  9600 | ` *  According to the PHP language reference manual` |
|         - |  9601 | ` *   Object Interfaces:` |
|         - |  9602 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9603 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9604 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9605 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9606 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9607 | ` */` |
|     68432 |  9608 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9609 | `{` |
|     68437 |  9610 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9611 | `	ph7_class *pClass,*pBase;` |
|         - |  9612 | `	SyToken *pEnd,*pTmp;` |
|         - |  9613 | `	SyString *pName;` |
|         - |  9614 | `	sxi32 nKwrd;` |
|         - |  9615 | `	sxi32 rc;` |
|         - |  9616 | `	/* Jump the 'interface' keyword */` |
|     68437 |  9617 | `	pGen->pIn++;` |
|         - |  9618 | `	/* Extract interface name */` |
|     68437 |  9619 | `	pName = &pGen->pIn->sData;` |
|         - |  9620 | `	/* Advance the stream cursor */` |
|     68437 |  9621 | `	pGen->pIn++;` |
|         - |  9622 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9623 | `		SyBlob sFQN;` |
|         - |  9624 | `		SyString sFQNStr;` |
|     68437 |  9625 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     68437 |  9626 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     68437 |  9627 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     68437 |  9628 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     68437 |  9629 | `		SyBlobRelease(&sFQN);` |
|         - |  9630 | `	}` |
|     68437 |  9631 | `	if( pClass == 0 ){` |
|       ! 0 |  9632 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9633 | `		return SXERR_ABORT;` |
|         - |  9634 | `	}` |
|     68437 |  9635 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     68437 |  9636 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9637 | `		return SXERR_ABORT;` |
|         - |  9638 | `	}` |
|         - |  9639 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     68437 |  9640 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9641 | `	/* Assume no base class is given */` |
|     68437 |  9642 | `	pBase = 0;` |
|     68437 |  9643 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26587 |  9644 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26587 |  9645 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9646 | `			SyBlob sResolved;` |
|         - |  9647 | `			SyString sBaseName;` |
|         - |  9648 | `			sxu32 nRefLine;` |
|         - |  9649 | `			/* Extract base interface */` |
|     26587 |  9650 | `			pGen->pIn++;` |
|     26587 |  9651 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26587 |  9652 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26587 |  9653 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9654 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9655 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9656 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9657 | `					pName);` |
|       ! 0 |  9658 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9659 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9660 | `					return SXERR_ABORT;` |
|         - |  9661 | `				}` |
|       ! 0 |  9662 | `				return SXRET_OK;` |
|         - |  9663 | `			}` |
|     39878 |  9664 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     26582 |  9665 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26587 |  9666 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9667 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9668 | `			/* Only interfaces is allowed */` |
|     26587 |  9669 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9670 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9671 | `			}` |
|     26587 |  9672 | `			if( pBase == 0 ){` |
|       ! 0 |  9673 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9674 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9675 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9676 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9677 | `					return SXERR_ABORT;` |
|         - |  9678 | `				}` |
|       ! 0 |  9679 | `			}` |
|     26587 |  9680 | `			SyBlobRelease(&sResolved);` |
|     13291 |  9681 | `		}` |
|     13291 |  9682 | `	}` |
|     68437 |  9683 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9684 | `		/* Syntax error */` |
|       ! 0 |  9685 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9686 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9687 | `		if( rc == SXERR_ABORT ){` |
|         - |  9688 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9689 | `			return SXERR_ABORT;` |
|         - |  9690 | `		}` |
|       ! 0 |  9691 | `		return SXRET_OK;` |
|         - |  9692 | `	}` |
|     68437 |  9693 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     68437 |  9694 | `	pEnd = 0; /* cc warning */` |
|         - |  9695 | `	/* Delimit the interface body */` |
|     68437 |  9696 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     68437 |  9697 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9698 | `		/* Syntax error */` |
|       ! 0 |  9699 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9700 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9701 | `		if( rc == SXERR_ABORT ){` |
|         - |  9702 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9703 | `			return SXERR_ABORT;` |
|         - |  9704 | `		}` |
|       ! 0 |  9705 | `		return SXRET_OK;` |
|         - |  9706 | `	}` |
|         - |  9707 | `	/* The delimiter token is the interface body's closing brace */` |
|     68437 |  9708 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9709 | `	/* Swap token stream */` |
|     68437 |  9710 | `	pTmp = pGen->pEnd;` |
|     68437 |  9711 | `	pGen->pEnd = pEnd;` |
|         - |  9712 | `	/* Start the parse process` |
|         - |  9713 | `	 * Note (According to the PHP reference manual):` |
|         - |  9714 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9715 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9716 | `	 */` |
|    125349 |  9717 | `	for(;;){` |
|         - |  9718 | `		/* Jump leading/trailing semi-colons */` |
|    432973 |  9719 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    182271 |  9720 | `			pGen->pIn++;` |
|         5 |  9721 | `		}` |
|    250707 |  9722 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9723 | `			/* End of interface body */` |
|     68433 |  9724 | `			break;` |
|         - |  9725 | `		}` |
|         - |  9726 | `		/* Bind a directly-preceding docblock to this member */` |
|    182279 |  9727 | `		GenStateSetPendingDoc(&(*pGen));` |
|    182279 |  9728 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9729 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9730 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9731 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9732 | `			if( rc == SXERR_ABORT ){` |
|         - |  9733 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9734 | `				return SXERR_ABORT;` |
|         - |  9735 | `			}` |
|       ! 0 |  9736 | `			goto done;` |
|         - |  9737 | `		}` |
|         - |  9738 | `		/* Extract the current keyword */` |
|    182279 |  9739 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    182279 |  9740 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9741 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9742 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9743 | `			const char *zKind = "member";` |
|         3 |  9744 | `			SyString *pMemberName = 0;` |
|         3 |  9745 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9746 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9747 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9748 | `					zKind = "constant";` |
|         3 |  9749 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9750 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9751 | `					}` |
|         1 |  9752 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9753 | `					zKind = "method";` |
|       ! 0 |  9754 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9755 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9756 | `					}` |
|       ! 0 |  9757 | `				}` |
|         1 |  9758 | `			}` |
|         3 |  9759 | `			if( pMemberName ){` |
|         4 |  9760 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9761 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9762 | `			}else{` |
|       ! 0 |  9763 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9764 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9765 | `			}` |
|         3 |  9766 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9767 | `				return SXERR_ABORT;` |
|         - |  9768 | `			}` |
|         3 |  9769 | `			goto done;` |
|         - |  9770 | `		}` |
|    182277 |  9771 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9772 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9773 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9774 | `			if( rc == SXERR_ABORT ){` |
|         - |  9775 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9776 | `				return SXERR_ABORT;` |
|         - |  9777 | `			}` |
|       ! 0 |  9778 | `			goto done;` |
|         - |  9779 | `		}` |
|    182277 |  9780 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9781 | `			/* Advance the stream cursor */` |
|    129115 |  9782 | `			pGen->pIn++;` |
|    129110 |  9783 | `			if( pGen->pIn < pGen->pEnd` |
|    129115 |  9784 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    129110 |  9785 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9786 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9787 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9788 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9789 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9790 | `				 * hooked properties" error). */` |
|       ! 0 |  9791 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9792 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9793 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9794 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9795 | `						return SXERR_ABORT;` |
|         - |  9796 | `					}` |
|       ! 0 |  9797 | `					goto done;` |
|         - |  9798 | `				}` |
|       ! 0 |  9799 | `				continue;` |
|         - |  9800 | `			}` |
|    129115 |  9801 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9802 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9803 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9804 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9805 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9806 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9807 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9808 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9809 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9810 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9811 | `							return SXERR_ABORT;` |
|         - |  9812 | `						}` |
|       ! 0 |  9813 | `						goto done;` |
|         - |  9814 | `					}` |
|       ! 0 |  9815 | `					continue;` |
|         - |  9816 | `				}` |
|       ! 0 |  9817 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9818 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9819 | `				if( rc == SXERR_ABORT ){` |
|         - |  9820 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9821 | `					return SXERR_ABORT;` |
|         - |  9822 | `				}` |
|       ! 0 |  9823 | `				goto done;` |
|         - |  9824 | `			}` |
|    129115 |  9825 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    129115 |  9826 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9827 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9828 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9829 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9830 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9831 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9832 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9833 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9834 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9835 | `							return SXERR_ABORT;` |
|         - |  9836 | `						}` |
|       ! 0 |  9837 | `						goto done;` |
|         - |  9838 | `					}` |
|         5 |  9839 | `					continue;` |
|         - |  9840 | `				}` |
|       ! 0 |  9841 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9842 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9843 | `				if( rc == SXERR_ABORT ){` |
|         - |  9844 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9845 | `					return SXERR_ABORT;` |
|         - |  9846 | `				}` |
|       ! 0 |  9847 | `				goto done;` |
|         - |  9848 | `			}` |
|     64553 |  9849 | `		}` |
|    182273 |  9850 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9851 | `			/* Parse constant */` |
|     53163 |  9852 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53163 |  9853 | `			if( rc != SXRET_OK ){` |
|         3 |  9854 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9855 | `					return SXERR_ABORT;` |
|         - |  9856 | `				}` |
|         3 |  9857 | `				goto done;` |
|         - |  9858 | `			}` |
|     26583 |  9859 | `		}else{` |
|    129115 |  9860 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    129115 |  9861 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9862 | `				/* Static method,record that */` |
|     11393 |  9863 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9864 | `				/* Advance the stream cursor */` |
|     11393 |  9865 | `				pGen->pIn++;` |
|     11388 |  9866 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11393 |  9867 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9868 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9869 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9870 | `						if( rc == SXERR_ABORT ){` |
|         - |  9871 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9872 | `							return SXERR_ABORT;` |
|         - |  9873 | `						}` |
|       ! 0 |  9874 | `						goto done;` |
|         - |  9875 | `				}` |
|      5694 |  9876 | `			}` |
|         - |  9877 | `			/* Process method signature (no body for interface methods) */` |
|    129115 |  9878 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    129115 |  9879 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9880 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9881 | `					return SXERR_ABORT;` |
|         - |  9882 | `				}` |
|       ! 0 |  9883 | `				goto done;` |
|         - |  9884 | `			}` |
|         - |  9885 | `		}` |
|         5 |  9886 | `	}` |
|         - |  9887 | `	/* Install the interface */` |
|     68433 |  9888 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     68433 |  9889 | `	if( rc == SXRET_OK && pBase ){` |
|         - |  9890 | `		/* Inherit from the base interface */` |
|     26587 |  9891 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13291 |  9892 | `	}` |
|     68433 |  9893 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9894 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9895 | `		return SXERR_ABORT;` |
|         - |  9896 | `	}` |
|     34214 |  9897 | `done:` |
|         - |  9898 | `	/* Point beyond the interface body */` |
|     68437 |  9899 | `	pGen->pIn  = &pEnd[1];` |
|     68437 |  9900 | `	pGen->pEnd = pTmp;` |
|     68437 |  9901 | `	return PH7_OK;` |
|     34221 |  9902 | `}` |
|         - |  9903 | `/*` |
|         - |  9904 | ` * Compile a user-defined class.` |
|         - |  9905 | ` * According to the PHP language reference manual` |
|         - |  9906 | ` *  class` |
|         - |  9907 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - |  9908 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - |  9909 | ` *  of the properties and methods belonging to the class.` |
|         - |  9910 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - |  9911 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - |  9912 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - |  9913 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  9914 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - |  9915 | ` *  (called "methods").` |
|         - |  9916 | ` */` |
|         - |  9917 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - |  9918 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - |  9919 | `struct TraitUseEntry {` |
|         - |  9920 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - |  9921 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - |  9922 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - |  9923 | `};` |
|         - |  9924 | `/*` |
|         - |  9925 | ` * Validate that methods implementing interface contracts have compatible` |
|         - |  9926 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - |  9927 | ` */` |
|    350960 |  9928 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9929 | `{` |
|         - |  9930 | `	ph7_class **apIface;` |
|         - |  9931 | `	sxu32 nIface,i;` |
|         - |  9932 | `	sxi32 rc;` |
|    350965 |  9933 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 |  9934 | `		return SXRET_OK;` |
|         - |  9935 | `	}` |
|    350965 |  9936 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    350965 |  9937 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    704325 |  9938 | `	for(i = 0; i < nIface; i++){` |
|    353365 |  9939 | `		ph7_class *pIface = apIface[i];` |
|         - |  9940 | `		SyHashEntry *pEntry;` |
|    353365 |  9941 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1018245 |  9942 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    664885 |  9943 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - |  9944 | `			ph7_class_method *pImplMeth;` |
|    664885 |  9945 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - |  9946 | `			/* Find the implementing method in the class */` |
|    664885 |  9947 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    664885 |  9948 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 |  9949 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - |  9950 | `			}` |
|         - |  9951 | `			/* Check visibility: interface methods must be implemented as public */` |
|    664867 |  9952 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 |  9953 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9954 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 |  9955 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 |  9956 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9957 | `					return SXERR_ABORT;` |
|         - |  9958 | `				}` |
|         1 |  9959 | `			}` |
|         - |  9960 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - |  9961 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - |  9962 | `			 */` |
|         - |  9963 | `			{` |
|    664867 |  9964 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    664867 |  9965 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    664867 |  9966 | `				int sigError = 0;` |
|    664867 |  9967 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 |  9968 | `					sigError = 1;` |
|    664866 |  9969 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - |  9970 | `					/* Extra parameters must all have default values */` |
|      3805 |  9971 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - |  9972 | `					sxu32 k;` |
|      7603 |  9973 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3805 |  9974 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 |  9975 | `							sigError = 1;` |
|         3 |  9976 | `							break;` |
|         - |  9977 | `						}` |
|      1904 |  9978 | `					}` |
|      1900 |  9979 | `				}` |
|    664867 |  9980 | `				if( sigError ){` |
|         - |  9981 | `					SyBlob sImplSig, sIfaceSig;` |
|         - |  9982 | `					ph7_vm_func_arg *aArgs;` |
|         - |  9983 | `					sxu32 j;` |
|         6 |  9984 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 |  9985 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - |  9986 | `					/* Build implementing method signature */` |
|         6 |  9987 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 |  9988 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 |  9989 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 |  9990 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 |  9991 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9992 | `					}` |
|         - |  9993 | `					/* Build interface method signature */` |
|         6 |  9994 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 |  9995 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 |  9996 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 |  9997 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 |  9998 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9999 | `					}` |
|         8 | 10000 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10001 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 10002 | `						&pClass->sName,pMName,` |
|         4 | 10003 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 10004 | `						&pIface->sName,pMName,` |
|         4 | 10005 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 10006 | `					SyBlobRelease(&sImplSig);` |
|         6 | 10007 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 10008 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10009 | `						return SXERR_ABORT;` |
|         - | 10010 | `					}` |
|         2 | 10011 | `				}` |
|         - | 10012 | `			}` |
|         5 | 10013 | `		}` |
|    176685 | 10014 | `	}` |
|    350965 | 10015 | `	return SXRET_OK;` |
|    175485 | 10016 | `}` |
|         - | 10017 | `/*` |
|         - | 10018 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 10019 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 10020 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 10021 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 10022 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 10023 | ` * means that specific hook is still missing.` |
|         - | 10024 | ` */` |
|        38 | 10025 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 10026 | `{` |
|         - | 10027 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 10028 | `	ph7_class_attr *pProp;` |
|        38 | 10029 | `	if( pMName->nByte <= nPfx` |
|        27 | 10030 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 10031 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 10032 | `		return 0; /* not a hook stub */` |
|         - | 10033 | `	}` |
|         7 | 10034 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 10035 | `	return pProp != 0` |
|         6 | 10036 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 10037 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 10038 | `}` |
|         - | 10039 | `/*` |
|         - | 10040 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 10041 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 10042 | ` */` |
|        16 | 10043 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 10044 | `{` |
|         - | 10045 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 10046 | `	if( pMName->nByte > nPfx` |
|        12 | 10047 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 10048 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 10049 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 10050 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 10051 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 10052 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 10053 | `		return;` |
|         - | 10054 | `	}` |
|        20 | 10055 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 10056 | `}` |
|         - | 10057 | `/*` |
|         - | 10058 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 10059 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 10060 | ` */` |
|    350960 | 10061 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10062 | `{` |
|         - | 10063 | `	ph7_class_method *pMeth;` |
|         - | 10064 | `	SyHashEntry *pEntry;` |
|         - | 10065 | `	sxu32 nAbstract;` |
|         - | 10066 | `	SyBlob sMsg;` |
|         - | 10067 | `	sxi32 rc;` |
|         - | 10068 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    350965 | 10069 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15233 | 10070 | `		return SXRET_OK;` |
|         - | 10071 | `	}` |
|         - | 10072 | `	/* Count abstract methods */` |
|    335737 | 10073 | `	nAbstract = 0;` |
|    335737 | 10074 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   4954991 | 10075 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4451393 | 10076 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4451393 | 10077 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 10078 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 10079 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10080 | `			}` |
|        20 | 10081 | `			nAbstract++;` |
|         8 | 10082 | `		}` |
|         5 | 10083 | `	}` |
|    335737 | 10084 | `	if( nAbstract == 0 ){` |
|    335723 | 10085 | `		return SXRET_OK;` |
|         - | 10086 | `	}` |
|         - | 10087 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 10088 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 10089 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 10090 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 10091 | `		&pClass->sName,nAbstract,` |
|         7 | 10092 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 10093 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 10094 | `	/* Second pass: list methods with origins */` |
|         - | 10095 | `	{` |
|        18 | 10096 | `		sxu32 nListed = 0;` |
|        18 | 10097 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 10098 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 10099 | `			ph7_class *pOrigin = 0;` |
|         - | 10100 | `			SyString *pMName;` |
|        22 | 10101 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 10102 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10103 | `				continue;` |
|         - | 10104 | `			}` |
|        20 | 10105 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10106 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10107 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10108 | `			}` |
|        20 | 10109 | `			if( nListed > 0 ){` |
|         3 | 10110 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10111 | `			}` |
|         - | 10112 | `			/* Find the origin of this abstract method.` |
|         - | 10113 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10114 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10115 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10116 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10117 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10118 | `			 * class's namespace.` |
|         - | 10119 | `			 */` |
|         - | 10120 | `			{` |
|         - | 10121 | `				ph7_class **apIface;` |
|         - | 10122 | `				ph7_class **apTrait;` |
|         - | 10123 | `				ph7_class *pWalk;` |
|         - | 10124 | `				sxu32 i;` |
|         - | 10125 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10126 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10127 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10128 | `				 */` |
|        20 | 10129 | `				if( pClass->pBase ){` |
|        11 | 10130 | `					pWalk = pClass->pBase;` |
|        19 | 10131 | `					while( pWalk ){` |
|         - | 10132 | `						ph7_class_method *pParentMeth;` |
|        13 | 10133 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10134 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10135 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10136 | `							 * in this class's ancestor chain.` |
|         - | 10137 | `							 */` |
|        13 | 10138 | `							int fromIface = 0;` |
|        13 | 10139 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10140 | `							while( pAnc ){` |
|         - | 10141 | `								ph7_class **apPI;` |
|         - | 10142 | `								sxu32 j;` |
|        15 | 10143 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10144 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10145 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10146 | `										fromIface = 1;` |
|        10 | 10147 | `										break;` |
|         - | 10148 | `									}` |
|       ! 0 | 10149 | `								}` |
|        15 | 10150 | `								if( fromIface ) break;` |
|         6 | 10151 | `								pAnc = pAnc->pBase;` |
|         2 | 10152 | `							}` |
|        13 | 10153 | `							if( !fromIface ){` |
|         3 | 10154 | `								pOrigin = pWalk;` |
|         3 | 10155 | `								break;` |
|         - | 10156 | `							}` |
|         4 | 10157 | `						}` |
|        10 | 10158 | `						pWalk = pWalk->pBase;` |
|         2 | 10159 | `					}` |
|         4 | 10160 | `				}` |
|         - | 10161 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10162 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10163 | `				 */` |
|        20 | 10164 | `				if( !pOrigin ){` |
|        18 | 10165 | `					pWalk = pClass;` |
|        40 | 10166 | `					while( pWalk && !pOrigin ){` |
|        26 | 10167 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10168 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10169 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10170 | `							ph7_class *pDeepest = 0;` |
|        28 | 10171 | `							while( pIface ){` |
|        16 | 10172 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10173 | `									pDeepest = pIface;` |
|         6 | 10174 | `								}` |
|        16 | 10175 | `								pIface = pIface->pBase;` |
|         4 | 10176 | `							}` |
|        16 | 10177 | `							if( pDeepest ){` |
|        16 | 10178 | `								pOrigin = pDeepest;` |
|        16 | 10179 | `								break;` |
|         - | 10180 | `							}` |
|       ! 0 | 10181 | `						}` |
|        26 | 10182 | `						pWalk = pWalk->pBase;` |
|         4 | 10183 | `					}` |
|         7 | 10184 | `				}` |
|         - | 10185 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10186 | `				if( !pOrigin ){` |
|         3 | 10187 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10188 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10189 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10190 | `							pOrigin = pClass;` |
|         3 | 10191 | `							break;` |
|         - | 10192 | `						}` |
|       ! 0 | 10193 | `					}` |
|         1 | 10194 | `				}` |
|         - | 10195 | `			}` |
|        20 | 10196 | `			if( pOrigin ){` |
|        20 | 10197 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10198 | `			}else{` |
|         - | 10199 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10200 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10201 | `			}` |
|        20 | 10202 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10203 | `			nListed++;` |
|         4 | 10204 | `		}` |
|         - | 10205 | `	}` |
|        18 | 10206 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10207 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10208 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10209 | `	SyBlobRelease(&sMsg);` |
|        18 | 10210 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10211 | `		return SXERR_ABORT;` |
|         - | 10212 | `	}` |
|        18 | 10213 | `	return SXRET_OK;` |
|    175485 | 10214 | `}` |
|         - | 10215 | `/*` |
|         - | 10216 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10217 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10218 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10219 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10220 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10221 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10222 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10223 | ` */` |
|    396812 | 10224 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10225 | `{` |
|    396817 | 10226 | `	int isAbsolute = 0;` |
|    396817 | 10227 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10228 | `	SyBlob sName;` |
|    396817 | 10229 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4385 | 10230 | `		isAbsolute = 1;` |
|      4385 | 10231 | `		pGen->pIn++;` |
|      2190 | 10232 | `	}` |
|    396817 | 10233 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         8 | 10234 | `		pGen->pIn = pStart;` |
|         8 | 10235 | `		return SXERR_INVALID;` |
|         - | 10236 | `	}` |
|    396811 | 10237 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    396811 | 10238 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    396811 | 10239 | `	pGen->pIn++;` |
|    595230 | 10240 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    198429 | 10241 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10242 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10243 | `		pGen->pIn++;` |
|        16 | 10244 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10245 | `		pGen->pIn++;` |
|         2 | 10246 | `	}` |
|    396811 | 10247 | `	if( isAbsolute ){` |
|      4383 | 10248 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2194 | 10249 | `	}else{` |
|         - | 10250 | `		SyString sRaw;` |
|    392433 | 10251 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    392433 | 10252 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10253 | `	}` |
|    396811 | 10254 | `	SyBlobRelease(&sName);` |
|    396811 | 10255 | `	return SXRET_OK;` |
|    198411 | 10256 | `}` |
|         - | 10257 | `/*` |
|         - | 10258 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10259 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10260 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10261 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10262 | ` * either direction cannot run unbounded.` |
|         - | 10263 | ` */` |
|         - | 10264 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    163428 | 10265 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10266 | `{` |
|         - | 10267 | `	ph7_class **apParent;` |
|         - | 10268 | `	sxu32 n;` |
|    425595 | 10269 | `	while( pInterface ){` |
|    269769 | 10270 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10271 | `			return FALSE;` |
|         - | 10272 | `		}` |
|    303952 | 10273 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     68366 | 10274 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7607 | 10275 | `			return TRUE;` |
|         - | 10276 | `		}` |
|    262167 | 10277 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    262167 | 10278 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10279 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10280 | `				return TRUE;` |
|         - | 10281 | `			}` |
|       ! 0 | 10282 | `		}` |
|    262167 | 10283 | `		pInterface = pInterface->pBase;` |
|    262167 | 10284 | `		iDepth++;` |
|         5 | 10285 | `	}` |
|    155831 | 10286 | `	return FALSE;` |
|     81719 | 10287 | `}` |
|    163428 | 10288 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10289 | `{` |
|    163433 | 10290 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10291 | `}` |
|         - | 10292 | `/*` |
|         - | 10293 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10294 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10295 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10296 | ` */` |
|      7602 | 10297 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10298 | `{` |
|      7611 | 10299 | `	while( pBase ){` |
|        10 | 10300 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10301 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10302 | `			return TRUE;` |
|         - | 10303 | `		}` |
|        10 | 10304 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10305 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10306 | `			return TRUE;` |
|         - | 10307 | `		}` |
|         5 | 10308 | `		pBase = pBase->pBase;` |
|         1 | 10309 | `	}` |
|      7603 | 10310 | `	return FALSE;` |
|      3806 | 10311 | `}` |
|         - | 10312 | `/*` |
|         - | 10313 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10314 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10315 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10316 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10317 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10318 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10319 | ` * pClass->aEnumCases for cases().` |
|         - | 10320 | ` */` |
|      7634 | 10321 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10322 | `{` |
|      7639 | 10323 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10324 | `	SySet *pInstrContainer;` |
|         - | 10325 | `	ph7_class_attr *pCase;` |
|         - | 10326 | `	SyString *pName;` |
|         - | 10327 | `	sxi32 rc;` |
|      7639 | 10328 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7639 | 10329 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10330 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10331 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10332 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10333 | `			return SXERR_ABORT;` |
|         - | 10334 | `		}` |
|       ! 0 | 10335 | `		goto Synchronize;` |
|         - | 10336 | `	}` |
|      7639 | 10337 | `	pName = &pGen->pIn->sData;` |
|         - | 10338 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7639 | 10339 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10340 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10341 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10342 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10343 | `			return SXERR_ABORT;` |
|         - | 10344 | `		}` |
|       ! 0 | 10345 | `		goto Synchronize;` |
|         - | 10346 | `	}` |
|      7639 | 10347 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10348 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7639 | 10349 | `	if( pCase == 0 ){` |
|       ! 0 | 10350 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10351 | `		return SXERR_ABORT;` |
|         - | 10352 | `	}` |
|      7639 | 10353 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7639 | 10354 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10355 | `		return SXERR_ABORT;` |
|         - | 10356 | `	}` |
|      7639 | 10357 | `	pGen->pIn++; /* Jump the case name */` |
|      7639 | 10358 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7625 | 10359 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10360 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10361 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10362 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10363 | `				return SXERR_ABORT;` |
|         - | 10364 | `			}` |
|         6 | 10365 | `			goto Synchronize;` |
|         - | 10366 | `		}` |
|      7621 | 10367 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10368 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10369 | `		 * (same technique as class constants). */` |
|      7621 | 10370 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7621 | 10371 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7621 | 10372 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7621 | 10373 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10374 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10375 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10376 | `		}` |
|      7621 | 10377 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7621 | 10378 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7621 | 10379 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10380 | `			return SXERR_ABORT;` |
|         - | 10381 | `		}` |
|      3813 | 10382 | `	}else{` |
|        17 | 10383 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10384 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10385 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10386 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10387 | `				return SXERR_ABORT;` |
|         - | 10388 | `			}` |
|       ! 0 | 10389 | `			goto Synchronize;` |
|         - | 10390 | `		}` |
|         - | 10391 | `	}` |
|      7635 | 10392 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7635 | 10393 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10394 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10395 | `		return SXERR_ABORT;` |
|         - | 10396 | `	}` |
|      7635 | 10397 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7635 | 10398 | `	return SXRET_OK;` |
|         2 | 10399 | `Synchronize:` |
|         - | 10400 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10401 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10402 | `		pGen->pIn++;` |
|         2 | 10403 | `	}` |
|         6 | 10404 | `	return SXERR_CORRUPT;` |
|      3822 | 10405 | `}` |
|         - | 10406 | `/*` |
|         - | 10407 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10408 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10409 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10410 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10411 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10412 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10413 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10414 | ` */` |
|      3820 | 10415 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10416 | `{` |
|         - | 10417 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10418 | `	const char *zBack;` |
|         - | 10419 | `	SySet sToken;` |
|         - | 10420 | `	char *zSrc;` |
|         - | 10421 | `	sxu32 nSrc,nMax;` |
|      3825 | 10422 | `	sxi32 rc = SXRET_OK;` |
|      3825 | 10423 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3820 | 10424 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3825 | 10425 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3825 | 10426 | `	if( zSrc == 0 ){` |
|       ! 0 | 10427 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10428 | `		return SXERR_ABORT;` |
|         - | 10429 | `	}` |
|      3825 | 10430 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3825 | 10431 | `	if( pClass->nEnumBacking != 0 ){` |
|      5717 | 10432 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10433 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10434 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10435 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1904 | 10436 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1909 | 10437 | `	}else{` |
|        21 | 10438 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10439 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10440 | `	}` |
|      3825 | 10441 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3825 | 10442 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3825 | 10443 | `	pSaveIn = pGen->pIn;` |
|      3825 | 10444 | `	pSaveEnd = pGen->pEnd;` |
|      3825 | 10445 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3825 | 10446 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15261 | 10447 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11441 | 10448 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10449 | `	}` |
|      3825 | 10450 | `	pGen->pIn = pSaveIn;` |
|      3825 | 10451 | `	pGen->pEnd = pSaveEnd;` |
|      3825 | 10452 | `	SySetRelease(&sToken);` |
|      3825 | 10453 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1915 | 10454 | `}` |
|         - | 10455 | `/*` |
|         - | 10456 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10457 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10458 | ` */` |
|         - | 10459 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10460 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10461 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10462 | `};` |
|         - | 10463 | `/*` |
|         - | 10464 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10465 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10466 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10467 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10468 | ` * and before the class is installed.` |
|         - | 10469 | ` */` |
|      3820 | 10470 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10471 | `{` |
|         - | 10472 | `	SyHashEntry *pEntry;` |
|         - | 10473 | `	sxi32 rc;` |
|         - | 10474 | `	sxu32 n;` |
|         - | 10475 | `	/* php: "Enum %s cannot include properties" */` |
|      3825 | 10476 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11459 | 10477 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7641 | 10478 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7641 | 10479 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10480 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10481 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10482 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10483 | `				return SXERR_ABORT;` |
|         - | 10484 | `			}` |
|         3 | 10485 | `			break;` |
|         - | 10486 | `		}` |
|         5 | 10487 | `	}` |
|         - | 10488 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     53485 | 10489 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     74490 | 10490 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     49665 | 10491 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10492 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10493 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10494 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10495 | `				return SXERR_ABORT;` |
|         - | 10496 | `			}` |
|       ! 0 | 10497 | `		}` |
|     24835 | 10498 | `	}` |
|         - | 10499 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10500 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10501 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10502 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10503 | `	{` |
|         - | 10504 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10505 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10506 | `		ph7_class_attr *pAttr;` |
|      3825 | 10507 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10508 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3825 | 10509 | `		if( pAttr == 0 ){` |
|       ! 0 | 10510 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10511 | `			return SXERR_ABORT;` |
|         - | 10512 | `		}` |
|      3825 | 10513 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3825 | 10514 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3825 | 10515 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3825 | 10516 | `		if( pClass->nEnumBacking != 0 ){` |
|      3813 | 10517 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10518 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3813 | 10519 | `			if( pAttr == 0 ){` |
|       ! 0 | 10520 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10521 | `				return SXERR_ABORT;` |
|         - | 10522 | `			}` |
|      3813 | 10523 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3813 | 10524 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10525 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10526 | `			}else{` |
|      3807 | 10527 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10528 | `			}` |
|      3813 | 10529 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1904 | 10530 | `		}` |
|         - | 10531 | `	}` |
|      3825 | 10532 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1915 | 10533 | `}` |
|         - | 10534 | `/*` |
|         - | 10535 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10536 | ` *` |
|         - | 10537 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10538 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10539 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10540 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10541 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10542 | ` * implements, body, install) is shared by both paths.` |
|         - | 10543 | ` */` |
|    351004 | 10544 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10545 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10546 | `{` |
|    351009 | 10547 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10548 | `	ph7_class *pClass,*pBase;` |
|         - | 10549 | `	SyToken *pEnd,*pTmp;` |
|         - | 10550 | `	sxi32 iProtection;` |
|         - | 10551 | `	SySet aInterfaces;` |
|         - | 10552 | `	SySet aUseEntries;` |
|         - | 10553 | `	sxi32 iAttrflags;` |
|         - | 10554 | `	SyString *pName;` |
|         - | 10555 | `	sxi32 nKwrd;` |
|         - | 10556 | `	sxi32 rc;` |
|         - | 10557 | `	/* Jump the 'class' keyword */` |
|    351009 | 10558 | `	pGen->pIn++;` |
|    351009 | 10559 | `	if( pAnonName ){` |
|         - | 10560 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10561 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10562 | `		 * then use the synthesized name. */` |
|        32 | 10563 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10564 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10565 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10566 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10567 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10568 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10569 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10570 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10571 | `		}` |
|        32 | 10572 | `		pName = pAnonName;` |
|        32 | 10573 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10574 | `	}else{` |
|    350981 | 10575 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10576 | `			/* Syntax error */` |
|       ! 0 | 10577 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10578 | `			if( rc == SXERR_ABORT ){` |
|         - | 10579 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10580 | `				return SXERR_ABORT;` |
|         - | 10581 | `			}` |
|         - | 10582 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10583 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10584 | `				pGen->pIn++;` |
|       ! 0 | 10585 | `			}` |
|       ! 0 | 10586 | `			return SXRET_OK;` |
|         - | 10587 | `		}` |
|         - | 10588 | `		/* Extract class name */` |
|    350981 | 10589 | `		pName = &pGen->pIn->sData;` |
|         - | 10590 | `		/* Advance the stream cursor */` |
|    350981 | 10591 | `		pGen->pIn++;` |
|         - | 10592 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10593 | `			SyBlob sFQN;` |
|         - | 10594 | `			SyString sFQNStr;` |
|    350981 | 10595 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    350981 | 10596 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    350981 | 10597 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    350981 | 10598 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    350981 | 10599 | `			SyBlobRelease(&sFQN);` |
|         - | 10600 | `		}` |
|         - | 10601 | `	}` |
|    351009 | 10602 | `	if( pClass == 0 ){` |
|       ! 0 | 10603 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10604 | `		return SXERR_ABORT;` |
|         - | 10605 | `	}` |
|    351004 | 10606 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3829 | 10607 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10608 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3815 | 10609 | `		pGen->pIn++; /* Jump ':' */` |
|      3810 | 10610 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3815 | 10611 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10612 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10613 | `			pGen->pIn++;` |
|      3808 | 10614 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3809 | 10615 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3807 | 10616 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3807 | 10617 | `			pGen->pIn++;` |
|      1906 | 10618 | `		}else{` |
|         3 | 10619 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10620 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10621 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10622 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10623 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10624 | `				return SXERR_ABORT;` |
|         - | 10625 | `			}` |
|         3 | 10626 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10627 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10628 | `			}` |
|         - | 10629 | `		}` |
|      1905 | 10630 | `	}` |
|    351009 | 10631 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    351009 | 10632 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10633 | `		return SXERR_ABORT;` |
|         - | 10634 | `	}` |
|         - | 10635 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    351009 | 10636 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    351009 | 10637 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10638 | `	/* Assume a standalone class */` |
|    351009 | 10639 | `	pBase = 0;` |
|    351009 | 10640 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    285135 | 10641 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    285135 | 10642 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10643 | `			SyBlob sResolved;` |
|         - | 10644 | `			SyString sBaseName;` |
|         - | 10645 | `			sxu32 nRefLine;` |
|    182467 | 10646 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10647 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10648 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10649 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10650 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10651 | `					return SXERR_ABORT;` |
|         - | 10652 | `				}` |
|       ! 0 | 10653 | `			}` |
|    182467 | 10654 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    182467 | 10655 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    182467 | 10656 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    182467 | 10657 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10658 | `				SyBlobRelease(&sResolved);` |
|         4 | 10659 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10660 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10661 | `					pName);` |
|         3 | 10662 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10663 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10664 | `					return SXERR_ABORT;` |
|         - | 10665 | `				}` |
|         3 | 10666 | `				return SXRET_OK;` |
|         - | 10667 | `			}` |
|    273695 | 10668 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    182460 | 10669 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    182465 | 10670 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10671 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10672 | `			/* Interfaces are not allowed */` |
|    182465 | 10673 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10674 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10675 | `			}` |
|    182465 | 10676 | `			if( pBase == 0 ){` |
|       ! 0 | 10677 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10678 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10679 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10680 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10681 | `					return SXERR_ABORT;` |
|         - | 10682 | `				}` |
|       ! 0 | 10683 | `			}else{` |
|    182465 | 10684 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10685 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10686 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10687 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10688 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10689 | `						return SXERR_ABORT;` |
|         - | 10690 | `					}` |
|         3 | 10691 | `					pBase = 0; /* Never inherit from an enum */` |
|    182464 | 10692 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10693 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10694 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10695 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10696 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10697 | `						return SXERR_ABORT;` |
|         - | 10698 | `					}` |
|       ! 0 | 10699 | `				}` |
|         - | 10700 | `			}` |
|    182465 | 10701 | `			SyBlobRelease(&sResolved);` |
|    182465 | 10702 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10703 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10704 | `			}` |
|     91230 | 10705 | `		}` |
|    285133 | 10706 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10707 | `			ph7_class *pInterface;` |
|         - | 10708 | `			/* Interface implementation */` |
|    106481 | 10709 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    110190 | 10710 | `			for(;;){` |
|         - | 10711 | `				SyBlob sResolved;` |
|         - | 10712 | `				SyString sIntName;` |
|         - | 10713 | `				sxu32 nRefLine;` |
|    163433 | 10714 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    163433 | 10715 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    163433 | 10716 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10717 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10718 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10719 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10720 | `						pName);` |
|       ! 0 | 10721 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10722 | `						return SXERR_ABORT;` |
|         - | 10723 | `					}` |
|       ! 0 | 10724 | `					break;` |
|         - | 10725 | `				}` |
|    326861 | 10726 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    163428 | 10727 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    163433 | 10728 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10729 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10730 | `				/* Only interfaces are allowed */` |
|    163433 | 10731 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10732 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10733 | `				}` |
|    163433 | 10734 | `				if( pInterface == 0 ){` |
|       ! 0 | 10735 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10736 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10737 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10738 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10739 | `						return SXERR_ABORT;` |
|         - | 10740 | `					}` |
|       ! 0 | 10741 | `				}else{` |
|         - | 10742 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10743 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10744 | `					 * unless they already extend Exception or Error.` |
|         - | 10745 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10746 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10747 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    163433 | 10748 | `					SyString *pFqn = &pClass->sName;` |
|    163433 | 10749 | `					int bIsExceptionOrError =` |
|     85514 | 10750 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    247044 | 10751 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    161537 | 10752 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3810 | 10753 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    167229 | 10754 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11406 | 10755 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3799 | 10756 | `						!bIsExceptionOrError ){` |
|        12 | 10757 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10758 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10759 | `							&pClass->sName);` |
|         9 | 10760 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10761 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10762 | `							return SXERR_ABORT;` |
|         - | 10763 | `						}` |
|         - | 10764 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10765 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10766 | `					}else{` |
|    163427 | 10767 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10768 | `					}` |
|         - | 10769 | `				}` |
|    163433 | 10770 | `				SyBlobRelease(&sResolved);` |
|    163433 | 10771 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53243 | 10772 | `					break;` |
|         - | 10773 | `				}` |
|     56957 | 10774 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10775 | `			}` |
|     53238 | 10776 | `		}` |
|    142564 | 10777 | `	}` |
|    351007 | 10778 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10779 | `		/* Syntax error */` |
|       ! 0 | 10780 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10781 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10782 | `		if( rc == SXERR_ABORT ){` |
|         - | 10783 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10784 | `			return SXERR_ABORT;` |
|         - | 10785 | `		}` |
|       ! 0 | 10786 | `		return SXRET_OK;` |
|         - | 10787 | `	}` |
|    351007 | 10788 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    351007 | 10789 | `	pEnd = 0; /* cc warning */` |
|         - | 10790 | `	/* Delimit the class body */` |
|    351007 | 10791 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    351007 | 10792 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10793 | `		/* Syntax error */` |
|       ! 0 | 10794 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10795 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10796 | `		if( rc == SXERR_ABORT ){` |
|         - | 10797 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10798 | `			return SXERR_ABORT;` |
|         - | 10799 | `		}` |
|       ! 0 | 10800 | `		return SXRET_OK;` |
|         - | 10801 | `	}` |
|         - | 10802 | `	/* The delimiter token is the class body's closing brace */` |
|    351007 | 10803 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10804 | `	/* Swap token stream */` |
|    351007 | 10805 | `	pTmp = pGen->pEnd;` |
|    351007 | 10806 | `	pGen->pEnd = pEnd;` |
|         - | 10807 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    351007 | 10808 | `	pClass->iFlags \|= iFlags;` |
|         - | 10809 | `	/* Start the parse process */` |
|   1358741 | 10810 | `	for(;;){` |
|         - | 10811 | `		/* Jump leading/trailing semi-colons */` |
|   3873627 | 10812 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    699577 | 10813 | `			pGen->pIn++;` |
|         5 | 10814 | `		}` |
|   3174055 | 10815 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10816 | `			/* End of class body */` |
|    350965 | 10817 | `			break;` |
|         - | 10818 | `		}` |
|         - | 10819 | `		/* Bind a directly-preceding docblock to this member */` |
|   2823095 | 10820 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2823090 | 10821 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1411550 | 10822 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10823 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10824 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10825 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10826 | `			if( rc == SXERR_ABORT ){` |
|         - | 10827 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10828 | `				return SXERR_ABORT;` |
|         - | 10829 | `			}` |
|       ! 0 | 10830 | `			goto done;` |
|         - | 10831 | `		}` |
|         - | 10832 | `		/* Assume public visibility */` |
|   2823095 | 10833 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2823095 | 10834 | `		iAttrflags = 0;` |
|         - | 10835 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10836 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10837 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10838 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2823095 | 10839 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10840 | `			int bMod = 0;` |
|       ! 0 | 10841 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10842 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10843 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10844 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10845 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10846 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10847 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10848 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10849 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10850 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10851 | `			}` |
|       ! 0 | 10852 | `			if( !bMod ){` |
|       ! 0 | 10853 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10854 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10855 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10856 | `						return SXERR_ABORT;` |
|         - | 10857 | `					}` |
|       ! 0 | 10858 | `					goto done;` |
|         - | 10859 | `				}` |
|       ! 0 | 10860 | `				continue;` |
|         - | 10861 | `			}` |
|       ! 0 | 10862 | `		}` |
|   2823095 | 10863 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10864 | `			/* Extract the current keyword */` |
|   2823095 | 10865 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2823095 | 10866 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10867 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7639 | 10868 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7639 | 10869 | `				if( rc != SXRET_OK ){` |
|         6 | 10870 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10871 | `						return SXERR_ABORT;` |
|         - | 10872 | `					}` |
|         6 | 10873 | `					goto done;` |
|         - | 10874 | `				}` |
|      7635 | 10875 | `				continue;` |
|         - | 10876 | `			}` |
|   2815461 | 10877 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 10878 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 10879 | `				TraitUseEntry sUse;` |
|     15253 | 10880 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15253 | 10881 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15253 | 10882 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7632 | 10883 | `				for(;;){` |
|         - | 10884 | `					ph7_class *pTrait;` |
|         - | 10885 | `					SyString *pTraitName;` |
|     15261 | 10886 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 10887 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10888 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 10889 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10890 | `							return SXERR_ABORT;` |
|         - | 10891 | `						}` |
|       ! 0 | 10892 | `						break;` |
|         - | 10893 | `					}` |
|     15261 | 10894 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 10895 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 10896 | `						SyBlob sResolved;` |
|     15261 | 10897 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15261 | 10898 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     30517 | 10899 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15256 | 10900 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15261 | 10901 | `						SyBlobRelease(&sResolved);` |
|         - | 10902 | `					}` |
|         - | 10903 | `					/* Only traits are allowed */` |
|     15261 | 10904 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 10905 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 10906 | `					}` |
|     15261 | 10907 | `					if( pTrait == 0 ){` |
|       ! 0 | 10908 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10909 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 10910 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10911 | `							return SXERR_ABORT;` |
|         - | 10912 | `						}` |
|       ! 0 | 10913 | `					}else{` |
|     15261 | 10914 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 10915 | `					}` |
|     15261 | 10916 | `					pGen->pIn++; /* Advance past trait name */` |
|     15261 | 10917 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7629 | 10918 | `						break;` |
|         - | 10919 | `					}` |
|        10 | 10920 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 10921 | `				}` |
|         - | 10922 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15253 | 10923 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 10924 | `					SyToken *pBlock;` |
|        13 | 10925 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 10926 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 10927 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 10928 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 10929 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 10930 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 10931 | `					}else{` |
|       ! 0 | 10932 | `						pGen->pIn = pGen->pEnd;` |
|         - | 10933 | `					}` |
|         5 | 10934 | `				}` |
|     15253 | 10935 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 10936 | `				/* The semicolon will be consumed by the outer loop */` |
|     15253 | 10937 | `				continue;` |
|         - | 10938 | `			}` |
|   2800213 | 10939 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 10940 | `				int nSetTok;` |
|   2556777 | 10941 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2556777 | 10942 | `				if( nSetVis ){` |
|         - | 10943 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 10944 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 10945 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10946 | `					pGen->pIn += nSetTok;` |
|         2 | 10947 | `				}else{` |
|   2556775 | 10948 | `					iProtection = nKwrd;` |
|   2556775 | 10949 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 10950 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 10951 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2556775 | 10952 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2556775 | 10953 | `					if( nSetVis ){` |
|         9 | 10954 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 10955 | `						pGen->pIn += nSetTok;` |
|         4 | 10956 | `					}` |
|         - | 10957 | `				}` |
|         - | 10958 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 10959 | ``				 * `public private(set) readonly int $x`. */`` |
|   2556777 | 10960 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 10961 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 10962 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 10963 | `				}` |
|   2556772 | 10964 | `				if( pGen->pIn >= pGen->pEnd` |
|   2556777 | 10965 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10966 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10967 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10968 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 10969 | `					if( rc == SXERR_ABORT ){` |
|         - | 10970 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 10971 | `						return SXERR_ABORT;` |
|         - | 10972 | `					}` |
|       ! 0 | 10973 | `					goto done;` |
|         - | 10974 | `				}` |
|   2556777 | 10975 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10976 | `					/* Attribute declaration (untyped) */` |
|    406787 | 10977 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    406787 | 10978 | `					if( rc != SXRET_OK ){` |
|        11 | 10979 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10980 | `							return SXERR_ABORT;` |
|         - | 10981 | `						}` |
|        11 | 10982 | `						goto done;` |
|         - | 10983 | `					}` |
|    406923 | 10984 | `					continue;` |
|         - | 10985 | `				}` |
|   2149995 | 10986 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10987 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       299 | 10988 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       299 | 10989 | `					if( rc != SXRET_OK ){` |
|         8 | 10990 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10991 | `							return SXERR_ABORT;` |
|         - | 10992 | `						}` |
|         8 | 10993 | `						goto done;` |
|         - | 10994 | `					}` |
|       293 | 10995 | `					continue;` |
|         - | 10996 | `				}` |
|         - | 10997 | `				/* Extract the keyword */` |
|   2149701 | 10998 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1074848 | 10999 | `			}` |
|   2393137 | 11000 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 11001 | `				/* Process constant declaration */` |
|    235507 | 11002 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    235507 | 11003 | `				if( rc != SXRET_OK ){` |
|        11 | 11004 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11005 | `						return SXERR_ABORT;` |
|         - | 11006 | `					}` |
|        11 | 11007 | `					goto done;` |
|         - | 11008 | `				}` |
|    117752 | 11009 | `			}else{` |
|   2157635 | 11010 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 11011 | `					/* Static method or attribute,record that */` |
|     95045 | 11012 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     95045 | 11013 | `					pGen->pIn++; /* Jump the static keyword */` |
|     95045 | 11014 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11015 | `						int nSetTok;` |
|     68443 | 11016 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     68443 | 11017 | `						if( nSetVis ){` |
|         - | 11018 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 11019 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11020 | `							pGen->pIn += nSetTok;` |
|         2 | 11021 | `						}else{` |
|         - | 11022 | `							/* Extract the keyword */` |
|     68441 | 11023 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     68441 | 11024 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11025 | `								iProtection = nKwrd;` |
|       ! 0 | 11026 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 11027 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 11028 | `								if( nSetVis ){` |
|       ! 0 | 11029 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 11030 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 11031 | `								}` |
|       ! 0 | 11032 | `							}` |
|         - | 11033 | `						}` |
|     34219 | 11034 | `					}` |
|         - | 11035 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 11036 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 11037 | `					 * than a generic "expecting method" parse error. */` |
|     95045 | 11038 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11039 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11040 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 11041 | `					}` |
|     95040 | 11042 | `					if( pGen->pIn >= pGen->pEnd` |
|     95045 | 11043 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11044 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11045 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 11046 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11047 | `						if( rc == SXERR_ABORT ){` |
|         - | 11048 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11049 | `							return SXERR_ABORT;` |
|         - | 11050 | `						}` |
|       ! 0 | 11051 | `						goto done;` |
|         - | 11052 | `					}` |
|     95045 | 11053 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11054 | `						/* Attribute declaration */` |
|     26605 | 11055 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26605 | 11056 | `						if( rc != SXRET_OK ){` |
|         3 | 11057 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11058 | `								return SXERR_ABORT;` |
|         - | 11059 | `							}` |
|         3 | 11060 | `							goto done;` |
|         - | 11061 | `						}` |
|     26603 | 11062 | `						continue;` |
|         - | 11063 | `					}` |
|     68445 | 11064 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11065 | `						/* Typed static attribute declaration */` |
|        17 | 11066 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        17 | 11067 | `						if( rc != SXRET_OK ){` |
|         3 | 11068 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11069 | `								return SXERR_ABORT;` |
|         - | 11070 | `							}` |
|         3 | 11071 | `							goto done;` |
|         - | 11072 | `						}` |
|        15 | 11073 | `						continue;` |
|         - | 11074 | `					}` |
|         - | 11075 | `					/* Extract the keyword */` |
|     68431 | 11076 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2096808 | 11077 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 11078 | `					/* Abstract method,record that */` |
|      7615 | 11079 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 11080 | `					/* Mark the whole class as abstract */` |
|      7615 | 11081 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 11082 | `					/* Advance the stream cursor */` |
|      7615 | 11083 | `					pGen->pIn++;` |
|      7615 | 11084 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7615 | 11085 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7615 | 11086 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7613 | 11087 | `							iProtection = nKwrd;` |
|      7613 | 11088 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3804 | 11089 | `						}` |
|      3805 | 11090 | `					}` |
|      7615 | 11091 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7610 | 11092 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11093 | `							/* Static method */` |
|       ! 0 | 11094 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11095 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11096 | `					}` |
|      7615 | 11097 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7610 | 11098 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 11099 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 11100 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 11101 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 11102 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11103 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11104 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11105 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11106 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11107 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11108 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11109 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11110 | `										return SXERR_ABORT;` |
|         - | 11111 | `									}` |
|       ! 0 | 11112 | `									goto done;` |
|         - | 11113 | `								}` |
|         7 | 11114 | `								continue;` |
|         - | 11115 | `							}` |
|       ! 0 | 11116 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11117 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11118 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11119 | `							if( rc == SXERR_ABORT ){` |
|         - | 11120 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11121 | `								return SXERR_ABORT;` |
|         - | 11122 | `							}` |
|       ! 0 | 11123 | `							goto done;` |
|         - | 11124 | `					}` |
|      7609 | 11125 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2058787 | 11126 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11127 | `					/* final method ,record that */` |
|        21 | 11128 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        21 | 11129 | `					pGen->pIn++; /* Jump the final keyword */` |
|        21 | 11130 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11131 | `						/* Extract the keyword */` |
|        21 | 11132 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        21 | 11133 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        11 | 11134 | `							iProtection = nKwrd;` |
|        11 | 11135 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11136 | `						}` |
|         9 | 11137 | `					}` |
|        21 | 11138 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11139 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11140 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11141 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11142 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11143 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11144 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11145 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11146 | `									return SXERR_ABORT;` |
|         - | 11147 | `								}` |
|       ! 0 | 11148 | `								goto done;` |
|         - | 11149 | `							}` |
|        14 | 11150 | `							continue;` |
|         - | 11151 | `					}` |
|         9 | 11152 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11153 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11154 | `							/* Static method */` |
|       ! 0 | 11155 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11156 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11157 | `					}` |
|         9 | 11158 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11159 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11160 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11161 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11162 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11163 | `							if( rc == SXERR_ABORT ){` |
|         - | 11164 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11165 | `								return SXERR_ABORT;` |
|         - | 11166 | `							}` |
|       ! 0 | 11167 | `							goto done;` |
|         - | 11168 | `					}` |
|         9 | 11169 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11170 | `				}` |
|   2131003 | 11171 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11172 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11173 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11174 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11175 | `						if( rc == SXERR_ABORT ){` |
|         - | 11176 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11177 | `							return SXERR_ABORT;` |
|         - | 11178 | `						}` |
|       ! 0 | 11179 | `						goto done;` |
|         - | 11180 | `				}` |
|   2131003 | 11181 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11182 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11183 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11184 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11185 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11186 | `						if( rc == SXERR_ABORT ){` |
|         - | 11187 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11188 | `							return SXERR_ABORT;` |
|         - | 11189 | `						}` |
|       ! 0 | 11190 | `						goto done;` |
|         - | 11191 | `					}` |
|         - | 11192 | `					/* Attribute declaration */` |
|         7 | 11193 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11194 | `				}else{` |
|         - | 11195 | `					/* Process method declaration */` |
|   2130997 | 11196 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11197 | `				}` |
|   2131003 | 11198 | `				if( rc != SXRET_OK ){` |
|        16 | 11199 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11200 | `						return SXERR_ABORT;` |
|         - | 11201 | `					}` |
|        16 | 11202 | `					goto done;` |
|         - | 11203 | `				}` |
|         - | 11204 | `			}` |
|   1183245 | 11205 | `		}else{` |
|         - | 11206 | `			/* Attribute declaration */` |
|       ! 0 | 11207 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11208 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11209 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11210 | `					return SXERR_ABORT;` |
|         - | 11211 | `				}` |
|       ! 0 | 11212 | `				goto done;` |
|         - | 11213 | `			}` |
|         - | 11214 | `		}` |
|         5 | 11215 | `	}` |
|         - | 11216 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11217 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11218 | `	 */` |
|         - | 11219 | `	{` |
|         - | 11220 | `		TraitUseEntry *apUse;` |
|         - | 11221 | `		sxu32 nU;` |
|    350965 | 11222 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    366213 | 11223 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15253 | 11224 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15253 | 11225 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15253 | 11226 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15253 | 11227 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11228 | `			sxu32 nT;` |
|     15253 | 11229 | `			if( !hasResolution ){` |
|         - | 11230 | `				/* No conflict resolution block: use standard trait application */` |
|     30487 | 11231 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15249 | 11232 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15249 | 11233 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11234 | `						break;` |
|         - | 11235 | `					}` |
|      7627 | 11236 | `				}` |
|      7624 | 11237 | `			}else{` |
|         - | 11238 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11239 | `				 * then use the block to resolve method conflicts.` |
|         - | 11240 | `				 */` |
|         - | 11241 | `				SyToken *pR;` |
|        25 | 11242 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11243 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11244 | `					ph7_class_attr *pAR;` |
|         - | 11245 | `					SyHashEntry *pER;` |
|         - | 11246 | `					SyString *pNR;` |
|        15 | 11247 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11248 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11249 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11250 | `						pNR = &pAR->sName;` |
|       ! 0 | 11251 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11252 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11253 | `						}` |
|       ! 0 | 11254 | `					}` |
|        15 | 11255 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11256 | `				}` |
|         - | 11257 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11258 | `				pR = pUse->pResolvStart;` |
|        27 | 11259 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11260 | `					SyString sTrait,sMethod;` |
|         - | 11261 | `					ph7_class *pSrcTrait;` |
|         - | 11262 | `					ph7_class_method *pMeth;` |
|         - | 11263 | `					sxi32 nRKwrd;` |
|        41 | 11264 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11265 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11266 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11267 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11268 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11269 | `					sMethod = pR->sData;` |
|        17 | 11270 | `					pR++;` |
|        17 | 11271 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11272 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11273 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11274 | `							sTrait = sMethod;` |
|         7 | 11275 | `							pR++;` |
|         7 | 11276 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11277 | `							sMethod = pR->sData;` |
|         7 | 11278 | `							pR++;` |
|         3 | 11279 | `						}` |
|         3 | 11280 | `					}` |
|        17 | 11281 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11282 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11283 | `						continue;` |
|         - | 11284 | `					}` |
|        17 | 11285 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11286 | `					pR++;` |
|        17 | 11287 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11288 | `						pSrcTrait = 0;` |
|         7 | 11289 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11290 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11291 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11292 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11293 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11294 | `								break;` |
|         - | 11295 | `							}` |
|         2 | 11296 | `						}` |
|         5 | 11297 | `						if( pSrcTrait ){` |
|         5 | 11298 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11299 | `							if( pMeth ){` |
|         5 | 11300 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11301 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11302 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11303 | `								}` |
|         2 | 11304 | `							}` |
|         2 | 11305 | `						}` |
|         2 | 11306 | `					}` |
|        35 | 11307 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11308 | `				}` |
|         - | 11309 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11310 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11311 | `					ph7_class_method *pMR;` |
|         - | 11312 | `					SyHashEntry *pER;` |
|         - | 11313 | `					SyString *pNR;` |
|        15 | 11314 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11315 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11316 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11317 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11318 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11319 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11320 | `						}` |
|         3 | 11321 | `					}` |
|         9 | 11322 | `				}` |
|         - | 11323 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11324 | `				pR = pUse->pResolvStart;` |
|        27 | 11325 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11326 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11327 | `					ph7_class *pSrcTrait;` |
|         - | 11328 | `					ph7_class_method *pMeth;` |
|        27 | 11329 | `					int hasQual = 0;` |
|         - | 11330 | `					sxi32 nRKwrd;` |
|        41 | 11331 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11332 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11333 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11334 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11335 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11336 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11337 | `					sMethod = pR->sData;` |
|        17 | 11338 | `					pR++;` |
|        17 | 11339 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11340 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11341 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11342 | `							sTrait = sMethod;` |
|         7 | 11343 | `							hasQual = 1;` |
|         7 | 11344 | `							pR++;` |
|         7 | 11345 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11346 | `							sMethod = pR->sData;` |
|         7 | 11347 | `							pR++;` |
|         3 | 11348 | `						}` |
|         3 | 11349 | `					}` |
|        17 | 11350 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11351 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11352 | `						continue;` |
|         - | 11353 | `					}` |
|        17 | 11354 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11355 | `					pR++;` |
|        17 | 11356 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11357 | `						sxi32 iNewVis = -1;` |
|        13 | 11358 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11359 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11360 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11361 | `								iNewVis = nAK;` |
|         7 | 11362 | `								pR++;` |
|         3 | 11363 | `							}` |
|         3 | 11364 | `						}` |
|        13 | 11365 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11366 | `							sAlias = pR->sData;` |
|        11 | 11367 | `							pR++;` |
|         4 | 11368 | `						}` |
|        13 | 11369 | `						pMeth = 0;` |
|        13 | 11370 | `						if( hasQual ){` |
|         3 | 11371 | `							pSrcTrait = 0;` |
|         5 | 11372 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11373 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11374 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11375 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11376 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11377 | `									break;` |
|         - | 11378 | `								}` |
|         2 | 11379 | `							}` |
|         3 | 11380 | `							if( pSrcTrait ){` |
|         3 | 11381 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11382 | `							}` |
|         2 | 11383 | `						}else{` |
|        10 | 11384 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11385 | `						}` |
|        13 | 11386 | `						if( pMeth ){` |
|        13 | 11387 | `							if( sAlias.nByte > 0 ){` |
|         - | 11388 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11389 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11390 | `								 */` |
|         - | 11391 | `								ph7_class_method *pAlias;` |
|         - | 11392 | `								char *zAliasDup;` |
|        11 | 11393 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11394 | `								if( pAlias ){` |
|        11 | 11395 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11396 | `									if( iNewVis >= 0 ){` |
|         5 | 11397 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11398 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11399 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11400 | `									}` |
|        11 | 11401 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11402 | `									if( zAliasDup ){` |
|        11 | 11403 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11404 | `									}` |
|         7 | 11405 | `								}` |
|         7 | 11406 | `							}else if( iNewVis >= 0 ){` |
|         - | 11407 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11408 | `								ph7_class_method *pCopy;` |
|         3 | 11409 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11410 | `								if( pCopy ){` |
|         3 | 11411 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11412 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11413 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11414 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11415 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11416 | `									/* Replace the method in the class hash */` |
|         3 | 11417 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11418 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11419 | `								}` |
|         1 | 11420 | `							}` |
|         5 | 11421 | `						}` |
|         5 | 11422 | `						SXUNUSED(hasQual);` |
|         5 | 11423 | `					}` |
|        21 | 11424 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11425 | `				}` |
|         - | 11426 | `			}` |
|     15253 | 11427 | `			SySetRelease(&pUse->aTraits);` |
|      7629 | 11428 | `		}` |
|         - | 11429 | `	}` |
|    350965 | 11430 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11431 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11432 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3825 | 11433 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3825 | 11434 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11435 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11436 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11437 | `			return SXERR_ABORT;` |
|         - | 11438 | `		}` |
|      1910 | 11439 | `	}` |
|         - | 11440 | `	/* Install the class */` |
|    350965 | 11441 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    350965 | 11442 | `	if( rc == SXRET_OK ){` |
|         - | 11443 | `		ph7_class **apInterface;` |
|         - | 11444 | `		sxu32 n;` |
|    350965 | 11445 | `		if( pBase ){` |
|         - | 11446 | `			/* Inherit from base class and mark as a subclass */` |
|    182463 | 11447 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     91229 | 11448 | `		}` |
|    350965 | 11449 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    514387 | 11450 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11451 | `			/* Implements one or more interface */` |
|    163427 | 11452 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    163427 | 11453 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11454 | `				break;` |
|         - | 11455 | `			}` |
|     81716 | 11456 | `		}` |
|         - | 11457 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11458 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    350965 | 11459 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3825 | 11460 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3825 | 11461 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11462 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11463 | `			}` |
|      3825 | 11464 | `			if( pIntf ){` |
|      3825 | 11465 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1910 | 11466 | `			}` |
|      3825 | 11467 | `			if( pClass->nEnumBacking != 0 ){` |
|      3813 | 11468 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3813 | 11469 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11470 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11471 | `				}` |
|      3813 | 11472 | `				if( pIntf ){` |
|      3813 | 11473 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1904 | 11474 | `				}` |
|      1904 | 11475 | `			}` |
|      1910 | 11476 | `		}` |
|         - | 11477 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11478 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    350960 | 11479 | `		if( rc == SXRET_OK` |
|    350960 | 11480 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    350965 | 11481 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    186113 | 11482 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11483 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    186113 | 11484 | `			if( pStringable ){` |
|    186113 | 11485 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    186113 | 11486 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11487 | `				sxu32 i;` |
|    186113 | 11488 | `				int bAlready = 0;` |
|    224077 | 11489 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     41767 | 11490 | `					if( apImpl[i] == pStringable ){` |
|      3803 | 11491 | `						bAlready = 1;` |
|      3803 | 11492 | `						break;` |
|         - | 11493 | `					}` |
|     18987 | 11494 | `				}` |
|    186113 | 11495 | `				if( !bAlready ){` |
|    182315 | 11496 | `					PH7_ClassImplement(pClass,pStringable);` |
|     91155 | 11497 | `				}` |
|     93054 | 11498 | `			}` |
|     93054 | 11499 | `		}` |
|         - | 11500 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    350965 | 11501 | `		if( rc == SXRET_OK ){` |
|    350965 | 11502 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    350965 | 11503 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11504 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11505 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11506 | `				return SXERR_ABORT;` |
|         - | 11507 | `			}` |
|    175480 | 11508 | `		}` |
|         - | 11509 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    350965 | 11510 | `		if( rc == SXRET_OK ){` |
|    350965 | 11511 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    350965 | 11512 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11513 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11514 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11515 | `				return SXERR_ABORT;` |
|         - | 11516 | `			}` |
|    175480 | 11517 | `		}` |
|    175480 | 11518 | `	}` |
|    350965 | 11519 | `	SySetRelease(&aUseEntries);` |
|    350965 | 11520 | `	SySetRelease(&aInterfaces);` |
|    350965 | 11521 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11522 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11523 | `		return SXERR_ABORT;` |
|         - | 11524 | `	}` |
|    175480 | 11525 | `done:` |
|         - | 11526 | `	/* Point beyond the class body */` |
|    351007 | 11527 | `	pGen->pIn = &pEnd[1];` |
|    351007 | 11528 | `	pGen->pEnd = pTmp;` |
|    351007 | 11529 | `	return PH7_OK;` |
|    175507 | 11530 | `}` |
|         - | 11531 | `/* Compile a named class declaration (the common case). */` |
|    350976 | 11532 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11533 | `{` |
|    350981 | 11534 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11535 | `}` |
|         - | 11536 | `/*` |
|         - | 11537 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11538 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11539 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11540 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11541 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11542 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11543 | ` */` |
|        28 | 11544 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11545 | `{` |
|         - | 11546 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11547 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11548 | `	SyString sName;` |
|         - | 11549 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11550 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11551 | `	                              * is keyed to this 'class' token */` |
|         - | 11552 | `	ph7_value *pObj;` |
|        32 | 11553 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11554 | `	sxu32 nIdx,nLen;` |
|         - | 11555 | `	sxi32 nArg,rc;` |
|        14 | 11556 | `	SXUNUSED(iCompileFlag);` |
|         - | 11557 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11558 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11559 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11560 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11561 | `	}` |
|        32 | 11562 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11563 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11564 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11565 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11566 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11567 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11568 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11569 | `		return rc;` |
|         - | 11570 | `	}` |
|         - | 11571 | `	{` |
|         - | 11572 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11573 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11574 | `		if( pAnonClass` |
|        32 | 11575 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11576 | `			return SXERR_ABORT;` |
|         - | 11577 | `		}` |
|         - | 11578 | `	}` |
|         - | 11579 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11580 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11581 | `	nArg = 0;` |
|        32 | 11582 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11583 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11584 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11585 | `		SyToken *pArgNext;` |
|         7 | 11586 | `		pGen->pIn = pArgStart;` |
|         7 | 11587 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11588 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11589 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11590 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11591 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11592 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11593 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11594 | `					return SXERR_ABORT;` |
|         - | 11595 | `				}` |
|         7 | 11596 | `				nArg++;` |
|         3 | 11597 | `			}` |
|         7 | 11598 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11599 | `		}` |
|         7 | 11600 | `		pGen->pIn = pSavedIn;` |
|         7 | 11601 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11602 | `	}` |
|         - | 11603 | `	/* Load the synthesized class name */` |
|        32 | 11604 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11605 | `	if( pObj == 0 ){` |
|       ! 0 | 11606 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11607 | `		return SXERR_ABORT;` |
|         - | 11608 | `	}` |
|        32 | 11609 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11610 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11611 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11612 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11613 | `	return SXRET_OK;` |
|        18 | 11614 | `}` |
|         - | 11615 | `/*` |
|         - | 11616 | ` * Compile a user-defined abstract class.` |
|         - | 11617 | ` *  According to the PHP language reference manual` |
|         - | 11618 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11619 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11620 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11621 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11622 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11623 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11624 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11625 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11626 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11627 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11628 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11629 | ` *   could differ.` |
|         - | 11630 | ` */` |
|         - | 11631 | `/*` |
|         - | 11632 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11633 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11634 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11635 | ` */` |
|  10830832 | 11636 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11637 | `{` |
|  10830837 | 11638 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   6398555 | 11639 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   6398555 | 11640 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   6352977 | 11641 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3161259 | 11642 | `	}` |
|  10754805 | 11643 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  10754745 | 11644 | `	return FALSE;` |
|   5415421 | 11645 | `}` |
|         - | 11646 | `/*` |
|         - | 11647 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11648 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11649 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11650 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11651 | ` */` |
|  10754740 | 11652 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11653 | `{` |
|  10754745 | 11654 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  10754745 | 11655 | `	sxi32 iFlags = 0,iFlag;` |
|  10830837 | 11656 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76097 | 11657 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11658 | `			pDup = pIn;` |
|         2 | 11659 | `		}` |
|     76097 | 11660 | `		iFlags \|= iFlag;` |
|     76097 | 11661 | `		pIn++;` |
|         5 | 11662 | `	}` |
|  10754745 | 11663 | `	*ppIn = pIn;` |
|  10754745 | 11664 | `	if( ppDup ){ *ppDup = pDup; }` |
|  10754745 | 11665 | `	return iFlags;` |
|         5 | 11666 | `}` |
|         - | 11667 | `/*` |
|         - | 11668 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11669 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11670 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11671 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11672 | `` * `readonly`) to their existing handlers.`` |
|         - | 11673 | ` */` |
|  10720500 | 11674 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11675 | `{` |
|  10720505 | 11676 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   5402089 | 11677 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  10741418 | 11678 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11679 | `}` |
|         - | 11680 | `/*` |
|         - | 11681 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11682 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11683 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11684 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11685 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11686 | ` */` |
|     34240 | 11687 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11688 | `{` |
|         - | 11689 | `	SyToken *pDup;` |
|     34245 | 11690 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11691 | `	sxi32 rc;` |
|     34245 | 11692 | `	if( pDup ){` |
|         4 | 11693 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11694 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11695 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11696 | `			return SXERR_ABORT;` |
|         - | 11697 | `		}` |
|         1 | 11698 | `	}` |
|     34240 | 11699 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17125 | 11700 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11701 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11702 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11703 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11704 | `			return SXERR_ABORT;` |
|         - | 11705 | `		}` |
|         1 | 11706 | `	}` |
|     34245 | 11707 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17125 | 11708 | `}` |
|         - | 11709 | `/*` |
|         - | 11710 | ` * Compile a user-defined trait.` |
|         - | 11711 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11712 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11713 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11714 | ` */` |
|      7670 | 11715 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11716 | `{` |
|      7675 | 11717 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11718 | `	ph7_class *pClass;` |
|         - | 11719 | `	SyToken *pEnd,*pTmp;` |
|         - | 11720 | `	sxi32 iProtection;` |
|         - | 11721 | `	sxi32 iAttrflags;` |
|         - | 11722 | `	SyString *pName;` |
|         - | 11723 | `	sxi32 nKwrd;` |
|         - | 11724 | `	sxi32 rc;` |
|         - | 11725 | `	/* Jump the 'trait' keyword */` |
|      7675 | 11726 | `	pGen->pIn++;` |
|      7675 | 11727 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11728 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11729 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11730 | `			return SXERR_ABORT;` |
|         - | 11731 | `		}` |
|       ! 0 | 11732 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11733 | `			pGen->pIn++;` |
|       ! 0 | 11734 | `		}` |
|       ! 0 | 11735 | `		return SXRET_OK;` |
|         - | 11736 | `	}` |
|         - | 11737 | `	/* Extract trait name */` |
|      7675 | 11738 | `	pName = &pGen->pIn->sData;` |
|      7675 | 11739 | `	pGen->pIn++;` |
|         - | 11740 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11741 | `		SyBlob sFQN;` |
|         - | 11742 | `		SyString sFQNStr;` |
|      7675 | 11743 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7675 | 11744 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7675 | 11745 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7675 | 11746 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7675 | 11747 | `		SyBlobRelease(&sFQN);` |
|         - | 11748 | `	}` |
|      7675 | 11749 | `	if( pClass == 0 ){` |
|       ! 0 | 11750 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11751 | `		return SXERR_ABORT;` |
|         - | 11752 | `	}` |
|      7675 | 11753 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7675 | 11754 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11755 | `		return SXERR_ABORT;` |
|         - | 11756 | `	}` |
|         - | 11757 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7675 | 11758 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11759 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11760 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11761 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11762 | `			return SXERR_ABORT;` |
|         - | 11763 | `		}` |
|       ! 0 | 11764 | `		return SXRET_OK;` |
|         - | 11765 | `	}` |
|      7675 | 11766 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7675 | 11767 | `	pEnd = 0;` |
|      7675 | 11768 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7675 | 11769 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11770 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11771 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11772 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11773 | `			return SXERR_ABORT;` |
|         - | 11774 | `		}` |
|       ! 0 | 11775 | `		return SXRET_OK;` |
|         - | 11776 | `	}` |
|         - | 11777 | `	/* The delimiter token is the trait body's closing brace */` |
|      7675 | 11778 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11779 | `	/* Swap token stream */` |
|      7675 | 11780 | `	pTmp = pGen->pEnd;` |
|      7675 | 11781 | `	pGen->pEnd = pEnd;` |
|         - | 11782 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7675 | 11783 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11784 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     53223 | 11785 | `	for(;;){` |
|    144459 | 11786 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     19011 | 11787 | `			pGen->pIn++;` |
|         5 | 11788 | `		}` |
|    125453 | 11789 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7675 | 11790 | `			break;` |
|         - | 11791 | `		}` |
|         - | 11792 | `		/* Bind a directly-preceding docblock to this member */` |
|    117783 | 11793 | `		GenStateSetPendingDoc(&(*pGen));` |
|    117783 | 11794 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11795 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11796 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11797 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11798 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11799 | `				return SXERR_ABORT;` |
|         - | 11800 | `			}` |
|       ! 0 | 11801 | `			goto done;` |
|         - | 11802 | `		}` |
|    117783 | 11803 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    117783 | 11804 | `		iAttrflags = 0;` |
|    117783 | 11805 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    117783 | 11806 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    117783 | 11807 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11808 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11809 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11810 | `				for(;;){` |
|         - | 11811 | `					ph7_class *pUsedTrait;` |
|         - | 11812 | `					SyString *pUsedName;` |
|         5 | 11813 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11814 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11815 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11816 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11817 | `							return SXERR_ABORT;` |
|         - | 11818 | `						}` |
|       ! 0 | 11819 | `						break;` |
|         - | 11820 | `					}` |
|         5 | 11821 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11822 | `					{` |
|         - | 11823 | `						SyBlob sResolved;` |
|         5 | 11824 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11825 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11826 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11827 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11828 | `						SyBlobRelease(&sResolved);` |
|         - | 11829 | `					}` |
|         5 | 11830 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11831 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11832 | `					}` |
|         5 | 11833 | `					if( pUsedTrait == 0 ){` |
|         4 | 11834 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11835 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11836 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11837 | `							return SXERR_ABORT;` |
|         - | 11838 | `						}` |
|         2 | 11839 | `					}else{` |
|         3 | 11840 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11841 | `					}` |
|         5 | 11842 | `					pGen->pIn++;` |
|         5 | 11843 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11844 | `						break;` |
|         - | 11845 | `					}` |
|       ! 0 | 11846 | `					pGen->pIn++;` |
|       ! 0 | 11847 | `				}` |
|         5 | 11848 | `				continue;` |
|         - | 11849 | `			}` |
|    117779 | 11850 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    117763 | 11851 | `				iProtection = nKwrd;` |
|    117763 | 11852 | `				pGen->pIn++;` |
|    117758 | 11853 | `				if( pGen->pIn >= pGen->pEnd` |
|    117763 | 11854 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11855 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11856 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11857 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11858 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11859 | `						return SXERR_ABORT;` |
|         - | 11860 | `					}` |
|       ! 0 | 11861 | `					goto done;` |
|         - | 11862 | `				}` |
|    117763 | 11863 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     18997 | 11864 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     18997 | 11865 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11866 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11867 | `							return SXERR_ABORT;` |
|         - | 11868 | `						}` |
|       ! 0 | 11869 | `						goto done;` |
|         - | 11870 | `					}` |
|     18997 | 11871 | `					continue;` |
|         - | 11872 | `				}` |
|     98771 | 11873 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 11874 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 11875 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11876 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11877 | `							return SXERR_ABORT;` |
|         - | 11878 | `						}` |
|       ! 0 | 11879 | `						goto done;` |
|         - | 11880 | `					}` |
|         5 | 11881 | `					continue;` |
|         - | 11882 | `				}` |
|     98767 | 11883 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     49381 | 11884 | `			}` |
|     98783 | 11885 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 11886 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11887 | `					"Traits cannot have constants");` |
|       ! 0 | 11888 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11889 | `					return SXERR_ABORT;` |
|         - | 11890 | `				}` |
|       ! 0 | 11891 | `				goto done;` |
|       ! 0 | 11892 | `			}else{` |
|     98783 | 11893 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7607 | 11894 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7607 | 11895 | `					pGen->pIn++;` |
|      7607 | 11896 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7605 | 11897 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7605 | 11898 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11899 | `							iProtection = nKwrd;` |
|       ! 0 | 11900 | `							pGen->pIn++;` |
|       ! 0 | 11901 | `						}` |
|      3800 | 11902 | `					}` |
|      7602 | 11903 | `					if( pGen->pIn >= pGen->pEnd` |
|      7607 | 11904 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11905 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11906 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 11907 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11908 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11909 | `							return SXERR_ABORT;` |
|         - | 11910 | `						}` |
|       ! 0 | 11911 | `						goto done;` |
|         - | 11912 | `					}` |
|      7607 | 11913 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 11914 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 11915 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11916 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11917 | `								return SXERR_ABORT;` |
|         - | 11918 | `							}` |
|       ! 0 | 11919 | `							goto done;` |
|         - | 11920 | `						}` |
|         3 | 11921 | `						continue;` |
|         - | 11922 | `					}` |
|      7605 | 11923 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 11924 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11925 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11926 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11927 | `								return SXERR_ABORT;` |
|         - | 11928 | `							}` |
|       ! 0 | 11929 | `							goto done;` |
|         - | 11930 | `						}` |
|       ! 0 | 11931 | `						continue;` |
|         - | 11932 | `					}` |
|      7605 | 11933 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     94981 | 11934 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 11935 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 11936 | `					pGen->pIn++;` |
|         6 | 11937 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 11938 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 11939 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 11940 | `							iProtection = nKwrd;` |
|         6 | 11941 | `							pGen->pIn++;` |
|         2 | 11942 | `						}` |
|         2 | 11943 | `					}` |
|         6 | 11944 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 11945 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11946 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11947 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 11948 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11949 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11950 | `							return SXERR_ABORT;` |
|         - | 11951 | `						}` |
|       ! 0 | 11952 | `						goto done;` |
|         - | 11953 | `					}` |
|         6 | 11954 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 11955 | `				}` |
|     98781 | 11956 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11957 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11958 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 11959 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11960 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11961 | `						return SXERR_ABORT;` |
|         - | 11962 | `					}` |
|       ! 0 | 11963 | `					goto done;` |
|         - | 11964 | `				}` |
|     98781 | 11965 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 11966 | `					pGen->pIn++;` |
|       ! 0 | 11967 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 11968 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11969 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11970 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11971 | `							return SXERR_ABORT;` |
|         - | 11972 | `						}` |
|       ! 0 | 11973 | `						goto done;` |
|         - | 11974 | `					}` |
|       ! 0 | 11975 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11976 | `				}else{` |
|     98781 | 11977 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11978 | `				}` |
|     98781 | 11979 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11980 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11981 | `						return SXERR_ABORT;` |
|         - | 11982 | `					}` |
|       ! 0 | 11983 | `					goto done;` |
|         - | 11984 | `				}` |
|         - | 11985 | `			}` |
|     49393 | 11986 | `		}else{` |
|       ! 0 | 11987 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11988 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11989 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11990 | `					return SXERR_ABORT;` |
|         - | 11991 | `				}` |
|       ! 0 | 11992 | `				goto done;` |
|         - | 11993 | `			}` |
|         - | 11994 | `		}` |
|         5 | 11995 | `	}` |
|         - | 11996 | `	/* Install the trait */` |
|      7675 | 11997 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7675 | 11998 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11999 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12000 | `		return SXERR_ABORT;` |
|         - | 12001 | `	}` |
|      3835 | 12002 | `done:` |
|         - | 12003 | `	/* Point beyond the trait body */` |
|      7675 | 12004 | `	pGen->pIn = &pEnd[1];` |
|      7675 | 12005 | `	pGen->pEnd = pTmp;` |
|      7675 | 12006 | `	return PH7_OK;` |
|      3840 | 12007 | `}` |
|         - | 12008 | `/*` |
|         - | 12009 | ` * Compile a user-defined class.` |
|         - | 12010 | ` *  According to the PHP language reference manual` |
|         - | 12011 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 12012 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 12013 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 12014 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 12015 | ` *   and functions (called "methods").` |
|         - | 12016 | ` */` |
|    312912 | 12017 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 12018 | `{` |
|         - | 12019 | `	sxi32 rc;` |
|    312917 | 12020 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    312917 | 12021 | `	return rc;` |
|         5 | 12022 | `}` |
|         - | 12023 | `/*` |
|         - | 12024 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 12025 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 12026 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 12027 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 12028 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 12029 | ` */` |
|  10678668 | 12030 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 12031 | `{` |
|  10859436 | 12032 | `	return (pIn->nType & PH7_TK_ID)` |
|   5520097 | 12033 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    190380 | 12034 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  10859431 | 12035 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 12036 | `}` |
|         - | 12037 | `/*` |
|         - | 12038 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 12039 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 12040 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 12041 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 12042 | ` */` |
|      3824 | 12043 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 12044 | `{` |
|      3829 | 12045 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 12046 | `}` |
|         - | 12047 | `/*` |
|         - | 12048 | ` * Exception handling.` |
|         - | 12049 | ` *  According to the PHP language reference manual` |
|         - | 12050 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 12051 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 12052 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 12053 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 12054 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 12055 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 12056 | ` *    (or re-thrown) within a catch block.` |
|         - | 12057 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 12058 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 12059 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 12060 | ` *    been defined with set_exception_handler().` |
|         - | 12061 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 12062 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 12063 | ` */` |
|         - | 12064 | `/*` |
|         - | 12065 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 12066 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 12067 | ` * indicates failure.` |
|         - | 12068 | ` */` |
|    474912 | 12069 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 12070 | `{` |
|    474917 | 12071 | `	sxi32 rc = SXRET_OK;` |
|    474917 | 12072 | `	if( pRoot->pOp ){` |
|    474905 | 12073 | `		switch( pRoot->pOp->iOp ){` |
|    237450 | 12074 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 12075 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 12076 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 12077 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 12078 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 12079 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    474905 | 12080 | `			break;` |
|       ! 0 | 12081 | `		default:` |
|         - | 12082 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 12083 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 12084 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 12085 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12086 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 12087 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 12088 | `				rc = SXERR_INVALID;` |
|       ! 0 | 12089 | `			}` |
|       ! 0 | 12090 | `			break;` |
|         - | 12091 | `		}` |
|    237467 | 12092 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 12093 | `		/* Unexpected expression */` |
|       ! 0 | 12094 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12095 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12096 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 12097 | `			rc = SXERR_INVALID;` |
|       ! 0 | 12098 | `		}` |
|       ! 0 | 12099 | `	}` |
|    474917 | 12100 | `	return rc;` |
|         5 | 12101 | `}` |
|         - | 12102 | `/*` |
|         - | 12103 | ` * Compile a 'throw' statement.` |
|         - | 12104 | ` * throw: This is how you trigger an exception.` |
|         - | 12105 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12106 | ` */` |
|    474876 | 12107 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12108 | `{` |
|    474881 | 12109 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12110 | `	GenBlock *pBlock;` |
|         - | 12111 | `	sxu32 nIdx;` |
|         - | 12112 | `	sxi32 rc;` |
|    474881 | 12113 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12114 | `	/* Compile the expression */` |
|    474881 | 12115 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    474881 | 12116 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12117 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12118 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12119 | `			return SXERR_ABORT;` |
|         - | 12120 | `		}` |
|       ! 0 | 12121 | `		return SXRET_OK;` |
|         - | 12122 | `	}` |
|    474881 | 12123 | `	pBlock = pGen->pCurrent;` |
|         - | 12124 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   1876001 | 12125 | `	while(pBlock->pParent){` |
|   1875997 | 12126 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    474877 | 12127 | `			break;` |
|         - | 12128 | `		}` |
|         - | 12129 | `		/* Point to the parent block */` |
|   1401125 | 12130 | `		pBlock = pBlock->pParent;` |
|         5 | 12131 | `	}` |
|         - | 12132 | `	/* Emit the throw instruction */` |
|    474881 | 12133 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12134 | `	/* Emit the jump */` |
|    474881 | 12135 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    474881 | 12136 | `	return SXRET_OK;` |
|    237443 | 12137 | `}` |
|         - | 12138 | `/*` |
|         - | 12139 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12140 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12141 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12142 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12143 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12144 | ` */` |
|        36 | 12145 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12146 | `{` |
|        38 | 12147 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12148 | `	GenBlock *pBlock;` |
|         - | 12149 | `	sxu32 nIdx;` |
|         - | 12150 | `	sxi32 rc;` |
|        18 | 12151 | `	(void)iCompileFlag;` |
|        38 | 12152 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12153 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12154 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12155 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12156 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12157 | `			return SXERR_ABORT;` |
|         - | 12158 | `		}` |
|       ! 0 | 12159 | `		return SXRET_OK;` |
|         - | 12160 | `	}` |
|        38 | 12161 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12162 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12163 | `		return SXERR_ABORT;` |
|         - | 12164 | `	}` |
|        38 | 12165 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12166 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12167 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12168 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12169 | `			return SXERR_ABORT;` |
|         - | 12170 | `		}` |
|       ! 0 | 12171 | `		return SXRET_OK;` |
|         - | 12172 | `	}` |
|         - | 12173 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12174 | `	pBlock = pGen->pCurrent;` |
|        60 | 12175 | `	while( pBlock->pParent ){` |
|        49 | 12176 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12177 | `			break;` |
|         - | 12178 | `		}` |
|        23 | 12179 | `		pBlock = pBlock->pParent;` |
|         1 | 12180 | `	}` |
|        38 | 12181 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12182 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12183 | `	return SXRET_OK;` |
|        20 | 12184 | `}` |
|         - | 12185 | `/*` |
|         - | 12186 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12187 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12188 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12189 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12190 | ` * compile error propagated from the parser.` |
|         - | 12191 | ` */` |
|        54 | 12192 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12193 | `{` |
|         - | 12194 | `	SyString sClassName;` |
|         - | 12195 | `	SyToken *pToken;` |
|         - | 12196 | `	SyString *pName;` |
|         - | 12197 | `	char *zDup;` |
|         - | 12198 | `	sxi32 rc;` |
|        59 | 12199 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        59 | 12200 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        59 | 12201 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        59 | 12202 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        59 | 12203 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12204 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12205 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12206 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12207 | `		return SXERR_INVALID;` |
|         - | 12208 | `	}` |
|        59 | 12209 | `	pGen->pIn++; /* '(' */` |
|        27 | 12210 | `	for(;;){` |
|         - | 12211 | `		SyBlob sResolved;` |
|        59 | 12212 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        59 | 12213 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12214 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12215 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12216 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12217 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12218 | `			return SXERR_INVALID;` |
|         - | 12219 | `		}` |
|        86 | 12220 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        54 | 12221 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        59 | 12222 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        59 | 12223 | `		SyBlobRelease(&sResolved);` |
|        59 | 12224 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12225 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        59 | 12226 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        54 | 12227 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12228 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12229 | `			pGen->pIn++; continue;` |
|         - | 12230 | `		}` |
|        59 | 12231 | `		break;` |
|       ! 0 | 12232 | `	}` |
|        54 | 12233 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12234 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12235 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12236 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12237 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12238 | `		return SXERR_INVALID;` |
|         - | 12239 | `	}` |
|        59 | 12240 | `	pGen->pIn++; /* '$' */` |
|        59 | 12241 | `	pName = &pGen->pIn->sData;` |
|        59 | 12242 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12243 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12244 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12245 | `	pGen->pIn++;` |
|        59 | 12246 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12247 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12248 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12249 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12250 | `		return SXERR_INVALID;` |
|         - | 12251 | `	}` |
|        59 | 12252 | `	pGen->pIn++; /* ')' */` |
|        59 | 12253 | `	return SXRET_OK;` |
|        32 | 12254 | `}` |
|         - | 12255 | `/*` |
|         - | 12256 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12257 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12258 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12259 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12260 | ` * VmThrowException):` |
|         - | 12261 | ` *` |
|         - | 12262 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12263 | ` *    <try body>` |
|         - | 12264 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12265 | ` *    JMP  -> finally\|end` |
|         - | 12266 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12267 | ` *    <catch body>` |
|         - | 12268 | ` *    JMP  -> finally\|end` |
|         - | 12269 | ` *    ... more catches ...` |
|         - | 12270 | ` *  Lfin: <finally body>` |
|         - | 12271 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12272 | ` *  Lend:` |
|         - | 12273 | ` */` |
|        98 | 12274 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12275 | `{` |
|       103 | 12276 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12277 | `	GenBlock *pTry;` |
|         - | 12278 | `	VmInstr *pInstr;` |
|       103 | 12279 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12280 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12281 | `	sxi32 rc;` |
|       103 | 12282 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12283 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       103 | 12284 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       103 | 12285 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       103 | 12286 | `	pTry->pUserData = pException;` |
|       103 | 12287 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       103 | 12288 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       103 | 12289 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       103 | 12290 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       103 | 12291 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       103 | 12292 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12293 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       103 | 12294 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       103 | 12295 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       103 | 12296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       103 | 12297 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12298 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       103 | 12299 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12300 | `	/* Catch clauses (inline) */` |
|       103 | 12301 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        98 | 12302 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        59 | 12303 | `		sxu32 k = 0;` |
|        81 | 12304 | `		for(;;){` |
|         - | 12305 | `			ph7_exception_block sCatch;` |
|         - | 12306 | `			GenBlock *pCatchBlk;` |
|       113 | 12307 | `			sxu32 idxJmp = 0;` |
|       108 | 12308 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       104 | 12309 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        32 | 12310 | `				break;` |
|         - | 12311 | `			}` |
|        59 | 12312 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        59 | 12313 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12314 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        59 | 12315 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        59 | 12316 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        59 | 12317 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        59 | 12318 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12319 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12320 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12321 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        59 | 12322 | `			pCatchBlk->pUserData = pException;` |
|        59 | 12323 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        59 | 12324 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12325 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        59 | 12326 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12327 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12328 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        59 | 12329 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        59 | 12330 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        59 | 12331 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        59 | 12332 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        59 | 12333 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        59 | 12334 | `			k++;` |
|         5 | 12335 | `		}` |
|        27 | 12336 | `	}` |
|         - | 12337 | `	/* Finally (inline) */` |
|       103 | 12338 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12339 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12340 | `		GenBlock *pFinBlk;` |
|        52 | 12341 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12342 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12343 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12344 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12345 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12346 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12347 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12348 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12349 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12350 | `		pException->iHasFinally = 1;` |
|        24 | 12351 | `	}` |
|       103 | 12352 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       103 | 12353 | `	pException->iInlined = 1;` |
|         - | 12354 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12355 | `	{` |
|       103 | 12356 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12357 | `		sxu32 *aJ; sxu32 n;` |
|       103 | 12358 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       103 | 12359 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       103 | 12360 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       157 | 12361 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        59 | 12362 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        59 | 12363 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        32 | 12364 | `		}` |
|         - | 12365 | `	}` |
|       103 | 12366 | `	SySetRelease(&aCatchJmp);` |
|       103 | 12367 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12368 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12369 | `	}` |
|       103 | 12370 | `	return SXRET_OK;` |
|        54 | 12371 | `}` |
|         - | 12372 | `/*` |
|         - | 12373 | ` * Compile a 'catch' block.` |
|         - | 12374 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12375 | ` * an object containing the exception information.` |
|         - | 12376 | ` */` |
|     24258 | 12377 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12378 | `{` |
|     24263 | 12379 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12380 | `	ph7_exception_block sCatch;` |
|         - | 12381 | `	SySet *pInstrContainer;` |
|         - | 12382 | `	SyString sClassName;` |
|         - | 12383 | `	GenBlock *pCatch;` |
|         - | 12384 | `	SyToken *pToken;` |
|         - | 12385 | `	SyString *pName;` |
|         - | 12386 | `	char *zDup;` |
|         - | 12387 | `	sxi32 rc;` |
|     24263 | 12388 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12389 | `	/* Zero the structure */` |
|     24263 | 12390 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12391 | `	/* Initialize fields */` |
|     24263 | 12392 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24263 | 12393 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24263 | 12394 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12395 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12396 | `			pToken = pGen->pIn;` |
|       ! 0 | 12397 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12398 | `				pToken--;` |
|       ! 0 | 12399 | `			}` |
|       ! 0 | 12400 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12401 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12402 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12403 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12404 | `				return SXERR_ABORT;` |
|         - | 12405 | `			}` |
|       ! 0 | 12406 | `			return SXERR_INVALID;` |
|         - | 12407 | `	}` |
|         - | 12408 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24263 | 12409 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12143 | 12410 | `	for(;;){` |
|         - | 12411 | `		SyBlob sResolved;` |
|     24291 | 12412 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24291 | 12413 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12414 | `			SyBlobRelease(&sResolved);` |
|         6 | 12415 | `			pToken = pGen->pIn;` |
|         6 | 12416 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12417 | `				pToken--;` |
|       ! 0 | 12418 | `			}` |
|         8 | 12419 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12420 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12421 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12422 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12423 | `				return SXERR_ABORT;` |
|         - | 12424 | `			}` |
|         6 | 12425 | `			return SXERR_INVALID;` |
|         - | 12426 | `		}` |
|         - | 12427 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12428 | `		 * transient SyBlob allocation. */` |
|     36428 | 12429 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24282 | 12430 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24287 | 12431 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24287 | 12432 | `		SyBlobRelease(&sResolved);` |
|     24287 | 12433 | `		if( zDup == 0 ){` |
|       ! 0 | 12434 | `			goto Mem;` |
|         - | 12435 | `		}` |
|     24287 | 12436 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24287 | 12437 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12438 | `			goto Mem;` |
|         - | 12439 | `		}` |
|         - | 12440 | `		/* Check for '\|' (multi-catch separator) */` |
|     24282 | 12441 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24282 | 12442 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        33 | 12443 | `			pGen->pIn->sData.nByte == 1 &&` |
|        28 | 12444 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        30 | 12445 | `			pGen->pIn++; /* Consume the '\|' */` |
|        30 | 12446 | `			continue;` |
|         - | 12447 | `		}` |
|     24259 | 12448 | `		break;` |
|       ! 0 | 12449 | `	}` |
|     24254 | 12450 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24259 | 12451 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12452 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12453 | `			pToken = pGen->pIn;` |
|       ! 0 | 12454 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12455 | `				pToken--;` |
|       ! 0 | 12456 | `			}` |
|       ! 0 | 12457 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12458 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12459 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12460 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12461 | `				return SXERR_ABORT;` |
|         - | 12462 | `			}` |
|       ! 0 | 12463 | `			return SXERR_INVALID;` |
|         - | 12464 | `	}` |
|     24259 | 12465 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12466 | `	/* Duplicate instance name */` |
|     24259 | 12467 | `	pName = &pGen->pIn->sData;` |
|     24259 | 12468 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24259 | 12469 | `	if( zDup == 0 ){` |
|       ! 0 | 12470 | `		goto Mem;` |
|         - | 12471 | `	}` |
|     24259 | 12472 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24259 | 12473 | `	pGen->pIn++;` |
|     24259 | 12474 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12475 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12476 | `		pToken = pGen->pIn;` |
|       ! 0 | 12477 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12478 | `			pToken--;` |
|       ! 0 | 12479 | `		}` |
|       ! 0 | 12480 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12481 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12482 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12483 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12484 | `			return SXERR_ABORT;` |
|         - | 12485 | `		}` |
|       ! 0 | 12486 | `		return SXERR_INVALID;` |
|         - | 12487 | `	}` |
|         - | 12488 | `	/* Compile the block */` |
|     24259 | 12489 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12490 | `	/* Create the catch block */` |
|     24259 | 12491 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24259 | 12492 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12493 | `		return SXERR_ABORT;` |
|         - | 12494 | `	}` |
|         - | 12495 | `	/* Swap bytecode container */` |
|     24259 | 12496 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24259 | 12497 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12498 | `	/* Compile the block */` |
|     24259 | 12499 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12500 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24259 | 12501 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12502 | `	/* Emit the DONE instruction */` |
|     24259 | 12503 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12504 | `	/* Leave the block */` |
|     24259 | 12505 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12506 | `	/* Restore the default container */` |
|     24259 | 12507 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12508 | `	/* Install the catch block */` |
|     24259 | 12509 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24259 | 12510 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12511 | `		goto Mem;` |
|         - | 12512 | `	}` |
|     24259 | 12513 | `	return SXRET_OK;` |
|       ! 0 | 12514 | `Mem:` |
|       ! 0 | 12515 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12516 | `	return SXERR_ABORT;` |
|     12134 | 12517 | `}` |
|         - | 12518 | `/*` |
|         - | 12519 | ` * Compile a 'try' block.` |
|         - | 12520 | ` * A function using an exception should be in a "try" block.` |
|         - | 12521 | ` * If the exception does not trigger, the code will continue` |
|         - | 12522 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12523 | ` * is "thrown".` |
|         - | 12524 | ` */` |
|     24414 | 12525 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12526 | `{` |
|         - | 12527 | `	ph7_exception *pException;` |
|     24419 | 12528 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12529 | `	GenBlock *pTry;` |
|         - | 12530 | `	sxu32 nJmpIdx;` |
|         - | 12531 | `	sxi32 rc;` |
|         - | 12532 | `	/* Create the exception container */` |
|     24419 | 12533 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24419 | 12534 | `	if( pException == 0 ){` |
|       ! 0 | 12535 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12536 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12537 | `		return SXERR_ABORT;` |
|         - | 12538 | `	}` |
|         - | 12539 | `	/* Zero the structure */` |
|     24419 | 12540 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12541 | `	/* Initialize fields */` |
|     24419 | 12542 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24419 | 12543 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24419 | 12544 | `	pException->iHasFinally = 0;` |
|     24419 | 12545 | `	pException->iFinallyDone = 0;` |
|     24419 | 12546 | `	pException->pVm = pGen->pVm;` |
|         - | 12547 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12548 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12549 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12550 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12551 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12552 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24419 | 12553 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       103 | 12554 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12555 | `	}` |
|         - | 12556 | `	/* Create the try block */` |
|     24321 | 12557 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24321 | 12558 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12559 | `		return SXERR_ABORT;` |
|         - | 12560 | `	}` |
|         - | 12561 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24321 | 12562 | `	pTry->pUserData = pException;` |
|         - | 12563 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24321 | 12564 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12565 | `	/* Fix the jump later when the destination is resolved */` |
|     24321 | 12566 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24321 | 12567 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12568 | `	/* Compile the block */` |
|     24321 | 12569 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24321 | 12570 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12571 | `		return SXERR_ABORT;` |
|         - | 12572 | `	}` |
|         - | 12573 | `	/* Fix forward jumps now the destination is resolved */` |
|     24321 | 12574 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12575 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24321 | 12576 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12577 | `	/* Leave the block */` |
|     24321 | 12578 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12579 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24321 | 12580 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24314 | 12581 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12582 | `		/* Compile one or more catch blocks */` |
|     24254 | 12583 | `		for(;;){` |
|     48508 | 12584 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36441 | 12585 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12130 | 12586 | `					break;` |
|         - | 12587 | `			}` |
|     24263 | 12588 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24263 | 12589 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12590 | `				return SXERR_ABORT;` |
|         - | 12591 | `			}` |
|         5 | 12592 | `		}` |
|     12125 | 12593 | `	}` |
|         - | 12594 | `	/* Compile optional finally block */` |
|     24321 | 12595 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       728 | 12596 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12597 | `		SySet *pInstrContainer;` |
|         - | 12598 | `		GenBlock *pFinBlock;` |
|       129 | 12599 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12600 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12601 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12602 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12603 | `			return SXERR_ABORT;` |
|         - | 12604 | `		}` |
|         - | 12605 | `		/* Swap bytecode container */` |
|       129 | 12606 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12607 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12608 | `		/* Compile the finally body */` |
|       129 | 12609 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12610 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12611 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12612 | `			return SXERR_ABORT;` |
|         - | 12613 | `		}` |
|         - | 12614 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12615 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12616 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12617 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12618 | `		/* Leave the block */` |
|       129 | 12619 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12620 | `		/* Restore the default container */` |
|       129 | 12621 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12622 | `		pException->iHasFinally = 1;` |
|        62 | 12623 | `	}` |
|         - | 12624 | `	/* Must have at least one catch or finally */` |
|     24321 | 12625 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12626 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12627 | `			"Cannot use try without catch or finally");` |
|         8 | 12628 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12629 | `			return SXERR_ABORT;` |
|         - | 12630 | `		}` |
|         3 | 12631 | `	}` |
|     24321 | 12632 | `	return SXRET_OK;` |
|     12212 | 12633 | `}` |
|         - | 12634 | `/*` |
|         - | 12635 | ` * Compile a switch block.` |
|         - | 12636 | ` *  (See block-comment below for more information)` |
|         - | 12637 | ` */` |
|       112 | 12638 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12639 | `{` |
|       117 | 12640 | `	sxi32 rc = SXRET_OK;` |
|       117 | 12641 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12642 | `		/* Unexpected token */` |
|       ! 0 | 12643 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12644 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12645 | `			return SXERR_ABORT;` |
|         - | 12646 | `		}` |
|       ! 0 | 12647 | `		pGen->pIn++;` |
|       ! 0 | 12648 | `	}` |
|       117 | 12649 | `	pGen->pIn++;` |
|         - | 12650 | `	/* First instruction to execute in this block. */` |
|       117 | 12651 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12652 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12653 | `	 * or the '}' token */` |
|       206 | 12654 | `	for(;;){` |
|       417 | 12655 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12656 | `			/* No more input to process */` |
|       ! 0 | 12657 | `			break;` |
|         - | 12658 | `		}` |
|       417 | 12659 | `		rc = SXRET_OK;` |
|       417 | 12660 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        85 | 12661 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|        31 | 12662 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12663 | `					/* Unexpected token */` |
|       ! 0 | 12664 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12665 | `						&pGen->pIn->sData);` |
|       ! 0 | 12666 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12667 | `						return SXERR_ABORT;` |
|         - | 12668 | `					}` |
|         - | 12669 | `					/* FALL THROUGH */` |
|       ! 0 | 12670 | `				}` |
|        31 | 12671 | `				rc = SXERR_EOF;` |
|        31 | 12672 | `				break;` |
|         - | 12673 | `			}` |
|        32 | 12674 | `		}else{` |
|         - | 12675 | `			sxi32 nKwrd;` |
|         - | 12676 | `			/* Extract the keyword */` |
|       337 | 12677 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       337 | 12678 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|        47 | 12679 | `				break;` |
|         - | 12680 | `			}` |
|       253 | 12681 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12682 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12683 | `					/* Unexpected token */` |
|       ! 0 | 12684 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12685 | `						&pGen->pIn->sData);` |
|       ! 0 | 12686 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12687 | `						return SXERR_ABORT;` |
|         - | 12688 | `					}` |
|         - | 12689 | `					/* FALL THROUGH */` |
|       ! 0 | 12690 | `				}` |
|         - | 12691 | `				/* Block compiled */` |
|         3 | 12692 | `				break;` |
|         - | 12693 | `			}` |
|         - | 12694 | `		}` |
|         - | 12695 | `		/* Compile block */` |
|       305 | 12696 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       305 | 12697 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12698 | `			return SXERR_ABORT;` |
|         - | 12699 | `		}` |
|         5 | 12700 | `	}` |
|       117 | 12701 | `	return rc;` |
|        61 | 12702 | `}` |
|         - | 12703 | `/*` |
|         - | 12704 | ` * Compile a case eXpression.` |
|         - | 12705 | ` *  (See block-comment below for more information)` |
|         - | 12706 | ` */` |
|        92 | 12707 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12708 | `{` |
|         - | 12709 | `	SySet *pInstrContainer;` |
|         - | 12710 | `	SyToken *pEnd,*pTmp;` |
|        97 | 12711 | `	sxi32 iNest = 0;` |
|         - | 12712 | `	sxi32 rc;` |
|         - | 12713 | `	/* Delimit the expression */` |
|        97 | 12714 | `	pEnd = pGen->pIn;` |
|       197 | 12715 | `	while( pEnd < pGen->pEnd ){` |
|       197 | 12716 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12717 | `			/* Increment nesting level */` |
|         3 | 12718 | `			iNest++;` |
|       196 | 12719 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12720 | `			/* Decrement nesting level */` |
|         3 | 12721 | `			iNest--;` |
|       194 | 12722 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|        97 | 12723 | `			break;` |
|         - | 12724 | `		}` |
|       105 | 12725 | `		pEnd++;` |
|         5 | 12726 | `	}` |
|        97 | 12727 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12728 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12729 | `		if( rc == SXERR_ABORT ){` |
|         - | 12730 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12731 | `			return SXERR_ABORT;` |
|         - | 12732 | `		}` |
|       ! 0 | 12733 | `	}` |
|         - | 12734 | `	/* Swap token stream */` |
|        97 | 12735 | `	pTmp = pGen->pEnd;` |
|        97 | 12736 | `	pGen->pEnd = pEnd;` |
|        97 | 12737 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        97 | 12738 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|        97 | 12739 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12740 | `	/* Emit the done instruction */` |
|        97 | 12741 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        97 | 12742 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12743 | `	/* Update token stream */` |
|        97 | 12744 | `	pGen->pIn  = pEnd;` |
|        97 | 12745 | `	pGen->pEnd = pTmp;` |
|        97 | 12746 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12747 | `		return SXERR_ABORT;` |
|         - | 12748 | `	}` |
|        97 | 12749 | `	return SXRET_OK;` |
|        51 | 12750 | `}` |
|         - | 12751 | `/*` |
|         - | 12752 | ` * Compile the smart switch statement.` |
|         - | 12753 | ` * According to the PHP language reference manual` |
|         - | 12754 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12755 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12756 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12757 | ` *  This is exactly what the switch statement is for.` |
|         - | 12758 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12759 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12760 | ` *  of the outer loop, use continue 2.` |
|         - | 12761 | ` *  Note that switch/case does loose comparision.` |
|         - | 12762 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12763 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12764 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12765 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12766 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12767 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12768 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12769 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12770 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12771 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12772 | ` *  list for the next case.` |
|         - | 12773 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12774 | ` *  or floating-point numbers and strings.` |
|         - | 12775 | ` */` |
|        28 | 12776 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12777 | `{` |
|         - | 12778 | `	GenBlock *pSwitchBlock;` |
|         - | 12779 | `	SyToken *pTmp,*pEnd;` |
|         - | 12780 | `	ph7_switch *pSwitch;` |
|         - | 12781 | `	sxu32 nToken;` |
|         - | 12782 | `	sxu32 nLine;` |
|         - | 12783 | `	sxi32 rc;` |
|        33 | 12784 | `	nLine = pGen->pIn->nLine;` |
|         - | 12785 | `	/* Jump the 'switch' keyword */` |
|        33 | 12786 | `	pGen->pIn++;` |
|        33 | 12787 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12788 | `		/* Syntax error */` |
|       ! 0 | 12789 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12790 | `		if( rc == SXERR_ABORT ){` |
|         - | 12791 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12792 | `			return SXERR_ABORT;` |
|         - | 12793 | `		}` |
|       ! 0 | 12794 | `		goto Synchronize;` |
|         - | 12795 | `	}` |
|         - | 12796 | `	/* Jump the left parenthesis '(' */` |
|        33 | 12797 | `	pGen->pIn++;` |
|        33 | 12798 | `	pEnd = 0; /* cc warning */` |
|         - | 12799 | `	/* Create the loop block */` |
|        47 | 12800 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|        14 | 12801 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|        33 | 12802 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12803 | `		return SXERR_ABORT;` |
|         - | 12804 | `	}` |
|         - | 12805 | `	/* Delimit the condition */` |
|        33 | 12806 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|        33 | 12807 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12808 | `		/* Empty expression */` |
|       ! 0 | 12809 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12810 | `		if( rc == SXERR_ABORT ){` |
|         - | 12811 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12812 | `			return SXERR_ABORT;` |
|         - | 12813 | `		}` |
|       ! 0 | 12814 | `	}` |
|         - | 12815 | `	/* Swap token streams */` |
|        33 | 12816 | `	pTmp = pGen->pEnd;` |
|        33 | 12817 | `	pGen->pEnd = pEnd;` |
|         - | 12818 | `	/* Compile the expression */` |
|        33 | 12819 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        33 | 12820 | `	if( rc == SXERR_ABORT ){` |
|         - | 12821 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12822 | `		return SXERR_ABORT;` |
|         - | 12823 | `	}` |
|         - | 12824 | `	/* Update token stream */` |
|        33 | 12825 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12826 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12827 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12828 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12829 | `			return SXERR_ABORT;` |
|         - | 12830 | `		}` |
|       ! 0 | 12831 | `		pGen->pIn++;` |
|       ! 0 | 12832 | `	}` |
|        33 | 12833 | `	pGen->pIn  = &pEnd[1];` |
|        33 | 12834 | `	pGen->pEnd = pTmp;` |
|        33 | 12835 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        28 | 12836 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12837 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12838 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12839 | `				pTmp--;` |
|       ! 0 | 12840 | `			}` |
|         - | 12841 | `			/* Unexpected token */` |
|       ! 0 | 12842 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12843 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12844 | `				return SXERR_ABORT;` |
|         - | 12845 | `			}` |
|       ! 0 | 12846 | `			goto Synchronize;` |
|         - | 12847 | `	}` |
|         - | 12848 | `	/* Set the delimiter token */` |
|        33 | 12849 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12850 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12851 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12852 | `	}else{` |
|        31 | 12853 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12854 | `	}` |
|        33 | 12855 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12856 | `	/* Create the switch blocks container */` |
|        33 | 12857 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|        33 | 12858 | `	if( pSwitch == 0 ){` |
|         - | 12859 | `		/* Abort compilation */` |
|       ! 0 | 12860 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12861 | `		return SXERR_ABORT;` |
|         - | 12862 | `	}` |
|         - | 12863 | `	/* Zero the structure */` |
|        33 | 12864 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 12865 | `	/* Initialize fields */` |
|        33 | 12866 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 12867 | `	/* Emit the switch instruction */` |
|        33 | 12868 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 12869 | `	/* Compile case blocks */` |
|       100 | 12870 | `	for(;;){` |
|         - | 12871 | `		sxu32 nKwrd;` |
|       119 | 12872 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12873 | `			/* No more input to process */` |
|       ! 0 | 12874 | `			break;` |
|         - | 12875 | `		}` |
|       119 | 12876 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 12877 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 12878 | `				/* Unexpected token */` |
|       ! 0 | 12879 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12880 | `					&pGen->pIn->sData);` |
|       ! 0 | 12881 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12882 | `					return SXERR_ABORT;` |
|         - | 12883 | `				}` |
|         - | 12884 | `				/* FALL THROUGH */` |
|       ! 0 | 12885 | `			}` |
|         - | 12886 | `			/* Block compiled */` |
|       ! 0 | 12887 | `			break;` |
|         - | 12888 | `		}` |
|         - | 12889 | `		/* Extract the keyword */` |
|       119 | 12890 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       119 | 12891 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12892 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 12893 | `				/* Unexpected token */` |
|       ! 0 | 12894 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12895 | `					&pGen->pIn->sData);` |
|       ! 0 | 12896 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12897 | `					return SXERR_ABORT;` |
|         - | 12898 | `				}` |
|         - | 12899 | `				/* FALL THROUGH */` |
|       ! 0 | 12900 | `			}` |
|         - | 12901 | `			/* Block compiled */` |
|         3 | 12902 | `			break;` |
|         - | 12903 | `		}` |
|       117 | 12904 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 12905 | `			/*` |
|         - | 12906 | `			 * Accroding to the PHP language reference manual` |
|         - | 12907 | `			 *  A special case is the default case. This case matches anything` |
|         - | 12908 | `			 *  that wasn't matched by the other cases.` |
|         - | 12909 | `			 */` |
|        25 | 12910 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 12911 | `				/* Default case already compiled */` |
|       ! 0 | 12912 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 12913 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12914 | `					return SXERR_ABORT;` |
|         - | 12915 | `				}` |
|       ! 0 | 12916 | `			}` |
|        25 | 12917 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 12918 | `			/* Compile the default block */` |
|        25 | 12919 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 12920 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12921 | `				return SXERR_ABORT;` |
|        25 | 12922 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 12923 | `				break;` |
|         1 | 12924 | `			}` |
|        98 | 12925 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 12926 | `			ph7_case_expr sCase;` |
|         - | 12927 | `			/* Standard case block */` |
|        97 | 12928 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 12929 | `			/* initialize the structure */` |
|        97 | 12930 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 12931 | `			/* Compile the case expression */` |
|        97 | 12932 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|        97 | 12933 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12934 | `				return SXERR_ABORT;` |
|         - | 12935 | `			}` |
|         - | 12936 | `			/* Compile the case block */` |
|        97 | 12937 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 12938 | `			/* Insert in the switch container */` |
|        97 | 12939 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|        97 | 12940 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12941 | `				return SXERR_ABORT;` |
|        97 | 12942 | `			}else if( rc == SXERR_EOF ){` |
|         9 | 12943 | `				break;` |
|         - | 12944 | `			}` |
|        47 | 12945 | `		}else{` |
|         - | 12946 | `			/* Unexpected token */` |
|       ! 0 | 12947 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12948 | `				&pGen->pIn->sData);` |
|       ! 0 | 12949 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12950 | `				return SXERR_ABORT;` |
|         - | 12951 | `			}` |
|       ! 0 | 12952 | `			break;` |
|         - | 12953 | `		}` |
|         5 | 12954 | `	}` |
|         - | 12955 | `	/* Fix all jumps now the destination is resolved */` |
|        33 | 12956 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|        33 | 12957 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12958 | `	/* Release the loop block */` |
|        33 | 12959 | `	GenStateLeaveBlock(pGen,0);` |
|        33 | 12960 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 12961 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|        33 | 12962 | `		pGen->pIn++;` |
|        14 | 12963 | `	}` |
|         - | 12964 | `	/* Statement successfully compiled */` |
|        33 | 12965 | `	return SXRET_OK;` |
|       ! 0 | 12966 | `Synchronize:` |
|         - | 12967 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 12968 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 12969 | `		pGen->pIn++;` |
|       ! 0 | 12970 | `	}` |
|       ! 0 | 12971 | `	return SXRET_OK;` |
|        19 | 12972 | `}` |
|         - | 12973 | `/*` |
|         - | 12974 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 12975 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 12976 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 12977 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 12978 | ` */` |
|         - | 12979 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 12980 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 12981 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 12982 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 12983 |  |
|         - | 12984 | `/*` |
|         - | 12985 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 12986 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 12987 | ` * patched entries from the pending set.` |
|         - | 12988 | ` */` |
|  39637032 | 12989 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 12990 | `{` |
|  39637037 | 12991 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 12992 | `	sxu32 nTarget;` |
|         - | 12993 | `	sxu32 *aIdx;` |
|         - | 12994 | `	sxu32 i;` |
|  39637037 | 12995 | `	if( nCur <= nBaseline ){` |
|  39636941 | 12996 | `		return;` |
|         - | 12997 | `	}` |
|       100 | 12998 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 12999 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 13000 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 13001 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 13002 | `		if( pInstr ){` |
|       108 | 13003 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 13004 | `		}` |
|        56 | 13005 | `	}` |
|       100 | 13006 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  19818521 | 13007 | `}` |
|         - | 13008 |  |
|         - | 13009 | `/*` |
|         - | 13010 | ` * By-reference out-parameters of builtin functions.` |
|         - | 13011 | ` *` |
|         - | 13012 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 13013 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 13014 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 13015 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 13016 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 13017 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 13018 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 13019 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 13020 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 13021 | ` * creates it" behaviour).` |
|         - | 13022 | ` *` |
|         - | 13023 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 13024 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 13025 | ` */` |
|   5384824 | 13026 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 13027 | `{` |
|         - | 13028 | `	static const struct {` |
|         - | 13029 | `		const char *zName;` |
|         - | 13030 | `		sxu32 nByte;` |
|         - | 13031 | `		sxu32 mask;` |
|         - | 13032 | `	} aByRef[] = {` |
|         - | 13033 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13034 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13035 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13036 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13037 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 13038 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 13039 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 13040 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 13041 | `	};` |
|         - | 13042 | `	sxu32 i;` |
|   5384829 | 13043 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1587537 | 13044 | `		return 0;` |
|         - | 13045 | `	}` |
|  33882551 | 13046 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  30123344 | 13047 | `		if( pName->nByte == aByRef[i].nByte` |
|  15741015 | 13048 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     38095 | 13049 | `			return aByRef[i].mask;` |
|         - | 13050 | `		}` |
|  15042632 | 13051 | `	}` |
|   3759207 | 13052 | `	return 0;` |
|   2692417 | 13053 | `}` |
|         - | 13054 | `/*` |
|         - | 13055 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 13056 | ` *` |
|         - | 13057 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 13058 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 13059 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 13060 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 13061 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 13062 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 13063 | ` */` |
|   5384824 | 13064 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 13065 | `{` |
|         - | 13066 | `	SyToken *p, *pEnd;` |
|   5384829 | 13067 | `	pOut->zString = 0;` |
|   5384829 | 13068 | `	pOut->nByte = 0;` |
|   5384829 | 13069 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 13070 | `		return;` |
|         - | 13071 | `	}` |
|   5384829 | 13072 | `	p = pLeft->pStart;` |
|   5384829 | 13073 | `	pEnd = pLeft->pEnd;` |
|         - | 13074 | `	/* Optional single leading namespace separator (absolute path). */` |
|   5384829 | 13075 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3829 | 13076 | `		p++;` |
|      1912 | 13077 | `	}` |
|   5384829 | 13078 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1587501 | 13079 | `		return;` |
|         - | 13080 | `	}` |
|         - | 13081 | `	/* Must be a single component: nothing follows the name token. */` |
|   3797333 | 13082 | `	if( p + 1 != pEnd ){` |
|        40 | 13083 | `		return;` |
|         - | 13084 | `	}` |
|   3797297 | 13085 | `	*pOut = p->sData;` |
|   2692417 | 13086 | `}` |
|         - | 13087 | `/*` |
|         - | 13088 | ` * Generate bytecode for a given expression tree.` |
|         - | 13089 | ` * If something goes wrong while generating bytecode` |
|         - | 13090 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 13091 | ` * this function takes care of generating the appropriate` |
|         - | 13092 | ` * error message.` |
|         - | 13093 | ` */` |
|  57262368 | 13094 | `static sxi32 GenStateEmitExprCode(` |
|         - | 13095 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 13096 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 13097 | `	sxi32 iFlags /* Control flags */` |
|         - | 13098 | `	)` |
|         5 | 13099 | `{` |
|         - | 13100 | `	VmInstr *pInstr;` |
|         - | 13101 | `	sxu32 nJmpIdx;` |
|  57262373 | 13102 | `	sxi32 iP1 = 0;` |
|  57262373 | 13103 | `	sxu32 iP2 = 0;` |
|  57262373 | 13104 | `	void *p3  = 0;` |
|         - | 13105 | `	sxi32 iVmOp;` |
|         - | 13106 | `	sxi32 rc;` |
|  57262373 | 13107 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  57262373 | 13108 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  57262373 | 13109 | `	sxu32 nRhsNsBase = 0;` |
|  57262373 | 13110 | `	if( pNode->xCode ){` |
|         - | 13111 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13112 | `		/* Compile node */` |
|  34249879 | 13113 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  34249879 | 13114 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  34249879 | 13115 | `		RE_SWAP_DELIMITER(pGen);` |
|  34249879 | 13116 | `		return rc;` |
|         - | 13117 | `	}` |
|  23012499 | 13118 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13119 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13120 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13121 | `		return SXERR_ABORT;` |
|         - | 13122 | `	}` |
|  23012499 | 13123 | `	iVmOp = pNode->pOp->iVmOp;` |
|  23012499 | 13124 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13125 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13126 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13127 | `		 * and later errors are still reported. */` |
|         3 | 13128 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13129 | `			"The (unset) cast is no longer supported");` |
|         3 | 13130 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13131 | `			return SXERR_ABORT;` |
|         - | 13132 | `		}` |
|         1 | 13133 | `	}` |
|  23012499 | 13134 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        91 | 13135 | `		sxu32 nJmp = 0;` |
|         - | 13136 | `		sxu32 nNcNsBase;` |
|         - | 13137 | `		VmInstr *pInstrFix;` |
|         - | 13138 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13139 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13140 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13141 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13142 | `		 * stack slot carries a writable nIdx. */` |
|        91 | 13143 | `		if( pNode->pRight ){` |
|        91 | 13144 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13145 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        91 | 13146 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13147 | `				return rc;` |
|         - | 13148 | `			}` |
|        91 | 13149 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13150 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13151 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13152 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13153 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13154 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13155 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13156 | `			 * cascade for the actual write path stays correct. */` |
|        91 | 13157 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        91 | 13158 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13159 | `				pInstrFix->iP2 = 3;` |
|        15 | 13160 | `			}` |
|        44 | 13161 | `		}` |
|         - | 13162 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        91 | 13163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13164 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        91 | 13165 | `		if( pNode->pLeft ){` |
|        91 | 13166 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13167 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        91 | 13168 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13169 | `				return rc;` |
|         - | 13170 | `			}` |
|        91 | 13171 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        44 | 13172 | `		}` |
|         - | 13173 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        91 | 13174 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13175 | `		/* Patch the short-circuit jump to land after the store. */` |
|        91 | 13176 | `		if( nJmp > 0 ){` |
|        91 | 13177 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        91 | 13178 | `			if( pInstrFix ){` |
|        91 | 13179 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        44 | 13180 | `			}` |
|        44 | 13181 | `		}` |
|        91 | 13182 | `		return SXRET_OK;` |
|         - | 13183 | `	}` |
|  23012411 | 13184 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13185 | `		sxu32 nJz,nJmp;` |
|         - | 13186 | `		sxu32 nTernaryNsBase;` |
|         - | 13187 | `		/* Ternary operator require special handling */` |
|         - | 13188 | `		/* Phase#1: Compile the condition */` |
|    367711 | 13189 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    367711 | 13190 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    367711 | 13191 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13192 | `			return rc;` |
|         - | 13193 | `		}` |
|         - | 13194 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13195 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13196 | `		 * condition expression, not leak past the ternary. */` |
|    367711 | 13197 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    367711 | 13198 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    367711 | 13199 | `		if( pNode->pLeft ){` |
|         - | 13200 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13201 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    363847 | 13202 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13203 | `			/* Phase#3: Compile the 'then' expression  */` |
|    363847 | 13204 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    363847 | 13205 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    363847 | 13206 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13207 | `				return rc;` |
|         - | 13208 | `			}` |
|    363847 | 13209 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    181926 | 13210 | `		}else{` |
|         - | 13211 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13212 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13213 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3869 | 13214 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3869 | 13215 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13216 | `		}` |
|         - | 13217 | `		/* Phase#4: Emit the unconditional jump */` |
|    367711 | 13218 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13219 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    367711 | 13220 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    367711 | 13221 | `		if( pInstr ){` |
|    367711 | 13222 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    183853 | 13223 | `		}` |
|    367711 | 13224 | `		if( !pNode->pLeft ){` |
|         - | 13225 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3869 | 13226 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1932 | 13227 | `		}` |
|         - | 13228 | `		/* Phase#6: Compile the 'else' expression */` |
|    367711 | 13229 | `		if( pNode->pRight ){` |
|    367711 | 13230 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    367711 | 13231 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    367711 | 13232 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13233 | `				return rc;` |
|         - | 13234 | `			}` |
|    367711 | 13235 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    183853 | 13236 | `		}` |
|    367711 | 13237 | `		if( nJmp > 0 ){` |
|         - | 13238 | `			/* Phase#7: Fix the unconditional jump */` |
|    367711 | 13239 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    367711 | 13240 | `			if( pInstr ){` |
|    367711 | 13241 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    183853 | 13242 | `			}` |
|    183853 | 13243 | `		}` |
|         - | 13244 | `		/* All done */` |
|    367711 | 13245 | `		return SXRET_OK;` |
|         - | 13246 | `	}` |
|  22644705 | 13247 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13248 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13249 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13250 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13251 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13252 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13253 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13254 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13255 | `		sxu32 nPipeNsBase;` |
|        27 | 13256 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13257 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13258 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13259 | `				"'\|>': Missing operand");` |
|       ! 0 | 13260 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13261 | `		}` |
|         - | 13262 | `		/* Argument: the LHS value. */` |
|        27 | 13263 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13264 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13265 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13266 | `			return rc;` |
|         - | 13267 | `		}` |
|        27 | 13268 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13269 | `		/* Callable: the RHS. */` |
|        27 | 13270 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13271 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13272 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13273 | `			return rc;` |
|         - | 13274 | `		}` |
|        27 | 13275 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13276 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13277 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13278 | `		return SXRET_OK;` |
|         - | 13279 | `	}` |
|  22644679 | 13280 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13281 | `	/* Generate code for the left tree */` |
|  22644679 | 13282 | `	if( pNode->pLeft ){` |
|  22621901 | 13283 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  22621901 | 13284 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13285 | `			ph7_expr_node **apNode;` |
|   5388941 | 13286 | `			int hasSpread = 0;` |
|   5388941 | 13287 | `			int hasNamed = 0;` |
|   5388941 | 13288 | `			int bAnySpread = 0;` |
|   5388941 | 13289 | `			sxu32 byRefMask = 0;` |
|         - | 13290 | `			sxi32 nArgs;` |
|         - | 13291 | `			sxi32 n;` |
|         - | 13292 | `			/* Recurse and generate bytecodes for function arguments */` |
|   5388941 | 13293 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5388941 | 13294 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13295 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13296 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13297 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   5388941 | 13298 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13299 | `				bFcc = 1;` |
|        81 | 13300 | `				nArgs = 0;` |
|        40 | 13301 | `			}` |
|         - | 13302 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13303 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13304 | `			{` |
|   5388941 | 13305 | `				int seenNamed = 0;` |
|   5388941 | 13306 | `				int seenSpread = 0;` |
|  11004091 | 13307 | `				for( n = 0; n < nArgs; ++n ){` |
|   5615157 | 13308 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      3985 | 13309 | `						bAnySpread = 1;` |
|      3985 | 13310 | `						seenSpread = 1;` |
|      3985 | 13311 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13312 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13313 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13314 | `							return SXERR_SYNTAX;` |
|         5 | 13315 | `						}` |
|   5613167 | 13316 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13317 | `						seenNamed = 1;` |
|       289 | 13318 | `						hasNamed = 1;` |
|   5611035 | 13319 | `					}else if( seenNamed ){` |
|         3 | 13320 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13321 | `							"Cannot use positional argument after named argument");` |
|         3 | 13322 | `						return SXERR_SYNTAX;` |
|   5610891 | 13323 | `					}else if( seenSpread ){` |
|       ! 0 | 13324 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13325 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13326 | `						return SXERR_SYNTAX;` |
|         - | 13327 | `					}` |
|   2807580 | 13328 | `				}` |
|         - | 13329 | `			}` |
|         - | 13330 | `			/* Read-only load */` |
|   5388939 | 13331 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13332 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13333 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13334 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13335 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   5388939 | 13336 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   5388939 | 13337 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   5388934 | 13338 | `				if( pCallName->nByte == 5` |
|   3029852 | 13339 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    273697 | 13340 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5252093 | 13341 | `				}else if( pCallName->nByte == 5` |
|   2756160 | 13342 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       107 | 13343 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        51 | 13344 | `				}` |
|         - | 13345 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13346 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13347 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13348 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13349 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13350 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   5388939 | 13351 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13352 | `					SyString sBuiltin;` |
|   5384829 | 13353 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   5384829 | 13354 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   2692412 | 13355 | `				}` |
|   2694467 | 13356 | `			}` |
|  11004087 | 13357 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   5615153 | 13358 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   5615153 | 13359 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13360 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13361 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13362 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13363 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13364 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13365 | `				 * (iP1=0 either way). */` |
|   5615153 | 13366 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     26645 | 13367 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     26645 | 13368 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     13320 | 13369 | `				}` |
|   5615153 | 13370 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   5615153 | 13371 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13372 | `					return rc;` |
|         - | 13373 | `				}` |
|         - | 13374 | `				/* Each argument is an independent nullsafe scope. */` |
|   5615153 | 13375 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   5615153 | 13376 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13377 | `					/* Emit spread opcode to unpack this array argument */` |
|      3985 | 13378 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      3985 | 13379 | `					hasSpread = 1;` |
|      1990 | 13380 | `				}` |
|   2807579 | 13381 | `			}` |
|         - | 13382 | `			/* Total number of given arguments */` |
|   5388939 | 13383 | `			iP1 = nArgs;` |
|   5388939 | 13384 | `			iP2 = hasSpread;` |
|         - | 13385 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13386 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   5388939 | 13387 | `			if( hasNamed ){` |
|       178 | 13388 | `				sxu32 nStrBytes = 0;` |
|         - | 13389 | `				char *zBuf;` |
|       534 | 13390 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13391 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13392 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13393 | `					}` |
|       182 | 13394 | `				}` |
|         - | 13395 | `				{` |
|       178 | 13396 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13397 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13398 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13399 | `				if( pMap ){` |
|       178 | 13400 | `					SyZero(pMap, mapSize);` |
|       178 | 13401 | `					pMap->bHasNamed = 1;` |
|       178 | 13402 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13403 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13404 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13405 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13406 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13407 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13408 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13409 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13410 | `							zBuf += nb;` |
|       141 | 13411 | `						}` |
|         - | 13412 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13413 | `					}` |
|       178 | 13414 | `					p3 = (void *)pMap;` |
|        87 | 13415 | `				}` |
|         - | 13416 | `				}` |
|        87 | 13417 | `			}` |
|         - | 13418 | `			/* Remove stale flags now */` |
|   5388939 | 13419 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   2694467 | 13420 | `		}` |
|         - | 13421 | `		{` |
|         - | 13422 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13423 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13424 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13425 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13426 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13427 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13428 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13429 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  22621899 | 13430 | `			sxi32 iLeftFlags = iFlags;` |
|  22621894 | 13431 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  18611884 | 13432 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   7300963 | 13433 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   6194239 | 13434 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2384769 | 13435 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1192382 | 13436 | `			}` |
|         - | 13437 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13438 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13439 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13440 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13441 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13442 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13443 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  22621894 | 13444 | `			if( pNode->pOp` |
|  31682557 | 13445 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  20371657 | 13446 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  18121368 | 13447 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4858289 | 13448 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2429142 | 13449 | `			}` |
|         - | 13450 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13451 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13452 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13453 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13454 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13455 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  22621894 | 13456 | `			if( pNode->pOp` |
|  22621899 | 13457 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    144745 | 13458 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     72370 | 13459 | `			}` |
|  22621899 | 13460 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13461 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13462 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|       211 | 13463 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|       103 | 13464 | `			}` |
|  22621899 | 13465 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13466 | `		}` |
|  22621899 | 13467 | `		if( rc != SXRET_OK ){` |
|        34 | 13468 | `			return rc;` |
|         - | 13469 | `		}` |
|  22621869 | 13470 | `		if( !bIsChainOp ){` |
|         - | 13471 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13472 | `			 * target the end of that LHS chain, which is right here. */` |
|   9877567 | 13473 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   4938781 | 13474 | `		}` |
|  22621869 | 13475 | `		if( iVmOp == PH7_OP_CALL ){` |
|   5388939 | 13476 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5388939 | 13477 | `			if( pInstr ){` |
|   5388939 | 13478 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   3797573 | 13479 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13480 | `					sxu32 nQual;` |
|   3797573 | 13481 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13482 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13483 | `					 * so the later NEW handler (if any) can see it. */` |
|   3797573 | 13484 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13485 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13486 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13487 | `					 * imports — class imports must NOT affect function` |
|         - | 13488 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13489 | `					 * before NEW; we store the original literal index in the` |
|         - | 13490 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13491 | `					 * the unqualified name and re-qualify with class imports. */` |
|   3797573 | 13492 | `					if( bAbsolute ){` |
|      3829 | 13493 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1917 | 13494 | `					}else{` |
|   3793749 | 13495 | `						int fromImport = 0;` |
|   3793749 | 13496 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   3793749 | 13497 | `						pInstr->iP2 = (sxi32)nQual;` |
|   3793749 | 13498 | `						if( nQual != nOrig ){` |
|         - | 13499 | `							/* Record the original literal index in the arg map` |
|         - | 13500 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13501 | `							 * flag) so the NEW handler can recover the` |
|         - | 13502 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13503 | `							 * imports. */` |
|        77 | 13504 | `							if( p3 == 0 ){` |
|        77 | 13505 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        72 | 13506 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        77 | 13507 | `								if( pMap ){` |
|        77 | 13508 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        77 | 13509 | `									p3 = (void *)pMap;` |
|        36 | 13510 | `								}` |
|        36 | 13511 | `							}` |
|        77 | 13512 | `							if( p3 ){` |
|        77 | 13513 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        77 | 13514 | `								if( !fromImport ){` |
|         - | 13515 | `									/* Mark as namespace-qualified */` |
|        67 | 13516 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        31 | 13517 | `								}` |
|        36 | 13518 | `							}` |
|        36 | 13519 | `						}` |
|         5 | 13520 | `					}` |
|   3490155 | 13521 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13522 | `					/* Method call,flag that */` |
|   1571779 | 13523 | `					pInstr->iP2 = 1;` |
|    785887 | 13524 | `				}` |
|   2694472 | 13525 | `			}` |
|  19927402 | 13526 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13527 | `			ph7_expr_node **apNode;` |
|         - | 13528 | `			sxi32 n;` |
|   2497089 | 13529 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13530 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13531 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13532 | `			/* Recurse and generate bytecodes for array index */` |
|   2497089 | 13533 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   4838317 | 13534 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2341233 | 13535 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2341233 | 13536 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2341233 | 13537 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13538 | `					return rc;` |
|         - | 13539 | `				}` |
|         - | 13540 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2341233 | 13541 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1170619 | 13542 | `			}` |
|   2497089 | 13543 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2341233 | 13544 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1170614 | 13545 | `			}` |
|   2497089 | 13546 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13547 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    311541 | 13548 | `				iP2 = 4;` |
|   2341321 | 13549 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13550 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13551 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     22857 | 13552 | `				iP2 = 5;` |
|   2174127 | 13553 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13554 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13555 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13556 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 13557 | `				iP2 = 6;` |
|   2162689 | 13558 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13559 | `				/* Create an empty entry when the desired index is not found */` |
|    365163 | 13560 | `				iP2 = 1;` |
|    182584 | 13561 | `			}` |
|  15984393 | 13562 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13563 | `			/* POP the left node */` |
|         5 | 13564 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13565 | `		}` |
|  11310932 | 13566 | `	}` |
|  22644647 | 13567 | `	rc = SXRET_OK;` |
|  22644647 | 13568 | `	nJmpIdx = 0;` |
|         - | 13569 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13570 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13571 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  22644647 | 13572 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    380489 | 13573 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    380489 | 13574 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    380489 | 13575 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    380489 | 13576 | `			int isSpecial = 0;` |
|    380489 | 13577 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    334905 | 13578 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    334905 | 13579 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    334900 | 13580 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    304472 | 13581 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    165524 | 13582 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    102641 | 13583 | `					isSpecial = 1;` |
|     51318 | 13584 | `				}` |
|    178846 | 13585 | `			}` |
|    403281 | 13586 | `			pInstr->iP1 = 0;` |
|    403281 | 13587 | `			if( !isSpecial ){` |
|    255061 | 13588 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    127528 | 13589 | `			}` |
|         - | 13590 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13591 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    357697 | 13592 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    255061 | 13593 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    255061 | 13594 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        60 | 13595 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        62 | 13596 | `					return SXRET_OK;` |
|         - | 13597 | `				}` |
|    127499 | 13598 | `			}` |
|    178817 | 13599 | `		}` |
|    224380 | 13600 | `	}` |
|         - | 13601 | `	/* Generate code for the right tree */` |
|  22621811 | 13602 | `	if( pNode->pRight ){` |
|  13003319 | 13603 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13604 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    311793 | 13605 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  12847425 | 13606 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13607 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    212751 | 13608 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  12585158 | 13609 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13610 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     53299 | 13611 | `			iVmOp = 0; /* No binary operator to emit */` |
|     53299 | 13612 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  12452190 | 13613 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13614 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13615 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13616 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13617 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13618 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13619 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13620 | `			sxu32 nNsJmp = 0;` |
|       108 | 13621 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13622 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  12425439 | 13623 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13624 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13625 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13626 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   3948097 | 13627 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   1974046 | 13628 | `		}` |
|  13003319 | 13629 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  13003319 | 13630 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  13003319 | 13631 | `		if( !bIsChainOp ){` |
|         - | 13632 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13633 | `			 * operator instruction is emitted. */` |
|   8145093 | 13634 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   4072544 | 13635 | `		}` |
|  13003319 | 13636 | `		if( iVmOp == PH7_OP_STORE ){` |
|   3537803 | 13637 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   3537766 | 13638 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13639 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13640 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13641 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13642 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13643 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13644 | `				 */` |
|        91 | 13645 | `				iVmOp = 0;` |
|   3537760 | 13646 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   3537717 | 13647 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13648 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    763709 | 13649 | `					iP2 = 1;` |
|    381857 | 13650 | `				}else{` |
|   2774013 | 13651 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13652 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    346085 | 13653 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    346085 | 13654 | `						iP1 = pInstr->iP1;` |
|    173045 | 13655 | `					}else{` |
|   2427933 | 13656 | `						p3 = pInstr->p3;` |
|         - | 13657 | `					}` |
|         - | 13658 | `					/* POP the last dynamic load instruction */` |
|   2774013 | 13659 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13660 | `				}` |
|   1768861 | 13661 | `			}` |
|  11234420 | 13662 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13663 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13664 | `			if( pInstr ){` |
|        63 | 13665 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13666 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13667 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13668 | `					 */` |
|        19 | 13669 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13670 | `					iP1 = pInstr->iP1;` |
|        19 | 13671 | `					iP2 = pInstr->iP2;` |
|        19 | 13672 | `					p3  = pInstr->p3;` |
|        10 | 13673 | `				}else{` |
|        45 | 13674 | `					p3 = pInstr->p3;` |
|         - | 13675 | `				}` |
|        30 | 13676 | `			}` |
|        30 | 13677 | `		}` |
|   6501657 | 13678 | `	}` |
|  22621806 | 13679 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    354640 | 13680 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13681 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13682 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13683 | `		iVmOp = 0;` |
|        14 | 13684 | `	}` |
|  22621811 | 13685 | `	if( iVmOp > 0 ){` |
|  22568399 | 13686 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    144745 | 13687 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13688 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     11425 | 13689 | `				iP1 = 1;` |
|      5715 | 13690 | `			}` |
|  22496029 | 13691 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13692 | `			/* Namespace-qualify the class name for NEW */ {` |
|    708923 | 13693 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    708923 | 13694 | `				VmInstr *pCallInstr = 0;` |
|    708923 | 13695 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    708627 | 13696 | `					pCallInstr = pPeek;` |
|    708627 | 13697 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    354311 | 13698 | `				}` |
|    708923 | 13699 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    693735 | 13700 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13701 | `					sxu32 nLitForClass;` |
|    693735 | 13702 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13703 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13704 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13705 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13706 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13707 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13708 | `					 * with class imports. */` |
|    693735 | 13709 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        37 | 13710 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        21 | 13711 | `					}else{` |
|    693703 | 13712 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13713 | `					}` |
|    693735 | 13714 | `					pPeek->iP1 = 0;` |
|    693735 | 13715 | `					if( !bAbsolute ){` |
|    689915 | 13716 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    344960 | 13717 | `					}else{` |
|      3825 | 13718 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13719 | `					}` |
|    346865 | 13720 | `				}` |
|         - | 13721 | `			}` |
|    708923 | 13722 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    708923 | 13723 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13724 | `				VmInstr *pPrev;` |
|    708627 | 13725 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    708627 | 13726 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13727 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13728 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13729 | `					 * accumulator exactly like OP_CALL would have). */` |
|    708627 | 13730 | `					iP1 = pInstr->iP1;` |
|    708627 | 13731 | `					iP2 = pInstr->iP2;` |
|    708627 | 13732 | `					if( pInstr->p3 ){` |
|        47 | 13733 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        21 | 13734 | `					}` |
|    708627 | 13735 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    354311 | 13736 | `				}` |
|    354316 | 13737 | `			}` |
|  22069200 | 13738 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13739 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13740 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     68585 | 13741 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     68585 | 13742 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     68585 | 13743 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     68585 | 13744 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     68585 | 13745 | `				int isSpecialIs = 0;` |
|     68585 | 13746 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     68585 | 13747 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     68585 | 13748 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     68580 | 13749 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     68583 | 13750 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     34290 | 13751 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13752 | `						isSpecialIs = 1;` |
|         5 | 13753 | `					}` |
|     34290 | 13754 | `				}` |
|     68585 | 13755 | `				pInstr->iP1 = 0;` |
|     68585 | 13756 | `				if( !isSpecialIs && !bAbsolute ){` |
|     68565 | 13757 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     34280 | 13758 | `				}` |
|     34295 | 13759 | `			}` |
|  21680451 | 13760 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13761 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13762 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13763 | `			 * should not trigger constant lookup. */` |
|   4858231 | 13764 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4858231 | 13765 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4626597 | 13766 | `				pInstr->iP1 = 0;` |
|   2313296 | 13767 | `			}` |
|   4858231 | 13768 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13769 | `				/* Static member access,remember that */` |
|    357653 | 13770 | `				iP1 = 1;` |
|    357653 | 13771 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    357653 | 13772 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    227827 | 13773 | `					p3 = pInstr->p3;` |
|    227827 | 13774 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    113911 | 13775 | `				}` |
|    178824 | 13776 | `			}` |
|         - | 13777 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13778 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13779 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13780 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4858231 | 13781 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4858231 | 13782 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13783 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4858211 | 13784 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     60839 | 13785 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4827774 | 13786 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13787 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4797349 | 13788 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13789 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    930917 | 13790 | `					iP2 = PH7_MEMBER_WRITE;` |
|    465456 | 13791 | `				}` |
|   2429113 | 13792 | `			}` |
|   2429113 | 13793 | `		}` |
|         - | 13794 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13795 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13796 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13797 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13798 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  22568399 | 13799 | `		if( bFcc ){` |
|        81 | 13800 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13801 | `			iP2 = 0;` |
|        81 | 13802 | `			p3 = 0;` |
|        81 | 13803 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13804 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13805 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13806 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13807 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13808 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13809 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13810 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13811 | `				if( pMemberName ){` |
|         3 | 13812 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13813 | `				}` |
|        37 | 13814 | `				iP1 = 2;` |
|        19 | 13815 | `			}else{` |
|        45 | 13816 | `				iP1 = 1;` |
|         - | 13817 | `			}` |
|        40 | 13818 | `		}` |
|         - | 13819 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13820 | `		 * This is the primary emit path for user-visible calls. */` |
|  22568399 | 13821 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6097777 | 13822 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3048886 | 13823 | `		}` |
|         - | 13824 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  22568399 | 13825 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  11284197 | 13826 | `	}` |
|  22621811 | 13827 | `	if( nJmpIdx > 0 ){` |
|         - | 13828 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    577833 | 13829 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    577833 | 13830 | `		if( pInstr ){` |
|    577833 | 13831 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    288914 | 13832 | `		}` |
|    288914 | 13833 | `	}` |
|  22621811 | 13834 | `	return rc;` |
|  28619800 | 13835 | `}` |
|         - | 13836 | `/*` |
|         - | 13837 | ` * Compile a PHP expression.` |
|         - | 13838 | ` * According to the PHP language reference manual:` |
|         - | 13839 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 13840 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 13841 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 13842 | ` *  is "anything that has a value".` |
|         - | 13843 | ` * If something goes wrong while compiling the expression,this` |
|         - | 13844 | ` * function takes care of generating the appropriate error` |
|         - | 13845 | ` * message.` |
|         - | 13846 | ` */` |
|         - | 13847 | `/*` |
|         - | 13848 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 13849 | ` *` |
|         - | 13850 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 13851 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 13852 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 13853 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 13854 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 13855 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 13856 | ` * except for() now reports php's parse error.` |
|         - | 13857 | ` */` |
| 189857822 | 13858 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 13859 | `{` |
|         - | 13860 | `	ph7_expr_node **apArg;` |
|         - | 13861 | `	sxu32 n;` |
| 189857827 | 13862 | `	if( pNode == 0 ){` |
| 133329085 | 13863 | `		return 0;` |
|         - | 13864 | `	}` |
|  56528747 | 13865 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 13866 | `		return 1;` |
|         - | 13867 | `	}` |
|  56528738 | 13868 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  56528739 | 13869 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 13870 | `		return 1;` |
|         - | 13871 | `	}` |
|  56528739 | 13872 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  64470011 | 13873 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   7941277 | 13874 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 13875 | `			return 1;` |
|         - | 13876 | `		}` |
|   3970641 | 13877 | `	}` |
|  56528739 | 13878 | `	return 0;` |
|  94928916 | 13879 | `}` |
|  12581498 | 13880 | `static sxi32 PH7_CompileExpr(` |
|         - | 13881 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 13882 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 13883 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 13884 | `	)` |
|         5 | 13885 | `{` |
|         - | 13886 | `	ph7_expr_node *pRoot;` |
|         - | 13887 | `	SySet sExprNode;` |
|         - | 13888 | `	SyToken *pEnd;` |
|         - | 13889 | `	sxi32 nExpr;` |
|         - | 13890 | `	sxi32 iNest;` |
|         - | 13891 | `	sxi32 rc;` |
|         - | 13892 | `	sxu32 nNullsafeBase;` |
|         - | 13893 | `	/* Initialize worker variables */` |
|  12581503 | 13894 | `	nExpr = 0;` |
|  12581503 | 13895 | `	pRoot = 0;` |
|         - | 13896 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 13897 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  12581503 | 13898 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  12581503 | 13899 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  12581503 | 13900 | `	SySetAlloc(&sExprNode,0x10);` |
|  12581503 | 13901 | `	rc = SXRET_OK;` |
|         - | 13902 | `	/* Delimit the expression */` |
|  12581503 | 13903 | `	pEnd = pGen->pIn;` |
|  12581503 | 13904 | `	iNest = 0;` |
|  99921289 | 13905 | `	while( pEnd < pGen->pEnd ){` |
|  95313325 | 13906 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 13907 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4505 | 13908 | `			iNest++;` |
|  95311075 | 13909 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4513 | 13910 | `			iNest--;` |
|  95306571 | 13911 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   7974155 | 13912 | `			if( iNest <= 0 ){` |
|   7973539 | 13913 | `				break;` |
|         - | 13914 | `			}` |
|       308 | 13915 | `		}` |
|  87339791 | 13916 | `		pEnd++;` |
|         5 | 13917 | `	}` |
|  12581503 | 13918 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    634941 | 13919 | `		SyToken *pEnd2 = pGen->pIn;` |
|    634941 | 13920 | `		iNest = 0;` |
|         - | 13921 | `		/* Stop at the first comma */` |
|   1384485 | 13922 | `		while( pEnd2 < pEnd ){` |
|    749551 | 13923 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     41865 | 13924 | `				iNest++;` |
|    728621 | 13925 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     41865 | 13926 | `				iNest--;` |
|    686761 | 13927 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|        63 | 13928 | `				if( iNest <= 0 ){` |
|         3 | 13929 | `					break;` |
|         - | 13930 | `				}` |
|        28 | 13931 | `			}` |
|    749549 | 13932 | `			pEnd2++;` |
|         5 | 13933 | `		}` |
|    634941 | 13934 | `		if( pEnd2 <pEnd ){` |
|         3 | 13935 | `			pEnd = pEnd2;` |
|         1 | 13936 | `		}` |
|    317468 | 13937 | `	}` |
|  12581503 | 13938 | `	if( pEnd > pGen->pIn ){` |
|  12558717 | 13939 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 13940 | `		/* Swap delimiter */` |
|  12558717 | 13941 | `		pGen->pEnd = pEnd;` |
|         - | 13942 | `		/* Try to get an expression tree */` |
|  12558717 | 13943 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  12558712 | 13944 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  12444441 | 13945 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 13946 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 13947 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 13948 | `				"syntax error, unexpected token \",\"");` |
|         6 | 13949 | `			pGen->pEnd = pTmp;` |
|         6 | 13950 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13951 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 13952 | `				return SXERR_ABORT;` |
|         - | 13953 | `			}` |
|         6 | 13954 | `			pGen->pIn = pEnd;` |
|         6 | 13955 | `			SySetRelease(&sExprNode);` |
|         6 | 13956 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 13957 | `			return SXRET_OK;` |
|         - | 13958 | `		}` |
|  12558713 | 13959 | `		if( rc == SXRET_OK && pRoot ){` |
|  12558529 | 13960 | `			rc = SXRET_OK;` |
|  12558529 | 13961 | `			if( xTreeValidator ){` |
|         - | 13962 | `				/* Call the upper layer validator callback */` |
|    825005 | 13963 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    412500 | 13964 | `			}` |
|  12558529 | 13965 | `			if( rc != SXERR_ABORT ){` |
|         - | 13966 | `				/* Generate code for the given tree */` |
|  12558529 | 13967 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 13968 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 13969 | `				 * expression so they short-circuit to its end. */` |
|  12558529 | 13970 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   6279262 | 13971 | `			}` |
|  12558529 | 13972 | `			nExpr = 1;` |
|   6279262 | 13973 | `		}` |
|         - | 13974 | `		/* Release the whole tree */` |
|  12558713 | 13975 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 13976 | `		/* Synchronize token stream */` |
|  12558713 | 13977 | `		pGen->pEnd = pTmp;` |
|  12558713 | 13978 | `		pGen->pIn  = pEnd;` |
|  12558713 | 13979 | `		if( rc == SXERR_ABORT ){` |
|        13 | 13980 | `			SySetRelease(&sExprNode);` |
|        13 | 13981 | `			return SXERR_ABORT;` |
|         - | 13982 | `		}` |
|   6279349 | 13983 | `	}` |
|  12581489 | 13984 | `	SySetRelease(&sExprNode);` |
|  12581489 | 13985 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   6290754 | 13986 | `}` |
|         - | 13987 | `/*` |
|         - | 13988 | ` * Return a pointer to the node construct handler associated` |
|         - | 13989 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 13990 | ` */` |
|   7059190 | 13991 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 13992 | `{` |
|   7059195 | 13993 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 13994 | `		/* Numeric literal: Either real or integer */` |
|   2695985 | 13995 | `		return PH7_CompileNumLiteral;` |
|   4363215 | 13996 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 13997 | `		/* Double quoted string */` |
|     80157 | 13998 | `		return PH7_CompileString;` |
|   4283063 | 13999 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 14000 | `		/* Single quoted string */` |
|   4282943 | 14001 | `		return PH7_CompileSimpleString;` |
|       124 | 14002 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 14003 | `		/* Heredoc */` |
|        70 | 14004 | `		return PH7_CompileHereDoc;` |
|        58 | 14005 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 14006 | `		/* Nowdoc */` |
|        51 | 14007 | `		return PH7_CompileNowDoc;` |
|         9 | 14008 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 14009 | `		/* Backtick quoted string */` |
|         6 | 14010 | `		return PH7_CompileBacktic;` |
|         - | 14011 | `	}` |
|         3 | 14012 | `	return 0;` |
|   3529600 | 14013 | `}` |
|         - | 14014 | `/*` |
|         - | 14015 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 14016 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 14017 | ` * in write context" parse error.` |
|         - | 14018 | ` */` |
|     29990 | 14019 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 14020 | `{` |
|         - | 14021 | `	sxi32 rc;` |
|     29995 | 14022 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     29993 | 14023 | `		return SXRET_OK;` |
|         - | 14024 | `	}` |
|         5 | 14025 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 14026 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 14027 | `		"Can't use nullsafe operator in write context");` |
|         3 | 14028 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     15000 | 14029 | `}` |
|         - | 14030 | `/*` |
|         - | 14031 | ` * Compile an unset() statement.` |
|         - | 14032 | ` * unset($var, $arr[$key], ...);` |
|         - | 14033 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 14034 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 14035 | ` * parent array before extracting the element to unset.` |
|         - | 14036 | ` */` |
|     25760 | 14037 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 14038 | `{` |
|     25765 | 14039 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     25765 | 14040 | `	sxu32 nIdx = 0;` |
|         - | 14041 | `	SyString sName;` |
|         - | 14042 | `	sxi32 rc;` |
|         - | 14043 | `	/* Jump the 'unset' keyword */` |
|     25765 | 14044 | `	pGen->pIn++;` |
|         - | 14045 | `	/* Save delimiter */` |
|     25765 | 14046 | `	pTmp = pGen->pEnd;` |
|         - | 14047 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     25765 | 14048 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     25765 | 14049 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14050 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 14051 | `		SyToken *pClose;` |
|     25765 | 14052 | `		pGen->pIn++;   /* Skip '(' */` |
|     25765 | 14053 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     25765 | 14054 | `		pEnd = pClose; /* Stop at ')' */` |
|     12880 | 14055 | `	}` |
|     25765 | 14056 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 14057 | `	/* Resolve the 'unset' builtin name once */` |
|     25765 | 14058 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3801 | 14059 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3801 | 14060 | `		if( pObj == 0 ){` |
|       ! 0 | 14061 | `			return SXERR_ABORT;` |
|         - | 14062 | `		}` |
|      3801 | 14063 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3801 | 14064 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1898 | 14065 | `	}` |
|         - | 14066 | `	/* Compile each comma-separated argument */` |
|     55757 | 14067 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     29997 | 14068 | `		if( pGen->pIn < pNext ){` |
|     29997 | 14069 | `			pGen->pEnd = pNext;` |
|     29997 | 14070 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 14071 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 14072 | `				GenStateUnsetValidator);` |
|     29997 | 14073 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14074 | `				return SXERR_ABORT;` |
|         - | 14075 | `			}` |
|     29997 | 14076 | `			if( rc != SXERR_EMPTY ){` |
|         - | 14077 | `				/* Emit call for this single argument */` |
|     29995 | 14078 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     29995 | 14079 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     29995 | 14080 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     14995 | 14081 | `			}` |
|     14996 | 14082 | `		}` |
|         - | 14083 | `		/* Jump trailing commas */` |
|     34231 | 14084 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|      4239 | 14085 | `			pNext++;` |
|         5 | 14086 | `		}` |
|     29997 | 14087 | `		pGen->pIn = pNext;` |
|         5 | 14088 | `	}` |
|         - | 14089 | `	/* Skip past the closing ')' if present */` |
|     25765 | 14090 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     25765 | 14091 | `		pGen->pIn++;` |
|     12880 | 14092 | `	}` |
|         - | 14093 | `	/* Restore token stream */` |
|     25765 | 14094 | `	pGen->pEnd = pTmp;` |
|     25765 | 14095 | `	return SXRET_OK;` |
|     12885 | 14096 | `}` |
|         - | 14097 | `/*` |
|         - | 14098 | ` * PHP Language construct table.` |
|         - | 14099 | ` */` |
|         - | 14100 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 14101 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 14102 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14103 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14104 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14105 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14106 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14107 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14108 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14109 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14110 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14111 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14112 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14113 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14114 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14115 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14116 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14117 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14118 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14119 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14120 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14121 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14122 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14123 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14124 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14125 | `};` |
|         - | 14126 | `/*` |
|         - | 14127 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14128 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14129 | ` */` |
|   6246392 | 14130 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14131 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14132 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14133 | `	)` |
|         5 | 14134 | `{` |
|   6246397 | 14135 | `	sxu32 n = 0;` |
|  25801680 | 14136 | `	for(;;){` |
|  51603365 | 14137 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    427455 | 14138 | `			break;` |
|         - | 14139 | `		}` |
|  51175915 | 14140 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   5818947 | 14141 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14142 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14143 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14144 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14145 | `					return 0;` |
|         - | 14146 | `				}` |
|       ! 0 | 14147 | `			}` |
|   5818942 | 14148 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|        14 | 14149 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|        14 | 14150 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14151 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14152 | `				return 0;` |
|         - | 14153 | `			}` |
|         - | 14154 | `			/* Return a pointer to the handler.` |
|         - | 14155 | `			*/` |
|   5818945 | 14156 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14157 | `		}` |
|  45356973 | 14158 | `		n++;` |
|         5 | 14159 | `	}` |
|    427455 | 14160 | `	if( pLookahed ){` |
|    427455 | 14161 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     68437 | 14162 | `			return PH7_CompileClassInterface;` |
|    359023 | 14163 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    312917 | 14164 | `			return PH7_CompileClass;` |
|     46111 | 14165 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7675 | 14166 | `			return PH7_CompileTrait;` |
|         - | 14167 | `		}` |
|         - | 14168 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14169 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14170 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14171 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19218 | 14172 | `	}` |
|         - | 14173 | `	/* Not a language construct */` |
|     38441 | 14174 | `	return 0;` |
|   3123201 | 14175 | `}` |
|         - | 14176 | `/*` |
|         - | 14177 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14178 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14179 | ` */` |
|     38438 | 14180 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14181 | `{` |
|         - | 14182 | `	int rc;` |
|     38443 | 14183 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38443 | 14184 | `	if( rc == FALSE ){` |
|     38334 | 14185 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15550 | 14186 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14187 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14188 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14189 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14190 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14191 | `			*/` |
|         - | 14192 | `			){` |
|     38331 | 14193 | `				rc = TRUE;` |
|     19163 | 14194 | `		}` |
|     19167 | 14195 | `	}` |
|     38443 | 14196 | `	return rc;` |
|         5 | 14197 | `}` |
|         - | 14198 | `/*` |
|         - | 14199 | ` * Compile a PHP chunk.` |
|         - | 14200 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14201 | ` * takes care of generating the appropriate error message.` |
|         - | 14202 | ` */` |
|         - | 14203 | `/*` |
|         - | 14204 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14205 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14206 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14207 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14208 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14209 | ` * intervening non-declaration statements.` |
|         - | 14210 | ` */` |
|  13839806 | 14211 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14212 | `{` |
|  13839811 | 14213 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  13839811 | 14214 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  13839811 | 14215 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14216 | `	sxu32 nIdx, n;` |
|  13839806 | 14217 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   1502329 | 14218 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14219 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14220 | `		 * indexes do not map to the sidecar */` |
|  12337489 | 14221 | `		return;` |
|         - | 14222 | `	}` |
|   1502327 | 14223 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14224 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14225 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   1502327 | 14226 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4508465 | 14227 | `	for( n = 0 ; n < nT ; n++ ){` |
|   3006143 | 14228 | `		if( aT[n].nTokIdx != nIdx ){` |
|   2998387 | 14229 | `			continue;` |
|         - | 14230 | `		}` |
|      7761 | 14231 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14232 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7749 | 14233 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7737 | 14234 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3866 | 14235 | `		}` |
|      3883 | 14236 | `	}` |
|   6919908 | 14237 | `}` |
|         - | 14238 | `/*` |
|         - | 14239 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14240 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14241 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14242 | ` */` |
|   3844436 | 14243 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14244 | `{` |
|         - | 14245 | `	char *zDup;` |
|   3844441 | 14246 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   3844421 | 14247 | `		return;` |
|         - | 14248 | `	}` |
|        35 | 14249 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14250 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14251 | `	if( zDup ){` |
|        25 | 14252 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14253 | `	}` |
|        25 | 14254 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   1922223 | 14255 | `}` |
|         - | 14256 | `/*` |
|         - | 14257 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14258 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14259 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14260 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14261 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14262 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14263 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14264 | ` */` |
|      7744 | 14265 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14266 | `{` |
|         - | 14267 | `	SySet *pToken;` |
|         - | 14268 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14269 | `	char *zSpan;` |
|      7749 | 14270 | `	sxi32 rc = SXRET_OK;` |
|      7749 | 14271 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14272 | `		return SXRET_OK;` |
|         - | 14273 | `	}` |
|     11621 | 14274 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3872 | 14275 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7749 | 14276 | `	if( zSpan == 0 ){` |
|       ! 0 | 14277 | `		return SXRET_OK;` |
|         - | 14278 | `	}` |
|         - | 14279 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14280 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14281 | `	 * the number of attribute declarations in the program. */` |
|      7749 | 14282 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7749 | 14283 | `	if( pToken == 0 ){` |
|       ! 0 | 14284 | `		return SXRET_OK;` |
|         - | 14285 | `	}` |
|      7749 | 14286 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7749 | 14287 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7749 | 14288 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7749 | 14289 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7749 | 14290 | `	pSavedIn = pGen->pIn;` |
|      7749 | 14291 | `	pSavedEnd = pGen->pEnd;` |
|      7753 | 14292 | `	while( pIn < pEnd ){` |
|         - | 14293 | `		ph7_attribute sAttr;` |
|         - | 14294 | `		SyBlob sFQN;` |
|      7753 | 14295 | `		int bAbsolute = 0;` |
|      7753 | 14296 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7753 | 14297 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7753 | 14298 | `		sAttr.nLine = pIn->nLine;` |
|      7753 | 14299 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14300 | `			bAbsolute = 1;` |
|        75 | 14301 | `			pIn++;` |
|        35 | 14302 | `		}` |
|      7753 | 14303 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7753 | 14304 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7753 | 14305 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7753 | 14306 | `			pIn++;` |
|      7753 | 14307 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14308 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14309 | `				pIn++;` |
|       ! 0 | 14310 | `				continue;` |
|         - | 14311 | `			}` |
|      7753 | 14312 | `			break;` |
|       ! 0 | 14313 | `		}` |
|      7753 | 14314 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14315 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14316 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14317 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14318 | `			break;` |
|         - | 14319 | `		}` |
|         - | 14320 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14321 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14322 | `		{` |
|      7753 | 14323 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7753 | 14324 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7753 | 14325 | `			char *zDup = 0;` |
|      7753 | 14326 | `			if( !bAbsolute ){` |
|      7683 | 14327 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7683 | 14328 | `				if( pImp ){` |
|       ! 0 | 14329 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14330 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14331 | `					if( zDup ){` |
|       ! 0 | 14332 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14333 | `					}` |
|      7683 | 14334 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14335 | `					SyBlob sTmp;` |
|       ! 0 | 14336 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14337 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14338 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14339 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14340 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14341 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14342 | `					if( zDup ){` |
|       ! 0 | 14343 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14344 | `					}` |
|       ! 0 | 14345 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14346 | `				}` |
|      3839 | 14347 | `			}` |
|      7753 | 14348 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7753 | 14349 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7753 | 14350 | `				if( zDup ){` |
|      7753 | 14351 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3874 | 14352 | `				}` |
|      3874 | 14353 | `			}` |
|         - | 14354 | `		}` |
|      7753 | 14355 | `		SyBlobRelease(&sFQN);` |
|      7753 | 14356 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14357 | `			SyToken *pArgsEnd;` |
|      7651 | 14358 | `			pIn++;` |
|      7651 | 14359 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15311 | 14360 | `			while( pIn < pArgsEnd ){` |
|      7665 | 14361 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7665 | 14362 | `				sxi32 iDepth = 0;` |
|         - | 14363 | `				ph7_attr_arg sArgRec;` |
|     76165 | 14364 | `				while( pArgStop < pArgsEnd ){` |
|     68521 | 14365 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14366 | `						iDepth++;` |
|     68516 | 14367 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14368 | `						iDepth--;` |
|     68506 | 14369 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14370 | `						break;` |
|         - | 14371 | `					}` |
|     68505 | 14372 | `					pArgStop++;` |
|         5 | 14373 | `				}` |
|      7665 | 14374 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7665 | 14375 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7660 | 14376 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7644 | 14377 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14378 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14379 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14380 | `					if( zN ){` |
|        19 | 14381 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14382 | `					}` |
|        19 | 14383 | `					pArgStart += 2;` |
|         9 | 14384 | `				}` |
|      7665 | 14385 | `				if( pArgStart < pArgStop ){` |
|         - | 14386 | `					SySet *pInstrContainer;` |
|      7665 | 14387 | `					pGen->pIn = pArgStart;` |
|      7665 | 14388 | `					pGen->pEnd = pArgStop;` |
|      7665 | 14389 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7665 | 14390 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7665 | 14391 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7665 | 14392 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7665 | 14393 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7665 | 14394 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14395 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14396 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14397 | `						return SXERR_ABORT;` |
|         - | 14398 | `					}` |
|      7665 | 14399 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3830 | 14400 | `				}` |
|      7665 | 14401 | `				pIn = pArgStop;` |
|      7665 | 14402 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14403 | `					pIn++;` |
|         8 | 14404 | `				}` |
|         5 | 14405 | `			}` |
|      7651 | 14406 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3823 | 14407 | `		}` |
|      7753 | 14408 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7753 | 14409 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14410 | `			pIn++;` |
|         5 | 14411 | `			continue;` |
|         - | 14412 | `		}` |
|      7749 | 14413 | `		break;` |
|       ! 0 | 14414 | `	}` |
|      7749 | 14415 | `	pGen->pIn = pSavedIn;` |
|      7749 | 14416 | `	pGen->pEnd = pSavedEnd;` |
|      7749 | 14417 | `	return SXRET_OK;` |
|      3877 | 14418 | `}` |
|         - | 14419 | `/*` |
|         - | 14420 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14421 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14422 | ` */` |
|   3844440 | 14423 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14424 | `{` |
|   3844445 | 14425 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14426 | `	sxu32 n;` |
|         - | 14427 | `	sxi32 rc;` |
|   3852177 | 14428 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7737 | 14429 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7737 | 14430 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14431 | `			return SXERR_ABORT;` |
|         - | 14432 | `		}` |
|      3871 | 14433 | `	}` |
|   3844445 | 14434 | `	SySetReset(&pGen->aPendingAttrs);` |
|   3844445 | 14435 | `	return SXRET_OK;` |
|   1922225 | 14436 | `}` |
|         - | 14437 | `/*` |
|         - | 14438 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14439 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14440 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14441 | ` */` |
|   1647306 | 14442 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14443 | `{` |
|   1647311 | 14444 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   1647311 | 14445 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   1647311 | 14446 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14447 | `	sxu32 nIdx, n;` |
|         - | 14448 | `	sxi32 rc;` |
|   1647306 | 14449 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    190135 | 14450 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1457181 | 14451 | `		return SXRET_OK;` |
|         - | 14452 | `	}` |
|    190135 | 14453 | `	nIdx = (sxu32)(pTok - pBase);` |
|    570393 | 14454 | `	for( n = 0 ; n < nT ; n++ ){` |
|    380263 | 14455 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14456 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14457 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14458 | `				return SXERR_ABORT;` |
|         - | 14459 | `			}` |
|         6 | 14460 | `		}` |
|    190134 | 14461 | `	}` |
|    190135 | 14462 | `	return SXRET_OK;` |
|    823658 | 14463 | `}` |
|  10021858 | 14464 | `static sxi32 GenStateCompileChunk(` |
|         - | 14465 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14466 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14467 | `	)` |
|         5 | 14468 | `{` |
|         - | 14469 | `	ProcLangConstruct xCons;` |
|         - | 14470 | `	sxi32 rc;` |
|  10021863 | 14471 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   5772925 | 14472 | `	for(;;){` |
|  10783859 | 14473 | `		int bStmtIsDeclare = 0;` |
|  10783859 | 14474 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14475 | `			/* No more input to process */` |
|     67195 | 14476 | `			break;` |
|         - | 14477 | `		}` |
|         - | 14478 | `		/* Bind a directly-preceding docblock to this statement */` |
|  10716669 | 14479 | `		GenStateSetPendingDoc(&(*pGen));` |
|  10716669 | 14480 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14481 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14482 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14483 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14484 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14485 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7655 | 14486 | `			int bAttrTarget = 0;` |
|      7650 | 14487 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3859 | 14488 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7597 | 14489 | `				bAttrTarget = 1;` |
|      3855 | 14490 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14491 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14492 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14493 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14494 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14495 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14496 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14497 | `					bAttrTarget = 1;` |
|        29 | 14498 | `				}` |
|        29 | 14499 | `			}` |
|      7655 | 14500 | `			if( !bAttrTarget ){` |
|       ! 0 | 14501 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14502 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14503 | `					&pGen->pIn->sData);` |
|       ! 0 | 14504 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14505 | `					break;` |
|         - | 14506 | `				}` |
|       ! 0 | 14507 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14508 | `			}` |
|      3825 | 14509 | `		}` |
|         - | 14510 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14511 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  10716669 | 14512 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6280611 | 14513 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   6280611 | 14514 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14515 | `				bStmtIsDeclare = 1;` |
|        21 | 14516 | `			}` |
|   3140303 | 14517 | `		}` |
|  10716669 | 14518 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14519 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14520 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    761969 | 14521 | `			pGen->bStrictTypesLocked = 1;` |
|    380982 | 14522 | `		}` |
|  10716669 | 14523 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14524 | `			/* Compile block */` |
|      3819 | 14525 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3819 | 14526 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14527 | `				break;` |
|         - | 14528 | `			}` |
|      1912 | 14529 | `		}else{` |
|  10712855 | 14530 | `			xCons = 0;` |
|  10712855 | 14531 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14532 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14533 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14534 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34245 | 14535 | `				xCons = PH7_CompileClassModifiers;` |
|  10695735 | 14536 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14537 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14538 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3829 | 14539 | `				xCons = PH7_CompileEnum;` |
|  10676703 | 14540 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6246397 | 14541 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14542 | `				/* Try to extract a language construct handler */` |
|   6246397 | 14543 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   6246397 | 14544 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14545 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14546 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14547 | `						&pGen->pIn->sData);` |
|         9 | 14548 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14549 | `						break;` |
|         - | 14550 | `					}` |
|         - | 14551 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14552 | `					 * this erroneous statement.` |
|         - | 14553 | `					 */` |
|         9 | 14554 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14555 | `				}` |
|   7551595 | 14556 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    357707 | 14557 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14558 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14559 | `				xCons = PH7_CompileLabel;` |
|        56 | 14560 | `			}` |
|  10712855 | 14561 | `			if( xCons == 0 ){` |
|         - | 14562 | `				/* Assume an expression an try to compile it */` |
|   4466717 | 14563 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   4466717 | 14564 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14565 | `					/* Pop l-value */` |
|   4466567 | 14566 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2233281 | 14567 | `				}` |
|   2233361 | 14568 | `			}else{` |
|         - | 14569 | `				/* Go compile the sucker */` |
|   6246143 | 14570 | `				rc = xCons(&(*pGen));` |
|         - | 14571 | `			}` |
|  10712855 | 14572 | `			if( rc == SXERR_ABORT ){` |
|         - | 14573 | `				/* Request to abort compilation */` |
|        13 | 14574 | `				break;` |
|         - | 14575 | `			}` |
|         - | 14576 | `		}` |
|         - | 14577 | `		/* Ignore trailing semi-colons ';' */` |
|  18460297 | 14578 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   7743643 | 14579 | `			pGen->pIn++;` |
|         5 | 14580 | `		}` |
|  10716659 | 14581 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14582 | `			/* Compile a single statement and return */` |
|   9954663 | 14583 | `			break;` |
|         - | 14584 | `		}` |
|         - | 14585 | `		/* LOOP ONE */` |
|         - | 14586 | `		/* LOOP TWO */` |
|         - | 14587 | `		/* LOOP THREE */` |
|         - | 14588 | `		/* LOOP FOUR */` |
|         5 | 14589 | `	}` |
|         - | 14590 | `	/* Return compilation status */` |
|  10021863 | 14591 | `	return rc;` |
|         5 | 14592 | `}` |
|         - | 14593 | `/*` |
|         - | 14594 | ` * Compile a Raw PHP chunk.` |
|         - | 14595 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14596 | ` * takes care of generating the appropriate error message.` |
|         - | 14597 | ` */` |
|     67202 | 14598 | `static sxi32 PH7_CompilePHP(` |
|         - | 14599 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14600 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14601 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14602 | `	)` |
|         5 | 14603 | `{` |
|     67207 | 14604 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14605 | `	sxi32 rc;` |
|         - | 14606 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67207 | 14607 | `	SySetReset(&(*pTokenSet));` |
|     67207 | 14608 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14609 | `	/* Mark as the default token set */` |
|     67207 | 14610 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14611 | `	/* Advance the stream cursor */` |
|     67207 | 14612 | `	pGen->pRawIn++;` |
|         - | 14613 | `	/* Tokenize the PHP chunk first */` |
|     67207 | 14614 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14615 | `	/* Point to the head and tail of the token stream. */` |
|     67207 | 14616 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67207 | 14617 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67207 | 14618 | `	if( is_expr ){` |
|       ! 0 | 14619 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14620 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14621 | `			/* A simple expression,compile it */` |
|       ! 0 | 14622 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14623 | `		}` |
|         - | 14624 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14625 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14626 | `		return SXRET_OK;` |
|         - | 14627 | `	}` |
|     67207 | 14628 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14629 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14630 | `		/*` |
|         - | 14631 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14632 | `		 * According to the PHP reference manual:` |
|         - | 14633 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14634 | `		 *  immediately follow` |
|         - | 14635 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14636 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14637 | `		 * Symisc extension:` |
|         - | 14638 | `		 *   This short syntax works with all PHP opening` |
|         - | 14639 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14640 | `		 *   only short tag.` |
|         - | 14641 | `		 */` |
|         - | 14642 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14643 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14644 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14645 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14646 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14647 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14648 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14649 | `		}` |
|         3 | 14650 | `		return SXRET_OK;` |
|         - | 14651 | `	}` |
|         - | 14652 | `	/* Compile the PHP chunk */` |
|     67205 | 14653 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14654 | `	/* Fix exceptions jumps */` |
|     67205 | 14655 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14656 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67205 | 14657 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14658 | `		rc = SXERR_ABORT;` |
|         1 | 14659 | `	}` |
|         - | 14660 | `	/* Reset container */` |
|     67205 | 14661 | `	SySetReset(&pGen->aGoto);` |
|     67205 | 14662 | `	SySetReset(&pGen->aLabel);` |
|     67205 | 14663 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14664 | `	/* Compilation result */` |
|     67205 | 14665 | `	return rc;` |
|     33606 | 14666 | `}` |
|         - | 14667 | `/*` |
|         - | 14668 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14669 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14670 | ` * This is the only compile interface exported from this file.` |
|         - | 14671 | ` */` |
|     70284 | 14672 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14673 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14674 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14675 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14676 | `	)` |
|         5 | 14677 | `{` |
|         - | 14678 | `	SySet aPhpToken,aRawToken;` |
|         - | 14679 | `	ph7_gen_state *pCodeGen;` |
|         - | 14680 | `	ph7_value *pRawObj;` |
|         - | 14681 | `	sxu32 nObjIdx;` |
|         - | 14682 | `	sxi32 nRawObj;` |
|         - | 14683 | `	int is_expr;` |
|         - | 14684 | `	sxi8 bSavedStrict;` |
|         - | 14685 | `	sxi8 bSavedStrictLocked;` |
|         - | 14686 | `	sxi32 rc;` |
|     70289 | 14687 | `	if( pScript->nByte < 1 ){` |
|         - | 14688 | `		/* Nothing to compile */` |
|       ! 0 | 14689 | `		return PH7_OK;` |
|         - | 14690 | `	}` |
|         - | 14691 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14692 | `	 * file's flags so include/require restore them on return. */` |
|     70289 | 14693 | `	pCodeGen = &pVm->sCodeGen;` |
|     70289 | 14694 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     70289 | 14695 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     70289 | 14696 | `	pCodeGen->bStrictTypes = 0;` |
|     70289 | 14697 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14698 | `	/* Initialize the tokens containers */` |
|     70289 | 14699 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70289 | 14700 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70289 | 14701 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     70289 | 14702 | `	is_expr = 0;` |
|     70289 | 14703 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14704 | `		SyToken sTmp;` |
|         - | 14705 | `		/* PHP only: -*/` |
|     57057 | 14706 | `		sTmp.nLine = 1;` |
|     57057 | 14707 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57057 | 14708 | `		sTmp.pUserData = 0;` |
|     57057 | 14709 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57057 | 14710 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57057 | 14711 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14712 | `			/* A simple PHP expression */` |
|       ! 0 | 14713 | `			is_expr = 1;` |
|       ! 0 | 14714 | `		}` |
|     28531 | 14715 | `	}else{` |
|         - | 14716 | `		/* Tokenize raw text */` |
|     13237 | 14717 | `		SySetAlloc(&aRawToken,32);` |
|     13237 | 14718 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|         - | 14719 | `	}` |
|         - | 14720 | `	/* Process high-level tokens */` |
|     70289 | 14721 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     70289 | 14722 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     70289 | 14723 | `	rc = PH7_OK;` |
|     70289 | 14724 | `	if( is_expr ){` |
|         - | 14725 | `		/* Compile the expression */` |
|       ! 0 | 14726 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14727 | `		goto cleanup;` |
|         - | 14728 | `	}` |
|     70289 | 14729 | `	nObjIdx = 0;` |
|         - | 14730 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14731 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14732 | `	 * preventing namespace bleeding across include()d files. */` |
|     70289 | 14733 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14734 | `	/* Start the compilation process */` |
|     41763 | 14735 | `	for(;;){` |
|    150721 | 14736 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     70277 | 14737 | `			break; /* No more tokens to process */` |
|         - | 14738 | `		}` |
|     80449 | 14739 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14740 | `			/* Compile the PHP chunk */` |
|     67207 | 14741 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67207 | 14742 | `			if( rc == SXERR_ABORT ){` |
|        16 | 14743 | `				break;` |
|         - | 14744 | `			}` |
|     67195 | 14745 | `			continue;` |
|         - | 14746 | `		}` |
|         - | 14747 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13247 | 14748 | `		nRawObj = 0;` |
|     26489 | 14749 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14750 | `			/* Consume the raw chunk without any processing */` |
|     13247 | 14751 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13247 | 14752 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14753 | `				rc = SXERR_MEM;` |
|       ! 0 | 14754 | `				break;` |
|         - | 14755 | `			}` |
|         - | 14756 | `			/* Mark as constant and emit the load constant instruction */` |
|     13247 | 14757 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13247 | 14758 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13247 | 14759 | `			++nRawObj;` |
|     13247 | 14760 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14761 | `		}` |
|     13247 | 14762 | `		if( nRawObj > 0 ){` |
|         - | 14763 | `			/* Emit the consume instruction */` |
|     13247 | 14764 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6621 | 14765 | `		}` |
|     35147 | 14766 | `	}` |
|     35142 | 14767 | `cleanup:` |
|     70289 | 14768 | `	SySetRelease(&aRawToken);` |
|     70289 | 14769 | `	SySetRelease(&aPhpToken);` |
|         - | 14770 | `	/* Restore outer file's strict_types scope */` |
|     70289 | 14771 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     70289 | 14772 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     70289 | 14773 | `	return rc;` |
|     35147 | 14774 | `}` |
|         - | 14775 | `/*` |
|         - | 14776 | ` * Utility routines.Initialize the code generator.` |
|         - | 14777 | ` */` |
|      3796 | 14778 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 14779 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14780 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14781 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14782 | `	)` |
|         5 | 14783 | `{` |
|      3801 | 14784 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14785 | `	/* Zero the structure */` |
|      3801 | 14786 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 14787 | `	/* Initial state */` |
|      3801 | 14788 | `	pGen->pVm  = &(*pVm);` |
|      3801 | 14789 | `	pGen->xErr = xErr;` |
|      3801 | 14790 | `	pGen->pErrData = pErrData;` |
|      3801 | 14791 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3801 | 14792 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3801 | 14793 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3801 | 14794 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3801 | 14795 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3801 | 14796 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3801 | 14797 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3801 | 14798 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3801 | 14799 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 14800 | `	/* Error log buffer */` |
|      3801 | 14801 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 14802 | `	/* General purpose working buffer */` |
|      3801 | 14803 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 14804 | `	/* Namespace state */` |
|      3801 | 14805 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3801 | 14806 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3801 | 14807 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3801 | 14808 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14809 | `	/* Create the global scope */` |
|      3801 | 14810 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 14811 | `	/* Point to the global scope */` |
|      3801 | 14812 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3801 | 14813 | `	return SXRET_OK;` |
|         5 | 14814 | `}` |
|         - | 14815 | `/*` |
|         - | 14816 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 14817 | ` */` |
|     73626 | 14818 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 14819 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14820 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14821 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14822 | `	)` |
|         5 | 14823 | `{` |
|     73631 | 14824 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14825 | `	GenBlock *pBlock,*pParent;` |
|         - | 14826 | `	/* Reset state */` |
|     73631 | 14827 | `	SySetReset(&pGen->aLabel);` |
|     73631 | 14828 | `	SySetReset(&pGen->aGoto);` |
|     73631 | 14829 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     73631 | 14830 | `	SySetReset(&pGen->aTrivia);` |
|     73631 | 14831 | `	SySetReset(&pGen->aPendingAttrs);` |
|     73631 | 14832 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     73631 | 14833 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     73631 | 14834 | `	SyBlobRelease(&pGen->sWorker);` |
|     73631 | 14835 | `	SyBlobRelease(&pGen->sNamespace);` |
|     73631 | 14836 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     73631 | 14837 | `	SyHashRelease(&pGen->hUseImports);` |
|     73631 | 14838 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     73631 | 14839 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     73631 | 14840 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     73631 | 14841 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     73631 | 14842 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14843 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 14844 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 14845 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 14846 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 14847 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 14848 | `	 * number of unique names, which is acceptable. */` |
|         - | 14849 | `	/* Point to the global scope */` |
|     73631 | 14850 | `	pBlock = pGen->pCurrent;` |
|     73631 | 14851 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 14852 | `		pParent = pBlock->pParent;` |
|       ! 0 | 14853 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 14854 | `		pBlock = pParent;` |
|       ! 0 | 14855 | `	}` |
|     73631 | 14856 | `	pGen->xErr = xErr;` |
|     73631 | 14857 | `	pGen->pErrData = pErrData;` |
|     73631 | 14858 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     73631 | 14859 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     73631 | 14860 | `	pGen->pIn = pGen->pEnd = 0;` |
|     73631 | 14861 | `	pGen->nErr = 0;` |
|     73631 | 14862 | `	return SXRET_OK;` |
|         5 | 14863 | `}` |
|         - | 14864 | `/*` |
|         - | 14865 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 14866 | ` * php's parser prints, e.g.` |
|         - | 14867 | ` *` |
|         - | 14868 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 14869 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 14870 | ` *   syntax error, unexpected end of file` |
|         - | 14871 | ` *` |
|         - | 14872 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 14873 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 14874 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 14875 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 14876 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 14877 | ` *` |
|         - | 14878 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 14879 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 14880 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 14881 | ` */` |
|       180 | 14882 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 14883 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 14884 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 14885 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 14886 | `	)` |
|         5 | 14887 | `{` |
|       185 | 14888 | `	const char *zNoun = "token";` |
|         - | 14889 | `	sxu32 nLine;` |
|       185 | 14890 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 14891 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 14892 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 14893 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 14894 | `		 * it before concluding "end of file". */` |
|        82 | 14895 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        82 | 14896 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        82 | 14897 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        82 | 14898 | `			pTok = pGen->pEnd;` |
|        39 | 14899 | `		}` |
|        39 | 14900 | `	}` |
|       185 | 14901 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       185 | 14902 | `	if( pTok == 0 ){` |
|       ! 0 | 14903 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 14904 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 14905 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 14906 | `			zExpecting);` |
|         - | 14907 | `	}` |
|       185 | 14908 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 14909 | `		zNoun = "identifier";` |
|       178 | 14910 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         8 | 14911 | `		zNoun = "variable";` |
|       169 | 14912 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        24 | 14913 | `		zNoun = "integer";` |
|       156 | 14914 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 14915 | `		zNoun = "float";` |
|       ! 0 | 14916 | `	}` |
|       185 | 14917 | `	if( zExpecting ){` |
|       115 | 14918 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        37 | 14919 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 14920 | `	}` |
|       164 | 14921 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 14922 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        95 | 14923 | `}` |
|         - | 14924 | `/*` |
|         - | 14925 | ` * Generate a compile-time error message.` |
|         - | 14926 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 14927 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 14928 | ` * abort compilation immediately.` |
|         - | 14929 | ` */` |
|     15860 | 14930 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 14931 | `{` |
|     15865 | 14932 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     15865 | 14933 | `	const char *zErr = "Error";` |
|         - | 14934 | `	SyString *pFile;` |
|         - | 14935 | `	va_list ap;` |
|         - | 14936 | `	sxi32 rc;` |
|         - | 14937 | `	/* Reset the working buffer */` |
|     15865 | 14938 | `	SyBlobReset(pWorker);` |
|         - | 14939 | `	/* Peek the processed file path if available */` |
|     15865 | 14940 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     15865 | 14941 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 14942 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 14943 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 14944 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 14945 | `		 * into execution with a 0 exit status. */` |
|       657 | 14946 | `		pGen->nErr++;` |
|       657 | 14947 | `		if( pGen->nErr > 15 ){` |
|         - | 14948 | `			/* Error count limit reached */` |
|         6 | 14949 | `			if( pGen->xErr ){` |
|         6 | 14950 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 14951 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 14952 | `				if( pFile ){` |
|         6 | 14953 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 14954 | `				}` |
|         6 | 14955 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 14956 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 14957 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 14958 | `				}` |
|         2 | 14959 | `			}` |
|         - | 14960 | `			/* Abort immediately */` |
|         6 | 14961 | `			return SXERR_ABORT;` |
|         - | 14962 | `		}` |
|       324 | 14963 | `	}` |
|     15861 | 14964 | `	if( pGen->xErr == 0 ){` |
|         - | 14965 | `		/* No available error consumer,return immediately */` |
|     15191 | 14966 | `		return SXRET_OK;` |
|         - | 14967 | `	}` |
|       675 | 14968 | `	switch(nErrType){` |
|       310 | 14969 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 14970 | `	case E_WARNING: zErr = "Warning";     break;` |
|       344 | 14971 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 14972 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 14973 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 14974 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 14975 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        16 | 14976 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 14977 | `	default:` |
|       ! 0 | 14978 | `		break;` |
|         - | 14979 | `	}` |
|       675 | 14980 | `	rc = SXRET_OK;` |
|         - | 14981 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       675 | 14982 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       675 | 14983 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       675 | 14984 | `	va_start(ap,zFormat);` |
|       675 | 14985 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       675 | 14986 | `	va_end(ap);` |
|       675 | 14987 | `	if( pFile ){` |
|       675 | 14988 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       335 | 14989 | `	}` |
|         - | 14990 | `	/* Append a new line */` |
|       675 | 14991 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       675 | 14992 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 14993 | `		/* Consume the generated error message */` |
|       675 | 14994 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       335 | 14995 | `	}` |
|       675 | 14996 | `	return rc;` |
|      7935 | 14997 | `}` |
|         - | 14998 |  |
