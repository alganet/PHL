# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7364/9090 lines (81.01%)

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
|    149316 |   139 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   140 | `{` |
|    149321 |   141 | `	GenBlock *pBlock = pCurrent;` |
|    336731 |   142 | `	for(;;){` |
|    673467 |   143 | `		if( pBlock->iFlags & iBlockType ){` |
|    149321 |   144 | `			iCount--; /* Decrement nesting level */` |
|    149321 |   145 | `			if( iCount < 1 ){` |
|         - |   146 | `				/* Block meet with the desired criteria */` |
|    149295 |   147 | `				return pBlock;` |
|         - |   148 | `			}` |
|        13 |   149 | `		}` |
|         - |   150 | `		/* Point to the upper block */` |
|    524177 |   151 | `		pBlock = pBlock->pParent;` |
|    524177 |   152 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   153 | `			/* Forbidden */` |
|        17 |   154 | `			break;` |
|         - |   155 | `		}` |
|         5 |   156 | `	}` |
|         - |   157 | `	/* No such block */` |
|        30 |   158 | `	return 0;` |
|     74663 |   159 | `}` |
|         - |   160 | `/*` |
|         - |   161 | ` * Initialize a freshly allocated block instance.` |
|         - |   162 | ` */` |
|  11543242 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|  11543247 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  11543247 |   173 | `	pBlock->pUserData   = pUserData;` |
|  11543247 |   174 | `	pBlock->pGen        = pGen;` |
|  11543247 |   175 | `	pBlock->iFlags      = iType;` |
|  11543247 |   176 | `	pBlock->pParent     = 0;` |
|  11543247 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11543247 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11543247 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|  11539414 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|  11539419 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  11539419 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|  11539419 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  11539419 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|  11539419 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  11539419 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    502041 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    502041 |   214 | `		pGen->nLoopId++;` |
|    502041 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    502041 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    502041 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    502041 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    251018 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|  11539419 |   221 | `	pGen->pCurrent = pBlock;` |
|  11539419 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   5525535 |   224 | `		*ppBlock = pBlock;` |
|   2762765 |   225 | `	}` |
|  11539419 |   226 | `	return SXRET_OK;` |
|   5769712 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|  11539402 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|  11539407 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|  11539407 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|  11539407 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|  11539398 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|  11539403 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  11539403 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|  11539403 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  11539403 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|  11539398 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|  11539403 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  11539403 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|  11539403 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    502033 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    251014 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|  11539403 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|  11539403 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|  11539403 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|  11539403 |   268 | `	return SXRET_OK;` |
|   5769704 |   269 | `}` |
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
|   4385124 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   4385129 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   4385129 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   4385129 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4385129 |   289 | `	return rc;` |
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
|   8092388 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   8092393 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  17430431 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9338043 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   3483657 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   5854391 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1469269 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   4385127 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4385127 |   322 | `		if( pInstr ){` |
|   4385127 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   4385127 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   4385127 |   326 | `			aFix[n].nJumpType = -1;` |
|   2192561 |   327 | `		}` |
|   2192566 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   8092393 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2821038 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2821043 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2821189 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|        11 |   387 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        11 |   388 | `			if( rc == SXERR_ABORT ){` |
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
|   2821041 |   400 | `	return SXRET_OK;` |
|   1410524 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  14718128 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  14718133 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  14718133 |   409 | `	if( pEntry == 0 ){` |
|   3842669 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|  10875469 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10875469 |   413 | `	return SXRET_OK;` |
|   7359069 |   414 | `}` |
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
|   3842664 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3842669 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3842669 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1921332 |   429 | `	}` |
|   3842669 |   430 | `	return SXRET_OK;` |
|         5 |   431 | `}` |
|         - |   432 | `/*` |
|         - |   433 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   434 | ` * in the constant table.` |
|         - |   435 | ` */` |
|   3554972 |   436 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   437 | `{` |
|         - |   438 | `	ph7_value *pObj;` |
|   3554977 |   439 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   440 | `	/* Reserve a new constant */` |
|   3554977 |   441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3554977 |   442 | `	if( pObj == 0 ){` |
|       ! 0 |   443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   444 | `		return 0;` |
|         - |   445 | `	}` |
|   3554977 |   446 | `	*pIdx = nIdx;` |
|         - |   447 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   448 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   449 | `	 */` |
|   3554977 |   450 | `	return pObj;` |
|   1777491 |   451 | `}` |
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
|   6955372 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6955377 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3477691 |   478 | `}` |
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
|      1080 |   520 | `static int GenStateIsBaseDigit(int c, int base)` |
|         5 |   521 | `{` |
|      1085 |   522 | `	if( base == 16 ){ return SyisHex(c); }` |
|       986 |   523 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|       707 |   524 | `	return SyisDigit(c);` |
|       545 |   525 | `}` |
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
|   3563646 |   542 | `static int GenStateFindBadNumericSeparator(` |
|         - |   543 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   544 | `{` |
|   3563651 |   545 | `	const char *z = pRaw->zString;` |
|   3563651 |   546 | `	sxu32 n = pRaw->nByte;` |
|   3563651 |   547 | `	int base = 10;` |
|         - |   548 | `	sxu32 i, start;` |
|   3563651 |   549 | `	if( n < 2 ) return 0;` |
|    744617 |   550 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|    103345 |   551 | `		base = 16;` |
|    692947 |   552 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       286 |   553 | `		base = 2;` |
|       142 |   554 | `	}` |
|   2806927 |   555 | `	for( i = 0; i < n; ++i ){` |
|   2062329 |   556 | `		if( z[i] != '_' ) continue;` |
|       548 |   557 | `		if( i > 0 && i + 1 < n` |
|       545 |   558 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|       545 |   559 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|       535 |   560 | `			continue; /* well-placed separator */` |
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
|    744603 |   573 | `	return 0;` |
|   1781828 |   574 | `}` |
|         - |   575 | `/*` |
|         - |   576 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   577 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   578 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   579 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   580 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   581 | ` * so callers can bail from the current construct).` |
|         - |   582 | ` */` |
|   3563646 |   583 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   584 | `{` |
|   3563651 |   585 | `	const char *zBad = 0;` |
|   3563651 |   586 | `	sxu32 nBad = 0;` |
|         - |   587 | `	SyString sBad;` |
|         - |   588 | `	sxi32 rc;` |
|   3563651 |   589 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   3563637 |   590 | `		return SXRET_OK;` |
|         - |   591 | `	}` |
|        18 |   592 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   593 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   594 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   595 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   596 | `		return SXERR_ABORT;` |
|         - |   597 | `	}` |
|        18 |   598 | `	return SXERR_SYNTAX;` |
|   1781828 |   599 | `}` |
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
|   3563632 |   616 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   617 | `	SyMemBackend *pAlloc,` |
|         - |   618 | `	const SyString *pToken,` |
|         - |   619 | `	char *zScratch, sxu32 nScratch,` |
|         - |   620 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   621 | `{` |
|         - |   622 | `	sxu32 i, j;` |
|   3563637 |   623 | `	int hasUnderscore = 0;` |
|         - |   624 | `	char *zBuf;` |
|   3563637 |   625 | `	*pzAlloc = 0;` |
|   8442905 |   626 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   4879527 |   627 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   2439639 |   628 | `	}` |
|   3563637 |   629 | `	if( !hasUnderscore ){` |
|   3563383 |   630 | `		SyStringDupPtr(pOut, pToken);` |
|   3563383 |   631 | `		return SXRET_OK;` |
|         - |   632 | `	}` |
|       255 |   633 | `	if( pToken->nByte <= nScratch ){` |
|       253 |   634 | `		zBuf = zScratch;` |
|       127 |   635 | `	}else{` |
|         3 |   636 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|         3 |   637 | `		if( zBuf == 0 ){` |
|       ! 0 |   638 | `			return SXERR_ABORT;` |
|         - |   639 | `		}` |
|         3 |   640 | `		*pzAlloc = zBuf;` |
|         - |   641 | `	}` |
|       255 |   642 | `	j = 0;` |
|      2913 |   643 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|      2659 |   644 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|      1330 |   645 | `	}` |
|       255 |   646 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|       255 |   647 | `	return SXRET_OK;` |
|   1781821 |   648 | `}` |
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
|   3555006 |   684 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   685 | `{` |
|   3555011 |   686 | `	const char *z = pNum->zString;` |
|   3555011 |   687 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   688 | `	const char *p, *q;` |
|         - |   689 | `	int n;` |
|   3555011 |   690 | `	*pbDecimal = FALSE;` |
|   3555011 |   691 | `	if( z >= zEnd ){` |
|       ! 0 |   692 | `		return FALSE;` |
|         - |   693 | `	}` |
|   3555011 |   694 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|         - |   695 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|    103343 |   696 | `		p = z + 2;` |
|    130119 |   697 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    421209 |   698 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|    103343 |   699 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|    103337 |   700 | `			return FALSE;` |
|         - |   701 | `		}` |
|         7 |   702 | `		{ ph7_real dv = 0;` |
|       103 |   703 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|        97 |   704 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|        49 |   705 | `		  }` |
|         7 |   706 | `		  *pReal = dv;` |
|         - |   707 | `		}` |
|         7 |   708 | `		return TRUE;` |
|   3451673 |   709 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|         - |   710 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|       283 |   711 | `		p = z + 2;` |
|       331 |   712 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|      2158 |   713 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|       283 |   714 | `		if( n <= 63 ){` |
|       281 |   715 | `			return FALSE;` |
|         - |   716 | `		}` |
|         3 |   717 | `		{ ph7_real dv = 0;` |
|       195 |   718 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|       129 |   719 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|        65 |   720 | `		  }` |
|         3 |   721 | `		  *pReal = dv;` |
|         - |   722 | `		}` |
|         3 |   723 | `		return TRUE;` |
|   3451391 |   724 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
|         - |   725 | `		/* PHP 8.1 explicit octal 0o/0O: 21 significant octal digits fit in int64. */` |
|        17 |   726 | `		p = z + 2;` |
|        21 |   727 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|        85 |   728 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|        17 |   729 | `		if( n <= 21 ){` |
|        17 |   730 | `			return FALSE;` |
|         - |   731 | `		}` |
|       ! 0 |   732 | `		{ ph7_real dv = 0;` |
|       ! 0 |   733 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|       ! 0 |   734 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|       ! 0 |   735 | `		  }` |
|       ! 0 |   736 | `		  *pReal = dv;` |
|         - |   737 | `		}` |
|       ! 0 |   738 | `		return TRUE;` |
|   3451375 |   739 | `	}else if( z[0] == '0' ){` |
|         - |   740 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   741 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   742 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1279611 |   743 | `		p = z;` |
|   2559219 |   744 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1291339 |   745 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1279611 |   746 | `		if( n <= 21 ){` |
|   1279609 |   747 | `			return FALSE;` |
|         - |   748 | `		}` |
|         3 |   749 | `		{ ph7_real dv = 0;` |
|        47 |   750 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|        45 |   751 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|        23 |   752 | `		  }` |
|         3 |   753 | `		  *pReal = dv;` |
|         - |   754 | `		}` |
|         3 |   755 | `		return TRUE;` |
|         - |   756 | `	}` |
|         - |   757 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|         - |   758 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|         - |   759 | `	 * for php-exact rounding. */` |
|   2171769 |   760 | `	p = z;` |
|   2171769 |   761 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   5179503 |   762 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   2171769 |   763 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   764 | `		*pbDecimal = TRUE;` |
|        25 |   765 | `		return TRUE;` |
|         - |   766 | `	}` |
|   2171745 |   767 | `	return FALSE;` |
|   1777508 |   768 | `}` |
|   3563618 |   769 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   770 | `{` |
|   3563623 |   771 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   3563623 |   772 | `	sxu32 nIdx = 0;` |
|         - |   773 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   3563623 |   774 | `	char *zAlloc = 0;` |
|         - |   775 | `	SyString sNum;` |
|         - |   776 | `	sxi32 rc;` |
|   1781809 |   777 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   3563623 |   778 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   3563623 |   779 | `	if( rc != SXRET_OK ){` |
|        14 |   780 | `		return rc;` |
|         - |   781 | `	}` |
|   5345417 |   782 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1781804 |   783 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   3563613 |   784 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   785 | `		return SXERR_ABORT;` |
|         - |   786 | `	}` |
|   3563613 |   787 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   788 | `		ph7_value *pObj;` |
|         - |   789 | `		sxi64 iValue;` |
|   3555011 |   790 | `		ph7_real rOverflow = 0;` |
|   3555011 |   791 | `		int bDecimalOverflow = 0;` |
|   3555011 |   792 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|         - |   793 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|         - |   794 | `			 * float instead of wrapping/dropping digits. */` |
|        35 |   795 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        35 |   796 | `			if( pObj == 0 ){` |
|       ! 0 |   797 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   798 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   799 | `				return SXERR_ABORT;` |
|         - |   800 | `			}` |
|        35 |   801 | `			if( bDecimalOverflow ){` |
|         - |   802 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|        25 |   803 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|        25 |   804 | `				PH7_MemObjToReal(pObj);` |
|        13 |   805 | `			}else{` |
|        11 |   806 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|         - |   807 | `			}` |
|        18 |   808 | `		}else{` |
|   3554977 |   809 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   3554977 |   810 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   3554977 |   811 | `			if( pObj == 0 ){` |
|       ! 0 |   812 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   813 | `				return SXERR_ABORT;` |
|         - |   814 | `			}` |
|   3554977 |   815 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   816 | `		}` |
|   1777508 |   817 | `	}else{` |
|         - |   818 | `		/* Real number */` |
|         - |   819 | `		ph7_value *pObj;` |
|         - |   820 | `		/* Reserve a new constant */` |
|      8607 |   821 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      8607 |   822 | `		if( pObj == 0 ){` |
|       ! 0 |   823 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   824 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   825 | `			return SXERR_ABORT;` |
|         - |   826 | `		}` |
|      8607 |   827 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      8607 |   828 | `		PH7_MemObjToReal(pObj);` |
|         - |   829 | `	}` |
|   3563613 |   830 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   831 | `	/* Emit the load constant instruction */` |
|   3563613 |   832 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   833 | `	/* Node successfully compiled */` |
|   3563613 |   834 | `	return SXRET_OK;` |
|   1781814 |   835 | `}` |
|         - |   836 | `/*` |
|         - |   837 | ` * Compile a single quoted string.` |
|         - |   838 | ` * According to the PHP language reference manual:` |
|         - |   839 | ` *` |
|         - |   840 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|         - |   841 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|         - |   842 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|         - |   843 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|         - |   844 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|         - |   845 | ` *` |
|         - |   846 | ` */` |
|   5102770 |   847 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   848 | `{` |
|   5102775 |   849 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   850 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   851 | `	ph7_value *pObj;` |
|         - |   852 | `	sxu32 nIdx;` |
|   5102775 |   853 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   854 | `	/* Delimit the string */` |
|   5102775 |   855 | `	zIn  = pStr->zString;` |
|   5102775 |   856 | `	zEnd = &zIn[pStr->nByte];` |
|   5102775 |   857 | `	if( zIn >= zEnd ){` |
|         - |   858 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   859 | `		 * rather than reserving a new object each time. */` |
|    332907 |   860 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    332907 |   861 | `		return SXRET_OK;` |
|         - |   862 | `	}` |
|   4769873 |   863 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   864 | `		/* Already processed,emit the load constant instruction` |
|         - |   865 | `		 * and return.` |
|         - |   866 | `		 */` |
|   2826803 |   867 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2826803 |   868 | `		return SXRET_OK;` |
|         - |   869 | `	}` |
|         - |   870 | `	/* Reserve a new constant */` |
|   1943075 |   871 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1943075 |   872 | `	if( pObj == 0 ){` |
|       ! 0 |   873 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   874 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   875 | `		return SXERR_ABORT;` |
|         - |   876 | `	}` |
|   1943075 |   877 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   878 | `	/* Compile the node */` |
|   1990944 |   879 | `	for(;;){` |
|   3981893 |   880 | `		if( zIn >= zEnd ){` |
|         - |   881 | `			/* End of input */` |
|   1943075 |   882 | `			break;` |
|         - |   883 | `		}` |
|   2038823 |   884 | `		zCur = zIn;` |
|  40479703 |   885 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  38440885 |   886 | `			zIn++;` |
|         5 |   887 | `		}` |
|   2038823 |   888 | `		if( zIn > zCur ){` |
|         - |   889 | `			/* Append raw contents*/` |
|   2000539 |   890 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   1000267 |   891 | `		}` |
|   2038823 |   892 | `		zIn++;` |
|   2038823 |   893 | `		if( zIn < zEnd ){` |
|    130197 |   894 | `			if( zIn[0] == '\\' ){` |
|         - |   895 | `				/* A literal backslash */` |
|     30637 |   896 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    114881 |   897 | `			}else if( zIn[0] == '\'' ){` |
|         - |   898 | `				/* A single quote */` |
|        11 |   899 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   900 | `			}else{` |
|         - |   901 | `				/* verbatim copy */` |
|     99555 |   902 | `				zIn--;` |
|     99555 |   903 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     99555 |   904 | `				zIn++;` |
|         - |   905 | `			}` |
|     65096 |   906 | `		}` |
|         - |   907 | `		/* Advance the stream cursor */` |
|   2038823 |   908 | `		zIn++;` |
|         5 |   909 | `	}` |
|         - |   910 | `	/* Emit the load constant instruction */` |
|   1943075 |   911 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1943075 |   912 | `	if( pStr->nByte < 1024 ){` |
|         - |   913 | `		/* Install in the literal table */` |
|   1943075 |   914 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    971535 |   915 | `	}` |
|         - |   916 | `	/* Node successfully compiled */` |
|   1943075 |   917 | `	return SXRET_OK;` |
|   2551390 |   918 | `}` |
|         - |   919 | `/*` |
|         - |   920 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|         - |   921 | ` *` |
|         - |   922 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|         - |   923 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|         - |   924 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|         - |   925 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|         - |   926 | ` * original source buffer — the buffer is stable through compilation.` |
|         - |   927 | ` *` |
|         - |   928 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|         - |   929 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|         - |   930 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|         - |   931 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|         - |   932 | ` *     at least N)" — line too short, or first differing byte is not` |
|         - |   933 | ` *     whitespace.` |
|         - |   934 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|         - |   935 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|         - |   936 | ` */` |
|       114 |   937 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|         4 |   938 | `{` |
|       118 |   939 | `	SyString *pIn = &pGen->pIn->sData;` |
|       118 |   940 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - |   941 | `	const char *zPrefix;` |
|         - |   942 | `	const char *z, *zEnd;` |
|         - |   943 | `	char *zBuf, *zDst;` |
|       118 |   944 | `	if( nIndent == 0 ){` |
|         - |   945 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|        72 |   946 | `		*pOut = *pIn;` |
|        72 |   947 | `		return SXRET_OK;` |
|         - |   948 | `	}` |
|         - |   949 | `	/* Recover the marker indent prefix from the original source buffer.` |
|         - |   950 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|         - |   951 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|         - |   952 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|         - |   953 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|         - |   954 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|        47 |   955 | `	zPrefix = pIn->zString + pIn->nByte;` |
|        47 |   956 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|       ! 0 |   957 | `		zPrefix += 2;` |
|       ! 0 |   958 | `	}else{` |
|        47 |   959 | `		zPrefix += 1;` |
|         - |   960 | `	}` |
|         - |   961 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|        47 |   962 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|        47 |   963 | `	if( zBuf == 0 ){` |
|       ! 0 |   964 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |   965 | `		return SXERR_ABORT;` |
|         - |   966 | `	}` |
|        47 |   967 | `	zDst = zBuf;` |
|        47 |   968 | `	z = pIn->zString;` |
|        47 |   969 | `	zEnd = z + pIn->nByte;` |
|       129 |   970 | `	while( z < zEnd ){` |
|        71 |   971 | `		const char *zLine = z;` |
|         - |   972 | `		sxu32 nLine;` |
|         - |   973 | `		int bEmpty;` |
|       799 |   974 | `		while( z < zEnd && z[0] != '\n' ){` |
|       731 |   975 | `			z++;` |
|         3 |   976 | `		}` |
|        71 |   977 | `		nLine = (sxu32)(z - zLine);` |
|        71 |   978 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|        71 |   979 | `		if( !bEmpty ){` |
|         - |   980 | `			sxu32 i;` |
|        67 |   981 | `			if( nLine < nIndent ){` |
|       ! 0 |   982 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   983 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       ! 0 |   984 | `					nIndent);` |
|       ! 0 |   985 | `				return SXERR_ABORT;` |
|         - |   986 | `			}` |
|       269 |   987 | `			for( i = 0; i < nIndent; i++ ){` |
|       213 |   988 | `				if( zLine[i] != zPrefix[i] ){` |
|        10 |   989 | `					unsigned char c = (unsigned char)zLine[i];` |
|        10 |   990 | `					if( c == ' ' \|\| c == '\t' ){` |
|         5 |   991 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   992 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|         3 |   993 | `					}else{` |
|         7 |   994 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   995 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|         2 |   996 | `							nIndent);` |
|         - |   997 | `					}` |
|        10 |   998 | `					return SXERR_ABORT;` |
|         - |   999 | `				}` |
|       103 |  1000 | `			}` |
|        57 |  1001 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|        57 |  1002 | `			zDst += nLine - nIndent;` |
|        33 |  1003 | `		}else if( nLine == 1 ){` |
|         - |  1004 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|       ! 0 |  1005 | `			*zDst++ = '\r';` |
|       ! 0 |  1006 | `		}` |
|        61 |  1007 | `		if( z < zEnd ){` |
|        25 |  1008 | `			*zDst++ = '\n';` |
|        25 |  1009 | `			z++;` |
|        12 |  1010 | `		}` |
|         1 |  1011 | `	}` |
|        37 |  1012 | `	pOut->zString = zBuf;` |
|        37 |  1013 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|        37 |  1014 | `	return SXRET_OK;` |
|        61 |  1015 | `}` |
|         - |  1016 | `/*` |
|         - |  1017 | ` * Compile a nowdoc string.` |
|         - |  1018 | ` * According to the PHP language reference manual:` |
|         - |  1019 | ` *` |
|         - |  1020 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - |  1021 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - |  1022 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|         - |  1023 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|         - |  1024 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|         - |  1025 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|         - |  1026 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|         - |  1027 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|         - |  1028 | ` *  of the closing identifier.` |
|         - |  1029 | ` */` |
|        48 |  1030 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 |  1031 | `{` |
|         - |  1032 | `	SyString sStripped;` |
|         - |  1033 | `	SyString *pStr;` |
|         - |  1034 | `	ph7_value *pObj;` |
|         - |  1035 | `	sxu32 nIdx;` |
|         - |  1036 | `	sxi32 rc;` |
|        52 |  1037 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        52 |  1038 | `	if( rc != SXRET_OK ){` |
|         6 |  1039 | `		return rc;` |
|         - |  1040 | `	}` |
|        46 |  1041 | `	pStr = &sStripped;` |
|        46 |  1042 | `	nIdx = 0; /* Prevent compiler warning */` |
|        46 |  1043 | `	if( pStr->nByte <= 0 ){` |
|         - |  1044 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|         - |  1045 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|         7 |  1046 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|         7 |  1047 | `		return SXRET_OK;` |
|         - |  1048 | `	}` |
|         - |  1049 | `	/* Reserve a new constant */` |
|        40 |  1050 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        40 |  1051 | `	if( pObj == 0 ){` |
|       ! 0 |  1052 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1053 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  1054 | `		return SXERR_ABORT;` |
|         - |  1055 | `	}` |
|         - |  1056 | `	/* No processing is done here, simply a memcpy() operation */` |
|        40 |  1057 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|         - |  1058 | `	/* Emit the load constant instruction */` |
|        40 |  1059 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  1060 | `	/* Node successfully compiled */` |
|        40 |  1061 | `	return SXRET_OK;` |
|        28 |  1062 | `}` |
|         - |  1063 | `/*` |
|         - |  1064 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|         - |  1065 | ` * According to the PHP language reference manual` |
|         - |  1066 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|         - |  1067 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|         - |  1068 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|         - |  1069 | ` *  property in a string with a minimum of effort.` |
|         - |  1070 | ` *  Simple syntax` |
|         - |  1071 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|         - |  1072 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|         - |  1073 | ` *   the end of the name.` |
|         - |  1074 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|         - |  1075 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|         - |  1076 | ` *   as to simple variables.` |
|         - |  1077 | ` *  Complex (curly) syntax` |
|         - |  1078 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|         - |  1079 | ` *   of complex expressions.` |
|         - |  1080 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|         - |  1081 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|         - |  1082 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|         - |  1083 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|         - |  1084 | ` */` |
|      2638 |  1085 | `static sxi32 GenStateProcessStringExpression(` |
|         - |  1086 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1087 | `	sxu32 nLine,         /* Line number */` |
|         - |  1088 | `	const char *zIn,     /* Raw expression */` |
|         - |  1089 | `	const char *zEnd     /* End of the expression */` |
|         - |  1090 | `	)` |
|         5 |  1091 | `{` |
|         - |  1092 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1093 | `	SySet sToken;` |
|         - |  1094 | `	sxi32 rc;` |
|         - |  1095 | `	/* Initialize the token set */` |
|      2643 |  1096 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1097 | `	/* Preallocate some slots */` |
|      2643 |  1098 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1099 | `	/* Tokenize the text */` |
|      2643 |  1100 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1101 | `	/* Swap delimiter */` |
|      2643 |  1102 | `	pTmpIn  = pGen->pIn;` |
|      2643 |  1103 | `	pTmpEnd = pGen->pEnd;` |
|      2643 |  1104 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2643 |  1105 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1106 | `	/* Compile the expression */` |
|      2643 |  1107 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1108 | `	/* Restore token stream */` |
|      2643 |  1109 | `	pGen->pIn  = pTmpIn;` |
|      2643 |  1110 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1111 | `	/* Release the token set */` |
|      2643 |  1112 | `	SySetRelease(&sToken);` |
|         - |  1113 | `	/* Compilation result */` |
|      2643 |  1114 | `	return rc;` |
|         5 |  1115 | `}` |
|         - |  1116 | `/*` |
|         - |  1117 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1118 | ` */` |
|    122094 |  1119 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1120 | `{` |
|         - |  1121 | `	ph7_value *pConstObj;` |
|    122099 |  1122 | `	sxu32 nIdx = 0;` |
|         - |  1123 | `	/* Reserve a new constant */` |
|    122099 |  1124 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    122099 |  1125 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1126 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1127 | `		return 0;` |
|         - |  1128 | `	}` |
|    122099 |  1129 | `	(*pCount)++;` |
|    122099 |  1130 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1131 | `	/* Emit the load constant instruction */` |
|    122099 |  1132 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    122099 |  1133 | `	return pConstObj;` |
|     61052 |  1134 | `}` |
|         - |  1135 | `/*` |
|         - |  1136 | ` * Compile a double quoted/heredoc string.` |
|         - |  1137 | ` * According to the PHP language reference manual` |
|         - |  1138 | ` * Heredoc` |
|         - |  1139 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - |  1140 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - |  1141 | ` *  to close the quotation.` |
|         - |  1142 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - |  1143 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - |  1144 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - |  1145 | ` *  Warning` |
|         - |  1146 | ` *  It is very important to note that the line with the closing identifier must contain` |
|         - |  1147 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|         - |  1148 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|         - |  1149 | ` *  It's also important to realize that the first character before the closing identifier must` |
|         - |  1150 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|         - |  1151 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|         - |  1152 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|         - |  1153 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|         - |  1154 | ` *  the end of the current file, a parse error will result at the last line.` |
|         - |  1155 | ` *  Heredocs can not be used for initializing class properties.` |
|         - |  1156 | ` * Double quoted` |
|         - |  1157 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|         - |  1158 | ` *  Escaped characters Sequence 	Meaning` |
|         - |  1159 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|         - |  1160 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|         - |  1161 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|         - |  1162 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|         - |  1163 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|         - |  1164 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|         - |  1165 | ` *  \\ backslash` |
|         - |  1166 | ` *  \$ dollar sign` |
|         - |  1167 | ` *  \" double-quote` |
|         - |  1168 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|         - |  1169 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|         - |  1170 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|         - |  1171 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|         - |  1172 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|         - |  1173 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|         - |  1174 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|         - |  1175 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|         - |  1176 | ` * See string parsing for details.` |
|         - |  1177 | ` */` |
|         - |  1178 | `/*` |
|         - |  1179 | ` * Line number of an escape sequence inside the string body being compiled:` |
|         - |  1180 | ` * the token's line plus every newline before the escape (php reports the` |
|         - |  1181 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|         - |  1182 | ` * on the line after the '<<<' marker, hence the +1.` |
|         - |  1183 | ` */` |
|         6 |  1184 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|         3 |  1185 | `{` |
|         9 |  1186 | `	const char *z = pGen->pIn->sData.zString;` |
|         9 |  1187 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|        15 |  1188 | `	for( ; z < zPos ; z++ ){` |
|         9 |  1189 | `		if( z[0] == '\n' ){` |
|       ! 0 |  1190 | `			nLine++;` |
|       ! 0 |  1191 | `		}` |
|         6 |  1192 | `	}` |
|         9 |  1193 | `	return nLine;` |
|         3 |  1194 | `}` |
|         - |  1195 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|         - |  1196 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|    120528 |  1197 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1198 | `{` |
|    120533 |  1199 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1200 | `	const char *zIn,*zCur,*zEnd;` |
|    120533 |  1201 | `	ph7_value *pObj = 0;` |
|         - |  1202 | `	sxi32 iCons;` |
|         - |  1203 | `	sxi32 rc;` |
|         - |  1204 | `	/* Delimit the string */` |
|    120533 |  1205 | `	zIn  = pStr->zString;` |
|    120533 |  1206 | `	zEnd = &zIn[pStr->nByte];` |
|    120533 |  1207 | `	if( zIn >= zEnd ){` |
|         - |  1208 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1209 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1210 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1211 | `		 */` |
|       413 |  1212 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       413 |  1213 | `		return SXRET_OK;` |
|         - |  1214 | `	}` |
|    120125 |  1215 | `	zCur = 0;` |
|         - |  1216 | `	/* Compile the node */` |
|    120125 |  1217 | `	iCons = 0;` |
|     61377 |  1218 | `	for(;;){` |
|    163381 |  1219 | `		zCur = zIn;` |
|   1661599 |  1220 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1500861 |  1221 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        69 |  1222 | `				break;` |
|   1500734 |  1223 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2516 |  1224 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1258 |  1225 | `					break;` |
|         - |  1226 | `			}` |
|   1498223 |  1227 | `			zIn++;` |
|         5 |  1228 | `		}` |
|    163381 |  1229 | `		if( zIn > zCur ){` |
|     95275 |  1230 | `			if( pObj == 0 ){` |
|     94649 |  1231 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     94649 |  1232 | `				if( pObj == 0 ){` |
|       ! 0 |  1233 | `					return SXERR_ABORT;` |
|         - |  1234 | `				}` |
|     47322 |  1235 | `			}` |
|     95275 |  1236 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     47635 |  1237 | `		}` |
|    163381 |  1238 | `		if( zIn >= zEnd ){` |
|    120123 |  1239 | `			break;` |
|         - |  1240 | `		}` |
|     43263 |  1241 | `		if( zIn[0] == '\\' ){` |
|     40625 |  1242 | `			const char *zPtr = 0;` |
|         - |  1243 | `			sxu32 n;` |
|     40625 |  1244 | `			zIn++;` |
|     40625 |  1245 | `			if( pObj == 0 ){` |
|     27455 |  1246 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27455 |  1247 | `				if( pObj == 0 ){` |
|       ! 0 |  1248 | `					return SXERR_ABORT;` |
|         - |  1249 | `				}` |
|     13725 |  1250 | `			}` |
|     40625 |  1251 | `			if( zIn >= zEnd ){` |
|         - |  1252 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1253 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1254 | `				break;` |
|         - |  1255 | `			}` |
|     40623 |  1256 | `			n = sizeof(char); /* size of conversion */` |
|     40623 |  1257 | `			switch( zIn[0] ){` |
|        15 |  1258 | `			case '$':` |
|         - |  1259 | `				/* Dollar sign */` |
|        33 |  1260 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        33 |  1261 | `				break;` |
|        57 |  1262 | `			case '\\':` |
|         - |  1263 | `				/* A literal backslash */` |
|       119 |  1264 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       119 |  1265 | `				break;` |
|         1 |  1266 | `			case 'e':` |
|         - |  1267 | `				/* Escape (ESC) ASCII code 27 */` |
|         3 |  1268 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|         3 |  1269 | `				break;` |
|         4 |  1270 | `			case 'f':` |
|         - |  1271 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|         9 |  1272 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|         9 |  1273 | `				break;` |
|     17720 |  1274 | `			case 'n':` |
|         - |  1275 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     35445 |  1276 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     35445 |  1277 | `				break;` |
|        27 |  1278 | `			case 'r':` |
|         - |  1279 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1280 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1281 | `				break;` |
|      1943 |  1282 | `			case 't':` |
|         - |  1283 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      3891 |  1284 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      3891 |  1285 | `				break;` |
|         3 |  1286 | `			case 'v':` |
|         - |  1287 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|         7 |  1288 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|         7 |  1289 | `				break;` |
|       141 |  1290 | `			case '"':` |
|       287 |  1291 | `				if( bHeredoc ){` |
|         - |  1292 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|         5 |  1293 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|         3 |  1294 | `				}else{` |
|         - |  1295 | `					/* Double quote */` |
|       283 |  1296 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|         - |  1297 | `				}` |
|       287 |  1298 | `				break;` |
|        24 |  1299 | `			case '0': case '1': case '2': case '3':` |
|         - |  1300 | `			case '4': case '5': case '6': case '7': {` |
|         - |  1301 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|         - |  1302 | `				 * warns and wraps to the low byte, matching php 8. */` |
|        50 |  1303 | `				int c = 0;` |
|         - |  1304 | `				char cOut;` |
|       144 |  1305 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|       122 |  1306 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|        14 |  1307 | `						break;` |
|         - |  1308 | `					}` |
|        96 |  1309 | `					c = c * 8 + (zPtr[0] - '0');` |
|        49 |  1310 | `				}` |
|        50 |  1311 | `				if( c > 0xFF ){` |
|         - |  1312 | `					SyString sSeq;` |
|         3 |  1313 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|         3 |  1314 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1315 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|         3 |  1316 | `					c &= 0xFF;` |
|         1 |  1317 | `				}` |
|        50 |  1318 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|        50 |  1319 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|        50 |  1320 | `				n = (sxu32)(zPtr-zIn);` |
|        50 |  1321 | `				break;` |
|         - |  1322 | `			}` |
|       349 |  1323 | `			case 'x':` |
|      1047 |  1324 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|         - |  1325 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|       696 |  1326 | `					int c = SyHexToint(zIn[1]);` |
|         - |  1327 | `					char cOut;` |
|       696 |  1328 | `					n += sizeof(char);` |
|       696 |  1329 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|       692 |  1330 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|       692 |  1331 | `						n += sizeof(char);` |
|       345 |  1332 | `					}` |
|       696 |  1333 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|       696 |  1334 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       349 |  1335 | `				}else{` |
|         - |  1336 | `					/* Not an escape: keep the backslash, as php does */` |
|         5 |  1337 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|         - |  1338 | `				}` |
|       700 |  1339 | `				break;` |
|         9 |  1340 | `			case 'u':` |
|        18 |  1341 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|        22 |  1342 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|         - |  1343 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|         - |  1344 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|         - |  1345 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|         - |  1346 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|         - |  1347 | `					 * followed by {$...} curly interpolation. */` |
|        15 |  1348 | `					sxu32 nCp = 0;` |
|        15 |  1349 | `					zPtr = &zIn[2];` |
|        59 |  1350 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|        46 |  1351 | `						if( nCp <= 0x10FFFF ){` |
|         - |  1352 | `							/* stop accumulating once out of range: keeps a long` |
|         - |  1353 | `							 * digit run from wrapping sxu32 */` |
|        46 |  1354 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|        22 |  1355 | `						}` |
|        46 |  1356 | `						zPtr++;` |
|         2 |  1357 | `					}` |
|        15 |  1358 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|         - |  1359 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|         - |  1360 | `						 * malformed sequence so later errors are still reported. */` |
|         3 |  1361 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1362 | `							"Invalid UTF-8 codepoint escape sequence");` |
|         3 |  1363 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1364 | `							return SXERR_ABORT;` |
|         - |  1365 | `						}` |
|         3 |  1366 | `						n = (sxu32)(zPtr-zIn);` |
|         3 |  1367 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|         3 |  1368 | `							n += sizeof(char);` |
|         1 |  1369 | `						}` |
|         3 |  1370 | `						break;` |
|         - |  1371 | `					}` |
|        12 |  1372 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|        12 |  1373 | `					if( nCp > 0x10FFFF ){` |
|         3 |  1374 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1375 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|         3 |  1376 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1377 | `							return SXERR_ABORT;` |
|         - |  1378 | `						}` |
|         3 |  1379 | `						break;` |
|         - |  1380 | `					}` |
|         - |  1381 | `					{` |
|         - |  1382 | `						char zUtf[4];` |
|         9 |  1383 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|         9 |  1384 | `						SX_WRITE_UTF8(zOut,nCp);` |
|         9 |  1385 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|         - |  1386 | `					}` |
|         5 |  1387 | `				}else{` |
|         - |  1388 | `					/* Not an escape: keep the backslash, as php does */` |
|         7 |  1389 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|         - |  1390 | `				}` |
|        15 |  1391 | `				break;` |
|        16 |  1392 | `			default:` |
|         - |  1393 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|         - |  1394 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|         - |  1395 | `				 * in the source buffer — one batched append. */` |
|        33 |  1396 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|        32 |  1397 | `				break;` |
|         - |  1398 | `			}` |
|         - |  1399 | `			/* Advance the stream cursor */` |
|     40623 |  1400 | `			zIn += n;` |
|     40623 |  1401 | `			continue;` |
|         - |  1402 | `		}` |
|      2643 |  1403 | `		if( zIn[0] == '{' ){` |
|         - |  1404 | `			/* Curly syntax */` |
|         - |  1405 | `			const char *zExpr;` |
|       135 |  1406 | `			sxi32 iNest = 1;` |
|       135 |  1407 | `			zIn++;` |
|       135 |  1408 | `			zExpr = zIn;` |
|         - |  1409 | `			/* Synchronize with the next closing curly braces */` |
|      1323 |  1410 | `			while( zIn < zEnd ){` |
|      1323 |  1411 | `				if( zIn[0] == '{' ){` |
|         - |  1412 | `					/* Increment nesting level */` |
|         3 |  1413 | `					iNest++;` |
|      1322 |  1414 | `				}else if(zIn[0] == '}' ){` |
|         - |  1415 | `					/* Decrement nesting level */` |
|       137 |  1416 | `					iNest--;` |
|       137 |  1417 | `					if( iNest <= 0 ){` |
|       135 |  1418 | `						break;` |
|         - |  1419 | `					}` |
|         1 |  1420 | `				}` |
|      1191 |  1421 | `				zIn++;` |
|         3 |  1422 | `			}` |
|         - |  1423 | `			/* Process the expression */` |
|       135 |  1424 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|       135 |  1425 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1426 | `				return SXERR_ABORT;` |
|         - |  1427 | `			}` |
|       135 |  1428 | `			if( rc != SXERR_EMPTY ){` |
|       135 |  1429 | `				++iCons;` |
|        66 |  1430 | `			}` |
|       135 |  1431 | `			if( zIn < zEnd ){` |
|         - |  1432 | `				/* Jump the trailing curly */` |
|       135 |  1433 | `				zIn++;` |
|        66 |  1434 | `			}` |
|        69 |  1435 | `		}else{` |
|         - |  1436 | `			/* Simple syntax */` |
|      2511 |  1437 | `			const char *zExpr = zIn;` |
|         - |  1438 | `			/* Assemble variable name */` |
|      1278 |  1439 | `			for(;;){` |
|         - |  1440 | `				/* Jump leading dollars */` |
|      5067 |  1441 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2511 |  1442 | `					zIn++;` |
|         5 |  1443 | `				}` |
|      1278 |  1444 | `				for(;;){` |
|     13087 |  1445 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9253 |  1446 | `						zIn++;` |
|         5 |  1447 | `					}` |
|      2561 |  1448 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1449 | `						/* UTF-8 stream */` |
|       ! 0 |  1450 | `						zIn++;` |
|       ! 0 |  1451 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1452 | `							zIn++;` |
|       ! 0 |  1453 | `						}` |
|       ! 0 |  1454 | `						continue;` |
|         - |  1455 | `					}` |
|      2561 |  1456 | `					break;` |
|       ! 0 |  1457 | `				}` |
|      2561 |  1458 | `				if( zIn >= zEnd ){` |
|       271 |  1459 | `					break;` |
|         - |  1460 | `				}` |
|      2295 |  1461 | `				if( zIn[0] == '[' ){` |
|        12 |  1462 | `					sxi32 iSquare = 1;` |
|        12 |  1463 | `					zIn++;` |
|        28 |  1464 | `					while( zIn < zEnd ){` |
|        28 |  1465 | `						if( zIn[0] == '[' ){` |
|       ! 0 |  1466 | `							iSquare++;` |
|        28 |  1467 | `						}else if (zIn[0] == ']' ){` |
|        12 |  1468 | `							iSquare--;` |
|        12 |  1469 | `							if( iSquare <= 0 ){` |
|        12 |  1470 | `								break;` |
|         - |  1471 | `							}` |
|       ! 0 |  1472 | `						}` |
|        18 |  1473 | `						zIn++;` |
|         2 |  1474 | `					}` |
|        12 |  1475 | `					if( zIn < zEnd ){` |
|        12 |  1476 | `						zIn++;` |
|         5 |  1477 | `					}` |
|        12 |  1478 | `					break;` |
|      2285 |  1479 | `				}else if(zIn[0] == '{' ){` |
|         6 |  1480 | `					sxi32 iCurly = 1;` |
|         6 |  1481 | `					zIn++;` |
|        18 |  1482 | `					while( zIn < zEnd ){` |
|        16 |  1483 | `						if( zIn[0] == '{' ){` |
|       ! 0 |  1484 | `							iCurly++;` |
|        16 |  1485 | `						}else if (zIn[0] == '}' ){` |
|         3 |  1486 | `							iCurly--;` |
|         3 |  1487 | `							if( iCurly <= 0 ){` |
|         3 |  1488 | `								break;` |
|         - |  1489 | `							}` |
|       ! 0 |  1490 | `						}` |
|        14 |  1491 | `						zIn++;` |
|         2 |  1492 | `					}` |
|         6 |  1493 | `					if( zIn < zEnd ){` |
|         3 |  1494 | `						zIn++;` |
|         1 |  1495 | `					}` |
|         6 |  1496 | `					break;` |
|      2281 |  1497 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1498 | `					/* Member access operator '->' */` |
|        53 |  1499 | `					zIn += 2;` |
|      2256 |  1500 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1501 | `					/* Static member access operator '::' */` |
|       ! 0 |  1502 | `					zIn += 2;` |
|       ! 0 |  1503 | `				}else{` |
|      1118 |  1504 | `					break;` |
|         - |  1505 | `				}` |
|         3 |  1506 | `			}` |
|         - |  1507 | `			/*` |
|         - |  1508 | `			 * "$a[name]" — php's SIMPLE syntax takes an unquoted subscript as the string key` |
|         - |  1509 | `			 * 'name', never as a constant. PH7 handed "$a[name]" straight to the expression` |
|         - |  1510 | `			 * compiler, where the bare word only resolved because an unknown constant used to` |
|         - |  1511 | `			 * fall back to its own name as a string. With undefined constants now a real` |
|         - |  1512 | `			 * Error, quote the key here so the simple syntax keeps meaning what php means.` |
|         - |  1513 | `			 * A numeric ($a[0]) or variable ($a[$k]) subscript is already unambiguous.` |
|         - |  1514 | `			 */` |
|         - |  1515 | `			{` |
|      2511 |  1516 | `				const char *zBr = zExpr;` |
|     14383 |  1517 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     11877 |  1518 | `					zBr++;` |
|         5 |  1519 | `				}` |
|      2511 |  1520 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
|        12 |  1521 | `					const char *zKey = &zBr[1];` |
|        12 |  1522 | `					const char *zKeyEnd = &zIn[-1];` |
|        12 |  1523 | `					const char *zScan = zKey;` |
|        12 |  1524 | `					int bBare = (zKey < zKeyEnd) && !SyisDigit(zKey[0]);` |
|        20 |  1525 | `					while( bBare && zScan < zKeyEnd ){` |
|         9 |  1526 | `						if( !SyisAlphaNum(zScan[0]) && zScan[0] != '_' ){` |
|       ! 0 |  1527 | `							bBare = 0;` |
|       ! 0 |  1528 | `						}` |
|         9 |  1529 | `						zScan++;` |
|         1 |  1530 | `					}` |
|        12 |  1531 | `					if( bBare ){` |
|         - |  1532 | `						SyBlob sSub;` |
|         3 |  1533 | `						SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|         3 |  1534 | `						SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|         3 |  1535 | `						SyBlobAppend(&sSub,"['",2);` |
|         3 |  1536 | `						SyBlobAppend(&sSub,zKey,(sxu32)(zKeyEnd - zKey));` |
|         3 |  1537 | `						SyBlobAppend(&sSub,"']",2);` |
|         4 |  1538 | `						rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|         2 |  1539 | `							(const char *)SyBlobData(&sSub),` |
|         2 |  1540 | `							(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|         3 |  1541 | `						SyBlobRelease(&sSub);` |
|         3 |  1542 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1543 | `							return SXERR_ABORT;` |
|         - |  1544 | `						}` |
|         3 |  1545 | `						if( rc != SXERR_EMPTY ){` |
|         3 |  1546 | `							++iCons;` |
|         1 |  1547 | `						}` |
|         3 |  1548 | `						pObj = 0;` |
|         3 |  1549 | `						continue;` |
|         - |  1550 | `					}` |
|         4 |  1551 | `				}` |
|         - |  1552 | `			}` |
|         - |  1553 | `			/*` |
|         - |  1554 | `			 * "${name}" is php's DEPRECATED (8.2) spelling of the variable $name — NOT an` |
|         - |  1555 | `			 * expression. PH7 handed the whole "${name}" to the expression compiler, whose` |
|         - |  1556 | ``			 * `${expr}` (variable-variable) rule evaluated the bare word `name`; that only`` |
|         - |  1557 | `			 * appeared to work while an unknown bare word fell back to its own name as a` |
|         - |  1558 | `			 * string. Now that an undefined constant is a real Error, rewrite the simple` |
|         - |  1559 | `			 * form to the variable it means. "${$x}" keeps the variable-variable meaning.` |
|         - |  1560 | `			 */` |
|      2504 |  1561 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' && zIn[-1] == '}'` |
|         8 |  1562 | `				&& zExpr[2] != '$' ){` |
|         3 |  1563 | `				const char *zName = &zExpr[2];` |
|         3 |  1564 | `				const char *zStop = &zIn[-1];` |
|         3 |  1565 | `				const char *zScan = zName;` |
|        12 |  1566 | `				while( zScan < zStop && (SyisAlphaNum(zScan[0]) \|\| zScan[0] == '_') ){` |
|         9 |  1567 | `					zScan++;` |
|         1 |  1568 | `				}` |
|         3 |  1569 | `				if( zScan == zStop && zName < zStop ){` |
|         - |  1570 | `					SyBlob sVar;` |
|         3 |  1571 | `					PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pGen->pIn->nLine,` |
|         - |  1572 | `						"Using ${var} in strings is deprecated, use {$var} instead");` |
|         3 |  1573 | `					SyBlobInit(&sVar,&pGen->pVm->sAllocator);` |
|         3 |  1574 | `					SyBlobAppend(&sVar,"$",1);` |
|         3 |  1575 | `					SyBlobAppend(&sVar,zName,(sxu32)(zStop - zName));` |
|         - |  1576 | `					/* The scanner reads one byte PAST the length it is given, so the rewritten` |
|         - |  1577 | `					 * source has to be NUL-terminated: in the ordinary path the byte after the` |
|         - |  1578 | `					 * expression is the string's own closing quote, which stops an identifier,` |
|         - |  1579 | `					 * but here it is whatever the allocator left after the blob -- and an` |
|         - |  1580 | `					 * identifier byte there silently EXTENDS the variable name. */` |
|         3 |  1581 | `					SyBlobNullAppend(&sVar);` |
|         4 |  1582 | `					rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|         2 |  1583 | `						(const char *)SyBlobData(&sVar),` |
|         2 |  1584 | `						(const char *)SyBlobData(&sVar) + SyBlobLength(&sVar));` |
|         3 |  1585 | `					SyBlobRelease(&sVar);` |
|         3 |  1586 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  1587 | `						return SXERR_ABORT;` |
|         - |  1588 | `					}` |
|         3 |  1589 | `					if( rc != SXERR_EMPTY ){` |
|         3 |  1590 | `						++iCons;` |
|         1 |  1591 | `					}` |
|         3 |  1592 | `					pObj = 0;` |
|         3 |  1593 | `					continue;` |
|         - |  1594 | `				}` |
|       ! 0 |  1595 | `			}` |
|         - |  1596 | `			/* Process the expression */` |
|      2507 |  1597 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2507 |  1598 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1599 | `				return SXERR_ABORT;` |
|         - |  1600 | `			}` |
|      2507 |  1601 | `			if( rc != SXERR_EMPTY ){` |
|      2505 |  1602 | `				++iCons;` |
|      1250 |  1603 | `			}` |
|         - |  1604 | `		}` |
|         - |  1605 | `		/* Invalidate the previously used constant */` |
|      2639 |  1606 | `		pObj = 0;` |
|         5 |  1607 | `	}/*for(;;)*/` |
|    120125 |  1608 | `	if( iCons > 1 ){` |
|         - |  1609 | `		/* Concatenate all compiled constants */` |
|      1911 |  1610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       953 |  1611 | `	}` |
|         - |  1612 | `	/* Node successfully compiled */` |
|    120125 |  1613 | `	return SXRET_OK;` |
|     60269 |  1614 | `}` |
|         - |  1615 | `/*` |
|         - |  1616 | ` * Compile a double quoted string.` |
|         - |  1617 | ` *  See the block-comment above for more information.` |
|         - |  1618 | ` */` |
|    120466 |  1619 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1620 | `{` |
|         - |  1621 | `	sxi32 rc;` |
|    120471 |  1622 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     60233 |  1623 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1624 | `	/* Compilation result */` |
|    120471 |  1625 | `	return rc;` |
|         5 |  1626 | `}` |
|         - |  1627 | `/*` |
|         - |  1628 | ` * Compile a Heredoc string.` |
|         - |  1629 | ` *  See the block-comment above for more information.` |
|         - |  1630 | ` */` |
|        66 |  1631 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 |  1632 | `{` |
|         - |  1633 | `	SyString sOrig, sStripped;` |
|         - |  1634 | `	sxi32 rc;` |
|        70 |  1635 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        70 |  1636 | `	if( rc != SXRET_OK ){` |
|         6 |  1637 | `		return rc;` |
|         - |  1638 | `	}` |
|         - |  1639 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|         - |  1640 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|         - |  1641 | `	 * Restore before returning so downstream code that references pIn is` |
|         - |  1642 | `	 * unaffected, including on the error path. */` |
|        64 |  1643 | `	sOrig = pGen->pIn->sData;` |
|        64 |  1644 | `	pGen->pIn->sData = sStripped;` |
|        64 |  1645 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|        64 |  1646 | `	pGen->pIn->sData = sOrig;` |
|        31 |  1647 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        64 |  1648 | `	return rc;` |
|        37 |  1649 | `}` |
|         - |  1650 | `/*` |
|         - |  1651 | ` * Compile an array entry whether it is a key or a value.` |
|         - |  1652 | ` *  Notes on array entries.` |
|         - |  1653 | ` *  According to the PHP language reference manual` |
|         - |  1654 | ` *  An array can be created by the array() language construct.` |
|         - |  1655 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|         - |  1656 | ` *  array(  key =>  value` |
|         - |  1657 | ` *    , ...` |
|         - |  1658 | ` *    )` |
|         - |  1659 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|         - |  1660 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|         - |  1661 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|         - |  1662 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|         - |  1663 | ` *  contain integer and string indices.` |
|         - |  1664 | ` *  A value can be any PHP type.` |
|         - |  1665 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|         - |  1666 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|         - |  1667 | ` *  is specified, that value will be overwritten.` |
|         - |  1668 | ` */` |
|   1439464 |  1669 | `static sxi32 GenStateCompileArrayEntry(` |
|         - |  1670 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1671 | `	SyToken *pIn,        /* Token stream */` |
|         - |  1672 | `	SyToken *pEnd,       /* End of the token stream */` |
|         - |  1673 | `	sxi32 iFlags,        /* Compilation flags */` |
|         - |  1674 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|         - |  1675 | `	)` |
|         5 |  1676 | `{` |
|         - |  1677 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1678 | `	sxi32 rc;` |
|         - |  1679 | `	/* Swap token stream */` |
|   1439469 |  1680 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1681 | `	/* Compile the expression*/` |
|   1439469 |  1682 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1683 | `	/* Restore token stream */` |
|   1439469 |  1684 | `	RE_SWAP_DELIMITER(pGen);` |
|   1439469 |  1685 | `	return rc;` |
|         5 |  1686 | `}` |
|         - |  1687 | `/*` |
|         - |  1688 | ` * Expression tree validator callback for the 'array' language construct.` |
|         - |  1689 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1690 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1691 | ` * error message.` |
|         - |  1692 | ` * See the routine responible of compiling the array language construct` |
|         - |  1693 | ` * for more inforation.` |
|         - |  1694 | ` */` |
|        36 |  1695 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  1696 | `{` |
|        41 |  1697 | `	sxi32 rc = SXRET_OK;` |
|        41 |  1698 | `	if( pRoot->pOp ){` |
|        14 |  1699 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|        12 |  1700 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|        16 |  1701 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1702 | `			/* Unexpected expression */` |
|        13 |  1703 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|        13 |  1704 | `			if( rc != SXERR_ABORT ){` |
|        13 |  1705 | `				rc = SXERR_INVALID;` |
|         5 |  1706 | `			}` |
|         9 |  1707 | `		}` |
|        31 |  1708 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1709 | `		/* Unexpected expression */` |
|         3 |  1710 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|         3 |  1711 | `		if( rc != SXERR_ABORT ){` |
|         3 |  1712 | `			rc = SXERR_INVALID;` |
|         1 |  1713 | `		}` |
|         1 |  1714 | `	}` |
|        41 |  1715 | `	return rc;` |
|         5 |  1716 | `}` |
|         - |  1717 | `/*` |
|         - |  1718 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|         - |  1719 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|         - |  1720 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|         - |  1721 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|         - |  1722 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|         - |  1723 | ` */` |
|   1380834 |  1724 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1725 | `{` |
|   1380839 |  1726 | `	SyToken *pCur = pStart;` |
|   1380839 |  1727 | `	sxi32 iNest = 0;` |
|   3564759 |  1728 | `	while( pCur < pEnd ){` |
|   2687279 |  1729 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    503355 |  1730 | `			return pCur;` |
|         - |  1731 | `		}` |
|         - |  1732 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1733 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1734 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1735 | `		 */` |
|   2183929 |  1736 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23055 |  1737 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23055 |  1738 | `			SyToken *pFn = pCur;` |
|     23050 |  1739 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1740 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1741 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1742 | `				pFn = &pCur[1];` |
|       ! 0 |  1743 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1744 | `			}` |
|     23055 |  1745 | `			if( nKw == PH7_TKWRD_FN ){` |
|         5 |  1746 | `				pCur = pFn + 1; /* past 'fn' */` |
|         5 |  1747 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  1748 | `					pCur++;` |
|       ! 0 |  1749 | `				}` |
|         5 |  1750 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         5 |  1751 | `					pCur++;` |
|         5 |  1752 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1753 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         5 |  1754 | `					if( pCur < pEnd ){` |
|         5 |  1755 | `						pCur++;` |
|         2 |  1756 | `					}` |
|         2 |  1757 | `				}` |
|         5 |  1758 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|       ! 0 |  1759 | `					pCur++;` |
|       ! 0 |  1760 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|       ! 0 |  1761 | `						&& pCur->sData.nByte == 1` |
|       ! 0 |  1762 | `						&& pCur->sData.zString[0] == '?' ){` |
|       ! 0 |  1763 | `						pCur++;` |
|       ! 0 |  1764 | `					}` |
|       ! 0 |  1765 | `					if( pCur < pEnd` |
|       ! 0 |  1766 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  1767 | `						pCur++;` |
|       ! 0 |  1768 | `					}` |
|       ! 0 |  1769 | `				}` |
|         - |  1770 | `				/* The rest of the entry is the arrow-function body — no outer` |
|         - |  1771 | `				 * key to extract. */` |
|         5 |  1772 | `				return pEnd;` |
|         - |  1773 | `			}` |
|         - |  1774 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|         - |  1775 | `			 * entry separator. Skip past the full match span. */` |
|     23051 |  1776 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|         3 |  1777 | `				pCur++; /* past 'match' */` |
|         3 |  1778 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         3 |  1779 | `					pCur++;` |
|         3 |  1780 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1781 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         3 |  1782 | `					if( pCur < pEnd ){` |
|         3 |  1783 | `						pCur++;` |
|         1 |  1784 | `					}` |
|         1 |  1785 | `				}` |
|         3 |  1786 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|         3 |  1787 | `					pCur++;` |
|         3 |  1788 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1789 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|         3 |  1790 | `					if( pCur < pEnd ){` |
|         3 |  1791 | `						pCur++;` |
|         1 |  1792 | `					}` |
|         1 |  1793 | `				}` |
|         3 |  1794 | `				continue;` |
|         - |  1795 | `			}` |
|     11522 |  1796 | `		}` |
|   2183923 |  1797 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     54107 |  1798 | `			iNest++;` |
|   2156872 |  1799 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1800 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1801 | `			 * parser will shortly detect any syntax error. */` |
|     54107 |  1802 | `			iNest--;` |
|     27051 |  1803 | `		}` |
|   2183923 |  1804 | `		pCur++;` |
|         5 |  1805 | `	}` |
|    877485 |  1806 | `	return pEnd;` |
|    690422 |  1807 | `}` |
|         - |  1808 | `/*` |
|         - |  1809 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1810 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1811 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1812 | ` */` |
|    615956 |  1813 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1814 | `{` |
|         - |  1815 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1816 | `	SyToken *pKey,*pCur;` |
|    615961 |  1817 | `	sxi32 iEmitRef = 0;` |
|    615961 |  1818 | `	sxi32 iSpread = 0;` |
|    615961 |  1819 | `	sxi32 nPair = 0;` |
|         - |  1820 | `	sxi32 rc;` |
|    615961 |  1821 | `	xValidator = 0;` |
|    842998 |  1822 | `	for(;;){` |
|         - |  1823 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|         - |  1824 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|         - |  1825 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|         - |  1826 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|    535020 |  1827 | `		{` |
|   1686001 |  1828 | `			int nSkip = 0;` |
|   2507907 |  1829 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    821911 |  1830 | `				nSkip++;` |
|    821911 |  1831 | `				pGen->pIn++;` |
|         5 |  1832 | `			}` |
|   1686001 |  1833 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|       ! 0 |  1834 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|         - |  1835 | `					"Cannot use empty array elements in arrays");` |
|       ! 0 |  1836 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1837 | `					return SXERR_ABORT;` |
|         - |  1838 | `				}` |
|       ! 0 |  1839 | `				return SXRET_OK;` |
|         - |  1840 | `			}` |
|         - |  1841 | `		}` |
|   1686001 |  1842 | `		pCur = pGen->pIn;` |
|   1686001 |  1843 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1844 | `			/* No more entry to process */` |
|    615943 |  1845 | `			break;` |
|         - |  1846 | `		}` |
|   1070063 |  1847 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1848 | `			continue;` |
|         - |  1849 | `		}` |
|         - |  1850 | `		/* Compile the key if available */` |
|   1070063 |  1851 | `		pKey = pCur;` |
|   1070063 |  1852 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   1070063 |  1853 | `		rc = SXERR_EMPTY;` |
|   1070063 |  1854 | `		if( pCur < pGen->pIn ){` |
|    369153 |  1855 | `			if( pKey == pCur ){` |
|         - |  1856 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|         - |  1857 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|         - |  1858 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|         - |  1859 | `				 * IS found here, so control never reached it.)` |
|         - |  1860 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|         3 |  1861 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|         - |  1862 | `					? "\"]\"" : "\")\"";` |
|         3 |  1863 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|         3 |  1864 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1865 | `					return SXERR_ABORT;` |
|         - |  1866 | `				}` |
|         3 |  1867 | `				return SXRET_OK;` |
|         - |  1868 | `			}` |
|    369151 |  1869 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1870 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|         - |  1871 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|         - |  1872 | `				 * makes the helper reach for the token past this entry's slice. */` |
|        12 |  1873 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        12 |  1874 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1875 | `					return SXERR_ABORT;` |
|         - |  1876 | `				}` |
|        12 |  1877 | `				return SXRET_OK;` |
|         - |  1878 | `			}` |
|         - |  1879 | `			/* Compile the expression holding the key */` |
|    369141 |  1880 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1881 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    369141 |  1882 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1883 | `				return SXERR_ABORT;` |
|         - |  1884 | `			}` |
|    369141 |  1885 | `			pCur++; /* Jump the '=>' operator */` |
|    184573 |  1886 | `		}else{` |
|         - |  1887 | `			/* Reset back the cursor and point to the entry value */` |
|    700915 |  1888 | `			pCur = pKey;` |
|         - |  1889 | `		}` |
|   1070051 |  1890 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1891 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|         - |  1892 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|    700915 |  1893 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|    350455 |  1894 | `		}` |
|   1070051 |  1895 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|         - |  1896 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|        45 |  1897 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|        45 |  1898 | `			iEmitRef = 1;` |
|        45 |  1899 | `			pCur++; /* Jump the '&' token */` |
|        45 |  1900 | `			if( pCur >= pGen->pIn ){` |
|         - |  1901 | `				/* Missing value */` |
|         3 |  1902 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|         3 |  1903 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1904 | `					return SXERR_ABORT;` |
|         - |  1905 | `				}` |
|         3 |  1906 | `				return SXRET_OK;` |
|         - |  1907 | `			}` |
|        19 |  1908 | `		}` |
|         - |  1909 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|         - |  1910 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|         - |  1911 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|         - |  1912 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|         - |  1913 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   1070049 |  1914 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   1070049 |  1915 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|         - |  1916 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|         - |  1917 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|         - |  1918 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|         - |  1919 | `			 * output is engine-portable. */` |
|         6 |  1920 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|         - |  1921 | `				"syntax error, unexpected token \"...\"");` |
|         6 |  1922 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1923 | `				return SXERR_ABORT;` |
|         - |  1924 | `			}` |
|         6 |  1925 | `			return SXRET_OK;` |
|         - |  1926 | `		}` |
|         - |  1927 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|         - |  1928 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|         - |  1929 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|         - |  1930 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|         - |  1931 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|   1605065 |  1932 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    535020 |  1933 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1934 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    535020 |  1935 | `			xValidator);` |
|   1070045 |  1936 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1937 | `			return SXERR_ABORT;` |
|         - |  1938 | `		}` |
|   1070045 |  1939 | `		if( iSpread ){` |
|         - |  1940 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        72 |  1941 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   1070010 |  1942 | `		}else if( iEmitRef ){` |
|         - |  1943 | `			/* Emit the load reference instruction */` |
|        41 |  1944 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1945 | `		}` |
|   1070045 |  1946 | `		xValidator = 0;` |
|   1070045 |  1947 | `		iEmitRef = 0;` |
|   1070045 |  1948 | `		iSpread = 0;` |
|   1070045 |  1949 | `		nPair++;` |
|         5 |  1950 | `	}` |
|         - |  1951 | `	/* Emit the load map instruction */` |
|    615943 |  1952 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1953 | `	/* Node successfully compiled */` |
|    615943 |  1954 | `	return SXRET_OK;` |
|    307983 |  1955 | `}` |
|         - |  1956 | `/*` |
|         - |  1957 | ` * Compile the 'array' language construct.` |
|         - |  1958 | ` *	 According to the PHP language reference manual` |
|         - |  1959 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1960 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1961 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1962 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1963 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1964 | ` */` |
|    399552 |  1965 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1966 | `{` |
|         - |  1967 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    399557 |  1968 | `	pGen->pIn += 2;` |
|    399557 |  1969 | `	pGen->pEnd--;` |
|    199776 |  1970 | `	SXUNUSED(iCompileFlag);` |
|    399557 |  1971 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1972 | `}` |
|         - |  1973 | `/*` |
|         - |  1974 | ` * Compile the PHP 8.5 clone(...) call form:` |
|         - |  1975 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|         - |  1976 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|         - |  1977 | ` *                                              property updates as scope-aware writes` |
|         - |  1978 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|         - |  1979 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|         - |  1980 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|         - |  1981 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|         - |  1982 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|         - |  1983 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|         - |  1984 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|         - |  1985 | ` */` |
|        22 |  1986 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  1987 | `{` |
|         - |  1988 | `	SyToken *pIn,*pEnd,*pNext;` |
|        24 |  1989 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|        24 |  1990 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|        24 |  1991 | `	int nArg = 0;` |
|         - |  1992 | `	sxi32 rc;` |
|        11 |  1993 | `	SXUNUSED(iCompileFlag);` |
|         - |  1994 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|        24 |  1995 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|        24 |  1996 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|         - |  1997 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|        24 |  1998 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       ! 0 |  1999 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  2000 | `			"clone(...) first-class callable form is not yet supported");` |
|         - |  2001 | `	}` |
|         - |  2002 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|        62 |  2003 | `	while( pIn < pEnd ){` |
|        40 |  2004 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|        40 |  2005 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|       ! 0 |  2006 | `			break;` |
|         - |  2007 | `		}` |
|        40 |  2008 | `		pArgStart = pIn;` |
|        40 |  2009 | `		pArgEnd   = pNext;` |
|         - |  2010 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|         - |  2011 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|        38 |  2012 | `		if( (pArgEnd - pArgStart) >= 2` |
|        37 |  2013 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        23 |  2014 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|         5 |  2015 | `			pName = pArgStart;` |
|         5 |  2016 | `			pArgStart += 2;` |
|         2 |  2017 | `		}` |
|        40 |  2018 | `		if( pName ){` |
|         - |  2019 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|         - |  2020 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|         4 |  2021 | `			if( pName->sData.nByte == sizeof("object")-1` |
|         4 |  2022 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|         3 |  2023 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|         4 |  2024 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|         3 |  2025 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|         3 |  2026 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         2 |  2027 | `			}else{` |
|       ! 0 |  2028 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|       ! 0 |  2029 | `					"Unknown named parameter $%z",&pName->sData);` |
|         1 |  2030 | `			}` |
|        38 |  2031 | `		}else if( nArg == 0 ){` |
|        22 |  2032 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        25 |  2033 | `		}else if( nArg == 1 ){` |
|        15 |  2034 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         8 |  2035 | `		}else{` |
|       ! 0 |  2036 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|         - |  2037 | `				"clone() expects at most 2 arguments");` |
|         - |  2038 | `		}` |
|        40 |  2039 | `		nArg++;` |
|        40 |  2040 | `		pIn = pNext;` |
|        40 |  2041 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 |  2042 | `			pIn++; /* step over the argument separator */` |
|         8 |  2043 | `		}` |
|         2 |  2044 | `	}` |
|        24 |  2045 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|       ! 0 |  2046 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  2047 | `			"clone() expects at least 1 argument, 0 given");` |
|         - |  2048 | `	}` |
|         - |  2049 | `	/* Object argument -> clone (+ __clone()). */` |
|        24 |  2050 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        24 |  2051 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2052 | `		return SXERR_ABORT;` |
|         - |  2053 | `	}` |
|        24 |  2054 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|         - |  2055 | `	/* Property updates (evaluated after __clone runs). */` |
|        24 |  2056 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|        17 |  2057 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        17 |  2058 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2059 | `			return SXERR_ABORT;` |
|         - |  2060 | `		}` |
|        17 |  2061 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|         8 |  2062 | `	}` |
|        24 |  2063 | `	return SXRET_OK;` |
|        13 |  2064 | `}` |
|         - |  2065 | `/*` |
|         - |  2066 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|         - |  2067 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|         - |  2068 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|         - |  2069 | ` */` |
|    216404 |  2070 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2071 | `{` |
|         - |  2072 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    216409 |  2073 | `	pGen->pIn++;` |
|    216409 |  2074 | `	pGen->pEnd--;` |
|    108202 |  2075 | `	SXUNUSED(iCompileFlag);` |
|    216409 |  2076 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  2077 | `}` |
|         - |  2078 | `/*` |
|         - |  2079 | ` * Expression tree validator callback for the 'list' language construct.` |
|         - |  2080 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  2081 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  2082 | ` * error message.` |
|         - |  2083 | ` * See the routine responible of compiling the list language construct` |
|         - |  2084 | ` * for more inforation.` |
|         - |  2085 | ` */` |
|       214 |  2086 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  2087 | `{` |
|       219 |  2088 | `	sxi32 rc = SXRET_OK;` |
|       219 |  2089 | `	if( pRoot->pOp ){` |
|         4 |  2090 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|         2 |  2091 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|         - |  2092 | `				/* Unexpected expression */` |
|       ! 0 |  2093 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2094 | `					"Assignments can only happen to writable values");` |
|       ! 0 |  2095 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 |  2096 | `					rc = SXERR_INVALID;` |
|       ! 0 |  2097 | `				}` |
|         1 |  2098 | `		}` |
|       217 |  2099 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  2100 | `		/* Unexpected expression */` |
|         6 |  2101 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2102 | `			"Assignments can only happen to writable values");` |
|         6 |  2103 | `		if( rc != SXERR_ABORT ){` |
|         6 |  2104 | `			rc = SXERR_INVALID;` |
|         2 |  2105 | `		}` |
|         2 |  2106 | `	}` |
|       219 |  2107 | `	return rc;` |
|         5 |  2108 | `}` |
|         - |  2109 | `/*` |
|         - |  2110 | ` * Compile the 'list' language construct.` |
|         - |  2111 | ` *  According to the PHP language reference` |
|         - |  2112 | ` *  list(): Assign variables as if they were an array.` |
|         - |  2113 | ` *  list() is used to assign a list of variables in one operation.` |
|         - |  2114 | ` *  Description` |
|         - |  2115 | ` *   array list (mixed $varname [, mixed $... ] )` |
|         - |  2116 | ` *   Like array(), this is not really a function, but a language construct.` |
|         - |  2117 | ` *   list() is used to assign a list of variables in one operation.` |
|         - |  2118 | ` *  Parameters` |
|         - |  2119 | ` *   $varname: A variable.` |
|         - |  2120 | ` *  Return Values` |
|         - |  2121 | ` *   The assigned array.` |
|         - |  2122 | ` */` |
|         - |  2123 | `/* Nested list entry recorded during first pass of list body compilation */` |
|         - |  2124 | `struct NestedListEntry {` |
|         - |  2125 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|         - |  2126 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|         - |  2127 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|         - |  2128 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|         - |  2129 | `};` |
|         - |  2130 | `/*` |
|         - |  2131 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|         - |  2132 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|         - |  2133 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|         - |  2134 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|         - |  2135 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|         - |  2136 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|         - |  2137 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|         - |  2138 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|         - |  2139 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|         - |  2140 | ` */` |
|        22 |  2141 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|         1 |  2142 | `{` |
|         - |  2143 | `	SyToken *pNext;` |
|         - |  2144 | `	sxi32 rc;` |
|        53 |  2145 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|         - |  2146 | `		SyToken *pArrow,*pTarget;` |
|         - |  2147 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|        31 |  2148 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|        31 |  2149 | `		pTarget = &pArrow[1];` |
|        31 |  2150 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|         - |  2151 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|         - |  2152 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|       ! 0 |  2153 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2154 | `				"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2155 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2156 | `		}` |
|         - |  2157 | `		/* DUP the source array (it is on the stack top) */` |
|        31 |  2158 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2159 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|        31 |  2160 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|        31 |  2161 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2162 | `			return SXERR_ABORT;` |
|         - |  2163 | `		}` |
|         - |  2164 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|         - |  2165 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|         - |  2166 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|         - |  2167 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|         - |  2168 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|         - |  2169 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|        31 |  2170 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|        31 |  2171 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|        28 |  2172 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|        15 |  2173 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|         - |  2174 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|         - |  2175 | `			 * Treat source[key] as the inner body's source, then drop the` |
|         - |  2176 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|         5 |  2177 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|         5 |  2178 | `			SyToken *pSavedIn = pGen->pIn;` |
|         5 |  2179 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         5 |  2180 | `			pGen->pIn = pTarget;` |
|         5 |  2181 | `			pGen->pEnd = pNext;` |
|         5 |  2182 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|         2 |  2183 | `			             : PH7_CompileList(&(*pGen),0);` |
|         5 |  2184 | `			pGen->pIn = pSavedIn;` |
|         5 |  2185 | `			pGen->pEnd = pSavedEnd;` |
|         5 |  2186 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2187 | `				return SXERR_ABORT;` |
|         - |  2188 | `			}` |
|         5 |  2189 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         3 |  2190 | `		}else{` |
|         - |  2191 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|         - |  2192 | `			 * is already on the stack as the value; compiling the target appends` |
|         - |  2193 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|         - |  2194 | `			 * assignment does. */` |
|         - |  2195 | `			VmInstr *pInstr;` |
|        27 |  2196 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|        27 |  2197 | `			sxi32 iP1 = 0, iP2 = 0;` |
|        27 |  2198 | `			void *p3 = 0;` |
|        27 |  2199 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|         - |  2200 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|        27 |  2201 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2202 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2203 | `			}` |
|        27 |  2204 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|        27 |  2205 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|         3 |  2206 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|        26 |  2207 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         3 |  2208 | `					iVmOp = PH7_OP_STORE_IDX;` |
|         3 |  2209 | `					iP1 = pInstr->iP1;` |
|         3 |  2210 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         2 |  2211 | `				}else{` |
|        23 |  2212 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|        23 |  2213 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - |  2214 | `				}` |
|        13 |  2215 | `			}` |
|        27 |  2216 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|         - |  2217 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|         - |  2218 | `			 * source array is back on top for the next entry. */` |
|        27 |  2219 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         - |  2220 | `		}` |
|        31 |  2221 | `		pGen->pIn = &pNext[1];` |
|         1 |  2222 | `	}` |
|        23 |  2223 | `	return SXRET_OK;` |
|        12 |  2224 | `}` |
|         - |  2225 | `/*` |
|         - |  2226 | ` * Shared body for list() and short list [...] compilation.` |
|         - |  2227 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|         - |  2228 | ` * the opening delimiter and before the closing delimiter.` |
|         - |  2229 | ` */` |
|       124 |  2230 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|         5 |  2231 | `{` |
|         - |  2232 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|         - |  2233 | `	SyToken *pNext;` |
|         - |  2234 | `	SyToken *pClassifyIn;` |
|       129 |  2235 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|         - |  2236 | `	sxi32 nExpr;` |
|         - |  2237 | `	sxi32 rc;` |
|         - |  2238 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|         - |  2239 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|         - |  2240 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|         - |  2241 | `	 * list. */` |
|       129 |  2242 | `	pClassifyIn = pGen->pIn;` |
|       373 |  2243 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       249 |  2244 | `		if( pGen->pIn >= pNext ){` |
|        13 |  2245 | `			nEmpty++;` |
|       243 |  2246 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|        31 |  2247 | `			nKeyed++;` |
|        16 |  2248 | `		}else{` |
|       207 |  2249 | `			nPositional++;` |
|         - |  2250 | `		}` |
|       249 |  2251 | `		pGen->pIn = &pNext[1];` |
|         5 |  2252 | `	}` |
|       129 |  2253 | `	pGen->pIn = pClassifyIn;` |
|       129 |  2254 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|       ! 0 |  2255 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2256 | `			"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2257 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2258 | `	}` |
|       129 |  2259 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|       ! 0 |  2260 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2261 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|       ! 0 |  2262 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2263 | `	}` |
|       129 |  2264 | `	if( nKeyed > 0 ){` |
|        23 |  2265 | `		return GenStateCompileKeyedListBody(pGen);` |
|         - |  2266 | `	}` |
|       107 |  2267 | `	nExpr = 0;` |
|       107 |  2268 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       321 |  2269 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       219 |  2270 | `		if( pGen->pIn < pNext ){` |
|         - |  2271 | `			/* Check for nested list() */` |
|       207 |  2272 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         3 |  2273 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  2274 | `				/* Record this nested list for post-processing */` |
|         3 |  2275 | `				SyToken *pListEnd = 0;` |
|         3 |  2276 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|         3 |  2277 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         1 |  2278 | `				}` |
|         3 |  2279 | `				if( pListEnd ){` |
|         - |  2280 | `					struct NestedListEntry sEntry;` |
|         3 |  2281 | `					sEntry.nIndex = nExpr;` |
|         3 |  2282 | `					sEntry.pStart = pGen->pIn;` |
|         3 |  2283 | `					sEntry.pEnd = pListEnd + 1;` |
|         3 |  2284 | `					sEntry.isShort = 0;` |
|         3 |  2285 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         1 |  2286 | `				}` |
|         - |  2287 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|         3 |  2288 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       206 |  2289 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  2290 | `				/* Nested short destructuring [...] */` |
|        13 |  2291 | `				SyToken *pBracketEnd = 0;` |
|        13 |  2292 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|        13 |  2293 | `				if( pBracketEnd ){` |
|         - |  2294 | `					struct NestedListEntry sEntry;` |
|        13 |  2295 | `					sEntry.nIndex = nExpr;` |
|        13 |  2296 | `					sEntry.pStart = pGen->pIn;` |
|        13 |  2297 | `					sEntry.pEnd = pBracketEnd + 1;` |
|        13 |  2298 | `					sEntry.isShort = 1;` |
|        13 |  2299 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         6 |  2300 | `				}` |
|         - |  2301 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        13 |  2302 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |  2303 | `			}else{` |
|         - |  2304 | `				/* Compile the expression holding the variable */` |
|       193 |  2305 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       193 |  2306 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  2307 | `					SySetRelease(&sNested);` |
|       ! 0 |  2308 | `					return SXRET_OK;` |
|         - |  2309 | `				}` |
|         - |  2310 | `			}` |
|       106 |  2311 | `		}else{` |
|         - |  2312 | `			/* Empty entry,load NULL */` |
|        13 |  2313 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|         - |  2314 | `		}` |
|       219 |  2315 | `		nExpr++;` |
|         - |  2316 | `		/* Advance the stream cursor */` |
|       219 |  2317 | `		pGen->pIn = &pNext[1];` |
|         5 |  2318 | `	}` |
|         - |  2319 | `	/* Emit the LOAD_LIST instruction */` |
|       107 |  2320 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|         - |  2321 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|         - |  2322 | `	 * For each nested entry, emit code to extract the sub-array` |
|         - |  2323 | `	 * at the corresponding index and recursively destructure it.` |
|         - |  2324 | `	 */` |
|       107 |  2325 | `	if( SySetUsed(&sNested) > 0 ){` |
|        13 |  2326 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|         - |  2327 | `		sxu32 i;` |
|        27 |  2328 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|        15 |  2329 | `			SyToken *pSavedIn = pGen->pIn;` |
|        15 |  2330 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         - |  2331 | `			ph7_value *pIdx;` |
|         - |  2332 | `			sxu32 nConstIdx;` |
|         - |  2333 | `			/* DUP the source array (it's on stack top) */` |
|        15 |  2334 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2335 | `			/* Push the integer index for this nested entry */` |
|        15 |  2336 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|        15 |  2337 | `			if( pIdx == 0 ){` |
|       ! 0 |  2338 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2339 | `				SySetRelease(&sNested);` |
|       ! 0 |  2340 | `				return SXERR_ABORT;` |
|         - |  2341 | `			}` |
|        15 |  2342 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|        15 |  2343 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|         - |  2344 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|         - |  2345 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|         - |  2346 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|         - |  2347 | `			 */` |
|        15 |  2348 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|         - |  2349 | `			/* Recursively compile the inner list */` |
|        15 |  2350 | `			pGen->pIn = apNested[i].pStart;` |
|        15 |  2351 | `			pGen->pEnd = apNested[i].pEnd;` |
|        15 |  2352 | `			if( apNested[i].isShort ){` |
|        13 |  2353 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  2354 | `			}else{` |
|         3 |  2355 | `				rc = PH7_CompileList(&(*pGen),0);` |
|         - |  2356 | `			}` |
|        15 |  2357 | `			pGen->pIn = pSavedIn;` |
|        15 |  2358 | `			pGen->pEnd = pSavedEnd;` |
|        15 |  2359 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2360 | `				SySetRelease(&sNested);` |
|       ! 0 |  2361 | `				return SXERR_ABORT;` |
|         - |  2362 | `			}` |
|         - |  2363 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|        15 |  2364 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         8 |  2365 | `		}` |
|         6 |  2366 | `	}` |
|       107 |  2367 | `	SySetRelease(&sNested);` |
|         - |  2368 | `	/* Node successfully compiled */` |
|       107 |  2369 | `	return SXRET_OK;` |
|        67 |  2370 | `}` |
|        40 |  2371 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2372 | `{` |
|         - |  2373 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|        45 |  2374 | `	pGen->pIn += 2;` |
|        45 |  2375 | `	pGen->pEnd--;` |
|        20 |  2376 | `	SXUNUSED(iCompileFlag);` |
|        45 |  2377 | `	return GenStateCompileListBody(pGen);` |
|         5 |  2378 | `}` |
|        84 |  2379 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2380 | `{` |
|         - |  2381 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        86 |  2382 | `	pGen->pIn++;` |
|        86 |  2383 | `	pGen->pEnd--;` |
|        42 |  2384 | `	SXUNUSED(iCompileFlag);` |
|        86 |  2385 | `	return GenStateCompileListBody(pGen);` |
|         2 |  2386 | `}` |
|         - |  2387 | `/* Forward declarations */` |
|         - |  2388 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|         - |  2389 | `static int GenStateIsReservedConstant(SyString *pName);` |
|         - |  2390 | `static int GenStateIsReadonly(SyToken *pTok);` |
|         - |  2391 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|         - |  2392 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|         - |  2393 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - |  2394 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|         - |  2395 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|         - |  2396 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|         - |  2397 | `/*` |
|         - |  2398 | ` * Compile an annoynmous function or a closure.` |
|         - |  2399 | ` * According to the PHP language reference` |
|         - |  2400 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  2401 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  2402 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|         - |  2403 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|         - |  2404 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|         - |  2405 | ` *  Example Anonymous function variable assignment example` |
|         - |  2406 | ` * <?php` |
|         - |  2407 | ` * $greet = function($name)` |
|         - |  2408 | ` * {` |
|         - |  2409 | ` *    printf("Hello %s\r\n", $name);` |
|         - |  2410 | ` * };` |
|         - |  2411 | ` * $greet('World');` |
|         - |  2412 | ` * $greet('PHP');` |
|         - |  2413 | ` * ?>` |
|         - |  2414 | ` * Note that the implementation of annoynmous function and closure under` |
|         - |  2415 | ` * PH7 is completely different from the one used by the zend engine.` |
|         - |  2416 | ` */` |
|       576 |  2417 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2418 | `{` |
|       581 |  2419 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2420 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2421 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2422 | `							  * one thread is allowed to compile the script.` |
|         - |  2423 | `						      */` |
|         - |  2424 | `	SyString sName;` |
|       581 |  2425 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2426 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2427 | `	sxu32 nKwLine;` |
|       581 |  2428 | `	sxi32 iFlags = 0;` |
|         - |  2429 | `	sxu32 nLen;` |
|         - |  2430 | `	sxi32 rc;` |
|       288 |  2431 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2432 |  |
|       581 |  2433 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       576 |  2434 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       581 |  2435 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2436 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        11 |  2437 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        11 |  2438 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         5 |  2439 | `	}` |
|       581 |  2440 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       581 |  2441 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2442 | `		pGen->pIn++;` |
|       ! 0 |  2443 | `	}` |
|         - |  2444 | `	/* Generate a unique name */` |
|       581 |  2445 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2446 | `	/* Make sure the generated name is unique */` |
|       581 |  2447 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2448 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2449 | `	}` |
|       581 |  2450 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2451 | `	/* Compile the lambda body */` |
|       581 |  2452 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       581 |  2453 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2454 | `		return SXERR_ABORT;` |
|         - |  2455 | `	}` |
|       581 |  2456 | `	if( pAnnonFunc ){` |
|       581 |  2457 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2458 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2459 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       581 |  2460 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2461 | `			return SXERR_ABORT;` |
|         - |  2462 | `		}` |
|       288 |  2463 | `	}` |
|         - |  2464 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2465 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2466 | `	 * the handler wraps either in a Closure instance. */` |
|       581 |  2467 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2468 | `	/* Node successfully compiled */` |
|       581 |  2469 | `	return SXRET_OK;` |
|       293 |  2470 | `}` |
|         - |  2471 | `/*` |
|         - |  2472 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2473 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2474 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2475 | ` */` |
|       236 |  2476 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2477 | `	ph7_gen_state *pGen,` |
|         - |  2478 | `	ph7_vm_func *pFunc,` |
|         - |  2479 | `	const char *zName,` |
|         - |  2480 | `	sxu32 nByte,` |
|         - |  2481 | `	SyString *aShadow,` |
|         - |  2482 | `	sxu32 nShadow)` |
|         3 |  2483 | `{` |
|         - |  2484 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2485 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2486 | `	sxu32 n, nEnv;` |
|         - |  2487 | `	char *zDup;` |
|       239 |  2488 | `	if( nByte == 0 ){` |
|       ! 0 |  2489 | `		return SXRET_OK;` |
|         - |  2490 | `	}` |
|       236 |  2491 | `	if( nByte == sizeof("this")-1` |
|       130 |  2492 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         9 |  2493 | `		return SXRET_OK;` |
|         - |  2494 | `	}` |
|       285 |  2495 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       212 |  2496 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       206 |  2497 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       161 |  2498 | `			return SXRET_OK;` |
|         - |  2499 | `		}` |
|        29 |  2500 | `	}` |
|        71 |  2501 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        71 |  2502 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|        99 |  2503 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2504 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2505 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2506 | `			return SXRET_OK;` |
|         - |  2507 | `		}` |
|        15 |  2508 | `	}` |
|        69 |  2509 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        69 |  2510 | `	if( zDup == 0 ){` |
|       ! 0 |  2511 | `		return SXERR_ABORT;` |
|         - |  2512 | `	}` |
|        69 |  2513 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        69 |  2514 | `	sEnv.iFlags = 0;` |
|        69 |  2515 | `	sEnv.nIdx = SXU32_HIGH;` |
|        69 |  2516 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        69 |  2517 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        69 |  2518 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        69 |  2519 | `	return SXRET_OK;` |
|       121 |  2520 | `}` |
|         - |  2521 | `/*` |
|         - |  2522 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|         - |  2523 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|         - |  2524 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|         - |  2525 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|         - |  2526 | ` */` |
|       108 |  2527 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|         - |  2528 | `	ph7_gen_state *pGen,` |
|         - |  2529 | `	ph7_vm_func *pFunc,` |
|         - |  2530 | `	const char *zIn,` |
|         - |  2531 | `	const char *zEnd,` |
|         - |  2532 | `	SyString *aShadow,` |
|         - |  2533 | `	sxu32 nShadow)` |
|         3 |  2534 | `{` |
|         - |  2535 | `	sxi32 rc;` |
|       589 |  2536 | `	while( zIn < zEnd ){` |
|       481 |  2537 | `		if( zIn[0] == '\\' ){` |
|        14 |  2538 | `			zIn++;` |
|        14 |  2539 | `			if( zIn < zEnd ){` |
|        14 |  2540 | `				zIn++;` |
|         6 |  2541 | `			}` |
|        14 |  2542 | `			continue;` |
|         - |  2543 | `		}` |
|       466 |  2544 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        27 |  2545 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|        24 |  2546 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|         - |  2547 | `			const char *zName;` |
|        26 |  2548 | `			zIn++; /* skip '$' */` |
|        26 |  2549 | `			zName = zIn;` |
|        82 |  2550 | `			while( zIn < zEnd ){` |
|        76 |  2551 | `				unsigned char c = (unsigned char)zIn[0];` |
|        76 |  2552 | `				if( c >= 0xc0 ){` |
|       ! 0 |  2553 | `					zIn++;` |
|       ! 0 |  2554 | `					while( zIn < zEnd` |
|       ! 0 |  2555 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  2556 | `						zIn++;` |
|       ! 0 |  2557 | `					}` |
|       ! 0 |  2558 | `					continue;` |
|         - |  2559 | `				}` |
|        76 |  2560 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        20 |  2561 | `					break;` |
|         - |  2562 | `				}` |
|        58 |  2563 | `				zIn++;` |
|         2 |  2564 | `			}` |
|        26 |  2565 | `			if( zIn > zName ){` |
|        38 |  2566 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|        24 |  2567 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|        26 |  2568 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2569 | `					return SXERR_ABORT;` |
|         - |  2570 | `				}` |
|        12 |  2571 | `			}` |
|        26 |  2572 | `			continue;` |
|         - |  2573 | `		}` |
|       445 |  2574 | `		zIn++;` |
|         3 |  2575 | `	}` |
|       111 |  2576 | `	return SXRET_OK;` |
|        57 |  2577 | `}` |
|         - |  2578 | `/*` |
|         - |  2579 | ` * Scan the body token range of an arrow function for free-variable` |
|         - |  2580 | ` * references and record them in pFunc's closure environment. Handles:` |
|         - |  2581 | ` *   - plain $<id> pairs` |
|         - |  2582 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|         - |  2583 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|         - |  2584 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|         - |  2585 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|         - |  2586 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|         - |  2587 | ` *     are never mistakenly captured.` |
|         - |  2588 | ` */` |
|       512 |  2589 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2590 | `	ph7_gen_state *pGen,` |
|         - |  2591 | `	ph7_vm_func *pFunc,` |
|         - |  2592 | `	SyToken *pStart,` |
|         - |  2593 | `	SyToken *pEnd,` |
|         - |  2594 | `	SyString *aShadow,` |
|         - |  2595 | `	sxu32 nShadow)` |
|         4 |  2596 | `{` |
|       516 |  2597 | `	SyToken *pScan = pStart;` |
|         - |  2598 | `	sxi32 rc;` |
|      3438 |  2599 | `	while( pScan < pEnd ){` |
|      2926 |  2600 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       165 |  2601 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        54 |  2602 | `				pScan->sData.zString,` |
|       108 |  2603 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        54 |  2604 | `				aShadow,nShadow);` |
|       111 |  2605 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2606 | `				return SXERR_ABORT;` |
|         - |  2607 | `			}` |
|       111 |  2608 | `			pScan++;` |
|       111 |  2609 | `			continue;` |
|         - |  2610 | `		}` |
|      2818 |  2611 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        39 |  2612 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        39 |  2613 | `			SyToken *pFnKw = pScan;` |
|        36 |  2614 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2615 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         3 |  2616 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2617 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2618 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2619 | `			}` |
|        39 |  2620 | `			if( nKw == PH7_TKWRD_FN ){` |
|         - |  2621 | `				SyToken *pInnerSigStart;` |
|         - |  2622 | `				SyToken *pInnerSigEnd;` |
|         - |  2623 | `				SyToken *pInnerBodyEnd;` |
|         - |  2624 | `				SyString *aInnerShadow;` |
|         - |  2625 | `				sxu32 nInnerShadow;` |
|         - |  2626 | `				sxu32 nInnerParamMax;` |
|         - |  2627 | `				SyToken *p;` |
|         - |  2628 | `				int iNestInner;` |
|        26 |  2629 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|        26 |  2630 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2631 | `					pScan++;` |
|       ! 0 |  2632 | `				}` |
|        26 |  2633 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2634 | `					pScan++;` |
|       ! 0 |  2635 | `					continue;` |
|         - |  2636 | `				}` |
|        26 |  2637 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|        26 |  2638 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|         - |  2639 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|        26 |  2640 | `				if( pInnerSigEnd >= pEnd ){` |
|       ! 0 |  2641 | `					pScan = pEnd;` |
|       ! 0 |  2642 | `					continue;` |
|         - |  2643 | `				}` |
|         - |  2644 | `				/* Build an augmented shadow list: inherited + inner params */` |
|        26 |  2645 | `				nInnerParamMax = 0;` |
|        76 |  2646 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        52 |  2647 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|        20 |  2648 | `						nInnerParamMax++;` |
|         9 |  2649 | `					}` |
|        27 |  2650 | `				}` |
|        26 |  2651 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|        24 |  2652 | `					&pGen->pVm->sAllocator,` |
|        24 |  2653 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|        26 |  2654 | `				if( aInnerShadow == 0 ){` |
|       ! 0 |  2655 | `					return SXERR_ABORT;` |
|         - |  2656 | `				}` |
|        26 |  2657 | `				nInnerShadow = 0;` |
|        32 |  2658 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|         7 |  2659 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|         4 |  2660 | `				}` |
|        76 |  2661 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        52 |  2662 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|        34 |  2663 | `						continue;` |
|         - |  2664 | `					}` |
|        20 |  2665 | `					if( &p[1] >= pInnerSigEnd ){` |
|       ! 0 |  2666 | `						break;` |
|         - |  2667 | `					}` |
|        20 |  2668 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2669 | `						continue;` |
|         - |  2670 | `					}` |
|        20 |  2671 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        11 |  2672 | `				}` |
|        26 |  2673 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|        26 |  2674 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|       ! 0 |  2675 | `					pScan++;` |
|       ! 0 |  2676 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|       ! 0 |  2677 | `						&& pScan->sData.nByte == 1` |
|       ! 0 |  2678 | `						&& pScan->sData.zString[0] == '?' ){` |
|       ! 0 |  2679 | `						pScan++;` |
|       ! 0 |  2680 | `					}` |
|       ! 0 |  2681 | `					if( pScan < pEnd` |
|       ! 0 |  2682 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  2683 | `						pScan++;` |
|       ! 0 |  2684 | `					}` |
|       ! 0 |  2685 | `				}` |
|        26 |  2686 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|        26 |  2687 | `					pScan++; /* past '=>' */` |
|        12 |  2688 | `				}` |
|        26 |  2689 | `				pInnerBodyEnd = pScan;` |
|        26 |  2690 | `				iNestInner = 0;` |
|       156 |  2691 | `				while( pInnerBodyEnd < pEnd ){` |
|       138 |  2692 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|         - |  2693 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|         - |  2694 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|         7 |  2695 | `						break;` |
|         - |  2696 | `					}` |
|       132 |  2697 | `					if( pInnerBodyEnd->nType &` |
|         - |  2698 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         6 |  2699 | `						iNestInner++;` |
|       130 |  2700 | `					}else if( pInnerBodyEnd->nType &` |
|         - |  2701 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         6 |  2702 | `						iNestInner--;` |
|         2 |  2703 | `					}` |
|       132 |  2704 | `					pInnerBodyEnd++;` |
|         2 |  2705 | `				}` |
|         - |  2706 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|         - |  2707 | `				 * the outer's body: a default value is evaluated at call time` |
|         - |  2708 | `				 * in the outer frame, so any free variable it references is` |
|         - |  2709 | `				 * an outer capture. We must NOT scan the parameter-name` |
|         - |  2710 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|         - |  2711 | `				 * or those names leak into the outer's closure environment.` |
|         - |  2712 | `				 *` |
|         - |  2713 | `				 * Walk the signature argument-by-argument, splitting on` |
|         - |  2714 | `				 * top-level commas, and for each argument scan only the token` |
|         - |  2715 | `				 * range after the '=' sign. */` |
|         - |  2716 | `				{` |
|        26 |  2717 | `					SyToken *pArgStart = pInnerSigStart;` |
|        44 |  2718 | `					while( pArgStart < pInnerSigEnd ){` |
|        20 |  2719 | `						SyToken *pArgEnd = pArgStart;` |
|        20 |  2720 | `						SyToken *pEq = 0;` |
|        20 |  2721 | `						int iNestArg = 0;` |
|        68 |  2722 | `						while( pArgEnd < pInnerSigEnd ){` |
|        50 |  2723 | `							if( iNestArg == 0` |
|        52 |  2724 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|         3 |  2725 | `								break;` |
|         - |  2726 | `							}` |
|        50 |  2727 | `							if( pArgEnd->nType &` |
|         - |  2728 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  2729 | `								iNestArg++;` |
|        50 |  2730 | `							}else if( pArgEnd->nType &` |
|         - |  2731 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  2732 | `								iNestArg--;` |
|       ! 0 |  2733 | `							}` |
|        48 |  2734 | `							if( pEq == 0 && iNestArg == 0` |
|        44 |  2735 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|         7 |  2736 | `								pEq = pArgEnd;` |
|         3 |  2737 | `							}` |
|        50 |  2738 | `							pArgEnd++;` |
|         2 |  2739 | `						}` |
|        20 |  2740 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|        10 |  2741 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         3 |  2742 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|         7 |  2743 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  2744 | `								return SXERR_ABORT;` |
|         - |  2745 | `							}` |
|         3 |  2746 | `						}` |
|        20 |  2747 | `						pArgStart = pArgEnd;` |
|        18 |  2748 | `						if( pArgStart < pInnerSigEnd` |
|        12 |  2749 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|         3 |  2750 | `							pArgStart++;` |
|         1 |  2751 | `						}` |
|         2 |  2752 | `					}` |
|         - |  2753 | `				}` |
|        38 |  2754 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        12 |  2755 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|        26 |  2756 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2757 | `					return SXERR_ABORT;` |
|         - |  2758 | `				}` |
|        26 |  2759 | `				pScan = pInnerBodyEnd;` |
|        26 |  2760 | `				continue;` |
|         - |  2761 | `			}` |
|         6 |  2762 | `		}` |
|      2794 |  2763 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      2582 |  2764 | `			pScan++;` |
|      2582 |  2765 | `			continue;` |
|         - |  2766 | `		}` |
|         - |  2767 | `		{` |
|         - |  2768 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       215 |  2769 | `			SyToken *pDollar = pScan;` |
|       318 |  2770 | `			while( &pDollar[1] < pEnd` |
|       215 |  2771 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2772 | `				pDollar++;` |
|       ! 0 |  2773 | `			}` |
|       215 |  2774 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2775 | `				break;` |
|         - |  2776 | `			}` |
|       215 |  2777 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2778 | `				pScan = pDollar + 1;` |
|       ! 0 |  2779 | `				continue;` |
|         - |  2780 | `			}` |
|       321 |  2781 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       212 |  2782 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       106 |  2783 | `				aShadow,nShadow);` |
|       215 |  2784 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2785 | `				return SXERR_ABORT;` |
|         - |  2786 | `			}` |
|       215 |  2787 | `			pScan = pDollar + 2;` |
|         - |  2788 | `		}` |
|         3 |  2789 | `	}` |
|       516 |  2790 | `	return SXRET_OK;` |
|       260 |  2791 | `}` |
|         - |  2792 | `/*` |
|         - |  2793 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2794 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2795 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2796 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2797 | ` * $this is also made available.` |
|         - |  2798 | ` */` |
|       488 |  2799 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2800 | `{` |
|         - |  2801 | `	ph7_vm_func *pFunc;` |
|         - |  2802 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2803 | `	GenBlock *pBlock;` |
|         - |  2804 | `	SySet *pInstrContainer;` |
|         - |  2805 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|         - |  2806 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|         - |  2807 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|         - |  2808 | `	SyToken *pSavedEnd;` |
|         - |  2809 | `	ph7_vm_func_arg *aArgs;` |
|         - |  2810 | `	char zName[512];` |
|         - |  2811 | `	static int iCnt = 1;` |
|         - |  2812 | `	char *zDup;` |
|         - |  2813 | `	SyToken *pTokKw;` |
|         - |  2814 | `	sxu32 nLen;` |
|         - |  2815 | `	sxu32 nLine;` |
|       493 |  2816 | `	sxi32 iFlags = 0;` |
|       493 |  2817 | `	int bStatic = 0;` |
|         - |  2818 | `	sxi32 rc;` |
|         - |  2819 | `	sxu32 n;` |
|       244 |  2820 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2821 |  |
|       493 |  2822 | `	nLine = pGen->pIn->nLine;` |
|         - |  2823 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       493 |  2824 | `	pTokKw = pGen->pIn;` |
|         - |  2825 | `	/* Optional 'static' prefix */` |
|       488 |  2826 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       493 |  2827 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         9 |  2828 | `		bStatic = 1;` |
|         9 |  2829 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         9 |  2830 | `		pGen->pIn++;` |
|         4 |  2831 | `	}` |
|         - |  2832 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       488 |  2833 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       493 |  2834 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2835 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2836 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2837 | `		return SXERR_SYNTAX;` |
|         - |  2838 | `	}` |
|       493 |  2839 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2840 | `	/* Optional '&' — return by reference */` |
|       493 |  2841 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2842 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2843 | `		pGen->pIn++;` |
|       ! 0 |  2844 | `	}` |
|         - |  2845 | `	/* Expect '(' */` |
|       493 |  2846 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  2847 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2848 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2849 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|         2 |  2850 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2851 | `		}else{` |
|       ! 0 |  2852 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2853 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|         - |  2854 | `		}` |
|         3 |  2855 | `		return SXERR_SYNTAX;` |
|         - |  2856 | `	}` |
|       491 |  2857 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2858 | `	/* Delimit the parameter list */` |
|       491 |  2859 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       491 |  2860 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2861 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2862 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2863 | `		return SXERR_SYNTAX;` |
|         - |  2864 | `	}` |
|         - |  2865 | `	/* Allocate the function state */` |
|       488 |  2866 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       488 |  2867 | `	if( pFunc == 0 ){` |
|       ! 0 |  2868 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2869 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2870 | `		return SXERR_ABORT;` |
|         - |  2871 | `	}` |
|         - |  2872 | `	/* Generate a unique lambda name */` |
|       488 |  2873 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       488 |  2874 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2875 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2876 | `	}` |
|       488 |  2877 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       488 |  2878 | `	if( zDup == 0 ){` |
|       ! 0 |  2879 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2880 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2881 | `		return SXERR_ABORT;` |
|         - |  2882 | `	}` |
|       488 |  2883 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2884 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       488 |  2885 | `	pFunc->nLine = nLine;` |
|         - |  2886 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       488 |  2887 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2888 | `		return SXERR_ABORT;` |
|         - |  2889 | `	}` |
|         - |  2890 | `	/* Collect function arguments */` |
|       488 |  2891 | `	if( pGen->pIn < pSigEnd ){` |
|       133 |  2892 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       133 |  2893 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2894 | `			return SXERR_ABORT;` |
|         - |  2895 | `		}` |
|        65 |  2896 | `	}` |
|         - |  2897 | `	/* Point past ')' and parse optional return type */` |
|       488 |  2898 | `	pGen->pIn = &pSigEnd[1];` |
|       488 |  2899 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       488 |  2900 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2901 | `		return SXERR_ABORT;` |
|       488 |  2902 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2903 | `		return SXERR_SYNTAX;` |
|         - |  2904 | `	}` |
|         - |  2905 | `	/* Expect '=>' */` |
|       488 |  2906 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  2907 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2908 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2909 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|         2 |  2910 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2911 | `		}else{` |
|       ! 0 |  2912 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2913 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|         - |  2914 | `		}` |
|         3 |  2915 | `		return SXERR_SYNTAX;` |
|         - |  2916 | `	}` |
|       486 |  2917 | `	pGen->pIn++; /* Jump '=>' */` |
|       486 |  2918 | `	pBodyStart = pGen->pIn;` |
|       486 |  2919 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2920 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2921 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2922 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2923 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       486 |  2924 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2925 | `	{` |
|       486 |  2926 | `		SyString *aShadow = 0;` |
|       486 |  2927 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       486 |  2928 | `		if( nShadow > 0 ){` |
|       131 |  2929 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       128 |  2930 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       131 |  2931 | `			if( aShadow == 0 ){` |
|       ! 0 |  2932 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2933 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2934 | `				return SXERR_ABORT;` |
|         - |  2935 | `			}` |
|       295 |  2936 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       167 |  2937 | `				aShadow[n] = aArgs[n].sName;` |
|        85 |  2938 | `			}` |
|        64 |  2939 | `		}` |
|       727 |  2940 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       241 |  2941 | `			aShadow,nShadow);` |
|       486 |  2942 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2943 | `			return SXERR_ABORT;` |
|         - |  2944 | `		}` |
|         - |  2945 | `	}` |
|         - |  2946 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2947 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2948 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2949 | `	 * $this. */` |
|       486 |  2950 | `	if( !bStatic ){` |
|         - |  2951 | `		char *zThisDup;` |
|       478 |  2952 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       478 |  2953 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2954 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2955 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2956 | `			return SXERR_ABORT;` |
|         - |  2957 | `		}` |
|       478 |  2958 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       478 |  2959 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       478 |  2960 | `		sEnv.nIdx = SXU32_HIGH;` |
|       478 |  2961 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       478 |  2962 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       478 |  2963 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       237 |  2964 | `	}` |
|         - |  2965 | `	/* Arrow functions are always closures */` |
|       486 |  2966 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2967 | `	/* Compile the body expression as an implicit return */` |
|       727 |  2968 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       241 |  2969 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       486 |  2970 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2971 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2972 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2973 | `		return SXERR_ABORT;` |
|         - |  2974 | `	}` |
|       486 |  2975 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       486 |  2976 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       486 |  2977 | `	pSavedEnd = pGen->pEnd;` |
|       486 |  2978 | `	pGen->pIn = pBodyStart;` |
|       486 |  2979 | `	pGen->pEnd = pBodyEnd;` |
|       486 |  2980 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       486 |  2981 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2982 | `		return SXERR_ABORT;` |
|         - |  2983 | `	}` |
|         - |  2984 | `	/* The cursor stopped just past the body expression */` |
|       486 |  2985 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2986 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2987 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2988 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2989 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       486 |  2990 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       486 |  2991 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       486 |  2992 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       486 |  2993 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       486 |  2994 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2995 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       486 |  2996 | `	pGen->pIn = pBodyEnd;` |
|       486 |  2997 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2998 | `	/* Emit the load-closure instruction */` |
|       486 |  2999 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       486 |  3000 | `	return SXRET_OK;` |
|       249 |  3001 | `}` |
|         - |  3002 | `/*` |
|         - |  3003 | ` * Compile a single arm's expression range into a freshly-allocated` |
|         - |  3004 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|         - |  3005 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|         - |  3006 | ` * expression's value.` |
|         - |  3007 | ` */` |
|       354 |  3008 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|         - |  3009 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|         3 |  3010 | `{` |
|         - |  3011 | `	SySet *pInstrContainer;` |
|         - |  3012 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  3013 | `	GenBlock *pArmBlock;` |
|         - |  3014 | `	sxi32 rc;` |
|       357 |  3015 | `	pTmpIn  = pGen->pIn;` |
|       357 |  3016 | `	pTmpEnd = pGen->pEnd;` |
|       357 |  3017 | `	pGen->pIn  = pStart;` |
|       357 |  3018 | `	pGen->pEnd = pStop;` |
|       357 |  3019 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       357 |  3020 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|         - |  3021 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|         - |  3022 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|         - |  3023 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|         - |  3024 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|         - |  3025 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|       534 |  3026 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       177 |  3027 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|       357 |  3028 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3029 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  3030 | `		pGen->pIn  = pTmpIn;` |
|       ! 0 |  3031 | `		pGen->pEnd = pTmpEnd;` |
|       ! 0 |  3032 | `		return SXERR_ABORT;` |
|         - |  3033 | `	}` |
|       357 |  3034 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       357 |  3035 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       357 |  3036 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       357 |  3037 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       357 |  3038 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       357 |  3039 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       357 |  3040 | `	pGen->pIn  = pTmpIn;` |
|       357 |  3041 | `	pGen->pEnd = pTmpEnd;` |
|       357 |  3042 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3043 | `		return SXERR_ABORT;` |
|         - |  3044 | `	}` |
|       357 |  3045 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 |  3046 | `		return SXERR_EMPTY;` |
|         - |  3047 | `	}` |
|       357 |  3048 | `	return SXRET_OK;` |
|       180 |  3049 | `}` |
|         - |  3050 | `/*` |
|         - |  3051 | ` * Compile a PHP 8.0 match expression:` |
|         - |  3052 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|         - |  3053 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|         - |  3054 | ` * Strict comparison (===) is used between the subject and each condition.` |
|         - |  3055 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|         - |  3056 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|         - |  3057 | ` */` |
|         - |  3058 | `/*` |
|         - |  3059 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|         - |  3060 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|         - |  3061 | ` * caller can bail out of the current expression.` |
|         - |  3062 | ` */` |
|         2 |  3063 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|         1 |  3064 | `{` |
|         - |  3065 | `	va_list ap;` |
|         - |  3066 | `	sxi32 rc;` |
|         - |  3067 | `	SyBlob sMsg;` |
|         3 |  3068 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|         3 |  3069 | `	va_start(ap,zFmt);` |
|         3 |  3070 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|         3 |  3071 | `	va_end(ap);` |
|         3 |  3072 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|         3 |  3073 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|         3 |  3074 | `	SyBlobRelease(&sMsg);` |
|         3 |  3075 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3076 | `		return SXERR_ABORT;` |
|         - |  3077 | `	}` |
|         3 |  3078 | `	return SXERR_SYNTAX;` |
|         2 |  3079 | `}` |
|         - |  3080 | `/*` |
|         - |  3081 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|         - |  3082 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|         - |  3083 | ` * Returns the stop token pointer (or pEnd if none found).` |
|         - |  3084 | ` */` |
|       356 |  3085 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|         4 |  3086 | `{` |
|       360 |  3087 | `	SyToken *pCur = pStart;` |
|       360 |  3088 | `	int iNest = 0;` |
|       838 |  3089 | `	while( pCur < pEnd ){` |
|       802 |  3090 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        13 |  3091 | `			iNest++;` |
|       796 |  3092 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        13 |  3093 | `			iNest--;` |
|       784 |  3094 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|       323 |  3095 | `			return pCur;` |
|         - |  3096 | `		}` |
|       482 |  3097 | `		pCur++;` |
|         4 |  3098 | `	}` |
|        39 |  3099 | `	return pEnd;` |
|       182 |  3100 | `}` |
|        72 |  3101 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3102 | `{` |
|         - |  3103 | `	ph7_match *pMatch;` |
|         - |  3104 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|        77 |  3105 | `	int bHasDefault = 0;` |
|         - |  3106 | `	sxu32 nLine;` |
|         - |  3107 | `	sxi32 rc;` |
|        36 |  3108 | `	SXUNUSED(iCompileFlag);` |
|        77 |  3109 | `	nLine = pGen->pIn->nLine;` |
|        77 |  3110 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|         - |  3111 | `	/* Expect '(' */` |
|        77 |  3112 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  3113 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3114 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|       ! 0 |  3115 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|         - |  3116 | `	}` |
|        77 |  3117 | `	pGen->pIn++; /* Jump '(' */` |
|        77 |  3118 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|        77 |  3119 | `	if( pSubjEnd >= pGen->pEnd ){` |
|       ! 0 |  3120 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3121 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         - |  3122 | `	}` |
|        77 |  3123 | `	if( pGen->pIn >= pSubjEnd ){` |
|       ! 0 |  3124 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3125 | `			"syntax error, unexpected \")\", expecting match subject");` |
|         - |  3126 | `	}` |
|         - |  3127 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|        77 |  3128 | `	pSavedEnd = pGen->pEnd;` |
|        77 |  3129 | `	pGen->pEnd = pSubjEnd;` |
|        77 |  3130 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        77 |  3131 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3132 | `		return SXERR_ABORT;` |
|         - |  3133 | `	}` |
|        77 |  3134 | `	pGen->pEnd = pSavedEnd;` |
|        77 |  3135 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|         - |  3136 | `	/* Expect '{' */` |
|        77 |  3137 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 |  3138 | `		return GenStateMatchError(pGen,` |
|       ! 0 |  3139 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  3140 | `			"syntax error, expecting \"{\" after match subject");` |
|         - |  3141 | `	}` |
|        77 |  3142 | `	pGen->pIn++; /* Jump '{' */` |
|        77 |  3143 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|        77 |  3144 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  3145 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3146 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|         - |  3147 | `	}` |
|         - |  3148 | `	/* Allocate ph7_match container */` |
|        77 |  3149 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|        77 |  3150 | `	if( pMatch == 0 ){` |
|       ! 0 |  3151 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  3152 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3153 | `		return SXERR_ABORT;` |
|         - |  3154 | `	}` |
|        77 |  3155 | `	SyZero(pMatch,sizeof(ph7_match));` |
|        77 |  3156 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|         - |  3157 | `	/* Iterate arms */` |
|       259 |  3158 | `	while( pGen->pIn < pBodyEnd ){` |
|         - |  3159 | `		ph7_match_arm sArm;` |
|         - |  3160 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|       190 |  3161 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|       190 |  3162 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|       190 |  3163 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|       190 |  3164 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3165 | `		/* 'default' arm? */` |
|       186 |  3166 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       107 |  3167 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|        22 |  3168 | `			if( bHasDefault ){` |
|         3 |  3169 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|         - |  3170 | `					"Match expressions may only contain one default arm");` |
|         4 |  3171 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  3172 | `			}` |
|        20 |  3173 | `			sArm.bDefault = 1;` |
|        20 |  3174 | `			bHasDefault = 1;` |
|        20 |  3175 | `			pGen->pIn++;` |
|        20 |  3176 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       ! 0 |  3177 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3178 | `					"syntax error, expecting \"=>\" after 'default'");` |
|         - |  3179 | `			}` |
|        20 |  3180 | `			pGen->pIn++; /* Jump '=>' */` |
|        11 |  3181 | `		}else{` |
|         - |  3182 | `			/* Condition list: cond (',' cond)* '=>' */` |
|       170 |  3183 | `			pCondStart = pGen->pIn;` |
|       170 |  3184 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|         - |  3185 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       178 |  3186 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|         - |  3187 | `				SySet sCondBc;` |
|         9 |  3188 | `				if( pCondStart >= pArrow ){` |
|       ! 0 |  3189 | `					return GenStateMatchError(pGen,nArmLine,` |
|         - |  3190 | `						"syntax error, empty match condition expression");` |
|         - |  3191 | `				}` |
|         9 |  3192 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         9 |  3193 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|         9 |  3194 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3195 | `					return SXERR_ABORT;` |
|         - |  3196 | `				}` |
|         9 |  3197 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         9 |  3198 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|         9 |  3199 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|         - |  3200 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|         1 |  3201 | `			}` |
|       170 |  3202 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  3203 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3204 | `					"syntax error, expecting \"=>\" in match arm");` |
|         - |  3205 | `			}` |
|       167 |  3206 | `			if( pCondStart >= pArrow ){` |
|       ! 0 |  3207 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3208 | `					"syntax error, empty match condition expression");` |
|         - |  3209 | `			}` |
|         - |  3210 | `			{` |
|         - |  3211 | `				SySet sCondBc;` |
|       167 |  3212 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       167 |  3213 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       167 |  3214 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3215 | `					return SXERR_ABORT;` |
|         - |  3216 | `				}` |
|       167 |  3217 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         - |  3218 | `			}` |
|       167 |  3219 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|         - |  3220 | `		}` |
|         - |  3221 | `		/* Compile result expression: up to top-level ',' or body end */` |
|       185 |  3222 | `		pResStart = pGen->pIn;` |
|       185 |  3223 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|       185 |  3224 | `		if( pResStart >= pResEnd ){` |
|       ! 0 |  3225 | `			return GenStateMatchError(pGen,nArmLine,` |
|         - |  3226 | `				"syntax error, expected expression after \"=>\"");` |
|         - |  3227 | `		}` |
|       185 |  3228 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|       185 |  3229 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3230 | `			return SXERR_ABORT;` |
|         - |  3231 | `		}` |
|       185 |  3232 | `		pGen->pIn = pResEnd;` |
|       185 |  3233 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       151 |  3234 | `			pGen->pIn++; /* Skip trailing ',' */` |
|        74 |  3235 | `		}` |
|       185 |  3236 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|         3 |  3237 | `	}` |
|        71 |  3238 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|        71 |  3239 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|        71 |  3240 | `	return SXRET_OK;` |
|        41 |  3241 | `}` |
|         - |  3242 | `/*` |
|         - |  3243 | ` * Compile a backtick quoted string.` |
|         - |  3244 | ` */` |
|         4 |  3245 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  3246 | `{` |
|         - |  3247 | `	static const SyString sName = { "shell_exec", sizeof("shell_exec")-1 };` |
|         6 |  3248 | `	sxu32 nIdx = 0;` |
|         - |  3249 | `	sxi32 rc;` |
|         - |  3250 | `	/*` |
|         - |  3251 | ``	 * `cmd` IS shell_exec("cmd") in php — it interpolates like a double-quoted string,`` |
|         - |  3252 | `	 * runs the command and yields its output. PH7 refused to run it at all (TICKET` |
|         - |  3253 | `	 * 1433-40) and quietly evaluated to NULL. php 8.5 deprecates the syntax but still` |
|         - |  3254 | `	 * executes it, so compile it to the real call and say what php says.` |
|         - |  3255 | `	 */` |
|         6 |  3256 | `	PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pGen->pIn->nLine,` |
|         - |  3257 | ``		"The backtick (`) operator is deprecated, use shell_exec() instead");`` |
|         - |  3258 | `	/* The body interpolates exactly like a double-quoted string */` |
|         6 |  3259 | `	pGen->pIn->nType &= ~PH7_TK_BSTR;` |
|         6 |  3260 | `	pGen->pIn->nType \|= PH7_TK_DSTR;` |
|         6 |  3261 | `	rc = PH7_CompileString(&(*pGen),iCompileFlag);` |
|         6 |  3262 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3263 | `		return rc;` |
|         - |  3264 | `	}` |
|         - |  3265 | `	/* ... and the command string is then handed to shell_exec() */` |
|         6 |  3266 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|         6 |  3267 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         6 |  3268 | `		if( pObj == 0 ){` |
|       ! 0 |  3269 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  3270 | `			return SXERR_ABORT;` |
|         - |  3271 | `		}` |
|         6 |  3272 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|         6 |  3273 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|         2 |  3274 | `	}` |
|         6 |  3275 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         6 |  3276 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         6 |  3277 | `	return SXRET_OK;` |
|         4 |  3278 | `}` |
|         - |  3279 | `/*` |
|         - |  3280 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|         - |  3281 | ` * construct.` |
|         - |  3282 | ` */` |
|        68 |  3283 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3284 | `{` |
|         - |  3285 | `	SyString *pName;` |
|         - |  3286 | `	sxu32 nKeyID;` |
|         - |  3287 | `	sxi32 rc;` |
|         - |  3288 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        73 |  3289 | `	pName = &pGen->pIn->sData;` |
|        73 |  3290 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        73 |  3291 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        73 |  3292 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|         9 |  3293 | `		SyToken *pTmp,*pNext = 0;` |
|         - |  3294 | `		/* Compile arguments one after one */` |
|         9 |  3295 | `		pTmp = pGen->pEnd;` |
|         - |  3296 | `		/* Symisc eXtension to the PHP programming language:` |
|         - |  3297 | `		 * 'echo' can be used in the context of a function which` |
|         - |  3298 | `		 *  mean that the following expression is valid:` |
|         - |  3299 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|         - |  3300 | `		 */` |
|         9 |  3301 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        17 |  3302 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|         9 |  3303 | `			if( pGen->pIn < pNext ){` |
|         9 |  3304 | `				pGen->pEnd = pNext;` |
|         9 |  3305 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|         9 |  3306 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3307 | `					return SXERR_ABORT;` |
|         - |  3308 | `				}` |
|         9 |  3309 | `				if( rc != SXERR_EMPTY ){` |
|         - |  3310 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|         - |  3311 | `					 * without the overhead of a function call.` |
|         - |  3312 | `					 * This is a very powerful optimization that improve` |
|         - |  3313 | `					 * performance greatly.` |
|         - |  3314 | `					 */` |
|         9 |  3315 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|         4 |  3316 | `				}` |
|         4 |  3317 | `			}` |
|         - |  3318 | `			/* Jump trailing commas */` |
|         9 |  3319 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|       ! 0 |  3320 | `				pNext++;` |
|       ! 0 |  3321 | `			}` |
|         9 |  3322 | `			pGen->pIn = pNext;` |
|         1 |  3323 | `		}` |
|         - |  3324 | `		/* Restore token stream */` |
|         9 |  3325 | `		pGen->pEnd = pTmp;` |
|         5 |  3326 | `	}else{` |
|        65 |  3327 | `		sxi32 nArg = 0;` |
|        65 |  3328 | `		sxu32 nIdx = 0;` |
|        65 |  3329 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        65 |  3330 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3331 | `			return SXERR_ABORT;` |
|        65 |  3332 | `		}else if(rc != SXERR_EMPTY ){` |
|        65 |  3333 | `			nArg = 1;` |
|        30 |  3334 | `		}` |
|        65 |  3335 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3336 | `			ph7_value *pObj;` |
|         - |  3337 | `			/* Emit the call instruction */` |
|        29 |  3338 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        29 |  3339 | `			if( pObj == 0 ){` |
|       ! 0 |  3340 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3341 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3342 | `				return SXERR_ABORT;` |
|         - |  3343 | `			}` |
|        29 |  3344 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3345 | `			/* Install in the literal table */` |
|        29 |  3346 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        12 |  3347 | `		}` |
|         - |  3348 | `		/* Emit the call instruction */` |
|        65 |  3349 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        65 |  3350 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3351 | `	}` |
|         - |  3352 | `	/* Node successfully compiled */` |
|        73 |  3353 | `	return SXRET_OK;` |
|        39 |  3354 | `}` |
|         - |  3355 | `/*` |
|         - |  3356 | ` * Compile a node holding a variable declaration.` |
|         - |  3357 | ` * According to the PHP language reference` |
|         - |  3358 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|         - |  3359 | ` *  The variable name is case-sensitive.` |
|         - |  3360 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|         - |  3361 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3362 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|         - |  3363 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|         - |  3364 | ` *  Note: $this is a special variable that can't be assigned.` |
|         - |  3365 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|         - |  3366 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|         - |  3367 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|         - |  3368 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|         - |  3369 | ` *  the chapter on Expressions.` |
|         - |  3370 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|         - |  3371 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|         - |  3372 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|         - |  3373 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|         - |  3374 | ` *  is being assigned (the source variable).` |
|         - |  3375 | ` */` |
|  19334778 |  3376 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3377 | `{` |
|  19334783 |  3378 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3379 | `	sxi32 iVv;` |
|         - |  3380 | `	sxi32 iP1;` |
|         - |  3381 | `	void *p3;` |
|         - |  3382 | `	sxi32 rc;` |
|  19334783 |  3383 | `	iVv = -1; /* Variable variable counter */` |
|  38669573 |  3384 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  19334795 |  3385 | `		pGen->pIn++;` |
|  19334795 |  3386 | `		iVv++;` |
|         5 |  3387 | `	}` |
|  19334783 |  3388 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3389 | `		/* Invalid variable name */` |
|       ! 0 |  3390 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3391 | `		if( rc == SXERR_ABORT ){` |
|         - |  3392 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3393 | `			return SXERR_ABORT;` |
|         - |  3394 | `		}` |
|       ! 0 |  3395 | `		return SXRET_OK;` |
|         - |  3396 | `	}` |
|  19334783 |  3397 | `	p3  = 0;` |
|  19334783 |  3398 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|         - |  3399 | `		/* Dynamic variable creation */` |
|        19 |  3400 | `		pGen->pIn++;  /* Jump the open curly */` |
|        19 |  3401 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|        19 |  3402 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3403 | `			/* Empty expression */` |
|         - |  3404 | `			{` |
|         - |  3405 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|         - |  3406 | `			 * the "expecting" tail only appears when something could still follow. */` |
|         3 |  3407 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|         3 |  3408 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|         1 |  3409 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|         - |  3410 | `			}` |
|         3 |  3411 | `			return SXRET_OK;` |
|         - |  3412 | `		}` |
|         - |  3413 | `		/* Compile the expression holding the variable name */` |
|        16 |  3414 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        16 |  3415 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3416 | `			return SXERR_ABORT;` |
|        16 |  3417 | `		}else if( rc == SXERR_EMPTY ){` |
|         3 |  3418 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|         3 |  3419 | `			return SXRET_OK;` |
|         - |  3420 | `		}` |
|         7 |  3421 | `	}else{` |
|         - |  3422 | `		SyHashEntry *pEntry;` |
|         - |  3423 | `		SyString *pName;` |
|  19334767 |  3424 | `		char *zName = 0;` |
|         - |  3425 | `		/* Extract variable name */` |
|  19334767 |  3426 | `		pName = &pGen->pIn->sData;` |
|         - |  3427 | `		/* Advance the stream cursor */` |
|  19334767 |  3428 | `		pGen->pIn++;` |
|  19334767 |  3429 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  19334767 |  3430 | `		if( pEntry == 0 ){` |
|         - |  3431 | `			/* Duplicate name */` |
|   1161503 |  3432 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   1161503 |  3433 | `			if( zName == 0 ){` |
|       ! 0 |  3434 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3435 | `				return SXERR_ABORT;` |
|         - |  3436 | `			}` |
|         - |  3437 | `			/* Install in the hashtable */` |
|   1161503 |  3438 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    580754 |  3439 | `		}else{` |
|         - |  3440 | `			/* Name already available */` |
|  18173269 |  3441 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3442 | `		}` |
|  19334767 |  3443 | `		p3 = (void *)zName;` |
|         - |  3444 | `	}` |
|  19334779 |  3445 | `	iP1 = 0;` |
|  19334779 |  3446 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   5714871 |  3447 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3448 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   5711023 |  3449 | `			iP1 = 1;` |
|   2855509 |  3450 | `		}` |
|   2857433 |  3451 | `	}` |
|         - |  3452 | `	/* Emit the load instruction */` |
|  19334779 |  3453 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  19334791 |  3454 | `	while( iVv > 0 ){` |
|        13 |  3455 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3456 | `		iVv--;` |
|         1 |  3457 | `	}` |
|         - |  3458 | `	/* Node successfully compiled */` |
|  19334779 |  3459 | `	return SXRET_OK;` |
|   9667394 |  3460 | `}` |
|         - |  3461 | `/*` |
|         - |  3462 | ` * Load a literal.` |
|         - |  3463 | ` */` |
|  11951700 |  3464 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3465 | `{` |
|  11951705 |  3466 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3467 | `	ph7_value *pObj;` |
|         - |  3468 | `	SyString *pStr;` |
|         - |  3469 | `	sxu32 nIdx;` |
|         - |  3470 | `	/* Extract token value */` |
|  11951705 |  3471 | `	pStr = &pToken->sData;` |
|         - |  3472 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|         - |  3473 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|         - |  3474 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|         - |  3475 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
|  11951705 |  3476 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|         - |  3477 | `		/* fall through to the plain-string literal path */` |
|   9592509 |  3478 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   1460443 |  3479 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3480 | `			/* NULL constant are always indexed at 0 */` |
|    983741 |  3481 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    983741 |  3482 | `			return SXRET_OK;` |
|    476707 |  3483 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3484 | `			/* TRUE constant are always indexed at 1 */` |
|    326255 |  3485 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    326255 |  3486 | `			return SXRET_OK;` |
|         5 |  3487 | `		}` |
|   6584143 |  3488 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1472084 |  3489 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3490 | `			/* FALSE constant are always indexed at 2 */` |
|    715751 |  3491 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    715751 |  3492 | `			return SXRET_OK;` |
|   5186146 |  3493 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    258034 |  3494 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3495 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      3837 |  3496 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3837 |  3497 | `			if( pObj == 0 ){` |
|       ! 0 |  3498 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3499 | `				return SXERR_ABORT;` |
|         - |  3500 | `			}` |
|      3837 |  3501 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3502 | `			/* Emit the load constant instruction */` |
|      3837 |  3503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3837 |  3504 | `			return SXRET_OK;` |
|   5180393 |  3505 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|    458481 |  3506 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|   5253674 |  3507 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|    408548 |  3508 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|         - |  3509 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|         - |  3510 | `			 * file being compiled (where the token is written), NOT the runtime` |
|         - |  3511 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|         - |  3512 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|         - |  3513 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|         - |  3514 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|      3919 |  3515 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|      3919 |  3516 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      3919 |  3517 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3919 |  3518 | `			if( pObj == 0 ){` |
|       ! 0 |  3519 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3520 | `				return SXERR_ABORT;` |
|         - |  3521 | `			}` |
|      3919 |  3522 | `			if( pFile && pFile->nByte > 0 ){` |
|        95 |  3523 | `				if( bDir ){` |
|         - |  3524 | `					const char *zDir;` |
|         - |  3525 | `					int nLen;` |
|         - |  3526 | `					SyString sDir;` |
|        47 |  3527 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|        47 |  3528 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|        47 |  3529 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|        25 |  3530 | `				}else{` |
|        51 |  3531 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|         - |  3532 | `				}` |
|        50 |  3533 | `			}else{` |
|         - |  3534 | `				SyString sMem;` |
|      3829 |  3535 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|      3829 |  3536 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|         - |  3537 | `			}` |
|      3919 |  3538 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3919 |  3539 | `			return SXRET_OK;` |
|   5151064 |  3540 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    203362 |  3541 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3542 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         7 |  3543 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3544 | `			if( pObj == 0 ){` |
|       ! 0 |  3545 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3546 | `				return SXERR_ABORT;` |
|         - |  3547 | `			}` |
|         7 |  3548 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3549 | `				SyString sNs;` |
|         7 |  3550 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         7 |  3551 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         4 |  3552 | `			}else{` |
|       ! 0 |  3553 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3554 | `			}` |
|         7 |  3555 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         7 |  3556 | `			return SXRET_OK;` |
|   5162745 |  3557 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    376855 |  3558 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   5199469 |  3559 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    300208 |  3560 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3561 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3562 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3563 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3564 | `				/* Point to the upper block */` |
|        11 |  3565 | `				pBlock = pBlock->pParent;` |
|         1 |  3566 | `			}` |
|        11 |  3567 | `			if( pBlock == 0 ){` |
|         - |  3568 | `				/* Called in the global scope,load NULL */` |
|         5 |  3569 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3570 | `			}else{` |
|         - |  3571 | `				/* Extract the target function/method */` |
|         7 |  3572 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3573 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|         7 |  3574 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3575 | `				if( pObj == 0 ){` |
|       ! 0 |  3576 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3577 | `					return SXERR_ABORT;` |
|         - |  3578 | `				}` |
|         - |  3579 | `				/*` |
|         - |  3580 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|         - |  3581 | `				 * function name inside a plain function (php does not answer "" there —` |
|         - |  3582 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|         - |  3583 | `				 * unqualified in every method).` |
|         - |  3584 | `				 */` |
|         8 |  3585 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 |  3586 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         - |  3587 | `					SyBlob sQual;` |
|         - |  3588 | `					SyString sOut;` |
|         3 |  3589 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|         3 |  3590 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|         3 |  3591 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|         3 |  3592 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|         3 |  3593 | `					SyBlobRelease(&sQual);` |
|         2 |  3594 | `				}else{` |
|         5 |  3595 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3596 | `				}` |
|         - |  3597 | `				/* Emit the load constant instruction */` |
|         7 |  3598 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3599 | `			}` |
|        11 |  3600 | `			return SXRET_OK;` |
|         - |  3601 | `	}` |
|         - |  3602 | `	/* Query literal table */` |
|   9918211 |  3603 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3604 | `		ph7_value *pLitObj;` |
|         - |  3605 | `		/* Unknown literal,install it in the literal table */` |
|   1891799 |  3606 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1891799 |  3607 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3608 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3609 | `			return SXERR_ABORT;` |
|         - |  3610 | `		}` |
|   1891799 |  3611 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1891799 |  3612 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    945897 |  3613 | `	}` |
|         - |  3614 | `	/* Emit the load constant instruction */` |
|   9918211 |  3615 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9918211 |  3616 | `	return SXRET_OK;` |
|   5975855 |  3617 | `}` |
|         - |  3618 | `/*` |
|         - |  3619 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3620 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3621 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3622 | ` * Otherwise, load the simple literal directly.` |
|         - |  3623 | ` */` |
|  11955608 |  3624 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3625 | `{` |
|         - |  3626 | `	sxi32 rc;` |
|  11955613 |  3627 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3628 | `		return SXRET_OK;` |
|         - |  3629 | `	}` |
|         - |  3630 | `	/* Check if this is a multi-token namespace path */` |
|  11955613 |  3631 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3632 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3913 |  3633 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3913 |  3634 | `		int isAbsolute = 0;` |
|      3913 |  3635 | `		SyBlobReset(pWorker);` |
|         - |  3636 | `		/* Check for leading backslash (absolute path) */` |
|      3913 |  3637 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3897 |  3638 | `			isAbsolute = 1;` |
|      3897 |  3639 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1946 |  3640 | `		}` |
|         - |  3641 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|         - |  3642 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|         - |  3643 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|         - |  3644 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|         - |  3645 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|         - |  3646 | `		{` |
|         - |  3647 | `			SyBlob sRaw;` |
|      3913 |  3648 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|      4065 |  3649 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4065 |  3650 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        81 |  3651 | `					SyBlobAppend(&sRaw,"\\",1);` |
|        43 |  3652 | `				}else{` |
|      3989 |  3653 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3654 | `				}` |
|      4065 |  3655 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3913 |  3656 | `					pGen->pIn++;` |
|      3913 |  3657 | `					break;` |
|         - |  3658 | `				}` |
|       157 |  3659 | `				pGen->pIn++;` |
|         5 |  3660 | `			}` |
|      3913 |  3661 | `			if( isAbsolute ){` |
|      3897 |  3662 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|      1951 |  3663 | `			}else{` |
|        18 |  3664 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|        18 |  3665 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|        18 |  3666 | `				sxu32 nFirst = 0;` |
|         - |  3667 | `				SyHashEntry *pNsImp;` |
|        84 |  3668 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|        18 |  3669 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|        18 |  3670 | `				if( pNsImp ){` |
|         - |  3671 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|        15 |  3672 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|        15 |  3673 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|        15 |  3674 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|        10 |  3675 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3676 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3677 | `					SyBlobAppend(pWorker,"\\",1);` |
|         3 |  3678 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|         2 |  3679 | `				}else{` |
|       ! 0 |  3680 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|         - |  3681 | `				}` |
|         - |  3682 | `			}` |
|      3913 |  3683 | `			SyBlobRelease(&sRaw);` |
|         - |  3684 | `		}` |
|      3913 |  3685 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3686 | `			ph7_value *pObj;` |
|         - |  3687 | `			SyString sPath;` |
|         - |  3688 | `			sxu32 nIdx;` |
|      3913 |  3689 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3690 | `			/* Install in the literal table */` |
|      3913 |  3691 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3859 |  3692 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3859 |  3693 | `				if( pObj == 0 ){` |
|       ! 0 |  3694 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3695 | `					return SXERR_ABORT;` |
|         - |  3696 | `				}` |
|      3859 |  3697 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3859 |  3698 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1927 |  3699 | `			}` |
|         - |  3700 | `			/* Emit the load constant instruction.` |
|         - |  3701 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3702 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5867 |  3703 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1954 |  3704 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1954 |  3705 | `				nIdx,0,0);` |
|      3913 |  3706 | `			return SXRET_OK;` |
|         - |  3707 | `		}` |
|       ! 0 |  3708 | `	}` |
|         - |  3709 | `	/* Single-token literal: load directly */` |
|  11951705 |  3710 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11951705 |  3711 | `	return rc;` |
|   5977809 |  3712 | `}` |
|         - |  3713 | `/*` |
|         - |  3714 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3715 | ` */` |
|         - |  3716 | `/*` |
|         - |  3717 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3718 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3719 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3720 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3721 | ` */` |
|       ! 0 |  3722 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3723 | `{` |
|       ! 0 |  3724 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3725 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3726 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3727 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3728 | `}` |
|  11955608 |  3729 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3730 | `{` |
|         - |  3731 | `	sxi32 rc;` |
|  11955613 |  3732 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11955613 |  3733 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3734 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3735 | `		return rc;` |
|         - |  3736 | `	}` |
|         - |  3737 | `	/* Node successfully compiled */` |
|  11955613 |  3738 | `	return SXRET_OK;` |
|   5977809 |  3739 | `}` |
|         - |  3740 | `/*` |
|         - |  3741 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3742 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3743 | ` */` |
|         8 |  3744 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3745 | `{` |
|         - |  3746 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3747 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3748 | `		pGen->pIn++;` |
|         1 |  3749 | `	}` |
|         9 |  3750 | `	return SXRET_OK;` |
|         1 |  3751 | `}` |
|         - |  3752 | `/*` |
|         - |  3753 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3754 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3755 | ` */` |
|    290856 |  3756 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3757 | `{` |
|    290861 |  3758 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3873 |  3759 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3760 | `			return TRUE;` |
|      3871 |  3761 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3762 | `			return TRUE;` |
|         5 |  3763 | `		}` |
|    288924 |  3764 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7673 |  3765 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3766 | `			return TRUE;` |
|         - |  3767 | `		}` |
|      3833 |  3768 | `	}` |
|         - |  3769 | `	/* Not a reserved constant */` |
|    290853 |  3770 | `	return FALSE;` |
|    145433 |  3771 | `}` |
|         - |  3772 | `/*` |
|         - |  3773 | ` * Compile the 'const' statement.` |
|         - |  3774 | ` * According to the PHP language reference` |
|         - |  3775 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3776 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3777 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3778 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3779 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3780 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3781 | ` *  Syntax` |
|         - |  3782 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3783 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3784 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3785 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3786 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3787 | ` *  to get a list of all defined constants.` |
|         - |  3788 | ` *` |
|         - |  3789 | ` * Symisc eXtension.` |
|         - |  3790 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3791 | ` *  would allow only simple scalar value.` |
|         - |  3792 | ` *  Example` |
|         - |  3793 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3794 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3795 | ` */` |
|        50 |  3796 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3797 | `{` |
|         - |  3798 | `	SySet *pConsCode,*pInstrContainer;` |
|        55 |  3799 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3800 | `	SyString *pName;` |
|         - |  3801 | `	sxi32 rc;` |
|        55 |  3802 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        55 |  3803 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3804 | `		/* Invalid constant name */` |
|         9 |  3805 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|         9 |  3806 | `		if( rc == SXERR_ABORT ){` |
|         - |  3807 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3808 | `			return SXERR_ABORT;` |
|         - |  3809 | `		}` |
|         9 |  3810 | `		goto Synchronize;` |
|         - |  3811 | `	}` |
|         - |  3812 | `	/* Peek constant name */` |
|        49 |  3813 | `	pName = &pGen->pIn->sData;` |
|         - |  3814 | `	/* Make sure the constant name isn't reserved */` |
|        49 |  3815 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3816 | `		/* Reserved constant */` |
|        10 |  3817 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        10 |  3818 | `		if( rc == SXERR_ABORT ){` |
|         - |  3819 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3820 | `			return SXERR_ABORT;` |
|         - |  3821 | `		}` |
|        10 |  3822 | `		goto Synchronize;` |
|         - |  3823 | `	}` |
|        40 |  3824 | `	pGen->pIn++;` |
|        40 |  3825 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3826 | `		/* Invalid statement*/` |
|         6 |  3827 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|         6 |  3828 | `		if( rc == SXERR_ABORT ){` |
|         - |  3829 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3830 | `			return SXERR_ABORT;` |
|         - |  3831 | `		}` |
|         6 |  3832 | `		goto Synchronize;` |
|         - |  3833 | `	}` |
|        34 |  3834 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3835 | `	/* Allocate a new constant value container */` |
|        34 |  3836 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        34 |  3837 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3838 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3839 | `		return SXERR_ABORT;` |
|         - |  3840 | `	}` |
|        34 |  3841 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3842 | `	/* Swap bytecode container */` |
|        34 |  3843 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        34 |  3844 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3845 | `	/* Compile constant value */` |
|        34 |  3846 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3847 | `	/* Emit the done instruction */` |
|        34 |  3848 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        34 |  3849 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        34 |  3850 | `	if( rc == SXERR_ABORT ){` |
|         - |  3851 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3852 | `		return SXERR_ABORT;` |
|         - |  3853 | `	}` |
|        34 |  3854 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3855 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3856 | `	{` |
|         - |  3857 | `		SyBlob sFQN;` |
|         - |  3858 | `		SyString sFQNStr;` |
|        34 |  3859 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        34 |  3860 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        34 |  3861 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        50 |  3862 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        32 |  3863 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        34 |  3864 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3865 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3866 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3867 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3868 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3869 | `			if( pCEntry ){` |
|         5 |  3870 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3871 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3872 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3873 | `					return SXERR_ABORT;` |
|         - |  3874 | `				}` |
|         2 |  3875 | `			}` |
|         2 |  3876 | `		}` |
|        34 |  3877 | `		SyBlobRelease(&sFQN);` |
|         - |  3878 | `	}` |
|        34 |  3879 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3880 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3881 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3882 | `	}` |
|        34 |  3883 | `	return SXRET_OK;` |
|         9 |  3884 | `Synchronize:` |
|         - |  3885 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3886 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        42 |  3887 | `		pGen->pIn++;` |
|         4 |  3888 | `	}` |
|        22 |  3889 | `	return SXRET_OK;` |
|        30 |  3890 | `}` |
|         - |  3891 | `/*` |
|         - |  3892 | ` * Compile the 'continue' statement.` |
|         - |  3893 | ` * According to the PHP language reference` |
|         - |  3894 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3895 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3896 | ` *  iteration.` |
|         - |  3897 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3898 | ` *  the purposes of continue.` |
|         - |  3899 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3900 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3901 | ` *  Note:` |
|         - |  3902 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3903 | ` */` |
|         - |  3904 | `/*` |
|         - |  3905 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3906 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3907 | ` * break/continue crosses a try boundary.` |
|         - |  3908 | ` *` |
|         - |  3909 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3910 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3911 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3912 | ` */` |
|    149290 |  3913 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3914 | `{` |
|    149295 |  3915 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    149295 |  3916 | `	int nInlineTry = 0;` |
|    673429 |  3917 | `	while( pBlock && pBlock != pTarget ){` |
|    524139 |  3918 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3919 | `			if( pBlock->pUserData ){` |
|         - |  3920 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3921 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3922 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3923 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3924 | `				if( pGen->bInGenerator ){` |
|         3 |  3925 | `					nInlineTry++;` |
|         2 |  3926 | `				}else{` |
|         3 |  3927 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3928 | `				}` |
|         4 |  3929 | `			}else{` |
|         - |  3930 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3931 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3932 | `				break;` |
|         - |  3933 | `			}` |
|         2 |  3934 | `		}` |
|    524139 |  3935 | `		pBlock = pBlock->pParent;` |
|         5 |  3936 | `	}` |
|    149295 |  3937 | `	return nInlineTry;` |
|         5 |  3938 | `}` |
|     84178 |  3939 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3940 | `{` |
|         - |  3941 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3942 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3943 | `	sxu32 nLineLocal;` |
|         - |  3944 | `	sxi32 rc;` |
|     84183 |  3945 | `	nLineLocal = pGen->pIn->nLine;` |
|     84183 |  3946 | `	iLevel = 0;` |
|         - |  3947 | `	/* Jump the 'continue' keyword */` |
|     84183 |  3948 | `	pGen->pIn++;` |
|     84183 |  3949 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3950 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3951 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3952 | `		 */` |
|         - |  3953 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3954 | `		char *zAlloc = 0;` |
|         - |  3955 | `		SyString sNum;` |
|        17 |  3956 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3957 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3958 | `			return SXERR_ABORT;` |
|         - |  3959 | `		}` |
|        17 |  3960 | `		if( rc == SXRET_OK ){` |
|        20 |  3961 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3962 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3963 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3964 | `				return SXERR_ABORT;` |
|         - |  3965 | `			}` |
|        14 |  3966 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3967 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3968 | `		}` |
|        17 |  3969 | `		if( iLevel < 2 ){` |
|         3 |  3970 | `			iLevel = 0;` |
|         1 |  3971 | `		}` |
|        17 |  3972 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3973 | `	}` |
|         - |  3974 | `	/* Point to the target loop */` |
|     84183 |  3975 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     84183 |  3976 | `	if( pLoop == 0 ){` |
|         - |  3977 | `		/* Illegal continue */` |
|        13 |  3978 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        13 |  3979 | `		if( rc == SXERR_ABORT ){` |
|         - |  3980 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3981 | `			return SXERR_ABORT;` |
|         - |  3982 | `		}` |
|         8 |  3983 | `	}else{` |
|     84173 |  3984 | `		sxu32 nInstrIdx = 0;` |
|         - |  3985 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     84173 |  3986 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3987 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3988 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     84173 |  3989 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     84173 |  3990 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3991 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|         - |  3992 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|         - |  3993 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|         - |  3994 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|         5 |  3995 | `			if( iLevel < 1 ){` |
|         5 |  3996 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|         - |  3997 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|         - |  3998 | `					" Did you mean to use \"continue 2\"?");` |
|         2 |  3999 | `			}` |
|         5 |  4000 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  4001 | `			if( rc == SXRET_OK ){` |
|         5 |  4002 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  4003 | `			}` |
|         3 |  4004 | `		}else{` |
|         - |  4005 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     84169 |  4006 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     84169 |  4007 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  4008 | `				JumpFixup sJumpFix;` |
|         - |  4009 | `				/* Post-continue */` |
|     26785 |  4010 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     26785 |  4011 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     26785 |  4012 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     13390 |  4013 | `			}` |
|         - |  4014 | `		}` |
|         - |  4015 | `	}` |
|     84183 |  4016 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4017 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4018 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  4019 | `	}` |
|         - |  4020 | `	/* Statement successfully compiled */` |
|     84183 |  4021 | `	return SXRET_OK;` |
|     42094 |  4022 | `}` |
|         - |  4023 | `/*` |
|         - |  4024 | ` * Compile the 'break' statement.` |
|         - |  4025 | ` * According to the PHP language reference` |
|         - |  4026 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  4027 | ` *  structure.` |
|         - |  4028 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  4029 | ` *  enclosing structures are to be broken out of.` |
|         - |  4030 | ` */` |
|     65138 |  4031 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  4032 | `{` |
|         - |  4033 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  4034 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  4035 | `	sxi32 rc;` |
|     65143 |  4036 | `	iLevel = 0;` |
|         - |  4037 | `	/* Jump the 'break' keyword */` |
|     65143 |  4038 | `	pGen->pIn++;` |
|     65143 |  4039 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  4040 | `		/* optional numeric argument which tells us how many levels` |
|         - |  4041 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  4042 | `		 */` |
|         - |  4043 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        18 |  4044 | `		char *zAlloc = 0;` |
|         - |  4045 | `		SyString sNum;` |
|        18 |  4046 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        18 |  4047 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4048 | `			return SXERR_ABORT;` |
|         - |  4049 | `		}` |
|        18 |  4050 | `		if( rc == SXRET_OK ){` |
|        21 |  4051 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  4052 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  4053 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  4054 | `				return SXERR_ABORT;` |
|         - |  4055 | `			}` |
|        15 |  4056 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  4057 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  4058 | `		}` |
|        18 |  4059 | `		if( iLevel < 2 ){` |
|         3 |  4060 | `			iLevel = 0;` |
|         1 |  4061 | `		}` |
|        18 |  4062 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  4063 | `	}` |
|         - |  4064 | `	/* Extract the target loop */` |
|     65143 |  4065 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     65143 |  4066 | `	if( pLoop == 0 ){` |
|         - |  4067 | `		/* Illegal break */` |
|        19 |  4068 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        19 |  4069 | `		if( rc == SXERR_ABORT ){` |
|         - |  4070 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4071 | `			return SXERR_ABORT;` |
|         - |  4072 | `		}` |
|        11 |  4073 | `	}else{` |
|         - |  4074 | `		sxu32 nInstrIdx;` |
|         - |  4075 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     65127 |  4076 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  4077 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     65127 |  4078 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     65127 |  4079 | `		if( rc == SXRET_OK ){` |
|         - |  4080 | `			/* Fix the jump later when the jump destination is resolved */` |
|     65127 |  4081 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     32561 |  4082 | `		}` |
|         - |  4083 | `	}` |
|     65143 |  4084 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4085 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4086 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  4087 | `	}` |
|         - |  4088 | `	/* Statement successfully compiled */` |
|     65143 |  4089 | `	return SXRET_OK;` |
|     32574 |  4090 | `}` |
|         - |  4091 | `/*` |
|         - |  4092 | ` * Compile or record a label.` |
|         - |  4093 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  4094 | ` * Example` |
|         - |  4095 | ` *  goto LABEL;` |
|         - |  4096 | ` *   echo 'Foo';` |
|         - |  4097 | ` *  LABEL:` |
|         - |  4098 | ` *   echo 'Bar';` |
|         - |  4099 | ` */` |
|       112 |  4100 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  4101 | `{` |
|         - |  4102 | `	GenBlock *pBlock;` |
|         - |  4103 | `	Label sLabel;` |
|         - |  4104 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|         - |  4105 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|         - |  4106 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|         - |  4107 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|         - |  4108 | `	{` |
|       117 |  4109 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4110 | `		char *zDup;` |
|         - |  4111 | `		/* Initialize label fields */` |
|       117 |  4112 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4113 | `		/* Duplicate label name */` |
|       117 |  4114 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       117 |  4115 | `		if( zDup == 0 ){` |
|       ! 0 |  4116 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4117 | `			return SXERR_ABORT;` |
|         - |  4118 | `		}` |
|       117 |  4119 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       117 |  4120 | `		sLabel.bRef  = FALSE;` |
|       117 |  4121 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       117 |  4122 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       117 |  4123 | `		pBlock = pGen->pCurrent;` |
|       233 |  4124 | `		while( pBlock ){` |
|       143 |  4125 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        26 |  4126 | `				break;` |
|         - |  4127 | `			}` |
|         - |  4128 | `			/* Point to the upper block */` |
|       121 |  4129 | `			pBlock = pBlock->pParent;` |
|         5 |  4130 | `		}` |
|       117 |  4131 | `		if( pBlock ){` |
|        26 |  4132 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        15 |  4133 | `		}else{` |
|        95 |  4134 | `			sLabel.pFunc = 0;` |
|         - |  4135 | `		}` |
|         - |  4136 | `		/* Insert in label set */` |
|       117 |  4137 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  4138 | `	}` |
|       117 |  4139 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  4140 | `	return SXRET_OK;` |
|        61 |  4141 | `}` |
|         - |  4142 | `/*` |
|         - |  4143 | ` * Compile the so hated 'goto' statement.` |
|         - |  4144 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  4145 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  4146 | ` * a compiler it has to do this.` |
|         - |  4147 | ` * According to the PHP language reference manual` |
|         - |  4148 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  4149 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  4150 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  4151 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  4152 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  4153 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  4154 | ` *   of a multi-level break` |
|         - |  4155 | ` */` |
|       152 |  4156 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  4157 | `{` |
|         - |  4158 | `	JumpFixup sJump;` |
|         - |  4159 | `	sxi32 rc;` |
|       157 |  4160 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  4161 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  4162 | `		/* Missing label */` |
|       ! 0 |  4163 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  4164 | `		if( rc == SXERR_ABORT ){` |
|         - |  4165 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4166 | `			return SXERR_ABORT;` |
|         - |  4167 | `		}` |
|       ! 0 |  4168 | `		return SXRET_OK;` |
|         - |  4169 | `	}` |
|       157 |  4170 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         6 |  4171 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|         6 |  4172 | `		if( rc == SXERR_ABORT ){` |
|         - |  4173 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4174 | `			return SXERR_ABORT;` |
|         - |  4175 | `		}` |
|         4 |  4176 | `	}else{` |
|       153 |  4177 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4178 | `		GenBlock *pBlock;` |
|         - |  4179 | `		char *zDup;` |
|         - |  4180 | `		/* Prepare the jump destination */` |
|       153 |  4181 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  4182 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  4183 | `		/* Duplicate label name */` |
|       153 |  4184 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  4185 | `		if( zDup == 0 ){` |
|       ! 0 |  4186 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4187 | `			return SXERR_ABORT;` |
|         - |  4188 | `		}` |
|       153 |  4189 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|         - |  4190 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|       153 |  4191 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|         - |  4192 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|         - |  4193 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|       153 |  4194 | `		pBlock = pGen->pCurrent;` |
|       327 |  4195 | `		while( pBlock ){` |
|       205 |  4196 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|        30 |  4197 | `				break;` |
|         - |  4198 | `			}` |
|         - |  4199 | `			/* Point to the upper block */` |
|       179 |  4200 | `			pBlock = pBlock->pParent;` |
|         5 |  4201 | `		}` |
|       153 |  4202 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        30 |  4203 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        17 |  4204 | `		}else{` |
|       127 |  4205 | `			sJump.pFunc = 0;` |
|         - |  4206 | `		}` |
|         - |  4207 | `		/* Emit the unconditional jump */` |
|       153 |  4208 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  4209 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  4210 | `		}` |
|         - |  4211 | `	}` |
|       157 |  4212 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  4213 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  4214 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  4215 | `	}` |
|         - |  4216 | `	/* Statement successfully compiled */` |
|       157 |  4217 | `	return SXRET_OK;` |
|        81 |  4218 | `}` |
|         - |  4219 | `/*` |
|         - |  4220 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  4221 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  4222 | ` * failure.` |
|         - |  4223 | ` */` |
|        20 |  4224 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         1 |  4225 | `{` |
|         - |  4226 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  4227 | `	sxu32 nRawObj;` |
|        10 |  4228 | `	sxu32 nObjIdx;` |
|         - |  4229 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  4230 | `	 * a PHP block.` |
|         - |  4231 | `	 */` |
|        10 |  4232 | `Consume:` |
|        21 |  4233 | `	nRawObj = nObjIdx = 0;` |
|        21 |  4234 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  4235 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  4236 | `		if( pRawObj == 0 ){` |
|       ! 0 |  4237 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4238 | `			return SXERR_ABORT;` |
|         - |  4239 | `		}` |
|         - |  4240 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  4241 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  4242 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  4243 | `		++nRawObj;` |
|       ! 0 |  4244 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  4245 | `	}` |
|        21 |  4246 | `	if( nRawObj > 0 ){` |
|         - |  4247 | `		/* Emit the consume instruction */` |
|       ! 0 |  4248 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4249 | `	}` |
|        21 |  4250 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  4251 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  4252 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  4253 | `		SySetReset(pTokenSet);` |
|       ! 0 |  4254 | `		SySetReset(&pGen->aTrivia);` |
|         - |  4255 | `		/* Tokenize input */` |
|       ! 0 |  4256 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  4257 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  4258 | `		/* Point to the fresh token stream */` |
|       ! 0 |  4259 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  4260 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  4261 | `		/* Advance the stream cursor */` |
|       ! 0 |  4262 | `		pGen->pRawIn++;` |
|         - |  4263 | `		/* TICKET 1433-011 */` |
|       ! 0 |  4264 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4265 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4266 | `			sxi32 rc;` |
|         - |  4267 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4268 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4269 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4270 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4271 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4272 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4273 | `				return SXERR_ABORT;` |
|       ! 0 |  4274 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4275 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4276 | `			}` |
|       ! 0 |  4277 | `			goto Consume;` |
|         - |  4278 | `		}` |
|       ! 0 |  4279 | `	}else{` |
|         - |  4280 | `		/* No more chunks to process */` |
|        21 |  4281 | `		pGen->pIn = pGen->pEnd;` |
|        21 |  4282 | `		return SXERR_EOF;` |
|         - |  4283 | `	}` |
|       ! 0 |  4284 | `	return SXRET_OK;` |
|        11 |  4285 | `}` |
|         - |  4286 | `/*` |
|         - |  4287 | ` * Compile a PHP block.` |
|         - |  4288 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4289 | ` * optionally delimited by braces {}.` |
|         - |  4290 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4291 | ` * and this function takes care of generating the appropriate error` |
|         - |  4292 | ` * message.` |
|         - |  4293 | ` */` |
|   6037918 |  4294 | `static sxi32 PH7_CompileBlock(` |
|         - |  4295 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4296 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4297 | `	)` |
|         5 |  4298 | `{` |
|         - |  4299 | `	sxi32 rc;` |
|         - |  4300 | `	sxu32 nLine;` |
|   6037923 |  4301 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   6013889 |  4302 | `		nLine = pGen->pIn->nLine;` |
|   6013889 |  4303 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   6013889 |  4304 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4305 | `			return SXERR_ABORT;` |
|         - |  4306 | `		}` |
|   6013889 |  4307 | `		pGen->pIn++;` |
|         - |  4308 | `		/* Compile until we hit the closing braces '}' */` |
|   8894640 |  4309 | `		for(;;){` |
|  17789285 |  4310 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        21 |  4311 | `				rc = GenStateNextChunk(&(*pGen));` |
|        21 |  4312 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4313 | `			 	   return SXERR_ABORT;` |
|         - |  4314 | `				}` |
|        21 |  4315 | `				if( rc == SXERR_EOF ){` |
|         - |  4316 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4317 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        21 |  4318 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        21 |  4319 | `					break;` |
|         - |  4320 | `				}` |
|       ! 0 |  4321 | `			}` |
|  17789265 |  4322 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4323 | `				/* Closing braces found,break immediately*/` |
|   6013869 |  4324 | `				pGen->pIn++;` |
|   6013869 |  4325 | `				break;` |
|         - |  4326 | `			}` |
|         - |  4327 | `			/* Compile a single statement */` |
|  11775401 |  4328 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  11775401 |  4329 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4330 | `				return SXERR_ABORT;` |
|         - |  4331 | `			}` |
|         5 |  4332 | `		}` |
|   6013889 |  4333 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   3030981 |  4334 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4335 | `		pGen->pIn++;` |
|       ! 0 |  4336 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4337 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4338 | `			return SXERR_ABORT;` |
|         - |  4339 | `		}` |
|         - |  4340 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4341 | `		for(;;){` |
|       ! 0 |  4342 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4343 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4344 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4345 | `			 	   return SXERR_ABORT;` |
|         - |  4346 | `				}` |
|       ! 0 |  4347 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4348 | `					/* No more token to process */` |
|       ! 0 |  4349 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4350 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4351 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4352 | `					}` |
|       ! 0 |  4353 | `					break;` |
|         - |  4354 | `				}` |
|       ! 0 |  4355 | `			}` |
|       ! 0 |  4356 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4357 | `				sxi32 nKwrd;` |
|         - |  4358 | `				/* Keyword found */` |
|       ! 0 |  4359 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4360 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4361 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4362 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4363 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4364 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4365 | `						}` |
|       ! 0 |  4366 | `						break;` |
|         - |  4367 | `				}` |
|       ! 0 |  4368 | `			}` |
|         - |  4369 | `			/* Compile a single statement */` |
|       ! 0 |  4370 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4371 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4372 | `				return SXERR_ABORT;` |
|         - |  4373 | `			}` |
|       ! 0 |  4374 | `		}` |
|       ! 0 |  4375 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4376 | `	}else{` |
|         - |  4377 | `		/* Compile a single statement */` |
|     24039 |  4378 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     24039 |  4379 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4380 | `			return SXERR_ABORT;` |
|         - |  4381 | `		}` |
|         - |  4382 | `	}` |
|         - |  4383 | `	/* Jump trailing semi-colons ';' */` |
|   6037923 |  4384 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4385 | `		pGen->pIn++;` |
|       ! 0 |  4386 | `	}` |
|   6037923 |  4387 | `	return SXRET_OK;` |
|   3018964 |  4388 | `}` |
|         - |  4389 | `/*` |
|         - |  4390 | ` * Compile the gentle 'while' statement.` |
|         - |  4391 | ` * According to the PHP language reference` |
|         - |  4392 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4393 | ` *  The basic form of a while statement is:` |
|         - |  4394 | ` *  while (expr)` |
|         - |  4395 | ` *   statement` |
|         - |  4396 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4397 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4398 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4399 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4400 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4401 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4402 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4403 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4404 | ` *  while (expr):` |
|         - |  4405 | ` *    statement` |
|         - |  4406 | ` *   endwhile;` |
|         - |  4407 | ` */` |
|     65152 |  4408 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4409 | `{` |
|     65157 |  4410 | `	GenBlock *pWhileBlock = 0;` |
|     65157 |  4411 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4412 | `	sxu32 nFalseJump;` |
|         - |  4413 | `	sxu32 nLine;` |
|         - |  4414 | `	sxi32 rc;` |
|     65157 |  4415 | `	nLine = pGen->pIn->nLine;` |
|         - |  4416 | `	/* Jump the 'while' keyword */` |
|     65157 |  4417 | `	pGen->pIn++;` |
|     65157 |  4418 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4419 | `		/* Syntax error */` |
|       ! 0 |  4420 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4421 | `		if( rc == SXERR_ABORT ){` |
|         - |  4422 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4423 | `			return SXERR_ABORT;` |
|         - |  4424 | `		}` |
|       ! 0 |  4425 | `		goto Synchronize;` |
|         - |  4426 | `	}` |
|         - |  4427 | `	/* Jump the left parenthesis '(' */` |
|     65157 |  4428 | `	pGen->pIn++;` |
|         - |  4429 | `	/* Create the loop block */` |
|     65157 |  4430 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     65157 |  4431 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4432 | `		return SXERR_ABORT;` |
|         - |  4433 | `	}` |
|         - |  4434 | `	/* Delimit the condition */` |
|     65157 |  4435 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     65157 |  4436 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4437 | `		/* Empty expression */` |
|         3 |  4438 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4439 | `		if( rc == SXERR_ABORT ){` |
|         - |  4440 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4441 | `			return SXERR_ABORT;` |
|         - |  4442 | `		}` |
|         1 |  4443 | `	}` |
|         - |  4444 | `	/* Swap token streams */` |
|     65157 |  4445 | `	pTmp = pGen->pEnd;` |
|     65157 |  4446 | `	pGen->pEnd = pEnd;` |
|         - |  4447 | `	/* Compile the expression */` |
|     65157 |  4448 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     65157 |  4449 | `	if( rc == SXERR_ABORT ){` |
|         - |  4450 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4451 | `		return SXERR_ABORT;` |
|         - |  4452 | `	}` |
|         - |  4453 | `	/* Update token stream */` |
|     65157 |  4454 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4455 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4456 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4457 | `			return SXERR_ABORT;` |
|         - |  4458 | `		}` |
|       ! 0 |  4459 | `		pGen->pIn++;` |
|       ! 0 |  4460 | `	}` |
|         - |  4461 | `	/* Synchronize pointers */` |
|     65157 |  4462 | `	pGen->pIn  = &pEnd[1];` |
|     65157 |  4463 | `	pGen->pEnd = pTmp;` |
|         - |  4464 | `	/* Emit the false jump */` |
|     65157 |  4465 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4466 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     65157 |  4467 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4468 | `	/* Compile the loop body */` |
|     65157 |  4469 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     65157 |  4470 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4471 | `		return SXERR_ABORT;` |
|         - |  4472 | `	}` |
|         - |  4473 | `	/* Emit the unconditional jump to the start of the loop */` |
|     65157 |  4474 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4475 | `	/* Fix all jumps now the destination is resolved */` |
|     65157 |  4476 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4477 | `	/* Release the loop block */` |
|     65157 |  4478 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4479 | `	/* Statement successfully compiled */` |
|     65157 |  4480 | `	return SXRET_OK;` |
|       ! 0 |  4481 | `Synchronize:` |
|         - |  4482 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4483 | `	 * compiling this erroneous block.` |
|         - |  4484 | `	 */` |
|       ! 0 |  4485 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4486 | `		pGen->pIn++;` |
|       ! 0 |  4487 | `	}` |
|       ! 0 |  4488 | `	return SXRET_OK;` |
|     32581 |  4489 | `}` |
|         - |  4490 | `/*` |
|         - |  4491 | ` * Compile the ugly do..while() statement.` |
|         - |  4492 | ` * According to the PHP language reference` |
|         - |  4493 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4494 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4495 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4496 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4497 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4498 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4499 | ` *  would end immediately).` |
|         - |  4500 | ` *  There is just one syntax for do-while loops:` |
|         - |  4501 | ` *  <?php` |
|         - |  4502 | ` *  $i = 0;` |
|         - |  4503 | ` *  do {` |
|         - |  4504 | ` *   echo $i;` |
|         - |  4505 | ` *  } while ($i > 0);` |
|         - |  4506 | ` * ?>` |
|         - |  4507 | ` */` |
|         2 |  4508 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4509 | `{` |
|         3 |  4510 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4511 | `	GenBlock *pDoBlock = 0;` |
|         - |  4512 | `	sxu32 nLine;` |
|         - |  4513 | `	sxi32 rc;` |
|         3 |  4514 | `	nLine = pGen->pIn->nLine;` |
|         - |  4515 | `	/* Jump the 'do' keyword */` |
|         3 |  4516 | `	pGen->pIn++;` |
|         - |  4517 | `	/* Create the loop block */` |
|         3 |  4518 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4519 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4520 | `		return SXERR_ABORT;` |
|         - |  4521 | `	}` |
|         - |  4522 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4523 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4524 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4525 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4526 | `		return SXERR_ABORT;` |
|         - |  4527 | `	}` |
|         3 |  4528 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4529 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4530 | `	}` |
|         3 |  4531 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4532 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4533 | `			/* Missing 'while' statement */` |
|         3 |  4534 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4535 | `			if( rc == SXERR_ABORT ){` |
|         - |  4536 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4537 | `				return SXERR_ABORT;` |
|         - |  4538 | `			}` |
|         3 |  4539 | `			goto Synchronize;` |
|         - |  4540 | `	}` |
|         - |  4541 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4542 | `	pGen->pIn++;` |
|       ! 0 |  4543 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4544 | `		/* Syntax error */` |
|       ! 0 |  4545 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4546 | `		if( rc == SXERR_ABORT ){` |
|         - |  4547 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4548 | `			return SXERR_ABORT;` |
|         - |  4549 | `		}` |
|       ! 0 |  4550 | `		goto Synchronize;` |
|         - |  4551 | `	}` |
|         - |  4552 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4553 | `	pGen->pIn++;` |
|         - |  4554 | `	/* Delimit the condition */` |
|       ! 0 |  4555 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4556 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4557 | `		/* Empty expression */` |
|       ! 0 |  4558 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4559 | `		if( rc == SXERR_ABORT ){` |
|         - |  4560 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4561 | `			return SXERR_ABORT;` |
|         - |  4562 | `		}` |
|       ! 0 |  4563 | `		goto Synchronize;` |
|         - |  4564 | `	}` |
|         - |  4565 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4566 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4567 | `		JumpFixup *aPost;` |
|         - |  4568 | `		VmInstr *pInstr;` |
|         - |  4569 | `		sxu32 nJumpDest;` |
|         - |  4570 | `		sxu32 n;` |
|       ! 0 |  4571 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4572 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4573 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4574 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4575 | `			if( pInstr ){` |
|         - |  4576 | `				/* Fix */` |
|       ! 0 |  4577 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4578 | `			}` |
|       ! 0 |  4579 | `		}` |
|       ! 0 |  4580 | `	}` |
|         - |  4581 | `	/* Swap token streams */` |
|       ! 0 |  4582 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4583 | `	pGen->pEnd = pEnd;` |
|         - |  4584 | `	/* Compile the expression */` |
|       ! 0 |  4585 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4586 | `	if( rc == SXERR_ABORT ){` |
|         - |  4587 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4588 | `		return SXERR_ABORT;` |
|         - |  4589 | `	}` |
|         - |  4590 | `	/* Update token stream */` |
|       ! 0 |  4591 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4592 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4593 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4594 | `			return SXERR_ABORT;` |
|         - |  4595 | `		}` |
|       ! 0 |  4596 | `		pGen->pIn++;` |
|       ! 0 |  4597 | `	}` |
|       ! 0 |  4598 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4599 | `	pGen->pEnd = pTmp;` |
|         - |  4600 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4601 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4602 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4603 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4604 | `	/* Release the loop block */` |
|       ! 0 |  4605 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4606 | `	/* Statement successfully compiled */` |
|       ! 0 |  4607 | `	return SXRET_OK;` |
|         1 |  4608 | `Synchronize:` |
|         - |  4609 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4610 | `	 * compiling this erroneous block.` |
|         - |  4611 | `	 */` |
|         3 |  4612 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4613 | `		pGen->pIn++;` |
|       ! 0 |  4614 | `	}` |
|         3 |  4615 | `	return SXRET_OK;` |
|         2 |  4616 | `}` |
|         - |  4617 | `/*` |
|         - |  4618 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4619 | ` * According to the PHP language reference` |
|         - |  4620 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4621 | ` *  The syntax of a for loop is:` |
|         - |  4622 | ` *  for (expr1; expr2; expr3)` |
|         - |  4623 | ` *   statement` |
|         - |  4624 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4625 | ` *  the beginning of the loop.` |
|         - |  4626 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4627 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4628 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4629 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4630 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4631 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4632 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4633 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4634 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4635 | ` *  of using the for truth expression.` |
|         - |  4636 | ` */` |
|    122516 |  4637 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4638 | `{` |
|    122521 |  4639 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    122521 |  4640 | `	GenBlock *pForBlock = 0;` |
|         - |  4641 | `	sxu32 nFalseJump;` |
|         - |  4642 | `	sxu32 nLine;` |
|         - |  4643 | `	sxi32 rc;` |
|    122521 |  4644 | `	nLine = pGen->pIn->nLine;` |
|         - |  4645 | `	/* Jump the 'for' keyword */` |
|    122521 |  4646 | `	pGen->pIn++;` |
|    122521 |  4647 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4648 | `		/* Syntax error */` |
|       ! 0 |  4649 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4650 | `		if( rc == SXERR_ABORT ){` |
|         - |  4651 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4652 | `			return SXERR_ABORT;` |
|         - |  4653 | `		}` |
|       ! 0 |  4654 | `		return SXRET_OK;` |
|         - |  4655 | `	}` |
|         - |  4656 | `	/* Jump the left parenthesis '(' */` |
|    122521 |  4657 | `	pGen->pIn++;` |
|         - |  4658 | `	/* Delimit the init-expr;condition;post-expr */` |
|    122521 |  4659 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    122521 |  4660 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4661 | `		/* Empty expression */` |
|       ! 0 |  4662 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4663 | `		if( rc == SXERR_ABORT ){` |
|         - |  4664 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4665 | `			return SXERR_ABORT;` |
|         - |  4666 | `		}` |
|         - |  4667 | `		/* Synchronize */` |
|       ! 0 |  4668 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4669 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4670 | `			pGen->pIn++;` |
|       ! 0 |  4671 | `		}` |
|       ! 0 |  4672 | `		return SXRET_OK;` |
|         - |  4673 | `	}` |
|         - |  4674 | `	/* Swap token streams */` |
|    122521 |  4675 | `	pTmp = pGen->pEnd;` |
|    122521 |  4676 | `	pGen->pEnd = pEnd;` |
|         - |  4677 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4678 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4679 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4680 | `	 * compiled through this same window — recorded as a known leniency. */` |
|    122521 |  4681 | `	pGen->nCommaExprOk++;` |
|         - |  4682 | `	/* Compile initialization expressions if available */` |
|    122521 |  4683 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4684 | `	/* Pop operand lvalues */` |
|    122521 |  4685 | `	if( rc == SXERR_ABORT ){` |
|         - |  4686 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4687 | `		return SXERR_ABORT;` |
|    122521 |  4688 | `	}else if( rc != SXERR_EMPTY ){` |
|    111047 |  4689 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55521 |  4690 | `	}` |
|    122521 |  4691 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4692 | `		/* Syntax error */` |
|       ! 0 |  4693 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4694 | `		if( rc == SXERR_ABORT ){` |
|         - |  4695 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4696 | `			return SXERR_ABORT;` |
|         - |  4697 | `		}` |
|       ! 0 |  4698 | `		return SXRET_OK;` |
|         - |  4699 | `	}` |
|         - |  4700 | `	/* Jump the trailing ';' */` |
|    122521 |  4701 | `	pGen->pIn++;` |
|         - |  4702 | `	/* Create the loop block */` |
|    122521 |  4703 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    122521 |  4704 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4705 | `		return SXERR_ABORT;` |
|         - |  4706 | `	}` |
|         - |  4707 | `	/* Deffer continue jumps */` |
|    122521 |  4708 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4709 | `	/* Compile the condition */` |
|    122521 |  4710 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    122521 |  4711 | `	if( rc == SXERR_ABORT ){` |
|         - |  4712 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4713 | `		return SXERR_ABORT;` |
|    122521 |  4714 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4715 | `		/* Emit the false jump */` |
|    111047 |  4716 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4717 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    111047 |  4718 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     55521 |  4719 | `	}` |
|    122521 |  4720 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4721 | `		/* Syntax error */` |
|         6 |  4722 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4723 | `		if( rc == SXERR_ABORT ){` |
|         - |  4724 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4725 | `			return SXERR_ABORT;` |
|         - |  4726 | `		}` |
|         6 |  4727 | `		return SXRET_OK;` |
|         - |  4728 | `	}` |
|         - |  4729 | `	/* Jump the trailing ';' */` |
|    122517 |  4730 | `	pGen->pIn++;` |
|         - |  4731 | `	/* Save the post condition stream */` |
|    122517 |  4732 | `	pPostStart = pGen->pIn;` |
|         - |  4733 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4734 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|    122517 |  4735 | `	pGen->nCommaExprOk--;` |
|    122517 |  4736 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    122517 |  4737 | `	pGen->pEnd = pTmp;` |
|    122517 |  4738 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    122517 |  4739 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4740 | `		return SXERR_ABORT;` |
|         - |  4741 | `	}` |
|         - |  4742 | `	/* Fix post-continue jumps */` |
|    122517 |  4743 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4744 | `		JumpFixup *aPost;` |
|         - |  4745 | `		VmInstr *pInstr;` |
|         - |  4746 | `		sxu32 nJumpDest;` |
|         - |  4747 | `		sxu32 n;` |
|     11489 |  4748 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|     11489 |  4749 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     38269 |  4750 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     26785 |  4751 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     26785 |  4752 | `			if( pInstr ){` |
|         - |  4753 | `				/* Fix jump */` |
|     26785 |  4754 | `				pInstr->iP2 = nJumpDest;` |
|     13390 |  4755 | `			}` |
|     13395 |  4756 | `		}` |
|      5742 |  4757 | `	}` |
|         - |  4758 | `	/* compile the post-expressions if available */` |
|    122517 |  4759 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4760 | `		pPostStart++;` |
|       ! 0 |  4761 | `	}` |
|    122517 |  4762 | `	if( pPostStart < pEnd ){` |
|         - |  4763 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    111045 |  4764 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    111045 |  4765 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|    111045 |  4766 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    111045 |  4767 | `		pGen->nCommaExprOk--;` |
|    111045 |  4768 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4769 | `			/* Syntax error */` |
|       ! 0 |  4770 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4771 | `			if( rc == SXERR_ABORT ){` |
|         - |  4772 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4773 | `				return SXERR_ABORT;` |
|         - |  4774 | `			}` |
|       ! 0 |  4775 | `			return SXRET_OK;` |
|         - |  4776 | `		}` |
|    111045 |  4777 | `		RE_SWAP_DELIMITER(pGen);` |
|    111045 |  4778 | `		if( rc == SXERR_ABORT ){` |
|         - |  4779 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4780 | `			return SXERR_ABORT;` |
|    111045 |  4781 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4782 | `			/* Pop operand lvalue */` |
|    111045 |  4783 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55520 |  4784 | `		}` |
|     55520 |  4785 | `	}` |
|         - |  4786 | `	/* Emit the unconditional jump to the start of the loop */` |
|    122517 |  4787 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4788 | `	/* Fix all jumps now the destination is resolved */` |
|    122517 |  4789 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4790 | `	/* Release the loop block */` |
|    122517 |  4791 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4792 | `	/* Statement successfully compiled */` |
|    122517 |  4793 | `	return SXRET_OK;` |
|     61263 |  4794 | `}` |
|         - |  4795 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4796 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4797 | ` * are allowed.` |
|         - |  4798 | ` */` |
|    444634 |  4799 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4800 | `{` |
|    444639 |  4801 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    444639 |  4802 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4803 | `		/* Unexpected expression */` |
|       ! 0 |  4804 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4805 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4806 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4807 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4808 | `		}` |
|       ! 0 |  4809 | `	}` |
|    444639 |  4810 | `	return rc;` |
|         5 |  4811 | `}` |
|         - |  4812 | `/*` |
|         - |  4813 | ` * Compile the 'foreach' statement.` |
|         - |  4814 | ` * According to the PHP language reference` |
|         - |  4815 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4816 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4817 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4818 | ` *  is a minor but useful extension of the first:` |
|         - |  4819 | ` *  foreach (array_expression as $value)` |
|         - |  4820 | ` *    statement` |
|         - |  4821 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4822 | ` *   statement` |
|         - |  4823 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4824 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4825 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4826 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4827 | ` *  to the variable $key on each loop.` |
|         - |  4828 | ` *  Note:` |
|         - |  4829 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4830 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4831 | ` *  Note:` |
|         - |  4832 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4833 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4834 | ` *  or after the foreach without resetting it.` |
|         - |  4835 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4836 | ` *  of copying the value.` |
|         - |  4837 | ` */` |
|    310514 |  4838 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4839 | `{` |
|    310519 |  4840 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    310519 |  4841 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    310519 |  4842 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4843 | `	ph7_foreach_info *pInfo;` |
|         - |  4844 | `	sxu32 nFalseJump;` |
|         - |  4845 | `	VmInstr *pInstr;` |
|         - |  4846 | `	sxu32 nLine;` |
|         - |  4847 | `	sxi32 rc;` |
|    310519 |  4848 | `	nLine = pGen->pIn->nLine;` |
|         - |  4849 | `	/* Jump the 'foreach' keyword */` |
|    310519 |  4850 | `	pGen->pIn++;` |
|    310519 |  4851 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4852 | `		/* Syntax error */` |
|       ! 0 |  4853 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4854 | `		if( rc == SXERR_ABORT ){` |
|         - |  4855 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4856 | `			return SXERR_ABORT;` |
|         - |  4857 | `		}` |
|       ! 0 |  4858 | `		goto Synchronize;` |
|         - |  4859 | `	}` |
|         - |  4860 | `	/* Jump the left parenthesis '(' */` |
|    310519 |  4861 | `	pGen->pIn++;` |
|         - |  4862 | `	/* Create the loop block */` |
|    310519 |  4863 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    310519 |  4864 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4865 | `		return SXERR_ABORT;` |
|         - |  4866 | `	}` |
|         - |  4867 | `	/* Delimit the expression */` |
|    310519 |  4868 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    310519 |  4869 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4870 | `		/* Empty expression */` |
|       ! 0 |  4871 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4872 | `		if( rc == SXERR_ABORT ){` |
|         - |  4873 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4874 | `			return SXERR_ABORT;` |
|         - |  4875 | `		}` |
|         - |  4876 | `		/* Synchronize */` |
|       ! 0 |  4877 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4878 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4879 | `			pGen->pIn++;` |
|       ! 0 |  4880 | `		}` |
|       ! 0 |  4881 | `		return SXRET_OK;` |
|         - |  4882 | `	}` |
|         - |  4883 | `	/* Compile the array expression */` |
|    310519 |  4884 | `	pCur = pGen->pIn;` |
|   1786457 |  4885 | `	while( pCur < pEnd ){` |
|   1786457 |  4886 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    341125 |  4887 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    341125 |  4888 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4889 | `				/* Break with the first 'as' found */` |
|    310519 |  4890 | `				break;` |
|         - |  4891 | `			}` |
|     15303 |  4892 | `		}` |
|         - |  4893 | `		/* Advance the stream cursor */` |
|   1475943 |  4894 | `		pCur++;` |
|         5 |  4895 | `	}` |
|    310519 |  4896 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4897 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4898 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4899 | `		if( rc == SXERR_ABORT ){` |
|         - |  4900 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4901 | `			return SXERR_ABORT;` |
|         - |  4902 | `		}` |
|       ! 0 |  4903 | `		goto Synchronize;` |
|         - |  4904 | `	}` |
|         - |  4905 | `	/* Swap token streams */` |
|    310519 |  4906 | `	pTmp = pGen->pEnd;` |
|    310519 |  4907 | `	pGen->pEnd = pCur;` |
|    310519 |  4908 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    310519 |  4909 | `	if( rc == SXERR_ABORT ){` |
|         - |  4910 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4911 | `		return SXERR_ABORT;` |
|         - |  4912 | `	}` |
|         - |  4913 | `	/* Update token stream */` |
|    310519 |  4914 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4915 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4916 | `		if( rc == SXERR_ABORT ){` |
|         - |  4917 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4918 | `			return SXERR_ABORT;` |
|         - |  4919 | `		}` |
|       ! 0 |  4920 | `		pGen->pIn++;` |
|       ! 0 |  4921 | `	}` |
|    310519 |  4922 | `	pCur++; /* Jump the 'as' keyword */` |
|    310519 |  4923 | `	pGen->pIn = pCur;` |
|    310519 |  4924 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4925 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4926 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4927 | `			return SXERR_ABORT;` |
|         - |  4928 | `		}` |
|       ! 0 |  4929 | `	}` |
|         - |  4930 | `	/* Create the foreach context */` |
|    310519 |  4931 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    310519 |  4932 | `	if( pInfo == 0 ){` |
|       ! 0 |  4933 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4934 | `		return SXERR_ABORT;` |
|         - |  4935 | `	}` |
|         - |  4936 | `	/* Zero the structure */` |
|    310519 |  4937 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4938 | `	/* Initialize structure fields */` |
|    310519 |  4939 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4940 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4941 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4942 | `	 * '=>'. */` |
|    310519 |  4943 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    310519 |  4944 | `	if( pCur < pEnd ){` |
|         - |  4945 | `		/* Compile the expression holding the key name */` |
|    134147 |  4946 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4947 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4948 | `			if( rc == SXERR_ABORT ){` |
|         - |  4949 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4950 | `				return SXERR_ABORT;` |
|         - |  4951 | `			}` |
|       ! 0 |  4952 | `		}else{` |
|    134147 |  4953 | `			pGen->pEnd = pCur;` |
|    134147 |  4954 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    134147 |  4955 | `			if( rc == SXERR_ABORT ){` |
|         - |  4956 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4957 | `				return SXERR_ABORT;` |
|         - |  4958 | `			}` |
|    134147 |  4959 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    134147 |  4960 | `			if( pInstr->p3 ){` |
|         - |  4961 | `				/* Record key name */` |
|    134147 |  4962 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     67071 |  4963 | `			}` |
|    134147 |  4964 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4965 | `		}` |
|    134147 |  4966 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     67071 |  4967 | `	}` |
|    310519 |  4968 | `	pGen->pEnd = pEnd;` |
|    310519 |  4969 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4970 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4971 | `		if( rc == SXERR_ABORT ){` |
|         - |  4972 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4973 | `			return SXERR_ABORT;` |
|         - |  4974 | `		}` |
|       ! 0 |  4975 | `		goto Synchronize;` |
|         - |  4976 | `	}` |
|    310519 |  4977 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4978 | `		pGen->pIn++;` |
|         - |  4979 | `		/* Pass by reference  */` |
|        33 |  4980 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4981 | `	}` |
|         - |  4982 | `	/* Check if the value target is list() */` |
|    310519 |  4983 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4984 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4985 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4986 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4987 | `		 */` |
|         - |  4988 | `		static int iForeachListCnt = 0;` |
|         - |  4989 | `		char zTmp[128];` |
|         - |  4990 | `		sxu32 nLen;` |
|         - |  4991 | `		char *zDup;` |
|        10 |  4992 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4993 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4994 | `		if( zDup == 0 ){` |
|       ! 0 |  4995 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4996 | `			return SXERR_ABORT;` |
|         - |  4997 | `		}` |
|        10 |  4998 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4999 | `		/* Save list() token boundaries */` |
|        10 |  5000 | `		pListStart = pGen->pIn;` |
|         - |  5001 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  5002 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  5003 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  5004 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  5005 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  5006 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5007 | `				return SXERR_ABORT;` |
|         - |  5008 | `			}` |
|         3 |  5009 | `			goto Synchronize;` |
|         - |  5010 | `		}` |
|         7 |  5011 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  5012 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  5013 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  5014 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5015 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  5016 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5017 | `				return SXERR_ABORT;` |
|         - |  5018 | `			}` |
|       ! 0 |  5019 | `			goto Synchronize;` |
|         - |  5020 | `		}` |
|         7 |  5021 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  5022 | `		pListEnd = pGen->pIn;` |
|         7 |  5023 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    310514 |  5024 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  5025 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  5026 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  5027 | `		 */` |
|         - |  5028 | `		static int iForeachShortListCnt = 0;` |
|         - |  5029 | `		char zTmp[128];` |
|         - |  5030 | `		sxu32 nLen;` |
|         - |  5031 | `		char *zDup;` |
|        15 |  5032 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        15 |  5033 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        15 |  5034 | `		if( zDup == 0 ){` |
|       ! 0 |  5035 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5036 | `			return SXERR_ABORT;` |
|         - |  5037 | `		}` |
|        15 |  5038 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  5039 | `		/* Save [...] token boundaries */` |
|        15 |  5040 | `		pListStart = pGen->pIn;` |
|         - |  5041 | `		/* Advance past [...] */` |
|        15 |  5042 | `		pGen->pIn++; /* Jump '[' */` |
|        15 |  5043 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        15 |  5044 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  5045 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5046 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  5047 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5048 | `				return SXERR_ABORT;` |
|         - |  5049 | `			}` |
|       ! 0 |  5050 | `			goto Synchronize;` |
|         - |  5051 | `		}` |
|        15 |  5052 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        15 |  5053 | `		pListEnd = pGen->pIn;` |
|        15 |  5054 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         8 |  5055 | `	}else{` |
|         - |  5056 | `		/* Compile the expression holding the value name */` |
|    310497 |  5057 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    310497 |  5058 | `		if( rc == SXERR_ABORT ){` |
|         - |  5059 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5060 | `			return SXERR_ABORT;` |
|         - |  5061 | `		}` |
|    310497 |  5062 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    310497 |  5063 | `		if( pInstr->p3 ){` |
|         - |  5064 | `			/* Record value name */` |
|    310497 |  5065 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    155246 |  5066 | `		}` |
|         - |  5067 | `	}` |
|         - |  5068 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    310517 |  5069 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  5070 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    310517 |  5071 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  5072 | `	/* Record the first instruction to execute */` |
|    310517 |  5073 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5074 | `	/* Emit the FOREACH_STEP instruction */` |
|    310517 |  5075 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  5076 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    310517 |  5077 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  5078 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    310517 |  5079 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  5080 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  5081 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  5082 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  5083 | `		 */` |
|        21 |  5084 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  5085 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  5086 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  5087 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  5088 | `		 */` |
|        21 |  5089 | `		pSavedIn = pGen->pIn;` |
|        21 |  5090 | `		pSavedEnd = pGen->pEnd;` |
|        21 |  5091 | `		pGen->pIn = pListStart;` |
|        21 |  5092 | `		pGen->pEnd = pListEnd;` |
|        21 |  5093 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        15 |  5094 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         8 |  5095 | `		}else{` |
|         7 |  5096 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  5097 | `		}` |
|        21 |  5098 | `		pGen->pIn = pSavedIn;` |
|        21 |  5099 | `		pGen->pEnd = pSavedEnd;` |
|        21 |  5100 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5101 | `			return SXERR_ABORT;` |
|         - |  5102 | `		}` |
|         - |  5103 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        21 |  5104 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        10 |  5105 | `	}` |
|         - |  5106 | `	/* Compile the loop body */` |
|    310517 |  5107 | `	pGen->pIn = &pEnd[1];` |
|    310517 |  5108 | `	pGen->pEnd = pTmp;` |
|    310517 |  5109 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    310517 |  5110 | `	if( rc == SXERR_ABORT ){` |
|         - |  5111 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5112 | `		return SXERR_ABORT;` |
|         - |  5113 | `	}` |
|         - |  5114 | `	/* Emit the unconditional jump to the start of the loop */` |
|    310517 |  5115 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  5116 | `	/* Fix all jumps now the destination is resolved */` |
|    310517 |  5117 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  5118 | `	/* Release the loop block */` |
|    310517 |  5119 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5120 | `	/* Statement successfully compiled */` |
|    310517 |  5121 | `	return SXRET_OK;` |
|         1 |  5122 | `Synchronize:` |
|         - |  5123 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  5124 | `	 * compiling this erroneous block.` |
|         - |  5125 | `	 */` |
|         3 |  5126 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  5127 | `		pGen->pIn++;` |
|       ! 0 |  5128 | `	}` |
|         3 |  5129 | `	return SXRET_OK;` |
|    155262 |  5130 | `}` |
|         - |  5131 | `/*` |
|         - |  5132 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  5133 | ` * According to the PHP language reference` |
|         - |  5134 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  5135 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  5136 | ` *  that is similar to that of C:` |
|         - |  5137 | ` *  if (expr)` |
|         - |  5138 | ` *   statement` |
|         - |  5139 | ` *  else construct:` |
|         - |  5140 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  5141 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  5142 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  5143 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  5144 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  5145 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  5146 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  5147 | ` *  elseif` |
|         - |  5148 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  5149 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  5150 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  5151 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  5152 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  5153 | ` *   <?php` |
|         - |  5154 | ` *    if ($a > $b) {` |
|         - |  5155 | ` *     echo "a is bigger than b";` |
|         - |  5156 | ` *    } elseif ($a == $b) {` |
|         - |  5157 | ` *     echo "a is equal to b";` |
|         - |  5158 | ` *    } else {` |
|         - |  5159 | ` *     echo "a is smaller than b";` |
|         - |  5160 | ` *    }` |
|         - |  5161 | ` *    ?>` |
|         - |  5162 | ` */` |
|   2219914 |  5163 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  5164 | `{` |
|   2219919 |  5165 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   2219919 |  5166 | `	GenBlock *pCondBlock = 0;` |
|         - |  5167 | `	sxu32 nJumpIdx;` |
|         - |  5168 | `	sxu32 nKeyID;` |
|         - |  5169 | `	sxi32 rc;` |
|         - |  5170 | `	/* Jump the 'if' keyword */` |
|   2219919 |  5171 | `	pGen->pIn++;` |
|   2219919 |  5172 | `	pToken = pGen->pIn;` |
|         - |  5173 | `	/* Create the conditional block */` |
|   2219919 |  5174 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   2219919 |  5175 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5176 | `		return SXERR_ABORT;` |
|         - |  5177 | `	}` |
|         - |  5178 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1249594 |  5179 | `	for(;;){` |
|   2499193 |  5180 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  5181 | `			/* Syntax error */` |
|       ! 0 |  5182 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5183 | `				pToken--;` |
|       ! 0 |  5184 | `			}` |
|       ! 0 |  5185 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  5186 | `			if( rc == SXERR_ABORT ){` |
|         - |  5187 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5188 | `				return SXERR_ABORT;` |
|         - |  5189 | `			}` |
|       ! 0 |  5190 | `			goto Synchronize;` |
|         - |  5191 | `		}` |
|         - |  5192 | `		/* Jump the left parenthesis '(' */` |
|   2499193 |  5193 | `		pToken++;` |
|         - |  5194 | `		/* Delimit the condition */` |
|   2499193 |  5195 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2499193 |  5196 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  5197 | `			/* Syntax error */` |
|        11 |  5198 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5199 | `				pToken--;` |
|       ! 0 |  5200 | `			}` |
|        11 |  5201 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  5202 | `			if( rc == SXERR_ABORT ){` |
|         - |  5203 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5204 | `				return SXERR_ABORT;` |
|         - |  5205 | `			}` |
|        11 |  5206 | `			goto Synchronize;` |
|         - |  5207 | `		}` |
|         - |  5208 | `		/* Swap token streams */` |
|   2499185 |  5209 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  5210 | `		/* Compile the condition */` |
|   2499185 |  5211 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5212 | `		/* Update token stream */` |
|   2499185 |  5213 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5214 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5215 | `			pGen->pIn++;` |
|       ! 0 |  5216 | `		}` |
|   2499185 |  5217 | `		pGen->pIn  = &pEnd[1];` |
|   2499185 |  5218 | `		pGen->pEnd = pTmp;` |
|   2499185 |  5219 | `		if( rc == SXERR_ABORT ){` |
|         - |  5220 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5221 | `			return SXERR_ABORT;` |
|         - |  5222 | `		}` |
|         - |  5223 | `		/* Emit the false jump */` |
|   2499185 |  5224 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5225 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2499185 |  5226 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5227 | `		/* Compile the body */` |
|   2499185 |  5228 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2499185 |  5229 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5230 | `			return SXERR_ABORT;` |
|         - |  5231 | `		}` |
|   2499185 |  5232 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    493917 |  5233 | `			break;` |
|         - |  5234 | `		}` |
|         - |  5235 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1511361 |  5236 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1511361 |  5237 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   1021313 |  5238 | `			break;` |
|         - |  5239 | `		}` |
|         - |  5240 | `		/* Emit the unconditional jump */` |
|    490053 |  5241 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5242 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    490053 |  5243 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    490053 |  5244 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    294907 |  5245 | `			pToken = &pGen->pIn[1];` |
|    294907 |  5246 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     84166 |  5247 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    105392 |  5248 | `					break;` |
|         - |  5249 | `			}` |
|     84133 |  5250 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     42064 |  5251 | `		}` |
|    279279 |  5252 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5253 | `		/* Synchronize cursors */` |
|    279279 |  5254 | `		pToken = pGen->pIn;` |
|         - |  5255 | `		/* Fix the false jump */` |
|    279279 |  5256 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5257 | `	} /* For(;;) */` |
|         - |  5258 | `	/* Fix the false jump */` |
|   2219911 |  5259 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   2219911 |  5260 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1232082 |  5261 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5262 | `			/* Compile the else block */` |
|    210779 |  5263 | `			pGen->pIn++;` |
|    210779 |  5264 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    210779 |  5265 | `			if( rc == SXERR_ABORT ){` |
|         - |  5266 |  |
|       ! 0 |  5267 | `				return SXERR_ABORT;` |
|         - |  5268 | `			}` |
|    105387 |  5269 | `	}` |
|   2219911 |  5270 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5271 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   2219911 |  5272 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5273 | `	/* Release the conditional block */` |
|   2219911 |  5274 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5275 | `	/* Statement successfully compiled */` |
|   2219911 |  5276 | `	return SXRET_OK;` |
|         4 |  5277 | `Synchronize:` |
|         - |  5278 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5279 | `	 */` |
|        67 |  5280 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5281 | `		pGen->pIn++;` |
|         3 |  5282 | `	}` |
|        11 |  5283 | `	return SXRET_OK;` |
|   1109962 |  5284 | `}` |
|         - |  5285 | `/*` |
|         - |  5286 | ` * Compile the global construct.` |
|         - |  5287 | ` * According to the PHP language reference` |
|         - |  5288 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5289 | ` *  to be used in that function.` |
|         - |  5290 | ` *  Example #1 Using global` |
|         - |  5291 | ` *  <?php` |
|         - |  5292 | ` *   $a = 1;` |
|         - |  5293 | ` *   $b = 2;` |
|         - |  5294 | ` *   function Sum()` |
|         - |  5295 | ` *   {` |
|         - |  5296 | ` *    global $a, $b;` |
|         - |  5297 | ` *    $b = $a + $b;` |
|         - |  5298 | ` *   }` |
|         - |  5299 | ` *   Sum();` |
|         - |  5300 | ` *   echo $b;` |
|         - |  5301 | ` *  ?>` |
|         - |  5302 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5303 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5304 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5305 | ` */` |
|        38 |  5306 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5307 | `{` |
|        43 |  5308 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5309 | `	sxi32 nExpr;` |
|         - |  5310 | `	sxi32 rc;` |
|         - |  5311 | `	/* Jump the 'global' keyword */` |
|        43 |  5312 | `	pGen->pIn++;` |
|        43 |  5313 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5314 | `		/* Nothing to process */` |
|       ! 0 |  5315 | `		return SXRET_OK;` |
|         - |  5316 | `	}` |
|        43 |  5317 | `	pTmp = pGen->pEnd;` |
|        43 |  5318 | `	nExpr = 0;` |
|        91 |  5319 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5320 | `		if( pGen->pIn < pNext ){` |
|        53 |  5321 | `			pGen->pEnd = pNext;` |
|        53 |  5322 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5323 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5324 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5325 | `					return SXERR_ABORT;` |
|         - |  5326 | `				}` |
|       ! 0 |  5327 | `			}else{` |
|        53 |  5328 | `				pGen->pIn++;` |
|        53 |  5329 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5330 | `					/* Emit a warning */` |
|       ! 0 |  5331 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5332 | `				}else{` |
|        53 |  5333 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5334 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5335 | `						return SXERR_ABORT;` |
|        53 |  5336 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5337 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5338 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5339 | `							/* Variable name, not a constant */` |
|        53 |  5340 | `							pLast->iP1 = 0;` |
|        24 |  5341 | `						}` |
|        53 |  5342 | `						nExpr++;` |
|        24 |  5343 | `					}` |
|         - |  5344 | `				}` |
|         - |  5345 | `			}` |
|        24 |  5346 | `		}` |
|         - |  5347 | `		/* Next expression in the stream */` |
|        53 |  5348 | `		pGen->pIn = pNext;` |
|         - |  5349 | `		/* Jump trailing commas */` |
|        63 |  5350 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5351 | `			pGen->pIn++;` |
|         5 |  5352 | `		}` |
|         5 |  5353 | `	}` |
|         - |  5354 | `	/* Restore token stream */` |
|        43 |  5355 | `	pGen->pEnd = pTmp;` |
|        43 |  5356 | `	if( nExpr > 0 ){` |
|         - |  5357 | `		/* Emit the uplink instruction */` |
|        43 |  5358 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5359 | `	}` |
|        43 |  5360 | `	return SXRET_OK;` |
|        24 |  5361 | `}` |
|         - |  5362 | `/*` |
|         - |  5363 | ` * Compile the return statement.` |
|         - |  5364 | ` * According to the PHP language reference` |
|         - |  5365 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5366 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5367 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5368 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5369 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5370 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5371 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5372 | ` *  from within the main script file, then script execution end.` |
|         - |  5373 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5374 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5375 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5376 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5377 | ` */` |
|   3004220 |  5378 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5379 | `{` |
|   3004225 |  5380 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5381 | `	sxi32 rc;` |
|   3004225 |  5382 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   3004225 |  5383 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5384 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5385 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5386 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5387 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5388 | `	 * normally below so token processing stays consistent. */` |
|   7921255 |  5389 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4917035 |  5390 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5391 | `	}` |
|   3004220 |  5392 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   3004191 |  5393 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5394 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5395 | `			"A never-returning function must not return");` |
|         3 |  5396 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5397 | `			return SXERR_ABORT;` |
|         - |  5398 | `		}` |
|         1 |  5399 | `	}` |
|         - |  5400 | `	/* Jump the 'return' keyword */` |
|   3004225 |  5401 | `	pGen->pIn++;` |
|   3004225 |  5402 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5403 | `		/* Compile the expression */` |
|   2908595 |  5404 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2908595 |  5405 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5406 | `			return SXERR_ABORT;` |
|   2908595 |  5407 | `		}else if(rc != SXERR_EMPTY ){` |
|   2908595 |  5408 | `			nRet = 1;` |
|   1454295 |  5409 | `		}` |
|   1454295 |  5410 | `	}` |
|         - |  5411 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5412 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5413 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5414 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   3004225 |  5415 | `	if( pGen->bInGenerator ){` |
|      3857 |  5416 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3857 |  5417 | `		return SXRET_OK;` |
|         - |  5418 | `	}` |
|         - |  5419 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5420 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5421 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5422 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5423 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   3000373 |  5424 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   3000373 |  5425 | `	return SXRET_OK;` |
|   1502115 |  5426 | `}` |
|         - |  5427 | `/*` |
|         - |  5428 | ` * Compile a yield expression.` |
|         - |  5429 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5430 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5431 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5432 | ` */` |
|     15682 |  5433 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5434 | `{` |
|         - |  5435 | `	SyToken *pTmp, *pSplit;` |
|     15687 |  5436 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15687 |  5437 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5438 | `	sxi32 rc;` |
|      7841 |  5439 | `	(void)iCompileFlag;` |
|         - |  5440 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15687 |  5441 | `	pGen->pIn++;` |
|         - |  5442 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5443 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5444 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5445 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5446 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15682 |  5447 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7876 |  5448 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5449 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5450 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5451 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5452 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5453 | `			return SXERR_ABORT;` |
|         - |  5454 | `		}` |
|        67 |  5455 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5456 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5457 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5458 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5459 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5460 | `				return SXERR_ABORT;` |
|         - |  5461 | `			}` |
|       ! 0 |  5462 | `		}` |
|        67 |  5463 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5464 | `		return SXRET_OK;` |
|         - |  5465 | `	}` |
|     15625 |  5466 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5467 | `		/* Bare yield — no value */` |
|         3 |  5468 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5469 | `		return SXRET_OK;` |
|         - |  5470 | `	}` |
|         - |  5471 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15623 |  5472 | `	pSplit = 0;` |
|         - |  5473 | `	{` |
|     15623 |  5474 | `		SyToken *pCur = pGen->pIn;` |
|     15623 |  5475 | `		sxi32 nNest = 0;` |
|     46673 |  5476 | `		while( pCur < pGen->pEnd ){` |
|     46365 |  5477 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5478 | `				nNest++;` |
|     46357 |  5479 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5480 | `				nNest--;` |
|     46341 |  5481 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15315 |  5482 | `				pSplit = pCur;` |
|     15315 |  5483 | `				break;` |
|         - |  5484 | `			}` |
|     31055 |  5485 | `			pCur++;` |
|         5 |  5486 | `		}` |
|         - |  5487 | `	}` |
|     15623 |  5488 | `	pTmp = pGen->pEnd;` |
|     15623 |  5489 | `	if( pSplit ){` |
|         - |  5490 | `		/* yield $key => $value */` |
|     15315 |  5491 | `		pGen->pEnd = pSplit;` |
|     15315 |  5492 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15315 |  5493 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15315 |  5494 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15315 |  5495 | `		pGen->pEnd = pTmp;` |
|     15315 |  5496 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15315 |  5497 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15315 |  5498 | `		iP1 = 1;` |
|     15315 |  5499 | `		iP2 = 1;` |
|      7660 |  5500 | `	}else{` |
|         - |  5501 | `		/* yield $value */` |
|       313 |  5502 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       313 |  5503 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       313 |  5504 | `		if( rc != SXERR_EMPTY ){` |
|       313 |  5505 | `			iP1 = 1;` |
|       154 |  5506 | `		}` |
|         - |  5507 | `	}` |
|     15623 |  5508 | `	pGen->pEnd = pTmp;` |
|     15623 |  5509 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15623 |  5510 | `	return SXRET_OK;` |
|      7846 |  5511 | `}` |
|         - |  5512 | `/*` |
|         - |  5513 | ` * Compile the die/exit language construct.` |
|         - |  5514 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5515 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5516 | ` */` |
|       130 |  5517 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5518 | `{` |
|       135 |  5519 | `	sxi32 nExpr = 0;` |
|         - |  5520 | `	sxi32 rc;` |
|         - |  5521 | `	/* Jump the die/exit keyword */` |
|       135 |  5522 | `	pGen->pIn++;` |
|       135 |  5523 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5524 | `		/* Compile the expression */` |
|       135 |  5525 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       135 |  5526 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5527 | `			return SXERR_ABORT;` |
|       135 |  5528 | `		}else if(rc != SXERR_EMPTY ){` |
|       135 |  5529 | `			nExpr = 1;` |
|        65 |  5530 | `		}` |
|        65 |  5531 | `	}` |
|         - |  5532 | `	/* Emit the HALT instruction */` |
|       135 |  5533 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       135 |  5534 | `	return SXRET_OK;` |
|        70 |  5535 | `}` |
|         - |  5536 | `/*` |
|         - |  5537 | ` * Compile the 'echo' language construct.` |
|         - |  5538 | ` */` |
|     17964 |  5539 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5540 | `{` |
|     17969 |  5541 | `	SyToken *pTmp,*pNext = 0;` |
|     17969 |  5542 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17969 |  5543 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17969 |  5544 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5545 | `	sxi32 rc;` |
|         - |  5546 | `	/* Jump the 'echo' keyword */` |
|     17969 |  5547 | `	pGen->pIn++;` |
|         - |  5548 | `	/* Compile arguments one after one */` |
|     17969 |  5549 | `	pTmp = pGen->pEnd;` |
|     45475 |  5550 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     27513 |  5551 | `		if( pGen->pIn < pNext ){` |
|     27513 |  5552 | `			pGen->pEnd = pNext;` |
|     27513 |  5553 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     27513 |  5554 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5555 | `				return SXERR_ABORT;` |
|     27513 |  5556 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5557 | `				/* Emit the consume instruction */` |
|     27487 |  5558 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     27487 |  5559 | `				nExpr++;` |
|     27487 |  5560 | `				bExpectMore = 0;` |
|     13741 |  5561 | `			}` |
|     13754 |  5562 | `		}` |
|         - |  5563 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5564 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     37063 |  5565 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      9557 |  5566 | `			if( bExpectMore ){` |
|         - |  5567 | `				/* two commas in a row */` |
|         3 |  5568 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5569 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5570 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5571 | `			}` |
|      9555 |  5572 | `			bExpectMore = 1;` |
|      9555 |  5573 | `			pNext++;` |
|         5 |  5574 | `		}` |
|     27511 |  5575 | `		pGen->pIn = pNext;` |
|         5 |  5576 | `	}` |
|         - |  5577 | `	/* Restore token stream */` |
|     17967 |  5578 | `	pGen->pEnd = pTmp;` |
|     17967 |  5579 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5580 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5581 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5582 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5583 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5584 | `	}` |
|     17937 |  5585 | `	return SXRET_OK;` |
|      8987 |  5586 | `}` |
|         - |  5587 | `/*` |
|         - |  5588 | ` * Compile the static statement.` |
|         - |  5589 | ` * According to the PHP language reference` |
|         - |  5590 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5591 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5592 | ` *  when program execution leaves this scope.` |
|         - |  5593 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5594 | ` * Symisc eXtension.` |
|         - |  5595 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5596 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5597 | ` *  Example` |
|         - |  5598 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5599 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5600 | ` */` |
|     11484 |  5601 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         5 |  5602 | `{` |
|         - |  5603 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5604 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5605 | `	GenBlock *pBlock;` |
|         - |  5606 | `	SyString *pName;` |
|         - |  5607 | `	char *zDup;` |
|         - |  5608 | `	sxu32 nLine;` |
|         - |  5609 | `	sxi32 rc;` |
|         - |  5610 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5611 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5612 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|     11484 |  5613 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      5748 |  5614 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5615 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5616 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5617 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5618 | `			return SXERR_ABORT;` |
|         3 |  5619 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5620 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5621 | `		}` |
|         3 |  5622 | `		return SXRET_OK;` |
|         - |  5623 | `	}` |
|         - |  5624 | `	/* Jump the static keyword */` |
|     11487 |  5625 | `	nLine = pGen->pIn->nLine;` |
|     11487 |  5626 | `	pGen->pIn++;` |
|         - |  5627 | `	/* Extract the enclosing function if any */` |
|     11487 |  5628 | `	pBlock = pGen->pCurrent;` |
|     22969 |  5629 | `	while( pBlock ){` |
|     22969 |  5630 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|     11487 |  5631 | `			break;` |
|         - |  5632 | `		}` |
|         - |  5633 | `		/* Point to the upper block */` |
|     11487 |  5634 | `		pBlock = pBlock->pParent;` |
|         5 |  5635 | `	}` |
|     11487 |  5636 | `	if( pBlock == 0 ){` |
|         - |  5637 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5638 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5639 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5640 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5641 | `				return SXERR_ABORT;` |
|         - |  5642 | `			}` |
|       ! 0 |  5643 | `			goto Synchronize;` |
|         - |  5644 | `		}` |
|         - |  5645 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5646 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5647 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5648 | `			return SXERR_ABORT;` |
|       ! 0 |  5649 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5650 | `			/* Emit the POP instruction */` |
|       ! 0 |  5651 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5652 | `		}` |
|       ! 0 |  5653 | `		return SXRET_OK;` |
|         - |  5654 | `	}` |
|     11487 |  5655 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5656 | `	/* Make sure we are dealing with a valid statement */` |
|     11487 |  5657 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     11480 |  5658 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5659 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5660 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5661 | `				return SXERR_ABORT;` |
|         - |  5662 | `			}` |
|         3 |  5663 | `			goto Synchronize;` |
|         - |  5664 | `	}` |
|     11485 |  5665 | `	pGen->pIn++;` |
|         - |  5666 | `	/* Extract variable name */` |
|     11485 |  5667 | `	pName = &pGen->pIn->sData;` |
|     11485 |  5668 | `	pGen->pIn++; /* Jump the var name */` |
|     11485 |  5669 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5670 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5671 | `		goto Synchronize;` |
|         - |  5672 | `	}` |
|         - |  5673 | `	/* Initialize the structure describing the static variable */` |
|     11485 |  5674 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     11485 |  5675 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5676 | `	/* Duplicate variable name */` |
|     11485 |  5677 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     11485 |  5678 | `	if( zDup == 0 ){` |
|       ! 0 |  5679 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5680 | `		return SXERR_ABORT;` |
|         - |  5681 | `	}` |
|     11485 |  5682 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5683 | `	/* Check if we have an expression to compile */` |
|     11485 |  5684 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5685 | `		SySet *pInstrContainer;` |
|         - |  5686 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5687 | `		 * Static variable can take any complex expression including function` |
|         - |  5688 | `		 * call as their initialization value.` |
|         - |  5689 | `		 * Example:` |
|         - |  5690 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5691 | `		 */` |
|     11485 |  5692 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5693 | `		/* Swap bytecode container */` |
|     11485 |  5694 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     11485 |  5695 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5696 | `		/* Compile the expression */` |
|     11485 |  5697 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5698 | `		/* Emit the done instruction */` |
|     11485 |  5699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5700 | `		/* Restore default bytecode container */` |
|     11485 |  5701 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      5740 |  5702 | `	}` |
|         - |  5703 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     11485 |  5704 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     11485 |  5705 | `	return SXRET_OK;` |
|         1 |  5706 | `Synchronize:` |
|         - |  5707 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5708 | `	 * statement.` |
|         - |  5709 | `	 */` |
|         5 |  5710 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5711 | `		pGen->pIn++;` |
|         1 |  5712 | `	}` |
|         3 |  5713 | `	return SXRET_OK;` |
|      5747 |  5714 | `}` |
|         - |  5715 | `/*` |
|         - |  5716 | ` * Compile the var statement.` |
|         - |  5717 | ` * Symisc Extension:` |
|         - |  5718 | ` *      var statement can be used outside of a class definition.` |
|         - |  5719 | ` */` |
|         4 |  5720 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5721 | `{` |
|         - |  5722 | `	sxu32 nLine;` |
|         - |  5723 | `	sxi32 rc;` |
|         5 |  5724 | `	nLine = pGen->pIn->nLine;` |
|         - |  5725 | `	/* Jump the 'var' keyword */` |
|         5 |  5726 | `	pGen->pIn++;` |
|         5 |  5727 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5728 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5729 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5730 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5731 | `			pGen->pIn++;` |
|       ! 0 |  5732 | `		}` |
|       ! 0 |  5733 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5734 | `			return SXERR_ABORT;` |
|         - |  5735 | `		}` |
|       ! 0 |  5736 | `	}else{` |
|         - |  5737 | `		/* Compile the expression */` |
|         5 |  5738 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5739 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5740 | `			return SXERR_ABORT;` |
|         5 |  5741 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5742 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5743 | `		}` |
|         - |  5744 | `	}` |
|         5 |  5745 | `	return SXRET_OK;` |
|         3 |  5746 | `}` |
|         - |  5747 | `/*` |
|         - |  5748 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5749 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5750 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5751 | ` */` |
|         - |  5752 | `/*` |
|         - |  5753 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5754 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5755 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5756 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5757 | ` *` |
|         - |  5758 | ` * Resolution order:` |
|         - |  5759 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5760 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5761 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5762 | ` *` |
|         - |  5763 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5764 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5765 | ` * Returns the (possibly new) literal index.` |
|         - |  5766 | ` */` |
|   5624228 |  5767 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5768 | `{` |
|         - |  5769 | `	ph7_value *pLit;` |
|         - |  5770 | `	const char *zLit;` |
|         - |  5771 | `	SyString sQualified;` |
|         - |  5772 | `	sxu32 nLit;` |
|         - |  5773 | `	sxu32 k;` |
|         - |  5774 | `	sxu32 nNewIdx;` |
|         - |  5775 | `	int hasNsSep;` |
|         - |  5776 | `	SyHashEntry *pImport;` |
|         - |  5777 | `	ph7_value *pNew;` |
|   5624233 |  5778 | `	if( pFromImport ){` |
|   4549485 |  5779 | `		*pFromImport = 0;` |
|   2274740 |  5780 | `	}` |
|   5624233 |  5781 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   5624233 |  5782 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5783 | `		return nOrigIdx;` |
|         - |  5784 | `	}` |
|   5624233 |  5785 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   5624233 |  5786 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5787 | `	/* Skip if already qualified (contains backslash) */` |
|   5624233 |  5788 | `	hasNsSep = 0;` |
|  66538143 |  5789 | `	for( k = 0; k < nLit; k++ ){` |
|  60913933 |  5790 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  30456960 |  5791 | `	}` |
|   5624233 |  5792 | `	if( hasNsSep ){` |
|        20 |  5793 | `		return nOrigIdx;` |
|         - |  5794 | `	}` |
|         - |  5795 | `	/* Check use imports first (works even outside namespaces) */` |
|   5624215 |  5796 | `	SyBlobReset(&pGen->sWorker);` |
|   5624215 |  5797 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   5624215 |  5798 | `	if( pImport ){` |
|        41 |  5799 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5800 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5801 | `		if( pFromImport ){` |
|        18 |  5802 | `			*pFromImport = 1;` |
|         8 |  5803 | `		}` |
|        23 |  5804 | `	}else{` |
|   5624179 |  5805 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   5624049 |  5806 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5807 | `		}` |
|         - |  5808 | `		/* Prepend current namespace */` |
|       135 |  5809 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       135 |  5810 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       135 |  5811 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5812 | `	}` |
|         - |  5813 | `	/* Look up or create a new literal for the qualified name */` |
|       171 |  5814 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       171 |  5815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        77 |  5816 | `		return nNewIdx; /* Already interned */` |
|         - |  5817 | `	}` |
|        99 |  5818 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        99 |  5819 | `	if( pNew == 0 ){` |
|       ! 0 |  5820 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5821 | `	}` |
|        99 |  5822 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        99 |  5823 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        99 |  5824 | `	return nNewIdx;` |
|   2812119 |  5825 | `}` |
|         - |  5826 | `/*` |
|         - |  5827 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5828 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5829 | ` */` |
|    449244 |  5830 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5831 | `{` |
|         - |  5832 | `	SyHashEntry *pImport;` |
|         - |  5833 | `	/* Check use imports first */` |
|    449249 |  5834 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    449249 |  5835 | `	if( pImport ){` |
|        21 |  5836 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        21 |  5837 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        21 |  5838 | `		return;` |
|         - |  5839 | `	}` |
|         - |  5840 | `	/* Prepend current namespace if active */` |
|    449231 |  5841 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        14 |  5842 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        14 |  5843 | `		SyBlobAppend(pOut,"\\",1);` |
|         6 |  5844 | `	}` |
|    449231 |  5845 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    224627 |  5846 | `}` |
|         - |  5847 | `/*` |
|         - |  5848 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5849 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5850 | ` * The caller must release pOut when done.` |
|         - |  5851 | ` */` |
|    430382 |  5852 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5853 | `{` |
|    430387 |  5854 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3909 |  5855 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3909 |  5856 | `		SyBlobAppend(pOut,"\\",1);` |
|      1952 |  5857 | `	}` |
|    430387 |  5858 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    430387 |  5859 | `}` |
|         - |  5860 | `/*` |
|         - |  5861 | ` * Compile a namespace statement` |
|         - |  5862 | ` * According to the PHP language reference manual` |
|         - |  5863 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5864 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5865 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5866 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5867 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5868 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5869 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5870 | ` *  programming world.` |
|         - |  5871 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5872 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5873 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5874 | ` *  classes/functions/constants.` |
|         - |  5875 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5876 | ` *  readability of source code.` |
|         - |  5877 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5878 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5879 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5880 | ` *       class MyClass {}` |
|         - |  5881 | ` *       function myfunction() {}` |
|         - |  5882 | ` *       const MYCONST = 1;` |
|         - |  5883 | ` *       $a = new MyClass;` |
|         - |  5884 | ` *       $c = new \my\name\MyClass;` |
|         - |  5885 | ` *       $a = strlen('hi');` |
|         - |  5886 | ` *       $d = namespace\MYCONST;` |
|         - |  5887 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5888 | ` *       echo constant($d);` |
|         - |  5889 | ` * NOTE` |
|         - |  5890 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5891 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5892 | ` */` |
|         - |  5893 | `/*` |
|         - |  5894 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5895 | ` */` |
|        14 |  5896 | `static const char * TokenTypeName(sxu32 nType)` |
|         4 |  5897 | `{` |
|        18 |  5898 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        11 |  5899 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        11 |  5900 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        11 |  5901 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        11 |  5902 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        11 |  5903 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5904 | `	return "token";` |
|        11 |  5905 | `}` |
|      3952 |  5906 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5907 | `{` |
|         - |  5908 | `	sxu32 nLine;` |
|         - |  5909 | `	sxi32 rc;` |
|      3957 |  5910 | `	nLine = pGen->pIn->nLine;` |
|      3957 |  5911 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5912 | `	/* Reset namespace and clear previous use imports */` |
|      3957 |  5913 | `	SyBlobReset(&pGen->sNamespace);` |
|      3957 |  5914 | `	SyHashRelease(&pGen->hUseImports);` |
|      3957 |  5915 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3957 |  5916 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3957 |  5917 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3957 |  5918 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3957 |  5919 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3957 |  5920 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5921 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5922 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5923 | `		return SXRET_OK;` |
|         - |  5924 | `	}` |
|      3957 |  5925 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5926 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5927 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5928 | `		return SXRET_OK;` |
|         - |  5929 | `	}` |
|      3957 |  5930 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5931 | `		/* namespace { } — global namespace block */` |
|         5 |  5932 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         5 |  5933 | `		return SXRET_OK;` |
|         - |  5934 | `	}` |
|         - |  5935 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7975 |  5936 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      4027 |  5937 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5938 | `			/* Append backslash separator */` |
|        42 |  5939 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        42 |  5940 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        19 |  5941 | `			}` |
|        23 |  5942 | `		}else{` |
|         - |  5943 | `			/* Append identifier */` |
|      3989 |  5944 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5945 | `		}` |
|      4027 |  5946 | `		pGen->pIn++;` |
|         5 |  5947 | `	}` |
|         - |  5948 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5949 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5950 | `	{` |
|      3953 |  5951 | `		char *zNsDup = 0;` |
|      3953 |  5952 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5924 |  5953 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3946 |  5954 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1973 |  5955 | `		}` |
|      3953 |  5956 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5957 | `	}` |
|      3953 |  5958 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5959 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5960 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5961 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5962 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5963 | `			return SXERR_ABORT;` |
|         - |  5964 | `		}` |
|         2 |  5965 | `	}` |
|      3953 |  5966 | `	return SXRET_OK;` |
|      1981 |  5967 | `}` |
|         - |  5968 | `/*` |
|         - |  5969 | ` * Compile the 'use' statement` |
|         - |  5970 | ` * According to the PHP language reference manual` |
|         - |  5971 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5972 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5973 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5974 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5975 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5976 | ` *  a function or constant is not supported.` |
|         - |  5977 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5978 | ` * NOTE` |
|         - |  5979 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5980 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5981 | ` */` |
|        78 |  5982 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5983 | `{` |
|         - |  5984 | `	sxu32 nLine;` |
|         - |  5985 | `	sxi32 rc;` |
|         - |  5986 | `	SyBlob sPath;` |
|         - |  5987 | `	SyString sAlias;` |
|         - |  5988 | `	SyToken *pLast;` |
|         - |  5989 | `	char *zDup;` |
|         - |  5990 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5991 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5992 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        83 |  5993 | `	nLine = pGen->pIn->nLine;` |
|        83 |  5994 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5995 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        83 |  5996 | `	iUseType = 0;` |
|        83 |  5997 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5998 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5999 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  6000 | `			iUseType = 1;` |
|        16 |  6001 | `			pGen->pIn++;` |
|        23 |  6002 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  6003 | `			iUseType = 2;` |
|        16 |  6004 | `			pGen->pIn++;` |
|         7 |  6005 | `		}` |
|        14 |  6006 | `	}` |
|         - |  6007 | `	/* Select target hash tables based on import type */` |
|        83 |  6008 | `	switch( iUseType ){` |
|         7 |  6009 | `		case 1:` |
|        16 |  6010 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  6011 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  6012 | `			break;` |
|         7 |  6013 | `		case 2:` |
|        16 |  6014 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  6015 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  6016 | `			break;` |
|        25 |  6017 | `		default:` |
|        55 |  6018 | `			pGenHash = &pGen->hUseImports;` |
|        55 |  6019 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        50 |  6020 | `			break;` |
|         - |  6021 | `	}` |
|        83 |  6022 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  6023 | `	/* Process one or more use declarations separated by commas */` |
|        40 |  6024 | `	for(;;){` |
|        85 |  6025 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  6026 | `			break;` |
|         - |  6027 | `		}` |
|        85 |  6028 | `		SyBlobReset(&sPath);` |
|        85 |  6029 | `		pLast = 0;` |
|         - |  6030 | `		/* Collect the full namespace path */` |
|       293 |  6031 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       213 |  6032 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       147 |  6033 | `				pLast = pGen->pIn;` |
|       147 |  6034 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        71 |  6035 | `					SyBlobAppend(&sPath,"\\",1);` |
|        33 |  6036 | `				}` |
|       147 |  6037 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        71 |  6038 | `			}` |
|       213 |  6039 | `			pGen->pIn++;` |
|         5 |  6040 | `		}` |
|        85 |  6041 | `		if( pLast == 0 ){` |
|         - |  6042 | `			/* Empty path */` |
|         6 |  6043 | `			break;` |
|         - |  6044 | `		}` |
|         - |  6045 | `		/* Default alias is the last component of the path */` |
|        81 |  6046 | `		sAlias = pLast->sData;` |
|         - |  6047 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        76 |  6048 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        55 |  6049 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        27 |  6050 | `			pGen->pIn++; /* Jump 'as' */` |
|        27 |  6051 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        27 |  6052 | `				sAlias = pGen->pIn->sData;` |
|        27 |  6053 | `				pGen->pIn++;` |
|        12 |  6054 | `			}` |
|        12 |  6055 | `		}` |
|         - |  6056 | `		/* Check for duplicate import alias (per-type) */` |
|        81 |  6057 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  6058 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6059 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  6060 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  6061 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6062 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  6063 | `				return SXERR_ABORT;` |
|         - |  6064 | `			}` |
|         2 |  6065 | `		}` |
|         - |  6066 | `		/* Register the import: alias -> FQN.` |
|         - |  6067 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  6068 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  6069 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       119 |  6070 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        76 |  6071 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        81 |  6072 | `		if( zDup ){` |
|        81 |  6073 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        81 |  6074 | `			if( pVmHash ){` |
|         - |  6075 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  6076 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        53 |  6077 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        53 |  6078 | `				if( zAliasDup ){` |
|        53 |  6079 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        24 |  6080 | `				}` |
|        24 |  6081 | `			}` |
|        81 |  6082 | `			if( iUseType == 2 ){` |
|         - |  6083 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  6084 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  6085 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  6086 | `				if( zAliasDup ){` |
|         - |  6087 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  6088 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  6089 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  6090 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  6091 | `					if( azPair ){` |
|        16 |  6092 | `						azPair[0] = zAliasDup;` |
|        16 |  6093 | `						azPair[1] = zDup;` |
|        16 |  6094 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  6095 | `					}` |
|         7 |  6096 | `				}` |
|         7 |  6097 | `			}` |
|        38 |  6098 | `		}` |
|         - |  6099 | `		/* Check for comma (multiple use declarations) */` |
|        81 |  6100 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  6101 | `			pGen->pIn++;` |
|         2 |  6102 | `		}else{` |
|        42 |  6103 | `			break;` |
|         - |  6104 | `		}` |
|         1 |  6105 | `	}` |
|        83 |  6106 | `	SyBlobRelease(&sPath);` |
|        83 |  6107 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  6108 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  6109 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  6110 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6111 | `			return SXERR_ABORT;` |
|         - |  6112 | `		}` |
|         1 |  6113 | `	}` |
|        83 |  6114 | `	return SXRET_OK;` |
|        44 |  6115 | `}` |
|         - |  6116 | `/*` |
|         - |  6117 | ` * Compile the stupid 'declare' language construct.` |
|         - |  6118 | ` *` |
|         - |  6119 | ` * According to the PHP language reference manual.` |
|         - |  6120 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  6121 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  6122 | ` *  declare (directive)` |
|         - |  6123 | ` *   statement` |
|         - |  6124 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  6125 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  6126 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  6127 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  6128 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  6129 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  6130 | ` * <?php` |
|         - |  6131 | ` * // these are the same:` |
|         - |  6132 | ` * // you can use this:` |
|         - |  6133 | ` * declare(ticks=1) {` |
|         - |  6134 | ` *   // entire script here` |
|         - |  6135 | ` * }` |
|         - |  6136 | ` * // or you can use this:` |
|         - |  6137 | ` * declare(ticks=1);` |
|         - |  6138 | ` * // entire script here` |
|         - |  6139 | ` * ?>` |
|         - |  6140 | ` *` |
|         - |  6141 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  6142 | ` */` |
|         - |  6143 | `/*` |
|         - |  6144 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  6145 | ` */` |
|        72 |  6146 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  6147 | `{` |
|       109 |  6148 | `	return SyStringLength(pName) == nWant` |
|        72 |  6149 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  6150 | `}` |
|         - |  6151 |  |
|        42 |  6152 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  6153 | `{` |
|        47 |  6154 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  6155 | `	SyToken *pBodyEnd = 0;` |
|         - |  6156 | `	SyToken *pBodyStart;` |
|         - |  6157 | `	SyToken *pCursor;` |
|         - |  6158 | `	int bHasStrictTypes;` |
|         - |  6159 | `	int bBlockForm;` |
|         - |  6160 | `	int bPlacementOk;` |
|         - |  6161 | `	sxi32 rc;` |
|        47 |  6162 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  6163 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  6164 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         6 |  6165 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6166 | `			return SXERR_ABORT;` |
|         - |  6167 | `		}` |
|         6 |  6168 | `		goto Synchro;` |
|         - |  6169 | `	}` |
|        43 |  6170 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  6171 | `	pBodyStart = pGen->pIn;` |
|         - |  6172 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  6173 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  6174 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  6175 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  6176 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6177 | `			return SXERR_ABORT;` |
|         - |  6178 | `		}` |
|       ! 0 |  6179 | `		return SXRET_OK;` |
|         - |  6180 | `	}` |
|         - |  6181 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  6182 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  6183 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  6184 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  6185 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  6186 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6187 | `			return SXERR_ABORT;` |
|         - |  6188 | `		}` |
|       ! 0 |  6189 | `	}` |
|        43 |  6190 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  6191 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  6192 | `	bHasStrictTypes = 0;` |
|         - |  6193 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  6194 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  6195 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  6196 | `	pCursor = pBodyStart;` |
|        55 |  6197 | `	while( pCursor < pBodyEnd ){` |
|        51 |  6198 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  6199 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  6200 | `				bHasStrictTypes = 1;` |
|        39 |  6201 | `				break;` |
|         - |  6202 | `			}` |
|         2 |  6203 | `		}` |
|        14 |  6204 | `		pCursor++;` |
|         2 |  6205 | `	}` |
|        43 |  6206 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  6207 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6208 | `			"strict_types declaration must not use block mode");` |
|         3 |  6209 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6210 | `		return SXRET_OK;` |
|         - |  6211 | `	}` |
|        41 |  6212 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  6213 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6214 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  6215 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  6216 | `		return SXRET_OK;` |
|         - |  6217 | `	}` |
|         - |  6218 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  6219 | `	pCursor = pBodyStart;` |
|        69 |  6220 | `	while( pCursor < pBodyEnd ){` |
|         - |  6221 | `		SyToken *pNameTok;` |
|         - |  6222 | `		SyToken *pEqTok;` |
|         - |  6223 | `		SyToken *pValTok;` |
|         - |  6224 | `		SyString *pDirName;` |
|         - |  6225 | `		int bIsStrict;` |
|         - |  6226 | `		int iStrictValue;` |
|        39 |  6227 | `		pNameTok = pCursor;` |
|        39 |  6228 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6229 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6230 | `				"declare: Expecting a directive name");` |
|       ! 0 |  6231 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6232 | `			return SXRET_OK;` |
|         - |  6233 | `		}` |
|        39 |  6234 | `		pEqTok = pNameTok + 1;` |
|        39 |  6235 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6236 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6237 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6238 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6239 | `			return SXRET_OK;` |
|         - |  6240 | `		}` |
|        39 |  6241 | `		pValTok = pEqTok + 1;` |
|        39 |  6242 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6243 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6244 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6245 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6246 | `			return SXRET_OK;` |
|         - |  6247 | `		}` |
|        39 |  6248 | `		pDirName = &pNameTok->sData;` |
|        39 |  6249 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6250 | `		if( bIsStrict ){` |
|         - |  6251 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6252 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6253 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6254 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6255 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6256 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6257 | `				return SXRET_OK;` |
|         - |  6258 | `			}` |
|        35 |  6259 | `			iStrictValue = -1;` |
|        35 |  6260 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6261 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6262 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6263 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6264 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6265 | `			}` |
|        35 |  6266 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6267 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6268 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6269 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6270 | `				return SXRET_OK;` |
|         - |  6271 | `			}` |
|        32 |  6272 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6273 | `		}else{` |
|         - |  6274 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6275 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6276 | `			 * behavior don't regress. */` |
|         8 |  6277 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6278 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6279 | `				ph7_lib_version()` |
|         - |  6280 | `				);` |
|         - |  6281 | `		}` |
|        37 |  6282 | `		pCursor = pValTok + 1;` |
|         - |  6283 | `		/* Consume separating comma (or end). */` |
|        37 |  6284 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6285 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6286 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6287 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6288 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6289 | `				return SXRET_OK;` |
|         - |  6290 | `			}` |
|         3 |  6291 | `			pCursor++;` |
|         1 |  6292 | `		}` |
|         5 |  6293 | `	}` |
|         - |  6294 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6295 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6296 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        35 |  6297 | `	return SXRET_OK;` |
|         2 |  6298 | `Synchro:` |
|         - |  6299 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6300 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6301 | `		pGen->pIn++;` |
|         2 |  6302 | `	}` |
|         6 |  6303 | `	return SXRET_OK;` |
|        26 |  6304 | `}` |
|         - |  6305 | `/*` |
|         - |  6306 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6307 | ` * as follows:` |
|         - |  6308 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6309 | ` * {` |
|         - |  6310 | ` *   return "Making a cup of $type.\n";` |
|         - |  6311 | ` * }` |
|         - |  6312 | ` * Symisc eXtension.` |
|         - |  6313 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6314 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6315 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6316 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6317 | ` *      {` |
|         - |  6318 | ` *       var_dump($a);` |
|         - |  6319 | ` *      }` |
|         - |  6320 | ` *     //call test without args` |
|         - |  6321 | ` *      test();` |
|         - |  6322 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6323 | ` *      Example:` |
|         - |  6324 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6325 | ` * 3 -) Function overloading!!` |
|         - |  6326 | ` *      Example:` |
|         - |  6327 | ` *      function foo($a) {` |
|         - |  6328 | ` *   	  return $a.PHP_EOL;` |
|         - |  6329 | ` *	    }` |
|         - |  6330 | ` *	    function foo($a, $b) {` |
|         - |  6331 | ` *   	  return $a + $b;` |
|         - |  6332 | ` *	    }` |
|         - |  6333 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6334 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6335 | ` *      // Same arg` |
|         - |  6336 | ` *	   function foo(string $a)` |
|         - |  6337 | ` *	   {` |
|         - |  6338 | ` *	     echo "a is a string\n";` |
|         - |  6339 | ` *	     var_dump($a);` |
|         - |  6340 | ` *	   }` |
|         - |  6341 | ` *	  function foo(int $a)` |
|         - |  6342 | ` *	  {` |
|         - |  6343 | ` *	    echo "a is integer\n";` |
|         - |  6344 | ` *	    var_dump($a);` |
|         - |  6345 | ` *	  }` |
|         - |  6346 | ` *	  function foo(array $a)` |
|         - |  6347 | ` *	  {` |
|         - |  6348 | ` * 	    echo "a is an array\n";` |
|         - |  6349 | ` * 	    var_dump($a);` |
|         - |  6350 | ` *	  }` |
|         - |  6351 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6352 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6353 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6354 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6355 | ` * introduced by the PH7 engine.` |
|         - |  6356 | ` */` |
|    566114 |  6357 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6358 | `{` |
|         - |  6359 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6360 | `	SySet *pInstrContainer;` |
|         - |  6361 | `	sxi32 rc;` |
|         - |  6362 | `	/* Swap token stream */` |
|    566119 |  6363 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    566119 |  6364 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    566119 |  6365 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6366 | `	/* Compile the expression holding the argument value */` |
|    566119 |  6367 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6368 | `	/* Emit the done instruction */` |
|    566119 |  6369 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    566119 |  6370 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    566119 |  6371 | `	RE_SWAP_DELIMITER(pGen);` |
|    566119 |  6372 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6373 | `		return SXERR_ABORT;` |
|         - |  6374 | `	}` |
|    566119 |  6375 | `	return SXRET_OK;` |
|    283062 |  6376 | `}` |
|         - |  6377 | `/*` |
|         - |  6378 | ` * Collect function arguments one after one.` |
|         - |  6379 | ` * According to the PHP language reference manual.` |
|         - |  6380 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6381 | ` * list of expressions.` |
|         - |  6382 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6383 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6384 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6385 | ` * for more information.` |
|         - |  6386 | ` * Example #1 Passing arrays to functions` |
|         - |  6387 | ` * <?php` |
|         - |  6388 | ` * function takes_array($input)` |
|         - |  6389 | ` * {` |
|         - |  6390 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6391 | ` * }` |
|         - |  6392 | ` * ?>` |
|         - |  6393 | ` * Making arguments be passed by reference` |
|         - |  6394 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6395 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6396 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6397 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6398 | ` * to the argument name in the function definition:` |
|         - |  6399 | ` * Example #2 Passing function parameters by reference` |
|         - |  6400 | ` * <?php` |
|         - |  6401 | ` * function add_some_extra(&$string)` |
|         - |  6402 | ` * {` |
|         - |  6403 | ` *   $string .= 'and something extra.';` |
|         - |  6404 | ` * }` |
|         - |  6405 | ` * $str = 'This is a string, ';` |
|         - |  6406 | ` * add_some_extra($str);` |
|         - |  6407 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6408 | ` * ?>` |
|         - |  6409 | ` *` |
|         - |  6410 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6411 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6412 | ` * on these extension.` |
|         - |  6413 | ` */` |
|   1290708 |  6414 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6415 | `{` |
|         - |  6416 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6417 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6418 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6419 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6420 | `	sxi32 rc;` |
|         - |  6421 |  |
|   1290713 |  6422 | `	pIn = pGen->pIn;` |
|   1290713 |  6423 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6424 | `	/* Process arguments one after one */` |
|   1673603 |  6425 | `	for(;;){` |
|   3347211 |  6426 | `		if( pIn >= pEnd ){` |
|         - |  6427 | `			/* No more arguments to process */` |
|   1290697 |  6428 | `			break;` |
|         - |  6429 | `		}` |
|   2056519 |  6430 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2056519 |  6431 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2056519 |  6432 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2056519 |  6433 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2056519 |  6434 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6435 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6436 | `		 * first token inside the main token stream */` |
|   2056519 |  6437 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6438 | `			return SXERR_ABORT;` |
|         - |  6439 | `		}` |
|         - |  6440 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6441 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6442 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6443 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6444 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6445 | `		{` |
|   2056519 |  6446 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2056519 |  6447 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2056519 |  6448 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6449 | `			int nSetTok;` |
|         - |  6450 | `			sxi32 nSetVis;` |
|   2056519 |  6451 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6452 | `				bReadonly = 1;` |
|         3 |  6453 | `				pIn++;` |
|         1 |  6454 | `			}` |
|   2056519 |  6455 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2056519 |  6456 | `			if( nSetVis ){` |
|         - |  6457 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6458 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6459 | `				bVisSeen = 1;` |
|         3 |  6460 | `				pIn += nSetTok;` |
|         3 |  6461 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6462 | `					bReadonly = 1;` |
|       ! 0 |  6463 | `					pIn++;` |
|         1 |  6464 | `				}` |
|   2056518 |  6465 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     88377 |  6466 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     88377 |  6467 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        91 |  6468 | `					bVisSeen = 1;` |
|        91 |  6469 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       121 |  6470 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6471 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        91 |  6472 | `					pIn++;` |
|        91 |  6473 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        91 |  6474 | `					if( nSetVis ){` |
|         - |  6475 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6476 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6477 | `						pIn += nSetTok;` |
|         1 |  6478 | `					}` |
|        91 |  6479 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6480 | `						bReadonly = 1;` |
|        18 |  6481 | `						pIn++;` |
|         7 |  6482 | `					}` |
|        43 |  6483 | `				}` |
|     44186 |  6484 | `			}` |
|   2056519 |  6485 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6486 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2056517 |  6487 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6488 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6489 | `			}` |
|   2056519 |  6490 | `			if( bVisSeen \|\| bReadonly ){` |
|        95 |  6491 | `				if( !bCtorCtx ){` |
|         6 |  6492 | `					if( bAbstractCtx ){` |
|         3 |  6493 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6494 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6495 | `					}else{` |
|         3 |  6496 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6497 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6498 | `					}` |
|         6 |  6499 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6500 | `						return SXERR_ABORT;` |
|         - |  6501 | `					}` |
|         6 |  6502 | `					return SXERR_SYNTAX;` |
|         - |  6503 | `				}` |
|        91 |  6504 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        91 |  6505 | `				sArg.iPromoteVis = iVis;` |
|        91 |  6506 | `				if( bReadonly ){` |
|        20 |  6507 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6508 | `				}` |
|        43 |  6509 | `			}` |
|         - |  6510 | `		}` |
|         - |  6511 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   2056510 |  6512 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1108895 |  6513 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    149796 |  6514 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    115281 |  6515 | `			sxu32 nLineLocal = pIn->nLine;` |
|    115281 |  6516 | `			sxi32 iTFlags = 0;` |
|    115281 |  6517 | `			pGen->pIn = pIn;` |
|    115281 |  6518 | `			rc = GenStateParseUnionTypeDecl(` |
|     57638 |  6519 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57638 |  6520 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6521 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6522 | `				/* bAllowVoid */ 0,` |
|     57638 |  6523 | `						nLineLocal);` |
|    115281 |  6524 | `			pIn = pGen->pIn;` |
|    115281 |  6525 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6526 | `				return SXERR_ABORT;` |
|    115281 |  6527 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6528 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6529 | `				return SXERR_SYNTAX;` |
|    115279 |  6530 | `			}else if( rc == SXERR_SYNTAX ){` |
|        10 |  6531 | `				if( pIn < pEnd ){` |
|        14 |  6532 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6533 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6534 | `						&pIn->sData);` |
|         6 |  6535 | `				}else{` |
|       ! 0 |  6536 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6537 | `						"syntax error, unexpected end of file");` |
|         - |  6538 | `				}` |
|        10 |  6539 | `				return SXERR_SYNTAX;` |
|         - |  6540 | `			}` |
|    115271 |  6541 | `			sArg.iFlags \|= iTFlags;` |
|     57633 |  6542 | `		}` |
|   2056505 |  6543 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6544 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6545 | `			return rc;` |
|         - |  6546 | `		}` |
|   2056505 |  6547 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6548 | `			/* Pass by reference,record that */` |
|     22991 |  6549 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     22991 |  6550 | `			pIn++;` |
|     11493 |  6551 | `		}` |
|   2056505 |  6552 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6553 | `			/* Variadic parameter: ...$args */` |
|     23053 |  6554 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     23053 |  6555 | `			pIn++;` |
|     11524 |  6556 | `		}` |
|   2056505 |  6557 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6558 | `			/* Invalid argument */` |
|       ! 0 |  6559 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6560 | `			return rc;` |
|         - |  6561 | `		}` |
|   2056505 |  6562 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6563 | `		/* Copy argument name */` |
|   2056505 |  6564 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2056505 |  6565 | `		if( zDup == 0 ){` |
|       ! 0 |  6566 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6567 | `			return SXERR_ABORT;` |
|         - |  6568 | `		}` |
|   2056505 |  6569 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2056505 |  6570 | `		pIn++;` |
|   2056505 |  6571 | `		if( pIn < pEnd ){` |
|   1144503 |  6572 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6573 | `				SyToken *pDefend;` |
|    566121 |  6574 | `				sxi32 iNest = 0;` |
|    566121 |  6575 | `				pIn++; /* Jump the equal sign */` |
|    566121 |  6576 | `				pDefend = pIn;` |
|         - |  6577 | `				/* Process the default value associated with this argument */` |
|   1189631 |  6578 | `				while( pDefend < pEnd ){` |
|    810945 |  6579 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    187435 |  6580 | `						break;` |
|         - |  6581 | `					}` |
|    623515 |  6582 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6583 | `						/* Increment nesting level */` |
|     26785 |  6584 | `						iNest++;` |
|    610125 |  6585 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6586 | `						/* Decrement nesting level */` |
|     26785 |  6587 | `						iNest--;` |
|     13390 |  6588 | `					}` |
|    623515 |  6589 | `					pDefend++;` |
|         5 |  6590 | `				}` |
|    566121 |  6591 | `				if( pIn >= pDefend ){` |
|         3 |  6592 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6593 | `					return rc;` |
|         - |  6594 | `				}` |
|         - |  6595 | `				/* Process default value */` |
|    566119 |  6596 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    566119 |  6597 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6598 | `					return rc;` |
|         - |  6599 | `				}` |
|         - |  6600 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6601 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6602 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6603 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6604 | `				 * arg-type check lets null through. */` |
|    566114 |  6605 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    307936 |  6606 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    307933 |  6607 | `					&& &pIn[1] == pDefend` |
|     45927 |  6608 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     34438 |  6609 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     21043 |  6610 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15307 |  6611 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6612 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6613 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6614 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6615 | `					 * already up at this point). */` |
|         - |  6616 | `					{` |
|     15307 |  6617 | `						const char *zSep = "";` |
|     15307 |  6618 | `						SyString sCls = { "", 0 };` |
|     15307 |  6619 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15301 |  6620 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15301 |  6621 | `							zSep = "::";` |
|      7648 |  6622 | `						}` |
|     22958 |  6623 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6624 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7651 |  6625 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6626 | `					}` |
|      7651 |  6627 | `				}` |
|         - |  6628 | `				/* Point beyond the default value */` |
|    566119 |  6629 | `				pIn = pDefend;` |
|    283057 |  6630 | `			}` |
|   1144501 |  6631 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6632 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6633 | `				return rc;` |
|         - |  6634 | `			}` |
|   1144501 |  6635 | `			pIn++; /* Jump the trailing comma */` |
|    572248 |  6636 | `		}` |
|         - |  6637 | `		/* Append argument signature */` |
|   2056503 |  6638 | `		if( sArg.nType > 0 ){` |
|    115209 |  6639 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6640 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26873 |  6641 | `				int marker = 'o';` |
|     26873 |  6642 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26873 |  6643 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13439 |  6644 | `			}else{` |
|         - |  6645 | `				int c;` |
|     88341 |  6646 | `				c = 'n'; /* cc warning */` |
|         - |  6647 | `				/* Type leading character */` |
|     88341 |  6648 | `				switch(sArg.nType){` |
|      5743 |  6649 | `				case MEMOBJ_HASHMAP:` |
|         - |  6650 | `					/* Hashmap aka 'array' */` |
|     11491 |  6651 | `					c = 'h';` |
|     11491 |  6652 | `					break;` |
|      9688 |  6653 | `				case MEMOBJ_INT:` |
|         - |  6654 | `					/* Integer */` |
|     19381 |  6655 | `					c = 'i';` |
|     19381 |  6656 | `					break;` |
|         2 |  6657 | `				case MEMOBJ_BOOL:` |
|         - |  6658 | `					/* Bool */` |
|         5 |  6659 | `					c = 'b';` |
|         5 |  6660 | `					break;` |
|         5 |  6661 | `				case MEMOBJ_REAL:` |
|         - |  6662 | `					/* Float */` |
|        12 |  6663 | `					c = 'f';` |
|        12 |  6664 | `					break;` |
|     28722 |  6665 | `				case MEMOBJ_STRING:` |
|         - |  6666 | `					/* String */` |
|     57449 |  6667 | `					c = 's';` |
|     57449 |  6668 | `					break;` |
|         7 |  6669 | `				case MEMOBJ_OBJ:` |
|         - |  6670 | `					/* Object */` |
|        16 |  6671 | `					c = 'o';` |
|        14 |  6672 | `					break;` |
|         1 |  6673 | `				default:` |
|         2 |  6674 | `					break;` |
|         - |  6675 | `				}` |
|     88341 |  6676 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6677 | `			}` |
|     57607 |  6678 | `		}else{` |
|         - |  6679 | `			/* No type is associated with this parameter which mean` |
|         - |  6680 | `			 * that this function is not condidate for overloading.` |
|         - |  6681 | `			 */` |
|   1941299 |  6682 | `			SyBlobRelease(&sSig);` |
|         - |  6683 | `		}` |
|         - |  6684 | `		/* Save in the argument set */` |
|   2056503 |  6685 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6686 | `	}` |
|   1290697 |  6687 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6688 | `		/* Save function signature */` |
|     84539 |  6689 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     42267 |  6690 | `	}` |
|   1290697 |  6691 | `	return SXRET_OK;` |
|    645359 |  6692 | `}` |
|         - |  6693 | `/*` |
|         - |  6694 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6695 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6696 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6697 | ` */` |
|     34462 |  6698 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6699 | `{` |
|     34467 |  6700 | `	sxi32 iParen = 0;` |
|     34467 |  6701 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6702 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6703 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6704 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    153215 |  6705 | `	while( pIn < pEnd ){` |
|    153215 |  6706 | `		sxu32 t = pIn->nType;` |
|    153215 |  6707 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    149333 |  6708 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    103387 |  6709 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     84221 |  6710 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    118753 |  6711 | `		pIn++;` |
|         5 |  6712 | `	}` |
|     19171 |  6713 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6714 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6715 | `	{` |
|     19171 |  6716 | `		sxi32 d = 0;` |
|    761497 |  6717 | `		while( pIn < pEnd ){` |
|    761497 |  6718 | `			sxu32 t = pIn->nType;` |
|    761497 |  6719 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    730851 |  6720 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    742331 |  6721 | `			pIn++;` |
|         5 |  6722 | `		}` |
|         - |  6723 | `	}` |
|     19171 |  6724 | `	return pIn;` |
|     17236 |  6725 | `}` |
|         - |  6726 | `/*` |
|         - |  6727 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6728 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6729 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6730 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6731 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6732 | ` * detached-mini-program path untouched.` |
|         - |  6733 | ` */` |
|         - |  6734 | `/*` |
|         - |  6735 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6736 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6737 | ` * mixed, object.` |
|         - |  6738 | ` */` |
|     11496 |  6739 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6740 | `{` |
|         - |  6741 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6742 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6743 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6744 | `	};` |
|         - |  6745 | `	sxu32 i;` |
|     11501 |  6746 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6747 | `		zName++;` |
|       ! 0 |  6748 | `		nName--;` |
|       ! 0 |  6749 | `	}` |
|     11509 |  6750 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11509 |  6751 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11501 |  6752 | `			return 1;` |
|         - |  6753 | `		}` |
|         5 |  6754 | `	}` |
|       ! 0 |  6755 | `	return 0;` |
|      5753 |  6756 | `}` |
|         - |  6757 | `/*` |
|         - |  6758 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6759 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6760 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6761 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6762 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6763 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6764 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6765 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6766 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6767 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6768 | ` */` |
|     11498 |  6769 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6770 | `{` |
|     11503 |  6771 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6772 | ``		return 1; /* bare `object` */`` |
|         - |  6773 | `	}` |
|     11503 |  6774 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6775 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6776 | `	}` |
|     11501 |  6777 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11501 |  6778 | `		return 1;` |
|         - |  6779 | `	}` |
|         - |  6780 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6781 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6782 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6783 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6784 | `	{` |
|         - |  6785 | `		SyBlob sFQN;` |
|         - |  6786 | `		int bOk;` |
|       ! 0 |  6787 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       ! 0 |  6788 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       ! 0 |  6789 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       ! 0 |  6790 | `		SyBlobRelease(&sFQN);` |
|       ! 0 |  6791 | `		return bOk;` |
|         - |  6792 | `	}` |
|      5754 |  6793 | `}` |
|         - |  6794 | `/*` |
|         - |  6795 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6796 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6797 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6798 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6799 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6800 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6801 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6802 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6803 | ` */` |
|     11738 |  6804 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6805 | `{` |
|     11743 |  6806 | `	int bOk = 0;` |
|         - |  6807 | `	sxu32 nLine;` |
|         - |  6808 | `	sxi32 rc;` |
|     11743 |  6809 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       245 |  6810 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6811 | `	}` |
|     11503 |  6812 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6813 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6814 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6815 | `		sxu32 i,j;` |
|       ! 0 |  6816 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6817 | `			int bGroupOk;` |
|       ! 0 |  6818 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6819 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6820 | `			}` |
|       ! 0 |  6821 | `			bGroupOk = 1;` |
|       ! 0 |  6822 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6823 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6824 | `					bGroupOk = 0;` |
|       ! 0 |  6825 | `					break;` |
|         - |  6826 | `				}` |
|       ! 0 |  6827 | `			}` |
|       ! 0 |  6828 | `			bOk = bGroupOk;` |
|       ! 0 |  6829 | `		}` |
|       ! 0 |  6830 | `	}else{` |
|     11503 |  6831 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6832 | `	}` |
|     11503 |  6833 | `	if( bOk ){` |
|     11501 |  6834 | `		return SXRET_OK;` |
|         - |  6835 | `	}` |
|         - |  6836 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6837 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6838 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6839 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6840 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6841 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6842 | `	{` |
|         3 |  6843 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6844 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6845 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6846 | `		}` |
|         3 |  6847 | `		if( sGiven.nByte < 1 ){` |
|         - |  6848 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6849 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6850 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6851 | `			const char *zScalar =` |
|       ! 0 |  6852 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6853 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6854 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6855 | `		}` |
|         3 |  6856 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6857 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6858 | `	}` |
|         3 |  6859 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5874 |  6860 | `}` |
|   2753344 |  6861 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6862 | `{` |
|   2753349 |  6863 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2753349 |  6864 | `	SyToken *pEnd = pGen->pEnd;` |
|   2753349 |  6865 | `	sxi32 iDepth = 0;` |
|   2753349 |  6866 | `	int bStarted = 0;` |
| 133407573 |  6867 | `	while( pIn < pEnd ){` |
| 133407573 |  6868 | `		sxu32 t = pIn->nType;` |
| 133407573 |  6869 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 127433393 |  6870 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 121494011 |  6871 | `		if( t & PH7_TK_KEYWORD ){` |
|   9053405 |  6872 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   9053405 |  6873 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   9041667 |  6874 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6875 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   4503600 |  6876 | `		}` |
| 121447811 |  6877 | `		pIn++;` |
|         5 |  6878 | `	}` |
|   2741611 |  6879 | `	return FALSE;` |
|   1376677 |  6880 | `}` |
|         - |  6881 | `/*` |
|         - |  6882 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6883 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6884 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6885 | ` */` |
|   2753344 |  6886 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6887 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6888 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6889 | `	)` |
|         5 |  6890 | `{` |
|         - |  6891 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6892 | `	GenBlock *pBlock;` |
|         - |  6893 | `	sxu32 nGotoOfft;` |
|         - |  6894 | `	sxi32 rc;` |
|         - |  6895 | `	/* Attach the new function */` |
|   2753349 |  6896 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2753349 |  6897 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6898 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6899 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6900 | `		return SXERR_ABORT;` |
|         - |  6901 | `	}` |
|   2753349 |  6902 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6903 | `	/* Swap bytecode containers */` |
|   2753349 |  6904 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2753349 |  6905 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6906 | `	/* Emit constructor property promotion prologue:` |
|         - |  6907 | `	 *   $this->NAME = $NAME;` |
|         - |  6908 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6909 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6910 | `	{` |
|   2753349 |  6911 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6912 | `		sxu32 i;` |
|   4756165 |  6913 | `		for( i = 0; i < nArg; i++ ){` |
|   2002821 |  6914 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6915 | `			char *zSrc;` |
|         - |  6916 | `			sxu32 nSrc,nName;` |
|         - |  6917 | `			SySet sToken;` |
|         - |  6918 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6919 | `			sxi32 rcPromote;` |
|   2002821 |  6920 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   2002745 |  6921 | `				continue;` |
|         - |  6922 | `			}` |
|         - |  6923 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6924 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6925 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6926 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6927 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        81 |  6928 | `			nName = SyStringLength(&pArg->sName);` |
|        81 |  6929 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        81 |  6930 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        81 |  6931 | `			if( zSrc == 0 ){` |
|       ! 0 |  6932 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6933 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6934 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6935 | `				return SXERR_ABORT;` |
|         - |  6936 | `			}` |
|         - |  6937 | `			{` |
|        81 |  6938 | `				char *z = zSrc;` |
|        81 |  6939 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        81 |  6940 | `				z += sizeof("$this->")-1;` |
|        81 |  6941 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        81 |  6942 | `				z += nName;` |
|        81 |  6943 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        81 |  6944 | `				z += sizeof(" = $")-1;` |
|        81 |  6945 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        81 |  6946 | `				z += nName;` |
|        81 |  6947 | `				*z = 0;` |
|         - |  6948 | `			}` |
|        81 |  6949 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        81 |  6950 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        81 |  6951 | `			pTmpIn = pGen->pIn;` |
|        81 |  6952 | `			pTmpEnd = pGen->pEnd;` |
|        81 |  6953 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        81 |  6954 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        81 |  6955 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        81 |  6956 | `			pGen->pIn = pTmpIn;` |
|        81 |  6957 | `			pGen->pEnd = pTmpEnd;` |
|        81 |  6958 | `			SySetRelease(&sToken);` |
|        81 |  6959 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6960 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6961 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6962 | `				return SXERR_ABORT;` |
|         - |  6963 | `			}` |
|         - |  6964 | `			/* Discard the assignment result — this is a statement expression. */` |
|        81 |  6965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        43 |  6966 | `		}` |
|         - |  6967 | `	}` |
|         - |  6968 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6969 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6970 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6971 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6972 | `	{` |
|   2753349 |  6973 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2753349 |  6974 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6975 | `		/* Compile the body */` |
|   2753349 |  6976 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2753349 |  6977 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6978 | `	}` |
|         - |  6979 | `	/* Fix exception jumps now the destination is resolved */` |
|   2753349 |  6980 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6981 | `	/* Emit the final return if not yet done */` |
|   2753349 |  6982 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6983 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2753349 |  6984 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6985 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6986 | `	}` |
|   2753349 |  6987 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6988 | `	/* Restore the default container */` |
|   2753349 |  6989 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6990 | `	/* Leave function block */` |
|   2753349 |  6991 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2753349 |  6992 | `	if( rc == SXERR_ABORT ){` |
|         - |  6993 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6994 | `		return SXERR_ABORT;` |
|         - |  6995 | `	}` |
|         - |  6996 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6997 | `	{` |
|   2753349 |  6998 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6999 | `		sxu32 i;` |
|  81403285 |  7000 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  78661679 |  7001 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11743 |  7002 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11743 |  7003 | `				break;` |
|         - |  7004 | `			}` |
|  39324973 |  7005 | `		}` |
|         - |  7006 | `	}` |
|   2753349 |  7007 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  7008 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11743 |  7009 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  7010 | `			return SXERR_ABORT;` |
|         - |  7011 | `		}` |
|      5869 |  7012 | `	}` |
|         - |  7013 | `	/* All done, function body compiled */` |
|   2753349 |  7014 | `	return SXRET_OK;` |
|   1376677 |  7015 | `}` |
|         - |  7016 | `/*` |
|         - |  7017 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  7018 | ` * According to the PHP language reference manual.` |
|         - |  7019 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  7020 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  7021 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  7022 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  7023 | ` *  Functions need not be defined before they are referenced.` |
|         - |  7024 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  7025 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  7026 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  7027 | ` *  calls with over 32-64 recursion levels.` |
|         - |  7028 | ` *` |
|         - |  7029 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  7030 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  7031 | ` * on these extension.` |
|         - |  7032 | ` */` |
|         - |  7033 | `/*` |
|         - |  7034 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  7035 | ` */` |
|       590 |  7036 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  7037 | `{` |
|         - |  7038 | `	sxu32 i;` |
|      1655 |  7039 | `	for( i = 0; i < n; i++ ){` |
|      1419 |  7040 | `		int a = zA[i], b = zB[i];` |
|      1419 |  7041 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1419 |  7042 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1419 |  7043 | `		if( a != b ) return a - b;` |
|       535 |  7044 | `	}` |
|       241 |  7045 | `	return 0;` |
|       300 |  7046 | `}` |
|         - |  7047 | `/*` |
|         - |  7048 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  7049 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  7050 | ` * (which are positive bit values stored in sxu32).` |
|         - |  7051 | ` */` |
|         - |  7052 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  7053 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  7054 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  7055 |  |
|         - |  7056 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  7057 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  7058 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  7059 |  |
|         - |  7060 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  7061 | `struct PhlTypeAtom {` |
|         - |  7062 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  7063 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  7064 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  7065 | `	sxu32 nCanon;` |
|         - |  7066 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  7067 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  7068 | `};` |
|         - |  7069 |  |
|         - |  7070 | `/*` |
|         - |  7071 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  7072 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  7073 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  7074 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  7075 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  7076 | ` * already be consumed by the caller.` |
|         - |  7077 | ` */` |
|         - |  7078 | `/*` |
|         - |  7079 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|         - |  7080 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|         - |  7081 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|         - |  7082 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|         - |  7083 | ` * null/void/never are matched before the class path), but the full set is listed` |
|         - |  7084 | ` * so the guard is robust to lexer changes.` |
|         - |  7085 | ` */` |
|     38576 |  7086 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|         5 |  7087 | `{` |
|         - |  7088 | `	static const char *azWords[] = {` |
|         - |  7089 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|         - |  7090 | `		"bool","boolean","int","integer","float","double","string","array",` |
|         - |  7091 | `		"object","self","static","parent"` |
|         - |  7092 | `	};` |
|         - |  7093 | `	sxu32 i;` |
|    808195 |  7094 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    769727 |  7095 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    769727 |  7096 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|       113 |  7097 | `			return 1;` |
|         - |  7098 | `		}` |
|    384812 |  7099 | `	}` |
|     38473 |  7100 | `	return 0;` |
|     19293 |  7101 | `}` |
|    128112 |  7102 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  7103 | `{` |
|    128117 |  7104 | `	SyToken *pIn = pGen->pIn;` |
|    128117 |  7105 | `	int bAbsolute = 0;` |
|    128117 |  7106 | `	SyZero(pOut, sizeof(*pOut));` |
|    128117 |  7107 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    128117 |  7108 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7109 | `		return SXERR_SYNTAX;` |
|         - |  7110 | `	}` |
|         - |  7111 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    128117 |  7112 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        10 |  7113 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|        10 |  7114 | `		pIn++;` |
|        10 |  7115 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7116 | `			return SXERR_SYNTAX;` |
|         - |  7117 | `		}` |
|         4 |  7118 | `	}` |
|    128117 |  7119 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7120 | `		return SXERR_SYNTAX;` |
|         - |  7121 | `	}` |
|    128117 |  7122 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     89303 |  7123 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     89303 |  7124 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11553 |  7125 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     83529 |  7126 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        85 |  7127 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     77715 |  7128 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19801 |  7129 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67777 |  7130 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57785 |  7131 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     28989 |  7132 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  7133 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        81 |  7134 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        28 |  7135 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        50 |  7136 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        17 |  7137 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        35 |  7138 | `			pOut->nType = SXU32_HIGH;` |
|        35 |  7139 | `			pOut->sClass = pIn->sData;` |
|        19 |  7140 | `		}else{` |
|         3 |  7141 | `			return SXERR_SYNTAX;` |
|         - |  7142 | `		}` |
|     89301 |  7143 | `		pIn++;` |
|     44653 |  7144 | `	}else{` |
|         - |  7145 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  7146 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38819 |  7147 | `		SyString *pT = &pIn->sData;` |
|     38819 |  7148 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  7149 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  7150 | `			pIn++;` |
|     38804 |  7151 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       183 |  7152 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       183 |  7153 | `			pIn++;` |
|     38700 |  7154 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        27 |  7155 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        27 |  7156 | `			pIn++;` |
|        16 |  7157 | `		}else{` |
|         - |  7158 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38589 |  7159 | `			SyToken *pFirst = pIn;` |
|     38589 |  7160 | `			SyToken *pLast = pIn;` |
|     38589 |  7161 | `			pOut->nType = SXU32_HIGH;` |
|     38589 |  7162 | `			pOut->sClass = pIn->sData;` |
|     38589 |  7163 | `			pIn++;` |
|     57879 |  7164 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38592 |  7165 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  7166 | `				pLast = &pIn[1];` |
|         3 |  7167 | `				pIn += 2;` |
|         1 |  7168 | `			}` |
|     38589 |  7169 | `			if( pLast != pFirst ){` |
|         3 |  7170 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  7171 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  7172 | `				pOut->sClass.zString = zFirst;` |
|         3 |  7173 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  7174 | `			}` |
|         - |  7175 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|         - |  7176 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|         - |  7177 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|         - |  7178 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|         - |  7179 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|         - |  7180 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|         - |  7181 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|         - |  7182 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|     38589 |  7183 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|         - |  7184 | `				SyBlob sFqn;` |
|     38473 |  7185 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     38473 |  7186 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|     38468 |  7187 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|     38468 |  7188 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|        17 |  7189 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 |  7190 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|        12 |  7191 | `					if( zDup ){` |
|        12 |  7192 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|         5 |  7193 | `					}` |
|         5 |  7194 | `				}` |
|     38473 |  7195 | `				SyBlobRelease(&sFqn);` |
|     19234 |  7196 | `			}` |
|         - |  7197 | `		}` |
|         - |  7198 | `	}` |
|    128115 |  7199 | `	pGen->pIn = pIn;` |
|    128115 |  7200 | `	return SXRET_OK;` |
|     64061 |  7201 | `}` |
|         - |  7202 |  |
|         - |  7203 | `/*` |
|         - |  7204 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  7205 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  7206 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  7207 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  7208 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  7209 | ` */` |
|    127934 |  7210 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  7211 | `{` |
|         - |  7212 | `	int i;` |
|    127939 |  7213 | `	int nNonNull = 0;` |
|    127939 |  7214 | `	int bAnyIntersection = 0;` |
|         - |  7215 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    127939 |  7216 | `	sxu32 nMaxGroup = 0;` |
|   4221827 |  7217 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    256025 |  7218 | `	for( i = 0; i < nAtoms; i++ ){` |
|    128091 |  7219 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    128061 |  7220 | `			nNonNull++;` |
|    128061 |  7221 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    128061 |  7222 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    128061 |  7223 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     64028 |  7224 | `			}` |
|     64028 |  7225 | `		}` |
|     64048 |  7226 | `	}` |
|    255973 |  7227 | `	for( i = 0; i < nAtoms; i++ ){` |
|    128063 |  7228 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  7229 | `			bAnyIntersection = 1;` |
|        29 |  7230 | `			break;` |
|         - |  7231 | `		}` |
|     64022 |  7232 | `	}` |
|    127939 |  7233 | `	if( bAnyIntersection ){` |
|         - |  7234 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  7235 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  7236 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  7237 | `		sxu32 g, nGroups = 0;` |
|        29 |  7238 | `		int bFirstGroup = 1;` |
|        59 |  7239 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  7240 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  7241 | `			int bFirstMember = 1;` |
|         - |  7242 | `			int bWrap;` |
|        35 |  7243 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  7244 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  7245 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  7246 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  7247 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  7248 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  7249 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  7250 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  7251 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  7252 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  7253 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  7254 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  7255 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  7256 | `				}else{` |
|         6 |  7257 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7258 | `				}` |
|        59 |  7259 | `				bFirstMember = 0;` |
|        32 |  7260 | `			}` |
|        35 |  7261 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7262 | `			bFirstGroup = 0;` |
|        20 |  7263 | `		}` |
|        29 |  7264 | `		if( bNullable ){` |
|       ! 0 |  7265 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7266 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7267 | `		}` |
|        85 |  7268 | `		return;` |
|         - |  7269 | `	}` |
|    127915 |  7270 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7271 | `		/* Shorthand: ?T */` |
|       117 |  7272 | `		for( i = 0; i < nAtoms; i++ ){` |
|       117 |  7273 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       117 |  7274 | `			SyBlobAppend(pBlob, "?", 1);` |
|       117 |  7275 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        24 |  7276 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  7277 | `			}else{` |
|        95 |  7278 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7279 | `			}` |
|       117 |  7280 | `			return;` |
|       ! 0 |  7281 | `		}` |
|       ! 0 |  7282 | `	}` |
|         - |  7283 | `	{` |
|    127803 |  7284 | `		int bFirst = 1;` |
|         - |  7285 | `		/* 1) Classes in declaration order */` |
|    255709 |  7286 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127911 |  7287 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38549 |  7288 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38549 |  7289 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38549 |  7290 | `				bFirst = 0;` |
|     19272 |  7291 | `			}` |
|     63958 |  7292 | `		}` |
|         - |  7293 | `		/* 2) Built-ins in canonical order */` |
|         - |  7294 | `		{` |
|         - |  7295 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7296 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7297 | `			int k;` |
|    894591 |  7298 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1444963 |  7299 | `				for( i = 0; i < nAtoms; i++ ){` |
|    767329 |  7300 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     89159 |  7301 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     89159 |  7302 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     89159 |  7303 | `						bFirst = 0;` |
|     89159 |  7304 | `						break;` |
|         - |  7305 | `					}` |
|    339090 |  7306 | `				}` |
|    383399 |  7307 | `			}` |
|         - |  7308 | `		}` |
|         - |  7309 | `		/* 3) null suffix */` |
|    127803 |  7310 | `		if( bNullable ){` |
|        20 |  7311 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        20 |  7312 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7313 | `		}` |
|         - |  7314 | `	}` |
|     63972 |  7315 | `}` |
|         - |  7316 |  |
|         - |  7317 | `/*` |
|         - |  7318 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7319 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7320 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7321 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7322 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7323 | ` * whether it was parenthesized.` |
|         - |  7324 | ` *` |
|         - |  7325 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7326 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7327 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7328 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7329 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7330 | ` */` |
|    128086 |  7331 | `static sxi32 GenStateParsePart(` |
|         - |  7332 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7333 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7334 | `{` |
|         - |  7335 | `	sxi32 rc;` |
|    128091 |  7336 | `	int nMembers = 0;` |
|    128091 |  7337 | `	int bParen = 0;` |
|    128091 |  7338 | `	*pnMembers = 0;` |
|    128091 |  7339 | `	*pbParen = 0;` |
|    128091 |  7340 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7341 | `		bParen = 1;` |
|         9 |  7342 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7343 | `	}` |
|     64043 |  7344 | `	for(;;){` |
|    128117 |  7345 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7346 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7347 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7348 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7349 | `		}` |
|    128117 |  7350 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    128117 |  7351 | `		if( rc != SXRET_OK ){` |
|         3 |  7352 | `			return rc;` |
|         - |  7353 | `		}` |
|    128115 |  7354 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    128115 |  7355 | `		(*pnAtoms)++;` |
|    128115 |  7356 | `		nMembers++;` |
|         - |  7357 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    128115 |  7358 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7359 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7360 | `			if( pNext < pGen->pEnd` |
|        39 |  7361 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7362 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7363 | `				continue;` |
|         - |  7364 | `			}` |
|         4 |  7365 | `		}` |
|    128089 |  7366 | `		break;` |
|       ! 0 |  7367 | `	}` |
|    128089 |  7368 | `	if( bParen ){` |
|         9 |  7369 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7370 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7371 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7372 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7373 | `		}` |
|         9 |  7374 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7375 | `		if( nMembers < 2 ){` |
|       ! 0 |  7376 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7377 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7378 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7379 | `		}` |
|         3 |  7380 | `	}` |
|    128089 |  7381 | `	*pnMembers = nMembers;` |
|    128089 |  7382 | `	*pbParen = bParen;` |
|    128089 |  7383 | `	return SXRET_OK;` |
|     64048 |  7384 | `}` |
|         - |  7385 |  |
|         - |  7386 | `/*` |
|         - |  7387 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7388 | ` *` |
|         - |  7389 | ` * Outputs:` |
|         - |  7390 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7391 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7392 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7393 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7394 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7395 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7396 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7397 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7398 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7399 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7400 | ` *` |
|         - |  7401 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7402 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7403 | ` */` |
|    127950 |  7404 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7405 | `	ph7_gen_state *pGen,` |
|         - |  7406 | `	sxu32 *pnType,` |
|         - |  7407 | `	SyString *pClass,` |
|         - |  7408 | `	SySet *pAlts,` |
|         - |  7409 | `	sxi32 *piTypeFlags,` |
|         - |  7410 | `	SyString *pTypeText,` |
|         - |  7411 | `	int iNullableFlag,` |
|         - |  7412 | `	int iUnionFlag,` |
|         - |  7413 | `	int bAllowVoid,` |
|         - |  7414 | `	sxu32 nLine` |
|         5 |  7415 | `){` |
|         - |  7416 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    127955 |  7417 | `	int nAtoms = 0;` |
|    127955 |  7418 | `	int bShortNullable = 0;` |
|    127955 |  7419 | `	int bExplicitNull = 0;` |
|         - |  7420 | `	sxi32 rc;` |
|    127955 |  7421 | `	*pnType = 0;` |
|    127955 |  7422 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    127955 |  7423 | `	*piTypeFlags = 0;` |
|    127955 |  7424 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7425 |  |
|    127955 |  7426 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7427 | `		return SXRET_OK;` |
|         - |  7428 | `	}` |
|         - |  7429 | ``	/* Optional `?` shorthand prefix */`` |
|    127950 |  7430 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       105 |  7431 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       105 |  7432 | `		bShortNullable = 1;` |
|       105 |  7433 | `		pGen->pIn++;` |
|       105 |  7434 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7435 | `			return SXERR_SYNTAX;` |
|         - |  7436 | `		}` |
|        50 |  7437 | `	}` |
|         - |  7438 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7439 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7440 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7441 | `	{` |
|         - |  7442 | `		int nMembers, bParen;` |
|    127955 |  7443 | `		sxu32 iGroup = 0;` |
|    127955 |  7444 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    127955 |  7445 | `		if( rc != SXRET_OK ){` |
|         4 |  7446 | `			return rc;` |
|         - |  7447 | `		}` |
|         - |  7448 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7449 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7450 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7451 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7452 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    192131 |  7453 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    128162 |  7454 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7455 | `			if( bShortNullable ){` |
|         - |  7456 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7457 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7458 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7459 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7460 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7461 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7462 | `			}` |
|       141 |  7463 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7464 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7465 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7466 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7467 | `			}` |
|       141 |  7468 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7469 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7470 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7471 | `				return rc;` |
|         - |  7472 | `			}` |
|         5 |  7473 | `		}` |
|    127951 |  7474 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7475 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7476 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7477 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7478 | `		}` |
|         - |  7479 | `	}` |
|         - |  7480 | `	/* Validation pass.` |
|         - |  7481 | `	 *` |
|         - |  7482 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7483 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7484 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7485 | `	 */` |
|         - |  7486 | `	{` |
|         - |  7487 | `		int i, j;` |
|    127951 |  7488 | `		int bHasNonNull = 0;` |
|    127951 |  7489 | `		int bAnyIntersection = 0;` |
|         - |  7490 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7491 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7492 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4222223 |  7493 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    256059 |  7494 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128113 |  7495 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     64059 |  7496 | `		}` |
|    256003 |  7497 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128083 |  7498 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     64031 |  7499 | `		}` |
|         - |  7500 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7501 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    127951 |  7502 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7503 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7504 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7505 | `			return SXERR_SYNTAX;` |
|         - |  7506 | `		}` |
|    256045 |  7507 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7508 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7509 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7510 | ``			 * `true`/`false` in an intersection). */`` |
|    128111 |  7511 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7512 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7513 | `				if( bClassLike ){` |
|        53 |  7514 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7515 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7516 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7517 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7518 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7519 | `						bClassLike = 0;` |
|       ! 0 |  7520 | `					}` |
|        24 |  7521 | `				}` |
|        55 |  7522 | `				if( !bClassLike ){` |
|         - |  7523 | `					const char *zName; sxu32 nName;` |
|         3 |  7524 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7525 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7526 | `					}else{` |
|         3 |  7527 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7528 | `					}` |
|         4 |  7529 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7530 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7531 | `						(int)nName, zName);` |
|         3 |  7532 | `					return SXERR_SYNTAX;` |
|         - |  7533 | `				}` |
|        24 |  7534 | `			}` |
|    128109 |  7535 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       183 |  7536 | `				if( nAtoms > 1 ){` |
|         3 |  7537 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7538 | `						"Void can only be used as a standalone type");` |
|         3 |  7539 | `					return SXERR_SYNTAX;` |
|         - |  7540 | `				}` |
|       181 |  7541 | `				if( !bAllowVoid ){` |
|       ! 0 |  7542 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7543 | `						"void cannot be used here");` |
|       ! 0 |  7544 | `					return SXERR_SYNTAX;` |
|         - |  7545 | `				}` |
|       181 |  7546 | `				if( bShortNullable ){` |
|       ! 0 |  7547 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7548 | `						"Void type cannot be nullable");` |
|       ! 0 |  7549 | `					return SXERR_SYNTAX;` |
|         - |  7550 | `				}` |
|        88 |  7551 | `			}` |
|    128107 |  7552 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7553 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7554 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7555 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7556 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        27 |  7557 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7558 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7559 | `					 * same as any other non-standalone use. */` |
|         6 |  7560 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7561 | `						"never can only be used as a standalone type");` |
|         6 |  7562 | `					return SXERR_SYNTAX;` |
|         - |  7563 | `				}` |
|        21 |  7564 | `				if( !bAllowVoid ){` |
|         - |  7565 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7566 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7567 | `						"never cannot be used as a parameter type");` |
|         3 |  7568 | `					return SXERR_SYNTAX;` |
|         - |  7569 | `				}` |
|         8 |  7570 | `			}` |
|    128101 |  7571 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7572 | `				bExplicitNull = 1;` |
|        19 |  7573 | `			}else{` |
|    128071 |  7574 | `				bHasNonNull = 1;` |
|         - |  7575 | `			}` |
|         - |  7576 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7577 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7578 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7579 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7580 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    128301 |  7581 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7582 | `				int bDup = 0;` |
|       207 |  7583 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7584 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7585 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7586 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7587 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7588 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7589 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7590 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7591 | `								aAtoms[j].sClass.zString,` |
|        34 |  7592 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7593 | `							bDup = 1;` |
|       ! 0 |  7594 | `						}` |
|        27 |  7595 | `					}else{` |
|         3 |  7596 | `						bDup = 1;` |
|         - |  7597 | `					}` |
|        23 |  7598 | `				}` |
|       195 |  7599 | `				if( bDup ){` |
|         - |  7600 | `					const char *zName;` |
|         - |  7601 | `					sxu32 nName;` |
|         3 |  7602 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7603 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7604 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7605 | `					}else{` |
|         3 |  7606 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7607 | `						nName = aAtoms[i].nCanon;` |
|         - |  7608 | `					}` |
|         4 |  7609 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7610 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7611 | `					return SXERR_SYNTAX;` |
|         - |  7612 | `				}` |
|        99 |  7613 | `			}` |
|     64052 |  7614 | `		}` |
|    127939 |  7615 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7616 | `			if( bShortNullable ){` |
|         - |  7617 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7618 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7619 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7620 | `				return SXERR_SYNTAX;` |
|         - |  7621 | `			}` |
|         - |  7622 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7623 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7624 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7625 | `			 * atom, so set it here. */` |
|         7 |  7626 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7627 | `		}` |
|         - |  7628 | `	}` |
|         - |  7629 | `	/* Compute nullability flag */` |
|    127939 |  7630 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       133 |  7631 | `		*piTypeFlags \|= iNullableFlag;` |
|        64 |  7632 | `	}` |
|         - |  7633 | `	/* Build canonical type text */` |
|    127939 |  7634 | `	if( pTypeText ){` |
|         - |  7635 | `		SyBlob sBlob;` |
|    127939 |  7636 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    191857 |  7637 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     63967 |  7638 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    127939 |  7639 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    191618 |  7640 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    127742 |  7641 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    127747 |  7642 | `			if( zDup ){` |
|    127747 |  7643 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     63871 |  7644 | `			}` |
|     63871 |  7645 | `		}` |
|    127939 |  7646 | `		SyBlobRelease(&sBlob);` |
|     63967 |  7647 | `	}` |
|         - |  7648 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7649 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7650 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7651 | `	{` |
|    127939 |  7652 | `		int nNonNull = 0;` |
|    127939 |  7653 | `		int iNonNullIdx = -1;` |
|         - |  7654 | `		int i;` |
|    256025 |  7655 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128091 |  7656 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    128061 |  7657 | `				nNonNull++;` |
|    128061 |  7658 | `				iNonNullIdx = i;` |
|     64028 |  7659 | `			}` |
|     64048 |  7660 | `		}` |
|    127939 |  7661 | `		if( nNonNull <= 1 ){` |
|         - |  7662 | `			/* Fast path: store as single type. */` |
|    127833 |  7663 | `			if( iNonNullIdx >= 0 ){` |
|    127827 |  7664 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    127827 |  7665 | `				if( pA->nType == SXU32_HIGH ){` |
|     57788 |  7666 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19261 |  7667 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38527 |  7668 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38527 |  7669 | `					*pnType = SXU32_HIGH;` |
|     38527 |  7670 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    108566 |  7671 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       181 |  7672 | `					*pnType = MEMOBJ_VOID;` |
|     89217 |  7673 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7674 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7675 | `				}else{` |
|     89113 |  7676 | `					*pnType = pA->nType;` |
|         - |  7677 | `				}` |
|     63911 |  7678 | `			}` |
|     63919 |  7679 | `		}else{` |
|         - |  7680 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7681 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7682 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7683 | `				ph7_type_alt sAlt;` |
|       249 |  7684 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7685 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7686 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7687 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7688 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7689 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7690 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7691 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7692 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7693 | `				}else{` |
|       145 |  7694 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7695 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7696 | `				}` |
|       239 |  7697 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7698 | `			}` |
|         - |  7699 | `		}` |
|         - |  7700 | `	}` |
|    127939 |  7701 | `	return SXRET_OK;` |
|     63980 |  7702 | `}` |
|         - |  7703 |  |
|         - |  7704 | `/*` |
|         - |  7705 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7706 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7707 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7708 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7709 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7710 | `` *          and union types `: T\|U`.`` |
|         - |  7711 | ` */` |
|   2891522 |  7712 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7713 | `{` |
|   2891527 |  7714 | `	sxi32 iFlags = 0;` |
|         - |  7715 | `	sxi32 rc;` |
|         - |  7716 | `	sxu32 nLine;` |
|   2891527 |  7717 | `	pFunc->nReturnType = 0;` |
|   2891527 |  7718 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2891527 |  7719 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7720 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7721 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7722 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7723 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7724 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2891527 |  7725 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2891527 |  7726 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2891527 |  7727 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2879235 |  7728 | `		return SXRET_OK;` |
|         - |  7729 | `	}` |
|     12297 |  7730 | `	pGen->pIn++; /* Skip ':' */` |
|     12297 |  7731 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7732 | `		return SXRET_OK;` |
|         - |  7733 | `	}` |
|     12297 |  7734 | `	nLine = pGen->pIn->nLine;` |
|     12297 |  7735 | `	rc = GenStateParseUnionTypeDecl(` |
|      6146 |  7736 | `		pGen,` |
|      6146 |  7737 | `		&pFunc->nReturnType,` |
|      6146 |  7738 | `		&pFunc->sReturnClass,` |
|      6146 |  7739 | `		&pFunc->aReturnUnion,` |
|         - |  7740 | `		&iFlags,` |
|      6146 |  7741 | `		&pFunc->sReturnTypeName,` |
|         - |  7742 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7743 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7744 | `		/* iUnionFlag */ 0,` |
|         - |  7745 | `		/* bAllowVoid */ 1,` |
|      6146 |  7746 | `		nLine);` |
|     12297 |  7747 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7748 | `		return SXERR_ABORT;` |
|         - |  7749 | `	}` |
|     12297 |  7750 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7751 | `		/* Error already reported */` |
|       ! 0 |  7752 | `		return SXERR_SYNTAX;` |
|         - |  7753 | `	}` |
|     12297 |  7754 | `	if( rc == SXERR_SYNTAX ){` |
|         9 |  7755 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 |  7756 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7757 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7758 | `				&pGen->pIn->sData);` |
|         6 |  7759 | `		}else{` |
|       ! 0 |  7760 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7761 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7762 | `		}` |
|         9 |  7763 | `		return SXERR_SYNTAX;` |
|         - |  7764 | `	}` |
|     12291 |  7765 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12291 |  7766 | `	return SXRET_OK;` |
|   1445766 |  7767 | `}` |
|         - |  7768 |  |
|    491590 |  7769 | `static sxi32 GenStateCompileFunc(` |
|         - |  7770 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7771 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7772 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7773 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7774 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7775 | `	)` |
|         5 |  7776 | `{` |
|         - |  7777 | `	ph7_vm_func *pFunc;` |
|         - |  7778 | `	SyToken *pEnd;` |
|         - |  7779 | `	sxu32 nLine;` |
|         - |  7780 | `	char *zName;` |
|         - |  7781 | `	sxi32 rc;` |
|         - |  7782 | `	/* Extract line number */` |
|    491595 |  7783 | `	nLine = pGen->pIn->nLine;` |
|         - |  7784 | `	/* Jump the left parenthesis '(' */` |
|    491595 |  7785 | `	pGen->pIn++;` |
|         - |  7786 | `	/* Delimit the function signature */` |
|    491595 |  7787 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    491595 |  7788 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7789 | `		/* Syntax error */` |
|         9 |  7790 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7791 | `		(void)pName;` |
|         9 |  7792 | `		if( rc == SXERR_ABORT ){` |
|         - |  7793 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7794 | `			return SXERR_ABORT;` |
|         - |  7795 | `		}` |
|         9 |  7796 | `		pGen->pIn = pGen->pEnd;` |
|         9 |  7797 | `		return SXRET_OK;` |
|         - |  7798 | `	}` |
|         - |  7799 | `	/* Create the function state */` |
|    491589 |  7800 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    491589 |  7801 | `	if( pFunc == 0 ){` |
|       ! 0 |  7802 | `		goto OutOfMem;` |
|         - |  7803 | `	}` |
|         - |  7804 | `	/* Build the function name, prepending namespace if active */` |
|    491597 |  7805 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7806 | `		SyBlob sFQN;` |
|         - |  7807 | `		sxu32 nLen;` |
|        18 |  7808 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        18 |  7809 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        18 |  7810 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        18 |  7811 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        18 |  7812 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        18 |  7813 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        18 |  7814 | `		SyBlobRelease(&sFQN);` |
|        18 |  7815 | `		if( zName == 0 ){` |
|       ! 0 |  7816 | `			goto OutOfMem;` |
|         - |  7817 | `		}` |
|        18 |  7818 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        10 |  7819 | `	}else{` |
|    491573 |  7820 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    491573 |  7821 | `		if( zName == 0 ){` |
|       ! 0 |  7822 | `			goto OutOfMem;` |
|         - |  7823 | `		}` |
|    491573 |  7824 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7825 | `	}` |
|         - |  7826 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7827 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    491589 |  7828 | `	pFunc->nLine = nLine;` |
|    491589 |  7829 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    491589 |  7830 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7831 | `		return SXERR_ABORT;` |
|         - |  7832 | `	}` |
|    491589 |  7833 | `	if( pGen->pIn < pEnd ){` |
|         - |  7834 | `		/* Collect function arguments */` |
|    425619 |  7835 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    425619 |  7836 | `		if( rc == SXERR_ABORT ){` |
|         - |  7837 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7838 | `			return SXERR_ABORT;` |
|         - |  7839 | `		}` |
|    212807 |  7840 | `	}` |
|         - |  7841 | `	/* Point past ')' and parse optional return type ': type' */` |
|    491589 |  7842 | `	pGen->pIn = &pEnd[1];` |
|         - |  7843 | `	{` |
|    491589 |  7844 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    491589 |  7845 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7846 | `			return SXERR_ABORT;` |
|    491589 |  7847 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 |  7848 | `			return SXERR_SYNTAX;` |
|         - |  7849 | `		}` |
|         - |  7850 | `	}` |
|    491583 |  7851 | `	if( bHandleClosure ){` |
|         - |  7852 | `		ph7_vm_func_closure_env sEnv;` |
|       581 |  7853 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       576 |  7854 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       337 |  7855 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        93 |  7856 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7857 | `				/* Closure,record environment variable */` |
|        93 |  7858 | `				pGen->pIn++;` |
|        93 |  7859 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7860 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7861 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7862 | `						return SXERR_ABORT;` |
|         - |  7863 | `					}` |
|       ! 0 |  7864 | `				}` |
|        93 |  7865 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7866 | `				/* Compile until we hit the first closing parenthesis */` |
|       191 |  7867 | `				while( pGen->pIn < pGen->pEnd ){` |
|       191 |  7868 | `					int iFlagsLocal = 0;` |
|       191 |  7869 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        93 |  7870 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        93 |  7871 | `						break;` |
|         - |  7872 | `					}` |
|       103 |  7873 | `					nLineLocal = pGen->pIn->nLine;` |
|       103 |  7874 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7875 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7876 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7877 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7878 | `						pGen->pIn++;` |
|        27 |  7879 | `					}` |
|        98 |  7880 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       103 |  7881 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7882 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7883 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7884 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7885 | `								return SXERR_ABORT;` |
|         - |  7886 | `							}` |
|         - |  7887 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7888 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7889 | `								pGen->pIn++;` |
|       ! 0 |  7890 | `							}` |
|       ! 0 |  7891 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7892 | `								pGen->pIn++;` |
|       ! 0 |  7893 | `							}` |
|       ! 0 |  7894 | `							break;` |
|         - |  7895 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7896 | `					}else{` |
|         - |  7897 | `						SyString *pNameLocal;` |
|         - |  7898 | `						char *zDup;` |
|         - |  7899 | `						/* Duplicate variable name */` |
|       103 |  7900 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       103 |  7901 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       103 |  7902 | `						if( zDup ){` |
|         - |  7903 | `							/* Zero the structure */` |
|       103 |  7904 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       103 |  7905 | `							sEnv.iFlags = iFlagsLocal;` |
|       103 |  7906 | `							sEnv.nIdx = SXU32_HIGH;` |
|       103 |  7907 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       103 |  7908 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       118 |  7909 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7910 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7911 | `									got_this = 1;` |
|       ! 0 |  7912 | `							}` |
|         - |  7913 | `							/* Save imported variable */` |
|       103 |  7914 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        54 |  7915 | `						}else{` |
|       ! 0 |  7916 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7917 | `							 return SXERR_ABORT;` |
|         - |  7918 | `						}` |
|         - |  7919 | `					}` |
|       103 |  7920 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       115 |  7921 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7922 | `						/* Ignore trailing commas */` |
|        13 |  7923 | `						pGen->pIn++;` |
|         1 |  7924 | `					}` |
|         5 |  7925 | `				}` |
|         - |  7926 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7927 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7928 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7929 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7930 | `				 * legacy pre-use position. */` |
|        93 |  7931 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7932 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7933 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7934 | `						return SXERR_ABORT;` |
|         7 |  7935 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7936 | `						return SXERR_SYNTAX;` |
|         - |  7937 | `					}` |
|         3 |  7938 | `				}` |
|        44 |  7939 | `		}` |
|       581 |  7940 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7941 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7942 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7943 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7944 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7945 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7946 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7947 | `			 * closure never binds $this (php). */` |
|       571 |  7948 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       571 |  7949 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       571 |  7950 | `			sEnv.nIdx = SXU32_HIGH;` |
|       571 |  7951 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       571 |  7952 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       571 |  7953 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       283 |  7954 | `		}` |
|       581 |  7955 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7956 | `			/* Mark as closure */` |
|       573 |  7957 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       284 |  7958 | `		}` |
|       288 |  7959 | `	}` |
|         - |  7960 | `	/* Compile the body */` |
|    491583 |  7961 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    491583 |  7962 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7963 | `		return SXERR_ABORT;` |
|         - |  7964 | `	}` |
|         - |  7965 | `	/* The cursor sits just past the body's closing brace */` |
|    491583 |  7966 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    491583 |  7967 | `	if( ppFunc ){` |
|    491583 |  7968 | `		*ppFunc = pFunc;` |
|    245789 |  7969 | `	}` |
|    491583 |  7970 | `	rc = SXRET_OK;` |
|    491583 |  7971 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7972 | `		/* Finally register the function */` |
|    491015 |  7973 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    245505 |  7974 | `	}` |
|    491583 |  7975 | `	if( rc == SXRET_OK ){` |
|    491583 |  7976 | `		return SXRET_OK;` |
|         - |  7977 | `	}` |
|         - |  7978 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7979 | `OutOfMem:` |
|         - |  7980 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7981 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7982 | `	 */` |
|       ! 0 |  7983 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7984 | `	return SXERR_ABORT;` |
|    245800 |  7985 | `}` |
|         - |  7986 | `/*` |
|         - |  7987 | ` * Compile a standard PHP function.` |
|         - |  7988 | ` *  Refer to the block-comment above for more information.` |
|         - |  7989 | ` */` |
|    491022 |  7990 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7991 | `{` |
|         - |  7992 | `	SyString *pName;` |
|         - |  7993 | `	sxi32 iFlags;` |
|         - |  7994 | `	sxu32 nKwLine;` |
|         - |  7995 | `	sxu32 nLine;` |
|         - |  7996 | `	sxi32 rc;` |
|         - |  7997 |  |
|    491027 |  7998 | `	nLine = pGen->pIn->nLine;` |
|    491027 |  7999 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    491027 |  8000 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    491027 |  8001 | `	iFlags = 0;` |
|    491027 |  8002 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8003 | `		/* Return by reference,remember that */` |
|        12 |  8004 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8005 | `		/* Jump the '&' token */` |
|        12 |  8006 | `		pGen->pIn++;` |
|         5 |  8007 | `	}` |
|    491027 |  8008 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8009 | `		/* Invalid function name */` |
|         7 |  8010 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         7 |  8011 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8012 | `			return SXERR_ABORT;` |
|         - |  8013 | `		}` |
|         - |  8014 | `		/* Sychronize with the next semi-colon or braces*/` |
|        21 |  8015 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        15 |  8016 | `			pGen->pIn++;` |
|         1 |  8017 | `		}` |
|         7 |  8018 | `		return SXRET_OK;` |
|         - |  8019 | `	}` |
|    491021 |  8020 | `	pName = &pGen->pIn->sData;` |
|    491021 |  8021 | `	nLine = pGen->pIn->nLine;` |
|         - |  8022 | `	/* Jump the function name */` |
|    491021 |  8023 | `	pGen->pIn++;` |
|    491021 |  8024 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8025 | `		/* Syntax error */` |
|         3 |  8026 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  8027 | `		if( rc == SXERR_ABORT ){` |
|         - |  8028 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8029 | `			return SXERR_ABORT;` |
|         - |  8030 | `		}` |
|         - |  8031 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  8032 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  8033 | `			pGen->pIn++;` |
|       ! 0 |  8034 | `		}` |
|         3 |  8035 | `		return SXRET_OK;` |
|         - |  8036 | `	}` |
|         - |  8037 | `	/* Compile function body */` |
|         - |  8038 | `	{` |
|    491019 |  8039 | `		ph7_vm_func *pFuncState = 0;` |
|    491019 |  8040 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    491019 |  8041 | `		if( pFuncState ){` |
|         - |  8042 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    491007 |  8043 | `			pFuncState->nLine = nKwLine;` |
|    245501 |  8044 | `		}` |
|         - |  8045 | `	}` |
|    491019 |  8046 | `	return rc;` |
|    245516 |  8047 | `}` |
|         - |  8048 | `/*` |
|         - |  8049 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  8050 | ` * According to the PHP language reference manual` |
|         - |  8051 | ` *  Visibility:` |
|         - |  8052 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  8053 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  8054 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  8055 | ` *  Members declared protected can be accessed only within the class` |
|         - |  8056 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  8057 | ` *  may only be accessed by the class that defines the member.` |
|         - |  8058 | ` */` |
|   3150166 |  8059 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  8060 | `{` |
|   3150171 |  8061 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    256423 |  8062 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2893753 |  8063 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    191301 |  8064 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  8065 | `	}` |
|         - |  8066 | `	/* Assume public by default */` |
|   2702457 |  8067 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1575088 |  8068 | `}` |
|         - |  8069 | `/*` |
|         - |  8070 | ` * Compile a class constant.` |
|         - |  8071 | ` * According to the PHP language reference manual` |
|         - |  8072 | ` *  Class Constants` |
|         - |  8073 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  8074 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  8075 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  8076 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  8077 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  8078 | ` *   It's also possible for interfaces to have constants.` |
|         - |  8079 | ` * Symisc eXtension.` |
|         - |  8080 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  8081 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8082 | ` *  Example:` |
|         - |  8083 | ` *   class Test{` |
|         - |  8084 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8085 | ` *   };` |
|         - |  8086 | ` *   var_dump(TEST::MyConst);` |
|         - |  8087 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8088 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8089 | ` */` |
|         - |  8090 | `/*` |
|         - |  8091 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  8092 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  8093 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  8094 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  8095 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  8096 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  8097 | ` */` |
|    290810 |  8098 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  8099 | `{` |
|         - |  8100 | `	SyToken *p0, *p1;` |
|    290815 |  8101 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8102 | `		return 0;` |
|         - |  8103 | `	}` |
|    290815 |  8104 | `	p0 = pGen->pIn;` |
|         - |  8105 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    290815 |  8106 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  8107 | `		return 1;` |
|         - |  8108 | `	}` |
|    290815 |  8109 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  8110 | `		return 1;` |
|         - |  8111 | `	}` |
|         - |  8112 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  8113 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  8114 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    290811 |  8115 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    290811 |  8116 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    290811 |  8117 | `		if( p1 ){` |
|    290811 |  8118 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  8119 | `				return 1;` |
|         - |  8120 | `			}` |
|    290781 |  8121 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  8122 | `				return 1;` |
|         - |  8123 | `			}` |
|    145386 |  8124 | `		}` |
|    145386 |  8125 | `	}` |
|    290777 |  8126 | `	return 0;` |
|    145410 |  8127 | `}` |
|         - |  8128 | `/*` |
|         - |  8129 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  8130 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  8131 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  8132 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  8133 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  8134 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  8135 | ` * Peek only; never consumes tokens.` |
|         - |  8136 | ` */` |
|        24 |  8137 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  8138 | `{` |
|        28 |  8139 | `	SyToken *p = pGen->pIn;` |
|        39 |  8140 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  8141 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  8142 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  8143 | `	}` |
|        28 |  8144 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  8145 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  8146 | `	}` |
|         6 |  8147 | `	p++;` |
|         - |  8148 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  8149 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  8150 | `}` |
|         - |  8151 | `/*` |
|         - |  8152 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  8153 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  8154 | `` * `$o->new`), not a `new` expression.`` |
|         - |  8155 | ` */` |
|       110 |  8156 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  8157 | `{` |
|         - |  8158 | `	sxi32 iOp;` |
|       114 |  8159 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  8160 | `		return 0;` |
|         - |  8161 | `	}` |
|       104 |  8162 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  8163 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  8164 | `}` |
|         - |  8165 | `/*` |
|         - |  8166 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  8167 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  8168 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  8169 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  8170 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  8171 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  8172 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  8173 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  8174 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  8175 | ` *` |
|         - |  8176 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  8177 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  8178 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  8179 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  8180 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  8181 | ` */` |
|    628040 |  8182 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  8183 | `{` |
|    628045 |  8184 | `	SyToken *p = pGen->pIn;` |
|    628045 |  8185 | `	int iDepth = 0;` |
|   1659279 |  8186 | `	while( p < pGen->pEnd ){` |
|   1659279 |  8187 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    627993 |  8188 | `			break; /* end of this initializer */` |
|         - |  8189 | `		}` |
|   1031286 |  8190 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    519489 |  8191 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7682 |  8192 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  8193 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  8194 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  8195 | `			 * expression. */` |
|         3 |  8196 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  8197 | `			p++;` |
|         3 |  8198 | `			if( bArrow ){` |
|         - |  8199 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  8200 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  8201 | `				int iBase = iDepth;` |
|        17 |  8202 | `				while( p < pGen->pEnd ){` |
|        17 |  8203 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  8204 | `						iDepth++;` |
|        15 |  8205 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  8206 | `						if( iDepth <= iBase ){` |
|       ! 0 |  8207 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  8208 | `						}` |
|         5 |  8209 | `						iDepth--;` |
|        11 |  8210 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  8211 | `						break;` |
|         - |  8212 | `					}` |
|        15 |  8213 | `					p++;` |
|         1 |  8214 | `				}` |
|         2 |  8215 | `			}else{` |
|         - |  8216 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  8217 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  8218 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  8219 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  8220 | `				int iLocal = 0;` |
|       ! 0 |  8221 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  8222 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  8223 | `						break; /* body brace */` |
|         - |  8224 | `					}` |
|       ! 0 |  8225 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  8226 | `						iLocal++;` |
|       ! 0 |  8227 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  8228 | `						if( iLocal > 0 ){` |
|       ! 0 |  8229 | `							iLocal--;` |
|       ! 0 |  8230 | `						}` |
|       ! 0 |  8231 | `					}` |
|       ! 0 |  8232 | `					p++;` |
|       ! 0 |  8233 | `				}` |
|       ! 0 |  8234 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  8235 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  8236 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  8237 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  8238 | `							iBrace++;` |
|       ! 0 |  8239 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  8240 | `							iBrace--;` |
|       ! 0 |  8241 | `							if( iBrace == 0 ){` |
|       ! 0 |  8242 | `								p++;` |
|       ! 0 |  8243 | `								break;` |
|         - |  8244 | `							}` |
|       ! 0 |  8245 | `						}` |
|       ! 0 |  8246 | `						p++;` |
|       ! 0 |  8247 | `					}` |
|       ! 0 |  8248 | `				}` |
|         - |  8249 | `			}` |
|         3 |  8250 | `			continue;` |
|         - |  8251 | `		}` |
|   1031289 |  8252 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  8253 | `			if( iDepth == 0 ){` |
|         - |  8254 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  8255 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  8256 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  8257 | `				 * is legal — don't scan into it. */` |
|        45 |  8258 | `				break;` |
|         - |  8259 | `			}` |
|       ! 0 |  8260 | `			iDepth++;` |
|   1031245 |  8261 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42161 |  8262 | `			iDepth++;` |
|   1010167 |  8263 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     42159 |  8264 | `			if( iDepth > 0 ){` |
|     42159 |  8265 | `				iDepth--;` |
|     21077 |  8266 | `			}` |
|    968012 |  8267 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    347161 |  8268 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8269 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8270 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8271 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  8272 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  8273 | `				return 1;` |
|         - |  8274 | `			}` |
|       ! 0 |  8275 | `		}` |
|   1031237 |  8276 | `		p++;` |
|         5 |  8277 | `	}` |
|    628037 |  8278 | `	return 0;` |
|    314025 |  8279 | `}` |
|         - |  8280 | `/*` |
|         - |  8281 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8282 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8283 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8284 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8285 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8286 | ` * share the same backing.` |
|         - |  8287 | ` */` |
|       372 |  8288 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8289 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8290 | `{` |
|       377 |  8291 | `	pAttr->nType = nType;` |
|       377 |  8292 | `	pAttr->sClass = *pClass;` |
|       377 |  8293 | `	pAttr->sTypeName = *pTypeName;` |
|       377 |  8294 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8295 | `		sxu32 i;` |
|        73 |  8296 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8297 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8298 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8299 | `		}` |
|        11 |  8300 | `	}` |
|       377 |  8301 | `}` |
|    290810 |  8302 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8303 | `{` |
|    290815 |  8304 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8305 | `	SySet *pInstrContainer;` |
|         - |  8306 | `	ph7_class_attr *pCons;` |
|         - |  8307 | `	SyString *pName;` |
|         - |  8308 | `	sxi32 rc;` |
|    290815 |  8309 | `	sxu32 nType = 0;` |
|         - |  8310 | `	SyString sTypeClass;` |
|         - |  8311 | `	SyString sTypeText;` |
|         - |  8312 | `	SySet aUnionAlts;` |
|    290815 |  8313 | `	sxi32 iTypeFlags = 0;` |
|    290815 |  8314 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    290815 |  8315 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    290815 |  8316 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8317 | `	/* Extract visibility level */` |
|    290815 |  8318 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8319 | `	/* Mark as constant */` |
|    290815 |  8320 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    290815 |  8321 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8322 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8323 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    290834 |  8324 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8325 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8326 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8327 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8328 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8329 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8330 | `		 * and success paths release. */` |
|        42 |  8331 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8332 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8333 | `			goto Synchronize;` |
|        42 |  8334 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8335 | `			return SXERR_ABORT;` |
|        42 |  8336 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8337 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8338 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8339 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8340 | `				return SXERR_ABORT;` |
|         - |  8341 | `			}` |
|       ! 0 |  8342 | `			goto Synchronize;` |
|         - |  8343 | `		}` |
|        42 |  8344 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8345 | `	}` |
|    145405 |  8346 | `loop:` |
|    290817 |  8347 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8348 | `		/* Invalid constant name */` |
|       ! 0 |  8349 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8350 | `		if( rc == SXERR_ABORT ){` |
|         - |  8351 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8352 | `			return SXERR_ABORT;` |
|         - |  8353 | `		}` |
|       ! 0 |  8354 | `		goto Synchronize;` |
|         - |  8355 | `	}` |
|         - |  8356 | `	/* Peek constant name */` |
|    290817 |  8357 | `	pName = &pGen->pIn->sData;` |
|         - |  8358 | `	/* Make sure the constant name isn't reserved */` |
|    290817 |  8359 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8360 | `		/* Reserved constant name */` |
|       ! 0 |  8361 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8362 | `		if( rc == SXERR_ABORT ){` |
|         - |  8363 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8364 | `			return SXERR_ABORT;` |
|         - |  8365 | `		}` |
|       ! 0 |  8366 | `		goto Synchronize;` |
|         - |  8367 | `	}` |
|         - |  8368 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    290817 |  8369 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8370 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8371 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8372 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8373 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8374 | `			return SXERR_ABORT;` |
|        42 |  8375 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8376 | `			goto Synchronize;` |
|         - |  8377 | `		}` |
|        18 |  8378 | `	}` |
|         - |  8379 | `	/* Advance the stream cursor */` |
|    290815 |  8380 | `	pGen->pIn++;` |
|    290815 |  8381 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8382 | `		/* Invalid declaration */` |
|       ! 0 |  8383 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8384 | `		if( rc == SXERR_ABORT ){` |
|         - |  8385 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8386 | `			return SXERR_ABORT;` |
|         - |  8387 | `		}` |
|       ! 0 |  8388 | `		goto Synchronize;` |
|         - |  8389 | `	}` |
|    290815 |  8390 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8391 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8392 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8393 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8394 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    290810 |  8395 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8396 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8397 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8398 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8399 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8400 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8401 | `			return SXERR_ABORT;` |
|         - |  8402 | `		}` |
|         6 |  8403 | `		goto Synchronize;` |
|         - |  8404 | `	}` |
|         - |  8405 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8406 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8407 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    290811 |  8408 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8409 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8410 | `			"New expressions are not supported in this context");` |
|         5 |  8411 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8412 | `			return SXERR_ABORT;` |
|         - |  8413 | `		}` |
|         5 |  8414 | `		goto Synchronize;` |
|         - |  8415 | `	}` |
|         - |  8416 | `	/* Allocate a new class attribute */` |
|    290807 |  8417 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    290807 |  8418 | `	if( pCons ){` |
|    290807 |  8419 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    290807 |  8420 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8421 | `			return SXERR_ABORT;` |
|         - |  8422 | `		}` |
|    145401 |  8423 | `	}` |
|    290807 |  8424 | `	if( pCons == 0 ){` |
|       ! 0 |  8425 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8426 | `		return SXERR_ABORT;` |
|         - |  8427 | `	}` |
|    290807 |  8428 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8429 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8430 | `	}` |
|         - |  8431 | `	/* Swap bytecode container */` |
|    290807 |  8432 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    290807 |  8433 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8434 | `	/* Compile constant value.` |
|         - |  8435 | `	 */` |
|    290807 |  8436 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    290807 |  8437 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8438 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8439 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8440 | `			return SXERR_ABORT;` |
|         - |  8441 | `		}` |
|         1 |  8442 | `	}` |
|         - |  8443 | `	/* Emit the done instruction */` |
|    290807 |  8444 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    290807 |  8445 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    290807 |  8446 | `	if( rc == SXERR_ABORT ){` |
|         - |  8447 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8448 | `		return SXERR_ABORT;` |
|         - |  8449 | `	}` |
|         - |  8450 | `	/* All done,install the constant */` |
|    290807 |  8451 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    290807 |  8452 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8453 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8454 | `		return SXERR_ABORT;` |
|         - |  8455 | `	}` |
|    290807 |  8456 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8457 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8458 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8459 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8460 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8461 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8462 | `				pTok--;` |
|       ! 0 |  8463 | `			}` |
|       ! 0 |  8464 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8465 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8466 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8467 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8468 | `				return SXERR_ABORT;` |
|         - |  8469 | `			}` |
|       ! 0 |  8470 | `		}else{` |
|         3 |  8471 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8472 | `				goto loop;` |
|         - |  8473 | `			}` |
|         - |  8474 | `		}` |
|       ! 0 |  8475 | `	}` |
|    290805 |  8476 | `	SySetRelease(&aUnionAlts);` |
|    290805 |  8477 | `	return SXRET_OK;` |
|         5 |  8478 | `Synchronize:` |
|        13 |  8479 | `	SySetRelease(&aUnionAlts);` |
|         - |  8480 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8481 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8482 | `		pGen->pIn++;` |
|         3 |  8483 | `	}` |
|        13 |  8484 | `	return SXERR_CORRUPT;` |
|    145410 |  8485 | `}` |
|         - |  8486 | `/*` |
|         - |  8487 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8488 | ` * According to the PHP language reference manual` |
|         - |  8489 | ` *  Properties` |
|         - |  8490 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8491 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8492 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8493 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8494 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8495 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8496 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8497 | ` * Symisc eXtension.` |
|         - |  8498 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8499 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8500 | ` *  Example:` |
|         - |  8501 | ` *   class Test{` |
|         - |  8502 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8503 | ` *   };` |
|         - |  8504 | ` *   var_dump(TEST::myVar);` |
|         - |  8505 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8506 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8507 | ` */` |
|         - |  8508 | `/*` |
|         - |  8509 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8510 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8511 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8512 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8513 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8514 | ` */` |
|   2353644 |  8515 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8516 | `{` |
|   2353649 |  8517 | `	SyToken *p = pStart;` |
|   2353649 |  8518 | `	int bFirst = 1;` |
|   2353649 |  8519 | `	if( p >= pEnd ) return 0;` |
|         - |  8520 | ``	/* Optional nullable `?` shorthand. */`` |
|   2353649 |  8521 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        39 |  8522 | `		p++;` |
|        39 |  8523 | `		if( p >= pEnd ) return 0;` |
|        18 |  8524 | `	}` |
|         - |  8525 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8526 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8527 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8528 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1176822 |  8529 | `	for(;;){` |
|   2353669 |  8530 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8531 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8532 | `			p++;` |
|         9 |  8533 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8534 | `			if( p >= pEnd ) return 0;` |
|         3 |  8535 | `			p++; /* skip ')' */` |
|         2 |  8536 | `		}else{` |
|         - |  8537 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8538 | ``			 * then any `&`-joined intersection members. */`` |
|   2353667 |  8539 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2353667 |  8540 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8541 | `				return 0;` |
|         - |  8542 | `			}` |
|         - |  8543 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8544 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8545 | `			 * may still appear at the initial dispatch site). */` |
|   2353667 |  8546 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2353617 |  8547 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2353612 |  8548 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    103722 |  8549 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2353315 |  8550 | `					return 0;` |
|         - |  8551 | `				}` |
|       151 |  8552 | `			}` |
|       357 |  8553 | `			p++;` |
|       359 |  8554 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8555 | `				p += 2;` |
|         1 |  8556 | `			}` |
|       531 |  8557 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       360 |  8558 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8559 | `				p++; /* skip '&' */` |
|         3 |  8560 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8561 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8562 | `				p++;` |
|         3 |  8563 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8564 | `					p += 2;` |
|       ! 0 |  8565 | `				}` |
|         1 |  8566 | `			}` |
|         - |  8567 | `		}` |
|       359 |  8568 | `		bFirst = 0;` |
|       354 |  8569 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8570 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8571 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8572 | `			continue;` |
|         - |  8573 | `		}` |
|       339 |  8574 | `		break;` |
|       ! 0 |  8575 | `	}` |
|       339 |  8576 | `	if( p >= pEnd ) return 0;` |
|       339 |  8577 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1176827 |  8578 | `}` |
|         - |  8579 |  |
|         - |  8580 | `/*` |
|         - |  8581 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8582 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8583 | ` * if not). Recognized forms:` |
|         - |  8584 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8585 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8586 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8587 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8588 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8589 | ` * on unrecoverable error.` |
|         - |  8590 | ` *` |
|         - |  8591 | ` * When a type is parsed:` |
|         - |  8592 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8593 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8594 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8595 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8596 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8597 | ` */` |
|       344 |  8598 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8599 | `	ph7_gen_state *pGen,` |
|         - |  8600 | `	sxu32 *pnType,` |
|         - |  8601 | `	SyString *pClass,` |
|         - |  8602 | `	sxi32 *piTypeFlags,` |
|         - |  8603 | `	SyString *pTypeText,` |
|         - |  8604 | `	SySet *pAlts` |
|         5 |  8605 | `){` |
|       349 |  8606 | `	sxi32 iFlags = 0;` |
|         - |  8607 | `	sxi32 rc;` |
|       349 |  8608 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8609 | `		return SXRET_OK;` |
|         - |  8610 | `	}` |
|         - |  8611 | `	/* If the first token is '$', there's no type */` |
|       349 |  8612 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8613 | `		return SXRET_OK;` |
|         - |  8614 | `	}` |
|       349 |  8615 | `	rc = GenStateParseUnionTypeDecl(` |
|       172 |  8616 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8617 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8618 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8619 | `		/* bAllowVoid */ 0,` |
|       344 |  8620 | `		pGen->pIn->nLine);` |
|       349 |  8621 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8622 | `		return rc;` |
|         - |  8623 | `	}` |
|         - |  8624 | `	/* Verify next token is '$' (start of property name) */` |
|       349 |  8625 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8626 | `		return SXERR_SYNTAX;` |
|         - |  8627 | `	}` |
|       349 |  8628 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       349 |  8629 | `	return SXRET_OK;` |
|       177 |  8630 | `}` |
|         - |  8631 |  |
|         - |  8632 | `/*` |
|         - |  8633 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8634 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8635 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8636 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8637 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8638 | ` * by the type parser itself before reaching here.` |
|         - |  8639 | ` *` |
|         - |  8640 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8641 | ` * use in the error message.` |
|         - |  8642 | ` */` |
|       522 |  8643 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8644 | `	sxu32 nType,` |
|         - |  8645 | `	const SyString *pClass,` |
|         - |  8646 | `	const char **pzName,` |
|         - |  8647 | `	sxu32 *pnName)` |
|         5 |  8648 | `{` |
|         - |  8649 | `	const char *z;` |
|         - |  8650 | `	sxu32 n;` |
|       527 |  8651 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       467 |  8652 | `		return 0;` |
|         - |  8653 | `	}` |
|        64 |  8654 | `	z = pClass->zString;` |
|        64 |  8655 | `	n = pClass->nByte;` |
|        64 |  8656 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8657 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8658 | `	}` |
|         - |  8659 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8660 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8661 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        58 |  8662 | `	return 0;` |
|       266 |  8663 | `}` |
|         - |  8664 |  |
|         - |  8665 | `/*` |
|         - |  8666 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8667 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8668 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8669 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8670 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8671 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8672 | ` *` |
|         - |  8673 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8674 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8675 | ` */` |
|       460 |  8676 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8677 | `	ph7_gen_state *pGen,` |
|         - |  8678 | `	ph7_class *pClass,` |
|         - |  8679 | `	const SyString *pMemberName,` |
|         - |  8680 | `	sxu32 nType,` |
|         - |  8681 | `	const SyString *pTypeClass,` |
|         - |  8682 | `	const SyString *pTypeText,` |
|         - |  8683 | `	SySet *pUnionAlts,` |
|         - |  8684 | `	const char *zErrFmt,` |
|         - |  8685 | `	sxu32 nLine)` |
|         5 |  8686 | `{` |
|       465 |  8687 | `	const char *zBad = 0;` |
|       465 |  8688 | `	sxu32 nBad = 0;` |
|         - |  8689 | `	SyString sFallback;` |
|         - |  8690 | `	const SyString *pBad;` |
|         - |  8691 | `	sxi32 rc;` |
|       465 |  8692 | `	int bDisallowed = 0;` |
|       465 |  8693 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8694 | `		bDisallowed = 1;` |
|       463 |  8695 | `	}else if( pUnionAlts ){` |
|         - |  8696 | `		sxu32 i;` |
|        95 |  8697 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8698 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8699 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8700 | `				bDisallowed = 1;` |
|         3 |  8701 | `				break;` |
|         - |  8702 | `			}` |
|        35 |  8703 | `		}` |
|        15 |  8704 | `	}` |
|       465 |  8705 | `	if( !bDisallowed ){` |
|       459 |  8706 | `		return SXRET_OK;` |
|         - |  8707 | `	}` |
|         - |  8708 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8709 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8710 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8711 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8712 | `		pBad = pTypeText;` |
|         5 |  8713 | `	}else{` |
|       ! 0 |  8714 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8715 | `		pBad = &sFallback;` |
|         - |  8716 | `	}` |
|        11 |  8717 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8718 | `		zErrFmt,` |
|         3 |  8719 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8720 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8721 | `		return SXERR_ABORT;` |
|         - |  8722 | `	}` |
|         8 |  8723 | `	return SXERR_SYNTAX;` |
|       235 |  8724 | `}` |
|         - |  8725 | `/*` |
|         - |  8726 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8727 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8728 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8729 | ` * than promoted to a lexer keyword.` |
|         - |  8730 | ` */` |
|  20511454 |  8731 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8732 | `{` |
|  20730215 |  8733 | `	return (pTok->nType & PH7_TK_ID)` |
|  10474483 |  8734 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  20730210 |  8735 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8736 | `}` |
|         - |  8737 | `/*` |
|         - |  8738 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8739 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8740 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8741 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8742 | ` */` |
|   7292526 |  8743 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8744 | `{` |
|   7292531 |  8745 | `	*pnTok = 0;` |
|   7292526 |  8746 | `	if( &pTok[3] < pEnd` |
|   6836558 |  8747 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5626385 |  8748 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2436098 |  8749 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8750 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8751 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8752 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8753 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8754 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8755 | `			*pnTok = 4;` |
|        17 |  8756 | `			return nKw;` |
|         - |  8757 | `		}` |
|       ! 0 |  8758 | `	}` |
|   7292515 |  8759 | `	return 0;` |
|   3646268 |  8760 | `}` |
|         - |  8761 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8762 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8763 | `{` |
|        17 |  8764 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8765 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8766 | `	}` |
|         5 |  8767 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8768 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8769 | `	}` |
|         3 |  8770 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8771 | `}` |
|    459906 |  8772 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8773 | `{` |
|    459911 |  8774 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8775 | `	ph7_class_attr *pAttr;` |
|         - |  8776 | `	SyString *pName;` |
|         - |  8777 | `	sxi32 rc;` |
|    459911 |  8778 | `	sxu32 nType = 0;` |
|         - |  8779 | `	SyString sTypeClass;` |
|         - |  8780 | `	SyString sTypeText;` |
|         - |  8781 | `	SySet aUnionAlts;` |
|    459911 |  8782 | `	sxi32 iTypeFlags = 0;` |
|    459911 |  8783 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    459911 |  8784 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    459911 |  8785 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8786 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8787 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8788 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    459911 |  8789 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8790 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8791 | `	}` |
|         - |  8792 | `	/* Extract visibility level */` |
|    459911 |  8793 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8794 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    460083 |  8795 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       349 |  8796 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       349 |  8797 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8798 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8799 | `			goto Synchronize;` |
|       349 |  8800 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8801 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8802 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8803 | `				&pGen->pIn->sData);` |
|       ! 0 |  8804 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8805 | `				return SXERR_ABORT;` |
|         - |  8806 | `			}` |
|       ! 0 |  8807 | `			goto Synchronize;` |
|       349 |  8808 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8809 | `			return SXERR_ABORT;` |
|         - |  8810 | `		}` |
|       172 |  8811 | `	}` |
|       ! 0 |  8812 | `loop:` |
|    459915 |  8813 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8814 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8815 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8816 | `			return SXERR_ABORT;` |
|         - |  8817 | `		}` |
|       ! 0 |  8818 | `		goto Synchronize;` |
|         - |  8819 | `	}` |
|    459915 |  8820 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    459915 |  8821 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8822 | `		/* Invalid attribute name */` |
|       ! 0 |  8823 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8824 | `		if( rc == SXERR_ABORT ){` |
|         - |  8825 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8826 | `			return SXERR_ABORT;` |
|         - |  8827 | `		}` |
|       ! 0 |  8828 | `		goto Synchronize;` |
|         - |  8829 | `	}` |
|         - |  8830 | `	/* Peek attribute name */` |
|    459915 |  8831 | `	pName = &pGen->pIn->sData;` |
|         - |  8832 | `	/* Advance the stream cursor */` |
|    459915 |  8833 | `	pGen->pIn++;` |
|    459915 |  8834 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8835 | `		/* Invalid declaration */` |
|         3 |  8836 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8837 | `		if( rc == SXERR_ABORT ){` |
|         - |  8838 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8839 | `			return SXERR_ABORT;` |
|         - |  8840 | `		}` |
|         3 |  8841 | `		goto Synchronize;` |
|         - |  8842 | `	}` |
|         - |  8843 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8844 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    459913 |  8845 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8846 | `		const char *zAvErr = 0;` |
|        19 |  8847 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8848 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8849 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8850 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8851 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8852 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8853 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8854 | `		}` |
|        13 |  8855 | `		if( zAvErr ){` |
|       ! 0 |  8856 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8857 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8858 | `				return SXERR_ABORT;` |
|         - |  8859 | `			}` |
|       ! 0 |  8860 | `			goto Synchronize;` |
|         - |  8861 | `		}` |
|         6 |  8862 | `	}` |
|         - |  8863 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8864 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    459913 |  8865 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        47 |  8866 | `		const char *zRoErr = 0;` |
|        47 |  8867 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8868 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        46 |  8869 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8870 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        43 |  8871 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8872 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8873 | `		}` |
|        47 |  8874 | `		if( zRoErr ){` |
|        13 |  8875 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8876 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8877 | `				return SXERR_ABORT;` |
|         - |  8878 | `			}` |
|        13 |  8879 | `			goto Synchronize;` |
|         - |  8880 | `		}` |
|        16 |  8881 | `	}` |
|         - |  8882 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8883 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8884 | `	 * by the type parser. */` |
|    459903 |  8885 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       518 |  8886 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8887 | `			&sTypeText,` |
|       342 |  8888 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       171 |  8889 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       347 |  8890 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8891 | `			return SXERR_ABORT;` |
|       347 |  8892 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8893 | `			goto Synchronize;` |
|         - |  8894 | `		}` |
|       171 |  8895 | `	}` |
|         - |  8896 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    459903 |  8897 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8898 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8899 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8900 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8901 | `			return SXERR_ABORT;` |
|         - |  8902 | `		}` |
|         3 |  8903 | `		goto Synchronize;` |
|         - |  8904 | `	}` |
|         - |  8905 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8906 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8907 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8908 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8909 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8910 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    459901 |  8911 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8912 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8913 | `			"New expressions are not supported in this context");` |
|         6 |  8914 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8915 | `			return SXERR_ABORT;` |
|         - |  8916 | `		}` |
|         6 |  8917 | `		goto Synchronize;` |
|         - |  8918 | `	}` |
|         - |  8919 | `	/* Allocate a new class attribute */` |
|    459897 |  8920 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    459897 |  8921 | `	if( pAttr ){` |
|    459897 |  8922 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    459897 |  8923 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8924 | `			return SXERR_ABORT;` |
|         - |  8925 | `		}` |
|    229946 |  8926 | `	}` |
|    459897 |  8927 | `	if( pAttr == 0 ){` |
|       ! 0 |  8928 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8929 | `		return SXERR_ABORT;` |
|         - |  8930 | `	}` |
|    459897 |  8931 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       345 |  8932 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       170 |  8933 | `	}` |
|    459897 |  8934 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8935 | `		SySet *pInstrContainer;` |
|    337235 |  8936 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    337235 |  8937 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8938 | `		{` |
|         - |  8939 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8940 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8941 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8942 | `			 * compiler would otherwise run into the hook tokens. */` |
|    337235 |  8943 | `			SyToken *pScan = pGen->pIn;` |
|    337235 |  8944 | `			sxi32 iNest = 0;` |
|    736347 |  8945 | `			while( pScan < pGen->pEnd ){` |
|    736347 |  8946 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42155 |  8947 | `					iNest++;` |
|    715272 |  8948 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     42155 |  8949 | `					iNest--;` |
|    673122 |  8950 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    337235 |  8951 | `					break;` |
|         - |  8952 | `				}` |
|    399117 |  8953 | `				pScan++;` |
|         5 |  8954 | `			}` |
|    337235 |  8955 | `			pGen->pEnd = pScan;` |
|         - |  8956 | `		}` |
|         - |  8957 | `		/* Swap bytecode container */` |
|    337235 |  8958 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    337235 |  8959 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8960 | `		/* Compile attribute value.` |
|         - |  8961 | `		 */` |
|    337235 |  8962 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    337235 |  8963 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8964 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8965 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8966 | `				return SXERR_ABORT;` |
|         - |  8967 | `			}` |
|       ! 0 |  8968 | `		}` |
|         - |  8969 | `		/* Emit the done instruction */` |
|    337235 |  8970 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    337235 |  8971 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    337235 |  8972 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    337235 |  8973 | `		pGen->pEnd = pSavedDefEnd;` |
|    168615 |  8974 | `	}` |
|         - |  8975 | `	/* All done,install the attribute */` |
|    459897 |  8976 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    459897 |  8977 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8978 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8979 | `		return SXERR_ABORT;` |
|         - |  8980 | `	}` |
|    459897 |  8981 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8982 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8983 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8984 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8985 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8986 | `			return SXERR_ABORT;` |
|         - |  8987 | `		}` |
|        95 |  8988 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8989 | `			goto Synchronize;` |
|         - |  8990 | `		}` |
|        95 |  8991 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8992 | `		return SXRET_OK;` |
|         - |  8993 | `	}` |
|    459803 |  8994 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8995 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8996 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8997 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8998 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8999 | `				? "Interfaces may only include hooked properties"` |
|         - |  9000 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  9001 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9002 | `			return SXERR_ABORT;` |
|         - |  9003 | `		}` |
|       ! 0 |  9004 | `		goto Synchronize;` |
|         - |  9005 | `	}` |
|    459803 |  9006 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  9007 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  9008 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  9009 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  9010 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  9011 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  9012 | `				pTok--;` |
|       ! 0 |  9013 | `			}` |
|       ! 0 |  9014 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9015 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  9016 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  9017 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9018 | `				return SXERR_ABORT;` |
|         - |  9019 | `			}` |
|       ! 0 |  9020 | `		}else{` |
|         5 |  9021 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  9022 | `				goto loop;` |
|         - |  9023 | `			}` |
|         - |  9024 | `		}` |
|       ! 0 |  9025 | `	}` |
|    459799 |  9026 | `	SySetRelease(&aUnionAlts);` |
|    459799 |  9027 | `	return SXRET_OK;` |
|         9 |  9028 | `Synchronize:` |
|         - |  9029 | `	/* Synchronize with the first semi-colon */` |
|        56 |  9030 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  9031 | `		pGen->pIn++;` |
|         3 |  9032 | `	}` |
|        22 |  9033 | `	SySetRelease(&aUnionAlts);` |
|        22 |  9034 | `	return SXERR_CORRUPT;` |
|    229958 |  9035 | `}` |
|         - |  9036 | `/*` |
|         - |  9037 | ` * Compile a class method.` |
|         - |  9038 | ` *` |
|         - |  9039 | ` * Refer to the official documentation for more information` |
|         - |  9040 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  9041 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  9042 | ` * overloading and many more.` |
|         - |  9043 | ` */` |
|   2399450 |  9044 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  9045 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  9046 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  9047 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  9048 | `	int doBody,          /* TRUE to process method body */` |
|         - |  9049 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  9050 | `	)` |
|         5 |  9051 | `{` |
|   2399455 |  9052 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2399455 |  9053 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  9054 | `	ph7_class_method *pMeth;` |
|         - |  9055 | `	sxi32 iFuncFlags;` |
|         - |  9056 | `	SyString *pName;` |
|         - |  9057 | `	SyToken *pEnd;` |
|         - |  9058 | `	sxi32 rc;` |
|         - |  9059 | `	/* Extract visibility level */` |
|   2399455 |  9060 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2399455 |  9061 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2399455 |  9062 | `	iFuncFlags = 0;` |
|   2399455 |  9063 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9064 | `		/* Invalid method name */` |
|       ! 0 |  9065 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9066 | `		if( rc == SXERR_ABORT ){` |
|         - |  9067 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9068 | `			return SXERR_ABORT;` |
|         - |  9069 | `		}` |
|       ! 0 |  9070 | `		goto Synchronize;` |
|         - |  9071 | `	}` |
|   2399455 |  9072 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  9073 | `		/* Return by reference,remember that */` |
|       ! 0 |  9074 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  9075 | `		/* Jump the '&' token */` |
|       ! 0 |  9076 | `		pGen->pIn++;` |
|       ! 0 |  9077 | `	}` |
|   2399455 |  9078 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  9079 | `		/* Invalid method name */` |
|       ! 0 |  9080 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9081 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9082 | `			return SXERR_ABORT;` |
|         - |  9083 | `		}` |
|       ! 0 |  9084 | `		goto Synchronize;` |
|         - |  9085 | `	}` |
|         - |  9086 | `	/* Peek method name */` |
|   2399455 |  9087 | `	pName = &pGen->pIn->sData;` |
|   2399455 |  9088 | `	nLine = pGen->pIn->nLine;` |
|         - |  9089 | `	/* Jump the method name */` |
|   2399455 |  9090 | `	pGen->pIn++;` |
|   2399455 |  9091 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  9092 | `		/* Abstract method */` |
|    137745 |  9093 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  9094 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9095 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  9096 | `				&pClass->sName,pName);` |
|       ! 0 |  9097 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9098 | `				return SXERR_ABORT;` |
|         - |  9099 | `			}` |
|       ! 0 |  9100 | `		}` |
|         - |  9101 | `		/* Assemble method signature only */` |
|    137745 |  9102 | `		doBody = FALSE;` |
|     68870 |  9103 | `	}` |
|   2399455 |  9104 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  9105 | `		/* Syntax error */` |
|       ! 0 |  9106 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  9107 | `		if( rc == SXERR_ABORT ){` |
|         - |  9108 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9109 | `			return SXERR_ABORT;` |
|         - |  9110 | `		}` |
|       ! 0 |  9111 | `		goto Synchronize;` |
|         - |  9112 | `	}` |
|         - |  9113 | `	/* Allocate a new class_method instance */` |
|   2399455 |  9114 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2399455 |  9115 | `	if( pMeth == 0 ){` |
|       ! 0 |  9116 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9117 | `		return SXERR_ABORT;` |
|         - |  9118 | `	}` |
|   2399455 |  9119 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2399455 |  9120 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2399455 |  9121 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9122 | `		return SXERR_ABORT;` |
|         - |  9123 | `	}` |
|         - |  9124 | `	/* Jump the left parenthesis '(' */` |
|   2399455 |  9125 | `	pGen->pIn++;` |
|   2399455 |  9126 | `	pEnd = 0; /* cc warning */` |
|         - |  9127 | `	/* Delimit the method signature */` |
|   2399455 |  9128 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2399455 |  9129 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9130 | `		/* Syntax error */` |
|         3 |  9131 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  9132 | `		if( rc == SXERR_ABORT ){` |
|         - |  9133 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9134 | `			return SXERR_ABORT;` |
|         - |  9135 | `		}` |
|         3 |  9136 | `		goto Synchronize;` |
|         - |  9137 | `	}` |
|         - |  9138 | `	{` |
|   2399453 |  9139 | `		int bIsCtor = 0;` |
|   2399453 |  9140 | `		int bAbstractCtor = 0;` |
|   2399448 |  9141 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1400648 |  9142 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2317128 |  9143 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    164655 |  9144 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  9145 | `				bAbstractCtor = 1;` |
|         2 |  9146 | `			}else{` |
|    164653 |  9147 | `				bIsCtor = 1;` |
|         - |  9148 | `			}` |
|     82325 |  9149 | `		}` |
|   2399453 |  9150 | `		if( pGen->pIn < pEnd ){` |
|         - |  9151 | `			/* Collect method arguments */` |
|    864953 |  9152 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    864953 |  9153 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9154 | `				return SXERR_ABORT;` |
|         - |  9155 | `			}` |
|    432474 |  9156 | `		}` |
|         - |  9157 | `	}` |
|         - |  9158 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2399453 |  9159 | `	pGen->pIn = &pEnd[1];` |
|         - |  9160 | `	{` |
|   2399453 |  9161 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2399453 |  9162 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  9163 | `			return SXERR_ABORT;` |
|   2399453 |  9164 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  9165 | `			goto Synchronize;` |
|         - |  9166 | `		}` |
|         - |  9167 | `	}` |
|         - |  9168 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  9169 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  9170 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  9171 | `	{` |
|   2399453 |  9172 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  9173 | `		sxu32 i;` |
|   3692871 |  9174 | `		for( i = 0; i < nArg; i++ ){` |
|   1293433 |  9175 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  9176 | `			ph7_class_attr *pAttr;` |
|   1293433 |  9177 | `			sxi32 iAttrFlags = 0;` |
|         - |  9178 | `			int bArgTyped;` |
|   1293433 |  9179 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1293347 |  9180 | `				continue;` |
|         - |  9181 | `			}` |
|         - |  9182 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  9183 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  9184 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        60 |  9185 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        92 |  9186 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        91 |  9187 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  9188 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9189 | `					"Cannot declare variadic promoted property");` |
|         3 |  9190 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9191 | `					return SXERR_ABORT;` |
|         - |  9192 | `				}` |
|         3 |  9193 | `				goto Synchronize;` |
|         - |  9194 | `			}` |
|         - |  9195 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  9196 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  9197 | `			 * appear as an alternative of a union type. */` |
|        89 |  9198 | `			if( bArgTyped ){` |
|       125 |  9199 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        80 |  9200 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        80 |  9201 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        40 |  9202 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        85 |  9203 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9204 | `					return SXERR_ABORT;` |
|        85 |  9205 | `				}else if( rc != SXRET_OK ){` |
|         6 |  9206 | `					goto Synchronize;` |
|         - |  9207 | `				}` |
|        38 |  9208 | `			}` |
|         - |  9209 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        85 |  9210 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  9211 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9212 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  9213 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9214 | `					return SXERR_ABORT;` |
|         - |  9215 | `				}` |
|         3 |  9216 | `				goto Synchronize;` |
|         - |  9217 | `			}` |
|        83 |  9218 | `			if( bArgTyped ){` |
|        79 |  9219 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        37 |  9220 | `			}` |
|        83 |  9221 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  9222 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  9223 | `			}` |
|        83 |  9224 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  9225 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  9226 | `			}` |
|        83 |  9227 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  9228 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  9229 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  9230 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  9231 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9232 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  9233 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9234 | `						return SXERR_ABORT;` |
|         - |  9235 | `					}` |
|         3 |  9236 | `					goto Synchronize;` |
|         - |  9237 | `				}` |
|        24 |  9238 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  9239 | `			}` |
|        81 |  9240 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  9241 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  9242 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  9243 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9244 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  9245 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  9246 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9247 | `						return SXERR_ABORT;` |
|         - |  9248 | `					}` |
|       ! 0 |  9249 | `					goto Synchronize;` |
|         - |  9250 | `				}` |
|         5 |  9251 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  9252 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  9253 | `			}` |
|        81 |  9254 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        81 |  9255 | `			if( pAttr == 0 ){` |
|       ! 0 |  9256 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9257 | `				return SXERR_ABORT;` |
|         - |  9258 | `			}` |
|        81 |  9259 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        79 |  9260 | `				pAttr->nType = pArg->nType;` |
|        79 |  9261 | `				pAttr->sClass = pArg->sClass;` |
|        79 |  9262 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        79 |  9263 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9264 | `					sxu32 k;` |
|        20 |  9265 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9266 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9267 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9268 | `					}` |
|         3 |  9269 | `				}` |
|        37 |  9270 | `			}` |
|        81 |  9271 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        81 |  9272 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9273 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9274 | `				return SXERR_ABORT;` |
|         - |  9275 | `			}` |
|        43 |  9276 | `		}` |
|         - |  9277 | `	}` |
|   2399443 |  9278 | `	if( doBody ){` |
|         - |  9279 | `		/* Compile method body */` |
|   2261703 |  9280 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2261703 |  9281 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9282 | `			return SXERR_ABORT;` |
|         - |  9283 | `		}` |
|         - |  9284 | `		/* The cursor sits just past the body's closing brace */` |
|   2261703 |  9285 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1130854 |  9286 | `	}else{` |
|         - |  9287 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    137745 |  9288 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    137745 |  9289 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     68870 |  9290 | `		}` |
|         - |  9291 | `		/* Only method signature is allowed */` |
|    137745 |  9292 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9293 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9294 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9295 | `				if( rc == SXERR_ABORT ){` |
|         - |  9296 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9297 | `					return SXERR_ABORT;` |
|         - |  9298 | `				}` |
|       ! 0 |  9299 | `				return SXERR_CORRUPT;` |
|         - |  9300 | `			}` |
|         - |  9301 | `	}` |
|         - |  9302 | `	/* All done,install the method */` |
|   2399443 |  9303 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2399443 |  9304 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9305 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9306 | `		return SXERR_ABORT;` |
|         - |  9307 | `	}` |
|   2399443 |  9308 | `	return SXRET_OK;` |
|         6 |  9309 | `Synchronize:` |
|         - |  9310 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9311 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9312 | `		pGen->pIn++;` |
|         4 |  9313 | `	}` |
|        16 |  9314 | `	return SXERR_CORRUPT;` |
|   1199730 |  9315 | `}` |
|         - |  9316 | `/*` |
|         - |  9317 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9318 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9319 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9320 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9321 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9322 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9323 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9324 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9325 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9326 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9327 | `` * implicit `$value` formal.`` |
|         - |  9328 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9329 | ` */` |
|         - |  9330 | `/*` |
|         - |  9331 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9332 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9333 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9334 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9335 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9336 | ` */` |
|        94 |  9337 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9338 | `{` |
|         - |  9339 | `	SyToken *p;` |
|       345 |  9340 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9341 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9342 | `			continue;` |
|         - |  9343 | `		}` |
|         - |  9344 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9345 | `		if( p + 3 < pEnd` |
|        80 |  9346 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9347 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9348 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9349 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9350 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9351 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9352 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9353 | `			return 1;` |
|         - |  9354 | `		}` |
|         - |  9355 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9356 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9357 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9358 | `		if( p > pStart` |
|        26 |  9359 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9360 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9361 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9362 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9363 | `			return 1;` |
|         - |  9364 | `		}` |
|        15 |  9365 | `	}` |
|        43 |  9366 | `	return 0;` |
|        48 |  9367 | `}` |
|         - |  9368 | `/*` |
|         - |  9369 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9370 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9371 | ` */` |
|       990 |  9372 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9373 | `{` |
|      1167 |  9374 | `	return p + 6 < pEnd` |
|       671 |  9375 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9376 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9377 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9378 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9379 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9380 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9381 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9382 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9383 | `	 && p[5].sData.nByte == 3` |
|         8 |  9384 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9385 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9386 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9387 | `}` |
|         - |  9388 | `/*` |
|         - |  9389 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9390 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9391 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9392 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9393 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9394 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9395 | ` * or SXERR_MEM.` |
|         - |  9396 | ` */` |
|         4 |  9397 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9398 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9399 | `{` |
|         5 |  9400 | `	SyToken *p = pStart;` |
|        35 |  9401 | `	while( p < pEnd ){` |
|        31 |  9402 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9403 | `			SyToken sTok;` |
|         - |  9404 | `			char zName[384];` |
|         - |  9405 | `			sxu32 nName;` |
|         - |  9406 | `			char *zDup;` |
|         - |  9407 | ``			/* `parent` `::` */`` |
|         5 |  9408 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9409 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9410 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9411 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9412 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9413 | `			if( zDup == 0 ){` |
|       ! 0 |  9414 | `				return SXERR_MEM;` |
|         - |  9415 | `			}` |
|         5 |  9416 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9417 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9418 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9419 | `			sTok.pUserData = 0;` |
|         5 |  9420 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9421 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9422 | `			continue;` |
|         - |  9423 | `		}` |
|        27 |  9424 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9425 | `		p++;` |
|         1 |  9426 | `	}` |
|         5 |  9427 | `	return SXRET_OK;` |
|         3 |  9428 | `}` |
|        94 |  9429 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9430 | `{` |
|        95 |  9431 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9432 | `	sxi32 rc;` |
|        95 |  9433 | `	int bRefsSelf = 0;` |
|        95 |  9434 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9435 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9436 | `		char zHook[384];` |
|         - |  9437 | `		SyString sHookName;` |
|         - |  9438 | `		ph7_class_method *pMeth;` |
|         - |  9439 | `		int bGet;` |
|       159 |  9440 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9441 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9442 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9443 | `			continue;` |
|         - |  9444 | `		}` |
|       145 |  9445 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9446 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9447 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9448 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9449 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9450 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9451 | `				return SXERR_ABORT;` |
|         - |  9452 | `			}` |
|       ! 0 |  9453 | `			return SXERR_CORRUPT;` |
|         - |  9454 | `		}` |
|       145 |  9455 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9456 | `			goto HookSyntax;` |
|         - |  9457 | `		}` |
|       144 |  9458 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9459 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9460 | `			bGet = 1;` |
|       106 |  9461 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9462 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9463 | `			bGet = 0;` |
|        34 |  9464 | `		}else{` |
|       ! 0 |  9465 | `			goto HookSyntax;` |
|         - |  9466 | `		}` |
|       145 |  9467 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9468 | `		sHookName.zString = zHook;` |
|       217 |  9469 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9470 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9471 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9472 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9473 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9474 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9475 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9476 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9477 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9478 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9479 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9480 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9481 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9482 | `					return SXERR_ABORT;` |
|         - |  9483 | `				}` |
|       ! 0 |  9484 | `				return SXERR_CORRUPT;` |
|         - |  9485 | `			}` |
|        15 |  9486 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9487 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9488 | `			if( pMeth == 0 ){` |
|       ! 0 |  9489 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9490 | `				return SXERR_ABORT;` |
|         - |  9491 | `			}` |
|        15 |  9492 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9493 | `			if( !bGet ){` |
|         - |  9494 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9495 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9496 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9497 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9498 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9499 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9500 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9501 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9502 | `				if( zVName == 0 ){` |
|       ! 0 |  9503 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9504 | `					return SXERR_ABORT;` |
|         - |  9505 | `				}` |
|         7 |  9506 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9507 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9508 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9509 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9510 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9511 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9512 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9513 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9514 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9515 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9516 | `				}` |
|         7 |  9517 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9518 | `			}` |
|        15 |  9519 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9520 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9521 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9522 | `				return SXERR_ABORT;` |
|         - |  9523 | `			}` |
|        15 |  9524 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9525 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9526 | `		}` |
|       130 |  9527 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9528 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9529 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9530 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9531 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9532 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9533 | `				return SXERR_ABORT;` |
|         - |  9534 | `			}` |
|       ! 0 |  9535 | `			return SXERR_CORRUPT;` |
|         - |  9536 | `		}` |
|       131 |  9537 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9538 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9539 | `		if( pMeth == 0 ){` |
|       ! 0 |  9540 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9541 | `			return SXERR_ABORT;` |
|         - |  9542 | `		}` |
|       131 |  9543 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9544 | `		if( !bGet ){` |
|         - |  9545 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9546 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9547 | `				SyToken *pRp = 0;` |
|        17 |  9548 | `				pGen->pIn++;` |
|        17 |  9549 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9550 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9551 | `					goto HookSyntax;` |
|         - |  9552 | `				}` |
|        17 |  9553 | `				if( pGen->pIn < pRp ){` |
|        17 |  9554 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9555 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9556 | `						return SXERR_ABORT;` |
|         - |  9557 | `					}` |
|         8 |  9558 | `				}` |
|        17 |  9559 | `				pGen->pIn = &pRp[1];` |
|         8 |  9560 | `			}` |
|        61 |  9561 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9562 | `				/* Implicit $value formal */` |
|         - |  9563 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9564 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9565 | `				if( zVName == 0 ){` |
|       ! 0 |  9566 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9567 | `					return SXERR_ABORT;` |
|         - |  9568 | `				}` |
|        45 |  9569 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9570 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9571 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9572 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9573 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9574 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9575 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9576 | `			}` |
|        30 |  9577 | `		}` |
|       165 |  9578 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9579 | `			/* Block body */` |
|        69 |  9580 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9581 | `			SyToken *pCloser = 0;` |
|        69 |  9582 | `			int bParentCall = 0;` |
|        69 |  9583 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9584 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9585 | `				SyToken *pScan;` |
|       753 |  9586 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9587 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9588 | `						bParentCall = 1;` |
|         3 |  9589 | `						break;` |
|         - |  9590 | `					}` |
|       343 |  9591 | `				}` |
|        34 |  9592 | `			}` |
|        69 |  9593 | `			if( bParentCall ){` |
|         - |  9594 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9595 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9596 | `				 * hook method), then continue past the original body. */` |
|         - |  9597 | `				SySet sBody;` |
|         3 |  9598 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9599 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9600 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9601 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9602 | `					SySetRelease(&sBody);` |
|       ! 0 |  9603 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9604 | `					return SXERR_ABORT;` |
|         - |  9605 | `				}` |
|         3 |  9606 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9607 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9608 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9609 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9610 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9611 | `				SySetRelease(&sBody);` |
|         3 |  9612 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9613 | `					return SXERR_ABORT;` |
|         - |  9614 | `				}` |
|         3 |  9615 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9616 | `			}else{` |
|        67 |  9617 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9618 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9619 | `					return SXERR_ABORT;` |
|         - |  9620 | `				}` |
|        67 |  9621 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9622 | `			}` |
|        69 |  9623 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9624 | `				bRefsSelf = 1;` |
|         9 |  9625 | `			}` |
|       128 |  9626 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9627 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9628 | `			GenBlock *pBlock;` |
|         - |  9629 | `			SySet *pInstrContainer;` |
|         - |  9630 | `			SyToken *pBodyStart;` |
|         - |  9631 | `			SyToken *pExprEnd;` |
|        63 |  9632 | `			SyToken *pSavedEnd = 0;` |
|         - |  9633 | `			SySet sBody;` |
|        63 |  9634 | `			int bParentCall = 0;` |
|        63 |  9635 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9636 | `			pBodyStart = pGen->pIn;` |
|         - |  9637 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9638 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9639 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9640 | `			 * method on a token copy. */` |
|         - |  9641 | `			{` |
|        63 |  9642 | `				sxi32 iNest = 0;` |
|        63 |  9643 | `				pExprEnd = pBodyStart;` |
|       355 |  9644 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9645 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9646 | `						iNest++;` |
|       351 |  9647 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9648 | `						if( iNest <= 0 ){` |
|       ! 0 |  9649 | `							break;` |
|         - |  9650 | `						}` |
|         9 |  9651 | `						iNest--;` |
|       343 |  9652 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9653 | `						break;` |
|         - |  9654 | `					}` |
|       293 |  9655 | `					pExprEnd++;` |
|         1 |  9656 | `				}` |
|         - |  9657 | `			}` |
|         - |  9658 | `			{` |
|         - |  9659 | `				SyToken *pScan;` |
|       335 |  9660 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9661 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9662 | `						bParentCall = 1;` |
|         3 |  9663 | `						break;` |
|         - |  9664 | `					}` |
|       137 |  9665 | `				}` |
|         - |  9666 | `			}` |
|        63 |  9667 | `			if( bParentCall ){` |
|         3 |  9668 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9669 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9670 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9671 | `					SySetRelease(&sBody);` |
|       ! 0 |  9672 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9673 | `					return SXERR_ABORT;` |
|         - |  9674 | `				}` |
|         3 |  9675 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9676 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9677 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9678 | `			}` |
|        94 |  9679 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9680 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9681 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9682 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9683 | `				return SXERR_ABORT;` |
|         - |  9684 | `			}` |
|        63 |  9685 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9686 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9687 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9688 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9689 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9690 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9691 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9692 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9693 | `			if( bParentCall ){` |
|         3 |  9694 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9695 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9696 | `				SySetRelease(&sBody);` |
|         1 |  9697 | `			}` |
|        63 |  9698 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9699 | `				return SXERR_ABORT;` |
|         - |  9700 | `			}` |
|        63 |  9701 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9702 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9703 | `				bRefsSelf = 1;` |
|        18 |  9704 | `			}` |
|        63 |  9705 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9706 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9707 | `			}` |
|        63 |  9708 | `			if( !bGet ){` |
|         - |  9709 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9710 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9711 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9712 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9713 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9714 | `				bRefsSelf = 1;` |
|         1 |  9715 | `			}` |
|        32 |  9716 | `		}else{` |
|       ! 0 |  9717 | `			goto HookSyntax;` |
|         - |  9718 | `		}` |
|       131 |  9719 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9720 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9721 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9722 | `			return SXERR_ABORT;` |
|         - |  9723 | `		}` |
|       131 |  9724 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9725 | `	}` |
|        95 |  9726 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9727 | `		goto HookSyntax;` |
|         - |  9728 | `	}` |
|        95 |  9729 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9730 | `	if( !bRefsSelf ){` |
|         - |  9731 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9732 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9733 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9734 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9735 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9736 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9737 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9738 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9739 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9740 | `				return SXERR_ABORT;` |
|         - |  9741 | `			}` |
|       ! 0 |  9742 | `			return SXERR_CORRUPT;` |
|         - |  9743 | `		}` |
|        20 |  9744 | `	}` |
|        95 |  9745 | `	return SXRET_OK;` |
|       ! 0 |  9746 | `HookSyntax:` |
|       ! 0 |  9747 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9748 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9749 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9750 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9751 | `		return SXERR_ABORT;` |
|         - |  9752 | `	}` |
|       ! 0 |  9753 | `	return SXERR_CORRUPT;` |
|        48 |  9754 | `}` |
|         - |  9755 | `/*` |
|         - |  9756 | ` * Compile an object interface.` |
|         - |  9757 | ` *  According to the PHP language reference manual` |
|         - |  9758 | ` *   Object Interfaces:` |
|         - |  9759 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9760 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9761 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9762 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9763 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9764 | ` */` |
|     68948 |  9765 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9766 | `{` |
|     68953 |  9767 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9768 | `	ph7_class *pClass,*pBase;` |
|         - |  9769 | `	SyToken *pEnd,*pTmp;` |
|         - |  9770 | `	SyString *pName;` |
|         - |  9771 | `	sxi32 nKwrd;` |
|         - |  9772 | `	sxi32 rc;` |
|         - |  9773 | `	/* Jump the 'interface' keyword */` |
|     68953 |  9774 | `	pGen->pIn++;` |
|         - |  9775 | `	/* Extract interface name */` |
|     68953 |  9776 | `	pName = &pGen->pIn->sData;` |
|         - |  9777 | `	/* Advance the stream cursor */` |
|     68953 |  9778 | `	pGen->pIn++;` |
|         - |  9779 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9780 | `		SyBlob sFQN;` |
|         - |  9781 | `		SyString sFQNStr;` |
|     68953 |  9782 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     68953 |  9783 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     68953 |  9784 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     68953 |  9785 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     68953 |  9786 | `		SyBlobRelease(&sFQN);` |
|         - |  9787 | `	}` |
|     68953 |  9788 | `	if( pClass == 0 ){` |
|       ! 0 |  9789 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9790 | `		return SXERR_ABORT;` |
|         - |  9791 | `	}` |
|     68953 |  9792 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     68953 |  9793 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9794 | `		return SXERR_ABORT;` |
|         - |  9795 | `	}` |
|         - |  9796 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     68953 |  9797 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9798 | `	/* Assume no base class is given */` |
|     68953 |  9799 | `	pBase = 0;` |
|     68953 |  9800 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26785 |  9801 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26785 |  9802 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|         - |  9803 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|         - |  9804 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|         - |  9805 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|         - |  9806 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|     26785 |  9807 | `			pGen->pIn++;` |
|     13391 |  9808 | `			for(;;){` |
|         - |  9809 | `				SyBlob sResolved;` |
|         - |  9810 | `				SyString sBaseName;` |
|         - |  9811 | `				sxu32 nRefLine;` |
|         - |  9812 | `				ph7_class *pParent;` |
|     26787 |  9813 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26787 |  9814 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26787 |  9815 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9816 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9817 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9818 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9819 | `						pName);` |
|       ! 0 |  9820 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9821 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9822 | `						return SXERR_ABORT;` |
|         - |  9823 | `					}` |
|       ! 0 |  9824 | `					return SXRET_OK;` |
|         - |  9825 | `				}` |
|     40178 |  9826 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|     26782 |  9827 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26787 |  9828 | `				SyStringInitFromBuf(&sBaseName,` |
|         - |  9829 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9830 | `				/* Only interfaces is allowed */` |
|     26787 |  9831 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9832 | `					pParent = pParent->pNextName;` |
|       ! 0 |  9833 | `				}` |
|     26787 |  9834 | `				if( pParent == 0 ){` |
|       ! 0 |  9835 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9836 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9837 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9838 | `						SyBlobRelease(&sResolved);` |
|       ! 0 |  9839 | `						return SXERR_ABORT;` |
|       ! 0 |  9840 | `					}` |
|     26787 |  9841 | `				}else if( pBase == 0 ){` |
|         - |  9842 | `					/* First parent → single-inheritance base */` |
|     26785 |  9843 | `					pBase = pParent;` |
|     13395 |  9844 | `				}else{` |
|         - |  9845 | `					/* Additional parent → record it in aInterface (+ copy its` |
|         - |  9846 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|         3 |  9847 | `					PH7_ClassImplement(pClass,pParent);` |
|         - |  9848 | `				}` |
|     26787 |  9849 | `				SyBlobRelease(&sResolved);` |
|         - |  9850 | `				/* Continue on a comma-separated list */` |
|     26787 |  9851 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  9852 | `					pGen->pIn++;` |
|         3 |  9853 | `					continue;` |
|         - |  9854 | `				}` |
|     26785 |  9855 | `				break;` |
|       ! 0 |  9856 | `			}` |
|     13390 |  9857 | `		}` |
|     13390 |  9858 | `	}` |
|     68953 |  9859 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9860 | `		/* Syntax error */` |
|       ! 0 |  9861 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9862 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9863 | `		if( rc == SXERR_ABORT ){` |
|         - |  9864 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9865 | `			return SXERR_ABORT;` |
|         - |  9866 | `		}` |
|       ! 0 |  9867 | `		return SXRET_OK;` |
|         - |  9868 | `	}` |
|     68953 |  9869 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     68953 |  9870 | `	pEnd = 0; /* cc warning */` |
|         - |  9871 | `	/* Delimit the interface body */` |
|     68953 |  9872 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     68953 |  9873 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9874 | `		/* Syntax error */` |
|       ! 0 |  9875 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9876 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9877 | `		if( rc == SXERR_ABORT ){` |
|         - |  9878 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9879 | `			return SXERR_ABORT;` |
|         - |  9880 | `		}` |
|       ! 0 |  9881 | `		return SXRET_OK;` |
|         - |  9882 | `	}` |
|         - |  9883 | `	/* The delimiter token is the interface body's closing brace */` |
|     68953 |  9884 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9885 | `	/* Swap token stream */` |
|     68953 |  9886 | `	pTmp = pGen->pEnd;` |
|     68953 |  9887 | `	pGen->pEnd = pEnd;` |
|         - |  9888 | `	/* Start the parse process` |
|         - |  9889 | `	 * Note (According to the PHP reference manual):` |
|         - |  9890 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9891 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9892 | `	 */` |
|    126283 |  9893 | `	for(;;){` |
|         - |  9894 | `		/* Jump leading/trailing semi-colons */` |
|    436193 |  9895 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    183623 |  9896 | `			pGen->pIn++;` |
|         5 |  9897 | `		}` |
|    252575 |  9898 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9899 | `			/* End of interface body */` |
|     68949 |  9900 | `			break;` |
|         - |  9901 | `		}` |
|         - |  9902 | `		/* Bind a directly-preceding docblock to this member */` |
|    183631 |  9903 | `		GenStateSetPendingDoc(&(*pGen));` |
|    183631 |  9904 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9906 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9907 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9908 | `			if( rc == SXERR_ABORT ){` |
|         - |  9909 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9910 | `				return SXERR_ABORT;` |
|         - |  9911 | `			}` |
|       ! 0 |  9912 | `			goto done;` |
|         - |  9913 | `		}` |
|         - |  9914 | `		/* Extract the current keyword */` |
|    183631 |  9915 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    183631 |  9916 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9917 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9918 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9919 | `			const char *zKind = "member";` |
|         3 |  9920 | `			SyString *pMemberName = 0;` |
|         3 |  9921 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9922 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9923 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9924 | `					zKind = "constant";` |
|         3 |  9925 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9926 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9927 | `					}` |
|         1 |  9928 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9929 | `					zKind = "method";` |
|       ! 0 |  9930 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9931 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9932 | `					}` |
|       ! 0 |  9933 | `				}` |
|         1 |  9934 | `			}` |
|         3 |  9935 | `			if( pMemberName ){` |
|         4 |  9936 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9937 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9938 | `			}else{` |
|       ! 0 |  9939 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9940 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9941 | `			}` |
|         3 |  9942 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9943 | `				return SXERR_ABORT;` |
|         - |  9944 | `			}` |
|         3 |  9945 | `			goto done;` |
|         - |  9946 | `		}` |
|    183629 |  9947 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9948 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9949 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9950 | `			if( rc == SXERR_ABORT ){` |
|         - |  9951 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9952 | `				return SXERR_ABORT;` |
|         - |  9953 | `			}` |
|       ! 0 |  9954 | `			goto done;` |
|         - |  9955 | `		}` |
|    183629 |  9956 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9957 | `			/* Advance the stream cursor */` |
|    130075 |  9958 | `			pGen->pIn++;` |
|    130070 |  9959 | `			if( pGen->pIn < pGen->pEnd` |
|    130075 |  9960 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    130070 |  9961 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9962 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9963 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9964 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9965 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9966 | `				 * hooked properties" error). */` |
|       ! 0 |  9967 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9968 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9969 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9970 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9971 | `						return SXERR_ABORT;` |
|         - |  9972 | `					}` |
|       ! 0 |  9973 | `					goto done;` |
|         - |  9974 | `				}` |
|       ! 0 |  9975 | `				continue;` |
|         - |  9976 | `			}` |
|    130075 |  9977 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9978 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9979 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9980 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9981 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9982 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9983 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9984 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9985 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9986 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9987 | `							return SXERR_ABORT;` |
|         - |  9988 | `						}` |
|       ! 0 |  9989 | `						goto done;` |
|         - |  9990 | `					}` |
|       ! 0 |  9991 | `					continue;` |
|         - |  9992 | `				}` |
|       ! 0 |  9993 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9994 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9995 | `				if( rc == SXERR_ABORT ){` |
|         - |  9996 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9997 | `					return SXERR_ABORT;` |
|         - |  9998 | `				}` |
|       ! 0 |  9999 | `				goto done;` |
|         - | 10000 | `			}` |
|    130075 | 10001 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    130075 | 10002 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - | 10003 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - | 10004 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 | 10005 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 | 10006 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 | 10007 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 | 10008 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 | 10009 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 10010 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10011 | `							return SXERR_ABORT;` |
|         - | 10012 | `						}` |
|       ! 0 | 10013 | `						goto done;` |
|         - | 10014 | `					}` |
|         5 | 10015 | `					continue;` |
|         - | 10016 | `				}` |
|       ! 0 | 10017 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10018 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 | 10019 | `				if( rc == SXERR_ABORT ){` |
|         - | 10020 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 10021 | `					return SXERR_ABORT;` |
|         - | 10022 | `				}` |
|       ! 0 | 10023 | `				goto done;` |
|         - | 10024 | `			}` |
|     65033 | 10025 | `		}` |
|    183625 | 10026 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 10027 | `			/* Parse constant */` |
|     53555 | 10028 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53555 | 10029 | `			if( rc != SXRET_OK ){` |
|         3 | 10030 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10031 | `					return SXERR_ABORT;` |
|         - | 10032 | `				}` |
|         3 | 10033 | `				goto done;` |
|         - | 10034 | `			}` |
|     26779 | 10035 | `		}else{` |
|    130075 | 10036 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    130075 | 10037 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 10038 | `				/* Static method,record that */` |
|     11477 | 10039 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - | 10040 | `				/* Advance the stream cursor */` |
|     11477 | 10041 | `				pGen->pIn++;` |
|     11472 | 10042 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11477 | 10043 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 10044 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10045 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 10046 | `						if( rc == SXERR_ABORT ){` |
|         - | 10047 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 10048 | `							return SXERR_ABORT;` |
|         - | 10049 | `						}` |
|       ! 0 | 10050 | `						goto done;` |
|         - | 10051 | `				}` |
|      5736 | 10052 | `			}` |
|         - | 10053 | `			/* Process method signature (no body for interface methods) */` |
|    130075 | 10054 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    130075 | 10055 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 10056 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10057 | `					return SXERR_ABORT;` |
|         - | 10058 | `				}` |
|       ! 0 | 10059 | `				goto done;` |
|         - | 10060 | `			}` |
|         - | 10061 | `		}` |
|         5 | 10062 | `	}` |
|         - | 10063 | `	/* Install the interface */` |
|     68949 | 10064 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     68949 | 10065 | `	if( rc == SXRET_OK && pBase ){` |
|         - | 10066 | `		/* Inherit from the base interface */` |
|     26785 | 10067 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13390 | 10068 | `	}` |
|     68949 | 10069 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10070 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10071 | `		return SXERR_ABORT;` |
|         - | 10072 | `	}` |
|     34472 | 10073 | `done:` |
|         - | 10074 | `	/* Point beyond the interface body */` |
|     68953 | 10075 | `	pGen->pIn  = &pEnd[1];` |
|     68953 | 10076 | `	pGen->pEnd = pTmp;` |
|     68953 | 10077 | `	return PH7_OK;` |
|     34479 | 10078 | `}` |
|         - | 10079 | `/*` |
|         - | 10080 | ` * Compile a user-defined class.` |
|         - | 10081 | ` * According to the PHP language reference manual` |
|         - | 10082 | ` *  class` |
|         - | 10083 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - | 10084 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - | 10085 | ` *  of the properties and methods belonging to the class.` |
|         - | 10086 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - | 10087 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - | 10088 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - | 10089 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - | 10090 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - | 10091 | ` *  (called "methods").` |
|         - | 10092 | ` */` |
|         - | 10093 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - | 10094 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - | 10095 | `struct TraitUseEntry {` |
|         - | 10096 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - | 10097 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - | 10098 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - | 10099 | `};` |
|         - | 10100 | `/*` |
|         - | 10101 | ` * Validate that methods implementing interface contracts have compatible` |
|         - | 10102 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - | 10103 | ` */` |
|    353652 | 10104 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10105 | `{` |
|         - | 10106 | `	ph7_class **apIface;` |
|         - | 10107 | `	sxu32 nIface,i;` |
|         - | 10108 | `	sxi32 rc;` |
|    353657 | 10109 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 | 10110 | `		return SXRET_OK;` |
|         - | 10111 | `	}` |
|    353657 | 10112 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    353657 | 10113 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    709635 | 10114 | `	for(i = 0; i < nIface; i++){` |
|    355983 | 10115 | `		ph7_class *pIface = apIface[i];` |
|         - | 10116 | `		SyHashEntry *pEntry;` |
|    355983 | 10117 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1025783 | 10118 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    669805 | 10119 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - | 10120 | `			ph7_class_method *pImplMeth;` |
|    669805 | 10121 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - | 10122 | `			/* Find the implementing method in the class */` |
|    669805 | 10123 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    669805 | 10124 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 | 10125 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - | 10126 | `			}` |
|         - | 10127 | `			/* Check visibility: interface methods must be implemented as public */` |
|    669787 | 10128 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 | 10129 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10130 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 | 10131 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 | 10132 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10133 | `					return SXERR_ABORT;` |
|         - | 10134 | `				}` |
|         1 | 10135 | `			}` |
|         - | 10136 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - | 10137 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - | 10138 | `			 */` |
|         - | 10139 | `			{` |
|    669787 | 10140 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    669787 | 10141 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    669787 | 10142 | `				int sigError = 0;` |
|    669787 | 10143 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 | 10144 | `					sigError = 1;` |
|    669786 | 10145 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - | 10146 | `					/* Extra parameters must all have default values */` |
|      3833 | 10147 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 10148 | `					sxu32 k;` |
|      7659 | 10149 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3833 | 10150 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 10151 | `							sigError = 1;` |
|         3 | 10152 | `							break;` |
|         - | 10153 | `						}` |
|      1918 | 10154 | `					}` |
|      1914 | 10155 | `				}` |
|    669787 | 10156 | `				if( sigError ){` |
|         - | 10157 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 10158 | `					ph7_vm_func_arg *aArgs;` |
|         - | 10159 | `					sxu32 j;` |
|         6 | 10160 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 10161 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 10162 | `					/* Build implementing method signature */` |
|         6 | 10163 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 10164 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 10165 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 10166 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 10167 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10168 | `					}` |
|         - | 10169 | `					/* Build interface method signature */` |
|         6 | 10170 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 10171 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 10172 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 10173 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 10174 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10175 | `					}` |
|         8 | 10176 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10177 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 10178 | `						&pClass->sName,pMName,` |
|         4 | 10179 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 10180 | `						&pIface->sName,pMName,` |
|         4 | 10181 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 10182 | `					SyBlobRelease(&sImplSig);` |
|         6 | 10183 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 10184 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10185 | `						return SXERR_ABORT;` |
|         - | 10186 | `					}` |
|         2 | 10187 | `				}` |
|         - | 10188 | `			}` |
|         5 | 10189 | `		}` |
|    177994 | 10190 | `	}` |
|    353657 | 10191 | `	return SXRET_OK;` |
|    176831 | 10192 | `}` |
|         - | 10193 | `/*` |
|         - | 10194 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 10195 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 10196 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 10197 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 10198 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 10199 | ` * means that specific hook is still missing.` |
|         - | 10200 | ` */` |
|        38 | 10201 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 10202 | `{` |
|         - | 10203 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 10204 | `	ph7_class_attr *pProp;` |
|        38 | 10205 | `	if( pMName->nByte <= nPfx` |
|        27 | 10206 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 10207 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 10208 | `		return 0; /* not a hook stub */` |
|         - | 10209 | `	}` |
|         7 | 10210 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 10211 | `	return pProp != 0` |
|         6 | 10212 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 10213 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 10214 | `}` |
|         - | 10215 | `/*` |
|         - | 10216 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 10217 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 10218 | ` */` |
|        16 | 10219 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 10220 | `{` |
|         - | 10221 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 10222 | `	if( pMName->nByte > nPfx` |
|        12 | 10223 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 10224 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 10225 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 10226 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 10227 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 10228 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 10229 | `		return;` |
|         - | 10230 | `	}` |
|        20 | 10231 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 10232 | `}` |
|         - | 10233 | `/*` |
|         - | 10234 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 10235 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 10236 | ` */` |
|    353652 | 10237 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10238 | `{` |
|         - | 10239 | `	ph7_class_method *pMeth;` |
|         - | 10240 | `	SyHashEntry *pEntry;` |
|         - | 10241 | `	sxu32 nAbstract;` |
|         - | 10242 | `	SyBlob sMsg;` |
|         - | 10243 | `	sxi32 rc;` |
|         - | 10244 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    353657 | 10245 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15355 | 10246 | `		return SXRET_OK;` |
|         - | 10247 | `	}` |
|         - | 10248 | `	/* Count abstract methods */` |
|    338307 | 10249 | `	nAbstract = 0;` |
|    338307 | 10250 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   5007184 | 10251 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4499731 | 10252 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4499731 | 10253 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 10254 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 10255 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10256 | `			}` |
|        20 | 10257 | `			nAbstract++;` |
|         8 | 10258 | `		}` |
|         5 | 10259 | `	}` |
|    338307 | 10260 | `	if( nAbstract == 0 ){` |
|    338293 | 10261 | `		return SXRET_OK;` |
|         - | 10262 | `	}` |
|         - | 10263 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 10264 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 10265 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 10266 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 10267 | `		&pClass->sName,nAbstract,` |
|         7 | 10268 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 10269 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 10270 | `	/* Second pass: list methods with origins */` |
|         - | 10271 | `	{` |
|        18 | 10272 | `		sxu32 nListed = 0;` |
|        18 | 10273 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 10274 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 10275 | `			ph7_class *pOrigin = 0;` |
|         - | 10276 | `			SyString *pMName;` |
|        22 | 10277 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 10278 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10279 | `				continue;` |
|         - | 10280 | `			}` |
|        20 | 10281 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10282 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10283 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10284 | `			}` |
|        20 | 10285 | `			if( nListed > 0 ){` |
|         3 | 10286 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10287 | `			}` |
|         - | 10288 | `			/* Find the origin of this abstract method.` |
|         - | 10289 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10290 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10291 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10292 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10293 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10294 | `			 * class's namespace.` |
|         - | 10295 | `			 */` |
|         - | 10296 | `			{` |
|         - | 10297 | `				ph7_class **apIface;` |
|         - | 10298 | `				ph7_class **apTrait;` |
|         - | 10299 | `				ph7_class *pWalk;` |
|         - | 10300 | `				sxu32 i;` |
|         - | 10301 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10302 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10303 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10304 | `				 */` |
|        20 | 10305 | `				if( pClass->pBase ){` |
|        11 | 10306 | `					pWalk = pClass->pBase;` |
|        19 | 10307 | `					while( pWalk ){` |
|         - | 10308 | `						ph7_class_method *pParentMeth;` |
|        13 | 10309 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10310 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10311 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10312 | `							 * in this class's ancestor chain.` |
|         - | 10313 | `							 */` |
|        13 | 10314 | `							int fromIface = 0;` |
|        13 | 10315 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10316 | `							while( pAnc ){` |
|         - | 10317 | `								ph7_class **apPI;` |
|         - | 10318 | `								sxu32 j;` |
|        15 | 10319 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10320 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10321 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10322 | `										fromIface = 1;` |
|        10 | 10323 | `										break;` |
|         - | 10324 | `									}` |
|       ! 0 | 10325 | `								}` |
|        15 | 10326 | `								if( fromIface ) break;` |
|         6 | 10327 | `								pAnc = pAnc->pBase;` |
|         2 | 10328 | `							}` |
|        13 | 10329 | `							if( !fromIface ){` |
|         3 | 10330 | `								pOrigin = pWalk;` |
|         3 | 10331 | `								break;` |
|         - | 10332 | `							}` |
|         4 | 10333 | `						}` |
|        10 | 10334 | `						pWalk = pWalk->pBase;` |
|         2 | 10335 | `					}` |
|         4 | 10336 | `				}` |
|         - | 10337 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10338 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10339 | `				 */` |
|        20 | 10340 | `				if( !pOrigin ){` |
|        18 | 10341 | `					pWalk = pClass;` |
|        40 | 10342 | `					while( pWalk && !pOrigin ){` |
|        26 | 10343 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10344 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10345 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10346 | `							ph7_class *pDeepest = 0;` |
|        28 | 10347 | `							while( pIface ){` |
|        16 | 10348 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10349 | `									pDeepest = pIface;` |
|         6 | 10350 | `								}` |
|        16 | 10351 | `								pIface = pIface->pBase;` |
|         4 | 10352 | `							}` |
|        16 | 10353 | `							if( pDeepest ){` |
|        16 | 10354 | `								pOrigin = pDeepest;` |
|        16 | 10355 | `								break;` |
|         - | 10356 | `							}` |
|       ! 0 | 10357 | `						}` |
|        26 | 10358 | `						pWalk = pWalk->pBase;` |
|         4 | 10359 | `					}` |
|         7 | 10360 | `				}` |
|         - | 10361 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10362 | `				if( !pOrigin ){` |
|         3 | 10363 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10364 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10365 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10366 | `							pOrigin = pClass;` |
|         3 | 10367 | `							break;` |
|         - | 10368 | `						}` |
|       ! 0 | 10369 | `					}` |
|         1 | 10370 | `				}` |
|         - | 10371 | `			}` |
|        20 | 10372 | `			if( pOrigin ){` |
|        20 | 10373 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10374 | `			}else{` |
|         - | 10375 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10376 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10377 | `			}` |
|        20 | 10378 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10379 | `			nListed++;` |
|         4 | 10380 | `		}` |
|         - | 10381 | `	}` |
|        18 | 10382 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10383 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10384 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10385 | `	SyBlobRelease(&sMsg);` |
|        18 | 10386 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10387 | `		return SXERR_ABORT;` |
|         - | 10388 | `	}` |
|        18 | 10389 | `	return SXRET_OK;` |
|    176831 | 10390 | `}` |
|         - | 10391 | `/*` |
|         - | 10392 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10393 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10394 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10395 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10396 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10397 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10398 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10399 | ` */` |
|    415202 | 10400 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10401 | `{` |
|    415207 | 10402 | `	int isAbsolute = 0;` |
|    415207 | 10403 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10404 | `	SyBlob sName;` |
|    415207 | 10405 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4431 | 10406 | `		isAbsolute = 1;` |
|      4431 | 10407 | `		pGen->pIn++;` |
|      2213 | 10408 | `	}` |
|    415207 | 10409 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         9 | 10410 | `		pGen->pIn = pStart;` |
|         9 | 10411 | `		return SXERR_INVALID;` |
|         - | 10412 | `	}` |
|    415201 | 10413 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    415201 | 10414 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    415201 | 10415 | `	pGen->pIn++;` |
|    622827 | 10416 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    207636 | 10417 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        24 | 10418 | `		SyBlobAppend(&sName,"\\",1);` |
|        24 | 10419 | `		pGen->pIn++;` |
|        24 | 10420 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        24 | 10421 | `		pGen->pIn++;` |
|         2 | 10422 | `	}` |
|    415201 | 10423 | `	if( isAbsolute ){` |
|      4429 | 10424 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2217 | 10425 | `	}else{` |
|         - | 10426 | `		SyString sRaw;` |
|    410777 | 10427 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    410777 | 10428 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10429 | `	}` |
|    415201 | 10430 | `	SyBlobRelease(&sName);` |
|    415201 | 10431 | `	return SXRET_OK;` |
|    207606 | 10432 | `}` |
|         - | 10433 | `/*` |
|         - | 10434 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10435 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10436 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10437 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10438 | ` * either direction cannot run unbounded.` |
|         - | 10439 | ` */` |
|         - | 10440 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    164642 | 10441 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10442 | `{` |
|         - | 10443 | `	ph7_class **apParent;` |
|         - | 10444 | `	sxu32 n;` |
|    428753 | 10445 | `	while( pInterface ){` |
|    271769 | 10446 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10447 | `			return FALSE;` |
|         - | 10448 | `		}` |
|    306205 | 10449 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     68872 | 10450 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7663 | 10451 | `			return TRUE;` |
|         - | 10452 | `		}` |
|    264111 | 10453 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    264113 | 10454 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|         3 | 10455 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10456 | `				return TRUE;` |
|         - | 10457 | `			}` |
|         2 | 10458 | `		}` |
|    264111 | 10459 | `		pInterface = pInterface->pBase;` |
|    264111 | 10460 | `		iDepth++;` |
|         5 | 10461 | `	}` |
|    156989 | 10462 | `	return FALSE;` |
|     82326 | 10463 | `}` |
|    164640 | 10464 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10465 | `{` |
|    164645 | 10466 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10467 | `}` |
|         - | 10468 | `/*` |
|         - | 10469 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10470 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10471 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10472 | ` */` |
|      7658 | 10473 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10474 | `{` |
|      7667 | 10475 | `	while( pBase ){` |
|        10 | 10476 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10477 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10478 | `			return TRUE;` |
|         - | 10479 | `		}` |
|        10 | 10480 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10481 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10482 | `			return TRUE;` |
|         - | 10483 | `		}` |
|         5 | 10484 | `		pBase = pBase->pBase;` |
|         1 | 10485 | `	}` |
|      7659 | 10486 | `	return FALSE;` |
|      3834 | 10487 | `}` |
|         - | 10488 | `/*` |
|         - | 10489 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10490 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10491 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10492 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10493 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10494 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10495 | ` * pClass->aEnumCases for cases().` |
|         - | 10496 | ` */` |
|      7702 | 10497 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10498 | `{` |
|      7707 | 10499 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10500 | `	SySet *pInstrContainer;` |
|         - | 10501 | `	ph7_class_attr *pCase;` |
|         - | 10502 | `	SyString *pName;` |
|         - | 10503 | `	sxi32 rc;` |
|      7707 | 10504 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7707 | 10505 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10506 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10507 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10508 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10509 | `			return SXERR_ABORT;` |
|         - | 10510 | `		}` |
|       ! 0 | 10511 | `		goto Synchronize;` |
|         - | 10512 | `	}` |
|      7707 | 10513 | `	pName = &pGen->pIn->sData;` |
|         - | 10514 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7707 | 10515 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10516 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10517 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10518 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10519 | `			return SXERR_ABORT;` |
|         - | 10520 | `		}` |
|       ! 0 | 10521 | `		goto Synchronize;` |
|         - | 10522 | `	}` |
|      7707 | 10523 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10524 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7707 | 10525 | `	if( pCase == 0 ){` |
|       ! 0 | 10526 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10527 | `		return SXERR_ABORT;` |
|         - | 10528 | `	}` |
|      7707 | 10529 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7707 | 10530 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10531 | `		return SXERR_ABORT;` |
|         - | 10532 | `	}` |
|      7707 | 10533 | `	pGen->pIn++; /* Jump the case name */` |
|      7707 | 10534 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7693 | 10535 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10536 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10537 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10538 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10539 | `				return SXERR_ABORT;` |
|         - | 10540 | `			}` |
|         6 | 10541 | `			goto Synchronize;` |
|         - | 10542 | `		}` |
|      7689 | 10543 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10544 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10545 | `		 * (same technique as class constants). */` |
|      7689 | 10546 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7689 | 10547 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7689 | 10548 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7689 | 10549 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10550 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10551 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10552 | `		}` |
|      7689 | 10553 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7689 | 10554 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7689 | 10555 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10556 | `			return SXERR_ABORT;` |
|         - | 10557 | `		}` |
|      3847 | 10558 | `	}else{` |
|        17 | 10559 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10560 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10561 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10562 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10563 | `				return SXERR_ABORT;` |
|         - | 10564 | `			}` |
|       ! 0 | 10565 | `			goto Synchronize;` |
|         - | 10566 | `		}` |
|         - | 10567 | `	}` |
|      7703 | 10568 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7703 | 10569 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10570 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10571 | `		return SXERR_ABORT;` |
|         - | 10572 | `	}` |
|      7703 | 10573 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7703 | 10574 | `	return SXRET_OK;` |
|         2 | 10575 | `Synchronize:` |
|         - | 10576 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10577 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10578 | `		pGen->pIn++;` |
|         2 | 10579 | `	}` |
|         6 | 10580 | `	return SXERR_CORRUPT;` |
|      3856 | 10581 | `}` |
|         - | 10582 | `/*` |
|         - | 10583 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10584 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10585 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10586 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10587 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10588 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10589 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10590 | ` */` |
|      3850 | 10591 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10592 | `{` |
|         - | 10593 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10594 | `	const char *zBack;` |
|         - | 10595 | `	SySet sToken;` |
|         - | 10596 | `	char *zSrc;` |
|         - | 10597 | `	sxu32 nSrc,nMax;` |
|      3855 | 10598 | `	sxi32 rc = SXRET_OK;` |
|      3855 | 10599 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3850 | 10600 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3855 | 10601 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3855 | 10602 | `	if( zSrc == 0 ){` |
|       ! 0 | 10603 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10604 | `		return SXERR_ABORT;` |
|         - | 10605 | `	}` |
|      3855 | 10606 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3855 | 10607 | `	if( pClass->nEnumBacking != 0 ){` |
|      5762 | 10608 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10609 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10610 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10611 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1919 | 10612 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1924 | 10613 | `	}else{` |
|        21 | 10614 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10615 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10616 | `	}` |
|      3855 | 10617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3855 | 10618 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3855 | 10619 | `	pSaveIn = pGen->pIn;` |
|      3855 | 10620 | `	pSaveEnd = pGen->pEnd;` |
|      3855 | 10621 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3855 | 10622 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15381 | 10623 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11531 | 10624 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10625 | `	}` |
|      3855 | 10626 | `	pGen->pIn = pSaveIn;` |
|      3855 | 10627 | `	pGen->pEnd = pSaveEnd;` |
|      3855 | 10628 | `	SySetRelease(&sToken);` |
|      3855 | 10629 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1930 | 10630 | `}` |
|         - | 10631 | `/*` |
|         - | 10632 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10633 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10634 | ` */` |
|         - | 10635 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10636 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10637 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10638 | `};` |
|         - | 10639 | `/*` |
|         - | 10640 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10641 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10642 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10643 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10644 | ` * and before the class is installed.` |
|         - | 10645 | ` */` |
|      3850 | 10646 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10647 | `{` |
|         - | 10648 | `	SyHashEntry *pEntry;` |
|         - | 10649 | `	sxi32 rc;` |
|         - | 10650 | `	sxu32 n;` |
|         - | 10651 | `	/* php: "Enum %s cannot include properties" */` |
|      3855 | 10652 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11557 | 10653 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7709 | 10654 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7709 | 10655 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10656 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10657 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10658 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10659 | `				return SXERR_ABORT;` |
|         - | 10660 | `			}` |
|         3 | 10661 | `			break;` |
|         - | 10662 | `		}` |
|         5 | 10663 | `	}` |
|         - | 10664 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     53905 | 10665 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     75075 | 10666 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     50055 | 10667 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10668 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10669 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10670 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10671 | `				return SXERR_ABORT;` |
|         - | 10672 | `			}` |
|       ! 0 | 10673 | `		}` |
|     25030 | 10674 | `	}` |
|         - | 10675 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10676 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10677 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10678 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10679 | `	{` |
|         - | 10680 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10681 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10682 | `		ph7_class_attr *pAttr;` |
|      3855 | 10683 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10684 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3855 | 10685 | `		if( pAttr == 0 ){` |
|       ! 0 | 10686 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10687 | `			return SXERR_ABORT;` |
|         - | 10688 | `		}` |
|      3855 | 10689 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3855 | 10690 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3855 | 10691 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3855 | 10692 | `		if( pClass->nEnumBacking != 0 ){` |
|      3843 | 10693 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10694 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3843 | 10695 | `			if( pAttr == 0 ){` |
|       ! 0 | 10696 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10697 | `				return SXERR_ABORT;` |
|         - | 10698 | `			}` |
|      3843 | 10699 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3843 | 10700 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10701 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10702 | `			}else{` |
|      3837 | 10703 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10704 | `			}` |
|      3843 | 10705 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1919 | 10706 | `		}` |
|         - | 10707 | `	}` |
|      3855 | 10708 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1930 | 10709 | `}` |
|         - | 10710 | `/*` |
|         - | 10711 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10712 | ` *` |
|         - | 10713 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10714 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10715 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10716 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10717 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10718 | ` * implements, body, install) is shared by both paths.` |
|         - | 10719 | ` */` |
|    353696 | 10720 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10721 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10722 | `{` |
|    353701 | 10723 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10724 | `	ph7_class *pClass,*pBase;` |
|         - | 10725 | `	SyToken *pEnd,*pTmp;` |
|         - | 10726 | `	sxi32 iProtection;` |
|         - | 10727 | `	SySet aInterfaces;` |
|         - | 10728 | `	SySet aUseEntries;` |
|         - | 10729 | `	sxi32 iAttrflags;` |
|         - | 10730 | `	SyString *pName;` |
|         - | 10731 | `	sxi32 nKwrd;` |
|         - | 10732 | `	sxi32 rc;` |
|         - | 10733 | `	/* Jump the 'class' keyword */` |
|    353701 | 10734 | `	pGen->pIn++;` |
|    353701 | 10735 | `	if( pAnonName ){` |
|         - | 10736 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10737 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10738 | `		 * then use the synthesized name. */` |
|        32 | 10739 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10740 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10741 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10742 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10743 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10744 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10745 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10746 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10747 | `		}` |
|        32 | 10748 | `		pName = pAnonName;` |
|        32 | 10749 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10750 | `	}else{` |
|    353673 | 10751 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10752 | `			/* Syntax error */` |
|       ! 0 | 10753 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10754 | `			if( rc == SXERR_ABORT ){` |
|         - | 10755 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10756 | `				return SXERR_ABORT;` |
|         - | 10757 | `			}` |
|         - | 10758 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10759 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10760 | `				pGen->pIn++;` |
|       ! 0 | 10761 | `			}` |
|       ! 0 | 10762 | `			return SXRET_OK;` |
|         - | 10763 | `		}` |
|         - | 10764 | `		/* Extract class name */` |
|    353673 | 10765 | `		pName = &pGen->pIn->sData;` |
|         - | 10766 | `		/* Advance the stream cursor */` |
|    353673 | 10767 | `		pGen->pIn++;` |
|         - | 10768 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10769 | `			SyBlob sFQN;` |
|         - | 10770 | `			SyString sFQNStr;` |
|    353673 | 10771 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    353673 | 10772 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    353673 | 10773 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    353673 | 10774 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    353673 | 10775 | `			SyBlobRelease(&sFQN);` |
|         - | 10776 | `		}` |
|         - | 10777 | `	}` |
|    353701 | 10778 | `	if( pClass == 0 ){` |
|       ! 0 | 10779 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10780 | `		return SXERR_ABORT;` |
|         - | 10781 | `	}` |
|    353696 | 10782 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3859 | 10783 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10784 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3845 | 10785 | `		pGen->pIn++; /* Jump ':' */` |
|      3840 | 10786 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3845 | 10787 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10788 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10789 | `			pGen->pIn++;` |
|      3838 | 10790 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3839 | 10791 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3837 | 10792 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3837 | 10793 | `			pGen->pIn++;` |
|      1921 | 10794 | `		}else{` |
|         3 | 10795 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10796 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10797 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10798 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10799 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10800 | `				return SXERR_ABORT;` |
|         - | 10801 | `			}` |
|         3 | 10802 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10803 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10804 | `			}` |
|         - | 10805 | `		}` |
|      1920 | 10806 | `	}` |
|    353701 | 10807 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    353701 | 10808 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10809 | `		return SXERR_ABORT;` |
|         - | 10810 | `	}` |
|         - | 10811 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    353701 | 10812 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    353701 | 10813 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10814 | `	/* Assume a standalone class */` |
|    353701 | 10815 | `	pBase = 0;` |
|    353701 | 10816 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    287265 | 10817 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    287265 | 10818 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10819 | `			SyBlob sResolved;` |
|         - | 10820 | `			SyString sBaseName;` |
|         - | 10821 | `			sxu32 nRefLine;` |
|    183837 | 10822 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10823 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10824 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10825 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10826 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10827 | `					return SXERR_ABORT;` |
|         - | 10828 | `				}` |
|       ! 0 | 10829 | `			}` |
|    183837 | 10830 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    183837 | 10831 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    183837 | 10832 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    183837 | 10833 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10834 | `				SyBlobRelease(&sResolved);` |
|         4 | 10835 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10836 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10837 | `					pName);` |
|         3 | 10838 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10839 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10840 | `					return SXERR_ABORT;` |
|         - | 10841 | `				}` |
|         3 | 10842 | `				return SXRET_OK;` |
|         - | 10843 | `			}` |
|    275750 | 10844 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    183830 | 10845 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    183835 | 10846 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10847 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10848 | `			/* Interfaces are not allowed */` |
|    183835 | 10849 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10850 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10851 | `			}` |
|    183835 | 10852 | `			if( pBase == 0 ){` |
|       ! 0 | 10853 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10854 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10855 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10856 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10857 | `					return SXERR_ABORT;` |
|         - | 10858 | `				}` |
|       ! 0 | 10859 | `			}else{` |
|    183835 | 10860 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10861 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10862 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10863 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10864 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10865 | `						return SXERR_ABORT;` |
|         - | 10866 | `					}` |
|         3 | 10867 | `					pBase = 0; /* Never inherit from an enum */` |
|    183834 | 10868 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10869 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10870 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10871 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10872 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10873 | `						return SXERR_ABORT;` |
|         - | 10874 | `					}` |
|       ! 0 | 10875 | `				}` |
|         - | 10876 | `			}` |
|    183835 | 10877 | `			SyBlobRelease(&sResolved);` |
|    183835 | 10878 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10879 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10880 | `			}` |
|     91915 | 10881 | `		}` |
|    287263 | 10882 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10883 | `			ph7_class *pInterface;` |
|         - | 10884 | `			/* Interface implementation */` |
|    107271 | 10885 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    111007 | 10886 | `			for(;;){` |
|         - | 10887 | `				SyBlob sResolved;` |
|         - | 10888 | `				SyString sIntName;` |
|         - | 10889 | `				sxu32 nRefLine;` |
|    164645 | 10890 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    164645 | 10891 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    164645 | 10892 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10893 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10894 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10895 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10896 | `						pName);` |
|       ! 0 | 10897 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10898 | `						return SXERR_ABORT;` |
|         - | 10899 | `					}` |
|       ! 0 | 10900 | `					break;` |
|         - | 10901 | `				}` |
|    329285 | 10902 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    164640 | 10903 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    164645 | 10904 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10905 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10906 | `				/* Only interfaces are allowed */` |
|    164645 | 10907 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10908 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10909 | `				}` |
|    164645 | 10910 | `				if( pInterface == 0 ){` |
|       ! 0 | 10911 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10912 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10913 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10914 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10915 | `						return SXERR_ABORT;` |
|         - | 10916 | `					}` |
|       ! 0 | 10917 | `				}else{` |
|         - | 10918 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10919 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10920 | `					 * unless they already extend Exception or Error.` |
|         - | 10921 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10922 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10923 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    164645 | 10924 | `					SyString *pFqn = &pClass->sName;` |
|    164645 | 10925 | `					int bIsExceptionOrError =` |
|     86148 | 10926 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    248876 | 10927 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    162735 | 10928 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3838 | 10929 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    168469 | 10930 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11490 | 10931 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3827 | 10932 | `						!bIsExceptionOrError ){` |
|        12 | 10933 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10934 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10935 | `							&pClass->sName);` |
|         9 | 10936 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10937 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10938 | `							return SXERR_ABORT;` |
|         - | 10939 | `						}` |
|         - | 10940 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10941 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10942 | `					}else{` |
|    164639 | 10943 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10944 | `					}` |
|         - | 10945 | `				}` |
|    164645 | 10946 | `				SyBlobRelease(&sResolved);` |
|    164645 | 10947 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53638 | 10948 | `					break;` |
|         - | 10949 | `				}` |
|     57379 | 10950 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10951 | `			}` |
|     53633 | 10952 | `		}` |
|    143629 | 10953 | `	}` |
|    353699 | 10954 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10955 | `		/* Syntax error */` |
|       ! 0 | 10956 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10957 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10958 | `		if( rc == SXERR_ABORT ){` |
|         - | 10959 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10960 | `			return SXERR_ABORT;` |
|         - | 10961 | `		}` |
|       ! 0 | 10962 | `		return SXRET_OK;` |
|         - | 10963 | `	}` |
|    353699 | 10964 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    353699 | 10965 | `	pEnd = 0; /* cc warning */` |
|         - | 10966 | `	/* Delimit the class body */` |
|    353699 | 10967 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    353699 | 10968 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10969 | `		/* Syntax error */` |
|       ! 0 | 10970 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10971 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10972 | `		if( rc == SXERR_ABORT ){` |
|         - | 10973 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10974 | `			return SXERR_ABORT;` |
|         - | 10975 | `		}` |
|       ! 0 | 10976 | `		return SXRET_OK;` |
|         - | 10977 | `	}` |
|         - | 10978 | `	/* The delimiter token is the class body's closing brace */` |
|    353699 | 10979 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10980 | `	/* Swap token stream */` |
|    353699 | 10981 | `	pTmp = pGen->pEnd;` |
|    353699 | 10982 | `	pGen->pEnd = pEnd;` |
|         - | 10983 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    353699 | 10984 | `	pClass->iFlags \|= iFlags;` |
|         - | 10985 | `	/* Start the parse process */` |
|   1372722 | 10986 | `	for(;;){` |
|         - | 10987 | `		/* Jump leading/trailing semi-colons */` |
|   3910259 | 10988 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    704815 | 10989 | `			pGen->pIn++;` |
|         5 | 10990 | `		}` |
|   3205449 | 10991 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10992 | `			/* End of class body */` |
|    353657 | 10993 | `			break;` |
|         - | 10994 | `		}` |
|         - | 10995 | `		/* Bind a directly-preceding docblock to this member */` |
|   2851797 | 10996 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2851792 | 10997 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1425901 | 10998 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10999 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11000 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11001 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11002 | `			if( rc == SXERR_ABORT ){` |
|         - | 11003 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 11004 | `				return SXERR_ABORT;` |
|         - | 11005 | `			}` |
|       ! 0 | 11006 | `			goto done;` |
|         - | 11007 | `		}` |
|         - | 11008 | `		/* Assume public visibility */` |
|   2851797 | 11009 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2851797 | 11010 | `		iAttrflags = 0;` |
|         - | 11011 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 11012 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 11013 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 11014 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2851797 | 11015 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11016 | `			int bMod = 0;` |
|       ! 0 | 11017 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11018 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 11019 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 11020 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 11021 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 11022 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 11023 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 11024 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 11025 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 11026 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 11027 | `			}` |
|       ! 0 | 11028 | `			if( !bMod ){` |
|       ! 0 | 11029 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11030 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11031 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11032 | `						return SXERR_ABORT;` |
|         - | 11033 | `					}` |
|       ! 0 | 11034 | `					goto done;` |
|         - | 11035 | `				}` |
|       ! 0 | 11036 | `				continue;` |
|         - | 11037 | `			}` |
|       ! 0 | 11038 | `		}` |
|   2851797 | 11039 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11040 | `			/* Extract the current keyword */` |
|   2851797 | 11041 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2851797 | 11042 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 11043 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7707 | 11044 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7707 | 11045 | `				if( rc != SXRET_OK ){` |
|         6 | 11046 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11047 | `						return SXERR_ABORT;` |
|         - | 11048 | `					}` |
|         6 | 11049 | `					goto done;` |
|         - | 11050 | `				}` |
|      7703 | 11051 | `				continue;` |
|         - | 11052 | `			}` |
|   2844095 | 11053 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11054 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 11055 | `				TraitUseEntry sUse;` |
|     15381 | 11056 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15381 | 11057 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15381 | 11058 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7696 | 11059 | `				for(;;){` |
|         - | 11060 | `					ph7_class *pTrait;` |
|         - | 11061 | `					SyBlob sResolved;` |
|         - | 11062 | `					SyString sTraitName;` |
|     15389 | 11063 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|         - | 11064 | `					/* A trait name is a full class reference: it may be qualified or` |
|         - | 11065 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|         - | 11066 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|         - | 11067 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|         - | 11068 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|         - | 11069 | `					 * choked on the first '\'. */` |
|     15389 | 11070 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15389 | 11071 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 11072 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 11073 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       ! 0 | 11074 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 11075 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11076 | `							return SXERR_ABORT;` |
|         - | 11077 | `						}` |
|       ! 0 | 11078 | `						break;` |
|         - | 11079 | `					}` |
|     30773 | 11080 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15384 | 11081 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15389 | 11082 | `					SyStringInitFromBuf(&sTraitName,` |
|         - | 11083 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 11084 | `					/* Only traits are allowed */` |
|     15389 | 11085 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11086 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 11087 | `					}` |
|     15389 | 11088 | `					if( pTrait == 0 ){` |
|       ! 0 | 11089 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|         - | 11090 | `							"'%z' is not a trait",&sTraitName);` |
|       ! 0 | 11091 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11092 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 11093 | `							return SXERR_ABORT;` |
|         - | 11094 | `						}` |
|       ! 0 | 11095 | `					}else{` |
|     15389 | 11096 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 11097 | `					}` |
|     15389 | 11098 | `					SyBlobRelease(&sResolved);` |
|         - | 11099 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|         - | 11100 | `					 * continue only across a comma-separated trait list. */` |
|     15389 | 11101 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7693 | 11102 | `						break;` |
|         - | 11103 | `					}` |
|        10 | 11104 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 11105 | `				}` |
|         - | 11106 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15381 | 11107 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 11108 | `					SyToken *pBlock;` |
|        13 | 11109 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 11110 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 11111 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 11112 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 11113 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 11114 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 11115 | `					}else{` |
|       ! 0 | 11116 | `						pGen->pIn = pGen->pEnd;` |
|         - | 11117 | `					}` |
|         5 | 11118 | `				}` |
|     15381 | 11119 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 11120 | `				/* The semicolon will be consumed by the outer loop */` |
|     15381 | 11121 | `				continue;` |
|         - | 11122 | `			}` |
|   2828719 | 11123 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 11124 | `				int nSetTok;` |
|   2583481 | 11125 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2583481 | 11126 | `				if( nSetVis ){` |
|         - | 11127 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 11128 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 11129 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11130 | `					pGen->pIn += nSetTok;` |
|         2 | 11131 | `				}else{` |
|   2583479 | 11132 | `					iProtection = nKwrd;` |
|   2583479 | 11133 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 11134 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 11135 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2583479 | 11136 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2583479 | 11137 | `					if( nSetVis ){` |
|         9 | 11138 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 11139 | `						pGen->pIn += nSetTok;` |
|         4 | 11140 | `					}` |
|         - | 11141 | `				}` |
|         - | 11142 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 11143 | ``				 * `public private(set) readonly int $x`. */`` |
|   2583481 | 11144 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        26 | 11145 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        26 | 11146 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        11 | 11147 | `				}` |
|   2583476 | 11148 | `				if( pGen->pIn >= pGen->pEnd` |
|   2583481 | 11149 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11150 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11151 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11152 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11153 | `					if( rc == SXERR_ABORT ){` |
|         - | 11154 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 11155 | `						return SXERR_ABORT;` |
|         - | 11156 | `					}` |
|       ! 0 | 11157 | `					goto done;` |
|         - | 11158 | `				}` |
|   2583481 | 11159 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11160 | `					/* Attribute declaration (untyped) */` |
|    409801 | 11161 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    409801 | 11162 | `					if( rc != SXRET_OK ){` |
|        11 | 11163 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11164 | `							return SXERR_ABORT;` |
|         - | 11165 | `						}` |
|        11 | 11166 | `						goto done;` |
|         - | 11167 | `					}` |
|    409946 | 11168 | `					continue;` |
|         - | 11169 | `				}` |
|   2173685 | 11170 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11171 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       317 | 11172 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       317 | 11173 | `					if( rc != SXRET_OK ){` |
|         8 | 11174 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11175 | `							return SXERR_ABORT;` |
|         - | 11176 | `						}` |
|         8 | 11177 | `						goto done;` |
|         - | 11178 | `					}` |
|       311 | 11179 | `					continue;` |
|         - | 11180 | `				}` |
|         - | 11181 | `				/* Extract the keyword */` |
|   2173373 | 11182 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1086684 | 11183 | `			}` |
|   2418611 | 11184 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 11185 | `				/* Process constant declaration */` |
|    237253 | 11186 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    237253 | 11187 | `				if( rc != SXRET_OK ){` |
|        11 | 11188 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11189 | `						return SXERR_ABORT;` |
|         - | 11190 | `					}` |
|        11 | 11191 | `					goto done;` |
|         - | 11192 | `				}` |
|    118625 | 11193 | `			}else{` |
|   2181363 | 11194 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 11195 | `					/* Static method or attribute,record that */` |
|     95787 | 11196 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     95787 | 11197 | `					pGen->pIn++; /* Jump the static keyword */` |
|     95787 | 11198 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11199 | `						int nSetTok;` |
|     68981 | 11200 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     68981 | 11201 | `						if( nSetVis ){` |
|         - | 11202 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 11203 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11204 | `							pGen->pIn += nSetTok;` |
|         2 | 11205 | `						}else{` |
|         - | 11206 | `							/* Extract the keyword */` |
|     68979 | 11207 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     68979 | 11208 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11209 | `								iProtection = nKwrd;` |
|       ! 0 | 11210 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 11211 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 11212 | `								if( nSetVis ){` |
|       ! 0 | 11213 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 11214 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 11215 | `								}` |
|       ! 0 | 11216 | `							}` |
|         - | 11217 | `						}` |
|     34488 | 11218 | `					}` |
|         - | 11219 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 11220 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 11221 | `					 * than a generic "expecting method" parse error. */` |
|     95787 | 11222 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11223 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11224 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 11225 | `					}` |
|     95782 | 11226 | `					if( pGen->pIn >= pGen->pEnd` |
|     95787 | 11227 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11228 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11229 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 11230 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11231 | `						if( rc == SXERR_ABORT ){` |
|         - | 11232 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11233 | `							return SXERR_ABORT;` |
|         - | 11234 | `						}` |
|       ! 0 | 11235 | `						goto done;` |
|         - | 11236 | `					}` |
|     95787 | 11237 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11238 | `						/* Attribute declaration */` |
|     26807 | 11239 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26807 | 11240 | `						if( rc != SXRET_OK ){` |
|         3 | 11241 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11242 | `								return SXERR_ABORT;` |
|         - | 11243 | `							}` |
|         3 | 11244 | `							goto done;` |
|         - | 11245 | `						}` |
|     26805 | 11246 | `						continue;` |
|         - | 11247 | `					}` |
|     68985 | 11248 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11249 | `						/* Typed static attribute declaration */` |
|        19 | 11250 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        19 | 11251 | `						if( rc != SXRET_OK ){` |
|         3 | 11252 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11253 | `								return SXERR_ABORT;` |
|         - | 11254 | `							}` |
|         3 | 11255 | `							goto done;` |
|         - | 11256 | `						}` |
|        17 | 11257 | `						continue;` |
|         - | 11258 | `					}` |
|         - | 11259 | `					/* Extract the keyword */` |
|     68969 | 11260 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2120063 | 11261 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 11262 | `					/* Abstract method,record that */` |
|      7675 | 11263 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 11264 | `					/* Mark the whole class as abstract */` |
|      7675 | 11265 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 11266 | `					/* Advance the stream cursor */` |
|      7675 | 11267 | `					pGen->pIn++;` |
|      7675 | 11268 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7675 | 11269 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7675 | 11270 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7673 | 11271 | `							iProtection = nKwrd;` |
|      7673 | 11272 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3834 | 11273 | `						}` |
|      3835 | 11274 | `					}` |
|      7675 | 11275 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7670 | 11276 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11277 | `							/* Static method */` |
|       ! 0 | 11278 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11279 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11280 | `					}` |
|      7675 | 11281 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7670 | 11282 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 11283 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 11284 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 11285 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 11286 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11287 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11288 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11289 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11290 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11291 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11292 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11293 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11294 | `										return SXERR_ABORT;` |
|         - | 11295 | `									}` |
|       ! 0 | 11296 | `									goto done;` |
|         - | 11297 | `								}` |
|         7 | 11298 | `								continue;` |
|         - | 11299 | `							}` |
|       ! 0 | 11300 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11301 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11302 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11303 | `							if( rc == SXERR_ABORT ){` |
|         - | 11304 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11305 | `								return SXERR_ABORT;` |
|         - | 11306 | `							}` |
|       ! 0 | 11307 | `							goto done;` |
|         - | 11308 | `					}` |
|      7669 | 11309 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2081743 | 11310 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11311 | `					/* final method ,record that */` |
|        21 | 11312 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        21 | 11313 | `					pGen->pIn++; /* Jump the final keyword */` |
|        21 | 11314 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11315 | `						/* Extract the keyword */` |
|        21 | 11316 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        21 | 11317 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        11 | 11318 | `							iProtection = nKwrd;` |
|        11 | 11319 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11320 | `						}` |
|         9 | 11321 | `					}` |
|        21 | 11322 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11323 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11324 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11325 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11326 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11327 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11328 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11329 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11330 | `									return SXERR_ABORT;` |
|         - | 11331 | `								}` |
|       ! 0 | 11332 | `								goto done;` |
|         - | 11333 | `							}` |
|        14 | 11334 | `							continue;` |
|         - | 11335 | `					}` |
|         9 | 11336 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11337 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11338 | `							/* Static method */` |
|       ! 0 | 11339 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11340 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11341 | `					}` |
|         9 | 11342 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11343 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11344 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11345 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11346 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11347 | `							if( rc == SXERR_ABORT ){` |
|         - | 11348 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11349 | `								return SXERR_ABORT;` |
|         - | 11350 | `							}` |
|       ! 0 | 11351 | `							goto done;` |
|         - | 11352 | `					}` |
|         9 | 11353 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11354 | `				}` |
|   2154527 | 11355 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11356 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11357 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11358 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11359 | `						if( rc == SXERR_ABORT ){` |
|         - | 11360 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11361 | `							return SXERR_ABORT;` |
|         - | 11362 | `						}` |
|       ! 0 | 11363 | `						goto done;` |
|         - | 11364 | `				}` |
|   2154527 | 11365 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11366 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11367 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11368 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11369 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11370 | `						if( rc == SXERR_ABORT ){` |
|         - | 11371 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11372 | `							return SXERR_ABORT;` |
|         - | 11373 | `						}` |
|       ! 0 | 11374 | `						goto done;` |
|         - | 11375 | `					}` |
|         - | 11376 | `					/* Attribute declaration */` |
|         7 | 11377 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11378 | `				}else{` |
|         - | 11379 | `					/* Process method declaration */` |
|   2154521 | 11380 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11381 | `				}` |
|   2154527 | 11382 | `				if( rc != SXRET_OK ){` |
|        16 | 11383 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11384 | `						return SXERR_ABORT;` |
|         - | 11385 | `					}` |
|        16 | 11386 | `					goto done;` |
|         - | 11387 | `				}` |
|         - | 11388 | `			}` |
|   1195880 | 11389 | `		}else{` |
|         - | 11390 | `			/* Attribute declaration */` |
|       ! 0 | 11391 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11392 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11393 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11394 | `					return SXERR_ABORT;` |
|         - | 11395 | `				}` |
|       ! 0 | 11396 | `				goto done;` |
|         - | 11397 | `			}` |
|         - | 11398 | `		}` |
|         5 | 11399 | `	}` |
|         - | 11400 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11401 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11402 | `	 */` |
|         - | 11403 | `	{` |
|         - | 11404 | `		TraitUseEntry *apUse;` |
|         - | 11405 | `		sxu32 nU;` |
|    353657 | 11406 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    369033 | 11407 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15381 | 11408 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15381 | 11409 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15381 | 11410 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15381 | 11411 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11412 | `			sxu32 nT;` |
|     15381 | 11413 | `			if( !hasResolution ){` |
|         - | 11414 | `				/* No conflict resolution block: use standard trait application */` |
|     30743 | 11415 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15377 | 11416 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15377 | 11417 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11418 | `						break;` |
|         - | 11419 | `					}` |
|      7691 | 11420 | `				}` |
|      7688 | 11421 | `			}else{` |
|         - | 11422 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11423 | `				 * then use the block to resolve method conflicts.` |
|         - | 11424 | `				 */` |
|         - | 11425 | `				SyToken *pR;` |
|        25 | 11426 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11427 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11428 | `					ph7_class_attr *pAR;` |
|         - | 11429 | `					SyHashEntry *pER;` |
|         - | 11430 | `					SyString *pNR;` |
|        15 | 11431 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11432 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11433 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11434 | `						pNR = &pAR->sName;` |
|       ! 0 | 11435 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11436 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11437 | `						}` |
|       ! 0 | 11438 | `					}` |
|        15 | 11439 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11440 | `				}` |
|         - | 11441 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11442 | `				pR = pUse->pResolvStart;` |
|        27 | 11443 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11444 | `					SyString sTrait,sMethod;` |
|         - | 11445 | `					ph7_class *pSrcTrait;` |
|         - | 11446 | `					ph7_class_method *pMeth;` |
|         - | 11447 | `					sxi32 nRKwrd;` |
|        41 | 11448 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11449 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11450 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11451 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11452 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11453 | `					sMethod = pR->sData;` |
|        17 | 11454 | `					pR++;` |
|        17 | 11455 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11456 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11457 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11458 | `							sTrait = sMethod;` |
|         7 | 11459 | `							pR++;` |
|         7 | 11460 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11461 | `							sMethod = pR->sData;` |
|         7 | 11462 | `							pR++;` |
|         3 | 11463 | `						}` |
|         3 | 11464 | `					}` |
|        17 | 11465 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11466 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11467 | `						continue;` |
|         - | 11468 | `					}` |
|        17 | 11469 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11470 | `					pR++;` |
|        17 | 11471 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11472 | `						pSrcTrait = 0;` |
|         7 | 11473 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11474 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11475 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11476 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11477 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11478 | `								break;` |
|         - | 11479 | `							}` |
|         2 | 11480 | `						}` |
|         5 | 11481 | `						if( pSrcTrait ){` |
|         5 | 11482 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11483 | `							if( pMeth ){` |
|         5 | 11484 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11485 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11486 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11487 | `								}` |
|         2 | 11488 | `							}` |
|         2 | 11489 | `						}` |
|         2 | 11490 | `					}` |
|        35 | 11491 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11492 | `				}` |
|         - | 11493 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11494 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11495 | `					ph7_class_method *pMR;` |
|         - | 11496 | `					SyHashEntry *pER;` |
|         - | 11497 | `					SyString *pNR;` |
|        15 | 11498 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11499 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11500 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11501 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11502 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11503 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11504 | `						}` |
|         3 | 11505 | `					}` |
|         9 | 11506 | `				}` |
|         - | 11507 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11508 | `				pR = pUse->pResolvStart;` |
|        27 | 11509 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11510 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11511 | `					ph7_class *pSrcTrait;` |
|         - | 11512 | `					ph7_class_method *pMeth;` |
|        27 | 11513 | `					int hasQual = 0;` |
|         - | 11514 | `					sxi32 nRKwrd;` |
|        41 | 11515 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11516 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11517 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11518 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11519 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11520 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11521 | `					sMethod = pR->sData;` |
|        17 | 11522 | `					pR++;` |
|        17 | 11523 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11524 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11525 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11526 | `							sTrait = sMethod;` |
|         7 | 11527 | `							hasQual = 1;` |
|         7 | 11528 | `							pR++;` |
|         7 | 11529 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11530 | `							sMethod = pR->sData;` |
|         7 | 11531 | `							pR++;` |
|         3 | 11532 | `						}` |
|         3 | 11533 | `					}` |
|        17 | 11534 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11535 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11536 | `						continue;` |
|         - | 11537 | `					}` |
|        17 | 11538 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11539 | `					pR++;` |
|        17 | 11540 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11541 | `						sxi32 iNewVis = -1;` |
|        13 | 11542 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11543 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11544 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11545 | `								iNewVis = nAK;` |
|         7 | 11546 | `								pR++;` |
|         3 | 11547 | `							}` |
|         3 | 11548 | `						}` |
|        13 | 11549 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11550 | `							sAlias = pR->sData;` |
|        11 | 11551 | `							pR++;` |
|         4 | 11552 | `						}` |
|        13 | 11553 | `						pMeth = 0;` |
|        13 | 11554 | `						if( hasQual ){` |
|         3 | 11555 | `							pSrcTrait = 0;` |
|         5 | 11556 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11557 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11558 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11559 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11560 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11561 | `									break;` |
|         - | 11562 | `								}` |
|         2 | 11563 | `							}` |
|         3 | 11564 | `							if( pSrcTrait ){` |
|         3 | 11565 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11566 | `							}` |
|         2 | 11567 | `						}else{` |
|        10 | 11568 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11569 | `						}` |
|        13 | 11570 | `						if( pMeth ){` |
|        13 | 11571 | `							if( sAlias.nByte > 0 ){` |
|         - | 11572 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11573 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11574 | `								 */` |
|         - | 11575 | `								ph7_class_method *pAlias;` |
|         - | 11576 | `								char *zAliasDup;` |
|        11 | 11577 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11578 | `								if( pAlias ){` |
|        11 | 11579 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11580 | `									if( iNewVis >= 0 ){` |
|         5 | 11581 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11582 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11583 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11584 | `									}` |
|        11 | 11585 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11586 | `									if( zAliasDup ){` |
|        11 | 11587 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11588 | `									}` |
|         7 | 11589 | `								}` |
|         7 | 11590 | `							}else if( iNewVis >= 0 ){` |
|         - | 11591 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11592 | `								ph7_class_method *pCopy;` |
|         3 | 11593 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11594 | `								if( pCopy ){` |
|         3 | 11595 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11596 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11597 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11598 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11599 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11600 | `									/* Replace the method in the class hash */` |
|         3 | 11601 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11602 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11603 | `								}` |
|         1 | 11604 | `							}` |
|         5 | 11605 | `						}` |
|         5 | 11606 | `						SXUNUSED(hasQual);` |
|         5 | 11607 | `					}` |
|        21 | 11608 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11609 | `				}` |
|         - | 11610 | `			}` |
|     15381 | 11611 | `			SySetRelease(&pUse->aTraits);` |
|      7693 | 11612 | `		}` |
|         - | 11613 | `	}` |
|    353657 | 11614 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11615 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11616 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3855 | 11617 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3855 | 11618 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11619 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11620 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11621 | `			return SXERR_ABORT;` |
|         - | 11622 | `		}` |
|      1925 | 11623 | `	}` |
|         - | 11624 | `	/* Install the class */` |
|    353657 | 11625 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    353657 | 11626 | `	if( rc == SXRET_OK ){` |
|         - | 11627 | `		ph7_class **apInterface;` |
|         - | 11628 | `		sxu32 n;` |
|    353657 | 11629 | `		if( pBase ){` |
|         - | 11630 | `			/* Inherit from base class and mark as a subclass */` |
|    183833 | 11631 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     91914 | 11632 | `		}` |
|    353657 | 11633 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    518291 | 11634 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11635 | `			/* Implements one or more interface */` |
|    164639 | 11636 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    164639 | 11637 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11638 | `				break;` |
|         - | 11639 | `			}` |
|     82322 | 11640 | `		}` |
|         - | 11641 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11642 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    353657 | 11643 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3855 | 11644 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3855 | 11645 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11646 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11647 | `			}` |
|      3855 | 11648 | `			if( pIntf ){` |
|      3855 | 11649 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1925 | 11650 | `			}` |
|      3855 | 11651 | `			if( pClass->nEnumBacking != 0 ){` |
|      3843 | 11652 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3843 | 11653 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11654 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11655 | `				}` |
|      3843 | 11656 | `				if( pIntf ){` |
|      3843 | 11657 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1919 | 11658 | `				}` |
|      1919 | 11659 | `			}` |
|      1925 | 11660 | `		}` |
|         - | 11661 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11662 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    353652 | 11663 | `		if( rc == SXRET_OK` |
|    353652 | 11664 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    353657 | 11665 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    187487 | 11666 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11667 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    187487 | 11668 | `			if( pStringable ){` |
|    187487 | 11669 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    187487 | 11670 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11671 | `				sxu32 i;` |
|    187487 | 11672 | `				int bAlready = 0;` |
|    225731 | 11673 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     42075 | 11674 | `					if( apImpl[i] == pStringable ){` |
|      3831 | 11675 | `						bAlready = 1;` |
|      3831 | 11676 | `						break;` |
|         - | 11677 | `					}` |
|     19127 | 11678 | `				}` |
|    187487 | 11679 | `				if( !bAlready ){` |
|    183661 | 11680 | `					PH7_ClassImplement(pClass,pStringable);` |
|     91828 | 11681 | `				}` |
|     93741 | 11682 | `			}` |
|     93741 | 11683 | `		}` |
|         - | 11684 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    353657 | 11685 | `		if( rc == SXRET_OK ){` |
|    353657 | 11686 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    353657 | 11687 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11688 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11689 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11690 | `				return SXERR_ABORT;` |
|         - | 11691 | `			}` |
|    176826 | 11692 | `		}` |
|         - | 11693 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    353657 | 11694 | `		if( rc == SXRET_OK ){` |
|    353657 | 11695 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    353657 | 11696 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11697 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11698 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11699 | `				return SXERR_ABORT;` |
|         - | 11700 | `			}` |
|    176826 | 11701 | `		}` |
|    176826 | 11702 | `	}` |
|    353657 | 11703 | `	SySetRelease(&aUseEntries);` |
|    353657 | 11704 | `	SySetRelease(&aInterfaces);` |
|    353657 | 11705 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11706 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11707 | `		return SXERR_ABORT;` |
|         - | 11708 | `	}` |
|    176826 | 11709 | `done:` |
|         - | 11710 | `	/* Point beyond the class body */` |
|    353699 | 11711 | `	pGen->pIn = &pEnd[1];` |
|    353699 | 11712 | `	pGen->pEnd = pTmp;` |
|    353699 | 11713 | `	return PH7_OK;` |
|    176853 | 11714 | `}` |
|         - | 11715 | `/* Compile a named class declaration (the common case). */` |
|    353668 | 11716 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11717 | `{` |
|    353673 | 11718 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11719 | `}` |
|         - | 11720 | `/*` |
|         - | 11721 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11722 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11723 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11724 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11725 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11726 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11727 | ` */` |
|        28 | 11728 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11729 | `{` |
|         - | 11730 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11731 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11732 | `	SyString sName;` |
|         - | 11733 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11734 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11735 | `	                              * is keyed to this 'class' token */` |
|         - | 11736 | `	ph7_value *pObj;` |
|        32 | 11737 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11738 | `	sxu32 nIdx,nLen;` |
|         - | 11739 | `	sxi32 nArg,rc;` |
|        14 | 11740 | `	SXUNUSED(iCompileFlag);` |
|         - | 11741 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11742 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11743 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11744 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11745 | `	}` |
|        32 | 11746 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11747 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11748 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11749 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11750 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11751 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11752 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11753 | `		return rc;` |
|         - | 11754 | `	}` |
|         - | 11755 | `	{` |
|         - | 11756 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11757 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11758 | `		if( pAnonClass` |
|        32 | 11759 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11760 | `			return SXERR_ABORT;` |
|         - | 11761 | `		}` |
|         - | 11762 | `	}` |
|         - | 11763 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11764 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11765 | `	nArg = 0;` |
|        32 | 11766 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11767 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11768 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11769 | `		SyToken *pArgNext;` |
|         7 | 11770 | `		pGen->pIn = pArgStart;` |
|         7 | 11771 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11772 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11773 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11774 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11775 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11776 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11777 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11778 | `					return SXERR_ABORT;` |
|         - | 11779 | `				}` |
|         7 | 11780 | `				nArg++;` |
|         3 | 11781 | `			}` |
|         7 | 11782 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11783 | `		}` |
|         7 | 11784 | `		pGen->pIn = pSavedIn;` |
|         7 | 11785 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11786 | `	}` |
|         - | 11787 | `	/* Load the synthesized class name */` |
|        32 | 11788 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11789 | `	if( pObj == 0 ){` |
|       ! 0 | 11790 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11791 | `		return SXERR_ABORT;` |
|         - | 11792 | `	}` |
|        32 | 11793 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11794 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11795 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11796 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11797 | `	return SXRET_OK;` |
|        18 | 11798 | `}` |
|         - | 11799 | `/*` |
|         - | 11800 | ` * Compile a user-defined abstract class.` |
|         - | 11801 | ` *  According to the PHP language reference manual` |
|         - | 11802 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11803 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11804 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11805 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11806 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11807 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11808 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11809 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11810 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11811 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11812 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11813 | ` *   could differ.` |
|         - | 11814 | ` */` |
|         - | 11815 | `/*` |
|         - | 11816 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11817 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11818 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11819 | ` */` |
|  12874142 | 11820 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11821 | `{` |
|  12874147 | 11822 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   7543995 | 11823 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   7543995 | 11824 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   7498069 | 11825 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3733683 | 11826 | `	}` |
|  12797523 | 11827 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  12797463 | 11828 | `	return FALSE;` |
|   6437076 | 11829 | `}` |
|         - | 11830 | `/*` |
|         - | 11831 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11832 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11833 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11834 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11835 | ` */` |
|  12797458 | 11836 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11837 | `{` |
|  12797463 | 11838 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  12797463 | 11839 | `	sxi32 iFlags = 0,iFlag;` |
|  12874147 | 11840 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76689 | 11841 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11842 | `			pDup = pIn;` |
|         2 | 11843 | `		}` |
|     76689 | 11844 | `		iFlags \|= iFlag;` |
|     76689 | 11845 | `		pIn++;` |
|         5 | 11846 | `	}` |
|  12797463 | 11847 | `	*ppIn = pIn;` |
|  12797463 | 11848 | `	if( ppDup ){ *ppDup = pDup; }` |
|  12797463 | 11849 | `	return iFlags;` |
|         5 | 11850 | `}` |
|         - | 11851 | `/*` |
|         - | 11852 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11853 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11854 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11855 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11856 | `` * `readonly`) to their existing handlers.`` |
|         - | 11857 | ` */` |
|  12762950 | 11858 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11859 | `{` |
|  12762955 | 11860 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   6423638 | 11861 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  12784030 | 11862 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11863 | `}` |
|         - | 11864 | `/*` |
|         - | 11865 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11866 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11867 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11868 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11869 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11870 | ` */` |
|     34508 | 11871 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11872 | `{` |
|         - | 11873 | `	SyToken *pDup;` |
|     34513 | 11874 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11875 | `	sxi32 rc;` |
|     34513 | 11876 | `	if( pDup ){` |
|         4 | 11877 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11878 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11879 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11880 | `			return SXERR_ABORT;` |
|         - | 11881 | `		}` |
|         1 | 11882 | `	}` |
|     34508 | 11883 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17259 | 11884 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11885 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11886 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11887 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11888 | `			return SXERR_ABORT;` |
|         - | 11889 | `		}` |
|         1 | 11890 | `	}` |
|     34513 | 11891 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17259 | 11892 | `}` |
|         - | 11893 | `/*` |
|         - | 11894 | ` * Compile a user-defined trait.` |
|         - | 11895 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11896 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11897 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11898 | ` */` |
|      7734 | 11899 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11900 | `{` |
|      7739 | 11901 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11902 | `	ph7_class *pClass;` |
|         - | 11903 | `	SyToken *pEnd,*pTmp;` |
|         - | 11904 | `	sxi32 iProtection;` |
|         - | 11905 | `	sxi32 iAttrflags;` |
|         - | 11906 | `	SyString *pName;` |
|         - | 11907 | `	sxi32 nKwrd;` |
|         - | 11908 | `	sxi32 rc;` |
|         - | 11909 | `	/* Jump the 'trait' keyword */` |
|      7739 | 11910 | `	pGen->pIn++;` |
|      7739 | 11911 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11912 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11913 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11914 | `			return SXERR_ABORT;` |
|         - | 11915 | `		}` |
|       ! 0 | 11916 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11917 | `			pGen->pIn++;` |
|       ! 0 | 11918 | `		}` |
|       ! 0 | 11919 | `		return SXRET_OK;` |
|         - | 11920 | `	}` |
|         - | 11921 | `	/* Extract trait name */` |
|      7739 | 11922 | `	pName = &pGen->pIn->sData;` |
|      7739 | 11923 | `	pGen->pIn++;` |
|         - | 11924 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11925 | `		SyBlob sFQN;` |
|         - | 11926 | `		SyString sFQNStr;` |
|      7739 | 11927 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7739 | 11928 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7739 | 11929 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7739 | 11930 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7739 | 11931 | `		SyBlobRelease(&sFQN);` |
|         - | 11932 | `	}` |
|      7739 | 11933 | `	if( pClass == 0 ){` |
|       ! 0 | 11934 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11935 | `		return SXERR_ABORT;` |
|         - | 11936 | `	}` |
|      7739 | 11937 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7739 | 11938 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11939 | `		return SXERR_ABORT;` |
|         - | 11940 | `	}` |
|         - | 11941 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7739 | 11942 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11943 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11944 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11945 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11946 | `			return SXERR_ABORT;` |
|         - | 11947 | `		}` |
|       ! 0 | 11948 | `		return SXRET_OK;` |
|         - | 11949 | `	}` |
|      7739 | 11950 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7739 | 11951 | `	pEnd = 0;` |
|      7739 | 11952 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7739 | 11953 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11954 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11955 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11956 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11957 | `			return SXERR_ABORT;` |
|         - | 11958 | `		}` |
|       ! 0 | 11959 | `		return SXRET_OK;` |
|         - | 11960 | `	}` |
|         - | 11961 | `	/* The delimiter token is the trait body's closing brace */` |
|      7739 | 11962 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11963 | `	/* Swap token stream */` |
|      7739 | 11964 | `	pTmp = pGen->pEnd;` |
|      7739 | 11965 | `	pGen->pEnd = pEnd;` |
|         - | 11966 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7739 | 11967 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11968 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     55536 | 11969 | `	for(;;){` |
|    157019 | 11970 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     22979 | 11971 | `			pGen->pIn++;` |
|         5 | 11972 | `		}` |
|    134045 | 11973 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7739 | 11974 | `			break;` |
|         - | 11975 | `		}` |
|         - | 11976 | `		/* Bind a directly-preceding docblock to this member */` |
|    126311 | 11977 | `		GenStateSetPendingDoc(&(*pGen));` |
|    126311 | 11978 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11979 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11980 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11981 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11982 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11983 | `				return SXERR_ABORT;` |
|         - | 11984 | `			}` |
|       ! 0 | 11985 | `			goto done;` |
|         - | 11986 | `		}` |
|    126311 | 11987 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    126311 | 11988 | `		iAttrflags = 0;` |
|    126311 | 11989 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    126311 | 11990 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    126311 | 11991 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11992 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11993 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11994 | `				for(;;){` |
|         - | 11995 | `					ph7_class *pUsedTrait;` |
|         - | 11996 | `					SyString *pUsedName;` |
|         5 | 11997 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11998 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11999 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 12000 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12001 | `							return SXERR_ABORT;` |
|         - | 12002 | `						}` |
|       ! 0 | 12003 | `						break;` |
|         - | 12004 | `					}` |
|         5 | 12005 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 12006 | `					{` |
|         - | 12007 | `						SyBlob sResolved;` |
|         5 | 12008 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 12009 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 12010 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 12011 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 12012 | `						SyBlobRelease(&sResolved);` |
|         - | 12013 | `					}` |
|         5 | 12014 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 12015 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 12016 | `					}` |
|         5 | 12017 | `					if( pUsedTrait == 0 ){` |
|         4 | 12018 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 12019 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 12020 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12021 | `							return SXERR_ABORT;` |
|         - | 12022 | `						}` |
|         2 | 12023 | `					}else{` |
|         3 | 12024 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 12025 | `					}` |
|         5 | 12026 | `					pGen->pIn++;` |
|         5 | 12027 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 12028 | `						break;` |
|         - | 12029 | `					}` |
|       ! 0 | 12030 | `					pGen->pIn++;` |
|       ! 0 | 12031 | `				}` |
|         5 | 12032 | `				continue;` |
|         - | 12033 | `			}` |
|    126307 | 12034 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    126289 | 12035 | `				iProtection = nKwrd;` |
|    126289 | 12036 | `				pGen->pIn++;` |
|         - | 12037 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|         - | 12038 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|         - | 12039 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|    126289 | 12040 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|         3 | 12041 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|         3 | 12042 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         1 | 12043 | `				}` |
|    126284 | 12044 | `				if( pGen->pIn >= pGen->pEnd` |
|    126289 | 12045 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 12046 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12047 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 12048 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12049 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12050 | `						return SXERR_ABORT;` |
|         - | 12051 | `					}` |
|       ! 0 | 12052 | `					goto done;` |
|         - | 12053 | `				}` |
|    126289 | 12054 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     22961 | 12055 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     22961 | 12056 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 12057 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12058 | `							return SXERR_ABORT;` |
|         - | 12059 | `						}` |
|       ! 0 | 12060 | `						goto done;` |
|         - | 12061 | `					}` |
|     22961 | 12062 | `					continue;` |
|         - | 12063 | `				}` |
|    103333 | 12064 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         7 | 12065 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 12066 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 12067 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12068 | `							return SXERR_ABORT;` |
|         - | 12069 | `						}` |
|       ! 0 | 12070 | `						goto done;` |
|         - | 12071 | `					}` |
|         7 | 12072 | `					continue;` |
|         - | 12073 | `				}` |
|    103327 | 12074 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     51661 | 12075 | `			}` |
|    103345 | 12076 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 12077 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12078 | `					"Traits cannot have constants");` |
|       ! 0 | 12079 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12080 | `					return SXERR_ABORT;` |
|         - | 12081 | `				}` |
|       ! 0 | 12082 | `				goto done;` |
|       ! 0 | 12083 | `			}else{` |
|    103345 | 12084 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7663 | 12085 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7663 | 12086 | `					pGen->pIn++;` |
|      7663 | 12087 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7661 | 12088 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7661 | 12089 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 12090 | `							iProtection = nKwrd;` |
|       ! 0 | 12091 | `							pGen->pIn++;` |
|       ! 0 | 12092 | `						}` |
|      3828 | 12093 | `					}` |
|      7658 | 12094 | `					if( pGen->pIn >= pGen->pEnd` |
|      7663 | 12095 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 12096 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12097 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 12098 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12099 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12100 | `							return SXERR_ABORT;` |
|         - | 12101 | `						}` |
|       ! 0 | 12102 | `						goto done;` |
|         - | 12103 | `					}` |
|      7663 | 12104 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 12105 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 12106 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12107 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12108 | `								return SXERR_ABORT;` |
|         - | 12109 | `							}` |
|       ! 0 | 12110 | `							goto done;` |
|         - | 12111 | `						}` |
|         3 | 12112 | `						continue;` |
|         - | 12113 | `					}` |
|      7661 | 12114 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 12115 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12116 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12117 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12118 | `								return SXERR_ABORT;` |
|         - | 12119 | `							}` |
|       ! 0 | 12120 | `							goto done;` |
|         - | 12121 | `						}` |
|       ! 0 | 12122 | `						continue;` |
|         - | 12123 | `					}` |
|      7661 | 12124 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     99515 | 12125 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         9 | 12126 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         9 | 12127 | `					pGen->pIn++;` |
|         9 | 12128 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         9 | 12129 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         9 | 12130 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         9 | 12131 | `							iProtection = nKwrd;` |
|         9 | 12132 | `							pGen->pIn++;` |
|         3 | 12133 | `						}` |
|         3 | 12134 | `					}` |
|         9 | 12135 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 12136 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 12137 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12138 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 12139 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12140 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12141 | `							return SXERR_ABORT;` |
|         - | 12142 | `						}` |
|       ! 0 | 12143 | `						goto done;` |
|         - | 12144 | `					}` |
|         9 | 12145 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 12146 | `				}` |
|    103343 | 12147 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 12148 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12149 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 12150 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12151 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12152 | `						return SXERR_ABORT;` |
|         - | 12153 | `					}` |
|       ! 0 | 12154 | `					goto done;` |
|         - | 12155 | `				}` |
|    103343 | 12156 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 12157 | `					pGen->pIn++;` |
|       ! 0 | 12158 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 12159 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12160 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 12161 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12162 | `							return SXERR_ABORT;` |
|         - | 12163 | `						}` |
|       ! 0 | 12164 | `						goto done;` |
|         - | 12165 | `					}` |
|       ! 0 | 12166 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12167 | `				}else{` |
|    103343 | 12168 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 12169 | `				}` |
|    103343 | 12170 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 12171 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12172 | `						return SXERR_ABORT;` |
|         - | 12173 | `					}` |
|       ! 0 | 12174 | `					goto done;` |
|         - | 12175 | `				}` |
|         - | 12176 | `			}` |
|     51674 | 12177 | `		}else{` |
|       ! 0 | 12178 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12179 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12180 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12181 | `					return SXERR_ABORT;` |
|         - | 12182 | `				}` |
|       ! 0 | 12183 | `				goto done;` |
|         - | 12184 | `			}` |
|         - | 12185 | `		}` |
|         5 | 12186 | `	}` |
|         - | 12187 | `	/* Install the trait */` |
|      7739 | 12188 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7739 | 12189 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12190 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12191 | `		return SXERR_ABORT;` |
|         - | 12192 | `	}` |
|      3867 | 12193 | `done:` |
|         - | 12194 | `	/* Point beyond the trait body */` |
|      7739 | 12195 | `	pGen->pIn = &pEnd[1];` |
|      7739 | 12196 | `	pGen->pEnd = pTmp;` |
|      7739 | 12197 | `	return PH7_OK;` |
|      3872 | 12198 | `}` |
|         - | 12199 | `/*` |
|         - | 12200 | ` * Compile a user-defined class.` |
|         - | 12201 | ` *  According to the PHP language reference manual` |
|         - | 12202 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 12203 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 12204 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 12205 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 12206 | ` *   and functions (called "methods").` |
|         - | 12207 | ` */` |
|    315306 | 12208 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 12209 | `{` |
|         - | 12210 | `	sxi32 rc;` |
|    315311 | 12211 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    315311 | 12212 | `	return rc;` |
|         5 | 12213 | `}` |
|         - | 12214 | `/*` |
|         - | 12215 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 12216 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 12217 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 12218 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 12219 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 12220 | ` */` |
|  12720794 | 12221 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 12222 | `{` |
|  12926058 | 12223 | `	return (pIn->nType & PH7_TK_ID)` |
|   6565656 | 12224 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    214955 | 12225 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  12926053 | 12226 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 12227 | `}` |
|         - | 12228 | `/*` |
|         - | 12229 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 12230 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 12231 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 12232 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 12233 | ` */` |
|      3854 | 12234 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 12235 | `{` |
|      3859 | 12236 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 12237 | `}` |
|         - | 12238 | `/*` |
|         - | 12239 | ` * Exception handling.` |
|         - | 12240 | ` *  According to the PHP language reference manual` |
|         - | 12241 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 12242 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 12243 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 12244 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 12245 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 12246 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 12247 | ` *    (or re-thrown) within a catch block.` |
|         - | 12248 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 12249 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 12250 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 12251 | ` *    been defined with set_exception_handler().` |
|         - | 12252 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 12253 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 12254 | ` */` |
|         - | 12255 | `/*` |
|         - | 12256 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 12257 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 12258 | ` * indicates failure.` |
|         - | 12259 | ` */` |
|    509016 | 12260 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 12261 | `{` |
|    509021 | 12262 | `	sxi32 rc = SXRET_OK;` |
|    509021 | 12263 | `	if( pRoot->pOp ){` |
|    509009 | 12264 | `		switch( pRoot->pOp->iOp ){` |
|    254502 | 12265 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 12266 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 12267 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 12268 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 12269 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 12270 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    509009 | 12271 | `			break;` |
|       ! 0 | 12272 | `		default:` |
|         - | 12273 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 12274 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 12275 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 12276 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12277 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 12278 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 12279 | `				rc = SXERR_INVALID;` |
|       ! 0 | 12280 | `			}` |
|       ! 0 | 12281 | `			break;` |
|         - | 12282 | `		}` |
|    254519 | 12283 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 12284 | `		/* Unexpected expression */` |
|       ! 0 | 12285 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12286 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12287 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 12288 | `			rc = SXERR_INVALID;` |
|       ! 0 | 12289 | `		}` |
|       ! 0 | 12290 | `	}` |
|    509021 | 12291 | `	return rc;` |
|         5 | 12292 | `}` |
|         - | 12293 | `/*` |
|         - | 12294 | ` * Compile a 'throw' statement.` |
|         - | 12295 | ` * throw: This is how you trigger an exception.` |
|         - | 12296 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12297 | ` */` |
|    508980 | 12298 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12299 | `{` |
|    508985 | 12300 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12301 | `	GenBlock *pBlock;` |
|         - | 12302 | `	sxu32 nIdx;` |
|         - | 12303 | `	sxi32 rc;` |
|    508985 | 12304 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12305 | `	/* Compile the expression */` |
|    508985 | 12306 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    508985 | 12307 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12308 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12309 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12310 | `			return SXERR_ABORT;` |
|         - | 12311 | `		}` |
|       ! 0 | 12312 | `		return SXRET_OK;` |
|         - | 12313 | `	}` |
|    508985 | 12314 | `	pBlock = pGen->pCurrent;` |
|         - | 12315 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   2027521 | 12316 | `	while(pBlock->pParent){` |
|   2027517 | 12317 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    508981 | 12318 | `			break;` |
|         - | 12319 | `		}` |
|         - | 12320 | `		/* Point to the parent block */` |
|   1518541 | 12321 | `		pBlock = pBlock->pParent;` |
|         5 | 12322 | `	}` |
|         - | 12323 | `	/* Emit the throw instruction */` |
|    508985 | 12324 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12325 | `	/* Emit the jump */` |
|    508985 | 12326 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    508985 | 12327 | `	return SXRET_OK;` |
|    254495 | 12328 | `}` |
|         - | 12329 | `/*` |
|         - | 12330 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12331 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12332 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12333 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12334 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12335 | ` */` |
|        36 | 12336 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12337 | `{` |
|        38 | 12338 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12339 | `	GenBlock *pBlock;` |
|         - | 12340 | `	sxu32 nIdx;` |
|         - | 12341 | `	sxi32 rc;` |
|        18 | 12342 | `	(void)iCompileFlag;` |
|        38 | 12343 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12344 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12345 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12346 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12347 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12348 | `			return SXERR_ABORT;` |
|         - | 12349 | `		}` |
|       ! 0 | 12350 | `		return SXRET_OK;` |
|         - | 12351 | `	}` |
|        38 | 12352 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12353 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12354 | `		return SXERR_ABORT;` |
|         - | 12355 | `	}` |
|        38 | 12356 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12357 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12358 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12359 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12360 | `			return SXERR_ABORT;` |
|         - | 12361 | `		}` |
|       ! 0 | 12362 | `		return SXRET_OK;` |
|         - | 12363 | `	}` |
|         - | 12364 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12365 | `	pBlock = pGen->pCurrent;` |
|        60 | 12366 | `	while( pBlock->pParent ){` |
|        49 | 12367 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12368 | `			break;` |
|         - | 12369 | `		}` |
|        23 | 12370 | `		pBlock = pBlock->pParent;` |
|         1 | 12371 | `	}` |
|        38 | 12372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12373 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12374 | `	return SXRET_OK;` |
|        20 | 12375 | `}` |
|         - | 12376 | `/*` |
|         - | 12377 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12378 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12379 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12380 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12381 | ` * compile error propagated from the parser.` |
|         - | 12382 | ` */` |
|        56 | 12383 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12384 | `{` |
|         - | 12385 | `	SyString sClassName;` |
|         - | 12386 | `	SyToken *pToken;` |
|         - | 12387 | `	SyString *pName;` |
|         - | 12388 | `	char *zDup;` |
|         - | 12389 | `	sxi32 rc;` |
|        61 | 12390 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        61 | 12391 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        61 | 12392 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        61 | 12393 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        61 | 12394 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12395 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12396 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12397 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12398 | `		return SXERR_INVALID;` |
|         - | 12399 | `	}` |
|        61 | 12400 | `	pGen->pIn++; /* '(' */` |
|        28 | 12401 | `	for(;;){` |
|         - | 12402 | `		SyBlob sResolved;` |
|        61 | 12403 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        61 | 12404 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12405 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12406 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12407 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12408 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12409 | `			return SXERR_INVALID;` |
|         - | 12410 | `		}` |
|        89 | 12411 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        56 | 12412 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        61 | 12413 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        61 | 12414 | `		SyBlobRelease(&sResolved);` |
|        61 | 12415 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        61 | 12416 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        61 | 12417 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        56 | 12418 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12419 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12420 | `			pGen->pIn++; continue;` |
|         - | 12421 | `		}` |
|        61 | 12422 | `		break;` |
|       ! 0 | 12423 | `	}` |
|         - | 12424 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12425 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|        61 | 12426 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|         3 | 12427 | `		pGen->pIn++; /* ')' */` |
|         3 | 12428 | `		return SXRET_OK;` |
|         - | 12429 | `	}` |
|        54 | 12430 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12431 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12432 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12433 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12434 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12435 | `		return SXERR_INVALID;` |
|         - | 12436 | `	}` |
|        59 | 12437 | `	pGen->pIn++; /* '$' */` |
|        59 | 12438 | `	pName = &pGen->pIn->sData;` |
|        59 | 12439 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12440 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12441 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12442 | `	pGen->pIn++;` |
|        59 | 12443 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12444 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12445 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12446 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12447 | `		return SXERR_INVALID;` |
|         - | 12448 | `	}` |
|        59 | 12449 | `	pGen->pIn++; /* ')' */` |
|        59 | 12450 | `	return SXRET_OK;` |
|        33 | 12451 | `}` |
|         - | 12452 | `/*` |
|         - | 12453 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12454 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12455 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12456 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12457 | ` * VmThrowException):` |
|         - | 12458 | ` *` |
|         - | 12459 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12460 | ` *    <try body>` |
|         - | 12461 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12462 | ` *    JMP  -> finally\|end` |
|         - | 12463 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12464 | ` *    <catch body>` |
|         - | 12465 | ` *    JMP  -> finally\|end` |
|         - | 12466 | ` *    ... more catches ...` |
|         - | 12467 | ` *  Lfin: <finally body>` |
|         - | 12468 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12469 | ` *  Lend:` |
|         - | 12470 | ` */` |
|       100 | 12471 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12472 | `{` |
|       105 | 12473 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12474 | `	GenBlock *pTry;` |
|         - | 12475 | `	VmInstr *pInstr;` |
|       105 | 12476 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12477 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12478 | `	sxi32 rc;` |
|       105 | 12479 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12480 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       105 | 12481 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       105 | 12482 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       105 | 12483 | `	pTry->pUserData = pException;` |
|       105 | 12484 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       105 | 12485 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       105 | 12486 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       105 | 12487 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       105 | 12488 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       105 | 12489 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12490 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       105 | 12491 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       105 | 12492 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       105 | 12493 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       105 | 12494 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12495 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       105 | 12496 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12497 | `	/* Catch clauses (inline) */` |
|       105 | 12498 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       100 | 12499 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        61 | 12500 | `		sxu32 k = 0;` |
|        84 | 12501 | `		for(;;){` |
|         - | 12502 | `			ph7_exception_block sCatch;` |
|         - | 12503 | `			GenBlock *pCatchBlk;` |
|       117 | 12504 | `			sxu32 idxJmp = 0;` |
|       112 | 12505 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       107 | 12506 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        33 | 12507 | `				break;` |
|         - | 12508 | `			}` |
|        61 | 12509 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        61 | 12510 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12511 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        61 | 12512 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        61 | 12513 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        61 | 12514 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        61 | 12515 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12516 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12517 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12518 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        61 | 12519 | `			pCatchBlk->pUserData = pException;` |
|        61 | 12520 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        61 | 12521 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12522 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        61 | 12523 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12524 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12525 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        61 | 12526 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        61 | 12527 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        61 | 12528 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        61 | 12529 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        61 | 12530 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        61 | 12531 | `			k++;` |
|         5 | 12532 | `		}` |
|        28 | 12533 | `	}` |
|         - | 12534 | `	/* Finally (inline) */` |
|       105 | 12535 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12536 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12537 | `		GenBlock *pFinBlk;` |
|        52 | 12538 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12539 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12540 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12541 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12542 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12543 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12544 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12545 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12546 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12547 | `		pException->iHasFinally = 1;` |
|        24 | 12548 | `	}` |
|       105 | 12549 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       105 | 12550 | `	pException->iInlined = 1;` |
|         - | 12551 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12552 | `	{` |
|       105 | 12553 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12554 | `		sxu32 *aJ; sxu32 n;` |
|       105 | 12555 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       105 | 12556 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       105 | 12557 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       161 | 12558 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        61 | 12559 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        61 | 12560 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        33 | 12561 | `		}` |
|         - | 12562 | `	}` |
|       105 | 12563 | `	SySetRelease(&aCatchJmp);` |
|       105 | 12564 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12565 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12566 | `	}` |
|       105 | 12567 | `	return SXRET_OK;` |
|        55 | 12568 | `}` |
|         - | 12569 | `/*` |
|         - | 12570 | ` * Compile a 'catch' block.` |
|         - | 12571 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12572 | ` * an object containing the exception information.` |
|         - | 12573 | ` */` |
|     24478 | 12574 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12575 | `{` |
|     24483 | 12576 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12577 | `	ph7_exception_block sCatch;` |
|         - | 12578 | `	SySet *pInstrContainer;` |
|         - | 12579 | `	SyString sClassName;` |
|         - | 12580 | `	GenBlock *pCatch;` |
|         - | 12581 | `	SyToken *pToken;` |
|         - | 12582 | `	SyString *pName;` |
|         - | 12583 | `	char *zDup;` |
|         - | 12584 | `	sxi32 rc;` |
|     24483 | 12585 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12586 | `	/* Zero the structure */` |
|     24483 | 12587 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12588 | `	/* Initialize fields */` |
|     24483 | 12589 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24483 | 12590 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24483 | 12591 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12592 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12593 | `			pToken = pGen->pIn;` |
|       ! 0 | 12594 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12595 | `				pToken--;` |
|       ! 0 | 12596 | `			}` |
|       ! 0 | 12597 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12598 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12599 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12600 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12601 | `				return SXERR_ABORT;` |
|         - | 12602 | `			}` |
|       ! 0 | 12603 | `			return SXERR_INVALID;` |
|         - | 12604 | `	}` |
|         - | 12605 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24483 | 12606 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12254 | 12607 | `	for(;;){` |
|         - | 12608 | `		SyBlob sResolved;` |
|     24513 | 12609 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24513 | 12610 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12611 | `			SyBlobRelease(&sResolved);` |
|         6 | 12612 | `			pToken = pGen->pIn;` |
|         6 | 12613 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12614 | `				pToken--;` |
|       ! 0 | 12615 | `			}` |
|         8 | 12616 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12617 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12618 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12619 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12620 | `				return SXERR_ABORT;` |
|         - | 12621 | `			}` |
|         6 | 12622 | `			return SXERR_INVALID;` |
|         - | 12623 | `		}` |
|         - | 12624 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12625 | `		 * transient SyBlob allocation. */` |
|     36761 | 12626 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24504 | 12627 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24509 | 12628 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24509 | 12629 | `		SyBlobRelease(&sResolved);` |
|     24509 | 12630 | `		if( zDup == 0 ){` |
|       ! 0 | 12631 | `			goto Mem;` |
|         - | 12632 | `		}` |
|     24509 | 12633 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24509 | 12634 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12635 | `			goto Mem;` |
|         - | 12636 | `		}` |
|         - | 12637 | `		/* Check for '\|' (multi-catch separator) */` |
|     24504 | 12638 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24504 | 12639 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        35 | 12640 | `			pGen->pIn->sData.nByte == 1 &&` |
|        30 | 12641 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        32 | 12642 | `			pGen->pIn++; /* Consume the '\|' */` |
|        32 | 12643 | `			continue;` |
|         - | 12644 | `		}` |
|     24479 | 12645 | `		break;` |
|       ! 0 | 12646 | `	}` |
|         - | 12647 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12648 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|         - | 12649 | `	 * jump straight to compiling the block below. */` |
|     24479 | 12650 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|         5 | 12651 | `		goto CatchBody;` |
|         - | 12652 | `	}` |
|     24470 | 12653 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24475 | 12654 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12655 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12656 | `			pToken = pGen->pIn;` |
|       ! 0 | 12657 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12658 | `				pToken--;` |
|       ! 0 | 12659 | `			}` |
|       ! 0 | 12660 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12661 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12662 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12663 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12664 | `				return SXERR_ABORT;` |
|         - | 12665 | `			}` |
|       ! 0 | 12666 | `			return SXERR_INVALID;` |
|         - | 12667 | `	}` |
|     24475 | 12668 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12669 | `	/* Duplicate instance name */` |
|     24475 | 12670 | `	pName = &pGen->pIn->sData;` |
|     24475 | 12671 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24475 | 12672 | `	if( zDup == 0 ){` |
|       ! 0 | 12673 | `		goto Mem;` |
|         - | 12674 | `	}` |
|     24475 | 12675 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24475 | 12676 | `	pGen->pIn++;` |
|     12237 | 12677 | `CatchBody:` |
|     24479 | 12678 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12679 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12680 | `		pToken = pGen->pIn;` |
|       ! 0 | 12681 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12682 | `			pToken--;` |
|       ! 0 | 12683 | `		}` |
|       ! 0 | 12684 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12685 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12686 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12687 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12688 | `			return SXERR_ABORT;` |
|         - | 12689 | `		}` |
|       ! 0 | 12690 | `		return SXERR_INVALID;` |
|         - | 12691 | `	}` |
|         - | 12692 | `	/* Compile the block */` |
|     24479 | 12693 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12694 | `	/* Create the catch block */` |
|     24479 | 12695 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24479 | 12696 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12697 | `		return SXERR_ABORT;` |
|         - | 12698 | `	}` |
|         - | 12699 | `	/* Swap bytecode container */` |
|     24479 | 12700 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24479 | 12701 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12702 | `	/* Compile the block */` |
|     24479 | 12703 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12704 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24479 | 12705 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12706 | `	/* Emit the DONE instruction */` |
|     24479 | 12707 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12708 | `	/* Leave the block */` |
|     24479 | 12709 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12710 | `	/* Restore the default container */` |
|     24479 | 12711 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12712 | `	/* Install the catch block */` |
|     24479 | 12713 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24479 | 12714 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12715 | `		goto Mem;` |
|         - | 12716 | `	}` |
|     24479 | 12717 | `	return SXRET_OK;` |
|       ! 0 | 12718 | `Mem:` |
|       ! 0 | 12719 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12720 | `	return SXERR_ABORT;` |
|     12244 | 12721 | `}` |
|         - | 12722 | `/*` |
|         - | 12723 | ` * Compile a 'try' block.` |
|         - | 12724 | ` * A function using an exception should be in a "try" block.` |
|         - | 12725 | ` * If the exception does not trigger, the code will continue` |
|         - | 12726 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12727 | ` * is "thrown".` |
|         - | 12728 | ` */` |
|     24636 | 12729 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12730 | `{` |
|         - | 12731 | `	ph7_exception *pException;` |
|     24641 | 12732 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12733 | `	GenBlock *pTry;` |
|         - | 12734 | `	sxu32 nJmpIdx;` |
|         - | 12735 | `	sxi32 rc;` |
|         - | 12736 | `	/* Create the exception container */` |
|     24641 | 12737 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24641 | 12738 | `	if( pException == 0 ){` |
|       ! 0 | 12739 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12740 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12741 | `		return SXERR_ABORT;` |
|         - | 12742 | `	}` |
|         - | 12743 | `	/* Zero the structure */` |
|     24641 | 12744 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12745 | `	/* Initialize fields */` |
|     24641 | 12746 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24641 | 12747 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24641 | 12748 | `	pException->iHasFinally = 0;` |
|     24641 | 12749 | `	pException->iFinallyDone = 0;` |
|     24641 | 12750 | `	pException->pVm = pGen->pVm;` |
|         - | 12751 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12752 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12753 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12754 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12755 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12756 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24641 | 12757 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       105 | 12758 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12759 | `	}` |
|         - | 12760 | `	/* Create the try block */` |
|     24541 | 12761 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24541 | 12762 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12763 | `		return SXERR_ABORT;` |
|         - | 12764 | `	}` |
|         - | 12765 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24541 | 12766 | `	pTry->pUserData = pException;` |
|         - | 12767 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24541 | 12768 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12769 | `	/* Fix the jump later when the destination is resolved */` |
|     24541 | 12770 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24541 | 12771 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12772 | `	/* Compile the block */` |
|     24541 | 12773 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24541 | 12774 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12775 | `		return SXERR_ABORT;` |
|         - | 12776 | `	}` |
|         - | 12777 | `	/* Fix forward jumps now the destination is resolved */` |
|     24541 | 12778 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12779 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24541 | 12780 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12781 | `	/* Leave the block */` |
|     24541 | 12782 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12783 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24541 | 12784 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24534 | 12785 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12786 | `		/* Compile one or more catch blocks */` |
|     24474 | 12787 | `		for(;;){` |
|     48948 | 12788 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36768 | 12789 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12240 | 12790 | `					break;` |
|         - | 12791 | `			}` |
|     24483 | 12792 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24483 | 12793 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12794 | `				return SXERR_ABORT;` |
|         - | 12795 | `			}` |
|         5 | 12796 | `		}` |
|     12235 | 12797 | `	}` |
|         - | 12798 | `	/* Compile optional finally block */` |
|     24541 | 12799 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       740 | 12800 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12801 | `		SySet *pInstrContainer;` |
|         - | 12802 | `		GenBlock *pFinBlock;` |
|       129 | 12803 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12804 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12805 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12806 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12807 | `			return SXERR_ABORT;` |
|         - | 12808 | `		}` |
|         - | 12809 | `		/* Swap bytecode container */` |
|       129 | 12810 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12811 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12812 | `		/* Compile the finally body */` |
|       129 | 12813 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12814 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12815 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12816 | `			return SXERR_ABORT;` |
|         - | 12817 | `		}` |
|         - | 12818 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12819 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12820 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12821 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12822 | `		/* Leave the block */` |
|       129 | 12823 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12824 | `		/* Restore the default container */` |
|       129 | 12825 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12826 | `		pException->iHasFinally = 1;` |
|        62 | 12827 | `	}` |
|         - | 12828 | `	/* Must have at least one catch or finally */` |
|     24541 | 12829 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12830 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12831 | `			"Cannot use try without catch or finally");` |
|         8 | 12832 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12833 | `			return SXERR_ABORT;` |
|         - | 12834 | `		}` |
|         3 | 12835 | `	}` |
|     24541 | 12836 | `	return SXRET_OK;` |
|     12323 | 12837 | `}` |
|         - | 12838 | `/*` |
|         - | 12839 | ` * Compile a switch block.` |
|         - | 12840 | ` *  (See block-comment below for more information)` |
|         - | 12841 | ` */` |
|     53648 | 12842 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12843 | `{` |
|     53653 | 12844 | `	sxi32 rc = SXRET_OK;` |
|     53653 | 12845 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12846 | `		/* Unexpected token */` |
|       ! 0 | 12847 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12848 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12849 | `			return SXERR_ABORT;` |
|         - | 12850 | `		}` |
|       ! 0 | 12851 | `		pGen->pIn++;` |
|       ! 0 | 12852 | `	}` |
|     53653 | 12853 | `	pGen->pIn++;` |
|         - | 12854 | `	/* First instruction to execute in this block. */` |
|     53653 | 12855 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12856 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12857 | `	 * or the '}' token */` |
|     38446 | 12858 | `	for(;;){` |
|     76897 | 12859 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12860 | `			/* No more input to process */` |
|       ! 0 | 12861 | `			break;` |
|         - | 12862 | `		}` |
|     76897 | 12863 | `		rc = SXRET_OK;` |
|     76897 | 12864 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      3909 | 12865 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      3855 | 12866 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12867 | `					/* Unexpected token */` |
|       ! 0 | 12868 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12869 | `						&pGen->pIn->sData);` |
|       ! 0 | 12870 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12871 | `						return SXERR_ABORT;` |
|         - | 12872 | `					}` |
|         - | 12873 | `					/* FALL THROUGH */` |
|       ! 0 | 12874 | `				}` |
|      3855 | 12875 | `				rc = SXERR_EOF;` |
|      3855 | 12876 | `				break;` |
|         - | 12877 | `			}` |
|        32 | 12878 | `		}else{` |
|         - | 12879 | `			sxi32 nKwrd;` |
|         - | 12880 | `			/* Extract the keyword */` |
|     72993 | 12881 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     72993 | 12882 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|     24903 | 12883 | `				break;` |
|         - | 12884 | `			}` |
|     23197 | 12885 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12886 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12887 | `					/* Unexpected token */` |
|       ! 0 | 12888 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12889 | `						&pGen->pIn->sData);` |
|       ! 0 | 12890 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12891 | `						return SXERR_ABORT;` |
|         - | 12892 | `					}` |
|         - | 12893 | `					/* FALL THROUGH */` |
|       ! 0 | 12894 | `				}` |
|         - | 12895 | `				/* Block compiled */` |
|         3 | 12896 | `				break;` |
|         - | 12897 | `			}` |
|         - | 12898 | `		}` |
|         - | 12899 | `		/* Compile block */` |
|     23249 | 12900 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     23249 | 12901 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12902 | `			return SXERR_ABORT;` |
|         - | 12903 | `		}` |
|         5 | 12904 | `	}` |
|     53653 | 12905 | `	return rc;` |
|     26829 | 12906 | `}` |
|         - | 12907 | `/*` |
|         - | 12908 | ` * Compile a case eXpression.` |
|         - | 12909 | ` *  (See block-comment below for more information)` |
|         - | 12910 | ` */` |
|     53628 | 12911 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12912 | `{` |
|         - | 12913 | `	SySet *pInstrContainer;` |
|         - | 12914 | `	SyToken *pEnd,*pTmp;` |
|     53633 | 12915 | `	sxi32 iNest = 0;` |
|         - | 12916 | `	sxi32 rc;` |
|         - | 12917 | `	/* Delimit the expression */` |
|     53633 | 12918 | `	pEnd = pGen->pIn;` |
|    107269 | 12919 | `	while( pEnd < pGen->pEnd ){` |
|    107269 | 12920 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12921 | `			/* Increment nesting level */` |
|         3 | 12922 | `			iNest++;` |
|    107268 | 12923 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12924 | `			/* Decrement nesting level */` |
|         3 | 12925 | `			iNest--;` |
|    107266 | 12926 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     53633 | 12927 | `			break;` |
|         - | 12928 | `		}` |
|     53641 | 12929 | `		pEnd++;` |
|         5 | 12930 | `	}` |
|     53633 | 12931 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12932 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12933 | `		if( rc == SXERR_ABORT ){` |
|         - | 12934 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12935 | `			return SXERR_ABORT;` |
|         - | 12936 | `		}` |
|       ! 0 | 12937 | `	}` |
|         - | 12938 | `	/* Swap token stream */` |
|     53633 | 12939 | `	pTmp = pGen->pEnd;` |
|     53633 | 12940 | `	pGen->pEnd = pEnd;` |
|     53633 | 12941 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     53633 | 12942 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     53633 | 12943 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12944 | `	/* Emit the done instruction */` |
|     53633 | 12945 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     53633 | 12946 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12947 | `	/* Update token stream */` |
|     53633 | 12948 | `	pGen->pIn  = pEnd;` |
|     53633 | 12949 | `	pGen->pEnd = pTmp;` |
|     53633 | 12950 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12951 | `		return SXERR_ABORT;` |
|         - | 12952 | `	}` |
|     53633 | 12953 | `	return SXRET_OK;` |
|     26819 | 12954 | `}` |
|         - | 12955 | `/*` |
|         - | 12956 | ` * Compile the smart switch statement.` |
|         - | 12957 | ` * According to the PHP language reference manual` |
|         - | 12958 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12959 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12960 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12961 | ` *  This is exactly what the switch statement is for.` |
|         - | 12962 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12963 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12964 | ` *  of the outer loop, use continue 2.` |
|         - | 12965 | ` *  Note that switch/case does loose comparision.` |
|         - | 12966 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12967 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12968 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12969 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12970 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12971 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12972 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12973 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12974 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12975 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12976 | ` *  list for the next case.` |
|         - | 12977 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12978 | ` *  or floating-point numbers and strings.` |
|         - | 12979 | ` */` |
|      3852 | 12980 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12981 | `{` |
|         - | 12982 | `	GenBlock *pSwitchBlock;` |
|         - | 12983 | `	SyToken *pTmp,*pEnd;` |
|         - | 12984 | `	ph7_switch *pSwitch;` |
|         - | 12985 | `	sxu32 nToken;` |
|         - | 12986 | `	sxu32 nLine;` |
|         - | 12987 | `	sxi32 rc;` |
|      3857 | 12988 | `	nLine = pGen->pIn->nLine;` |
|         - | 12989 | `	/* Jump the 'switch' keyword */` |
|      3857 | 12990 | `	pGen->pIn++;` |
|      3857 | 12991 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12992 | `		/* Syntax error */` |
|       ! 0 | 12993 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12994 | `		if( rc == SXERR_ABORT ){` |
|         - | 12995 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12996 | `			return SXERR_ABORT;` |
|         - | 12997 | `		}` |
|       ! 0 | 12998 | `		goto Synchronize;` |
|         - | 12999 | `	}` |
|         - | 13000 | `	/* Jump the left parenthesis '(' */` |
|      3857 | 13001 | `	pGen->pIn++;` |
|      3857 | 13002 | `	pEnd = 0; /* cc warning */` |
|         - | 13003 | `	/* Create the loop block */` |
|      5783 | 13004 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      1926 | 13005 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      3857 | 13006 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 13007 | `		return SXERR_ABORT;` |
|         - | 13008 | `	}` |
|         - | 13009 | `	/* Delimit the condition */` |
|      3857 | 13010 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      3857 | 13011 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 13012 | `		/* Empty expression */` |
|       ! 0 | 13013 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 13014 | `		if( rc == SXERR_ABORT ){` |
|         - | 13015 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 13016 | `			return SXERR_ABORT;` |
|         - | 13017 | `		}` |
|       ! 0 | 13018 | `	}` |
|         - | 13019 | `	/* Swap token streams */` |
|      3857 | 13020 | `	pTmp = pGen->pEnd;` |
|      3857 | 13021 | `	pGen->pEnd = pEnd;` |
|         - | 13022 | `	/* Compile the expression */` |
|      3857 | 13023 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      3857 | 13024 | `	if( rc == SXERR_ABORT ){` |
|         - | 13025 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 13026 | `		return SXERR_ABORT;` |
|         - | 13027 | `	}` |
|         - | 13028 | `	/* Update token stream */` |
|      3857 | 13029 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 13030 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 13031 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 13032 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13033 | `			return SXERR_ABORT;` |
|         - | 13034 | `		}` |
|       ! 0 | 13035 | `		pGen->pIn++;` |
|       ! 0 | 13036 | `	}` |
|      3857 | 13037 | `	pGen->pIn  = &pEnd[1];` |
|      3857 | 13038 | `	pGen->pEnd = pTmp;` |
|      3857 | 13039 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      3852 | 13040 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 13041 | `			pTmp = pGen->pIn;` |
|       ! 0 | 13042 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 13043 | `				pTmp--;` |
|       ! 0 | 13044 | `			}` |
|         - | 13045 | `			/* Unexpected token */` |
|       ! 0 | 13046 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 13047 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13048 | `				return SXERR_ABORT;` |
|         - | 13049 | `			}` |
|       ! 0 | 13050 | `			goto Synchronize;` |
|         - | 13051 | `	}` |
|         - | 13052 | `	/* Set the delimiter token */` |
|      3857 | 13053 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 13054 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 13055 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 13056 | `	}else{` |
|      3855 | 13057 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 13058 | `	}` |
|      3857 | 13059 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 13060 | `	/* Create the switch blocks container */` |
|      3857 | 13061 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      3857 | 13062 | `	if( pSwitch == 0 ){` |
|         - | 13063 | `		/* Abort compilation */` |
|       ! 0 | 13064 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 13065 | `		return SXERR_ABORT;` |
|         - | 13066 | `	}` |
|         - | 13067 | `	/* Zero the structure */` |
|      3857 | 13068 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 13069 | `	/* Initialize fields */` |
|      3857 | 13070 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 13071 | `	/* Emit the switch instruction */` |
|      3857 | 13072 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 13073 | `	/* Compile case blocks */` |
|     51724 | 13074 | `	for(;;){` |
|         - | 13075 | `		sxu32 nKwrd;` |
|     53655 | 13076 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 13077 | `			/* No more input to process */` |
|       ! 0 | 13078 | `			break;` |
|         - | 13079 | `		}` |
|     53655 | 13080 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 13081 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 13082 | `				/* Unexpected token */` |
|       ! 0 | 13083 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13084 | `					&pGen->pIn->sData);` |
|       ! 0 | 13085 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13086 | `					return SXERR_ABORT;` |
|         - | 13087 | `				}` |
|         - | 13088 | `				/* FALL THROUGH */` |
|       ! 0 | 13089 | `			}` |
|         - | 13090 | `			/* Block compiled */` |
|       ! 0 | 13091 | `			break;` |
|         - | 13092 | `		}` |
|         - | 13093 | `		/* Extract the keyword */` |
|     53655 | 13094 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     53655 | 13095 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 13096 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 13097 | `				/* Unexpected token */` |
|       ! 0 | 13098 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13099 | `					&pGen->pIn->sData);` |
|       ! 0 | 13100 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13101 | `					return SXERR_ABORT;` |
|         - | 13102 | `				}` |
|         - | 13103 | `				/* FALL THROUGH */` |
|       ! 0 | 13104 | `			}` |
|         - | 13105 | `			/* Block compiled */` |
|         3 | 13106 | `			break;` |
|         - | 13107 | `		}` |
|     53653 | 13108 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 13109 | `			/*` |
|         - | 13110 | `			 * Accroding to the PHP language reference manual` |
|         - | 13111 | `			 *  A special case is the default case. This case matches anything` |
|         - | 13112 | `			 *  that wasn't matched by the other cases.` |
|         - | 13113 | `			 */` |
|        25 | 13114 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 13115 | `				/* Default case already compiled */` |
|       ! 0 | 13116 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 13117 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13118 | `					return SXERR_ABORT;` |
|         - | 13119 | `				}` |
|       ! 0 | 13120 | `			}` |
|        25 | 13121 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 13122 | `			/* Compile the default block */` |
|        25 | 13123 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 13124 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13125 | `				return SXERR_ABORT;` |
|        25 | 13126 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 13127 | `				break;` |
|         1 | 13128 | `			}` |
|     53634 | 13129 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 13130 | `			ph7_case_expr sCase;` |
|         - | 13131 | `			/* Standard case block */` |
|     53633 | 13132 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 13133 | `			/* initialize the structure */` |
|     53633 | 13134 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 13135 | `			/* Compile the case expression */` |
|     53633 | 13136 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     53633 | 13137 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13138 | `				return SXERR_ABORT;` |
|         - | 13139 | `			}` |
|         - | 13140 | `			/* Compile the case block */` |
|     53633 | 13141 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 13142 | `			/* Insert in the switch container */` |
|     53633 | 13143 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     53633 | 13144 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13145 | `				return SXERR_ABORT;` |
|     53633 | 13146 | `			}else if( rc == SXERR_EOF ){` |
|      3837 | 13147 | `				break;` |
|         - | 13148 | `			}` |
|     24903 | 13149 | `		}else{` |
|         - | 13150 | `			/* Unexpected token */` |
|       ! 0 | 13151 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13152 | `				&pGen->pIn->sData);` |
|       ! 0 | 13153 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13154 | `				return SXERR_ABORT;` |
|         - | 13155 | `			}` |
|       ! 0 | 13156 | `			break;` |
|         - | 13157 | `		}` |
|         5 | 13158 | `	}` |
|         - | 13159 | `	/* Fix all jumps now the destination is resolved */` |
|      3857 | 13160 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      3857 | 13161 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 13162 | `	/* Release the loop block */` |
|      3857 | 13163 | `	GenStateLeaveBlock(pGen,0);` |
|      3857 | 13164 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 13165 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      3857 | 13166 | `		pGen->pIn++;` |
|      1926 | 13167 | `	}` |
|         - | 13168 | `	/* Statement successfully compiled */` |
|      3857 | 13169 | `	return SXRET_OK;` |
|       ! 0 | 13170 | `Synchronize:` |
|         - | 13171 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 13172 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 13173 | `		pGen->pIn++;` |
|       ! 0 | 13174 | `	}` |
|       ! 0 | 13175 | `	return SXRET_OK;` |
|      1931 | 13176 | `}` |
|         - | 13177 | `/*` |
|         - | 13178 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 13179 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 13180 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 13181 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 13182 | ` */` |
|         - | 13183 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 13184 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 13185 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 13186 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 13187 |  |
|         - | 13188 | `/*` |
|         - | 13189 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 13190 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 13191 | ` * patched entries from the pending set.` |
|         - | 13192 | ` */` |
|  48501104 | 13193 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 13194 | `{` |
|  48501109 | 13195 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 13196 | `	sxu32 nTarget;` |
|         - | 13197 | `	sxu32 *aIdx;` |
|         - | 13198 | `	sxu32 i;` |
|  48501109 | 13199 | `	if( nCur <= nBaseline ){` |
|  48501013 | 13200 | `		return;` |
|         - | 13201 | `	}` |
|       100 | 13202 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 13203 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 13204 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 13205 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 13206 | `		if( pInstr ){` |
|       108 | 13207 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 13208 | `		}` |
|        56 | 13209 | `	}` |
|       100 | 13210 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  24250557 | 13211 | `}` |
|         - | 13212 |  |
|         - | 13213 | `/*` |
|         - | 13214 | ` * By-reference out-parameters of builtin functions.` |
|         - | 13215 | ` *` |
|         - | 13216 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 13217 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 13218 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 13219 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 13220 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 13221 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 13222 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 13223 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 13224 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 13225 | ` * creates it" behaviour).` |
|         - | 13226 | ` *` |
|         - | 13227 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 13228 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 13229 | ` */` |
|   6175582 | 13230 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 13231 | `{` |
|         - | 13232 | `	static const struct {` |
|         - | 13233 | `		const char *zName;` |
|         - | 13234 | `		sxu32 nByte;` |
|         - | 13235 | `		sxu32 mask;` |
|         - | 13236 | `	} aByRef[] = {` |
|         - | 13237 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - | 13238 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13239 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13240 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13241 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13242 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 13243 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 13244 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 13245 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 13246 | `	};` |
|         - | 13247 | `	sxu32 i;` |
|   6175587 | 13248 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1622537 | 13249 | `		return 0;` |
|         - | 13250 | `	}` |
|  45112751 | 13251 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  40617204 | 13252 | `		if( pName->nByte == aByRef[i].nByte` |
|  21428067 | 13253 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     57513 | 13254 | `			return aByRef[i].mask;` |
|         - | 13255 | `		}` |
|  20279853 | 13256 | `	}` |
|   4495547 | 13257 | `	return 0;` |
|   3087796 | 13258 | `}` |
|         - | 13259 | `/*` |
|         - | 13260 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 13261 | ` *` |
|         - | 13262 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 13263 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 13264 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 13265 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 13266 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 13267 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 13268 | ` */` |
|   6175582 | 13269 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 13270 | `{` |
|         - | 13271 | `	SyToken *p, *pEnd;` |
|   6175587 | 13272 | `	pOut->zString = 0;` |
|   6175587 | 13273 | `	pOut->nByte = 0;` |
|   6175587 | 13274 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 13275 | `		return;` |
|         - | 13276 | `	}` |
|   6175587 | 13277 | `	p = pLeft->pStart;` |
|   6175587 | 13278 | `	pEnd = pLeft->pEnd;` |
|         - | 13279 | `	/* Optional single leading namespace separator (absolute path). */` |
|   6175587 | 13280 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3867 | 13281 | `		p++;` |
|      1931 | 13282 | `	}` |
|   6175587 | 13283 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1622495 | 13284 | `		return;` |
|         - | 13285 | `	}` |
|         - | 13286 | `	/* Must be a single component: nothing follows the name token. */` |
|   4553097 | 13287 | `	if( p + 1 != pEnd ){` |
|        47 | 13288 | `		return;` |
|         - | 13289 | `	}` |
|   4553055 | 13290 | `	*pOut = p->sData;` |
|   3087796 | 13291 | `}` |
|         - | 13292 | `/*` |
|         - | 13293 | ` * Generate bytecode for a given expression tree.` |
|         - | 13294 | ` * If something goes wrong while generating bytecode` |
|         - | 13295 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 13296 | ` * this function takes care of generating the appropriate` |
|         - | 13297 | ` * error message.` |
|         - | 13298 | ` */` |
|  67482662 | 13299 | `static sxi32 GenStateEmitExprCode(` |
|         - | 13300 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 13301 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 13302 | `	sxi32 iFlags /* Control flags */` |
|         - | 13303 | `	)` |
|         5 | 13304 | `{` |
|         - | 13305 | `	VmInstr *pInstr;` |
|         - | 13306 | `	sxu32 nJmpIdx;` |
|  67482667 | 13307 | `	sxi32 iP1 = 0;` |
|  67482667 | 13308 | `	sxu32 iP2 = 0;` |
|  67482667 | 13309 | `	void *p3  = 0;` |
|         - | 13310 | `	sxi32 iVmOp;` |
|         - | 13311 | `	sxi32 rc;` |
|  67482667 | 13312 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  67482667 | 13313 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  67482667 | 13314 | `	sxu32 nRhsNsBase = 0;` |
|  67482667 | 13315 | `	if( pNode->xCode ){` |
|         - | 13316 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13317 | `		/* Compile node */` |
|  40710373 | 13318 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  40710373 | 13319 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  40710373 | 13320 | `		RE_SWAP_DELIMITER(pGen);` |
|  40710373 | 13321 | `		return rc;` |
|         - | 13322 | `	}` |
|  26772299 | 13323 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13324 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13325 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13326 | `		return SXERR_ABORT;` |
|         - | 13327 | `	}` |
|  26772299 | 13328 | `	iVmOp = pNode->pOp->iVmOp;` |
|  26772299 | 13329 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13330 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13331 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13332 | `		 * and later errors are still reported. */` |
|         3 | 13333 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13334 | `			"The (unset) cast is no longer supported");` |
|         3 | 13335 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13336 | `			return SXERR_ABORT;` |
|         - | 13337 | `		}` |
|         1 | 13338 | `	}` |
|  26772299 | 13339 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        93 | 13340 | `		sxu32 nJmp = 0;` |
|         - | 13341 | `		sxu32 nNcNsBase;` |
|         - | 13342 | `		VmInstr *pInstrFix;` |
|         - | 13343 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13344 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13345 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13346 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13347 | `		 * stack slot carries a writable nIdx. */` |
|        93 | 13348 | `		if( pNode->pRight ){` |
|        93 | 13349 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13350 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        93 | 13351 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13352 | `				return rc;` |
|         - | 13353 | `			}` |
|        93 | 13354 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13355 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13356 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13357 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13358 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13359 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13360 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13361 | `			 * cascade for the actual write path stays correct. */` |
|        93 | 13362 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        93 | 13363 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13364 | `				pInstrFix->iP2 = 3;` |
|        15 | 13365 | `			}` |
|        45 | 13366 | `		}` |
|         - | 13367 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        93 | 13368 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13369 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        93 | 13370 | `		if( pNode->pLeft ){` |
|        93 | 13371 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13372 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        93 | 13373 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13374 | `				return rc;` |
|         - | 13375 | `			}` |
|        93 | 13376 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        45 | 13377 | `		}` |
|         - | 13378 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        93 | 13379 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13380 | `		/* Patch the short-circuit jump to land after the store. */` |
|        93 | 13381 | `		if( nJmp > 0 ){` |
|        93 | 13382 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        93 | 13383 | `			if( pInstrFix ){` |
|        93 | 13384 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        45 | 13385 | `			}` |
|        45 | 13386 | `		}` |
|        93 | 13387 | `		return SXRET_OK;` |
|         - | 13388 | `	}` |
|  26772209 | 13389 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13390 | `		sxu32 nJz,nJmp;` |
|         - | 13391 | `		sxu32 nTernaryNsBase;` |
|         - | 13392 | `		/* Ternary operator require special handling */` |
|         - | 13393 | `		/* Phase#1: Compile the condition */` |
|    458431 | 13394 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    458431 | 13395 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    458431 | 13396 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13397 | `			return rc;` |
|         - | 13398 | `		}` |
|         - | 13399 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13400 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13401 | `		 * condition expression, not leak past the ternary. */` |
|    458431 | 13402 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    458431 | 13403 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    458431 | 13404 | `		if( pNode->pLeft ){` |
|         - | 13405 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13406 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    454539 | 13407 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13408 | `			/* Phase#3: Compile the 'then' expression  */` |
|    454539 | 13409 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    454539 | 13410 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    454539 | 13411 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13412 | `				return rc;` |
|         - | 13413 | `			}` |
|    454539 | 13414 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    227272 | 13415 | `		}else{` |
|         - | 13416 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13417 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13418 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3897 | 13419 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3897 | 13420 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13421 | `		}` |
|         - | 13422 | `		/* Phase#4: Emit the unconditional jump */` |
|    458431 | 13423 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13424 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    458431 | 13425 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    458431 | 13426 | `		if( pInstr ){` |
|    458431 | 13427 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    229213 | 13428 | `		}` |
|    458431 | 13429 | `		if( !pNode->pLeft ){` |
|         - | 13430 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3897 | 13431 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1946 | 13432 | `		}` |
|         - | 13433 | `		/* Phase#6: Compile the 'else' expression */` |
|    458431 | 13434 | `		if( pNode->pRight ){` |
|    458431 | 13435 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    458431 | 13436 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    458431 | 13437 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13438 | `				return rc;` |
|         - | 13439 | `			}` |
|    458431 | 13440 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    229213 | 13441 | `		}` |
|    458431 | 13442 | `		if( nJmp > 0 ){` |
|         - | 13443 | `			/* Phase#7: Fix the unconditional jump */` |
|    458431 | 13444 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    458431 | 13445 | `			if( pInstr ){` |
|    458431 | 13446 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    229213 | 13447 | `			}` |
|    229213 | 13448 | `		}` |
|         - | 13449 | `		/* All done */` |
|    458431 | 13450 | `		return SXRET_OK;` |
|         - | 13451 | `	}` |
|  26313783 | 13452 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13453 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13454 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13455 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13456 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13457 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13458 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13459 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13460 | `		sxu32 nPipeNsBase;` |
|        27 | 13461 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13462 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13463 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13464 | `				"'\|>': Missing operand");` |
|       ! 0 | 13465 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13466 | `		}` |
|         - | 13467 | `		/* Argument: the LHS value. */` |
|        27 | 13468 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13469 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13470 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13471 | `			return rc;` |
|         - | 13472 | `		}` |
|        27 | 13473 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13474 | `		/* Callable: the RHS. */` |
|        27 | 13475 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13476 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13477 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13478 | `			return rc;` |
|         - | 13479 | `		}` |
|        27 | 13480 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13481 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13482 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13483 | `		return SXRET_OK;` |
|         - | 13484 | `	}` |
|  26313757 | 13485 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13486 | `	/* Generate code for the left tree */` |
|  26313757 | 13487 | `	if( pNode->pLeft ){` |
|  26290825 | 13488 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  26290825 | 13489 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13490 | `			ph7_expr_node **apNode;` |
|   6179737 | 13491 | `			int hasSpread = 0;` |
|   6179737 | 13492 | `			int hasNamed = 0;` |
|   6179737 | 13493 | `			int bAnySpread = 0;` |
|   6179737 | 13494 | `			sxu32 byRefMask = 0;` |
|         - | 13495 | `			sxi32 nArgs;` |
|         - | 13496 | `			sxi32 n;` |
|         - | 13497 | `			/* Recurse and generate bytecodes for function arguments */` |
|   6179737 | 13498 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6179737 | 13499 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13500 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13501 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13502 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   6179737 | 13503 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13504 | `				bFcc = 1;` |
|        81 | 13505 | `				nArgs = 0;` |
|        40 | 13506 | `			}` |
|         - | 13507 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13508 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13509 | `			{` |
|   6179737 | 13510 | `				int seenNamed = 0;` |
|   6179737 | 13511 | `				int seenSpread = 0;` |
|  12993007 | 13512 | `				for( n = 0; n < nArgs; ++n ){` |
|   6813277 | 13513 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4023 | 13514 | `						bAnySpread = 1;` |
|      4023 | 13515 | `						seenSpread = 1;` |
|      4023 | 13516 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13517 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13518 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13519 | `							return SXERR_SYNTAX;` |
|         5 | 13520 | `						}` |
|   6811268 | 13521 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13522 | `						seenNamed = 1;` |
|       289 | 13523 | `						hasNamed = 1;` |
|   6809117 | 13524 | `					}else if( seenNamed ){` |
|         3 | 13525 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13526 | `							"Cannot use positional argument after named argument");` |
|         3 | 13527 | `						return SXERR_SYNTAX;` |
|   6808973 | 13528 | `					}else if( seenSpread ){` |
|       ! 0 | 13529 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13530 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13531 | `						return SXERR_SYNTAX;` |
|         - | 13532 | `					}` |
|   3406640 | 13533 | `				}` |
|         - | 13534 | `			}` |
|         - | 13535 | `			/* Read-only load */` |
|   6179735 | 13536 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13537 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13538 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13539 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13540 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   6179735 | 13541 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   6179735 | 13542 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   6179730 | 13543 | `				if( pCallName->nByte == 5` |
|   3466055 | 13544 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    313959 | 13545 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6022758 | 13546 | `				}else if( pCallName->nByte == 5` |
|   3152101 | 13547 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       117 | 13548 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        56 | 13549 | `				}` |
|         - | 13550 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13551 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13552 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13553 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13554 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13555 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   6179735 | 13556 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13557 | `					SyString sBuiltin;` |
|   6175587 | 13558 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   6175587 | 13559 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3087791 | 13560 | `				}` |
|   3089865 | 13561 | `			}` |
|  12993003 | 13562 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   6813273 | 13563 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   6813273 | 13564 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13565 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13566 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13567 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13568 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13569 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13570 | `				 * (iP1=0 either way). */` |
|   6813273 | 13571 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38329 | 13572 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38329 | 13573 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19162 | 13574 | `				}` |
|   6813273 | 13575 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   6813273 | 13576 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13577 | `					return rc;` |
|         - | 13578 | `				}` |
|         - | 13579 | `				/* Each argument is an independent nullsafe scope. */` |
|   6813273 | 13580 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   6813273 | 13581 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13582 | `					/* Emit spread opcode to unpack this array argument */` |
|      4023 | 13583 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4023 | 13584 | `					hasSpread = 1;` |
|      2009 | 13585 | `				}` |
|   3406639 | 13586 | `			}` |
|         - | 13587 | `			/* Total number of given arguments */` |
|   6179735 | 13588 | `			iP1 = nArgs;` |
|   6179735 | 13589 | `			iP2 = hasSpread;` |
|         - | 13590 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13591 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   6179735 | 13592 | `			if( hasNamed ){` |
|       178 | 13593 | `				sxu32 nStrBytes = 0;` |
|         - | 13594 | `				char *zBuf;` |
|       534 | 13595 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13596 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13597 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13598 | `					}` |
|       182 | 13599 | `				}` |
|         - | 13600 | `				{` |
|       178 | 13601 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13602 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13603 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13604 | `				if( pMap ){` |
|       178 | 13605 | `					SyZero(pMap, mapSize);` |
|       178 | 13606 | `					pMap->bHasNamed = 1;` |
|       178 | 13607 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13608 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13609 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13610 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13611 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13612 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13613 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13614 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13615 | `							zBuf += nb;` |
|       141 | 13616 | `						}` |
|         - | 13617 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13618 | `					}` |
|       178 | 13619 | `					p3 = (void *)pMap;` |
|        87 | 13620 | `				}` |
|         - | 13621 | `				}` |
|        87 | 13622 | `			}` |
|         - | 13623 | `			/* Remove stale flags now */` |
|   6179735 | 13624 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3089865 | 13625 | `		}` |
|         - | 13626 | `		{` |
|         - | 13627 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13628 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13629 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13630 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13631 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13632 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13633 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13634 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  26290823 | 13635 | `			sxi32 iLeftFlags = iFlags;` |
|  26290818 | 13636 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  21606128 | 13637 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   8460745 | 13638 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7338093 | 13639 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2425615 | 13640 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1212805 | 13641 | `			}` |
|         - | 13642 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13643 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13644 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13645 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13646 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13647 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13648 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  26290818 | 13649 | `			if( pNode->pOp` |
|  37144360 | 13650 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  23998998 | 13651 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  21707126 | 13652 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4951847 | 13653 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2475921 | 13654 | `			}` |
|         - | 13655 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13656 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13657 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13658 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13659 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13660 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  26290818 | 13661 | `			if( pNode->pOp` |
|  26290823 | 13662 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    195529 | 13663 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     97762 | 13664 | `			}` |
|         - | 13665 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 13666 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 13667 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 13668 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 13669 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 13670 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 13671 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  26290818 | 13672 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC` |
|  13174171 | 13673 | `				&& pNode->pLeft && pNode->pLeft->pOp` |
|     86223 | 13674 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     57467 | 13675 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     57449 | 13676 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|        39 | 13677 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        19 | 13678 | `			}` |
|  26290823 | 13679 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13680 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13681 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     11695 | 13682 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      5845 | 13683 | `			}` |
|  26290823 | 13684 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13685 | `		}` |
|  26290823 | 13686 | `		if( rc != SXRET_OK ){` |
|        34 | 13687 | `			return rc;` |
|         - | 13688 | `		}` |
|  26290793 | 13689 | `		if( !bIsChainOp ){` |
|         - | 13690 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13691 | `			 * target the end of that LHS chain, which is right here. */` |
|  12283973 | 13692 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6141984 | 13693 | `		}` |
|  26290793 | 13694 | `		if( iVmOp == PH7_OP_CALL ){` |
|   6179735 | 13695 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6179735 | 13696 | `			if( pInstr ){` |
|   6179735 | 13697 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   4553347 | 13698 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13699 | `					sxu32 nQual;` |
|   4553347 | 13700 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13701 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13702 | `					 * so the later NEW handler (if any) can see it. */` |
|   4553347 | 13703 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13704 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13705 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13706 | `					 * imports — class imports must NOT affect function` |
|         - | 13707 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13708 | `					 * before NEW; we store the original literal index in the` |
|         - | 13709 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13710 | `					 * the unqualified name and re-qualify with class imports. */` |
|   4553347 | 13711 | `					if( bAbsolute ){` |
|      3867 | 13712 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1936 | 13713 | `					}else{` |
|   4549485 | 13714 | `						int fromImport = 0;` |
|   4549485 | 13715 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   4549485 | 13716 | `						pInstr->iP2 = (sxi32)nQual;` |
|   4549485 | 13717 | `						if( nQual != nOrig ){` |
|         - | 13718 | `							/* Record the original literal index in the arg map` |
|         - | 13719 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13720 | `							 * flag) so the NEW handler can recover the` |
|         - | 13721 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13722 | `							 * imports. */` |
|        97 | 13723 | `							if( p3 == 0 ){` |
|        97 | 13724 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        92 | 13725 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        97 | 13726 | `								if( pMap ){` |
|        97 | 13727 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        97 | 13728 | `									p3 = (void *)pMap;` |
|        46 | 13729 | `								}` |
|        46 | 13730 | `							}` |
|        97 | 13731 | `							if( p3 ){` |
|        97 | 13732 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        97 | 13733 | `								if( !fromImport ){` |
|         - | 13734 | `									/* Mark as namespace-qualified */` |
|        87 | 13735 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        41 | 13736 | `								}` |
|        46 | 13737 | `							}` |
|        46 | 13738 | `						}` |
|         - | 13739 | `					}` |
|   3903064 | 13740 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1616488 | 13741 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    823101 | 13742 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13743 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 13744 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 13745 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 13746 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 13747 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 13748 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 13749 | ``					 * the method call `$o->p()`. */`` |
|   1606595 | 13750 | `					pInstr->iP2 = 1;` |
|         - | 13751 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 13752 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 13753 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 13754 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 13755 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 13756 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 13757 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1606595 | 13758 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 13759 | `						void *pDynName = pInstr->p3;` |
|        11 | 13760 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 13761 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 13762 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 13763 | `					}` |
|    803295 | 13764 | `				}` |
|   3089870 | 13765 | `			}` |
|  23200928 | 13766 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13767 | `			ph7_expr_node **apNode;` |
|         - | 13768 | `			sxi32 n;` |
|   2875253 | 13769 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13770 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13771 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13772 | `			/* Recurse and generate bytecodes for array index */` |
|   2875253 | 13773 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5528313 | 13774 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2653065 | 13775 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2653065 | 13776 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2653065 | 13777 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13778 | `					return rc;` |
|         - | 13779 | `				}` |
|         - | 13780 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2653065 | 13781 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1326535 | 13782 | `			}` |
|   2875253 | 13783 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2653065 | 13784 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1326530 | 13785 | `			}` |
|   2875253 | 13786 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13787 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    352091 | 13788 | `				iP2 = 4;` |
|   2699210 | 13789 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13790 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13791 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23025 | 13792 | `				iP2 = 5;` |
|   2511657 | 13793 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13794 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13795 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13796 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        31 | 13797 | `				iP2 = 6;` |
|   2500134 | 13798 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13799 | `				/* Create an empty entry when the desired index is not found */` |
|    532479 | 13800 | `				iP2 = 1;` |
|    266242 | 13801 | `			}` |
|  18673439 | 13802 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13803 | `			/* POP the left node */` |
|         5 | 13804 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13805 | `		}` |
|  13145394 | 13806 | `	}` |
|  26313725 | 13807 | `	rc = SXRET_OK;` |
|  26313725 | 13808 | `	nJmpIdx = 0;` |
|         - | 13809 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13810 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13811 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  26313725 | 13812 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    391035 | 13813 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    391035 | 13814 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    391035 | 13815 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    391035 | 13816 | `			int isSpecial = 0;` |
|    391035 | 13817 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    345135 | 13818 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    345135 | 13819 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    345130 | 13820 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    314450 | 13821 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    170610 | 13822 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    103419 | 13823 | `					isSpecial = 1;` |
|     51707 | 13824 | `				}` |
|    184040 | 13825 | `			}` |
|    413985 | 13826 | `			pInstr->iP1 = 0;` |
|         - | 13827 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 13828 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 13829 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 13830 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 13831 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 13832 | `			{` |
|    598025 | 13833 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    552120 | 13834 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    368085 | 13835 | `				if( !isSpecial && !bAbsolute ){` |
|    264653 | 13836 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    132324 | 13837 | `				}` |
|         - | 13838 | `			}` |
|         - | 13839 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13840 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    368085 | 13841 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    264671 | 13842 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    264671 | 13843 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        70 | 13844 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        70 | 13845 | `					return SXRET_OK;` |
|         - | 13846 | `				}` |
|    132300 | 13847 | `			}` |
|    184007 | 13848 | `		}` |
|    229880 | 13849 | `	}` |
|         - | 13850 | `	/* Generate code for the right tree */` |
|  26290727 | 13851 | `	if( pNode->pRight ){` |
|  15092597 | 13852 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13853 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    413525 | 13854 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14885837 | 13855 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13856 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    286971 | 13857 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14535594 | 13858 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13859 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     57529 | 13860 | `			iVmOp = 0; /* No binary operator to emit */` |
|     57529 | 13861 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  14363401 | 13862 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13863 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13864 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13865 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13866 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13867 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13868 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13869 | `			sxu32 nNsJmp = 0;` |
|       108 | 13870 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13871 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  14334535 | 13872 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13873 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13874 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13875 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4815193 | 13876 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2407594 | 13877 | `		}` |
|  15092597 | 13878 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15092597 | 13879 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  15092597 | 13880 | `		if( !bIsChainOp ){` |
|         - | 13881 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13882 | `			 * operator instruction is emitted. */` |
|  10140821 | 13883 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5070408 | 13884 | `		}` |
|  15092597 | 13885 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4378915 | 13886 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4378878 | 13887 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13888 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13889 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13890 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13891 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13892 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13893 | `				 */` |
|        91 | 13894 | `				iVmOp = 0;` |
|   4378872 | 13895 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4378829 | 13896 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13897 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    780821 | 13898 | `					iP2 = 1;` |
|    390413 | 13899 | `				}else{` |
|   3598013 | 13900 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13901 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    513261 | 13902 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    513261 | 13903 | `						iP1 = pInstr->iP1;` |
|    256633 | 13904 | `					}else{` |
|   3084757 | 13905 | `						p3 = pInstr->p3;` |
|         - | 13906 | `					}` |
|         - | 13907 | `					/* POP the last dynamic load instruction */` |
|   3598013 | 13908 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13909 | `				}` |
|   2189417 | 13910 | `			}` |
|  12903142 | 13911 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13912 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13913 | `			if( pInstr ){` |
|        63 | 13914 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13915 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13916 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13917 | `					 */` |
|        19 | 13918 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13919 | `					iP1 = pInstr->iP1;` |
|        19 | 13920 | `					iP2 = pInstr->iP2;` |
|        19 | 13921 | `					p3  = pInstr->p3;` |
|        10 | 13922 | `				}else{` |
|        45 | 13923 | `					p3 = pInstr->p3;` |
|         - | 13924 | `				}` |
|        30 | 13925 | `			}` |
|        30 | 13926 | `		}` |
|   7546296 | 13927 | `	}` |
|  26290722 | 13928 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    376462 | 13929 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13930 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13931 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13932 | `		iVmOp = 0;` |
|        14 | 13933 | `	}` |
|  26290727 | 13934 | `	if( iVmOp > 0 ){` |
|  26233085 | 13935 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    195529 | 13936 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13937 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15333 | 13938 | `				iP1 = 1;` |
|      7669 | 13939 | `			}` |
|  26135323 | 13940 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13941 | `			/* Namespace-qualify the class name for NEW */ {` |
|    752547 | 13942 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    752547 | 13943 | `				VmInstr *pCallInstr = 0;` |
|    752547 | 13944 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    752231 | 13945 | `					pCallInstr = pPeek;` |
|    752231 | 13946 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    376113 | 13947 | `				}` |
|    752547 | 13948 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    737245 | 13949 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13950 | `					sxu32 nLitForClass;` |
|    737245 | 13951 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13952 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13953 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13954 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13955 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13956 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13957 | `					 * with class imports. */` |
|    737245 | 13958 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        53 | 13959 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        29 | 13960 | `					}else{` |
|    737197 | 13961 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13962 | `					}` |
|    737245 | 13963 | `					pPeek->iP1 = 0;` |
|    737245 | 13964 | `					if( !bAbsolute ){` |
|         - | 13965 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 13966 | `						 * current class — never namespace-qualify them (else` |
|         - | 13967 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 13968 | `						 * instanceof (IS_A) guard below. */` |
|    733393 | 13969 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    733393 | 13970 | `						int isSpecialNew = 0;` |
|    733393 | 13971 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    733393 | 13972 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    733393 | 13973 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    733388 | 13974 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    733432 | 13975 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    366739 | 13976 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        33 | 13977 | `								isSpecialNew = 1;` |
|        16 | 13978 | `							}` |
|    366694 | 13979 | `						}` |
|    733393 | 13980 | `						if( isSpecialNew ){` |
|        33 | 13981 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|        17 | 13982 | `						}else{` |
|    733361 | 13983 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 13984 | `						}` |
|    366699 | 13985 | `					}else{` |
|      3857 | 13986 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13987 | `					}` |
|    368620 | 13988 | `				}` |
|         - | 13989 | `			}` |
|    752547 | 13990 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    752547 | 13991 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13992 | `				VmInstr *pPrev;` |
|    752231 | 13993 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    752231 | 13994 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13995 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13996 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13997 | `					 * accumulator exactly like OP_CALL would have). */` |
|    752231 | 13998 | `					iP1 = pInstr->iP1;` |
|    752231 | 13999 | `					iP2 = pInstr->iP2;` |
|    752231 | 14000 | `					if( pInstr->p3 ){` |
|        63 | 14001 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        29 | 14002 | `					}` |
|    752231 | 14003 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    376113 | 14004 | `				}` |
|    376118 | 14005 | `			}` |
|  25661290 | 14006 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 14007 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 14008 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     76769 | 14009 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     76769 | 14010 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     76769 | 14011 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     76769 | 14012 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     76769 | 14013 | `				int isSpecialIs = 0;` |
|     76769 | 14014 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     76769 | 14015 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     76769 | 14016 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     76764 | 14017 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     76767 | 14018 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     38382 | 14019 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 14020 | `						isSpecialIs = 1;` |
|         5 | 14021 | `					}` |
|     38382 | 14022 | `				}` |
|     76769 | 14023 | `				pInstr->iP1 = 0;` |
|     76769 | 14024 | `				if( !isSpecialIs && !bAbsolute ){` |
|     76749 | 14025 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     38372 | 14026 | `				}` |
|     38387 | 14027 | `			}` |
|  25246637 | 14028 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 14029 | `			/* Prevent constant expansion for member/property names.` |
|         - | 14030 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 14031 | `			 * should not trigger constant lookup. */` |
|   4951781 | 14032 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4951781 | 14033 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4718421 | 14034 | `				pInstr->iP1 = 0;` |
|   2359208 | 14035 | `			}` |
|   4951781 | 14036 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 14037 | `				/* Static member access,remember that */` |
|    368037 | 14038 | `				iP1 = 1;` |
|    368037 | 14039 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    368037 | 14040 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    229525 | 14041 | `					p3 = pInstr->p3;` |
|    229525 | 14042 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    114760 | 14043 | `				}` |
|    184016 | 14044 | `			}` |
|         - | 14045 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 14046 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 14047 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 14048 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4951781 | 14049 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4951781 | 14050 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 14051 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4951761 | 14052 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     61327 | 14053 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4921080 | 14054 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 14055 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4890411 | 14056 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 14057 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    949263 | 14058 | `					iP2 = PH7_MEMBER_WRITE;` |
|    474629 | 14059 | `				}` |
|   2475888 | 14060 | `			}` |
|   2475888 | 14061 | `		}` |
|         - | 14062 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 14063 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 14064 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 14065 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 14066 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  26233085 | 14067 | `		if( bFcc ){` |
|        81 | 14068 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 14069 | `			iP2 = 0;` |
|        81 | 14070 | `			p3 = 0;` |
|        81 | 14071 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 14072 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 14073 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 14074 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 14075 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 14076 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 14077 | `				void *pMemberName = pInstr->p3;` |
|        37 | 14078 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 14079 | `				if( pMemberName ){` |
|       ! 0 | 14080 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 14081 | `				}` |
|        37 | 14082 | `				iP1 = 2;` |
|        19 | 14083 | `			}else{` |
|        45 | 14084 | `				iP1 = 1;` |
|         - | 14085 | `			}` |
|        40 | 14086 | `		}` |
|         - | 14087 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 14088 | `		 * This is the primary emit path for user-visible calls. */` |
|  26233085 | 14089 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6932197 | 14090 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3466096 | 14091 | `		}` |
|         - | 14092 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  26233085 | 14093 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  13116540 | 14094 | `	}` |
|  26290727 | 14095 | `	if( nJmpIdx > 0 ){` |
|         - | 14096 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    758015 | 14097 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    758015 | 14098 | `		if( pInstr ){` |
|    758015 | 14099 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    379005 | 14100 | `		}` |
|    379005 | 14101 | `	}` |
|  26290727 | 14102 | `	return rc;` |
|  33729870 | 14103 | `}` |
|         - | 14104 | `/*` |
|         - | 14105 | ` * Compile a PHP expression.` |
|         - | 14106 | ` * According to the PHP language reference manual:` |
|         - | 14107 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 14108 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 14109 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 14110 | ` *  is "anything that has a value".` |
|         - | 14111 | ` * If something goes wrong while compiling the expression,this` |
|         - | 14112 | ` * function takes care of generating the appropriate error` |
|         - | 14113 | ` * message.` |
|         - | 14114 | ` */` |
|         - | 14115 | `/*` |
|         - | 14116 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 14117 | ` *` |
|         - | 14118 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 14119 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 14120 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 14121 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 14122 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 14123 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 14124 | ` * except for() now reports php's parse error.` |
|         - | 14125 | ` */` |
| 223627388 | 14126 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 14127 | `{` |
|         - | 14128 | `	ph7_expr_node **apArg;` |
|         - | 14129 | `	sxu32 n;` |
| 223627393 | 14130 | `	if( pNode == 0 ){` |
| 157201163 | 14131 | `		return 0;` |
|         - | 14132 | `	}` |
|  66426235 | 14133 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 14134 | `		return 1;` |
|         - | 14135 | `	}` |
|  66426226 | 14136 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  66426227 | 14137 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 14138 | `		return 1;` |
|         - | 14139 | `	}` |
|  66426227 | 14140 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  75869691 | 14141 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   9443469 | 14142 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 14143 | `			return 1;` |
|         - | 14144 | `		}` |
|   4721737 | 14145 | `	}` |
|  66426227 | 14146 | `	return 0;` |
| 111813699 | 14147 | `}` |
|  15261514 | 14148 | `static sxi32 PH7_CompileExpr(` |
|         - | 14149 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14150 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 14151 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 14152 | `	)` |
|         5 | 14153 | `{` |
|         - | 14154 | `	ph7_expr_node *pRoot;` |
|         - | 14155 | `	SySet sExprNode;` |
|         - | 14156 | `	SyToken *pEnd;` |
|         - | 14157 | `	sxi32 nExpr;` |
|         - | 14158 | `	sxi32 iNest;` |
|         - | 14159 | `	sxi32 rc;` |
|         - | 14160 | `	sxu32 nNullsafeBase;` |
|         - | 14161 | `	/* Initialize worker variables */` |
|  15261519 | 14162 | `	nExpr = 0;` |
|  15261519 | 14163 | `	pRoot = 0;` |
|         - | 14164 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 14165 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  15261519 | 14166 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15261519 | 14167 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  15261519 | 14168 | `	SySetAlloc(&sExprNode,0x10);` |
|  15261519 | 14169 | `	rc = SXRET_OK;` |
|         - | 14170 | `	/* Delimit the expression */` |
|  15261519 | 14171 | `	pEnd = pGen->pIn;` |
|  15261519 | 14172 | `	iNest = 0;` |
| 119311761 | 14173 | `	while( pEnd < pGen->pEnd ){` |
| 113387867 | 14174 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14175 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4657 | 14176 | `			iNest++;` |
| 113385541 | 14177 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4665 | 14178 | `			iNest--;` |
| 113380885 | 14179 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   9338489 | 14180 | `			if( iNest <= 0 ){` |
|   9337625 | 14181 | `				break;` |
|         - | 14182 | `			}` |
|       432 | 14183 | `		}` |
| 104050247 | 14184 | `		pEnd++;` |
|         5 | 14185 | `	}` |
|  15261519 | 14186 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    643499 | 14187 | `		SyToken *pEnd2 = pGen->pIn;` |
|    643499 | 14188 | `		iNest = 0;` |
|         - | 14189 | `		/* Stop at the first comma */` |
|   1414457 | 14190 | `		while( pEnd2 < pEnd ){` |
|    770965 | 14191 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     42181 | 14192 | `				iNest++;` |
|    749877 | 14193 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     42181 | 14194 | `				iNest--;` |
|    707701 | 14195 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 14196 | `				if( iNest <= 0 ){` |
|         3 | 14197 | `					break;` |
|         - | 14198 | `				}` |
|      3027 | 14199 | `			}` |
|    770963 | 14200 | `			pEnd2++;` |
|         5 | 14201 | `		}` |
|    643499 | 14202 | `		if( pEnd2 <pEnd ){` |
|         3 | 14203 | `			pEnd = pEnd2;` |
|         1 | 14204 | `		}` |
|    321747 | 14205 | `	}` |
|  15261519 | 14206 | `	if( pEnd > pGen->pIn ){` |
|  15238567 | 14207 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 14208 | `		/* Swap delimiter */` |
|  15238567 | 14209 | `		pGen->pEnd = pEnd;` |
|         - | 14210 | `		/* Try to get an expression tree */` |
|  15238567 | 14211 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  15238562 | 14212 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  15071821 | 14213 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 14214 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 14215 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 14216 | `				"syntax error, unexpected token \",\"");` |
|         6 | 14217 | `			pGen->pEnd = pTmp;` |
|         6 | 14218 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14219 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 14220 | `				return SXERR_ABORT;` |
|         - | 14221 | `			}` |
|         6 | 14222 | `			pGen->pIn = pEnd;` |
|         6 | 14223 | `			SySetRelease(&sExprNode);` |
|         6 | 14224 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 14225 | `			return SXRET_OK;` |
|         - | 14226 | `		}` |
|  15238563 | 14227 | `		if( rc == SXRET_OK && pRoot ){` |
|  15238379 | 14228 | `			rc = SXRET_OK;` |
|  15238379 | 14229 | `			if( xTreeValidator ){` |
|         - | 14230 | `				/* Call the upper layer validator callback */` |
|    976967 | 14231 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    488481 | 14232 | `			}` |
|  15238379 | 14233 | `			if( rc != SXERR_ABORT ){` |
|         - | 14234 | `				/* Generate code for the given tree */` |
|  15238379 | 14235 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 14236 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 14237 | `				 * expression so they short-circuit to its end. */` |
|  15238379 | 14238 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   7619187 | 14239 | `			}` |
|  15238379 | 14240 | `			nExpr = 1;` |
|   7619187 | 14241 | `		}` |
|         - | 14242 | `		/* Release the whole tree */` |
|  15238563 | 14243 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 14244 | `		/* Synchronize token stream */` |
|  15238563 | 14245 | `		pGen->pEnd = pTmp;` |
|  15238563 | 14246 | `		pGen->pIn  = pEnd;` |
|  15238563 | 14247 | `		if( rc == SXERR_ABORT ){` |
|        12 | 14248 | `			SySetRelease(&sExprNode);` |
|        12 | 14249 | `			return SXERR_ABORT;` |
|         - | 14250 | `		}` |
|   7619274 | 14251 | `	}` |
|  15261505 | 14252 | `	SySetRelease(&sExprNode);` |
|  15261505 | 14253 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   7630762 | 14254 | `}` |
|         - | 14255 | `/*` |
|         - | 14256 | ` * Return a pointer to the node construct handler associated` |
|         - | 14257 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 14258 | ` */` |
|   8787072 | 14259 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 14260 | `{` |
|   8787077 | 14261 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 14262 | `		/* Numeric literal: Either real or integer */` |
|   3563719 | 14263 | `		return PH7_CompileNumLiteral;` |
|   5223363 | 14264 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 14265 | `		/* Double quoted string */` |
|    120473 | 14266 | `		return PH7_CompileString;` |
|   5102895 | 14267 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 14268 | `		/* Single quoted string */` |
|   5102775 | 14269 | `		return PH7_CompileSimpleString;` |
|       124 | 14270 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 14271 | `		/* Heredoc */` |
|        70 | 14272 | `		return PH7_CompileHereDoc;` |
|        58 | 14273 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 14274 | `		/* Nowdoc */` |
|        52 | 14275 | `		return PH7_CompileNowDoc;` |
|         8 | 14276 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 14277 | `		/* Backtick quoted string */` |
|         6 | 14278 | `		return PH7_CompileBacktic;` |
|         - | 14279 | `	}` |
|         3 | 14280 | `	return 0;` |
|   4393541 | 14281 | `}` |
|         - | 14282 | `/*` |
|         - | 14283 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 14284 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 14285 | ` * in write context" parse error.` |
|         - | 14286 | ` */` |
|     23062 | 14287 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 14288 | `{` |
|         - | 14289 | `	sxi32 rc;` |
|     23067 | 14290 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23065 | 14291 | `		return SXRET_OK;` |
|         - | 14292 | `	}` |
|         5 | 14293 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 14294 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 14295 | `		"Can't use nullsafe operator in write context");` |
|         3 | 14296 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11536 | 14297 | `}` |
|         - | 14298 | `/*` |
|         - | 14299 | ` * Compile an unset() statement.` |
|         - | 14300 | ` * unset($var, $arr[$key], ...);` |
|         - | 14301 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 14302 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 14303 | ` * parent array before extracting the element to unset.` |
|         - | 14304 | ` */` |
|     25916 | 14305 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 14306 | `{` |
|     25921 | 14307 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     25921 | 14308 | `	sxu32 nIdx = 0;` |
|         - | 14309 | `	SyString sName;` |
|         - | 14310 | `	sxi32 rc;` |
|         - | 14311 | `	/* Jump the 'unset' keyword */` |
|     25921 | 14312 | `	pGen->pIn++;` |
|         - | 14313 | `	/* Save delimiter */` |
|     25921 | 14314 | `	pTmp = pGen->pEnd;` |
|         - | 14315 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     25921 | 14316 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     25921 | 14317 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14318 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 14319 | `		SyToken *pClose;` |
|     25921 | 14320 | `		pGen->pIn++;   /* Skip '(' */` |
|     25921 | 14321 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     25921 | 14322 | `		pEnd = pClose; /* Stop at ')' */` |
|     12958 | 14323 | `	}` |
|     25921 | 14324 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 14325 | `	/* Resolve the 'unset' builtin name once */` |
|     25921 | 14326 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3829 | 14327 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3829 | 14328 | `		if( pObj == 0 ){` |
|       ! 0 | 14329 | `			return SXERR_ABORT;` |
|         - | 14330 | `		}` |
|      3829 | 14331 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3829 | 14332 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1912 | 14333 | `	}` |
|         - | 14334 | `	/* Compile each comma-separated argument */` |
|     56063 | 14335 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30147 | 14336 | `		if( pGen->pIn < pNext ){` |
|         - | 14337 | `			/*` |
|         - | 14338 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 14339 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 14340 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 14341 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 14342 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 14343 | `			 * already removes just the element/property.` |
|         - | 14344 | `			 */` |
|     30142 | 14345 | `			if( &pGen->pIn[2] == pNext` |
|     18611 | 14346 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      7085 | 14347 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 14348 | `				SyString *pVarName;` |
|     10622 | 14349 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      7078 | 14350 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      7083 | 14351 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      7083 | 14352 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 14353 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 14354 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 14355 | `					return SXERR_ABORT;` |
|         - | 14356 | `				}` |
|      7083 | 14357 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      7083 | 14358 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      7083 | 14359 | `				pGen->pIn = pNext;` |
|      7083 | 14360 | `				if( pGen->pIn < pEnd ){` |
|      4227 | 14361 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2111 | 14362 | `				}` |
|      7083 | 14363 | `				continue;` |
|         - | 14364 | `			}` |
|     23069 | 14365 | `			pGen->pEnd = pNext;` |
|     23069 | 14366 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 14367 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 14368 | `				GenStateUnsetValidator);` |
|     23069 | 14369 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14370 | `				return SXERR_ABORT;` |
|         - | 14371 | `			}` |
|     23069 | 14372 | `			if( rc != SXERR_EMPTY ){` |
|         - | 14373 | `				/* Emit call for this single argument */` |
|     23067 | 14374 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23067 | 14375 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23067 | 14376 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11531 | 14377 | `			}` |
|     11532 | 14378 | `		}` |
|         - | 14379 | `		/* Jump trailing commas */` |
|     23075 | 14380 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 14381 | `			pNext++;` |
|         1 | 14382 | `		}` |
|     23069 | 14383 | `		pGen->pIn = pNext;` |
|         5 | 14384 | `	}` |
|         - | 14385 | `	/* Skip past the closing ')' if present */` |
|     25921 | 14386 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     25921 | 14387 | `		pGen->pIn++;` |
|     12958 | 14388 | `	}` |
|         - | 14389 | `	/* Restore token stream */` |
|     25921 | 14390 | `	pGen->pEnd = pTmp;` |
|     25921 | 14391 | `	return SXRET_OK;` |
|     12963 | 14392 | `}` |
|         - | 14393 | `/*` |
|         - | 14394 | ` * PHP Language construct table.` |
|         - | 14395 | ` */` |
|         - | 14396 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 14397 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 14398 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14399 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14400 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14401 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14402 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14403 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14404 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14405 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14406 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14407 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14408 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14409 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14410 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14411 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14412 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14413 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14414 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14415 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14416 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14417 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14418 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14419 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14420 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14421 | `};` |
|         - | 14422 | `/*` |
|         - | 14423 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14424 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14425 | ` */` |
|   7390648 | 14426 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14427 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14428 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14429 | `	)` |
|         5 | 14430 | `{` |
|   7390653 | 14431 | `	sxu32 n = 0;` |
|  29224632 | 14432 | `	for(;;){` |
|  58449269 | 14433 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    430717 | 14434 | `			break;` |
|         - | 14435 | `		}` |
|  58018557 | 14436 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6959941 | 14437 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14438 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14439 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14440 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14441 | `					return 0;` |
|         - | 14442 | `				}` |
|       ! 0 | 14443 | `			}` |
|   6959936 | 14444 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11486 | 14445 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5750 | 14446 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14447 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14448 | `				return 0;` |
|         - | 14449 | `			}` |
|         - | 14450 | `			/* Return a pointer to the handler.` |
|         - | 14451 | `			*/` |
|   6959939 | 14452 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14453 | `		}` |
|  51058621 | 14454 | `		n++;` |
|         5 | 14455 | `	}` |
|    430717 | 14456 | `	if( pLookahed ){` |
|    430717 | 14457 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     68953 | 14458 | `			return PH7_CompileClassInterface;` |
|    361769 | 14459 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    315311 | 14460 | `			return PH7_CompileClass;` |
|     46463 | 14461 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7739 | 14462 | `			return PH7_CompileTrait;` |
|         - | 14463 | `		}` |
|         - | 14464 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14465 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14466 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14467 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19362 | 14468 | `	}` |
|         - | 14469 | `	/* Not a language construct */` |
|     38729 | 14470 | `	return 0;` |
|   3695329 | 14471 | `}` |
|         - | 14472 | `/*` |
|         - | 14473 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14474 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14475 | ` */` |
|     38726 | 14476 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14477 | `{` |
|         - | 14478 | `	int rc;` |
|     38731 | 14479 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38731 | 14480 | `	if( rc == FALSE ){` |
|     38618 | 14481 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15664 | 14482 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14483 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14484 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14485 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14486 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14487 | `			*/` |
|         - | 14488 | `			){` |
|     38615 | 14489 | `				rc = TRUE;` |
|     19305 | 14490 | `		}` |
|     19309 | 14491 | `	}` |
|     38731 | 14492 | `	return rc;` |
|         5 | 14493 | `}` |
|         - | 14494 | `/*` |
|         - | 14495 | ` * Compile a PHP chunk.` |
|         - | 14496 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14497 | ` * takes care of generating the appropriate error message.` |
|         - | 14498 | ` */` |
|         - | 14499 | `/*` |
|         - | 14500 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14501 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14502 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14503 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14504 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14505 | ` * intervening non-declaration statements.` |
|         - | 14506 | ` */` |
|  15920828 | 14507 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14508 | `{` |
|  15920833 | 14509 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  15920833 | 14510 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  15920833 | 14511 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14512 | `	sxu32 nIdx, n;` |
|  15920828 | 14513 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3348949 | 14514 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14515 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14516 | `		 * indexes do not map to the sidecar */` |
|  12571891 | 14517 | `		return;` |
|         - | 14518 | `	}` |
|   3348947 | 14519 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14520 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14521 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3348947 | 14522 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10048325 | 14523 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6699383 | 14524 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6691571 | 14525 | `			continue;` |
|         - | 14526 | `		}` |
|      7817 | 14527 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14528 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7805 | 14529 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7793 | 14530 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3894 | 14531 | `		}` |
|      3911 | 14532 | `	}` |
|   7960419 | 14533 | `}` |
|         - | 14534 | `/*` |
|         - | 14535 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14536 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14537 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14538 | ` */` |
|   4079808 | 14539 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14540 | `{` |
|         - | 14541 | `	char *zDup;` |
|   4079813 | 14542 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4079793 | 14543 | `		return;` |
|         - | 14544 | `	}` |
|        35 | 14545 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14546 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14547 | `	if( zDup ){` |
|        25 | 14548 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14549 | `	}` |
|        25 | 14550 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2039909 | 14551 | `}` |
|         - | 14552 | `/*` |
|         - | 14553 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14554 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14555 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14556 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14557 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14558 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14559 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14560 | ` */` |
|      7800 | 14561 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14562 | `{` |
|         - | 14563 | `	SySet *pToken;` |
|         - | 14564 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14565 | `	char *zSpan;` |
|      7805 | 14566 | `	sxi32 rc = SXRET_OK;` |
|      7805 | 14567 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14568 | `		return SXRET_OK;` |
|         - | 14569 | `	}` |
|     11705 | 14570 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3900 | 14571 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7805 | 14572 | `	if( zSpan == 0 ){` |
|       ! 0 | 14573 | `		return SXRET_OK;` |
|         - | 14574 | `	}` |
|         - | 14575 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14576 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14577 | `	 * the number of attribute declarations in the program. */` |
|      7805 | 14578 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7805 | 14579 | `	if( pToken == 0 ){` |
|       ! 0 | 14580 | `		return SXRET_OK;` |
|         - | 14581 | `	}` |
|      7805 | 14582 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7805 | 14583 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7805 | 14584 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7805 | 14585 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7805 | 14586 | `	pSavedIn = pGen->pIn;` |
|      7805 | 14587 | `	pSavedEnd = pGen->pEnd;` |
|      7809 | 14588 | `	while( pIn < pEnd ){` |
|         - | 14589 | `		ph7_attribute sAttr;` |
|         - | 14590 | `		SyBlob sFQN;` |
|      7809 | 14591 | `		int bAbsolute = 0;` |
|      7809 | 14592 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7809 | 14593 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7809 | 14594 | `		sAttr.nLine = pIn->nLine;` |
|      7809 | 14595 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14596 | `			bAbsolute = 1;` |
|        75 | 14597 | `			pIn++;` |
|        35 | 14598 | `		}` |
|      7809 | 14599 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7809 | 14600 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7809 | 14601 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7809 | 14602 | `			pIn++;` |
|      7809 | 14603 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14604 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14605 | `				pIn++;` |
|       ! 0 | 14606 | `				continue;` |
|         - | 14607 | `			}` |
|      7809 | 14608 | `			break;` |
|       ! 0 | 14609 | `		}` |
|      7809 | 14610 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14611 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14612 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14613 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14614 | `			break;` |
|         - | 14615 | `		}` |
|         - | 14616 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14617 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14618 | `		{` |
|      7809 | 14619 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7809 | 14620 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7809 | 14621 | `			char *zDup = 0;` |
|      7809 | 14622 | `			if( !bAbsolute ){` |
|      7739 | 14623 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7739 | 14624 | `				if( pImp ){` |
|       ! 0 | 14625 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14626 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14627 | `					if( zDup ){` |
|       ! 0 | 14628 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14629 | `					}` |
|      7739 | 14630 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14631 | `					SyBlob sTmp;` |
|       ! 0 | 14632 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14633 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14634 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14635 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14636 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14637 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14638 | `					if( zDup ){` |
|       ! 0 | 14639 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14640 | `					}` |
|       ! 0 | 14641 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14642 | `				}` |
|      3867 | 14643 | `			}` |
|      7809 | 14644 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7809 | 14645 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7809 | 14646 | `				if( zDup ){` |
|      7809 | 14647 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3902 | 14648 | `				}` |
|      3902 | 14649 | `			}` |
|         - | 14650 | `		}` |
|      7809 | 14651 | `		SyBlobRelease(&sFQN);` |
|      7809 | 14652 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14653 | `			SyToken *pArgsEnd;` |
|      7707 | 14654 | `			pIn++;` |
|      7707 | 14655 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15423 | 14656 | `			while( pIn < pArgsEnd ){` |
|      7721 | 14657 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7721 | 14658 | `				sxi32 iDepth = 0;` |
|         - | 14659 | `				ph7_attr_arg sArgRec;` |
|     76725 | 14660 | `				while( pArgStop < pArgsEnd ){` |
|     69025 | 14661 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14662 | `						iDepth++;` |
|     69020 | 14663 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14664 | `						iDepth--;` |
|     69010 | 14665 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14666 | `						break;` |
|         - | 14667 | `					}` |
|     69009 | 14668 | `					pArgStop++;` |
|         5 | 14669 | `				}` |
|      7721 | 14670 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7721 | 14671 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7716 | 14672 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7700 | 14673 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14674 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14675 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14676 | `					if( zN ){` |
|        19 | 14677 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14678 | `					}` |
|        19 | 14679 | `					pArgStart += 2;` |
|         9 | 14680 | `				}` |
|      7721 | 14681 | `				if( pArgStart < pArgStop ){` |
|         - | 14682 | `					SySet *pInstrContainer;` |
|      7721 | 14683 | `					pGen->pIn = pArgStart;` |
|      7721 | 14684 | `					pGen->pEnd = pArgStop;` |
|      7721 | 14685 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7721 | 14686 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7721 | 14687 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7721 | 14688 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7721 | 14689 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7721 | 14690 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14691 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14692 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14693 | `						return SXERR_ABORT;` |
|         - | 14694 | `					}` |
|      7721 | 14695 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3858 | 14696 | `				}` |
|      7721 | 14697 | `				pIn = pArgStop;` |
|      7721 | 14698 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14699 | `					pIn++;` |
|         8 | 14700 | `				}` |
|         5 | 14701 | `			}` |
|      7707 | 14702 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3851 | 14703 | `		}` |
|      7809 | 14704 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7809 | 14705 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14706 | `			pIn++;` |
|         5 | 14707 | `			continue;` |
|         - | 14708 | `		}` |
|      7805 | 14709 | `		break;` |
|       ! 0 | 14710 | `	}` |
|      7805 | 14711 | `	pGen->pIn = pSavedIn;` |
|      7805 | 14712 | `	pGen->pEnd = pSavedEnd;` |
|      7805 | 14713 | `	return SXRET_OK;` |
|      3905 | 14714 | `}` |
|         - | 14715 | `/*` |
|         - | 14716 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14717 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14718 | ` */` |
|   4079812 | 14719 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14720 | `{` |
|   4079817 | 14721 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14722 | `	sxu32 n;` |
|         - | 14723 | `	sxi32 rc;` |
|   4087605 | 14724 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7793 | 14725 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7793 | 14726 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14727 | `			return SXERR_ABORT;` |
|         - | 14728 | `		}` |
|      3899 | 14729 | `	}` |
|   4079817 | 14730 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4079817 | 14731 | `	return SXRET_OK;` |
|   2039911 | 14732 | `}` |
|         - | 14733 | `/*` |
|         - | 14734 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14735 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14736 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14737 | ` */` |
|   2057602 | 14738 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14739 | `{` |
|   2057607 | 14740 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2057607 | 14741 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2057607 | 14742 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14743 | `	sxu32 nIdx, n;` |
|         - | 14744 | `	sxi32 rc;` |
|   2057602 | 14745 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    547171 | 14746 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1510441 | 14747 | `		return SXRET_OK;` |
|         - | 14748 | `	}` |
|    547171 | 14749 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1641501 | 14750 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1094335 | 14751 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14752 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14753 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14754 | `				return SXERR_ABORT;` |
|         - | 14755 | `			}` |
|         6 | 14756 | `		}` |
|    547170 | 14757 | `	}` |
|    547171 | 14758 | `	return SXRET_OK;` |
|   1028806 | 14759 | `}` |
|  11867124 | 14760 | `static sxi32 GenStateCompileChunk(` |
|         - | 14761 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14762 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14763 | `	)` |
|         5 | 14764 | `{` |
|         - | 14765 | `	ProcLangConstruct xCons;` |
|         - | 14766 | `	sxi32 rc;` |
|  11867129 | 14767 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   6893226 | 14768 | `	for(;;){` |
|  12826793 | 14769 | `		int bStmtIsDeclare = 0;` |
|  12826793 | 14770 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14771 | `			/* No more input to process */` |
|     67689 | 14772 | `			break;` |
|         - | 14773 | `		}` |
|         - | 14774 | `		/* Bind a directly-preceding docblock to this statement */` |
|  12759109 | 14775 | `		GenStateSetPendingDoc(&(*pGen));` |
|  12759109 | 14776 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14777 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14778 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14779 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14780 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14781 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7711 | 14782 | `			int bAttrTarget = 0;` |
|      7706 | 14783 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3887 | 14784 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7653 | 14785 | `				bAttrTarget = 1;` |
|      3883 | 14786 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14787 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14788 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14789 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14790 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14791 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14792 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14793 | `					bAttrTarget = 1;` |
|        29 | 14794 | `				}` |
|        29 | 14795 | `			}` |
|      7711 | 14796 | `			if( !bAttrTarget ){` |
|       ! 0 | 14797 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14798 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14799 | `					&pGen->pIn->sData);` |
|       ! 0 | 14800 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14801 | `					break;` |
|         - | 14802 | `				}` |
|       ! 0 | 14803 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14804 | `			}` |
|      3853 | 14805 | `		}` |
|         - | 14806 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14807 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  12759109 | 14808 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7425135 | 14809 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   7425135 | 14810 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14811 | `				bStmtIsDeclare = 1;` |
|        21 | 14812 | `			}` |
|   3712565 | 14813 | `		}` |
|  12759109 | 14814 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14815 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14816 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    959637 | 14817 | `			pGen->bStrictTypesLocked = 1;` |
|    479816 | 14818 | `		}` |
|  12759109 | 14819 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14820 | `			/* Compile block */` |
|      3865 | 14821 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3865 | 14822 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14823 | `				break;` |
|         - | 14824 | `			}` |
|      1935 | 14825 | `		}else{` |
|  12755249 | 14826 | `			xCons = 0;` |
|  12755249 | 14827 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14828 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14829 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14830 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34513 | 14831 | `				xCons = PH7_CompileClassModifiers;` |
|  12737995 | 14832 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14833 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14834 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3859 | 14835 | `				xCons = PH7_CompileEnum;` |
|  12718814 | 14836 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7390653 | 14837 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14838 | `				/* Try to extract a language construct handler */` |
|   7390653 | 14839 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   7390653 | 14840 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14841 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14842 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14843 | `						&pGen->pIn->sData);` |
|         9 | 14844 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14845 | `						break;` |
|         - | 14846 | `					}` |
|         - | 14847 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14848 | `					 * this erroneous statement.` |
|         - | 14849 | `					 */` |
|         9 | 14850 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14851 | `				}` |
|   9021563 | 14852 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    406669 | 14853 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14854 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14855 | `				xCons = PH7_CompileLabel;` |
|        56 | 14856 | `			}` |
|  12755249 | 14857 | `			if( xCons == 0 ){` |
|         - | 14858 | `				/* Assume an expression an try to compile it */` |
|   5364845 | 14859 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5364845 | 14860 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14861 | `					/* Pop l-value */` |
|   5364695 | 14862 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2682345 | 14863 | `				}` |
|   2682425 | 14864 | `			}else{` |
|         - | 14865 | `				/* Go compile the sucker */` |
|   7390409 | 14866 | `				rc = xCons(&(*pGen));` |
|         - | 14867 | `			}` |
|  12755249 | 14868 | `			if( rc == SXERR_ABORT ){` |
|         - | 14869 | `				/* Request to abort compilation */` |
|        12 | 14870 | `				break;` |
|         - | 14871 | `			}` |
|         - | 14872 | `		}` |
|         - | 14873 | `		/* Ignore trailing semi-colons ';' */` |
|  21842387 | 14874 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   9083293 | 14875 | `			pGen->pIn++;` |
|         5 | 14876 | `		}` |
|  12759099 | 14877 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14878 | `			/* Compile a single statement and return */` |
|  11799435 | 14879 | `			break;` |
|         - | 14880 | `		}` |
|         - | 14881 | `		/* LOOP ONE */` |
|         - | 14882 | `		/* LOOP TWO */` |
|         - | 14883 | `		/* LOOP THREE */` |
|         - | 14884 | `		/* LOOP FOUR */` |
|         5 | 14885 | `	}` |
|         - | 14886 | `	/* Return compilation status */` |
|  11867129 | 14887 | `	return rc;` |
|         5 | 14888 | `}` |
|         - | 14889 | `/*` |
|         - | 14890 | ` * Compile a Raw PHP chunk.` |
|         - | 14891 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14892 | ` * takes care of generating the appropriate error message.` |
|         - | 14893 | ` */` |
|     67696 | 14894 | `static sxi32 PH7_CompilePHP(` |
|         - | 14895 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14896 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14897 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14898 | `	)` |
|         5 | 14899 | `{` |
|     67701 | 14900 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14901 | `	sxi32 rc;` |
|         - | 14902 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67701 | 14903 | `	SySetReset(&(*pTokenSet));` |
|     67701 | 14904 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14905 | `	/* Mark as the default token set */` |
|     67701 | 14906 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14907 | `	/* Advance the stream cursor */` |
|     67701 | 14908 | `	pGen->pRawIn++;` |
|         - | 14909 | `	/* Tokenize the PHP chunk first */` |
|     67701 | 14910 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14911 | `	/* Point to the head and tail of the token stream. */` |
|     67701 | 14912 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67701 | 14913 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67701 | 14914 | `	if( is_expr ){` |
|       ! 0 | 14915 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14916 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14917 | `			/* A simple expression,compile it */` |
|       ! 0 | 14918 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14919 | `		}` |
|         - | 14920 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14921 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14922 | `		return SXRET_OK;` |
|         - | 14923 | `	}` |
|     67701 | 14924 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14925 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14926 | `		/*` |
|         - | 14927 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14928 | `		 * According to the PHP reference manual:` |
|         - | 14929 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14930 | `		 *  immediately follow` |
|         - | 14931 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14932 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14933 | `		 * Symisc extension:` |
|         - | 14934 | `		 *   This short syntax works with all PHP opening` |
|         - | 14935 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14936 | `		 *   only short tag.` |
|         - | 14937 | `		 */` |
|         - | 14938 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14939 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14940 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14941 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14942 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14943 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14944 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14945 | `		}` |
|         3 | 14946 | `		return SXRET_OK;` |
|         - | 14947 | `	}` |
|         - | 14948 | `	/* Compile the PHP chunk */` |
|     67699 | 14949 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14950 | `	/* Fix exceptions jumps */` |
|     67699 | 14951 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14952 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67699 | 14953 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14954 | `		rc = SXERR_ABORT;` |
|         1 | 14955 | `	}` |
|         - | 14956 | `	/* Reset container */` |
|     67699 | 14957 | `	SySetReset(&pGen->aGoto);` |
|     67699 | 14958 | `	SySetReset(&pGen->aLabel);` |
|     67699 | 14959 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14960 | `	/* Compilation result */` |
|     67699 | 14961 | `	return rc;` |
|     33853 | 14962 | `}` |
|         - | 14963 | `/*` |
|         - | 14964 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14965 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14966 | ` * This is the only compile interface exported from this file.` |
|         - | 14967 | ` */` |
|     70920 | 14968 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14969 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14970 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14971 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14972 | `	)` |
|         5 | 14973 | `{` |
|         - | 14974 | `	SySet aPhpToken,aRawToken;` |
|         - | 14975 | `	ph7_gen_state *pCodeGen;` |
|         - | 14976 | `	ph7_value *pRawObj;` |
|         - | 14977 | `	sxu32 nObjIdx;` |
|         - | 14978 | `	sxi32 nRawObj;` |
|         - | 14979 | `	int is_expr;` |
|         - | 14980 | `	sxi8 bSavedStrict;` |
|         - | 14981 | `	sxi8 bSavedStrictLocked;` |
|         - | 14982 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 14983 | `	sxi32 rc;` |
|     70925 | 14984 | `	sxu32 nBaseLine = 1;` |
|     70925 | 14985 | `	if( pScript->nByte < 1 ){` |
|         - | 14986 | `		/* Nothing to compile */` |
|       ! 0 | 14987 | `		return PH7_OK;` |
|         - | 14988 | `	}` |
|         - | 14989 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 14990 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 14991 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     70925 | 14992 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 14993 | `		const char *z = pScript->zString;` |
|         3 | 14994 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 14995 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 14996 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 14997 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 14998 | `		pScript->zString = z;` |
|         3 | 14999 | `		nBaseLine = 2;` |
|         3 | 15000 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 15001 | `			return PH7_OK;` |
|         - | 15002 | `		}` |
|         1 | 15003 | `	}` |
|         - | 15004 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 15005 | `	 * file's flags so include/require restore them on return. */` |
|     70925 | 15006 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 15007 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 15008 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 15009 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 15010 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 15011 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 15012 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     70925 | 15013 | `	pSavedIn = pCodeGen->pIn;` |
|     70925 | 15014 | `	pSavedEnd = pCodeGen->pEnd;` |
|     70925 | 15015 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     70925 | 15016 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     70925 | 15017 | `	pCodeGen->bStrictTypes = 0;` |
|     70925 | 15018 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 15019 | `	/* Initialize the tokens containers */` |
|     70925 | 15020 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70925 | 15021 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70925 | 15022 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     70925 | 15023 | `	is_expr = 0;` |
|     70925 | 15024 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 15025 | `		SyToken sTmp;` |
|         - | 15026 | `		/* PHP only: -*/` |
|     57485 | 15027 | `		sTmp.nLine = 1;` |
|     57485 | 15028 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57485 | 15029 | `		sTmp.pUserData = 0;` |
|     57485 | 15030 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57485 | 15031 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57485 | 15032 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 15033 | `			/* A simple PHP expression */` |
|       ! 0 | 15034 | `			is_expr = 1;` |
|       ! 0 | 15035 | `		}` |
|     28745 | 15036 | `	}else{` |
|         - | 15037 | `		/* Tokenize raw text */` |
|     13445 | 15038 | `		SySetAlloc(&aRawToken,32);` |
|     13445 | 15039 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 15040 | `	}` |
|         - | 15041 | `	/* Process high-level tokens */` |
|     70925 | 15042 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     70925 | 15043 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     70925 | 15044 | `	rc = PH7_OK;` |
|     70925 | 15045 | `	if( is_expr ){` |
|         - | 15046 | `		/* Compile the expression */` |
|       ! 0 | 15047 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 15048 | `		goto cleanup;` |
|         - | 15049 | `	}` |
|     70925 | 15050 | `	nObjIdx = 0;` |
|         - | 15051 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 15052 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 15053 | `	 * preventing namespace bleeding across include()d files. */` |
|     70925 | 15054 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 15055 | `	/* Start the compilation process */` |
|     42185 | 15056 | `	for(;;){` |
|    152059 | 15057 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     70913 | 15058 | `			break; /* No more tokens to process */` |
|         - | 15059 | `		}` |
|     81151 | 15060 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 15061 | `			/* Compile the PHP chunk */` |
|     67701 | 15062 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67701 | 15063 | `			if( rc == SXERR_ABORT ){` |
|        15 | 15064 | `				break;` |
|         - | 15065 | `			}` |
|     67689 | 15066 | `			continue;` |
|         - | 15067 | `		}` |
|         - | 15068 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13455 | 15069 | `		nRawObj = 0;` |
|     26905 | 15070 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 15071 | `			/* Consume the raw chunk without any processing */` |
|     13455 | 15072 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13455 | 15073 | `			if( pRawObj == 0 ){` |
|       ! 0 | 15074 | `				rc = SXERR_MEM;` |
|       ! 0 | 15075 | `				break;` |
|         - | 15076 | `			}` |
|         - | 15077 | `			/* Mark as constant and emit the load constant instruction */` |
|     13455 | 15078 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13455 | 15079 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13455 | 15080 | `			++nRawObj;` |
|     13455 | 15081 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 15082 | `		}` |
|     13455 | 15083 | `		if( nRawObj > 0 ){` |
|         - | 15084 | `			/* Emit the consume instruction */` |
|     13455 | 15085 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6725 | 15086 | `		}` |
|     35465 | 15087 | `	}` |
|     35460 | 15088 | `cleanup:` |
|         - | 15089 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     70925 | 15090 | `	pCodeGen->pIn = pSavedIn;` |
|     70925 | 15091 | `	pCodeGen->pEnd = pSavedEnd;` |
|     70925 | 15092 | `	SySetRelease(&aRawToken);` |
|     70925 | 15093 | `	SySetRelease(&aPhpToken);` |
|         - | 15094 | `	/* Restore outer file's strict_types scope */` |
|     70925 | 15095 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     70925 | 15096 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     70925 | 15097 | `	return rc;` |
|     35465 | 15098 | `}` |
|         - | 15099 | `/*` |
|         - | 15100 | ` * Utility routines.Initialize the code generator.` |
|         - | 15101 | ` */` |
|      3824 | 15102 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 15103 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15104 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15105 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15106 | `	)` |
|         5 | 15107 | `{` |
|      3829 | 15108 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15109 | `	/* Zero the structure */` |
|      3829 | 15110 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 15111 | `	/* Initial state */` |
|      3829 | 15112 | `	pGen->pVm  = &(*pVm);` |
|      3829 | 15113 | `	pGen->xErr = xErr;` |
|      3829 | 15114 | `	pGen->pErrData = pErrData;` |
|      3829 | 15115 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3829 | 15116 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3829 | 15117 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3829 | 15118 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3829 | 15119 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3829 | 15120 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3829 | 15121 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3829 | 15122 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3829 | 15123 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 15124 | `	/* Error log buffer */` |
|      3829 | 15125 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 15126 | `	/* General purpose working buffer */` |
|      3829 | 15127 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 15128 | `	/* Namespace state */` |
|      3829 | 15129 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3829 | 15130 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3829 | 15131 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3829 | 15132 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15133 | `	/* Create the global scope */` |
|      3829 | 15134 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 15135 | `	/* Point to the global scope */` |
|      3829 | 15136 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3829 | 15137 | `	return SXRET_OK;` |
|         5 | 15138 | `}` |
|         - | 15139 | `/*` |
|         - | 15140 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 15141 | ` */` |
|     74284 | 15142 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 15143 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15144 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15145 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15146 | `	)` |
|         5 | 15147 | `{` |
|     74289 | 15148 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15149 | `	GenBlock *pBlock,*pParent;` |
|         - | 15150 | `	/* Reset state */` |
|     74289 | 15151 | `	SySetReset(&pGen->aLabel);` |
|     74289 | 15152 | `	SySetReset(&pGen->aGoto);` |
|     74289 | 15153 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     74289 | 15154 | `	SySetReset(&pGen->aTrivia);` |
|     74289 | 15155 | `	SySetReset(&pGen->aPendingAttrs);` |
|     74289 | 15156 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     74289 | 15157 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     74289 | 15158 | `	SyBlobRelease(&pGen->sWorker);` |
|     74289 | 15159 | `	SyBlobRelease(&pGen->sNamespace);` |
|     74289 | 15160 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     74289 | 15161 | `	SyHashRelease(&pGen->hUseImports);` |
|     74289 | 15162 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     74289 | 15163 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     74289 | 15164 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     74289 | 15165 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     74289 | 15166 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15167 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 15168 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 15169 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 15170 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 15171 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 15172 | `	 * number of unique names, which is acceptable. */` |
|         - | 15173 | `	/* Point to the global scope */` |
|     74289 | 15174 | `	pBlock = pGen->pCurrent;` |
|     74289 | 15175 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 15176 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15177 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15178 | `		pBlock = pParent;` |
|       ! 0 | 15179 | `	}` |
|     74289 | 15180 | `	pGen->xErr = xErr;` |
|     74289 | 15181 | `	pGen->pErrData = pErrData;` |
|     74289 | 15182 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     74289 | 15183 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     74289 | 15184 | `	pGen->pIn = pGen->pEnd = 0;` |
|     74289 | 15185 | `	pGen->nErr = 0;` |
|     74289 | 15186 | `	return SXRET_OK;` |
|         5 | 15187 | `}` |
|         - | 15188 | `/*` |
|         - | 15189 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 15190 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 15191 | ` *` |
|         - | 15192 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 15193 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 15194 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 15195 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 15196 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 15197 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 15198 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 15199 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 15200 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 15201 | ` *` |
|         - | 15202 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 15203 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 15204 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 15205 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 15206 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 15207 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 15208 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 15209 | ` */` |
|         4 | 15210 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 15211 | `{` |
|         5 | 15212 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15213 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 15214 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 15215 | `	*pSaved = *pGen;` |
|         5 | 15216 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 15217 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 15218 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15219 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15220 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15221 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15222 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 15223 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 15224 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 15225 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 15226 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 15227 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15228 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 15229 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 15230 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 15231 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 15232 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 15233 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 15234 | `	pGen->pTokenSet = 0;` |
|         5 | 15235 | `	pGen->nErr = 0;` |
|         5 | 15236 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 15237 | `	pGen->nCommaExprOk = 0;` |
|         5 | 15238 | `	pGen->bInGenerator = 0;` |
|         5 | 15239 | `	pGen->bStrictTypes = 0;` |
|         5 | 15240 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 15241 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 15242 | `	pGen->xErr = xErr;` |
|         5 | 15243 | `	pGen->pErrData = pErrData;` |
|         5 | 15244 | `}` |
|         - | 15245 | `/*` |
|         - | 15246 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 15247 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 15248 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 15249 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 15250 | ` */` |
|         4 | 15251 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 15252 | `{` |
|         5 | 15253 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15254 | `	GenBlock *pBlock,*pParent;` |
|         - | 15255 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 15256 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 15257 | `	 * nested global block's own fixup sets. */` |
|         5 | 15258 | `	pBlock = pGen->pCurrent;` |
|         5 | 15259 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 15260 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15261 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15262 | `		pBlock = pParent;` |
|       ! 0 | 15263 | `	}` |
|         5 | 15264 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 15265 | `	/* Release the nested unit's position containers. */` |
|         5 | 15266 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 15267 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 15268 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 15269 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 15270 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 15271 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 15272 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 15273 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 15274 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 15275 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 15276 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 15277 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 15278 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 15279 | `	hVar = pGen->hVar;` |
|         5 | 15280 | `	hLiteral = pGen->hLiteral;` |
|         5 | 15281 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 15282 | `	*pGen = *pSaved;` |
|         5 | 15283 | `	pGen->hVar = hVar;` |
|         5 | 15284 | `	pGen->hLiteral = hLiteral;` |
|         5 | 15285 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 15286 | `}` |
|         - | 15287 | `/*` |
|         - | 15288 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 15289 | ` * php's parser prints, e.g.` |
|         - | 15290 | ` *` |
|         - | 15291 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 15292 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 15293 | ` *   syntax error, unexpected end of file` |
|         - | 15294 | ` *` |
|         - | 15295 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 15296 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 15297 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 15298 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 15299 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 15300 | ` *` |
|         - | 15301 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 15302 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 15303 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 15304 | ` */` |
|       182 | 15305 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 15306 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 15307 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 15308 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 15309 | `	)` |
|         5 | 15310 | `{` |
|       187 | 15311 | `	const char *zNoun = "token";` |
|         - | 15312 | `	sxu32 nLine;` |
|       187 | 15313 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 15314 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 15315 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 15316 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 15317 | `		 * it before concluding "end of file". */` |
|        92 | 15318 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        92 | 15319 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        92 | 15320 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 15321 | `			pTok = pGen->pEnd;` |
|        44 | 15322 | `		}` |
|        44 | 15323 | `	}` |
|       187 | 15324 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       187 | 15325 | `	if( pTok == 0 ){` |
|       ! 0 | 15326 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 15327 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 15328 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 15329 | `			zExpecting);` |
|         - | 15330 | `	}` |
|       187 | 15331 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 15332 | `		zNoun = "identifier";` |
|       180 | 15333 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         8 | 15334 | `		zNoun = "variable";` |
|       171 | 15335 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        24 | 15336 | `		zNoun = "integer";` |
|       158 | 15337 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 15338 | `		zNoun = "float";` |
|       ! 0 | 15339 | `	}` |
|       187 | 15340 | `	if( zExpecting ){` |
|       118 | 15341 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 15342 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 15343 | `	}` |
|       164 | 15344 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 15345 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        96 | 15346 | `}` |
|         - | 15347 | `/*` |
|         - | 15348 | ` * Generate a compile-time error message.` |
|         - | 15349 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 15350 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 15351 | ` * abort compilation immediately.` |
|         - | 15352 | ` */` |
|     15972 | 15353 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 15354 | `{` |
|     15977 | 15355 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     15977 | 15356 | `	const char *zErr = "Error";` |
|         - | 15357 | `	SyString *pFile;` |
|         - | 15358 | `	va_list ap;` |
|         - | 15359 | `	sxi32 rc;` |
|         - | 15360 | `	/* Reset the working buffer */` |
|     15977 | 15361 | `	SyBlobReset(pWorker);` |
|         - | 15362 | `	/* Peek the processed file path if available */` |
|     15977 | 15363 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     15977 | 15364 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 15365 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 15366 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 15367 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 15368 | `		 * into execution with a 0 exit status. */` |
|       659 | 15369 | `		pGen->nErr++;` |
|       659 | 15370 | `		if( pGen->nErr > 15 ){` |
|         - | 15371 | `			/* Error count limit reached */` |
|         6 | 15372 | `			if( pGen->xErr ){` |
|         6 | 15373 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 15374 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 15375 | `				if( pFile ){` |
|         6 | 15376 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 15377 | `				}` |
|         6 | 15378 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 15379 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 15380 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 15381 | `				}` |
|         2 | 15382 | `			}` |
|         - | 15383 | `			/* Abort immediately */` |
|         6 | 15384 | `			return SXERR_ABORT;` |
|         - | 15385 | `		}` |
|       325 | 15386 | `	}` |
|     15973 | 15387 | `	if( pGen->xErr == 0 ){` |
|         - | 15388 | `		/* No available error consumer,return immediately */` |
|     15303 | 15389 | `		return SXRET_OK;` |
|         - | 15390 | `	}` |
|       675 | 15391 | `	switch(nErrType){` |
|       310 | 15392 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|         8 | 15393 | `	case E_WARNING: zErr = "Warning";     break;` |
|       346 | 15394 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 15395 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 15396 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 15397 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 15398 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        16 | 15399 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 15400 | `	default:` |
|       ! 0 | 15401 | `		break;` |
|         - | 15402 | `	}` |
|       675 | 15403 | `	rc = SXRET_OK;` |
|         - | 15404 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       675 | 15405 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       675 | 15406 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       675 | 15407 | `	va_start(ap,zFormat);` |
|       675 | 15408 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       675 | 15409 | `	va_end(ap);` |
|       675 | 15410 | `	if( pFile ){` |
|       675 | 15411 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       335 | 15412 | `	}` |
|         - | 15413 | `	/* Append a new line */` |
|       675 | 15414 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       675 | 15415 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 15416 | `		/* Consume the generated error message */` |
|       675 | 15417 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       335 | 15418 | `	}` |
|       675 | 15419 | `	return rc;` |
|      7991 | 15420 | `}` |
|         - | 15421 |  |
