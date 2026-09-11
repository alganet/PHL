# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7368/9094 lines (81.02%)

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
|   8092390 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   8092395 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  17430433 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
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
|   8092395 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2821040 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2821045 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2821191 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   2821043 |   400 | `	return SXRET_OK;` |
|   1410525 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  14641522 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  14641527 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  14641527 |   409 | `	if( pEntry == 0 ){` |
|   3773727 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|  10867805 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10867805 |   413 | `	return SXRET_OK;` |
|   7320766 |   414 | `}` |
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
|   3773722 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3773727 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3773727 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1886861 |   429 | `	}` |
|   3773727 |   430 | `	return SXRET_OK;` |
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
|   6955402 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6955407 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3477706 |   478 | `}` |
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
|   5102798 |   847 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   848 | `{` |
|   5102803 |   849 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   850 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   851 | `	ph7_value *pObj;` |
|         - |   852 | `	sxu32 nIdx;` |
|         - |   853 | `	sxi32 bHasEsc;` |
|   5102803 |   854 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   855 | `	/* Delimit the string */` |
|   5102803 |   856 | `	zIn  = pStr->zString;` |
|   5102803 |   857 | `	zEnd = &zIn[pStr->nByte];` |
|   5102803 |   858 | `	if( zIn >= zEnd ){` |
|         - |   859 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   860 | `		 * rather than reserving a new object each time. */` |
|    332907 |   861 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    332907 |   862 | `		return SXRET_OK;` |
|         - |   863 | `	}` |
|         - |   864 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|         - |   865 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|         - |   866 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|         - |   867 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|         - |   868 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|         - |   869 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|         - |   870 | `	 * their source, i.e. those with no backslash to unescape. */` |
|   4769901 |   871 | `	bHasEsc = 0;` |
|         - |   872 | `	{` |
|         - |   873 | `		const char *zScan;` |
|  57514309 |   874 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
|  52821077 |   875 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
|  26372209 |   876 | `		}` |
|         - |   877 | `	}` |
|   4769901 |   878 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   879 | `		/* Already processed,emit the load constant instruction` |
|         - |   880 | `		 * and return.` |
|         - |   881 | `		 */` |
|   2819113 |   882 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2819113 |   883 | `		return SXRET_OK;` |
|         - |   884 | `	}` |
|         - |   885 | `	/* Reserve a new constant */` |
|   1950793 |   886 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1950793 |   887 | `	if( pObj == 0 ){` |
|       ! 0 |   888 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   889 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   890 | `		return SXERR_ABORT;` |
|         - |   891 | `	}` |
|   1950793 |   892 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   893 | `	/* Compile the node */` |
|   2002521 |   894 | `	for(;;){` |
|   4005047 |   895 | `		if( zIn >= zEnd ){` |
|         - |   896 | `			/* End of input */` |
|   1950793 |   897 | `			break;` |
|         - |   898 | `		}` |
|   2054259 |   899 | `		zCur = zIn;` |
|  40579503 |   900 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  38525249 |   901 | `			zIn++;` |
|         5 |   902 | `		}` |
|   2054259 |   903 | `		if( zIn > zCur ){` |
|         - |   904 | `			/* Append raw contents*/` |
|   2015941 |   905 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   1007968 |   906 | `		}` |
|   2054259 |   907 | `		zIn++;` |
|   2054259 |   908 | `		if( zIn < zEnd ){` |
|    137935 |   909 | `			if( zIn[0] == '\\' ){` |
|         - |   910 | `				/* A literal backslash */` |
|     30665 |   911 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    122605 |   912 | `			}else if( zIn[0] == '\'' ){` |
|         - |   913 | `				/* A single quote */` |
|        15 |   914 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         8 |   915 | `			}else{` |
|         - |   916 | `				/* verbatim copy */` |
|    107261 |   917 | `				zIn--;` |
|    107261 |   918 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|    107261 |   919 | `				zIn++;` |
|         - |   920 | `			}` |
|     68965 |   921 | `		}` |
|         - |   922 | `		/* Advance the stream cursor */` |
|   2054259 |   923 | `		zIn++;` |
|         5 |   924 | `	}` |
|         - |   925 | `	/* Emit the load constant instruction */` |
|   1950793 |   926 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1950793 |   927 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|         - |   928 | `		/* Install in the literal table (only when value == source; see above) */` |
|   1874129 |   929 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    937062 |   930 | `	}` |
|         - |   931 | `	/* Node successfully compiled */` |
|   1950793 |   932 | `	return SXRET_OK;` |
|   2551404 |   933 | `}` |
|         - |   934 | `/*` |
|         - |   935 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|         - |   936 | ` *` |
|         - |   937 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|         - |   938 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|         - |   939 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|         - |   940 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|         - |   941 | ` * original source buffer — the buffer is stable through compilation.` |
|         - |   942 | ` *` |
|         - |   943 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|         - |   944 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|         - |   945 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|         - |   946 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|         - |   947 | ` *     at least N)" — line too short, or first differing byte is not` |
|         - |   948 | ` *     whitespace.` |
|         - |   949 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|         - |   950 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|         - |   951 | ` */` |
|       114 |   952 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|         4 |   953 | `{` |
|       118 |   954 | `	SyString *pIn = &pGen->pIn->sData;` |
|       118 |   955 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - |   956 | `	const char *zPrefix;` |
|         - |   957 | `	const char *z, *zEnd;` |
|         - |   958 | `	char *zBuf, *zDst;` |
|       118 |   959 | `	if( nIndent == 0 ){` |
|         - |   960 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|        72 |   961 | `		*pOut = *pIn;` |
|        72 |   962 | `		return SXRET_OK;` |
|         - |   963 | `	}` |
|         - |   964 | `	/* Recover the marker indent prefix from the original source buffer.` |
|         - |   965 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|         - |   966 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|         - |   967 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|         - |   968 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|         - |   969 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|        47 |   970 | `	zPrefix = pIn->zString + pIn->nByte;` |
|        47 |   971 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|       ! 0 |   972 | `		zPrefix += 2;` |
|       ! 0 |   973 | `	}else{` |
|        47 |   974 | `		zPrefix += 1;` |
|         - |   975 | `	}` |
|         - |   976 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|        47 |   977 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|        47 |   978 | `	if( zBuf == 0 ){` |
|       ! 0 |   979 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |   980 | `		return SXERR_ABORT;` |
|         - |   981 | `	}` |
|        47 |   982 | `	zDst = zBuf;` |
|        47 |   983 | `	z = pIn->zString;` |
|        47 |   984 | `	zEnd = z + pIn->nByte;` |
|       129 |   985 | `	while( z < zEnd ){` |
|        71 |   986 | `		const char *zLine = z;` |
|         - |   987 | `		sxu32 nLine;` |
|         - |   988 | `		int bEmpty;` |
|       799 |   989 | `		while( z < zEnd && z[0] != '\n' ){` |
|       731 |   990 | `			z++;` |
|         3 |   991 | `		}` |
|        71 |   992 | `		nLine = (sxu32)(z - zLine);` |
|        71 |   993 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|        71 |   994 | `		if( !bEmpty ){` |
|         - |   995 | `			sxu32 i;` |
|        67 |   996 | `			if( nLine < nIndent ){` |
|       ! 0 |   997 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   998 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       ! 0 |   999 | `					nIndent);` |
|       ! 0 |  1000 | `				return SXERR_ABORT;` |
|         - |  1001 | `			}` |
|       269 |  1002 | `			for( i = 0; i < nIndent; i++ ){` |
|       213 |  1003 | `				if( zLine[i] != zPrefix[i] ){` |
|        10 |  1004 | `					unsigned char c = (unsigned char)zLine[i];` |
|        10 |  1005 | `					if( c == ' ' \|\| c == '\t' ){` |
|         5 |  1006 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  1007 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|         3 |  1008 | `					}else{` |
|         7 |  1009 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  1010 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|         2 |  1011 | `							nIndent);` |
|         - |  1012 | `					}` |
|        10 |  1013 | `					return SXERR_ABORT;` |
|         - |  1014 | `				}` |
|       103 |  1015 | `			}` |
|        57 |  1016 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|        57 |  1017 | `			zDst += nLine - nIndent;` |
|        33 |  1018 | `		}else if( nLine == 1 ){` |
|         - |  1019 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|       ! 0 |  1020 | `			*zDst++ = '\r';` |
|       ! 0 |  1021 | `		}` |
|        61 |  1022 | `		if( z < zEnd ){` |
|        25 |  1023 | `			*zDst++ = '\n';` |
|        25 |  1024 | `			z++;` |
|        12 |  1025 | `		}` |
|         1 |  1026 | `	}` |
|        37 |  1027 | `	pOut->zString = zBuf;` |
|        37 |  1028 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|        37 |  1029 | `	return SXRET_OK;` |
|        61 |  1030 | `}` |
|         - |  1031 | `/*` |
|         - |  1032 | ` * Compile a nowdoc string.` |
|         - |  1033 | ` * According to the PHP language reference manual:` |
|         - |  1034 | ` *` |
|         - |  1035 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - |  1036 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - |  1037 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|         - |  1038 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|         - |  1039 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|         - |  1040 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|         - |  1041 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|         - |  1042 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|         - |  1043 | ` *  of the closing identifier.` |
|         - |  1044 | ` */` |
|        48 |  1045 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 |  1046 | `{` |
|         - |  1047 | `	SyString sStripped;` |
|         - |  1048 | `	SyString *pStr;` |
|         - |  1049 | `	ph7_value *pObj;` |
|         - |  1050 | `	sxu32 nIdx;` |
|         - |  1051 | `	sxi32 rc;` |
|        52 |  1052 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        52 |  1053 | `	if( rc != SXRET_OK ){` |
|         6 |  1054 | `		return rc;` |
|         - |  1055 | `	}` |
|        46 |  1056 | `	pStr = &sStripped;` |
|        46 |  1057 | `	nIdx = 0; /* Prevent compiler warning */` |
|        46 |  1058 | `	if( pStr->nByte <= 0 ){` |
|         - |  1059 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|         - |  1060 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|         7 |  1061 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|         7 |  1062 | `		return SXRET_OK;` |
|         - |  1063 | `	}` |
|         - |  1064 | `	/* Reserve a new constant */` |
|        40 |  1065 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        40 |  1066 | `	if( pObj == 0 ){` |
|       ! 0 |  1067 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1068 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  1069 | `		return SXERR_ABORT;` |
|         - |  1070 | `	}` |
|         - |  1071 | `	/* No processing is done here, simply a memcpy() operation */` |
|        40 |  1072 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|         - |  1073 | `	/* Emit the load constant instruction */` |
|        40 |  1074 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  1075 | `	/* Node successfully compiled */` |
|        40 |  1076 | `	return SXRET_OK;` |
|        28 |  1077 | `}` |
|         - |  1078 | `/*` |
|         - |  1079 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|         - |  1080 | ` * According to the PHP language reference manual` |
|         - |  1081 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|         - |  1082 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|         - |  1083 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|         - |  1084 | ` *  property in a string with a minimum of effort.` |
|         - |  1085 | ` *  Simple syntax` |
|         - |  1086 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|         - |  1087 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|         - |  1088 | ` *   the end of the name.` |
|         - |  1089 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|         - |  1090 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|         - |  1091 | ` *   as to simple variables.` |
|         - |  1092 | ` *  Complex (curly) syntax` |
|         - |  1093 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|         - |  1094 | ` *   of complex expressions.` |
|         - |  1095 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|         - |  1096 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|         - |  1097 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|         - |  1098 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|         - |  1099 | ` */` |
|      2638 |  1100 | `static sxi32 GenStateProcessStringExpression(` |
|         - |  1101 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1102 | `	sxu32 nLine,         /* Line number */` |
|         - |  1103 | `	const char *zIn,     /* Raw expression */` |
|         - |  1104 | `	const char *zEnd     /* End of the expression */` |
|         - |  1105 | `	)` |
|         5 |  1106 | `{` |
|         - |  1107 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1108 | `	SySet sToken;` |
|         - |  1109 | `	sxi32 rc;` |
|         - |  1110 | `	/* Initialize the token set */` |
|      2643 |  1111 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1112 | `	/* Preallocate some slots */` |
|      2643 |  1113 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1114 | `	/* Tokenize the text */` |
|      2643 |  1115 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1116 | `	/* Swap delimiter */` |
|      2643 |  1117 | `	pTmpIn  = pGen->pIn;` |
|      2643 |  1118 | `	pTmpEnd = pGen->pEnd;` |
|      2643 |  1119 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2643 |  1120 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1121 | `	/* Compile the expression */` |
|      2643 |  1122 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1123 | `	/* Restore token stream */` |
|      2643 |  1124 | `	pGen->pIn  = pTmpIn;` |
|      2643 |  1125 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1126 | `	/* Release the token set */` |
|      2643 |  1127 | `	SySetRelease(&sToken);` |
|         - |  1128 | `	/* Compilation result */` |
|      2643 |  1129 | `	return rc;` |
|         5 |  1130 | `}` |
|         - |  1131 | `/*` |
|         - |  1132 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1133 | ` */` |
|    122118 |  1134 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1135 | `{` |
|         - |  1136 | `	ph7_value *pConstObj;` |
|    122123 |  1137 | `	sxu32 nIdx = 0;` |
|         - |  1138 | `	/* Reserve a new constant */` |
|    122123 |  1139 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    122123 |  1140 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1141 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1142 | `		return 0;` |
|         - |  1143 | `	}` |
|    122123 |  1144 | `	(*pCount)++;` |
|    122123 |  1145 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1146 | `	/* Emit the load constant instruction */` |
|    122123 |  1147 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    122123 |  1148 | `	return pConstObj;` |
|     61064 |  1149 | `}` |
|         - |  1150 | `/*` |
|         - |  1151 | ` * Compile a double quoted/heredoc string.` |
|         - |  1152 | ` * According to the PHP language reference manual` |
|         - |  1153 | ` * Heredoc` |
|         - |  1154 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - |  1155 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - |  1156 | ` *  to close the quotation.` |
|         - |  1157 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - |  1158 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - |  1159 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - |  1160 | ` *  Warning` |
|         - |  1161 | ` *  It is very important to note that the line with the closing identifier must contain` |
|         - |  1162 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|         - |  1163 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|         - |  1164 | ` *  It's also important to realize that the first character before the closing identifier must` |
|         - |  1165 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|         - |  1166 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|         - |  1167 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|         - |  1168 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|         - |  1169 | ` *  the end of the current file, a parse error will result at the last line.` |
|         - |  1170 | ` *  Heredocs can not be used for initializing class properties.` |
|         - |  1171 | ` * Double quoted` |
|         - |  1172 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|         - |  1173 | ` *  Escaped characters Sequence 	Meaning` |
|         - |  1174 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|         - |  1175 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|         - |  1176 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|         - |  1177 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|         - |  1178 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|         - |  1179 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|         - |  1180 | ` *  \\ backslash` |
|         - |  1181 | ` *  \$ dollar sign` |
|         - |  1182 | ` *  \" double-quote` |
|         - |  1183 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|         - |  1184 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|         - |  1185 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|         - |  1186 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|         - |  1187 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|         - |  1188 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|         - |  1189 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|         - |  1190 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|         - |  1191 | ` * See string parsing for details.` |
|         - |  1192 | ` */` |
|         - |  1193 | `/*` |
|         - |  1194 | ` * Line number of an escape sequence inside the string body being compiled:` |
|         - |  1195 | ` * the token's line plus every newline before the escape (php reports the` |
|         - |  1196 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|         - |  1197 | ` * on the line after the '<<<' marker, hence the +1.` |
|         - |  1198 | ` */` |
|         6 |  1199 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|         3 |  1200 | `{` |
|         9 |  1201 | `	const char *z = pGen->pIn->sData.zString;` |
|         9 |  1202 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|        15 |  1203 | `	for( ; z < zPos ; z++ ){` |
|         9 |  1204 | `		if( z[0] == '\n' ){` |
|       ! 0 |  1205 | `			nLine++;` |
|       ! 0 |  1206 | `		}` |
|         6 |  1207 | `	}` |
|         9 |  1208 | `	return nLine;` |
|         3 |  1209 | `}` |
|         - |  1210 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|         - |  1211 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|    120552 |  1212 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1213 | `{` |
|    120557 |  1214 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1215 | `	const char *zIn,*zCur,*zEnd;` |
|    120557 |  1216 | `	ph7_value *pObj = 0;` |
|         - |  1217 | `	sxi32 iCons;` |
|         - |  1218 | `	sxi32 rc;` |
|         - |  1219 | `	/* Delimit the string */` |
|    120557 |  1220 | `	zIn  = pStr->zString;` |
|    120557 |  1221 | `	zEnd = &zIn[pStr->nByte];` |
|    120557 |  1222 | `	if( zIn >= zEnd ){` |
|         - |  1223 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1224 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1225 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1226 | `		 */` |
|       413 |  1227 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       413 |  1228 | `		return SXRET_OK;` |
|         - |  1229 | `	}` |
|    120149 |  1230 | `	zCur = 0;` |
|         - |  1231 | `	/* Compile the node */` |
|    120149 |  1232 | `	iCons = 0;` |
|     61389 |  1233 | `	for(;;){` |
|    163421 |  1234 | `		zCur = zIn;` |
|   1661659 |  1235 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1500881 |  1236 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        69 |  1237 | `				break;` |
|   1500754 |  1238 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2516 |  1239 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1258 |  1240 | `					break;` |
|         - |  1241 | `			}` |
|   1498243 |  1242 | `			zIn++;` |
|         5 |  1243 | `		}` |
|    163421 |  1244 | `		if( zIn > zCur ){` |
|     95283 |  1245 | `			if( pObj == 0 ){` |
|     94657 |  1246 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     94657 |  1247 | `				if( pObj == 0 ){` |
|       ! 0 |  1248 | `					return SXERR_ABORT;` |
|         - |  1249 | `				}` |
|     47326 |  1250 | `			}` |
|     95283 |  1251 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     47639 |  1252 | `		}` |
|    163421 |  1253 | `		if( zIn >= zEnd ){` |
|    120147 |  1254 | `			break;` |
|         - |  1255 | `		}` |
|     43279 |  1256 | `		if( zIn[0] == '\\' ){` |
|     40641 |  1257 | `			const char *zPtr = 0;` |
|         - |  1258 | `			sxu32 n;` |
|     40641 |  1259 | `			zIn++;` |
|     40641 |  1260 | `			if( pObj == 0 ){` |
|     27471 |  1261 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27471 |  1262 | `				if( pObj == 0 ){` |
|       ! 0 |  1263 | `					return SXERR_ABORT;` |
|         - |  1264 | `				}` |
|     13733 |  1265 | `			}` |
|     40641 |  1266 | `			if( zIn >= zEnd ){` |
|         - |  1267 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1268 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1269 | `				break;` |
|         - |  1270 | `			}` |
|     40639 |  1271 | `			n = sizeof(char); /* size of conversion */` |
|     40639 |  1272 | `			switch( zIn[0] ){` |
|        15 |  1273 | `			case '$':` |
|         - |  1274 | `				/* Dollar sign */` |
|        33 |  1275 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        33 |  1276 | `				break;` |
|        57 |  1277 | `			case '\\':` |
|         - |  1278 | `				/* A literal backslash */` |
|       119 |  1279 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       119 |  1280 | `				break;` |
|         1 |  1281 | `			case 'e':` |
|         - |  1282 | `				/* Escape (ESC) ASCII code 27 */` |
|         3 |  1283 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|         3 |  1284 | `				break;` |
|         4 |  1285 | `			case 'f':` |
|         - |  1286 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|         9 |  1287 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|         9 |  1288 | `				break;` |
|     17728 |  1289 | `			case 'n':` |
|         - |  1290 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     35461 |  1291 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     35461 |  1292 | `				break;` |
|        27 |  1293 | `			case 'r':` |
|         - |  1294 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1295 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1296 | `				break;` |
|      1943 |  1297 | `			case 't':` |
|         - |  1298 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      3891 |  1299 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      3891 |  1300 | `				break;` |
|         3 |  1301 | `			case 'v':` |
|         - |  1302 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|         7 |  1303 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|         7 |  1304 | `				break;` |
|       141 |  1305 | `			case '"':` |
|       287 |  1306 | `				if( bHeredoc ){` |
|         - |  1307 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|         5 |  1308 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|         3 |  1309 | `				}else{` |
|         - |  1310 | `					/* Double quote */` |
|       283 |  1311 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|         - |  1312 | `				}` |
|       287 |  1313 | `				break;` |
|        24 |  1314 | `			case '0': case '1': case '2': case '3':` |
|         - |  1315 | `			case '4': case '5': case '6': case '7': {` |
|         - |  1316 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|         - |  1317 | `				 * warns and wraps to the low byte, matching php 8. */` |
|        50 |  1318 | `				int c = 0;` |
|         - |  1319 | `				char cOut;` |
|       144 |  1320 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|       122 |  1321 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|        14 |  1322 | `						break;` |
|         - |  1323 | `					}` |
|        96 |  1324 | `					c = c * 8 + (zPtr[0] - '0');` |
|        49 |  1325 | `				}` |
|        50 |  1326 | `				if( c > 0xFF ){` |
|         - |  1327 | `					SyString sSeq;` |
|         3 |  1328 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|         3 |  1329 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1330 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|         3 |  1331 | `					c &= 0xFF;` |
|         1 |  1332 | `				}` |
|        50 |  1333 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|        50 |  1334 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|        50 |  1335 | `				n = (sxu32)(zPtr-zIn);` |
|        50 |  1336 | `				break;` |
|         - |  1337 | `			}` |
|       349 |  1338 | `			case 'x':` |
|      1047 |  1339 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|         - |  1340 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|       696 |  1341 | `					int c = SyHexToint(zIn[1]);` |
|         - |  1342 | `					char cOut;` |
|       696 |  1343 | `					n += sizeof(char);` |
|       696 |  1344 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|       692 |  1345 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|       692 |  1346 | `						n += sizeof(char);` |
|       345 |  1347 | `					}` |
|       696 |  1348 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|       696 |  1349 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       349 |  1350 | `				}else{` |
|         - |  1351 | `					/* Not an escape: keep the backslash, as php does */` |
|         5 |  1352 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|         - |  1353 | `				}` |
|       700 |  1354 | `				break;` |
|         9 |  1355 | `			case 'u':` |
|        18 |  1356 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|        22 |  1357 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|         - |  1358 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|         - |  1359 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|         - |  1360 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|         - |  1361 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|         - |  1362 | `					 * followed by {$...} curly interpolation. */` |
|        15 |  1363 | `					sxu32 nCp = 0;` |
|        15 |  1364 | `					zPtr = &zIn[2];` |
|        59 |  1365 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|        46 |  1366 | `						if( nCp <= 0x10FFFF ){` |
|         - |  1367 | `							/* stop accumulating once out of range: keeps a long` |
|         - |  1368 | `							 * digit run from wrapping sxu32 */` |
|        46 |  1369 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|        22 |  1370 | `						}` |
|        46 |  1371 | `						zPtr++;` |
|         2 |  1372 | `					}` |
|        15 |  1373 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|         - |  1374 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|         - |  1375 | `						 * malformed sequence so later errors are still reported. */` |
|         3 |  1376 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1377 | `							"Invalid UTF-8 codepoint escape sequence");` |
|         3 |  1378 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1379 | `							return SXERR_ABORT;` |
|         - |  1380 | `						}` |
|         3 |  1381 | `						n = (sxu32)(zPtr-zIn);` |
|         3 |  1382 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|         3 |  1383 | `							n += sizeof(char);` |
|         1 |  1384 | `						}` |
|         3 |  1385 | `						break;` |
|         - |  1386 | `					}` |
|        12 |  1387 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|        12 |  1388 | `					if( nCp > 0x10FFFF ){` |
|         3 |  1389 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1390 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|         3 |  1391 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1392 | `							return SXERR_ABORT;` |
|         - |  1393 | `						}` |
|         3 |  1394 | `						break;` |
|         - |  1395 | `					}` |
|         - |  1396 | `					{` |
|         - |  1397 | `						char zUtf[4];` |
|         9 |  1398 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|         9 |  1399 | `						SX_WRITE_UTF8(zOut,nCp);` |
|         9 |  1400 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|         - |  1401 | `					}` |
|         5 |  1402 | `				}else{` |
|         - |  1403 | `					/* Not an escape: keep the backslash, as php does */` |
|         7 |  1404 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|         - |  1405 | `				}` |
|        15 |  1406 | `				break;` |
|        16 |  1407 | `			default:` |
|         - |  1408 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|         - |  1409 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|         - |  1410 | `				 * in the source buffer — one batched append. */` |
|        33 |  1411 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|        32 |  1412 | `				break;` |
|         - |  1413 | `			}` |
|         - |  1414 | `			/* Advance the stream cursor */` |
|     40639 |  1415 | `			zIn += n;` |
|     40639 |  1416 | `			continue;` |
|         - |  1417 | `		}` |
|      2643 |  1418 | `		if( zIn[0] == '{' ){` |
|         - |  1419 | `			/* Curly syntax */` |
|         - |  1420 | `			const char *zExpr;` |
|       135 |  1421 | `			sxi32 iNest = 1;` |
|       135 |  1422 | `			zIn++;` |
|       135 |  1423 | `			zExpr = zIn;` |
|         - |  1424 | `			/* Synchronize with the next closing curly braces */` |
|      1323 |  1425 | `			while( zIn < zEnd ){` |
|      1323 |  1426 | `				if( zIn[0] == '{' ){` |
|         - |  1427 | `					/* Increment nesting level */` |
|         3 |  1428 | `					iNest++;` |
|      1322 |  1429 | `				}else if(zIn[0] == '}' ){` |
|         - |  1430 | `					/* Decrement nesting level */` |
|       137 |  1431 | `					iNest--;` |
|       137 |  1432 | `					if( iNest <= 0 ){` |
|       135 |  1433 | `						break;` |
|         - |  1434 | `					}` |
|         1 |  1435 | `				}` |
|      1191 |  1436 | `				zIn++;` |
|         3 |  1437 | `			}` |
|         - |  1438 | `			/* Process the expression */` |
|       135 |  1439 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|       135 |  1440 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1441 | `				return SXERR_ABORT;` |
|         - |  1442 | `			}` |
|       135 |  1443 | `			if( rc != SXERR_EMPTY ){` |
|       135 |  1444 | `				++iCons;` |
|        66 |  1445 | `			}` |
|       135 |  1446 | `			if( zIn < zEnd ){` |
|         - |  1447 | `				/* Jump the trailing curly */` |
|       135 |  1448 | `				zIn++;` |
|        66 |  1449 | `			}` |
|        69 |  1450 | `		}else{` |
|         - |  1451 | `			/* Simple syntax */` |
|      2511 |  1452 | `			const char *zExpr = zIn;` |
|         - |  1453 | `			/* Assemble variable name */` |
|      1278 |  1454 | `			for(;;){` |
|         - |  1455 | `				/* Jump leading dollars */` |
|      5067 |  1456 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2511 |  1457 | `					zIn++;` |
|         5 |  1458 | `				}` |
|      1278 |  1459 | `				for(;;){` |
|     13087 |  1460 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9253 |  1461 | `						zIn++;` |
|         5 |  1462 | `					}` |
|      2561 |  1463 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1464 | `						/* UTF-8 stream */` |
|       ! 0 |  1465 | `						zIn++;` |
|       ! 0 |  1466 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1467 | `							zIn++;` |
|       ! 0 |  1468 | `						}` |
|       ! 0 |  1469 | `						continue;` |
|         - |  1470 | `					}` |
|      2561 |  1471 | `					break;` |
|       ! 0 |  1472 | `				}` |
|      2561 |  1473 | `				if( zIn >= zEnd ){` |
|       271 |  1474 | `					break;` |
|         - |  1475 | `				}` |
|      2295 |  1476 | `				if( zIn[0] == '[' ){` |
|        12 |  1477 | `					sxi32 iSquare = 1;` |
|        12 |  1478 | `					zIn++;` |
|        28 |  1479 | `					while( zIn < zEnd ){` |
|        28 |  1480 | `						if( zIn[0] == '[' ){` |
|       ! 0 |  1481 | `							iSquare++;` |
|        28 |  1482 | `						}else if (zIn[0] == ']' ){` |
|        12 |  1483 | `							iSquare--;` |
|        12 |  1484 | `							if( iSquare <= 0 ){` |
|        12 |  1485 | `								break;` |
|         - |  1486 | `							}` |
|       ! 0 |  1487 | `						}` |
|        18 |  1488 | `						zIn++;` |
|         2 |  1489 | `					}` |
|        12 |  1490 | `					if( zIn < zEnd ){` |
|        12 |  1491 | `						zIn++;` |
|         5 |  1492 | `					}` |
|        12 |  1493 | `					break;` |
|      2285 |  1494 | `				}else if(zIn[0] == '{' ){` |
|         6 |  1495 | `					sxi32 iCurly = 1;` |
|         6 |  1496 | `					zIn++;` |
|        18 |  1497 | `					while( zIn < zEnd ){` |
|        16 |  1498 | `						if( zIn[0] == '{' ){` |
|       ! 0 |  1499 | `							iCurly++;` |
|        16 |  1500 | `						}else if (zIn[0] == '}' ){` |
|         3 |  1501 | `							iCurly--;` |
|         3 |  1502 | `							if( iCurly <= 0 ){` |
|         3 |  1503 | `								break;` |
|         - |  1504 | `							}` |
|       ! 0 |  1505 | `						}` |
|        14 |  1506 | `						zIn++;` |
|         2 |  1507 | `					}` |
|         6 |  1508 | `					if( zIn < zEnd ){` |
|         3 |  1509 | `						zIn++;` |
|         1 |  1510 | `					}` |
|         6 |  1511 | `					break;` |
|      2281 |  1512 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1513 | `					/* Member access operator '->' */` |
|        53 |  1514 | `					zIn += 2;` |
|      2256 |  1515 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1516 | `					/* Static member access operator '::' */` |
|       ! 0 |  1517 | `					zIn += 2;` |
|       ! 0 |  1518 | `				}else{` |
|      1118 |  1519 | `					break;` |
|         - |  1520 | `				}` |
|         3 |  1521 | `			}` |
|         - |  1522 | `			/*` |
|         - |  1523 | `			 * "$a[name]" — php's SIMPLE syntax takes an unquoted subscript as the string key` |
|         - |  1524 | `			 * 'name', never as a constant. PH7 handed "$a[name]" straight to the expression` |
|         - |  1525 | `			 * compiler, where the bare word only resolved because an unknown constant used to` |
|         - |  1526 | `			 * fall back to its own name as a string. With undefined constants now a real` |
|         - |  1527 | `			 * Error, quote the key here so the simple syntax keeps meaning what php means.` |
|         - |  1528 | `			 * A numeric ($a[0]) or variable ($a[$k]) subscript is already unambiguous.` |
|         - |  1529 | `			 */` |
|         - |  1530 | `			{` |
|      2511 |  1531 | `				const char *zBr = zExpr;` |
|     14383 |  1532 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     11877 |  1533 | `					zBr++;` |
|         5 |  1534 | `				}` |
|      2511 |  1535 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
|        12 |  1536 | `					const char *zKey = &zBr[1];` |
|        12 |  1537 | `					const char *zKeyEnd = &zIn[-1];` |
|        12 |  1538 | `					const char *zScan = zKey;` |
|        12 |  1539 | `					int bBare = (zKey < zKeyEnd) && !SyisDigit(zKey[0]);` |
|        20 |  1540 | `					while( bBare && zScan < zKeyEnd ){` |
|         9 |  1541 | `						if( !SyisAlphaNum(zScan[0]) && zScan[0] != '_' ){` |
|       ! 0 |  1542 | `							bBare = 0;` |
|       ! 0 |  1543 | `						}` |
|         9 |  1544 | `						zScan++;` |
|         1 |  1545 | `					}` |
|        12 |  1546 | `					if( bBare ){` |
|         - |  1547 | `						SyBlob sSub;` |
|         3 |  1548 | `						SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|         3 |  1549 | `						SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|         3 |  1550 | `						SyBlobAppend(&sSub,"['",2);` |
|         3 |  1551 | `						SyBlobAppend(&sSub,zKey,(sxu32)(zKeyEnd - zKey));` |
|         3 |  1552 | `						SyBlobAppend(&sSub,"']",2);` |
|         4 |  1553 | `						rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|         2 |  1554 | `							(const char *)SyBlobData(&sSub),` |
|         2 |  1555 | `							(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|         3 |  1556 | `						SyBlobRelease(&sSub);` |
|         3 |  1557 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1558 | `							return SXERR_ABORT;` |
|         - |  1559 | `						}` |
|         3 |  1560 | `						if( rc != SXERR_EMPTY ){` |
|         3 |  1561 | `							++iCons;` |
|         1 |  1562 | `						}` |
|         3 |  1563 | `						pObj = 0;` |
|         3 |  1564 | `						continue;` |
|         - |  1565 | `					}` |
|         4 |  1566 | `				}` |
|         - |  1567 | `			}` |
|         - |  1568 | `			/*` |
|         - |  1569 | `			 * "${name}" is php's DEPRECATED (8.2) spelling of the variable $name — NOT an` |
|         - |  1570 | `			 * expression. PH7 handed the whole "${name}" to the expression compiler, whose` |
|         - |  1571 | ``			 * `${expr}` (variable-variable) rule evaluated the bare word `name`; that only`` |
|         - |  1572 | `			 * appeared to work while an unknown bare word fell back to its own name as a` |
|         - |  1573 | `			 * string. Now that an undefined constant is a real Error, rewrite the simple` |
|         - |  1574 | `			 * form to the variable it means. "${$x}" keeps the variable-variable meaning.` |
|         - |  1575 | `			 */` |
|      2504 |  1576 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' && zIn[-1] == '}'` |
|         8 |  1577 | `				&& zExpr[2] != '$' ){` |
|         3 |  1578 | `				const char *zName = &zExpr[2];` |
|         3 |  1579 | `				const char *zStop = &zIn[-1];` |
|         3 |  1580 | `				const char *zScan = zName;` |
|        12 |  1581 | `				while( zScan < zStop && (SyisAlphaNum(zScan[0]) \|\| zScan[0] == '_') ){` |
|         9 |  1582 | `					zScan++;` |
|         1 |  1583 | `				}` |
|         3 |  1584 | `				if( zScan == zStop && zName < zStop ){` |
|         - |  1585 | `					SyBlob sVar;` |
|         3 |  1586 | `					PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pGen->pIn->nLine,` |
|         - |  1587 | `						"Using ${var} in strings is deprecated, use {$var} instead");` |
|         3 |  1588 | `					SyBlobInit(&sVar,&pGen->pVm->sAllocator);` |
|         3 |  1589 | `					SyBlobAppend(&sVar,"$",1);` |
|         3 |  1590 | `					SyBlobAppend(&sVar,zName,(sxu32)(zStop - zName));` |
|         - |  1591 | `					/* The scanner reads one byte PAST the length it is given, so the rewritten` |
|         - |  1592 | `					 * source has to be NUL-terminated: in the ordinary path the byte after the` |
|         - |  1593 | `					 * expression is the string's own closing quote, which stops an identifier,` |
|         - |  1594 | `					 * but here it is whatever the allocator left after the blob -- and an` |
|         - |  1595 | `					 * identifier byte there silently EXTENDS the variable name. */` |
|         3 |  1596 | `					SyBlobNullAppend(&sVar);` |
|         4 |  1597 | `					rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|         2 |  1598 | `						(const char *)SyBlobData(&sVar),` |
|         2 |  1599 | `						(const char *)SyBlobData(&sVar) + SyBlobLength(&sVar));` |
|         3 |  1600 | `					SyBlobRelease(&sVar);` |
|         3 |  1601 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  1602 | `						return SXERR_ABORT;` |
|         - |  1603 | `					}` |
|         3 |  1604 | `					if( rc != SXERR_EMPTY ){` |
|         3 |  1605 | `						++iCons;` |
|         1 |  1606 | `					}` |
|         3 |  1607 | `					pObj = 0;` |
|         3 |  1608 | `					continue;` |
|         - |  1609 | `				}` |
|       ! 0 |  1610 | `			}` |
|         - |  1611 | `			/* Process the expression */` |
|      2507 |  1612 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2507 |  1613 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1614 | `				return SXERR_ABORT;` |
|         - |  1615 | `			}` |
|      2507 |  1616 | `			if( rc != SXERR_EMPTY ){` |
|      2505 |  1617 | `				++iCons;` |
|      1250 |  1618 | `			}` |
|         - |  1619 | `		}` |
|         - |  1620 | `		/* Invalidate the previously used constant */` |
|      2639 |  1621 | `		pObj = 0;` |
|         5 |  1622 | `	}/*for(;;)*/` |
|    120149 |  1623 | `	if( iCons > 1 ){` |
|         - |  1624 | `		/* Concatenate all compiled constants */` |
|      1911 |  1625 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       953 |  1626 | `	}` |
|         - |  1627 | `	/* Node successfully compiled */` |
|    120149 |  1628 | `	return SXRET_OK;` |
|     60281 |  1629 | `}` |
|         - |  1630 | `/*` |
|         - |  1631 | ` * Compile a double quoted string.` |
|         - |  1632 | ` *  See the block-comment above for more information.` |
|         - |  1633 | ` */` |
|    120490 |  1634 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1635 | `{` |
|         - |  1636 | `	sxi32 rc;` |
|    120495 |  1637 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     60245 |  1638 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1639 | `	/* Compilation result */` |
|    120495 |  1640 | `	return rc;` |
|         5 |  1641 | `}` |
|         - |  1642 | `/*` |
|         - |  1643 | ` * Compile a Heredoc string.` |
|         - |  1644 | ` *  See the block-comment above for more information.` |
|         - |  1645 | ` */` |
|        66 |  1646 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 |  1647 | `{` |
|         - |  1648 | `	SyString sOrig, sStripped;` |
|         - |  1649 | `	sxi32 rc;` |
|        70 |  1650 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        70 |  1651 | `	if( rc != SXRET_OK ){` |
|         6 |  1652 | `		return rc;` |
|         - |  1653 | `	}` |
|         - |  1654 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|         - |  1655 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|         - |  1656 | `	 * Restore before returning so downstream code that references pIn is` |
|         - |  1657 | `	 * unaffected, including on the error path. */` |
|        64 |  1658 | `	sOrig = pGen->pIn->sData;` |
|        64 |  1659 | `	pGen->pIn->sData = sStripped;` |
|        64 |  1660 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|        64 |  1661 | `	pGen->pIn->sData = sOrig;` |
|        31 |  1662 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        64 |  1663 | `	return rc;` |
|        37 |  1664 | `}` |
|         - |  1665 | `/*` |
|         - |  1666 | ` * Compile an array entry whether it is a key or a value.` |
|         - |  1667 | ` *  Notes on array entries.` |
|         - |  1668 | ` *  According to the PHP language reference manual` |
|         - |  1669 | ` *  An array can be created by the array() language construct.` |
|         - |  1670 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|         - |  1671 | ` *  array(  key =>  value` |
|         - |  1672 | ` *    , ...` |
|         - |  1673 | ` *    )` |
|         - |  1674 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|         - |  1675 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|         - |  1676 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|         - |  1677 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|         - |  1678 | ` *  contain integer and string indices.` |
|         - |  1679 | ` *  A value can be any PHP type.` |
|         - |  1680 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|         - |  1681 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|         - |  1682 | ` *  is specified, that value will be overwritten.` |
|         - |  1683 | ` */` |
|   1439464 |  1684 | `static sxi32 GenStateCompileArrayEntry(` |
|         - |  1685 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1686 | `	SyToken *pIn,        /* Token stream */` |
|         - |  1687 | `	SyToken *pEnd,       /* End of the token stream */` |
|         - |  1688 | `	sxi32 iFlags,        /* Compilation flags */` |
|         - |  1689 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|         - |  1690 | `	)` |
|         5 |  1691 | `{` |
|         - |  1692 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1693 | `	sxi32 rc;` |
|         - |  1694 | `	/* Swap token stream */` |
|   1439469 |  1695 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1696 | `	/* Compile the expression*/` |
|   1439469 |  1697 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1698 | `	/* Restore token stream */` |
|   1439469 |  1699 | `	RE_SWAP_DELIMITER(pGen);` |
|   1439469 |  1700 | `	return rc;` |
|         5 |  1701 | `}` |
|         - |  1702 | `/*` |
|         - |  1703 | ` * Expression tree validator callback for the 'array' language construct.` |
|         - |  1704 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1705 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1706 | ` * error message.` |
|         - |  1707 | ` * See the routine responible of compiling the array language construct` |
|         - |  1708 | ` * for more inforation.` |
|         - |  1709 | ` */` |
|        36 |  1710 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  1711 | `{` |
|        41 |  1712 | `	sxi32 rc = SXRET_OK;` |
|        41 |  1713 | `	if( pRoot->pOp ){` |
|        14 |  1714 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|        12 |  1715 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|        16 |  1716 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1717 | `			/* Unexpected expression */` |
|        13 |  1718 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|        13 |  1719 | `			if( rc != SXERR_ABORT ){` |
|        13 |  1720 | `				rc = SXERR_INVALID;` |
|         5 |  1721 | `			}` |
|         9 |  1722 | `		}` |
|        31 |  1723 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1724 | `		/* Unexpected expression */` |
|         3 |  1725 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|         3 |  1726 | `		if( rc != SXERR_ABORT ){` |
|         3 |  1727 | `			rc = SXERR_INVALID;` |
|         1 |  1728 | `		}` |
|         1 |  1729 | `	}` |
|        41 |  1730 | `	return rc;` |
|         5 |  1731 | `}` |
|         - |  1732 | `/*` |
|         - |  1733 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|         - |  1734 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|         - |  1735 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|         - |  1736 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|         - |  1737 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|         - |  1738 | ` */` |
|   1380834 |  1739 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1740 | `{` |
|   1380839 |  1741 | `	SyToken *pCur = pStart;` |
|   1380839 |  1742 | `	sxi32 iNest = 0;` |
|   3564759 |  1743 | `	while( pCur < pEnd ){` |
|   2687279 |  1744 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    503355 |  1745 | `			return pCur;` |
|         - |  1746 | `		}` |
|         - |  1747 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1748 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1749 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1750 | `		 */` |
|   2183929 |  1751 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23055 |  1752 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23055 |  1753 | `			SyToken *pFn = pCur;` |
|     23050 |  1754 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1755 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1756 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1757 | `				pFn = &pCur[1];` |
|       ! 0 |  1758 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1759 | `			}` |
|     23055 |  1760 | `			if( nKw == PH7_TKWRD_FN ){` |
|         5 |  1761 | `				pCur = pFn + 1; /* past 'fn' */` |
|         5 |  1762 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  1763 | `					pCur++;` |
|       ! 0 |  1764 | `				}` |
|         5 |  1765 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         5 |  1766 | `					pCur++;` |
|         5 |  1767 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1768 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         5 |  1769 | `					if( pCur < pEnd ){` |
|         5 |  1770 | `						pCur++;` |
|         2 |  1771 | `					}` |
|         2 |  1772 | `				}` |
|         5 |  1773 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|       ! 0 |  1774 | `					pCur++;` |
|       ! 0 |  1775 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|       ! 0 |  1776 | `						&& pCur->sData.nByte == 1` |
|       ! 0 |  1777 | `						&& pCur->sData.zString[0] == '?' ){` |
|       ! 0 |  1778 | `						pCur++;` |
|       ! 0 |  1779 | `					}` |
|       ! 0 |  1780 | `					if( pCur < pEnd` |
|       ! 0 |  1781 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  1782 | `						pCur++;` |
|       ! 0 |  1783 | `					}` |
|       ! 0 |  1784 | `				}` |
|         - |  1785 | `				/* The rest of the entry is the arrow-function body — no outer` |
|         - |  1786 | `				 * key to extract. */` |
|         5 |  1787 | `				return pEnd;` |
|         - |  1788 | `			}` |
|         - |  1789 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|         - |  1790 | `			 * entry separator. Skip past the full match span. */` |
|     23051 |  1791 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|         3 |  1792 | `				pCur++; /* past 'match' */` |
|         3 |  1793 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         3 |  1794 | `					pCur++;` |
|         3 |  1795 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1796 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         3 |  1797 | `					if( pCur < pEnd ){` |
|         3 |  1798 | `						pCur++;` |
|         1 |  1799 | `					}` |
|         1 |  1800 | `				}` |
|         3 |  1801 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|         3 |  1802 | `					pCur++;` |
|         3 |  1803 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1804 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|         3 |  1805 | `					if( pCur < pEnd ){` |
|         3 |  1806 | `						pCur++;` |
|         1 |  1807 | `					}` |
|         1 |  1808 | `				}` |
|         3 |  1809 | `				continue;` |
|         - |  1810 | `			}` |
|     11522 |  1811 | `		}` |
|   2183923 |  1812 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     54107 |  1813 | `			iNest++;` |
|   2156872 |  1814 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1815 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1816 | `			 * parser will shortly detect any syntax error. */` |
|     54107 |  1817 | `			iNest--;` |
|     27051 |  1818 | `		}` |
|   2183923 |  1819 | `		pCur++;` |
|         5 |  1820 | `	}` |
|    877485 |  1821 | `	return pEnd;` |
|    690422 |  1822 | `}` |
|         - |  1823 | `/*` |
|         - |  1824 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1825 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1826 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1827 | ` */` |
|    615956 |  1828 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1829 | `{` |
|         - |  1830 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1831 | `	SyToken *pKey,*pCur;` |
|    615961 |  1832 | `	sxi32 iEmitRef = 0;` |
|    615961 |  1833 | `	sxi32 iSpread = 0;` |
|    615961 |  1834 | `	sxi32 nPair = 0;` |
|         - |  1835 | `	sxi32 rc;` |
|    615961 |  1836 | `	xValidator = 0;` |
|    842998 |  1837 | `	for(;;){` |
|         - |  1838 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|         - |  1839 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|         - |  1840 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|         - |  1841 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|    535020 |  1842 | `		{` |
|   1686001 |  1843 | `			int nSkip = 0;` |
|   2507907 |  1844 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    821911 |  1845 | `				nSkip++;` |
|    821911 |  1846 | `				pGen->pIn++;` |
|         5 |  1847 | `			}` |
|   1686001 |  1848 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|       ! 0 |  1849 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|         - |  1850 | `					"Cannot use empty array elements in arrays");` |
|       ! 0 |  1851 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1852 | `					return SXERR_ABORT;` |
|         - |  1853 | `				}` |
|       ! 0 |  1854 | `				return SXRET_OK;` |
|         - |  1855 | `			}` |
|         - |  1856 | `		}` |
|   1686001 |  1857 | `		pCur = pGen->pIn;` |
|   1686001 |  1858 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1859 | `			/* No more entry to process */` |
|    615943 |  1860 | `			break;` |
|         - |  1861 | `		}` |
|   1070063 |  1862 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1863 | `			continue;` |
|         - |  1864 | `		}` |
|         - |  1865 | `		/* Compile the key if available */` |
|   1070063 |  1866 | `		pKey = pCur;` |
|   1070063 |  1867 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   1070063 |  1868 | `		rc = SXERR_EMPTY;` |
|   1070063 |  1869 | `		if( pCur < pGen->pIn ){` |
|    369153 |  1870 | `			if( pKey == pCur ){` |
|         - |  1871 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|         - |  1872 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|         - |  1873 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|         - |  1874 | `				 * IS found here, so control never reached it.)` |
|         - |  1875 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|         3 |  1876 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|         - |  1877 | `					? "\"]\"" : "\")\"";` |
|         3 |  1878 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|         3 |  1879 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1880 | `					return SXERR_ABORT;` |
|         - |  1881 | `				}` |
|         3 |  1882 | `				return SXRET_OK;` |
|         - |  1883 | `			}` |
|    369151 |  1884 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1885 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|         - |  1886 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|         - |  1887 | `				 * makes the helper reach for the token past this entry's slice. */` |
|        12 |  1888 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        12 |  1889 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1890 | `					return SXERR_ABORT;` |
|         - |  1891 | `				}` |
|        12 |  1892 | `				return SXRET_OK;` |
|         - |  1893 | `			}` |
|         - |  1894 | `			/* Compile the expression holding the key */` |
|    369141 |  1895 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1896 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    369141 |  1897 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1898 | `				return SXERR_ABORT;` |
|         - |  1899 | `			}` |
|    369141 |  1900 | `			pCur++; /* Jump the '=>' operator */` |
|    184573 |  1901 | `		}else{` |
|         - |  1902 | `			/* Reset back the cursor and point to the entry value */` |
|    700915 |  1903 | `			pCur = pKey;` |
|         - |  1904 | `		}` |
|   1070051 |  1905 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1906 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|         - |  1907 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|    700915 |  1908 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|    350455 |  1909 | `		}` |
|   1070051 |  1910 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|         - |  1911 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|        45 |  1912 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|        45 |  1913 | `			iEmitRef = 1;` |
|        45 |  1914 | `			pCur++; /* Jump the '&' token */` |
|        45 |  1915 | `			if( pCur >= pGen->pIn ){` |
|         - |  1916 | `				/* Missing value */` |
|         3 |  1917 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|         3 |  1918 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1919 | `					return SXERR_ABORT;` |
|         - |  1920 | `				}` |
|         3 |  1921 | `				return SXRET_OK;` |
|         - |  1922 | `			}` |
|        19 |  1923 | `		}` |
|         - |  1924 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|         - |  1925 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|         - |  1926 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|         - |  1927 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|         - |  1928 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   1070049 |  1929 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   1070049 |  1930 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|         - |  1931 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|         - |  1932 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|         - |  1933 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|         - |  1934 | `			 * output is engine-portable. */` |
|         6 |  1935 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|         - |  1936 | `				"syntax error, unexpected token \"...\"");` |
|         6 |  1937 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1938 | `				return SXERR_ABORT;` |
|         - |  1939 | `			}` |
|         6 |  1940 | `			return SXRET_OK;` |
|         - |  1941 | `		}` |
|         - |  1942 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|         - |  1943 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|         - |  1944 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|         - |  1945 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|         - |  1946 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|   1605065 |  1947 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    535020 |  1948 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1949 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    535020 |  1950 | `			xValidator);` |
|   1070045 |  1951 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1952 | `			return SXERR_ABORT;` |
|         - |  1953 | `		}` |
|   1070045 |  1954 | `		if( iSpread ){` |
|         - |  1955 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        72 |  1956 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   1070010 |  1957 | `		}else if( iEmitRef ){` |
|         - |  1958 | `			/* Emit the load reference instruction */` |
|        41 |  1959 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1960 | `		}` |
|   1070045 |  1961 | `		xValidator = 0;` |
|   1070045 |  1962 | `		iEmitRef = 0;` |
|   1070045 |  1963 | `		iSpread = 0;` |
|   1070045 |  1964 | `		nPair++;` |
|         5 |  1965 | `	}` |
|         - |  1966 | `	/* Emit the load map instruction */` |
|    615943 |  1967 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1968 | `	/* Node successfully compiled */` |
|    615943 |  1969 | `	return SXRET_OK;` |
|    307983 |  1970 | `}` |
|         - |  1971 | `/*` |
|         - |  1972 | ` * Compile the 'array' language construct.` |
|         - |  1973 | ` *	 According to the PHP language reference manual` |
|         - |  1974 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1975 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1976 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1977 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1978 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1979 | ` */` |
|    399552 |  1980 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1981 | `{` |
|         - |  1982 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    399557 |  1983 | `	pGen->pIn += 2;` |
|    399557 |  1984 | `	pGen->pEnd--;` |
|    199776 |  1985 | `	SXUNUSED(iCompileFlag);` |
|    399557 |  1986 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1987 | `}` |
|         - |  1988 | `/*` |
|         - |  1989 | ` * Compile the PHP 8.5 clone(...) call form:` |
|         - |  1990 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|         - |  1991 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|         - |  1992 | ` *                                              property updates as scope-aware writes` |
|         - |  1993 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|         - |  1994 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|         - |  1995 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|         - |  1996 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|         - |  1997 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|         - |  1998 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|         - |  1999 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|         - |  2000 | ` */` |
|        22 |  2001 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2002 | `{` |
|         - |  2003 | `	SyToken *pIn,*pEnd,*pNext;` |
|        24 |  2004 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|        24 |  2005 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|        24 |  2006 | `	int nArg = 0;` |
|         - |  2007 | `	sxi32 rc;` |
|        11 |  2008 | `	SXUNUSED(iCompileFlag);` |
|         - |  2009 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|        24 |  2010 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|        24 |  2011 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|         - |  2012 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|        24 |  2013 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       ! 0 |  2014 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  2015 | `			"clone(...) first-class callable form is not yet supported");` |
|         - |  2016 | `	}` |
|         - |  2017 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|        62 |  2018 | `	while( pIn < pEnd ){` |
|        40 |  2019 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|        40 |  2020 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|       ! 0 |  2021 | `			break;` |
|         - |  2022 | `		}` |
|        40 |  2023 | `		pArgStart = pIn;` |
|        40 |  2024 | `		pArgEnd   = pNext;` |
|         - |  2025 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|         - |  2026 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|        38 |  2027 | `		if( (pArgEnd - pArgStart) >= 2` |
|        37 |  2028 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        23 |  2029 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|         5 |  2030 | `			pName = pArgStart;` |
|         5 |  2031 | `			pArgStart += 2;` |
|         2 |  2032 | `		}` |
|        40 |  2033 | `		if( pName ){` |
|         - |  2034 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|         - |  2035 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|         4 |  2036 | `			if( pName->sData.nByte == sizeof("object")-1` |
|         4 |  2037 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|         3 |  2038 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|         4 |  2039 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|         3 |  2040 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|         3 |  2041 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         2 |  2042 | `			}else{` |
|       ! 0 |  2043 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|       ! 0 |  2044 | `					"Unknown named parameter $%z",&pName->sData);` |
|         1 |  2045 | `			}` |
|        38 |  2046 | `		}else if( nArg == 0 ){` |
|        22 |  2047 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        25 |  2048 | `		}else if( nArg == 1 ){` |
|        15 |  2049 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         8 |  2050 | `		}else{` |
|       ! 0 |  2051 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|         - |  2052 | `				"clone() expects at most 2 arguments");` |
|         - |  2053 | `		}` |
|        40 |  2054 | `		nArg++;` |
|        40 |  2055 | `		pIn = pNext;` |
|        40 |  2056 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 |  2057 | `			pIn++; /* step over the argument separator */` |
|         8 |  2058 | `		}` |
|         2 |  2059 | `	}` |
|        24 |  2060 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|       ! 0 |  2061 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  2062 | `			"clone() expects at least 1 argument, 0 given");` |
|         - |  2063 | `	}` |
|         - |  2064 | `	/* Object argument -> clone (+ __clone()). */` |
|        24 |  2065 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        24 |  2066 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2067 | `		return SXERR_ABORT;` |
|         - |  2068 | `	}` |
|        24 |  2069 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|         - |  2070 | `	/* Property updates (evaluated after __clone runs). */` |
|        24 |  2071 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|        17 |  2072 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        17 |  2073 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2074 | `			return SXERR_ABORT;` |
|         - |  2075 | `		}` |
|        17 |  2076 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|         8 |  2077 | `	}` |
|        24 |  2078 | `	return SXRET_OK;` |
|        13 |  2079 | `}` |
|         - |  2080 | `/*` |
|         - |  2081 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|         - |  2082 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|         - |  2083 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|         - |  2084 | ` */` |
|    216404 |  2085 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2086 | `{` |
|         - |  2087 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    216409 |  2088 | `	pGen->pIn++;` |
|    216409 |  2089 | `	pGen->pEnd--;` |
|    108202 |  2090 | `	SXUNUSED(iCompileFlag);` |
|    216409 |  2091 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  2092 | `}` |
|         - |  2093 | `/*` |
|         - |  2094 | ` * Expression tree validator callback for the 'list' language construct.` |
|         - |  2095 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  2096 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  2097 | ` * error message.` |
|         - |  2098 | ` * See the routine responible of compiling the list language construct` |
|         - |  2099 | ` * for more inforation.` |
|         - |  2100 | ` */` |
|       214 |  2101 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  2102 | `{` |
|       219 |  2103 | `	sxi32 rc = SXRET_OK;` |
|       219 |  2104 | `	if( pRoot->pOp ){` |
|         4 |  2105 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|         2 |  2106 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|         - |  2107 | `				/* Unexpected expression */` |
|       ! 0 |  2108 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2109 | `					"Assignments can only happen to writable values");` |
|       ! 0 |  2110 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 |  2111 | `					rc = SXERR_INVALID;` |
|       ! 0 |  2112 | `				}` |
|         1 |  2113 | `		}` |
|       217 |  2114 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  2115 | `		/* Unexpected expression */` |
|         6 |  2116 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2117 | `			"Assignments can only happen to writable values");` |
|         6 |  2118 | `		if( rc != SXERR_ABORT ){` |
|         6 |  2119 | `			rc = SXERR_INVALID;` |
|         2 |  2120 | `		}` |
|         2 |  2121 | `	}` |
|       219 |  2122 | `	return rc;` |
|         5 |  2123 | `}` |
|         - |  2124 | `/*` |
|         - |  2125 | ` * Compile the 'list' language construct.` |
|         - |  2126 | ` *  According to the PHP language reference` |
|         - |  2127 | ` *  list(): Assign variables as if they were an array.` |
|         - |  2128 | ` *  list() is used to assign a list of variables in one operation.` |
|         - |  2129 | ` *  Description` |
|         - |  2130 | ` *   array list (mixed $varname [, mixed $... ] )` |
|         - |  2131 | ` *   Like array(), this is not really a function, but a language construct.` |
|         - |  2132 | ` *   list() is used to assign a list of variables in one operation.` |
|         - |  2133 | ` *  Parameters` |
|         - |  2134 | ` *   $varname: A variable.` |
|         - |  2135 | ` *  Return Values` |
|         - |  2136 | ` *   The assigned array.` |
|         - |  2137 | ` */` |
|         - |  2138 | `/* Nested list entry recorded during first pass of list body compilation */` |
|         - |  2139 | `struct NestedListEntry {` |
|         - |  2140 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|         - |  2141 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|         - |  2142 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|         - |  2143 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|         - |  2144 | `};` |
|         - |  2145 | `/*` |
|         - |  2146 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|         - |  2147 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|         - |  2148 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|         - |  2149 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|         - |  2150 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|         - |  2151 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|         - |  2152 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|         - |  2153 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|         - |  2154 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|         - |  2155 | ` */` |
|        22 |  2156 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|         1 |  2157 | `{` |
|         - |  2158 | `	SyToken *pNext;` |
|         - |  2159 | `	sxi32 rc;` |
|        53 |  2160 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|         - |  2161 | `		SyToken *pArrow,*pTarget;` |
|         - |  2162 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|        31 |  2163 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|        31 |  2164 | `		pTarget = &pArrow[1];` |
|        31 |  2165 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|         - |  2166 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|         - |  2167 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|       ! 0 |  2168 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2169 | `				"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2170 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2171 | `		}` |
|         - |  2172 | `		/* DUP the source array (it is on the stack top) */` |
|        31 |  2173 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2174 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|        31 |  2175 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|        31 |  2176 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2177 | `			return SXERR_ABORT;` |
|         - |  2178 | `		}` |
|         - |  2179 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|         - |  2180 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|         - |  2181 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|         - |  2182 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|         - |  2183 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|         - |  2184 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|        31 |  2185 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|        31 |  2186 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|        28 |  2187 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|        15 |  2188 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|         - |  2189 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|         - |  2190 | `			 * Treat source[key] as the inner body's source, then drop the` |
|         - |  2191 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|         5 |  2192 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|         5 |  2193 | `			SyToken *pSavedIn = pGen->pIn;` |
|         5 |  2194 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         5 |  2195 | `			pGen->pIn = pTarget;` |
|         5 |  2196 | `			pGen->pEnd = pNext;` |
|         5 |  2197 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|         2 |  2198 | `			             : PH7_CompileList(&(*pGen),0);` |
|         5 |  2199 | `			pGen->pIn = pSavedIn;` |
|         5 |  2200 | `			pGen->pEnd = pSavedEnd;` |
|         5 |  2201 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2202 | `				return SXERR_ABORT;` |
|         - |  2203 | `			}` |
|         5 |  2204 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         3 |  2205 | `		}else{` |
|         - |  2206 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|         - |  2207 | `			 * is already on the stack as the value; compiling the target appends` |
|         - |  2208 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|         - |  2209 | `			 * assignment does. */` |
|         - |  2210 | `			VmInstr *pInstr;` |
|        27 |  2211 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|        27 |  2212 | `			sxi32 iP1 = 0, iP2 = 0;` |
|        27 |  2213 | `			void *p3 = 0;` |
|        27 |  2214 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|         - |  2215 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|        27 |  2216 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2217 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2218 | `			}` |
|        27 |  2219 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|        27 |  2220 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|         3 |  2221 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|        26 |  2222 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         3 |  2223 | `					iVmOp = PH7_OP_STORE_IDX;` |
|         3 |  2224 | `					iP1 = pInstr->iP1;` |
|         3 |  2225 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         2 |  2226 | `				}else{` |
|        23 |  2227 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|        23 |  2228 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - |  2229 | `				}` |
|        13 |  2230 | `			}` |
|        27 |  2231 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|         - |  2232 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|         - |  2233 | `			 * source array is back on top for the next entry. */` |
|        27 |  2234 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         - |  2235 | `		}` |
|        31 |  2236 | `		pGen->pIn = &pNext[1];` |
|         1 |  2237 | `	}` |
|        23 |  2238 | `	return SXRET_OK;` |
|        12 |  2239 | `}` |
|         - |  2240 | `/*` |
|         - |  2241 | ` * Shared body for list() and short list [...] compilation.` |
|         - |  2242 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|         - |  2243 | ` * the opening delimiter and before the closing delimiter.` |
|         - |  2244 | ` */` |
|       124 |  2245 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|         5 |  2246 | `{` |
|         - |  2247 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|         - |  2248 | `	SyToken *pNext;` |
|         - |  2249 | `	SyToken *pClassifyIn;` |
|       129 |  2250 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|         - |  2251 | `	sxi32 nExpr;` |
|         - |  2252 | `	sxi32 rc;` |
|         - |  2253 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|         - |  2254 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|         - |  2255 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|         - |  2256 | `	 * list. */` |
|       129 |  2257 | `	pClassifyIn = pGen->pIn;` |
|       373 |  2258 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       249 |  2259 | `		if( pGen->pIn >= pNext ){` |
|        13 |  2260 | `			nEmpty++;` |
|       243 |  2261 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|        31 |  2262 | `			nKeyed++;` |
|        16 |  2263 | `		}else{` |
|       207 |  2264 | `			nPositional++;` |
|         - |  2265 | `		}` |
|       249 |  2266 | `		pGen->pIn = &pNext[1];` |
|         5 |  2267 | `	}` |
|       129 |  2268 | `	pGen->pIn = pClassifyIn;` |
|       129 |  2269 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|       ! 0 |  2270 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2271 | `			"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2272 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2273 | `	}` |
|       129 |  2274 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|       ! 0 |  2275 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2276 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|       ! 0 |  2277 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2278 | `	}` |
|       129 |  2279 | `	if( nKeyed > 0 ){` |
|        23 |  2280 | `		return GenStateCompileKeyedListBody(pGen);` |
|         - |  2281 | `	}` |
|       107 |  2282 | `	nExpr = 0;` |
|       107 |  2283 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       321 |  2284 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       219 |  2285 | `		if( pGen->pIn < pNext ){` |
|         - |  2286 | `			/* Check for nested list() */` |
|       207 |  2287 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         3 |  2288 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  2289 | `				/* Record this nested list for post-processing */` |
|         3 |  2290 | `				SyToken *pListEnd = 0;` |
|         3 |  2291 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|         3 |  2292 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         1 |  2293 | `				}` |
|         3 |  2294 | `				if( pListEnd ){` |
|         - |  2295 | `					struct NestedListEntry sEntry;` |
|         3 |  2296 | `					sEntry.nIndex = nExpr;` |
|         3 |  2297 | `					sEntry.pStart = pGen->pIn;` |
|         3 |  2298 | `					sEntry.pEnd = pListEnd + 1;` |
|         3 |  2299 | `					sEntry.isShort = 0;` |
|         3 |  2300 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         1 |  2301 | `				}` |
|         - |  2302 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|         3 |  2303 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       206 |  2304 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  2305 | `				/* Nested short destructuring [...] */` |
|        13 |  2306 | `				SyToken *pBracketEnd = 0;` |
|        13 |  2307 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|        13 |  2308 | `				if( pBracketEnd ){` |
|         - |  2309 | `					struct NestedListEntry sEntry;` |
|        13 |  2310 | `					sEntry.nIndex = nExpr;` |
|        13 |  2311 | `					sEntry.pStart = pGen->pIn;` |
|        13 |  2312 | `					sEntry.pEnd = pBracketEnd + 1;` |
|        13 |  2313 | `					sEntry.isShort = 1;` |
|        13 |  2314 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         6 |  2315 | `				}` |
|         - |  2316 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        13 |  2317 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |  2318 | `			}else{` |
|         - |  2319 | `				/* Compile the expression holding the variable */` |
|       193 |  2320 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       193 |  2321 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  2322 | `					SySetRelease(&sNested);` |
|       ! 0 |  2323 | `					return SXRET_OK;` |
|         - |  2324 | `				}` |
|         - |  2325 | `			}` |
|       106 |  2326 | `		}else{` |
|         - |  2327 | `			/* Empty entry,load NULL */` |
|        13 |  2328 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|         - |  2329 | `		}` |
|       219 |  2330 | `		nExpr++;` |
|         - |  2331 | `		/* Advance the stream cursor */` |
|       219 |  2332 | `		pGen->pIn = &pNext[1];` |
|         5 |  2333 | `	}` |
|         - |  2334 | `	/* Emit the LOAD_LIST instruction */` |
|       107 |  2335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|         - |  2336 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|         - |  2337 | `	 * For each nested entry, emit code to extract the sub-array` |
|         - |  2338 | `	 * at the corresponding index and recursively destructure it.` |
|         - |  2339 | `	 */` |
|       107 |  2340 | `	if( SySetUsed(&sNested) > 0 ){` |
|        13 |  2341 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|         - |  2342 | `		sxu32 i;` |
|        27 |  2343 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|        15 |  2344 | `			SyToken *pSavedIn = pGen->pIn;` |
|        15 |  2345 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         - |  2346 | `			ph7_value *pIdx;` |
|         - |  2347 | `			sxu32 nConstIdx;` |
|         - |  2348 | `			/* DUP the source array (it's on stack top) */` |
|        15 |  2349 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2350 | `			/* Push the integer index for this nested entry */` |
|        15 |  2351 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|        15 |  2352 | `			if( pIdx == 0 ){` |
|       ! 0 |  2353 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2354 | `				SySetRelease(&sNested);` |
|       ! 0 |  2355 | `				return SXERR_ABORT;` |
|         - |  2356 | `			}` |
|        15 |  2357 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|        15 |  2358 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|         - |  2359 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|         - |  2360 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|         - |  2361 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|         - |  2362 | `			 */` |
|        15 |  2363 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|         - |  2364 | `			/* Recursively compile the inner list */` |
|        15 |  2365 | `			pGen->pIn = apNested[i].pStart;` |
|        15 |  2366 | `			pGen->pEnd = apNested[i].pEnd;` |
|        15 |  2367 | `			if( apNested[i].isShort ){` |
|        13 |  2368 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  2369 | `			}else{` |
|         3 |  2370 | `				rc = PH7_CompileList(&(*pGen),0);` |
|         - |  2371 | `			}` |
|        15 |  2372 | `			pGen->pIn = pSavedIn;` |
|        15 |  2373 | `			pGen->pEnd = pSavedEnd;` |
|        15 |  2374 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2375 | `				SySetRelease(&sNested);` |
|       ! 0 |  2376 | `				return SXERR_ABORT;` |
|         - |  2377 | `			}` |
|         - |  2378 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|        15 |  2379 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         8 |  2380 | `		}` |
|         6 |  2381 | `	}` |
|       107 |  2382 | `	SySetRelease(&sNested);` |
|         - |  2383 | `	/* Node successfully compiled */` |
|       107 |  2384 | `	return SXRET_OK;` |
|        67 |  2385 | `}` |
|        40 |  2386 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2387 | `{` |
|         - |  2388 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|        45 |  2389 | `	pGen->pIn += 2;` |
|        45 |  2390 | `	pGen->pEnd--;` |
|        20 |  2391 | `	SXUNUSED(iCompileFlag);` |
|        45 |  2392 | `	return GenStateCompileListBody(pGen);` |
|         5 |  2393 | `}` |
|        84 |  2394 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2395 | `{` |
|         - |  2396 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        86 |  2397 | `	pGen->pIn++;` |
|        86 |  2398 | `	pGen->pEnd--;` |
|        42 |  2399 | `	SXUNUSED(iCompileFlag);` |
|        86 |  2400 | `	return GenStateCompileListBody(pGen);` |
|         2 |  2401 | `}` |
|         - |  2402 | `/* Forward declarations */` |
|         - |  2403 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|         - |  2404 | `static int GenStateIsReservedConstant(SyString *pName);` |
|         - |  2405 | `static int GenStateIsReadonly(SyToken *pTok);` |
|         - |  2406 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|         - |  2407 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|         - |  2408 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - |  2409 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|         - |  2410 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|         - |  2411 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|         - |  2412 | `/*` |
|         - |  2413 | ` * Compile an annoynmous function or a closure.` |
|         - |  2414 | ` * According to the PHP language reference` |
|         - |  2415 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  2416 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  2417 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|         - |  2418 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|         - |  2419 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|         - |  2420 | ` *  Example Anonymous function variable assignment example` |
|         - |  2421 | ` * <?php` |
|         - |  2422 | ` * $greet = function($name)` |
|         - |  2423 | ` * {` |
|         - |  2424 | ` *    printf("Hello %s\r\n", $name);` |
|         - |  2425 | ` * };` |
|         - |  2426 | ` * $greet('World');` |
|         - |  2427 | ` * $greet('PHP');` |
|         - |  2428 | ` * ?>` |
|         - |  2429 | ` * Note that the implementation of annoynmous function and closure under` |
|         - |  2430 | ` * PH7 is completely different from the one used by the zend engine.` |
|         - |  2431 | ` */` |
|       576 |  2432 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2433 | `{` |
|       581 |  2434 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2435 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2436 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2437 | `							  * one thread is allowed to compile the script.` |
|         - |  2438 | `						      */` |
|         - |  2439 | `	SyString sName;` |
|       581 |  2440 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2441 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2442 | `	sxu32 nKwLine;` |
|       581 |  2443 | `	sxi32 iFlags = 0;` |
|         - |  2444 | `	sxu32 nLen;` |
|         - |  2445 | `	sxi32 rc;` |
|       288 |  2446 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2447 |  |
|       581 |  2448 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       576 |  2449 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       581 |  2450 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2451 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        11 |  2452 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        11 |  2453 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         5 |  2454 | `	}` |
|       581 |  2455 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       581 |  2456 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2457 | `		pGen->pIn++;` |
|       ! 0 |  2458 | `	}` |
|         - |  2459 | `	/* Generate a unique name */` |
|       581 |  2460 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2461 | `	/* Make sure the generated name is unique */` |
|       581 |  2462 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2463 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2464 | `	}` |
|       581 |  2465 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2466 | `	/* Compile the lambda body */` |
|       581 |  2467 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       581 |  2468 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2469 | `		return SXERR_ABORT;` |
|         - |  2470 | `	}` |
|       581 |  2471 | `	if( pAnnonFunc ){` |
|       581 |  2472 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2473 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2474 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       581 |  2475 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2476 | `			return SXERR_ABORT;` |
|         - |  2477 | `		}` |
|       288 |  2478 | `	}` |
|         - |  2479 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2480 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2481 | `	 * the handler wraps either in a Closure instance. */` |
|       581 |  2482 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2483 | `	/* Node successfully compiled */` |
|       581 |  2484 | `	return SXRET_OK;` |
|       293 |  2485 | `}` |
|         - |  2486 | `/*` |
|         - |  2487 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2488 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2489 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2490 | ` */` |
|       236 |  2491 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2492 | `	ph7_gen_state *pGen,` |
|         - |  2493 | `	ph7_vm_func *pFunc,` |
|         - |  2494 | `	const char *zName,` |
|         - |  2495 | `	sxu32 nByte,` |
|         - |  2496 | `	SyString *aShadow,` |
|         - |  2497 | `	sxu32 nShadow)` |
|         3 |  2498 | `{` |
|         - |  2499 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2500 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2501 | `	sxu32 n, nEnv;` |
|         - |  2502 | `	char *zDup;` |
|       239 |  2503 | `	if( nByte == 0 ){` |
|       ! 0 |  2504 | `		return SXRET_OK;` |
|         - |  2505 | `	}` |
|       236 |  2506 | `	if( nByte == sizeof("this")-1` |
|       130 |  2507 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         9 |  2508 | `		return SXRET_OK;` |
|         - |  2509 | `	}` |
|       285 |  2510 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       212 |  2511 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       206 |  2512 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       161 |  2513 | `			return SXRET_OK;` |
|         - |  2514 | `		}` |
|        29 |  2515 | `	}` |
|        71 |  2516 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        71 |  2517 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|        99 |  2518 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2519 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2520 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2521 | `			return SXRET_OK;` |
|         - |  2522 | `		}` |
|        15 |  2523 | `	}` |
|        69 |  2524 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        69 |  2525 | `	if( zDup == 0 ){` |
|       ! 0 |  2526 | `		return SXERR_ABORT;` |
|         - |  2527 | `	}` |
|        69 |  2528 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        69 |  2529 | `	sEnv.iFlags = 0;` |
|        69 |  2530 | `	sEnv.nIdx = SXU32_HIGH;` |
|        69 |  2531 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        69 |  2532 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        69 |  2533 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        69 |  2534 | `	return SXRET_OK;` |
|       121 |  2535 | `}` |
|         - |  2536 | `/*` |
|         - |  2537 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|         - |  2538 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|         - |  2539 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|         - |  2540 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|         - |  2541 | ` */` |
|       108 |  2542 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|         - |  2543 | `	ph7_gen_state *pGen,` |
|         - |  2544 | `	ph7_vm_func *pFunc,` |
|         - |  2545 | `	const char *zIn,` |
|         - |  2546 | `	const char *zEnd,` |
|         - |  2547 | `	SyString *aShadow,` |
|         - |  2548 | `	sxu32 nShadow)` |
|         3 |  2549 | `{` |
|         - |  2550 | `	sxi32 rc;` |
|       589 |  2551 | `	while( zIn < zEnd ){` |
|       481 |  2552 | `		if( zIn[0] == '\\' ){` |
|        14 |  2553 | `			zIn++;` |
|        14 |  2554 | `			if( zIn < zEnd ){` |
|        14 |  2555 | `				zIn++;` |
|         6 |  2556 | `			}` |
|        14 |  2557 | `			continue;` |
|         - |  2558 | `		}` |
|       466 |  2559 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        27 |  2560 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|        24 |  2561 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|         - |  2562 | `			const char *zName;` |
|        26 |  2563 | `			zIn++; /* skip '$' */` |
|        26 |  2564 | `			zName = zIn;` |
|        82 |  2565 | `			while( zIn < zEnd ){` |
|        76 |  2566 | `				unsigned char c = (unsigned char)zIn[0];` |
|        76 |  2567 | `				if( c >= 0xc0 ){` |
|       ! 0 |  2568 | `					zIn++;` |
|       ! 0 |  2569 | `					while( zIn < zEnd` |
|       ! 0 |  2570 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  2571 | `						zIn++;` |
|       ! 0 |  2572 | `					}` |
|       ! 0 |  2573 | `					continue;` |
|         - |  2574 | `				}` |
|        76 |  2575 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        20 |  2576 | `					break;` |
|         - |  2577 | `				}` |
|        58 |  2578 | `				zIn++;` |
|         2 |  2579 | `			}` |
|        26 |  2580 | `			if( zIn > zName ){` |
|        38 |  2581 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|        24 |  2582 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|        26 |  2583 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2584 | `					return SXERR_ABORT;` |
|         - |  2585 | `				}` |
|        12 |  2586 | `			}` |
|        26 |  2587 | `			continue;` |
|         - |  2588 | `		}` |
|       445 |  2589 | `		zIn++;` |
|         3 |  2590 | `	}` |
|       111 |  2591 | `	return SXRET_OK;` |
|        57 |  2592 | `}` |
|         - |  2593 | `/*` |
|         - |  2594 | ` * Scan the body token range of an arrow function for free-variable` |
|         - |  2595 | ` * references and record them in pFunc's closure environment. Handles:` |
|         - |  2596 | ` *   - plain $<id> pairs` |
|         - |  2597 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|         - |  2598 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|         - |  2599 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|         - |  2600 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|         - |  2601 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|         - |  2602 | ` *     are never mistakenly captured.` |
|         - |  2603 | ` */` |
|       512 |  2604 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2605 | `	ph7_gen_state *pGen,` |
|         - |  2606 | `	ph7_vm_func *pFunc,` |
|         - |  2607 | `	SyToken *pStart,` |
|         - |  2608 | `	SyToken *pEnd,` |
|         - |  2609 | `	SyString *aShadow,` |
|         - |  2610 | `	sxu32 nShadow)` |
|         4 |  2611 | `{` |
|       516 |  2612 | `	SyToken *pScan = pStart;` |
|         - |  2613 | `	sxi32 rc;` |
|      3438 |  2614 | `	while( pScan < pEnd ){` |
|      2926 |  2615 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       165 |  2616 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        54 |  2617 | `				pScan->sData.zString,` |
|       108 |  2618 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        54 |  2619 | `				aShadow,nShadow);` |
|       111 |  2620 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2621 | `				return SXERR_ABORT;` |
|         - |  2622 | `			}` |
|       111 |  2623 | `			pScan++;` |
|       111 |  2624 | `			continue;` |
|         - |  2625 | `		}` |
|      2818 |  2626 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        39 |  2627 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        39 |  2628 | `			SyToken *pFnKw = pScan;` |
|        36 |  2629 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2630 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         3 |  2631 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2632 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2633 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2634 | `			}` |
|        39 |  2635 | `			if( nKw == PH7_TKWRD_FN ){` |
|         - |  2636 | `				SyToken *pInnerSigStart;` |
|         - |  2637 | `				SyToken *pInnerSigEnd;` |
|         - |  2638 | `				SyToken *pInnerBodyEnd;` |
|         - |  2639 | `				SyString *aInnerShadow;` |
|         - |  2640 | `				sxu32 nInnerShadow;` |
|         - |  2641 | `				sxu32 nInnerParamMax;` |
|         - |  2642 | `				SyToken *p;` |
|         - |  2643 | `				int iNestInner;` |
|        26 |  2644 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|        26 |  2645 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2646 | `					pScan++;` |
|       ! 0 |  2647 | `				}` |
|        26 |  2648 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2649 | `					pScan++;` |
|       ! 0 |  2650 | `					continue;` |
|         - |  2651 | `				}` |
|        26 |  2652 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|        26 |  2653 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|         - |  2654 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|        26 |  2655 | `				if( pInnerSigEnd >= pEnd ){` |
|       ! 0 |  2656 | `					pScan = pEnd;` |
|       ! 0 |  2657 | `					continue;` |
|         - |  2658 | `				}` |
|         - |  2659 | `				/* Build an augmented shadow list: inherited + inner params */` |
|        26 |  2660 | `				nInnerParamMax = 0;` |
|        76 |  2661 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        52 |  2662 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|        20 |  2663 | `						nInnerParamMax++;` |
|         9 |  2664 | `					}` |
|        27 |  2665 | `				}` |
|        26 |  2666 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|        24 |  2667 | `					&pGen->pVm->sAllocator,` |
|        24 |  2668 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|        26 |  2669 | `				if( aInnerShadow == 0 ){` |
|       ! 0 |  2670 | `					return SXERR_ABORT;` |
|         - |  2671 | `				}` |
|        26 |  2672 | `				nInnerShadow = 0;` |
|        32 |  2673 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|         7 |  2674 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|         4 |  2675 | `				}` |
|        76 |  2676 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        52 |  2677 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|        34 |  2678 | `						continue;` |
|         - |  2679 | `					}` |
|        20 |  2680 | `					if( &p[1] >= pInnerSigEnd ){` |
|       ! 0 |  2681 | `						break;` |
|         - |  2682 | `					}` |
|        20 |  2683 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2684 | `						continue;` |
|         - |  2685 | `					}` |
|        20 |  2686 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        11 |  2687 | `				}` |
|        26 |  2688 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|        26 |  2689 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|       ! 0 |  2690 | `					pScan++;` |
|       ! 0 |  2691 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|       ! 0 |  2692 | `						&& pScan->sData.nByte == 1` |
|       ! 0 |  2693 | `						&& pScan->sData.zString[0] == '?' ){` |
|       ! 0 |  2694 | `						pScan++;` |
|       ! 0 |  2695 | `					}` |
|       ! 0 |  2696 | `					if( pScan < pEnd` |
|       ! 0 |  2697 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  2698 | `						pScan++;` |
|       ! 0 |  2699 | `					}` |
|       ! 0 |  2700 | `				}` |
|        26 |  2701 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|        26 |  2702 | `					pScan++; /* past '=>' */` |
|        12 |  2703 | `				}` |
|        26 |  2704 | `				pInnerBodyEnd = pScan;` |
|        26 |  2705 | `				iNestInner = 0;` |
|       156 |  2706 | `				while( pInnerBodyEnd < pEnd ){` |
|       138 |  2707 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|         - |  2708 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|         - |  2709 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|         7 |  2710 | `						break;` |
|         - |  2711 | `					}` |
|       132 |  2712 | `					if( pInnerBodyEnd->nType &` |
|         - |  2713 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         6 |  2714 | `						iNestInner++;` |
|       130 |  2715 | `					}else if( pInnerBodyEnd->nType &` |
|         - |  2716 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         6 |  2717 | `						iNestInner--;` |
|         2 |  2718 | `					}` |
|       132 |  2719 | `					pInnerBodyEnd++;` |
|         2 |  2720 | `				}` |
|         - |  2721 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|         - |  2722 | `				 * the outer's body: a default value is evaluated at call time` |
|         - |  2723 | `				 * in the outer frame, so any free variable it references is` |
|         - |  2724 | `				 * an outer capture. We must NOT scan the parameter-name` |
|         - |  2725 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|         - |  2726 | `				 * or those names leak into the outer's closure environment.` |
|         - |  2727 | `				 *` |
|         - |  2728 | `				 * Walk the signature argument-by-argument, splitting on` |
|         - |  2729 | `				 * top-level commas, and for each argument scan only the token` |
|         - |  2730 | `				 * range after the '=' sign. */` |
|         - |  2731 | `				{` |
|        26 |  2732 | `					SyToken *pArgStart = pInnerSigStart;` |
|        44 |  2733 | `					while( pArgStart < pInnerSigEnd ){` |
|        20 |  2734 | `						SyToken *pArgEnd = pArgStart;` |
|        20 |  2735 | `						SyToken *pEq = 0;` |
|        20 |  2736 | `						int iNestArg = 0;` |
|        68 |  2737 | `						while( pArgEnd < pInnerSigEnd ){` |
|        50 |  2738 | `							if( iNestArg == 0` |
|        52 |  2739 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|         3 |  2740 | `								break;` |
|         - |  2741 | `							}` |
|        50 |  2742 | `							if( pArgEnd->nType &` |
|         - |  2743 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  2744 | `								iNestArg++;` |
|        50 |  2745 | `							}else if( pArgEnd->nType &` |
|         - |  2746 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  2747 | `								iNestArg--;` |
|       ! 0 |  2748 | `							}` |
|        48 |  2749 | `							if( pEq == 0 && iNestArg == 0` |
|        44 |  2750 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|         7 |  2751 | `								pEq = pArgEnd;` |
|         3 |  2752 | `							}` |
|        50 |  2753 | `							pArgEnd++;` |
|         2 |  2754 | `						}` |
|        20 |  2755 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|        10 |  2756 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         3 |  2757 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|         7 |  2758 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  2759 | `								return SXERR_ABORT;` |
|         - |  2760 | `							}` |
|         3 |  2761 | `						}` |
|        20 |  2762 | `						pArgStart = pArgEnd;` |
|        18 |  2763 | `						if( pArgStart < pInnerSigEnd` |
|        12 |  2764 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|         3 |  2765 | `							pArgStart++;` |
|         1 |  2766 | `						}` |
|         2 |  2767 | `					}` |
|         - |  2768 | `				}` |
|        38 |  2769 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        12 |  2770 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|        26 |  2771 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2772 | `					return SXERR_ABORT;` |
|         - |  2773 | `				}` |
|        26 |  2774 | `				pScan = pInnerBodyEnd;` |
|        26 |  2775 | `				continue;` |
|         - |  2776 | `			}` |
|         6 |  2777 | `		}` |
|      2794 |  2778 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      2582 |  2779 | `			pScan++;` |
|      2582 |  2780 | `			continue;` |
|         - |  2781 | `		}` |
|         - |  2782 | `		{` |
|         - |  2783 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       215 |  2784 | `			SyToken *pDollar = pScan;` |
|       318 |  2785 | `			while( &pDollar[1] < pEnd` |
|       215 |  2786 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2787 | `				pDollar++;` |
|       ! 0 |  2788 | `			}` |
|       215 |  2789 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2790 | `				break;` |
|         - |  2791 | `			}` |
|       215 |  2792 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2793 | `				pScan = pDollar + 1;` |
|       ! 0 |  2794 | `				continue;` |
|         - |  2795 | `			}` |
|       321 |  2796 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       212 |  2797 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       106 |  2798 | `				aShadow,nShadow);` |
|       215 |  2799 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2800 | `				return SXERR_ABORT;` |
|         - |  2801 | `			}` |
|       215 |  2802 | `			pScan = pDollar + 2;` |
|         - |  2803 | `		}` |
|         3 |  2804 | `	}` |
|       516 |  2805 | `	return SXRET_OK;` |
|       260 |  2806 | `}` |
|         - |  2807 | `/*` |
|         - |  2808 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2809 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2810 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2811 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2812 | ` * $this is also made available.` |
|         - |  2813 | ` */` |
|       488 |  2814 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2815 | `{` |
|         - |  2816 | `	ph7_vm_func *pFunc;` |
|         - |  2817 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2818 | `	GenBlock *pBlock;` |
|         - |  2819 | `	SySet *pInstrContainer;` |
|         - |  2820 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|         - |  2821 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|         - |  2822 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|         - |  2823 | `	SyToken *pSavedEnd;` |
|         - |  2824 | `	ph7_vm_func_arg *aArgs;` |
|         - |  2825 | `	char zName[512];` |
|         - |  2826 | `	static int iCnt = 1;` |
|         - |  2827 | `	char *zDup;` |
|         - |  2828 | `	SyToken *pTokKw;` |
|         - |  2829 | `	sxu32 nLen;` |
|         - |  2830 | `	sxu32 nLine;` |
|       493 |  2831 | `	sxi32 iFlags = 0;` |
|       493 |  2832 | `	int bStatic = 0;` |
|         - |  2833 | `	sxi32 rc;` |
|         - |  2834 | `	sxu32 n;` |
|       244 |  2835 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2836 |  |
|       493 |  2837 | `	nLine = pGen->pIn->nLine;` |
|         - |  2838 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       493 |  2839 | `	pTokKw = pGen->pIn;` |
|         - |  2840 | `	/* Optional 'static' prefix */` |
|       488 |  2841 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       493 |  2842 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         9 |  2843 | `		bStatic = 1;` |
|         9 |  2844 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         9 |  2845 | `		pGen->pIn++;` |
|         4 |  2846 | `	}` |
|         - |  2847 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       488 |  2848 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       493 |  2849 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2850 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2851 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2852 | `		return SXERR_SYNTAX;` |
|         - |  2853 | `	}` |
|       493 |  2854 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2855 | `	/* Optional '&' — return by reference */` |
|       493 |  2856 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2857 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2858 | `		pGen->pIn++;` |
|       ! 0 |  2859 | `	}` |
|         - |  2860 | `	/* Expect '(' */` |
|       493 |  2861 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  2862 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2863 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2864 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|         2 |  2865 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2866 | `		}else{` |
|       ! 0 |  2867 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2868 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|         - |  2869 | `		}` |
|         3 |  2870 | `		return SXERR_SYNTAX;` |
|         - |  2871 | `	}` |
|       491 |  2872 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2873 | `	/* Delimit the parameter list */` |
|       491 |  2874 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       491 |  2875 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2876 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2877 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2878 | `		return SXERR_SYNTAX;` |
|         - |  2879 | `	}` |
|         - |  2880 | `	/* Allocate the function state */` |
|       488 |  2881 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       488 |  2882 | `	if( pFunc == 0 ){` |
|       ! 0 |  2883 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2884 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2885 | `		return SXERR_ABORT;` |
|         - |  2886 | `	}` |
|         - |  2887 | `	/* Generate a unique lambda name */` |
|       488 |  2888 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       488 |  2889 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2890 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2891 | `	}` |
|       488 |  2892 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       488 |  2893 | `	if( zDup == 0 ){` |
|       ! 0 |  2894 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2895 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2896 | `		return SXERR_ABORT;` |
|         - |  2897 | `	}` |
|       488 |  2898 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2899 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       488 |  2900 | `	pFunc->nLine = nLine;` |
|         - |  2901 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       488 |  2902 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2903 | `		return SXERR_ABORT;` |
|         - |  2904 | `	}` |
|         - |  2905 | `	/* Collect function arguments */` |
|       488 |  2906 | `	if( pGen->pIn < pSigEnd ){` |
|       133 |  2907 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       133 |  2908 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2909 | `			return SXERR_ABORT;` |
|         - |  2910 | `		}` |
|        65 |  2911 | `	}` |
|         - |  2912 | `	/* Point past ')' and parse optional return type */` |
|       488 |  2913 | `	pGen->pIn = &pSigEnd[1];` |
|       488 |  2914 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       488 |  2915 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2916 | `		return SXERR_ABORT;` |
|       488 |  2917 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2918 | `		return SXERR_SYNTAX;` |
|         - |  2919 | `	}` |
|         - |  2920 | `	/* Expect '=>' */` |
|       488 |  2921 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  2922 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2923 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2924 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|         2 |  2925 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2926 | `		}else{` |
|       ! 0 |  2927 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2928 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|         - |  2929 | `		}` |
|         3 |  2930 | `		return SXERR_SYNTAX;` |
|         - |  2931 | `	}` |
|       486 |  2932 | `	pGen->pIn++; /* Jump '=>' */` |
|       486 |  2933 | `	pBodyStart = pGen->pIn;` |
|       486 |  2934 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2935 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2936 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2937 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2938 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       486 |  2939 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2940 | `	{` |
|       486 |  2941 | `		SyString *aShadow = 0;` |
|       486 |  2942 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       486 |  2943 | `		if( nShadow > 0 ){` |
|       131 |  2944 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       128 |  2945 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       131 |  2946 | `			if( aShadow == 0 ){` |
|       ! 0 |  2947 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2948 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2949 | `				return SXERR_ABORT;` |
|         - |  2950 | `			}` |
|       295 |  2951 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       167 |  2952 | `				aShadow[n] = aArgs[n].sName;` |
|        85 |  2953 | `			}` |
|        64 |  2954 | `		}` |
|       727 |  2955 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       241 |  2956 | `			aShadow,nShadow);` |
|       486 |  2957 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2958 | `			return SXERR_ABORT;` |
|         - |  2959 | `		}` |
|         - |  2960 | `	}` |
|         - |  2961 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2962 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2963 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2964 | `	 * $this. */` |
|       486 |  2965 | `	if( !bStatic ){` |
|         - |  2966 | `		char *zThisDup;` |
|       478 |  2967 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       478 |  2968 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2969 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2970 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2971 | `			return SXERR_ABORT;` |
|         - |  2972 | `		}` |
|       478 |  2973 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       478 |  2974 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       478 |  2975 | `		sEnv.nIdx = SXU32_HIGH;` |
|       478 |  2976 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       478 |  2977 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       478 |  2978 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       237 |  2979 | `	}` |
|         - |  2980 | `	/* Arrow functions are always closures */` |
|       486 |  2981 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2982 | `	/* Compile the body expression as an implicit return */` |
|       727 |  2983 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       241 |  2984 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       486 |  2985 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2986 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2987 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2988 | `		return SXERR_ABORT;` |
|         - |  2989 | `	}` |
|       486 |  2990 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       486 |  2991 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       486 |  2992 | `	pSavedEnd = pGen->pEnd;` |
|       486 |  2993 | `	pGen->pIn = pBodyStart;` |
|       486 |  2994 | `	pGen->pEnd = pBodyEnd;` |
|       486 |  2995 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       486 |  2996 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2997 | `		return SXERR_ABORT;` |
|         - |  2998 | `	}` |
|         - |  2999 | `	/* The cursor stopped just past the body expression */` |
|       486 |  3000 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  3001 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  3002 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  3003 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  3004 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       486 |  3005 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       486 |  3006 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       486 |  3007 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       486 |  3008 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       486 |  3009 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  3010 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       486 |  3011 | `	pGen->pIn = pBodyEnd;` |
|       486 |  3012 | `	pGen->pEnd = pSavedEnd;` |
|         - |  3013 | `	/* Emit the load-closure instruction */` |
|       486 |  3014 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       486 |  3015 | `	return SXRET_OK;` |
|       249 |  3016 | `}` |
|         - |  3017 | `/*` |
|         - |  3018 | ` * Compile a single arm's expression range into a freshly-allocated` |
|         - |  3019 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|         - |  3020 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|         - |  3021 | ` * expression's value.` |
|         - |  3022 | ` */` |
|       354 |  3023 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|         - |  3024 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|         3 |  3025 | `{` |
|         - |  3026 | `	SySet *pInstrContainer;` |
|         - |  3027 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  3028 | `	GenBlock *pArmBlock;` |
|         - |  3029 | `	sxi32 rc;` |
|       357 |  3030 | `	pTmpIn  = pGen->pIn;` |
|       357 |  3031 | `	pTmpEnd = pGen->pEnd;` |
|       357 |  3032 | `	pGen->pIn  = pStart;` |
|       357 |  3033 | `	pGen->pEnd = pStop;` |
|       357 |  3034 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       357 |  3035 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|         - |  3036 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|         - |  3037 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|         - |  3038 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|         - |  3039 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|         - |  3040 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|       534 |  3041 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       177 |  3042 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|       357 |  3043 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3044 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  3045 | `		pGen->pIn  = pTmpIn;` |
|       ! 0 |  3046 | `		pGen->pEnd = pTmpEnd;` |
|       ! 0 |  3047 | `		return SXERR_ABORT;` |
|         - |  3048 | `	}` |
|       357 |  3049 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       357 |  3050 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       357 |  3051 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       357 |  3052 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       357 |  3053 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       357 |  3054 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       357 |  3055 | `	pGen->pIn  = pTmpIn;` |
|       357 |  3056 | `	pGen->pEnd = pTmpEnd;` |
|       357 |  3057 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3058 | `		return SXERR_ABORT;` |
|         - |  3059 | `	}` |
|       357 |  3060 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 |  3061 | `		return SXERR_EMPTY;` |
|         - |  3062 | `	}` |
|       357 |  3063 | `	return SXRET_OK;` |
|       180 |  3064 | `}` |
|         - |  3065 | `/*` |
|         - |  3066 | ` * Compile a PHP 8.0 match expression:` |
|         - |  3067 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|         - |  3068 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|         - |  3069 | ` * Strict comparison (===) is used between the subject and each condition.` |
|         - |  3070 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|         - |  3071 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|         - |  3072 | ` */` |
|         - |  3073 | `/*` |
|         - |  3074 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|         - |  3075 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|         - |  3076 | ` * caller can bail out of the current expression.` |
|         - |  3077 | ` */` |
|         2 |  3078 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|         1 |  3079 | `{` |
|         - |  3080 | `	va_list ap;` |
|         - |  3081 | `	sxi32 rc;` |
|         - |  3082 | `	SyBlob sMsg;` |
|         3 |  3083 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|         3 |  3084 | `	va_start(ap,zFmt);` |
|         3 |  3085 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|         3 |  3086 | `	va_end(ap);` |
|         3 |  3087 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|         3 |  3088 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|         3 |  3089 | `	SyBlobRelease(&sMsg);` |
|         3 |  3090 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3091 | `		return SXERR_ABORT;` |
|         - |  3092 | `	}` |
|         3 |  3093 | `	return SXERR_SYNTAX;` |
|         2 |  3094 | `}` |
|         - |  3095 | `/*` |
|         - |  3096 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|         - |  3097 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|         - |  3098 | ` * Returns the stop token pointer (or pEnd if none found).` |
|         - |  3099 | ` */` |
|       356 |  3100 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|         4 |  3101 | `{` |
|       360 |  3102 | `	SyToken *pCur = pStart;` |
|       360 |  3103 | `	int iNest = 0;` |
|       838 |  3104 | `	while( pCur < pEnd ){` |
|       802 |  3105 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        13 |  3106 | `			iNest++;` |
|       796 |  3107 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        13 |  3108 | `			iNest--;` |
|       784 |  3109 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|       323 |  3110 | `			return pCur;` |
|         - |  3111 | `		}` |
|       482 |  3112 | `		pCur++;` |
|         4 |  3113 | `	}` |
|        39 |  3114 | `	return pEnd;` |
|       182 |  3115 | `}` |
|        72 |  3116 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3117 | `{` |
|         - |  3118 | `	ph7_match *pMatch;` |
|         - |  3119 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|        77 |  3120 | `	int bHasDefault = 0;` |
|         - |  3121 | `	sxu32 nLine;` |
|         - |  3122 | `	sxi32 rc;` |
|        36 |  3123 | `	SXUNUSED(iCompileFlag);` |
|        77 |  3124 | `	nLine = pGen->pIn->nLine;` |
|        77 |  3125 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|         - |  3126 | `	/* Expect '(' */` |
|        77 |  3127 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  3128 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3129 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|       ! 0 |  3130 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|         - |  3131 | `	}` |
|        77 |  3132 | `	pGen->pIn++; /* Jump '(' */` |
|        77 |  3133 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|        77 |  3134 | `	if( pSubjEnd >= pGen->pEnd ){` |
|       ! 0 |  3135 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3136 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         - |  3137 | `	}` |
|        77 |  3138 | `	if( pGen->pIn >= pSubjEnd ){` |
|       ! 0 |  3139 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3140 | `			"syntax error, unexpected \")\", expecting match subject");` |
|         - |  3141 | `	}` |
|         - |  3142 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|        77 |  3143 | `	pSavedEnd = pGen->pEnd;` |
|        77 |  3144 | `	pGen->pEnd = pSubjEnd;` |
|        77 |  3145 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        77 |  3146 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3147 | `		return SXERR_ABORT;` |
|         - |  3148 | `	}` |
|        77 |  3149 | `	pGen->pEnd = pSavedEnd;` |
|        77 |  3150 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|         - |  3151 | `	/* Expect '{' */` |
|        77 |  3152 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 |  3153 | `		return GenStateMatchError(pGen,` |
|       ! 0 |  3154 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  3155 | `			"syntax error, expecting \"{\" after match subject");` |
|         - |  3156 | `	}` |
|        77 |  3157 | `	pGen->pIn++; /* Jump '{' */` |
|        77 |  3158 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|        77 |  3159 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  3160 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3161 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|         - |  3162 | `	}` |
|         - |  3163 | `	/* Allocate ph7_match container */` |
|        77 |  3164 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|        77 |  3165 | `	if( pMatch == 0 ){` |
|       ! 0 |  3166 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  3167 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3168 | `		return SXERR_ABORT;` |
|         - |  3169 | `	}` |
|        77 |  3170 | `	SyZero(pMatch,sizeof(ph7_match));` |
|        77 |  3171 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|         - |  3172 | `	/* Iterate arms */` |
|       259 |  3173 | `	while( pGen->pIn < pBodyEnd ){` |
|         - |  3174 | `		ph7_match_arm sArm;` |
|         - |  3175 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|       190 |  3176 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|       190 |  3177 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|       190 |  3178 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|       190 |  3179 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3180 | `		/* 'default' arm? */` |
|       186 |  3181 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       107 |  3182 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|        22 |  3183 | `			if( bHasDefault ){` |
|         3 |  3184 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|         - |  3185 | `					"Match expressions may only contain one default arm");` |
|         4 |  3186 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  3187 | `			}` |
|        20 |  3188 | `			sArm.bDefault = 1;` |
|        20 |  3189 | `			bHasDefault = 1;` |
|        20 |  3190 | `			pGen->pIn++;` |
|        20 |  3191 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       ! 0 |  3192 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3193 | `					"syntax error, expecting \"=>\" after 'default'");` |
|         - |  3194 | `			}` |
|        20 |  3195 | `			pGen->pIn++; /* Jump '=>' */` |
|        11 |  3196 | `		}else{` |
|         - |  3197 | `			/* Condition list: cond (',' cond)* '=>' */` |
|       170 |  3198 | `			pCondStart = pGen->pIn;` |
|       170 |  3199 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|         - |  3200 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       178 |  3201 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|         - |  3202 | `				SySet sCondBc;` |
|         9 |  3203 | `				if( pCondStart >= pArrow ){` |
|       ! 0 |  3204 | `					return GenStateMatchError(pGen,nArmLine,` |
|         - |  3205 | `						"syntax error, empty match condition expression");` |
|         - |  3206 | `				}` |
|         9 |  3207 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         9 |  3208 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|         9 |  3209 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3210 | `					return SXERR_ABORT;` |
|         - |  3211 | `				}` |
|         9 |  3212 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         9 |  3213 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|         9 |  3214 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|         - |  3215 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|         1 |  3216 | `			}` |
|       170 |  3217 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  3218 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3219 | `					"syntax error, expecting \"=>\" in match arm");` |
|         - |  3220 | `			}` |
|       167 |  3221 | `			if( pCondStart >= pArrow ){` |
|       ! 0 |  3222 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3223 | `					"syntax error, empty match condition expression");` |
|         - |  3224 | `			}` |
|         - |  3225 | `			{` |
|         - |  3226 | `				SySet sCondBc;` |
|       167 |  3227 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       167 |  3228 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       167 |  3229 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3230 | `					return SXERR_ABORT;` |
|         - |  3231 | `				}` |
|       167 |  3232 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         - |  3233 | `			}` |
|       167 |  3234 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|         - |  3235 | `		}` |
|         - |  3236 | `		/* Compile result expression: up to top-level ',' or body end */` |
|       185 |  3237 | `		pResStart = pGen->pIn;` |
|       185 |  3238 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|       185 |  3239 | `		if( pResStart >= pResEnd ){` |
|       ! 0 |  3240 | `			return GenStateMatchError(pGen,nArmLine,` |
|         - |  3241 | `				"syntax error, expected expression after \"=>\"");` |
|         - |  3242 | `		}` |
|       185 |  3243 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|       185 |  3244 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3245 | `			return SXERR_ABORT;` |
|         - |  3246 | `		}` |
|       185 |  3247 | `		pGen->pIn = pResEnd;` |
|       185 |  3248 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       151 |  3249 | `			pGen->pIn++; /* Skip trailing ',' */` |
|        74 |  3250 | `		}` |
|       185 |  3251 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|         3 |  3252 | `	}` |
|        71 |  3253 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|        71 |  3254 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|        71 |  3255 | `	return SXRET_OK;` |
|        41 |  3256 | `}` |
|         - |  3257 | `/*` |
|         - |  3258 | ` * Compile a backtick quoted string.` |
|         - |  3259 | ` */` |
|         4 |  3260 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  3261 | `{` |
|         - |  3262 | `	static const SyString sName = { "shell_exec", sizeof("shell_exec")-1 };` |
|         6 |  3263 | `	sxu32 nIdx = 0;` |
|         - |  3264 | `	sxi32 rc;` |
|         - |  3265 | `	/*` |
|         - |  3266 | ``	 * `cmd` IS shell_exec("cmd") in php — it interpolates like a double-quoted string,`` |
|         - |  3267 | `	 * runs the command and yields its output. PH7 refused to run it at all (TICKET` |
|         - |  3268 | `	 * 1433-40) and quietly evaluated to NULL. php 8.5 deprecates the syntax but still` |
|         - |  3269 | `	 * executes it, so compile it to the real call and say what php says.` |
|         - |  3270 | `	 */` |
|         6 |  3271 | `	PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pGen->pIn->nLine,` |
|         - |  3272 | ``		"The backtick (`) operator is deprecated, use shell_exec() instead");`` |
|         - |  3273 | `	/* The body interpolates exactly like a double-quoted string */` |
|         6 |  3274 | `	pGen->pIn->nType &= ~PH7_TK_BSTR;` |
|         6 |  3275 | `	pGen->pIn->nType \|= PH7_TK_DSTR;` |
|         6 |  3276 | `	rc = PH7_CompileString(&(*pGen),iCompileFlag);` |
|         6 |  3277 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3278 | `		return rc;` |
|         - |  3279 | `	}` |
|         - |  3280 | `	/* ... and the command string is then handed to shell_exec() */` |
|         6 |  3281 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|         6 |  3282 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         6 |  3283 | `		if( pObj == 0 ){` |
|       ! 0 |  3284 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  3285 | `			return SXERR_ABORT;` |
|         - |  3286 | `		}` |
|         6 |  3287 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|         6 |  3288 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|         2 |  3289 | `	}` |
|         6 |  3290 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         6 |  3291 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         6 |  3292 | `	return SXRET_OK;` |
|         4 |  3293 | `}` |
|         - |  3294 | `/*` |
|         - |  3295 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|         - |  3296 | ` * construct.` |
|         - |  3297 | ` */` |
|        68 |  3298 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3299 | `{` |
|         - |  3300 | `	SyString *pName;` |
|         - |  3301 | `	sxu32 nKeyID;` |
|         - |  3302 | `	sxi32 rc;` |
|         - |  3303 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        73 |  3304 | `	pName = &pGen->pIn->sData;` |
|        73 |  3305 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        73 |  3306 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        73 |  3307 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|         9 |  3308 | `		SyToken *pTmp,*pNext = 0;` |
|         - |  3309 | `		/* Compile arguments one after one */` |
|         9 |  3310 | `		pTmp = pGen->pEnd;` |
|         - |  3311 | `		/* Symisc eXtension to the PHP programming language:` |
|         - |  3312 | `		 * 'echo' can be used in the context of a function which` |
|         - |  3313 | `		 *  mean that the following expression is valid:` |
|         - |  3314 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|         - |  3315 | `		 */` |
|         9 |  3316 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        17 |  3317 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|         9 |  3318 | `			if( pGen->pIn < pNext ){` |
|         9 |  3319 | `				pGen->pEnd = pNext;` |
|         9 |  3320 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|         9 |  3321 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3322 | `					return SXERR_ABORT;` |
|         - |  3323 | `				}` |
|         9 |  3324 | `				if( rc != SXERR_EMPTY ){` |
|         - |  3325 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|         - |  3326 | `					 * without the overhead of a function call.` |
|         - |  3327 | `					 * This is a very powerful optimization that improve` |
|         - |  3328 | `					 * performance greatly.` |
|         - |  3329 | `					 */` |
|         9 |  3330 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|         4 |  3331 | `				}` |
|         4 |  3332 | `			}` |
|         - |  3333 | `			/* Jump trailing commas */` |
|         9 |  3334 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|       ! 0 |  3335 | `				pNext++;` |
|       ! 0 |  3336 | `			}` |
|         9 |  3337 | `			pGen->pIn = pNext;` |
|         1 |  3338 | `		}` |
|         - |  3339 | `		/* Restore token stream */` |
|         9 |  3340 | `		pGen->pEnd = pTmp;` |
|         5 |  3341 | `	}else{` |
|        65 |  3342 | `		sxi32 nArg = 0;` |
|        65 |  3343 | `		sxu32 nIdx = 0;` |
|        65 |  3344 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        65 |  3345 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3346 | `			return SXERR_ABORT;` |
|        65 |  3347 | `		}else if(rc != SXERR_EMPTY ){` |
|        65 |  3348 | `			nArg = 1;` |
|        30 |  3349 | `		}` |
|        65 |  3350 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3351 | `			ph7_value *pObj;` |
|         - |  3352 | `			/* Emit the call instruction */` |
|        29 |  3353 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        29 |  3354 | `			if( pObj == 0 ){` |
|       ! 0 |  3355 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3356 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3357 | `				return SXERR_ABORT;` |
|         - |  3358 | `			}` |
|        29 |  3359 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3360 | `			/* Install in the literal table */` |
|        29 |  3361 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        12 |  3362 | `		}` |
|         - |  3363 | `		/* Emit the call instruction */` |
|        65 |  3364 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        65 |  3365 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3366 | `	}` |
|         - |  3367 | `	/* Node successfully compiled */` |
|        73 |  3368 | `	return SXRET_OK;` |
|        39 |  3369 | `}` |
|         - |  3370 | `/*` |
|         - |  3371 | ` * Compile a node holding a variable declaration.` |
|         - |  3372 | ` * According to the PHP language reference` |
|         - |  3373 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|         - |  3374 | ` *  The variable name is case-sensitive.` |
|         - |  3375 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|         - |  3376 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3377 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|         - |  3378 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|         - |  3379 | ` *  Note: $this is a special variable that can't be assigned.` |
|         - |  3380 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|         - |  3381 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|         - |  3382 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|         - |  3383 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|         - |  3384 | ` *  the chapter on Expressions.` |
|         - |  3385 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|         - |  3386 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|         - |  3387 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|         - |  3388 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|         - |  3389 | ` *  is being assigned (the source variable).` |
|         - |  3390 | ` */` |
|  19334824 |  3391 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3392 | `{` |
|  19334829 |  3393 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3394 | `	sxi32 iVv;` |
|         - |  3395 | `	sxi32 iP1;` |
|         - |  3396 | `	void *p3;` |
|         - |  3397 | `	sxi32 rc;` |
|  19334829 |  3398 | `	iVv = -1; /* Variable variable counter */` |
|  38669665 |  3399 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  19334841 |  3400 | `		pGen->pIn++;` |
|  19334841 |  3401 | `		iVv++;` |
|         5 |  3402 | `	}` |
|  19334829 |  3403 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3404 | `		/* Invalid variable name */` |
|       ! 0 |  3405 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3406 | `		if( rc == SXERR_ABORT ){` |
|         - |  3407 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3408 | `			return SXERR_ABORT;` |
|         - |  3409 | `		}` |
|       ! 0 |  3410 | `		return SXRET_OK;` |
|         - |  3411 | `	}` |
|  19334829 |  3412 | `	p3  = 0;` |
|  19334829 |  3413 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|         - |  3414 | `		/* Dynamic variable creation */` |
|        19 |  3415 | `		pGen->pIn++;  /* Jump the open curly */` |
|        19 |  3416 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|        19 |  3417 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3418 | `			/* Empty expression */` |
|         - |  3419 | `			{` |
|         - |  3420 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|         - |  3421 | `			 * the "expecting" tail only appears when something could still follow. */` |
|         3 |  3422 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|         3 |  3423 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|         1 |  3424 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|         - |  3425 | `			}` |
|         3 |  3426 | `			return SXRET_OK;` |
|         - |  3427 | `		}` |
|         - |  3428 | `		/* Compile the expression holding the variable name */` |
|        16 |  3429 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        16 |  3430 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3431 | `			return SXERR_ABORT;` |
|        16 |  3432 | `		}else if( rc == SXERR_EMPTY ){` |
|         3 |  3433 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|         3 |  3434 | `			return SXRET_OK;` |
|         - |  3435 | `		}` |
|         7 |  3436 | `	}else{` |
|         - |  3437 | `		SyHashEntry *pEntry;` |
|         - |  3438 | `		SyString *pName;` |
|  19334813 |  3439 | `		char *zName = 0;` |
|         - |  3440 | `		/* Extract variable name */` |
|  19334813 |  3441 | `		pName = &pGen->pIn->sData;` |
|         - |  3442 | `		/* Advance the stream cursor */` |
|  19334813 |  3443 | `		pGen->pIn++;` |
|  19334813 |  3444 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  19334813 |  3445 | `		if( pEntry == 0 ){` |
|         - |  3446 | `			/* Duplicate name */` |
|   1161521 |  3447 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   1161521 |  3448 | `			if( zName == 0 ){` |
|       ! 0 |  3449 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3450 | `				return SXERR_ABORT;` |
|         - |  3451 | `			}` |
|         - |  3452 | `			/* Install in the hashtable */` |
|   1161521 |  3453 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    580763 |  3454 | `		}else{` |
|         - |  3455 | `			/* Name already available */` |
|  18173297 |  3456 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3457 | `		}` |
|  19334813 |  3458 | `		p3 = (void *)zName;` |
|         - |  3459 | `	}` |
|  19334825 |  3460 | `	iP1 = 0;` |
|  19334825 |  3461 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   5714899 |  3462 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3463 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   5711051 |  3464 | `			iP1 = 1;` |
|   2855523 |  3465 | `		}` |
|   2857447 |  3466 | `	}` |
|         - |  3467 | `	/* Emit the load instruction */` |
|  19334825 |  3468 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  19334837 |  3469 | `	while( iVv > 0 ){` |
|        13 |  3470 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3471 | `		iVv--;` |
|         1 |  3472 | `	}` |
|         - |  3473 | `	/* Node successfully compiled */` |
|  19334825 |  3474 | `	return SXRET_OK;` |
|   9667417 |  3475 | `}` |
|         - |  3476 | `/*` |
|         - |  3477 | ` * Load a literal.` |
|         - |  3478 | ` */` |
|  11951730 |  3479 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3480 | `{` |
|  11951735 |  3481 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3482 | `	ph7_value *pObj;` |
|         - |  3483 | `	SyString *pStr;` |
|         - |  3484 | `	sxu32 nIdx;` |
|         - |  3485 | `	/* Extract token value */` |
|  11951735 |  3486 | `	pStr = &pToken->sData;` |
|         - |  3487 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|         - |  3488 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|         - |  3489 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|         - |  3490 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
|  11951735 |  3491 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|         - |  3492 | `		/* fall through to the plain-string literal path */` |
|   9592539 |  3493 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   1460443 |  3494 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3495 | `			/* NULL constant are always indexed at 0 */` |
|    983741 |  3496 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    983741 |  3497 | `			return SXRET_OK;` |
|    476707 |  3498 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3499 | `			/* TRUE constant are always indexed at 1 */` |
|    326255 |  3500 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    326255 |  3501 | `			return SXRET_OK;` |
|         5 |  3502 | `		}` |
|   6584173 |  3503 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1472084 |  3504 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3505 | `			/* FALSE constant are always indexed at 2 */` |
|    715751 |  3506 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    715751 |  3507 | `			return SXRET_OK;` |
|   5186176 |  3508 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    258034 |  3509 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3510 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      3837 |  3511 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3837 |  3512 | `			if( pObj == 0 ){` |
|       ! 0 |  3513 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3514 | `				return SXERR_ABORT;` |
|         - |  3515 | `			}` |
|      3837 |  3516 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3517 | `			/* Emit the load constant instruction */` |
|      3837 |  3518 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3837 |  3519 | `			return SXRET_OK;` |
|   5180423 |  3520 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|    458485 |  3521 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|   5253708 |  3522 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|    408556 |  3523 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|         - |  3524 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|         - |  3525 | `			 * file being compiled (where the token is written), NOT the runtime` |
|         - |  3526 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|         - |  3527 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|         - |  3528 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|         - |  3529 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|      3919 |  3530 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|      3919 |  3531 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      3919 |  3532 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3919 |  3533 | `			if( pObj == 0 ){` |
|       ! 0 |  3534 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3535 | `				return SXERR_ABORT;` |
|         - |  3536 | `			}` |
|      3919 |  3537 | `			if( pFile && pFile->nByte > 0 ){` |
|        95 |  3538 | `				if( bDir ){` |
|         - |  3539 | `					const char *zDir;` |
|         - |  3540 | `					int nLen;` |
|         - |  3541 | `					SyString sDir;` |
|        47 |  3542 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|        47 |  3543 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|        47 |  3544 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|        25 |  3545 | `				}else{` |
|        51 |  3546 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|         - |  3547 | `				}` |
|        50 |  3548 | `			}else{` |
|         - |  3549 | `				SyString sMem;` |
|      3829 |  3550 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|      3829 |  3551 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|         - |  3552 | `			}` |
|      3919 |  3553 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3919 |  3554 | `			return SXRET_OK;` |
|   5151094 |  3555 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    203362 |  3556 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3557 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         7 |  3558 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3559 | `			if( pObj == 0 ){` |
|       ! 0 |  3560 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3561 | `				return SXERR_ABORT;` |
|         - |  3562 | `			}` |
|         7 |  3563 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3564 | `				SyString sNs;` |
|         7 |  3565 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         7 |  3566 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         4 |  3567 | `			}else{` |
|       ! 0 |  3568 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3569 | `			}` |
|         7 |  3570 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         7 |  3571 | `			return SXRET_OK;` |
|   5162775 |  3572 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    376860 |  3573 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   5199504 |  3574 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    300218 |  3575 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3576 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3577 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3578 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3579 | `				/* Point to the upper block */` |
|        11 |  3580 | `				pBlock = pBlock->pParent;` |
|         1 |  3581 | `			}` |
|        11 |  3582 | `			if( pBlock == 0 ){` |
|         - |  3583 | `				/* Called in the global scope,load NULL */` |
|         5 |  3584 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3585 | `			}else{` |
|         - |  3586 | `				/* Extract the target function/method */` |
|         7 |  3587 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3588 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|         7 |  3589 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3590 | `				if( pObj == 0 ){` |
|       ! 0 |  3591 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3592 | `					return SXERR_ABORT;` |
|         - |  3593 | `				}` |
|         - |  3594 | `				/*` |
|         - |  3595 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|         - |  3596 | `				 * function name inside a plain function (php does not answer "" there —` |
|         - |  3597 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|         - |  3598 | `				 * unqualified in every method).` |
|         - |  3599 | `				 */` |
|         8 |  3600 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 |  3601 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         - |  3602 | `					SyBlob sQual;` |
|         - |  3603 | `					SyString sOut;` |
|         3 |  3604 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|         3 |  3605 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|         3 |  3606 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|         3 |  3607 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|         3 |  3608 | `					SyBlobRelease(&sQual);` |
|         2 |  3609 | `				}else{` |
|         5 |  3610 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3611 | `				}` |
|         - |  3612 | `				/* Emit the load constant instruction */` |
|         7 |  3613 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3614 | `			}` |
|        11 |  3615 | `			return SXRET_OK;` |
|         - |  3616 | `	}` |
|         - |  3617 | `	/* Query literal table */` |
|   9918241 |  3618 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3619 | `		ph7_value *pLitObj;` |
|         - |  3620 | `		/* Unknown literal,install it in the literal table */` |
|   1891801 |  3621 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1891801 |  3622 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3623 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3624 | `			return SXERR_ABORT;` |
|         - |  3625 | `		}` |
|   1891801 |  3626 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1891801 |  3627 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    945898 |  3628 | `	}` |
|         - |  3629 | `	/* Emit the load constant instruction */` |
|   9918241 |  3630 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9918241 |  3631 | `	return SXRET_OK;` |
|   5975870 |  3632 | `}` |
|         - |  3633 | `/*` |
|         - |  3634 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3635 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3636 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3637 | ` * Otherwise, load the simple literal directly.` |
|         - |  3638 | ` */` |
|  11955638 |  3639 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3640 | `{` |
|         - |  3641 | `	sxi32 rc;` |
|  11955643 |  3642 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3643 | `		return SXRET_OK;` |
|         - |  3644 | `	}` |
|         - |  3645 | `	/* Check if this is a multi-token namespace path */` |
|  11955643 |  3646 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3647 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3913 |  3648 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3913 |  3649 | `		int isAbsolute = 0;` |
|      3913 |  3650 | `		SyBlobReset(pWorker);` |
|         - |  3651 | `		/* Check for leading backslash (absolute path) */` |
|      3913 |  3652 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3897 |  3653 | `			isAbsolute = 1;` |
|      3897 |  3654 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1946 |  3655 | `		}` |
|         - |  3656 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|         - |  3657 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|         - |  3658 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|         - |  3659 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|         - |  3660 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|         - |  3661 | `		{` |
|         - |  3662 | `			SyBlob sRaw;` |
|      3913 |  3663 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|      4065 |  3664 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4065 |  3665 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        81 |  3666 | `					SyBlobAppend(&sRaw,"\\",1);` |
|        43 |  3667 | `				}else{` |
|      3989 |  3668 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3669 | `				}` |
|      4065 |  3670 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3913 |  3671 | `					pGen->pIn++;` |
|      3913 |  3672 | `					break;` |
|         - |  3673 | `				}` |
|       157 |  3674 | `				pGen->pIn++;` |
|         5 |  3675 | `			}` |
|      3913 |  3676 | `			if( isAbsolute ){` |
|      3897 |  3677 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|      1951 |  3678 | `			}else{` |
|        18 |  3679 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|        18 |  3680 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|        18 |  3681 | `				sxu32 nFirst = 0;` |
|         - |  3682 | `				SyHashEntry *pNsImp;` |
|        84 |  3683 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|        18 |  3684 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|        18 |  3685 | `				if( pNsImp ){` |
|         - |  3686 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|        15 |  3687 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|        15 |  3688 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|        15 |  3689 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|        10 |  3690 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3691 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3692 | `					SyBlobAppend(pWorker,"\\",1);` |
|         3 |  3693 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|         2 |  3694 | `				}else{` |
|       ! 0 |  3695 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|         - |  3696 | `				}` |
|         - |  3697 | `			}` |
|      3913 |  3698 | `			SyBlobRelease(&sRaw);` |
|         - |  3699 | `		}` |
|      3913 |  3700 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3701 | `			ph7_value *pObj;` |
|         - |  3702 | `			SyString sPath;` |
|         - |  3703 | `			sxu32 nIdx;` |
|      3913 |  3704 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3705 | `			/* Install in the literal table */` |
|      3913 |  3706 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3861 |  3707 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3861 |  3708 | `				if( pObj == 0 ){` |
|       ! 0 |  3709 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3710 | `					return SXERR_ABORT;` |
|         - |  3711 | `				}` |
|      3861 |  3712 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3861 |  3713 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1928 |  3714 | `			}` |
|         - |  3715 | `			/* Emit the load constant instruction.` |
|         - |  3716 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3717 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5867 |  3718 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1954 |  3719 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1954 |  3720 | `				nIdx,0,0);` |
|      3913 |  3721 | `			return SXRET_OK;` |
|         - |  3722 | `		}` |
|       ! 0 |  3723 | `	}` |
|         - |  3724 | `	/* Single-token literal: load directly */` |
|  11951735 |  3725 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11951735 |  3726 | `	return rc;` |
|   5977824 |  3727 | `}` |
|         - |  3728 | `/*` |
|         - |  3729 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3730 | ` */` |
|         - |  3731 | `/*` |
|         - |  3732 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3733 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3734 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3735 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3736 | ` */` |
|       ! 0 |  3737 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3738 | `{` |
|       ! 0 |  3739 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3740 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3741 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3742 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3743 | `}` |
|  11955638 |  3744 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3745 | `{` |
|         - |  3746 | `	sxi32 rc;` |
|  11955643 |  3747 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11955643 |  3748 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3749 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3750 | `		return rc;` |
|         - |  3751 | `	}` |
|         - |  3752 | `	/* Node successfully compiled */` |
|  11955643 |  3753 | `	return SXRET_OK;` |
|   5977824 |  3754 | `}` |
|         - |  3755 | `/*` |
|         - |  3756 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3757 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3758 | ` */` |
|         8 |  3759 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3760 | `{` |
|         - |  3761 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3762 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3763 | `		pGen->pIn++;` |
|         1 |  3764 | `	}` |
|         9 |  3765 | `	return SXRET_OK;` |
|         1 |  3766 | `}` |
|         - |  3767 | `/*` |
|         - |  3768 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3769 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3770 | ` */` |
|    290856 |  3771 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3772 | `{` |
|    290861 |  3773 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3873 |  3774 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3775 | `			return TRUE;` |
|      3871 |  3776 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3777 | `			return TRUE;` |
|         5 |  3778 | `		}` |
|    288924 |  3779 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7673 |  3780 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3781 | `			return TRUE;` |
|         - |  3782 | `		}` |
|      3833 |  3783 | `	}` |
|         - |  3784 | `	/* Not a reserved constant */` |
|    290853 |  3785 | `	return FALSE;` |
|    145433 |  3786 | `}` |
|         - |  3787 | `/*` |
|         - |  3788 | ` * Compile the 'const' statement.` |
|         - |  3789 | ` * According to the PHP language reference` |
|         - |  3790 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3791 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3792 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3793 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3794 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3795 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3796 | ` *  Syntax` |
|         - |  3797 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3798 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3799 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3800 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3801 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3802 | ` *  to get a list of all defined constants.` |
|         - |  3803 | ` *` |
|         - |  3804 | ` * Symisc eXtension.` |
|         - |  3805 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3806 | ` *  would allow only simple scalar value.` |
|         - |  3807 | ` *  Example` |
|         - |  3808 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3809 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3810 | ` */` |
|        50 |  3811 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3812 | `{` |
|         - |  3813 | `	SySet *pConsCode,*pInstrContainer;` |
|        55 |  3814 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3815 | `	SyString *pName;` |
|         - |  3816 | `	sxi32 rc;` |
|        55 |  3817 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        55 |  3818 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3819 | `		/* Invalid constant name */` |
|         9 |  3820 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|         9 |  3821 | `		if( rc == SXERR_ABORT ){` |
|         - |  3822 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3823 | `			return SXERR_ABORT;` |
|         - |  3824 | `		}` |
|         9 |  3825 | `		goto Synchronize;` |
|         - |  3826 | `	}` |
|         - |  3827 | `	/* Peek constant name */` |
|        49 |  3828 | `	pName = &pGen->pIn->sData;` |
|         - |  3829 | `	/* Make sure the constant name isn't reserved */` |
|        49 |  3830 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3831 | `		/* Reserved constant */` |
|        10 |  3832 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        10 |  3833 | `		if( rc == SXERR_ABORT ){` |
|         - |  3834 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3835 | `			return SXERR_ABORT;` |
|         - |  3836 | `		}` |
|        10 |  3837 | `		goto Synchronize;` |
|         - |  3838 | `	}` |
|        40 |  3839 | `	pGen->pIn++;` |
|        40 |  3840 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3841 | `		/* Invalid statement*/` |
|         6 |  3842 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|         6 |  3843 | `		if( rc == SXERR_ABORT ){` |
|         - |  3844 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3845 | `			return SXERR_ABORT;` |
|         - |  3846 | `		}` |
|         6 |  3847 | `		goto Synchronize;` |
|         - |  3848 | `	}` |
|        34 |  3849 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3850 | `	/* Allocate a new constant value container */` |
|        34 |  3851 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        34 |  3852 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3853 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3854 | `		return SXERR_ABORT;` |
|         - |  3855 | `	}` |
|        34 |  3856 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3857 | `	/* Swap bytecode container */` |
|        34 |  3858 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        34 |  3859 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3860 | `	/* Compile constant value */` |
|        34 |  3861 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3862 | `	/* Emit the done instruction */` |
|        34 |  3863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        34 |  3864 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        34 |  3865 | `	if( rc == SXERR_ABORT ){` |
|         - |  3866 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3867 | `		return SXERR_ABORT;` |
|         - |  3868 | `	}` |
|        34 |  3869 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3870 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3871 | `	{` |
|         - |  3872 | `		SyBlob sFQN;` |
|         - |  3873 | `		SyString sFQNStr;` |
|        34 |  3874 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        34 |  3875 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        34 |  3876 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        50 |  3877 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        32 |  3878 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        34 |  3879 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3880 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3881 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3882 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3883 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3884 | `			if( pCEntry ){` |
|         5 |  3885 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3886 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3887 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3888 | `					return SXERR_ABORT;` |
|         - |  3889 | `				}` |
|         2 |  3890 | `			}` |
|         2 |  3891 | `		}` |
|        34 |  3892 | `		SyBlobRelease(&sFQN);` |
|         - |  3893 | `	}` |
|        34 |  3894 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3895 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3896 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3897 | `	}` |
|        34 |  3898 | `	return SXRET_OK;` |
|         9 |  3899 | `Synchronize:` |
|         - |  3900 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3901 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        42 |  3902 | `		pGen->pIn++;` |
|         4 |  3903 | `	}` |
|        22 |  3904 | `	return SXRET_OK;` |
|        30 |  3905 | `}` |
|         - |  3906 | `/*` |
|         - |  3907 | ` * Compile the 'continue' statement.` |
|         - |  3908 | ` * According to the PHP language reference` |
|         - |  3909 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3910 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3911 | ` *  iteration.` |
|         - |  3912 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3913 | ` *  the purposes of continue.` |
|         - |  3914 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3915 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3916 | ` *  Note:` |
|         - |  3917 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3918 | ` */` |
|         - |  3919 | `/*` |
|         - |  3920 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3921 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3922 | ` * break/continue crosses a try boundary.` |
|         - |  3923 | ` *` |
|         - |  3924 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3925 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3926 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3927 | ` */` |
|    149290 |  3928 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3929 | `{` |
|    149295 |  3930 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    149295 |  3931 | `	int nInlineTry = 0;` |
|    673429 |  3932 | `	while( pBlock && pBlock != pTarget ){` |
|    524139 |  3933 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3934 | `			if( pBlock->pUserData ){` |
|         - |  3935 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3936 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3937 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3938 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3939 | `				if( pGen->bInGenerator ){` |
|         3 |  3940 | `					nInlineTry++;` |
|         2 |  3941 | `				}else{` |
|         3 |  3942 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3943 | `				}` |
|         4 |  3944 | `			}else{` |
|         - |  3945 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3946 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3947 | `				break;` |
|         - |  3948 | `			}` |
|         2 |  3949 | `		}` |
|    524139 |  3950 | `		pBlock = pBlock->pParent;` |
|         5 |  3951 | `	}` |
|    149295 |  3952 | `	return nInlineTry;` |
|         5 |  3953 | `}` |
|     84178 |  3954 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3955 | `{` |
|         - |  3956 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3957 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3958 | `	sxu32 nLineLocal;` |
|         - |  3959 | `	sxi32 rc;` |
|     84183 |  3960 | `	nLineLocal = pGen->pIn->nLine;` |
|     84183 |  3961 | `	iLevel = 0;` |
|         - |  3962 | `	/* Jump the 'continue' keyword */` |
|     84183 |  3963 | `	pGen->pIn++;` |
|     84183 |  3964 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3965 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3966 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3967 | `		 */` |
|         - |  3968 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3969 | `		char *zAlloc = 0;` |
|         - |  3970 | `		SyString sNum;` |
|        17 |  3971 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3972 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3973 | `			return SXERR_ABORT;` |
|         - |  3974 | `		}` |
|        17 |  3975 | `		if( rc == SXRET_OK ){` |
|        20 |  3976 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3977 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3978 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3979 | `				return SXERR_ABORT;` |
|         - |  3980 | `			}` |
|        14 |  3981 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3982 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3983 | `		}` |
|        17 |  3984 | `		if( iLevel < 2 ){` |
|         3 |  3985 | `			iLevel = 0;` |
|         1 |  3986 | `		}` |
|        17 |  3987 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3988 | `	}` |
|         - |  3989 | `	/* Point to the target loop */` |
|     84183 |  3990 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     84183 |  3991 | `	if( pLoop == 0 ){` |
|         - |  3992 | `		/* Illegal continue */` |
|        13 |  3993 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        13 |  3994 | `		if( rc == SXERR_ABORT ){` |
|         - |  3995 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3996 | `			return SXERR_ABORT;` |
|         - |  3997 | `		}` |
|         8 |  3998 | `	}else{` |
|     84173 |  3999 | `		sxu32 nInstrIdx = 0;` |
|         - |  4000 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     84173 |  4001 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  4002 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  4003 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     84173 |  4004 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     84173 |  4005 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  4006 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|         - |  4007 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|         - |  4008 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|         - |  4009 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|         5 |  4010 | `			if( iLevel < 1 ){` |
|         5 |  4011 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|         - |  4012 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|         - |  4013 | `					" Did you mean to use \"continue 2\"?");` |
|         2 |  4014 | `			}` |
|         5 |  4015 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  4016 | `			if( rc == SXRET_OK ){` |
|         5 |  4017 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  4018 | `			}` |
|         3 |  4019 | `		}else{` |
|         - |  4020 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     84169 |  4021 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     84169 |  4022 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  4023 | `				JumpFixup sJumpFix;` |
|         - |  4024 | `				/* Post-continue */` |
|     26785 |  4025 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     26785 |  4026 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     26785 |  4027 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     13390 |  4028 | `			}` |
|         - |  4029 | `		}` |
|         - |  4030 | `	}` |
|     84183 |  4031 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4032 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4033 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  4034 | `	}` |
|         - |  4035 | `	/* Statement successfully compiled */` |
|     84183 |  4036 | `	return SXRET_OK;` |
|     42094 |  4037 | `}` |
|         - |  4038 | `/*` |
|         - |  4039 | ` * Compile the 'break' statement.` |
|         - |  4040 | ` * According to the PHP language reference` |
|         - |  4041 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  4042 | ` *  structure.` |
|         - |  4043 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  4044 | ` *  enclosing structures are to be broken out of.` |
|         - |  4045 | ` */` |
|     65138 |  4046 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  4047 | `{` |
|         - |  4048 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  4049 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  4050 | `	sxi32 rc;` |
|     65143 |  4051 | `	iLevel = 0;` |
|         - |  4052 | `	/* Jump the 'break' keyword */` |
|     65143 |  4053 | `	pGen->pIn++;` |
|     65143 |  4054 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  4055 | `		/* optional numeric argument which tells us how many levels` |
|         - |  4056 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  4057 | `		 */` |
|         - |  4058 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        18 |  4059 | `		char *zAlloc = 0;` |
|         - |  4060 | `		SyString sNum;` |
|        18 |  4061 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        18 |  4062 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4063 | `			return SXERR_ABORT;` |
|         - |  4064 | `		}` |
|        18 |  4065 | `		if( rc == SXRET_OK ){` |
|        21 |  4066 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  4067 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  4068 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  4069 | `				return SXERR_ABORT;` |
|         - |  4070 | `			}` |
|        15 |  4071 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  4072 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  4073 | `		}` |
|        18 |  4074 | `		if( iLevel < 2 ){` |
|         3 |  4075 | `			iLevel = 0;` |
|         1 |  4076 | `		}` |
|        18 |  4077 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  4078 | `	}` |
|         - |  4079 | `	/* Extract the target loop */` |
|     65143 |  4080 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     65143 |  4081 | `	if( pLoop == 0 ){` |
|         - |  4082 | `		/* Illegal break */` |
|        19 |  4083 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        19 |  4084 | `		if( rc == SXERR_ABORT ){` |
|         - |  4085 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4086 | `			return SXERR_ABORT;` |
|         - |  4087 | `		}` |
|        11 |  4088 | `	}else{` |
|         - |  4089 | `		sxu32 nInstrIdx;` |
|         - |  4090 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     65127 |  4091 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  4092 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     65127 |  4093 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     65127 |  4094 | `		if( rc == SXRET_OK ){` |
|         - |  4095 | `			/* Fix the jump later when the jump destination is resolved */` |
|     65127 |  4096 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     32561 |  4097 | `		}` |
|         - |  4098 | `	}` |
|     65143 |  4099 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4100 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4101 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  4102 | `	}` |
|         - |  4103 | `	/* Statement successfully compiled */` |
|     65143 |  4104 | `	return SXRET_OK;` |
|     32574 |  4105 | `}` |
|         - |  4106 | `/*` |
|         - |  4107 | ` * Compile or record a label.` |
|         - |  4108 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  4109 | ` * Example` |
|         - |  4110 | ` *  goto LABEL;` |
|         - |  4111 | ` *   echo 'Foo';` |
|         - |  4112 | ` *  LABEL:` |
|         - |  4113 | ` *   echo 'Bar';` |
|         - |  4114 | ` */` |
|       112 |  4115 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  4116 | `{` |
|         - |  4117 | `	GenBlock *pBlock;` |
|         - |  4118 | `	Label sLabel;` |
|         - |  4119 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|         - |  4120 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|         - |  4121 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|         - |  4122 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|         - |  4123 | `	{` |
|       117 |  4124 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4125 | `		char *zDup;` |
|         - |  4126 | `		/* Initialize label fields */` |
|       117 |  4127 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4128 | `		/* Duplicate label name */` |
|       117 |  4129 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       117 |  4130 | `		if( zDup == 0 ){` |
|       ! 0 |  4131 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4132 | `			return SXERR_ABORT;` |
|         - |  4133 | `		}` |
|       117 |  4134 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       117 |  4135 | `		sLabel.bRef  = FALSE;` |
|       117 |  4136 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       117 |  4137 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       117 |  4138 | `		pBlock = pGen->pCurrent;` |
|       233 |  4139 | `		while( pBlock ){` |
|       143 |  4140 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        26 |  4141 | `				break;` |
|         - |  4142 | `			}` |
|         - |  4143 | `			/* Point to the upper block */` |
|       121 |  4144 | `			pBlock = pBlock->pParent;` |
|         5 |  4145 | `		}` |
|       117 |  4146 | `		if( pBlock ){` |
|        26 |  4147 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        15 |  4148 | `		}else{` |
|        95 |  4149 | `			sLabel.pFunc = 0;` |
|         - |  4150 | `		}` |
|         - |  4151 | `		/* Insert in label set */` |
|       117 |  4152 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  4153 | `	}` |
|       117 |  4154 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  4155 | `	return SXRET_OK;` |
|        61 |  4156 | `}` |
|         - |  4157 | `/*` |
|         - |  4158 | ` * Compile the so hated 'goto' statement.` |
|         - |  4159 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  4160 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  4161 | ` * a compiler it has to do this.` |
|         - |  4162 | ` * According to the PHP language reference manual` |
|         - |  4163 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  4164 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  4165 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  4166 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  4167 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  4168 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  4169 | ` *   of a multi-level break` |
|         - |  4170 | ` */` |
|       152 |  4171 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  4172 | `{` |
|         - |  4173 | `	JumpFixup sJump;` |
|         - |  4174 | `	sxi32 rc;` |
|       157 |  4175 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  4176 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  4177 | `		/* Missing label */` |
|       ! 0 |  4178 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  4179 | `		if( rc == SXERR_ABORT ){` |
|         - |  4180 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4181 | `			return SXERR_ABORT;` |
|         - |  4182 | `		}` |
|       ! 0 |  4183 | `		return SXRET_OK;` |
|         - |  4184 | `	}` |
|       157 |  4185 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         6 |  4186 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|         6 |  4187 | `		if( rc == SXERR_ABORT ){` |
|         - |  4188 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4189 | `			return SXERR_ABORT;` |
|         - |  4190 | `		}` |
|         4 |  4191 | `	}else{` |
|       153 |  4192 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4193 | `		GenBlock *pBlock;` |
|         - |  4194 | `		char *zDup;` |
|         - |  4195 | `		/* Prepare the jump destination */` |
|       153 |  4196 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  4197 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  4198 | `		/* Duplicate label name */` |
|       153 |  4199 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  4200 | `		if( zDup == 0 ){` |
|       ! 0 |  4201 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4202 | `			return SXERR_ABORT;` |
|         - |  4203 | `		}` |
|       153 |  4204 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|         - |  4205 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|       153 |  4206 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|         - |  4207 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|         - |  4208 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|       153 |  4209 | `		pBlock = pGen->pCurrent;` |
|       327 |  4210 | `		while( pBlock ){` |
|       205 |  4211 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|        30 |  4212 | `				break;` |
|         - |  4213 | `			}` |
|         - |  4214 | `			/* Point to the upper block */` |
|       179 |  4215 | `			pBlock = pBlock->pParent;` |
|         5 |  4216 | `		}` |
|       153 |  4217 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        30 |  4218 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        17 |  4219 | `		}else{` |
|       127 |  4220 | `			sJump.pFunc = 0;` |
|         - |  4221 | `		}` |
|         - |  4222 | `		/* Emit the unconditional jump */` |
|       153 |  4223 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  4224 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  4225 | `		}` |
|         - |  4226 | `	}` |
|       157 |  4227 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  4228 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  4229 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  4230 | `	}` |
|         - |  4231 | `	/* Statement successfully compiled */` |
|       157 |  4232 | `	return SXRET_OK;` |
|        81 |  4233 | `}` |
|         - |  4234 | `/*` |
|         - |  4235 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  4236 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  4237 | ` * failure.` |
|         - |  4238 | ` */` |
|        20 |  4239 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         1 |  4240 | `{` |
|         - |  4241 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  4242 | `	sxu32 nRawObj;` |
|        10 |  4243 | `	sxu32 nObjIdx;` |
|         - |  4244 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  4245 | `	 * a PHP block.` |
|         - |  4246 | `	 */` |
|        10 |  4247 | `Consume:` |
|        21 |  4248 | `	nRawObj = nObjIdx = 0;` |
|        21 |  4249 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  4250 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  4251 | `		if( pRawObj == 0 ){` |
|       ! 0 |  4252 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4253 | `			return SXERR_ABORT;` |
|         - |  4254 | `		}` |
|         - |  4255 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  4256 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  4257 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  4258 | `		++nRawObj;` |
|       ! 0 |  4259 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  4260 | `	}` |
|        21 |  4261 | `	if( nRawObj > 0 ){` |
|         - |  4262 | `		/* Emit the consume instruction */` |
|       ! 0 |  4263 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4264 | `	}` |
|        21 |  4265 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  4266 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  4267 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  4268 | `		SySetReset(pTokenSet);` |
|       ! 0 |  4269 | `		SySetReset(&pGen->aTrivia);` |
|         - |  4270 | `		/* Tokenize input */` |
|       ! 0 |  4271 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  4272 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  4273 | `		/* Point to the fresh token stream */` |
|       ! 0 |  4274 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  4275 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  4276 | `		/* Advance the stream cursor */` |
|       ! 0 |  4277 | `		pGen->pRawIn++;` |
|         - |  4278 | `		/* TICKET 1433-011 */` |
|       ! 0 |  4279 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4280 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4281 | `			sxi32 rc;` |
|         - |  4282 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4283 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4284 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4285 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4286 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4287 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4288 | `				return SXERR_ABORT;` |
|       ! 0 |  4289 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4290 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4291 | `			}` |
|       ! 0 |  4292 | `			goto Consume;` |
|         - |  4293 | `		}` |
|       ! 0 |  4294 | `	}else{` |
|         - |  4295 | `		/* No more chunks to process */` |
|        21 |  4296 | `		pGen->pIn = pGen->pEnd;` |
|        21 |  4297 | `		return SXERR_EOF;` |
|         - |  4298 | `	}` |
|       ! 0 |  4299 | `	return SXRET_OK;` |
|        11 |  4300 | `}` |
|         - |  4301 | `/*` |
|         - |  4302 | ` * Compile a PHP block.` |
|         - |  4303 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4304 | ` * optionally delimited by braces {}.` |
|         - |  4305 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4306 | ` * and this function takes care of generating the appropriate error` |
|         - |  4307 | ` * message.` |
|         - |  4308 | ` */` |
|   6037918 |  4309 | `static sxi32 PH7_CompileBlock(` |
|         - |  4310 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4311 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4312 | `	)` |
|         5 |  4313 | `{` |
|         - |  4314 | `	sxi32 rc;` |
|         - |  4315 | `	sxu32 nLine;` |
|   6037923 |  4316 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   6013889 |  4317 | `		nLine = pGen->pIn->nLine;` |
|   6013889 |  4318 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   6013889 |  4319 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4320 | `			return SXERR_ABORT;` |
|         - |  4321 | `		}` |
|   6013889 |  4322 | `		pGen->pIn++;` |
|         - |  4323 | `		/* Compile until we hit the closing braces '}' */` |
|   8894640 |  4324 | `		for(;;){` |
|  17789285 |  4325 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        21 |  4326 | `				rc = GenStateNextChunk(&(*pGen));` |
|        21 |  4327 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4328 | `			 	   return SXERR_ABORT;` |
|         - |  4329 | `				}` |
|        21 |  4330 | `				if( rc == SXERR_EOF ){` |
|         - |  4331 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4332 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        21 |  4333 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        21 |  4334 | `					break;` |
|         - |  4335 | `				}` |
|       ! 0 |  4336 | `			}` |
|  17789265 |  4337 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4338 | `				/* Closing braces found,break immediately*/` |
|   6013869 |  4339 | `				pGen->pIn++;` |
|   6013869 |  4340 | `				break;` |
|         - |  4341 | `			}` |
|         - |  4342 | `			/* Compile a single statement */` |
|  11775401 |  4343 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  11775401 |  4344 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4345 | `				return SXERR_ABORT;` |
|         - |  4346 | `			}` |
|         5 |  4347 | `		}` |
|   6013889 |  4348 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   3030981 |  4349 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4350 | `		pGen->pIn++;` |
|       ! 0 |  4351 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4352 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4353 | `			return SXERR_ABORT;` |
|         - |  4354 | `		}` |
|         - |  4355 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4356 | `		for(;;){` |
|       ! 0 |  4357 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4358 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4359 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4360 | `			 	   return SXERR_ABORT;` |
|         - |  4361 | `				}` |
|       ! 0 |  4362 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4363 | `					/* No more token to process */` |
|       ! 0 |  4364 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4365 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4366 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4367 | `					}` |
|       ! 0 |  4368 | `					break;` |
|         - |  4369 | `				}` |
|       ! 0 |  4370 | `			}` |
|       ! 0 |  4371 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4372 | `				sxi32 nKwrd;` |
|         - |  4373 | `				/* Keyword found */` |
|       ! 0 |  4374 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4375 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4376 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4377 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4378 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4379 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4380 | `						}` |
|       ! 0 |  4381 | `						break;` |
|         - |  4382 | `				}` |
|       ! 0 |  4383 | `			}` |
|         - |  4384 | `			/* Compile a single statement */` |
|       ! 0 |  4385 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4386 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4387 | `				return SXERR_ABORT;` |
|         - |  4388 | `			}` |
|       ! 0 |  4389 | `		}` |
|       ! 0 |  4390 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4391 | `	}else{` |
|         - |  4392 | `		/* Compile a single statement */` |
|     24039 |  4393 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     24039 |  4394 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4395 | `			return SXERR_ABORT;` |
|         - |  4396 | `		}` |
|         - |  4397 | `	}` |
|         - |  4398 | `	/* Jump trailing semi-colons ';' */` |
|   6037923 |  4399 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4400 | `		pGen->pIn++;` |
|       ! 0 |  4401 | `	}` |
|   6037923 |  4402 | `	return SXRET_OK;` |
|   3018964 |  4403 | `}` |
|         - |  4404 | `/*` |
|         - |  4405 | ` * Compile the gentle 'while' statement.` |
|         - |  4406 | ` * According to the PHP language reference` |
|         - |  4407 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4408 | ` *  The basic form of a while statement is:` |
|         - |  4409 | ` *  while (expr)` |
|         - |  4410 | ` *   statement` |
|         - |  4411 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4412 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4413 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4414 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4415 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4416 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4417 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4418 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4419 | ` *  while (expr):` |
|         - |  4420 | ` *    statement` |
|         - |  4421 | ` *   endwhile;` |
|         - |  4422 | ` */` |
|     65152 |  4423 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4424 | `{` |
|     65157 |  4425 | `	GenBlock *pWhileBlock = 0;` |
|     65157 |  4426 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4427 | `	sxu32 nFalseJump;` |
|         - |  4428 | `	sxu32 nLine;` |
|         - |  4429 | `	sxi32 rc;` |
|     65157 |  4430 | `	nLine = pGen->pIn->nLine;` |
|         - |  4431 | `	/* Jump the 'while' keyword */` |
|     65157 |  4432 | `	pGen->pIn++;` |
|     65157 |  4433 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4434 | `		/* Syntax error */` |
|       ! 0 |  4435 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4436 | `		if( rc == SXERR_ABORT ){` |
|         - |  4437 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4438 | `			return SXERR_ABORT;` |
|         - |  4439 | `		}` |
|       ! 0 |  4440 | `		goto Synchronize;` |
|         - |  4441 | `	}` |
|         - |  4442 | `	/* Jump the left parenthesis '(' */` |
|     65157 |  4443 | `	pGen->pIn++;` |
|         - |  4444 | `	/* Create the loop block */` |
|     65157 |  4445 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     65157 |  4446 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4447 | `		return SXERR_ABORT;` |
|         - |  4448 | `	}` |
|         - |  4449 | `	/* Delimit the condition */` |
|     65157 |  4450 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     65157 |  4451 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4452 | `		/* Empty expression */` |
|         3 |  4453 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4454 | `		if( rc == SXERR_ABORT ){` |
|         - |  4455 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4456 | `			return SXERR_ABORT;` |
|         - |  4457 | `		}` |
|         1 |  4458 | `	}` |
|         - |  4459 | `	/* Swap token streams */` |
|     65157 |  4460 | `	pTmp = pGen->pEnd;` |
|     65157 |  4461 | `	pGen->pEnd = pEnd;` |
|         - |  4462 | `	/* Compile the expression */` |
|     65157 |  4463 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     65157 |  4464 | `	if( rc == SXERR_ABORT ){` |
|         - |  4465 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4466 | `		return SXERR_ABORT;` |
|         - |  4467 | `	}` |
|         - |  4468 | `	/* Update token stream */` |
|     65157 |  4469 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4470 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4471 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4472 | `			return SXERR_ABORT;` |
|         - |  4473 | `		}` |
|       ! 0 |  4474 | `		pGen->pIn++;` |
|       ! 0 |  4475 | `	}` |
|         - |  4476 | `	/* Synchronize pointers */` |
|     65157 |  4477 | `	pGen->pIn  = &pEnd[1];` |
|     65157 |  4478 | `	pGen->pEnd = pTmp;` |
|         - |  4479 | `	/* Emit the false jump */` |
|     65157 |  4480 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4481 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     65157 |  4482 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4483 | `	/* Compile the loop body */` |
|     65157 |  4484 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     65157 |  4485 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4486 | `		return SXERR_ABORT;` |
|         - |  4487 | `	}` |
|         - |  4488 | `	/* Emit the unconditional jump to the start of the loop */` |
|     65157 |  4489 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4490 | `	/* Fix all jumps now the destination is resolved */` |
|     65157 |  4491 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4492 | `	/* Release the loop block */` |
|     65157 |  4493 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4494 | `	/* Statement successfully compiled */` |
|     65157 |  4495 | `	return SXRET_OK;` |
|       ! 0 |  4496 | `Synchronize:` |
|         - |  4497 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4498 | `	 * compiling this erroneous block.` |
|         - |  4499 | `	 */` |
|       ! 0 |  4500 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4501 | `		pGen->pIn++;` |
|       ! 0 |  4502 | `	}` |
|       ! 0 |  4503 | `	return SXRET_OK;` |
|     32581 |  4504 | `}` |
|         - |  4505 | `/*` |
|         - |  4506 | ` * Compile the ugly do..while() statement.` |
|         - |  4507 | ` * According to the PHP language reference` |
|         - |  4508 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4509 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4510 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4511 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4512 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4513 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4514 | ` *  would end immediately).` |
|         - |  4515 | ` *  There is just one syntax for do-while loops:` |
|         - |  4516 | ` *  <?php` |
|         - |  4517 | ` *  $i = 0;` |
|         - |  4518 | ` *  do {` |
|         - |  4519 | ` *   echo $i;` |
|         - |  4520 | ` *  } while ($i > 0);` |
|         - |  4521 | ` * ?>` |
|         - |  4522 | ` */` |
|         2 |  4523 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4524 | `{` |
|         3 |  4525 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4526 | `	GenBlock *pDoBlock = 0;` |
|         - |  4527 | `	sxu32 nLine;` |
|         - |  4528 | `	sxi32 rc;` |
|         3 |  4529 | `	nLine = pGen->pIn->nLine;` |
|         - |  4530 | `	/* Jump the 'do' keyword */` |
|         3 |  4531 | `	pGen->pIn++;` |
|         - |  4532 | `	/* Create the loop block */` |
|         3 |  4533 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4534 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4535 | `		return SXERR_ABORT;` |
|         - |  4536 | `	}` |
|         - |  4537 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4538 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4539 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4540 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4541 | `		return SXERR_ABORT;` |
|         - |  4542 | `	}` |
|         3 |  4543 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4544 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4545 | `	}` |
|         3 |  4546 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4547 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4548 | `			/* Missing 'while' statement */` |
|         3 |  4549 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4550 | `			if( rc == SXERR_ABORT ){` |
|         - |  4551 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4552 | `				return SXERR_ABORT;` |
|         - |  4553 | `			}` |
|         3 |  4554 | `			goto Synchronize;` |
|         - |  4555 | `	}` |
|         - |  4556 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4557 | `	pGen->pIn++;` |
|       ! 0 |  4558 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4559 | `		/* Syntax error */` |
|       ! 0 |  4560 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4561 | `		if( rc == SXERR_ABORT ){` |
|         - |  4562 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4563 | `			return SXERR_ABORT;` |
|         - |  4564 | `		}` |
|       ! 0 |  4565 | `		goto Synchronize;` |
|         - |  4566 | `	}` |
|         - |  4567 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4568 | `	pGen->pIn++;` |
|         - |  4569 | `	/* Delimit the condition */` |
|       ! 0 |  4570 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4571 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4572 | `		/* Empty expression */` |
|       ! 0 |  4573 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4574 | `		if( rc == SXERR_ABORT ){` |
|         - |  4575 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4576 | `			return SXERR_ABORT;` |
|         - |  4577 | `		}` |
|       ! 0 |  4578 | `		goto Synchronize;` |
|         - |  4579 | `	}` |
|         - |  4580 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4581 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4582 | `		JumpFixup *aPost;` |
|         - |  4583 | `		VmInstr *pInstr;` |
|         - |  4584 | `		sxu32 nJumpDest;` |
|         - |  4585 | `		sxu32 n;` |
|       ! 0 |  4586 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4587 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4588 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4589 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4590 | `			if( pInstr ){` |
|         - |  4591 | `				/* Fix */` |
|       ! 0 |  4592 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4593 | `			}` |
|       ! 0 |  4594 | `		}` |
|       ! 0 |  4595 | `	}` |
|         - |  4596 | `	/* Swap token streams */` |
|       ! 0 |  4597 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4598 | `	pGen->pEnd = pEnd;` |
|         - |  4599 | `	/* Compile the expression */` |
|       ! 0 |  4600 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4601 | `	if( rc == SXERR_ABORT ){` |
|         - |  4602 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4603 | `		return SXERR_ABORT;` |
|         - |  4604 | `	}` |
|         - |  4605 | `	/* Update token stream */` |
|       ! 0 |  4606 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4607 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4608 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4609 | `			return SXERR_ABORT;` |
|         - |  4610 | `		}` |
|       ! 0 |  4611 | `		pGen->pIn++;` |
|       ! 0 |  4612 | `	}` |
|       ! 0 |  4613 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4614 | `	pGen->pEnd = pTmp;` |
|         - |  4615 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4616 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4617 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4618 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4619 | `	/* Release the loop block */` |
|       ! 0 |  4620 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4621 | `	/* Statement successfully compiled */` |
|       ! 0 |  4622 | `	return SXRET_OK;` |
|         1 |  4623 | `Synchronize:` |
|         - |  4624 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4625 | `	 * compiling this erroneous block.` |
|         - |  4626 | `	 */` |
|         3 |  4627 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4628 | `		pGen->pIn++;` |
|       ! 0 |  4629 | `	}` |
|         3 |  4630 | `	return SXRET_OK;` |
|         2 |  4631 | `}` |
|         - |  4632 | `/*` |
|         - |  4633 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4634 | ` * According to the PHP language reference` |
|         - |  4635 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4636 | ` *  The syntax of a for loop is:` |
|         - |  4637 | ` *  for (expr1; expr2; expr3)` |
|         - |  4638 | ` *   statement` |
|         - |  4639 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4640 | ` *  the beginning of the loop.` |
|         - |  4641 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4642 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4643 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4644 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4645 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4646 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4647 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4648 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4649 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4650 | ` *  of using the for truth expression.` |
|         - |  4651 | ` */` |
|    122516 |  4652 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4653 | `{` |
|    122521 |  4654 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    122521 |  4655 | `	GenBlock *pForBlock = 0;` |
|         - |  4656 | `	sxu32 nFalseJump;` |
|         - |  4657 | `	sxu32 nLine;` |
|         - |  4658 | `	sxi32 rc;` |
|    122521 |  4659 | `	nLine = pGen->pIn->nLine;` |
|         - |  4660 | `	/* Jump the 'for' keyword */` |
|    122521 |  4661 | `	pGen->pIn++;` |
|    122521 |  4662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4663 | `		/* Syntax error */` |
|       ! 0 |  4664 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4665 | `		if( rc == SXERR_ABORT ){` |
|         - |  4666 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4667 | `			return SXERR_ABORT;` |
|         - |  4668 | `		}` |
|       ! 0 |  4669 | `		return SXRET_OK;` |
|         - |  4670 | `	}` |
|         - |  4671 | `	/* Jump the left parenthesis '(' */` |
|    122521 |  4672 | `	pGen->pIn++;` |
|         - |  4673 | `	/* Delimit the init-expr;condition;post-expr */` |
|    122521 |  4674 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    122521 |  4675 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4676 | `		/* Empty expression */` |
|       ! 0 |  4677 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4678 | `		if( rc == SXERR_ABORT ){` |
|         - |  4679 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4680 | `			return SXERR_ABORT;` |
|         - |  4681 | `		}` |
|         - |  4682 | `		/* Synchronize */` |
|       ! 0 |  4683 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4684 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4685 | `			pGen->pIn++;` |
|       ! 0 |  4686 | `		}` |
|       ! 0 |  4687 | `		return SXRET_OK;` |
|         - |  4688 | `	}` |
|         - |  4689 | `	/* Swap token streams */` |
|    122521 |  4690 | `	pTmp = pGen->pEnd;` |
|    122521 |  4691 | `	pGen->pEnd = pEnd;` |
|         - |  4692 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4693 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4694 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4695 | `	 * compiled through this same window — recorded as a known leniency. */` |
|    122521 |  4696 | `	pGen->nCommaExprOk++;` |
|         - |  4697 | `	/* Compile initialization expressions if available */` |
|    122521 |  4698 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4699 | `	/* Pop operand lvalues */` |
|    122521 |  4700 | `	if( rc == SXERR_ABORT ){` |
|         - |  4701 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4702 | `		return SXERR_ABORT;` |
|    122521 |  4703 | `	}else if( rc != SXERR_EMPTY ){` |
|    111047 |  4704 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55521 |  4705 | `	}` |
|    122521 |  4706 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4707 | `		/* Syntax error */` |
|       ! 0 |  4708 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4709 | `		if( rc == SXERR_ABORT ){` |
|         - |  4710 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4711 | `			return SXERR_ABORT;` |
|         - |  4712 | `		}` |
|       ! 0 |  4713 | `		return SXRET_OK;` |
|         - |  4714 | `	}` |
|         - |  4715 | `	/* Jump the trailing ';' */` |
|    122521 |  4716 | `	pGen->pIn++;` |
|         - |  4717 | `	/* Create the loop block */` |
|    122521 |  4718 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    122521 |  4719 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4720 | `		return SXERR_ABORT;` |
|         - |  4721 | `	}` |
|         - |  4722 | `	/* Deffer continue jumps */` |
|    122521 |  4723 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4724 | `	/* Compile the condition */` |
|    122521 |  4725 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    122521 |  4726 | `	if( rc == SXERR_ABORT ){` |
|         - |  4727 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4728 | `		return SXERR_ABORT;` |
|    122521 |  4729 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4730 | `		/* Emit the false jump */` |
|    111047 |  4731 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4732 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    111047 |  4733 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     55521 |  4734 | `	}` |
|    122521 |  4735 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4736 | `		/* Syntax error */` |
|         6 |  4737 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4738 | `		if( rc == SXERR_ABORT ){` |
|         - |  4739 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4740 | `			return SXERR_ABORT;` |
|         - |  4741 | `		}` |
|         6 |  4742 | `		return SXRET_OK;` |
|         - |  4743 | `	}` |
|         - |  4744 | `	/* Jump the trailing ';' */` |
|    122517 |  4745 | `	pGen->pIn++;` |
|         - |  4746 | `	/* Save the post condition stream */` |
|    122517 |  4747 | `	pPostStart = pGen->pIn;` |
|         - |  4748 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4749 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|    122517 |  4750 | `	pGen->nCommaExprOk--;` |
|    122517 |  4751 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    122517 |  4752 | `	pGen->pEnd = pTmp;` |
|    122517 |  4753 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    122517 |  4754 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4755 | `		return SXERR_ABORT;` |
|         - |  4756 | `	}` |
|         - |  4757 | `	/* Fix post-continue jumps */` |
|    122517 |  4758 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4759 | `		JumpFixup *aPost;` |
|         - |  4760 | `		VmInstr *pInstr;` |
|         - |  4761 | `		sxu32 nJumpDest;` |
|         - |  4762 | `		sxu32 n;` |
|     11489 |  4763 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|     11489 |  4764 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     38269 |  4765 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     26785 |  4766 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     26785 |  4767 | `			if( pInstr ){` |
|         - |  4768 | `				/* Fix jump */` |
|     26785 |  4769 | `				pInstr->iP2 = nJumpDest;` |
|     13390 |  4770 | `			}` |
|     13395 |  4771 | `		}` |
|      5742 |  4772 | `	}` |
|         - |  4773 | `	/* compile the post-expressions if available */` |
|    122517 |  4774 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4775 | `		pPostStart++;` |
|       ! 0 |  4776 | `	}` |
|    122517 |  4777 | `	if( pPostStart < pEnd ){` |
|         - |  4778 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    111045 |  4779 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    111045 |  4780 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|    111045 |  4781 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    111045 |  4782 | `		pGen->nCommaExprOk--;` |
|    111045 |  4783 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4784 | `			/* Syntax error */` |
|       ! 0 |  4785 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4786 | `			if( rc == SXERR_ABORT ){` |
|         - |  4787 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4788 | `				return SXERR_ABORT;` |
|         - |  4789 | `			}` |
|       ! 0 |  4790 | `			return SXRET_OK;` |
|         - |  4791 | `		}` |
|    111045 |  4792 | `		RE_SWAP_DELIMITER(pGen);` |
|    111045 |  4793 | `		if( rc == SXERR_ABORT ){` |
|         - |  4794 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4795 | `			return SXERR_ABORT;` |
|    111045 |  4796 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4797 | `			/* Pop operand lvalue */` |
|    111045 |  4798 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55520 |  4799 | `		}` |
|     55520 |  4800 | `	}` |
|         - |  4801 | `	/* Emit the unconditional jump to the start of the loop */` |
|    122517 |  4802 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4803 | `	/* Fix all jumps now the destination is resolved */` |
|    122517 |  4804 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4805 | `	/* Release the loop block */` |
|    122517 |  4806 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4807 | `	/* Statement successfully compiled */` |
|    122517 |  4808 | `	return SXRET_OK;` |
|     61263 |  4809 | `}` |
|         - |  4810 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4811 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4812 | ` * are allowed.` |
|         - |  4813 | ` */` |
|    444634 |  4814 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4815 | `{` |
|    444639 |  4816 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    444639 |  4817 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4818 | `		/* Unexpected expression */` |
|       ! 0 |  4819 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4820 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4821 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4822 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4823 | `		}` |
|       ! 0 |  4824 | `	}` |
|    444639 |  4825 | `	return rc;` |
|         5 |  4826 | `}` |
|         - |  4827 | `/*` |
|         - |  4828 | ` * Compile the 'foreach' statement.` |
|         - |  4829 | ` * According to the PHP language reference` |
|         - |  4830 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4831 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4832 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4833 | ` *  is a minor but useful extension of the first:` |
|         - |  4834 | ` *  foreach (array_expression as $value)` |
|         - |  4835 | ` *    statement` |
|         - |  4836 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4837 | ` *   statement` |
|         - |  4838 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4839 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4840 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4841 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4842 | ` *  to the variable $key on each loop.` |
|         - |  4843 | ` *  Note:` |
|         - |  4844 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4845 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4846 | ` *  Note:` |
|         - |  4847 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4848 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4849 | ` *  or after the foreach without resetting it.` |
|         - |  4850 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4851 | ` *  of copying the value.` |
|         - |  4852 | ` */` |
|    310514 |  4853 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4854 | `{` |
|    310519 |  4855 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    310519 |  4856 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    310519 |  4857 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4858 | `	ph7_foreach_info *pInfo;` |
|         - |  4859 | `	sxu32 nFalseJump;` |
|         - |  4860 | `	VmInstr *pInstr;` |
|         - |  4861 | `	sxu32 nLine;` |
|         - |  4862 | `	sxi32 rc;` |
|    310519 |  4863 | `	nLine = pGen->pIn->nLine;` |
|         - |  4864 | `	/* Jump the 'foreach' keyword */` |
|    310519 |  4865 | `	pGen->pIn++;` |
|    310519 |  4866 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4867 | `		/* Syntax error */` |
|       ! 0 |  4868 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4869 | `		if( rc == SXERR_ABORT ){` |
|         - |  4870 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4871 | `			return SXERR_ABORT;` |
|         - |  4872 | `		}` |
|       ! 0 |  4873 | `		goto Synchronize;` |
|         - |  4874 | `	}` |
|         - |  4875 | `	/* Jump the left parenthesis '(' */` |
|    310519 |  4876 | `	pGen->pIn++;` |
|         - |  4877 | `	/* Create the loop block */` |
|    310519 |  4878 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    310519 |  4879 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4880 | `		return SXERR_ABORT;` |
|         - |  4881 | `	}` |
|         - |  4882 | `	/* Delimit the expression */` |
|    310519 |  4883 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    310519 |  4884 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4885 | `		/* Empty expression */` |
|       ! 0 |  4886 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4887 | `		if( rc == SXERR_ABORT ){` |
|         - |  4888 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4889 | `			return SXERR_ABORT;` |
|         - |  4890 | `		}` |
|         - |  4891 | `		/* Synchronize */` |
|       ! 0 |  4892 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4893 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4894 | `			pGen->pIn++;` |
|       ! 0 |  4895 | `		}` |
|       ! 0 |  4896 | `		return SXRET_OK;` |
|         - |  4897 | `	}` |
|         - |  4898 | `	/* Compile the array expression */` |
|    310519 |  4899 | `	pCur = pGen->pIn;` |
|   1786457 |  4900 | `	while( pCur < pEnd ){` |
|   1786457 |  4901 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    341125 |  4902 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    341125 |  4903 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4904 | `				/* Break with the first 'as' found */` |
|    310519 |  4905 | `				break;` |
|         - |  4906 | `			}` |
|     15303 |  4907 | `		}` |
|         - |  4908 | `		/* Advance the stream cursor */` |
|   1475943 |  4909 | `		pCur++;` |
|         5 |  4910 | `	}` |
|    310519 |  4911 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4912 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4913 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4914 | `		if( rc == SXERR_ABORT ){` |
|         - |  4915 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4916 | `			return SXERR_ABORT;` |
|         - |  4917 | `		}` |
|       ! 0 |  4918 | `		goto Synchronize;` |
|         - |  4919 | `	}` |
|         - |  4920 | `	/* Swap token streams */` |
|    310519 |  4921 | `	pTmp = pGen->pEnd;` |
|    310519 |  4922 | `	pGen->pEnd = pCur;` |
|    310519 |  4923 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    310519 |  4924 | `	if( rc == SXERR_ABORT ){` |
|         - |  4925 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4926 | `		return SXERR_ABORT;` |
|         - |  4927 | `	}` |
|         - |  4928 | `	/* Update token stream */` |
|    310519 |  4929 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4930 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4931 | `		if( rc == SXERR_ABORT ){` |
|         - |  4932 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4933 | `			return SXERR_ABORT;` |
|         - |  4934 | `		}` |
|       ! 0 |  4935 | `		pGen->pIn++;` |
|       ! 0 |  4936 | `	}` |
|    310519 |  4937 | `	pCur++; /* Jump the 'as' keyword */` |
|    310519 |  4938 | `	pGen->pIn = pCur;` |
|    310519 |  4939 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4940 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4941 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4942 | `			return SXERR_ABORT;` |
|         - |  4943 | `		}` |
|       ! 0 |  4944 | `	}` |
|         - |  4945 | `	/* Create the foreach context */` |
|    310519 |  4946 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    310519 |  4947 | `	if( pInfo == 0 ){` |
|       ! 0 |  4948 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4949 | `		return SXERR_ABORT;` |
|         - |  4950 | `	}` |
|         - |  4951 | `	/* Zero the structure */` |
|    310519 |  4952 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4953 | `	/* Initialize structure fields */` |
|    310519 |  4954 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4955 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4956 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4957 | `	 * '=>'. */` |
|    310519 |  4958 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    310519 |  4959 | `	if( pCur < pEnd ){` |
|         - |  4960 | `		/* Compile the expression holding the key name */` |
|    134147 |  4961 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4962 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4963 | `			if( rc == SXERR_ABORT ){` |
|         - |  4964 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4965 | `				return SXERR_ABORT;` |
|         - |  4966 | `			}` |
|       ! 0 |  4967 | `		}else{` |
|    134147 |  4968 | `			pGen->pEnd = pCur;` |
|    134147 |  4969 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    134147 |  4970 | `			if( rc == SXERR_ABORT ){` |
|         - |  4971 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4972 | `				return SXERR_ABORT;` |
|         - |  4973 | `			}` |
|    134147 |  4974 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    134147 |  4975 | `			if( pInstr->p3 ){` |
|         - |  4976 | `				/* Record key name */` |
|    134147 |  4977 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     67071 |  4978 | `			}` |
|    134147 |  4979 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4980 | `		}` |
|    134147 |  4981 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     67071 |  4982 | `	}` |
|    310519 |  4983 | `	pGen->pEnd = pEnd;` |
|    310519 |  4984 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4985 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4986 | `		if( rc == SXERR_ABORT ){` |
|         - |  4987 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4988 | `			return SXERR_ABORT;` |
|         - |  4989 | `		}` |
|       ! 0 |  4990 | `		goto Synchronize;` |
|         - |  4991 | `	}` |
|    310519 |  4992 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4993 | `		pGen->pIn++;` |
|         - |  4994 | `		/* Pass by reference  */` |
|        33 |  4995 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4996 | `	}` |
|         - |  4997 | `	/* Check if the value target is list() */` |
|    310519 |  4998 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4999 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  5000 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  5001 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  5002 | `		 */` |
|         - |  5003 | `		static int iForeachListCnt = 0;` |
|         - |  5004 | `		char zTmp[128];` |
|         - |  5005 | `		sxu32 nLen;` |
|         - |  5006 | `		char *zDup;` |
|        10 |  5007 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  5008 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  5009 | `		if( zDup == 0 ){` |
|       ! 0 |  5010 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5011 | `			return SXERR_ABORT;` |
|         - |  5012 | `		}` |
|        10 |  5013 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  5014 | `		/* Save list() token boundaries */` |
|        10 |  5015 | `		pListStart = pGen->pIn;` |
|         - |  5016 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  5017 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  5018 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  5019 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  5020 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  5021 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5022 | `				return SXERR_ABORT;` |
|         - |  5023 | `			}` |
|         3 |  5024 | `			goto Synchronize;` |
|         - |  5025 | `		}` |
|         7 |  5026 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  5027 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  5028 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  5029 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5030 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  5031 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5032 | `				return SXERR_ABORT;` |
|         - |  5033 | `			}` |
|       ! 0 |  5034 | `			goto Synchronize;` |
|         - |  5035 | `		}` |
|         7 |  5036 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  5037 | `		pListEnd = pGen->pIn;` |
|         7 |  5038 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    310514 |  5039 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  5040 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  5041 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  5042 | `		 */` |
|         - |  5043 | `		static int iForeachShortListCnt = 0;` |
|         - |  5044 | `		char zTmp[128];` |
|         - |  5045 | `		sxu32 nLen;` |
|         - |  5046 | `		char *zDup;` |
|        15 |  5047 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        15 |  5048 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        15 |  5049 | `		if( zDup == 0 ){` |
|       ! 0 |  5050 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5051 | `			return SXERR_ABORT;` |
|         - |  5052 | `		}` |
|        15 |  5053 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  5054 | `		/* Save [...] token boundaries */` |
|        15 |  5055 | `		pListStart = pGen->pIn;` |
|         - |  5056 | `		/* Advance past [...] */` |
|        15 |  5057 | `		pGen->pIn++; /* Jump '[' */` |
|        15 |  5058 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        15 |  5059 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  5060 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5061 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  5062 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5063 | `				return SXERR_ABORT;` |
|         - |  5064 | `			}` |
|       ! 0 |  5065 | `			goto Synchronize;` |
|         - |  5066 | `		}` |
|        15 |  5067 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        15 |  5068 | `		pListEnd = pGen->pIn;` |
|        15 |  5069 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         8 |  5070 | `	}else{` |
|         - |  5071 | `		/* Compile the expression holding the value name */` |
|    310497 |  5072 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    310497 |  5073 | `		if( rc == SXERR_ABORT ){` |
|         - |  5074 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5075 | `			return SXERR_ABORT;` |
|         - |  5076 | `		}` |
|    310497 |  5077 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    310497 |  5078 | `		if( pInstr->p3 ){` |
|         - |  5079 | `			/* Record value name */` |
|    310497 |  5080 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    155246 |  5081 | `		}` |
|         - |  5082 | `	}` |
|         - |  5083 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    310517 |  5084 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  5085 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    310517 |  5086 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  5087 | `	/* Record the first instruction to execute */` |
|    310517 |  5088 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5089 | `	/* Emit the FOREACH_STEP instruction */` |
|    310517 |  5090 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  5091 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    310517 |  5092 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  5093 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    310517 |  5094 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  5095 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  5096 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  5097 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  5098 | `		 */` |
|        21 |  5099 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  5100 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  5101 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  5102 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  5103 | `		 */` |
|        21 |  5104 | `		pSavedIn = pGen->pIn;` |
|        21 |  5105 | `		pSavedEnd = pGen->pEnd;` |
|        21 |  5106 | `		pGen->pIn = pListStart;` |
|        21 |  5107 | `		pGen->pEnd = pListEnd;` |
|        21 |  5108 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        15 |  5109 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         8 |  5110 | `		}else{` |
|         7 |  5111 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  5112 | `		}` |
|        21 |  5113 | `		pGen->pIn = pSavedIn;` |
|        21 |  5114 | `		pGen->pEnd = pSavedEnd;` |
|        21 |  5115 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5116 | `			return SXERR_ABORT;` |
|         - |  5117 | `		}` |
|         - |  5118 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        21 |  5119 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        10 |  5120 | `	}` |
|         - |  5121 | `	/* Compile the loop body */` |
|    310517 |  5122 | `	pGen->pIn = &pEnd[1];` |
|    310517 |  5123 | `	pGen->pEnd = pTmp;` |
|    310517 |  5124 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    310517 |  5125 | `	if( rc == SXERR_ABORT ){` |
|         - |  5126 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5127 | `		return SXERR_ABORT;` |
|         - |  5128 | `	}` |
|         - |  5129 | `	/* Emit the unconditional jump to the start of the loop */` |
|    310517 |  5130 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  5131 | `	/* Fix all jumps now the destination is resolved */` |
|    310517 |  5132 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  5133 | `	/* Release the loop block */` |
|    310517 |  5134 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5135 | `	/* Statement successfully compiled */` |
|    310517 |  5136 | `	return SXRET_OK;` |
|         1 |  5137 | `Synchronize:` |
|         - |  5138 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  5139 | `	 * compiling this erroneous block.` |
|         - |  5140 | `	 */` |
|         3 |  5141 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  5142 | `		pGen->pIn++;` |
|       ! 0 |  5143 | `	}` |
|         3 |  5144 | `	return SXRET_OK;` |
|    155262 |  5145 | `}` |
|         - |  5146 | `/*` |
|         - |  5147 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  5148 | ` * According to the PHP language reference` |
|         - |  5149 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  5150 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  5151 | ` *  that is similar to that of C:` |
|         - |  5152 | ` *  if (expr)` |
|         - |  5153 | ` *   statement` |
|         - |  5154 | ` *  else construct:` |
|         - |  5155 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  5156 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  5157 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  5158 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  5159 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  5160 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  5161 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  5162 | ` *  elseif` |
|         - |  5163 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  5164 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  5165 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  5166 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  5167 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  5168 | ` *   <?php` |
|         - |  5169 | ` *    if ($a > $b) {` |
|         - |  5170 | ` *     echo "a is bigger than b";` |
|         - |  5171 | ` *    } elseif ($a == $b) {` |
|         - |  5172 | ` *     echo "a is equal to b";` |
|         - |  5173 | ` *    } else {` |
|         - |  5174 | ` *     echo "a is smaller than b";` |
|         - |  5175 | ` *    }` |
|         - |  5176 | ` *    ?>` |
|         - |  5177 | ` */` |
|   2219914 |  5178 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  5179 | `{` |
|   2219919 |  5180 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   2219919 |  5181 | `	GenBlock *pCondBlock = 0;` |
|         - |  5182 | `	sxu32 nJumpIdx;` |
|         - |  5183 | `	sxu32 nKeyID;` |
|         - |  5184 | `	sxi32 rc;` |
|         - |  5185 | `	/* Jump the 'if' keyword */` |
|   2219919 |  5186 | `	pGen->pIn++;` |
|   2219919 |  5187 | `	pToken = pGen->pIn;` |
|         - |  5188 | `	/* Create the conditional block */` |
|   2219919 |  5189 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   2219919 |  5190 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5191 | `		return SXERR_ABORT;` |
|         - |  5192 | `	}` |
|         - |  5193 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1249594 |  5194 | `	for(;;){` |
|   2499193 |  5195 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  5196 | `			/* Syntax error */` |
|       ! 0 |  5197 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5198 | `				pToken--;` |
|       ! 0 |  5199 | `			}` |
|       ! 0 |  5200 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  5201 | `			if( rc == SXERR_ABORT ){` |
|         - |  5202 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5203 | `				return SXERR_ABORT;` |
|         - |  5204 | `			}` |
|       ! 0 |  5205 | `			goto Synchronize;` |
|         - |  5206 | `		}` |
|         - |  5207 | `		/* Jump the left parenthesis '(' */` |
|   2499193 |  5208 | `		pToken++;` |
|         - |  5209 | `		/* Delimit the condition */` |
|   2499193 |  5210 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2499193 |  5211 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  5212 | `			/* Syntax error */` |
|        11 |  5213 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5214 | `				pToken--;` |
|       ! 0 |  5215 | `			}` |
|        11 |  5216 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  5217 | `			if( rc == SXERR_ABORT ){` |
|         - |  5218 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5219 | `				return SXERR_ABORT;` |
|         - |  5220 | `			}` |
|        11 |  5221 | `			goto Synchronize;` |
|         - |  5222 | `		}` |
|         - |  5223 | `		/* Swap token streams */` |
|   2499185 |  5224 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  5225 | `		/* Compile the condition */` |
|   2499185 |  5226 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5227 | `		/* Update token stream */` |
|   2499185 |  5228 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5229 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5230 | `			pGen->pIn++;` |
|       ! 0 |  5231 | `		}` |
|   2499185 |  5232 | `		pGen->pIn  = &pEnd[1];` |
|   2499185 |  5233 | `		pGen->pEnd = pTmp;` |
|   2499185 |  5234 | `		if( rc == SXERR_ABORT ){` |
|         - |  5235 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5236 | `			return SXERR_ABORT;` |
|         - |  5237 | `		}` |
|         - |  5238 | `		/* Emit the false jump */` |
|   2499185 |  5239 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5240 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2499185 |  5241 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5242 | `		/* Compile the body */` |
|   2499185 |  5243 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2499185 |  5244 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5245 | `			return SXERR_ABORT;` |
|         - |  5246 | `		}` |
|   2499185 |  5247 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    493917 |  5248 | `			break;` |
|         - |  5249 | `		}` |
|         - |  5250 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1511361 |  5251 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1511361 |  5252 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   1021313 |  5253 | `			break;` |
|         - |  5254 | `		}` |
|         - |  5255 | `		/* Emit the unconditional jump */` |
|    490053 |  5256 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5257 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    490053 |  5258 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    490053 |  5259 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    294907 |  5260 | `			pToken = &pGen->pIn[1];` |
|    294907 |  5261 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     84166 |  5262 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    105392 |  5263 | `					break;` |
|         - |  5264 | `			}` |
|     84133 |  5265 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     42064 |  5266 | `		}` |
|    279279 |  5267 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5268 | `		/* Synchronize cursors */` |
|    279279 |  5269 | `		pToken = pGen->pIn;` |
|         - |  5270 | `		/* Fix the false jump */` |
|    279279 |  5271 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5272 | `	} /* For(;;) */` |
|         - |  5273 | `	/* Fix the false jump */` |
|   2219911 |  5274 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   2219911 |  5275 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1232082 |  5276 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5277 | `			/* Compile the else block */` |
|    210779 |  5278 | `			pGen->pIn++;` |
|    210779 |  5279 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    210779 |  5280 | `			if( rc == SXERR_ABORT ){` |
|         - |  5281 |  |
|       ! 0 |  5282 | `				return SXERR_ABORT;` |
|         - |  5283 | `			}` |
|    105387 |  5284 | `	}` |
|   2219911 |  5285 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5286 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   2219911 |  5287 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5288 | `	/* Release the conditional block */` |
|   2219911 |  5289 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5290 | `	/* Statement successfully compiled */` |
|   2219911 |  5291 | `	return SXRET_OK;` |
|         4 |  5292 | `Synchronize:` |
|         - |  5293 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5294 | `	 */` |
|        67 |  5295 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5296 | `		pGen->pIn++;` |
|         3 |  5297 | `	}` |
|        11 |  5298 | `	return SXRET_OK;` |
|   1109962 |  5299 | `}` |
|         - |  5300 | `/*` |
|         - |  5301 | ` * Compile the global construct.` |
|         - |  5302 | ` * According to the PHP language reference` |
|         - |  5303 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5304 | ` *  to be used in that function.` |
|         - |  5305 | ` *  Example #1 Using global` |
|         - |  5306 | ` *  <?php` |
|         - |  5307 | ` *   $a = 1;` |
|         - |  5308 | ` *   $b = 2;` |
|         - |  5309 | ` *   function Sum()` |
|         - |  5310 | ` *   {` |
|         - |  5311 | ` *    global $a, $b;` |
|         - |  5312 | ` *    $b = $a + $b;` |
|         - |  5313 | ` *   }` |
|         - |  5314 | ` *   Sum();` |
|         - |  5315 | ` *   echo $b;` |
|         - |  5316 | ` *  ?>` |
|         - |  5317 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5318 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5319 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5320 | ` */` |
|        38 |  5321 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5322 | `{` |
|        43 |  5323 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5324 | `	sxi32 nExpr;` |
|         - |  5325 | `	sxi32 rc;` |
|         - |  5326 | `	/* Jump the 'global' keyword */` |
|        43 |  5327 | `	pGen->pIn++;` |
|        43 |  5328 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5329 | `		/* Nothing to process */` |
|       ! 0 |  5330 | `		return SXRET_OK;` |
|         - |  5331 | `	}` |
|        43 |  5332 | `	pTmp = pGen->pEnd;` |
|        43 |  5333 | `	nExpr = 0;` |
|        91 |  5334 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5335 | `		if( pGen->pIn < pNext ){` |
|        53 |  5336 | `			pGen->pEnd = pNext;` |
|        53 |  5337 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5338 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5339 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5340 | `					return SXERR_ABORT;` |
|         - |  5341 | `				}` |
|       ! 0 |  5342 | `			}else{` |
|        53 |  5343 | `				pGen->pIn++;` |
|        53 |  5344 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5345 | `					/* Emit a warning */` |
|       ! 0 |  5346 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5347 | `				}else{` |
|        53 |  5348 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5349 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5350 | `						return SXERR_ABORT;` |
|        53 |  5351 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5352 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5353 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5354 | `							/* Variable name, not a constant */` |
|        53 |  5355 | `							pLast->iP1 = 0;` |
|        24 |  5356 | `						}` |
|        53 |  5357 | `						nExpr++;` |
|        24 |  5358 | `					}` |
|         - |  5359 | `				}` |
|         - |  5360 | `			}` |
|        24 |  5361 | `		}` |
|         - |  5362 | `		/* Next expression in the stream */` |
|        53 |  5363 | `		pGen->pIn = pNext;` |
|         - |  5364 | `		/* Jump trailing commas */` |
|        63 |  5365 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5366 | `			pGen->pIn++;` |
|         5 |  5367 | `		}` |
|         5 |  5368 | `	}` |
|         - |  5369 | `	/* Restore token stream */` |
|        43 |  5370 | `	pGen->pEnd = pTmp;` |
|        43 |  5371 | `	if( nExpr > 0 ){` |
|         - |  5372 | `		/* Emit the uplink instruction */` |
|        43 |  5373 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5374 | `	}` |
|        43 |  5375 | `	return SXRET_OK;` |
|        24 |  5376 | `}` |
|         - |  5377 | `/*` |
|         - |  5378 | ` * Compile the return statement.` |
|         - |  5379 | ` * According to the PHP language reference` |
|         - |  5380 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5381 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5382 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5383 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5384 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5385 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5386 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5387 | ` *  from within the main script file, then script execution end.` |
|         - |  5388 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5389 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5390 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5391 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5392 | ` */` |
|   3004220 |  5393 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5394 | `{` |
|   3004225 |  5395 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5396 | `	sxi32 rc;` |
|   3004225 |  5397 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   3004225 |  5398 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5399 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5400 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5401 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5402 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5403 | `	 * normally below so token processing stays consistent. */` |
|   7921255 |  5404 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4917035 |  5405 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5406 | `	}` |
|   3004220 |  5407 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   3004191 |  5408 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5409 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5410 | `			"A never-returning function must not return");` |
|         3 |  5411 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5412 | `			return SXERR_ABORT;` |
|         - |  5413 | `		}` |
|         1 |  5414 | `	}` |
|         - |  5415 | `	/* Jump the 'return' keyword */` |
|   3004225 |  5416 | `	pGen->pIn++;` |
|   3004225 |  5417 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5418 | `		/* Compile the expression */` |
|   2908595 |  5419 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2908595 |  5420 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5421 | `			return SXERR_ABORT;` |
|   2908595 |  5422 | `		}else if(rc != SXERR_EMPTY ){` |
|   2908595 |  5423 | `			nRet = 1;` |
|   1454295 |  5424 | `		}` |
|   1454295 |  5425 | `	}` |
|         - |  5426 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5427 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5428 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5429 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   3004225 |  5430 | `	if( pGen->bInGenerator ){` |
|      3857 |  5431 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3857 |  5432 | `		return SXRET_OK;` |
|         - |  5433 | `	}` |
|         - |  5434 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5435 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5436 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5437 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5438 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   3000373 |  5439 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   3000373 |  5440 | `	return SXRET_OK;` |
|   1502115 |  5441 | `}` |
|         - |  5442 | `/*` |
|         - |  5443 | ` * Compile a yield expression.` |
|         - |  5444 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5445 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5446 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5447 | ` */` |
|     15682 |  5448 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5449 | `{` |
|         - |  5450 | `	SyToken *pTmp, *pSplit;` |
|     15687 |  5451 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15687 |  5452 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5453 | `	sxi32 rc;` |
|      7841 |  5454 | `	(void)iCompileFlag;` |
|         - |  5455 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15687 |  5456 | `	pGen->pIn++;` |
|         - |  5457 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5458 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5459 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5460 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5461 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15682 |  5462 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7876 |  5463 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5464 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5465 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5466 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5467 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5468 | `			return SXERR_ABORT;` |
|         - |  5469 | `		}` |
|        67 |  5470 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5471 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5472 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5473 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5474 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5475 | `				return SXERR_ABORT;` |
|         - |  5476 | `			}` |
|       ! 0 |  5477 | `		}` |
|        67 |  5478 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5479 | `		return SXRET_OK;` |
|         - |  5480 | `	}` |
|     15625 |  5481 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5482 | `		/* Bare yield — no value */` |
|         3 |  5483 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5484 | `		return SXRET_OK;` |
|         - |  5485 | `	}` |
|         - |  5486 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15623 |  5487 | `	pSplit = 0;` |
|         - |  5488 | `	{` |
|     15623 |  5489 | `		SyToken *pCur = pGen->pIn;` |
|     15623 |  5490 | `		sxi32 nNest = 0;` |
|     46673 |  5491 | `		while( pCur < pGen->pEnd ){` |
|     46365 |  5492 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5493 | `				nNest++;` |
|     46357 |  5494 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5495 | `				nNest--;` |
|     46341 |  5496 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15315 |  5497 | `				pSplit = pCur;` |
|     15315 |  5498 | `				break;` |
|         - |  5499 | `			}` |
|     31055 |  5500 | `			pCur++;` |
|         5 |  5501 | `		}` |
|         - |  5502 | `	}` |
|     15623 |  5503 | `	pTmp = pGen->pEnd;` |
|     15623 |  5504 | `	if( pSplit ){` |
|         - |  5505 | `		/* yield $key => $value */` |
|     15315 |  5506 | `		pGen->pEnd = pSplit;` |
|     15315 |  5507 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15315 |  5508 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15315 |  5509 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15315 |  5510 | `		pGen->pEnd = pTmp;` |
|     15315 |  5511 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15315 |  5512 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15315 |  5513 | `		iP1 = 1;` |
|     15315 |  5514 | `		iP2 = 1;` |
|      7660 |  5515 | `	}else{` |
|         - |  5516 | `		/* yield $value */` |
|       313 |  5517 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       313 |  5518 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       313 |  5519 | `		if( rc != SXERR_EMPTY ){` |
|       313 |  5520 | `			iP1 = 1;` |
|       154 |  5521 | `		}` |
|         - |  5522 | `	}` |
|     15623 |  5523 | `	pGen->pEnd = pTmp;` |
|     15623 |  5524 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15623 |  5525 | `	return SXRET_OK;` |
|      7846 |  5526 | `}` |
|         - |  5527 | `/*` |
|         - |  5528 | ` * Compile the die/exit language construct.` |
|         - |  5529 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5530 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5531 | ` */` |
|       130 |  5532 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5533 | `{` |
|       135 |  5534 | `	sxi32 nExpr = 0;` |
|         - |  5535 | `	sxi32 rc;` |
|         - |  5536 | `	/* Jump the die/exit keyword */` |
|       135 |  5537 | `	pGen->pIn++;` |
|       135 |  5538 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5539 | `		/* Compile the expression */` |
|       135 |  5540 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       135 |  5541 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5542 | `			return SXERR_ABORT;` |
|       135 |  5543 | `		}else if(rc != SXERR_EMPTY ){` |
|       135 |  5544 | `			nExpr = 1;` |
|        65 |  5545 | `		}` |
|        65 |  5546 | `	}` |
|         - |  5547 | `	/* Emit the HALT instruction */` |
|       135 |  5548 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       135 |  5549 | `	return SXRET_OK;` |
|        70 |  5550 | `}` |
|         - |  5551 | `/*` |
|         - |  5552 | ` * Compile the 'echo' language construct.` |
|         - |  5553 | ` */` |
|     17980 |  5554 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5555 | `{` |
|     17985 |  5556 | `	SyToken *pTmp,*pNext = 0;` |
|     17985 |  5557 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17985 |  5558 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17985 |  5559 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5560 | `	sxi32 rc;` |
|         - |  5561 | `	/* Jump the 'echo' keyword */` |
|     17985 |  5562 | `	pGen->pIn++;` |
|         - |  5563 | `	/* Compile arguments one after one */` |
|     17985 |  5564 | `	pTmp = pGen->pEnd;` |
|     45533 |  5565 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     27555 |  5566 | `		if( pGen->pIn < pNext ){` |
|     27555 |  5567 | `			pGen->pEnd = pNext;` |
|     27555 |  5568 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     27555 |  5569 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5570 | `				return SXERR_ABORT;` |
|     27555 |  5571 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5572 | `				/* Emit the consume instruction */` |
|     27529 |  5573 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     27529 |  5574 | `				nExpr++;` |
|     27529 |  5575 | `				bExpectMore = 0;` |
|     13762 |  5576 | `			}` |
|     13775 |  5577 | `		}` |
|         - |  5578 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5579 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     37131 |  5580 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      9583 |  5581 | `			if( bExpectMore ){` |
|         - |  5582 | `				/* two commas in a row */` |
|         3 |  5583 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5584 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5585 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5586 | `			}` |
|      9581 |  5587 | `			bExpectMore = 1;` |
|      9581 |  5588 | `			pNext++;` |
|         5 |  5589 | `		}` |
|     27553 |  5590 | `		pGen->pIn = pNext;` |
|         5 |  5591 | `	}` |
|         - |  5592 | `	/* Restore token stream */` |
|     17983 |  5593 | `	pGen->pEnd = pTmp;` |
|     17983 |  5594 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5595 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5596 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5597 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5598 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5599 | `	}` |
|     17953 |  5600 | `	return SXRET_OK;` |
|      8995 |  5601 | `}` |
|         - |  5602 | `/*` |
|         - |  5603 | ` * Compile the static statement.` |
|         - |  5604 | ` * According to the PHP language reference` |
|         - |  5605 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5606 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5607 | ` *  when program execution leaves this scope.` |
|         - |  5608 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5609 | ` * Symisc eXtension.` |
|         - |  5610 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5611 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5612 | ` *  Example` |
|         - |  5613 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5614 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5615 | ` */` |
|     11484 |  5616 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         5 |  5617 | `{` |
|         - |  5618 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5619 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5620 | `	GenBlock *pBlock;` |
|         - |  5621 | `	SyString *pName;` |
|         - |  5622 | `	char *zDup;` |
|         - |  5623 | `	sxu32 nLine;` |
|         - |  5624 | `	sxi32 rc;` |
|         - |  5625 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5626 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5627 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|     11484 |  5628 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      5748 |  5629 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5630 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5631 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5632 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5633 | `			return SXERR_ABORT;` |
|         3 |  5634 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5635 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5636 | `		}` |
|         3 |  5637 | `		return SXRET_OK;` |
|         - |  5638 | `	}` |
|         - |  5639 | `	/* Jump the static keyword */` |
|     11487 |  5640 | `	nLine = pGen->pIn->nLine;` |
|     11487 |  5641 | `	pGen->pIn++;` |
|         - |  5642 | `	/* Extract the enclosing function if any */` |
|     11487 |  5643 | `	pBlock = pGen->pCurrent;` |
|     22969 |  5644 | `	while( pBlock ){` |
|     22969 |  5645 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|     11487 |  5646 | `			break;` |
|         - |  5647 | `		}` |
|         - |  5648 | `		/* Point to the upper block */` |
|     11487 |  5649 | `		pBlock = pBlock->pParent;` |
|         5 |  5650 | `	}` |
|     11487 |  5651 | `	if( pBlock == 0 ){` |
|         - |  5652 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5653 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5654 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5655 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5656 | `				return SXERR_ABORT;` |
|         - |  5657 | `			}` |
|       ! 0 |  5658 | `			goto Synchronize;` |
|         - |  5659 | `		}` |
|         - |  5660 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5661 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5662 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5663 | `			return SXERR_ABORT;` |
|       ! 0 |  5664 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5665 | `			/* Emit the POP instruction */` |
|       ! 0 |  5666 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5667 | `		}` |
|       ! 0 |  5668 | `		return SXRET_OK;` |
|         - |  5669 | `	}` |
|     11487 |  5670 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5671 | `	/* Make sure we are dealing with a valid statement */` |
|     11487 |  5672 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     11480 |  5673 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5674 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5675 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5676 | `				return SXERR_ABORT;` |
|         - |  5677 | `			}` |
|         3 |  5678 | `			goto Synchronize;` |
|         - |  5679 | `	}` |
|     11485 |  5680 | `	pGen->pIn++;` |
|         - |  5681 | `	/* Extract variable name */` |
|     11485 |  5682 | `	pName = &pGen->pIn->sData;` |
|     11485 |  5683 | `	pGen->pIn++; /* Jump the var name */` |
|     11485 |  5684 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5685 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5686 | `		goto Synchronize;` |
|         - |  5687 | `	}` |
|         - |  5688 | `	/* Initialize the structure describing the static variable */` |
|     11485 |  5689 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     11485 |  5690 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5691 | `	/* Duplicate variable name */` |
|     11485 |  5692 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     11485 |  5693 | `	if( zDup == 0 ){` |
|       ! 0 |  5694 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5695 | `		return SXERR_ABORT;` |
|         - |  5696 | `	}` |
|     11485 |  5697 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5698 | `	/* Check if we have an expression to compile */` |
|     11485 |  5699 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5700 | `		SySet *pInstrContainer;` |
|         - |  5701 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5702 | `		 * Static variable can take any complex expression including function` |
|         - |  5703 | `		 * call as their initialization value.` |
|         - |  5704 | `		 * Example:` |
|         - |  5705 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5706 | `		 */` |
|     11485 |  5707 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5708 | `		/* Swap bytecode container */` |
|     11485 |  5709 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     11485 |  5710 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5711 | `		/* Compile the expression */` |
|     11485 |  5712 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5713 | `		/* Emit the done instruction */` |
|     11485 |  5714 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5715 | `		/* Restore default bytecode container */` |
|     11485 |  5716 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      5740 |  5717 | `	}` |
|         - |  5718 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     11485 |  5719 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     11485 |  5720 | `	return SXRET_OK;` |
|         1 |  5721 | `Synchronize:` |
|         - |  5722 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5723 | `	 * statement.` |
|         - |  5724 | `	 */` |
|         5 |  5725 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5726 | `		pGen->pIn++;` |
|         1 |  5727 | `	}` |
|         3 |  5728 | `	return SXRET_OK;` |
|      5747 |  5729 | `}` |
|         - |  5730 | `/*` |
|         - |  5731 | ` * Compile the var statement.` |
|         - |  5732 | ` * Symisc Extension:` |
|         - |  5733 | ` *      var statement can be used outside of a class definition.` |
|         - |  5734 | ` */` |
|         4 |  5735 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5736 | `{` |
|         - |  5737 | `	sxu32 nLine;` |
|         - |  5738 | `	sxi32 rc;` |
|         5 |  5739 | `	nLine = pGen->pIn->nLine;` |
|         - |  5740 | `	/* Jump the 'var' keyword */` |
|         5 |  5741 | `	pGen->pIn++;` |
|         5 |  5742 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5743 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5744 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5745 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5746 | `			pGen->pIn++;` |
|       ! 0 |  5747 | `		}` |
|       ! 0 |  5748 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5749 | `			return SXERR_ABORT;` |
|         - |  5750 | `		}` |
|       ! 0 |  5751 | `	}else{` |
|         - |  5752 | `		/* Compile the expression */` |
|         5 |  5753 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5754 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5755 | `			return SXERR_ABORT;` |
|         5 |  5756 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5757 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5758 | `		}` |
|         - |  5759 | `	}` |
|         5 |  5760 | `	return SXRET_OK;` |
|         3 |  5761 | `}` |
|         - |  5762 | `/*` |
|         - |  5763 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5764 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5765 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5766 | ` */` |
|         - |  5767 | `/*` |
|         - |  5768 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5769 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5770 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5771 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5772 | ` *` |
|         - |  5773 | ` * Resolution order:` |
|         - |  5774 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5775 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5776 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5777 | ` *` |
|         - |  5778 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5779 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5780 | ` * Returns the (possibly new) literal index.` |
|         - |  5781 | ` */` |
|   5624258 |  5782 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5783 | `{` |
|         - |  5784 | `	ph7_value *pLit;` |
|         - |  5785 | `	const char *zLit;` |
|         - |  5786 | `	SyString sQualified;` |
|         - |  5787 | `	sxu32 nLit;` |
|         - |  5788 | `	sxu32 k;` |
|         - |  5789 | `	sxu32 nNewIdx;` |
|         - |  5790 | `	int hasNsSep;` |
|         - |  5791 | `	SyHashEntry *pImport;` |
|         - |  5792 | `	ph7_value *pNew;` |
|   5624263 |  5793 | `	if( pFromImport ){` |
|   4549515 |  5794 | `		*pFromImport = 0;` |
|   2274755 |  5795 | `	}` |
|   5624263 |  5796 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   5624263 |  5797 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5798 | `		return nOrigIdx;` |
|         - |  5799 | `	}` |
|   5624263 |  5800 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   5624263 |  5801 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5802 | `	/* Skip if already qualified (contains backslash) */` |
|   5624263 |  5803 | `	hasNsSep = 0;` |
|  66538401 |  5804 | `	for( k = 0; k < nLit; k++ ){` |
|  60914161 |  5805 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  30457074 |  5806 | `	}` |
|   5624263 |  5807 | `	if( hasNsSep ){` |
|        20 |  5808 | `		return nOrigIdx;` |
|         - |  5809 | `	}` |
|         - |  5810 | `	/* Check use imports first (works even outside namespaces) */` |
|   5624245 |  5811 | `	SyBlobReset(&pGen->sWorker);` |
|   5624245 |  5812 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   5624245 |  5813 | `	if( pImport ){` |
|        41 |  5814 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5815 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5816 | `		if( pFromImport ){` |
|        18 |  5817 | `			*pFromImport = 1;` |
|         8 |  5818 | `		}` |
|        23 |  5819 | `	}else{` |
|   5624209 |  5820 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   5624079 |  5821 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5822 | `		}` |
|         - |  5823 | `		/* Prepend current namespace */` |
|       135 |  5824 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       135 |  5825 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       135 |  5826 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5827 | `	}` |
|         - |  5828 | `	/* Look up or create a new literal for the qualified name */` |
|       171 |  5829 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       171 |  5830 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        77 |  5831 | `		return nNewIdx; /* Already interned */` |
|         - |  5832 | `	}` |
|        99 |  5833 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        99 |  5834 | `	if( pNew == 0 ){` |
|       ! 0 |  5835 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5836 | `	}` |
|        99 |  5837 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        99 |  5838 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        99 |  5839 | `	return nNewIdx;` |
|   2812134 |  5840 | `}` |
|         - |  5841 | `/*` |
|         - |  5842 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5843 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5844 | ` */` |
|    449244 |  5845 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5846 | `{` |
|         - |  5847 | `	SyHashEntry *pImport;` |
|         - |  5848 | `	/* Check use imports first */` |
|    449249 |  5849 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    449249 |  5850 | `	if( pImport ){` |
|        21 |  5851 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        21 |  5852 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        21 |  5853 | `		return;` |
|         - |  5854 | `	}` |
|         - |  5855 | `	/* Prepend current namespace if active */` |
|    449231 |  5856 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        14 |  5857 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        14 |  5858 | `		SyBlobAppend(pOut,"\\",1);` |
|         6 |  5859 | `	}` |
|    449231 |  5860 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    224627 |  5861 | `}` |
|         - |  5862 | `/*` |
|         - |  5863 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5864 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5865 | ` * The caller must release pOut when done.` |
|         - |  5866 | ` */` |
|    430382 |  5867 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5868 | `{` |
|    430387 |  5869 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3909 |  5870 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3909 |  5871 | `		SyBlobAppend(pOut,"\\",1);` |
|      1952 |  5872 | `	}` |
|    430387 |  5873 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    430387 |  5874 | `}` |
|         - |  5875 | `/*` |
|         - |  5876 | ` * Compile a namespace statement` |
|         - |  5877 | ` * According to the PHP language reference manual` |
|         - |  5878 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5879 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5880 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5881 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5882 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5883 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5884 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5885 | ` *  programming world.` |
|         - |  5886 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5887 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5888 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5889 | ` *  classes/functions/constants.` |
|         - |  5890 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5891 | ` *  readability of source code.` |
|         - |  5892 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5893 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5894 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5895 | ` *       class MyClass {}` |
|         - |  5896 | ` *       function myfunction() {}` |
|         - |  5897 | ` *       const MYCONST = 1;` |
|         - |  5898 | ` *       $a = new MyClass;` |
|         - |  5899 | ` *       $c = new \my\name\MyClass;` |
|         - |  5900 | ` *       $a = strlen('hi');` |
|         - |  5901 | ` *       $d = namespace\MYCONST;` |
|         - |  5902 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5903 | ` *       echo constant($d);` |
|         - |  5904 | ` * NOTE` |
|         - |  5905 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5906 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5907 | ` */` |
|         - |  5908 | `/*` |
|         - |  5909 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5910 | ` */` |
|        14 |  5911 | `static const char * TokenTypeName(sxu32 nType)` |
|         4 |  5912 | `{` |
|        18 |  5913 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        11 |  5914 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        11 |  5915 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        11 |  5916 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        11 |  5917 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        11 |  5918 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5919 | `	return "token";` |
|        11 |  5920 | `}` |
|      3952 |  5921 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5922 | `{` |
|         - |  5923 | `	sxu32 nLine;` |
|         - |  5924 | `	sxi32 rc;` |
|      3957 |  5925 | `	nLine = pGen->pIn->nLine;` |
|      3957 |  5926 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5927 | `	/* Reset namespace and clear previous use imports */` |
|      3957 |  5928 | `	SyBlobReset(&pGen->sNamespace);` |
|      3957 |  5929 | `	SyHashRelease(&pGen->hUseImports);` |
|      3957 |  5930 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3957 |  5931 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3957 |  5932 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3957 |  5933 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3957 |  5934 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3957 |  5935 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5936 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5937 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5938 | `		return SXRET_OK;` |
|         - |  5939 | `	}` |
|      3957 |  5940 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5941 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5942 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5943 | `		return SXRET_OK;` |
|         - |  5944 | `	}` |
|      3957 |  5945 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5946 | `		/* namespace { } — global namespace block */` |
|         5 |  5947 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         5 |  5948 | `		return SXRET_OK;` |
|         - |  5949 | `	}` |
|         - |  5950 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7975 |  5951 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      4027 |  5952 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5953 | `			/* Append backslash separator */` |
|        42 |  5954 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        42 |  5955 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        19 |  5956 | `			}` |
|        23 |  5957 | `		}else{` |
|         - |  5958 | `			/* Append identifier */` |
|      3989 |  5959 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5960 | `		}` |
|      4027 |  5961 | `		pGen->pIn++;` |
|         5 |  5962 | `	}` |
|         - |  5963 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5964 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5965 | `	{` |
|      3953 |  5966 | `		char *zNsDup = 0;` |
|      3953 |  5967 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5924 |  5968 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3946 |  5969 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1973 |  5970 | `		}` |
|      3953 |  5971 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5972 | `	}` |
|      3953 |  5973 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5974 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5975 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5976 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5977 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5978 | `			return SXERR_ABORT;` |
|         - |  5979 | `		}` |
|         2 |  5980 | `	}` |
|      3953 |  5981 | `	return SXRET_OK;` |
|      1981 |  5982 | `}` |
|         - |  5983 | `/*` |
|         - |  5984 | ` * Compile the 'use' statement` |
|         - |  5985 | ` * According to the PHP language reference manual` |
|         - |  5986 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5987 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5988 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5989 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5990 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5991 | ` *  a function or constant is not supported.` |
|         - |  5992 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5993 | ` * NOTE` |
|         - |  5994 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5995 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5996 | ` */` |
|        78 |  5997 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5998 | `{` |
|         - |  5999 | `	sxu32 nLine;` |
|         - |  6000 | `	sxi32 rc;` |
|         - |  6001 | `	SyBlob sPath;` |
|         - |  6002 | `	SyString sAlias;` |
|         - |  6003 | `	SyToken *pLast;` |
|         - |  6004 | `	char *zDup;` |
|         - |  6005 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  6006 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  6007 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        83 |  6008 | `	nLine = pGen->pIn->nLine;` |
|        83 |  6009 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  6010 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        83 |  6011 | `	iUseType = 0;` |
|        83 |  6012 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  6013 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  6014 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  6015 | `			iUseType = 1;` |
|        16 |  6016 | `			pGen->pIn++;` |
|        23 |  6017 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  6018 | `			iUseType = 2;` |
|        16 |  6019 | `			pGen->pIn++;` |
|         7 |  6020 | `		}` |
|        14 |  6021 | `	}` |
|         - |  6022 | `	/* Select target hash tables based on import type */` |
|        83 |  6023 | `	switch( iUseType ){` |
|         7 |  6024 | `		case 1:` |
|        16 |  6025 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  6026 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  6027 | `			break;` |
|         7 |  6028 | `		case 2:` |
|        16 |  6029 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  6030 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  6031 | `			break;` |
|        25 |  6032 | `		default:` |
|        55 |  6033 | `			pGenHash = &pGen->hUseImports;` |
|        55 |  6034 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        50 |  6035 | `			break;` |
|         - |  6036 | `	}` |
|        83 |  6037 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  6038 | `	/* Process one or more use declarations separated by commas */` |
|        40 |  6039 | `	for(;;){` |
|        85 |  6040 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  6041 | `			break;` |
|         - |  6042 | `		}` |
|        85 |  6043 | `		SyBlobReset(&sPath);` |
|        85 |  6044 | `		pLast = 0;` |
|         - |  6045 | `		/* Collect the full namespace path */` |
|       293 |  6046 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       213 |  6047 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       147 |  6048 | `				pLast = pGen->pIn;` |
|       147 |  6049 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        71 |  6050 | `					SyBlobAppend(&sPath,"\\",1);` |
|        33 |  6051 | `				}` |
|       147 |  6052 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        71 |  6053 | `			}` |
|       213 |  6054 | `			pGen->pIn++;` |
|         5 |  6055 | `		}` |
|        85 |  6056 | `		if( pLast == 0 ){` |
|         - |  6057 | `			/* Empty path */` |
|         6 |  6058 | `			break;` |
|         - |  6059 | `		}` |
|         - |  6060 | `		/* Default alias is the last component of the path */` |
|        81 |  6061 | `		sAlias = pLast->sData;` |
|         - |  6062 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        76 |  6063 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        55 |  6064 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        27 |  6065 | `			pGen->pIn++; /* Jump 'as' */` |
|        27 |  6066 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        27 |  6067 | `				sAlias = pGen->pIn->sData;` |
|        27 |  6068 | `				pGen->pIn++;` |
|        12 |  6069 | `			}` |
|        12 |  6070 | `		}` |
|         - |  6071 | `		/* Check for duplicate import alias (per-type) */` |
|        81 |  6072 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  6073 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6074 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  6075 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  6076 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6077 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  6078 | `				return SXERR_ABORT;` |
|         - |  6079 | `			}` |
|         2 |  6080 | `		}` |
|         - |  6081 | `		/* Register the import: alias -> FQN.` |
|         - |  6082 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  6083 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  6084 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       119 |  6085 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        76 |  6086 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        81 |  6087 | `		if( zDup ){` |
|        81 |  6088 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        81 |  6089 | `			if( pVmHash ){` |
|         - |  6090 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  6091 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        53 |  6092 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        53 |  6093 | `				if( zAliasDup ){` |
|        53 |  6094 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        24 |  6095 | `				}` |
|        24 |  6096 | `			}` |
|        81 |  6097 | `			if( iUseType == 2 ){` |
|         - |  6098 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  6099 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  6100 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  6101 | `				if( zAliasDup ){` |
|         - |  6102 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  6103 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  6104 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  6105 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  6106 | `					if( azPair ){` |
|        16 |  6107 | `						azPair[0] = zAliasDup;` |
|        16 |  6108 | `						azPair[1] = zDup;` |
|        16 |  6109 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  6110 | `					}` |
|         7 |  6111 | `				}` |
|         7 |  6112 | `			}` |
|        38 |  6113 | `		}` |
|         - |  6114 | `		/* Check for comma (multiple use declarations) */` |
|        81 |  6115 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  6116 | `			pGen->pIn++;` |
|         2 |  6117 | `		}else{` |
|        42 |  6118 | `			break;` |
|         - |  6119 | `		}` |
|         1 |  6120 | `	}` |
|        83 |  6121 | `	SyBlobRelease(&sPath);` |
|        83 |  6122 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  6123 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  6124 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  6125 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6126 | `			return SXERR_ABORT;` |
|         - |  6127 | `		}` |
|         1 |  6128 | `	}` |
|        83 |  6129 | `	return SXRET_OK;` |
|        44 |  6130 | `}` |
|         - |  6131 | `/*` |
|         - |  6132 | ` * Compile the stupid 'declare' language construct.` |
|         - |  6133 | ` *` |
|         - |  6134 | ` * According to the PHP language reference manual.` |
|         - |  6135 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  6136 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  6137 | ` *  declare (directive)` |
|         - |  6138 | ` *   statement` |
|         - |  6139 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  6140 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  6141 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  6142 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  6143 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  6144 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  6145 | ` * <?php` |
|         - |  6146 | ` * // these are the same:` |
|         - |  6147 | ` * // you can use this:` |
|         - |  6148 | ` * declare(ticks=1) {` |
|         - |  6149 | ` *   // entire script here` |
|         - |  6150 | ` * }` |
|         - |  6151 | ` * // or you can use this:` |
|         - |  6152 | ` * declare(ticks=1);` |
|         - |  6153 | ` * // entire script here` |
|         - |  6154 | ` * ?>` |
|         - |  6155 | ` *` |
|         - |  6156 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  6157 | ` */` |
|         - |  6158 | `/*` |
|         - |  6159 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  6160 | ` */` |
|        72 |  6161 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  6162 | `{` |
|       109 |  6163 | `	return SyStringLength(pName) == nWant` |
|        72 |  6164 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  6165 | `}` |
|         - |  6166 |  |
|        42 |  6167 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  6168 | `{` |
|        47 |  6169 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  6170 | `	SyToken *pBodyEnd = 0;` |
|         - |  6171 | `	SyToken *pBodyStart;` |
|         - |  6172 | `	SyToken *pCursor;` |
|         - |  6173 | `	int bHasStrictTypes;` |
|         - |  6174 | `	int bBlockForm;` |
|         - |  6175 | `	int bPlacementOk;` |
|         - |  6176 | `	sxi32 rc;` |
|        47 |  6177 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  6178 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  6179 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         6 |  6180 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6181 | `			return SXERR_ABORT;` |
|         - |  6182 | `		}` |
|         6 |  6183 | `		goto Synchro;` |
|         - |  6184 | `	}` |
|        43 |  6185 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  6186 | `	pBodyStart = pGen->pIn;` |
|         - |  6187 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  6188 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  6189 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  6190 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  6191 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6192 | `			return SXERR_ABORT;` |
|         - |  6193 | `		}` |
|       ! 0 |  6194 | `		return SXRET_OK;` |
|         - |  6195 | `	}` |
|         - |  6196 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  6197 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  6198 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  6199 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  6200 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  6201 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6202 | `			return SXERR_ABORT;` |
|         - |  6203 | `		}` |
|       ! 0 |  6204 | `	}` |
|        43 |  6205 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  6206 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  6207 | `	bHasStrictTypes = 0;` |
|         - |  6208 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  6209 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  6210 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  6211 | `	pCursor = pBodyStart;` |
|        55 |  6212 | `	while( pCursor < pBodyEnd ){` |
|        51 |  6213 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  6214 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  6215 | `				bHasStrictTypes = 1;` |
|        39 |  6216 | `				break;` |
|         - |  6217 | `			}` |
|         2 |  6218 | `		}` |
|        14 |  6219 | `		pCursor++;` |
|         2 |  6220 | `	}` |
|        43 |  6221 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  6222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6223 | `			"strict_types declaration must not use block mode");` |
|         3 |  6224 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6225 | `		return SXRET_OK;` |
|         - |  6226 | `	}` |
|        41 |  6227 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  6228 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6229 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  6230 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  6231 | `		return SXRET_OK;` |
|         - |  6232 | `	}` |
|         - |  6233 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  6234 | `	pCursor = pBodyStart;` |
|        69 |  6235 | `	while( pCursor < pBodyEnd ){` |
|         - |  6236 | `		SyToken *pNameTok;` |
|         - |  6237 | `		SyToken *pEqTok;` |
|         - |  6238 | `		SyToken *pValTok;` |
|         - |  6239 | `		SyString *pDirName;` |
|         - |  6240 | `		int bIsStrict;` |
|         - |  6241 | `		int iStrictValue;` |
|        39 |  6242 | `		pNameTok = pCursor;` |
|        39 |  6243 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6244 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6245 | `				"declare: Expecting a directive name");` |
|       ! 0 |  6246 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6247 | `			return SXRET_OK;` |
|         - |  6248 | `		}` |
|        39 |  6249 | `		pEqTok = pNameTok + 1;` |
|        39 |  6250 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6251 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6252 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6253 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6254 | `			return SXRET_OK;` |
|         - |  6255 | `		}` |
|        39 |  6256 | `		pValTok = pEqTok + 1;` |
|        39 |  6257 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6258 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6259 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6260 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6261 | `			return SXRET_OK;` |
|         - |  6262 | `		}` |
|        39 |  6263 | `		pDirName = &pNameTok->sData;` |
|        39 |  6264 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6265 | `		if( bIsStrict ){` |
|         - |  6266 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6267 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6268 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6269 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6270 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6271 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6272 | `				return SXRET_OK;` |
|         - |  6273 | `			}` |
|        35 |  6274 | `			iStrictValue = -1;` |
|        35 |  6275 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6276 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6277 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6278 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6279 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6280 | `			}` |
|        35 |  6281 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6282 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6283 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6284 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6285 | `				return SXRET_OK;` |
|         - |  6286 | `			}` |
|        32 |  6287 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6288 | `		}else{` |
|         - |  6289 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6290 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6291 | `			 * behavior don't regress. */` |
|         8 |  6292 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6293 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6294 | `				ph7_lib_version()` |
|         - |  6295 | `				);` |
|         - |  6296 | `		}` |
|        37 |  6297 | `		pCursor = pValTok + 1;` |
|         - |  6298 | `		/* Consume separating comma (or end). */` |
|        37 |  6299 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6300 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6301 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6302 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6303 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6304 | `				return SXRET_OK;` |
|         - |  6305 | `			}` |
|         3 |  6306 | `			pCursor++;` |
|         1 |  6307 | `		}` |
|         5 |  6308 | `	}` |
|         - |  6309 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6310 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6311 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        35 |  6312 | `	return SXRET_OK;` |
|         2 |  6313 | `Synchro:` |
|         - |  6314 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6315 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6316 | `		pGen->pIn++;` |
|         2 |  6317 | `	}` |
|         6 |  6318 | `	return SXRET_OK;` |
|        26 |  6319 | `}` |
|         - |  6320 | `/*` |
|         - |  6321 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6322 | ` * as follows:` |
|         - |  6323 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6324 | ` * {` |
|         - |  6325 | ` *   return "Making a cup of $type.\n";` |
|         - |  6326 | ` * }` |
|         - |  6327 | ` * Symisc eXtension.` |
|         - |  6328 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6329 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6330 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6331 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6332 | ` *      {` |
|         - |  6333 | ` *       var_dump($a);` |
|         - |  6334 | ` *      }` |
|         - |  6335 | ` *     //call test without args` |
|         - |  6336 | ` *      test();` |
|         - |  6337 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6338 | ` *      Example:` |
|         - |  6339 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6340 | ` * 3 -) Function overloading!!` |
|         - |  6341 | ` *      Example:` |
|         - |  6342 | ` *      function foo($a) {` |
|         - |  6343 | ` *   	  return $a.PHP_EOL;` |
|         - |  6344 | ` *	    }` |
|         - |  6345 | ` *	    function foo($a, $b) {` |
|         - |  6346 | ` *   	  return $a + $b;` |
|         - |  6347 | ` *	    }` |
|         - |  6348 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6349 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6350 | ` *      // Same arg` |
|         - |  6351 | ` *	   function foo(string $a)` |
|         - |  6352 | ` *	   {` |
|         - |  6353 | ` *	     echo "a is a string\n";` |
|         - |  6354 | ` *	     var_dump($a);` |
|         - |  6355 | ` *	   }` |
|         - |  6356 | ` *	  function foo(int $a)` |
|         - |  6357 | ` *	  {` |
|         - |  6358 | ` *	    echo "a is integer\n";` |
|         - |  6359 | ` *	    var_dump($a);` |
|         - |  6360 | ` *	  }` |
|         - |  6361 | ` *	  function foo(array $a)` |
|         - |  6362 | ` *	  {` |
|         - |  6363 | ` * 	    echo "a is an array\n";` |
|         - |  6364 | ` * 	    var_dump($a);` |
|         - |  6365 | ` *	  }` |
|         - |  6366 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6367 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6368 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6369 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6370 | ` * introduced by the PH7 engine.` |
|         - |  6371 | ` */` |
|    566114 |  6372 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6373 | `{` |
|         - |  6374 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6375 | `	SySet *pInstrContainer;` |
|         - |  6376 | `	sxi32 rc;` |
|         - |  6377 | `	/* Swap token stream */` |
|    566119 |  6378 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    566119 |  6379 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    566119 |  6380 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6381 | `	/* Compile the expression holding the argument value */` |
|    566119 |  6382 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6383 | `	/* Emit the done instruction */` |
|    566119 |  6384 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    566119 |  6385 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    566119 |  6386 | `	RE_SWAP_DELIMITER(pGen);` |
|    566119 |  6387 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6388 | `		return SXERR_ABORT;` |
|         - |  6389 | `	}` |
|    566119 |  6390 | `	return SXRET_OK;` |
|    283062 |  6391 | `}` |
|         - |  6392 | `/*` |
|         - |  6393 | ` * Collect function arguments one after one.` |
|         - |  6394 | ` * According to the PHP language reference manual.` |
|         - |  6395 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6396 | ` * list of expressions.` |
|         - |  6397 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6398 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6399 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6400 | ` * for more information.` |
|         - |  6401 | ` * Example #1 Passing arrays to functions` |
|         - |  6402 | ` * <?php` |
|         - |  6403 | ` * function takes_array($input)` |
|         - |  6404 | ` * {` |
|         - |  6405 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6406 | ` * }` |
|         - |  6407 | ` * ?>` |
|         - |  6408 | ` * Making arguments be passed by reference` |
|         - |  6409 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6410 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6411 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6412 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6413 | ` * to the argument name in the function definition:` |
|         - |  6414 | ` * Example #2 Passing function parameters by reference` |
|         - |  6415 | ` * <?php` |
|         - |  6416 | ` * function add_some_extra(&$string)` |
|         - |  6417 | ` * {` |
|         - |  6418 | ` *   $string .= 'and something extra.';` |
|         - |  6419 | ` * }` |
|         - |  6420 | ` * $str = 'This is a string, ';` |
|         - |  6421 | ` * add_some_extra($str);` |
|         - |  6422 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6423 | ` * ?>` |
|         - |  6424 | ` *` |
|         - |  6425 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6426 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6427 | ` * on these extension.` |
|         - |  6428 | ` */` |
|   1290708 |  6429 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6430 | `{` |
|         - |  6431 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6432 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6433 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6434 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6435 | `	sxi32 rc;` |
|         - |  6436 |  |
|   1290713 |  6437 | `	pIn = pGen->pIn;` |
|   1290713 |  6438 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6439 | `	/* Process arguments one after one */` |
|   1673603 |  6440 | `	for(;;){` |
|   3347211 |  6441 | `		if( pIn >= pEnd ){` |
|         - |  6442 | `			/* No more arguments to process */` |
|   1290697 |  6443 | `			break;` |
|         - |  6444 | `		}` |
|   2056519 |  6445 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2056519 |  6446 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2056519 |  6447 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2056519 |  6448 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2056519 |  6449 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6450 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6451 | `		 * first token inside the main token stream */` |
|   2056519 |  6452 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6453 | `			return SXERR_ABORT;` |
|         - |  6454 | `		}` |
|         - |  6455 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6456 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6457 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6458 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6459 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6460 | `		{` |
|   2056519 |  6461 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2056519 |  6462 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2056519 |  6463 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6464 | `			int nSetTok;` |
|         - |  6465 | `			sxi32 nSetVis;` |
|   2056519 |  6466 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6467 | `				bReadonly = 1;` |
|         3 |  6468 | `				pIn++;` |
|         1 |  6469 | `			}` |
|   2056519 |  6470 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2056519 |  6471 | `			if( nSetVis ){` |
|         - |  6472 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6473 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6474 | `				bVisSeen = 1;` |
|         3 |  6475 | `				pIn += nSetTok;` |
|         3 |  6476 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6477 | `					bReadonly = 1;` |
|       ! 0 |  6478 | `					pIn++;` |
|         1 |  6479 | `				}` |
|   2056518 |  6480 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     88377 |  6481 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     88377 |  6482 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        91 |  6483 | `					bVisSeen = 1;` |
|        91 |  6484 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       121 |  6485 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6486 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        91 |  6487 | `					pIn++;` |
|        91 |  6488 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        91 |  6489 | `					if( nSetVis ){` |
|         - |  6490 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6491 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6492 | `						pIn += nSetTok;` |
|         1 |  6493 | `					}` |
|        91 |  6494 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6495 | `						bReadonly = 1;` |
|        18 |  6496 | `						pIn++;` |
|         7 |  6497 | `					}` |
|        43 |  6498 | `				}` |
|     44186 |  6499 | `			}` |
|   2056519 |  6500 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6501 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2056517 |  6502 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6503 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6504 | `			}` |
|   2056519 |  6505 | `			if( bVisSeen \|\| bReadonly ){` |
|        95 |  6506 | `				if( !bCtorCtx ){` |
|         6 |  6507 | `					if( bAbstractCtx ){` |
|         3 |  6508 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6509 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6510 | `					}else{` |
|         3 |  6511 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6512 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6513 | `					}` |
|         6 |  6514 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6515 | `						return SXERR_ABORT;` |
|         - |  6516 | `					}` |
|         6 |  6517 | `					return SXERR_SYNTAX;` |
|         - |  6518 | `				}` |
|        91 |  6519 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        91 |  6520 | `				sArg.iPromoteVis = iVis;` |
|        91 |  6521 | `				if( bReadonly ){` |
|        20 |  6522 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6523 | `				}` |
|        43 |  6524 | `			}` |
|         - |  6525 | `		}` |
|         - |  6526 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   2056510 |  6527 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1108895 |  6528 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    149796 |  6529 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    115281 |  6530 | `			sxu32 nLineLocal = pIn->nLine;` |
|    115281 |  6531 | `			sxi32 iTFlags = 0;` |
|    115281 |  6532 | `			pGen->pIn = pIn;` |
|    115281 |  6533 | `			rc = GenStateParseUnionTypeDecl(` |
|     57638 |  6534 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57638 |  6535 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6536 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6537 | `				/* bAllowVoid */ 0,` |
|     57638 |  6538 | `						nLineLocal);` |
|    115281 |  6539 | `			pIn = pGen->pIn;` |
|    115281 |  6540 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6541 | `				return SXERR_ABORT;` |
|    115281 |  6542 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6543 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6544 | `				return SXERR_SYNTAX;` |
|    115279 |  6545 | `			}else if( rc == SXERR_SYNTAX ){` |
|        10 |  6546 | `				if( pIn < pEnd ){` |
|        14 |  6547 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6548 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6549 | `						&pIn->sData);` |
|         6 |  6550 | `				}else{` |
|       ! 0 |  6551 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6552 | `						"syntax error, unexpected end of file");` |
|         - |  6553 | `				}` |
|        10 |  6554 | `				return SXERR_SYNTAX;` |
|         - |  6555 | `			}` |
|    115271 |  6556 | `			sArg.iFlags \|= iTFlags;` |
|     57633 |  6557 | `		}` |
|   2056505 |  6558 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6559 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6560 | `			return rc;` |
|         - |  6561 | `		}` |
|   2056505 |  6562 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6563 | `			/* Pass by reference,record that */` |
|     22991 |  6564 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     22991 |  6565 | `			pIn++;` |
|     11493 |  6566 | `		}` |
|   2056505 |  6567 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6568 | `			/* Variadic parameter: ...$args */` |
|     23053 |  6569 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     23053 |  6570 | `			pIn++;` |
|     11524 |  6571 | `		}` |
|   2056505 |  6572 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6573 | `			/* Invalid argument */` |
|       ! 0 |  6574 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6575 | `			return rc;` |
|         - |  6576 | `		}` |
|   2056505 |  6577 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6578 | `		/* Copy argument name */` |
|   2056505 |  6579 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2056505 |  6580 | `		if( zDup == 0 ){` |
|       ! 0 |  6581 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6582 | `			return SXERR_ABORT;` |
|         - |  6583 | `		}` |
|   2056505 |  6584 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2056505 |  6585 | `		pIn++;` |
|   2056505 |  6586 | `		if( pIn < pEnd ){` |
|   1144503 |  6587 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6588 | `				SyToken *pDefend;` |
|    566121 |  6589 | `				sxi32 iNest = 0;` |
|    566121 |  6590 | `				pIn++; /* Jump the equal sign */` |
|    566121 |  6591 | `				pDefend = pIn;` |
|         - |  6592 | `				/* Process the default value associated with this argument */` |
|   1189631 |  6593 | `				while( pDefend < pEnd ){` |
|    810945 |  6594 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    187435 |  6595 | `						break;` |
|         - |  6596 | `					}` |
|    623515 |  6597 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6598 | `						/* Increment nesting level */` |
|     26785 |  6599 | `						iNest++;` |
|    610125 |  6600 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6601 | `						/* Decrement nesting level */` |
|     26785 |  6602 | `						iNest--;` |
|     13390 |  6603 | `					}` |
|    623515 |  6604 | `					pDefend++;` |
|         5 |  6605 | `				}` |
|    566121 |  6606 | `				if( pIn >= pDefend ){` |
|         3 |  6607 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6608 | `					return rc;` |
|         - |  6609 | `				}` |
|         - |  6610 | `				/* Process default value */` |
|    566119 |  6611 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    566119 |  6612 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6613 | `					return rc;` |
|         - |  6614 | `				}` |
|         - |  6615 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6616 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6617 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6618 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6619 | `				 * arg-type check lets null through. */` |
|    566114 |  6620 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    307936 |  6621 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    307933 |  6622 | `					&& &pIn[1] == pDefend` |
|     45927 |  6623 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     34438 |  6624 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     21043 |  6625 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15307 |  6626 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6627 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6628 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6629 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6630 | `					 * already up at this point). */` |
|         - |  6631 | `					{` |
|     15307 |  6632 | `						const char *zSep = "";` |
|     15307 |  6633 | `						SyString sCls = { "", 0 };` |
|     15307 |  6634 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15301 |  6635 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15301 |  6636 | `							zSep = "::";` |
|      7648 |  6637 | `						}` |
|     22958 |  6638 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6639 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7651 |  6640 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6641 | `					}` |
|      7651 |  6642 | `				}` |
|         - |  6643 | `				/* Point beyond the default value */` |
|    566119 |  6644 | `				pIn = pDefend;` |
|    283057 |  6645 | `			}` |
|   1144501 |  6646 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6647 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6648 | `				return rc;` |
|         - |  6649 | `			}` |
|   1144501 |  6650 | `			pIn++; /* Jump the trailing comma */` |
|    572248 |  6651 | `		}` |
|         - |  6652 | `		/* Append argument signature */` |
|   2056503 |  6653 | `		if( sArg.nType > 0 ){` |
|    115209 |  6654 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6655 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26873 |  6656 | `				int marker = 'o';` |
|     26873 |  6657 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26873 |  6658 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13439 |  6659 | `			}else{` |
|         - |  6660 | `				int c;` |
|     88341 |  6661 | `				c = 'n'; /* cc warning */` |
|         - |  6662 | `				/* Type leading character */` |
|     88341 |  6663 | `				switch(sArg.nType){` |
|      5743 |  6664 | `				case MEMOBJ_HASHMAP:` |
|         - |  6665 | `					/* Hashmap aka 'array' */` |
|     11491 |  6666 | `					c = 'h';` |
|     11491 |  6667 | `					break;` |
|      9688 |  6668 | `				case MEMOBJ_INT:` |
|         - |  6669 | `					/* Integer */` |
|     19381 |  6670 | `					c = 'i';` |
|     19381 |  6671 | `					break;` |
|         2 |  6672 | `				case MEMOBJ_BOOL:` |
|         - |  6673 | `					/* Bool */` |
|         5 |  6674 | `					c = 'b';` |
|         5 |  6675 | `					break;` |
|         5 |  6676 | `				case MEMOBJ_REAL:` |
|         - |  6677 | `					/* Float */` |
|        12 |  6678 | `					c = 'f';` |
|        12 |  6679 | `					break;` |
|     28722 |  6680 | `				case MEMOBJ_STRING:` |
|         - |  6681 | `					/* String */` |
|     57449 |  6682 | `					c = 's';` |
|     57449 |  6683 | `					break;` |
|         7 |  6684 | `				case MEMOBJ_OBJ:` |
|         - |  6685 | `					/* Object */` |
|        16 |  6686 | `					c = 'o';` |
|        14 |  6687 | `					break;` |
|         1 |  6688 | `				default:` |
|         2 |  6689 | `					break;` |
|         - |  6690 | `				}` |
|     88341 |  6691 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6692 | `			}` |
|     57607 |  6693 | `		}else{` |
|         - |  6694 | `			/* No type is associated with this parameter which mean` |
|         - |  6695 | `			 * that this function is not condidate for overloading.` |
|         - |  6696 | `			 */` |
|   1941299 |  6697 | `			SyBlobRelease(&sSig);` |
|         - |  6698 | `		}` |
|         - |  6699 | `		/* Save in the argument set */` |
|   2056503 |  6700 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6701 | `	}` |
|   1290697 |  6702 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6703 | `		/* Save function signature */` |
|     84539 |  6704 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     42267 |  6705 | `	}` |
|   1290697 |  6706 | `	return SXRET_OK;` |
|    645359 |  6707 | `}` |
|         - |  6708 | `/*` |
|         - |  6709 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6710 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6711 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6712 | ` */` |
|     34462 |  6713 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6714 | `{` |
|     34467 |  6715 | `	sxi32 iParen = 0;` |
|     34467 |  6716 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6717 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6718 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6719 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    153215 |  6720 | `	while( pIn < pEnd ){` |
|    153215 |  6721 | `		sxu32 t = pIn->nType;` |
|    153215 |  6722 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    149333 |  6723 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    103387 |  6724 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     84221 |  6725 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    118753 |  6726 | `		pIn++;` |
|         5 |  6727 | `	}` |
|     19171 |  6728 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6729 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6730 | `	{` |
|     19171 |  6731 | `		sxi32 d = 0;` |
|    761497 |  6732 | `		while( pIn < pEnd ){` |
|    761497 |  6733 | `			sxu32 t = pIn->nType;` |
|    761497 |  6734 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    730851 |  6735 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    742331 |  6736 | `			pIn++;` |
|         5 |  6737 | `		}` |
|         - |  6738 | `	}` |
|     19171 |  6739 | `	return pIn;` |
|     17236 |  6740 | `}` |
|         - |  6741 | `/*` |
|         - |  6742 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6743 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6744 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6745 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6746 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6747 | ` * detached-mini-program path untouched.` |
|         - |  6748 | ` */` |
|         - |  6749 | `/*` |
|         - |  6750 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6751 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6752 | ` * mixed, object.` |
|         - |  6753 | ` */` |
|     11496 |  6754 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6755 | `{` |
|         - |  6756 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6757 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6758 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6759 | `	};` |
|         - |  6760 | `	sxu32 i;` |
|     11501 |  6761 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6762 | `		zName++;` |
|       ! 0 |  6763 | `		nName--;` |
|       ! 0 |  6764 | `	}` |
|     11509 |  6765 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11509 |  6766 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11501 |  6767 | `			return 1;` |
|         - |  6768 | `		}` |
|         5 |  6769 | `	}` |
|       ! 0 |  6770 | `	return 0;` |
|      5753 |  6771 | `}` |
|         - |  6772 | `/*` |
|         - |  6773 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6774 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6775 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6776 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6777 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6778 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6779 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6780 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6781 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6782 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6783 | ` */` |
|     11498 |  6784 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6785 | `{` |
|     11503 |  6786 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6787 | ``		return 1; /* bare `object` */`` |
|         - |  6788 | `	}` |
|     11503 |  6789 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6790 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6791 | `	}` |
|     11501 |  6792 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11501 |  6793 | `		return 1;` |
|         - |  6794 | `	}` |
|         - |  6795 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6796 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6797 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6798 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6799 | `	{` |
|         - |  6800 | `		SyBlob sFQN;` |
|         - |  6801 | `		int bOk;` |
|       ! 0 |  6802 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       ! 0 |  6803 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       ! 0 |  6804 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       ! 0 |  6805 | `		SyBlobRelease(&sFQN);` |
|       ! 0 |  6806 | `		return bOk;` |
|         - |  6807 | `	}` |
|      5754 |  6808 | `}` |
|         - |  6809 | `/*` |
|         - |  6810 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6811 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6812 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6813 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6814 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6815 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6816 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6817 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6818 | ` */` |
|     11738 |  6819 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6820 | `{` |
|     11743 |  6821 | `	int bOk = 0;` |
|         - |  6822 | `	sxu32 nLine;` |
|         - |  6823 | `	sxi32 rc;` |
|     11743 |  6824 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       245 |  6825 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6826 | `	}` |
|     11503 |  6827 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6828 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6829 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6830 | `		sxu32 i,j;` |
|       ! 0 |  6831 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6832 | `			int bGroupOk;` |
|       ! 0 |  6833 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6834 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6835 | `			}` |
|       ! 0 |  6836 | `			bGroupOk = 1;` |
|       ! 0 |  6837 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6838 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6839 | `					bGroupOk = 0;` |
|       ! 0 |  6840 | `					break;` |
|         - |  6841 | `				}` |
|       ! 0 |  6842 | `			}` |
|       ! 0 |  6843 | `			bOk = bGroupOk;` |
|       ! 0 |  6844 | `		}` |
|       ! 0 |  6845 | `	}else{` |
|     11503 |  6846 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6847 | `	}` |
|     11503 |  6848 | `	if( bOk ){` |
|     11501 |  6849 | `		return SXRET_OK;` |
|         - |  6850 | `	}` |
|         - |  6851 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6852 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6853 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6854 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6855 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6856 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6857 | `	{` |
|         3 |  6858 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6859 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6860 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6861 | `		}` |
|         3 |  6862 | `		if( sGiven.nByte < 1 ){` |
|         - |  6863 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6864 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6865 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6866 | `			const char *zScalar =` |
|       ! 0 |  6867 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6868 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6869 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6870 | `		}` |
|         3 |  6871 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6872 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6873 | `	}` |
|         3 |  6874 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5874 |  6875 | `}` |
|   2753344 |  6876 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6877 | `{` |
|   2753349 |  6878 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2753349 |  6879 | `	SyToken *pEnd = pGen->pEnd;` |
|   2753349 |  6880 | `	sxi32 iDepth = 0;` |
|   2753349 |  6881 | `	int bStarted = 0;` |
| 133407573 |  6882 | `	while( pIn < pEnd ){` |
| 133407573 |  6883 | `		sxu32 t = pIn->nType;` |
| 133407573 |  6884 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 127433393 |  6885 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 121494011 |  6886 | `		if( t & PH7_TK_KEYWORD ){` |
|   9053405 |  6887 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   9053405 |  6888 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   9041667 |  6889 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6890 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   4503600 |  6891 | `		}` |
| 121447811 |  6892 | `		pIn++;` |
|         5 |  6893 | `	}` |
|   2741611 |  6894 | `	return FALSE;` |
|   1376677 |  6895 | `}` |
|         - |  6896 | `/*` |
|         - |  6897 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6898 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6899 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6900 | ` */` |
|   2753344 |  6901 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6902 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6903 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6904 | `	)` |
|         5 |  6905 | `{` |
|         - |  6906 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6907 | `	GenBlock *pBlock;` |
|         - |  6908 | `	sxu32 nGotoOfft;` |
|         - |  6909 | `	sxi32 rc;` |
|         - |  6910 | `	/* Attach the new function */` |
|   2753349 |  6911 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2753349 |  6912 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6913 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6914 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6915 | `		return SXERR_ABORT;` |
|         - |  6916 | `	}` |
|   2753349 |  6917 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6918 | `	/* Swap bytecode containers */` |
|   2753349 |  6919 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2753349 |  6920 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6921 | `	/* Emit constructor property promotion prologue:` |
|         - |  6922 | `	 *   $this->NAME = $NAME;` |
|         - |  6923 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6924 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6925 | `	{` |
|   2753349 |  6926 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6927 | `		sxu32 i;` |
|   4756165 |  6928 | `		for( i = 0; i < nArg; i++ ){` |
|   2002821 |  6929 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6930 | `			char *zSrc;` |
|         - |  6931 | `			sxu32 nSrc,nName;` |
|         - |  6932 | `			SySet sToken;` |
|         - |  6933 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6934 | `			sxi32 rcPromote;` |
|   2002821 |  6935 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   2002745 |  6936 | `				continue;` |
|         - |  6937 | `			}` |
|         - |  6938 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6939 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6940 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6941 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6942 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        81 |  6943 | `			nName = SyStringLength(&pArg->sName);` |
|        81 |  6944 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        81 |  6945 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        81 |  6946 | `			if( zSrc == 0 ){` |
|       ! 0 |  6947 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6948 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6949 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6950 | `				return SXERR_ABORT;` |
|         - |  6951 | `			}` |
|         - |  6952 | `			{` |
|        81 |  6953 | `				char *z = zSrc;` |
|        81 |  6954 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        81 |  6955 | `				z += sizeof("$this->")-1;` |
|        81 |  6956 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        81 |  6957 | `				z += nName;` |
|        81 |  6958 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        81 |  6959 | `				z += sizeof(" = $")-1;` |
|        81 |  6960 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        81 |  6961 | `				z += nName;` |
|        81 |  6962 | `				*z = 0;` |
|         - |  6963 | `			}` |
|        81 |  6964 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        81 |  6965 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        81 |  6966 | `			pTmpIn = pGen->pIn;` |
|        81 |  6967 | `			pTmpEnd = pGen->pEnd;` |
|        81 |  6968 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        81 |  6969 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        81 |  6970 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        81 |  6971 | `			pGen->pIn = pTmpIn;` |
|        81 |  6972 | `			pGen->pEnd = pTmpEnd;` |
|        81 |  6973 | `			SySetRelease(&sToken);` |
|        81 |  6974 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6975 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6976 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6977 | `				return SXERR_ABORT;` |
|         - |  6978 | `			}` |
|         - |  6979 | `			/* Discard the assignment result — this is a statement expression. */` |
|        81 |  6980 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        43 |  6981 | `		}` |
|         - |  6982 | `	}` |
|         - |  6983 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6984 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6985 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6986 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6987 | `	{` |
|   2753349 |  6988 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2753349 |  6989 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6990 | `		/* Compile the body */` |
|   2753349 |  6991 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2753349 |  6992 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6993 | `	}` |
|         - |  6994 | `	/* Fix exception jumps now the destination is resolved */` |
|   2753349 |  6995 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6996 | `	/* Emit the final return if not yet done */` |
|   2753349 |  6997 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6998 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2753349 |  6999 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  7000 | `		rc = SXERR_ABORT;` |
|       ! 0 |  7001 | `	}` |
|   2753349 |  7002 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  7003 | `	/* Restore the default container */` |
|   2753349 |  7004 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  7005 | `	/* Leave function block */` |
|   2753349 |  7006 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2753349 |  7007 | `	if( rc == SXERR_ABORT ){` |
|         - |  7008 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7009 | `		return SXERR_ABORT;` |
|         - |  7010 | `	}` |
|         - |  7011 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  7012 | `	{` |
|   2753349 |  7013 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  7014 | `		sxu32 i;` |
|  81403285 |  7015 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  78661679 |  7016 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11743 |  7017 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11743 |  7018 | `				break;` |
|         - |  7019 | `			}` |
|  39324973 |  7020 | `		}` |
|         - |  7021 | `	}` |
|   2753349 |  7022 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  7023 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11743 |  7024 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  7025 | `			return SXERR_ABORT;` |
|         - |  7026 | `		}` |
|      5869 |  7027 | `	}` |
|         - |  7028 | `	/* All done, function body compiled */` |
|   2753349 |  7029 | `	return SXRET_OK;` |
|   1376677 |  7030 | `}` |
|         - |  7031 | `/*` |
|         - |  7032 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  7033 | ` * According to the PHP language reference manual.` |
|         - |  7034 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  7035 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  7036 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  7037 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  7038 | ` *  Functions need not be defined before they are referenced.` |
|         - |  7039 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  7040 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  7041 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  7042 | ` *  calls with over 32-64 recursion levels.` |
|         - |  7043 | ` *` |
|         - |  7044 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  7045 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  7046 | ` * on these extension.` |
|         - |  7047 | ` */` |
|         - |  7048 | `/*` |
|         - |  7049 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  7050 | ` */` |
|       590 |  7051 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  7052 | `{` |
|         - |  7053 | `	sxu32 i;` |
|      1655 |  7054 | `	for( i = 0; i < n; i++ ){` |
|      1419 |  7055 | `		int a = zA[i], b = zB[i];` |
|      1419 |  7056 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1419 |  7057 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1419 |  7058 | `		if( a != b ) return a - b;` |
|       535 |  7059 | `	}` |
|       241 |  7060 | `	return 0;` |
|       300 |  7061 | `}` |
|         - |  7062 | `/*` |
|         - |  7063 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  7064 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  7065 | ` * (which are positive bit values stored in sxu32).` |
|         - |  7066 | ` */` |
|         - |  7067 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  7068 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  7069 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  7070 |  |
|         - |  7071 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  7072 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  7073 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  7074 |  |
|         - |  7075 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  7076 | `struct PhlTypeAtom {` |
|         - |  7077 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  7078 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  7079 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  7080 | `	sxu32 nCanon;` |
|         - |  7081 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  7082 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  7083 | `};` |
|         - |  7084 |  |
|         - |  7085 | `/*` |
|         - |  7086 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  7087 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  7088 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  7089 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  7090 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  7091 | ` * already be consumed by the caller.` |
|         - |  7092 | ` */` |
|         - |  7093 | `/*` |
|         - |  7094 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|         - |  7095 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|         - |  7096 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|         - |  7097 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|         - |  7098 | ` * null/void/never are matched before the class path), but the full set is listed` |
|         - |  7099 | ` * so the guard is robust to lexer changes.` |
|         - |  7100 | ` */` |
|     38576 |  7101 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|         5 |  7102 | `{` |
|         - |  7103 | `	static const char *azWords[] = {` |
|         - |  7104 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|         - |  7105 | `		"bool","boolean","int","integer","float","double","string","array",` |
|         - |  7106 | `		"object","self","static","parent"` |
|         - |  7107 | `	};` |
|         - |  7108 | `	sxu32 i;` |
|    808195 |  7109 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    769727 |  7110 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    769727 |  7111 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|       113 |  7112 | `			return 1;` |
|         - |  7113 | `		}` |
|    384812 |  7114 | `	}` |
|     38473 |  7115 | `	return 0;` |
|     19293 |  7116 | `}` |
|    128112 |  7117 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  7118 | `{` |
|    128117 |  7119 | `	SyToken *pIn = pGen->pIn;` |
|    128117 |  7120 | `	int bAbsolute = 0;` |
|    128117 |  7121 | `	SyZero(pOut, sizeof(*pOut));` |
|    128117 |  7122 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    128117 |  7123 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7124 | `		return SXERR_SYNTAX;` |
|         - |  7125 | `	}` |
|         - |  7126 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    128117 |  7127 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        10 |  7128 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|        10 |  7129 | `		pIn++;` |
|        10 |  7130 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7131 | `			return SXERR_SYNTAX;` |
|         - |  7132 | `		}` |
|         4 |  7133 | `	}` |
|    128117 |  7134 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7135 | `		return SXERR_SYNTAX;` |
|         - |  7136 | `	}` |
|    128117 |  7137 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     89303 |  7138 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     89303 |  7139 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11553 |  7140 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     83529 |  7141 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        85 |  7142 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     77715 |  7143 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19801 |  7144 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67777 |  7145 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57785 |  7146 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     28989 |  7147 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  7148 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        81 |  7149 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        28 |  7150 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        50 |  7151 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        17 |  7152 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        35 |  7153 | `			pOut->nType = SXU32_HIGH;` |
|        35 |  7154 | `			pOut->sClass = pIn->sData;` |
|        19 |  7155 | `		}else{` |
|         3 |  7156 | `			return SXERR_SYNTAX;` |
|         - |  7157 | `		}` |
|     89301 |  7158 | `		pIn++;` |
|     44653 |  7159 | `	}else{` |
|         - |  7160 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  7161 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38819 |  7162 | `		SyString *pT = &pIn->sData;` |
|     38819 |  7163 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  7164 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  7165 | `			pIn++;` |
|     38804 |  7166 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       183 |  7167 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       183 |  7168 | `			pIn++;` |
|     38700 |  7169 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        27 |  7170 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        27 |  7171 | `			pIn++;` |
|        16 |  7172 | `		}else{` |
|         - |  7173 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38589 |  7174 | `			SyToken *pFirst = pIn;` |
|     38589 |  7175 | `			SyToken *pLast = pIn;` |
|     38589 |  7176 | `			pOut->nType = SXU32_HIGH;` |
|     38589 |  7177 | `			pOut->sClass = pIn->sData;` |
|     38589 |  7178 | `			pIn++;` |
|     57879 |  7179 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38592 |  7180 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  7181 | `				pLast = &pIn[1];` |
|         3 |  7182 | `				pIn += 2;` |
|         1 |  7183 | `			}` |
|     38589 |  7184 | `			if( pLast != pFirst ){` |
|         3 |  7185 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  7186 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  7187 | `				pOut->sClass.zString = zFirst;` |
|         3 |  7188 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  7189 | `			}` |
|         - |  7190 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|         - |  7191 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|         - |  7192 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|         - |  7193 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|         - |  7194 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|         - |  7195 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|         - |  7196 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|         - |  7197 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|     38589 |  7198 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|         - |  7199 | `				SyBlob sFqn;` |
|     38473 |  7200 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     38473 |  7201 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|     38468 |  7202 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|     38468 |  7203 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|        17 |  7204 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 |  7205 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|        12 |  7206 | `					if( zDup ){` |
|        12 |  7207 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|         5 |  7208 | `					}` |
|         5 |  7209 | `				}` |
|     38473 |  7210 | `				SyBlobRelease(&sFqn);` |
|     19234 |  7211 | `			}` |
|         - |  7212 | `		}` |
|         - |  7213 | `	}` |
|    128115 |  7214 | `	pGen->pIn = pIn;` |
|    128115 |  7215 | `	return SXRET_OK;` |
|     64061 |  7216 | `}` |
|         - |  7217 |  |
|         - |  7218 | `/*` |
|         - |  7219 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  7220 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  7221 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  7222 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  7223 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  7224 | ` */` |
|    127934 |  7225 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  7226 | `{` |
|         - |  7227 | `	int i;` |
|    127939 |  7228 | `	int nNonNull = 0;` |
|    127939 |  7229 | `	int bAnyIntersection = 0;` |
|         - |  7230 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    127939 |  7231 | `	sxu32 nMaxGroup = 0;` |
|   4221827 |  7232 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    256025 |  7233 | `	for( i = 0; i < nAtoms; i++ ){` |
|    128091 |  7234 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    128061 |  7235 | `			nNonNull++;` |
|    128061 |  7236 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    128061 |  7237 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    128061 |  7238 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     64028 |  7239 | `			}` |
|     64028 |  7240 | `		}` |
|     64048 |  7241 | `	}` |
|    255973 |  7242 | `	for( i = 0; i < nAtoms; i++ ){` |
|    128063 |  7243 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  7244 | `			bAnyIntersection = 1;` |
|        29 |  7245 | `			break;` |
|         - |  7246 | `		}` |
|     64022 |  7247 | `	}` |
|    127939 |  7248 | `	if( bAnyIntersection ){` |
|         - |  7249 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  7250 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  7251 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  7252 | `		sxu32 g, nGroups = 0;` |
|        29 |  7253 | `		int bFirstGroup = 1;` |
|        59 |  7254 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  7255 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  7256 | `			int bFirstMember = 1;` |
|         - |  7257 | `			int bWrap;` |
|        35 |  7258 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  7259 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  7260 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  7261 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  7262 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  7263 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  7264 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  7265 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  7266 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  7267 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  7268 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  7269 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  7270 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  7271 | `				}else{` |
|         6 |  7272 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7273 | `				}` |
|        59 |  7274 | `				bFirstMember = 0;` |
|        32 |  7275 | `			}` |
|        35 |  7276 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7277 | `			bFirstGroup = 0;` |
|        20 |  7278 | `		}` |
|        29 |  7279 | `		if( bNullable ){` |
|       ! 0 |  7280 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7281 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7282 | `		}` |
|        85 |  7283 | `		return;` |
|         - |  7284 | `	}` |
|    127915 |  7285 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7286 | `		/* Shorthand: ?T */` |
|       117 |  7287 | `		for( i = 0; i < nAtoms; i++ ){` |
|       117 |  7288 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       117 |  7289 | `			SyBlobAppend(pBlob, "?", 1);` |
|       117 |  7290 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        24 |  7291 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  7292 | `			}else{` |
|        95 |  7293 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7294 | `			}` |
|       117 |  7295 | `			return;` |
|       ! 0 |  7296 | `		}` |
|       ! 0 |  7297 | `	}` |
|         - |  7298 | `	{` |
|    127803 |  7299 | `		int bFirst = 1;` |
|         - |  7300 | `		/* 1) Classes in declaration order */` |
|    255709 |  7301 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127911 |  7302 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38549 |  7303 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38549 |  7304 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38549 |  7305 | `				bFirst = 0;` |
|     19272 |  7306 | `			}` |
|     63958 |  7307 | `		}` |
|         - |  7308 | `		/* 2) Built-ins in canonical order */` |
|         - |  7309 | `		{` |
|         - |  7310 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7311 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7312 | `			int k;` |
|    894591 |  7313 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1444963 |  7314 | `				for( i = 0; i < nAtoms; i++ ){` |
|    767329 |  7315 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     89159 |  7316 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     89159 |  7317 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     89159 |  7318 | `						bFirst = 0;` |
|     89159 |  7319 | `						break;` |
|         - |  7320 | `					}` |
|    339090 |  7321 | `				}` |
|    383399 |  7322 | `			}` |
|         - |  7323 | `		}` |
|         - |  7324 | `		/* 3) null suffix */` |
|    127803 |  7325 | `		if( bNullable ){` |
|        20 |  7326 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        20 |  7327 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7328 | `		}` |
|         - |  7329 | `	}` |
|     63972 |  7330 | `}` |
|         - |  7331 |  |
|         - |  7332 | `/*` |
|         - |  7333 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7334 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7335 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7336 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7337 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7338 | ` * whether it was parenthesized.` |
|         - |  7339 | ` *` |
|         - |  7340 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7341 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7342 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7343 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7344 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7345 | ` */` |
|    128086 |  7346 | `static sxi32 GenStateParsePart(` |
|         - |  7347 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7348 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7349 | `{` |
|         - |  7350 | `	sxi32 rc;` |
|    128091 |  7351 | `	int nMembers = 0;` |
|    128091 |  7352 | `	int bParen = 0;` |
|    128091 |  7353 | `	*pnMembers = 0;` |
|    128091 |  7354 | `	*pbParen = 0;` |
|    128091 |  7355 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7356 | `		bParen = 1;` |
|         9 |  7357 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7358 | `	}` |
|     64043 |  7359 | `	for(;;){` |
|    128117 |  7360 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7361 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7362 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7363 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7364 | `		}` |
|    128117 |  7365 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    128117 |  7366 | `		if( rc != SXRET_OK ){` |
|         3 |  7367 | `			return rc;` |
|         - |  7368 | `		}` |
|    128115 |  7369 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    128115 |  7370 | `		(*pnAtoms)++;` |
|    128115 |  7371 | `		nMembers++;` |
|         - |  7372 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    128115 |  7373 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7374 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7375 | `			if( pNext < pGen->pEnd` |
|        39 |  7376 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7377 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7378 | `				continue;` |
|         - |  7379 | `			}` |
|         4 |  7380 | `		}` |
|    128089 |  7381 | `		break;` |
|       ! 0 |  7382 | `	}` |
|    128089 |  7383 | `	if( bParen ){` |
|         9 |  7384 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7385 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7386 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7387 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7388 | `		}` |
|         9 |  7389 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7390 | `		if( nMembers < 2 ){` |
|       ! 0 |  7391 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7392 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7393 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7394 | `		}` |
|         3 |  7395 | `	}` |
|    128089 |  7396 | `	*pnMembers = nMembers;` |
|    128089 |  7397 | `	*pbParen = bParen;` |
|    128089 |  7398 | `	return SXRET_OK;` |
|     64048 |  7399 | `}` |
|         - |  7400 |  |
|         - |  7401 | `/*` |
|         - |  7402 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7403 | ` *` |
|         - |  7404 | ` * Outputs:` |
|         - |  7405 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7406 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7407 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7408 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7409 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7410 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7411 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7412 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7413 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7414 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7415 | ` *` |
|         - |  7416 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7417 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7418 | ` */` |
|    127950 |  7419 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7420 | `	ph7_gen_state *pGen,` |
|         - |  7421 | `	sxu32 *pnType,` |
|         - |  7422 | `	SyString *pClass,` |
|         - |  7423 | `	SySet *pAlts,` |
|         - |  7424 | `	sxi32 *piTypeFlags,` |
|         - |  7425 | `	SyString *pTypeText,` |
|         - |  7426 | `	int iNullableFlag,` |
|         - |  7427 | `	int iUnionFlag,` |
|         - |  7428 | `	int bAllowVoid,` |
|         - |  7429 | `	sxu32 nLine` |
|         5 |  7430 | `){` |
|         - |  7431 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    127955 |  7432 | `	int nAtoms = 0;` |
|    127955 |  7433 | `	int bShortNullable = 0;` |
|    127955 |  7434 | `	int bExplicitNull = 0;` |
|         - |  7435 | `	sxi32 rc;` |
|    127955 |  7436 | `	*pnType = 0;` |
|    127955 |  7437 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    127955 |  7438 | `	*piTypeFlags = 0;` |
|    127955 |  7439 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7440 |  |
|    127955 |  7441 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7442 | `		return SXRET_OK;` |
|         - |  7443 | `	}` |
|         - |  7444 | ``	/* Optional `?` shorthand prefix */`` |
|    127950 |  7445 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       105 |  7446 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       105 |  7447 | `		bShortNullable = 1;` |
|       105 |  7448 | `		pGen->pIn++;` |
|       105 |  7449 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7450 | `			return SXERR_SYNTAX;` |
|         - |  7451 | `		}` |
|        50 |  7452 | `	}` |
|         - |  7453 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7454 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7455 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7456 | `	{` |
|         - |  7457 | `		int nMembers, bParen;` |
|    127955 |  7458 | `		sxu32 iGroup = 0;` |
|    127955 |  7459 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    127955 |  7460 | `		if( rc != SXRET_OK ){` |
|         4 |  7461 | `			return rc;` |
|         - |  7462 | `		}` |
|         - |  7463 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7464 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7465 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7466 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7467 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    192131 |  7468 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    128162 |  7469 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7470 | `			if( bShortNullable ){` |
|         - |  7471 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7472 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7473 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7474 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7475 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7476 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7477 | `			}` |
|       141 |  7478 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7479 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7480 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7481 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7482 | `			}` |
|       141 |  7483 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7484 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7485 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7486 | `				return rc;` |
|         - |  7487 | `			}` |
|         5 |  7488 | `		}` |
|    127951 |  7489 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7490 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7491 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7492 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7493 | `		}` |
|         - |  7494 | `	}` |
|         - |  7495 | `	/* Validation pass.` |
|         - |  7496 | `	 *` |
|         - |  7497 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7498 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7499 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7500 | `	 */` |
|         - |  7501 | `	{` |
|         - |  7502 | `		int i, j;` |
|    127951 |  7503 | `		int bHasNonNull = 0;` |
|    127951 |  7504 | `		int bAnyIntersection = 0;` |
|         - |  7505 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7506 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7507 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4222223 |  7508 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    256059 |  7509 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128113 |  7510 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     64059 |  7511 | `		}` |
|    256003 |  7512 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128083 |  7513 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     64031 |  7514 | `		}` |
|         - |  7515 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7516 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    127951 |  7517 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7518 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7519 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7520 | `			return SXERR_SYNTAX;` |
|         - |  7521 | `		}` |
|    256045 |  7522 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7523 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7524 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7525 | ``			 * `true`/`false` in an intersection). */`` |
|    128111 |  7526 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7527 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7528 | `				if( bClassLike ){` |
|        53 |  7529 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7530 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7531 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7532 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7533 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7534 | `						bClassLike = 0;` |
|       ! 0 |  7535 | `					}` |
|        24 |  7536 | `				}` |
|        55 |  7537 | `				if( !bClassLike ){` |
|         - |  7538 | `					const char *zName; sxu32 nName;` |
|         3 |  7539 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7540 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7541 | `					}else{` |
|         3 |  7542 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7543 | `					}` |
|         4 |  7544 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7545 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7546 | `						(int)nName, zName);` |
|         3 |  7547 | `					return SXERR_SYNTAX;` |
|         - |  7548 | `				}` |
|        24 |  7549 | `			}` |
|    128109 |  7550 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       183 |  7551 | `				if( nAtoms > 1 ){` |
|         3 |  7552 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7553 | `						"Void can only be used as a standalone type");` |
|         3 |  7554 | `					return SXERR_SYNTAX;` |
|         - |  7555 | `				}` |
|       181 |  7556 | `				if( !bAllowVoid ){` |
|       ! 0 |  7557 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7558 | `						"void cannot be used here");` |
|       ! 0 |  7559 | `					return SXERR_SYNTAX;` |
|         - |  7560 | `				}` |
|       181 |  7561 | `				if( bShortNullable ){` |
|       ! 0 |  7562 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7563 | `						"Void type cannot be nullable");` |
|       ! 0 |  7564 | `					return SXERR_SYNTAX;` |
|         - |  7565 | `				}` |
|        88 |  7566 | `			}` |
|    128107 |  7567 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7568 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7569 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7570 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7571 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        27 |  7572 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7573 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7574 | `					 * same as any other non-standalone use. */` |
|         6 |  7575 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7576 | `						"never can only be used as a standalone type");` |
|         6 |  7577 | `					return SXERR_SYNTAX;` |
|         - |  7578 | `				}` |
|        21 |  7579 | `				if( !bAllowVoid ){` |
|         - |  7580 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7581 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7582 | `						"never cannot be used as a parameter type");` |
|         3 |  7583 | `					return SXERR_SYNTAX;` |
|         - |  7584 | `				}` |
|         8 |  7585 | `			}` |
|    128101 |  7586 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7587 | `				bExplicitNull = 1;` |
|        19 |  7588 | `			}else{` |
|    128071 |  7589 | `				bHasNonNull = 1;` |
|         - |  7590 | `			}` |
|         - |  7591 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7592 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7593 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7594 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7595 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    128301 |  7596 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7597 | `				int bDup = 0;` |
|       207 |  7598 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7599 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7600 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7601 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7602 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7603 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7604 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7605 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7606 | `								aAtoms[j].sClass.zString,` |
|        34 |  7607 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7608 | `							bDup = 1;` |
|       ! 0 |  7609 | `						}` |
|        27 |  7610 | `					}else{` |
|         3 |  7611 | `						bDup = 1;` |
|         - |  7612 | `					}` |
|        23 |  7613 | `				}` |
|       195 |  7614 | `				if( bDup ){` |
|         - |  7615 | `					const char *zName;` |
|         - |  7616 | `					sxu32 nName;` |
|         3 |  7617 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7618 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7619 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7620 | `					}else{` |
|         3 |  7621 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7622 | `						nName = aAtoms[i].nCanon;` |
|         - |  7623 | `					}` |
|         4 |  7624 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7625 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7626 | `					return SXERR_SYNTAX;` |
|         - |  7627 | `				}` |
|        99 |  7628 | `			}` |
|     64052 |  7629 | `		}` |
|    127939 |  7630 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7631 | `			if( bShortNullable ){` |
|         - |  7632 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7633 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7634 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7635 | `				return SXERR_SYNTAX;` |
|         - |  7636 | `			}` |
|         - |  7637 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7638 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7639 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7640 | `			 * atom, so set it here. */` |
|         7 |  7641 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7642 | `		}` |
|         - |  7643 | `	}` |
|         - |  7644 | `	/* Compute nullability flag */` |
|    127939 |  7645 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       133 |  7646 | `		*piTypeFlags \|= iNullableFlag;` |
|        64 |  7647 | `	}` |
|         - |  7648 | `	/* Build canonical type text */` |
|    127939 |  7649 | `	if( pTypeText ){` |
|         - |  7650 | `		SyBlob sBlob;` |
|    127939 |  7651 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    191857 |  7652 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     63967 |  7653 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    127939 |  7654 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    191618 |  7655 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    127742 |  7656 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    127747 |  7657 | `			if( zDup ){` |
|    127747 |  7658 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     63871 |  7659 | `			}` |
|     63871 |  7660 | `		}` |
|    127939 |  7661 | `		SyBlobRelease(&sBlob);` |
|     63967 |  7662 | `	}` |
|         - |  7663 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7664 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7665 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7666 | `	{` |
|    127939 |  7667 | `		int nNonNull = 0;` |
|    127939 |  7668 | `		int iNonNullIdx = -1;` |
|         - |  7669 | `		int i;` |
|    256025 |  7670 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128091 |  7671 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    128061 |  7672 | `				nNonNull++;` |
|    128061 |  7673 | `				iNonNullIdx = i;` |
|     64028 |  7674 | `			}` |
|     64048 |  7675 | `		}` |
|    127939 |  7676 | `		if( nNonNull <= 1 ){` |
|         - |  7677 | `			/* Fast path: store as single type. */` |
|    127833 |  7678 | `			if( iNonNullIdx >= 0 ){` |
|    127827 |  7679 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    127827 |  7680 | `				if( pA->nType == SXU32_HIGH ){` |
|     57788 |  7681 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19261 |  7682 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38527 |  7683 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38527 |  7684 | `					*pnType = SXU32_HIGH;` |
|     38527 |  7685 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    108566 |  7686 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       181 |  7687 | `					*pnType = MEMOBJ_VOID;` |
|     89217 |  7688 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7689 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7690 | `				}else{` |
|     89113 |  7691 | `					*pnType = pA->nType;` |
|         - |  7692 | `				}` |
|     63911 |  7693 | `			}` |
|     63919 |  7694 | `		}else{` |
|         - |  7695 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7696 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7697 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7698 | `				ph7_type_alt sAlt;` |
|       249 |  7699 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7700 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7701 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7702 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7703 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7704 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7705 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7706 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7707 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7708 | `				}else{` |
|       145 |  7709 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7710 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7711 | `				}` |
|       239 |  7712 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7713 | `			}` |
|         - |  7714 | `		}` |
|         - |  7715 | `	}` |
|    127939 |  7716 | `	return SXRET_OK;` |
|     63980 |  7717 | `}` |
|         - |  7718 |  |
|         - |  7719 | `/*` |
|         - |  7720 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7721 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7722 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7723 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7724 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7725 | `` *          and union types `: T\|U`.`` |
|         - |  7726 | ` */` |
|   2891522 |  7727 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7728 | `{` |
|   2891527 |  7729 | `	sxi32 iFlags = 0;` |
|         - |  7730 | `	sxi32 rc;` |
|         - |  7731 | `	sxu32 nLine;` |
|   2891527 |  7732 | `	pFunc->nReturnType = 0;` |
|   2891527 |  7733 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2891527 |  7734 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7735 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7736 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7737 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7738 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7739 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2891527 |  7740 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2891527 |  7741 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2891527 |  7742 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2879235 |  7743 | `		return SXRET_OK;` |
|         - |  7744 | `	}` |
|     12297 |  7745 | `	pGen->pIn++; /* Skip ':' */` |
|     12297 |  7746 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7747 | `		return SXRET_OK;` |
|         - |  7748 | `	}` |
|     12297 |  7749 | `	nLine = pGen->pIn->nLine;` |
|     12297 |  7750 | `	rc = GenStateParseUnionTypeDecl(` |
|      6146 |  7751 | `		pGen,` |
|      6146 |  7752 | `		&pFunc->nReturnType,` |
|      6146 |  7753 | `		&pFunc->sReturnClass,` |
|      6146 |  7754 | `		&pFunc->aReturnUnion,` |
|         - |  7755 | `		&iFlags,` |
|      6146 |  7756 | `		&pFunc->sReturnTypeName,` |
|         - |  7757 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7758 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7759 | `		/* iUnionFlag */ 0,` |
|         - |  7760 | `		/* bAllowVoid */ 1,` |
|      6146 |  7761 | `		nLine);` |
|     12297 |  7762 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7763 | `		return SXERR_ABORT;` |
|         - |  7764 | `	}` |
|     12297 |  7765 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7766 | `		/* Error already reported */` |
|       ! 0 |  7767 | `		return SXERR_SYNTAX;` |
|         - |  7768 | `	}` |
|     12297 |  7769 | `	if( rc == SXERR_SYNTAX ){` |
|         9 |  7770 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 |  7771 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7772 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7773 | `				&pGen->pIn->sData);` |
|         6 |  7774 | `		}else{` |
|       ! 0 |  7775 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7776 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7777 | `		}` |
|         9 |  7778 | `		return SXERR_SYNTAX;` |
|         - |  7779 | `	}` |
|     12291 |  7780 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12291 |  7781 | `	return SXRET_OK;` |
|   1445766 |  7782 | `}` |
|         - |  7783 |  |
|    491590 |  7784 | `static sxi32 GenStateCompileFunc(` |
|         - |  7785 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7786 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7787 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7788 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7789 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7790 | `	)` |
|         5 |  7791 | `{` |
|         - |  7792 | `	ph7_vm_func *pFunc;` |
|         - |  7793 | `	SyToken *pEnd;` |
|         - |  7794 | `	sxu32 nLine;` |
|         - |  7795 | `	char *zName;` |
|         - |  7796 | `	sxi32 rc;` |
|         - |  7797 | `	/* Extract line number */` |
|    491595 |  7798 | `	nLine = pGen->pIn->nLine;` |
|         - |  7799 | `	/* Jump the left parenthesis '(' */` |
|    491595 |  7800 | `	pGen->pIn++;` |
|         - |  7801 | `	/* Delimit the function signature */` |
|    491595 |  7802 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    491595 |  7803 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7804 | `		/* Syntax error */` |
|         9 |  7805 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7806 | `		(void)pName;` |
|         9 |  7807 | `		if( rc == SXERR_ABORT ){` |
|         - |  7808 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7809 | `			return SXERR_ABORT;` |
|         - |  7810 | `		}` |
|         9 |  7811 | `		pGen->pIn = pGen->pEnd;` |
|         9 |  7812 | `		return SXRET_OK;` |
|         - |  7813 | `	}` |
|         - |  7814 | `	/* Create the function state */` |
|    491589 |  7815 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    491589 |  7816 | `	if( pFunc == 0 ){` |
|       ! 0 |  7817 | `		goto OutOfMem;` |
|         - |  7818 | `	}` |
|         - |  7819 | `	/* Build the function name, prepending namespace if active */` |
|    491597 |  7820 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7821 | `		SyBlob sFQN;` |
|         - |  7822 | `		sxu32 nLen;` |
|        18 |  7823 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        18 |  7824 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        18 |  7825 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        18 |  7826 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        18 |  7827 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        18 |  7828 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        18 |  7829 | `		SyBlobRelease(&sFQN);` |
|        18 |  7830 | `		if( zName == 0 ){` |
|       ! 0 |  7831 | `			goto OutOfMem;` |
|         - |  7832 | `		}` |
|        18 |  7833 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        10 |  7834 | `	}else{` |
|    491573 |  7835 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    491573 |  7836 | `		if( zName == 0 ){` |
|       ! 0 |  7837 | `			goto OutOfMem;` |
|         - |  7838 | `		}` |
|    491573 |  7839 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7840 | `	}` |
|         - |  7841 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7842 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    491589 |  7843 | `	pFunc->nLine = nLine;` |
|    491589 |  7844 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    491589 |  7845 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7846 | `		return SXERR_ABORT;` |
|         - |  7847 | `	}` |
|    491589 |  7848 | `	if( pGen->pIn < pEnd ){` |
|         - |  7849 | `		/* Collect function arguments */` |
|    425619 |  7850 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    425619 |  7851 | `		if( rc == SXERR_ABORT ){` |
|         - |  7852 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7853 | `			return SXERR_ABORT;` |
|         - |  7854 | `		}` |
|    212807 |  7855 | `	}` |
|         - |  7856 | `	/* Point past ')' and parse optional return type ': type' */` |
|    491589 |  7857 | `	pGen->pIn = &pEnd[1];` |
|         - |  7858 | `	{` |
|    491589 |  7859 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    491589 |  7860 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7861 | `			return SXERR_ABORT;` |
|    491589 |  7862 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 |  7863 | `			return SXERR_SYNTAX;` |
|         - |  7864 | `		}` |
|         - |  7865 | `	}` |
|    491583 |  7866 | `	if( bHandleClosure ){` |
|         - |  7867 | `		ph7_vm_func_closure_env sEnv;` |
|       581 |  7868 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       576 |  7869 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       337 |  7870 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        93 |  7871 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7872 | `				/* Closure,record environment variable */` |
|        93 |  7873 | `				pGen->pIn++;` |
|        93 |  7874 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7875 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7876 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7877 | `						return SXERR_ABORT;` |
|         - |  7878 | `					}` |
|       ! 0 |  7879 | `				}` |
|        93 |  7880 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7881 | `				/* Compile until we hit the first closing parenthesis */` |
|       191 |  7882 | `				while( pGen->pIn < pGen->pEnd ){` |
|       191 |  7883 | `					int iFlagsLocal = 0;` |
|       191 |  7884 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        93 |  7885 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        93 |  7886 | `						break;` |
|         - |  7887 | `					}` |
|       103 |  7888 | `					nLineLocal = pGen->pIn->nLine;` |
|       103 |  7889 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7890 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7891 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7892 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7893 | `						pGen->pIn++;` |
|        27 |  7894 | `					}` |
|        98 |  7895 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       103 |  7896 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7897 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7898 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7899 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7900 | `								return SXERR_ABORT;` |
|         - |  7901 | `							}` |
|         - |  7902 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7903 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7904 | `								pGen->pIn++;` |
|       ! 0 |  7905 | `							}` |
|       ! 0 |  7906 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7907 | `								pGen->pIn++;` |
|       ! 0 |  7908 | `							}` |
|       ! 0 |  7909 | `							break;` |
|         - |  7910 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7911 | `					}else{` |
|         - |  7912 | `						SyString *pNameLocal;` |
|         - |  7913 | `						char *zDup;` |
|         - |  7914 | `						/* Duplicate variable name */` |
|       103 |  7915 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       103 |  7916 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       103 |  7917 | `						if( zDup ){` |
|         - |  7918 | `							/* Zero the structure */` |
|       103 |  7919 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       103 |  7920 | `							sEnv.iFlags = iFlagsLocal;` |
|       103 |  7921 | `							sEnv.nIdx = SXU32_HIGH;` |
|       103 |  7922 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       103 |  7923 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       118 |  7924 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7925 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7926 | `									got_this = 1;` |
|       ! 0 |  7927 | `							}` |
|         - |  7928 | `							/* Save imported variable */` |
|       103 |  7929 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        54 |  7930 | `						}else{` |
|       ! 0 |  7931 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7932 | `							 return SXERR_ABORT;` |
|         - |  7933 | `						}` |
|         - |  7934 | `					}` |
|       103 |  7935 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       115 |  7936 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7937 | `						/* Ignore trailing commas */` |
|        13 |  7938 | `						pGen->pIn++;` |
|         1 |  7939 | `					}` |
|         5 |  7940 | `				}` |
|         - |  7941 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7942 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7943 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7944 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7945 | `				 * legacy pre-use position. */` |
|        93 |  7946 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7947 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7948 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7949 | `						return SXERR_ABORT;` |
|         7 |  7950 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7951 | `						return SXERR_SYNTAX;` |
|         - |  7952 | `					}` |
|         3 |  7953 | `				}` |
|        44 |  7954 | `		}` |
|       581 |  7955 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7956 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7957 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7958 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7959 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7960 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7961 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7962 | `			 * closure never binds $this (php). */` |
|       571 |  7963 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       571 |  7964 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       571 |  7965 | `			sEnv.nIdx = SXU32_HIGH;` |
|       571 |  7966 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       571 |  7967 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       571 |  7968 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       283 |  7969 | `		}` |
|       581 |  7970 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7971 | `			/* Mark as closure */` |
|       573 |  7972 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       284 |  7973 | `		}` |
|       288 |  7974 | `	}` |
|         - |  7975 | `	/* Compile the body */` |
|    491583 |  7976 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    491583 |  7977 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7978 | `		return SXERR_ABORT;` |
|         - |  7979 | `	}` |
|         - |  7980 | `	/* The cursor sits just past the body's closing brace */` |
|    491583 |  7981 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    491583 |  7982 | `	if( ppFunc ){` |
|    491583 |  7983 | `		*ppFunc = pFunc;` |
|    245789 |  7984 | `	}` |
|    491583 |  7985 | `	rc = SXRET_OK;` |
|    491583 |  7986 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7987 | `		/* Finally register the function */` |
|    491015 |  7988 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    245505 |  7989 | `	}` |
|    491583 |  7990 | `	if( rc == SXRET_OK ){` |
|    491583 |  7991 | `		return SXRET_OK;` |
|         - |  7992 | `	}` |
|         - |  7993 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7994 | `OutOfMem:` |
|         - |  7995 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7996 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7997 | `	 */` |
|       ! 0 |  7998 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7999 | `	return SXERR_ABORT;` |
|    245800 |  8000 | `}` |
|         - |  8001 | `/*` |
|         - |  8002 | ` * Compile a standard PHP function.` |
|         - |  8003 | ` *  Refer to the block-comment above for more information.` |
|         - |  8004 | ` */` |
|    491022 |  8005 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  8006 | `{` |
|         - |  8007 | `	SyString *pName;` |
|         - |  8008 | `	sxi32 iFlags;` |
|         - |  8009 | `	sxu32 nKwLine;` |
|         - |  8010 | `	sxu32 nLine;` |
|         - |  8011 | `	sxi32 rc;` |
|         - |  8012 |  |
|    491027 |  8013 | `	nLine = pGen->pIn->nLine;` |
|    491027 |  8014 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    491027 |  8015 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    491027 |  8016 | `	iFlags = 0;` |
|    491027 |  8017 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8018 | `		/* Return by reference,remember that */` |
|        12 |  8019 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8020 | `		/* Jump the '&' token */` |
|        12 |  8021 | `		pGen->pIn++;` |
|         5 |  8022 | `	}` |
|    491027 |  8023 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8024 | `		/* Invalid function name */` |
|         7 |  8025 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         7 |  8026 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8027 | `			return SXERR_ABORT;` |
|         - |  8028 | `		}` |
|         - |  8029 | `		/* Sychronize with the next semi-colon or braces*/` |
|        21 |  8030 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        15 |  8031 | `			pGen->pIn++;` |
|         1 |  8032 | `		}` |
|         7 |  8033 | `		return SXRET_OK;` |
|         - |  8034 | `	}` |
|    491021 |  8035 | `	pName = &pGen->pIn->sData;` |
|    491021 |  8036 | `	nLine = pGen->pIn->nLine;` |
|         - |  8037 | `	/* Jump the function name */` |
|    491021 |  8038 | `	pGen->pIn++;` |
|    491021 |  8039 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8040 | `		/* Syntax error */` |
|         3 |  8041 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  8042 | `		if( rc == SXERR_ABORT ){` |
|         - |  8043 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8044 | `			return SXERR_ABORT;` |
|         - |  8045 | `		}` |
|         - |  8046 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  8047 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  8048 | `			pGen->pIn++;` |
|       ! 0 |  8049 | `		}` |
|         3 |  8050 | `		return SXRET_OK;` |
|         - |  8051 | `	}` |
|         - |  8052 | `	/* Compile function body */` |
|         - |  8053 | `	{` |
|    491019 |  8054 | `		ph7_vm_func *pFuncState = 0;` |
|    491019 |  8055 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    491019 |  8056 | `		if( pFuncState ){` |
|         - |  8057 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    491007 |  8058 | `			pFuncState->nLine = nKwLine;` |
|    245501 |  8059 | `		}` |
|         - |  8060 | `	}` |
|    491019 |  8061 | `	return rc;` |
|    245516 |  8062 | `}` |
|         - |  8063 | `/*` |
|         - |  8064 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  8065 | ` * According to the PHP language reference manual` |
|         - |  8066 | ` *  Visibility:` |
|         - |  8067 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  8068 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  8069 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  8070 | ` *  Members declared protected can be accessed only within the class` |
|         - |  8071 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  8072 | ` *  may only be accessed by the class that defines the member.` |
|         - |  8073 | ` */` |
|   3150166 |  8074 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  8075 | `{` |
|   3150171 |  8076 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    256423 |  8077 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2893753 |  8078 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    191301 |  8079 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  8080 | `	}` |
|         - |  8081 | `	/* Assume public by default */` |
|   2702457 |  8082 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1575088 |  8083 | `}` |
|         - |  8084 | `/*` |
|         - |  8085 | ` * Compile a class constant.` |
|         - |  8086 | ` * According to the PHP language reference manual` |
|         - |  8087 | ` *  Class Constants` |
|         - |  8088 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  8089 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  8090 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  8091 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  8092 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  8093 | ` *   It's also possible for interfaces to have constants.` |
|         - |  8094 | ` * Symisc eXtension.` |
|         - |  8095 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  8096 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8097 | ` *  Example:` |
|         - |  8098 | ` *   class Test{` |
|         - |  8099 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8100 | ` *   };` |
|         - |  8101 | ` *   var_dump(TEST::MyConst);` |
|         - |  8102 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8103 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8104 | ` */` |
|         - |  8105 | `/*` |
|         - |  8106 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  8107 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  8108 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  8109 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  8110 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  8111 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  8112 | ` */` |
|    290810 |  8113 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  8114 | `{` |
|         - |  8115 | `	SyToken *p0, *p1;` |
|    290815 |  8116 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8117 | `		return 0;` |
|         - |  8118 | `	}` |
|    290815 |  8119 | `	p0 = pGen->pIn;` |
|         - |  8120 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    290815 |  8121 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  8122 | `		return 1;` |
|         - |  8123 | `	}` |
|    290815 |  8124 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  8125 | `		return 1;` |
|         - |  8126 | `	}` |
|         - |  8127 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  8128 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  8129 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    290811 |  8130 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    290811 |  8131 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    290811 |  8132 | `		if( p1 ){` |
|    290811 |  8133 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  8134 | `				return 1;` |
|         - |  8135 | `			}` |
|    290781 |  8136 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  8137 | `				return 1;` |
|         - |  8138 | `			}` |
|    145386 |  8139 | `		}` |
|    145386 |  8140 | `	}` |
|    290777 |  8141 | `	return 0;` |
|    145410 |  8142 | `}` |
|         - |  8143 | `/*` |
|         - |  8144 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  8145 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  8146 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  8147 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  8148 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  8149 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  8150 | ` * Peek only; never consumes tokens.` |
|         - |  8151 | ` */` |
|        24 |  8152 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  8153 | `{` |
|        28 |  8154 | `	SyToken *p = pGen->pIn;` |
|        39 |  8155 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  8156 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  8157 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  8158 | `	}` |
|        28 |  8159 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  8160 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  8161 | `	}` |
|         6 |  8162 | `	p++;` |
|         - |  8163 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  8164 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  8165 | `}` |
|         - |  8166 | `/*` |
|         - |  8167 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  8168 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  8169 | `` * `$o->new`), not a `new` expression.`` |
|         - |  8170 | ` */` |
|       110 |  8171 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  8172 | `{` |
|         - |  8173 | `	sxi32 iOp;` |
|       114 |  8174 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  8175 | `		return 0;` |
|         - |  8176 | `	}` |
|       104 |  8177 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  8178 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  8179 | `}` |
|         - |  8180 | `/*` |
|         - |  8181 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  8182 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  8183 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  8184 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  8185 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  8186 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  8187 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  8188 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  8189 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  8190 | ` *` |
|         - |  8191 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  8192 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  8193 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  8194 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  8195 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  8196 | ` */` |
|    628040 |  8197 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  8198 | `{` |
|    628045 |  8199 | `	SyToken *p = pGen->pIn;` |
|    628045 |  8200 | `	int iDepth = 0;` |
|   1659279 |  8201 | `	while( p < pGen->pEnd ){` |
|   1659279 |  8202 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    627993 |  8203 | `			break; /* end of this initializer */` |
|         - |  8204 | `		}` |
|   1031286 |  8205 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    519489 |  8206 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7682 |  8207 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  8208 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  8209 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  8210 | `			 * expression. */` |
|         3 |  8211 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  8212 | `			p++;` |
|         3 |  8213 | `			if( bArrow ){` |
|         - |  8214 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  8215 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  8216 | `				int iBase = iDepth;` |
|        17 |  8217 | `				while( p < pGen->pEnd ){` |
|        17 |  8218 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  8219 | `						iDepth++;` |
|        15 |  8220 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  8221 | `						if( iDepth <= iBase ){` |
|       ! 0 |  8222 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  8223 | `						}` |
|         5 |  8224 | `						iDepth--;` |
|        11 |  8225 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  8226 | `						break;` |
|         - |  8227 | `					}` |
|        15 |  8228 | `					p++;` |
|         1 |  8229 | `				}` |
|         2 |  8230 | `			}else{` |
|         - |  8231 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  8232 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  8233 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  8234 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  8235 | `				int iLocal = 0;` |
|       ! 0 |  8236 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  8237 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  8238 | `						break; /* body brace */` |
|         - |  8239 | `					}` |
|       ! 0 |  8240 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  8241 | `						iLocal++;` |
|       ! 0 |  8242 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  8243 | `						if( iLocal > 0 ){` |
|       ! 0 |  8244 | `							iLocal--;` |
|       ! 0 |  8245 | `						}` |
|       ! 0 |  8246 | `					}` |
|       ! 0 |  8247 | `					p++;` |
|       ! 0 |  8248 | `				}` |
|       ! 0 |  8249 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  8250 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  8251 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  8252 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  8253 | `							iBrace++;` |
|       ! 0 |  8254 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  8255 | `							iBrace--;` |
|       ! 0 |  8256 | `							if( iBrace == 0 ){` |
|       ! 0 |  8257 | `								p++;` |
|       ! 0 |  8258 | `								break;` |
|         - |  8259 | `							}` |
|       ! 0 |  8260 | `						}` |
|       ! 0 |  8261 | `						p++;` |
|       ! 0 |  8262 | `					}` |
|       ! 0 |  8263 | `				}` |
|         - |  8264 | `			}` |
|         3 |  8265 | `			continue;` |
|         - |  8266 | `		}` |
|   1031289 |  8267 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  8268 | `			if( iDepth == 0 ){` |
|         - |  8269 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  8270 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  8271 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  8272 | `				 * is legal — don't scan into it. */` |
|        45 |  8273 | `				break;` |
|         - |  8274 | `			}` |
|       ! 0 |  8275 | `			iDepth++;` |
|   1031245 |  8276 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42161 |  8277 | `			iDepth++;` |
|   1010167 |  8278 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     42159 |  8279 | `			if( iDepth > 0 ){` |
|     42159 |  8280 | `				iDepth--;` |
|     21077 |  8281 | `			}` |
|    968012 |  8282 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    347161 |  8283 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8284 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8285 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8286 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  8287 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  8288 | `				return 1;` |
|         - |  8289 | `			}` |
|       ! 0 |  8290 | `		}` |
|   1031237 |  8291 | `		p++;` |
|         5 |  8292 | `	}` |
|    628037 |  8293 | `	return 0;` |
|    314025 |  8294 | `}` |
|         - |  8295 | `/*` |
|         - |  8296 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8297 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8298 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8299 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8300 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8301 | ` * share the same backing.` |
|         - |  8302 | ` */` |
|       372 |  8303 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8304 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8305 | `{` |
|       377 |  8306 | `	pAttr->nType = nType;` |
|       377 |  8307 | `	pAttr->sClass = *pClass;` |
|       377 |  8308 | `	pAttr->sTypeName = *pTypeName;` |
|       377 |  8309 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8310 | `		sxu32 i;` |
|        73 |  8311 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8312 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8313 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8314 | `		}` |
|        11 |  8315 | `	}` |
|       377 |  8316 | `}` |
|    290810 |  8317 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8318 | `{` |
|    290815 |  8319 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8320 | `	SySet *pInstrContainer;` |
|         - |  8321 | `	ph7_class_attr *pCons;` |
|         - |  8322 | `	SyString *pName;` |
|         - |  8323 | `	sxi32 rc;` |
|    290815 |  8324 | `	sxu32 nType = 0;` |
|         - |  8325 | `	SyString sTypeClass;` |
|         - |  8326 | `	SyString sTypeText;` |
|         - |  8327 | `	SySet aUnionAlts;` |
|    290815 |  8328 | `	sxi32 iTypeFlags = 0;` |
|    290815 |  8329 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    290815 |  8330 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    290815 |  8331 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8332 | `	/* Extract visibility level */` |
|    290815 |  8333 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8334 | `	/* Mark as constant */` |
|    290815 |  8335 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    290815 |  8336 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8337 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8338 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    290834 |  8339 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8340 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8341 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8342 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8343 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8344 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8345 | `		 * and success paths release. */` |
|        42 |  8346 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8347 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8348 | `			goto Synchronize;` |
|        42 |  8349 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8350 | `			return SXERR_ABORT;` |
|        42 |  8351 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8352 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8353 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8354 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8355 | `				return SXERR_ABORT;` |
|         - |  8356 | `			}` |
|       ! 0 |  8357 | `			goto Synchronize;` |
|         - |  8358 | `		}` |
|        42 |  8359 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8360 | `	}` |
|    145405 |  8361 | `loop:` |
|    290817 |  8362 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8363 | `		/* Invalid constant name */` |
|       ! 0 |  8364 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8365 | `		if( rc == SXERR_ABORT ){` |
|         - |  8366 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8367 | `			return SXERR_ABORT;` |
|         - |  8368 | `		}` |
|       ! 0 |  8369 | `		goto Synchronize;` |
|         - |  8370 | `	}` |
|         - |  8371 | `	/* Peek constant name */` |
|    290817 |  8372 | `	pName = &pGen->pIn->sData;` |
|         - |  8373 | `	/* Make sure the constant name isn't reserved */` |
|    290817 |  8374 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8375 | `		/* Reserved constant name */` |
|       ! 0 |  8376 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8377 | `		if( rc == SXERR_ABORT ){` |
|         - |  8378 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8379 | `			return SXERR_ABORT;` |
|         - |  8380 | `		}` |
|       ! 0 |  8381 | `		goto Synchronize;` |
|         - |  8382 | `	}` |
|         - |  8383 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    290817 |  8384 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8385 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8386 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8387 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8388 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8389 | `			return SXERR_ABORT;` |
|        42 |  8390 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8391 | `			goto Synchronize;` |
|         - |  8392 | `		}` |
|        18 |  8393 | `	}` |
|         - |  8394 | `	/* Advance the stream cursor */` |
|    290815 |  8395 | `	pGen->pIn++;` |
|    290815 |  8396 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8397 | `		/* Invalid declaration */` |
|       ! 0 |  8398 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8399 | `		if( rc == SXERR_ABORT ){` |
|         - |  8400 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8401 | `			return SXERR_ABORT;` |
|         - |  8402 | `		}` |
|       ! 0 |  8403 | `		goto Synchronize;` |
|         - |  8404 | `	}` |
|    290815 |  8405 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8406 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8407 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8408 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8409 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    290810 |  8410 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8411 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8412 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8413 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8414 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8415 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8416 | `			return SXERR_ABORT;` |
|         - |  8417 | `		}` |
|         6 |  8418 | `		goto Synchronize;` |
|         - |  8419 | `	}` |
|         - |  8420 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8421 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8422 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    290811 |  8423 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8424 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8425 | `			"New expressions are not supported in this context");` |
|         5 |  8426 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8427 | `			return SXERR_ABORT;` |
|         - |  8428 | `		}` |
|         5 |  8429 | `		goto Synchronize;` |
|         - |  8430 | `	}` |
|         - |  8431 | `	/* Allocate a new class attribute */` |
|    290807 |  8432 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    290807 |  8433 | `	if( pCons ){` |
|    290807 |  8434 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    290807 |  8435 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8436 | `			return SXERR_ABORT;` |
|         - |  8437 | `		}` |
|    145401 |  8438 | `	}` |
|    290807 |  8439 | `	if( pCons == 0 ){` |
|       ! 0 |  8440 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8441 | `		return SXERR_ABORT;` |
|         - |  8442 | `	}` |
|    290807 |  8443 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8444 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8445 | `	}` |
|         - |  8446 | `	/* Swap bytecode container */` |
|    290807 |  8447 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    290807 |  8448 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8449 | `	/* Compile constant value.` |
|         - |  8450 | `	 */` |
|    290807 |  8451 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    290807 |  8452 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8453 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8454 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8455 | `			return SXERR_ABORT;` |
|         - |  8456 | `		}` |
|         1 |  8457 | `	}` |
|         - |  8458 | `	/* Emit the done instruction */` |
|    290807 |  8459 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    290807 |  8460 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    290807 |  8461 | `	if( rc == SXERR_ABORT ){` |
|         - |  8462 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8463 | `		return SXERR_ABORT;` |
|         - |  8464 | `	}` |
|         - |  8465 | `	/* All done,install the constant */` |
|    290807 |  8466 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    290807 |  8467 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8468 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8469 | `		return SXERR_ABORT;` |
|         - |  8470 | `	}` |
|    290807 |  8471 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8472 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8473 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8474 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8475 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8476 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8477 | `				pTok--;` |
|       ! 0 |  8478 | `			}` |
|       ! 0 |  8479 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8480 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8481 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8482 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8483 | `				return SXERR_ABORT;` |
|         - |  8484 | `			}` |
|       ! 0 |  8485 | `		}else{` |
|         3 |  8486 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8487 | `				goto loop;` |
|         - |  8488 | `			}` |
|         - |  8489 | `		}` |
|       ! 0 |  8490 | `	}` |
|    290805 |  8491 | `	SySetRelease(&aUnionAlts);` |
|    290805 |  8492 | `	return SXRET_OK;` |
|         5 |  8493 | `Synchronize:` |
|        13 |  8494 | `	SySetRelease(&aUnionAlts);` |
|         - |  8495 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8496 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8497 | `		pGen->pIn++;` |
|         3 |  8498 | `	}` |
|        13 |  8499 | `	return SXERR_CORRUPT;` |
|    145410 |  8500 | `}` |
|         - |  8501 | `/*` |
|         - |  8502 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8503 | ` * According to the PHP language reference manual` |
|         - |  8504 | ` *  Properties` |
|         - |  8505 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8506 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8507 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8508 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8509 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8510 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8511 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8512 | ` * Symisc eXtension.` |
|         - |  8513 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8514 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8515 | ` *  Example:` |
|         - |  8516 | ` *   class Test{` |
|         - |  8517 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8518 | ` *   };` |
|         - |  8519 | ` *   var_dump(TEST::myVar);` |
|         - |  8520 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8521 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8522 | ` */` |
|         - |  8523 | `/*` |
|         - |  8524 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8525 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8526 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8527 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8528 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8529 | ` */` |
|   2353644 |  8530 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8531 | `{` |
|   2353649 |  8532 | `	SyToken *p = pStart;` |
|   2353649 |  8533 | `	int bFirst = 1;` |
|   2353649 |  8534 | `	if( p >= pEnd ) return 0;` |
|         - |  8535 | ``	/* Optional nullable `?` shorthand. */`` |
|   2353649 |  8536 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        39 |  8537 | `		p++;` |
|        39 |  8538 | `		if( p >= pEnd ) return 0;` |
|        18 |  8539 | `	}` |
|         - |  8540 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8541 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8542 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8543 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1176822 |  8544 | `	for(;;){` |
|   2353669 |  8545 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8546 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8547 | `			p++;` |
|         9 |  8548 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8549 | `			if( p >= pEnd ) return 0;` |
|         3 |  8550 | `			p++; /* skip ')' */` |
|         2 |  8551 | `		}else{` |
|         - |  8552 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8553 | ``			 * then any `&`-joined intersection members. */`` |
|   2353667 |  8554 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2353667 |  8555 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8556 | `				return 0;` |
|         - |  8557 | `			}` |
|         - |  8558 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8559 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8560 | `			 * may still appear at the initial dispatch site). */` |
|   2353667 |  8561 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2353617 |  8562 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2353612 |  8563 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    103722 |  8564 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2353315 |  8565 | `					return 0;` |
|         - |  8566 | `				}` |
|       151 |  8567 | `			}` |
|       357 |  8568 | `			p++;` |
|       359 |  8569 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8570 | `				p += 2;` |
|         1 |  8571 | `			}` |
|       531 |  8572 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       360 |  8573 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8574 | `				p++; /* skip '&' */` |
|         3 |  8575 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8576 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8577 | `				p++;` |
|         3 |  8578 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8579 | `					p += 2;` |
|       ! 0 |  8580 | `				}` |
|         1 |  8581 | `			}` |
|         - |  8582 | `		}` |
|       359 |  8583 | `		bFirst = 0;` |
|       354 |  8584 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8585 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8586 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8587 | `			continue;` |
|         - |  8588 | `		}` |
|       339 |  8589 | `		break;` |
|       ! 0 |  8590 | `	}` |
|       339 |  8591 | `	if( p >= pEnd ) return 0;` |
|       339 |  8592 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1176827 |  8593 | `}` |
|         - |  8594 |  |
|         - |  8595 | `/*` |
|         - |  8596 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8597 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8598 | ` * if not). Recognized forms:` |
|         - |  8599 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8600 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8601 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8602 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8603 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8604 | ` * on unrecoverable error.` |
|         - |  8605 | ` *` |
|         - |  8606 | ` * When a type is parsed:` |
|         - |  8607 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8608 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8609 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8610 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8611 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8612 | ` */` |
|       344 |  8613 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8614 | `	ph7_gen_state *pGen,` |
|         - |  8615 | `	sxu32 *pnType,` |
|         - |  8616 | `	SyString *pClass,` |
|         - |  8617 | `	sxi32 *piTypeFlags,` |
|         - |  8618 | `	SyString *pTypeText,` |
|         - |  8619 | `	SySet *pAlts` |
|         5 |  8620 | `){` |
|       349 |  8621 | `	sxi32 iFlags = 0;` |
|         - |  8622 | `	sxi32 rc;` |
|       349 |  8623 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8624 | `		return SXRET_OK;` |
|         - |  8625 | `	}` |
|         - |  8626 | `	/* If the first token is '$', there's no type */` |
|       349 |  8627 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8628 | `		return SXRET_OK;` |
|         - |  8629 | `	}` |
|       349 |  8630 | `	rc = GenStateParseUnionTypeDecl(` |
|       172 |  8631 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8632 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8633 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8634 | `		/* bAllowVoid */ 0,` |
|       344 |  8635 | `		pGen->pIn->nLine);` |
|       349 |  8636 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8637 | `		return rc;` |
|         - |  8638 | `	}` |
|         - |  8639 | `	/* Verify next token is '$' (start of property name) */` |
|       349 |  8640 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8641 | `		return SXERR_SYNTAX;` |
|         - |  8642 | `	}` |
|       349 |  8643 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       349 |  8644 | `	return SXRET_OK;` |
|       177 |  8645 | `}` |
|         - |  8646 |  |
|         - |  8647 | `/*` |
|         - |  8648 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8649 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8650 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8651 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8652 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8653 | ` * by the type parser itself before reaching here.` |
|         - |  8654 | ` *` |
|         - |  8655 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8656 | ` * use in the error message.` |
|         - |  8657 | ` */` |
|       522 |  8658 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8659 | `	sxu32 nType,` |
|         - |  8660 | `	const SyString *pClass,` |
|         - |  8661 | `	const char **pzName,` |
|         - |  8662 | `	sxu32 *pnName)` |
|         5 |  8663 | `{` |
|         - |  8664 | `	const char *z;` |
|         - |  8665 | `	sxu32 n;` |
|       527 |  8666 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       467 |  8667 | `		return 0;` |
|         - |  8668 | `	}` |
|        64 |  8669 | `	z = pClass->zString;` |
|        64 |  8670 | `	n = pClass->nByte;` |
|        64 |  8671 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8672 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8673 | `	}` |
|         - |  8674 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8675 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8676 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        58 |  8677 | `	return 0;` |
|       266 |  8678 | `}` |
|         - |  8679 |  |
|         - |  8680 | `/*` |
|         - |  8681 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8682 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8683 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8684 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8685 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8686 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8687 | ` *` |
|         - |  8688 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8689 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8690 | ` */` |
|       460 |  8691 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8692 | `	ph7_gen_state *pGen,` |
|         - |  8693 | `	ph7_class *pClass,` |
|         - |  8694 | `	const SyString *pMemberName,` |
|         - |  8695 | `	sxu32 nType,` |
|         - |  8696 | `	const SyString *pTypeClass,` |
|         - |  8697 | `	const SyString *pTypeText,` |
|         - |  8698 | `	SySet *pUnionAlts,` |
|         - |  8699 | `	const char *zErrFmt,` |
|         - |  8700 | `	sxu32 nLine)` |
|         5 |  8701 | `{` |
|       465 |  8702 | `	const char *zBad = 0;` |
|       465 |  8703 | `	sxu32 nBad = 0;` |
|         - |  8704 | `	SyString sFallback;` |
|         - |  8705 | `	const SyString *pBad;` |
|         - |  8706 | `	sxi32 rc;` |
|       465 |  8707 | `	int bDisallowed = 0;` |
|       465 |  8708 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8709 | `		bDisallowed = 1;` |
|       463 |  8710 | `	}else if( pUnionAlts ){` |
|         - |  8711 | `		sxu32 i;` |
|        95 |  8712 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8713 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8714 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8715 | `				bDisallowed = 1;` |
|         3 |  8716 | `				break;` |
|         - |  8717 | `			}` |
|        35 |  8718 | `		}` |
|        15 |  8719 | `	}` |
|       465 |  8720 | `	if( !bDisallowed ){` |
|       459 |  8721 | `		return SXRET_OK;` |
|         - |  8722 | `	}` |
|         - |  8723 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8724 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8725 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8726 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8727 | `		pBad = pTypeText;` |
|         5 |  8728 | `	}else{` |
|       ! 0 |  8729 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8730 | `		pBad = &sFallback;` |
|         - |  8731 | `	}` |
|        11 |  8732 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8733 | `		zErrFmt,` |
|         3 |  8734 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8735 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8736 | `		return SXERR_ABORT;` |
|         - |  8737 | `	}` |
|         8 |  8738 | `	return SXERR_SYNTAX;` |
|       235 |  8739 | `}` |
|         - |  8740 | `/*` |
|         - |  8741 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8742 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8743 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8744 | ` * than promoted to a lexer keyword.` |
|         - |  8745 | ` */` |
|  20511492 |  8746 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8747 | `{` |
|  20730255 |  8748 | `	return (pTok->nType & PH7_TK_ID)` |
|  10474504 |  8749 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  20730250 |  8750 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8751 | `}` |
|         - |  8752 | `/*` |
|         - |  8753 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8754 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8755 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8756 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8757 | ` */` |
|   7292526 |  8758 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8759 | `{` |
|   7292531 |  8760 | `	*pnTok = 0;` |
|   7292526 |  8761 | `	if( &pTok[3] < pEnd` |
|   6836558 |  8762 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5626385 |  8763 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2436098 |  8764 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8765 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8766 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8767 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8768 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8769 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8770 | `			*pnTok = 4;` |
|        17 |  8771 | `			return nKw;` |
|         - |  8772 | `		}` |
|       ! 0 |  8773 | `	}` |
|   7292515 |  8774 | `	return 0;` |
|   3646268 |  8775 | `}` |
|         - |  8776 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8777 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8778 | `{` |
|        17 |  8779 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8780 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8781 | `	}` |
|         5 |  8782 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8783 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8784 | `	}` |
|         3 |  8785 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8786 | `}` |
|    459906 |  8787 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8788 | `{` |
|    459911 |  8789 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8790 | `	ph7_class_attr *pAttr;` |
|         - |  8791 | `	SyString *pName;` |
|         - |  8792 | `	sxi32 rc;` |
|    459911 |  8793 | `	sxu32 nType = 0;` |
|         - |  8794 | `	SyString sTypeClass;` |
|         - |  8795 | `	SyString sTypeText;` |
|         - |  8796 | `	SySet aUnionAlts;` |
|    459911 |  8797 | `	sxi32 iTypeFlags = 0;` |
|    459911 |  8798 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    459911 |  8799 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    459911 |  8800 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8801 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8802 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8803 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    459911 |  8804 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8805 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8806 | `	}` |
|         - |  8807 | `	/* Extract visibility level */` |
|    459911 |  8808 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8809 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    460083 |  8810 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       349 |  8811 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       349 |  8812 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8813 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8814 | `			goto Synchronize;` |
|       349 |  8815 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8816 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8817 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8818 | `				&pGen->pIn->sData);` |
|       ! 0 |  8819 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8820 | `				return SXERR_ABORT;` |
|         - |  8821 | `			}` |
|       ! 0 |  8822 | `			goto Synchronize;` |
|       349 |  8823 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8824 | `			return SXERR_ABORT;` |
|         - |  8825 | `		}` |
|       172 |  8826 | `	}` |
|       ! 0 |  8827 | `loop:` |
|    459915 |  8828 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8829 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8830 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8831 | `			return SXERR_ABORT;` |
|         - |  8832 | `		}` |
|       ! 0 |  8833 | `		goto Synchronize;` |
|         - |  8834 | `	}` |
|    459915 |  8835 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    459915 |  8836 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8837 | `		/* Invalid attribute name */` |
|       ! 0 |  8838 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8839 | `		if( rc == SXERR_ABORT ){` |
|         - |  8840 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8841 | `			return SXERR_ABORT;` |
|         - |  8842 | `		}` |
|       ! 0 |  8843 | `		goto Synchronize;` |
|         - |  8844 | `	}` |
|         - |  8845 | `	/* Peek attribute name */` |
|    459915 |  8846 | `	pName = &pGen->pIn->sData;` |
|         - |  8847 | `	/* Advance the stream cursor */` |
|    459915 |  8848 | `	pGen->pIn++;` |
|    459915 |  8849 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8850 | `		/* Invalid declaration */` |
|         3 |  8851 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8852 | `		if( rc == SXERR_ABORT ){` |
|         - |  8853 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8854 | `			return SXERR_ABORT;` |
|         - |  8855 | `		}` |
|         3 |  8856 | `		goto Synchronize;` |
|         - |  8857 | `	}` |
|         - |  8858 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8859 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    459913 |  8860 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8861 | `		const char *zAvErr = 0;` |
|        19 |  8862 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8863 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8864 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8865 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8866 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8867 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8868 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8869 | `		}` |
|        13 |  8870 | `		if( zAvErr ){` |
|       ! 0 |  8871 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8872 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8873 | `				return SXERR_ABORT;` |
|         - |  8874 | `			}` |
|       ! 0 |  8875 | `			goto Synchronize;` |
|         - |  8876 | `		}` |
|         6 |  8877 | `	}` |
|         - |  8878 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8879 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    459913 |  8880 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        47 |  8881 | `		const char *zRoErr = 0;` |
|        47 |  8882 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8883 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        46 |  8884 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8885 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        43 |  8886 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8887 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8888 | `		}` |
|        47 |  8889 | `		if( zRoErr ){` |
|        13 |  8890 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8891 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8892 | `				return SXERR_ABORT;` |
|         - |  8893 | `			}` |
|        13 |  8894 | `			goto Synchronize;` |
|         - |  8895 | `		}` |
|        16 |  8896 | `	}` |
|         - |  8897 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8898 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8899 | `	 * by the type parser. */` |
|    459903 |  8900 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       518 |  8901 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8902 | `			&sTypeText,` |
|       342 |  8903 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       171 |  8904 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       347 |  8905 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8906 | `			return SXERR_ABORT;` |
|       347 |  8907 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8908 | `			goto Synchronize;` |
|         - |  8909 | `		}` |
|       171 |  8910 | `	}` |
|         - |  8911 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    459903 |  8912 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8913 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8914 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8915 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8916 | `			return SXERR_ABORT;` |
|         - |  8917 | `		}` |
|         3 |  8918 | `		goto Synchronize;` |
|         - |  8919 | `	}` |
|         - |  8920 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8921 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8922 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8923 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8924 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8925 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    459901 |  8926 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8927 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8928 | `			"New expressions are not supported in this context");` |
|         6 |  8929 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8930 | `			return SXERR_ABORT;` |
|         - |  8931 | `		}` |
|         6 |  8932 | `		goto Synchronize;` |
|         - |  8933 | `	}` |
|         - |  8934 | `	/* Allocate a new class attribute */` |
|    459897 |  8935 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    459897 |  8936 | `	if( pAttr ){` |
|    459897 |  8937 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    459897 |  8938 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8939 | `			return SXERR_ABORT;` |
|         - |  8940 | `		}` |
|    229946 |  8941 | `	}` |
|    459897 |  8942 | `	if( pAttr == 0 ){` |
|       ! 0 |  8943 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8944 | `		return SXERR_ABORT;` |
|         - |  8945 | `	}` |
|    459897 |  8946 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       345 |  8947 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       170 |  8948 | `	}` |
|    459897 |  8949 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8950 | `		SySet *pInstrContainer;` |
|    337235 |  8951 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    337235 |  8952 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8953 | `		{` |
|         - |  8954 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8955 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8956 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8957 | `			 * compiler would otherwise run into the hook tokens. */` |
|    337235 |  8958 | `			SyToken *pScan = pGen->pIn;` |
|    337235 |  8959 | `			sxi32 iNest = 0;` |
|    736347 |  8960 | `			while( pScan < pGen->pEnd ){` |
|    736347 |  8961 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42155 |  8962 | `					iNest++;` |
|    715272 |  8963 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     42155 |  8964 | `					iNest--;` |
|    673122 |  8965 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    337235 |  8966 | `					break;` |
|         - |  8967 | `				}` |
|    399117 |  8968 | `				pScan++;` |
|         5 |  8969 | `			}` |
|    337235 |  8970 | `			pGen->pEnd = pScan;` |
|         - |  8971 | `		}` |
|         - |  8972 | `		/* Swap bytecode container */` |
|    337235 |  8973 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    337235 |  8974 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8975 | `		/* Compile attribute value.` |
|         - |  8976 | `		 */` |
|    337235 |  8977 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    337235 |  8978 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8979 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8980 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8981 | `				return SXERR_ABORT;` |
|         - |  8982 | `			}` |
|       ! 0 |  8983 | `		}` |
|         - |  8984 | `		/* Emit the done instruction */` |
|    337235 |  8985 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    337235 |  8986 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    337235 |  8987 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    337235 |  8988 | `		pGen->pEnd = pSavedDefEnd;` |
|    168615 |  8989 | `	}` |
|         - |  8990 | `	/* All done,install the attribute */` |
|    459897 |  8991 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    459897 |  8992 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8993 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8994 | `		return SXERR_ABORT;` |
|         - |  8995 | `	}` |
|    459897 |  8996 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8997 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8998 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8999 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  9000 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9001 | `			return SXERR_ABORT;` |
|         - |  9002 | `		}` |
|        95 |  9003 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9004 | `			goto Synchronize;` |
|         - |  9005 | `		}` |
|        95 |  9006 | `		SySetRelease(&aUnionAlts);` |
|        95 |  9007 | `		return SXRET_OK;` |
|         - |  9008 | `	}` |
|    459803 |  9009 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  9010 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  9011 | `		 * wording differs per declaration site) */` |
|       ! 0 |  9012 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  9013 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  9014 | `				? "Interfaces may only include hooked properties"` |
|         - |  9015 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  9016 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9017 | `			return SXERR_ABORT;` |
|         - |  9018 | `		}` |
|       ! 0 |  9019 | `		goto Synchronize;` |
|         - |  9020 | `	}` |
|    459803 |  9021 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  9022 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  9023 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  9024 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  9025 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  9026 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  9027 | `				pTok--;` |
|       ! 0 |  9028 | `			}` |
|       ! 0 |  9029 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9030 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  9031 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  9032 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9033 | `				return SXERR_ABORT;` |
|         - |  9034 | `			}` |
|       ! 0 |  9035 | `		}else{` |
|         5 |  9036 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  9037 | `				goto loop;` |
|         - |  9038 | `			}` |
|         - |  9039 | `		}` |
|       ! 0 |  9040 | `	}` |
|    459799 |  9041 | `	SySetRelease(&aUnionAlts);` |
|    459799 |  9042 | `	return SXRET_OK;` |
|         9 |  9043 | `Synchronize:` |
|         - |  9044 | `	/* Synchronize with the first semi-colon */` |
|        56 |  9045 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  9046 | `		pGen->pIn++;` |
|         3 |  9047 | `	}` |
|        22 |  9048 | `	SySetRelease(&aUnionAlts);` |
|        22 |  9049 | `	return SXERR_CORRUPT;` |
|    229958 |  9050 | `}` |
|         - |  9051 | `/*` |
|         - |  9052 | ` * Compile a class method.` |
|         - |  9053 | ` *` |
|         - |  9054 | ` * Refer to the official documentation for more information` |
|         - |  9055 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  9056 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  9057 | ` * overloading and many more.` |
|         - |  9058 | ` */` |
|   2399450 |  9059 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  9060 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  9061 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  9062 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  9063 | `	int doBody,          /* TRUE to process method body */` |
|         - |  9064 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  9065 | `	)` |
|         5 |  9066 | `{` |
|   2399455 |  9067 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2399455 |  9068 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  9069 | `	ph7_class_method *pMeth;` |
|         - |  9070 | `	sxi32 iFuncFlags;` |
|         - |  9071 | `	SyString *pName;` |
|         - |  9072 | `	SyToken *pEnd;` |
|         - |  9073 | `	sxi32 rc;` |
|         - |  9074 | `	/* Extract visibility level */` |
|   2399455 |  9075 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2399455 |  9076 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2399455 |  9077 | `	iFuncFlags = 0;` |
|   2399455 |  9078 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9079 | `		/* Invalid method name */` |
|       ! 0 |  9080 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9081 | `		if( rc == SXERR_ABORT ){` |
|         - |  9082 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9083 | `			return SXERR_ABORT;` |
|         - |  9084 | `		}` |
|       ! 0 |  9085 | `		goto Synchronize;` |
|         - |  9086 | `	}` |
|   2399455 |  9087 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  9088 | `		/* Return by reference,remember that */` |
|       ! 0 |  9089 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  9090 | `		/* Jump the '&' token */` |
|       ! 0 |  9091 | `		pGen->pIn++;` |
|       ! 0 |  9092 | `	}` |
|   2399455 |  9093 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  9094 | `		/* Invalid method name */` |
|       ! 0 |  9095 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9096 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9097 | `			return SXERR_ABORT;` |
|         - |  9098 | `		}` |
|       ! 0 |  9099 | `		goto Synchronize;` |
|         - |  9100 | `	}` |
|         - |  9101 | `	/* Peek method name */` |
|   2399455 |  9102 | `	pName = &pGen->pIn->sData;` |
|   2399455 |  9103 | `	nLine = pGen->pIn->nLine;` |
|         - |  9104 | `	/* Jump the method name */` |
|   2399455 |  9105 | `	pGen->pIn++;` |
|   2399455 |  9106 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  9107 | `		/* Abstract method */` |
|    137745 |  9108 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  9109 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9110 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  9111 | `				&pClass->sName,pName);` |
|       ! 0 |  9112 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9113 | `				return SXERR_ABORT;` |
|         - |  9114 | `			}` |
|       ! 0 |  9115 | `		}` |
|         - |  9116 | `		/* Assemble method signature only */` |
|    137745 |  9117 | `		doBody = FALSE;` |
|     68870 |  9118 | `	}` |
|   2399455 |  9119 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  9120 | `		/* Syntax error */` |
|       ! 0 |  9121 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  9122 | `		if( rc == SXERR_ABORT ){` |
|         - |  9123 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9124 | `			return SXERR_ABORT;` |
|         - |  9125 | `		}` |
|       ! 0 |  9126 | `		goto Synchronize;` |
|         - |  9127 | `	}` |
|         - |  9128 | `	/* Allocate a new class_method instance */` |
|   2399455 |  9129 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2399455 |  9130 | `	if( pMeth == 0 ){` |
|       ! 0 |  9131 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9132 | `		return SXERR_ABORT;` |
|         - |  9133 | `	}` |
|   2399455 |  9134 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2399455 |  9135 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2399455 |  9136 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9137 | `		return SXERR_ABORT;` |
|         - |  9138 | `	}` |
|         - |  9139 | `	/* Jump the left parenthesis '(' */` |
|   2399455 |  9140 | `	pGen->pIn++;` |
|   2399455 |  9141 | `	pEnd = 0; /* cc warning */` |
|         - |  9142 | `	/* Delimit the method signature */` |
|   2399455 |  9143 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2399455 |  9144 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9145 | `		/* Syntax error */` |
|         3 |  9146 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  9147 | `		if( rc == SXERR_ABORT ){` |
|         - |  9148 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9149 | `			return SXERR_ABORT;` |
|         - |  9150 | `		}` |
|         3 |  9151 | `		goto Synchronize;` |
|         - |  9152 | `	}` |
|         - |  9153 | `	{` |
|   2399453 |  9154 | `		int bIsCtor = 0;` |
|   2399453 |  9155 | `		int bAbstractCtor = 0;` |
|   2399448 |  9156 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1400648 |  9157 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2317128 |  9158 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    164655 |  9159 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  9160 | `				bAbstractCtor = 1;` |
|         2 |  9161 | `			}else{` |
|    164653 |  9162 | `				bIsCtor = 1;` |
|         - |  9163 | `			}` |
|     82325 |  9164 | `		}` |
|   2399453 |  9165 | `		if( pGen->pIn < pEnd ){` |
|         - |  9166 | `			/* Collect method arguments */` |
|    864953 |  9167 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    864953 |  9168 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9169 | `				return SXERR_ABORT;` |
|         - |  9170 | `			}` |
|    432474 |  9171 | `		}` |
|         - |  9172 | `	}` |
|         - |  9173 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2399453 |  9174 | `	pGen->pIn = &pEnd[1];` |
|         - |  9175 | `	{` |
|   2399453 |  9176 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2399453 |  9177 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  9178 | `			return SXERR_ABORT;` |
|   2399453 |  9179 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  9180 | `			goto Synchronize;` |
|         - |  9181 | `		}` |
|         - |  9182 | `	}` |
|         - |  9183 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  9184 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  9185 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  9186 | `	{` |
|   2399453 |  9187 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  9188 | `		sxu32 i;` |
|   3692871 |  9189 | `		for( i = 0; i < nArg; i++ ){` |
|   1293433 |  9190 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  9191 | `			ph7_class_attr *pAttr;` |
|   1293433 |  9192 | `			sxi32 iAttrFlags = 0;` |
|         - |  9193 | `			int bArgTyped;` |
|   1293433 |  9194 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1293347 |  9195 | `				continue;` |
|         - |  9196 | `			}` |
|         - |  9197 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  9198 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  9199 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        60 |  9200 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        92 |  9201 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        91 |  9202 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  9203 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9204 | `					"Cannot declare variadic promoted property");` |
|         3 |  9205 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9206 | `					return SXERR_ABORT;` |
|         - |  9207 | `				}` |
|         3 |  9208 | `				goto Synchronize;` |
|         - |  9209 | `			}` |
|         - |  9210 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  9211 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  9212 | `			 * appear as an alternative of a union type. */` |
|        89 |  9213 | `			if( bArgTyped ){` |
|       125 |  9214 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        80 |  9215 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        80 |  9216 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        40 |  9217 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        85 |  9218 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9219 | `					return SXERR_ABORT;` |
|        85 |  9220 | `				}else if( rc != SXRET_OK ){` |
|         6 |  9221 | `					goto Synchronize;` |
|         - |  9222 | `				}` |
|        38 |  9223 | `			}` |
|         - |  9224 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        85 |  9225 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  9226 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9227 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  9228 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9229 | `					return SXERR_ABORT;` |
|         - |  9230 | `				}` |
|         3 |  9231 | `				goto Synchronize;` |
|         - |  9232 | `			}` |
|        83 |  9233 | `			if( bArgTyped ){` |
|        79 |  9234 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        37 |  9235 | `			}` |
|        83 |  9236 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  9237 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  9238 | `			}` |
|        83 |  9239 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  9240 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  9241 | `			}` |
|        83 |  9242 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  9243 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  9244 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  9245 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  9246 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9247 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  9248 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9249 | `						return SXERR_ABORT;` |
|         - |  9250 | `					}` |
|         3 |  9251 | `					goto Synchronize;` |
|         - |  9252 | `				}` |
|        24 |  9253 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  9254 | `			}` |
|        81 |  9255 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  9256 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  9257 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  9258 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9259 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  9260 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  9261 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9262 | `						return SXERR_ABORT;` |
|         - |  9263 | `					}` |
|       ! 0 |  9264 | `					goto Synchronize;` |
|         - |  9265 | `				}` |
|         5 |  9266 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  9267 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  9268 | `			}` |
|        81 |  9269 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        81 |  9270 | `			if( pAttr == 0 ){` |
|       ! 0 |  9271 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9272 | `				return SXERR_ABORT;` |
|         - |  9273 | `			}` |
|        81 |  9274 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        79 |  9275 | `				pAttr->nType = pArg->nType;` |
|        79 |  9276 | `				pAttr->sClass = pArg->sClass;` |
|        79 |  9277 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        79 |  9278 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9279 | `					sxu32 k;` |
|        20 |  9280 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9281 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9282 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9283 | `					}` |
|         3 |  9284 | `				}` |
|        37 |  9285 | `			}` |
|        81 |  9286 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        81 |  9287 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9288 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9289 | `				return SXERR_ABORT;` |
|         - |  9290 | `			}` |
|        43 |  9291 | `		}` |
|         - |  9292 | `	}` |
|   2399443 |  9293 | `	if( doBody ){` |
|         - |  9294 | `		/* Compile method body */` |
|   2261703 |  9295 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2261703 |  9296 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9297 | `			return SXERR_ABORT;` |
|         - |  9298 | `		}` |
|         - |  9299 | `		/* The cursor sits just past the body's closing brace */` |
|   2261703 |  9300 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1130854 |  9301 | `	}else{` |
|         - |  9302 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    137745 |  9303 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    137745 |  9304 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     68870 |  9305 | `		}` |
|         - |  9306 | `		/* Only method signature is allowed */` |
|    137745 |  9307 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9308 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9309 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9310 | `				if( rc == SXERR_ABORT ){` |
|         - |  9311 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9312 | `					return SXERR_ABORT;` |
|         - |  9313 | `				}` |
|       ! 0 |  9314 | `				return SXERR_CORRUPT;` |
|         - |  9315 | `			}` |
|         - |  9316 | `	}` |
|         - |  9317 | `	/* All done,install the method */` |
|   2399443 |  9318 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2399443 |  9319 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9320 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9321 | `		return SXERR_ABORT;` |
|         - |  9322 | `	}` |
|   2399443 |  9323 | `	return SXRET_OK;` |
|         6 |  9324 | `Synchronize:` |
|         - |  9325 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9326 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9327 | `		pGen->pIn++;` |
|         4 |  9328 | `	}` |
|        16 |  9329 | `	return SXERR_CORRUPT;` |
|   1199730 |  9330 | `}` |
|         - |  9331 | `/*` |
|         - |  9332 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9333 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9334 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9335 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9336 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9337 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9338 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9339 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9340 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9341 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9342 | `` * implicit `$value` formal.`` |
|         - |  9343 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9344 | ` */` |
|         - |  9345 | `/*` |
|         - |  9346 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9347 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9348 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9349 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9350 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9351 | ` */` |
|        94 |  9352 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9353 | `{` |
|         - |  9354 | `	SyToken *p;` |
|       345 |  9355 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9356 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9357 | `			continue;` |
|         - |  9358 | `		}` |
|         - |  9359 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9360 | `		if( p + 3 < pEnd` |
|        80 |  9361 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9362 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9363 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9364 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9365 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9366 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9367 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9368 | `			return 1;` |
|         - |  9369 | `		}` |
|         - |  9370 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9371 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9372 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9373 | `		if( p > pStart` |
|        26 |  9374 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9375 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9376 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9377 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9378 | `			return 1;` |
|         - |  9379 | `		}` |
|        15 |  9380 | `	}` |
|        43 |  9381 | `	return 0;` |
|        48 |  9382 | `}` |
|         - |  9383 | `/*` |
|         - |  9384 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9385 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9386 | ` */` |
|       990 |  9387 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9388 | `{` |
|      1167 |  9389 | `	return p + 6 < pEnd` |
|       671 |  9390 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9391 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9392 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9393 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9394 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9395 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9396 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9397 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9398 | `	 && p[5].sData.nByte == 3` |
|         8 |  9399 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9400 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9401 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9402 | `}` |
|         - |  9403 | `/*` |
|         - |  9404 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9405 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9406 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9407 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9408 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9409 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9410 | ` * or SXERR_MEM.` |
|         - |  9411 | ` */` |
|         4 |  9412 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9413 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9414 | `{` |
|         5 |  9415 | `	SyToken *p = pStart;` |
|        35 |  9416 | `	while( p < pEnd ){` |
|        31 |  9417 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9418 | `			SyToken sTok;` |
|         - |  9419 | `			char zName[384];` |
|         - |  9420 | `			sxu32 nName;` |
|         - |  9421 | `			char *zDup;` |
|         - |  9422 | ``			/* `parent` `::` */`` |
|         5 |  9423 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9424 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9425 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9426 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9427 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9428 | `			if( zDup == 0 ){` |
|       ! 0 |  9429 | `				return SXERR_MEM;` |
|         - |  9430 | `			}` |
|         5 |  9431 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9432 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9433 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9434 | `			sTok.pUserData = 0;` |
|         5 |  9435 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9436 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9437 | `			continue;` |
|         - |  9438 | `		}` |
|        27 |  9439 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9440 | `		p++;` |
|         1 |  9441 | `	}` |
|         5 |  9442 | `	return SXRET_OK;` |
|         3 |  9443 | `}` |
|        94 |  9444 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9445 | `{` |
|        95 |  9446 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9447 | `	sxi32 rc;` |
|        95 |  9448 | `	int bRefsSelf = 0;` |
|        95 |  9449 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9450 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9451 | `		char zHook[384];` |
|         - |  9452 | `		SyString sHookName;` |
|         - |  9453 | `		ph7_class_method *pMeth;` |
|         - |  9454 | `		int bGet;` |
|       159 |  9455 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9456 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9457 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9458 | `			continue;` |
|         - |  9459 | `		}` |
|       145 |  9460 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9461 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9462 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9463 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9464 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9465 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9466 | `				return SXERR_ABORT;` |
|         - |  9467 | `			}` |
|       ! 0 |  9468 | `			return SXERR_CORRUPT;` |
|         - |  9469 | `		}` |
|       145 |  9470 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9471 | `			goto HookSyntax;` |
|         - |  9472 | `		}` |
|       144 |  9473 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9474 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9475 | `			bGet = 1;` |
|       106 |  9476 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9477 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9478 | `			bGet = 0;` |
|        34 |  9479 | `		}else{` |
|       ! 0 |  9480 | `			goto HookSyntax;` |
|         - |  9481 | `		}` |
|       145 |  9482 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9483 | `		sHookName.zString = zHook;` |
|       217 |  9484 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9485 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9486 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9487 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9488 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9489 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9490 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9491 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9492 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9493 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9494 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9495 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9496 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9497 | `					return SXERR_ABORT;` |
|         - |  9498 | `				}` |
|       ! 0 |  9499 | `				return SXERR_CORRUPT;` |
|         - |  9500 | `			}` |
|        15 |  9501 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9502 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9503 | `			if( pMeth == 0 ){` |
|       ! 0 |  9504 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9505 | `				return SXERR_ABORT;` |
|         - |  9506 | `			}` |
|        15 |  9507 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9508 | `			if( !bGet ){` |
|         - |  9509 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9510 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9511 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9512 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9513 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9514 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9515 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9516 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9517 | `				if( zVName == 0 ){` |
|       ! 0 |  9518 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9519 | `					return SXERR_ABORT;` |
|         - |  9520 | `				}` |
|         7 |  9521 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9522 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9523 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9524 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9525 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9526 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9527 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9528 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9529 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9530 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9531 | `				}` |
|         7 |  9532 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9533 | `			}` |
|        15 |  9534 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9535 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9536 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9537 | `				return SXERR_ABORT;` |
|         - |  9538 | `			}` |
|        15 |  9539 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9540 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9541 | `		}` |
|       130 |  9542 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9543 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9544 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9545 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9546 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9547 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9548 | `				return SXERR_ABORT;` |
|         - |  9549 | `			}` |
|       ! 0 |  9550 | `			return SXERR_CORRUPT;` |
|         - |  9551 | `		}` |
|       131 |  9552 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9553 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9554 | `		if( pMeth == 0 ){` |
|       ! 0 |  9555 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9556 | `			return SXERR_ABORT;` |
|         - |  9557 | `		}` |
|       131 |  9558 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9559 | `		if( !bGet ){` |
|         - |  9560 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9561 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9562 | `				SyToken *pRp = 0;` |
|        17 |  9563 | `				pGen->pIn++;` |
|        17 |  9564 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9565 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9566 | `					goto HookSyntax;` |
|         - |  9567 | `				}` |
|        17 |  9568 | `				if( pGen->pIn < pRp ){` |
|        17 |  9569 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9570 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9571 | `						return SXERR_ABORT;` |
|         - |  9572 | `					}` |
|         8 |  9573 | `				}` |
|        17 |  9574 | `				pGen->pIn = &pRp[1];` |
|         8 |  9575 | `			}` |
|        61 |  9576 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9577 | `				/* Implicit $value formal */` |
|         - |  9578 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9579 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9580 | `				if( zVName == 0 ){` |
|       ! 0 |  9581 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9582 | `					return SXERR_ABORT;` |
|         - |  9583 | `				}` |
|        45 |  9584 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9585 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9586 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9587 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9588 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9589 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9590 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9591 | `			}` |
|        30 |  9592 | `		}` |
|       165 |  9593 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9594 | `			/* Block body */` |
|        69 |  9595 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9596 | `			SyToken *pCloser = 0;` |
|        69 |  9597 | `			int bParentCall = 0;` |
|        69 |  9598 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9599 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9600 | `				SyToken *pScan;` |
|       753 |  9601 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9602 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9603 | `						bParentCall = 1;` |
|         3 |  9604 | `						break;` |
|         - |  9605 | `					}` |
|       343 |  9606 | `				}` |
|        34 |  9607 | `			}` |
|        69 |  9608 | `			if( bParentCall ){` |
|         - |  9609 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9610 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9611 | `				 * hook method), then continue past the original body. */` |
|         - |  9612 | `				SySet sBody;` |
|         3 |  9613 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9614 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9615 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9616 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9617 | `					SySetRelease(&sBody);` |
|       ! 0 |  9618 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9619 | `					return SXERR_ABORT;` |
|         - |  9620 | `				}` |
|         3 |  9621 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9622 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9623 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9624 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9625 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9626 | `				SySetRelease(&sBody);` |
|         3 |  9627 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9628 | `					return SXERR_ABORT;` |
|         - |  9629 | `				}` |
|         3 |  9630 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9631 | `			}else{` |
|        67 |  9632 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9633 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9634 | `					return SXERR_ABORT;` |
|         - |  9635 | `				}` |
|        67 |  9636 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9637 | `			}` |
|        69 |  9638 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9639 | `				bRefsSelf = 1;` |
|         9 |  9640 | `			}` |
|       128 |  9641 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9642 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9643 | `			GenBlock *pBlock;` |
|         - |  9644 | `			SySet *pInstrContainer;` |
|         - |  9645 | `			SyToken *pBodyStart;` |
|         - |  9646 | `			SyToken *pExprEnd;` |
|        63 |  9647 | `			SyToken *pSavedEnd = 0;` |
|         - |  9648 | `			SySet sBody;` |
|        63 |  9649 | `			int bParentCall = 0;` |
|        63 |  9650 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9651 | `			pBodyStart = pGen->pIn;` |
|         - |  9652 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9653 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9654 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9655 | `			 * method on a token copy. */` |
|         - |  9656 | `			{` |
|        63 |  9657 | `				sxi32 iNest = 0;` |
|        63 |  9658 | `				pExprEnd = pBodyStart;` |
|       355 |  9659 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9660 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9661 | `						iNest++;` |
|       351 |  9662 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9663 | `						if( iNest <= 0 ){` |
|       ! 0 |  9664 | `							break;` |
|         - |  9665 | `						}` |
|         9 |  9666 | `						iNest--;` |
|       343 |  9667 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9668 | `						break;` |
|         - |  9669 | `					}` |
|       293 |  9670 | `					pExprEnd++;` |
|         1 |  9671 | `				}` |
|         - |  9672 | `			}` |
|         - |  9673 | `			{` |
|         - |  9674 | `				SyToken *pScan;` |
|       335 |  9675 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9676 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9677 | `						bParentCall = 1;` |
|         3 |  9678 | `						break;` |
|         - |  9679 | `					}` |
|       137 |  9680 | `				}` |
|         - |  9681 | `			}` |
|        63 |  9682 | `			if( bParentCall ){` |
|         3 |  9683 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9684 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9685 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9686 | `					SySetRelease(&sBody);` |
|       ! 0 |  9687 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9688 | `					return SXERR_ABORT;` |
|         - |  9689 | `				}` |
|         3 |  9690 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9691 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9692 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9693 | `			}` |
|        94 |  9694 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9695 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9696 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9697 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9698 | `				return SXERR_ABORT;` |
|         - |  9699 | `			}` |
|        63 |  9700 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9701 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9702 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9703 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9704 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9705 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9706 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9707 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9708 | `			if( bParentCall ){` |
|         3 |  9709 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9710 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9711 | `				SySetRelease(&sBody);` |
|         1 |  9712 | `			}` |
|        63 |  9713 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9714 | `				return SXERR_ABORT;` |
|         - |  9715 | `			}` |
|        63 |  9716 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9717 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9718 | `				bRefsSelf = 1;` |
|        18 |  9719 | `			}` |
|        63 |  9720 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9721 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9722 | `			}` |
|        63 |  9723 | `			if( !bGet ){` |
|         - |  9724 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9725 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9726 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9727 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9728 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9729 | `				bRefsSelf = 1;` |
|         1 |  9730 | `			}` |
|        32 |  9731 | `		}else{` |
|       ! 0 |  9732 | `			goto HookSyntax;` |
|         - |  9733 | `		}` |
|       131 |  9734 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9735 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9736 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9737 | `			return SXERR_ABORT;` |
|         - |  9738 | `		}` |
|       131 |  9739 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9740 | `	}` |
|        95 |  9741 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9742 | `		goto HookSyntax;` |
|         - |  9743 | `	}` |
|        95 |  9744 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9745 | `	if( !bRefsSelf ){` |
|         - |  9746 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9747 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9748 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9749 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9750 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9751 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9752 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9753 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9754 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9755 | `				return SXERR_ABORT;` |
|         - |  9756 | `			}` |
|       ! 0 |  9757 | `			return SXERR_CORRUPT;` |
|         - |  9758 | `		}` |
|        20 |  9759 | `	}` |
|        95 |  9760 | `	return SXRET_OK;` |
|       ! 0 |  9761 | `HookSyntax:` |
|       ! 0 |  9762 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9763 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9764 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9765 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9766 | `		return SXERR_ABORT;` |
|         - |  9767 | `	}` |
|       ! 0 |  9768 | `	return SXERR_CORRUPT;` |
|        48 |  9769 | `}` |
|         - |  9770 | `/*` |
|         - |  9771 | ` * Compile an object interface.` |
|         - |  9772 | ` *  According to the PHP language reference manual` |
|         - |  9773 | ` *   Object Interfaces:` |
|         - |  9774 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9775 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9776 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9777 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9778 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9779 | ` */` |
|     68948 |  9780 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9781 | `{` |
|     68953 |  9782 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9783 | `	ph7_class *pClass,*pBase;` |
|         - |  9784 | `	SyToken *pEnd,*pTmp;` |
|         - |  9785 | `	SyString *pName;` |
|         - |  9786 | `	sxi32 nKwrd;` |
|         - |  9787 | `	sxi32 rc;` |
|         - |  9788 | `	/* Jump the 'interface' keyword */` |
|     68953 |  9789 | `	pGen->pIn++;` |
|         - |  9790 | `	/* Extract interface name */` |
|     68953 |  9791 | `	pName = &pGen->pIn->sData;` |
|         - |  9792 | `	/* Advance the stream cursor */` |
|     68953 |  9793 | `	pGen->pIn++;` |
|         - |  9794 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9795 | `		SyBlob sFQN;` |
|         - |  9796 | `		SyString sFQNStr;` |
|     68953 |  9797 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     68953 |  9798 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     68953 |  9799 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     68953 |  9800 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     68953 |  9801 | `		SyBlobRelease(&sFQN);` |
|         - |  9802 | `	}` |
|     68953 |  9803 | `	if( pClass == 0 ){` |
|       ! 0 |  9804 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9805 | `		return SXERR_ABORT;` |
|         - |  9806 | `	}` |
|     68953 |  9807 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     68953 |  9808 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9809 | `		return SXERR_ABORT;` |
|         - |  9810 | `	}` |
|         - |  9811 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     68953 |  9812 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9813 | `	/* Assume no base class is given */` |
|     68953 |  9814 | `	pBase = 0;` |
|     68953 |  9815 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26785 |  9816 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26785 |  9817 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|         - |  9818 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|         - |  9819 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|         - |  9820 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|         - |  9821 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|     26785 |  9822 | `			pGen->pIn++;` |
|     13391 |  9823 | `			for(;;){` |
|         - |  9824 | `				SyBlob sResolved;` |
|         - |  9825 | `				SyString sBaseName;` |
|         - |  9826 | `				sxu32 nRefLine;` |
|         - |  9827 | `				ph7_class *pParent;` |
|     26787 |  9828 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26787 |  9829 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26787 |  9830 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9831 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9832 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9833 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9834 | `						pName);` |
|       ! 0 |  9835 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9836 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9837 | `						return SXERR_ABORT;` |
|         - |  9838 | `					}` |
|       ! 0 |  9839 | `					return SXRET_OK;` |
|         - |  9840 | `				}` |
|     40178 |  9841 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|     26782 |  9842 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26787 |  9843 | `				SyStringInitFromBuf(&sBaseName,` |
|         - |  9844 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9845 | `				/* Only interfaces is allowed */` |
|     26787 |  9846 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9847 | `					pParent = pParent->pNextName;` |
|       ! 0 |  9848 | `				}` |
|     26787 |  9849 | `				if( pParent == 0 ){` |
|       ! 0 |  9850 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9851 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9852 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9853 | `						SyBlobRelease(&sResolved);` |
|       ! 0 |  9854 | `						return SXERR_ABORT;` |
|       ! 0 |  9855 | `					}` |
|     26787 |  9856 | `				}else if( pBase == 0 ){` |
|         - |  9857 | `					/* First parent → single-inheritance base */` |
|     26785 |  9858 | `					pBase = pParent;` |
|     13395 |  9859 | `				}else{` |
|         - |  9860 | `					/* Additional parent → record it in aInterface (+ copy its` |
|         - |  9861 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|         3 |  9862 | `					PH7_ClassImplement(pClass,pParent);` |
|         - |  9863 | `				}` |
|     26787 |  9864 | `				SyBlobRelease(&sResolved);` |
|         - |  9865 | `				/* Continue on a comma-separated list */` |
|     26787 |  9866 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  9867 | `					pGen->pIn++;` |
|         3 |  9868 | `					continue;` |
|         - |  9869 | `				}` |
|     26785 |  9870 | `				break;` |
|       ! 0 |  9871 | `			}` |
|     13390 |  9872 | `		}` |
|     13390 |  9873 | `	}` |
|     68953 |  9874 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9875 | `		/* Syntax error */` |
|       ! 0 |  9876 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9877 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9878 | `		if( rc == SXERR_ABORT ){` |
|         - |  9879 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9880 | `			return SXERR_ABORT;` |
|         - |  9881 | `		}` |
|       ! 0 |  9882 | `		return SXRET_OK;` |
|         - |  9883 | `	}` |
|     68953 |  9884 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     68953 |  9885 | `	pEnd = 0; /* cc warning */` |
|         - |  9886 | `	/* Delimit the interface body */` |
|     68953 |  9887 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     68953 |  9888 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9889 | `		/* Syntax error */` |
|       ! 0 |  9890 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9891 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9892 | `		if( rc == SXERR_ABORT ){` |
|         - |  9893 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9894 | `			return SXERR_ABORT;` |
|         - |  9895 | `		}` |
|       ! 0 |  9896 | `		return SXRET_OK;` |
|         - |  9897 | `	}` |
|         - |  9898 | `	/* The delimiter token is the interface body's closing brace */` |
|     68953 |  9899 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9900 | `	/* Swap token stream */` |
|     68953 |  9901 | `	pTmp = pGen->pEnd;` |
|     68953 |  9902 | `	pGen->pEnd = pEnd;` |
|         - |  9903 | `	/* Start the parse process` |
|         - |  9904 | `	 * Note (According to the PHP reference manual):` |
|         - |  9905 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9906 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9907 | `	 */` |
|    126283 |  9908 | `	for(;;){` |
|         - |  9909 | `		/* Jump leading/trailing semi-colons */` |
|    436193 |  9910 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    183623 |  9911 | `			pGen->pIn++;` |
|         5 |  9912 | `		}` |
|    252575 |  9913 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9914 | `			/* End of interface body */` |
|     68949 |  9915 | `			break;` |
|         - |  9916 | `		}` |
|         - |  9917 | `		/* Bind a directly-preceding docblock to this member */` |
|    183631 |  9918 | `		GenStateSetPendingDoc(&(*pGen));` |
|    183631 |  9919 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9921 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9922 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9923 | `			if( rc == SXERR_ABORT ){` |
|         - |  9924 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9925 | `				return SXERR_ABORT;` |
|         - |  9926 | `			}` |
|       ! 0 |  9927 | `			goto done;` |
|         - |  9928 | `		}` |
|         - |  9929 | `		/* Extract the current keyword */` |
|    183631 |  9930 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    183631 |  9931 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9932 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9933 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9934 | `			const char *zKind = "member";` |
|         3 |  9935 | `			SyString *pMemberName = 0;` |
|         3 |  9936 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9937 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9938 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9939 | `					zKind = "constant";` |
|         3 |  9940 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9941 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9942 | `					}` |
|         1 |  9943 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9944 | `					zKind = "method";` |
|       ! 0 |  9945 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9946 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9947 | `					}` |
|       ! 0 |  9948 | `				}` |
|         1 |  9949 | `			}` |
|         3 |  9950 | `			if( pMemberName ){` |
|         4 |  9951 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9952 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9953 | `			}else{` |
|       ! 0 |  9954 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9955 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9956 | `			}` |
|         3 |  9957 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9958 | `				return SXERR_ABORT;` |
|         - |  9959 | `			}` |
|         3 |  9960 | `			goto done;` |
|         - |  9961 | `		}` |
|    183629 |  9962 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9963 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9964 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9965 | `			if( rc == SXERR_ABORT ){` |
|         - |  9966 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9967 | `				return SXERR_ABORT;` |
|         - |  9968 | `			}` |
|       ! 0 |  9969 | `			goto done;` |
|         - |  9970 | `		}` |
|    183629 |  9971 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9972 | `			/* Advance the stream cursor */` |
|    130075 |  9973 | `			pGen->pIn++;` |
|    130070 |  9974 | `			if( pGen->pIn < pGen->pEnd` |
|    130075 |  9975 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    130070 |  9976 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9977 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9978 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9979 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9980 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9981 | `				 * hooked properties" error). */` |
|       ! 0 |  9982 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9983 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9984 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9985 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9986 | `						return SXERR_ABORT;` |
|         - |  9987 | `					}` |
|       ! 0 |  9988 | `					goto done;` |
|         - |  9989 | `				}` |
|       ! 0 |  9990 | `				continue;` |
|         - |  9991 | `			}` |
|    130075 |  9992 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9993 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9994 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9995 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9996 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9997 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9998 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9999 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 | 10000 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 10001 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10002 | `							return SXERR_ABORT;` |
|         - | 10003 | `						}` |
|       ! 0 | 10004 | `						goto done;` |
|         - | 10005 | `					}` |
|       ! 0 | 10006 | `					continue;` |
|         - | 10007 | `				}` |
|       ! 0 | 10008 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10009 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 10010 | `				if( rc == SXERR_ABORT ){` |
|         - | 10011 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 10012 | `					return SXERR_ABORT;` |
|         - | 10013 | `				}` |
|       ! 0 | 10014 | `				goto done;` |
|         - | 10015 | `			}` |
|    130075 | 10016 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    130075 | 10017 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - | 10018 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - | 10019 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 | 10020 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 | 10021 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 | 10022 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 | 10023 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 | 10024 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 10025 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10026 | `							return SXERR_ABORT;` |
|         - | 10027 | `						}` |
|       ! 0 | 10028 | `						goto done;` |
|         - | 10029 | `					}` |
|         5 | 10030 | `					continue;` |
|         - | 10031 | `				}` |
|       ! 0 | 10032 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10033 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 | 10034 | `				if( rc == SXERR_ABORT ){` |
|         - | 10035 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 10036 | `					return SXERR_ABORT;` |
|         - | 10037 | `				}` |
|       ! 0 | 10038 | `				goto done;` |
|         - | 10039 | `			}` |
|     65033 | 10040 | `		}` |
|    183625 | 10041 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 10042 | `			/* Parse constant */` |
|     53555 | 10043 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53555 | 10044 | `			if( rc != SXRET_OK ){` |
|         3 | 10045 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10046 | `					return SXERR_ABORT;` |
|         - | 10047 | `				}` |
|         3 | 10048 | `				goto done;` |
|         - | 10049 | `			}` |
|     26779 | 10050 | `		}else{` |
|    130075 | 10051 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    130075 | 10052 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 10053 | `				/* Static method,record that */` |
|     11477 | 10054 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - | 10055 | `				/* Advance the stream cursor */` |
|     11477 | 10056 | `				pGen->pIn++;` |
|     11472 | 10057 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11477 | 10058 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 10059 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10060 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 10061 | `						if( rc == SXERR_ABORT ){` |
|         - | 10062 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 10063 | `							return SXERR_ABORT;` |
|         - | 10064 | `						}` |
|       ! 0 | 10065 | `						goto done;` |
|         - | 10066 | `				}` |
|      5736 | 10067 | `			}` |
|         - | 10068 | `			/* Process method signature (no body for interface methods) */` |
|    130075 | 10069 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    130075 | 10070 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 10071 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10072 | `					return SXERR_ABORT;` |
|         - | 10073 | `				}` |
|       ! 0 | 10074 | `				goto done;` |
|         - | 10075 | `			}` |
|         - | 10076 | `		}` |
|         5 | 10077 | `	}` |
|         - | 10078 | `	/* Install the interface */` |
|     68949 | 10079 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     68949 | 10080 | `	if( rc == SXRET_OK && pBase ){` |
|         - | 10081 | `		/* Inherit from the base interface */` |
|     26785 | 10082 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13390 | 10083 | `	}` |
|     68949 | 10084 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10085 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10086 | `		return SXERR_ABORT;` |
|         - | 10087 | `	}` |
|     34472 | 10088 | `done:` |
|         - | 10089 | `	/* Point beyond the interface body */` |
|     68953 | 10090 | `	pGen->pIn  = &pEnd[1];` |
|     68953 | 10091 | `	pGen->pEnd = pTmp;` |
|     68953 | 10092 | `	return PH7_OK;` |
|     34479 | 10093 | `}` |
|         - | 10094 | `/*` |
|         - | 10095 | ` * Compile a user-defined class.` |
|         - | 10096 | ` * According to the PHP language reference manual` |
|         - | 10097 | ` *  class` |
|         - | 10098 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - | 10099 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - | 10100 | ` *  of the properties and methods belonging to the class.` |
|         - | 10101 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - | 10102 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - | 10103 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - | 10104 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - | 10105 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - | 10106 | ` *  (called "methods").` |
|         - | 10107 | ` */` |
|         - | 10108 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - | 10109 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - | 10110 | `struct TraitUseEntry {` |
|         - | 10111 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - | 10112 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - | 10113 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - | 10114 | `};` |
|         - | 10115 | `/*` |
|         - | 10116 | ` * Validate that methods implementing interface contracts have compatible` |
|         - | 10117 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - | 10118 | ` */` |
|    353652 | 10119 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10120 | `{` |
|         - | 10121 | `	ph7_class **apIface;` |
|         - | 10122 | `	sxu32 nIface,i;` |
|         - | 10123 | `	sxi32 rc;` |
|    353657 | 10124 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 | 10125 | `		return SXRET_OK;` |
|         - | 10126 | `	}` |
|    353657 | 10127 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    353657 | 10128 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    709635 | 10129 | `	for(i = 0; i < nIface; i++){` |
|    355983 | 10130 | `		ph7_class *pIface = apIface[i];` |
|         - | 10131 | `		SyHashEntry *pEntry;` |
|    355983 | 10132 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1025783 | 10133 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    669805 | 10134 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - | 10135 | `			ph7_class_method *pImplMeth;` |
|    669805 | 10136 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - | 10137 | `			/* Find the implementing method in the class */` |
|    669805 | 10138 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    669805 | 10139 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 | 10140 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - | 10141 | `			}` |
|         - | 10142 | `			/* Check visibility: interface methods must be implemented as public */` |
|    669787 | 10143 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 | 10144 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10145 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 | 10146 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 | 10147 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10148 | `					return SXERR_ABORT;` |
|         - | 10149 | `				}` |
|         1 | 10150 | `			}` |
|         - | 10151 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - | 10152 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - | 10153 | `			 */` |
|         - | 10154 | `			{` |
|    669787 | 10155 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    669787 | 10156 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    669787 | 10157 | `				int sigError = 0;` |
|    669787 | 10158 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 | 10159 | `					sigError = 1;` |
|    669786 | 10160 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - | 10161 | `					/* Extra parameters must all have default values */` |
|      3833 | 10162 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 10163 | `					sxu32 k;` |
|      7659 | 10164 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3833 | 10165 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 10166 | `							sigError = 1;` |
|         3 | 10167 | `							break;` |
|         - | 10168 | `						}` |
|      1918 | 10169 | `					}` |
|      1914 | 10170 | `				}` |
|    669787 | 10171 | `				if( sigError ){` |
|         - | 10172 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 10173 | `					ph7_vm_func_arg *aArgs;` |
|         - | 10174 | `					sxu32 j;` |
|         6 | 10175 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 10176 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 10177 | `					/* Build implementing method signature */` |
|         6 | 10178 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 10179 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 10180 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 10181 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 10182 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10183 | `					}` |
|         - | 10184 | `					/* Build interface method signature */` |
|         6 | 10185 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 10186 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 10187 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 10188 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 10189 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10190 | `					}` |
|         8 | 10191 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10192 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 10193 | `						&pClass->sName,pMName,` |
|         4 | 10194 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 10195 | `						&pIface->sName,pMName,` |
|         4 | 10196 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 10197 | `					SyBlobRelease(&sImplSig);` |
|         6 | 10198 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 10199 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10200 | `						return SXERR_ABORT;` |
|         - | 10201 | `					}` |
|         2 | 10202 | `				}` |
|         - | 10203 | `			}` |
|         5 | 10204 | `		}` |
|    177994 | 10205 | `	}` |
|    353657 | 10206 | `	return SXRET_OK;` |
|    176831 | 10207 | `}` |
|         - | 10208 | `/*` |
|         - | 10209 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 10210 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 10211 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 10212 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 10213 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 10214 | ` * means that specific hook is still missing.` |
|         - | 10215 | ` */` |
|        38 | 10216 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 10217 | `{` |
|         - | 10218 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 10219 | `	ph7_class_attr *pProp;` |
|        38 | 10220 | `	if( pMName->nByte <= nPfx` |
|        27 | 10221 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 10222 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 10223 | `		return 0; /* not a hook stub */` |
|         - | 10224 | `	}` |
|         7 | 10225 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 10226 | `	return pProp != 0` |
|         6 | 10227 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 10228 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 10229 | `}` |
|         - | 10230 | `/*` |
|         - | 10231 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 10232 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 10233 | ` */` |
|        16 | 10234 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 10235 | `{` |
|         - | 10236 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 10237 | `	if( pMName->nByte > nPfx` |
|        12 | 10238 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 10239 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 10240 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 10241 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 10242 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 10243 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 10244 | `		return;` |
|         - | 10245 | `	}` |
|        20 | 10246 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 10247 | `}` |
|         - | 10248 | `/*` |
|         - | 10249 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 10250 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 10251 | ` */` |
|    353652 | 10252 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10253 | `{` |
|         - | 10254 | `	ph7_class_method *pMeth;` |
|         - | 10255 | `	SyHashEntry *pEntry;` |
|         - | 10256 | `	sxu32 nAbstract;` |
|         - | 10257 | `	SyBlob sMsg;` |
|         - | 10258 | `	sxi32 rc;` |
|         - | 10259 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    353657 | 10260 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15355 | 10261 | `		return SXRET_OK;` |
|         - | 10262 | `	}` |
|         - | 10263 | `	/* Count abstract methods */` |
|    338307 | 10264 | `	nAbstract = 0;` |
|    338307 | 10265 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   5007184 | 10266 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4499731 | 10267 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4499731 | 10268 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 10269 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 10270 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10271 | `			}` |
|        20 | 10272 | `			nAbstract++;` |
|         8 | 10273 | `		}` |
|         5 | 10274 | `	}` |
|    338307 | 10275 | `	if( nAbstract == 0 ){` |
|    338293 | 10276 | `		return SXRET_OK;` |
|         - | 10277 | `	}` |
|         - | 10278 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 10279 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 10280 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 10281 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 10282 | `		&pClass->sName,nAbstract,` |
|         7 | 10283 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 10284 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 10285 | `	/* Second pass: list methods with origins */` |
|         - | 10286 | `	{` |
|        18 | 10287 | `		sxu32 nListed = 0;` |
|        18 | 10288 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 10289 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 10290 | `			ph7_class *pOrigin = 0;` |
|         - | 10291 | `			SyString *pMName;` |
|        22 | 10292 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 10293 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10294 | `				continue;` |
|         - | 10295 | `			}` |
|        20 | 10296 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10297 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10298 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10299 | `			}` |
|        20 | 10300 | `			if( nListed > 0 ){` |
|         3 | 10301 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10302 | `			}` |
|         - | 10303 | `			/* Find the origin of this abstract method.` |
|         - | 10304 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10305 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10306 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10307 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10308 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10309 | `			 * class's namespace.` |
|         - | 10310 | `			 */` |
|         - | 10311 | `			{` |
|         - | 10312 | `				ph7_class **apIface;` |
|         - | 10313 | `				ph7_class **apTrait;` |
|         - | 10314 | `				ph7_class *pWalk;` |
|         - | 10315 | `				sxu32 i;` |
|         - | 10316 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10317 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10318 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10319 | `				 */` |
|        20 | 10320 | `				if( pClass->pBase ){` |
|        11 | 10321 | `					pWalk = pClass->pBase;` |
|        19 | 10322 | `					while( pWalk ){` |
|         - | 10323 | `						ph7_class_method *pParentMeth;` |
|        13 | 10324 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10325 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10326 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10327 | `							 * in this class's ancestor chain.` |
|         - | 10328 | `							 */` |
|        13 | 10329 | `							int fromIface = 0;` |
|        13 | 10330 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10331 | `							while( pAnc ){` |
|         - | 10332 | `								ph7_class **apPI;` |
|         - | 10333 | `								sxu32 j;` |
|        15 | 10334 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10335 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10336 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10337 | `										fromIface = 1;` |
|        10 | 10338 | `										break;` |
|         - | 10339 | `									}` |
|       ! 0 | 10340 | `								}` |
|        15 | 10341 | `								if( fromIface ) break;` |
|         6 | 10342 | `								pAnc = pAnc->pBase;` |
|         2 | 10343 | `							}` |
|        13 | 10344 | `							if( !fromIface ){` |
|         3 | 10345 | `								pOrigin = pWalk;` |
|         3 | 10346 | `								break;` |
|         - | 10347 | `							}` |
|         4 | 10348 | `						}` |
|        10 | 10349 | `						pWalk = pWalk->pBase;` |
|         2 | 10350 | `					}` |
|         4 | 10351 | `				}` |
|         - | 10352 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10353 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10354 | `				 */` |
|        20 | 10355 | `				if( !pOrigin ){` |
|        18 | 10356 | `					pWalk = pClass;` |
|        40 | 10357 | `					while( pWalk && !pOrigin ){` |
|        26 | 10358 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10359 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10360 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10361 | `							ph7_class *pDeepest = 0;` |
|        28 | 10362 | `							while( pIface ){` |
|        16 | 10363 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10364 | `									pDeepest = pIface;` |
|         6 | 10365 | `								}` |
|        16 | 10366 | `								pIface = pIface->pBase;` |
|         4 | 10367 | `							}` |
|        16 | 10368 | `							if( pDeepest ){` |
|        16 | 10369 | `								pOrigin = pDeepest;` |
|        16 | 10370 | `								break;` |
|         - | 10371 | `							}` |
|       ! 0 | 10372 | `						}` |
|        26 | 10373 | `						pWalk = pWalk->pBase;` |
|         4 | 10374 | `					}` |
|         7 | 10375 | `				}` |
|         - | 10376 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10377 | `				if( !pOrigin ){` |
|         3 | 10378 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10379 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10380 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10381 | `							pOrigin = pClass;` |
|         3 | 10382 | `							break;` |
|         - | 10383 | `						}` |
|       ! 0 | 10384 | `					}` |
|         1 | 10385 | `				}` |
|         - | 10386 | `			}` |
|        20 | 10387 | `			if( pOrigin ){` |
|        20 | 10388 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10389 | `			}else{` |
|         - | 10390 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10391 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10392 | `			}` |
|        20 | 10393 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10394 | `			nListed++;` |
|         4 | 10395 | `		}` |
|         - | 10396 | `	}` |
|        18 | 10397 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10398 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10399 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10400 | `	SyBlobRelease(&sMsg);` |
|        18 | 10401 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10402 | `		return SXERR_ABORT;` |
|         - | 10403 | `	}` |
|        18 | 10404 | `	return SXRET_OK;` |
|    176831 | 10405 | `}` |
|         - | 10406 | `/*` |
|         - | 10407 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10408 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10409 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10410 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10411 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10412 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10413 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10414 | ` */` |
|    415202 | 10415 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10416 | `{` |
|    415207 | 10417 | `	int isAbsolute = 0;` |
|    415207 | 10418 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10419 | `	SyBlob sName;` |
|    415207 | 10420 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4431 | 10421 | `		isAbsolute = 1;` |
|      4431 | 10422 | `		pGen->pIn++;` |
|      2213 | 10423 | `	}` |
|    415207 | 10424 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         9 | 10425 | `		pGen->pIn = pStart;` |
|         9 | 10426 | `		return SXERR_INVALID;` |
|         - | 10427 | `	}` |
|    415201 | 10428 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    415201 | 10429 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    415201 | 10430 | `	pGen->pIn++;` |
|    622827 | 10431 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    207636 | 10432 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        24 | 10433 | `		SyBlobAppend(&sName,"\\",1);` |
|        24 | 10434 | `		pGen->pIn++;` |
|        24 | 10435 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        24 | 10436 | `		pGen->pIn++;` |
|         2 | 10437 | `	}` |
|    415201 | 10438 | `	if( isAbsolute ){` |
|      4429 | 10439 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2217 | 10440 | `	}else{` |
|         - | 10441 | `		SyString sRaw;` |
|    410777 | 10442 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    410777 | 10443 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10444 | `	}` |
|    415201 | 10445 | `	SyBlobRelease(&sName);` |
|    415201 | 10446 | `	return SXRET_OK;` |
|    207606 | 10447 | `}` |
|         - | 10448 | `/*` |
|         - | 10449 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10450 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10451 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10452 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10453 | ` * either direction cannot run unbounded.` |
|         - | 10454 | ` */` |
|         - | 10455 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    164642 | 10456 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10457 | `{` |
|         - | 10458 | `	ph7_class **apParent;` |
|         - | 10459 | `	sxu32 n;` |
|    428753 | 10460 | `	while( pInterface ){` |
|    271769 | 10461 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10462 | `			return FALSE;` |
|         - | 10463 | `		}` |
|    306205 | 10464 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     68872 | 10465 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7663 | 10466 | `			return TRUE;` |
|         - | 10467 | `		}` |
|    264111 | 10468 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    264113 | 10469 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|         3 | 10470 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10471 | `				return TRUE;` |
|         - | 10472 | `			}` |
|         2 | 10473 | `		}` |
|    264111 | 10474 | `		pInterface = pInterface->pBase;` |
|    264111 | 10475 | `		iDepth++;` |
|         5 | 10476 | `	}` |
|    156989 | 10477 | `	return FALSE;` |
|     82326 | 10478 | `}` |
|    164640 | 10479 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10480 | `{` |
|    164645 | 10481 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10482 | `}` |
|         - | 10483 | `/*` |
|         - | 10484 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10485 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10486 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10487 | ` */` |
|      7658 | 10488 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10489 | `{` |
|      7667 | 10490 | `	while( pBase ){` |
|        10 | 10491 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10492 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10493 | `			return TRUE;` |
|         - | 10494 | `		}` |
|        10 | 10495 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10496 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10497 | `			return TRUE;` |
|         - | 10498 | `		}` |
|         5 | 10499 | `		pBase = pBase->pBase;` |
|         1 | 10500 | `	}` |
|      7659 | 10501 | `	return FALSE;` |
|      3834 | 10502 | `}` |
|         - | 10503 | `/*` |
|         - | 10504 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10505 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10506 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10507 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10508 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10509 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10510 | ` * pClass->aEnumCases for cases().` |
|         - | 10511 | ` */` |
|      7702 | 10512 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10513 | `{` |
|      7707 | 10514 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10515 | `	SySet *pInstrContainer;` |
|         - | 10516 | `	ph7_class_attr *pCase;` |
|         - | 10517 | `	SyString *pName;` |
|         - | 10518 | `	sxi32 rc;` |
|      7707 | 10519 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7707 | 10520 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10521 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10522 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10523 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10524 | `			return SXERR_ABORT;` |
|         - | 10525 | `		}` |
|       ! 0 | 10526 | `		goto Synchronize;` |
|         - | 10527 | `	}` |
|      7707 | 10528 | `	pName = &pGen->pIn->sData;` |
|         - | 10529 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7707 | 10530 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10531 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10532 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10533 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10534 | `			return SXERR_ABORT;` |
|         - | 10535 | `		}` |
|       ! 0 | 10536 | `		goto Synchronize;` |
|         - | 10537 | `	}` |
|      7707 | 10538 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10539 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7707 | 10540 | `	if( pCase == 0 ){` |
|       ! 0 | 10541 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10542 | `		return SXERR_ABORT;` |
|         - | 10543 | `	}` |
|      7707 | 10544 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7707 | 10545 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10546 | `		return SXERR_ABORT;` |
|         - | 10547 | `	}` |
|      7707 | 10548 | `	pGen->pIn++; /* Jump the case name */` |
|      7707 | 10549 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7693 | 10550 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10551 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10552 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10553 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10554 | `				return SXERR_ABORT;` |
|         - | 10555 | `			}` |
|         6 | 10556 | `			goto Synchronize;` |
|         - | 10557 | `		}` |
|      7689 | 10558 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10559 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10560 | `		 * (same technique as class constants). */` |
|      7689 | 10561 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7689 | 10562 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7689 | 10563 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7689 | 10564 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10565 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10566 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10567 | `		}` |
|      7689 | 10568 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7689 | 10569 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7689 | 10570 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10571 | `			return SXERR_ABORT;` |
|         - | 10572 | `		}` |
|      3847 | 10573 | `	}else{` |
|        17 | 10574 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10575 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10576 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10577 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10578 | `				return SXERR_ABORT;` |
|         - | 10579 | `			}` |
|       ! 0 | 10580 | `			goto Synchronize;` |
|         - | 10581 | `		}` |
|         - | 10582 | `	}` |
|      7703 | 10583 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7703 | 10584 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10585 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10586 | `		return SXERR_ABORT;` |
|         - | 10587 | `	}` |
|      7703 | 10588 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7703 | 10589 | `	return SXRET_OK;` |
|         2 | 10590 | `Synchronize:` |
|         - | 10591 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10592 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10593 | `		pGen->pIn++;` |
|         2 | 10594 | `	}` |
|         6 | 10595 | `	return SXERR_CORRUPT;` |
|      3856 | 10596 | `}` |
|         - | 10597 | `/*` |
|         - | 10598 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10599 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10600 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10601 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10602 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10603 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10604 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10605 | ` */` |
|      3850 | 10606 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10607 | `{` |
|         - | 10608 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10609 | `	const char *zBack;` |
|         - | 10610 | `	SySet sToken;` |
|         - | 10611 | `	char *zSrc;` |
|         - | 10612 | `	sxu32 nSrc,nMax;` |
|      3855 | 10613 | `	sxi32 rc = SXRET_OK;` |
|      3855 | 10614 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3850 | 10615 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3855 | 10616 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3855 | 10617 | `	if( zSrc == 0 ){` |
|       ! 0 | 10618 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10619 | `		return SXERR_ABORT;` |
|         - | 10620 | `	}` |
|      3855 | 10621 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3855 | 10622 | `	if( pClass->nEnumBacking != 0 ){` |
|      5762 | 10623 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10624 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10625 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10626 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1919 | 10627 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1924 | 10628 | `	}else{` |
|        21 | 10629 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10630 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10631 | `	}` |
|      3855 | 10632 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3855 | 10633 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3855 | 10634 | `	pSaveIn = pGen->pIn;` |
|      3855 | 10635 | `	pSaveEnd = pGen->pEnd;` |
|      3855 | 10636 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3855 | 10637 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15381 | 10638 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11531 | 10639 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10640 | `	}` |
|      3855 | 10641 | `	pGen->pIn = pSaveIn;` |
|      3855 | 10642 | `	pGen->pEnd = pSaveEnd;` |
|      3855 | 10643 | `	SySetRelease(&sToken);` |
|      3855 | 10644 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1930 | 10645 | `}` |
|         - | 10646 | `/*` |
|         - | 10647 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10648 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10649 | ` */` |
|         - | 10650 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10651 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10652 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10653 | `};` |
|         - | 10654 | `/*` |
|         - | 10655 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10656 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10657 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10658 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10659 | ` * and before the class is installed.` |
|         - | 10660 | ` */` |
|      3850 | 10661 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10662 | `{` |
|         - | 10663 | `	SyHashEntry *pEntry;` |
|         - | 10664 | `	sxi32 rc;` |
|         - | 10665 | `	sxu32 n;` |
|         - | 10666 | `	/* php: "Enum %s cannot include properties" */` |
|      3855 | 10667 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11557 | 10668 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7709 | 10669 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7709 | 10670 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10671 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10672 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10673 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10674 | `				return SXERR_ABORT;` |
|         - | 10675 | `			}` |
|         3 | 10676 | `			break;` |
|         - | 10677 | `		}` |
|         5 | 10678 | `	}` |
|         - | 10679 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     53905 | 10680 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     75075 | 10681 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     50055 | 10682 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10683 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10684 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10685 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10686 | `				return SXERR_ABORT;` |
|         - | 10687 | `			}` |
|       ! 0 | 10688 | `		}` |
|     25030 | 10689 | `	}` |
|         - | 10690 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10691 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10692 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10693 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10694 | `	{` |
|         - | 10695 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10696 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10697 | `		ph7_class_attr *pAttr;` |
|      3855 | 10698 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10699 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3855 | 10700 | `		if( pAttr == 0 ){` |
|       ! 0 | 10701 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10702 | `			return SXERR_ABORT;` |
|         - | 10703 | `		}` |
|      3855 | 10704 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3855 | 10705 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3855 | 10706 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3855 | 10707 | `		if( pClass->nEnumBacking != 0 ){` |
|      3843 | 10708 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10709 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3843 | 10710 | `			if( pAttr == 0 ){` |
|       ! 0 | 10711 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10712 | `				return SXERR_ABORT;` |
|         - | 10713 | `			}` |
|      3843 | 10714 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3843 | 10715 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10716 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10717 | `			}else{` |
|      3837 | 10718 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10719 | `			}` |
|      3843 | 10720 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1919 | 10721 | `		}` |
|         - | 10722 | `	}` |
|      3855 | 10723 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1930 | 10724 | `}` |
|         - | 10725 | `/*` |
|         - | 10726 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10727 | ` *` |
|         - | 10728 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10729 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10730 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10731 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10732 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10733 | ` * implements, body, install) is shared by both paths.` |
|         - | 10734 | ` */` |
|    353696 | 10735 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10736 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10737 | `{` |
|    353701 | 10738 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10739 | `	ph7_class *pClass,*pBase;` |
|         - | 10740 | `	SyToken *pEnd,*pTmp;` |
|         - | 10741 | `	sxi32 iProtection;` |
|         - | 10742 | `	SySet aInterfaces;` |
|         - | 10743 | `	SySet aUseEntries;` |
|         - | 10744 | `	sxi32 iAttrflags;` |
|         - | 10745 | `	SyString *pName;` |
|         - | 10746 | `	sxi32 nKwrd;` |
|         - | 10747 | `	sxi32 rc;` |
|         - | 10748 | `	/* Jump the 'class' keyword */` |
|    353701 | 10749 | `	pGen->pIn++;` |
|    353701 | 10750 | `	if( pAnonName ){` |
|         - | 10751 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10752 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10753 | `		 * then use the synthesized name. */` |
|        32 | 10754 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10755 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10756 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10757 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10758 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10759 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10760 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10761 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10762 | `		}` |
|        32 | 10763 | `		pName = pAnonName;` |
|        32 | 10764 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10765 | `	}else{` |
|    353673 | 10766 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10767 | `			/* Syntax error */` |
|       ! 0 | 10768 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10769 | `			if( rc == SXERR_ABORT ){` |
|         - | 10770 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10771 | `				return SXERR_ABORT;` |
|         - | 10772 | `			}` |
|         - | 10773 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10774 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10775 | `				pGen->pIn++;` |
|       ! 0 | 10776 | `			}` |
|       ! 0 | 10777 | `			return SXRET_OK;` |
|         - | 10778 | `		}` |
|         - | 10779 | `		/* Extract class name */` |
|    353673 | 10780 | `		pName = &pGen->pIn->sData;` |
|         - | 10781 | `		/* Advance the stream cursor */` |
|    353673 | 10782 | `		pGen->pIn++;` |
|         - | 10783 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10784 | `			SyBlob sFQN;` |
|         - | 10785 | `			SyString sFQNStr;` |
|    353673 | 10786 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    353673 | 10787 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    353673 | 10788 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    353673 | 10789 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    353673 | 10790 | `			SyBlobRelease(&sFQN);` |
|         - | 10791 | `		}` |
|         - | 10792 | `	}` |
|    353701 | 10793 | `	if( pClass == 0 ){` |
|       ! 0 | 10794 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10795 | `		return SXERR_ABORT;` |
|         - | 10796 | `	}` |
|    353696 | 10797 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3859 | 10798 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10799 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3845 | 10800 | `		pGen->pIn++; /* Jump ':' */` |
|      3840 | 10801 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3845 | 10802 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10803 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10804 | `			pGen->pIn++;` |
|      3838 | 10805 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3839 | 10806 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3837 | 10807 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3837 | 10808 | `			pGen->pIn++;` |
|      1921 | 10809 | `		}else{` |
|         3 | 10810 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10811 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10812 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10813 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10814 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10815 | `				return SXERR_ABORT;` |
|         - | 10816 | `			}` |
|         3 | 10817 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10818 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10819 | `			}` |
|         - | 10820 | `		}` |
|      1920 | 10821 | `	}` |
|    353701 | 10822 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    353701 | 10823 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10824 | `		return SXERR_ABORT;` |
|         - | 10825 | `	}` |
|         - | 10826 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    353701 | 10827 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    353701 | 10828 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10829 | `	/* Assume a standalone class */` |
|    353701 | 10830 | `	pBase = 0;` |
|    353701 | 10831 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    287265 | 10832 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    287265 | 10833 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10834 | `			SyBlob sResolved;` |
|         - | 10835 | `			SyString sBaseName;` |
|         - | 10836 | `			sxu32 nRefLine;` |
|    183837 | 10837 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10838 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10839 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10840 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10841 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10842 | `					return SXERR_ABORT;` |
|         - | 10843 | `				}` |
|       ! 0 | 10844 | `			}` |
|    183837 | 10845 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    183837 | 10846 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    183837 | 10847 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    183837 | 10848 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10849 | `				SyBlobRelease(&sResolved);` |
|         4 | 10850 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10851 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10852 | `					pName);` |
|         3 | 10853 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10854 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10855 | `					return SXERR_ABORT;` |
|         - | 10856 | `				}` |
|         3 | 10857 | `				return SXRET_OK;` |
|         - | 10858 | `			}` |
|    275750 | 10859 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    183830 | 10860 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    183835 | 10861 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10862 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10863 | `			/* Interfaces are not allowed */` |
|    183835 | 10864 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10865 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10866 | `			}` |
|    183835 | 10867 | `			if( pBase == 0 ){` |
|       ! 0 | 10868 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10869 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10870 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10871 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10872 | `					return SXERR_ABORT;` |
|         - | 10873 | `				}` |
|       ! 0 | 10874 | `			}else{` |
|    183835 | 10875 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10876 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10877 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10878 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10879 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10880 | `						return SXERR_ABORT;` |
|         - | 10881 | `					}` |
|         3 | 10882 | `					pBase = 0; /* Never inherit from an enum */` |
|    183834 | 10883 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10884 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10885 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10886 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10887 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10888 | `						return SXERR_ABORT;` |
|         - | 10889 | `					}` |
|       ! 0 | 10890 | `				}` |
|         - | 10891 | `			}` |
|    183835 | 10892 | `			SyBlobRelease(&sResolved);` |
|    183835 | 10893 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10894 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10895 | `			}` |
|     91915 | 10896 | `		}` |
|    287263 | 10897 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10898 | `			ph7_class *pInterface;` |
|         - | 10899 | `			/* Interface implementation */` |
|    107271 | 10900 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    111007 | 10901 | `			for(;;){` |
|         - | 10902 | `				SyBlob sResolved;` |
|         - | 10903 | `				SyString sIntName;` |
|         - | 10904 | `				sxu32 nRefLine;` |
|    164645 | 10905 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    164645 | 10906 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    164645 | 10907 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10908 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10909 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10910 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10911 | `						pName);` |
|       ! 0 | 10912 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10913 | `						return SXERR_ABORT;` |
|         - | 10914 | `					}` |
|       ! 0 | 10915 | `					break;` |
|         - | 10916 | `				}` |
|    329285 | 10917 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    164640 | 10918 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    164645 | 10919 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10920 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10921 | `				/* Only interfaces are allowed */` |
|    164645 | 10922 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10923 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10924 | `				}` |
|    164645 | 10925 | `				if( pInterface == 0 ){` |
|       ! 0 | 10926 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10927 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10928 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10929 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10930 | `						return SXERR_ABORT;` |
|         - | 10931 | `					}` |
|       ! 0 | 10932 | `				}else{` |
|         - | 10933 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10934 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10935 | `					 * unless they already extend Exception or Error.` |
|         - | 10936 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10937 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10938 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    164645 | 10939 | `					SyString *pFqn = &pClass->sName;` |
|    164645 | 10940 | `					int bIsExceptionOrError =` |
|     86148 | 10941 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    248876 | 10942 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    162735 | 10943 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3838 | 10944 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    168469 | 10945 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11490 | 10946 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3827 | 10947 | `						!bIsExceptionOrError ){` |
|        12 | 10948 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10949 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10950 | `							&pClass->sName);` |
|         9 | 10951 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10952 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10953 | `							return SXERR_ABORT;` |
|         - | 10954 | `						}` |
|         - | 10955 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10956 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10957 | `					}else{` |
|    164639 | 10958 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10959 | `					}` |
|         - | 10960 | `				}` |
|    164645 | 10961 | `				SyBlobRelease(&sResolved);` |
|    164645 | 10962 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53638 | 10963 | `					break;` |
|         - | 10964 | `				}` |
|     57379 | 10965 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10966 | `			}` |
|     53633 | 10967 | `		}` |
|    143629 | 10968 | `	}` |
|    353699 | 10969 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10970 | `		/* Syntax error */` |
|       ! 0 | 10971 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10972 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10973 | `		if( rc == SXERR_ABORT ){` |
|         - | 10974 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10975 | `			return SXERR_ABORT;` |
|         - | 10976 | `		}` |
|       ! 0 | 10977 | `		return SXRET_OK;` |
|         - | 10978 | `	}` |
|    353699 | 10979 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    353699 | 10980 | `	pEnd = 0; /* cc warning */` |
|         - | 10981 | `	/* Delimit the class body */` |
|    353699 | 10982 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    353699 | 10983 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10984 | `		/* Syntax error */` |
|       ! 0 | 10985 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10986 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10987 | `		if( rc == SXERR_ABORT ){` |
|         - | 10988 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10989 | `			return SXERR_ABORT;` |
|         - | 10990 | `		}` |
|       ! 0 | 10991 | `		return SXRET_OK;` |
|         - | 10992 | `	}` |
|         - | 10993 | `	/* The delimiter token is the class body's closing brace */` |
|    353699 | 10994 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10995 | `	/* Swap token stream */` |
|    353699 | 10996 | `	pTmp = pGen->pEnd;` |
|    353699 | 10997 | `	pGen->pEnd = pEnd;` |
|         - | 10998 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    353699 | 10999 | `	pClass->iFlags \|= iFlags;` |
|         - | 11000 | `	/* Start the parse process */` |
|   1372722 | 11001 | `	for(;;){` |
|         - | 11002 | `		/* Jump leading/trailing semi-colons */` |
|   3910259 | 11003 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    704815 | 11004 | `			pGen->pIn++;` |
|         5 | 11005 | `		}` |
|   3205449 | 11006 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 11007 | `			/* End of class body */` |
|    353657 | 11008 | `			break;` |
|         - | 11009 | `		}` |
|         - | 11010 | `		/* Bind a directly-preceding docblock to this member */` |
|   2851797 | 11011 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2851792 | 11012 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1425901 | 11013 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 11014 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11015 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11016 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11017 | `			if( rc == SXERR_ABORT ){` |
|         - | 11018 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 11019 | `				return SXERR_ABORT;` |
|         - | 11020 | `			}` |
|       ! 0 | 11021 | `			goto done;` |
|         - | 11022 | `		}` |
|         - | 11023 | `		/* Assume public visibility */` |
|   2851797 | 11024 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2851797 | 11025 | `		iAttrflags = 0;` |
|         - | 11026 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 11027 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 11028 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 11029 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2851797 | 11030 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11031 | `			int bMod = 0;` |
|       ! 0 | 11032 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11033 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 11034 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 11035 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 11036 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 11037 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 11038 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 11039 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 11040 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 11041 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 11042 | `			}` |
|       ! 0 | 11043 | `			if( !bMod ){` |
|       ! 0 | 11044 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11045 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11046 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11047 | `						return SXERR_ABORT;` |
|         - | 11048 | `					}` |
|       ! 0 | 11049 | `					goto done;` |
|         - | 11050 | `				}` |
|       ! 0 | 11051 | `				continue;` |
|         - | 11052 | `			}` |
|       ! 0 | 11053 | `		}` |
|   2851797 | 11054 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11055 | `			/* Extract the current keyword */` |
|   2851797 | 11056 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2851797 | 11057 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 11058 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7707 | 11059 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7707 | 11060 | `				if( rc != SXRET_OK ){` |
|         6 | 11061 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11062 | `						return SXERR_ABORT;` |
|         - | 11063 | `					}` |
|         6 | 11064 | `					goto done;` |
|         - | 11065 | `				}` |
|      7703 | 11066 | `				continue;` |
|         - | 11067 | `			}` |
|   2844095 | 11068 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11069 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 11070 | `				TraitUseEntry sUse;` |
|     15381 | 11071 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15381 | 11072 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15381 | 11073 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7696 | 11074 | `				for(;;){` |
|         - | 11075 | `					ph7_class *pTrait;` |
|         - | 11076 | `					SyBlob sResolved;` |
|         - | 11077 | `					SyString sTraitName;` |
|     15389 | 11078 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|         - | 11079 | `					/* A trait name is a full class reference: it may be qualified or` |
|         - | 11080 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|         - | 11081 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|         - | 11082 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|         - | 11083 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|         - | 11084 | `					 * choked on the first '\'. */` |
|     15389 | 11085 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15389 | 11086 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 11087 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 11088 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       ! 0 | 11089 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 11090 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11091 | `							return SXERR_ABORT;` |
|         - | 11092 | `						}` |
|       ! 0 | 11093 | `						break;` |
|         - | 11094 | `					}` |
|     30773 | 11095 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15384 | 11096 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15389 | 11097 | `					SyStringInitFromBuf(&sTraitName,` |
|         - | 11098 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 11099 | `					/* Only traits are allowed */` |
|     15389 | 11100 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11101 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 11102 | `					}` |
|     15389 | 11103 | `					if( pTrait == 0 ){` |
|       ! 0 | 11104 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|         - | 11105 | `							"'%z' is not a trait",&sTraitName);` |
|       ! 0 | 11106 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11107 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 11108 | `							return SXERR_ABORT;` |
|         - | 11109 | `						}` |
|       ! 0 | 11110 | `					}else{` |
|     15389 | 11111 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 11112 | `					}` |
|     15389 | 11113 | `					SyBlobRelease(&sResolved);` |
|         - | 11114 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|         - | 11115 | `					 * continue only across a comma-separated trait list. */` |
|     15389 | 11116 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7693 | 11117 | `						break;` |
|         - | 11118 | `					}` |
|        10 | 11119 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 11120 | `				}` |
|         - | 11121 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15381 | 11122 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 11123 | `					SyToken *pBlock;` |
|        13 | 11124 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 11125 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 11126 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 11127 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 11128 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 11129 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 11130 | `					}else{` |
|       ! 0 | 11131 | `						pGen->pIn = pGen->pEnd;` |
|         - | 11132 | `					}` |
|         5 | 11133 | `				}` |
|     15381 | 11134 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 11135 | `				/* The semicolon will be consumed by the outer loop */` |
|     15381 | 11136 | `				continue;` |
|         - | 11137 | `			}` |
|   2828719 | 11138 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 11139 | `				int nSetTok;` |
|   2583481 | 11140 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2583481 | 11141 | `				if( nSetVis ){` |
|         - | 11142 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 11143 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 11144 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11145 | `					pGen->pIn += nSetTok;` |
|         2 | 11146 | `				}else{` |
|   2583479 | 11147 | `					iProtection = nKwrd;` |
|   2583479 | 11148 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 11149 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 11150 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2583479 | 11151 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2583479 | 11152 | `					if( nSetVis ){` |
|         9 | 11153 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 11154 | `						pGen->pIn += nSetTok;` |
|         4 | 11155 | `					}` |
|         - | 11156 | `				}` |
|         - | 11157 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 11158 | ``				 * `public private(set) readonly int $x`. */`` |
|   2583481 | 11159 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        26 | 11160 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        26 | 11161 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        11 | 11162 | `				}` |
|   2583476 | 11163 | `				if( pGen->pIn >= pGen->pEnd` |
|   2583481 | 11164 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11165 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11166 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11167 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11168 | `					if( rc == SXERR_ABORT ){` |
|         - | 11169 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 11170 | `						return SXERR_ABORT;` |
|         - | 11171 | `					}` |
|       ! 0 | 11172 | `					goto done;` |
|         - | 11173 | `				}` |
|   2583481 | 11174 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11175 | `					/* Attribute declaration (untyped) */` |
|    409801 | 11176 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    409801 | 11177 | `					if( rc != SXRET_OK ){` |
|        11 | 11178 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11179 | `							return SXERR_ABORT;` |
|         - | 11180 | `						}` |
|        11 | 11181 | `						goto done;` |
|         - | 11182 | `					}` |
|    409946 | 11183 | `					continue;` |
|         - | 11184 | `				}` |
|   2173685 | 11185 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11186 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       317 | 11187 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       317 | 11188 | `					if( rc != SXRET_OK ){` |
|         8 | 11189 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11190 | `							return SXERR_ABORT;` |
|         - | 11191 | `						}` |
|         8 | 11192 | `						goto done;` |
|         - | 11193 | `					}` |
|       311 | 11194 | `					continue;` |
|         - | 11195 | `				}` |
|         - | 11196 | `				/* Extract the keyword */` |
|   2173373 | 11197 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1086684 | 11198 | `			}` |
|   2418611 | 11199 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 11200 | `				/* Process constant declaration */` |
|    237253 | 11201 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    237253 | 11202 | `				if( rc != SXRET_OK ){` |
|        11 | 11203 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11204 | `						return SXERR_ABORT;` |
|         - | 11205 | `					}` |
|        11 | 11206 | `					goto done;` |
|         - | 11207 | `				}` |
|    118625 | 11208 | `			}else{` |
|   2181363 | 11209 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 11210 | `					/* Static method or attribute,record that */` |
|     95787 | 11211 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     95787 | 11212 | `					pGen->pIn++; /* Jump the static keyword */` |
|     95787 | 11213 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11214 | `						int nSetTok;` |
|     68981 | 11215 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     68981 | 11216 | `						if( nSetVis ){` |
|         - | 11217 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 11218 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11219 | `							pGen->pIn += nSetTok;` |
|         2 | 11220 | `						}else{` |
|         - | 11221 | `							/* Extract the keyword */` |
|     68979 | 11222 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     68979 | 11223 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11224 | `								iProtection = nKwrd;` |
|       ! 0 | 11225 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 11226 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 11227 | `								if( nSetVis ){` |
|       ! 0 | 11228 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 11229 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 11230 | `								}` |
|       ! 0 | 11231 | `							}` |
|         - | 11232 | `						}` |
|     34488 | 11233 | `					}` |
|         - | 11234 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 11235 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 11236 | `					 * than a generic "expecting method" parse error. */` |
|     95787 | 11237 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11238 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11239 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 11240 | `					}` |
|     95782 | 11241 | `					if( pGen->pIn >= pGen->pEnd` |
|     95787 | 11242 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11243 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11244 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 11245 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11246 | `						if( rc == SXERR_ABORT ){` |
|         - | 11247 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11248 | `							return SXERR_ABORT;` |
|         - | 11249 | `						}` |
|       ! 0 | 11250 | `						goto done;` |
|         - | 11251 | `					}` |
|     95787 | 11252 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11253 | `						/* Attribute declaration */` |
|     26807 | 11254 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26807 | 11255 | `						if( rc != SXRET_OK ){` |
|         3 | 11256 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11257 | `								return SXERR_ABORT;` |
|         - | 11258 | `							}` |
|         3 | 11259 | `							goto done;` |
|         - | 11260 | `						}` |
|     26805 | 11261 | `						continue;` |
|         - | 11262 | `					}` |
|     68985 | 11263 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11264 | `						/* Typed static attribute declaration */` |
|        19 | 11265 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        19 | 11266 | `						if( rc != SXRET_OK ){` |
|         3 | 11267 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11268 | `								return SXERR_ABORT;` |
|         - | 11269 | `							}` |
|         3 | 11270 | `							goto done;` |
|         - | 11271 | `						}` |
|        17 | 11272 | `						continue;` |
|         - | 11273 | `					}` |
|         - | 11274 | `					/* Extract the keyword */` |
|     68969 | 11275 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2120063 | 11276 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 11277 | `					/* Abstract method,record that */` |
|      7675 | 11278 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 11279 | `					/* Mark the whole class as abstract */` |
|      7675 | 11280 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 11281 | `					/* Advance the stream cursor */` |
|      7675 | 11282 | `					pGen->pIn++;` |
|      7675 | 11283 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7675 | 11284 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7675 | 11285 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7673 | 11286 | `							iProtection = nKwrd;` |
|      7673 | 11287 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3834 | 11288 | `						}` |
|      3835 | 11289 | `					}` |
|      7675 | 11290 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7670 | 11291 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11292 | `							/* Static method */` |
|       ! 0 | 11293 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11294 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11295 | `					}` |
|      7675 | 11296 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7670 | 11297 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 11298 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 11299 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 11300 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 11301 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11302 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11303 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11304 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11305 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11306 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11307 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11308 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11309 | `										return SXERR_ABORT;` |
|         - | 11310 | `									}` |
|       ! 0 | 11311 | `									goto done;` |
|         - | 11312 | `								}` |
|         7 | 11313 | `								continue;` |
|         - | 11314 | `							}` |
|       ! 0 | 11315 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11316 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11317 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11318 | `							if( rc == SXERR_ABORT ){` |
|         - | 11319 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11320 | `								return SXERR_ABORT;` |
|         - | 11321 | `							}` |
|       ! 0 | 11322 | `							goto done;` |
|         - | 11323 | `					}` |
|      7669 | 11324 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2081743 | 11325 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11326 | `					/* final method ,record that */` |
|        21 | 11327 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        21 | 11328 | `					pGen->pIn++; /* Jump the final keyword */` |
|        21 | 11329 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11330 | `						/* Extract the keyword */` |
|        21 | 11331 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        21 | 11332 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        11 | 11333 | `							iProtection = nKwrd;` |
|        11 | 11334 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11335 | `						}` |
|         9 | 11336 | `					}` |
|        21 | 11337 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11338 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11339 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11340 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11341 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11342 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11343 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11344 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11345 | `									return SXERR_ABORT;` |
|         - | 11346 | `								}` |
|       ! 0 | 11347 | `								goto done;` |
|         - | 11348 | `							}` |
|        14 | 11349 | `							continue;` |
|         - | 11350 | `					}` |
|         9 | 11351 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11352 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11353 | `							/* Static method */` |
|       ! 0 | 11354 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11355 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11356 | `					}` |
|         9 | 11357 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11358 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11359 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11360 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11361 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11362 | `							if( rc == SXERR_ABORT ){` |
|         - | 11363 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11364 | `								return SXERR_ABORT;` |
|         - | 11365 | `							}` |
|       ! 0 | 11366 | `							goto done;` |
|         - | 11367 | `					}` |
|         9 | 11368 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11369 | `				}` |
|   2154527 | 11370 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11371 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11372 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11373 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11374 | `						if( rc == SXERR_ABORT ){` |
|         - | 11375 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11376 | `							return SXERR_ABORT;` |
|         - | 11377 | `						}` |
|       ! 0 | 11378 | `						goto done;` |
|         - | 11379 | `				}` |
|   2154527 | 11380 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11381 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11382 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11383 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11384 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11385 | `						if( rc == SXERR_ABORT ){` |
|         - | 11386 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11387 | `							return SXERR_ABORT;` |
|         - | 11388 | `						}` |
|       ! 0 | 11389 | `						goto done;` |
|         - | 11390 | `					}` |
|         - | 11391 | `					/* Attribute declaration */` |
|         7 | 11392 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11393 | `				}else{` |
|         - | 11394 | `					/* Process method declaration */` |
|   2154521 | 11395 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11396 | `				}` |
|   2154527 | 11397 | `				if( rc != SXRET_OK ){` |
|        16 | 11398 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11399 | `						return SXERR_ABORT;` |
|         - | 11400 | `					}` |
|        16 | 11401 | `					goto done;` |
|         - | 11402 | `				}` |
|         - | 11403 | `			}` |
|   1195880 | 11404 | `		}else{` |
|         - | 11405 | `			/* Attribute declaration */` |
|       ! 0 | 11406 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11407 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11408 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11409 | `					return SXERR_ABORT;` |
|         - | 11410 | `				}` |
|       ! 0 | 11411 | `				goto done;` |
|         - | 11412 | `			}` |
|         - | 11413 | `		}` |
|         5 | 11414 | `	}` |
|         - | 11415 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11416 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11417 | `	 */` |
|         - | 11418 | `	{` |
|         - | 11419 | `		TraitUseEntry *apUse;` |
|         - | 11420 | `		sxu32 nU;` |
|    353657 | 11421 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    369033 | 11422 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15381 | 11423 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15381 | 11424 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15381 | 11425 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15381 | 11426 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11427 | `			sxu32 nT;` |
|     15381 | 11428 | `			if( !hasResolution ){` |
|         - | 11429 | `				/* No conflict resolution block: use standard trait application */` |
|     30743 | 11430 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15377 | 11431 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15377 | 11432 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11433 | `						break;` |
|         - | 11434 | `					}` |
|      7691 | 11435 | `				}` |
|      7688 | 11436 | `			}else{` |
|         - | 11437 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11438 | `				 * then use the block to resolve method conflicts.` |
|         - | 11439 | `				 */` |
|         - | 11440 | `				SyToken *pR;` |
|        25 | 11441 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11442 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11443 | `					ph7_class_attr *pAR;` |
|         - | 11444 | `					SyHashEntry *pER;` |
|         - | 11445 | `					SyString *pNR;` |
|        15 | 11446 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11447 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11448 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11449 | `						pNR = &pAR->sName;` |
|       ! 0 | 11450 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11451 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11452 | `						}` |
|       ! 0 | 11453 | `					}` |
|        15 | 11454 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11455 | `				}` |
|         - | 11456 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11457 | `				pR = pUse->pResolvStart;` |
|        27 | 11458 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11459 | `					SyString sTrait,sMethod;` |
|         - | 11460 | `					ph7_class *pSrcTrait;` |
|         - | 11461 | `					ph7_class_method *pMeth;` |
|         - | 11462 | `					sxi32 nRKwrd;` |
|        41 | 11463 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11464 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11465 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11466 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11467 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11468 | `					sMethod = pR->sData;` |
|        17 | 11469 | `					pR++;` |
|        17 | 11470 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11471 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11472 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11473 | `							sTrait = sMethod;` |
|         7 | 11474 | `							pR++;` |
|         7 | 11475 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11476 | `							sMethod = pR->sData;` |
|         7 | 11477 | `							pR++;` |
|         3 | 11478 | `						}` |
|         3 | 11479 | `					}` |
|        17 | 11480 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11481 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11482 | `						continue;` |
|         - | 11483 | `					}` |
|        17 | 11484 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11485 | `					pR++;` |
|        17 | 11486 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11487 | `						pSrcTrait = 0;` |
|         7 | 11488 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11489 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11490 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11491 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11492 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11493 | `								break;` |
|         - | 11494 | `							}` |
|         2 | 11495 | `						}` |
|         5 | 11496 | `						if( pSrcTrait ){` |
|         5 | 11497 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11498 | `							if( pMeth ){` |
|         5 | 11499 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11500 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11501 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11502 | `								}` |
|         2 | 11503 | `							}` |
|         2 | 11504 | `						}` |
|         2 | 11505 | `					}` |
|        35 | 11506 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11507 | `				}` |
|         - | 11508 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11509 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11510 | `					ph7_class_method *pMR;` |
|         - | 11511 | `					SyHashEntry *pER;` |
|         - | 11512 | `					SyString *pNR;` |
|        15 | 11513 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11514 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11515 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11516 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11517 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11518 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11519 | `						}` |
|         3 | 11520 | `					}` |
|         9 | 11521 | `				}` |
|         - | 11522 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11523 | `				pR = pUse->pResolvStart;` |
|        27 | 11524 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11525 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11526 | `					ph7_class *pSrcTrait;` |
|         - | 11527 | `					ph7_class_method *pMeth;` |
|        27 | 11528 | `					int hasQual = 0;` |
|         - | 11529 | `					sxi32 nRKwrd;` |
|        41 | 11530 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11531 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11532 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11533 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11534 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11535 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11536 | `					sMethod = pR->sData;` |
|        17 | 11537 | `					pR++;` |
|        17 | 11538 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11539 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11540 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11541 | `							sTrait = sMethod;` |
|         7 | 11542 | `							hasQual = 1;` |
|         7 | 11543 | `							pR++;` |
|         7 | 11544 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11545 | `							sMethod = pR->sData;` |
|         7 | 11546 | `							pR++;` |
|         3 | 11547 | `						}` |
|         3 | 11548 | `					}` |
|        17 | 11549 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11550 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11551 | `						continue;` |
|         - | 11552 | `					}` |
|        17 | 11553 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11554 | `					pR++;` |
|        17 | 11555 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11556 | `						sxi32 iNewVis = -1;` |
|        13 | 11557 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11558 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11559 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11560 | `								iNewVis = nAK;` |
|         7 | 11561 | `								pR++;` |
|         3 | 11562 | `							}` |
|         3 | 11563 | `						}` |
|        13 | 11564 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11565 | `							sAlias = pR->sData;` |
|        11 | 11566 | `							pR++;` |
|         4 | 11567 | `						}` |
|        13 | 11568 | `						pMeth = 0;` |
|        13 | 11569 | `						if( hasQual ){` |
|         3 | 11570 | `							pSrcTrait = 0;` |
|         5 | 11571 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11572 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11573 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11574 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11575 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11576 | `									break;` |
|         - | 11577 | `								}` |
|         2 | 11578 | `							}` |
|         3 | 11579 | `							if( pSrcTrait ){` |
|         3 | 11580 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11581 | `							}` |
|         2 | 11582 | `						}else{` |
|        10 | 11583 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11584 | `						}` |
|        13 | 11585 | `						if( pMeth ){` |
|        13 | 11586 | `							if( sAlias.nByte > 0 ){` |
|         - | 11587 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11588 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11589 | `								 */` |
|         - | 11590 | `								ph7_class_method *pAlias;` |
|         - | 11591 | `								char *zAliasDup;` |
|        11 | 11592 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11593 | `								if( pAlias ){` |
|        11 | 11594 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11595 | `									if( iNewVis >= 0 ){` |
|         5 | 11596 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11597 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11598 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11599 | `									}` |
|        11 | 11600 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11601 | `									if( zAliasDup ){` |
|        11 | 11602 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11603 | `									}` |
|         7 | 11604 | `								}` |
|         7 | 11605 | `							}else if( iNewVis >= 0 ){` |
|         - | 11606 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11607 | `								ph7_class_method *pCopy;` |
|         3 | 11608 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11609 | `								if( pCopy ){` |
|         3 | 11610 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11611 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11612 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11613 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11614 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11615 | `									/* Replace the method in the class hash */` |
|         3 | 11616 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11617 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11618 | `								}` |
|         1 | 11619 | `							}` |
|         5 | 11620 | `						}` |
|         5 | 11621 | `						SXUNUSED(hasQual);` |
|         5 | 11622 | `					}` |
|        21 | 11623 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11624 | `				}` |
|         - | 11625 | `			}` |
|     15381 | 11626 | `			SySetRelease(&pUse->aTraits);` |
|      7693 | 11627 | `		}` |
|         - | 11628 | `	}` |
|    353657 | 11629 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11630 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11631 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3855 | 11632 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3855 | 11633 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11634 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11635 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11636 | `			return SXERR_ABORT;` |
|         - | 11637 | `		}` |
|      1925 | 11638 | `	}` |
|         - | 11639 | `	/* Install the class */` |
|    353657 | 11640 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    353657 | 11641 | `	if( rc == SXRET_OK ){` |
|         - | 11642 | `		ph7_class **apInterface;` |
|         - | 11643 | `		sxu32 n;` |
|    353657 | 11644 | `		if( pBase ){` |
|         - | 11645 | `			/* Inherit from base class and mark as a subclass */` |
|    183833 | 11646 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     91914 | 11647 | `		}` |
|    353657 | 11648 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    518291 | 11649 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11650 | `			/* Implements one or more interface */` |
|    164639 | 11651 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    164639 | 11652 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11653 | `				break;` |
|         - | 11654 | `			}` |
|     82322 | 11655 | `		}` |
|         - | 11656 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11657 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    353657 | 11658 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3855 | 11659 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3855 | 11660 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11661 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11662 | `			}` |
|      3855 | 11663 | `			if( pIntf ){` |
|      3855 | 11664 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1925 | 11665 | `			}` |
|      3855 | 11666 | `			if( pClass->nEnumBacking != 0 ){` |
|      3843 | 11667 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3843 | 11668 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11669 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11670 | `				}` |
|      3843 | 11671 | `				if( pIntf ){` |
|      3843 | 11672 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1919 | 11673 | `				}` |
|      1919 | 11674 | `			}` |
|      1925 | 11675 | `		}` |
|         - | 11676 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11677 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    353652 | 11678 | `		if( rc == SXRET_OK` |
|    353652 | 11679 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    353657 | 11680 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    187487 | 11681 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11682 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    187487 | 11683 | `			if( pStringable ){` |
|    187487 | 11684 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    187487 | 11685 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11686 | `				sxu32 i;` |
|    187487 | 11687 | `				int bAlready = 0;` |
|    225731 | 11688 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     42075 | 11689 | `					if( apImpl[i] == pStringable ){` |
|      3831 | 11690 | `						bAlready = 1;` |
|      3831 | 11691 | `						break;` |
|         - | 11692 | `					}` |
|     19127 | 11693 | `				}` |
|    187487 | 11694 | `				if( !bAlready ){` |
|    183661 | 11695 | `					PH7_ClassImplement(pClass,pStringable);` |
|     91828 | 11696 | `				}` |
|     93741 | 11697 | `			}` |
|     93741 | 11698 | `		}` |
|         - | 11699 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    353657 | 11700 | `		if( rc == SXRET_OK ){` |
|    353657 | 11701 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    353657 | 11702 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11703 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11704 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11705 | `				return SXERR_ABORT;` |
|         - | 11706 | `			}` |
|    176826 | 11707 | `		}` |
|         - | 11708 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    353657 | 11709 | `		if( rc == SXRET_OK ){` |
|    353657 | 11710 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    353657 | 11711 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11712 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11713 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11714 | `				return SXERR_ABORT;` |
|         - | 11715 | `			}` |
|    176826 | 11716 | `		}` |
|    176826 | 11717 | `	}` |
|    353657 | 11718 | `	SySetRelease(&aUseEntries);` |
|    353657 | 11719 | `	SySetRelease(&aInterfaces);` |
|    353657 | 11720 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11721 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11722 | `		return SXERR_ABORT;` |
|         - | 11723 | `	}` |
|    176826 | 11724 | `done:` |
|         - | 11725 | `	/* Point beyond the class body */` |
|    353699 | 11726 | `	pGen->pIn = &pEnd[1];` |
|    353699 | 11727 | `	pGen->pEnd = pTmp;` |
|    353699 | 11728 | `	return PH7_OK;` |
|    176853 | 11729 | `}` |
|         - | 11730 | `/* Compile a named class declaration (the common case). */` |
|    353668 | 11731 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11732 | `{` |
|    353673 | 11733 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11734 | `}` |
|         - | 11735 | `/*` |
|         - | 11736 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11737 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11738 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11739 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11740 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11741 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11742 | ` */` |
|        28 | 11743 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11744 | `{` |
|         - | 11745 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11746 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11747 | `	SyString sName;` |
|         - | 11748 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11749 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11750 | `	                              * is keyed to this 'class' token */` |
|         - | 11751 | `	ph7_value *pObj;` |
|        32 | 11752 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11753 | `	sxu32 nIdx,nLen;` |
|         - | 11754 | `	sxi32 nArg,rc;` |
|        14 | 11755 | `	SXUNUSED(iCompileFlag);` |
|         - | 11756 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11757 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11758 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11759 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11760 | `	}` |
|        32 | 11761 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11762 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11763 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11764 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11765 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11766 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11767 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11768 | `		return rc;` |
|         - | 11769 | `	}` |
|         - | 11770 | `	{` |
|         - | 11771 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11772 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11773 | `		if( pAnonClass` |
|        32 | 11774 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11775 | `			return SXERR_ABORT;` |
|         - | 11776 | `		}` |
|         - | 11777 | `	}` |
|         - | 11778 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11779 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11780 | `	nArg = 0;` |
|        32 | 11781 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11782 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11783 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11784 | `		SyToken *pArgNext;` |
|         7 | 11785 | `		pGen->pIn = pArgStart;` |
|         7 | 11786 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11787 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11788 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11789 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11790 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11791 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11792 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11793 | `					return SXERR_ABORT;` |
|         - | 11794 | `				}` |
|         7 | 11795 | `				nArg++;` |
|         3 | 11796 | `			}` |
|         7 | 11797 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11798 | `		}` |
|         7 | 11799 | `		pGen->pIn = pSavedIn;` |
|         7 | 11800 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11801 | `	}` |
|         - | 11802 | `	/* Load the synthesized class name */` |
|        32 | 11803 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11804 | `	if( pObj == 0 ){` |
|       ! 0 | 11805 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11806 | `		return SXERR_ABORT;` |
|         - | 11807 | `	}` |
|        32 | 11808 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11809 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11810 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11811 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11812 | `	return SXRET_OK;` |
|        18 | 11813 | `}` |
|         - | 11814 | `/*` |
|         - | 11815 | ` * Compile a user-defined abstract class.` |
|         - | 11816 | ` *  According to the PHP language reference manual` |
|         - | 11817 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11818 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11819 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11820 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11821 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11822 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11823 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11824 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11825 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11826 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11827 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11828 | ` *   could differ.` |
|         - | 11829 | ` */` |
|         - | 11830 | `/*` |
|         - | 11831 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11832 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11833 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11834 | ` */` |
|  12874180 | 11835 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11836 | `{` |
|  12874185 | 11837 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   7544011 | 11838 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   7544011 | 11839 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   7498085 | 11840 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3733691 | 11841 | `	}` |
|  12797561 | 11842 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  12797501 | 11843 | `	return FALSE;` |
|   6437095 | 11844 | `}` |
|         - | 11845 | `/*` |
|         - | 11846 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11847 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11848 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11849 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11850 | ` */` |
|  12797496 | 11851 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11852 | `{` |
|  12797501 | 11853 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  12797501 | 11854 | `	sxi32 iFlags = 0,iFlag;` |
|  12874185 | 11855 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76689 | 11856 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11857 | `			pDup = pIn;` |
|         2 | 11858 | `		}` |
|     76689 | 11859 | `		iFlags \|= iFlag;` |
|     76689 | 11860 | `		pIn++;` |
|         5 | 11861 | `	}` |
|  12797501 | 11862 | `	*ppIn = pIn;` |
|  12797501 | 11863 | `	if( ppDup ){ *ppDup = pDup; }` |
|  12797501 | 11864 | `	return iFlags;` |
|         5 | 11865 | `}` |
|         - | 11866 | `/*` |
|         - | 11867 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11868 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11869 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11870 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11871 | `` * `readonly`) to their existing handlers.`` |
|         - | 11872 | ` */` |
|  12762988 | 11873 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11874 | `{` |
|  12762993 | 11875 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   6423657 | 11876 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  12784068 | 11877 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11878 | `}` |
|         - | 11879 | `/*` |
|         - | 11880 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11881 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11882 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11883 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11884 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11885 | ` */` |
|     34508 | 11886 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11887 | `{` |
|         - | 11888 | `	SyToken *pDup;` |
|     34513 | 11889 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11890 | `	sxi32 rc;` |
|     34513 | 11891 | `	if( pDup ){` |
|         4 | 11892 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11893 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11894 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11895 | `			return SXERR_ABORT;` |
|         - | 11896 | `		}` |
|         1 | 11897 | `	}` |
|     34508 | 11898 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17259 | 11899 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11900 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11901 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11902 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11903 | `			return SXERR_ABORT;` |
|         - | 11904 | `		}` |
|         1 | 11905 | `	}` |
|     34513 | 11906 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17259 | 11907 | `}` |
|         - | 11908 | `/*` |
|         - | 11909 | ` * Compile a user-defined trait.` |
|         - | 11910 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11911 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11912 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11913 | ` */` |
|      7734 | 11914 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11915 | `{` |
|      7739 | 11916 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11917 | `	ph7_class *pClass;` |
|         - | 11918 | `	SyToken *pEnd,*pTmp;` |
|         - | 11919 | `	sxi32 iProtection;` |
|         - | 11920 | `	sxi32 iAttrflags;` |
|         - | 11921 | `	SyString *pName;` |
|         - | 11922 | `	sxi32 nKwrd;` |
|         - | 11923 | `	sxi32 rc;` |
|         - | 11924 | `	/* Jump the 'trait' keyword */` |
|      7739 | 11925 | `	pGen->pIn++;` |
|      7739 | 11926 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11927 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11928 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11929 | `			return SXERR_ABORT;` |
|         - | 11930 | `		}` |
|       ! 0 | 11931 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11932 | `			pGen->pIn++;` |
|       ! 0 | 11933 | `		}` |
|       ! 0 | 11934 | `		return SXRET_OK;` |
|         - | 11935 | `	}` |
|         - | 11936 | `	/* Extract trait name */` |
|      7739 | 11937 | `	pName = &pGen->pIn->sData;` |
|      7739 | 11938 | `	pGen->pIn++;` |
|         - | 11939 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11940 | `		SyBlob sFQN;` |
|         - | 11941 | `		SyString sFQNStr;` |
|      7739 | 11942 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7739 | 11943 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7739 | 11944 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7739 | 11945 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7739 | 11946 | `		SyBlobRelease(&sFQN);` |
|         - | 11947 | `	}` |
|      7739 | 11948 | `	if( pClass == 0 ){` |
|       ! 0 | 11949 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11950 | `		return SXERR_ABORT;` |
|         - | 11951 | `	}` |
|      7739 | 11952 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7739 | 11953 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11954 | `		return SXERR_ABORT;` |
|         - | 11955 | `	}` |
|         - | 11956 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7739 | 11957 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11958 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11959 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11960 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11961 | `			return SXERR_ABORT;` |
|         - | 11962 | `		}` |
|       ! 0 | 11963 | `		return SXRET_OK;` |
|         - | 11964 | `	}` |
|      7739 | 11965 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7739 | 11966 | `	pEnd = 0;` |
|      7739 | 11967 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7739 | 11968 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11969 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11970 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11971 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11972 | `			return SXERR_ABORT;` |
|         - | 11973 | `		}` |
|       ! 0 | 11974 | `		return SXRET_OK;` |
|         - | 11975 | `	}` |
|         - | 11976 | `	/* The delimiter token is the trait body's closing brace */` |
|      7739 | 11977 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11978 | `	/* Swap token stream */` |
|      7739 | 11979 | `	pTmp = pGen->pEnd;` |
|      7739 | 11980 | `	pGen->pEnd = pEnd;` |
|         - | 11981 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7739 | 11982 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11983 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     55536 | 11984 | `	for(;;){` |
|    157019 | 11985 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     22979 | 11986 | `			pGen->pIn++;` |
|         5 | 11987 | `		}` |
|    134045 | 11988 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7739 | 11989 | `			break;` |
|         - | 11990 | `		}` |
|         - | 11991 | `		/* Bind a directly-preceding docblock to this member */` |
|    126311 | 11992 | `		GenStateSetPendingDoc(&(*pGen));` |
|    126311 | 11993 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11994 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11995 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11996 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11997 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11998 | `				return SXERR_ABORT;` |
|         - | 11999 | `			}` |
|       ! 0 | 12000 | `			goto done;` |
|         - | 12001 | `		}` |
|    126311 | 12002 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    126311 | 12003 | `		iAttrflags = 0;` |
|    126311 | 12004 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    126311 | 12005 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    126311 | 12006 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 12007 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 12008 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 12009 | `				for(;;){` |
|         - | 12010 | `					ph7_class *pUsedTrait;` |
|         - | 12011 | `					SyString *pUsedName;` |
|         5 | 12012 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 12013 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12014 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 12015 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12016 | `							return SXERR_ABORT;` |
|         - | 12017 | `						}` |
|       ! 0 | 12018 | `						break;` |
|         - | 12019 | `					}` |
|         5 | 12020 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 12021 | `					{` |
|         - | 12022 | `						SyBlob sResolved;` |
|         5 | 12023 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 12024 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 12025 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 12026 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 12027 | `						SyBlobRelease(&sResolved);` |
|         - | 12028 | `					}` |
|         5 | 12029 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 12030 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 12031 | `					}` |
|         5 | 12032 | `					if( pUsedTrait == 0 ){` |
|         4 | 12033 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 12034 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 12035 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12036 | `							return SXERR_ABORT;` |
|         - | 12037 | `						}` |
|         2 | 12038 | `					}else{` |
|         3 | 12039 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 12040 | `					}` |
|         5 | 12041 | `					pGen->pIn++;` |
|         5 | 12042 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 12043 | `						break;` |
|         - | 12044 | `					}` |
|       ! 0 | 12045 | `					pGen->pIn++;` |
|       ! 0 | 12046 | `				}` |
|         5 | 12047 | `				continue;` |
|         - | 12048 | `			}` |
|    126307 | 12049 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    126289 | 12050 | `				iProtection = nKwrd;` |
|    126289 | 12051 | `				pGen->pIn++;` |
|         - | 12052 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|         - | 12053 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|         - | 12054 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|    126289 | 12055 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|         3 | 12056 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|         3 | 12057 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         1 | 12058 | `				}` |
|    126284 | 12059 | `				if( pGen->pIn >= pGen->pEnd` |
|    126289 | 12060 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 12061 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12062 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 12063 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12064 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12065 | `						return SXERR_ABORT;` |
|         - | 12066 | `					}` |
|       ! 0 | 12067 | `					goto done;` |
|         - | 12068 | `				}` |
|    126289 | 12069 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     22961 | 12070 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     22961 | 12071 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 12072 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12073 | `							return SXERR_ABORT;` |
|         - | 12074 | `						}` |
|       ! 0 | 12075 | `						goto done;` |
|         - | 12076 | `					}` |
|     22961 | 12077 | `					continue;` |
|         - | 12078 | `				}` |
|    103333 | 12079 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         7 | 12080 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 12081 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 12082 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12083 | `							return SXERR_ABORT;` |
|         - | 12084 | `						}` |
|       ! 0 | 12085 | `						goto done;` |
|         - | 12086 | `					}` |
|         7 | 12087 | `					continue;` |
|         - | 12088 | `				}` |
|    103327 | 12089 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     51661 | 12090 | `			}` |
|    103345 | 12091 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 12092 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12093 | `					"Traits cannot have constants");` |
|       ! 0 | 12094 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12095 | `					return SXERR_ABORT;` |
|         - | 12096 | `				}` |
|       ! 0 | 12097 | `				goto done;` |
|       ! 0 | 12098 | `			}else{` |
|    103345 | 12099 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7663 | 12100 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7663 | 12101 | `					pGen->pIn++;` |
|      7663 | 12102 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7661 | 12103 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7661 | 12104 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 12105 | `							iProtection = nKwrd;` |
|       ! 0 | 12106 | `							pGen->pIn++;` |
|       ! 0 | 12107 | `						}` |
|      3828 | 12108 | `					}` |
|      7658 | 12109 | `					if( pGen->pIn >= pGen->pEnd` |
|      7663 | 12110 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 12111 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12112 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 12113 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12114 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12115 | `							return SXERR_ABORT;` |
|         - | 12116 | `						}` |
|       ! 0 | 12117 | `						goto done;` |
|         - | 12118 | `					}` |
|      7663 | 12119 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 12120 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 12121 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12122 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12123 | `								return SXERR_ABORT;` |
|         - | 12124 | `							}` |
|       ! 0 | 12125 | `							goto done;` |
|         - | 12126 | `						}` |
|         3 | 12127 | `						continue;` |
|         - | 12128 | `					}` |
|      7661 | 12129 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 12130 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12131 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12132 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12133 | `								return SXERR_ABORT;` |
|         - | 12134 | `							}` |
|       ! 0 | 12135 | `							goto done;` |
|         - | 12136 | `						}` |
|       ! 0 | 12137 | `						continue;` |
|         - | 12138 | `					}` |
|      7661 | 12139 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     99515 | 12140 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         9 | 12141 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         9 | 12142 | `					pGen->pIn++;` |
|         9 | 12143 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         9 | 12144 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         9 | 12145 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         9 | 12146 | `							iProtection = nKwrd;` |
|         9 | 12147 | `							pGen->pIn++;` |
|         3 | 12148 | `						}` |
|         3 | 12149 | `					}` |
|         9 | 12150 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 12151 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 12152 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12153 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 12154 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12155 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12156 | `							return SXERR_ABORT;` |
|         - | 12157 | `						}` |
|       ! 0 | 12158 | `						goto done;` |
|         - | 12159 | `					}` |
|         9 | 12160 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 12161 | `				}` |
|    103343 | 12162 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 12163 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12164 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 12165 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12166 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12167 | `						return SXERR_ABORT;` |
|         - | 12168 | `					}` |
|       ! 0 | 12169 | `					goto done;` |
|         - | 12170 | `				}` |
|    103343 | 12171 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 12172 | `					pGen->pIn++;` |
|       ! 0 | 12173 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 12174 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12175 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 12176 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12177 | `							return SXERR_ABORT;` |
|         - | 12178 | `						}` |
|       ! 0 | 12179 | `						goto done;` |
|         - | 12180 | `					}` |
|       ! 0 | 12181 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12182 | `				}else{` |
|    103343 | 12183 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 12184 | `				}` |
|    103343 | 12185 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 12186 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12187 | `						return SXERR_ABORT;` |
|         - | 12188 | `					}` |
|       ! 0 | 12189 | `					goto done;` |
|         - | 12190 | `				}` |
|         - | 12191 | `			}` |
|     51674 | 12192 | `		}else{` |
|       ! 0 | 12193 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12194 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12195 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12196 | `					return SXERR_ABORT;` |
|         - | 12197 | `				}` |
|       ! 0 | 12198 | `				goto done;` |
|         - | 12199 | `			}` |
|         - | 12200 | `		}` |
|         5 | 12201 | `	}` |
|         - | 12202 | `	/* Install the trait */` |
|      7739 | 12203 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7739 | 12204 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12205 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12206 | `		return SXERR_ABORT;` |
|         - | 12207 | `	}` |
|      3867 | 12208 | `done:` |
|         - | 12209 | `	/* Point beyond the trait body */` |
|      7739 | 12210 | `	pGen->pIn = &pEnd[1];` |
|      7739 | 12211 | `	pGen->pEnd = pTmp;` |
|      7739 | 12212 | `	return PH7_OK;` |
|      3872 | 12213 | `}` |
|         - | 12214 | `/*` |
|         - | 12215 | ` * Compile a user-defined class.` |
|         - | 12216 | ` *  According to the PHP language reference manual` |
|         - | 12217 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 12218 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 12219 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 12220 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 12221 | ` *   and functions (called "methods").` |
|         - | 12222 | ` */` |
|    315306 | 12223 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 12224 | `{` |
|         - | 12225 | `	sxi32 rc;` |
|    315311 | 12226 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    315311 | 12227 | `	return rc;` |
|         5 | 12228 | `}` |
|         - | 12229 | `/*` |
|         - | 12230 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 12231 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 12232 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 12233 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 12234 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 12235 | ` */` |
|  12720832 | 12236 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 12237 | `{` |
|  12926098 | 12238 | `	return (pIn->nType & PH7_TK_ID)` |
|   6565677 | 12239 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    214957 | 12240 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  12926093 | 12241 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 12242 | `}` |
|         - | 12243 | `/*` |
|         - | 12244 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 12245 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 12246 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 12247 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 12248 | ` */` |
|      3854 | 12249 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 12250 | `{` |
|      3859 | 12251 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 12252 | `}` |
|         - | 12253 | `/*` |
|         - | 12254 | ` * Exception handling.` |
|         - | 12255 | ` *  According to the PHP language reference manual` |
|         - | 12256 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 12257 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 12258 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 12259 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 12260 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 12261 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 12262 | ` *    (or re-thrown) within a catch block.` |
|         - | 12263 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 12264 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 12265 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 12266 | ` *    been defined with set_exception_handler().` |
|         - | 12267 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 12268 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 12269 | ` */` |
|         - | 12270 | `/*` |
|         - | 12271 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 12272 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 12273 | ` * indicates failure.` |
|         - | 12274 | ` */` |
|    509016 | 12275 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 12276 | `{` |
|    509021 | 12277 | `	sxi32 rc = SXRET_OK;` |
|    509021 | 12278 | `	if( pRoot->pOp ){` |
|    509009 | 12279 | `		switch( pRoot->pOp->iOp ){` |
|    254502 | 12280 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 12281 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 12282 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 12283 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 12284 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 12285 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    509009 | 12286 | `			break;` |
|       ! 0 | 12287 | `		default:` |
|         - | 12288 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 12289 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 12290 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 12291 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12292 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 12293 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 12294 | `				rc = SXERR_INVALID;` |
|       ! 0 | 12295 | `			}` |
|       ! 0 | 12296 | `			break;` |
|         - | 12297 | `		}` |
|    254519 | 12298 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 12299 | `		/* Unexpected expression */` |
|       ! 0 | 12300 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12301 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12302 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 12303 | `			rc = SXERR_INVALID;` |
|       ! 0 | 12304 | `		}` |
|       ! 0 | 12305 | `	}` |
|    509021 | 12306 | `	return rc;` |
|         5 | 12307 | `}` |
|         - | 12308 | `/*` |
|         - | 12309 | ` * Compile a 'throw' statement.` |
|         - | 12310 | ` * throw: This is how you trigger an exception.` |
|         - | 12311 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12312 | ` */` |
|    508980 | 12313 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12314 | `{` |
|    508985 | 12315 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12316 | `	GenBlock *pBlock;` |
|         - | 12317 | `	sxu32 nIdx;` |
|         - | 12318 | `	sxi32 rc;` |
|    508985 | 12319 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12320 | `	/* Compile the expression */` |
|    508985 | 12321 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    508985 | 12322 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12323 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12324 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12325 | `			return SXERR_ABORT;` |
|         - | 12326 | `		}` |
|       ! 0 | 12327 | `		return SXRET_OK;` |
|         - | 12328 | `	}` |
|    508985 | 12329 | `	pBlock = pGen->pCurrent;` |
|         - | 12330 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   2027521 | 12331 | `	while(pBlock->pParent){` |
|   2027517 | 12332 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    508981 | 12333 | `			break;` |
|         - | 12334 | `		}` |
|         - | 12335 | `		/* Point to the parent block */` |
|   1518541 | 12336 | `		pBlock = pBlock->pParent;` |
|         5 | 12337 | `	}` |
|         - | 12338 | `	/* Emit the throw instruction */` |
|    508985 | 12339 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12340 | `	/* Emit the jump */` |
|    508985 | 12341 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    508985 | 12342 | `	return SXRET_OK;` |
|    254495 | 12343 | `}` |
|         - | 12344 | `/*` |
|         - | 12345 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12346 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12347 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12348 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12349 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12350 | ` */` |
|        36 | 12351 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12352 | `{` |
|        38 | 12353 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12354 | `	GenBlock *pBlock;` |
|         - | 12355 | `	sxu32 nIdx;` |
|         - | 12356 | `	sxi32 rc;` |
|        18 | 12357 | `	(void)iCompileFlag;` |
|        38 | 12358 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12359 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12360 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12361 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12362 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12363 | `			return SXERR_ABORT;` |
|         - | 12364 | `		}` |
|       ! 0 | 12365 | `		return SXRET_OK;` |
|         - | 12366 | `	}` |
|        38 | 12367 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12368 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12369 | `		return SXERR_ABORT;` |
|         - | 12370 | `	}` |
|        38 | 12371 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12372 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12373 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12374 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12375 | `			return SXERR_ABORT;` |
|         - | 12376 | `		}` |
|       ! 0 | 12377 | `		return SXRET_OK;` |
|         - | 12378 | `	}` |
|         - | 12379 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12380 | `	pBlock = pGen->pCurrent;` |
|        60 | 12381 | `	while( pBlock->pParent ){` |
|        49 | 12382 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12383 | `			break;` |
|         - | 12384 | `		}` |
|        23 | 12385 | `		pBlock = pBlock->pParent;` |
|         1 | 12386 | `	}` |
|        38 | 12387 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12388 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12389 | `	return SXRET_OK;` |
|        20 | 12390 | `}` |
|         - | 12391 | `/*` |
|         - | 12392 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12393 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12394 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12395 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12396 | ` * compile error propagated from the parser.` |
|         - | 12397 | ` */` |
|        56 | 12398 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12399 | `{` |
|         - | 12400 | `	SyString sClassName;` |
|         - | 12401 | `	SyToken *pToken;` |
|         - | 12402 | `	SyString *pName;` |
|         - | 12403 | `	char *zDup;` |
|         - | 12404 | `	sxi32 rc;` |
|        61 | 12405 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        61 | 12406 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        61 | 12407 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        61 | 12408 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        61 | 12409 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12410 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12411 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12412 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12413 | `		return SXERR_INVALID;` |
|         - | 12414 | `	}` |
|        61 | 12415 | `	pGen->pIn++; /* '(' */` |
|        28 | 12416 | `	for(;;){` |
|         - | 12417 | `		SyBlob sResolved;` |
|        61 | 12418 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        61 | 12419 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12420 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12421 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12422 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12423 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12424 | `			return SXERR_INVALID;` |
|         - | 12425 | `		}` |
|        89 | 12426 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        56 | 12427 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        61 | 12428 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        61 | 12429 | `		SyBlobRelease(&sResolved);` |
|        61 | 12430 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        61 | 12431 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        61 | 12432 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        56 | 12433 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12434 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12435 | `			pGen->pIn++; continue;` |
|         - | 12436 | `		}` |
|        61 | 12437 | `		break;` |
|       ! 0 | 12438 | `	}` |
|         - | 12439 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12440 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|        61 | 12441 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|         3 | 12442 | `		pGen->pIn++; /* ')' */` |
|         3 | 12443 | `		return SXRET_OK;` |
|         - | 12444 | `	}` |
|        54 | 12445 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12446 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12447 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12448 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12449 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12450 | `		return SXERR_INVALID;` |
|         - | 12451 | `	}` |
|        59 | 12452 | `	pGen->pIn++; /* '$' */` |
|        59 | 12453 | `	pName = &pGen->pIn->sData;` |
|        59 | 12454 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12455 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12456 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12457 | `	pGen->pIn++;` |
|        59 | 12458 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12459 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12460 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12461 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12462 | `		return SXERR_INVALID;` |
|         - | 12463 | `	}` |
|        59 | 12464 | `	pGen->pIn++; /* ')' */` |
|        59 | 12465 | `	return SXRET_OK;` |
|        33 | 12466 | `}` |
|         - | 12467 | `/*` |
|         - | 12468 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12469 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12470 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12471 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12472 | ` * VmThrowException):` |
|         - | 12473 | ` *` |
|         - | 12474 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12475 | ` *    <try body>` |
|         - | 12476 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12477 | ` *    JMP  -> finally\|end` |
|         - | 12478 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12479 | ` *    <catch body>` |
|         - | 12480 | ` *    JMP  -> finally\|end` |
|         - | 12481 | ` *    ... more catches ...` |
|         - | 12482 | ` *  Lfin: <finally body>` |
|         - | 12483 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12484 | ` *  Lend:` |
|         - | 12485 | ` */` |
|       100 | 12486 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12487 | `{` |
|       105 | 12488 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12489 | `	GenBlock *pTry;` |
|         - | 12490 | `	VmInstr *pInstr;` |
|       105 | 12491 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12492 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12493 | `	sxi32 rc;` |
|       105 | 12494 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12495 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       105 | 12496 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       105 | 12497 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       105 | 12498 | `	pTry->pUserData = pException;` |
|       105 | 12499 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       105 | 12500 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       105 | 12501 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       105 | 12502 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       105 | 12503 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       105 | 12504 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12505 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       105 | 12506 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       105 | 12507 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       105 | 12508 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       105 | 12509 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12510 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       105 | 12511 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12512 | `	/* Catch clauses (inline) */` |
|       105 | 12513 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       100 | 12514 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        61 | 12515 | `		sxu32 k = 0;` |
|        84 | 12516 | `		for(;;){` |
|         - | 12517 | `			ph7_exception_block sCatch;` |
|         - | 12518 | `			GenBlock *pCatchBlk;` |
|       117 | 12519 | `			sxu32 idxJmp = 0;` |
|       112 | 12520 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       107 | 12521 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        33 | 12522 | `				break;` |
|         - | 12523 | `			}` |
|        61 | 12524 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        61 | 12525 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12526 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        61 | 12527 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        61 | 12528 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        61 | 12529 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        61 | 12530 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12531 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12532 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12533 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        61 | 12534 | `			pCatchBlk->pUserData = pException;` |
|        61 | 12535 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        61 | 12536 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12537 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        61 | 12538 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12539 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12540 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        61 | 12541 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        61 | 12542 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        61 | 12543 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        61 | 12544 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        61 | 12545 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        61 | 12546 | `			k++;` |
|         5 | 12547 | `		}` |
|        28 | 12548 | `	}` |
|         - | 12549 | `	/* Finally (inline) */` |
|       105 | 12550 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12551 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12552 | `		GenBlock *pFinBlk;` |
|        52 | 12553 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12554 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12555 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12556 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12557 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12558 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12559 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12560 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12561 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12562 | `		pException->iHasFinally = 1;` |
|        24 | 12563 | `	}` |
|       105 | 12564 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       105 | 12565 | `	pException->iInlined = 1;` |
|         - | 12566 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12567 | `	{` |
|       105 | 12568 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12569 | `		sxu32 *aJ; sxu32 n;` |
|       105 | 12570 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       105 | 12571 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       105 | 12572 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       161 | 12573 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        61 | 12574 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        61 | 12575 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        33 | 12576 | `		}` |
|         - | 12577 | `	}` |
|       105 | 12578 | `	SySetRelease(&aCatchJmp);` |
|       105 | 12579 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12580 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12581 | `	}` |
|       105 | 12582 | `	return SXRET_OK;` |
|        55 | 12583 | `}` |
|         - | 12584 | `/*` |
|         - | 12585 | ` * Compile a 'catch' block.` |
|         - | 12586 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12587 | ` * an object containing the exception information.` |
|         - | 12588 | ` */` |
|     24478 | 12589 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12590 | `{` |
|     24483 | 12591 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12592 | `	ph7_exception_block sCatch;` |
|         - | 12593 | `	SySet *pInstrContainer;` |
|         - | 12594 | `	SyString sClassName;` |
|         - | 12595 | `	GenBlock *pCatch;` |
|         - | 12596 | `	SyToken *pToken;` |
|         - | 12597 | `	SyString *pName;` |
|         - | 12598 | `	char *zDup;` |
|         - | 12599 | `	sxi32 rc;` |
|     24483 | 12600 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12601 | `	/* Zero the structure */` |
|     24483 | 12602 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12603 | `	/* Initialize fields */` |
|     24483 | 12604 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24483 | 12605 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24483 | 12606 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12607 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12608 | `			pToken = pGen->pIn;` |
|       ! 0 | 12609 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12610 | `				pToken--;` |
|       ! 0 | 12611 | `			}` |
|       ! 0 | 12612 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12613 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12614 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12615 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12616 | `				return SXERR_ABORT;` |
|         - | 12617 | `			}` |
|       ! 0 | 12618 | `			return SXERR_INVALID;` |
|         - | 12619 | `	}` |
|         - | 12620 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24483 | 12621 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12254 | 12622 | `	for(;;){` |
|         - | 12623 | `		SyBlob sResolved;` |
|     24513 | 12624 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24513 | 12625 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12626 | `			SyBlobRelease(&sResolved);` |
|         6 | 12627 | `			pToken = pGen->pIn;` |
|         6 | 12628 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12629 | `				pToken--;` |
|       ! 0 | 12630 | `			}` |
|         8 | 12631 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12632 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12633 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12634 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12635 | `				return SXERR_ABORT;` |
|         - | 12636 | `			}` |
|         6 | 12637 | `			return SXERR_INVALID;` |
|         - | 12638 | `		}` |
|         - | 12639 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12640 | `		 * transient SyBlob allocation. */` |
|     36761 | 12641 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24504 | 12642 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24509 | 12643 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24509 | 12644 | `		SyBlobRelease(&sResolved);` |
|     24509 | 12645 | `		if( zDup == 0 ){` |
|       ! 0 | 12646 | `			goto Mem;` |
|         - | 12647 | `		}` |
|     24509 | 12648 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24509 | 12649 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12650 | `			goto Mem;` |
|         - | 12651 | `		}` |
|         - | 12652 | `		/* Check for '\|' (multi-catch separator) */` |
|     24504 | 12653 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24504 | 12654 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        35 | 12655 | `			pGen->pIn->sData.nByte == 1 &&` |
|        30 | 12656 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        32 | 12657 | `			pGen->pIn++; /* Consume the '\|' */` |
|        32 | 12658 | `			continue;` |
|         - | 12659 | `		}` |
|     24479 | 12660 | `		break;` |
|       ! 0 | 12661 | `	}` |
|         - | 12662 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12663 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|         - | 12664 | `	 * jump straight to compiling the block below. */` |
|     24479 | 12665 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|         5 | 12666 | `		goto CatchBody;` |
|         - | 12667 | `	}` |
|     24470 | 12668 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24475 | 12669 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12670 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12671 | `			pToken = pGen->pIn;` |
|       ! 0 | 12672 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12673 | `				pToken--;` |
|       ! 0 | 12674 | `			}` |
|       ! 0 | 12675 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12676 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12677 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12678 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12679 | `				return SXERR_ABORT;` |
|         - | 12680 | `			}` |
|       ! 0 | 12681 | `			return SXERR_INVALID;` |
|         - | 12682 | `	}` |
|     24475 | 12683 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12684 | `	/* Duplicate instance name */` |
|     24475 | 12685 | `	pName = &pGen->pIn->sData;` |
|     24475 | 12686 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24475 | 12687 | `	if( zDup == 0 ){` |
|       ! 0 | 12688 | `		goto Mem;` |
|         - | 12689 | `	}` |
|     24475 | 12690 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24475 | 12691 | `	pGen->pIn++;` |
|     12237 | 12692 | `CatchBody:` |
|     24479 | 12693 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12694 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12695 | `		pToken = pGen->pIn;` |
|       ! 0 | 12696 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12697 | `			pToken--;` |
|       ! 0 | 12698 | `		}` |
|       ! 0 | 12699 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12700 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12701 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12702 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12703 | `			return SXERR_ABORT;` |
|         - | 12704 | `		}` |
|       ! 0 | 12705 | `		return SXERR_INVALID;` |
|         - | 12706 | `	}` |
|         - | 12707 | `	/* Compile the block */` |
|     24479 | 12708 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12709 | `	/* Create the catch block */` |
|     24479 | 12710 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24479 | 12711 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12712 | `		return SXERR_ABORT;` |
|         - | 12713 | `	}` |
|         - | 12714 | `	/* Swap bytecode container */` |
|     24479 | 12715 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24479 | 12716 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12717 | `	/* Compile the block */` |
|     24479 | 12718 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12719 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24479 | 12720 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12721 | `	/* Emit the DONE instruction */` |
|     24479 | 12722 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12723 | `	/* Leave the block */` |
|     24479 | 12724 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12725 | `	/* Restore the default container */` |
|     24479 | 12726 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12727 | `	/* Install the catch block */` |
|     24479 | 12728 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24479 | 12729 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12730 | `		goto Mem;` |
|         - | 12731 | `	}` |
|     24479 | 12732 | `	return SXRET_OK;` |
|       ! 0 | 12733 | `Mem:` |
|       ! 0 | 12734 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12735 | `	return SXERR_ABORT;` |
|     12244 | 12736 | `}` |
|         - | 12737 | `/*` |
|         - | 12738 | ` * Compile a 'try' block.` |
|         - | 12739 | ` * A function using an exception should be in a "try" block.` |
|         - | 12740 | ` * If the exception does not trigger, the code will continue` |
|         - | 12741 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12742 | ` * is "thrown".` |
|         - | 12743 | ` */` |
|     24636 | 12744 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12745 | `{` |
|         - | 12746 | `	ph7_exception *pException;` |
|     24641 | 12747 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12748 | `	GenBlock *pTry;` |
|         - | 12749 | `	sxu32 nJmpIdx;` |
|         - | 12750 | `	sxi32 rc;` |
|         - | 12751 | `	/* Create the exception container */` |
|     24641 | 12752 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24641 | 12753 | `	if( pException == 0 ){` |
|       ! 0 | 12754 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12755 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12756 | `		return SXERR_ABORT;` |
|         - | 12757 | `	}` |
|         - | 12758 | `	/* Zero the structure */` |
|     24641 | 12759 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12760 | `	/* Initialize fields */` |
|     24641 | 12761 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24641 | 12762 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24641 | 12763 | `	pException->iHasFinally = 0;` |
|     24641 | 12764 | `	pException->iFinallyDone = 0;` |
|     24641 | 12765 | `	pException->pVm = pGen->pVm;` |
|         - | 12766 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12767 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12768 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12769 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12770 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12771 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24641 | 12772 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       105 | 12773 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12774 | `	}` |
|         - | 12775 | `	/* Create the try block */` |
|     24541 | 12776 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24541 | 12777 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12778 | `		return SXERR_ABORT;` |
|         - | 12779 | `	}` |
|         - | 12780 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24541 | 12781 | `	pTry->pUserData = pException;` |
|         - | 12782 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24541 | 12783 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12784 | `	/* Fix the jump later when the destination is resolved */` |
|     24541 | 12785 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24541 | 12786 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12787 | `	/* Compile the block */` |
|     24541 | 12788 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24541 | 12789 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12790 | `		return SXERR_ABORT;` |
|         - | 12791 | `	}` |
|         - | 12792 | `	/* Fix forward jumps now the destination is resolved */` |
|     24541 | 12793 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12794 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24541 | 12795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12796 | `	/* Leave the block */` |
|     24541 | 12797 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12798 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24541 | 12799 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24534 | 12800 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12801 | `		/* Compile one or more catch blocks */` |
|     24474 | 12802 | `		for(;;){` |
|     48948 | 12803 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36768 | 12804 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12240 | 12805 | `					break;` |
|         - | 12806 | `			}` |
|     24483 | 12807 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24483 | 12808 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12809 | `				return SXERR_ABORT;` |
|         - | 12810 | `			}` |
|         5 | 12811 | `		}` |
|     12235 | 12812 | `	}` |
|         - | 12813 | `	/* Compile optional finally block */` |
|     24541 | 12814 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       740 | 12815 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12816 | `		SySet *pInstrContainer;` |
|         - | 12817 | `		GenBlock *pFinBlock;` |
|       129 | 12818 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12819 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12820 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12821 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12822 | `			return SXERR_ABORT;` |
|         - | 12823 | `		}` |
|         - | 12824 | `		/* Swap bytecode container */` |
|       129 | 12825 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12826 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12827 | `		/* Compile the finally body */` |
|       129 | 12828 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12829 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12830 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12831 | `			return SXERR_ABORT;` |
|         - | 12832 | `		}` |
|         - | 12833 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12834 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12835 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12836 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12837 | `		/* Leave the block */` |
|       129 | 12838 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12839 | `		/* Restore the default container */` |
|       129 | 12840 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12841 | `		pException->iHasFinally = 1;` |
|        62 | 12842 | `	}` |
|         - | 12843 | `	/* Must have at least one catch or finally */` |
|     24541 | 12844 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12845 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12846 | `			"Cannot use try without catch or finally");` |
|         8 | 12847 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12848 | `			return SXERR_ABORT;` |
|         - | 12849 | `		}` |
|         3 | 12850 | `	}` |
|     24541 | 12851 | `	return SXRET_OK;` |
|     12323 | 12852 | `}` |
|         - | 12853 | `/*` |
|         - | 12854 | ` * Compile a switch block.` |
|         - | 12855 | ` *  (See block-comment below for more information)` |
|         - | 12856 | ` */` |
|     53648 | 12857 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12858 | `{` |
|     53653 | 12859 | `	sxi32 rc = SXRET_OK;` |
|     53653 | 12860 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12861 | `		/* Unexpected token */` |
|       ! 0 | 12862 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12863 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12864 | `			return SXERR_ABORT;` |
|         - | 12865 | `		}` |
|       ! 0 | 12866 | `		pGen->pIn++;` |
|       ! 0 | 12867 | `	}` |
|     53653 | 12868 | `	pGen->pIn++;` |
|         - | 12869 | `	/* First instruction to execute in this block. */` |
|     53653 | 12870 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12871 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12872 | `	 * or the '}' token */` |
|     38446 | 12873 | `	for(;;){` |
|     76897 | 12874 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12875 | `			/* No more input to process */` |
|       ! 0 | 12876 | `			break;` |
|         - | 12877 | `		}` |
|     76897 | 12878 | `		rc = SXRET_OK;` |
|     76897 | 12879 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      3909 | 12880 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      3855 | 12881 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12882 | `					/* Unexpected token */` |
|       ! 0 | 12883 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12884 | `						&pGen->pIn->sData);` |
|       ! 0 | 12885 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12886 | `						return SXERR_ABORT;` |
|         - | 12887 | `					}` |
|         - | 12888 | `					/* FALL THROUGH */` |
|       ! 0 | 12889 | `				}` |
|      3855 | 12890 | `				rc = SXERR_EOF;` |
|      3855 | 12891 | `				break;` |
|         - | 12892 | `			}` |
|        32 | 12893 | `		}else{` |
|         - | 12894 | `			sxi32 nKwrd;` |
|         - | 12895 | `			/* Extract the keyword */` |
|     72993 | 12896 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     72993 | 12897 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|     24903 | 12898 | `				break;` |
|         - | 12899 | `			}` |
|     23197 | 12900 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12901 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12902 | `					/* Unexpected token */` |
|       ! 0 | 12903 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12904 | `						&pGen->pIn->sData);` |
|       ! 0 | 12905 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12906 | `						return SXERR_ABORT;` |
|         - | 12907 | `					}` |
|         - | 12908 | `					/* FALL THROUGH */` |
|       ! 0 | 12909 | `				}` |
|         - | 12910 | `				/* Block compiled */` |
|         3 | 12911 | `				break;` |
|         - | 12912 | `			}` |
|         - | 12913 | `		}` |
|         - | 12914 | `		/* Compile block */` |
|     23249 | 12915 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     23249 | 12916 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12917 | `			return SXERR_ABORT;` |
|         - | 12918 | `		}` |
|         5 | 12919 | `	}` |
|     53653 | 12920 | `	return rc;` |
|     26829 | 12921 | `}` |
|         - | 12922 | `/*` |
|         - | 12923 | ` * Compile a case eXpression.` |
|         - | 12924 | ` *  (See block-comment below for more information)` |
|         - | 12925 | ` */` |
|     53628 | 12926 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12927 | `{` |
|         - | 12928 | `	SySet *pInstrContainer;` |
|         - | 12929 | `	SyToken *pEnd,*pTmp;` |
|     53633 | 12930 | `	sxi32 iNest = 0;` |
|         - | 12931 | `	sxi32 rc;` |
|         - | 12932 | `	/* Delimit the expression */` |
|     53633 | 12933 | `	pEnd = pGen->pIn;` |
|    107269 | 12934 | `	while( pEnd < pGen->pEnd ){` |
|    107269 | 12935 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12936 | `			/* Increment nesting level */` |
|         3 | 12937 | `			iNest++;` |
|    107268 | 12938 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12939 | `			/* Decrement nesting level */` |
|         3 | 12940 | `			iNest--;` |
|    107266 | 12941 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     53633 | 12942 | `			break;` |
|         - | 12943 | `		}` |
|     53641 | 12944 | `		pEnd++;` |
|         5 | 12945 | `	}` |
|     53633 | 12946 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12947 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12948 | `		if( rc == SXERR_ABORT ){` |
|         - | 12949 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12950 | `			return SXERR_ABORT;` |
|         - | 12951 | `		}` |
|       ! 0 | 12952 | `	}` |
|         - | 12953 | `	/* Swap token stream */` |
|     53633 | 12954 | `	pTmp = pGen->pEnd;` |
|     53633 | 12955 | `	pGen->pEnd = pEnd;` |
|     53633 | 12956 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     53633 | 12957 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     53633 | 12958 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12959 | `	/* Emit the done instruction */` |
|     53633 | 12960 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     53633 | 12961 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12962 | `	/* Update token stream */` |
|     53633 | 12963 | `	pGen->pIn  = pEnd;` |
|     53633 | 12964 | `	pGen->pEnd = pTmp;` |
|     53633 | 12965 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12966 | `		return SXERR_ABORT;` |
|         - | 12967 | `	}` |
|     53633 | 12968 | `	return SXRET_OK;` |
|     26819 | 12969 | `}` |
|         - | 12970 | `/*` |
|         - | 12971 | ` * Compile the smart switch statement.` |
|         - | 12972 | ` * According to the PHP language reference manual` |
|         - | 12973 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12974 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12975 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12976 | ` *  This is exactly what the switch statement is for.` |
|         - | 12977 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12978 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12979 | ` *  of the outer loop, use continue 2.` |
|         - | 12980 | ` *  Note that switch/case does loose comparision.` |
|         - | 12981 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12982 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12983 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12984 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12985 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12986 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12987 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12988 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12989 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12990 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12991 | ` *  list for the next case.` |
|         - | 12992 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12993 | ` *  or floating-point numbers and strings.` |
|         - | 12994 | ` */` |
|      3852 | 12995 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12996 | `{` |
|         - | 12997 | `	GenBlock *pSwitchBlock;` |
|         - | 12998 | `	SyToken *pTmp,*pEnd;` |
|         - | 12999 | `	ph7_switch *pSwitch;` |
|         - | 13000 | `	sxu32 nToken;` |
|         - | 13001 | `	sxu32 nLine;` |
|         - | 13002 | `	sxi32 rc;` |
|      3857 | 13003 | `	nLine = pGen->pIn->nLine;` |
|         - | 13004 | `	/* Jump the 'switch' keyword */` |
|      3857 | 13005 | `	pGen->pIn++;` |
|      3857 | 13006 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 13007 | `		/* Syntax error */` |
|       ! 0 | 13008 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 13009 | `		if( rc == SXERR_ABORT ){` |
|         - | 13010 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 13011 | `			return SXERR_ABORT;` |
|         - | 13012 | `		}` |
|       ! 0 | 13013 | `		goto Synchronize;` |
|         - | 13014 | `	}` |
|         - | 13015 | `	/* Jump the left parenthesis '(' */` |
|      3857 | 13016 | `	pGen->pIn++;` |
|      3857 | 13017 | `	pEnd = 0; /* cc warning */` |
|         - | 13018 | `	/* Create the loop block */` |
|      5783 | 13019 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      1926 | 13020 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      3857 | 13021 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 13022 | `		return SXERR_ABORT;` |
|         - | 13023 | `	}` |
|         - | 13024 | `	/* Delimit the condition */` |
|      3857 | 13025 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      3857 | 13026 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 13027 | `		/* Empty expression */` |
|       ! 0 | 13028 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 13029 | `		if( rc == SXERR_ABORT ){` |
|         - | 13030 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 13031 | `			return SXERR_ABORT;` |
|         - | 13032 | `		}` |
|       ! 0 | 13033 | `	}` |
|         - | 13034 | `	/* Swap token streams */` |
|      3857 | 13035 | `	pTmp = pGen->pEnd;` |
|      3857 | 13036 | `	pGen->pEnd = pEnd;` |
|         - | 13037 | `	/* Compile the expression */` |
|      3857 | 13038 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      3857 | 13039 | `	if( rc == SXERR_ABORT ){` |
|         - | 13040 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 13041 | `		return SXERR_ABORT;` |
|         - | 13042 | `	}` |
|         - | 13043 | `	/* Update token stream */` |
|      3857 | 13044 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 13045 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 13046 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 13047 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13048 | `			return SXERR_ABORT;` |
|         - | 13049 | `		}` |
|       ! 0 | 13050 | `		pGen->pIn++;` |
|       ! 0 | 13051 | `	}` |
|      3857 | 13052 | `	pGen->pIn  = &pEnd[1];` |
|      3857 | 13053 | `	pGen->pEnd = pTmp;` |
|      3857 | 13054 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      3852 | 13055 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 13056 | `			pTmp = pGen->pIn;` |
|       ! 0 | 13057 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 13058 | `				pTmp--;` |
|       ! 0 | 13059 | `			}` |
|         - | 13060 | `			/* Unexpected token */` |
|       ! 0 | 13061 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 13062 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13063 | `				return SXERR_ABORT;` |
|         - | 13064 | `			}` |
|       ! 0 | 13065 | `			goto Synchronize;` |
|         - | 13066 | `	}` |
|         - | 13067 | `	/* Set the delimiter token */` |
|      3857 | 13068 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 13069 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 13070 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 13071 | `	}else{` |
|      3855 | 13072 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 13073 | `	}` |
|      3857 | 13074 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 13075 | `	/* Create the switch blocks container */` |
|      3857 | 13076 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      3857 | 13077 | `	if( pSwitch == 0 ){` |
|         - | 13078 | `		/* Abort compilation */` |
|       ! 0 | 13079 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 13080 | `		return SXERR_ABORT;` |
|         - | 13081 | `	}` |
|         - | 13082 | `	/* Zero the structure */` |
|      3857 | 13083 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 13084 | `	/* Initialize fields */` |
|      3857 | 13085 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 13086 | `	/* Emit the switch instruction */` |
|      3857 | 13087 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 13088 | `	/* Compile case blocks */` |
|     51724 | 13089 | `	for(;;){` |
|         - | 13090 | `		sxu32 nKwrd;` |
|     53655 | 13091 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 13092 | `			/* No more input to process */` |
|       ! 0 | 13093 | `			break;` |
|         - | 13094 | `		}` |
|     53655 | 13095 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 13096 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 13097 | `				/* Unexpected token */` |
|       ! 0 | 13098 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13099 | `					&pGen->pIn->sData);` |
|       ! 0 | 13100 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13101 | `					return SXERR_ABORT;` |
|         - | 13102 | `				}` |
|         - | 13103 | `				/* FALL THROUGH */` |
|       ! 0 | 13104 | `			}` |
|         - | 13105 | `			/* Block compiled */` |
|       ! 0 | 13106 | `			break;` |
|         - | 13107 | `		}` |
|         - | 13108 | `		/* Extract the keyword */` |
|     53655 | 13109 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     53655 | 13110 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 13111 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 13112 | `				/* Unexpected token */` |
|       ! 0 | 13113 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13114 | `					&pGen->pIn->sData);` |
|       ! 0 | 13115 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13116 | `					return SXERR_ABORT;` |
|         - | 13117 | `				}` |
|         - | 13118 | `				/* FALL THROUGH */` |
|       ! 0 | 13119 | `			}` |
|         - | 13120 | `			/* Block compiled */` |
|         3 | 13121 | `			break;` |
|         - | 13122 | `		}` |
|     53653 | 13123 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 13124 | `			/*` |
|         - | 13125 | `			 * Accroding to the PHP language reference manual` |
|         - | 13126 | `			 *  A special case is the default case. This case matches anything` |
|         - | 13127 | `			 *  that wasn't matched by the other cases.` |
|         - | 13128 | `			 */` |
|        25 | 13129 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 13130 | `				/* Default case already compiled */` |
|       ! 0 | 13131 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 13132 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13133 | `					return SXERR_ABORT;` |
|         - | 13134 | `				}` |
|       ! 0 | 13135 | `			}` |
|        25 | 13136 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 13137 | `			/* Compile the default block */` |
|        25 | 13138 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 13139 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13140 | `				return SXERR_ABORT;` |
|        25 | 13141 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 13142 | `				break;` |
|         1 | 13143 | `			}` |
|     53634 | 13144 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 13145 | `			ph7_case_expr sCase;` |
|         - | 13146 | `			/* Standard case block */` |
|     53633 | 13147 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 13148 | `			/* initialize the structure */` |
|     53633 | 13149 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 13150 | `			/* Compile the case expression */` |
|     53633 | 13151 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     53633 | 13152 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13153 | `				return SXERR_ABORT;` |
|         - | 13154 | `			}` |
|         - | 13155 | `			/* Compile the case block */` |
|     53633 | 13156 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 13157 | `			/* Insert in the switch container */` |
|     53633 | 13158 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     53633 | 13159 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13160 | `				return SXERR_ABORT;` |
|     53633 | 13161 | `			}else if( rc == SXERR_EOF ){` |
|      3837 | 13162 | `				break;` |
|         - | 13163 | `			}` |
|     24903 | 13164 | `		}else{` |
|         - | 13165 | `			/* Unexpected token */` |
|       ! 0 | 13166 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13167 | `				&pGen->pIn->sData);` |
|       ! 0 | 13168 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13169 | `				return SXERR_ABORT;` |
|         - | 13170 | `			}` |
|       ! 0 | 13171 | `			break;` |
|         - | 13172 | `		}` |
|         5 | 13173 | `	}` |
|         - | 13174 | `	/* Fix all jumps now the destination is resolved */` |
|      3857 | 13175 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      3857 | 13176 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 13177 | `	/* Release the loop block */` |
|      3857 | 13178 | `	GenStateLeaveBlock(pGen,0);` |
|      3857 | 13179 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 13180 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      3857 | 13181 | `		pGen->pIn++;` |
|      1926 | 13182 | `	}` |
|         - | 13183 | `	/* Statement successfully compiled */` |
|      3857 | 13184 | `	return SXRET_OK;` |
|       ! 0 | 13185 | `Synchronize:` |
|         - | 13186 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 13187 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 13188 | `		pGen->pIn++;` |
|       ! 0 | 13189 | `	}` |
|       ! 0 | 13190 | `	return SXRET_OK;` |
|      1931 | 13191 | `}` |
|         - | 13192 | `/*` |
|         - | 13193 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 13194 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 13195 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 13196 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 13197 | ` */` |
|         - | 13198 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 13199 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 13200 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 13201 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 13202 |  |
|         - | 13203 | `/*` |
|         - | 13204 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 13205 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 13206 | ` * patched entries from the pending set.` |
|         - | 13207 | ` */` |
|  48501258 | 13208 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 13209 | `{` |
|  48501263 | 13210 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 13211 | `	sxu32 nTarget;` |
|         - | 13212 | `	sxu32 *aIdx;` |
|         - | 13213 | `	sxu32 i;` |
|  48501263 | 13214 | `	if( nCur <= nBaseline ){` |
|  48501167 | 13215 | `		return;` |
|         - | 13216 | `	}` |
|       100 | 13217 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 13218 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 13219 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 13220 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 13221 | `		if( pInstr ){` |
|       108 | 13222 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 13223 | `		}` |
|        56 | 13224 | `	}` |
|       100 | 13225 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  24250634 | 13226 | `}` |
|         - | 13227 |  |
|         - | 13228 | `/*` |
|         - | 13229 | ` * By-reference out-parameters of builtin functions.` |
|         - | 13230 | ` *` |
|         - | 13231 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 13232 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 13233 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 13234 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 13235 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 13236 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 13237 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 13238 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 13239 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 13240 | ` * creates it" behaviour).` |
|         - | 13241 | ` *` |
|         - | 13242 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 13243 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 13244 | ` */` |
|   6175612 | 13245 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 13246 | `{` |
|         - | 13247 | `	static const struct {` |
|         - | 13248 | `		const char *zName;` |
|         - | 13249 | `		sxu32 nByte;` |
|         - | 13250 | `		sxu32 mask;` |
|         - | 13251 | `	} aByRef[] = {` |
|         - | 13252 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - | 13253 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13254 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13255 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13256 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13257 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 13258 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 13259 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 13260 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 13261 | `	};` |
|         - | 13262 | `	sxu32 i;` |
|   6175617 | 13263 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1622537 | 13264 | `		return 0;` |
|         - | 13265 | `	}` |
|  45113019 | 13266 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  40617446 | 13267 | `		if( pName->nByte == aByRef[i].nByte` |
|  21428196 | 13268 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     57517 | 13269 | `			return aByRef[i].mask;` |
|         - | 13270 | `		}` |
|  20279972 | 13271 | `	}` |
|   4495573 | 13272 | `	return 0;` |
|   3087811 | 13273 | `}` |
|         - | 13274 | `/*` |
|         - | 13275 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 13276 | ` *` |
|         - | 13277 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 13278 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 13279 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 13280 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 13281 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 13282 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 13283 | ` */` |
|   6175612 | 13284 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 13285 | `{` |
|         - | 13286 | `	SyToken *p, *pEnd;` |
|   6175617 | 13287 | `	pOut->zString = 0;` |
|   6175617 | 13288 | `	pOut->nByte = 0;` |
|   6175617 | 13289 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 13290 | `		return;` |
|         - | 13291 | `	}` |
|   6175617 | 13292 | `	p = pLeft->pStart;` |
|   6175617 | 13293 | `	pEnd = pLeft->pEnd;` |
|         - | 13294 | `	/* Optional single leading namespace separator (absolute path). */` |
|   6175617 | 13295 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3867 | 13296 | `		p++;` |
|      1931 | 13297 | `	}` |
|   6175617 | 13298 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1622495 | 13299 | `		return;` |
|         - | 13300 | `	}` |
|         - | 13301 | `	/* Must be a single component: nothing follows the name token. */` |
|   4553127 | 13302 | `	if( p + 1 != pEnd ){` |
|        47 | 13303 | `		return;` |
|         - | 13304 | `	}` |
|   4553085 | 13305 | `	*pOut = p->sData;` |
|   3087811 | 13306 | `}` |
|         - | 13307 | `/*` |
|         - | 13308 | ` * Generate bytecode for a given expression tree.` |
|         - | 13309 | ` * If something goes wrong while generating bytecode` |
|         - | 13310 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 13311 | ` * this function takes care of generating the appropriate` |
|         - | 13312 | ` * error message.` |
|         - | 13313 | ` */` |
|  67482846 | 13314 | `static sxi32 GenStateEmitExprCode(` |
|         - | 13315 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 13316 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 13317 | `	sxi32 iFlags /* Control flags */` |
|         - | 13318 | `	)` |
|         5 | 13319 | `{` |
|         - | 13320 | `	VmInstr *pInstr;` |
|         - | 13321 | `	sxu32 nJmpIdx;` |
|  67482851 | 13322 | `	sxi32 iP1 = 0;` |
|  67482851 | 13323 | `	sxu32 iP2 = 0;` |
|  67482851 | 13324 | `	void *p3  = 0;` |
|         - | 13325 | `	sxi32 iVmOp;` |
|         - | 13326 | `	sxi32 rc;` |
|  67482851 | 13327 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  67482851 | 13328 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  67482851 | 13329 | `	sxu32 nRhsNsBase = 0;` |
|  67482851 | 13330 | `	if( pNode->xCode ){` |
|         - | 13331 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13332 | `		/* Compile node */` |
|  40710501 | 13333 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  40710501 | 13334 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  40710501 | 13335 | `		RE_SWAP_DELIMITER(pGen);` |
|  40710501 | 13336 | `		return rc;` |
|         - | 13337 | `	}` |
|  26772355 | 13338 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13339 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13340 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13341 | `		return SXERR_ABORT;` |
|         - | 13342 | `	}` |
|  26772355 | 13343 | `	iVmOp = pNode->pOp->iVmOp;` |
|  26772355 | 13344 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13345 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13346 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13347 | `		 * and later errors are still reported. */` |
|         3 | 13348 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13349 | `			"The (unset) cast is no longer supported");` |
|         3 | 13350 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13351 | `			return SXERR_ABORT;` |
|         - | 13352 | `		}` |
|         1 | 13353 | `	}` |
|  26772355 | 13354 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        93 | 13355 | `		sxu32 nJmp = 0;` |
|         - | 13356 | `		sxu32 nNcNsBase;` |
|         - | 13357 | `		VmInstr *pInstrFix;` |
|         - | 13358 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13359 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13360 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13361 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13362 | `		 * stack slot carries a writable nIdx. */` |
|        93 | 13363 | `		if( pNode->pRight ){` |
|        93 | 13364 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13365 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        93 | 13366 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13367 | `				return rc;` |
|         - | 13368 | `			}` |
|        93 | 13369 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13370 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13371 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13372 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13373 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13374 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13375 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13376 | `			 * cascade for the actual write path stays correct. */` |
|        93 | 13377 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        93 | 13378 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13379 | `				pInstrFix->iP2 = 3;` |
|        15 | 13380 | `			}` |
|        45 | 13381 | `		}` |
|         - | 13382 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        93 | 13383 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13384 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        93 | 13385 | `		if( pNode->pLeft ){` |
|        93 | 13386 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13387 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        93 | 13388 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13389 | `				return rc;` |
|         - | 13390 | `			}` |
|        93 | 13391 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        45 | 13392 | `		}` |
|         - | 13393 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        93 | 13394 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13395 | `		/* Patch the short-circuit jump to land after the store. */` |
|        93 | 13396 | `		if( nJmp > 0 ){` |
|        93 | 13397 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        93 | 13398 | `			if( pInstrFix ){` |
|        93 | 13399 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        45 | 13400 | `			}` |
|        45 | 13401 | `		}` |
|        93 | 13402 | `		return SXRET_OK;` |
|         - | 13403 | `	}` |
|  26772265 | 13404 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13405 | `		sxu32 nJz,nJmp;` |
|         - | 13406 | `		sxu32 nTernaryNsBase;` |
|         - | 13407 | `		/* Ternary operator require special handling */` |
|         - | 13408 | `		/* Phase#1: Compile the condition */` |
|    458433 | 13409 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    458433 | 13410 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    458433 | 13411 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13412 | `			return rc;` |
|         - | 13413 | `		}` |
|         - | 13414 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13415 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13416 | `		 * condition expression, not leak past the ternary. */` |
|    458433 | 13417 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    458433 | 13418 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    458433 | 13419 | `		if( pNode->pLeft ){` |
|         - | 13420 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13421 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    454541 | 13422 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13423 | `			/* Phase#3: Compile the 'then' expression  */` |
|    454541 | 13424 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    454541 | 13425 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    454541 | 13426 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13427 | `				return rc;` |
|         - | 13428 | `			}` |
|    454541 | 13429 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    227273 | 13430 | `		}else{` |
|         - | 13431 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13432 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13433 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3897 | 13434 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3897 | 13435 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13436 | `		}` |
|         - | 13437 | `		/* Phase#4: Emit the unconditional jump */` |
|    458433 | 13438 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13439 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    458433 | 13440 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    458433 | 13441 | `		if( pInstr ){` |
|    458433 | 13442 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    229214 | 13443 | `		}` |
|    458433 | 13444 | `		if( !pNode->pLeft ){` |
|         - | 13445 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3897 | 13446 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1946 | 13447 | `		}` |
|         - | 13448 | `		/* Phase#6: Compile the 'else' expression */` |
|    458433 | 13449 | `		if( pNode->pRight ){` |
|    458433 | 13450 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    458433 | 13451 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    458433 | 13452 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13453 | `				return rc;` |
|         - | 13454 | `			}` |
|    458433 | 13455 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    229214 | 13456 | `		}` |
|    458433 | 13457 | `		if( nJmp > 0 ){` |
|         - | 13458 | `			/* Phase#7: Fix the unconditional jump */` |
|    458433 | 13459 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    458433 | 13460 | `			if( pInstr ){` |
|    458433 | 13461 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    229214 | 13462 | `			}` |
|    229214 | 13463 | `		}` |
|         - | 13464 | `		/* All done */` |
|    458433 | 13465 | `		return SXRET_OK;` |
|         - | 13466 | `	}` |
|  26313837 | 13467 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13468 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13469 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13470 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13471 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13472 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13473 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13474 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13475 | `		sxu32 nPipeNsBase;` |
|        27 | 13476 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13477 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13478 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13479 | `				"'\|>': Missing operand");` |
|       ! 0 | 13480 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13481 | `		}` |
|         - | 13482 | `		/* Argument: the LHS value. */` |
|        27 | 13483 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13484 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13485 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13486 | `			return rc;` |
|         - | 13487 | `		}` |
|        27 | 13488 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13489 | `		/* Callable: the RHS. */` |
|        27 | 13490 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13491 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13492 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13493 | `			return rc;` |
|         - | 13494 | `		}` |
|        27 | 13495 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13496 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13497 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13498 | `		return SXRET_OK;` |
|         - | 13499 | `	}` |
|  26313811 | 13500 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13501 | `	/* Generate code for the left tree */` |
|  26313811 | 13502 | `	if( pNode->pLeft ){` |
|  26290879 | 13503 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  26290879 | 13504 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13505 | `			ph7_expr_node **apNode;` |
|   6179767 | 13506 | `			int hasSpread = 0;` |
|   6179767 | 13507 | `			int hasNamed = 0;` |
|   6179767 | 13508 | `			int bAnySpread = 0;` |
|   6179767 | 13509 | `			sxu32 byRefMask = 0;` |
|         - | 13510 | `			sxi32 nArgs;` |
|         - | 13511 | `			sxi32 n;` |
|         - | 13512 | `			/* Recurse and generate bytecodes for function arguments */` |
|   6179767 | 13513 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6179767 | 13514 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13515 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13516 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13517 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   6179767 | 13518 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13519 | `				bFcc = 1;` |
|        81 | 13520 | `				nArgs = 0;` |
|        40 | 13521 | `			}` |
|         - | 13522 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13523 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13524 | `			{` |
|   6179767 | 13525 | `				int seenNamed = 0;` |
|   6179767 | 13526 | `				int seenSpread = 0;` |
|  12993073 | 13527 | `				for( n = 0; n < nArgs; ++n ){` |
|   6813313 | 13528 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4023 | 13529 | `						bAnySpread = 1;` |
|      4023 | 13530 | `						seenSpread = 1;` |
|      4023 | 13531 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13532 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13533 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13534 | `							return SXERR_SYNTAX;` |
|         5 | 13535 | `						}` |
|   6811304 | 13536 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13537 | `						seenNamed = 1;` |
|       289 | 13538 | `						hasNamed = 1;` |
|   6809153 | 13539 | `					}else if( seenNamed ){` |
|         3 | 13540 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13541 | `							"Cannot use positional argument after named argument");` |
|         3 | 13542 | `						return SXERR_SYNTAX;` |
|   6809009 | 13543 | `					}else if( seenSpread ){` |
|       ! 0 | 13544 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13545 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13546 | `						return SXERR_SYNTAX;` |
|         - | 13547 | `					}` |
|   3406658 | 13548 | `				}` |
|         - | 13549 | `			}` |
|         - | 13550 | `			/* Read-only load */` |
|   6179765 | 13551 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13552 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13553 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13554 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13555 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   6179765 | 13556 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   6179765 | 13557 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   6179760 | 13558 | `				if( pCallName->nByte == 5` |
|   3466070 | 13559 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    313959 | 13560 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6022788 | 13561 | `				}else if( pCallName->nByte == 5` |
|   3152116 | 13562 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       117 | 13563 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        56 | 13564 | `				}` |
|         - | 13565 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13566 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13567 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13568 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13569 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13570 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   6179765 | 13571 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13572 | `					SyString sBuiltin;` |
|   6175617 | 13573 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   6175617 | 13574 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3087806 | 13575 | `				}` |
|   3089880 | 13576 | `			}` |
|  12993069 | 13577 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   6813309 | 13578 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   6813309 | 13579 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13580 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13581 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13582 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13583 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13584 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13585 | `				 * (iP1=0 either way). */` |
|   6813309 | 13586 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38329 | 13587 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38329 | 13588 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19162 | 13589 | `				}` |
|   6813309 | 13590 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   6813309 | 13591 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13592 | `					return rc;` |
|         - | 13593 | `				}` |
|         - | 13594 | `				/* Each argument is an independent nullsafe scope. */` |
|   6813309 | 13595 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   6813309 | 13596 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13597 | `					/* Emit spread opcode to unpack this array argument */` |
|      4023 | 13598 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4023 | 13599 | `					hasSpread = 1;` |
|      2009 | 13600 | `				}` |
|   3406657 | 13601 | `			}` |
|         - | 13602 | `			/* Total number of given arguments */` |
|   6179765 | 13603 | `			iP1 = nArgs;` |
|   6179765 | 13604 | `			iP2 = hasSpread;` |
|         - | 13605 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13606 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   6179765 | 13607 | `			if( hasNamed ){` |
|       178 | 13608 | `				sxu32 nStrBytes = 0;` |
|         - | 13609 | `				char *zBuf;` |
|       534 | 13610 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13611 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13612 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13613 | `					}` |
|       182 | 13614 | `				}` |
|         - | 13615 | `				{` |
|       178 | 13616 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13617 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13618 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13619 | `				if( pMap ){` |
|       178 | 13620 | `					SyZero(pMap, mapSize);` |
|       178 | 13621 | `					pMap->bHasNamed = 1;` |
|       178 | 13622 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13623 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13624 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13625 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13626 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13627 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13628 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13629 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13630 | `							zBuf += nb;` |
|       141 | 13631 | `						}` |
|         - | 13632 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13633 | `					}` |
|       178 | 13634 | `					p3 = (void *)pMap;` |
|        87 | 13635 | `				}` |
|         - | 13636 | `				}` |
|        87 | 13637 | `			}` |
|         - | 13638 | `			/* Remove stale flags now */` |
|   6179765 | 13639 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3089880 | 13640 | `		}` |
|         - | 13641 | `		{` |
|         - | 13642 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13643 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13644 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13645 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13646 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13647 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13648 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13649 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  26290877 | 13650 | `			sxi32 iLeftFlags = iFlags;` |
|  26290872 | 13651 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  21606159 | 13652 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   8460749 | 13653 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7338097 | 13654 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2425615 | 13655 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1212805 | 13656 | `			}` |
|         - | 13657 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13658 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13659 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13660 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13661 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13662 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13663 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  26290872 | 13664 | `			if( pNode->pOp` |
|  37144441 | 13665 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  23999052 | 13666 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  21707180 | 13667 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4951847 | 13668 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2475921 | 13669 | `			}` |
|         - | 13670 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13671 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13672 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13673 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13674 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13675 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  26290872 | 13676 | `			if( pNode->pOp` |
|  26290877 | 13677 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    195529 | 13678 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     97762 | 13679 | `			}` |
|         - | 13680 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 13681 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 13682 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 13683 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 13684 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 13685 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 13686 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  26290872 | 13687 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC` |
|  13174198 | 13688 | `				&& pNode->pLeft && pNode->pLeft->pOp` |
|     86223 | 13689 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     57467 | 13690 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     57449 | 13691 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|        39 | 13692 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        19 | 13693 | `			}` |
|  26290877 | 13694 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13695 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13696 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     11695 | 13697 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      5845 | 13698 | `			}` |
|  26290877 | 13699 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13700 | `		}` |
|  26290877 | 13701 | `		if( rc != SXRET_OK ){` |
|        34 | 13702 | `			return rc;` |
|         - | 13703 | `		}` |
|  26290847 | 13704 | `		if( !bIsChainOp ){` |
|         - | 13705 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13706 | `			 * target the end of that LHS chain, which is right here. */` |
|  12283997 | 13707 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6141996 | 13708 | `		}` |
|  26290847 | 13709 | `		if( iVmOp == PH7_OP_CALL ){` |
|   6179765 | 13710 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6179765 | 13711 | `			if( pInstr ){` |
|   6179765 | 13712 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   4553377 | 13713 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13714 | `					sxu32 nQual;` |
|   4553377 | 13715 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13716 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13717 | `					 * so the later NEW handler (if any) can see it. */` |
|   4553377 | 13718 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13719 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13720 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13721 | `					 * imports — class imports must NOT affect function` |
|         - | 13722 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13723 | `					 * before NEW; we store the original literal index in the` |
|         - | 13724 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13725 | `					 * the unqualified name and re-qualify with class imports. */` |
|   4553377 | 13726 | `					if( bAbsolute ){` |
|      3867 | 13727 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1936 | 13728 | `					}else{` |
|   4549515 | 13729 | `						int fromImport = 0;` |
|   4549515 | 13730 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   4549515 | 13731 | `						pInstr->iP2 = (sxi32)nQual;` |
|   4549515 | 13732 | `						if( nQual != nOrig ){` |
|         - | 13733 | `							/* Record the original literal index in the arg map` |
|         - | 13734 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13735 | `							 * flag) so the NEW handler can recover the` |
|         - | 13736 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13737 | `							 * imports. */` |
|        97 | 13738 | `							if( p3 == 0 ){` |
|        97 | 13739 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        92 | 13740 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        97 | 13741 | `								if( pMap ){` |
|        97 | 13742 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        97 | 13743 | `									p3 = (void *)pMap;` |
|        46 | 13744 | `								}` |
|        46 | 13745 | `							}` |
|        97 | 13746 | `							if( p3 ){` |
|        97 | 13747 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        97 | 13748 | `								if( !fromImport ){` |
|         - | 13749 | `									/* Mark as namespace-qualified */` |
|        87 | 13750 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        41 | 13751 | `								}` |
|        46 | 13752 | `							}` |
|        46 | 13753 | `						}` |
|         - | 13754 | `					}` |
|   3903079 | 13755 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1616488 | 13756 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    823101 | 13757 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13758 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 13759 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 13760 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 13761 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 13762 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 13763 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 13764 | ``					 * the method call `$o->p()`. */`` |
|   1606595 | 13765 | `					pInstr->iP2 = 1;` |
|         - | 13766 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 13767 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 13768 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 13769 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 13770 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 13771 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 13772 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1606595 | 13773 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 13774 | `						void *pDynName = pInstr->p3;` |
|        11 | 13775 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 13776 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 13777 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 13778 | `					}` |
|    803295 | 13779 | `				}` |
|   3089885 | 13780 | `			}` |
|  23200967 | 13781 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13782 | `			ph7_expr_node **apNode;` |
|         - | 13783 | `			sxi32 n;` |
|   2875253 | 13784 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13785 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13786 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13787 | `			/* Recurse and generate bytecodes for array index */` |
|   2875253 | 13788 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5528313 | 13789 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2653065 | 13790 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2653065 | 13791 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2653065 | 13792 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13793 | `					return rc;` |
|         - | 13794 | `				}` |
|         - | 13795 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2653065 | 13796 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1326535 | 13797 | `			}` |
|   2875253 | 13798 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2653065 | 13799 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1326530 | 13800 | `			}` |
|   2875253 | 13801 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13802 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    352091 | 13803 | `				iP2 = 4;` |
|   2699210 | 13804 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13805 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13806 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23025 | 13807 | `				iP2 = 5;` |
|   2511657 | 13808 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13809 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13810 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13811 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        31 | 13812 | `				iP2 = 6;` |
|   2500134 | 13813 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13814 | `				/* Create an empty entry when the desired index is not found */` |
|    532479 | 13815 | `				iP2 = 1;` |
|    266242 | 13816 | `			}` |
|  18673463 | 13817 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13818 | `			/* POP the left node */` |
|         5 | 13819 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13820 | `		}` |
|  13145421 | 13821 | `	}` |
|  26313779 | 13822 | `	rc = SXRET_OK;` |
|  26313779 | 13823 | `	nJmpIdx = 0;` |
|         - | 13824 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13825 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13826 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  26313779 | 13827 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    391035 | 13828 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    391035 | 13829 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    391035 | 13830 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    391035 | 13831 | `			int isSpecial = 0;` |
|    391035 | 13832 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    345135 | 13833 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    345135 | 13834 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    345130 | 13835 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    314450 | 13836 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    170610 | 13837 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    103419 | 13838 | `					isSpecial = 1;` |
|     51707 | 13839 | `				}` |
|    184040 | 13840 | `			}` |
|    413985 | 13841 | `			pInstr->iP1 = 0;` |
|         - | 13842 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 13843 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 13844 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 13845 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 13846 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 13847 | `			{` |
|    598025 | 13848 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    552120 | 13849 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    368085 | 13850 | `				if( !isSpecial && !bAbsolute ){` |
|    264653 | 13851 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    132324 | 13852 | `				}` |
|         - | 13853 | `			}` |
|         - | 13854 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13855 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    368085 | 13856 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    264671 | 13857 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    264671 | 13858 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        70 | 13859 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        70 | 13860 | `					return SXRET_OK;` |
|         - | 13861 | `				}` |
|    132300 | 13862 | `			}` |
|    184007 | 13863 | `		}` |
|    229880 | 13864 | `	}` |
|         - | 13865 | `	/* Generate code for the right tree */` |
|  26290781 | 13866 | `	if( pNode->pRight ){` |
|  15092621 | 13867 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13868 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    413525 | 13869 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14885861 | 13870 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13871 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    286971 | 13872 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14535618 | 13873 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13874 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     57529 | 13875 | `			iVmOp = 0; /* No binary operator to emit */` |
|     57529 | 13876 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  14363425 | 13877 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13878 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13879 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13880 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13881 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13882 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13883 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13884 | `			sxu32 nNsJmp = 0;` |
|       108 | 13885 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13886 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  14334559 | 13887 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13888 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13889 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13890 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4815211 | 13891 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2407603 | 13892 | `		}` |
|  15092621 | 13893 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15092621 | 13894 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  15092621 | 13895 | `		if( !bIsChainOp ){` |
|         - | 13896 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13897 | `			 * operator instruction is emitted. */` |
|  10140845 | 13898 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5070420 | 13899 | `		}` |
|  15092621 | 13900 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4378933 | 13901 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4378896 | 13902 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13903 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13904 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13905 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13906 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13907 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13908 | `				 */` |
|        91 | 13909 | `				iVmOp = 0;` |
|   4378890 | 13910 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4378847 | 13911 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13912 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    780821 | 13913 | `					iP2 = 1;` |
|    390413 | 13914 | `				}else{` |
|   3598031 | 13915 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13916 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    513261 | 13917 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    513261 | 13918 | `						iP1 = pInstr->iP1;` |
|    256633 | 13919 | `					}else{` |
|   3084775 | 13920 | `						p3 = pInstr->p3;` |
|         - | 13921 | `					}` |
|         - | 13922 | `					/* POP the last dynamic load instruction */` |
|   3598031 | 13923 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13924 | `				}` |
|   2189426 | 13925 | `			}` |
|  12903157 | 13926 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13927 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13928 | `			if( pInstr ){` |
|        63 | 13929 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13930 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13931 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13932 | `					 */` |
|        19 | 13933 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13934 | `					iP1 = pInstr->iP1;` |
|        19 | 13935 | `					iP2 = pInstr->iP2;` |
|        19 | 13936 | `					p3  = pInstr->p3;` |
|        10 | 13937 | `				}else{` |
|        45 | 13938 | `					p3 = pInstr->p3;` |
|         - | 13939 | `				}` |
|        30 | 13940 | `			}` |
|        30 | 13941 | `		}` |
|   7546308 | 13942 | `	}` |
|  26290776 | 13943 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    376462 | 13944 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13945 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13946 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13947 | `		iVmOp = 0;` |
|        14 | 13948 | `	}` |
|  26290781 | 13949 | `	if( iVmOp > 0 ){` |
|  26233139 | 13950 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    195529 | 13951 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13952 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15333 | 13953 | `				iP1 = 1;` |
|      7669 | 13954 | `			}` |
|  26135377 | 13955 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13956 | `			/* Namespace-qualify the class name for NEW */ {` |
|    752547 | 13957 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    752547 | 13958 | `				VmInstr *pCallInstr = 0;` |
|    752547 | 13959 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    752231 | 13960 | `					pCallInstr = pPeek;` |
|    752231 | 13961 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    376113 | 13962 | `				}` |
|    752547 | 13963 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    737245 | 13964 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13965 | `					sxu32 nLitForClass;` |
|    737245 | 13966 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13967 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13968 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13969 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13970 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13971 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13972 | `					 * with class imports. */` |
|    737245 | 13973 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        53 | 13974 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        29 | 13975 | `					}else{` |
|    737197 | 13976 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13977 | `					}` |
|    737245 | 13978 | `					pPeek->iP1 = 0;` |
|    737245 | 13979 | `					if( !bAbsolute ){` |
|         - | 13980 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 13981 | `						 * current class — never namespace-qualify them (else` |
|         - | 13982 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 13983 | `						 * instanceof (IS_A) guard below. */` |
|    733393 | 13984 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    733393 | 13985 | `						int isSpecialNew = 0;` |
|    733393 | 13986 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    733393 | 13987 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    733393 | 13988 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    733388 | 13989 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    733432 | 13990 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    366739 | 13991 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        33 | 13992 | `								isSpecialNew = 1;` |
|        16 | 13993 | `							}` |
|    366694 | 13994 | `						}` |
|    733393 | 13995 | `						if( isSpecialNew ){` |
|        33 | 13996 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|        17 | 13997 | `						}else{` |
|    733361 | 13998 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 13999 | `						}` |
|    366699 | 14000 | `					}else{` |
|      3857 | 14001 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 14002 | `					}` |
|    368620 | 14003 | `				}` |
|         - | 14004 | `			}` |
|    752547 | 14005 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    752547 | 14006 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 14007 | `				VmInstr *pPrev;` |
|    752231 | 14008 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    752231 | 14009 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 14010 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 14011 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 14012 | `					 * accumulator exactly like OP_CALL would have). */` |
|    752231 | 14013 | `					iP1 = pInstr->iP1;` |
|    752231 | 14014 | `					iP2 = pInstr->iP2;` |
|    752231 | 14015 | `					if( pInstr->p3 ){` |
|        63 | 14016 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        29 | 14017 | `					}` |
|    752231 | 14018 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    376113 | 14019 | `				}` |
|    376118 | 14020 | `			}` |
|  25661344 | 14021 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 14022 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 14023 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     76769 | 14024 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     76769 | 14025 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     76769 | 14026 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     76769 | 14027 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     76769 | 14028 | `				int isSpecialIs = 0;` |
|     76769 | 14029 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     76769 | 14030 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     76769 | 14031 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     76764 | 14032 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     76767 | 14033 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     38382 | 14034 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 14035 | `						isSpecialIs = 1;` |
|         5 | 14036 | `					}` |
|     38382 | 14037 | `				}` |
|     76769 | 14038 | `				pInstr->iP1 = 0;` |
|     76769 | 14039 | `				if( !isSpecialIs && !bAbsolute ){` |
|     76749 | 14040 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     38372 | 14041 | `				}` |
|     38387 | 14042 | `			}` |
|  25246691 | 14043 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 14044 | `			/* Prevent constant expansion for member/property names.` |
|         - | 14045 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 14046 | `			 * should not trigger constant lookup. */` |
|   4951781 | 14047 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4951781 | 14048 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4718421 | 14049 | `				pInstr->iP1 = 0;` |
|   2359208 | 14050 | `			}` |
|   4951781 | 14051 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 14052 | `				/* Static member access,remember that */` |
|    368037 | 14053 | `				iP1 = 1;` |
|    368037 | 14054 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    368037 | 14055 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    229525 | 14056 | `					p3 = pInstr->p3;` |
|    229525 | 14057 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    114760 | 14058 | `				}` |
|    184016 | 14059 | `			}` |
|         - | 14060 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 14061 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 14062 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 14063 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4951781 | 14064 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4951781 | 14065 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 14066 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4951761 | 14067 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     61327 | 14068 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4921080 | 14069 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 14070 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4890411 | 14071 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 14072 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    949263 | 14073 | `					iP2 = PH7_MEMBER_WRITE;` |
|    474629 | 14074 | `				}` |
|   2475888 | 14075 | `			}` |
|   2475888 | 14076 | `		}` |
|         - | 14077 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 14078 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 14079 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 14080 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 14081 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  26233139 | 14082 | `		if( bFcc ){` |
|        81 | 14083 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 14084 | `			iP2 = 0;` |
|        81 | 14085 | `			p3 = 0;` |
|        81 | 14086 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 14087 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 14088 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 14089 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 14090 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 14091 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 14092 | `				void *pMemberName = pInstr->p3;` |
|        37 | 14093 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 14094 | `				if( pMemberName ){` |
|       ! 0 | 14095 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 14096 | `				}` |
|        37 | 14097 | `				iP1 = 2;` |
|        19 | 14098 | `			}else{` |
|        45 | 14099 | `				iP1 = 1;` |
|         - | 14100 | `			}` |
|        40 | 14101 | `		}` |
|         - | 14102 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 14103 | `		 * This is the primary emit path for user-visible calls. */` |
|  26233139 | 14104 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6932227 | 14105 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3466111 | 14106 | `		}` |
|         - | 14107 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  26233139 | 14108 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  13116567 | 14109 | `	}` |
|  26290781 | 14110 | `	if( nJmpIdx > 0 ){` |
|         - | 14111 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    758015 | 14112 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    758015 | 14113 | `		if( pInstr ){` |
|    758015 | 14114 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    379005 | 14115 | `		}` |
|    379005 | 14116 | `	}` |
|  26290781 | 14117 | `	return rc;` |
|  33729962 | 14118 | `}` |
|         - | 14119 | `/*` |
|         - | 14120 | ` * Compile a PHP expression.` |
|         - | 14121 | ` * According to the PHP language reference manual:` |
|         - | 14122 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 14123 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 14124 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 14125 | ` *  is "anything that has a value".` |
|         - | 14126 | ` * If something goes wrong while compiling the expression,this` |
|         - | 14127 | ` * function takes care of generating the appropriate error` |
|         - | 14128 | ` * message.` |
|         - | 14129 | ` */` |
|         - | 14130 | `/*` |
|         - | 14131 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 14132 | ` *` |
|         - | 14133 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 14134 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 14135 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 14136 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 14137 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 14138 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 14139 | ` * except for() now reports php's parse error.` |
|         - | 14140 | ` */` |
| 223628040 | 14141 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 14142 | `{` |
|         - | 14143 | `	ph7_expr_node **apArg;` |
|         - | 14144 | `	sxu32 n;` |
| 223628045 | 14145 | `	if( pNode == 0 ){` |
| 157201631 | 14146 | `		return 0;` |
|         - | 14147 | `	}` |
|  66426419 | 14148 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 14149 | `		return 1;` |
|         - | 14150 | `	}` |
|  66426410 | 14151 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  66426411 | 14152 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 14153 | `		return 1;` |
|         - | 14154 | `	}` |
|  66426411 | 14155 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  75869911 | 14156 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   9443505 | 14157 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 14158 | `			return 1;` |
|         - | 14159 | `		}` |
|   4721755 | 14160 | `	}` |
|  66426411 | 14161 | `	return 0;` |
| 111814025 | 14162 | `}` |
|  15261578 | 14163 | `static sxi32 PH7_CompileExpr(` |
|         - | 14164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14165 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 14166 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 14167 | `	)` |
|         5 | 14168 | `{` |
|         - | 14169 | `	ph7_expr_node *pRoot;` |
|         - | 14170 | `	SySet sExprNode;` |
|         - | 14171 | `	SyToken *pEnd;` |
|         - | 14172 | `	sxi32 nExpr;` |
|         - | 14173 | `	sxi32 iNest;` |
|         - | 14174 | `	sxi32 rc;` |
|         - | 14175 | `	sxu32 nNullsafeBase;` |
|         - | 14176 | `	/* Initialize worker variables */` |
|  15261583 | 14177 | `	nExpr = 0;` |
|  15261583 | 14178 | `	pRoot = 0;` |
|         - | 14179 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 14180 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  15261583 | 14181 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15261583 | 14182 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  15261583 | 14183 | `	SySetAlloc(&sExprNode,0x10);` |
|  15261583 | 14184 | `	rc = SXRET_OK;` |
|         - | 14185 | `	/* Delimit the expression */` |
|  15261583 | 14186 | `	pEnd = pGen->pIn;` |
|  15261583 | 14187 | `	iNest = 0;` |
| 119312097 | 14188 | `	while( pEnd < pGen->pEnd ){` |
| 113388161 | 14189 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14190 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4657 | 14191 | `			iNest++;` |
| 113385835 | 14192 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4665 | 14193 | `			iNest--;` |
| 113381179 | 14194 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   9338511 | 14195 | `			if( iNest <= 0 ){` |
|   9337647 | 14196 | `				break;` |
|         - | 14197 | `			}` |
|       432 | 14198 | `		}` |
| 104050519 | 14199 | `		pEnd++;` |
|         5 | 14200 | `	}` |
|  15261583 | 14201 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    643499 | 14202 | `		SyToken *pEnd2 = pGen->pIn;` |
|    643499 | 14203 | `		iNest = 0;` |
|         - | 14204 | `		/* Stop at the first comma */` |
|   1414457 | 14205 | `		while( pEnd2 < pEnd ){` |
|    770965 | 14206 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     42181 | 14207 | `				iNest++;` |
|    749877 | 14208 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     42181 | 14209 | `				iNest--;` |
|    707701 | 14210 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 14211 | `				if( iNest <= 0 ){` |
|         3 | 14212 | `					break;` |
|         - | 14213 | `				}` |
|      3027 | 14214 | `			}` |
|    770963 | 14215 | `			pEnd2++;` |
|         5 | 14216 | `		}` |
|    643499 | 14217 | `		if( pEnd2 <pEnd ){` |
|         3 | 14218 | `			pEnd = pEnd2;` |
|         1 | 14219 | `		}` |
|    321747 | 14220 | `	}` |
|  15261583 | 14221 | `	if( pEnd > pGen->pIn ){` |
|  15238631 | 14222 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 14223 | `		/* Swap delimiter */` |
|  15238631 | 14224 | `		pGen->pEnd = pEnd;` |
|         - | 14225 | `		/* Try to get an expression tree */` |
|  15238631 | 14226 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  15238626 | 14227 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  15071885 | 14228 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 14229 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 14230 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 14231 | `				"syntax error, unexpected token \",\"");` |
|         6 | 14232 | `			pGen->pEnd = pTmp;` |
|         6 | 14233 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14234 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 14235 | `				return SXERR_ABORT;` |
|         - | 14236 | `			}` |
|         6 | 14237 | `			pGen->pIn = pEnd;` |
|         6 | 14238 | `			SySetRelease(&sExprNode);` |
|         6 | 14239 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 14240 | `			return SXRET_OK;` |
|         - | 14241 | `		}` |
|  15238627 | 14242 | `		if( rc == SXRET_OK && pRoot ){` |
|  15238443 | 14243 | `			rc = SXRET_OK;` |
|  15238443 | 14244 | `			if( xTreeValidator ){` |
|         - | 14245 | `				/* Call the upper layer validator callback */` |
|    976967 | 14246 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    488481 | 14247 | `			}` |
|  15238443 | 14248 | `			if( rc != SXERR_ABORT ){` |
|         - | 14249 | `				/* Generate code for the given tree */` |
|  15238443 | 14250 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 14251 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 14252 | `				 * expression so they short-circuit to its end. */` |
|  15238443 | 14253 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   7619219 | 14254 | `			}` |
|  15238443 | 14255 | `			nExpr = 1;` |
|   7619219 | 14256 | `		}` |
|         - | 14257 | `		/* Release the whole tree */` |
|  15238627 | 14258 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 14259 | `		/* Synchronize token stream */` |
|  15238627 | 14260 | `		pGen->pEnd = pTmp;` |
|  15238627 | 14261 | `		pGen->pIn  = pEnd;` |
|  15238627 | 14262 | `		if( rc == SXERR_ABORT ){` |
|        12 | 14263 | `			SySetRelease(&sExprNode);` |
|        12 | 14264 | `			return SXERR_ABORT;` |
|         - | 14265 | `		}` |
|   7619306 | 14266 | `	}` |
|  15261569 | 14267 | `	SySetRelease(&sExprNode);` |
|  15261569 | 14268 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   7630794 | 14269 | `}` |
|         - | 14270 | `/*` |
|         - | 14271 | ` * Return a pointer to the node construct handler associated` |
|         - | 14272 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 14273 | ` */` |
|   8787124 | 14274 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 14275 | `{` |
|   8787129 | 14276 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 14277 | `		/* Numeric literal: Either real or integer */` |
|   3563719 | 14278 | `		return PH7_CompileNumLiteral;` |
|   5223415 | 14279 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 14280 | `		/* Double quoted string */` |
|    120497 | 14281 | `		return PH7_CompileString;` |
|   5102923 | 14282 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 14283 | `		/* Single quoted string */` |
|   5102803 | 14284 | `		return PH7_CompileSimpleString;` |
|       124 | 14285 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 14286 | `		/* Heredoc */` |
|        70 | 14287 | `		return PH7_CompileHereDoc;` |
|        58 | 14288 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 14289 | `		/* Nowdoc */` |
|        52 | 14290 | `		return PH7_CompileNowDoc;` |
|         8 | 14291 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 14292 | `		/* Backtick quoted string */` |
|         6 | 14293 | `		return PH7_CompileBacktic;` |
|         - | 14294 | `	}` |
|         3 | 14295 | `	return 0;` |
|   4393567 | 14296 | `}` |
|         - | 14297 | `/*` |
|         - | 14298 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 14299 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 14300 | ` * in write context" parse error.` |
|         - | 14301 | ` */` |
|     23062 | 14302 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 14303 | `{` |
|         - | 14304 | `	sxi32 rc;` |
|     23067 | 14305 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23065 | 14306 | `		return SXRET_OK;` |
|         - | 14307 | `	}` |
|         5 | 14308 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 14309 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 14310 | `		"Can't use nullsafe operator in write context");` |
|         3 | 14311 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11536 | 14312 | `}` |
|         - | 14313 | `/*` |
|         - | 14314 | ` * Compile an unset() statement.` |
|         - | 14315 | ` * unset($var, $arr[$key], ...);` |
|         - | 14316 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 14317 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 14318 | ` * parent array before extracting the element to unset.` |
|         - | 14319 | ` */` |
|     25916 | 14320 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 14321 | `{` |
|     25921 | 14322 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     25921 | 14323 | `	sxu32 nIdx = 0;` |
|         - | 14324 | `	SyString sName;` |
|         - | 14325 | `	sxi32 rc;` |
|         - | 14326 | `	/* Jump the 'unset' keyword */` |
|     25921 | 14327 | `	pGen->pIn++;` |
|         - | 14328 | `	/* Save delimiter */` |
|     25921 | 14329 | `	pTmp = pGen->pEnd;` |
|         - | 14330 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     25921 | 14331 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     25921 | 14332 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14333 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 14334 | `		SyToken *pClose;` |
|     25921 | 14335 | `		pGen->pIn++;   /* Skip '(' */` |
|     25921 | 14336 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     25921 | 14337 | `		pEnd = pClose; /* Stop at ')' */` |
|     12958 | 14338 | `	}` |
|     25921 | 14339 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 14340 | `	/* Resolve the 'unset' builtin name once */` |
|     25921 | 14341 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3829 | 14342 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3829 | 14343 | `		if( pObj == 0 ){` |
|       ! 0 | 14344 | `			return SXERR_ABORT;` |
|         - | 14345 | `		}` |
|      3829 | 14346 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3829 | 14347 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1912 | 14348 | `	}` |
|         - | 14349 | `	/* Compile each comma-separated argument */` |
|     56063 | 14350 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30147 | 14351 | `		if( pGen->pIn < pNext ){` |
|         - | 14352 | `			/*` |
|         - | 14353 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 14354 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 14355 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 14356 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 14357 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 14358 | `			 * already removes just the element/property.` |
|         - | 14359 | `			 */` |
|     30142 | 14360 | `			if( &pGen->pIn[2] == pNext` |
|     18611 | 14361 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      7085 | 14362 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 14363 | `				SyString *pVarName;` |
|     10622 | 14364 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      7078 | 14365 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      7083 | 14366 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      7083 | 14367 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 14368 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 14369 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 14370 | `					return SXERR_ABORT;` |
|         - | 14371 | `				}` |
|      7083 | 14372 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      7083 | 14373 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      7083 | 14374 | `				pGen->pIn = pNext;` |
|      7083 | 14375 | `				if( pGen->pIn < pEnd ){` |
|      4227 | 14376 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2111 | 14377 | `				}` |
|      7083 | 14378 | `				continue;` |
|         - | 14379 | `			}` |
|     23069 | 14380 | `			pGen->pEnd = pNext;` |
|     23069 | 14381 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 14382 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 14383 | `				GenStateUnsetValidator);` |
|     23069 | 14384 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14385 | `				return SXERR_ABORT;` |
|         - | 14386 | `			}` |
|     23069 | 14387 | `			if( rc != SXERR_EMPTY ){` |
|         - | 14388 | `				/* Emit call for this single argument */` |
|     23067 | 14389 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23067 | 14390 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23067 | 14391 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11531 | 14392 | `			}` |
|     11532 | 14393 | `		}` |
|         - | 14394 | `		/* Jump trailing commas */` |
|     23075 | 14395 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 14396 | `			pNext++;` |
|         1 | 14397 | `		}` |
|     23069 | 14398 | `		pGen->pIn = pNext;` |
|         5 | 14399 | `	}` |
|         - | 14400 | `	/* Skip past the closing ')' if present */` |
|     25921 | 14401 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     25921 | 14402 | `		pGen->pIn++;` |
|     12958 | 14403 | `	}` |
|         - | 14404 | `	/* Restore token stream */` |
|     25921 | 14405 | `	pGen->pEnd = pTmp;` |
|     25921 | 14406 | `	return SXRET_OK;` |
|     12963 | 14407 | `}` |
|         - | 14408 | `/*` |
|         - | 14409 | ` * PHP Language construct table.` |
|         - | 14410 | ` */` |
|         - | 14411 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 14412 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 14413 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14414 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14415 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14416 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14417 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14418 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14419 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14420 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14421 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14422 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14423 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14424 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14425 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14426 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14427 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14428 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14429 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14430 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14431 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14432 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14433 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14434 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14435 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14436 | `};` |
|         - | 14437 | `/*` |
|         - | 14438 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14439 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14440 | ` */` |
|   7390664 | 14441 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14442 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14443 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14444 | `	)` |
|         5 | 14445 | `{` |
|   7390669 | 14446 | `	sxu32 n = 0;` |
|  29224640 | 14447 | `	for(;;){` |
|  58449285 | 14448 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    430717 | 14449 | `			break;` |
|         - | 14450 | `		}` |
|  58018573 | 14451 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6959957 | 14452 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14453 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14454 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14455 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14456 | `					return 0;` |
|         - | 14457 | `				}` |
|       ! 0 | 14458 | `			}` |
|   6959952 | 14459 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11486 | 14460 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5750 | 14461 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14462 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14463 | `				return 0;` |
|         - | 14464 | `			}` |
|         - | 14465 | `			/* Return a pointer to the handler.` |
|         - | 14466 | `			*/` |
|   6959955 | 14467 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14468 | `		}` |
|  51058621 | 14469 | `		n++;` |
|         5 | 14470 | `	}` |
|    430717 | 14471 | `	if( pLookahed ){` |
|    430717 | 14472 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     68953 | 14473 | `			return PH7_CompileClassInterface;` |
|    361769 | 14474 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    315311 | 14475 | `			return PH7_CompileClass;` |
|     46463 | 14476 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7739 | 14477 | `			return PH7_CompileTrait;` |
|         - | 14478 | `		}` |
|         - | 14479 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14480 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14481 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14482 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19362 | 14483 | `	}` |
|         - | 14484 | `	/* Not a language construct */` |
|     38729 | 14485 | `	return 0;` |
|   3695337 | 14486 | `}` |
|         - | 14487 | `/*` |
|         - | 14488 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14489 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14490 | ` */` |
|     38726 | 14491 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14492 | `{` |
|         - | 14493 | `	int rc;` |
|     38731 | 14494 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38731 | 14495 | `	if( rc == FALSE ){` |
|     38618 | 14496 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15664 | 14497 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14498 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14499 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14500 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14501 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14502 | `			*/` |
|         - | 14503 | `			){` |
|     38615 | 14504 | `				rc = TRUE;` |
|     19305 | 14505 | `		}` |
|     19309 | 14506 | `	}` |
|     38731 | 14507 | `	return rc;` |
|         5 | 14508 | `}` |
|         - | 14509 | `/*` |
|         - | 14510 | ` * Compile a PHP chunk.` |
|         - | 14511 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14512 | ` * takes care of generating the appropriate error message.` |
|         - | 14513 | ` */` |
|         - | 14514 | `/*` |
|         - | 14515 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14516 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14517 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14518 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14519 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14520 | ` * intervening non-declaration statements.` |
|         - | 14521 | ` */` |
|  15920866 | 14522 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14523 | `{` |
|  15920871 | 14524 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  15920871 | 14525 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  15920871 | 14526 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14527 | `	sxu32 nIdx, n;` |
|  15920866 | 14528 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3348949 | 14529 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14530 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14531 | `		 * indexes do not map to the sidecar */` |
|  12571929 | 14532 | `		return;` |
|         - | 14533 | `	}` |
|   3348947 | 14534 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14535 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14536 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3348947 | 14537 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10048325 | 14538 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6699383 | 14539 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6691571 | 14540 | `			continue;` |
|         - | 14541 | `		}` |
|      7817 | 14542 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14543 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7805 | 14544 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7793 | 14545 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3894 | 14546 | `		}` |
|      3911 | 14547 | `	}` |
|   7960438 | 14548 | `}` |
|         - | 14549 | `/*` |
|         - | 14550 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14551 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14552 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14553 | ` */` |
|   4079808 | 14554 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14555 | `{` |
|         - | 14556 | `	char *zDup;` |
|   4079813 | 14557 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4079793 | 14558 | `		return;` |
|         - | 14559 | `	}` |
|        35 | 14560 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14561 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14562 | `	if( zDup ){` |
|        25 | 14563 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14564 | `	}` |
|        25 | 14565 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2039909 | 14566 | `}` |
|         - | 14567 | `/*` |
|         - | 14568 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14569 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14570 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14571 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14572 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14573 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14574 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14575 | ` */` |
|      7800 | 14576 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14577 | `{` |
|         - | 14578 | `	SySet *pToken;` |
|         - | 14579 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14580 | `	char *zSpan;` |
|      7805 | 14581 | `	sxi32 rc = SXRET_OK;` |
|      7805 | 14582 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14583 | `		return SXRET_OK;` |
|         - | 14584 | `	}` |
|     11705 | 14585 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3900 | 14586 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7805 | 14587 | `	if( zSpan == 0 ){` |
|       ! 0 | 14588 | `		return SXRET_OK;` |
|         - | 14589 | `	}` |
|         - | 14590 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14591 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14592 | `	 * the number of attribute declarations in the program. */` |
|      7805 | 14593 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7805 | 14594 | `	if( pToken == 0 ){` |
|       ! 0 | 14595 | `		return SXRET_OK;` |
|         - | 14596 | `	}` |
|      7805 | 14597 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7805 | 14598 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7805 | 14599 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7805 | 14600 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7805 | 14601 | `	pSavedIn = pGen->pIn;` |
|      7805 | 14602 | `	pSavedEnd = pGen->pEnd;` |
|      7809 | 14603 | `	while( pIn < pEnd ){` |
|         - | 14604 | `		ph7_attribute sAttr;` |
|         - | 14605 | `		SyBlob sFQN;` |
|      7809 | 14606 | `		int bAbsolute = 0;` |
|      7809 | 14607 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7809 | 14608 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7809 | 14609 | `		sAttr.nLine = pIn->nLine;` |
|      7809 | 14610 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14611 | `			bAbsolute = 1;` |
|        75 | 14612 | `			pIn++;` |
|        35 | 14613 | `		}` |
|      7809 | 14614 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7809 | 14615 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7809 | 14616 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7809 | 14617 | `			pIn++;` |
|      7809 | 14618 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14619 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14620 | `				pIn++;` |
|       ! 0 | 14621 | `				continue;` |
|         - | 14622 | `			}` |
|      7809 | 14623 | `			break;` |
|       ! 0 | 14624 | `		}` |
|      7809 | 14625 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14626 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14627 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14628 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14629 | `			break;` |
|         - | 14630 | `		}` |
|         - | 14631 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14632 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14633 | `		{` |
|      7809 | 14634 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7809 | 14635 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7809 | 14636 | `			char *zDup = 0;` |
|      7809 | 14637 | `			if( !bAbsolute ){` |
|      7739 | 14638 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7739 | 14639 | `				if( pImp ){` |
|       ! 0 | 14640 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14641 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14642 | `					if( zDup ){` |
|       ! 0 | 14643 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14644 | `					}` |
|      7739 | 14645 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14646 | `					SyBlob sTmp;` |
|       ! 0 | 14647 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14648 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14649 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14650 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14651 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14652 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14653 | `					if( zDup ){` |
|       ! 0 | 14654 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14655 | `					}` |
|       ! 0 | 14656 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14657 | `				}` |
|      3867 | 14658 | `			}` |
|      7809 | 14659 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7809 | 14660 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7809 | 14661 | `				if( zDup ){` |
|      7809 | 14662 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3902 | 14663 | `				}` |
|      3902 | 14664 | `			}` |
|         - | 14665 | `		}` |
|      7809 | 14666 | `		SyBlobRelease(&sFQN);` |
|      7809 | 14667 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14668 | `			SyToken *pArgsEnd;` |
|      7707 | 14669 | `			pIn++;` |
|      7707 | 14670 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15423 | 14671 | `			while( pIn < pArgsEnd ){` |
|      7721 | 14672 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7721 | 14673 | `				sxi32 iDepth = 0;` |
|         - | 14674 | `				ph7_attr_arg sArgRec;` |
|     76725 | 14675 | `				while( pArgStop < pArgsEnd ){` |
|     69025 | 14676 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14677 | `						iDepth++;` |
|     69020 | 14678 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14679 | `						iDepth--;` |
|     69010 | 14680 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14681 | `						break;` |
|         - | 14682 | `					}` |
|     69009 | 14683 | `					pArgStop++;` |
|         5 | 14684 | `				}` |
|      7721 | 14685 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7721 | 14686 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7716 | 14687 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7700 | 14688 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14689 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14690 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14691 | `					if( zN ){` |
|        19 | 14692 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14693 | `					}` |
|        19 | 14694 | `					pArgStart += 2;` |
|         9 | 14695 | `				}` |
|      7721 | 14696 | `				if( pArgStart < pArgStop ){` |
|         - | 14697 | `					SySet *pInstrContainer;` |
|      7721 | 14698 | `					pGen->pIn = pArgStart;` |
|      7721 | 14699 | `					pGen->pEnd = pArgStop;` |
|      7721 | 14700 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7721 | 14701 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7721 | 14702 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7721 | 14703 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7721 | 14704 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7721 | 14705 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14706 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14707 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14708 | `						return SXERR_ABORT;` |
|         - | 14709 | `					}` |
|      7721 | 14710 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3858 | 14711 | `				}` |
|      7721 | 14712 | `				pIn = pArgStop;` |
|      7721 | 14713 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14714 | `					pIn++;` |
|         8 | 14715 | `				}` |
|         5 | 14716 | `			}` |
|      7707 | 14717 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3851 | 14718 | `		}` |
|      7809 | 14719 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7809 | 14720 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14721 | `			pIn++;` |
|         5 | 14722 | `			continue;` |
|         - | 14723 | `		}` |
|      7805 | 14724 | `		break;` |
|       ! 0 | 14725 | `	}` |
|      7805 | 14726 | `	pGen->pIn = pSavedIn;` |
|      7805 | 14727 | `	pGen->pEnd = pSavedEnd;` |
|      7805 | 14728 | `	return SXRET_OK;` |
|      3905 | 14729 | `}` |
|         - | 14730 | `/*` |
|         - | 14731 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14732 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14733 | ` */` |
|   4079812 | 14734 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14735 | `{` |
|   4079817 | 14736 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14737 | `	sxu32 n;` |
|         - | 14738 | `	sxi32 rc;` |
|   4087605 | 14739 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7793 | 14740 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7793 | 14741 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14742 | `			return SXERR_ABORT;` |
|         - | 14743 | `		}` |
|      3899 | 14744 | `	}` |
|   4079817 | 14745 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4079817 | 14746 | `	return SXRET_OK;` |
|   2039911 | 14747 | `}` |
|         - | 14748 | `/*` |
|         - | 14749 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14750 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14751 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14752 | ` */` |
|   2057602 | 14753 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14754 | `{` |
|   2057607 | 14755 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2057607 | 14756 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2057607 | 14757 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14758 | `	sxu32 nIdx, n;` |
|         - | 14759 | `	sxi32 rc;` |
|   2057602 | 14760 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    547171 | 14761 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1510441 | 14762 | `		return SXRET_OK;` |
|         - | 14763 | `	}` |
|    547171 | 14764 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1641501 | 14765 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1094335 | 14766 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14767 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14768 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14769 | `				return SXERR_ABORT;` |
|         - | 14770 | `			}` |
|         6 | 14771 | `		}` |
|    547170 | 14772 | `	}` |
|    547171 | 14773 | `	return SXRET_OK;` |
|   1028806 | 14774 | `}` |
|  11867126 | 14775 | `static sxi32 GenStateCompileChunk(` |
|         - | 14776 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14777 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14778 | `	)` |
|         5 | 14779 | `{` |
|         - | 14780 | `	ProcLangConstruct xCons;` |
|         - | 14781 | `	sxi32 rc;` |
|  11867131 | 14782 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   6893265 | 14783 | `	for(;;){` |
|  12826833 | 14784 | `		int bStmtIsDeclare = 0;` |
|  12826833 | 14785 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14786 | `			/* No more input to process */` |
|     67691 | 14787 | `			break;` |
|         - | 14788 | `		}` |
|         - | 14789 | `		/* Bind a directly-preceding docblock to this statement */` |
|  12759147 | 14790 | `		GenStateSetPendingDoc(&(*pGen));` |
|  12759147 | 14791 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14792 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14793 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14794 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14795 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14796 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7711 | 14797 | `			int bAttrTarget = 0;` |
|      7706 | 14798 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3887 | 14799 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7653 | 14800 | `				bAttrTarget = 1;` |
|      3883 | 14801 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14802 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14803 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14804 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14805 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14806 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14807 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14808 | `					bAttrTarget = 1;` |
|        29 | 14809 | `				}` |
|        29 | 14810 | `			}` |
|      7711 | 14811 | `			if( !bAttrTarget ){` |
|       ! 0 | 14812 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14813 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14814 | `					&pGen->pIn->sData);` |
|       ! 0 | 14815 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14816 | `					break;` |
|         - | 14817 | `				}` |
|       ! 0 | 14818 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14819 | `			}` |
|      3853 | 14820 | `		}` |
|         - | 14821 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14822 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  12759147 | 14823 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7425151 | 14824 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   7425151 | 14825 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14826 | `				bStmtIsDeclare = 1;` |
|        21 | 14827 | `			}` |
|   3712573 | 14828 | `		}` |
|  12759147 | 14829 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14830 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14831 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    959675 | 14832 | `			pGen->bStrictTypesLocked = 1;` |
|    479835 | 14833 | `		}` |
|  12759147 | 14834 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14835 | `			/* Compile block */` |
|      3865 | 14836 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3865 | 14837 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14838 | `				break;` |
|         - | 14839 | `			}` |
|      1935 | 14840 | `		}else{` |
|  12755287 | 14841 | `			xCons = 0;` |
|  12755287 | 14842 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14843 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14844 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14845 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34513 | 14846 | `				xCons = PH7_CompileClassModifiers;` |
|  12738033 | 14847 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14848 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14849 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3859 | 14850 | `				xCons = PH7_CompileEnum;` |
|  12718852 | 14851 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7390669 | 14852 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14853 | `				/* Try to extract a language construct handler */` |
|   7390669 | 14854 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   7390669 | 14855 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14856 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14857 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14858 | `						&pGen->pIn->sData);` |
|         9 | 14859 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14860 | `						break;` |
|         - | 14861 | `					}` |
|         - | 14862 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14863 | `					 * this erroneous statement.` |
|         - | 14864 | `					 */` |
|         9 | 14865 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14866 | `				}` |
|   9021593 | 14867 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    406673 | 14868 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14869 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14870 | `				xCons = PH7_CompileLabel;` |
|        56 | 14871 | `			}` |
|  12755287 | 14872 | `			if( xCons == 0 ){` |
|         - | 14873 | `				/* Assume an expression an try to compile it */` |
|   5364867 | 14874 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5364867 | 14875 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14876 | `					/* Pop l-value */` |
|   5364717 | 14877 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2682356 | 14878 | `				}` |
|   2682436 | 14879 | `			}else{` |
|         - | 14880 | `				/* Go compile the sucker */` |
|   7390425 | 14881 | `				rc = xCons(&(*pGen));` |
|         - | 14882 | `			}` |
|  12755287 | 14883 | `			if( rc == SXERR_ABORT ){` |
|         - | 14884 | `				/* Request to abort compilation */` |
|        12 | 14885 | `				break;` |
|         - | 14886 | `			}` |
|         - | 14887 | `		}` |
|         - | 14888 | `		/* Ignore trailing semi-colons ';' */` |
|  21842463 | 14889 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   9083331 | 14890 | `			pGen->pIn++;` |
|         5 | 14891 | `		}` |
|  12759137 | 14892 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14893 | `			/* Compile a single statement and return */` |
|  11799435 | 14894 | `			break;` |
|         - | 14895 | `		}` |
|         - | 14896 | `		/* LOOP ONE */` |
|         - | 14897 | `		/* LOOP TWO */` |
|         - | 14898 | `		/* LOOP THREE */` |
|         - | 14899 | `		/* LOOP FOUR */` |
|         5 | 14900 | `	}` |
|         - | 14901 | `	/* Return compilation status */` |
|  11867131 | 14902 | `	return rc;` |
|         5 | 14903 | `}` |
|         - | 14904 | `/*` |
|         - | 14905 | ` * Compile a Raw PHP chunk.` |
|         - | 14906 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14907 | ` * takes care of generating the appropriate error message.` |
|         - | 14908 | ` */` |
|     67698 | 14909 | `static sxi32 PH7_CompilePHP(` |
|         - | 14910 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14911 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14912 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14913 | `	)` |
|         5 | 14914 | `{` |
|     67703 | 14915 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14916 | `	sxi32 rc;` |
|         - | 14917 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67703 | 14918 | `	SySetReset(&(*pTokenSet));` |
|     67703 | 14919 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14920 | `	/* Mark as the default token set */` |
|     67703 | 14921 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14922 | `	/* Advance the stream cursor */` |
|     67703 | 14923 | `	pGen->pRawIn++;` |
|         - | 14924 | `	/* Tokenize the PHP chunk first */` |
|     67703 | 14925 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14926 | `	/* Point to the head and tail of the token stream. */` |
|     67703 | 14927 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67703 | 14928 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67703 | 14929 | `	if( is_expr ){` |
|       ! 0 | 14930 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14931 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14932 | `			/* A simple expression,compile it */` |
|       ! 0 | 14933 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14934 | `		}` |
|         - | 14935 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14936 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14937 | `		return SXRET_OK;` |
|         - | 14938 | `	}` |
|     67703 | 14939 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14940 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14941 | `		/*` |
|         - | 14942 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14943 | `		 * According to the PHP reference manual:` |
|         - | 14944 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14945 | `		 *  immediately follow` |
|         - | 14946 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14947 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14948 | `		 * Symisc extension:` |
|         - | 14949 | `		 *   This short syntax works with all PHP opening` |
|         - | 14950 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14951 | `		 *   only short tag.` |
|         - | 14952 | `		 */` |
|         - | 14953 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14954 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14955 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14956 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14957 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14958 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14959 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14960 | `		}` |
|         3 | 14961 | `		return SXRET_OK;` |
|         - | 14962 | `	}` |
|         - | 14963 | `	/* Compile the PHP chunk */` |
|     67701 | 14964 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14965 | `	/* Fix exceptions jumps */` |
|     67701 | 14966 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14967 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67701 | 14968 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14969 | `		rc = SXERR_ABORT;` |
|         1 | 14970 | `	}` |
|         - | 14971 | `	/* Reset container */` |
|     67701 | 14972 | `	SySetReset(&pGen->aGoto);` |
|     67701 | 14973 | `	SySetReset(&pGen->aLabel);` |
|     67701 | 14974 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14975 | `	/* Compilation result */` |
|     67701 | 14976 | `	return rc;` |
|     33854 | 14977 | `}` |
|         - | 14978 | `/*` |
|         - | 14979 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14980 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14981 | ` * This is the only compile interface exported from this file.` |
|         - | 14982 | ` */` |
|     70922 | 14983 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14984 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14985 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14986 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14987 | `	)` |
|         5 | 14988 | `{` |
|         - | 14989 | `	SySet aPhpToken,aRawToken;` |
|         - | 14990 | `	ph7_gen_state *pCodeGen;` |
|         - | 14991 | `	ph7_value *pRawObj;` |
|         - | 14992 | `	sxu32 nObjIdx;` |
|         - | 14993 | `	sxi32 nRawObj;` |
|         - | 14994 | `	int is_expr;` |
|         - | 14995 | `	sxi8 bSavedStrict;` |
|         - | 14996 | `	sxi8 bSavedStrictLocked;` |
|         - | 14997 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 14998 | `	sxi32 rc;` |
|     70927 | 14999 | `	sxu32 nBaseLine = 1;` |
|     70927 | 15000 | `	if( pScript->nByte < 1 ){` |
|         - | 15001 | `		/* Nothing to compile */` |
|       ! 0 | 15002 | `		return PH7_OK;` |
|         - | 15003 | `	}` |
|         - | 15004 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 15005 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 15006 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     70927 | 15007 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 15008 | `		const char *z = pScript->zString;` |
|         3 | 15009 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 15010 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 15011 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 15012 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 15013 | `		pScript->zString = z;` |
|         3 | 15014 | `		nBaseLine = 2;` |
|         3 | 15015 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 15016 | `			return PH7_OK;` |
|         - | 15017 | `		}` |
|         1 | 15018 | `	}` |
|         - | 15019 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 15020 | `	 * file's flags so include/require restore them on return. */` |
|     70927 | 15021 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 15022 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 15023 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 15024 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 15025 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 15026 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 15027 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     70927 | 15028 | `	pSavedIn = pCodeGen->pIn;` |
|     70927 | 15029 | `	pSavedEnd = pCodeGen->pEnd;` |
|     70927 | 15030 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     70927 | 15031 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     70927 | 15032 | `	pCodeGen->bStrictTypes = 0;` |
|     70927 | 15033 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 15034 | `	/* Initialize the tokens containers */` |
|     70927 | 15035 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70927 | 15036 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70927 | 15037 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     70927 | 15038 | `	is_expr = 0;` |
|     70927 | 15039 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 15040 | `		SyToken sTmp;` |
|         - | 15041 | `		/* PHP only: -*/` |
|     57485 | 15042 | `		sTmp.nLine = 1;` |
|     57485 | 15043 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57485 | 15044 | `		sTmp.pUserData = 0;` |
|     57485 | 15045 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57485 | 15046 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57485 | 15047 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 15048 | `			/* A simple PHP expression */` |
|       ! 0 | 15049 | `			is_expr = 1;` |
|       ! 0 | 15050 | `		}` |
|     28745 | 15051 | `	}else{` |
|         - | 15052 | `		/* Tokenize raw text */` |
|     13447 | 15053 | `		SySetAlloc(&aRawToken,32);` |
|     13447 | 15054 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 15055 | `	}` |
|         - | 15056 | `	/* Process high-level tokens */` |
|     70927 | 15057 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     70927 | 15058 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     70927 | 15059 | `	rc = PH7_OK;` |
|     70927 | 15060 | `	if( is_expr ){` |
|         - | 15061 | `		/* Compile the expression */` |
|       ! 0 | 15062 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 15063 | `		goto cleanup;` |
|         - | 15064 | `	}` |
|     70927 | 15065 | `	nObjIdx = 0;` |
|         - | 15066 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 15067 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 15068 | `	 * preventing namespace bleeding across include()d files. */` |
|     70927 | 15069 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 15070 | `	/* Start the compilation process */` |
|     42187 | 15071 | `	for(;;){` |
|    152065 | 15072 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     70915 | 15073 | `			break; /* No more tokens to process */` |
|         - | 15074 | `		}` |
|     81155 | 15075 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 15076 | `			/* Compile the PHP chunk */` |
|     67703 | 15077 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67703 | 15078 | `			if( rc == SXERR_ABORT ){` |
|        15 | 15079 | `				break;` |
|         - | 15080 | `			}` |
|     67691 | 15081 | `			continue;` |
|         - | 15082 | `		}` |
|         - | 15083 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13457 | 15084 | `		nRawObj = 0;` |
|     26909 | 15085 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 15086 | `			/* Consume the raw chunk without any processing */` |
|     13457 | 15087 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13457 | 15088 | `			if( pRawObj == 0 ){` |
|       ! 0 | 15089 | `				rc = SXERR_MEM;` |
|       ! 0 | 15090 | `				break;` |
|         - | 15091 | `			}` |
|         - | 15092 | `			/* Mark as constant and emit the load constant instruction */` |
|     13457 | 15093 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13457 | 15094 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13457 | 15095 | `			++nRawObj;` |
|     13457 | 15096 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 15097 | `		}` |
|     13457 | 15098 | `		if( nRawObj > 0 ){` |
|         - | 15099 | `			/* Emit the consume instruction */` |
|     13457 | 15100 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6726 | 15101 | `		}` |
|     35466 | 15102 | `	}` |
|     35461 | 15103 | `cleanup:` |
|         - | 15104 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     70927 | 15105 | `	pCodeGen->pIn = pSavedIn;` |
|     70927 | 15106 | `	pCodeGen->pEnd = pSavedEnd;` |
|     70927 | 15107 | `	SySetRelease(&aRawToken);` |
|     70927 | 15108 | `	SySetRelease(&aPhpToken);` |
|         - | 15109 | `	/* Restore outer file's strict_types scope */` |
|     70927 | 15110 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     70927 | 15111 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     70927 | 15112 | `	return rc;` |
|     35466 | 15113 | `}` |
|         - | 15114 | `/*` |
|         - | 15115 | ` * Utility routines.Initialize the code generator.` |
|         - | 15116 | ` */` |
|      3824 | 15117 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 15118 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15119 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15120 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15121 | `	)` |
|         5 | 15122 | `{` |
|      3829 | 15123 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15124 | `	/* Zero the structure */` |
|      3829 | 15125 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 15126 | `	/* Initial state */` |
|      3829 | 15127 | `	pGen->pVm  = &(*pVm);` |
|      3829 | 15128 | `	pGen->xErr = xErr;` |
|      3829 | 15129 | `	pGen->pErrData = pErrData;` |
|      3829 | 15130 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3829 | 15131 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3829 | 15132 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3829 | 15133 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3829 | 15134 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3829 | 15135 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3829 | 15136 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3829 | 15137 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3829 | 15138 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 15139 | `	/* Error log buffer */` |
|      3829 | 15140 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 15141 | `	/* General purpose working buffer */` |
|      3829 | 15142 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 15143 | `	/* Namespace state */` |
|      3829 | 15144 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3829 | 15145 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3829 | 15146 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3829 | 15147 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15148 | `	/* Create the global scope */` |
|      3829 | 15149 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 15150 | `	/* Point to the global scope */` |
|      3829 | 15151 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3829 | 15152 | `	return SXRET_OK;` |
|         5 | 15153 | `}` |
|         - | 15154 | `/*` |
|         - | 15155 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 15156 | ` */` |
|     74286 | 15157 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 15158 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15159 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15160 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15161 | `	)` |
|         5 | 15162 | `{` |
|     74291 | 15163 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15164 | `	GenBlock *pBlock,*pParent;` |
|         - | 15165 | `	/* Reset state */` |
|     74291 | 15166 | `	SySetReset(&pGen->aLabel);` |
|     74291 | 15167 | `	SySetReset(&pGen->aGoto);` |
|     74291 | 15168 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     74291 | 15169 | `	SySetReset(&pGen->aTrivia);` |
|     74291 | 15170 | `	SySetReset(&pGen->aPendingAttrs);` |
|     74291 | 15171 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     74291 | 15172 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     74291 | 15173 | `	SyBlobRelease(&pGen->sWorker);` |
|     74291 | 15174 | `	SyBlobRelease(&pGen->sNamespace);` |
|     74291 | 15175 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     74291 | 15176 | `	SyHashRelease(&pGen->hUseImports);` |
|     74291 | 15177 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     74291 | 15178 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     74291 | 15179 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     74291 | 15180 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     74291 | 15181 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15182 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 15183 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 15184 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 15185 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 15186 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 15187 | `	 * number of unique names, which is acceptable. */` |
|         - | 15188 | `	/* Point to the global scope */` |
|     74291 | 15189 | `	pBlock = pGen->pCurrent;` |
|     74291 | 15190 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 15191 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15192 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15193 | `		pBlock = pParent;` |
|       ! 0 | 15194 | `	}` |
|     74291 | 15195 | `	pGen->xErr = xErr;` |
|     74291 | 15196 | `	pGen->pErrData = pErrData;` |
|     74291 | 15197 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     74291 | 15198 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     74291 | 15199 | `	pGen->pIn = pGen->pEnd = 0;` |
|     74291 | 15200 | `	pGen->nErr = 0;` |
|     74291 | 15201 | `	return SXRET_OK;` |
|         5 | 15202 | `}` |
|         - | 15203 | `/*` |
|         - | 15204 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 15205 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 15206 | ` *` |
|         - | 15207 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 15208 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 15209 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 15210 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 15211 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 15212 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 15213 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 15214 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 15215 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 15216 | ` *` |
|         - | 15217 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 15218 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 15219 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 15220 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 15221 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 15222 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 15223 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 15224 | ` */` |
|         4 | 15225 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 15226 | `{` |
|         5 | 15227 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15228 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 15229 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 15230 | `	*pSaved = *pGen;` |
|         5 | 15231 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 15232 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 15233 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15234 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15235 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15236 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15237 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 15238 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 15239 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 15240 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 15241 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 15242 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15243 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 15244 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 15245 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 15246 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 15247 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 15248 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 15249 | `	pGen->pTokenSet = 0;` |
|         5 | 15250 | `	pGen->nErr = 0;` |
|         5 | 15251 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 15252 | `	pGen->nCommaExprOk = 0;` |
|         5 | 15253 | `	pGen->bInGenerator = 0;` |
|         5 | 15254 | `	pGen->bStrictTypes = 0;` |
|         5 | 15255 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 15256 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 15257 | `	pGen->xErr = xErr;` |
|         5 | 15258 | `	pGen->pErrData = pErrData;` |
|         5 | 15259 | `}` |
|         - | 15260 | `/*` |
|         - | 15261 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 15262 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 15263 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 15264 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 15265 | ` */` |
|         4 | 15266 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 15267 | `{` |
|         5 | 15268 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15269 | `	GenBlock *pBlock,*pParent;` |
|         - | 15270 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 15271 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 15272 | `	 * nested global block's own fixup sets. */` |
|         5 | 15273 | `	pBlock = pGen->pCurrent;` |
|         5 | 15274 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 15275 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15276 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15277 | `		pBlock = pParent;` |
|       ! 0 | 15278 | `	}` |
|         5 | 15279 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 15280 | `	/* Release the nested unit's position containers. */` |
|         5 | 15281 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 15282 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 15283 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 15284 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 15285 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 15286 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 15287 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 15288 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 15289 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 15290 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 15291 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 15292 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 15293 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 15294 | `	hVar = pGen->hVar;` |
|         5 | 15295 | `	hLiteral = pGen->hLiteral;` |
|         5 | 15296 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 15297 | `	*pGen = *pSaved;` |
|         5 | 15298 | `	pGen->hVar = hVar;` |
|         5 | 15299 | `	pGen->hLiteral = hLiteral;` |
|         5 | 15300 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 15301 | `}` |
|         - | 15302 | `/*` |
|         - | 15303 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 15304 | ` * php's parser prints, e.g.` |
|         - | 15305 | ` *` |
|         - | 15306 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 15307 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 15308 | ` *   syntax error, unexpected end of file` |
|         - | 15309 | ` *` |
|         - | 15310 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 15311 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 15312 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 15313 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 15314 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 15315 | ` *` |
|         - | 15316 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 15317 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 15318 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 15319 | ` */` |
|       182 | 15320 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 15321 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 15322 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 15323 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 15324 | `	)` |
|         5 | 15325 | `{` |
|       187 | 15326 | `	const char *zNoun = "token";` |
|         - | 15327 | `	sxu32 nLine;` |
|       187 | 15328 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 15329 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 15330 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 15331 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 15332 | `		 * it before concluding "end of file". */` |
|        92 | 15333 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        92 | 15334 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        92 | 15335 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 15336 | `			pTok = pGen->pEnd;` |
|        44 | 15337 | `		}` |
|        44 | 15338 | `	}` |
|       187 | 15339 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       187 | 15340 | `	if( pTok == 0 ){` |
|       ! 0 | 15341 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 15342 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 15343 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 15344 | `			zExpecting);` |
|         - | 15345 | `	}` |
|       187 | 15346 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 15347 | `		zNoun = "identifier";` |
|       180 | 15348 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         8 | 15349 | `		zNoun = "variable";` |
|       171 | 15350 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        24 | 15351 | `		zNoun = "integer";` |
|       158 | 15352 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 15353 | `		zNoun = "float";` |
|       ! 0 | 15354 | `	}` |
|       187 | 15355 | `	if( zExpecting ){` |
|       118 | 15356 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 15357 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 15358 | `	}` |
|       164 | 15359 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 15360 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        96 | 15361 | `}` |
|         - | 15362 | `/*` |
|         - | 15363 | ` * Generate a compile-time error message.` |
|         - | 15364 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 15365 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 15366 | ` * abort compilation immediately.` |
|         - | 15367 | ` */` |
|     15972 | 15368 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 15369 | `{` |
|     15977 | 15370 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     15977 | 15371 | `	const char *zErr = "Error";` |
|         - | 15372 | `	SyString *pFile;` |
|         - | 15373 | `	va_list ap;` |
|         - | 15374 | `	sxi32 rc;` |
|         - | 15375 | `	/* Reset the working buffer */` |
|     15977 | 15376 | `	SyBlobReset(pWorker);` |
|         - | 15377 | `	/* Peek the processed file path if available */` |
|     15977 | 15378 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     15977 | 15379 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 15380 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 15381 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 15382 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 15383 | `		 * into execution with a 0 exit status. */` |
|       659 | 15384 | `		pGen->nErr++;` |
|       659 | 15385 | `		if( pGen->nErr > 15 ){` |
|         - | 15386 | `			/* Error count limit reached */` |
|         6 | 15387 | `			if( pGen->xErr ){` |
|         6 | 15388 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 15389 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 15390 | `				if( pFile ){` |
|         6 | 15391 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 15392 | `				}` |
|         6 | 15393 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 15394 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 15395 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 15396 | `				}` |
|         2 | 15397 | `			}` |
|         - | 15398 | `			/* Abort immediately */` |
|         6 | 15399 | `			return SXERR_ABORT;` |
|         - | 15400 | `		}` |
|       325 | 15401 | `	}` |
|     15973 | 15402 | `	if( pGen->xErr == 0 ){` |
|         - | 15403 | `		/* No available error consumer,return immediately */` |
|     15303 | 15404 | `		return SXRET_OK;` |
|         - | 15405 | `	}` |
|       675 | 15406 | `	switch(nErrType){` |
|       310 | 15407 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|         8 | 15408 | `	case E_WARNING: zErr = "Warning";     break;` |
|       346 | 15409 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 15410 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 15411 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 15412 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 15413 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        16 | 15414 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 15415 | `	default:` |
|       ! 0 | 15416 | `		break;` |
|         - | 15417 | `	}` |
|       675 | 15418 | `	rc = SXRET_OK;` |
|         - | 15419 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       675 | 15420 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       675 | 15421 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       675 | 15422 | `	va_start(ap,zFormat);` |
|       675 | 15423 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       675 | 15424 | `	va_end(ap);` |
|       675 | 15425 | `	if( pFile ){` |
|       675 | 15426 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       335 | 15427 | `	}` |
|         - | 15428 | `	/* Append a new line */` |
|       675 | 15429 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       675 | 15430 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 15431 | `		/* Consume the generated error message */` |
|       675 | 15432 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       335 | 15433 | `	}` |
|       675 | 15434 | `	return rc;` |
|      7991 | 15435 | `}` |
|         - | 15436 |  |
