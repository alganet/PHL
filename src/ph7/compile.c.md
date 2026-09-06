# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7085/8780 lines (80.69%)

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
|       276 |   122 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   123 | `			/* Jump destination found */` |
|        96 |   124 | `			aLabel[n].bRef = TRUE;` |
|        96 |   125 | `			if( ppOut ){` |
|        96 |   126 | `				*ppOut = &aLabel[n];` |
|        46 |   127 | `			}` |
|        96 |   128 | `			return SXRET_OK;` |
|         - |   129 | `		}` |
|        93 |   130 | `	}` |
|         - |   131 | `	/* No such destination */` |
|        60 |   132 | `	return SXERR_NOTFOUND;` |
|        79 |   133 | `}` |
|         - |   134 | `/*` |
|         - |   135 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   136 | ` * compiled blocks.` |
|         - |   137 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   138 | ` */` |
|    115380 |   139 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   140 | `{` |
|    115385 |   141 | `	GenBlock *pBlock = pCurrent;` |
|    269019 |   142 | `	for(;;){` |
|    538043 |   143 | `		if( pBlock->iFlags & iBlockType ){` |
|    115385 |   144 | `			iCount--; /* Decrement nesting level */` |
|    115385 |   145 | `			if( iCount < 1 ){` |
|         - |   146 | `				/* Block meet with the desired criteria */` |
|    115359 |   147 | `				return pBlock;` |
|         - |   148 | `			}` |
|        13 |   149 | `		}` |
|         - |   150 | `		/* Point to the upper block */` |
|    422689 |   151 | `		pBlock = pBlock->pParent;` |
|    422689 |   152 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   153 | `			/* Forbidden */` |
|        17 |   154 | `			break;` |
|         - |   155 | `		}` |
|         5 |   156 | `	}` |
|         - |   157 | `	/* No such block */` |
|        30 |   158 | `	return 0;` |
|     57695 |   159 | `}` |
|         - |   160 | `/*` |
|         - |   161 | ` * Initialize a freshly allocated block instance.` |
|         - |   162 | ` */` |
|  10085228 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|  10085233 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  10085233 |   173 | `	pBlock->pUserData   = pUserData;` |
|  10085233 |   174 | `	pBlock->pGen        = pGen;` |
|  10085233 |   175 | `	pBlock->iFlags      = iType;` |
|  10085233 |   176 | `	pBlock->pParent     = 0;` |
|  10085233 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  10085233 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  10085233 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|  10081388 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|  10081393 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  10081393 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|  10081393 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  10081393 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|  10081393 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  10081393 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    365835 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    365835 |   214 | `		pGen->nLoopId++;` |
|    365835 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    365835 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    365835 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    365835 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    182915 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|  10081393 |   221 | `	pGen->pCurrent = pBlock;` |
|  10081393 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   4853065 |   224 | `		*ppBlock = pBlock;` |
|   2426530 |   225 | `	}` |
|  10081393 |   226 | `	return SXRET_OK;` |
|   5040699 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|  10081372 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|  10081377 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|  10081377 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|  10081377 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|  10081372 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|  10081377 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  10081377 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|  10081377 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  10081377 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|  10081372 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|  10081377 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  10081377 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|  10081377 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    365827 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    182911 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|  10081377 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|  10081377 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|  10081377 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|  10081377 |   268 | `	return SXRET_OK;` |
|   5040691 |   269 | `}` |
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
|   3604772 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   3604777 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   3604777 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   3604777 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   3604777 |   289 | `	return rc;` |
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
|   7012314 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   7012319 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  14807391 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   7795077 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   2956821 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   4838261 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1233491 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   3604775 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   3604775 |   322 | `		if( pInstr ){` |
|   3604775 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   3604775 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   3604775 |   326 | `			aFix[n].nJumpType = -1;` |
|   1802385 |   327 | `		}` |
|   1802390 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   7012319 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2628948 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2628953 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2629099 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|        96 |   365 | `		if( pLabel->nLoopId != 0 ){` |
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
|        96 |   386 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|        11 |   387 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        11 |   388 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |   389 | `				return SXERR_ABORT;` |
|         - |   390 | `			}` |
|         4 |   391 | `		}` |
|         - |   392 | `		/* Fix the jump now the destination is resolved */` |
|        96 |   393 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|        96 |   394 | `		if( pInstr ){` |
|        96 |   395 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|        46 |   396 | `		}` |
|        50 |   397 | `	}` |
|         - |   398 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|         - |   399 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|   2628951 |   400 | `	return SXRET_OK;` |
|   1314479 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  13268190 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  13268195 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  13268195 |   409 | `	if( pEntry == 0 ){` |
|   3474337 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|   9793863 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   9793863 |   413 | `	return SXRET_OK;` |
|   6634100 |   414 | `}` |
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
|   3474332 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3474337 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3474337 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1737166 |   429 | `	}` |
|   3474337 |   430 | `	return SXRET_OK;` |
|         5 |   431 | `}` |
|         - |   432 | `/*` |
|         - |   433 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   434 | ` * in the constant table.` |
|         - |   435 | ` */` |
|   2725974 |   436 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   437 | `{` |
|         - |   438 | `	ph7_value *pObj;` |
|   2725979 |   439 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   440 | `	/* Reserve a new constant */` |
|   2725979 |   441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   2725979 |   442 | `	if( pObj == 0 ){` |
|       ! 0 |   443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   444 | `		return 0;` |
|         - |   445 | `	}` |
|   2725979 |   446 | `	*pIdx = nIdx;` |
|         - |   447 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   448 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   449 | `	 */` |
|   2725979 |   450 | `	return pObj;` |
|   1362992 |   451 | `}` |
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
|   6198618 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6198623 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3099314 |   478 | `}` |
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
|   2726966 |   542 | `static int GenStateFindBadNumericSeparator(` |
|         - |   543 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   544 | `{` |
|   2726971 |   545 | `	const char *z = pRaw->zString;` |
|   2726971 |   546 | `	sxu32 n = pRaw->nByte;` |
|   2726971 |   547 | `	int base = 10;` |
|         - |   548 | `	sxu32 i, start;` |
|   2726971 |   549 | `	if( n < 2 ) return 0;` |
|    445877 |   550 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        80 |   551 | `		base = 16;` |
|    445838 |   552 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       284 |   553 | `		base = 2;` |
|       141 |   554 | `	}` |
|   1492455 |   555 | `	for( i = 0; i < n; ++i ){` |
|   1046597 |   556 | `		if( z[i] != '_' ) continue;` |
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
|    445863 |   573 | `	return 0;` |
|   1363488 |   574 | `}` |
|         - |   575 | `/*` |
|         - |   576 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   577 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   578 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   579 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   580 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   581 | ` * so callers can bail from the current construct).` |
|         - |   582 | ` */` |
|   2726966 |   583 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   584 | `{` |
|   2726971 |   585 | `	const char *zBad = 0;` |
|   2726971 |   586 | `	sxu32 nBad = 0;` |
|         - |   587 | `	SyString sBad;` |
|         - |   588 | `	sxi32 rc;` |
|   2726971 |   589 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   2726957 |   590 | `		return SXRET_OK;` |
|         - |   591 | `	}` |
|        18 |   592 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   593 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   594 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   595 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   596 | `		return SXERR_ABORT;` |
|         - |   597 | `	}` |
|        18 |   598 | `	return SXERR_SYNTAX;` |
|   1363488 |   599 | `}` |
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
|   2726952 |   616 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   617 | `	SyMemBackend *pAlloc,` |
|         - |   618 | `	const SyString *pToken,` |
|         - |   619 | `	char *zScratch, sxu32 nScratch,` |
|         - |   620 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   621 | `{` |
|         - |   622 | `	sxu32 i, j;` |
|   2726957 |   623 | `	int hasUnderscore = 0;` |
|         - |   624 | `	char *zBuf;` |
|   2726957 |   625 | `	*pzAlloc = 0;` |
|   6052563 |   626 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   3325863 |   627 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   1662808 |   628 | `	}` |
|   2726957 |   629 | `	if( !hasUnderscore ){` |
|   2726705 |   630 | `		SyStringDupPtr(pOut, pToken);` |
|   2726705 |   631 | `		return SXRET_OK;` |
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
|   1363481 |   648 | `}` |
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
|   2726008 |   684 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   685 | `{` |
|   2726013 |   686 | `	const char *z = pNum->zString;` |
|   2726013 |   687 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   688 | `	const char *p, *q;` |
|         - |   689 | `	int n;` |
|   2726013 |   690 | `	*pbDecimal = FALSE;` |
|   2726013 |   691 | `	if( z >= zEnd ){` |
|       ! 0 |   692 | `		return FALSE;` |
|         - |   693 | `	}` |
|   2726013 |   694 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
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
|   2725937 |   709 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|   2725657 |   724 | `	}else if( z[0] == '0' ){` |
|         - |   725 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   726 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   727 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1031403 |   728 | `		p = z;` |
|   2062803 |   729 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1031631 |   730 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1031403 |   731 | `		if( n <= 21 ){` |
|   1031401 |   732 | `			return FALSE;` |
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
|   1694259 |   745 | `	p = z;` |
|   1694259 |   746 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   3981995 |   747 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   1694259 |   748 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   749 | `		*pbDecimal = TRUE;` |
|        25 |   750 | `		return TRUE;` |
|         - |   751 | `	}` |
|   1694235 |   752 | `	return FALSE;` |
|   1363009 |   753 | `}` |
|   2726938 |   754 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   755 | `{` |
|   2726943 |   756 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   2726943 |   757 | `	sxu32 nIdx = 0;` |
|         - |   758 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   2726943 |   759 | `	char *zAlloc = 0;` |
|         - |   760 | `	SyString sNum;` |
|         - |   761 | `	sxi32 rc;` |
|   1363469 |   762 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   2726943 |   763 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   2726943 |   764 | `	if( rc != SXRET_OK ){` |
|        14 |   765 | `		return rc;` |
|         - |   766 | `	}` |
|   4090397 |   767 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1363464 |   768 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   2726933 |   769 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   770 | `		return SXERR_ABORT;` |
|         - |   771 | `	}` |
|   2726933 |   772 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   773 | `		ph7_value *pObj;` |
|         - |   774 | `		sxi64 iValue;` |
|   2726013 |   775 | `		ph7_real rOverflow = 0;` |
|   2726013 |   776 | `		int bDecimalOverflow = 0;` |
|   2726013 |   777 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|   2725979 |   794 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   2725979 |   795 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   2725979 |   796 | `			if( pObj == 0 ){` |
|       ! 0 |   797 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   798 | `				return SXERR_ABORT;` |
|         - |   799 | `			}` |
|   2725979 |   800 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   801 | `		}` |
|   1363009 |   802 | `	}else{` |
|         - |   803 | `		/* Real number */` |
|         - |   804 | `		ph7_value *pObj;` |
|         - |   805 | `		/* Reserve a new constant */` |
|       924 |   806 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       924 |   807 | `		if( pObj == 0 ){` |
|       ! 0 |   808 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   809 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   810 | `			return SXERR_ABORT;` |
|         - |   811 | `		}` |
|       924 |   812 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       924 |   813 | `		PH7_MemObjToReal(pObj);` |
|         - |   814 | `	}` |
|   2726933 |   815 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   816 | `	/* Emit the load constant instruction */` |
|   2726933 |   817 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   818 | `	/* Node successfully compiled */` |
|   2726933 |   819 | `	return SXRET_OK;` |
|   1363474 |   820 | `}` |
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
|   4332670 |   832 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   833 | `{` |
|   4332675 |   834 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   835 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   836 | `	ph7_value *pObj;` |
|         - |   837 | `	sxu32 nIdx;` |
|   4332675 |   838 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   839 | `	/* Delimit the string */` |
|   4332675 |   840 | `	zIn  = pStr->zString;` |
|   4332675 |   841 | `	zEnd = &zIn[pStr->nByte];` |
|   4332675 |   842 | `	if( zIn >= zEnd ){` |
|         - |   843 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   844 | `		 * rather than reserving a new object each time. */` |
|    203733 |   845 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    203733 |   846 | `		return SXRET_OK;` |
|         - |   847 | `	}` |
|   4128947 |   848 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   849 | `		/* Already processed,emit the load constant instruction` |
|         - |   850 | `		 * and return.` |
|         - |   851 | `		 */` |
|   2408289 |   852 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2408289 |   853 | `		return SXRET_OK;` |
|         - |   854 | `	}` |
|         - |   855 | `	/* Reserve a new constant */` |
|   1720663 |   856 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1720663 |   857 | `	if( pObj == 0 ){` |
|       ! 0 |   858 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   859 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   860 | `		return SXERR_ABORT;` |
|         - |   861 | `	}` |
|   1720663 |   862 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   863 | `	/* Compile the node */` |
|   1762958 |   864 | `	for(;;){` |
|   3525921 |   865 | `		if( zIn >= zEnd ){` |
|         - |   866 | `			/* End of input */` |
|   1720663 |   867 | `			break;` |
|         - |   868 | `		}` |
|   1805263 |   869 | `		zCur = zIn;` |
|  36986897 |   870 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  35181639 |   871 | `			zIn++;` |
|         5 |   872 | `		}` |
|   1805263 |   873 | `		if( zIn > zCur ){` |
|         - |   874 | `			/* Append raw contents*/` |
|   1766833 |   875 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    883414 |   876 | `		}` |
|   1805263 |   877 | `		zIn++;` |
|   1805263 |   878 | `		if( zIn < zEnd ){` |
|    119191 |   879 | `			if( zIn[0] == '\\' ){` |
|         - |   880 | `				/* A literal backslash */` |
|     30759 |   881 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    103814 |   882 | `			}else if( zIn[0] == '\'' ){` |
|         - |   883 | `				/* A single quote */` |
|        11 |   884 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   885 | `			}else{` |
|         - |   886 | `				/* verbatim copy */` |
|     88427 |   887 | `				zIn--;` |
|     88427 |   888 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     88427 |   889 | `				zIn++;` |
|         - |   890 | `			}` |
|     59593 |   891 | `		}` |
|         - |   892 | `		/* Advance the stream cursor */` |
|   1805263 |   893 | `		zIn++;` |
|         5 |   894 | `	}` |
|         - |   895 | `	/* Emit the load constant instruction */` |
|   1720663 |   896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1720663 |   897 | `	if( pStr->nByte < 1024 ){` |
|         - |   898 | `		/* Install in the literal table */` |
|   1720663 |   899 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    860329 |   900 | `	}` |
|         - |   901 | `	/* Node successfully compiled */` |
|   1720663 |   902 | `	return SXRET_OK;` |
|   2166340 |   903 | `}` |
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
|     82388 |  1103 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1104 | `{` |
|         - |  1105 | `	ph7_value *pConstObj;` |
|     82393 |  1106 | `	sxu32 nIdx = 0;` |
|         - |  1107 | `	/* Reserve a new constant */` |
|     82393 |  1108 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     82393 |  1109 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1110 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1111 | `		return 0;` |
|         - |  1112 | `	}` |
|     82393 |  1113 | `	(*pCount)++;` |
|     82393 |  1114 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1115 | `	/* Emit the load constant instruction */` |
|     82393 |  1116 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     82393 |  1117 | `	return pConstObj;` |
|     41199 |  1118 | `}` |
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
|     80828 |  1181 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1182 | `{` |
|     80833 |  1183 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1184 | `	const char *zIn,*zCur,*zEnd;` |
|     80833 |  1185 | `	ph7_value *pObj = 0;` |
|         - |  1186 | `	sxi32 iCons;` |
|         - |  1187 | `	sxi32 rc;` |
|         - |  1188 | `	/* Delimit the string */` |
|     80833 |  1189 | `	zIn  = pStr->zString;` |
|     80833 |  1190 | `	zEnd = &zIn[pStr->nByte];` |
|     80833 |  1191 | `	if( zIn >= zEnd ){` |
|         - |  1192 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1193 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1194 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1195 | `		 */` |
|       381 |  1196 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       381 |  1197 | `		return SXRET_OK;` |
|         - |  1198 | `	}` |
|     80457 |  1199 | `	zCur = 0;` |
|         - |  1200 | `	/* Compile the node */` |
|     80457 |  1201 | `	iCons = 0;` |
|     41510 |  1202 | `	for(;;){` |
|    115411 |  1203 | `		zCur = zIn;` |
|   1550079 |  1204 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1437241 |  1205 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        72 |  1206 | `				break;` |
|   1437108 |  1207 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2440 |  1208 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1220 |  1209 | `					break;` |
|         - |  1210 | `			}` |
|   1434673 |  1211 | `			zIn++;` |
|         5 |  1212 | `		}` |
|    115411 |  1213 | `		if( zIn > zCur ){` |
|     55909 |  1214 | `			if( pObj == 0 ){` |
|     55315 |  1215 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     55315 |  1216 | `				if( pObj == 0 ){` |
|       ! 0 |  1217 | `					return SXERR_ABORT;` |
|         - |  1218 | `				}` |
|     27655 |  1219 | `			}` |
|     55909 |  1220 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     27952 |  1221 | `		}` |
|    115411 |  1222 | `		if( zIn >= zEnd ){` |
|     80455 |  1223 | `			break;` |
|         - |  1224 | `		}` |
|     34961 |  1225 | `		if( zIn[0] == '\\' ){` |
|     32393 |  1226 | `			const char *zPtr = 0;` |
|         - |  1227 | `			sxu32 n;` |
|     32393 |  1228 | `			zIn++;` |
|     32393 |  1229 | `			if( pObj == 0 ){` |
|     27083 |  1230 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27083 |  1231 | `				if( pObj == 0 ){` |
|       ! 0 |  1232 | `					return SXERR_ABORT;` |
|         - |  1233 | `				}` |
|     13539 |  1234 | `			}` |
|     32393 |  1235 | `			if( zIn >= zEnd ){` |
|         - |  1236 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1237 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1238 | `				break;` |
|         - |  1239 | `			}` |
|     32391 |  1240 | `			n = sizeof(char); /* size of conversion */` |
|     32391 |  1241 | `			switch( zIn[0] ){` |
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
|     13680 |  1258 | `			case 'n':` |
|         - |  1259 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     27365 |  1260 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     27365 |  1261 | `				break;` |
|        27 |  1262 | `			case 'r':` |
|         - |  1263 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1264 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1265 | `				break;` |
|      1951 |  1266 | `			case 't':` |
|         - |  1267 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      3907 |  1268 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      3907 |  1269 | `				break;` |
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
|     32391 |  1384 | `			zIn += n;` |
|     32391 |  1385 | `			continue;` |
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
|         9 |  1397 | `					iNest++;` |
|      1415 |  1398 | `				}else if(zIn[0] == '}' ){` |
|         - |  1399 | `					/* Decrement nesting level */` |
|       149 |  1400 | `					iNest--;` |
|       149 |  1401 | `					if( iNest <= 0 ){` |
|       141 |  1402 | `						break;` |
|         - |  1403 | `					}` |
|         4 |  1404 | `				}` |
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
|         - |  1491 | `			/* Process the expression */` |
|      2435 |  1492 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2435 |  1493 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1494 | `				return SXERR_ABORT;` |
|         - |  1495 | `			}` |
|      2435 |  1496 | `			if( rc != SXERR_EMPTY ){` |
|      2433 |  1497 | `				++iCons;` |
|      1214 |  1498 | `			}` |
|         - |  1499 | `		}` |
|         - |  1500 | `		/* Invalidate the previously used constant */` |
|      2573 |  1501 | `		pObj = 0;` |
|         5 |  1502 | `	}/*for(;;)*/` |
|     80457 |  1503 | `	if( iCons > 1 ){` |
|         - |  1504 | `		/* Concatenate all compiled constants */` |
|      1861 |  1505 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       928 |  1506 | `	}` |
|         - |  1507 | `	/* Node successfully compiled */` |
|     80457 |  1508 | `	return SXRET_OK;` |
|     40419 |  1509 | `}` |
|         - |  1510 | `/*` |
|         - |  1511 | ` * Compile a double quoted string.` |
|         - |  1512 | ` *  See the block-comment above for more information.` |
|         - |  1513 | ` */` |
|     80766 |  1514 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1515 | `{` |
|         - |  1516 | `	sxi32 rc;` |
|     80771 |  1517 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     40383 |  1518 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1519 | `	/* Compilation result */` |
|     80771 |  1520 | `	return rc;` |
|         5 |  1521 | `}` |
|         - |  1522 | `/*` |
|         - |  1523 | ` * Compile a Heredoc string.` |
|         - |  1524 | ` *  See the block-comment above for more information.` |
|         - |  1525 | ` */` |
|        66 |  1526 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 |  1527 | `{` |
|         - |  1528 | `	SyString sOrig, sStripped;` |
|         - |  1529 | `	sxi32 rc;` |
|        70 |  1530 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        70 |  1531 | `	if( rc != SXRET_OK ){` |
|         6 |  1532 | `		return rc;` |
|         - |  1533 | `	}` |
|         - |  1534 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|         - |  1535 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|         - |  1536 | `	 * Restore before returning so downstream code that references pIn is` |
|         - |  1537 | `	 * unaffected, including on the error path. */` |
|        65 |  1538 | `	sOrig = pGen->pIn->sData;` |
|        65 |  1539 | `	pGen->pIn->sData = sStripped;` |
|        65 |  1540 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|        65 |  1541 | `	pGen->pIn->sData = sOrig;` |
|        31 |  1542 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        65 |  1543 | `	return rc;` |
|        37 |  1544 | `}` |
|         - |  1545 | `/*` |
|         - |  1546 | ` * Compile an array entry whether it is a key or a value.` |
|         - |  1547 | ` *  Notes on array entries.` |
|         - |  1548 | ` *  According to the PHP language reference manual` |
|         - |  1549 | ` *  An array can be created by the array() language construct.` |
|         - |  1550 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|         - |  1551 | ` *  array(  key =>  value` |
|         - |  1552 | ` *    , ...` |
|         - |  1553 | ` *    )` |
|         - |  1554 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|         - |  1555 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|         - |  1556 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|         - |  1557 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|         - |  1558 | ` *  contain integer and string indices.` |
|         - |  1559 | ` *  A value can be any PHP type.` |
|         - |  1560 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|         - |  1561 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|         - |  1562 | ` *  is specified, that value will be overwritten.` |
|         - |  1563 | ` */` |
|   1000606 |  1564 | `static sxi32 GenStateCompileArrayEntry(` |
|         - |  1565 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1566 | `	SyToken *pIn,        /* Token stream */` |
|         - |  1567 | `	SyToken *pEnd,       /* End of the token stream */` |
|         - |  1568 | `	sxi32 iFlags,        /* Compilation flags */` |
|         - |  1569 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|         - |  1570 | `	)` |
|         5 |  1571 | `{` |
|         - |  1572 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1573 | `	sxi32 rc;` |
|         - |  1574 | `	/* Swap token stream */` |
|   1000611 |  1575 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1576 | `	/* Compile the expression*/` |
|   1000611 |  1577 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1578 | `	/* Restore token stream */` |
|   1000611 |  1579 | `	RE_SWAP_DELIMITER(pGen);` |
|   1000611 |  1580 | `	return rc;` |
|         5 |  1581 | `}` |
|         - |  1582 | `/*` |
|         - |  1583 | ` * Expression tree validator callback for the 'array' language construct.` |
|         - |  1584 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1585 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1586 | ` * error message.` |
|         - |  1587 | ` * See the routine responible of compiling the array language construct` |
|         - |  1588 | ` * for more inforation.` |
|         - |  1589 | ` */` |
|        36 |  1590 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         4 |  1591 | `{` |
|        40 |  1592 | `	sxi32 rc = SXRET_OK;` |
|        40 |  1593 | `	if( pRoot->pOp ){` |
|        14 |  1594 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|        12 |  1595 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|        16 |  1596 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1597 | `			/* Unexpected expression */` |
|        13 |  1598 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|        13 |  1599 | `			if( rc != SXERR_ABORT ){` |
|        13 |  1600 | `				rc = SXERR_INVALID;` |
|         5 |  1601 | `			}` |
|         9 |  1602 | `		}` |
|        31 |  1603 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1604 | `		/* Unexpected expression */` |
|         3 |  1605 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|         3 |  1606 | `		if( rc != SXERR_ABORT ){` |
|         3 |  1607 | `			rc = SXERR_INVALID;` |
|         1 |  1608 | `		}` |
|         1 |  1609 | `	}` |
|        40 |  1610 | `	return rc;` |
|         4 |  1611 | `}` |
|         - |  1612 | `/*` |
|         - |  1613 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|         - |  1614 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|         - |  1615 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|         - |  1616 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|         - |  1617 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|         - |  1618 | ` */` |
|    941768 |  1619 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1620 | `{` |
|    941773 |  1621 | `	SyToken *pCur = pStart;` |
|    941773 |  1622 | `	sxi32 iNest = 0;` |
|   2597217 |  1623 | `	while( pCur < pEnd ){` |
|   2037919 |  1624 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    382471 |  1625 | `			return pCur;` |
|         - |  1626 | `		}` |
|         - |  1627 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1628 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1629 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1630 | `		 */` |
|   1655453 |  1631 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23147 |  1632 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23147 |  1633 | `			SyToken *pFn = pCur;` |
|     23142 |  1634 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1635 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1636 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1637 | `				pFn = &pCur[1];` |
|       ! 0 |  1638 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1639 | `			}` |
|     23147 |  1640 | `			if( nKw == PH7_TKWRD_FN ){` |
|         5 |  1641 | `				pCur = pFn + 1; /* past 'fn' */` |
|         5 |  1642 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  1643 | `					pCur++;` |
|       ! 0 |  1644 | `				}` |
|         5 |  1645 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         5 |  1646 | `					pCur++;` |
|         5 |  1647 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1648 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         5 |  1649 | `					if( pCur < pEnd ){` |
|         5 |  1650 | `						pCur++;` |
|         2 |  1651 | `					}` |
|         2 |  1652 | `				}` |
|         5 |  1653 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|       ! 0 |  1654 | `					pCur++;` |
|       ! 0 |  1655 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|       ! 0 |  1656 | `						&& pCur->sData.nByte == 1` |
|       ! 0 |  1657 | `						&& pCur->sData.zString[0] == '?' ){` |
|       ! 0 |  1658 | `						pCur++;` |
|       ! 0 |  1659 | `					}` |
|       ! 0 |  1660 | `					if( pCur < pEnd` |
|       ! 0 |  1661 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  1662 | `						pCur++;` |
|       ! 0 |  1663 | `					}` |
|       ! 0 |  1664 | `				}` |
|         - |  1665 | `				/* The rest of the entry is the arrow-function body — no outer` |
|         - |  1666 | `				 * key to extract. */` |
|         5 |  1667 | `				return pEnd;` |
|         - |  1668 | `			}` |
|         - |  1669 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|         - |  1670 | `			 * entry separator. Skip past the full match span. */` |
|     23143 |  1671 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|         3 |  1672 | `				pCur++; /* past 'match' */` |
|         3 |  1673 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         3 |  1674 | `					pCur++;` |
|         3 |  1675 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1676 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         3 |  1677 | `					if( pCur < pEnd ){` |
|         3 |  1678 | `						pCur++;` |
|         1 |  1679 | `					}` |
|         1 |  1680 | `				}` |
|         3 |  1681 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|         3 |  1682 | `					pCur++;` |
|         3 |  1683 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1684 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|         3 |  1685 | `					if( pCur < pEnd ){` |
|         3 |  1686 | `						pCur++;` |
|         1 |  1687 | `					}` |
|         1 |  1688 | `				}` |
|         3 |  1689 | `				continue;` |
|         - |  1690 | `			}` |
|     11568 |  1691 | `		}` |
|   1655447 |  1692 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     54215 |  1693 | `			iNest++;` |
|   1628342 |  1694 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1695 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1696 | `			 * parser will shortly detect any syntax error. */` |
|     54215 |  1697 | `			iNest--;` |
|     27105 |  1698 | `		}` |
|   1655447 |  1699 | `		pCur++;` |
|         5 |  1700 | `	}` |
|    559303 |  1701 | `	return pEnd;` |
|    470889 |  1702 | `}` |
|         - |  1703 | `/*` |
|         - |  1704 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1705 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1706 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1707 | ` */` |
|    502972 |  1708 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1709 | `{` |
|         - |  1710 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1711 | `	SyToken *pKey,*pCur;` |
|    502977 |  1712 | `	sxi32 iEmitRef = 0;` |
|    502977 |  1713 | `	sxi32 iSpread = 0;` |
|    502977 |  1714 | `	sxi32 nPair = 0;` |
|         - |  1715 | `	sxi32 rc;` |
|    502977 |  1716 | `	xValidator = 0;` |
|    610516 |  1717 | `	for(;;){` |
|         - |  1718 | `		/* Jump leading commas */` |
|   1720899 |  1719 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    499867 |  1720 | `			pGen->pIn++;` |
|         5 |  1721 | `		}` |
|   1221037 |  1722 | `		pCur = pGen->pIn;` |
|   1221037 |  1723 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1724 | `			/* No more entry to process */` |
|    502961 |  1725 | `			break;` |
|         - |  1726 | `		}` |
|    718081 |  1727 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1728 | `			continue;` |
|         - |  1729 | `		}` |
|         - |  1730 | `		/* Compile the key if available */` |
|    718081 |  1731 | `		pKey = pCur;` |
|    718081 |  1732 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|    718081 |  1733 | `		rc = SXERR_EMPTY;` |
|    718081 |  1734 | `		if( pCur < pGen->pIn ){` |
|    282277 |  1735 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1736 | `				/* Missing value */` |
|        13 |  1737 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,0);` |
|        13 |  1738 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1739 | `					return SXERR_ABORT;` |
|         - |  1740 | `				}` |
|        13 |  1741 | `				return SXRET_OK;` |
|         - |  1742 | `			}` |
|         - |  1743 | `			/* Compile the expression holding the key */` |
|    282267 |  1744 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1745 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    282267 |  1746 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1747 | `				return SXERR_ABORT;` |
|         - |  1748 | `			}` |
|    282267 |  1749 | `			pCur++; /* Jump the '=>' operator */` |
|    576940 |  1750 | `		}else if( pKey == pCur ){` |
|         - |  1751 | `			/* Key is omitted,emit a warning */` |
|       ! 0 |  1752 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|       ! 0 |  1753 | `			pCur++; /* Jump the '=>' operator */` |
|       ! 0 |  1754 | `		}else{` |
|         - |  1755 | `			/* Reset back the cursor and point to the entry value */` |
|    435809 |  1756 | `			pCur = pKey;` |
|         - |  1757 | `		}` |
|    718071 |  1758 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1759 | `			/* No available key,load NULL */` |
|    435811 |  1760 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    217903 |  1761 | `		}` |
|    718071 |  1762 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|         - |  1763 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|        45 |  1764 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|        45 |  1765 | `			iEmitRef = 1;` |
|        45 |  1766 | `			pCur++; /* Jump the '&' token */` |
|        45 |  1767 | `			if( pCur >= pGen->pIn ){` |
|         - |  1768 | `				/* Missing value */` |
|         3 |  1769 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|         3 |  1770 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1771 | `					return SXERR_ABORT;` |
|         - |  1772 | `				}` |
|         3 |  1773 | `				return SXRET_OK;` |
|         - |  1774 | `			}` |
|        19 |  1775 | `		}` |
|         - |  1776 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|         - |  1777 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|         - |  1778 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|         - |  1779 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|         - |  1780 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|    718069 |  1781 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|    718069 |  1782 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|         - |  1783 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|         - |  1784 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|         - |  1785 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|         - |  1786 | `			 * output is engine-portable. */` |
|         6 |  1787 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|         - |  1788 | `				"syntax error, unexpected token \"...\"");` |
|         6 |  1789 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1790 | `				return SXERR_ABORT;` |
|         - |  1791 | `			}` |
|         6 |  1792 | `			return SXRET_OK;` |
|         - |  1793 | `		}` |
|         - |  1794 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|         - |  1795 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|         - |  1796 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|         - |  1797 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|         - |  1798 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|   1077095 |  1799 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    359030 |  1800 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1801 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    359030 |  1802 | `			xValidator);` |
|    718065 |  1803 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1804 | `			return SXERR_ABORT;` |
|         - |  1805 | `		}` |
|    718065 |  1806 | `		if( iSpread ){` |
|         - |  1807 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        69 |  1808 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|    718032 |  1809 | `		}else if( iEmitRef ){` |
|         - |  1810 | `			/* Emit the load reference instruction */` |
|        40 |  1811 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1812 | `		}` |
|    718065 |  1813 | `		xValidator = 0;` |
|    718065 |  1814 | `		iEmitRef = 0;` |
|    718065 |  1815 | `		iSpread = 0;` |
|    718065 |  1816 | `		nPair++;` |
|         5 |  1817 | `	}` |
|         - |  1818 | `	/* Emit the load map instruction */` |
|    502961 |  1819 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1820 | `	/* Node successfully compiled */` |
|    502961 |  1821 | `	return SXRET_OK;` |
|    251491 |  1822 | `}` |
|         - |  1823 | `/*` |
|         - |  1824 | ` * Compile the 'array' language construct.` |
|         - |  1825 | ` *	 According to the PHP language reference manual` |
|         - |  1826 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1827 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1828 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1829 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1830 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1831 | ` */` |
|    286012 |  1832 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1833 | `{` |
|         - |  1834 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    286017 |  1835 | `	pGen->pIn += 2;` |
|    286017 |  1836 | `	pGen->pEnd--;` |
|    143006 |  1837 | `	SXUNUSED(iCompileFlag);` |
|    286017 |  1838 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1839 | `}` |
|         - |  1840 | `/*` |
|         - |  1841 | ` * Compile the PHP 8.5 clone(...) call form:` |
|         - |  1842 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|         - |  1843 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|         - |  1844 | ` *                                              property updates as scope-aware writes` |
|         - |  1845 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|         - |  1846 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|         - |  1847 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|         - |  1848 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|         - |  1849 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|         - |  1850 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|         - |  1851 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|         - |  1852 | ` */` |
|        22 |  1853 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  1854 | `{` |
|         - |  1855 | `	SyToken *pIn,*pEnd,*pNext;` |
|        24 |  1856 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|        24 |  1857 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|        24 |  1858 | `	int nArg = 0;` |
|         - |  1859 | `	sxi32 rc;` |
|        11 |  1860 | `	SXUNUSED(iCompileFlag);` |
|         - |  1861 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|        24 |  1862 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|        24 |  1863 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|         - |  1864 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|        24 |  1865 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       ! 0 |  1866 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  1867 | `			"clone(...) first-class callable form is not yet supported");` |
|         - |  1868 | `	}` |
|         - |  1869 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|        62 |  1870 | `	while( pIn < pEnd ){` |
|        40 |  1871 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|        40 |  1872 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|       ! 0 |  1873 | `			break;` |
|         - |  1874 | `		}` |
|        40 |  1875 | `		pArgStart = pIn;` |
|        40 |  1876 | `		pArgEnd   = pNext;` |
|         - |  1877 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|         - |  1878 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|        38 |  1879 | `		if( (pArgEnd - pArgStart) >= 2` |
|        37 |  1880 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        23 |  1881 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|         5 |  1882 | `			pName = pArgStart;` |
|         5 |  1883 | `			pArgStart += 2;` |
|         2 |  1884 | `		}` |
|        40 |  1885 | `		if( pName ){` |
|         - |  1886 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|         - |  1887 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|         4 |  1888 | `			if( pName->sData.nByte == sizeof("object")-1` |
|         4 |  1889 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|         3 |  1890 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|         4 |  1891 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|         3 |  1892 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|         3 |  1893 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         2 |  1894 | `			}else{` |
|       ! 0 |  1895 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|       ! 0 |  1896 | `					"Unknown named parameter $%z",&pName->sData);` |
|         1 |  1897 | `			}` |
|        38 |  1898 | `		}else if( nArg == 0 ){` |
|        22 |  1899 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        25 |  1900 | `		}else if( nArg == 1 ){` |
|        15 |  1901 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         8 |  1902 | `		}else{` |
|       ! 0 |  1903 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|         - |  1904 | `				"clone() expects at most 2 arguments");` |
|         - |  1905 | `		}` |
|        40 |  1906 | `		nArg++;` |
|        40 |  1907 | `		pIn = pNext;` |
|        40 |  1908 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 |  1909 | `			pIn++; /* step over the argument separator */` |
|         8 |  1910 | `		}` |
|         2 |  1911 | `	}` |
|        24 |  1912 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|       ! 0 |  1913 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  1914 | `			"clone() expects at least 1 argument, 0 given");` |
|         - |  1915 | `	}` |
|         - |  1916 | `	/* Object argument -> clone (+ __clone()). */` |
|        24 |  1917 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        24 |  1918 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  1919 | `		return SXERR_ABORT;` |
|         - |  1920 | `	}` |
|        24 |  1921 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|         - |  1922 | `	/* Property updates (evaluated after __clone runs). */` |
|        24 |  1923 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|        17 |  1924 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        17 |  1925 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1926 | `			return SXERR_ABORT;` |
|         - |  1927 | `		}` |
|        17 |  1928 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|         8 |  1929 | `	}` |
|        24 |  1930 | `	return SXRET_OK;` |
|        13 |  1931 | `}` |
|         - |  1932 | `/*` |
|         - |  1933 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|         - |  1934 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|         - |  1935 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|         - |  1936 | ` */` |
|    216960 |  1937 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1938 | `{` |
|         - |  1939 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    216965 |  1940 | `	pGen->pIn++;` |
|    216965 |  1941 | `	pGen->pEnd--;` |
|    108480 |  1942 | `	SXUNUSED(iCompileFlag);` |
|    216965 |  1943 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1944 | `}` |
|         - |  1945 | `/*` |
|         - |  1946 | ` * Expression tree validator callback for the 'list' language construct.` |
|         - |  1947 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1948 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1949 | ` * error message.` |
|         - |  1950 | ` * See the routine responible of compiling the list language construct` |
|         - |  1951 | ` * for more inforation.` |
|         - |  1952 | ` */` |
|       210 |  1953 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  1954 | `{` |
|       215 |  1955 | `	sxi32 rc = SXRET_OK;` |
|       215 |  1956 | `	if( pRoot->pOp ){` |
|         4 |  1957 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|         2 |  1958 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|         - |  1959 | `				/* Unexpected expression */` |
|       ! 0 |  1960 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1961 | `					"Assignments can only happen to writable values");` |
|       ! 0 |  1962 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 |  1963 | `					rc = SXERR_INVALID;` |
|       ! 0 |  1964 | `				}` |
|         1 |  1965 | `		}` |
|       213 |  1966 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1967 | `		/* Unexpected expression */` |
|         6 |  1968 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1969 | `			"Assignments can only happen to writable values");` |
|         6 |  1970 | `		if( rc != SXERR_ABORT ){` |
|         6 |  1971 | `			rc = SXERR_INVALID;` |
|         2 |  1972 | `		}` |
|         2 |  1973 | `	}` |
|       215 |  1974 | `	return rc;` |
|         5 |  1975 | `}` |
|         - |  1976 | `/*` |
|         - |  1977 | ` * Compile the 'list' language construct.` |
|         - |  1978 | ` *  According to the PHP language reference` |
|         - |  1979 | ` *  list(): Assign variables as if they were an array.` |
|         - |  1980 | ` *  list() is used to assign a list of variables in one operation.` |
|         - |  1981 | ` *  Description` |
|         - |  1982 | ` *   array list (mixed $varname [, mixed $... ] )` |
|         - |  1983 | ` *   Like array(), this is not really a function, but a language construct.` |
|         - |  1984 | ` *   list() is used to assign a list of variables in one operation.` |
|         - |  1985 | ` *  Parameters` |
|         - |  1986 | ` *   $varname: A variable.` |
|         - |  1987 | ` *  Return Values` |
|         - |  1988 | ` *   The assigned array.` |
|         - |  1989 | ` */` |
|         - |  1990 | `/* Nested list entry recorded during first pass of list body compilation */` |
|         - |  1991 | `struct NestedListEntry {` |
|         - |  1992 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|         - |  1993 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|         - |  1994 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|         - |  1995 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|         - |  1996 | `};` |
|         - |  1997 | `/*` |
|         - |  1998 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|         - |  1999 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|         - |  2000 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|         - |  2001 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|         - |  2002 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|         - |  2003 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|         - |  2004 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|         - |  2005 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|         - |  2006 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|         - |  2007 | ` */` |
|        22 |  2008 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|         1 |  2009 | `{` |
|         - |  2010 | `	SyToken *pNext;` |
|         - |  2011 | `	sxi32 rc;` |
|        53 |  2012 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|         - |  2013 | `		SyToken *pArrow,*pTarget;` |
|         - |  2014 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|        31 |  2015 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|        31 |  2016 | `		pTarget = &pArrow[1];` |
|        31 |  2017 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|         - |  2018 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|         - |  2019 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|       ! 0 |  2020 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2021 | `				"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2022 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2023 | `		}` |
|         - |  2024 | `		/* DUP the source array (it is on the stack top) */` |
|        31 |  2025 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2026 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|        31 |  2027 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|        31 |  2028 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2029 | `			return SXERR_ABORT;` |
|         - |  2030 | `		}` |
|         - |  2031 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|         - |  2032 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|         - |  2033 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|         - |  2034 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|         - |  2035 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|         - |  2036 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|        31 |  2037 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|        31 |  2038 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|        28 |  2039 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|        15 |  2040 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|         - |  2041 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|         - |  2042 | `			 * Treat source[key] as the inner body's source, then drop the` |
|         - |  2043 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|         5 |  2044 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|         5 |  2045 | `			SyToken *pSavedIn = pGen->pIn;` |
|         5 |  2046 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         5 |  2047 | `			pGen->pIn = pTarget;` |
|         5 |  2048 | `			pGen->pEnd = pNext;` |
|         5 |  2049 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|         2 |  2050 | `			             : PH7_CompileList(&(*pGen),0);` |
|         5 |  2051 | `			pGen->pIn = pSavedIn;` |
|         5 |  2052 | `			pGen->pEnd = pSavedEnd;` |
|         5 |  2053 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2054 | `				return SXERR_ABORT;` |
|         - |  2055 | `			}` |
|         5 |  2056 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         3 |  2057 | `		}else{` |
|         - |  2058 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|         - |  2059 | `			 * is already on the stack as the value; compiling the target appends` |
|         - |  2060 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|         - |  2061 | `			 * assignment does. */` |
|         - |  2062 | `			VmInstr *pInstr;` |
|        27 |  2063 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|        27 |  2064 | `			sxi32 iP1 = 0, iP2 = 0;` |
|        27 |  2065 | `			void *p3 = 0;` |
|        27 |  2066 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|         - |  2067 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|        27 |  2068 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2069 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2070 | `			}` |
|        27 |  2071 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|        27 |  2072 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|         3 |  2073 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|        26 |  2074 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         3 |  2075 | `					iVmOp = PH7_OP_STORE_IDX;` |
|         3 |  2076 | `					iP1 = pInstr->iP1;` |
|         3 |  2077 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         2 |  2078 | `				}else{` |
|        23 |  2079 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|        23 |  2080 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - |  2081 | `				}` |
|        13 |  2082 | `			}` |
|        27 |  2083 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|         - |  2084 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|         - |  2085 | `			 * source array is back on top for the next entry. */` |
|        27 |  2086 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         - |  2087 | `		}` |
|        31 |  2088 | `		pGen->pIn = &pNext[1];` |
|         1 |  2089 | `	}` |
|        23 |  2090 | `	return SXRET_OK;` |
|        12 |  2091 | `}` |
|         - |  2092 | `/*` |
|         - |  2093 | ` * Shared body for list() and short list [...] compilation.` |
|         - |  2094 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|         - |  2095 | ` * the opening delimiter and before the closing delimiter.` |
|         - |  2096 | ` */` |
|       122 |  2097 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|         5 |  2098 | `{` |
|         - |  2099 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|         - |  2100 | `	SyToken *pNext;` |
|         - |  2101 | `	SyToken *pClassifyIn;` |
|       127 |  2102 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|         - |  2103 | `	sxi32 nExpr;` |
|         - |  2104 | `	sxi32 rc;` |
|         - |  2105 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|         - |  2106 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|         - |  2107 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|         - |  2108 | `	 * list. */` |
|       127 |  2109 | `	pClassifyIn = pGen->pIn;` |
|       367 |  2110 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       245 |  2111 | `		if( pGen->pIn >= pNext ){` |
|        13 |  2112 | `			nEmpty++;` |
|       239 |  2113 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|        31 |  2114 | `			nKeyed++;` |
|        16 |  2115 | `		}else{` |
|       203 |  2116 | `			nPositional++;` |
|         - |  2117 | `		}` |
|       245 |  2118 | `		pGen->pIn = &pNext[1];` |
|         5 |  2119 | `	}` |
|       127 |  2120 | `	pGen->pIn = pClassifyIn;` |
|       127 |  2121 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|       ! 0 |  2122 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2123 | `			"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2124 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2125 | `	}` |
|       127 |  2126 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|       ! 0 |  2127 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2128 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|       ! 0 |  2129 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2130 | `	}` |
|       127 |  2131 | `	if( nKeyed > 0 ){` |
|        23 |  2132 | `		return GenStateCompileKeyedListBody(pGen);` |
|         - |  2133 | `	}` |
|       105 |  2134 | `	nExpr = 0;` |
|       105 |  2135 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       315 |  2136 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       215 |  2137 | `		if( pGen->pIn < pNext ){` |
|         - |  2138 | `			/* Check for nested list() */` |
|       203 |  2139 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         3 |  2140 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  2141 | `				/* Record this nested list for post-processing */` |
|         3 |  2142 | `				SyToken *pListEnd = 0;` |
|         3 |  2143 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|         3 |  2144 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         1 |  2145 | `				}` |
|         3 |  2146 | `				if( pListEnd ){` |
|         - |  2147 | `					struct NestedListEntry sEntry;` |
|         3 |  2148 | `					sEntry.nIndex = nExpr;` |
|         3 |  2149 | `					sEntry.pStart = pGen->pIn;` |
|         3 |  2150 | `					sEntry.pEnd = pListEnd + 1;` |
|         3 |  2151 | `					sEntry.isShort = 0;` |
|         3 |  2152 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         1 |  2153 | `				}` |
|         - |  2154 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|         3 |  2155 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       202 |  2156 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  2157 | `				/* Nested short destructuring [...] */` |
|        13 |  2158 | `				SyToken *pBracketEnd = 0;` |
|        13 |  2159 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|        13 |  2160 | `				if( pBracketEnd ){` |
|         - |  2161 | `					struct NestedListEntry sEntry;` |
|        13 |  2162 | `					sEntry.nIndex = nExpr;` |
|        13 |  2163 | `					sEntry.pStart = pGen->pIn;` |
|        13 |  2164 | `					sEntry.pEnd = pBracketEnd + 1;` |
|        13 |  2165 | `					sEntry.isShort = 1;` |
|        13 |  2166 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         6 |  2167 | `				}` |
|         - |  2168 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        13 |  2169 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |  2170 | `			}else{` |
|         - |  2171 | `				/* Compile the expression holding the variable */` |
|       189 |  2172 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       189 |  2173 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  2174 | `					SySetRelease(&sNested);` |
|       ! 0 |  2175 | `					return SXRET_OK;` |
|         - |  2176 | `				}` |
|         - |  2177 | `			}` |
|       104 |  2178 | `		}else{` |
|         - |  2179 | `			/* Empty entry,load NULL */` |
|        13 |  2180 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|         - |  2181 | `		}` |
|       215 |  2182 | `		nExpr++;` |
|         - |  2183 | `		/* Advance the stream cursor */` |
|       215 |  2184 | `		pGen->pIn = &pNext[1];` |
|         5 |  2185 | `	}` |
|         - |  2186 | `	/* Emit the LOAD_LIST instruction */` |
|       105 |  2187 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|         - |  2188 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|         - |  2189 | `	 * For each nested entry, emit code to extract the sub-array` |
|         - |  2190 | `	 * at the corresponding index and recursively destructure it.` |
|         - |  2191 | `	 */` |
|       105 |  2192 | `	if( SySetUsed(&sNested) > 0 ){` |
|        13 |  2193 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|         - |  2194 | `		sxu32 i;` |
|        27 |  2195 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|        15 |  2196 | `			SyToken *pSavedIn = pGen->pIn;` |
|        15 |  2197 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         - |  2198 | `			ph7_value *pIdx;` |
|         - |  2199 | `			sxu32 nConstIdx;` |
|         - |  2200 | `			/* DUP the source array (it's on stack top) */` |
|        15 |  2201 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2202 | `			/* Push the integer index for this nested entry */` |
|        15 |  2203 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|        15 |  2204 | `			if( pIdx == 0 ){` |
|       ! 0 |  2205 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2206 | `				SySetRelease(&sNested);` |
|       ! 0 |  2207 | `				return SXERR_ABORT;` |
|         - |  2208 | `			}` |
|        15 |  2209 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|        15 |  2210 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|         - |  2211 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|         - |  2212 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|         - |  2213 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|         - |  2214 | `			 */` |
|        15 |  2215 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|         - |  2216 | `			/* Recursively compile the inner list */` |
|        15 |  2217 | `			pGen->pIn = apNested[i].pStart;` |
|        15 |  2218 | `			pGen->pEnd = apNested[i].pEnd;` |
|        15 |  2219 | `			if( apNested[i].isShort ){` |
|        13 |  2220 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  2221 | `			}else{` |
|         3 |  2222 | `				rc = PH7_CompileList(&(*pGen),0);` |
|         - |  2223 | `			}` |
|        15 |  2224 | `			pGen->pIn = pSavedIn;` |
|        15 |  2225 | `			pGen->pEnd = pSavedEnd;` |
|        15 |  2226 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2227 | `				SySetRelease(&sNested);` |
|       ! 0 |  2228 | `				return SXERR_ABORT;` |
|         - |  2229 | `			}` |
|         - |  2230 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|        15 |  2231 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         8 |  2232 | `		}` |
|         6 |  2233 | `	}` |
|       105 |  2234 | `	SySetRelease(&sNested);` |
|         - |  2235 | `	/* Node successfully compiled */` |
|       105 |  2236 | `	return SXRET_OK;` |
|        66 |  2237 | `}` |
|        40 |  2238 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2239 | `{` |
|         - |  2240 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|        45 |  2241 | `	pGen->pIn += 2;` |
|        45 |  2242 | `	pGen->pEnd--;` |
|        20 |  2243 | `	SXUNUSED(iCompileFlag);` |
|        45 |  2244 | `	return GenStateCompileListBody(pGen);` |
|         5 |  2245 | `}` |
|        82 |  2246 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2247 | `{` |
|         - |  2248 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        84 |  2249 | `	pGen->pIn++;` |
|        84 |  2250 | `	pGen->pEnd--;` |
|        41 |  2251 | `	SXUNUSED(iCompileFlag);` |
|        84 |  2252 | `	return GenStateCompileListBody(pGen);` |
|         2 |  2253 | `}` |
|         - |  2254 | `/* Forward declarations */` |
|         - |  2255 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|         - |  2256 | `static int GenStateIsReservedConstant(SyString *pName);` |
|         - |  2257 | `static int GenStateIsReadonly(SyToken *pTok);` |
|         - |  2258 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|         - |  2259 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|         - |  2260 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - |  2261 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|         - |  2262 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|         - |  2263 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|         - |  2264 | `/*` |
|         - |  2265 | ` * Compile an annoynmous function or a closure.` |
|         - |  2266 | ` * According to the PHP language reference` |
|         - |  2267 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  2268 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  2269 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|         - |  2270 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|         - |  2271 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|         - |  2272 | ` *  Example Anonymous function variable assignment example` |
|         - |  2273 | ` * <?php` |
|         - |  2274 | ` * $greet = function($name)` |
|         - |  2275 | ` * {` |
|         - |  2276 | ` *    printf("Hello %s\r\n", $name);` |
|         - |  2277 | ` * };` |
|         - |  2278 | ` * $greet('World');` |
|         - |  2279 | ` * $greet('PHP');` |
|         - |  2280 | ` * ?>` |
|         - |  2281 | ` * Note that the implementation of annoynmous function and closure under` |
|         - |  2282 | ` * PH7 is completely different from the one used by the zend engine.` |
|         - |  2283 | ` */` |
|       464 |  2284 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2285 | `{` |
|       469 |  2286 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2287 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2288 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2289 | `							  * one thread is allowed to compile the script.` |
|         - |  2290 | `						      */` |
|         - |  2291 | `	SyString sName;` |
|       469 |  2292 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2293 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2294 | `	sxu32 nKwLine;` |
|       469 |  2295 | `	sxi32 iFlags = 0;` |
|         - |  2296 | `	sxu32 nLen;` |
|         - |  2297 | `	sxi32 rc;` |
|       232 |  2298 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2299 |  |
|       469 |  2300 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       464 |  2301 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       469 |  2302 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2303 | `		/* Static closure: no $this auto-capture, bind refused */` |
|         9 |  2304 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         9 |  2305 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         4 |  2306 | `	}` |
|       469 |  2307 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       469 |  2308 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2309 | `		pGen->pIn++;` |
|       ! 0 |  2310 | `	}` |
|         - |  2311 | `	/* Generate a unique name */` |
|       469 |  2312 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2313 | `	/* Make sure the generated name is unique */` |
|       469 |  2314 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2315 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2316 | `	}` |
|       469 |  2317 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2318 | `	/* Compile the lambda body */` |
|       469 |  2319 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       469 |  2320 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2321 | `		return SXERR_ABORT;` |
|         - |  2322 | `	}` |
|       469 |  2323 | `	if( pAnnonFunc ){` |
|       469 |  2324 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2325 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2326 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       469 |  2327 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2328 | `			return SXERR_ABORT;` |
|         - |  2329 | `		}` |
|       232 |  2330 | `	}` |
|         - |  2331 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2332 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2333 | `	 * the handler wraps either in a Closure instance. */` |
|       469 |  2334 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2335 | `	/* Node successfully compiled */` |
|       469 |  2336 | `	return SXRET_OK;` |
|       237 |  2337 | `}` |
|         - |  2338 | `/*` |
|         - |  2339 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2340 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2341 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2342 | ` */` |
|       204 |  2343 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2344 | `	ph7_gen_state *pGen,` |
|         - |  2345 | `	ph7_vm_func *pFunc,` |
|         - |  2346 | `	const char *zName,` |
|         - |  2347 | `	sxu32 nByte,` |
|         - |  2348 | `	SyString *aShadow,` |
|         - |  2349 | `	sxu32 nShadow)` |
|         3 |  2350 | `{` |
|         - |  2351 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2352 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2353 | `	sxu32 n, nEnv;` |
|         - |  2354 | `	char *zDup;` |
|       207 |  2355 | `	if( nByte == 0 ){` |
|       ! 0 |  2356 | `		return SXRET_OK;` |
|         - |  2357 | `	}` |
|       204 |  2358 | `	if( nByte == sizeof("this")-1` |
|       111 |  2359 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         3 |  2360 | `		return SXRET_OK;` |
|         - |  2361 | `	}` |
|       257 |  2362 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       192 |  2363 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       186 |  2364 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       143 |  2365 | `			return SXRET_OK;` |
|         - |  2366 | `		}` |
|        28 |  2367 | `	}` |
|        63 |  2368 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        63 |  2369 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|        91 |  2370 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2371 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2372 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2373 | `			return SXRET_OK;` |
|         - |  2374 | `		}` |
|        15 |  2375 | `	}` |
|        61 |  2376 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        61 |  2377 | `	if( zDup == 0 ){` |
|       ! 0 |  2378 | `		return SXERR_ABORT;` |
|         - |  2379 | `	}` |
|        61 |  2380 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        61 |  2381 | `	sEnv.iFlags = 0;` |
|        61 |  2382 | `	sEnv.nIdx = SXU32_HIGH;` |
|        61 |  2383 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        61 |  2384 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        61 |  2385 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        61 |  2386 | `	return SXRET_OK;` |
|       105 |  2387 | `}` |
|         - |  2388 | `/*` |
|         - |  2389 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|         - |  2390 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|         - |  2391 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|         - |  2392 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|         - |  2393 | ` */` |
|        56 |  2394 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|         - |  2395 | `	ph7_gen_state *pGen,` |
|         - |  2396 | `	ph7_vm_func *pFunc,` |
|         - |  2397 | `	const char *zIn,` |
|         - |  2398 | `	const char *zEnd,` |
|         - |  2399 | `	SyString *aShadow,` |
|         - |  2400 | `	sxu32 nShadow)` |
|         2 |  2401 | `{` |
|         - |  2402 | `	sxi32 rc;` |
|       370 |  2403 | `	while( zIn < zEnd ){` |
|       314 |  2404 | `		if( zIn[0] == '\\' ){` |
|         5 |  2405 | `			zIn++;` |
|         5 |  2406 | `			if( zIn < zEnd ){` |
|         5 |  2407 | `				zIn++;` |
|         2 |  2408 | `			}` |
|         5 |  2409 | `			continue;` |
|         - |  2410 | `		}` |
|       308 |  2411 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        26 |  2412 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|        24 |  2413 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|         - |  2414 | `			const char *zName;` |
|        26 |  2415 | `			zIn++; /* skip '$' */` |
|        26 |  2416 | `			zName = zIn;` |
|        82 |  2417 | `			while( zIn < zEnd ){` |
|        76 |  2418 | `				unsigned char c = (unsigned char)zIn[0];` |
|        76 |  2419 | `				if( c >= 0xc0 ){` |
|       ! 0 |  2420 | `					zIn++;` |
|       ! 0 |  2421 | `					while( zIn < zEnd` |
|       ! 0 |  2422 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  2423 | `						zIn++;` |
|       ! 0 |  2424 | `					}` |
|       ! 0 |  2425 | `					continue;` |
|         - |  2426 | `				}` |
|        76 |  2427 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        20 |  2428 | `					break;` |
|         - |  2429 | `				}` |
|        58 |  2430 | `				zIn++;` |
|         2 |  2431 | `			}` |
|        26 |  2432 | `			if( zIn > zName ){` |
|        38 |  2433 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|        24 |  2434 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|        26 |  2435 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2436 | `					return SXERR_ABORT;` |
|         - |  2437 | `				}` |
|        12 |  2438 | `			}` |
|        26 |  2439 | `			continue;` |
|         - |  2440 | `		}` |
|       286 |  2441 | `		zIn++;` |
|         2 |  2442 | `	}` |
|        58 |  2443 | `	return SXRET_OK;` |
|        30 |  2444 | `}` |
|         - |  2445 | `/*` |
|         - |  2446 | ` * Scan the body token range of an arrow function for free-variable` |
|         - |  2447 | ` * references and record them in pFunc's closure environment. Handles:` |
|         - |  2448 | ` *   - plain $<id> pairs` |
|         - |  2449 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|         - |  2450 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|         - |  2451 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|         - |  2452 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|         - |  2453 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|         - |  2454 | ` *     are never mistakenly captured.` |
|         - |  2455 | ` */` |
|       304 |  2456 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2457 | `	ph7_gen_state *pGen,` |
|         - |  2458 | `	ph7_vm_func *pFunc,` |
|         - |  2459 | `	SyToken *pStart,` |
|         - |  2460 | `	SyToken *pEnd,` |
|         - |  2461 | `	SyString *aShadow,` |
|         - |  2462 | `	sxu32 nShadow)` |
|         5 |  2463 | `{` |
|       309 |  2464 | `	SyToken *pScan = pStart;` |
|         - |  2465 | `	sxi32 rc;` |
|      1741 |  2466 | `	while( pScan < pEnd ){` |
|      1437 |  2467 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|        86 |  2468 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        28 |  2469 | `				pScan->sData.zString,` |
|        56 |  2470 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        28 |  2471 | `				aShadow,nShadow);` |
|        58 |  2472 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2473 | `				return SXERR_ABORT;` |
|         - |  2474 | `			}` |
|        58 |  2475 | `			pScan++;` |
|        58 |  2476 | `			continue;` |
|         - |  2477 | `		}` |
|      1381 |  2478 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        30 |  2479 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        30 |  2480 | `			SyToken *pFnKw = pScan;` |
|        28 |  2481 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2482 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         2 |  2483 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2484 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2485 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2486 | `			}` |
|        30 |  2487 | `			if( nKw == PH7_TKWRD_FN ){` |
|         - |  2488 | `				SyToken *pInnerSigStart;` |
|         - |  2489 | `				SyToken *pInnerSigEnd;` |
|         - |  2490 | `				SyToken *pInnerBodyEnd;` |
|         - |  2491 | `				SyString *aInnerShadow;` |
|         - |  2492 | `				sxu32 nInnerShadow;` |
|         - |  2493 | `				sxu32 nInnerParamMax;` |
|         - |  2494 | `				SyToken *p;` |
|         - |  2495 | `				int iNestInner;` |
|        19 |  2496 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|        19 |  2497 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2498 | `					pScan++;` |
|       ! 0 |  2499 | `				}` |
|        19 |  2500 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2501 | `					pScan++;` |
|       ! 0 |  2502 | `					continue;` |
|         - |  2503 | `				}` |
|        19 |  2504 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|        19 |  2505 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|         - |  2506 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|        19 |  2507 | `				if( pInnerSigEnd >= pEnd ){` |
|       ! 0 |  2508 | `					pScan = pEnd;` |
|       ! 0 |  2509 | `					continue;` |
|         - |  2510 | `				}` |
|         - |  2511 | `				/* Build an augmented shadow list: inherited + inner params */` |
|        19 |  2512 | `				nInnerParamMax = 0;` |
|        57 |  2513 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2514 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|        13 |  2515 | `						nInnerParamMax++;` |
|         6 |  2516 | `					}` |
|        20 |  2517 | `				}` |
|        19 |  2518 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|        18 |  2519 | `					&pGen->pVm->sAllocator,` |
|        18 |  2520 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|        19 |  2521 | `				if( aInnerShadow == 0 ){` |
|       ! 0 |  2522 | `					return SXERR_ABORT;` |
|         - |  2523 | `				}` |
|        19 |  2524 | `				nInnerShadow = 0;` |
|        25 |  2525 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|         7 |  2526 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|         4 |  2527 | `				}` |
|        57 |  2528 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2529 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|        27 |  2530 | `						continue;` |
|         - |  2531 | `					}` |
|        13 |  2532 | `					if( &p[1] >= pInnerSigEnd ){` |
|       ! 0 |  2533 | `						break;` |
|         - |  2534 | `					}` |
|        13 |  2535 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2536 | `						continue;` |
|         - |  2537 | `					}` |
|        13 |  2538 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|         7 |  2539 | `				}` |
|        19 |  2540 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|        19 |  2541 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|       ! 0 |  2542 | `					pScan++;` |
|       ! 0 |  2543 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|       ! 0 |  2544 | `						&& pScan->sData.nByte == 1` |
|       ! 0 |  2545 | `						&& pScan->sData.zString[0] == '?' ){` |
|       ! 0 |  2546 | `						pScan++;` |
|       ! 0 |  2547 | `					}` |
|       ! 0 |  2548 | `					if( pScan < pEnd` |
|       ! 0 |  2549 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  2550 | `						pScan++;` |
|       ! 0 |  2551 | `					}` |
|       ! 0 |  2552 | `				}` |
|        19 |  2553 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|        19 |  2554 | `					pScan++; /* past '=>' */` |
|         9 |  2555 | `				}` |
|        19 |  2556 | `				pInnerBodyEnd = pScan;` |
|        19 |  2557 | `				iNestInner = 0;` |
|       131 |  2558 | `				while( pInnerBodyEnd < pEnd ){` |
|       113 |  2559 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|         - |  2560 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|         - |  2561 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       ! 0 |  2562 | `						break;` |
|         - |  2563 | `					}` |
|       113 |  2564 | `					if( pInnerBodyEnd->nType &` |
|         - |  2565 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         3 |  2566 | `						iNestInner++;` |
|       112 |  2567 | `					}else if( pInnerBodyEnd->nType &` |
|         - |  2568 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         3 |  2569 | `						iNestInner--;` |
|         1 |  2570 | `					}` |
|       113 |  2571 | `					pInnerBodyEnd++;` |
|         1 |  2572 | `				}` |
|         - |  2573 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|         - |  2574 | `				 * the outer's body: a default value is evaluated at call time` |
|         - |  2575 | `				 * in the outer frame, so any free variable it references is` |
|         - |  2576 | `				 * an outer capture. We must NOT scan the parameter-name` |
|         - |  2577 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|         - |  2578 | `				 * or those names leak into the outer's closure environment.` |
|         - |  2579 | `				 *` |
|         - |  2580 | `				 * Walk the signature argument-by-argument, splitting on` |
|         - |  2581 | `				 * top-level commas, and for each argument scan only the token` |
|         - |  2582 | `				 * range after the '=' sign. */` |
|         - |  2583 | `				{` |
|        19 |  2584 | `					SyToken *pArgStart = pInnerSigStart;` |
|        31 |  2585 | `					while( pArgStart < pInnerSigEnd ){` |
|        13 |  2586 | `						SyToken *pArgEnd = pArgStart;` |
|        13 |  2587 | `						SyToken *pEq = 0;` |
|        13 |  2588 | `						int iNestArg = 0;` |
|        49 |  2589 | `						while( pArgEnd < pInnerSigEnd ){` |
|        38 |  2590 | `							if( iNestArg == 0` |
|        39 |  2591 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|         3 |  2592 | `								break;` |
|         - |  2593 | `							}` |
|        37 |  2594 | `							if( pArgEnd->nType &` |
|         - |  2595 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  2596 | `								iNestArg++;` |
|        37 |  2597 | `							}else if( pArgEnd->nType &` |
|         - |  2598 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  2599 | `								iNestArg--;` |
|       ! 0 |  2600 | `							}` |
|        36 |  2601 | `							if( pEq == 0 && iNestArg == 0` |
|        31 |  2602 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|         7 |  2603 | `								pEq = pArgEnd;` |
|         3 |  2604 | `							}` |
|        37 |  2605 | `							pArgEnd++;` |
|         1 |  2606 | `						}` |
|        13 |  2607 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|        10 |  2608 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         3 |  2609 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|         7 |  2610 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  2611 | `								return SXERR_ABORT;` |
|         - |  2612 | `							}` |
|         3 |  2613 | `						}` |
|        13 |  2614 | `						pArgStart = pArgEnd;` |
|        12 |  2615 | `						if( pArgStart < pInnerSigEnd` |
|         8 |  2616 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|         3 |  2617 | `							pArgStart++;` |
|         1 |  2618 | `						}` |
|         1 |  2619 | `					}` |
|         - |  2620 | `				}` |
|        28 |  2621 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         9 |  2622 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|        19 |  2623 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2624 | `					return SXERR_ABORT;` |
|         - |  2625 | `				}` |
|        19 |  2626 | `				pScan = pInnerBodyEnd;` |
|        19 |  2627 | `				continue;` |
|         - |  2628 | `			}` |
|         5 |  2629 | `		}` |
|      1363 |  2630 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      1183 |  2631 | `			pScan++;` |
|      1183 |  2632 | `			continue;` |
|         - |  2633 | `		}` |
|         - |  2634 | `		{` |
|         - |  2635 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       183 |  2636 | `			SyToken *pDollar = pScan;` |
|       270 |  2637 | `			while( &pDollar[1] < pEnd` |
|       183 |  2638 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2639 | `				pDollar++;` |
|       ! 0 |  2640 | `			}` |
|       183 |  2641 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2642 | `				break;` |
|         - |  2643 | `			}` |
|       183 |  2644 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2645 | `				pScan = pDollar + 1;` |
|       ! 0 |  2646 | `				continue;` |
|         - |  2647 | `			}` |
|       273 |  2648 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       180 |  2649 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|        90 |  2650 | `				aShadow,nShadow);` |
|       183 |  2651 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2652 | `				return SXERR_ABORT;` |
|         - |  2653 | `			}` |
|       183 |  2654 | `			pScan = pDollar + 2;` |
|         - |  2655 | `		}` |
|         3 |  2656 | `	}` |
|       309 |  2657 | `	return SXRET_OK;` |
|       157 |  2658 | `}` |
|         - |  2659 | `/*` |
|         - |  2660 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2661 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2662 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2663 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2664 | ` * $this is also made available.` |
|         - |  2665 | ` */` |
|       286 |  2666 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2667 | `{` |
|         - |  2668 | `	ph7_vm_func *pFunc;` |
|         - |  2669 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2670 | `	GenBlock *pBlock;` |
|         - |  2671 | `	SySet *pInstrContainer;` |
|         - |  2672 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|         - |  2673 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|         - |  2674 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|         - |  2675 | `	SyToken *pSavedEnd;` |
|         - |  2676 | `	ph7_vm_func_arg *aArgs;` |
|         - |  2677 | `	char zName[512];` |
|         - |  2678 | `	static int iCnt = 1;` |
|         - |  2679 | `	char *zDup;` |
|         - |  2680 | `	SyToken *pTokKw;` |
|         - |  2681 | `	sxu32 nLen;` |
|         - |  2682 | `	sxu32 nLine;` |
|       291 |  2683 | `	sxi32 iFlags = 0;` |
|       291 |  2684 | `	int bStatic = 0;` |
|         - |  2685 | `	sxi32 rc;` |
|         - |  2686 | `	sxu32 n;` |
|       143 |  2687 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2688 |  |
|       291 |  2689 | `	nLine = pGen->pIn->nLine;` |
|         - |  2690 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       291 |  2691 | `	pTokKw = pGen->pIn;` |
|         - |  2692 | `	/* Optional 'static' prefix */` |
|       286 |  2693 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       291 |  2694 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  2695 | `		bStatic = 1;` |
|         7 |  2696 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         7 |  2697 | `		pGen->pIn++;` |
|         3 |  2698 | `	}` |
|         - |  2699 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       286 |  2700 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       291 |  2701 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2702 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2703 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2704 | `		return SXERR_SYNTAX;` |
|         - |  2705 | `	}` |
|       291 |  2706 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2707 | `	/* Optional '&' — return by reference */` |
|       291 |  2708 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2709 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2710 | `		pGen->pIn++;` |
|       ! 0 |  2711 | `	}` |
|         - |  2712 | `	/* Expect '(' */` |
|       291 |  2713 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  2714 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2715 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2716 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|         2 |  2717 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2718 | `		}else{` |
|       ! 0 |  2719 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2720 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|         - |  2721 | `		}` |
|         3 |  2722 | `		return SXERR_SYNTAX;` |
|         - |  2723 | `	}` |
|       289 |  2724 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2725 | `	/* Delimit the parameter list */` |
|       289 |  2726 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       289 |  2727 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2728 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2729 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2730 | `		return SXERR_SYNTAX;` |
|         - |  2731 | `	}` |
|         - |  2732 | `	/* Allocate the function state */` |
|       287 |  2733 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       287 |  2734 | `	if( pFunc == 0 ){` |
|       ! 0 |  2735 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2736 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2737 | `		return SXERR_ABORT;` |
|         - |  2738 | `	}` |
|         - |  2739 | `	/* Generate a unique lambda name */` |
|       287 |  2740 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       287 |  2741 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2742 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2743 | `	}` |
|       287 |  2744 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       287 |  2745 | `	if( zDup == 0 ){` |
|       ! 0 |  2746 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2747 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2748 | `		return SXERR_ABORT;` |
|         - |  2749 | `	}` |
|       287 |  2750 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2751 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       287 |  2752 | `	pFunc->nLine = nLine;` |
|         - |  2753 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       287 |  2754 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2755 | `		return SXERR_ABORT;` |
|         - |  2756 | `	}` |
|         - |  2757 | `	/* Collect function arguments */` |
|       287 |  2758 | `	if( pGen->pIn < pSigEnd ){` |
|       116 |  2759 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       116 |  2760 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2761 | `			return SXERR_ABORT;` |
|         - |  2762 | `		}` |
|        56 |  2763 | `	}` |
|         - |  2764 | `	/* Point past ')' and parse optional return type */` |
|       287 |  2765 | `	pGen->pIn = &pSigEnd[1];` |
|       287 |  2766 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       287 |  2767 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2768 | `		return SXERR_ABORT;` |
|       287 |  2769 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2770 | `		return SXERR_SYNTAX;` |
|         - |  2771 | `	}` |
|         - |  2772 | `	/* Expect '=>' */` |
|       287 |  2773 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  2774 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2775 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2776 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|         2 |  2777 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2778 | `		}else{` |
|       ! 0 |  2779 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2780 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|         - |  2781 | `		}` |
|         3 |  2782 | `		return SXERR_SYNTAX;` |
|         - |  2783 | `	}` |
|       285 |  2784 | `	pGen->pIn++; /* Jump '=>' */` |
|       285 |  2785 | `	pBodyStart = pGen->pIn;` |
|       285 |  2786 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2787 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2788 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2789 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2790 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       285 |  2791 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2792 | `	{` |
|       285 |  2793 | `		SyString *aShadow = 0;` |
|       285 |  2794 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       285 |  2795 | `		if( nShadow > 0 ){` |
|       113 |  2796 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       110 |  2797 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       113 |  2798 | `			if( aShadow == 0 ){` |
|       ! 0 |  2799 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2800 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2801 | `				return SXERR_ABORT;` |
|         - |  2802 | `			}` |
|       257 |  2803 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       147 |  2804 | `				aShadow[n] = aArgs[n].sName;` |
|        75 |  2805 | `			}` |
|        55 |  2806 | `		}` |
|       425 |  2807 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       140 |  2808 | `			aShadow,nShadow);` |
|       285 |  2809 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2810 | `			return SXERR_ABORT;` |
|         - |  2811 | `		}` |
|         - |  2812 | `	}` |
|         - |  2813 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2814 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2815 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2816 | `	 * $this. */` |
|       285 |  2817 | `	if( !bStatic ){` |
|         - |  2818 | `		char *zThisDup;` |
|       279 |  2819 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       279 |  2820 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2821 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2822 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2823 | `			return SXERR_ABORT;` |
|         - |  2824 | `		}` |
|       279 |  2825 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       279 |  2826 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       279 |  2827 | `		sEnv.nIdx = SXU32_HIGH;` |
|       279 |  2828 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       279 |  2829 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       279 |  2830 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       137 |  2831 | `	}` |
|         - |  2832 | `	/* Arrow functions are always closures */` |
|       285 |  2833 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2834 | `	/* Compile the body expression as an implicit return */` |
|       425 |  2835 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       140 |  2836 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       285 |  2837 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2838 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2839 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2840 | `		return SXERR_ABORT;` |
|         - |  2841 | `	}` |
|       285 |  2842 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       285 |  2843 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       285 |  2844 | `	pSavedEnd = pGen->pEnd;` |
|       285 |  2845 | `	pGen->pIn = pBodyStart;` |
|       285 |  2846 | `	pGen->pEnd = pBodyEnd;` |
|       285 |  2847 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       285 |  2848 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2849 | `		return SXERR_ABORT;` |
|         - |  2850 | `	}` |
|         - |  2851 | `	/* The cursor stopped just past the body expression */` |
|       285 |  2852 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2853 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2854 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2855 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2856 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       285 |  2857 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       285 |  2858 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       285 |  2859 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       285 |  2860 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       285 |  2861 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2862 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       285 |  2863 | `	pGen->pIn = pBodyEnd;` |
|       285 |  2864 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2865 | `	/* Emit the load-closure instruction */` |
|       285 |  2866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       285 |  2867 | `	return SXRET_OK;` |
|       148 |  2868 | `}` |
|         - |  2869 | `/*` |
|         - |  2870 | ` * Compile a single arm's expression range into a freshly-allocated` |
|         - |  2871 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|         - |  2872 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|         - |  2873 | ` * expression's value.` |
|         - |  2874 | ` */` |
|       354 |  2875 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|         - |  2876 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|         3 |  2877 | `{` |
|         - |  2878 | `	SySet *pInstrContainer;` |
|         - |  2879 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  2880 | `	GenBlock *pArmBlock;` |
|         - |  2881 | `	sxi32 rc;` |
|       357 |  2882 | `	pTmpIn  = pGen->pIn;` |
|       357 |  2883 | `	pTmpEnd = pGen->pEnd;` |
|       357 |  2884 | `	pGen->pIn  = pStart;` |
|       357 |  2885 | `	pGen->pEnd = pStop;` |
|       357 |  2886 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       357 |  2887 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|         - |  2888 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|         - |  2889 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|         - |  2890 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|         - |  2891 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|         - |  2892 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|       534 |  2893 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       177 |  2894 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|       357 |  2895 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2896 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  2897 | `		pGen->pIn  = pTmpIn;` |
|       ! 0 |  2898 | `		pGen->pEnd = pTmpEnd;` |
|       ! 0 |  2899 | `		return SXERR_ABORT;` |
|         - |  2900 | `	}` |
|       357 |  2901 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       357 |  2902 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       357 |  2903 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       357 |  2904 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       357 |  2905 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       357 |  2906 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       357 |  2907 | `	pGen->pIn  = pTmpIn;` |
|       357 |  2908 | `	pGen->pEnd = pTmpEnd;` |
|       357 |  2909 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2910 | `		return SXERR_ABORT;` |
|         - |  2911 | `	}` |
|       357 |  2912 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 |  2913 | `		return SXERR_EMPTY;` |
|         - |  2914 | `	}` |
|       357 |  2915 | `	return SXRET_OK;` |
|       180 |  2916 | `}` |
|         - |  2917 | `/*` |
|         - |  2918 | ` * Compile a PHP 8.0 match expression:` |
|         - |  2919 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|         - |  2920 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|         - |  2921 | ` * Strict comparison (===) is used between the subject and each condition.` |
|         - |  2922 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|         - |  2923 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|         - |  2924 | ` */` |
|         - |  2925 | `/*` |
|         - |  2926 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|         - |  2927 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|         - |  2928 | ` * caller can bail out of the current expression.` |
|         - |  2929 | ` */` |
|         2 |  2930 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|         1 |  2931 | `{` |
|         - |  2932 | `	va_list ap;` |
|         - |  2933 | `	sxi32 rc;` |
|         - |  2934 | `	SyBlob sMsg;` |
|         3 |  2935 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|         3 |  2936 | `	va_start(ap,zFmt);` |
|         3 |  2937 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|         3 |  2938 | `	va_end(ap);` |
|         3 |  2939 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|         3 |  2940 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|         3 |  2941 | `	SyBlobRelease(&sMsg);` |
|         3 |  2942 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2943 | `		return SXERR_ABORT;` |
|         - |  2944 | `	}` |
|         3 |  2945 | `	return SXERR_SYNTAX;` |
|         2 |  2946 | `}` |
|         - |  2947 | `/*` |
|         - |  2948 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|         - |  2949 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|         - |  2950 | ` * Returns the stop token pointer (or pEnd if none found).` |
|         - |  2951 | ` */` |
|       356 |  2952 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|         4 |  2953 | `{` |
|       360 |  2954 | `	SyToken *pCur = pStart;` |
|       360 |  2955 | `	int iNest = 0;` |
|       838 |  2956 | `	while( pCur < pEnd ){` |
|       802 |  2957 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        13 |  2958 | `			iNest++;` |
|       796 |  2959 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        13 |  2960 | `			iNest--;` |
|       784 |  2961 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|       323 |  2962 | `			return pCur;` |
|         - |  2963 | `		}` |
|       482 |  2964 | `		pCur++;` |
|         4 |  2965 | `	}` |
|        39 |  2966 | `	return pEnd;` |
|       182 |  2967 | `}` |
|        72 |  2968 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2969 | `{` |
|         - |  2970 | `	ph7_match *pMatch;` |
|         - |  2971 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|        77 |  2972 | `	int bHasDefault = 0;` |
|         - |  2973 | `	sxu32 nLine;` |
|         - |  2974 | `	sxi32 rc;` |
|        36 |  2975 | `	SXUNUSED(iCompileFlag);` |
|        77 |  2976 | `	nLine = pGen->pIn->nLine;` |
|        77 |  2977 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|         - |  2978 | `	/* Expect '(' */` |
|        77 |  2979 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2980 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2981 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|       ! 0 |  2982 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|         - |  2983 | `	}` |
|        77 |  2984 | `	pGen->pIn++; /* Jump '(' */` |
|        77 |  2985 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|        77 |  2986 | `	if( pSubjEnd >= pGen->pEnd ){` |
|       ! 0 |  2987 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2988 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         - |  2989 | `	}` |
|        77 |  2990 | `	if( pGen->pIn >= pSubjEnd ){` |
|       ! 0 |  2991 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2992 | `			"syntax error, unexpected \")\", expecting match subject");` |
|         - |  2993 | `	}` |
|         - |  2994 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|        77 |  2995 | `	pSavedEnd = pGen->pEnd;` |
|        77 |  2996 | `	pGen->pEnd = pSubjEnd;` |
|        77 |  2997 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        77 |  2998 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2999 | `		return SXERR_ABORT;` |
|         - |  3000 | `	}` |
|        77 |  3001 | `	pGen->pEnd = pSavedEnd;` |
|        77 |  3002 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|         - |  3003 | `	/* Expect '{' */` |
|        77 |  3004 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 |  3005 | `		return GenStateMatchError(pGen,` |
|       ! 0 |  3006 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  3007 | `			"syntax error, expecting \"{\" after match subject");` |
|         - |  3008 | `	}` |
|        77 |  3009 | `	pGen->pIn++; /* Jump '{' */` |
|        77 |  3010 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|        77 |  3011 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  3012 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3013 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|         - |  3014 | `	}` |
|         - |  3015 | `	/* Allocate ph7_match container */` |
|        77 |  3016 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|        77 |  3017 | `	if( pMatch == 0 ){` |
|       ! 0 |  3018 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  3019 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3020 | `		return SXERR_ABORT;` |
|         - |  3021 | `	}` |
|        77 |  3022 | `	SyZero(pMatch,sizeof(ph7_match));` |
|        77 |  3023 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|         - |  3024 | `	/* Iterate arms */` |
|       259 |  3025 | `	while( pGen->pIn < pBodyEnd ){` |
|         - |  3026 | `		ph7_match_arm sArm;` |
|         - |  3027 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|       190 |  3028 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|       190 |  3029 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|       190 |  3030 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|       190 |  3031 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3032 | `		/* 'default' arm? */` |
|       186 |  3033 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       107 |  3034 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|        22 |  3035 | `			if( bHasDefault ){` |
|         3 |  3036 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|         - |  3037 | `					"Match expressions may only contain one default arm");` |
|         4 |  3038 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  3039 | `			}` |
|        20 |  3040 | `			sArm.bDefault = 1;` |
|        20 |  3041 | `			bHasDefault = 1;` |
|        20 |  3042 | `			pGen->pIn++;` |
|        20 |  3043 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       ! 0 |  3044 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3045 | `					"syntax error, expecting \"=>\" after 'default'");` |
|         - |  3046 | `			}` |
|        20 |  3047 | `			pGen->pIn++; /* Jump '=>' */` |
|        11 |  3048 | `		}else{` |
|         - |  3049 | `			/* Condition list: cond (',' cond)* '=>' */` |
|       170 |  3050 | `			pCondStart = pGen->pIn;` |
|       170 |  3051 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|         - |  3052 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       178 |  3053 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|         - |  3054 | `				SySet sCondBc;` |
|         9 |  3055 | `				if( pCondStart >= pArrow ){` |
|       ! 0 |  3056 | `					return GenStateMatchError(pGen,nArmLine,` |
|         - |  3057 | `						"syntax error, empty match condition expression");` |
|         - |  3058 | `				}` |
|         9 |  3059 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         9 |  3060 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|         9 |  3061 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3062 | `					return SXERR_ABORT;` |
|         - |  3063 | `				}` |
|         9 |  3064 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         9 |  3065 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|         9 |  3066 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|         - |  3067 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|         1 |  3068 | `			}` |
|       170 |  3069 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  3070 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3071 | `					"syntax error, expecting \"=>\" in match arm");` |
|         - |  3072 | `			}` |
|       167 |  3073 | `			if( pCondStart >= pArrow ){` |
|       ! 0 |  3074 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3075 | `					"syntax error, empty match condition expression");` |
|         - |  3076 | `			}` |
|         - |  3077 | `			{` |
|         - |  3078 | `				SySet sCondBc;` |
|       167 |  3079 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       167 |  3080 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       167 |  3081 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3082 | `					return SXERR_ABORT;` |
|         - |  3083 | `				}` |
|       167 |  3084 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         - |  3085 | `			}` |
|       167 |  3086 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|         - |  3087 | `		}` |
|         - |  3088 | `		/* Compile result expression: up to top-level ',' or body end */` |
|       185 |  3089 | `		pResStart = pGen->pIn;` |
|       185 |  3090 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|       185 |  3091 | `		if( pResStart >= pResEnd ){` |
|       ! 0 |  3092 | `			return GenStateMatchError(pGen,nArmLine,` |
|         - |  3093 | `				"syntax error, expected expression after \"=>\"");` |
|         - |  3094 | `		}` |
|       185 |  3095 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|       185 |  3096 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3097 | `			return SXERR_ABORT;` |
|         - |  3098 | `		}` |
|       185 |  3099 | `		pGen->pIn = pResEnd;` |
|       185 |  3100 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       151 |  3101 | `			pGen->pIn++; /* Skip trailing ',' */` |
|        74 |  3102 | `		}` |
|       185 |  3103 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|         3 |  3104 | `	}` |
|        71 |  3105 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|        71 |  3106 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|        71 |  3107 | `	return SXRET_OK;` |
|        41 |  3108 | `}` |
|         - |  3109 | `/*` |
|         - |  3110 | ` * Compile a backtick quoted string.` |
|         - |  3111 | ` */` |
|         4 |  3112 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  3113 | `{` |
|         - |  3114 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|         - |  3115 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|         - |  3116 | `	 */` |
|         8 |  3117 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|         - |  3118 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|         2 |  3119 | `		ph7_lib_version()` |
|         - |  3120 | `		);` |
|         - |  3121 | `	/* Load NULL */` |
|         6 |  3122 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         2 |  3123 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  3124 | `	/* Node successfully compiled */` |
|         6 |  3125 | `	return SXRET_OK;` |
|         2 |  3126 | `}` |
|         - |  3127 | `/*` |
|         - |  3128 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|         - |  3129 | ` * construct.` |
|         - |  3130 | ` */` |
|        82 |  3131 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3132 | `{` |
|         - |  3133 | `	SyString *pName;` |
|         - |  3134 | `	sxu32 nKeyID;` |
|         - |  3135 | `	sxi32 rc;` |
|         - |  3136 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        87 |  3137 | `	pName = &pGen->pIn->sData;` |
|        87 |  3138 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        87 |  3139 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        87 |  3140 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|         9 |  3141 | `		SyToken *pTmp,*pNext = 0;` |
|         - |  3142 | `		/* Compile arguments one after one */` |
|         9 |  3143 | `		pTmp = pGen->pEnd;` |
|         - |  3144 | `		/* Symisc eXtension to the PHP programming language:` |
|         - |  3145 | `		 * 'echo' can be used in the context of a function which` |
|         - |  3146 | `		 *  mean that the following expression is valid:` |
|         - |  3147 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|         - |  3148 | `		 */` |
|         9 |  3149 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        17 |  3150 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|         9 |  3151 | `			if( pGen->pIn < pNext ){` |
|         9 |  3152 | `				pGen->pEnd = pNext;` |
|         9 |  3153 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|         9 |  3154 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3155 | `					return SXERR_ABORT;` |
|         - |  3156 | `				}` |
|         9 |  3157 | `				if( rc != SXERR_EMPTY ){` |
|         - |  3158 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|         - |  3159 | `					 * without the overhead of a function call.` |
|         - |  3160 | `					 * This is a very powerful optimization that improve` |
|         - |  3161 | `					 * performance greatly.` |
|         - |  3162 | `					 */` |
|         9 |  3163 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|         4 |  3164 | `				}` |
|         4 |  3165 | `			}` |
|         - |  3166 | `			/* Jump trailing commas */` |
|         9 |  3167 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|       ! 0 |  3168 | `				pNext++;` |
|       ! 0 |  3169 | `			}` |
|         9 |  3170 | `			pGen->pIn = pNext;` |
|         1 |  3171 | `		}` |
|         - |  3172 | `		/* Restore token stream */` |
|         9 |  3173 | `		pGen->pEnd = pTmp;` |
|         5 |  3174 | `	}else{` |
|        79 |  3175 | `		sxi32 nArg = 0;` |
|        79 |  3176 | `		sxu32 nIdx = 0;` |
|        79 |  3177 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        79 |  3178 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3179 | `			return SXERR_ABORT;` |
|        79 |  3180 | `		}else if(rc != SXERR_EMPTY ){` |
|        79 |  3181 | `			nArg = 1;` |
|        37 |  3182 | `		}` |
|        79 |  3183 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3184 | `			ph7_value *pObj;` |
|         - |  3185 | `			/* Emit the call instruction */` |
|        31 |  3186 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        31 |  3187 | `			if( pObj == 0 ){` |
|       ! 0 |  3188 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3189 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3190 | `				return SXERR_ABORT;` |
|         - |  3191 | `			}` |
|        31 |  3192 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3193 | `			/* Install in the literal table */` |
|        31 |  3194 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        13 |  3195 | `		}` |
|         - |  3196 | `		/* Emit the call instruction */` |
|        79 |  3197 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        79 |  3198 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3199 | `	}` |
|         - |  3200 | `	/* Node successfully compiled */` |
|        87 |  3201 | `	return SXRET_OK;` |
|        46 |  3202 | `}` |
|         - |  3203 | `/*` |
|         - |  3204 | ` * Compile a node holding a variable declaration.` |
|         - |  3205 | ` * According to the PHP language reference` |
|         - |  3206 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|         - |  3207 | ` *  The variable name is case-sensitive.` |
|         - |  3208 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|         - |  3209 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3210 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|         - |  3211 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|         - |  3212 | ` *  Note: $this is a special variable that can't be assigned.` |
|         - |  3213 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|         - |  3214 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|         - |  3215 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|         - |  3216 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|         - |  3217 | ` *  the chapter on Expressions.` |
|         - |  3218 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|         - |  3219 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|         - |  3220 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|         - |  3221 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|         - |  3222 | ` *  is being assigned (the source variable).` |
|         - |  3223 | ` */` |
|  16083960 |  3224 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3225 | `{` |
|  16083965 |  3226 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3227 | `	sxi32 iVv;` |
|         - |  3228 | `	sxi32 iP1;` |
|         - |  3229 | `	void *p3;` |
|         - |  3230 | `	sxi32 rc;` |
|  16083965 |  3231 | `	iVv = -1; /* Variable variable counter */` |
|  32167937 |  3232 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  16083977 |  3233 | `		pGen->pIn++;` |
|  16083977 |  3234 | `		iVv++;` |
|         5 |  3235 | `	}` |
|  16083965 |  3236 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3237 | `		/* Invalid variable name */` |
|       ! 0 |  3238 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3239 | `		if( rc == SXERR_ABORT ){` |
|         - |  3240 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3241 | `			return SXERR_ABORT;` |
|         - |  3242 | `		}` |
|       ! 0 |  3243 | `		return SXRET_OK;` |
|         - |  3244 | `	}` |
|  16083965 |  3245 | `	p3  = 0;` |
|  16083965 |  3246 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|         - |  3247 | `		/* Dynamic variable creation */` |
|        21 |  3248 | `		pGen->pIn++;  /* Jump the open curly */` |
|        21 |  3249 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|        21 |  3250 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3251 | `			/* Empty expression */` |
|         - |  3252 | `			{` |
|         - |  3253 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|         - |  3254 | `			 * the "expecting" tail only appears when something could still follow. */` |
|         3 |  3255 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|         3 |  3256 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|         1 |  3257 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|         - |  3258 | `			}` |
|         3 |  3259 | `			return SXRET_OK;` |
|         - |  3260 | `		}` |
|         - |  3261 | `		/* Compile the expression holding the variable name */` |
|        18 |  3262 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        18 |  3263 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3264 | `			return SXERR_ABORT;` |
|        18 |  3265 | `		}else if( rc == SXERR_EMPTY ){` |
|         3 |  3266 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|         3 |  3267 | `			return SXRET_OK;` |
|         - |  3268 | `		}` |
|         8 |  3269 | `	}else{` |
|         - |  3270 | `		SyHashEntry *pEntry;` |
|         - |  3271 | `		SyString *pName;` |
|  16083947 |  3272 | `		char *zName = 0;` |
|         - |  3273 | `		/* Extract variable name */` |
|  16083947 |  3274 | `		pName = &pGen->pIn->sData;` |
|         - |  3275 | `		/* Advance the stream cursor */` |
|  16083947 |  3276 | `		pGen->pIn++;` |
|  16083947 |  3277 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  16083947 |  3278 | `		if( pEntry == 0 ){` |
|         - |  3279 | `			/* Duplicate name */` |
|    928771 |  3280 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    928771 |  3281 | `			if( zName == 0 ){` |
|       ! 0 |  3282 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3283 | `				return SXERR_ABORT;` |
|         - |  3284 | `			}` |
|         - |  3285 | `			/* Install in the hashtable */` |
|    928771 |  3286 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    464388 |  3287 | `		}else{` |
|         - |  3288 | `			/* Name already available */` |
|  15155181 |  3289 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3290 | `		}` |
|  16083947 |  3291 | `		p3 = (void *)zName;` |
|         - |  3292 | `	}` |
|  16083961 |  3293 | `	iP1 = 0;` |
|  16083961 |  3294 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   4789231 |  3295 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3296 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   4785367 |  3297 | `			iP1 = 1;` |
|   2392681 |  3298 | `		}` |
|   2394613 |  3299 | `	}` |
|         - |  3300 | `	/* Emit the load instruction */` |
|  16083961 |  3301 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  16083973 |  3302 | `	while( iVv > 0 ){` |
|        13 |  3303 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3304 | `		iVv--;` |
|         1 |  3305 | `	}` |
|         - |  3306 | `	/* Node successfully compiled */` |
|  16083961 |  3307 | `	return SXRET_OK;` |
|   8041985 |  3308 | `}` |
|         - |  3309 | `/*` |
|         - |  3310 | ` * Load a literal.` |
|         - |  3311 | ` */` |
|  10897450 |  3312 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3313 | `{` |
|  10897455 |  3314 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3315 | `	ph7_value *pObj;` |
|         - |  3316 | `	SyString *pStr;` |
|         - |  3317 | `	sxu32 nIdx;` |
|         - |  3318 | `	/* Extract token value */` |
|  10897455 |  3319 | `	pStr = &pToken->sData;` |
|         - |  3320 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  10897455 |  3321 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2104405 |  3322 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3323 | `			/* NULL constant are always indexed at 0 */` |
|    876457 |  3324 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    876457 |  3325 | `			return SXRET_OK;` |
|   1227953 |  3326 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3327 | `			/* TRUE constant are always indexed at 1 */` |
|    277601 |  3328 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    277601 |  3329 | `			return SXRET_OK;` |
|         5 |  3330 | `		}` |
|  10178386 |  3331 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1820310 |  3332 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3333 | `			/* FALSE constant are always indexed at 2 */` |
|    622729 |  3334 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    622729 |  3335 | `			return SXRET_OK;` |
|   8560989 |  3336 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    781316 |  3337 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3338 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     11531 |  3339 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     11531 |  3340 | `			if( pObj == 0 ){` |
|       ! 0 |  3341 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3342 | `				return SXERR_ABORT;` |
|         - |  3343 | `			}` |
|     11531 |  3344 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3345 | `			/* Emit the load constant instruction */` |
|     11531 |  3346 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     11531 |  3347 | `			return SXRET_OK;` |
|   8251375 |  3348 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    185140 |  3349 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3350 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         8 |  3351 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         8 |  3352 | `			if( pObj == 0 ){` |
|       ! 0 |  3353 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3354 | `				return SXERR_ABORT;` |
|         - |  3355 | `			}` |
|         8 |  3356 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3357 | `				SyString sNs;` |
|         8 |  3358 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  3359 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         5 |  3360 | `			}else{` |
|       ! 0 |  3361 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3362 | `			}` |
|         8 |  3363 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         8 |  3364 | `			return SXRET_OK;` |
|   8255410 |  3365 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    381089 |  3366 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   8346639 |  3367 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    375704 |  3368 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3369 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3370 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3371 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3372 | `				/* Point to the upper block */` |
|        11 |  3373 | `				pBlock = pBlock->pParent;` |
|         1 |  3374 | `			}` |
|        11 |  3375 | `			if( pBlock == 0 ){` |
|         - |  3376 | `				/* Called in the global scope,load NULL */` |
|         5 |  3377 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3378 | `			}else{` |
|         - |  3379 | `				/* Extract the target function/method */` |
|         7 |  3380 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3381 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|         - |  3382 | `					/* Not a class method,Load null */` |
|         3 |  3383 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         2 |  3384 | `				}else{` |
|         5 |  3385 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         5 |  3386 | `					if( pObj == 0 ){` |
|       ! 0 |  3387 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3388 | `						return SXERR_ABORT;` |
|         - |  3389 | `					}` |
|         5 |  3390 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3391 | `					/* Emit the load constant instruction */` |
|         5 |  3392 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3393 | `				}` |
|         - |  3394 | `			}` |
|        11 |  3395 | `			return SXRET_OK;` |
|         - |  3396 | `	}` |
|         - |  3397 | `	/* Query literal table */` |
|   9109141 |  3398 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3399 | `		ph7_value *pLitObj;` |
|         - |  3400 | `		/* Unknown literal,install it in the literal table */` |
|   1745881 |  3401 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1745881 |  3402 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3403 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3404 | `			return SXERR_ABORT;` |
|         - |  3405 | `		}` |
|   1745881 |  3406 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1745881 |  3407 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    872938 |  3408 | `	}` |
|         - |  3409 | `	/* Emit the load constant instruction */` |
|   9109141 |  3410 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9109141 |  3411 | `	return SXRET_OK;` |
|   5448730 |  3412 | `}` |
|         - |  3413 | `/*` |
|         - |  3414 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3415 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3416 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3417 | ` * Otherwise, load the simple literal directly.` |
|         - |  3418 | ` */` |
|  10901338 |  3419 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3420 | `{` |
|         - |  3421 | `	sxi32 rc;` |
|  10901343 |  3422 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3423 | `		return SXRET_OK;` |
|         - |  3424 | `	}` |
|         - |  3425 | `	/* Check if this is a multi-token namespace path */` |
|  10901343 |  3426 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3427 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3893 |  3428 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3893 |  3429 | `		int isAbsolute = 0;` |
|      3893 |  3430 | `		SyBlobReset(pWorker);` |
|         - |  3431 | `		/* Check for leading backslash (absolute path) */` |
|      3893 |  3432 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3891 |  3433 | `			isAbsolute = 1;` |
|      3891 |  3434 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1943 |  3435 | `		}` |
|         - |  3436 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3893 |  3437 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3438 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3439 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3440 | `		}` |
|         - |  3441 | `		/* Collect all path components */` |
|      4001 |  3442 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4001 |  3443 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        59 |  3444 | `				SyBlobAppend(pWorker,"\\",1);` |
|        32 |  3445 | `			}else{` |
|      3947 |  3446 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3447 | `			}` |
|      4001 |  3448 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3893 |  3449 | `				pGen->pIn++;` |
|      3893 |  3450 | `				break;` |
|         - |  3451 | `			}` |
|       113 |  3452 | `			pGen->pIn++;` |
|         5 |  3453 | `		}` |
|      3893 |  3454 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3455 | `			ph7_value *pObj;` |
|         - |  3456 | `			SyString sPath;` |
|         - |  3457 | `			sxu32 nIdx;` |
|      3893 |  3458 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3459 | `			/* Install in the literal table */` |
|      3893 |  3460 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3863 |  3461 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3863 |  3462 | `				if( pObj == 0 ){` |
|       ! 0 |  3463 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3464 | `					return SXERR_ABORT;` |
|         - |  3465 | `				}` |
|      3863 |  3466 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3863 |  3467 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1929 |  3468 | `			}` |
|         - |  3469 | `			/* Emit the load constant instruction.` |
|         - |  3470 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3471 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5837 |  3472 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1944 |  3473 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1944 |  3474 | `				nIdx,0,0);` |
|      3893 |  3475 | `			return SXRET_OK;` |
|         - |  3476 | `		}` |
|       ! 0 |  3477 | `	}` |
|         - |  3478 | `	/* Single-token literal: load directly */` |
|  10897455 |  3479 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  10897455 |  3480 | `	return rc;` |
|   5450674 |  3481 | `}` |
|         - |  3482 | `/*` |
|         - |  3483 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3484 | ` */` |
|         - |  3485 | `/*` |
|         - |  3486 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3487 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3488 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3489 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3490 | ` */` |
|       ! 0 |  3491 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3492 | `{` |
|       ! 0 |  3493 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3494 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3495 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3496 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3497 | `}` |
|  10901338 |  3498 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3499 | `{` |
|         - |  3500 | `	sxi32 rc;` |
|  10901343 |  3501 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  10901343 |  3502 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3503 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3504 | `		return rc;` |
|         - |  3505 | `	}` |
|         - |  3506 | `	/* Node successfully compiled */` |
|  10901343 |  3507 | `	return SXRET_OK;` |
|   5450674 |  3508 | `}` |
|         - |  3509 | `/*` |
|         - |  3510 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3511 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3512 | ` */` |
|         8 |  3513 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3514 | `{` |
|         - |  3515 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3516 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3517 | `		pGen->pIn++;` |
|         1 |  3518 | `	}` |
|         9 |  3519 | `	return SXRET_OK;` |
|         1 |  3520 | `}` |
|         - |  3521 | `/*` |
|         - |  3522 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3523 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3524 | ` */` |
|    292060 |  3525 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3526 | `{` |
|    292065 |  3527 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3889 |  3528 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3529 | `			return TRUE;` |
|      3887 |  3530 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3531 | `			return TRUE;` |
|         5 |  3532 | `		}` |
|    290120 |  3533 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7705 |  3534 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3535 | `			return TRUE;` |
|         - |  3536 | `		}` |
|      3849 |  3537 | `	}` |
|         - |  3538 | `	/* Not a reserved constant */` |
|    292057 |  3539 | `	return FALSE;` |
|    146035 |  3540 | `}` |
|         - |  3541 | `/*` |
|         - |  3542 | ` * Compile the 'const' statement.` |
|         - |  3543 | ` * According to the PHP language reference` |
|         - |  3544 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3545 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3546 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3547 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3548 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3549 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3550 | ` *  Syntax` |
|         - |  3551 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3552 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3553 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3554 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3555 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3556 | ` *  to get a list of all defined constants.` |
|         - |  3557 | ` *` |
|         - |  3558 | ` * Symisc eXtension.` |
|         - |  3559 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3560 | ` *  would allow only simple scalar value.` |
|         - |  3561 | ` *  Example` |
|         - |  3562 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3563 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3564 | ` */` |
|        48 |  3565 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3566 | `{` |
|         - |  3567 | `	SySet *pConsCode,*pInstrContainer;` |
|        53 |  3568 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3569 | `	SyString *pName;` |
|         - |  3570 | `	sxi32 rc;` |
|        53 |  3571 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        53 |  3572 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3573 | `		/* Invalid constant name */` |
|         8 |  3574 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|         8 |  3575 | `		if( rc == SXERR_ABORT ){` |
|         - |  3576 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3577 | `			return SXERR_ABORT;` |
|         - |  3578 | `		}` |
|         8 |  3579 | `		goto Synchronize;` |
|         - |  3580 | `	}` |
|         - |  3581 | `	/* Peek constant name */` |
|        47 |  3582 | `	pName = &pGen->pIn->sData;` |
|         - |  3583 | `	/* Make sure the constant name isn't reserved */` |
|        47 |  3584 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3585 | `		/* Reserved constant */` |
|        10 |  3586 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        10 |  3587 | `		if( rc == SXERR_ABORT ){` |
|         - |  3588 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3589 | `			return SXERR_ABORT;` |
|         - |  3590 | `		}` |
|        10 |  3591 | `		goto Synchronize;` |
|         - |  3592 | `	}` |
|        38 |  3593 | `	pGen->pIn++;` |
|        38 |  3594 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3595 | `		/* Invalid statement*/` |
|         6 |  3596 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|         6 |  3597 | `		if( rc == SXERR_ABORT ){` |
|         - |  3598 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3599 | `			return SXERR_ABORT;` |
|         - |  3600 | `		}` |
|         6 |  3601 | `		goto Synchronize;` |
|         - |  3602 | `	}` |
|        32 |  3603 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3604 | `	/* Allocate a new constant value container */` |
|        32 |  3605 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        32 |  3606 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3607 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3608 | `		return SXERR_ABORT;` |
|         - |  3609 | `	}` |
|        32 |  3610 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3611 | `	/* Swap bytecode container */` |
|        32 |  3612 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        32 |  3613 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3614 | `	/* Compile constant value */` |
|        32 |  3615 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3616 | `	/* Emit the done instruction */` |
|        32 |  3617 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        32 |  3618 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        32 |  3619 | `	if( rc == SXERR_ABORT ){` |
|         - |  3620 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3621 | `		return SXERR_ABORT;` |
|         - |  3622 | `	}` |
|        32 |  3623 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3624 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3625 | `	{` |
|         - |  3626 | `		SyBlob sFQN;` |
|         - |  3627 | `		SyString sFQNStr;` |
|        32 |  3628 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        32 |  3629 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        32 |  3630 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        47 |  3631 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        30 |  3632 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        32 |  3633 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3634 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3635 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3636 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3637 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3638 | `			if( pCEntry ){` |
|         5 |  3639 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3640 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3641 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3642 | `					return SXERR_ABORT;` |
|         - |  3643 | `				}` |
|         2 |  3644 | `			}` |
|         2 |  3645 | `		}` |
|        32 |  3646 | `		SyBlobRelease(&sFQN);` |
|         - |  3647 | `	}` |
|        32 |  3648 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3649 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3650 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3651 | `	}` |
|        32 |  3652 | `	return SXRET_OK;` |
|         9 |  3653 | `Synchronize:` |
|         - |  3654 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3655 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        42 |  3656 | `		pGen->pIn++;` |
|         4 |  3657 | `	}` |
|        22 |  3658 | `	return SXRET_OK;` |
|        29 |  3659 | `}` |
|         - |  3660 | `/*` |
|         - |  3661 | ` * Compile the 'continue' statement.` |
|         - |  3662 | ` * According to the PHP language reference` |
|         - |  3663 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3664 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3665 | ` *  iteration.` |
|         - |  3666 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3667 | ` *  the purposes of continue.` |
|         - |  3668 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3669 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3670 | ` *  Note:` |
|         - |  3671 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3672 | ` */` |
|         - |  3673 | `/*` |
|         - |  3674 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3675 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3676 | ` * break/continue crosses a try boundary.` |
|         - |  3677 | ` *` |
|         - |  3678 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3679 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3680 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3681 | ` */` |
|    115354 |  3682 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3683 | `{` |
|    115359 |  3684 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    115359 |  3685 | `	int nInlineTry = 0;` |
|    538005 |  3686 | `	while( pBlock && pBlock != pTarget ){` |
|    422651 |  3687 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3688 | `			if( pBlock->pUserData ){` |
|         - |  3689 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3690 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3691 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3692 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3693 | `				if( pGen->bInGenerator ){` |
|         3 |  3694 | `					nInlineTry++;` |
|         2 |  3695 | `				}else{` |
|         3 |  3696 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3697 | `				}` |
|         4 |  3698 | `			}else{` |
|         - |  3699 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3700 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3701 | `				break;` |
|         - |  3702 | `			}` |
|         2 |  3703 | `		}` |
|    422651 |  3704 | `		pBlock = pBlock->pParent;` |
|         5 |  3705 | `	}` |
|    115359 |  3706 | `	return nInlineTry;` |
|         5 |  3707 | `}` |
|     57650 |  3708 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3709 | `{` |
|         - |  3710 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3711 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3712 | `	sxu32 nLineLocal;` |
|         - |  3713 | `	sxi32 rc;` |
|     57655 |  3714 | `	nLineLocal = pGen->pIn->nLine;` |
|     57655 |  3715 | `	iLevel = 0;` |
|         - |  3716 | `	/* Jump the 'continue' keyword */` |
|     57655 |  3717 | `	pGen->pIn++;` |
|     57655 |  3718 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3719 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3720 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3721 | `		 */` |
|         - |  3722 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3723 | `		char *zAlloc = 0;` |
|         - |  3724 | `		SyString sNum;` |
|        17 |  3725 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3726 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3727 | `			return SXERR_ABORT;` |
|         - |  3728 | `		}` |
|        17 |  3729 | `		if( rc == SXRET_OK ){` |
|        20 |  3730 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3731 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3732 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3733 | `				return SXERR_ABORT;` |
|         - |  3734 | `			}` |
|        14 |  3735 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3736 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3737 | `		}` |
|        17 |  3738 | `		if( iLevel < 2 ){` |
|         3 |  3739 | `			iLevel = 0;` |
|         1 |  3740 | `		}` |
|        17 |  3741 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3742 | `	}` |
|         - |  3743 | `	/* Point to the target loop */` |
|     57655 |  3744 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     57655 |  3745 | `	if( pLoop == 0 ){` |
|         - |  3746 | `		/* Illegal continue */` |
|        12 |  3747 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        12 |  3748 | `		if( rc == SXERR_ABORT ){` |
|         - |  3749 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3750 | `			return SXERR_ABORT;` |
|         - |  3751 | `		}` |
|         7 |  3752 | `	}else{` |
|     57645 |  3753 | `		sxu32 nInstrIdx = 0;` |
|         - |  3754 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     57645 |  3755 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3756 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3757 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     57645 |  3758 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     57645 |  3759 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3760 | `			/* According to the PHP language reference manual` |
|         - |  3761 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|         - |  3762 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|         - |  3763 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|         - |  3764 | `			 */` |
|         5 |  3765 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3766 | `			if( rc == SXRET_OK ){` |
|         5 |  3767 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3768 | `			}` |
|         3 |  3769 | `		}else{` |
|         - |  3770 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     57641 |  3771 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     57641 |  3772 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  3773 | `				JumpFixup sJumpFix;` |
|         - |  3774 | `				/* Post-continue */` |
|     19217 |  3775 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     19217 |  3776 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     19217 |  3777 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|      9606 |  3778 | `			}` |
|         - |  3779 | `		}` |
|         - |  3780 | `	}` |
|     57655 |  3781 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3782 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3783 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  3784 | `	}` |
|         - |  3785 | `	/* Statement successfully compiled */` |
|     57655 |  3786 | `	return SXRET_OK;` |
|     28830 |  3787 | `}` |
|         - |  3788 | `/*` |
|         - |  3789 | ` * Compile the 'break' statement.` |
|         - |  3790 | ` * According to the PHP language reference` |
|         - |  3791 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  3792 | ` *  structure.` |
|         - |  3793 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  3794 | ` *  enclosing structures are to be broken out of.` |
|         - |  3795 | ` */` |
|     57730 |  3796 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  3797 | `{` |
|         - |  3798 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3799 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3800 | `	sxi32 rc;` |
|     57735 |  3801 | `	iLevel = 0;` |
|         - |  3802 | `	/* Jump the 'break' keyword */` |
|     57735 |  3803 | `	pGen->pIn++;` |
|     57735 |  3804 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3805 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3806 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3807 | `		 */` |
|         - |  3808 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3809 | `		char *zAlloc = 0;` |
|         - |  3810 | `		SyString sNum;` |
|        17 |  3811 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3812 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3813 | `			return SXERR_ABORT;` |
|         - |  3814 | `		}` |
|        17 |  3815 | `		if( rc == SXRET_OK ){` |
|        21 |  3816 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3817 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  3818 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3819 | `				return SXERR_ABORT;` |
|         - |  3820 | `			}` |
|        15 |  3821 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  3822 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3823 | `		}` |
|        17 |  3824 | `		if( iLevel < 2 ){` |
|         3 |  3825 | `			iLevel = 0;` |
|         1 |  3826 | `		}` |
|        17 |  3827 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3828 | `	}` |
|         - |  3829 | `	/* Extract the target loop */` |
|     57735 |  3830 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     57735 |  3831 | `	if( pLoop == 0 ){` |
|         - |  3832 | `		/* Illegal break */` |
|        19 |  3833 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        19 |  3834 | `		if( rc == SXERR_ABORT ){` |
|         - |  3835 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3836 | `			return SXERR_ABORT;` |
|         - |  3837 | `		}` |
|        11 |  3838 | `	}else{` |
|         - |  3839 | `		sxu32 nInstrIdx;` |
|         - |  3840 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     57719 |  3841 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3842 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     57719 |  3843 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     57719 |  3844 | `		if( rc == SXRET_OK ){` |
|         - |  3845 | `			/* Fix the jump later when the jump destination is resolved */` |
|     57719 |  3846 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     28857 |  3847 | `		}` |
|         - |  3848 | `	}` |
|     57735 |  3849 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3850 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3851 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  3852 | `	}` |
|         - |  3853 | `	/* Statement successfully compiled */` |
|     57735 |  3854 | `	return SXRET_OK;` |
|     28870 |  3855 | `}` |
|         - |  3856 | `/*` |
|         - |  3857 | ` * Compile or record a label.` |
|         - |  3858 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  3859 | ` * Example` |
|         - |  3860 | ` *  goto LABEL;` |
|         - |  3861 | ` *   echo 'Foo';` |
|         - |  3862 | ` *  LABEL:` |
|         - |  3863 | ` *   echo 'Bar';` |
|         - |  3864 | ` */` |
|       112 |  3865 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  3866 | `{` |
|         - |  3867 | `	GenBlock *pBlock;` |
|         - |  3868 | `	Label sLabel;` |
|         - |  3869 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|         - |  3870 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|         - |  3871 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|         - |  3872 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|         - |  3873 | `	{` |
|       117 |  3874 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3875 | `		char *zDup;` |
|         - |  3876 | `		/* Initialize label fields */` |
|       117 |  3877 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  3878 | `		/* Duplicate label name */` |
|       117 |  3879 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       117 |  3880 | `		if( zDup == 0 ){` |
|       ! 0 |  3881 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3882 | `			return SXERR_ABORT;` |
|         - |  3883 | `		}` |
|       117 |  3884 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       117 |  3885 | `		sLabel.bRef  = FALSE;` |
|       117 |  3886 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       117 |  3887 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       117 |  3888 | `		pBlock = pGen->pCurrent;` |
|       233 |  3889 | `		while( pBlock ){` |
|       143 |  3890 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        26 |  3891 | `				break;` |
|         - |  3892 | `			}` |
|         - |  3893 | `			/* Point to the upper block */` |
|       121 |  3894 | `			pBlock = pBlock->pParent;` |
|         5 |  3895 | `		}` |
|       117 |  3896 | `		if( pBlock ){` |
|        26 |  3897 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        15 |  3898 | `		}else{` |
|        95 |  3899 | `			sLabel.pFunc = 0;` |
|         - |  3900 | `		}` |
|         - |  3901 | `		/* Insert in label set */` |
|       117 |  3902 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  3903 | `	}` |
|       117 |  3904 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  3905 | `	return SXRET_OK;` |
|        61 |  3906 | `}` |
|         - |  3907 | `/*` |
|         - |  3908 | ` * Compile the so hated 'goto' statement.` |
|         - |  3909 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  3910 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  3911 | ` * a compiler it has to do this.` |
|         - |  3912 | ` * According to the PHP language reference manual` |
|         - |  3913 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  3914 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  3915 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  3916 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  3917 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  3918 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  3919 | ` *   of a multi-level break` |
|         - |  3920 | ` */` |
|       152 |  3921 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  3922 | `{` |
|         - |  3923 | `	JumpFixup sJump;` |
|         - |  3924 | `	sxi32 rc;` |
|       157 |  3925 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  3926 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3927 | `		/* Missing label */` |
|       ! 0 |  3928 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  3929 | `		if( rc == SXERR_ABORT ){` |
|         - |  3930 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3931 | `			return SXERR_ABORT;` |
|         - |  3932 | `		}` |
|       ! 0 |  3933 | `		return SXRET_OK;` |
|         - |  3934 | `	}` |
|       157 |  3935 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         5 |  3936 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|         5 |  3937 | `		if( rc == SXERR_ABORT ){` |
|         - |  3938 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3939 | `			return SXERR_ABORT;` |
|         - |  3940 | `		}` |
|         3 |  3941 | `	}else{` |
|       153 |  3942 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3943 | `		GenBlock *pBlock;` |
|         - |  3944 | `		char *zDup;` |
|         - |  3945 | `		/* Prepare the jump destination */` |
|       153 |  3946 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  3947 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  3948 | `		/* Duplicate label name */` |
|       153 |  3949 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  3950 | `		if( zDup == 0 ){` |
|       ! 0 |  3951 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3952 | `			return SXERR_ABORT;` |
|         - |  3953 | `		}` |
|       153 |  3954 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|         - |  3955 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|       153 |  3956 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|         - |  3957 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|         - |  3958 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|       153 |  3959 | `		pBlock = pGen->pCurrent;` |
|       327 |  3960 | `		while( pBlock ){` |
|       205 |  3961 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|        29 |  3962 | `				break;` |
|         - |  3963 | `			}` |
|         - |  3964 | `			/* Point to the upper block */` |
|       179 |  3965 | `			pBlock = pBlock->pParent;` |
|         5 |  3966 | `		}` |
|       153 |  3967 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        29 |  3968 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        16 |  3969 | `		}else{` |
|       127 |  3970 | `			sJump.pFunc = 0;` |
|         - |  3971 | `		}` |
|         - |  3972 | `		/* Emit the unconditional jump */` |
|       153 |  3973 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  3974 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  3975 | `		}` |
|         - |  3976 | `	}` |
|       157 |  3977 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  3978 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  3979 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  3980 | `	}` |
|         - |  3981 | `	/* Statement successfully compiled */` |
|       157 |  3982 | `	return SXRET_OK;` |
|        81 |  3983 | `}` |
|         - |  3984 | `/*` |
|         - |  3985 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  3986 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  3987 | ` * failure.` |
|         - |  3988 | ` */` |
|        20 |  3989 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         2 |  3990 | `{` |
|         - |  3991 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  3992 | `	sxu32 nRawObj;` |
|        10 |  3993 | `	sxu32 nObjIdx;` |
|         - |  3994 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  3995 | `	 * a PHP block.` |
|         - |  3996 | `	 */` |
|        10 |  3997 | `Consume:` |
|        22 |  3998 | `	nRawObj = nObjIdx = 0;` |
|        22 |  3999 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  4000 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  4001 | `		if( pRawObj == 0 ){` |
|       ! 0 |  4002 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4003 | `			return SXERR_ABORT;` |
|         - |  4004 | `		}` |
|         - |  4005 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  4006 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  4007 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  4008 | `		++nRawObj;` |
|       ! 0 |  4009 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  4010 | `	}` |
|        22 |  4011 | `	if( nRawObj > 0 ){` |
|         - |  4012 | `		/* Emit the consume instruction */` |
|       ! 0 |  4013 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4014 | `	}` |
|        22 |  4015 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  4016 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  4017 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  4018 | `		SySetReset(pTokenSet);` |
|       ! 0 |  4019 | `		SySetReset(&pGen->aTrivia);` |
|         - |  4020 | `		/* Tokenize input */` |
|       ! 0 |  4021 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  4022 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  4023 | `		/* Point to the fresh token stream */` |
|       ! 0 |  4024 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  4025 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  4026 | `		/* Advance the stream cursor */` |
|       ! 0 |  4027 | `		pGen->pRawIn++;` |
|         - |  4028 | `		/* TICKET 1433-011 */` |
|       ! 0 |  4029 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4030 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4031 | `			sxi32 rc;` |
|         - |  4032 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4033 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4034 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4035 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4036 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4037 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4038 | `				return SXERR_ABORT;` |
|       ! 0 |  4039 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4040 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4041 | `			}` |
|       ! 0 |  4042 | `			goto Consume;` |
|         - |  4043 | `		}` |
|       ! 0 |  4044 | `	}else{` |
|         - |  4045 | `		/* No more chunks to process */` |
|        22 |  4046 | `		pGen->pIn = pGen->pEnd;` |
|        22 |  4047 | `		return SXERR_EOF;` |
|         - |  4048 | `	}` |
|       ! 0 |  4049 | `	return SXRET_OK;` |
|        12 |  4050 | `}` |
|         - |  4051 | `/*` |
|         - |  4052 | ` * Compile a PHP block.` |
|         - |  4053 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4054 | ` * optionally delimited by braces {}.` |
|         - |  4055 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4056 | ` * and this function takes care of generating the appropriate error` |
|         - |  4057 | ` * message.` |
|         - |  4058 | ` */` |
|   5229542 |  4059 | `static sxi32 PH7_CompileBlock(` |
|         - |  4060 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4061 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4062 | `	)` |
|         5 |  4063 | `{` |
|         - |  4064 | `	sxi32 rc;` |
|         - |  4065 | `	sxu32 nLine;` |
|   5229547 |  4066 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5228333 |  4067 | `		nLine = pGen->pIn->nLine;` |
|   5228333 |  4068 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5228333 |  4069 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4070 | `			return SXERR_ABORT;` |
|         - |  4071 | `		}` |
|   5228333 |  4072 | `		pGen->pIn++;` |
|         - |  4073 | `		/* Compile until we hit the closing braces '}' */` |
|   7648563 |  4074 | `		for(;;){` |
|  15297131 |  4075 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        22 |  4076 | `				rc = GenStateNextChunk(&(*pGen));` |
|        22 |  4077 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4078 | `			 	   return SXERR_ABORT;` |
|         - |  4079 | `				}` |
|        22 |  4080 | `				if( rc == SXERR_EOF ){` |
|         - |  4081 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4082 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        22 |  4083 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        22 |  4084 | `					break;` |
|         - |  4085 | `				}` |
|       ! 0 |  4086 | `			}` |
|  15297111 |  4087 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4088 | `				/* Closing braces found,break immediately*/` |
|   5228313 |  4089 | `				pGen->pIn++;` |
|   5228313 |  4090 | `				break;` |
|         - |  4091 | `			}` |
|         - |  4092 | `			/* Compile a single statement */` |
|  10068803 |  4093 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  10068803 |  4094 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4095 | `				return SXERR_ABORT;` |
|         - |  4096 | `			}` |
|         5 |  4097 | `		}` |
|   5228333 |  4098 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   2615383 |  4099 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4100 | `		pGen->pIn++;` |
|       ! 0 |  4101 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4102 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4103 | `			return SXERR_ABORT;` |
|         - |  4104 | `		}` |
|         - |  4105 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4106 | `		for(;;){` |
|       ! 0 |  4107 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4108 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4109 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4110 | `			 	   return SXERR_ABORT;` |
|         - |  4111 | `				}` |
|       ! 0 |  4112 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4113 | `					/* No more token to process */` |
|       ! 0 |  4114 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4115 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4116 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4117 | `					}` |
|       ! 0 |  4118 | `					break;` |
|         - |  4119 | `				}` |
|       ! 0 |  4120 | `			}` |
|       ! 0 |  4121 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4122 | `				sxi32 nKwrd;` |
|         - |  4123 | `				/* Keyword found */` |
|       ! 0 |  4124 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4125 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4126 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4127 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4128 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4129 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4130 | `						}` |
|       ! 0 |  4131 | `						break;` |
|         - |  4132 | `				}` |
|       ! 0 |  4133 | `			}` |
|         - |  4134 | `			/* Compile a single statement */` |
|       ! 0 |  4135 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4136 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4137 | `				return SXERR_ABORT;` |
|         - |  4138 | `			}` |
|       ! 0 |  4139 | `		}` |
|       ! 0 |  4140 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4141 | `	}else{` |
|         - |  4142 | `		/* Compile a single statement */` |
|      1219 |  4143 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      1219 |  4144 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4145 | `			return SXERR_ABORT;` |
|         - |  4146 | `		}` |
|         - |  4147 | `	}` |
|         - |  4148 | `	/* Jump trailing semi-colons ';' */` |
|   5229547 |  4149 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4150 | `		pGen->pIn++;` |
|       ! 0 |  4151 | `	}` |
|   5229547 |  4152 | `	return SXRET_OK;` |
|   2614776 |  4153 | `}` |
|         - |  4154 | `/*` |
|         - |  4155 | ` * Compile the gentle 'while' statement.` |
|         - |  4156 | ` * According to the PHP language reference` |
|         - |  4157 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4158 | ` *  The basic form of a while statement is:` |
|         - |  4159 | ` *  while (expr)` |
|         - |  4160 | ` *   statement` |
|         - |  4161 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4162 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4163 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4164 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4165 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4166 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4167 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4168 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4169 | ` *  while (expr):` |
|         - |  4170 | ` *    statement` |
|         - |  4171 | ` *   endwhile;` |
|         - |  4172 | ` */` |
|     53902 |  4173 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4174 | `{` |
|     53907 |  4175 | `	GenBlock *pWhileBlock = 0;` |
|     53907 |  4176 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4177 | `	sxu32 nFalseJump;` |
|         - |  4178 | `	sxu32 nLine;` |
|         - |  4179 | `	sxi32 rc;` |
|     53907 |  4180 | `	nLine = pGen->pIn->nLine;` |
|         - |  4181 | `	/* Jump the 'while' keyword */` |
|     53907 |  4182 | `	pGen->pIn++;` |
|     53907 |  4183 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4184 | `		/* Syntax error */` |
|       ! 0 |  4185 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4186 | `		if( rc == SXERR_ABORT ){` |
|         - |  4187 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4188 | `			return SXERR_ABORT;` |
|         - |  4189 | `		}` |
|       ! 0 |  4190 | `		goto Synchronize;` |
|         - |  4191 | `	}` |
|         - |  4192 | `	/* Jump the left parenthesis '(' */` |
|     53907 |  4193 | `	pGen->pIn++;` |
|         - |  4194 | `	/* Create the loop block */` |
|     53907 |  4195 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     53907 |  4196 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4197 | `		return SXERR_ABORT;` |
|         - |  4198 | `	}` |
|         - |  4199 | `	/* Delimit the condition */` |
|     53907 |  4200 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     53907 |  4201 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4202 | `		/* Empty expression */` |
|         3 |  4203 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4204 | `		if( rc == SXERR_ABORT ){` |
|         - |  4205 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4206 | `			return SXERR_ABORT;` |
|         - |  4207 | `		}` |
|         1 |  4208 | `	}` |
|         - |  4209 | `	/* Swap token streams */` |
|     53907 |  4210 | `	pTmp = pGen->pEnd;` |
|     53907 |  4211 | `	pGen->pEnd = pEnd;` |
|         - |  4212 | `	/* Compile the expression */` |
|     53907 |  4213 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     53907 |  4214 | `	if( rc == SXERR_ABORT ){` |
|         - |  4215 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4216 | `		return SXERR_ABORT;` |
|         - |  4217 | `	}` |
|         - |  4218 | `	/* Update token stream */` |
|     53907 |  4219 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4220 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4221 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4222 | `			return SXERR_ABORT;` |
|         - |  4223 | `		}` |
|       ! 0 |  4224 | `		pGen->pIn++;` |
|       ! 0 |  4225 | `	}` |
|         - |  4226 | `	/* Synchronize pointers */` |
|     53907 |  4227 | `	pGen->pIn  = &pEnd[1];` |
|     53907 |  4228 | `	pGen->pEnd = pTmp;` |
|         - |  4229 | `	/* Emit the false jump */` |
|     53907 |  4230 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4231 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     53907 |  4232 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4233 | `	/* Compile the loop body */` |
|     53907 |  4234 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     53907 |  4235 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4236 | `		return SXERR_ABORT;` |
|         - |  4237 | `	}` |
|         - |  4238 | `	/* Emit the unconditional jump to the start of the loop */` |
|     53907 |  4239 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4240 | `	/* Fix all jumps now the destination is resolved */` |
|     53907 |  4241 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4242 | `	/* Release the loop block */` |
|     53907 |  4243 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4244 | `	/* Statement successfully compiled */` |
|     53907 |  4245 | `	return SXRET_OK;` |
|       ! 0 |  4246 | `Synchronize:` |
|         - |  4247 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4248 | `	 * compiling this erroneous block.` |
|         - |  4249 | `	 */` |
|       ! 0 |  4250 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4251 | `		pGen->pIn++;` |
|       ! 0 |  4252 | `	}` |
|       ! 0 |  4253 | `	return SXRET_OK;` |
|     26956 |  4254 | `}` |
|         - |  4255 | `/*` |
|         - |  4256 | ` * Compile the ugly do..while() statement.` |
|         - |  4257 | ` * According to the PHP language reference` |
|         - |  4258 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4259 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4260 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4261 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4262 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4263 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4264 | ` *  would end immediately).` |
|         - |  4265 | ` *  There is just one syntax for do-while loops:` |
|         - |  4266 | ` *  <?php` |
|         - |  4267 | ` *  $i = 0;` |
|         - |  4268 | ` *  do {` |
|         - |  4269 | ` *   echo $i;` |
|         - |  4270 | ` *  } while ($i > 0);` |
|         - |  4271 | ` * ?>` |
|         - |  4272 | ` */` |
|         2 |  4273 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4274 | `{` |
|         3 |  4275 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4276 | `	GenBlock *pDoBlock = 0;` |
|         - |  4277 | `	sxu32 nLine;` |
|         - |  4278 | `	sxi32 rc;` |
|         3 |  4279 | `	nLine = pGen->pIn->nLine;` |
|         - |  4280 | `	/* Jump the 'do' keyword */` |
|         3 |  4281 | `	pGen->pIn++;` |
|         - |  4282 | `	/* Create the loop block */` |
|         3 |  4283 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4284 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4285 | `		return SXERR_ABORT;` |
|         - |  4286 | `	}` |
|         - |  4287 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4288 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4289 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4290 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4291 | `		return SXERR_ABORT;` |
|         - |  4292 | `	}` |
|         3 |  4293 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4294 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4295 | `	}` |
|         3 |  4296 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4297 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4298 | `			/* Missing 'while' statement */` |
|         3 |  4299 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4300 | `			if( rc == SXERR_ABORT ){` |
|         - |  4301 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4302 | `				return SXERR_ABORT;` |
|         - |  4303 | `			}` |
|         3 |  4304 | `			goto Synchronize;` |
|         - |  4305 | `	}` |
|         - |  4306 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4307 | `	pGen->pIn++;` |
|       ! 0 |  4308 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4309 | `		/* Syntax error */` |
|       ! 0 |  4310 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4311 | `		if( rc == SXERR_ABORT ){` |
|         - |  4312 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4313 | `			return SXERR_ABORT;` |
|         - |  4314 | `		}` |
|       ! 0 |  4315 | `		goto Synchronize;` |
|         - |  4316 | `	}` |
|         - |  4317 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4318 | `	pGen->pIn++;` |
|         - |  4319 | `	/* Delimit the condition */` |
|       ! 0 |  4320 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4321 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4322 | `		/* Empty expression */` |
|       ! 0 |  4323 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4324 | `		if( rc == SXERR_ABORT ){` |
|         - |  4325 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4326 | `			return SXERR_ABORT;` |
|         - |  4327 | `		}` |
|       ! 0 |  4328 | `		goto Synchronize;` |
|         - |  4329 | `	}` |
|         - |  4330 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4331 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4332 | `		JumpFixup *aPost;` |
|         - |  4333 | `		VmInstr *pInstr;` |
|         - |  4334 | `		sxu32 nJumpDest;` |
|         - |  4335 | `		sxu32 n;` |
|       ! 0 |  4336 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4337 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4338 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4339 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4340 | `			if( pInstr ){` |
|         - |  4341 | `				/* Fix */` |
|       ! 0 |  4342 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4343 | `			}` |
|       ! 0 |  4344 | `		}` |
|       ! 0 |  4345 | `	}` |
|         - |  4346 | `	/* Swap token streams */` |
|       ! 0 |  4347 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4348 | `	pGen->pEnd = pEnd;` |
|         - |  4349 | `	/* Compile the expression */` |
|       ! 0 |  4350 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4351 | `	if( rc == SXERR_ABORT ){` |
|         - |  4352 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4353 | `		return SXERR_ABORT;` |
|         - |  4354 | `	}` |
|         - |  4355 | `	/* Update token stream */` |
|       ! 0 |  4356 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4357 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4358 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4359 | `			return SXERR_ABORT;` |
|         - |  4360 | `		}` |
|       ! 0 |  4361 | `		pGen->pIn++;` |
|       ! 0 |  4362 | `	}` |
|       ! 0 |  4363 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4364 | `	pGen->pEnd = pTmp;` |
|         - |  4365 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4366 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4367 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4368 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4369 | `	/* Release the loop block */` |
|       ! 0 |  4370 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4371 | `	/* Statement successfully compiled */` |
|       ! 0 |  4372 | `	return SXRET_OK;` |
|         1 |  4373 | `Synchronize:` |
|         - |  4374 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4375 | `	 * compiling this erroneous block.` |
|         - |  4376 | `	 */` |
|         3 |  4377 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4378 | `		pGen->pIn++;` |
|       ! 0 |  4379 | `	}` |
|         3 |  4380 | `	return SXRET_OK;` |
|         2 |  4381 | `}` |
|         - |  4382 | `/*` |
|         - |  4383 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4384 | ` * According to the PHP language reference` |
|         - |  4385 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4386 | ` *  The syntax of a for loop is:` |
|         - |  4387 | ` *  for (expr1; expr2; expr3)` |
|         - |  4388 | ` *   statement` |
|         - |  4389 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4390 | ` *  the beginning of the loop.` |
|         - |  4391 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4392 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4393 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4394 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4395 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4396 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4397 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4398 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4399 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4400 | ` *  of using the for truth expression.` |
|         - |  4401 | ` */` |
|     88464 |  4402 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4403 | `{` |
|     88469 |  4404 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|     88469 |  4405 | `	GenBlock *pForBlock = 0;` |
|         - |  4406 | `	sxu32 nFalseJump;` |
|         - |  4407 | `	sxu32 nLine;` |
|         - |  4408 | `	sxi32 rc;` |
|     88469 |  4409 | `	nLine = pGen->pIn->nLine;` |
|         - |  4410 | `	/* Jump the 'for' keyword */` |
|     88469 |  4411 | `	pGen->pIn++;` |
|     88469 |  4412 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4413 | `		/* Syntax error */` |
|       ! 0 |  4414 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4415 | `		if( rc == SXERR_ABORT ){` |
|         - |  4416 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4417 | `			return SXERR_ABORT;` |
|         - |  4418 | `		}` |
|       ! 0 |  4419 | `		return SXRET_OK;` |
|         - |  4420 | `	}` |
|         - |  4421 | `	/* Jump the left parenthesis '(' */` |
|     88469 |  4422 | `	pGen->pIn++;` |
|         - |  4423 | `	/* Delimit the init-expr;condition;post-expr */` |
|     88469 |  4424 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     88469 |  4425 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4426 | `		/* Empty expression */` |
|       ! 0 |  4427 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4428 | `		if( rc == SXERR_ABORT ){` |
|         - |  4429 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4430 | `			return SXERR_ABORT;` |
|         - |  4431 | `		}` |
|         - |  4432 | `		/* Synchronize */` |
|       ! 0 |  4433 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4434 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4435 | `			pGen->pIn++;` |
|       ! 0 |  4436 | `		}` |
|       ! 0 |  4437 | `		return SXRET_OK;` |
|         - |  4438 | `	}` |
|         - |  4439 | `	/* Swap token streams */` |
|     88469 |  4440 | `	pTmp = pGen->pEnd;` |
|     88469 |  4441 | `	pGen->pEnd = pEnd;` |
|         - |  4442 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4443 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4444 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4445 | `	 * compiled through this same window — recorded as a known leniency. */` |
|     88469 |  4446 | `	pGen->nCommaExprOk++;` |
|         - |  4447 | `	/* Compile initialization expressions if available */` |
|     88469 |  4448 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4449 | `	/* Pop operand lvalues */` |
|     88469 |  4450 | `	if( rc == SXERR_ABORT ){` |
|         - |  4451 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4452 | `		return SXERR_ABORT;` |
|     88469 |  4453 | `	}else if( rc != SXERR_EMPTY ){` |
|     76947 |  4454 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     38471 |  4455 | `	}` |
|     88469 |  4456 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4457 | `		/* Syntax error */` |
|       ! 0 |  4458 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4459 | `		if( rc == SXERR_ABORT ){` |
|         - |  4460 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4461 | `			return SXERR_ABORT;` |
|         - |  4462 | `		}` |
|       ! 0 |  4463 | `		return SXRET_OK;` |
|         - |  4464 | `	}` |
|         - |  4465 | `	/* Jump the trailing ';' */` |
|     88469 |  4466 | `	pGen->pIn++;` |
|         - |  4467 | `	/* Create the loop block */` |
|     88469 |  4468 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|     88469 |  4469 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4470 | `		return SXERR_ABORT;` |
|         - |  4471 | `	}` |
|         - |  4472 | `	/* Deffer continue jumps */` |
|     88469 |  4473 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4474 | `	/* Compile the condition */` |
|     88469 |  4475 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     88469 |  4476 | `	if( rc == SXERR_ABORT ){` |
|         - |  4477 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4478 | `		return SXERR_ABORT;` |
|     88469 |  4479 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4480 | `		/* Emit the false jump */` |
|     76947 |  4481 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4482 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     76947 |  4483 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     38471 |  4484 | `	}` |
|     88469 |  4485 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4486 | `		/* Syntax error */` |
|         6 |  4487 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4488 | `		if( rc == SXERR_ABORT ){` |
|         - |  4489 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4490 | `			return SXERR_ABORT;` |
|         - |  4491 | `		}` |
|         6 |  4492 | `		return SXRET_OK;` |
|         - |  4493 | `	}` |
|         - |  4494 | `	/* Jump the trailing ';' */` |
|     88465 |  4495 | `	pGen->pIn++;` |
|         - |  4496 | `	/* Save the post condition stream */` |
|     88465 |  4497 | `	pPostStart = pGen->pIn;` |
|         - |  4498 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4499 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|     88465 |  4500 | `	pGen->nCommaExprOk--;` |
|     88465 |  4501 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|     88465 |  4502 | `	pGen->pEnd = pTmp;` |
|     88465 |  4503 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|     88465 |  4504 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4505 | `		return SXERR_ABORT;` |
|         - |  4506 | `	}` |
|         - |  4507 | `	/* Fix post-continue jumps */` |
|     88465 |  4508 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4509 | `		JumpFixup *aPost;` |
|         - |  4510 | `		VmInstr *pInstr;` |
|         - |  4511 | `		sxu32 nJumpDest;` |
|         - |  4512 | `		sxu32 n;` |
|      7697 |  4513 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      7697 |  4514 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     26909 |  4515 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     19217 |  4516 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     19217 |  4517 | `			if( pInstr ){` |
|         - |  4518 | `				/* Fix jump */` |
|     19217 |  4519 | `				pInstr->iP2 = nJumpDest;` |
|      9606 |  4520 | `			}` |
|      9611 |  4521 | `		}` |
|      3846 |  4522 | `	}` |
|         - |  4523 | `	/* compile the post-expressions if available */` |
|     88465 |  4524 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4525 | `		pPostStart++;` |
|       ! 0 |  4526 | `	}` |
|     88465 |  4527 | `	if( pPostStart < pEnd ){` |
|         - |  4528 | `		SyToken *pTmpIn,*pTmpEnd;` |
|     76945 |  4529 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|     76945 |  4530 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|     76945 |  4531 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     76945 |  4532 | `		pGen->nCommaExprOk--;` |
|     76945 |  4533 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4534 | `			/* Syntax error */` |
|       ! 0 |  4535 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4536 | `			if( rc == SXERR_ABORT ){` |
|         - |  4537 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4538 | `				return SXERR_ABORT;` |
|         - |  4539 | `			}` |
|       ! 0 |  4540 | `			return SXRET_OK;` |
|         - |  4541 | `		}` |
|     76945 |  4542 | `		RE_SWAP_DELIMITER(pGen);` |
|     76945 |  4543 | `		if( rc == SXERR_ABORT ){` |
|         - |  4544 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4545 | `			return SXERR_ABORT;` |
|     76945 |  4546 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4547 | `			/* Pop operand lvalue */` |
|     76945 |  4548 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     38470 |  4549 | `		}` |
|     38470 |  4550 | `	}` |
|         - |  4551 | `	/* Emit the unconditional jump to the start of the loop */` |
|     88465 |  4552 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4553 | `	/* Fix all jumps now the destination is resolved */` |
|     88465 |  4554 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4555 | `	/* Release the loop block */` |
|     88465 |  4556 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4557 | `	/* Statement successfully compiled */` |
|     88465 |  4558 | `	return SXRET_OK;` |
|     44237 |  4559 | `}` |
|         - |  4560 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4561 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4562 | ` * are allowed.` |
|         - |  4563 | ` */` |
|    323548 |  4564 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4565 | `{` |
|    323553 |  4566 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    323553 |  4567 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4568 | `		/* Unexpected expression */` |
|       ! 0 |  4569 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4570 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4571 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4572 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4573 | `		}` |
|       ! 0 |  4574 | `	}` |
|    323553 |  4575 | `	return rc;` |
|         5 |  4576 | `}` |
|         - |  4577 | `/*` |
|         - |  4578 | ` * Compile the 'foreach' statement.` |
|         - |  4579 | ` * According to the PHP language reference` |
|         - |  4580 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4581 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4582 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4583 | ` *  is a minor but useful extension of the first:` |
|         - |  4584 | ` *  foreach (array_expression as $value)` |
|         - |  4585 | ` *    statement` |
|         - |  4586 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4587 | ` *   statement` |
|         - |  4588 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4589 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4590 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4591 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4592 | ` *  to the variable $key on each loop.` |
|         - |  4593 | ` *  Note:` |
|         - |  4594 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4595 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4596 | ` *  Note:` |
|         - |  4597 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4598 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4599 | ` *  or after the foreach without resetting it.` |
|         - |  4600 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4601 | ` *  of copying the value.` |
|         - |  4602 | ` */` |
|    223434 |  4603 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4604 | `{` |
|    223439 |  4605 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    223439 |  4606 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    223439 |  4607 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4608 | `	ph7_foreach_info *pInfo;` |
|         - |  4609 | `	sxu32 nFalseJump;` |
|         - |  4610 | `	VmInstr *pInstr;` |
|         - |  4611 | `	sxu32 nLine;` |
|         - |  4612 | `	sxi32 rc;` |
|    223439 |  4613 | `	nLine = pGen->pIn->nLine;` |
|         - |  4614 | `	/* Jump the 'foreach' keyword */` |
|    223439 |  4615 | `	pGen->pIn++;` |
|    223439 |  4616 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4617 | `		/* Syntax error */` |
|       ! 0 |  4618 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4619 | `		if( rc == SXERR_ABORT ){` |
|         - |  4620 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4621 | `			return SXERR_ABORT;` |
|         - |  4622 | `		}` |
|       ! 0 |  4623 | `		goto Synchronize;` |
|         - |  4624 | `	}` |
|         - |  4625 | `	/* Jump the left parenthesis '(' */` |
|    223439 |  4626 | `	pGen->pIn++;` |
|         - |  4627 | `	/* Create the loop block */` |
|    223439 |  4628 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    223439 |  4629 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4630 | `		return SXERR_ABORT;` |
|         - |  4631 | `	}` |
|         - |  4632 | `	/* Delimit the expression */` |
|    223439 |  4633 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    223439 |  4634 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4635 | `		/* Empty expression */` |
|       ! 0 |  4636 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4637 | `		if( rc == SXERR_ABORT ){` |
|         - |  4638 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4639 | `			return SXERR_ABORT;` |
|         - |  4640 | `		}` |
|         - |  4641 | `		/* Synchronize */` |
|       ! 0 |  4642 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4643 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4644 | `			pGen->pIn++;` |
|       ! 0 |  4645 | `		}` |
|       ! 0 |  4646 | `		return SXRET_OK;` |
|         - |  4647 | `	}` |
|         - |  4648 | `	/* Compile the array expression */` |
|    223439 |  4649 | `	pCur = pGen->pIn;` |
|   1205917 |  4650 | `	while( pCur < pEnd ){` |
|   1205917 |  4651 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    234973 |  4652 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    234973 |  4653 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4654 | `				/* Break with the first 'as' found */` |
|    223439 |  4655 | `				break;` |
|         - |  4656 | `			}` |
|      5767 |  4657 | `		}` |
|         - |  4658 | `		/* Advance the stream cursor */` |
|    982483 |  4659 | `		pCur++;` |
|         5 |  4660 | `	}` |
|    223439 |  4661 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4662 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4663 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4664 | `		if( rc == SXERR_ABORT ){` |
|         - |  4665 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4666 | `			return SXERR_ABORT;` |
|         - |  4667 | `		}` |
|       ! 0 |  4668 | `		goto Synchronize;` |
|         - |  4669 | `	}` |
|         - |  4670 | `	/* Swap token streams */` |
|    223439 |  4671 | `	pTmp = pGen->pEnd;` |
|    223439 |  4672 | `	pGen->pEnd = pCur;` |
|    223439 |  4673 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    223439 |  4674 | `	if( rc == SXERR_ABORT ){` |
|         - |  4675 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4676 | `		return SXERR_ABORT;` |
|         - |  4677 | `	}` |
|         - |  4678 | `	/* Update token stream */` |
|    223439 |  4679 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4680 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4681 | `		if( rc == SXERR_ABORT ){` |
|         - |  4682 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4683 | `			return SXERR_ABORT;` |
|         - |  4684 | `		}` |
|       ! 0 |  4685 | `		pGen->pIn++;` |
|       ! 0 |  4686 | `	}` |
|    223439 |  4687 | `	pCur++; /* Jump the 'as' keyword */` |
|    223439 |  4688 | `	pGen->pIn = pCur;` |
|    223439 |  4689 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4690 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4691 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4692 | `			return SXERR_ABORT;` |
|         - |  4693 | `		}` |
|       ! 0 |  4694 | `	}` |
|         - |  4695 | `	/* Create the foreach context */` |
|    223439 |  4696 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    223439 |  4697 | `	if( pInfo == 0 ){` |
|       ! 0 |  4698 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4699 | `		return SXERR_ABORT;` |
|         - |  4700 | `	}` |
|         - |  4701 | `	/* Zero the structure */` |
|    223439 |  4702 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4703 | `	/* Initialize structure fields */` |
|    223439 |  4704 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4705 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4706 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4707 | `	 * '=>'. */` |
|    223439 |  4708 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    223439 |  4709 | `	if( pCur < pEnd ){` |
|         - |  4710 | `		/* Compile the expression holding the key name */` |
|    100139 |  4711 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4712 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4713 | `			if( rc == SXERR_ABORT ){` |
|         - |  4714 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4715 | `				return SXERR_ABORT;` |
|         - |  4716 | `			}` |
|       ! 0 |  4717 | `		}else{` |
|    100139 |  4718 | `			pGen->pEnd = pCur;` |
|    100139 |  4719 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    100139 |  4720 | `			if( rc == SXERR_ABORT ){` |
|         - |  4721 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4722 | `				return SXERR_ABORT;` |
|         - |  4723 | `			}` |
|    100139 |  4724 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    100139 |  4725 | `			if( pInstr->p3 ){` |
|         - |  4726 | `				/* Record key name */` |
|    100139 |  4727 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     50067 |  4728 | `			}` |
|    100139 |  4729 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4730 | `		}` |
|    100139 |  4731 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     50067 |  4732 | `	}` |
|    223439 |  4733 | `	pGen->pEnd = pEnd;` |
|    223439 |  4734 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4735 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4736 | `		if( rc == SXERR_ABORT ){` |
|         - |  4737 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4738 | `			return SXERR_ABORT;` |
|         - |  4739 | `		}` |
|       ! 0 |  4740 | `		goto Synchronize;` |
|         - |  4741 | `	}` |
|    223439 |  4742 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4743 | `		pGen->pIn++;` |
|         - |  4744 | `		/* Pass by reference  */` |
|        33 |  4745 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4746 | `	}` |
|         - |  4747 | `	/* Check if the value target is list() */` |
|    223439 |  4748 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4749 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4750 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4751 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4752 | `		 */` |
|         - |  4753 | `		static int iForeachListCnt = 0;` |
|         - |  4754 | `		char zTmp[128];` |
|         - |  4755 | `		sxu32 nLen;` |
|         - |  4756 | `		char *zDup;` |
|        10 |  4757 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4758 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4759 | `		if( zDup == 0 ){` |
|       ! 0 |  4760 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4761 | `			return SXERR_ABORT;` |
|         - |  4762 | `		}` |
|        10 |  4763 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4764 | `		/* Save list() token boundaries */` |
|        10 |  4765 | `		pListStart = pGen->pIn;` |
|         - |  4766 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4767 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4768 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4769 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  4770 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  4771 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4772 | `				return SXERR_ABORT;` |
|         - |  4773 | `			}` |
|         3 |  4774 | `			goto Synchronize;` |
|         - |  4775 | `		}` |
|         7 |  4776 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  4777 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  4778 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4779 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4780 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  4781 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4782 | `				return SXERR_ABORT;` |
|         - |  4783 | `			}` |
|       ! 0 |  4784 | `			goto Synchronize;` |
|         - |  4785 | `		}` |
|         7 |  4786 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  4787 | `		pListEnd = pGen->pIn;` |
|         7 |  4788 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    223434 |  4789 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  4790 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  4791 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  4792 | `		 */` |
|         - |  4793 | `		static int iForeachShortListCnt = 0;` |
|         - |  4794 | `		char zTmp[128];` |
|         - |  4795 | `		sxu32 nLen;` |
|         - |  4796 | `		char *zDup;` |
|        13 |  4797 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        13 |  4798 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        13 |  4799 | `		if( zDup == 0 ){` |
|       ! 0 |  4800 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4801 | `			return SXERR_ABORT;` |
|         - |  4802 | `		}` |
|        13 |  4803 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4804 | `		/* Save [...] token boundaries */` |
|        13 |  4805 | `		pListStart = pGen->pIn;` |
|         - |  4806 | `		/* Advance past [...] */` |
|        13 |  4807 | `		pGen->pIn++; /* Jump '[' */` |
|        13 |  4808 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        13 |  4809 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4810 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4811 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  4812 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4813 | `				return SXERR_ABORT;` |
|         - |  4814 | `			}` |
|       ! 0 |  4815 | `			goto Synchronize;` |
|         - |  4816 | `		}` |
|        13 |  4817 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        13 |  4818 | `		pListEnd = pGen->pIn;` |
|        13 |  4819 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         7 |  4820 | `	}else{` |
|         - |  4821 | `		/* Compile the expression holding the value name */` |
|    223419 |  4822 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    223419 |  4823 | `		if( rc == SXERR_ABORT ){` |
|         - |  4824 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4825 | `			return SXERR_ABORT;` |
|         - |  4826 | `		}` |
|    223419 |  4827 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    223419 |  4828 | `		if( pInstr->p3 ){` |
|         - |  4829 | `			/* Record value name */` |
|    223419 |  4830 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    111707 |  4831 | `		}` |
|         - |  4832 | `	}` |
|         - |  4833 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    223437 |  4834 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  4835 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    223437 |  4836 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  4837 | `	/* Record the first instruction to execute */` |
|    223437 |  4838 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4839 | `	/* Emit the FOREACH_STEP instruction */` |
|    223437 |  4840 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  4841 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    223437 |  4842 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  4843 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    223437 |  4844 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  4845 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  4846 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  4847 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  4848 | `		 */` |
|        19 |  4849 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  4850 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  4851 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  4852 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  4853 | `		 */` |
|        19 |  4854 | `		pSavedIn = pGen->pIn;` |
|        19 |  4855 | `		pSavedEnd = pGen->pEnd;` |
|        19 |  4856 | `		pGen->pIn = pListStart;` |
|        19 |  4857 | `		pGen->pEnd = pListEnd;` |
|        19 |  4858 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        13 |  4859 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  4860 | `		}else{` |
|         7 |  4861 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  4862 | `		}` |
|        19 |  4863 | `		pGen->pIn = pSavedIn;` |
|        19 |  4864 | `		pGen->pEnd = pSavedEnd;` |
|        19 |  4865 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4866 | `			return SXERR_ABORT;` |
|         - |  4867 | `		}` |
|         - |  4868 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        19 |  4869 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         9 |  4870 | `	}` |
|         - |  4871 | `	/* Compile the loop body */` |
|    223437 |  4872 | `	pGen->pIn = &pEnd[1];` |
|    223437 |  4873 | `	pGen->pEnd = pTmp;` |
|    223437 |  4874 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    223437 |  4875 | `	if( rc == SXERR_ABORT ){` |
|         - |  4876 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4877 | `		return SXERR_ABORT;` |
|         - |  4878 | `	}` |
|         - |  4879 | `	/* Emit the unconditional jump to the start of the loop */` |
|    223437 |  4880 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  4881 | `	/* Fix all jumps now the destination is resolved */` |
|    223437 |  4882 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4883 | `	/* Release the loop block */` |
|    223437 |  4884 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4885 | `	/* Statement successfully compiled */` |
|    223437 |  4886 | `	return SXRET_OK;` |
|         1 |  4887 | `Synchronize:` |
|         - |  4888 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4889 | `	 * compiling this erroneous block.` |
|         - |  4890 | `	 */` |
|         3 |  4891 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4892 | `		pGen->pIn++;` |
|       ! 0 |  4893 | `	}` |
|         3 |  4894 | `	return SXRET_OK;` |
|    111722 |  4895 | `}` |
|         - |  4896 | `/*` |
|         - |  4897 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  4898 | ` * According to the PHP language reference` |
|         - |  4899 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  4900 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  4901 | ` *  that is similar to that of C:` |
|         - |  4902 | ` *  if (expr)` |
|         - |  4903 | ` *   statement` |
|         - |  4904 | ` *  else construct:` |
|         - |  4905 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  4906 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  4907 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  4908 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  4909 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  4910 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  4911 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  4912 | ` *  elseif` |
|         - |  4913 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  4914 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  4915 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  4916 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  4917 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  4918 | ` *   <?php` |
|         - |  4919 | ` *    if ($a > $b) {` |
|         - |  4920 | ` *     echo "a is bigger than b";` |
|         - |  4921 | ` *    } elseif ($a == $b) {` |
|         - |  4922 | ` *     echo "a is equal to b";` |
|         - |  4923 | ` *    } else {` |
|         - |  4924 | ` *     echo "a is smaller than b";` |
|         - |  4925 | ` *    }` |
|         - |  4926 | ` *    ?>` |
|         - |  4927 | ` */` |
|   1876146 |  4928 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  4929 | `{` |
|   1876151 |  4930 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   1876151 |  4931 | `	GenBlock *pCondBlock = 0;` |
|         - |  4932 | `	sxu32 nJumpIdx;` |
|         - |  4933 | `	sxu32 nKeyID;` |
|         - |  4934 | `	sxi32 rc;` |
|         - |  4935 | `	/* Jump the 'if' keyword */` |
|   1876151 |  4936 | `	pGen->pIn++;` |
|   1876151 |  4937 | `	pToken = pGen->pIn;` |
|         - |  4938 | `	/* Create the conditional block */` |
|   1876151 |  4939 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   1876151 |  4940 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4941 | `		return SXERR_ABORT;` |
|         - |  4942 | `	}` |
|         - |  4943 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1045652 |  4944 | `	for(;;){` |
|   2091309 |  4945 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4946 | `			/* Syntax error */` |
|       ! 0 |  4947 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4948 | `				pToken--;` |
|       ! 0 |  4949 | `			}` |
|       ! 0 |  4950 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  4951 | `			if( rc == SXERR_ABORT ){` |
|         - |  4952 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4953 | `				return SXERR_ABORT;` |
|         - |  4954 | `			}` |
|       ! 0 |  4955 | `			goto Synchronize;` |
|         - |  4956 | `		}` |
|         - |  4957 | `		/* Jump the left parenthesis '(' */` |
|   2091309 |  4958 | `		pToken++;` |
|         - |  4959 | `		/* Delimit the condition */` |
|   2091309 |  4960 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2091309 |  4961 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  4962 | `			/* Syntax error */` |
|        10 |  4963 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4964 | `				pToken--;` |
|       ! 0 |  4965 | `			}` |
|        10 |  4966 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        10 |  4967 | `			if( rc == SXERR_ABORT ){` |
|         - |  4968 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4969 | `				return SXERR_ABORT;` |
|         - |  4970 | `			}` |
|        10 |  4971 | `			goto Synchronize;` |
|         - |  4972 | `		}` |
|         - |  4973 | `		/* Swap token streams */` |
|   2091301 |  4974 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  4975 | `		/* Compile the condition */` |
|   2091301 |  4976 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4977 | `		/* Update token stream */` |
|   2091301 |  4978 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  4979 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4980 | `			pGen->pIn++;` |
|       ! 0 |  4981 | `		}` |
|   2091301 |  4982 | `		pGen->pIn  = &pEnd[1];` |
|   2091301 |  4983 | `		pGen->pEnd = pTmp;` |
|   2091301 |  4984 | `		if( rc == SXERR_ABORT ){` |
|         - |  4985 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4986 | `			return SXERR_ABORT;` |
|         - |  4987 | `		}` |
|         - |  4988 | `		/* Emit the false jump */` |
|   2091301 |  4989 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  4990 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2091301 |  4991 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  4992 | `		/* Compile the body */` |
|   2091301 |  4993 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2091301 |  4994 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4995 | `			return SXERR_ABORT;` |
|         - |  4996 | `		}` |
|   2091301 |  4997 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    415430 |  4998 | `			break;` |
|         - |  4999 | `		}` |
|         - |  5000 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1260451 |  5001 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1260451 |  5002 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|    887389 |  5003 | `			break;` |
|         - |  5004 | `		}` |
|         - |  5005 | `		/* Emit the unconditional jump */` |
|    373067 |  5006 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5007 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    373067 |  5008 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    373067 |  5009 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    242389 |  5010 | `			pToken = &pGen->pIn[1];` |
|    242389 |  5011 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     84518 |  5012 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|     78957 |  5013 | `					break;` |
|         - |  5014 | `			}` |
|     84485 |  5015 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     42240 |  5016 | `		}` |
|    215163 |  5017 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5018 | `		/* Synchronize cursors */` |
|    215163 |  5019 | `		pToken = pGen->pIn;` |
|         - |  5020 | `		/* Fix the false jump */` |
|    215163 |  5021 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5022 | `	} /* For(;;) */` |
|         - |  5023 | `	/* Fix the false jump */` |
|   1876143 |  5024 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   1876143 |  5025 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1045288 |  5026 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5027 | `			/* Compile the else block */` |
|    157909 |  5028 | `			pGen->pIn++;` |
|    157909 |  5029 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    157909 |  5030 | `			if( rc == SXERR_ABORT ){` |
|         - |  5031 |  |
|       ! 0 |  5032 | `				return SXERR_ABORT;` |
|         - |  5033 | `			}` |
|     78952 |  5034 | `	}` |
|   1876143 |  5035 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5036 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   1876143 |  5037 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5038 | `	/* Release the conditional block */` |
|   1876143 |  5039 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5040 | `	/* Statement successfully compiled */` |
|   1876143 |  5041 | `	return SXRET_OK;` |
|         4 |  5042 | `Synchronize:` |
|         - |  5043 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5044 | `	 */` |
|        66 |  5045 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        58 |  5046 | `		pGen->pIn++;` |
|         2 |  5047 | `	}` |
|        10 |  5048 | `	return SXRET_OK;` |
|    938078 |  5049 | `}` |
|         - |  5050 | `/*` |
|         - |  5051 | ` * Compile the global construct.` |
|         - |  5052 | ` * According to the PHP language reference` |
|         - |  5053 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5054 | ` *  to be used in that function.` |
|         - |  5055 | ` *  Example #1 Using global` |
|         - |  5056 | ` *  <?php` |
|         - |  5057 | ` *   $a = 1;` |
|         - |  5058 | ` *   $b = 2;` |
|         - |  5059 | ` *   function Sum()` |
|         - |  5060 | ` *   {` |
|         - |  5061 | ` *    global $a, $b;` |
|         - |  5062 | ` *    $b = $a + $b;` |
|         - |  5063 | ` *   }` |
|         - |  5064 | ` *   Sum();` |
|         - |  5065 | ` *   echo $b;` |
|         - |  5066 | ` *  ?>` |
|         - |  5067 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5068 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5069 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5070 | ` */` |
|        38 |  5071 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5072 | `{` |
|        43 |  5073 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5074 | `	sxi32 nExpr;` |
|         - |  5075 | `	sxi32 rc;` |
|         - |  5076 | `	/* Jump the 'global' keyword */` |
|        43 |  5077 | `	pGen->pIn++;` |
|        43 |  5078 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5079 | `		/* Nothing to process */` |
|       ! 0 |  5080 | `		return SXRET_OK;` |
|         - |  5081 | `	}` |
|        43 |  5082 | `	pTmp = pGen->pEnd;` |
|        43 |  5083 | `	nExpr = 0;` |
|        91 |  5084 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5085 | `		if( pGen->pIn < pNext ){` |
|        53 |  5086 | `			pGen->pEnd = pNext;` |
|        53 |  5087 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5088 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5089 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5090 | `					return SXERR_ABORT;` |
|         - |  5091 | `				}` |
|       ! 0 |  5092 | `			}else{` |
|        53 |  5093 | `				pGen->pIn++;` |
|        53 |  5094 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5095 | `					/* Emit a warning */` |
|       ! 0 |  5096 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5097 | `				}else{` |
|        53 |  5098 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5099 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5100 | `						return SXERR_ABORT;` |
|        53 |  5101 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5102 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5103 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5104 | `							/* Variable name, not a constant */` |
|        53 |  5105 | `							pLast->iP1 = 0;` |
|        24 |  5106 | `						}` |
|        53 |  5107 | `						nExpr++;` |
|        24 |  5108 | `					}` |
|         - |  5109 | `				}` |
|         - |  5110 | `			}` |
|        24 |  5111 | `		}` |
|         - |  5112 | `		/* Next expression in the stream */` |
|        53 |  5113 | `		pGen->pIn = pNext;` |
|         - |  5114 | `		/* Jump trailing commas */` |
|        63 |  5115 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5116 | `			pGen->pIn++;` |
|         5 |  5117 | `		}` |
|         5 |  5118 | `	}` |
|         - |  5119 | `	/* Restore token stream */` |
|        43 |  5120 | `	pGen->pEnd = pTmp;` |
|        43 |  5121 | `	if( nExpr > 0 ){` |
|         - |  5122 | `		/* Emit the uplink instruction */` |
|        43 |  5123 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5124 | `	}` |
|        43 |  5125 | `	return SXRET_OK;` |
|        24 |  5126 | `}` |
|         - |  5127 | `/*` |
|         - |  5128 | ` * Compile the return statement.` |
|         - |  5129 | ` * According to the PHP language reference` |
|         - |  5130 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5131 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5132 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5133 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5134 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5135 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5136 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5137 | ` *  from within the main script file, then script execution end.` |
|         - |  5138 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5139 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5140 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5141 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5142 | ` */` |
|   2674710 |  5143 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5144 | `{` |
|   2674715 |  5145 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5146 | `	sxi32 rc;` |
|   2674715 |  5147 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2674715 |  5148 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5149 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5150 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5151 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5152 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5153 | `	 * normally below so token processing stays consistent. */` |
|   6932303 |  5154 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4257593 |  5155 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5156 | `	}` |
|   2674710 |  5157 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2674683 |  5158 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5159 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5160 | `			"A never-returning function must not return");` |
|         3 |  5161 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5162 | `			return SXERR_ABORT;` |
|         - |  5163 | `		}` |
|         1 |  5164 | `	}` |
|         - |  5165 | `	/* Jump the 'return' keyword */` |
|   2674715 |  5166 | `	pGen->pIn++;` |
|   2674715 |  5167 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5168 | `		/* Compile the expression */` |
|   2590205 |  5169 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2590205 |  5170 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5171 | `			return SXERR_ABORT;` |
|   2590205 |  5172 | `		}else if(rc != SXERR_EMPTY ){` |
|   2590205 |  5173 | `			nRet = 1;` |
|   1295100 |  5174 | `		}` |
|   1295100 |  5175 | `	}` |
|         - |  5176 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5177 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5178 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5179 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2674715 |  5180 | `	if( pGen->bInGenerator ){` |
|      3873 |  5181 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3873 |  5182 | `		return SXRET_OK;` |
|         - |  5183 | `	}` |
|         - |  5184 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5185 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5186 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5187 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5188 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2670847 |  5189 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2670847 |  5190 | `	return SXRET_OK;` |
|   1337360 |  5191 | `}` |
|         - |  5192 | `/*` |
|         - |  5193 | ` * Compile a yield expression.` |
|         - |  5194 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5195 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5196 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5197 | ` */` |
|     15744 |  5198 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5199 | `{` |
|         - |  5200 | `	SyToken *pTmp, *pSplit;` |
|     15749 |  5201 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15749 |  5202 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5203 | `	sxi32 rc;` |
|      7872 |  5204 | `	(void)iCompileFlag;` |
|         - |  5205 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15749 |  5206 | `	pGen->pIn++;` |
|         - |  5207 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5208 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5209 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5210 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5211 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15744 |  5212 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7907 |  5213 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5214 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5215 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5216 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5217 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5218 | `			return SXERR_ABORT;` |
|         - |  5219 | `		}` |
|        67 |  5220 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5221 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5222 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5223 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5224 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5225 | `				return SXERR_ABORT;` |
|         - |  5226 | `			}` |
|       ! 0 |  5227 | `		}` |
|        67 |  5228 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5229 | `		return SXRET_OK;` |
|         - |  5230 | `	}` |
|     15687 |  5231 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5232 | `		/* Bare yield — no value */` |
|         3 |  5233 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5234 | `		return SXRET_OK;` |
|         - |  5235 | `	}` |
|         - |  5236 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15685 |  5237 | `	pSplit = 0;` |
|         - |  5238 | `	{` |
|     15685 |  5239 | `		SyToken *pCur = pGen->pIn;` |
|     15685 |  5240 | `		sxi32 nNest = 0;` |
|     46861 |  5241 | `		while( pCur < pGen->pEnd ){` |
|     46555 |  5242 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5243 | `				nNest++;` |
|     46547 |  5244 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5245 | `				nNest--;` |
|     46531 |  5246 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15379 |  5247 | `				pSplit = pCur;` |
|     15379 |  5248 | `				break;` |
|         - |  5249 | `			}` |
|     31181 |  5250 | `			pCur++;` |
|         5 |  5251 | `		}` |
|         - |  5252 | `	}` |
|     15685 |  5253 | `	pTmp = pGen->pEnd;` |
|     15685 |  5254 | `	if( pSplit ){` |
|         - |  5255 | `		/* yield $key => $value */` |
|     15379 |  5256 | `		pGen->pEnd = pSplit;` |
|     15379 |  5257 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15379 |  5258 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15379 |  5259 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15379 |  5260 | `		pGen->pEnd = pTmp;` |
|     15379 |  5261 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15379 |  5262 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15379 |  5263 | `		iP1 = 1;` |
|     15379 |  5264 | `		iP2 = 1;` |
|      7692 |  5265 | `	}else{` |
|         - |  5266 | `		/* yield $value */` |
|       311 |  5267 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       311 |  5268 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       311 |  5269 | `		if( rc != SXERR_EMPTY ){` |
|       311 |  5270 | `			iP1 = 1;` |
|       153 |  5271 | `		}` |
|         - |  5272 | `	}` |
|     15685 |  5273 | `	pGen->pEnd = pTmp;` |
|     15685 |  5274 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15685 |  5275 | `	return SXRET_OK;` |
|      7877 |  5276 | `}` |
|         - |  5277 | `/*` |
|         - |  5278 | ` * Compile the die/exit language construct.` |
|         - |  5279 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5280 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5281 | ` */` |
|       128 |  5282 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5283 | `{` |
|       133 |  5284 | `	sxi32 nExpr = 0;` |
|         - |  5285 | `	sxi32 rc;` |
|         - |  5286 | `	/* Jump the die/exit keyword */` |
|       133 |  5287 | `	pGen->pIn++;` |
|       133 |  5288 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5289 | `		/* Compile the expression */` |
|       133 |  5290 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5291 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5292 | `			return SXERR_ABORT;` |
|       133 |  5293 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5294 | `			nExpr = 1;` |
|        64 |  5295 | `		}` |
|        64 |  5296 | `	}` |
|         - |  5297 | `	/* Emit the HALT instruction */` |
|       133 |  5298 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5299 | `	return SXRET_OK;` |
|        69 |  5300 | `}` |
|         - |  5301 | `/*` |
|         - |  5302 | ` * Compile the 'echo' language construct.` |
|         - |  5303 | ` */` |
|     17700 |  5304 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5305 | `{` |
|     17705 |  5306 | `	SyToken *pTmp,*pNext = 0;` |
|     17705 |  5307 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17705 |  5308 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17705 |  5309 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5310 | `	sxi32 rc;` |
|         - |  5311 | `	/* Jump the 'echo' keyword */` |
|     17705 |  5312 | `	pGen->pIn++;` |
|         - |  5313 | `	/* Compile arguments one after one */` |
|     17705 |  5314 | `	pTmp = pGen->pEnd;` |
|     44345 |  5315 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     26647 |  5316 | `		if( pGen->pIn < pNext ){` |
|     26647 |  5317 | `			pGen->pEnd = pNext;` |
|     26647 |  5318 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     26647 |  5319 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5320 | `				return SXERR_ABORT;` |
|     26647 |  5321 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5322 | `				/* Emit the consume instruction */` |
|     26623 |  5323 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     26623 |  5324 | `				nExpr++;` |
|     26623 |  5325 | `				bExpectMore = 0;` |
|     13309 |  5326 | `			}` |
|     13321 |  5327 | `		}` |
|         - |  5328 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5329 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     35595 |  5330 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      8955 |  5331 | `			if( bExpectMore ){` |
|         - |  5332 | `				/* two commas in a row */` |
|         3 |  5333 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5334 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5335 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5336 | `			}` |
|      8953 |  5337 | `			bExpectMore = 1;` |
|      8953 |  5338 | `			pNext++;` |
|         5 |  5339 | `		}` |
|     26645 |  5340 | `		pGen->pIn = pNext;` |
|         5 |  5341 | `	}` |
|         - |  5342 | `	/* Restore token stream */` |
|     17703 |  5343 | `	pGen->pEnd = pTmp;` |
|     17703 |  5344 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5345 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        32 |  5346 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5347 | `			"syntax error, unexpected token \";\"");` |
|        32 |  5348 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5349 | `	}` |
|     17675 |  5350 | `	return SXRET_OK;` |
|      8855 |  5351 | `}` |
|         - |  5352 | `/*` |
|         - |  5353 | ` * Compile the static statement.` |
|         - |  5354 | ` * According to the PHP language reference` |
|         - |  5355 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5356 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5357 | ` *  when program execution leaves this scope.` |
|         - |  5358 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5359 | ` * Symisc eXtension.` |
|         - |  5360 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5361 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5362 | ` *  Example` |
|         - |  5363 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5364 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5365 | ` */` |
|        12 |  5366 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         3 |  5367 | `{` |
|         - |  5368 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5369 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5370 | `	GenBlock *pBlock;` |
|         - |  5371 | `	SyString *pName;` |
|         - |  5372 | `	char *zDup;` |
|         - |  5373 | `	sxu32 nLine;` |
|         - |  5374 | `	sxi32 rc;` |
|         - |  5375 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5376 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5377 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|        12 |  5378 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|        10 |  5379 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5380 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5381 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5382 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5383 | `			return SXERR_ABORT;` |
|         3 |  5384 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5385 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5386 | `		}` |
|         3 |  5387 | `		return SXRET_OK;` |
|         - |  5388 | `	}` |
|         - |  5389 | `	/* Jump the static keyword */` |
|        13 |  5390 | `	nLine = pGen->pIn->nLine;` |
|        13 |  5391 | `	pGen->pIn++;` |
|         - |  5392 | `	/* Extract the enclosing function if any */` |
|        13 |  5393 | `	pBlock = pGen->pCurrent;` |
|        23 |  5394 | `	while( pBlock ){` |
|        23 |  5395 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|        13 |  5396 | `			break;` |
|         - |  5397 | `		}` |
|         - |  5398 | `		/* Point to the upper block */` |
|        13 |  5399 | `		pBlock = pBlock->pParent;` |
|         3 |  5400 | `	}` |
|        13 |  5401 | `	if( pBlock == 0 ){` |
|         - |  5402 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5403 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5404 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5405 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5406 | `				return SXERR_ABORT;` |
|         - |  5407 | `			}` |
|       ! 0 |  5408 | `			goto Synchronize;` |
|         - |  5409 | `		}` |
|         - |  5410 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5411 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5412 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5413 | `			return SXERR_ABORT;` |
|       ! 0 |  5414 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5415 | `			/* Emit the POP instruction */` |
|       ! 0 |  5416 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5417 | `		}` |
|       ! 0 |  5418 | `		return SXRET_OK;` |
|         - |  5419 | `	}` |
|        13 |  5420 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5421 | `	/* Make sure we are dealing with a valid statement */` |
|        13 |  5422 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|         8 |  5423 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5424 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5425 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5426 | `				return SXERR_ABORT;` |
|         - |  5427 | `			}` |
|         3 |  5428 | `			goto Synchronize;` |
|         - |  5429 | `	}` |
|        10 |  5430 | `	pGen->pIn++;` |
|         - |  5431 | `	/* Extract variable name */` |
|        10 |  5432 | `	pName = &pGen->pIn->sData;` |
|        10 |  5433 | `	pGen->pIn++; /* Jump the var name */` |
|        10 |  5434 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5435 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5436 | `		goto Synchronize;` |
|         - |  5437 | `	}` |
|         - |  5438 | `	/* Initialize the structure describing the static variable */` |
|        10 |  5439 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        10 |  5440 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5441 | `	/* Duplicate variable name */` |
|        10 |  5442 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        10 |  5443 | `	if( zDup == 0 ){` |
|       ! 0 |  5444 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5445 | `		return SXERR_ABORT;` |
|         - |  5446 | `	}` |
|        10 |  5447 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5448 | `	/* Check if we have an expression to compile */` |
|        10 |  5449 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5450 | `		SySet *pInstrContainer;` |
|         - |  5451 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5452 | `		 * Static variable can take any complex expression including function` |
|         - |  5453 | `		 * call as their initialization value.` |
|         - |  5454 | `		 * Example:` |
|         - |  5455 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5456 | `		 */` |
|        10 |  5457 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5458 | `		/* Swap bytecode container */` |
|        10 |  5459 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        10 |  5460 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5461 | `		/* Compile the expression */` |
|        10 |  5462 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5463 | `		/* Emit the done instruction */` |
|        10 |  5464 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5465 | `		/* Restore default bytecode container */` |
|        10 |  5466 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         4 |  5467 | `	}` |
|         - |  5468 | `	/* Finally save the compiled static variable in the appropriate container */` |
|        10 |  5469 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|        10 |  5470 | `	return SXRET_OK;` |
|         1 |  5471 | `Synchronize:` |
|         - |  5472 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5473 | `	 * statement.` |
|         - |  5474 | `	 */` |
|         5 |  5475 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5476 | `		pGen->pIn++;` |
|         1 |  5477 | `	}` |
|         3 |  5478 | `	return SXRET_OK;` |
|         9 |  5479 | `}` |
|         - |  5480 | `/*` |
|         - |  5481 | ` * Compile the var statement.` |
|         - |  5482 | ` * Symisc Extension:` |
|         - |  5483 | ` *      var statement can be used outside of a class definition.` |
|         - |  5484 | ` */` |
|         4 |  5485 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5486 | `{` |
|         - |  5487 | `	sxu32 nLine;` |
|         - |  5488 | `	sxi32 rc;` |
|         5 |  5489 | `	nLine = pGen->pIn->nLine;` |
|         - |  5490 | `	/* Jump the 'var' keyword */` |
|         5 |  5491 | `	pGen->pIn++;` |
|         5 |  5492 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5493 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5494 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5495 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5496 | `			pGen->pIn++;` |
|       ! 0 |  5497 | `		}` |
|       ! 0 |  5498 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5499 | `			return SXERR_ABORT;` |
|         - |  5500 | `		}` |
|       ! 0 |  5501 | `	}else{` |
|         - |  5502 | `		/* Compile the expression */` |
|         5 |  5503 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5504 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5505 | `			return SXERR_ABORT;` |
|         5 |  5506 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5507 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5508 | `		}` |
|         - |  5509 | `	}` |
|         5 |  5510 | `	return SXRET_OK;` |
|         3 |  5511 | `}` |
|         - |  5512 | `/*` |
|         - |  5513 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5514 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5515 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5516 | ` */` |
|         - |  5517 | `/*` |
|         - |  5518 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5519 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5520 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5521 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5522 | ` *` |
|         - |  5523 | ` * Resolution order:` |
|         - |  5524 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5525 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5526 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5527 | ` *` |
|         - |  5528 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5529 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5530 | ` * Returns the (possibly new) literal index.` |
|         - |  5531 | ` */` |
|   4862826 |  5532 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5533 | `{` |
|         - |  5534 | `	ph7_value *pLit;` |
|         - |  5535 | `	const char *zLit;` |
|         - |  5536 | `	SyString sQualified;` |
|         - |  5537 | `	sxu32 nLit;` |
|         - |  5538 | `	sxu32 k;` |
|         - |  5539 | `	sxu32 nNewIdx;` |
|         - |  5540 | `	int hasNsSep;` |
|         - |  5541 | `	SyHashEntry *pImport;` |
|         - |  5542 | `	ph7_value *pNew;` |
|   4862831 |  5543 | `	if( pFromImport ){` |
|   3837603 |  5544 | `		*pFromImport = 0;` |
|   1918799 |  5545 | `	}` |
|   4862831 |  5546 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   4862831 |  5547 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5548 | `		return nOrigIdx;` |
|         - |  5549 | `	}` |
|   4862831 |  5550 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   4862831 |  5551 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5552 | `	/* Skip if already qualified (contains backslash) */` |
|   4862831 |  5553 | `	hasNsSep = 0;` |
|  59532277 |  5554 | `	for( k = 0; k < nLit; k++ ){` |
|  54669459 |  5555 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  27334728 |  5556 | `	}` |
|   4862831 |  5557 | `	if( hasNsSep ){` |
|        10 |  5558 | `		return nOrigIdx;` |
|         - |  5559 | `	}` |
|         - |  5560 | `	/* Check use imports first (works even outside namespaces) */` |
|   4862823 |  5561 | `	SyBlobReset(&pGen->sWorker);` |
|   4862823 |  5562 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   4862823 |  5563 | `	if( pImport ){` |
|        41 |  5564 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5565 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5566 | `		if( pFromImport ){` |
|        18 |  5567 | `			*pFromImport = 1;` |
|         8 |  5568 | `		}` |
|        23 |  5569 | `	}else{` |
|   4862787 |  5570 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   4862697 |  5571 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5572 | `		}` |
|         - |  5573 | `		/* Prepend current namespace */` |
|        95 |  5574 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        95 |  5575 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|        95 |  5576 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5577 | `	}` |
|         - |  5578 | `	/* Look up or create a new literal for the qualified name */` |
|       131 |  5579 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       131 |  5580 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        57 |  5581 | `		return nNewIdx; /* Already interned */` |
|         - |  5582 | `	}` |
|        79 |  5583 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        79 |  5584 | `	if( pNew == 0 ){` |
|       ! 0 |  5585 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5586 | `	}` |
|        79 |  5587 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        79 |  5588 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        79 |  5589 | `	return nNewIdx;` |
|   2431418 |  5590 | `}` |
|         - |  5591 | `/*` |
|         - |  5592 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5593 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5594 | ` */` |
|    412396 |  5595 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5596 | `{` |
|         - |  5597 | `	SyHashEntry *pImport;` |
|         - |  5598 | `	/* Check use imports first */` |
|    412401 |  5599 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    412401 |  5600 | `	if( pImport ){` |
|        20 |  5601 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        20 |  5602 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        20 |  5603 | `		return;` |
|         - |  5604 | `	}` |
|         - |  5605 | `	/* Prepend current namespace if active */` |
|    412385 |  5606 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         8 |  5607 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  5608 | `		SyBlobAppend(pOut,"\\",1);` |
|         3 |  5609 | `	}` |
|    412385 |  5610 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    206203 |  5611 | `}` |
|         - |  5612 | `/*` |
|         - |  5613 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5614 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5615 | ` * The caller must release pOut when done.` |
|         - |  5616 | ` */` |
|    432034 |  5617 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5618 | `{` |
|    432039 |  5619 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3903 |  5620 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3903 |  5621 | `		SyBlobAppend(pOut,"\\",1);` |
|      1949 |  5622 | `	}` |
|    432039 |  5623 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    432039 |  5624 | `}` |
|         - |  5625 | `/*` |
|         - |  5626 | ` * Compile a namespace statement` |
|         - |  5627 | ` * According to the PHP language reference manual` |
|         - |  5628 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5629 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5630 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5631 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5632 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5633 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5634 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5635 | ` *  programming world.` |
|         - |  5636 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5637 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5638 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5639 | ` *  classes/functions/constants.` |
|         - |  5640 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5641 | ` *  readability of source code.` |
|         - |  5642 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5643 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5644 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5645 | ` *       class MyClass {}` |
|         - |  5646 | ` *       function myfunction() {}` |
|         - |  5647 | ` *       const MYCONST = 1;` |
|         - |  5648 | ` *       $a = new MyClass;` |
|         - |  5649 | ` *       $c = new \my\name\MyClass;` |
|         - |  5650 | ` *       $a = strlen('hi');` |
|         - |  5651 | ` *       $d = namespace\MYCONST;` |
|         - |  5652 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5653 | ` *       echo constant($d);` |
|         - |  5654 | ` * NOTE` |
|         - |  5655 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5656 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5657 | ` */` |
|         - |  5658 | `/*` |
|         - |  5659 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5660 | ` */` |
|        14 |  5661 | `static const char * TokenTypeName(sxu32 nType)` |
|         3 |  5662 | `{` |
|        17 |  5663 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        10 |  5664 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        10 |  5665 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        10 |  5666 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        10 |  5667 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        10 |  5668 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5669 | `	return "token";` |
|        10 |  5670 | `}` |
|      3946 |  5671 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5672 | `{` |
|         - |  5673 | `	sxu32 nLine;` |
|         - |  5674 | `	sxi32 rc;` |
|      3951 |  5675 | `	nLine = pGen->pIn->nLine;` |
|      3951 |  5676 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5677 | `	/* Reset namespace and clear previous use imports */` |
|      3951 |  5678 | `	SyBlobReset(&pGen->sNamespace);` |
|      3951 |  5679 | `	SyHashRelease(&pGen->hUseImports);` |
|      3951 |  5680 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3951 |  5681 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3951 |  5682 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3951 |  5683 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3951 |  5684 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3951 |  5685 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5686 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5687 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5688 | `		return SXRET_OK;` |
|         - |  5689 | `	}` |
|      3951 |  5690 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5691 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5693 | `		return SXRET_OK;` |
|         - |  5694 | `	}` |
|      3951 |  5695 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5696 | `		/* namespace { } — global namespace block */` |
|       ! 0 |  5697 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5698 | `		return SXRET_OK;` |
|         - |  5699 | `	}` |
|         - |  5700 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7939 |  5701 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3993 |  5702 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5703 | `			/* Append backslash separator */` |
|        27 |  5704 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        27 |  5705 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        11 |  5706 | `			}` |
|        16 |  5707 | `		}else{` |
|         - |  5708 | `			/* Append identifier */` |
|      3971 |  5709 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5710 | `		}` |
|      3993 |  5711 | `		pGen->pIn++;` |
|         5 |  5712 | `	}` |
|         - |  5713 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5714 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5715 | `	{` |
|      3951 |  5716 | `		char *zNsDup = 0;` |
|      3951 |  5717 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5921 |  5718 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3944 |  5719 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1972 |  5720 | `		}` |
|      3951 |  5721 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5722 | `	}` |
|      3951 |  5723 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5724 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5725 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5726 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5727 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5728 | `			return SXERR_ABORT;` |
|         - |  5729 | `		}` |
|         2 |  5730 | `	}` |
|      3951 |  5731 | `	return SXRET_OK;` |
|      1978 |  5732 | `}` |
|         - |  5733 | `/*` |
|         - |  5734 | ` * Compile the 'use' statement` |
|         - |  5735 | ` * According to the PHP language reference manual` |
|         - |  5736 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5737 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5738 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5739 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5740 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5741 | ` *  a function or constant is not supported.` |
|         - |  5742 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5743 | ` * NOTE` |
|         - |  5744 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5745 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5746 | ` */` |
|        72 |  5747 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5748 | `{` |
|         - |  5749 | `	sxu32 nLine;` |
|         - |  5750 | `	sxi32 rc;` |
|         - |  5751 | `	SyBlob sPath;` |
|         - |  5752 | `	SyString sAlias;` |
|         - |  5753 | `	SyToken *pLast;` |
|         - |  5754 | `	char *zDup;` |
|         - |  5755 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5756 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5757 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        77 |  5758 | `	nLine = pGen->pIn->nLine;` |
|        77 |  5759 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5760 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        77 |  5761 | `	iUseType = 0;` |
|        77 |  5762 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5763 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5764 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5765 | `			iUseType = 1;` |
|        16 |  5766 | `			pGen->pIn++;` |
|        23 |  5767 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5768 | `			iUseType = 2;` |
|        16 |  5769 | `			pGen->pIn++;` |
|         7 |  5770 | `		}` |
|        14 |  5771 | `	}` |
|         - |  5772 | `	/* Select target hash tables based on import type */` |
|        77 |  5773 | `	switch( iUseType ){` |
|         7 |  5774 | `		case 1:` |
|        16 |  5775 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5776 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5777 | `			break;` |
|         7 |  5778 | `		case 2:` |
|        16 |  5779 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5780 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5781 | `			break;` |
|        22 |  5782 | `		default:` |
|        49 |  5783 | `			pGenHash = &pGen->hUseImports;` |
|        49 |  5784 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        44 |  5785 | `			break;` |
|         - |  5786 | `	}` |
|        77 |  5787 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5788 | `	/* Process one or more use declarations separated by commas */` |
|        37 |  5789 | `	for(;;){` |
|        79 |  5790 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5791 | `			break;` |
|         - |  5792 | `		}` |
|        79 |  5793 | `		SyBlobReset(&sPath);` |
|        79 |  5794 | `		pLast = 0;` |
|         - |  5795 | `		/* Collect the full namespace path */` |
|       269 |  5796 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       195 |  5797 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       135 |  5798 | `				pLast = pGen->pIn;` |
|       135 |  5799 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        65 |  5800 | `					SyBlobAppend(&sPath,"\\",1);` |
|        30 |  5801 | `				}` |
|       135 |  5802 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        65 |  5803 | `			}` |
|       195 |  5804 | `			pGen->pIn++;` |
|         5 |  5805 | `		}` |
|        79 |  5806 | `		if( pLast == 0 ){` |
|         - |  5807 | `			/* Empty path */` |
|         6 |  5808 | `			break;` |
|         - |  5809 | `		}` |
|         - |  5810 | `		/* Default alias is the last component of the path */` |
|        75 |  5811 | `		sAlias = pLast->sData;` |
|         - |  5812 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        70 |  5813 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        50 |  5814 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        23 |  5815 | `			pGen->pIn++; /* Jump 'as' */` |
|        23 |  5816 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        23 |  5817 | `				sAlias = pGen->pIn->sData;` |
|        23 |  5818 | `				pGen->pIn++;` |
|        10 |  5819 | `			}` |
|        10 |  5820 | `		}` |
|         - |  5821 | `		/* Check for duplicate import alias (per-type) */` |
|        75 |  5822 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  5823 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  5824 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  5825 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  5826 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5827 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  5828 | `				return SXERR_ABORT;` |
|         - |  5829 | `			}` |
|         2 |  5830 | `		}` |
|         - |  5831 | `		/* Register the import: alias -> FQN.` |
|         - |  5832 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  5833 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  5834 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       110 |  5835 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        70 |  5836 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        75 |  5837 | `		if( zDup ){` |
|        75 |  5838 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        75 |  5839 | `			if( pVmHash ){` |
|         - |  5840 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  5841 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        47 |  5842 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        47 |  5843 | `				if( zAliasDup ){` |
|        47 |  5844 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        21 |  5845 | `				}` |
|        21 |  5846 | `			}` |
|        75 |  5847 | `			if( iUseType == 2 ){` |
|         - |  5848 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  5849 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  5850 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  5851 | `				if( zAliasDup ){` |
|         - |  5852 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  5853 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  5854 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  5855 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  5856 | `					if( azPair ){` |
|        16 |  5857 | `						azPair[0] = zAliasDup;` |
|        16 |  5858 | `						azPair[1] = zDup;` |
|        16 |  5859 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  5860 | `					}` |
|         7 |  5861 | `				}` |
|         7 |  5862 | `			}` |
|        35 |  5863 | `		}` |
|         - |  5864 | `		/* Check for comma (multiple use declarations) */` |
|        75 |  5865 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  5866 | `			pGen->pIn++;` |
|         2 |  5867 | `		}else{` |
|        39 |  5868 | `			break;` |
|         - |  5869 | `		}` |
|         1 |  5870 | `	}` |
|        77 |  5871 | `	SyBlobRelease(&sPath);` |
|        77 |  5872 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  5873 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  5874 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  5875 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5876 | `			return SXERR_ABORT;` |
|         - |  5877 | `		}` |
|         1 |  5878 | `	}` |
|        77 |  5879 | `	return SXRET_OK;` |
|        41 |  5880 | `}` |
|         - |  5881 | `/*` |
|         - |  5882 | ` * Compile the stupid 'declare' language construct.` |
|         - |  5883 | ` *` |
|         - |  5884 | ` * According to the PHP language reference manual.` |
|         - |  5885 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  5886 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  5887 | ` *  declare (directive)` |
|         - |  5888 | ` *   statement` |
|         - |  5889 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  5890 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  5891 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  5892 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  5893 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  5894 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  5895 | ` * <?php` |
|         - |  5896 | ` * // these are the same:` |
|         - |  5897 | ` * // you can use this:` |
|         - |  5898 | ` * declare(ticks=1) {` |
|         - |  5899 | ` *   // entire script here` |
|         - |  5900 | ` * }` |
|         - |  5901 | ` * // or you can use this:` |
|         - |  5902 | ` * declare(ticks=1);` |
|         - |  5903 | ` * // entire script here` |
|         - |  5904 | ` * ?>` |
|         - |  5905 | ` *` |
|         - |  5906 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  5907 | ` */` |
|         - |  5908 | `/*` |
|         - |  5909 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  5910 | ` */` |
|        72 |  5911 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  5912 | `{` |
|       109 |  5913 | `	return SyStringLength(pName) == nWant` |
|        72 |  5914 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  5915 | `}` |
|         - |  5916 |  |
|        42 |  5917 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  5918 | `{` |
|        47 |  5919 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  5920 | `	SyToken *pBodyEnd = 0;` |
|         - |  5921 | `	SyToken *pBodyStart;` |
|         - |  5922 | `	SyToken *pCursor;` |
|         - |  5923 | `	int bHasStrictTypes;` |
|         - |  5924 | `	int bBlockForm;` |
|         - |  5925 | `	int bPlacementOk;` |
|         - |  5926 | `	sxi32 rc;` |
|        47 |  5927 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  5928 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         5 |  5929 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         5 |  5930 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5931 | `			return SXERR_ABORT;` |
|         - |  5932 | `		}` |
|         5 |  5933 | `		goto Synchro;` |
|         - |  5934 | `	}` |
|        43 |  5935 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  5936 | `	pBodyStart = pGen->pIn;` |
|         - |  5937 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  5938 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  5939 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  5940 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  5941 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5942 | `			return SXERR_ABORT;` |
|         - |  5943 | `		}` |
|       ! 0 |  5944 | `		return SXRET_OK;` |
|         - |  5945 | `	}` |
|         - |  5946 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  5947 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  5948 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  5949 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  5950 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  5951 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5952 | `			return SXERR_ABORT;` |
|         - |  5953 | `		}` |
|       ! 0 |  5954 | `	}` |
|        43 |  5955 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  5956 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  5957 | `	bHasStrictTypes = 0;` |
|         - |  5958 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  5959 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  5960 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  5961 | `	pCursor = pBodyStart;` |
|        55 |  5962 | `	while( pCursor < pBodyEnd ){` |
|        51 |  5963 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  5964 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  5965 | `				bHasStrictTypes = 1;` |
|        39 |  5966 | `				break;` |
|         - |  5967 | `			}` |
|         2 |  5968 | `		}` |
|        14 |  5969 | `		pCursor++;` |
|         2 |  5970 | `	}` |
|        43 |  5971 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  5972 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5973 | `			"strict_types declaration must not use block mode");` |
|         3 |  5974 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  5975 | `		return SXRET_OK;` |
|         - |  5976 | `	}` |
|        41 |  5977 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  5978 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5979 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  5980 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  5981 | `		return SXRET_OK;` |
|         - |  5982 | `	}` |
|         - |  5983 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  5984 | `	pCursor = pBodyStart;` |
|        69 |  5985 | `	while( pCursor < pBodyEnd ){` |
|         - |  5986 | `		SyToken *pNameTok;` |
|         - |  5987 | `		SyToken *pEqTok;` |
|         - |  5988 | `		SyToken *pValTok;` |
|         - |  5989 | `		SyString *pDirName;` |
|         - |  5990 | `		int bIsStrict;` |
|         - |  5991 | `		int iStrictValue;` |
|        39 |  5992 | `		pNameTok = pCursor;` |
|        39 |  5993 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  5994 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5995 | `				"declare: Expecting a directive name");` |
|       ! 0 |  5996 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5997 | `			return SXRET_OK;` |
|         - |  5998 | `		}` |
|        39 |  5999 | `		pEqTok = pNameTok + 1;` |
|        39 |  6000 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6001 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6002 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6003 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6004 | `			return SXRET_OK;` |
|         - |  6005 | `		}` |
|        39 |  6006 | `		pValTok = pEqTok + 1;` |
|        39 |  6007 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6008 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6009 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6010 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6011 | `			return SXRET_OK;` |
|         - |  6012 | `		}` |
|        39 |  6013 | `		pDirName = &pNameTok->sData;` |
|        39 |  6014 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6015 | `		if( bIsStrict ){` |
|         - |  6016 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6017 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6018 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6019 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6020 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6021 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6022 | `				return SXRET_OK;` |
|         - |  6023 | `			}` |
|        35 |  6024 | `			iStrictValue = -1;` |
|        35 |  6025 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6026 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6027 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6028 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6029 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6030 | `			}` |
|        35 |  6031 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6032 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6033 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6034 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6035 | `				return SXRET_OK;` |
|         - |  6036 | `			}` |
|        32 |  6037 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6038 | `		}else{` |
|         - |  6039 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6040 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6041 | `			 * behavior don't regress. */` |
|         8 |  6042 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6043 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6044 | `				ph7_lib_version()` |
|         - |  6045 | `				);` |
|         - |  6046 | `		}` |
|        36 |  6047 | `		pCursor = pValTok + 1;` |
|         - |  6048 | `		/* Consume separating comma (or end). */` |
|        36 |  6049 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6050 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6051 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6052 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6053 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6054 | `				return SXRET_OK;` |
|         - |  6055 | `			}` |
|         3 |  6056 | `			pCursor++;` |
|         1 |  6057 | `		}` |
|         4 |  6058 | `	}` |
|         - |  6059 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6060 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6061 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        34 |  6062 | `	return SXRET_OK;` |
|         2 |  6063 | `Synchro:` |
|         - |  6064 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        15 |  6065 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        11 |  6066 | `		pGen->pIn++;` |
|         1 |  6067 | `	}` |
|         5 |  6068 | `	return SXRET_OK;` |
|        26 |  6069 | `}` |
|         - |  6070 | `/*` |
|         - |  6071 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6072 | ` * as follows:` |
|         - |  6073 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6074 | ` * {` |
|         - |  6075 | ` *   return "Making a cup of $type.\n";` |
|         - |  6076 | ` * }` |
|         - |  6077 | ` * Symisc eXtension.` |
|         - |  6078 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6079 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6080 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6081 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6082 | ` *      {` |
|         - |  6083 | ` *       var_dump($a);` |
|         - |  6084 | ` *      }` |
|         - |  6085 | ` *     //call test without args` |
|         - |  6086 | ` *      test();` |
|         - |  6087 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6088 | ` *      Example:` |
|         - |  6089 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6090 | ` * 3 -) Function overloading!!` |
|         - |  6091 | ` *      Example:` |
|         - |  6092 | ` *      function foo($a) {` |
|         - |  6093 | ` *   	  return $a.PHP_EOL;` |
|         - |  6094 | ` *	    }` |
|         - |  6095 | ` *	    function foo($a, $b) {` |
|         - |  6096 | ` *   	  return $a + $b;` |
|         - |  6097 | ` *	    }` |
|         - |  6098 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6099 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6100 | ` *      // Same arg` |
|         - |  6101 | ` *	   function foo(string $a)` |
|         - |  6102 | ` *	   {` |
|         - |  6103 | ` *	     echo "a is a string\n";` |
|         - |  6104 | ` *	     var_dump($a);` |
|         - |  6105 | ` *	   }` |
|         - |  6106 | ` *	  function foo(int $a)` |
|         - |  6107 | ` *	  {` |
|         - |  6108 | ` *	    echo "a is integer\n";` |
|         - |  6109 | ` *	    var_dump($a);` |
|         - |  6110 | ` *	  }` |
|         - |  6111 | ` *	  function foo(array $a)` |
|         - |  6112 | ` *	  {` |
|         - |  6113 | ` * 	    echo "a is an array\n";` |
|         - |  6114 | ` * 	    var_dump($a);` |
|         - |  6115 | ` *	  }` |
|         - |  6116 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6117 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6118 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6119 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6120 | ` * introduced by the PH7 engine.` |
|         - |  6121 | ` */` |
|    457118 |  6122 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6123 | `{` |
|         - |  6124 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6125 | `	SySet *pInstrContainer;` |
|         - |  6126 | `	sxi32 rc;` |
|         - |  6127 | `	/* Swap token stream */` |
|    457123 |  6128 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    457123 |  6129 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    457123 |  6130 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6131 | `	/* Compile the expression holding the argument value */` |
|    457123 |  6132 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6133 | `	/* Emit the done instruction */` |
|    457123 |  6134 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    457123 |  6135 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    457123 |  6136 | `	RE_SWAP_DELIMITER(pGen);` |
|    457123 |  6137 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6138 | `		return SXERR_ABORT;` |
|         - |  6139 | `	}` |
|    457123 |  6140 | `	return SXRET_OK;` |
|    228564 |  6141 | `}` |
|         - |  6142 | `/*` |
|         - |  6143 | ` * Collect function arguments one after one.` |
|         - |  6144 | ` * According to the PHP language reference manual.` |
|         - |  6145 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6146 | ` * list of expressions.` |
|         - |  6147 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6148 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6149 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6150 | ` * for more information.` |
|         - |  6151 | ` * Example #1 Passing arrays to functions` |
|         - |  6152 | ` * <?php` |
|         - |  6153 | ` * function takes_array($input)` |
|         - |  6154 | ` * {` |
|         - |  6155 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6156 | ` * }` |
|         - |  6157 | ` * ?>` |
|         - |  6158 | ` * Making arguments be passed by reference` |
|         - |  6159 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6160 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6161 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6162 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6163 | ` * to the argument name in the function definition:` |
|         - |  6164 | ` * Example #2 Passing function parameters by reference` |
|         - |  6165 | ` * <?php` |
|         - |  6166 | ` * function add_some_extra(&$string)` |
|         - |  6167 | ` * {` |
|         - |  6168 | ` *   $string .= 'and something extra.';` |
|         - |  6169 | ` * }` |
|         - |  6170 | ` * $str = 'This is a string, ';` |
|         - |  6171 | ` * add_some_extra($str);` |
|         - |  6172 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6173 | ` * ?>` |
|         - |  6174 | ` *` |
|         - |  6175 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6176 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6177 | ` * on these extension.` |
|         - |  6178 | ` */` |
|   1103984 |  6179 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6180 | `{` |
|         - |  6181 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6182 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6183 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6184 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6185 | `	sxi32 rc;` |
|         - |  6186 |  |
|   1103989 |  6187 | `	pIn = pGen->pIn;` |
|   1103989 |  6188 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6189 | `	/* Process arguments one after one */` |
|   1384774 |  6190 | `	for(;;){` |
|   2769553 |  6191 | `		if( pIn >= pEnd ){` |
|         - |  6192 | `			/* No more arguments to process */` |
|   1103973 |  6193 | `			break;` |
|         - |  6194 | `		}` |
|   1665585 |  6195 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   1665585 |  6196 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   1665585 |  6197 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   1665585 |  6198 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   1665585 |  6199 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6200 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6201 | `		 * first token inside the main token stream */` |
|   1665585 |  6202 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6203 | `			return SXERR_ABORT;` |
|         - |  6204 | `		}` |
|         - |  6205 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6206 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6207 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6208 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6209 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6210 | `		{` |
|   1665585 |  6211 | `			int bReadonly = 0, bVisSeen = 0;` |
|   1665585 |  6212 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   1665585 |  6213 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6214 | `			int nSetTok;` |
|         - |  6215 | `			sxi32 nSetVis;` |
|   1665585 |  6216 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6217 | `				bReadonly = 1;` |
|         3 |  6218 | `				pIn++;` |
|         1 |  6219 | `			}` |
|   1665585 |  6220 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   1665585 |  6221 | `			if( nSetVis ){` |
|         - |  6222 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6223 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6224 | `				bVisSeen = 1;` |
|         3 |  6225 | `				pIn += nSetTok;` |
|         3 |  6226 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6227 | `					bReadonly = 1;` |
|       ! 0 |  6228 | `					pIn++;` |
|         1 |  6229 | `				}` |
|   1665584 |  6230 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     88719 |  6231 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     88719 |  6232 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6233 | `					bVisSeen = 1;` |
|        89 |  6234 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6235 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6236 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6237 | `					pIn++;` |
|        89 |  6238 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6239 | `					if( nSetVis ){` |
|         - |  6240 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6241 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6242 | `						pIn += nSetTok;` |
|         1 |  6243 | `					}` |
|        89 |  6244 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6245 | `						bReadonly = 1;` |
|        18 |  6246 | `						pIn++;` |
|         7 |  6247 | `					}` |
|        42 |  6248 | `				}` |
|     44357 |  6249 | `			}` |
|   1665585 |  6250 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6251 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   1665583 |  6252 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6253 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6254 | `			}` |
|   1665585 |  6255 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6256 | `				if( !bCtorCtx ){` |
|         6 |  6257 | `					if( bAbstractCtx ){` |
|         3 |  6258 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6259 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6260 | `					}else{` |
|         3 |  6261 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6262 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6263 | `					}` |
|         6 |  6264 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6265 | `						return SXERR_ABORT;` |
|         - |  6266 | `					}` |
|         6 |  6267 | `					return SXERR_SYNTAX;` |
|         - |  6268 | `				}` |
|        89 |  6269 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6270 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6271 | `				if( bReadonly ){` |
|        20 |  6272 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6273 | `				}` |
|        42 |  6274 | `			}` |
|         - |  6275 | `		}` |
|         - |  6276 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   1665576 |  6277 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|    906062 |  6278 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    140776 |  6279 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    115717 |  6280 | `			sxu32 nLineLocal = pIn->nLine;` |
|    115717 |  6281 | `			sxi32 iTFlags = 0;` |
|    115717 |  6282 | `			pGen->pIn = pIn;` |
|    115717 |  6283 | `			rc = GenStateParseUnionTypeDecl(` |
|     57856 |  6284 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57856 |  6285 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6286 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6287 | `				/* bAllowVoid */ 0,` |
|     57856 |  6288 | `						nLineLocal);` |
|    115717 |  6289 | `			pIn = pGen->pIn;` |
|    115717 |  6290 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6291 | `				return SXERR_ABORT;` |
|    115717 |  6292 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6293 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6294 | `				return SXERR_SYNTAX;` |
|    115715 |  6295 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6296 | `				if( pIn < pEnd ){` |
|        15 |  6297 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6298 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6299 | `						&pIn->sData);` |
|         7 |  6300 | `				}else{` |
|       ! 0 |  6301 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6302 | `						"syntax error, unexpected end of file");` |
|         - |  6303 | `				}` |
|        11 |  6304 | `				return SXERR_SYNTAX;` |
|         - |  6305 | `			}` |
|    115707 |  6306 | `			sArg.iFlags \|= iTFlags;` |
|     57851 |  6307 | `		}` |
|   1665571 |  6308 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6309 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6310 | `			return rc;` |
|         - |  6311 | `		}` |
|   1665571 |  6312 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6313 | `			/* Pass by reference,record that */` |
|     11567 |  6314 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     11567 |  6315 | `			pIn++;` |
|      5781 |  6316 | `		}` |
|   1665571 |  6317 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6318 | `			/* Variadic parameter: ...$args */` |
|     19309 |  6319 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     19309 |  6320 | `			pIn++;` |
|      9652 |  6321 | `		}` |
|   1665571 |  6322 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6323 | `			/* Invalid argument */` |
|       ! 0 |  6324 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6325 | `			return rc;` |
|         - |  6326 | `		}` |
|   1665571 |  6327 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6328 | `		/* Copy argument name */` |
|   1665571 |  6329 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   1665571 |  6330 | `		if( zDup == 0 ){` |
|       ! 0 |  6331 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6332 | `			return SXERR_ABORT;` |
|         - |  6333 | `		}` |
|   1665571 |  6334 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   1665571 |  6335 | `		pIn++;` |
|   1665571 |  6336 | `		if( pIn < pEnd ){` |
|    861237 |  6337 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6338 | `				SyToken *pDefend;` |
|    457125 |  6339 | `				sxi32 iNest = 0;` |
|    457125 |  6340 | `				pIn++; /* Jump the equal sign */` |
|    457125 |  6341 | `				pDefend = pIn;` |
|         - |  6342 | `				/* Process the default value associated with this argument */` |
|    964191 |  6343 | `				while( pDefend < pEnd ){` |
|    664561 |  6344 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    157495 |  6345 | `						break;` |
|         - |  6346 | `					}` |
|    507071 |  6347 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6348 | `						/* Increment nesting level */` |
|     26893 |  6349 | `						iNest++;` |
|    493627 |  6350 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6351 | `						/* Decrement nesting level */` |
|     26893 |  6352 | `						iNest--;` |
|     13444 |  6353 | `					}` |
|    507071 |  6354 | `					pDefend++;` |
|         5 |  6355 | `				}` |
|    457125 |  6356 | `				if( pIn >= pDefend ){` |
|         3 |  6357 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6358 | `					return rc;` |
|         - |  6359 | `				}` |
|         - |  6360 | `				/* Process default value */` |
|    457123 |  6361 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    457123 |  6362 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6363 | `					return rc;` |
|         - |  6364 | `				}` |
|         - |  6365 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6366 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6367 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6368 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6369 | `				 * arg-type check lets null through. */` |
|    457118 |  6370 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    253542 |  6371 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    253539 |  6372 | `					&& &pIn[1] == pDefend` |
|     46119 |  6373 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     34582 |  6374 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     21131 |  6375 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15371 |  6376 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6377 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6378 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6379 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6380 | `					 * already up at this point). */` |
|         - |  6381 | `					{` |
|     15371 |  6382 | `						const char *zSep = "";` |
|     15371 |  6383 | `						SyString sCls = { "", 0 };` |
|     15371 |  6384 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15365 |  6385 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15365 |  6386 | `							zSep = "::";` |
|      7680 |  6387 | `						}` |
|     23054 |  6388 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6389 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7683 |  6390 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6391 | `					}` |
|      7683 |  6392 | `				}` |
|         - |  6393 | `				/* Point beyond the default value */` |
|    457123 |  6394 | `				pIn = pDefend;` |
|    228559 |  6395 | `			}` |
|    861235 |  6396 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6397 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6398 | `				return rc;` |
|         - |  6399 | `			}` |
|    861235 |  6400 | `			pIn++; /* Jump the trailing comma */` |
|    430615 |  6401 | `		}` |
|         - |  6402 | `		/* Append argument signature */` |
|   1665569 |  6403 | `		if( sArg.nType > 0 ){` |
|    115645 |  6404 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6405 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26965 |  6406 | `				int marker = 'o';` |
|     26965 |  6407 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26965 |  6408 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13485 |  6409 | `			}else{` |
|         - |  6410 | `				int c;` |
|     88685 |  6411 | `				c = 'n'; /* cc warning */` |
|         - |  6412 | `				/* Type leading character */` |
|     88685 |  6413 | `				switch(sArg.nType){` |
|      5766 |  6414 | `				case MEMOBJ_HASHMAP:` |
|         - |  6415 | `					/* Hashmap aka 'array' */` |
|     11537 |  6416 | `					c = 'h';` |
|     11537 |  6417 | `					break;` |
|      9720 |  6418 | `				case MEMOBJ_INT:` |
|         - |  6419 | `					/* Integer */` |
|     19445 |  6420 | `					c = 'i';` |
|     19445 |  6421 | `					break;` |
|         2 |  6422 | `				case MEMOBJ_BOOL:` |
|         - |  6423 | `					/* Bool */` |
|         5 |  6424 | `					c = 'b';` |
|         5 |  6425 | `					break;` |
|         5 |  6426 | `				case MEMOBJ_REAL:` |
|         - |  6427 | `					/* Float */` |
|        12 |  6428 | `					c = 'f';` |
|        12 |  6429 | `					break;` |
|     28839 |  6430 | `				case MEMOBJ_STRING:` |
|         - |  6431 | `					/* String */` |
|     57683 |  6432 | `					c = 's';` |
|     57683 |  6433 | `					break;` |
|         7 |  6434 | `				case MEMOBJ_OBJ:` |
|         - |  6435 | `					/* Object */` |
|        16 |  6436 | `					c = 'o';` |
|        14 |  6437 | `					break;` |
|         1 |  6438 | `				default:` |
|         2 |  6439 | `					break;` |
|         - |  6440 | `				}` |
|     88685 |  6441 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6442 | `			}` |
|     57825 |  6443 | `		}else{` |
|         - |  6444 | `			/* No type is associated with this parameter which mean` |
|         - |  6445 | `			 * that this function is not condidate for overloading.` |
|         - |  6446 | `			 */` |
|   1549929 |  6447 | `			SyBlobRelease(&sSig);` |
|         - |  6448 | `		}` |
|         - |  6449 | `		/* Save in the argument set */` |
|   1665569 |  6450 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6451 | `	}` |
|   1103973 |  6452 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6453 | `		/* Save function signature */` |
|     84853 |  6454 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     42424 |  6455 | `	}` |
|   1103973 |  6456 | `	return SXRET_OK;` |
|    551997 |  6457 | `}` |
|         - |  6458 | `/*` |
|         - |  6459 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6460 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6461 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6462 | ` */` |
|     34602 |  6463 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6464 | `{` |
|     34607 |  6465 | `	sxi32 iParen = 0;` |
|     34607 |  6466 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6467 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6468 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6469 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    153833 |  6470 | `	while( pIn < pEnd ){` |
|    153833 |  6471 | `		sxu32 t = pIn->nType;` |
|    153833 |  6472 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    149939 |  6473 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    103805 |  6474 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     84563 |  6475 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    119231 |  6476 | `		pIn++;` |
|         5 |  6477 | `	}` |
|     19247 |  6478 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6479 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6480 | `	{` |
|     19247 |  6481 | `		sxi32 d = 0;` |
|    764585 |  6482 | `		while( pIn < pEnd ){` |
|    764585 |  6483 | `			sxu32 t = pIn->nType;` |
|    764585 |  6484 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    733819 |  6485 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    745343 |  6486 | `			pIn++;` |
|         5 |  6487 | `		}` |
|         - |  6488 | `	}` |
|     19247 |  6489 | `	return pIn;` |
|     17306 |  6490 | `}` |
|         - |  6491 | `/*` |
|         - |  6492 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6493 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6494 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6495 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6496 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6497 | ` * detached-mini-program path untouched.` |
|         - |  6498 | ` */` |
|         - |  6499 | `/*` |
|         - |  6500 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6501 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6502 | ` * mixed, object.` |
|         - |  6503 | ` */` |
|     11548 |  6504 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6505 | `{` |
|         - |  6506 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6507 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6508 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6509 | `	};` |
|         - |  6510 | `	sxu32 i;` |
|     11553 |  6511 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6512 | `		zName++;` |
|       ! 0 |  6513 | `		nName--;` |
|       ! 0 |  6514 | `	}` |
|     11585 |  6515 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11581 |  6516 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11549 |  6517 | `			return 1;` |
|         - |  6518 | `		}` |
|        17 |  6519 | `	}` |
|         5 |  6520 | `	return 0;` |
|      5779 |  6521 | `}` |
|         - |  6522 | `/*` |
|         - |  6523 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6524 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6525 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6526 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6527 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6528 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6529 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6530 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6531 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6532 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6533 | ` */` |
|     11546 |  6534 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6535 | `{` |
|     11551 |  6536 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6537 | ``		return 1; /* bare `object` */`` |
|         - |  6538 | `	}` |
|     11551 |  6539 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6540 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6541 | `	}` |
|     11549 |  6542 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11545 |  6543 | `		return 1;` |
|         - |  6544 | `	}` |
|         - |  6545 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6546 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6547 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6548 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6549 | `	{` |
|         - |  6550 | `		SyBlob sFQN;` |
|         - |  6551 | `		int bOk;` |
|         5 |  6552 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         5 |  6553 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|         5 |  6554 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|         5 |  6555 | `		SyBlobRelease(&sFQN);` |
|         5 |  6556 | `		return bOk;` |
|         - |  6557 | `	}` |
|      5778 |  6558 | `}` |
|         - |  6559 | `/*` |
|         - |  6560 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6561 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6562 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6563 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6564 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6565 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6566 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6567 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6568 | ` */` |
|     11784 |  6569 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6570 | `{` |
|     11789 |  6571 | `	int bOk = 0;` |
|         - |  6572 | `	sxu32 nLine;` |
|         - |  6573 | `	sxi32 rc;` |
|     11789 |  6574 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       243 |  6575 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6576 | `	}` |
|     11551 |  6577 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6578 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6579 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6580 | `		sxu32 i,j;` |
|       ! 0 |  6581 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6582 | `			int bGroupOk;` |
|       ! 0 |  6583 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6584 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6585 | `			}` |
|       ! 0 |  6586 | `			bGroupOk = 1;` |
|       ! 0 |  6587 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6588 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6589 | `					bGroupOk = 0;` |
|       ! 0 |  6590 | `					break;` |
|         - |  6591 | `				}` |
|       ! 0 |  6592 | `			}` |
|       ! 0 |  6593 | `			bOk = bGroupOk;` |
|       ! 0 |  6594 | `		}` |
|       ! 0 |  6595 | `	}else{` |
|     11551 |  6596 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6597 | `	}` |
|     11551 |  6598 | `	if( bOk ){` |
|     11549 |  6599 | `		return SXRET_OK;` |
|         - |  6600 | `	}` |
|         - |  6601 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6602 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6603 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6604 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6605 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6606 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6607 | `	{` |
|         3 |  6608 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6609 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6610 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6611 | `		}` |
|         3 |  6612 | `		if( sGiven.nByte < 1 ){` |
|         - |  6613 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6614 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6615 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6616 | `			const char *zScalar =` |
|       ! 0 |  6617 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6618 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6619 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6620 | `		}` |
|         3 |  6621 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6622 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6623 | `	}` |
|         3 |  6624 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5897 |  6625 | `}` |
|   2560974 |  6626 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6627 | `{` |
|   2560979 |  6628 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2560979 |  6629 | `	SyToken *pEnd = pGen->pEnd;` |
|   2560979 |  6630 | `	sxi32 iDepth = 0;` |
|   2560979 |  6631 | `	int bStarted = 0;` |
| 113020787 |  6632 | `	while( pIn < pEnd ){` |
| 113020787 |  6633 | `		sxu32 t = pIn->nType;` |
| 113020787 |  6634 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 107836129 |  6635 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 102686409 |  6636 | `		if( t & PH7_TK_KEYWORD ){` |
|   7493273 |  6637 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   7493273 |  6638 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   7481489 |  6639 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6640 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   3723441 |  6641 | `		}` |
| 102640023 |  6642 | `		pIn++;` |
|         5 |  6643 | `	}` |
|   2549195 |  6644 | `	return FALSE;` |
|   1280492 |  6645 | `}` |
|         - |  6646 | `/*` |
|         - |  6647 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6648 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6649 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6650 | ` */` |
|   2560974 |  6651 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6652 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6653 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6654 | `	)` |
|         5 |  6655 | `{` |
|         - |  6656 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6657 | `	GenBlock *pBlock;` |
|         - |  6658 | `	sxu32 nGotoOfft;` |
|         - |  6659 | `	sxi32 rc;` |
|         - |  6660 | `	/* Attach the new function */` |
|   2560979 |  6661 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2560979 |  6662 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6663 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6664 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6665 | `		return SXERR_ABORT;` |
|         - |  6666 | `	}` |
|   2560979 |  6667 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6668 | `	/* Swap bytecode containers */` |
|   2560979 |  6669 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2560979 |  6670 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6671 | `	/* Emit constructor property promotion prologue:` |
|         - |  6672 | `	 *   $this->NAME = $NAME;` |
|         - |  6673 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6674 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6675 | `	{` |
|   2560979 |  6676 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6677 | `		sxu32 i;` |
|   4172657 |  6678 | `		for( i = 0; i < nArg; i++ ){` |
|   1611683 |  6679 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6680 | `			char *zSrc;` |
|         - |  6681 | `			sxu32 nSrc,nName;` |
|         - |  6682 | `			SySet sToken;` |
|         - |  6683 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6684 | `			sxi32 rcPromote;` |
|   1611683 |  6685 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1611609 |  6686 | `				continue;` |
|         - |  6687 | `			}` |
|         - |  6688 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6689 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6690 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6691 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6692 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6693 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6694 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6695 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6696 | `			if( zSrc == 0 ){` |
|       ! 0 |  6697 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6698 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6699 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6700 | `				return SXERR_ABORT;` |
|         - |  6701 | `			}` |
|         - |  6702 | `			{` |
|        79 |  6703 | `				char *z = zSrc;` |
|        79 |  6704 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6705 | `				z += sizeof("$this->")-1;` |
|        79 |  6706 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6707 | `				z += nName;` |
|        79 |  6708 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6709 | `				z += sizeof(" = $")-1;` |
|        79 |  6710 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6711 | `				z += nName;` |
|        79 |  6712 | `				*z = 0;` |
|         - |  6713 | `			}` |
|        79 |  6714 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6715 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6716 | `			pTmpIn = pGen->pIn;` |
|        79 |  6717 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6718 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6719 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6720 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6721 | `			pGen->pIn = pTmpIn;` |
|        79 |  6722 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6723 | `			SySetRelease(&sToken);` |
|        79 |  6724 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6725 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6726 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6727 | `				return SXERR_ABORT;` |
|         - |  6728 | `			}` |
|         - |  6729 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6730 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6731 | `		}` |
|         - |  6732 | `	}` |
|         - |  6733 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6734 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6735 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6736 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6737 | `	{` |
|   2560979 |  6738 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2560979 |  6739 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6740 | `		/* Compile the body */` |
|   2560979 |  6741 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2560979 |  6742 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6743 | `	}` |
|         - |  6744 | `	/* Fix exception jumps now the destination is resolved */` |
|   2560979 |  6745 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6746 | `	/* Emit the final return if not yet done */` |
|   2560979 |  6747 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6748 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2560979 |  6749 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6750 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6751 | `	}` |
|   2560979 |  6752 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6753 | `	/* Restore the default container */` |
|   2560979 |  6754 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6755 | `	/* Leave function block */` |
|   2560979 |  6756 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2560979 |  6757 | `	if( rc == SXERR_ABORT ){` |
|         - |  6758 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6759 | `		return SXERR_ABORT;` |
|         - |  6760 | `	}` |
|         - |  6761 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6762 | `	{` |
|   2560979 |  6763 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6764 | `		sxu32 i;` |
|  69987647 |  6765 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  67438457 |  6766 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11789 |  6767 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11789 |  6768 | `				break;` |
|         - |  6769 | `			}` |
|  33713339 |  6770 | `		}` |
|         - |  6771 | `	}` |
|   2560979 |  6772 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6773 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11789 |  6774 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6775 | `			return SXERR_ABORT;` |
|         - |  6776 | `		}` |
|      5892 |  6777 | `	}` |
|         - |  6778 | `	/* All done, function body compiled */` |
|   2560979 |  6779 | `	return SXRET_OK;` |
|   1280492 |  6780 | `}` |
|         - |  6781 | `/*` |
|         - |  6782 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  6783 | ` * According to the PHP language reference manual.` |
|         - |  6784 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  6785 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  6786 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  6787 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  6788 | ` *  Functions need not be defined before they are referenced.` |
|         - |  6789 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  6790 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  6791 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  6792 | ` *  calls with over 32-64 recursion levels.` |
|         - |  6793 | ` *` |
|         - |  6794 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  6795 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  6796 | ` * on these extension.` |
|         - |  6797 | ` */` |
|         - |  6798 | `/*` |
|         - |  6799 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  6800 | ` */` |
|       570 |  6801 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  6802 | `{` |
|         - |  6803 | `	sxu32 i;` |
|      1611 |  6804 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  6805 | `		int a = zA[i], b = zB[i];` |
|      1381 |  6806 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  6807 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  6808 | `		if( a != b ) return a - b;` |
|       523 |  6809 | `	}` |
|       235 |  6810 | `	return 0;` |
|       290 |  6811 | `}` |
|         - |  6812 | `/*` |
|         - |  6813 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  6814 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  6815 | ` * (which are positive bit values stored in sxu32).` |
|         - |  6816 | ` */` |
|         - |  6817 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  6818 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  6819 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  6820 |  |
|         - |  6821 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  6822 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  6823 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  6824 |  |
|         - |  6825 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  6826 | `struct PhlTypeAtom {` |
|         - |  6827 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  6828 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  6829 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  6830 | `	sxu32 nCanon;` |
|         - |  6831 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  6832 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  6833 | `};` |
|         - |  6834 |  |
|         - |  6835 | `/*` |
|         - |  6836 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  6837 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  6838 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  6839 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  6840 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  6841 | ` * already be consumed by the caller.` |
|         - |  6842 | ` */` |
|    128412 |  6843 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  6844 | `{` |
|    128417 |  6845 | `	SyToken *pIn = pGen->pIn;` |
|    128417 |  6846 | `	SyZero(pOut, sizeof(*pOut));` |
|    128417 |  6847 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    128417 |  6848 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6849 | `		return SXERR_SYNTAX;` |
|         - |  6850 | `	}` |
|         - |  6851 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    128417 |  6852 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|         8 |  6853 | `		pIn++;` |
|         8 |  6854 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6855 | `			return SXERR_SYNTAX;` |
|         - |  6856 | `		}` |
|         3 |  6857 | `	}` |
|    128417 |  6858 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6859 | `		return SXERR_SYNTAX;` |
|         - |  6860 | `	}` |
|    128417 |  6861 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     89477 |  6862 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     89477 |  6863 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11569 |  6864 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     83695 |  6865 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        81 |  6866 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     77875 |  6867 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19851 |  6868 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67914 |  6869 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57909 |  6870 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     29039 |  6871 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  6872 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        67 |  6873 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        27 |  6874 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        37 |  6875 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        13 |  6876 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        23 |  6877 | `			pOut->nType = SXU32_HIGH;` |
|        23 |  6878 | `			pOut->sClass = pIn->sData;` |
|        13 |  6879 | `		}else{` |
|         3 |  6880 | `			return SXERR_SYNTAX;` |
|         - |  6881 | `		}` |
|     89475 |  6882 | `		pIn++;` |
|     44740 |  6883 | `	}else{` |
|         - |  6884 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  6885 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38945 |  6886 | `		SyString *pT = &pIn->sData;` |
|     38945 |  6887 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  6888 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  6889 | `			pIn++;` |
|     38930 |  6890 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  6891 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  6892 | `			pIn++;` |
|     38829 |  6893 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        26 |  6894 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        26 |  6895 | `			pIn++;` |
|        15 |  6896 | `		}else{` |
|         - |  6897 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38721 |  6898 | `			SyToken *pFirst = pIn;` |
|     38721 |  6899 | `			SyToken *pLast = pIn;` |
|     38721 |  6900 | `			pOut->nType = SXU32_HIGH;` |
|     38721 |  6901 | `			pOut->sClass = pIn->sData;` |
|     38721 |  6902 | `			pIn++;` |
|     58077 |  6903 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38724 |  6904 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  6905 | `				pLast = &pIn[1];` |
|         3 |  6906 | `				pIn += 2;` |
|         1 |  6907 | `			}` |
|     38721 |  6908 | `			if( pLast != pFirst ){` |
|         3 |  6909 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  6910 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  6911 | `				pOut->sClass.zString = zFirst;` |
|         3 |  6912 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  6913 | `			}` |
|         - |  6914 | `		}` |
|         - |  6915 | `	}` |
|    128415 |  6916 | `	pGen->pIn = pIn;` |
|    128415 |  6917 | `	return SXRET_OK;` |
|     64211 |  6918 | `}` |
|         - |  6919 |  |
|         - |  6920 | `/*` |
|         - |  6921 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  6922 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  6923 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  6924 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  6925 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  6926 | ` */` |
|    128234 |  6927 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  6928 | `{` |
|         - |  6929 | `	int i;` |
|    128239 |  6930 | `	int nNonNull = 0;` |
|    128239 |  6931 | `	int bAnyIntersection = 0;` |
|         - |  6932 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    128239 |  6933 | `	sxu32 nMaxGroup = 0;` |
|   4231727 |  6934 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    256625 |  6935 | `	for( i = 0; i < nAtoms; i++ ){` |
|    128391 |  6936 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    128361 |  6937 | `			nNonNull++;` |
|    128361 |  6938 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    128361 |  6939 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    128361 |  6940 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     64178 |  6941 | `			}` |
|     64178 |  6942 | `		}` |
|     64198 |  6943 | `	}` |
|    256573 |  6944 | `	for( i = 0; i < nAtoms; i++ ){` |
|    128363 |  6945 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  6946 | `			bAnyIntersection = 1;` |
|        29 |  6947 | `			break;` |
|         - |  6948 | `		}` |
|     64172 |  6949 | `	}` |
|    128239 |  6950 | `	if( bAnyIntersection ){` |
|         - |  6951 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  6952 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  6953 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  6954 | `		sxu32 g, nGroups = 0;` |
|        29 |  6955 | `		int bFirstGroup = 1;` |
|        59 |  6956 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  6957 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  6958 | `			int bFirstMember = 1;` |
|         - |  6959 | `			int bWrap;` |
|        35 |  6960 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  6961 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  6962 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  6963 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  6964 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  6965 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  6966 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  6967 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  6968 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  6969 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  6970 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  6971 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  6972 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  6973 | `				}else{` |
|         6 |  6974 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  6975 | `				}` |
|        59 |  6976 | `				bFirstMember = 0;` |
|        32 |  6977 | `			}` |
|        35 |  6978 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  6979 | `			bFirstGroup = 0;` |
|        20 |  6980 | `		}` |
|        29 |  6981 | `		if( bNullable ){` |
|       ! 0 |  6982 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  6983 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  6984 | `		}` |
|        83 |  6985 | `		return;` |
|         - |  6986 | `	}` |
|    128215 |  6987 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  6988 | `		/* Shorthand: ?T */` |
|       112 |  6989 | `		for( i = 0; i < nAtoms; i++ ){` |
|       112 |  6990 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       112 |  6991 | `			SyBlobAppend(pBlob, "?", 1);` |
|       112 |  6992 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        23 |  6993 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  6994 | `			}else{` |
|        92 |  6995 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  6996 | `			}` |
|       112 |  6997 | `			return;` |
|       ! 0 |  6998 | `		}` |
|       ! 0 |  6999 | `	}` |
|         - |  7000 | `	{` |
|    128107 |  7001 | `		int bFirst = 1;` |
|         - |  7002 | `		/* 1) Classes in declaration order */` |
|    256317 |  7003 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128215 |  7004 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38671 |  7005 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38671 |  7006 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38671 |  7007 | `				bFirst = 0;` |
|     19333 |  7008 | `			}` |
|     64110 |  7009 | `		}` |
|         - |  7010 | `		/* 2) Built-ins in canonical order */` |
|         - |  7011 | `		{` |
|         - |  7012 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7013 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7014 | `			int k;` |
|    896719 |  7015 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1448423 |  7016 | `				for( i = 0; i < nAtoms; i++ ){` |
|    769153 |  7017 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     89347 |  7018 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     89347 |  7019 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     89347 |  7020 | `						bFirst = 0;` |
|     89347 |  7021 | `						break;` |
|         - |  7022 | `					}` |
|    339908 |  7023 | `				}` |
|    384311 |  7024 | `			}` |
|         - |  7025 | `		}` |
|         - |  7026 | `		/* 3) null suffix */` |
|    128107 |  7027 | `		if( bNullable ){` |
|        19 |  7028 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        19 |  7029 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7030 | `		}` |
|         - |  7031 | `	}` |
|     64122 |  7032 | `}` |
|         - |  7033 |  |
|         - |  7034 | `/*` |
|         - |  7035 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7036 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7037 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7038 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7039 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7040 | ` * whether it was parenthesized.` |
|         - |  7041 | ` *` |
|         - |  7042 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7043 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7044 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7045 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7046 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7047 | ` */` |
|    128386 |  7048 | `static sxi32 GenStateParsePart(` |
|         - |  7049 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7050 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7051 | `{` |
|         - |  7052 | `	sxi32 rc;` |
|    128391 |  7053 | `	int nMembers = 0;` |
|    128391 |  7054 | `	int bParen = 0;` |
|    128391 |  7055 | `	*pnMembers = 0;` |
|    128391 |  7056 | `	*pbParen = 0;` |
|    128391 |  7057 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7058 | `		bParen = 1;` |
|         9 |  7059 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7060 | `	}` |
|     64193 |  7061 | `	for(;;){` |
|    128417 |  7062 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7063 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7064 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7065 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7066 | `		}` |
|    128417 |  7067 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    128417 |  7068 | `		if( rc != SXRET_OK ){` |
|         3 |  7069 | `			return rc;` |
|         - |  7070 | `		}` |
|    128415 |  7071 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    128415 |  7072 | `		(*pnAtoms)++;` |
|    128415 |  7073 | `		nMembers++;` |
|         - |  7074 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    128415 |  7075 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7076 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7077 | `			if( pNext < pGen->pEnd` |
|        39 |  7078 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7079 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7080 | `				continue;` |
|         - |  7081 | `			}` |
|         4 |  7082 | `		}` |
|    128389 |  7083 | `		break;` |
|       ! 0 |  7084 | `	}` |
|    128389 |  7085 | `	if( bParen ){` |
|         9 |  7086 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7087 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7088 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7089 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7090 | `		}` |
|         9 |  7091 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7092 | `		if( nMembers < 2 ){` |
|       ! 0 |  7093 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7094 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7095 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7096 | `		}` |
|         3 |  7097 | `	}` |
|    128389 |  7098 | `	*pnMembers = nMembers;` |
|    128389 |  7099 | `	*pbParen = bParen;` |
|    128389 |  7100 | `	return SXRET_OK;` |
|     64198 |  7101 | `}` |
|         - |  7102 |  |
|         - |  7103 | `/*` |
|         - |  7104 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7105 | ` *` |
|         - |  7106 | ` * Outputs:` |
|         - |  7107 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7108 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7109 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7110 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7111 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7112 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7113 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7114 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7115 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7116 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7117 | ` *` |
|         - |  7118 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7119 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7120 | ` */` |
|    128250 |  7121 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7122 | `	ph7_gen_state *pGen,` |
|         - |  7123 | `	sxu32 *pnType,` |
|         - |  7124 | `	SyString *pClass,` |
|         - |  7125 | `	SySet *pAlts,` |
|         - |  7126 | `	sxi32 *piTypeFlags,` |
|         - |  7127 | `	SyString *pTypeText,` |
|         - |  7128 | `	int iNullableFlag,` |
|         - |  7129 | `	int iUnionFlag,` |
|         - |  7130 | `	int bAllowVoid,` |
|         - |  7131 | `	sxu32 nLine` |
|         5 |  7132 | `){` |
|         - |  7133 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    128255 |  7134 | `	int nAtoms = 0;` |
|    128255 |  7135 | `	int bShortNullable = 0;` |
|    128255 |  7136 | `	int bExplicitNull = 0;` |
|         - |  7137 | `	sxi32 rc;` |
|    128255 |  7138 | `	*pnType = 0;` |
|    128255 |  7139 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    128255 |  7140 | `	*piTypeFlags = 0;` |
|    128255 |  7141 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7142 |  |
|    128255 |  7143 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7144 | `		return SXRET_OK;` |
|         - |  7145 | `	}` |
|         - |  7146 | ``	/* Optional `?` shorthand prefix */`` |
|    128250 |  7147 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       101 |  7148 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       100 |  7149 | `		bShortNullable = 1;` |
|       100 |  7150 | `		pGen->pIn++;` |
|       100 |  7151 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7152 | `			return SXERR_SYNTAX;` |
|         - |  7153 | `		}` |
|        48 |  7154 | `	}` |
|         - |  7155 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7156 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7157 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7158 | `	{` |
|         - |  7159 | `		int nMembers, bParen;` |
|    128255 |  7160 | `		sxu32 iGroup = 0;` |
|    128255 |  7161 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    128255 |  7162 | `		if( rc != SXRET_OK ){` |
|         4 |  7163 | `			return rc;` |
|         - |  7164 | `		}` |
|         - |  7165 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7166 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7167 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7168 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7169 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    192581 |  7170 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    128462 |  7171 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7172 | `			if( bShortNullable ){` |
|         - |  7173 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7174 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7175 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7176 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7177 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7178 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7179 | `			}` |
|       141 |  7180 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7181 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7182 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7183 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7184 | `			}` |
|       141 |  7185 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7186 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7187 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7188 | `				return rc;` |
|         - |  7189 | `			}` |
|         5 |  7190 | `		}` |
|    128251 |  7191 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7192 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7193 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7194 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7195 | `		}` |
|         - |  7196 | `	}` |
|         - |  7197 | `	/* Validation pass.` |
|         - |  7198 | `	 *` |
|         - |  7199 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7200 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7201 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7202 | `	 */` |
|         - |  7203 | `	{` |
|         - |  7204 | `		int i, j;` |
|    128251 |  7205 | `		int bHasNonNull = 0;` |
|    128251 |  7206 | `		int bAnyIntersection = 0;` |
|         - |  7207 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7208 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7209 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4232123 |  7210 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    256659 |  7211 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128413 |  7212 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     64209 |  7213 | `		}` |
|    256603 |  7214 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128383 |  7215 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     64181 |  7216 | `		}` |
|         - |  7217 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7218 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    128251 |  7219 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7220 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7221 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7222 | `			return SXERR_SYNTAX;` |
|         - |  7223 | `		}` |
|    256645 |  7224 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7225 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7226 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7227 | ``			 * `true`/`false` in an intersection). */`` |
|    128411 |  7228 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7229 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7230 | `				if( bClassLike ){` |
|        53 |  7231 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7232 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7233 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7234 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7235 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7236 | `						bClassLike = 0;` |
|       ! 0 |  7237 | `					}` |
|        24 |  7238 | `				}` |
|        55 |  7239 | `				if( !bClassLike ){` |
|         - |  7240 | `					const char *zName; sxu32 nName;` |
|         3 |  7241 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7242 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7243 | `					}else{` |
|         3 |  7244 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7245 | `					}` |
|         4 |  7246 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7247 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7248 | `						(int)nName, zName);` |
|         3 |  7249 | `					return SXERR_SYNTAX;` |
|         - |  7250 | `				}` |
|        24 |  7251 | `			}` |
|    128409 |  7252 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7253 | `				if( nAtoms > 1 ){` |
|         3 |  7254 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7255 | `						"Void can only be used as a standalone type");` |
|         3 |  7256 | `					return SXERR_SYNTAX;` |
|         - |  7257 | `				}` |
|       175 |  7258 | `				if( !bAllowVoid ){` |
|       ! 0 |  7259 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7260 | `						"void cannot be used here");` |
|       ! 0 |  7261 | `					return SXERR_SYNTAX;` |
|         - |  7262 | `				}` |
|       175 |  7263 | `				if( bShortNullable ){` |
|       ! 0 |  7264 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7265 | `						"Void type cannot be nullable");` |
|       ! 0 |  7266 | `					return SXERR_SYNTAX;` |
|         - |  7267 | `				}` |
|        85 |  7268 | `			}` |
|    128407 |  7269 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7270 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7271 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7272 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7273 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        26 |  7274 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7275 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7276 | `					 * same as any other non-standalone use. */` |
|         5 |  7277 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7278 | `						"never can only be used as a standalone type");` |
|         5 |  7279 | `					return SXERR_SYNTAX;` |
|         - |  7280 | `				}` |
|        21 |  7281 | `				if( !bAllowVoid ){` |
|         - |  7282 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7283 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7284 | `						"never cannot be used as a parameter type");` |
|         3 |  7285 | `					return SXERR_SYNTAX;` |
|         - |  7286 | `				}` |
|         8 |  7287 | `			}` |
|    128401 |  7288 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7289 | `				bExplicitNull = 1;` |
|        19 |  7290 | `			}else{` |
|    128371 |  7291 | `				bHasNonNull = 1;` |
|         - |  7292 | `			}` |
|         - |  7293 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7294 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7295 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7296 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7297 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    128601 |  7298 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7299 | `				int bDup = 0;` |
|       207 |  7300 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7301 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7302 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7303 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7304 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7305 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7306 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7307 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7308 | `								aAtoms[j].sClass.zString,` |
|        34 |  7309 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7310 | `							bDup = 1;` |
|       ! 0 |  7311 | `						}` |
|        27 |  7312 | `					}else{` |
|         3 |  7313 | `						bDup = 1;` |
|         - |  7314 | `					}` |
|        23 |  7315 | `				}` |
|       195 |  7316 | `				if( bDup ){` |
|         - |  7317 | `					const char *zName;` |
|         - |  7318 | `					sxu32 nName;` |
|         3 |  7319 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7320 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7321 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7322 | `					}else{` |
|         3 |  7323 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7324 | `						nName = aAtoms[i].nCanon;` |
|         - |  7325 | `					}` |
|         4 |  7326 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7327 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7328 | `					return SXERR_SYNTAX;` |
|         - |  7329 | `				}` |
|        99 |  7330 | `			}` |
|     64202 |  7331 | `		}` |
|    128239 |  7332 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7333 | `			if( bShortNullable ){` |
|         - |  7334 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7335 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7336 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7337 | `				return SXERR_SYNTAX;` |
|         - |  7338 | `			}` |
|         - |  7339 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7340 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7341 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7342 | `			 * atom, so set it here. */` |
|         7 |  7343 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7344 | `		}` |
|         - |  7345 | `	}` |
|         - |  7346 | `	/* Compute nullability flag */` |
|    128239 |  7347 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       128 |  7348 | `		*piTypeFlags \|= iNullableFlag;` |
|        62 |  7349 | `	}` |
|         - |  7350 | `	/* Build canonical type text */` |
|    128239 |  7351 | `	if( pTypeText ){` |
|         - |  7352 | `		SyBlob sBlob;` |
|    128239 |  7353 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    192309 |  7354 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     64117 |  7355 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    128239 |  7356 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    192077 |  7357 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    128048 |  7358 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    128053 |  7359 | `			if( zDup ){` |
|    128053 |  7360 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     64024 |  7361 | `			}` |
|     64024 |  7362 | `		}` |
|    128239 |  7363 | `		SyBlobRelease(&sBlob);` |
|     64117 |  7364 | `	}` |
|         - |  7365 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7366 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7367 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7368 | `	{` |
|    128239 |  7369 | `		int nNonNull = 0;` |
|    128239 |  7370 | `		int iNonNullIdx = -1;` |
|         - |  7371 | `		int i;` |
|    256625 |  7372 | `		for( i = 0; i < nAtoms; i++ ){` |
|    128391 |  7373 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    128361 |  7374 | `				nNonNull++;` |
|    128361 |  7375 | `				iNonNullIdx = i;` |
|     64178 |  7376 | `			}` |
|     64198 |  7377 | `		}` |
|    128239 |  7378 | `		if( nNonNull <= 1 ){` |
|         - |  7379 | `			/* Fast path: store as single type. */` |
|    128133 |  7380 | `			if( iNonNullIdx >= 0 ){` |
|    128127 |  7381 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    128127 |  7382 | `				if( pA->nType == SXU32_HIGH ){` |
|     57968 |  7383 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19321 |  7384 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38647 |  7385 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38647 |  7386 | `					*pnType = SXU32_HIGH;` |
|     38647 |  7387 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    108806 |  7388 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7389 | `					*pnType = MEMOBJ_VOID;` |
|     89400 |  7390 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7391 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7392 | `				}else{` |
|     89299 |  7393 | `					*pnType = pA->nType;` |
|         - |  7394 | `				}` |
|     64061 |  7395 | `			}` |
|     64069 |  7396 | `		}else{` |
|         - |  7397 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7398 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7399 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7400 | `				ph7_type_alt sAlt;` |
|       249 |  7401 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7402 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7403 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7404 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7405 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7406 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7407 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7408 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7409 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7410 | `				}else{` |
|       145 |  7411 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7412 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7413 | `				}` |
|       239 |  7414 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7415 | `			}` |
|         - |  7416 | `		}` |
|         - |  7417 | `	}` |
|    128239 |  7418 | `	return SXRET_OK;` |
|     64130 |  7419 | `}` |
|         - |  7420 |  |
|         - |  7421 | `/*` |
|         - |  7422 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7423 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7424 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7425 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7426 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7427 | `` *          and union types `: T\|U`.`` |
|         - |  7428 | ` */` |
|   2699512 |  7429 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7430 | `{` |
|   2699517 |  7431 | `	sxi32 iFlags = 0;` |
|         - |  7432 | `	sxi32 rc;` |
|         - |  7433 | `	sxu32 nLine;` |
|   2699517 |  7434 | `	pFunc->nReturnType = 0;` |
|   2699517 |  7435 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2699517 |  7436 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7437 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7438 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7439 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7440 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7441 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2699517 |  7442 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2699517 |  7443 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2699517 |  7444 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2687339 |  7445 | `		return SXRET_OK;` |
|         - |  7446 | `	}` |
|     12183 |  7447 | `	pGen->pIn++; /* Skip ':' */` |
|     12183 |  7448 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7449 | `		return SXRET_OK;` |
|         - |  7450 | `	}` |
|     12183 |  7451 | `	nLine = pGen->pIn->nLine;` |
|     12183 |  7452 | `	rc = GenStateParseUnionTypeDecl(` |
|      6089 |  7453 | `		pGen,` |
|      6089 |  7454 | `		&pFunc->nReturnType,` |
|      6089 |  7455 | `		&pFunc->sReturnClass,` |
|      6089 |  7456 | `		&pFunc->aReturnUnion,` |
|         - |  7457 | `		&iFlags,` |
|      6089 |  7458 | `		&pFunc->sReturnTypeName,` |
|         - |  7459 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7460 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7461 | `		/* iUnionFlag */ 0,` |
|         - |  7462 | `		/* bAllowVoid */ 1,` |
|      6089 |  7463 | `		nLine);` |
|     12183 |  7464 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7465 | `		return SXERR_ABORT;` |
|         - |  7466 | `	}` |
|     12183 |  7467 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7468 | `		/* Error already reported */` |
|       ! 0 |  7469 | `		return SXERR_SYNTAX;` |
|         - |  7470 | `	}` |
|     12183 |  7471 | `	if( rc == SXERR_SYNTAX ){` |
|         8 |  7472 | `		if( pGen->pIn < pGen->pEnd ){` |
|        11 |  7473 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7474 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7475 | `				&pGen->pIn->sData);` |
|         5 |  7476 | `		}else{` |
|       ! 0 |  7477 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7478 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7479 | `		}` |
|         8 |  7480 | `		return SXERR_SYNTAX;` |
|         - |  7481 | `	}` |
|     12177 |  7482 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12177 |  7483 | `	return SXRET_OK;` |
|   1349761 |  7484 | `}` |
|         - |  7485 |  |
|    301462 |  7486 | `static sxi32 GenStateCompileFunc(` |
|         - |  7487 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7488 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7489 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7490 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7491 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7492 | `	)` |
|         5 |  7493 | `{` |
|         - |  7494 | `	ph7_vm_func *pFunc;` |
|         - |  7495 | `	SyToken *pEnd;` |
|         - |  7496 | `	sxu32 nLine;` |
|         - |  7497 | `	char *zName;` |
|         - |  7498 | `	sxi32 rc;` |
|         - |  7499 | `	/* Extract line number */` |
|    301467 |  7500 | `	nLine = pGen->pIn->nLine;` |
|         - |  7501 | `	/* Jump the left parenthesis '(' */` |
|    301467 |  7502 | `	pGen->pIn++;` |
|         - |  7503 | `	/* Delimit the function signature */` |
|    301467 |  7504 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    301467 |  7505 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7506 | `		/* Syntax error */` |
|         8 |  7507 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7508 | `		(void)pName;` |
|         8 |  7509 | `		if( rc == SXERR_ABORT ){` |
|         - |  7510 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7511 | `			return SXERR_ABORT;` |
|         - |  7512 | `		}` |
|         8 |  7513 | `		pGen->pIn = pGen->pEnd;` |
|         8 |  7514 | `		return SXRET_OK;` |
|         - |  7515 | `	}` |
|         - |  7516 | `	/* Create the function state */` |
|    301461 |  7517 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    301461 |  7518 | `	if( pFunc == 0 ){` |
|       ! 0 |  7519 | `		goto OutOfMem;` |
|         - |  7520 | `	}` |
|         - |  7521 | `	/* Build the function name, prepending namespace if active */` |
|    301468 |  7522 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7523 | `		SyBlob sFQN;` |
|         - |  7524 | `		sxu32 nLen;` |
|        16 |  7525 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7526 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7527 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7528 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7529 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7530 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7531 | `		SyBlobRelease(&sFQN);` |
|        16 |  7532 | `		if( zName == 0 ){` |
|       ! 0 |  7533 | `			goto OutOfMem;` |
|         - |  7534 | `		}` |
|        16 |  7535 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7536 | `	}else{` |
|    301447 |  7537 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    301447 |  7538 | `		if( zName == 0 ){` |
|       ! 0 |  7539 | `			goto OutOfMem;` |
|         - |  7540 | `		}` |
|    301447 |  7541 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7542 | `	}` |
|         - |  7543 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7544 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    301461 |  7545 | `	pFunc->nLine = nLine;` |
|    301461 |  7546 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    301461 |  7547 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7548 | `		return SXERR_ABORT;` |
|         - |  7549 | `	}` |
|    301461 |  7550 | `	if( pGen->pIn < pEnd ){` |
|         - |  7551 | `		/* Collect function arguments */` |
|    243023 |  7552 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    243023 |  7553 | `		if( rc == SXERR_ABORT ){` |
|         - |  7554 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7555 | `			return SXERR_ABORT;` |
|         - |  7556 | `		}` |
|    121509 |  7557 | `	}` |
|         - |  7558 | `	/* Point past ')' and parse optional return type ': type' */` |
|    301461 |  7559 | `	pGen->pIn = &pEnd[1];` |
|         - |  7560 | `	{` |
|    301461 |  7561 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    301461 |  7562 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7563 | `			return SXERR_ABORT;` |
|    301461 |  7564 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         8 |  7565 | `			return SXERR_SYNTAX;` |
|         - |  7566 | `		}` |
|         - |  7567 | `	}` |
|    301455 |  7568 | `	if( bHandleClosure ){` |
|         - |  7569 | `		ph7_vm_func_closure_env sEnv;` |
|       469 |  7570 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       464 |  7571 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       280 |  7572 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        91 |  7573 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7574 | `				/* Closure,record environment variable */` |
|        91 |  7575 | `				pGen->pIn++;` |
|        91 |  7576 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7577 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7578 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7579 | `						return SXERR_ABORT;` |
|         - |  7580 | `					}` |
|       ! 0 |  7581 | `				}` |
|        91 |  7582 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7583 | `				/* Compile until we hit the first closing parenthesis */` |
|       187 |  7584 | `				while( pGen->pIn < pGen->pEnd ){` |
|       187 |  7585 | `					int iFlagsLocal = 0;` |
|       187 |  7586 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        91 |  7587 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        91 |  7588 | `						break;` |
|         - |  7589 | `					}` |
|       101 |  7590 | `					nLineLocal = pGen->pIn->nLine;` |
|       101 |  7591 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7592 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7593 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7594 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7595 | `						pGen->pIn++;` |
|        27 |  7596 | `					}` |
|        96 |  7597 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       101 |  7598 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7599 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7600 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7601 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7602 | `								return SXERR_ABORT;` |
|         - |  7603 | `							}` |
|         - |  7604 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7605 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7606 | `								pGen->pIn++;` |
|       ! 0 |  7607 | `							}` |
|       ! 0 |  7608 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7609 | `								pGen->pIn++;` |
|       ! 0 |  7610 | `							}` |
|       ! 0 |  7611 | `							break;` |
|         - |  7612 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7613 | `					}else{` |
|         - |  7614 | `						SyString *pNameLocal;` |
|         - |  7615 | `						char *zDup;` |
|         - |  7616 | `						/* Duplicate variable name */` |
|       101 |  7617 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       101 |  7618 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       101 |  7619 | `						if( zDup ){` |
|         - |  7620 | `							/* Zero the structure */` |
|       101 |  7621 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       101 |  7622 | `							sEnv.iFlags = iFlagsLocal;` |
|       101 |  7623 | `							sEnv.nIdx = SXU32_HIGH;` |
|       101 |  7624 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       101 |  7625 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       116 |  7626 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7627 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7628 | `									got_this = 1;` |
|       ! 0 |  7629 | `							}` |
|         - |  7630 | `							/* Save imported variable */` |
|       101 |  7631 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        53 |  7632 | `						}else{` |
|       ! 0 |  7633 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7634 | `							 return SXERR_ABORT;` |
|         - |  7635 | `						}` |
|         - |  7636 | `					}` |
|       101 |  7637 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       113 |  7638 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7639 | `						/* Ignore trailing commas */` |
|        13 |  7640 | `						pGen->pIn++;` |
|         1 |  7641 | `					}` |
|         5 |  7642 | `				}` |
|         - |  7643 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7644 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7645 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7646 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7647 | `				 * legacy pre-use position. */` |
|        91 |  7648 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7649 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7650 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7651 | `						return SXERR_ABORT;` |
|         7 |  7652 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7653 | `						return SXERR_SYNTAX;` |
|         - |  7654 | `					}` |
|         3 |  7655 | `				}` |
|        43 |  7656 | `		}` |
|       469 |  7657 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7658 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7659 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7660 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7661 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7662 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7663 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7664 | `			 * closure never binds $this (php). */` |
|       461 |  7665 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       461 |  7666 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       461 |  7667 | `			sEnv.nIdx = SXU32_HIGH;` |
|       461 |  7668 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       461 |  7669 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       461 |  7670 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       228 |  7671 | `		}` |
|       469 |  7672 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7673 | `			/* Mark as closure */` |
|       463 |  7674 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       229 |  7675 | `		}` |
|       232 |  7676 | `	}` |
|         - |  7677 | `	/* Compile the body */` |
|    301455 |  7678 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    301455 |  7679 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7680 | `		return SXERR_ABORT;` |
|         - |  7681 | `	}` |
|         - |  7682 | `	/* The cursor sits just past the body's closing brace */` |
|    301455 |  7683 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    301455 |  7684 | `	if( ppFunc ){` |
|    301455 |  7685 | `		*ppFunc = pFunc;` |
|    150725 |  7686 | `	}` |
|    301455 |  7687 | `	rc = SXRET_OK;` |
|    301455 |  7688 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7689 | `		/* Finally register the function */` |
|    300997 |  7690 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    150496 |  7691 | `	}` |
|    301455 |  7692 | `	if( rc == SXRET_OK ){` |
|    301455 |  7693 | `		return SXRET_OK;` |
|         - |  7694 | `	}` |
|         - |  7695 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7696 | `OutOfMem:` |
|         - |  7697 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7698 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7699 | `	 */` |
|       ! 0 |  7700 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7701 | `	return SXERR_ABORT;` |
|    150736 |  7702 | `}` |
|         - |  7703 | `/*` |
|         - |  7704 | ` * Compile a standard PHP function.` |
|         - |  7705 | ` *  Refer to the block-comment above for more information.` |
|         - |  7706 | ` */` |
|    301006 |  7707 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7708 | `{` |
|         - |  7709 | `	SyString *pName;` |
|         - |  7710 | `	sxi32 iFlags;` |
|         - |  7711 | `	sxu32 nKwLine;` |
|         - |  7712 | `	sxu32 nLine;` |
|         - |  7713 | `	sxi32 rc;` |
|         - |  7714 |  |
|    301011 |  7715 | `	nLine = pGen->pIn->nLine;` |
|    301011 |  7716 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    301011 |  7717 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    301011 |  7718 | `	iFlags = 0;` |
|    301011 |  7719 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7720 | `		/* Return by reference,remember that */` |
|        12 |  7721 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7722 | `		/* Jump the '&' token */` |
|        12 |  7723 | `		pGen->pIn++;` |
|         5 |  7724 | `	}` |
|    301011 |  7725 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7726 | `		/* Invalid function name */` |
|         8 |  7727 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         8 |  7728 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7729 | `			return SXERR_ABORT;` |
|         - |  7730 | `		}` |
|         - |  7731 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  7732 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  7733 | `			pGen->pIn++;` |
|         2 |  7734 | `		}` |
|         8 |  7735 | `		return SXRET_OK;` |
|         - |  7736 | `	}` |
|    301005 |  7737 | `	pName = &pGen->pIn->sData;` |
|    301005 |  7738 | `	nLine = pGen->pIn->nLine;` |
|         - |  7739 | `	/* Jump the function name */` |
|    301005 |  7740 | `	pGen->pIn++;` |
|    301005 |  7741 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7742 | `		/* Syntax error */` |
|         3 |  7743 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  7744 | `		if( rc == SXERR_ABORT ){` |
|         - |  7745 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7746 | `			return SXERR_ABORT;` |
|         - |  7747 | `		}` |
|         - |  7748 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  7749 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  7750 | `			pGen->pIn++;` |
|       ! 0 |  7751 | `		}` |
|         3 |  7752 | `		return SXRET_OK;` |
|         - |  7753 | `	}` |
|         - |  7754 | `	/* Compile function body */` |
|         - |  7755 | `	{` |
|    301003 |  7756 | `		ph7_vm_func *pFuncState = 0;` |
|    301003 |  7757 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    301003 |  7758 | `		if( pFuncState ){` |
|         - |  7759 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    300991 |  7760 | `			pFuncState->nLine = nKwLine;` |
|    150493 |  7761 | `		}` |
|         - |  7762 | `	}` |
|    301003 |  7763 | `	return rc;` |
|    150508 |  7764 | `}` |
|         - |  7765 | `/*` |
|         - |  7766 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  7767 | ` * According to the PHP language reference manual` |
|         - |  7768 | ` *  Visibility:` |
|         - |  7769 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  7770 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  7771 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  7772 | ` *  Members declared protected can be accessed only within the class` |
|         - |  7773 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  7774 | ` *  may only be accessed by the class that defines the member.` |
|         - |  7775 | ` */` |
|   3147724 |  7776 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  7777 | `{` |
|   3147729 |  7778 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    253633 |  7779 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2894101 |  7780 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    192087 |  7781 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  7782 | `	}` |
|         - |  7783 | `	/* Assume public by default */` |
|   2702019 |  7784 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1573867 |  7785 | `}` |
|         - |  7786 | `/*` |
|         - |  7787 | ` * Compile a class constant.` |
|         - |  7788 | ` * According to the PHP language reference manual` |
|         - |  7789 | ` *  Class Constants` |
|         - |  7790 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  7791 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  7792 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  7793 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  7794 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  7795 | ` *   It's also possible for interfaces to have constants.` |
|         - |  7796 | ` * Symisc eXtension.` |
|         - |  7797 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  7798 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  7799 | ` *  Example:` |
|         - |  7800 | ` *   class Test{` |
|         - |  7801 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  7802 | ` *   };` |
|         - |  7803 | ` *   var_dump(TEST::MyConst);` |
|         - |  7804 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  7805 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  7806 | ` */` |
|         - |  7807 | `/*` |
|         - |  7808 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  7809 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  7810 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  7811 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  7812 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  7813 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  7814 | ` */` |
|    292016 |  7815 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  7816 | `{` |
|         - |  7817 | `	SyToken *p0, *p1;` |
|    292021 |  7818 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7819 | `		return 0;` |
|         - |  7820 | `	}` |
|    292021 |  7821 | `	p0 = pGen->pIn;` |
|         - |  7822 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    292021 |  7823 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  7824 | `		return 1;` |
|         - |  7825 | `	}` |
|    292021 |  7826 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  7827 | `		return 1;` |
|         - |  7828 | `	}` |
|         - |  7829 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  7830 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  7831 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    292017 |  7832 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    292017 |  7833 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    292017 |  7834 | `		if( p1 ){` |
|    292017 |  7835 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  7836 | `				return 1;` |
|         - |  7837 | `			}` |
|    291987 |  7838 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  7839 | `				return 1;` |
|         - |  7840 | `			}` |
|    145989 |  7841 | `		}` |
|    145989 |  7842 | `	}` |
|    291983 |  7843 | `	return 0;` |
|    146013 |  7844 | `}` |
|         - |  7845 | `/*` |
|         - |  7846 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  7847 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  7848 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  7849 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  7850 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  7851 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  7852 | ` * Peek only; never consumes tokens.` |
|         - |  7853 | ` */` |
|        24 |  7854 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  7855 | `{` |
|        28 |  7856 | `	SyToken *p = pGen->pIn;` |
|        39 |  7857 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  7858 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  7859 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  7860 | `	}` |
|        28 |  7861 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  7862 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  7863 | `	}` |
|         6 |  7864 | `	p++;` |
|         - |  7865 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  7866 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  7867 | `}` |
|         - |  7868 | `/*` |
|         - |  7869 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  7870 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  7871 | `` * `$o->new`), not a `new` expression.`` |
|         - |  7872 | ` */` |
|       110 |  7873 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  7874 | `{` |
|         - |  7875 | `	sxi32 iOp;` |
|       114 |  7876 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  7877 | `		return 0;` |
|         - |  7878 | `	}` |
|       104 |  7879 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  7880 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  7881 | `}` |
|         - |  7882 | `/*` |
|         - |  7883 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  7884 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  7885 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  7886 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  7887 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  7888 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  7889 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  7890 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  7891 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  7892 | ` *` |
|         - |  7893 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  7894 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  7895 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  7896 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  7897 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  7898 | ` */` |
|    626778 |  7899 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  7900 | `{` |
|    626783 |  7901 | `	SyToken *p = pGen->pIn;` |
|    626783 |  7902 | `	int iDepth = 0;` |
|   1642495 |  7903 | `	while( p < pGen->pEnd ){` |
|   1642495 |  7904 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    626731 |  7905 | `			break; /* end of this initializer */` |
|         - |  7906 | `		}` |
|   1015764 |  7907 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    511743 |  7908 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7712 |  7909 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  7910 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  7911 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  7912 | `			 * expression. */` |
|         3 |  7913 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  7914 | `			p++;` |
|         3 |  7915 | `			if( bArrow ){` |
|         - |  7916 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  7917 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  7918 | `				int iBase = iDepth;` |
|        17 |  7919 | `				while( p < pGen->pEnd ){` |
|        17 |  7920 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  7921 | `						iDepth++;` |
|        15 |  7922 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  7923 | `						if( iDepth <= iBase ){` |
|       ! 0 |  7924 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  7925 | `						}` |
|         5 |  7926 | `						iDepth--;` |
|        11 |  7927 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  7928 | `						break;` |
|         - |  7929 | `					}` |
|        15 |  7930 | `					p++;` |
|         1 |  7931 | `				}` |
|         2 |  7932 | `			}else{` |
|         - |  7933 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  7934 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  7935 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  7936 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  7937 | `				int iLocal = 0;` |
|       ! 0 |  7938 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  7939 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  7940 | `						break; /* body brace */` |
|         - |  7941 | `					}` |
|       ! 0 |  7942 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  7943 | `						iLocal++;` |
|       ! 0 |  7944 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  7945 | `						if( iLocal > 0 ){` |
|       ! 0 |  7946 | `							iLocal--;` |
|       ! 0 |  7947 | `						}` |
|       ! 0 |  7948 | `					}` |
|       ! 0 |  7949 | `					p++;` |
|       ! 0 |  7950 | `				}` |
|       ! 0 |  7951 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  7952 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  7953 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  7954 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  7955 | `							iBrace++;` |
|       ! 0 |  7956 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  7957 | `							iBrace--;` |
|       ! 0 |  7958 | `							if( iBrace == 0 ){` |
|       ! 0 |  7959 | `								p++;` |
|       ! 0 |  7960 | `								break;` |
|         - |  7961 | `							}` |
|       ! 0 |  7962 | `						}` |
|       ! 0 |  7963 | `						p++;` |
|       ! 0 |  7964 | `					}` |
|       ! 0 |  7965 | `				}` |
|         - |  7966 | `			}` |
|         3 |  7967 | `			continue;` |
|         - |  7968 | `		}` |
|   1015767 |  7969 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  7970 | `			if( iDepth == 0 ){` |
|         - |  7971 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  7972 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  7973 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  7974 | `				 * is legal — don't scan into it. */` |
|        45 |  7975 | `				break;` |
|         - |  7976 | `			}` |
|       ! 0 |  7977 | `			iDepth++;` |
|   1015723 |  7978 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42329 |  7979 | `			iDepth++;` |
|    994561 |  7980 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     42327 |  7981 | `			if( iDepth > 0 ){` |
|     42327 |  7982 | `				iDepth--;` |
|     21161 |  7983 | `			}` |
|    952238 |  7984 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    338709 |  7985 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  7986 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  7987 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  7988 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  7989 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  7990 | `				return 1;` |
|         - |  7991 | `			}` |
|       ! 0 |  7992 | `		}` |
|   1015715 |  7993 | `		p++;` |
|         5 |  7994 | `	}` |
|    626775 |  7995 | `	return 0;` |
|    313394 |  7996 | `}` |
|         - |  7997 | `/*` |
|         - |  7998 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  7999 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8000 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8001 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8002 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8003 | ` * share the same backing.` |
|         - |  8004 | ` */` |
|       350 |  8005 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8006 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8007 | `{` |
|       355 |  8008 | `	pAttr->nType = nType;` |
|       355 |  8009 | `	pAttr->sClass = *pClass;` |
|       355 |  8010 | `	pAttr->sTypeName = *pTypeName;` |
|       355 |  8011 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8012 | `		sxu32 i;` |
|        73 |  8013 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8014 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8015 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8016 | `		}` |
|        11 |  8017 | `	}` |
|       355 |  8018 | `}` |
|    292016 |  8019 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8020 | `{` |
|    292021 |  8021 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8022 | `	SySet *pInstrContainer;` |
|         - |  8023 | `	ph7_class_attr *pCons;` |
|         - |  8024 | `	SyString *pName;` |
|         - |  8025 | `	sxi32 rc;` |
|    292021 |  8026 | `	sxu32 nType = 0;` |
|         - |  8027 | `	SyString sTypeClass;` |
|         - |  8028 | `	SyString sTypeText;` |
|         - |  8029 | `	SySet aUnionAlts;` |
|    292021 |  8030 | `	sxi32 iTypeFlags = 0;` |
|    292021 |  8031 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    292021 |  8032 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    292021 |  8033 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8034 | `	/* Extract visibility level */` |
|    292021 |  8035 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8036 | `	/* Mark as constant */` |
|    292021 |  8037 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    292021 |  8038 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8039 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8040 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    292040 |  8041 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8042 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8043 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8044 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8045 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8046 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8047 | `		 * and success paths release. */` |
|        42 |  8048 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8049 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8050 | `			goto Synchronize;` |
|        42 |  8051 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8052 | `			return SXERR_ABORT;` |
|        42 |  8053 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8054 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8055 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8056 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8057 | `				return SXERR_ABORT;` |
|         - |  8058 | `			}` |
|       ! 0 |  8059 | `			goto Synchronize;` |
|         - |  8060 | `		}` |
|        42 |  8061 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8062 | `	}` |
|    146008 |  8063 | `loop:` |
|    292023 |  8064 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8065 | `		/* Invalid constant name */` |
|       ! 0 |  8066 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8067 | `		if( rc == SXERR_ABORT ){` |
|         - |  8068 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8069 | `			return SXERR_ABORT;` |
|         - |  8070 | `		}` |
|       ! 0 |  8071 | `		goto Synchronize;` |
|         - |  8072 | `	}` |
|         - |  8073 | `	/* Peek constant name */` |
|    292023 |  8074 | `	pName = &pGen->pIn->sData;` |
|         - |  8075 | `	/* Make sure the constant name isn't reserved */` |
|    292023 |  8076 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8077 | `		/* Reserved constant name */` |
|       ! 0 |  8078 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8079 | `		if( rc == SXERR_ABORT ){` |
|         - |  8080 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8081 | `			return SXERR_ABORT;` |
|         - |  8082 | `		}` |
|       ! 0 |  8083 | `		goto Synchronize;` |
|         - |  8084 | `	}` |
|         - |  8085 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    292023 |  8086 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8087 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8088 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8089 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8090 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8091 | `			return SXERR_ABORT;` |
|        42 |  8092 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8093 | `			goto Synchronize;` |
|         - |  8094 | `		}` |
|        18 |  8095 | `	}` |
|         - |  8096 | `	/* Advance the stream cursor */` |
|    292021 |  8097 | `	pGen->pIn++;` |
|    292021 |  8098 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8099 | `		/* Invalid declaration */` |
|       ! 0 |  8100 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8101 | `		if( rc == SXERR_ABORT ){` |
|         - |  8102 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8103 | `			return SXERR_ABORT;` |
|         - |  8104 | `		}` |
|       ! 0 |  8105 | `		goto Synchronize;` |
|         - |  8106 | `	}` |
|    292021 |  8107 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8108 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8109 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8110 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8111 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    292016 |  8112 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8113 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8114 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8115 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8116 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8117 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8118 | `			return SXERR_ABORT;` |
|         - |  8119 | `		}` |
|         6 |  8120 | `		goto Synchronize;` |
|         - |  8121 | `	}` |
|         - |  8122 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8123 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8124 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    292017 |  8125 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8126 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8127 | `			"New expressions are not supported in this context");` |
|         5 |  8128 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8129 | `			return SXERR_ABORT;` |
|         - |  8130 | `		}` |
|         5 |  8131 | `		goto Synchronize;` |
|         - |  8132 | `	}` |
|         - |  8133 | `	/* Allocate a new class attribute */` |
|    292013 |  8134 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    292013 |  8135 | `	if( pCons ){` |
|    292013 |  8136 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    292013 |  8137 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8138 | `			return SXERR_ABORT;` |
|         - |  8139 | `		}` |
|    146004 |  8140 | `	}` |
|    292013 |  8141 | `	if( pCons == 0 ){` |
|       ! 0 |  8142 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8143 | `		return SXERR_ABORT;` |
|         - |  8144 | `	}` |
|    292013 |  8145 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8146 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8147 | `	}` |
|         - |  8148 | `	/* Swap bytecode container */` |
|    292013 |  8149 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    292013 |  8150 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8151 | `	/* Compile constant value.` |
|         - |  8152 | `	 */` |
|    292013 |  8153 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    292013 |  8154 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8155 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8156 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8157 | `			return SXERR_ABORT;` |
|         - |  8158 | `		}` |
|         1 |  8159 | `	}` |
|         - |  8160 | `	/* Emit the done instruction */` |
|    292013 |  8161 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    292013 |  8162 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    292013 |  8163 | `	if( rc == SXERR_ABORT ){` |
|         - |  8164 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8165 | `		return SXERR_ABORT;` |
|         - |  8166 | `	}` |
|         - |  8167 | `	/* All done,install the constant */` |
|    292013 |  8168 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    292013 |  8169 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8170 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8171 | `		return SXERR_ABORT;` |
|         - |  8172 | `	}` |
|    292013 |  8173 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8174 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8175 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8176 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8177 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8178 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8179 | `				pTok--;` |
|       ! 0 |  8180 | `			}` |
|       ! 0 |  8181 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8182 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8183 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8184 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8185 | `				return SXERR_ABORT;` |
|         - |  8186 | `			}` |
|       ! 0 |  8187 | `		}else{` |
|         3 |  8188 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8189 | `				goto loop;` |
|         - |  8190 | `			}` |
|         - |  8191 | `		}` |
|       ! 0 |  8192 | `	}` |
|    292011 |  8193 | `	SySetRelease(&aUnionAlts);` |
|    292011 |  8194 | `	return SXRET_OK;` |
|         5 |  8195 | `Synchronize:` |
|        13 |  8196 | `	SySetRelease(&aUnionAlts);` |
|         - |  8197 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8198 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8199 | `		pGen->pIn++;` |
|         3 |  8200 | `	}` |
|        13 |  8201 | `	return SXERR_CORRUPT;` |
|    146013 |  8202 | `}` |
|         - |  8203 | `/*` |
|         - |  8204 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8205 | ` * According to the PHP language reference manual` |
|         - |  8206 | ` *  Properties` |
|         - |  8207 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8208 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8209 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8210 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8211 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8212 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8213 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8214 | ` * Symisc eXtension.` |
|         - |  8215 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8216 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8217 | ` *  Example:` |
|         - |  8218 | ` *   class Test{` |
|         - |  8219 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8220 | ` *   };` |
|         - |  8221 | ` *   var_dump(TEST::myVar);` |
|         - |  8222 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8223 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8224 | ` */` |
|         - |  8225 | `/*` |
|         - |  8226 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8227 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8228 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8229 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8230 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8231 | ` */` |
|   2351724 |  8232 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8233 | `{` |
|   2351729 |  8234 | `	SyToken *p = pStart;` |
|   2351729 |  8235 | `	int bFirst = 1;` |
|   2351729 |  8236 | `	if( p >= pEnd ) return 0;` |
|         - |  8237 | ``	/* Optional nullable `?` shorthand. */`` |
|   2351729 |  8238 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        35 |  8239 | `		p++;` |
|        35 |  8240 | `		if( p >= pEnd ) return 0;` |
|        16 |  8241 | `	}` |
|         - |  8242 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8243 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8244 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8245 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1175862 |  8246 | `	for(;;){` |
|   2351749 |  8247 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8248 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8249 | `			p++;` |
|         9 |  8250 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8251 | `			if( p >= pEnd ) return 0;` |
|         3 |  8252 | `			p++; /* skip ')' */` |
|         2 |  8253 | `		}else{` |
|         - |  8254 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8255 | ``			 * then any `&`-joined intersection members. */`` |
|   2351747 |  8256 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2351747 |  8257 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8258 | `				return 0;` |
|         - |  8259 | `			}` |
|         - |  8260 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8261 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8262 | `			 * may still appear at the initial dispatch site). */` |
|   2351747 |  8263 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2351699 |  8264 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2351694 |  8265 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    104090 |  8266 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2351417 |  8267 | `					return 0;` |
|         - |  8268 | `				}` |
|       141 |  8269 | `			}` |
|       335 |  8270 | `			p++;` |
|       337 |  8271 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8272 | `				p += 2;` |
|         1 |  8273 | `			}` |
|       498 |  8274 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       338 |  8275 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8276 | `				p++; /* skip '&' */` |
|         3 |  8277 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8278 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8279 | `				p++;` |
|         3 |  8280 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8281 | `					p += 2;` |
|       ! 0 |  8282 | `				}` |
|         1 |  8283 | `			}` |
|         - |  8284 | `		}` |
|       337 |  8285 | `		bFirst = 0;` |
|       332 |  8286 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8287 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8288 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8289 | `			continue;` |
|         - |  8290 | `		}` |
|       317 |  8291 | `		break;` |
|       ! 0 |  8292 | `	}` |
|       317 |  8293 | `	if( p >= pEnd ) return 0;` |
|       317 |  8294 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1175867 |  8295 | `}` |
|         - |  8296 |  |
|         - |  8297 | `/*` |
|         - |  8298 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8299 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8300 | ` * if not). Recognized forms:` |
|         - |  8301 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8302 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8303 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8304 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8305 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8306 | ` * on unrecoverable error.` |
|         - |  8307 | ` *` |
|         - |  8308 | ` * When a type is parsed:` |
|         - |  8309 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8310 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8311 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8312 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8313 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8314 | ` */` |
|       322 |  8315 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8316 | `	ph7_gen_state *pGen,` |
|         - |  8317 | `	sxu32 *pnType,` |
|         - |  8318 | `	SyString *pClass,` |
|         - |  8319 | `	sxi32 *piTypeFlags,` |
|         - |  8320 | `	SyString *pTypeText,` |
|         - |  8321 | `	SySet *pAlts` |
|         5 |  8322 | `){` |
|       327 |  8323 | `	sxi32 iFlags = 0;` |
|         - |  8324 | `	sxi32 rc;` |
|       327 |  8325 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8326 | `		return SXRET_OK;` |
|         - |  8327 | `	}` |
|         - |  8328 | `	/* If the first token is '$', there's no type */` |
|       327 |  8329 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8330 | `		return SXRET_OK;` |
|         - |  8331 | `	}` |
|       327 |  8332 | `	rc = GenStateParseUnionTypeDecl(` |
|       161 |  8333 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8334 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8335 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8336 | `		/* bAllowVoid */ 0,` |
|       322 |  8337 | `		pGen->pIn->nLine);` |
|       327 |  8338 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8339 | `		return rc;` |
|         - |  8340 | `	}` |
|         - |  8341 | `	/* Verify next token is '$' (start of property name) */` |
|       327 |  8342 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8343 | `		return SXERR_SYNTAX;` |
|         - |  8344 | `	}` |
|       327 |  8345 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       327 |  8346 | `	return SXRET_OK;` |
|       166 |  8347 | `}` |
|         - |  8348 |  |
|         - |  8349 | `/*` |
|         - |  8350 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8351 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8352 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8353 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8354 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8355 | ` * by the type parser itself before reaching here.` |
|         - |  8356 | ` *` |
|         - |  8357 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8358 | ` * use in the error message.` |
|         - |  8359 | ` */` |
|       498 |  8360 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8361 | `	sxu32 nType,` |
|         - |  8362 | `	const SyString *pClass,` |
|         - |  8363 | `	const char **pzName,` |
|         - |  8364 | `	sxu32 *pnName)` |
|         5 |  8365 | `{` |
|         - |  8366 | `	const char *z;` |
|         - |  8367 | `	sxu32 n;` |
|       503 |  8368 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       449 |  8369 | `		return 0;` |
|         - |  8370 | `	}` |
|        59 |  8371 | `	z = pClass->zString;` |
|        59 |  8372 | `	n = pClass->nByte;` |
|        59 |  8373 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8374 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8375 | `	}` |
|         - |  8376 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8377 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8378 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        52 |  8379 | `	return 0;` |
|       254 |  8380 | `}` |
|         - |  8381 |  |
|         - |  8382 | `/*` |
|         - |  8383 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8384 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8385 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8386 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8387 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8388 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8389 | ` *` |
|         - |  8390 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8391 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8392 | ` */` |
|       436 |  8393 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8394 | `	ph7_gen_state *pGen,` |
|         - |  8395 | `	ph7_class *pClass,` |
|         - |  8396 | `	const SyString *pMemberName,` |
|         - |  8397 | `	sxu32 nType,` |
|         - |  8398 | `	const SyString *pTypeClass,` |
|         - |  8399 | `	const SyString *pTypeText,` |
|         - |  8400 | `	SySet *pUnionAlts,` |
|         - |  8401 | `	const char *zErrFmt,` |
|         - |  8402 | `	sxu32 nLine)` |
|         5 |  8403 | `{` |
|       441 |  8404 | `	const char *zBad = 0;` |
|       441 |  8405 | `	sxu32 nBad = 0;` |
|         - |  8406 | `	SyString sFallback;` |
|         - |  8407 | `	const SyString *pBad;` |
|         - |  8408 | `	sxi32 rc;` |
|       441 |  8409 | `	int bDisallowed = 0;` |
|       441 |  8410 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8411 | `		bDisallowed = 1;` |
|       439 |  8412 | `	}else if( pUnionAlts ){` |
|         - |  8413 | `		sxu32 i;` |
|        95 |  8414 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8415 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8416 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8417 | `				bDisallowed = 1;` |
|         3 |  8418 | `				break;` |
|         - |  8419 | `			}` |
|        35 |  8420 | `		}` |
|        15 |  8421 | `	}` |
|       441 |  8422 | `	if( !bDisallowed ){` |
|       435 |  8423 | `		return SXRET_OK;` |
|         - |  8424 | `	}` |
|         - |  8425 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8426 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8427 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8428 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8429 | `		pBad = pTypeText;` |
|         5 |  8430 | `	}else{` |
|       ! 0 |  8431 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8432 | `		pBad = &sFallback;` |
|         - |  8433 | `	}` |
|        11 |  8434 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8435 | `		zErrFmt,` |
|         3 |  8436 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8437 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8438 | `		return SXERR_ABORT;` |
|         - |  8439 | `	}` |
|         8 |  8440 | `	return SXERR_SYNTAX;` |
|       223 |  8441 | `}` |
|         - |  8442 | `/*` |
|         - |  8443 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8444 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8445 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8446 | ` * than promoted to a lexer keyword.` |
|         - |  8447 | ` */` |
|  18083084 |  8448 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8449 | `{` |
|  18279457 |  8450 | `	return (pTok->nType & PH7_TK_ID)` |
|   9237910 |  8451 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  18279452 |  8452 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8453 | `}` |
|         - |  8454 | `/*` |
|         - |  8455 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8456 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8457 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8458 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8459 | ` */` |
|   6907656 |  8460 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8461 | `{` |
|   6907661 |  8462 | `	*pnTok = 0;` |
|   6907656 |  8463 | `	if( &pTok[3] < pEnd` |
|   6505522 |  8464 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5490065 |  8465 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2438379 |  8466 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8467 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8468 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8469 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8470 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8471 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8472 | `			*pnTok = 4;` |
|        17 |  8473 | `			return nKw;` |
|         - |  8474 | `		}` |
|       ! 0 |  8475 | `	}` |
|   6907645 |  8476 | `	return 0;` |
|   3453833 |  8477 | `}` |
|         - |  8478 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8479 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8480 | `{` |
|        17 |  8481 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8482 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8483 | `	}` |
|         5 |  8484 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8485 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8486 | `	}` |
|         3 |  8487 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8488 | `}` |
|    457938 |  8489 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8490 | `{` |
|    457943 |  8491 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8492 | `	ph7_class_attr *pAttr;` |
|         - |  8493 | `	SyString *pName;` |
|         - |  8494 | `	sxi32 rc;` |
|    457943 |  8495 | `	sxu32 nType = 0;` |
|         - |  8496 | `	SyString sTypeClass;` |
|         - |  8497 | `	SyString sTypeText;` |
|         - |  8498 | `	SySet aUnionAlts;` |
|    457943 |  8499 | `	sxi32 iTypeFlags = 0;` |
|    457943 |  8500 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    457943 |  8501 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    457943 |  8502 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8503 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8504 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8505 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    457943 |  8506 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8507 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8508 | `	}` |
|         - |  8509 | `	/* Extract visibility level */` |
|    457943 |  8510 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8511 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    458104 |  8512 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       327 |  8513 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       327 |  8514 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8515 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8516 | `			goto Synchronize;` |
|       327 |  8517 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8518 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8519 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8520 | `				&pGen->pIn->sData);` |
|       ! 0 |  8521 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8522 | `				return SXERR_ABORT;` |
|         - |  8523 | `			}` |
|       ! 0 |  8524 | `			goto Synchronize;` |
|       327 |  8525 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8526 | `			return SXERR_ABORT;` |
|         - |  8527 | `		}` |
|       161 |  8528 | `	}` |
|       ! 0 |  8529 | `loop:` |
|    457947 |  8530 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8531 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8532 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8533 | `			return SXERR_ABORT;` |
|         - |  8534 | `		}` |
|       ! 0 |  8535 | `		goto Synchronize;` |
|         - |  8536 | `	}` |
|    457947 |  8537 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    457947 |  8538 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8539 | `		/* Invalid attribute name */` |
|       ! 0 |  8540 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8541 | `		if( rc == SXERR_ABORT ){` |
|         - |  8542 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8543 | `			return SXERR_ABORT;` |
|         - |  8544 | `		}` |
|       ! 0 |  8545 | `		goto Synchronize;` |
|         - |  8546 | `	}` |
|         - |  8547 | `	/* Peek attribute name */` |
|    457947 |  8548 | `	pName = &pGen->pIn->sData;` |
|         - |  8549 | `	/* Advance the stream cursor */` |
|    457947 |  8550 | `	pGen->pIn++;` |
|    457947 |  8551 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8552 | `		/* Invalid declaration */` |
|         3 |  8553 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8554 | `		if( rc == SXERR_ABORT ){` |
|         - |  8555 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8556 | `			return SXERR_ABORT;` |
|         - |  8557 | `		}` |
|         3 |  8558 | `		goto Synchronize;` |
|         - |  8559 | `	}` |
|         - |  8560 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8561 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    457945 |  8562 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8563 | `		const char *zAvErr = 0;` |
|        19 |  8564 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8565 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8566 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8567 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8568 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8569 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8570 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8571 | `		}` |
|        13 |  8572 | `		if( zAvErr ){` |
|       ! 0 |  8573 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8574 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8575 | `				return SXERR_ABORT;` |
|         - |  8576 | `			}` |
|       ! 0 |  8577 | `			goto Synchronize;` |
|         - |  8578 | `		}` |
|         6 |  8579 | `	}` |
|         - |  8580 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8581 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    457945 |  8582 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8583 | `		const char *zRoErr = 0;` |
|        43 |  8584 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8585 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8586 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8587 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8588 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8589 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8590 | `		}` |
|        43 |  8591 | `		if( zRoErr ){` |
|        13 |  8592 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8593 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8594 | `				return SXERR_ABORT;` |
|         - |  8595 | `			}` |
|        13 |  8596 | `			goto Synchronize;` |
|         - |  8597 | `		}` |
|        14 |  8598 | `	}` |
|         - |  8599 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8600 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8601 | `	 * by the type parser. */` |
|    457935 |  8602 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       485 |  8603 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8604 | `			&sTypeText,` |
|       320 |  8605 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       160 |  8606 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       325 |  8607 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8608 | `			return SXERR_ABORT;` |
|       325 |  8609 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8610 | `			goto Synchronize;` |
|         - |  8611 | `		}` |
|       160 |  8612 | `	}` |
|         - |  8613 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    457935 |  8614 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8615 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8616 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8617 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8618 | `			return SXERR_ABORT;` |
|         - |  8619 | `		}` |
|         3 |  8620 | `		goto Synchronize;` |
|         - |  8621 | `	}` |
|         - |  8622 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8623 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8624 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8625 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8626 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8627 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    457933 |  8628 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8629 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8630 | `			"New expressions are not supported in this context");` |
|         6 |  8631 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8632 | `			return SXERR_ABORT;` |
|         - |  8633 | `		}` |
|         6 |  8634 | `		goto Synchronize;` |
|         - |  8635 | `	}` |
|         - |  8636 | `	/* Allocate a new class attribute */` |
|    457929 |  8637 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    457929 |  8638 | `	if( pAttr ){` |
|    457929 |  8639 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    457929 |  8640 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8641 | `			return SXERR_ABORT;` |
|         - |  8642 | `		}` |
|    228962 |  8643 | `	}` |
|    457929 |  8644 | `	if( pAttr == 0 ){` |
|       ! 0 |  8645 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8646 | `		return SXERR_ABORT;` |
|         - |  8647 | `	}` |
|    457929 |  8648 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       323 |  8649 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       159 |  8650 | `	}` |
|    457929 |  8651 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8652 | `		SySet *pInstrContainer;` |
|    334767 |  8653 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    334767 |  8654 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8655 | `		{` |
|         - |  8656 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8657 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8658 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8659 | `			 * compiler would otherwise run into the hook tokens. */` |
|    334767 |  8660 | `			SyToken *pScan = pGen->pIn;` |
|    334767 |  8661 | `			sxi32 iNest = 0;` |
|    723635 |  8662 | `			while( pScan < pGen->pEnd ){` |
|    723635 |  8663 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42327 |  8664 | `					iNest++;` |
|    702474 |  8665 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     42327 |  8666 | `					iNest--;` |
|    660152 |  8667 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    334767 |  8668 | `					break;` |
|         - |  8669 | `				}` |
|    388873 |  8670 | `				pScan++;` |
|         5 |  8671 | `			}` |
|    334767 |  8672 | `			pGen->pEnd = pScan;` |
|         - |  8673 | `		}` |
|         - |  8674 | `		/* Swap bytecode container */` |
|    334767 |  8675 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    334767 |  8676 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8677 | `		/* Compile attribute value.` |
|         - |  8678 | `		 */` |
|    334767 |  8679 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    334767 |  8680 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8681 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8682 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8683 | `				return SXERR_ABORT;` |
|         - |  8684 | `			}` |
|       ! 0 |  8685 | `		}` |
|         - |  8686 | `		/* Emit the done instruction */` |
|    334767 |  8687 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    334767 |  8688 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    334767 |  8689 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    334767 |  8690 | `		pGen->pEnd = pSavedDefEnd;` |
|    167381 |  8691 | `	}` |
|         - |  8692 | `	/* All done,install the attribute */` |
|    457929 |  8693 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    457929 |  8694 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8695 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8696 | `		return SXERR_ABORT;` |
|         - |  8697 | `	}` |
|    457929 |  8698 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8699 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8700 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8701 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8702 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8703 | `			return SXERR_ABORT;` |
|         - |  8704 | `		}` |
|        95 |  8705 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8706 | `			goto Synchronize;` |
|         - |  8707 | `		}` |
|        95 |  8708 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8709 | `		return SXRET_OK;` |
|         - |  8710 | `	}` |
|    457835 |  8711 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8712 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8713 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8714 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8715 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8716 | `				? "Interfaces may only include hooked properties"` |
|         - |  8717 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8718 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8719 | `			return SXERR_ABORT;` |
|         - |  8720 | `		}` |
|       ! 0 |  8721 | `		goto Synchronize;` |
|         - |  8722 | `	}` |
|    457835 |  8723 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8724 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8725 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8726 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8727 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8728 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8729 | `				pTok--;` |
|       ! 0 |  8730 | `			}` |
|       ! 0 |  8731 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8732 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8733 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8734 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8735 | `				return SXERR_ABORT;` |
|         - |  8736 | `			}` |
|       ! 0 |  8737 | `		}else{` |
|         5 |  8738 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8739 | `				goto loop;` |
|         - |  8740 | `			}` |
|         - |  8741 | `		}` |
|       ! 0 |  8742 | `	}` |
|    457831 |  8743 | `	SySetRelease(&aUnionAlts);` |
|    457831 |  8744 | `	return SXRET_OK;` |
|         9 |  8745 | `Synchronize:` |
|         - |  8746 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8747 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  8748 | `		pGen->pIn++;` |
|         3 |  8749 | `	}` |
|        22 |  8750 | `	SySetRelease(&aUnionAlts);` |
|        22 |  8751 | `	return SXERR_CORRUPT;` |
|    228974 |  8752 | `}` |
|         - |  8753 | `/*` |
|         - |  8754 | ` * Compile a class method.` |
|         - |  8755 | ` *` |
|         - |  8756 | ` * Refer to the official documentation for more information` |
|         - |  8757 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  8758 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  8759 | ` * overloading and many more.` |
|         - |  8760 | ` */` |
|   2397770 |  8761 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  8762 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  8763 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  8764 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  8765 | `	int doBody,          /* TRUE to process method body */` |
|         - |  8766 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  8767 | `	)` |
|         5 |  8768 | `{` |
|   2397775 |  8769 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2397775 |  8770 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  8771 | `	ph7_class_method *pMeth;` |
|         - |  8772 | `	sxi32 iFuncFlags;` |
|         - |  8773 | `	SyString *pName;` |
|         - |  8774 | `	SyToken *pEnd;` |
|         - |  8775 | `	sxi32 rc;` |
|         - |  8776 | `	/* Extract visibility level */` |
|   2397775 |  8777 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2397775 |  8778 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2397775 |  8779 | `	iFuncFlags = 0;` |
|   2397775 |  8780 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  8781 | `		/* Invalid method name */` |
|       ! 0 |  8782 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8783 | `		if( rc == SXERR_ABORT ){` |
|         - |  8784 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8785 | `			return SXERR_ABORT;` |
|         - |  8786 | `		}` |
|       ! 0 |  8787 | `		goto Synchronize;` |
|         - |  8788 | `	}` |
|   2397775 |  8789 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8790 | `		/* Return by reference,remember that */` |
|       ! 0 |  8791 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8792 | `		/* Jump the '&' token */` |
|       ! 0 |  8793 | `		pGen->pIn++;` |
|       ! 0 |  8794 | `	}` |
|   2397775 |  8795 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8796 | `		/* Invalid method name */` |
|       ! 0 |  8797 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8798 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8799 | `			return SXERR_ABORT;` |
|         - |  8800 | `		}` |
|       ! 0 |  8801 | `		goto Synchronize;` |
|         - |  8802 | `	}` |
|         - |  8803 | `	/* Peek method name */` |
|   2397775 |  8804 | `	pName = &pGen->pIn->sData;` |
|   2397775 |  8805 | `	nLine = pGen->pIn->nLine;` |
|         - |  8806 | `	/* Jump the method name */` |
|   2397775 |  8807 | `	pGen->pIn++;` |
|   2397775 |  8808 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8809 | `		/* Abstract method */` |
|    138307 |  8810 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  8811 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8812 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  8813 | `				&pClass->sName,pName);` |
|       ! 0 |  8814 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8815 | `				return SXERR_ABORT;` |
|         - |  8816 | `			}` |
|       ! 0 |  8817 | `		}` |
|         - |  8818 | `		/* Assemble method signature only */` |
|    138307 |  8819 | `		doBody = FALSE;` |
|     69151 |  8820 | `	}` |
|   2397775 |  8821 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8822 | `		/* Syntax error */` |
|       ! 0 |  8823 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  8824 | `		if( rc == SXERR_ABORT ){` |
|         - |  8825 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8826 | `			return SXERR_ABORT;` |
|         - |  8827 | `		}` |
|       ! 0 |  8828 | `		goto Synchronize;` |
|         - |  8829 | `	}` |
|         - |  8830 | `	/* Allocate a new class_method instance */` |
|   2397775 |  8831 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2397775 |  8832 | `	if( pMeth == 0 ){` |
|       ! 0 |  8833 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8834 | `		return SXERR_ABORT;` |
|         - |  8835 | `	}` |
|   2397775 |  8836 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2397775 |  8837 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2397775 |  8838 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8839 | `		return SXERR_ABORT;` |
|         - |  8840 | `	}` |
|         - |  8841 | `	/* Jump the left parenthesis '(' */` |
|   2397775 |  8842 | `	pGen->pIn++;` |
|   2397775 |  8843 | `	pEnd = 0; /* cc warning */` |
|         - |  8844 | `	/* Delimit the method signature */` |
|   2397775 |  8845 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2397775 |  8846 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  8847 | `		/* Syntax error */` |
|         3 |  8848 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  8849 | `		if( rc == SXERR_ABORT ){` |
|         - |  8850 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8851 | `			return SXERR_ABORT;` |
|         - |  8852 | `		}` |
|         3 |  8853 | `		goto Synchronize;` |
|         - |  8854 | `	}` |
|         - |  8855 | `	{` |
|   2397773 |  8856 | `		int bIsCtor = 0;` |
|   2397773 |  8857 | `		int bAbstractCtor = 0;` |
|   2397768 |  8858 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1400644 |  8859 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2315109 |  8860 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    165333 |  8861 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  8862 | `				bAbstractCtor = 1;` |
|         2 |  8863 | `			}else{` |
|    165331 |  8864 | `				bIsCtor = 1;` |
|         - |  8865 | `			}` |
|     82664 |  8866 | `		}` |
|   2397773 |  8867 | `		if( pGen->pIn < pEnd ){` |
|         - |  8868 | `			/* Collect method arguments */` |
|    860843 |  8869 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    860843 |  8870 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8871 | `				return SXERR_ABORT;` |
|         - |  8872 | `			}` |
|    430419 |  8873 | `		}` |
|         - |  8874 | `	}` |
|         - |  8875 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2397773 |  8876 | `	pGen->pIn = &pEnd[1];` |
|         - |  8877 | `	{` |
|   2397773 |  8878 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2397773 |  8879 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  8880 | `			return SXERR_ABORT;` |
|   2397773 |  8881 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  8882 | `			goto Synchronize;` |
|         - |  8883 | `		}` |
|         - |  8884 | `	}` |
|         - |  8885 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  8886 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  8887 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  8888 | `	{` |
|   2397773 |  8889 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  8890 | `		sxu32 i;` |
|   3688869 |  8891 | `		for( i = 0; i < nArg; i++ ){` |
|   1291111 |  8892 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  8893 | `			ph7_class_attr *pAttr;` |
|   1291111 |  8894 | `			sxi32 iAttrFlags = 0;` |
|         - |  8895 | `			int bArgTyped;` |
|   1291111 |  8896 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1291027 |  8897 | `				continue;` |
|         - |  8898 | `			}` |
|         - |  8899 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  8900 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  8901 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  8902 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  8903 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  8904 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  8905 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8906 | `					"Cannot declare variadic promoted property");` |
|         3 |  8907 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8908 | `					return SXERR_ABORT;` |
|         - |  8909 | `				}` |
|         3 |  8910 | `				goto Synchronize;` |
|         - |  8911 | `			}` |
|         - |  8912 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  8913 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  8914 | `			 * appear as an alternative of a union type. */` |
|        87 |  8915 | `			if( bArgTyped ){` |
|       122 |  8916 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  8917 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  8918 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  8919 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  8920 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8921 | `					return SXERR_ABORT;` |
|        83 |  8922 | `				}else if( rc != SXRET_OK ){` |
|         6 |  8923 | `					goto Synchronize;` |
|         - |  8924 | `				}` |
|        37 |  8925 | `			}` |
|         - |  8926 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  8927 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  8928 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8929 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  8930 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8931 | `					return SXERR_ABORT;` |
|         - |  8932 | `				}` |
|         3 |  8933 | `				goto Synchronize;` |
|         - |  8934 | `			}` |
|        81 |  8935 | `			if( bArgTyped ){` |
|        77 |  8936 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  8937 | `			}` |
|        81 |  8938 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  8939 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  8940 | `			}` |
|        81 |  8941 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  8942 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  8943 | `			}` |
|        81 |  8944 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  8945 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  8946 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  8947 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  8948 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8949 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  8950 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8951 | `						return SXERR_ABORT;` |
|         - |  8952 | `					}` |
|         3 |  8953 | `					goto Synchronize;` |
|         - |  8954 | `				}` |
|        24 |  8955 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  8956 | `			}` |
|        79 |  8957 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  8958 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  8959 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8960 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8961 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  8962 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  8963 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8964 | `						return SXERR_ABORT;` |
|         - |  8965 | `					}` |
|       ! 0 |  8966 | `					goto Synchronize;` |
|         - |  8967 | `				}` |
|         5 |  8968 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  8969 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  8970 | `			}` |
|        79 |  8971 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  8972 | `			if( pAttr == 0 ){` |
|       ! 0 |  8973 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8974 | `				return SXERR_ABORT;` |
|         - |  8975 | `			}` |
|        79 |  8976 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  8977 | `				pAttr->nType = pArg->nType;` |
|        77 |  8978 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  8979 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  8980 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8981 | `					sxu32 k;` |
|        20 |  8982 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  8983 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  8984 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  8985 | `					}` |
|         3 |  8986 | `				}` |
|        36 |  8987 | `			}` |
|        79 |  8988 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  8989 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  8990 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8991 | `				return SXERR_ABORT;` |
|         - |  8992 | `			}` |
|        42 |  8993 | `		}` |
|         - |  8994 | `	}` |
|   2397763 |  8995 | `	if( doBody ){` |
|         - |  8996 | `		/* Compile method body */` |
|   2259461 |  8997 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2259461 |  8998 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8999 | `			return SXERR_ABORT;` |
|         - |  9000 | `		}` |
|         - |  9001 | `		/* The cursor sits just past the body's closing brace */` |
|   2259461 |  9002 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1129733 |  9003 | `	}else{` |
|         - |  9004 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    138307 |  9005 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    138307 |  9006 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     69151 |  9007 | `		}` |
|         - |  9008 | `		/* Only method signature is allowed */` |
|    138307 |  9009 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9010 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9011 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9012 | `				if( rc == SXERR_ABORT ){` |
|         - |  9013 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9014 | `					return SXERR_ABORT;` |
|         - |  9015 | `				}` |
|       ! 0 |  9016 | `				return SXERR_CORRUPT;` |
|         - |  9017 | `			}` |
|         - |  9018 | `	}` |
|         - |  9019 | `	/* All done,install the method */` |
|   2397763 |  9020 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2397763 |  9021 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9022 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9023 | `		return SXERR_ABORT;` |
|         - |  9024 | `	}` |
|   2397763 |  9025 | `	return SXRET_OK;` |
|         6 |  9026 | `Synchronize:` |
|         - |  9027 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9028 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9029 | `		pGen->pIn++;` |
|         4 |  9030 | `	}` |
|        16 |  9031 | `	return SXERR_CORRUPT;` |
|   1198890 |  9032 | `}` |
|         - |  9033 | `/*` |
|         - |  9034 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9035 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9036 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9037 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9038 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9039 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9040 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9041 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9042 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9043 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9044 | `` * implicit `$value` formal.`` |
|         - |  9045 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9046 | ` */` |
|         - |  9047 | `/*` |
|         - |  9048 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9049 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9050 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9051 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9052 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9053 | ` */` |
|        94 |  9054 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9055 | `{` |
|         - |  9056 | `	SyToken *p;` |
|       345 |  9057 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9058 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9059 | `			continue;` |
|         - |  9060 | `		}` |
|         - |  9061 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9062 | `		if( p + 3 < pEnd` |
|        80 |  9063 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9064 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9065 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9066 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9067 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9068 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9069 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9070 | `			return 1;` |
|         - |  9071 | `		}` |
|         - |  9072 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9073 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9074 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9075 | `		if( p > pStart` |
|        26 |  9076 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9077 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9078 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9079 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9080 | `			return 1;` |
|         - |  9081 | `		}` |
|        15 |  9082 | `	}` |
|        43 |  9083 | `	return 0;` |
|        48 |  9084 | `}` |
|         - |  9085 | `/*` |
|         - |  9086 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9087 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9088 | ` */` |
|       990 |  9089 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9090 | `{` |
|      1167 |  9091 | `	return p + 6 < pEnd` |
|       671 |  9092 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9093 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9094 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9095 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9096 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9097 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9098 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9099 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9100 | `	 && p[5].sData.nByte == 3` |
|         8 |  9101 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9102 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9103 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9104 | `}` |
|         - |  9105 | `/*` |
|         - |  9106 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9107 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9108 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9109 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9110 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9111 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9112 | ` * or SXERR_MEM.` |
|         - |  9113 | ` */` |
|         4 |  9114 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9115 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9116 | `{` |
|         5 |  9117 | `	SyToken *p = pStart;` |
|        35 |  9118 | `	while( p < pEnd ){` |
|        31 |  9119 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9120 | `			SyToken sTok;` |
|         - |  9121 | `			char zName[384];` |
|         - |  9122 | `			sxu32 nName;` |
|         - |  9123 | `			char *zDup;` |
|         - |  9124 | ``			/* `parent` `::` */`` |
|         5 |  9125 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9126 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9127 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9128 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9129 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9130 | `			if( zDup == 0 ){` |
|       ! 0 |  9131 | `				return SXERR_MEM;` |
|         - |  9132 | `			}` |
|         5 |  9133 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9134 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9135 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9136 | `			sTok.pUserData = 0;` |
|         5 |  9137 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9138 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9139 | `			continue;` |
|         - |  9140 | `		}` |
|        27 |  9141 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9142 | `		p++;` |
|         1 |  9143 | `	}` |
|         5 |  9144 | `	return SXRET_OK;` |
|         3 |  9145 | `}` |
|        94 |  9146 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9147 | `{` |
|        95 |  9148 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9149 | `	sxi32 rc;` |
|        95 |  9150 | `	int bRefsSelf = 0;` |
|        95 |  9151 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9152 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9153 | `		char zHook[384];` |
|         - |  9154 | `		SyString sHookName;` |
|         - |  9155 | `		ph7_class_method *pMeth;` |
|         - |  9156 | `		int bGet;` |
|       159 |  9157 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9158 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9159 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9160 | `			continue;` |
|         - |  9161 | `		}` |
|       145 |  9162 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9163 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9164 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9165 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9166 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9167 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9168 | `				return SXERR_ABORT;` |
|         - |  9169 | `			}` |
|       ! 0 |  9170 | `			return SXERR_CORRUPT;` |
|         - |  9171 | `		}` |
|       145 |  9172 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9173 | `			goto HookSyntax;` |
|         - |  9174 | `		}` |
|       144 |  9175 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9176 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9177 | `			bGet = 1;` |
|       106 |  9178 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9179 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9180 | `			bGet = 0;` |
|        34 |  9181 | `		}else{` |
|       ! 0 |  9182 | `			goto HookSyntax;` |
|         - |  9183 | `		}` |
|       145 |  9184 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9185 | `		sHookName.zString = zHook;` |
|       217 |  9186 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9187 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9188 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9189 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9190 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9191 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9192 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9193 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9194 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9195 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9196 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9197 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9198 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9199 | `					return SXERR_ABORT;` |
|         - |  9200 | `				}` |
|       ! 0 |  9201 | `				return SXERR_CORRUPT;` |
|         - |  9202 | `			}` |
|        15 |  9203 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9204 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9205 | `			if( pMeth == 0 ){` |
|       ! 0 |  9206 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9207 | `				return SXERR_ABORT;` |
|         - |  9208 | `			}` |
|        15 |  9209 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9210 | `			if( !bGet ){` |
|         - |  9211 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9212 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9213 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9214 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9215 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9216 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9217 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9218 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9219 | `				if( zVName == 0 ){` |
|       ! 0 |  9220 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9221 | `					return SXERR_ABORT;` |
|         - |  9222 | `				}` |
|         7 |  9223 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9224 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9225 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9226 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9227 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9228 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9229 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9230 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9231 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9232 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9233 | `				}` |
|         7 |  9234 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9235 | `			}` |
|        15 |  9236 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9237 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9238 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9239 | `				return SXERR_ABORT;` |
|         - |  9240 | `			}` |
|        15 |  9241 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9242 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9243 | `		}` |
|       130 |  9244 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9245 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9246 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9247 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9248 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9249 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9250 | `				return SXERR_ABORT;` |
|         - |  9251 | `			}` |
|       ! 0 |  9252 | `			return SXERR_CORRUPT;` |
|         - |  9253 | `		}` |
|       131 |  9254 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9255 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9256 | `		if( pMeth == 0 ){` |
|       ! 0 |  9257 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9258 | `			return SXERR_ABORT;` |
|         - |  9259 | `		}` |
|       131 |  9260 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9261 | `		if( !bGet ){` |
|         - |  9262 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9263 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9264 | `				SyToken *pRp = 0;` |
|        17 |  9265 | `				pGen->pIn++;` |
|        17 |  9266 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9267 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9268 | `					goto HookSyntax;` |
|         - |  9269 | `				}` |
|        17 |  9270 | `				if( pGen->pIn < pRp ){` |
|        17 |  9271 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9272 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9273 | `						return SXERR_ABORT;` |
|         - |  9274 | `					}` |
|         8 |  9275 | `				}` |
|        17 |  9276 | `				pGen->pIn = &pRp[1];` |
|         8 |  9277 | `			}` |
|        61 |  9278 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9279 | `				/* Implicit $value formal */` |
|         - |  9280 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9281 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9282 | `				if( zVName == 0 ){` |
|       ! 0 |  9283 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9284 | `					return SXERR_ABORT;` |
|         - |  9285 | `				}` |
|        45 |  9286 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9287 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9288 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9289 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9290 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9291 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9292 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9293 | `			}` |
|        30 |  9294 | `		}` |
|       165 |  9295 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9296 | `			/* Block body */` |
|        69 |  9297 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9298 | `			SyToken *pCloser = 0;` |
|        69 |  9299 | `			int bParentCall = 0;` |
|        69 |  9300 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9301 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9302 | `				SyToken *pScan;` |
|       753 |  9303 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9304 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9305 | `						bParentCall = 1;` |
|         3 |  9306 | `						break;` |
|         - |  9307 | `					}` |
|       343 |  9308 | `				}` |
|        34 |  9309 | `			}` |
|        69 |  9310 | `			if( bParentCall ){` |
|         - |  9311 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9312 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9313 | `				 * hook method), then continue past the original body. */` |
|         - |  9314 | `				SySet sBody;` |
|         3 |  9315 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9316 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9317 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9318 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9319 | `					SySetRelease(&sBody);` |
|       ! 0 |  9320 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9321 | `					return SXERR_ABORT;` |
|         - |  9322 | `				}` |
|         3 |  9323 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9324 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9325 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9326 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9327 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9328 | `				SySetRelease(&sBody);` |
|         3 |  9329 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9330 | `					return SXERR_ABORT;` |
|         - |  9331 | `				}` |
|         3 |  9332 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9333 | `			}else{` |
|        67 |  9334 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9335 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9336 | `					return SXERR_ABORT;` |
|         - |  9337 | `				}` |
|        67 |  9338 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9339 | `			}` |
|        69 |  9340 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9341 | `				bRefsSelf = 1;` |
|         9 |  9342 | `			}` |
|       128 |  9343 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9344 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9345 | `			GenBlock *pBlock;` |
|         - |  9346 | `			SySet *pInstrContainer;` |
|         - |  9347 | `			SyToken *pBodyStart;` |
|         - |  9348 | `			SyToken *pExprEnd;` |
|        63 |  9349 | `			SyToken *pSavedEnd = 0;` |
|         - |  9350 | `			SySet sBody;` |
|        63 |  9351 | `			int bParentCall = 0;` |
|        63 |  9352 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9353 | `			pBodyStart = pGen->pIn;` |
|         - |  9354 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9355 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9356 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9357 | `			 * method on a token copy. */` |
|         - |  9358 | `			{` |
|        63 |  9359 | `				sxi32 iNest = 0;` |
|        63 |  9360 | `				pExprEnd = pBodyStart;` |
|       355 |  9361 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9362 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9363 | `						iNest++;` |
|       351 |  9364 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9365 | `						if( iNest <= 0 ){` |
|       ! 0 |  9366 | `							break;` |
|         - |  9367 | `						}` |
|         9 |  9368 | `						iNest--;` |
|       343 |  9369 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9370 | `						break;` |
|         - |  9371 | `					}` |
|       293 |  9372 | `					pExprEnd++;` |
|         1 |  9373 | `				}` |
|         - |  9374 | `			}` |
|         - |  9375 | `			{` |
|         - |  9376 | `				SyToken *pScan;` |
|       335 |  9377 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9378 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9379 | `						bParentCall = 1;` |
|         3 |  9380 | `						break;` |
|         - |  9381 | `					}` |
|       137 |  9382 | `				}` |
|         - |  9383 | `			}` |
|        63 |  9384 | `			if( bParentCall ){` |
|         3 |  9385 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9386 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9387 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9388 | `					SySetRelease(&sBody);` |
|       ! 0 |  9389 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9390 | `					return SXERR_ABORT;` |
|         - |  9391 | `				}` |
|         3 |  9392 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9393 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9394 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9395 | `			}` |
|        94 |  9396 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9397 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9398 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9399 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9400 | `				return SXERR_ABORT;` |
|         - |  9401 | `			}` |
|        63 |  9402 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9403 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9404 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9405 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9406 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9407 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9408 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9409 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9410 | `			if( bParentCall ){` |
|         3 |  9411 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9412 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9413 | `				SySetRelease(&sBody);` |
|         1 |  9414 | `			}` |
|        63 |  9415 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9416 | `				return SXERR_ABORT;` |
|         - |  9417 | `			}` |
|        63 |  9418 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9419 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9420 | `				bRefsSelf = 1;` |
|        18 |  9421 | `			}` |
|        63 |  9422 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9423 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9424 | `			}` |
|        63 |  9425 | `			if( !bGet ){` |
|         - |  9426 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9427 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9428 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9429 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9430 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9431 | `				bRefsSelf = 1;` |
|         1 |  9432 | `			}` |
|        32 |  9433 | `		}else{` |
|       ! 0 |  9434 | `			goto HookSyntax;` |
|         - |  9435 | `		}` |
|       131 |  9436 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9437 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9438 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9439 | `			return SXERR_ABORT;` |
|         - |  9440 | `		}` |
|       131 |  9441 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9442 | `	}` |
|        95 |  9443 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9444 | `		goto HookSyntax;` |
|         - |  9445 | `	}` |
|        95 |  9446 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9447 | `	if( !bRefsSelf ){` |
|         - |  9448 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9449 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9450 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9451 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9452 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9453 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9454 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9455 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9456 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9457 | `				return SXERR_ABORT;` |
|         - |  9458 | `			}` |
|       ! 0 |  9459 | `			return SXERR_CORRUPT;` |
|         - |  9460 | `		}` |
|        20 |  9461 | `	}` |
|        95 |  9462 | `	return SXRET_OK;` |
|       ! 0 |  9463 | `HookSyntax:` |
|       ! 0 |  9464 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9465 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9466 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9467 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9468 | `		return SXERR_ABORT;` |
|         - |  9469 | `	}` |
|       ! 0 |  9470 | `	return SXERR_CORRUPT;` |
|        48 |  9471 | `}` |
|         - |  9472 | `/*` |
|         - |  9473 | ` * Compile an object interface.` |
|         - |  9474 | ` *  According to the PHP language reference manual` |
|         - |  9475 | ` *   Object Interfaces:` |
|         - |  9476 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9477 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9478 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9479 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9480 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9481 | ` */` |
|     69224 |  9482 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9483 | `{` |
|     69229 |  9484 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9485 | `	ph7_class *pClass,*pBase;` |
|         - |  9486 | `	SyToken *pEnd,*pTmp;` |
|         - |  9487 | `	SyString *pName;` |
|         - |  9488 | `	sxi32 nKwrd;` |
|         - |  9489 | `	sxi32 rc;` |
|         - |  9490 | `	/* Jump the 'interface' keyword */` |
|     69229 |  9491 | `	pGen->pIn++;` |
|         - |  9492 | `	/* Extract interface name */` |
|     69229 |  9493 | `	pName = &pGen->pIn->sData;` |
|         - |  9494 | `	/* Advance the stream cursor */` |
|     69229 |  9495 | `	pGen->pIn++;` |
|         - |  9496 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9497 | `		SyBlob sFQN;` |
|         - |  9498 | `		SyString sFQNStr;` |
|     69229 |  9499 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     69229 |  9500 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     69229 |  9501 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     69229 |  9502 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     69229 |  9503 | `		SyBlobRelease(&sFQN);` |
|         - |  9504 | `	}` |
|     69229 |  9505 | `	if( pClass == 0 ){` |
|       ! 0 |  9506 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9507 | `		return SXERR_ABORT;` |
|         - |  9508 | `	}` |
|     69229 |  9509 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     69229 |  9510 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9511 | `		return SXERR_ABORT;` |
|         - |  9512 | `	}` |
|         - |  9513 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     69229 |  9514 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9515 | `	/* Assume no base class is given */` |
|     69229 |  9516 | `	pBase = 0;` |
|     69229 |  9517 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26895 |  9518 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26895 |  9519 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9520 | `			SyBlob sResolved;` |
|         - |  9521 | `			SyString sBaseName;` |
|         - |  9522 | `			sxu32 nRefLine;` |
|         - |  9523 | `			/* Extract base interface */` |
|     26895 |  9524 | `			pGen->pIn++;` |
|     26895 |  9525 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26895 |  9526 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26895 |  9527 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9528 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9529 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9530 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9531 | `					pName);` |
|       ! 0 |  9532 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9533 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9534 | `					return SXERR_ABORT;` |
|         - |  9535 | `				}` |
|       ! 0 |  9536 | `				return SXRET_OK;` |
|         - |  9537 | `			}` |
|     40340 |  9538 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     26890 |  9539 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26895 |  9540 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9541 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9542 | `			/* Only interfaces is allowed */` |
|     26895 |  9543 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9544 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9545 | `			}` |
|     26895 |  9546 | `			if( pBase == 0 ){` |
|       ! 0 |  9547 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9548 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9549 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9550 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9551 | `					return SXERR_ABORT;` |
|         - |  9552 | `				}` |
|       ! 0 |  9553 | `			}` |
|     26895 |  9554 | `			SyBlobRelease(&sResolved);` |
|     13445 |  9555 | `		}` |
|     13445 |  9556 | `	}` |
|     69229 |  9557 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9558 | `		/* Syntax error */` |
|       ! 0 |  9559 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9560 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9561 | `		if( rc == SXERR_ABORT ){` |
|         - |  9562 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9563 | `			return SXERR_ABORT;` |
|         - |  9564 | `		}` |
|       ! 0 |  9565 | `		return SXRET_OK;` |
|         - |  9566 | `	}` |
|     69229 |  9567 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     69229 |  9568 | `	pEnd = 0; /* cc warning */` |
|         - |  9569 | `	/* Delimit the interface body */` |
|     69229 |  9570 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     69229 |  9571 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9572 | `		/* Syntax error */` |
|       ! 0 |  9573 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9574 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9575 | `		if( rc == SXERR_ABORT ){` |
|         - |  9576 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9577 | `			return SXERR_ABORT;` |
|         - |  9578 | `		}` |
|       ! 0 |  9579 | `		return SXRET_OK;` |
|         - |  9580 | `	}` |
|         - |  9581 | `	/* The delimiter token is the interface body's closing brace */` |
|     69229 |  9582 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9583 | `	/* Swap token stream */` |
|     69229 |  9584 | `	pTmp = pGen->pEnd;` |
|     69229 |  9585 | `	pGen->pEnd = pEnd;` |
|         - |  9586 | `	/* Start the parse process` |
|         - |  9587 | `	 * Note (According to the PHP reference manual):` |
|         - |  9588 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9589 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9590 | `	 */` |
|    126801 |  9591 | `	for(;;){` |
|         - |  9592 | `		/* Jump leading/trailing semi-colons */` |
|    437989 |  9593 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    184383 |  9594 | `			pGen->pIn++;` |
|         5 |  9595 | `		}` |
|    253611 |  9596 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9597 | `			/* End of interface body */` |
|     69225 |  9598 | `			break;` |
|         - |  9599 | `		}` |
|         - |  9600 | `		/* Bind a directly-preceding docblock to this member */` |
|    184391 |  9601 | `		GenStateSetPendingDoc(&(*pGen));` |
|    184391 |  9602 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9603 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9604 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9605 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9606 | `			if( rc == SXERR_ABORT ){` |
|         - |  9607 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9608 | `				return SXERR_ABORT;` |
|         - |  9609 | `			}` |
|       ! 0 |  9610 | `			goto done;` |
|         - |  9611 | `		}` |
|         - |  9612 | `		/* Extract the current keyword */` |
|    184391 |  9613 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    184391 |  9614 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9615 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9616 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9617 | `			const char *zKind = "member";` |
|         3 |  9618 | `			SyString *pMemberName = 0;` |
|         3 |  9619 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9620 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9621 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9622 | `					zKind = "constant";` |
|         3 |  9623 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9624 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9625 | `					}` |
|         1 |  9626 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9627 | `					zKind = "method";` |
|       ! 0 |  9628 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9629 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9630 | `					}` |
|       ! 0 |  9631 | `				}` |
|         1 |  9632 | `			}` |
|         3 |  9633 | `			if( pMemberName ){` |
|         4 |  9634 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9635 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9636 | `			}else{` |
|       ! 0 |  9637 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9638 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9639 | `			}` |
|         3 |  9640 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9641 | `				return SXERR_ABORT;` |
|         - |  9642 | `			}` |
|         3 |  9643 | `			goto done;` |
|         - |  9644 | `		}` |
|    184389 |  9645 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9646 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9647 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9648 | `			if( rc == SXERR_ABORT ){` |
|         - |  9649 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9650 | `				return SXERR_ABORT;` |
|         - |  9651 | `			}` |
|       ! 0 |  9652 | `			goto done;` |
|         - |  9653 | `		}` |
|    184389 |  9654 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9655 | `			/* Advance the stream cursor */` |
|    130611 |  9656 | `			pGen->pIn++;` |
|    130606 |  9657 | `			if( pGen->pIn < pGen->pEnd` |
|    130611 |  9658 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    130606 |  9659 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9660 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9661 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9662 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9663 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9664 | `				 * hooked properties" error). */` |
|       ! 0 |  9665 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9666 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9667 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9668 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9669 | `						return SXERR_ABORT;` |
|         - |  9670 | `					}` |
|       ! 0 |  9671 | `					goto done;` |
|         - |  9672 | `				}` |
|       ! 0 |  9673 | `				continue;` |
|         - |  9674 | `			}` |
|    130611 |  9675 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9676 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9677 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9678 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9679 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9680 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9681 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9682 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9683 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9684 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9685 | `							return SXERR_ABORT;` |
|         - |  9686 | `						}` |
|       ! 0 |  9687 | `						goto done;` |
|         - |  9688 | `					}` |
|       ! 0 |  9689 | `					continue;` |
|         - |  9690 | `				}` |
|       ! 0 |  9691 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9692 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9693 | `				if( rc == SXERR_ABORT ){` |
|         - |  9694 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9695 | `					return SXERR_ABORT;` |
|         - |  9696 | `				}` |
|       ! 0 |  9697 | `				goto done;` |
|         - |  9698 | `			}` |
|    130611 |  9699 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    130611 |  9700 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9701 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9702 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9703 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9704 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9705 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9706 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9707 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9708 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9709 | `							return SXERR_ABORT;` |
|         - |  9710 | `						}` |
|       ! 0 |  9711 | `						goto done;` |
|         - |  9712 | `					}` |
|         5 |  9713 | `					continue;` |
|         - |  9714 | `				}` |
|       ! 0 |  9715 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9716 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9717 | `				if( rc == SXERR_ABORT ){` |
|         - |  9718 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9719 | `					return SXERR_ABORT;` |
|         - |  9720 | `				}` |
|       ! 0 |  9721 | `				goto done;` |
|         - |  9722 | `			}` |
|     65301 |  9723 | `		}` |
|    184385 |  9724 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9725 | `			/* Parse constant */` |
|     53779 |  9726 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53779 |  9727 | `			if( rc != SXRET_OK ){` |
|         3 |  9728 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9729 | `					return SXERR_ABORT;` |
|         - |  9730 | `				}` |
|         3 |  9731 | `				goto done;` |
|         - |  9732 | `			}` |
|     26891 |  9733 | `		}else{` |
|    130611 |  9734 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    130611 |  9735 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9736 | `				/* Static method,record that */` |
|     11525 |  9737 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9738 | `				/* Advance the stream cursor */` |
|     11525 |  9739 | `				pGen->pIn++;` |
|     11520 |  9740 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11525 |  9741 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9742 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9743 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9744 | `						if( rc == SXERR_ABORT ){` |
|         - |  9745 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9746 | `							return SXERR_ABORT;` |
|         - |  9747 | `						}` |
|       ! 0 |  9748 | `						goto done;` |
|         - |  9749 | `				}` |
|      5760 |  9750 | `			}` |
|         - |  9751 | `			/* Process method signature (no body for interface methods) */` |
|    130611 |  9752 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    130611 |  9753 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9754 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9755 | `					return SXERR_ABORT;` |
|         - |  9756 | `				}` |
|       ! 0 |  9757 | `				goto done;` |
|         - |  9758 | `			}` |
|         - |  9759 | `		}` |
|         5 |  9760 | `	}` |
|         - |  9761 | `	/* Install the interface */` |
|     69225 |  9762 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     69225 |  9763 | `	if( rc == SXRET_OK && pBase ){` |
|         - |  9764 | `		/* Inherit from the base interface */` |
|     26895 |  9765 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13445 |  9766 | `	}` |
|     69225 |  9767 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9768 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9769 | `		return SXERR_ABORT;` |
|         - |  9770 | `	}` |
|     34610 |  9771 | `done:` |
|         - |  9772 | `	/* Point beyond the interface body */` |
|     69229 |  9773 | `	pGen->pIn  = &pEnd[1];` |
|     69229 |  9774 | `	pGen->pEnd = pTmp;` |
|     69229 |  9775 | `	return PH7_OK;` |
|     34617 |  9776 | `}` |
|         - |  9777 | `/*` |
|         - |  9778 | ` * Compile a user-defined class.` |
|         - |  9779 | ` * According to the PHP language reference manual` |
|         - |  9780 | ` *  class` |
|         - |  9781 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - |  9782 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - |  9783 | ` *  of the properties and methods belonging to the class.` |
|         - |  9784 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - |  9785 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - |  9786 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - |  9787 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  9788 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - |  9789 | ` *  (called "methods").` |
|         - |  9790 | ` */` |
|         - |  9791 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - |  9792 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - |  9793 | `struct TraitUseEntry {` |
|         - |  9794 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - |  9795 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - |  9796 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - |  9797 | `};` |
|         - |  9798 | `/*` |
|         - |  9799 | ` * Validate that methods implementing interface contracts have compatible` |
|         - |  9800 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - |  9801 | ` */` |
|    355006 |  9802 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9803 | `{` |
|         - |  9804 | `	ph7_class **apIface;` |
|         - |  9805 | `	sxu32 nIface,i;` |
|         - |  9806 | `	sxi32 rc;` |
|    355011 |  9807 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 |  9808 | `		return SXRET_OK;` |
|         - |  9809 | `	}` |
|    355011 |  9810 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    355011 |  9811 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    712463 |  9812 | `	for(i = 0; i < nIface; i++){` |
|    357457 |  9813 | `		ph7_class *pIface = apIface[i];` |
|         - |  9814 | `		SyHashEntry *pEntry;` |
|    357457 |  9815 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1030037 |  9816 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    672585 |  9817 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - |  9818 | `			ph7_class_method *pImplMeth;` |
|    672585 |  9819 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - |  9820 | `			/* Find the implementing method in the class */` |
|    672585 |  9821 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    672585 |  9822 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 |  9823 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - |  9824 | `			}` |
|         - |  9825 | `			/* Check visibility: interface methods must be implemented as public */` |
|    672567 |  9826 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 |  9827 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9828 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 |  9829 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 |  9830 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9831 | `					return SXERR_ABORT;` |
|         - |  9832 | `				}` |
|         1 |  9833 | `			}` |
|         - |  9834 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - |  9835 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - |  9836 | `			 */` |
|         - |  9837 | `			{` |
|    672567 |  9838 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    672567 |  9839 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    672567 |  9840 | `				int sigError = 0;` |
|    672567 |  9841 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 |  9842 | `					sigError = 1;` |
|    672566 |  9843 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - |  9844 | `					/* Extra parameters must all have default values */` |
|      3849 |  9845 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - |  9846 | `					sxu32 k;` |
|      7691 |  9847 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3849 |  9848 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 |  9849 | `							sigError = 1;` |
|         3 |  9850 | `							break;` |
|         - |  9851 | `						}` |
|      1926 |  9852 | `					}` |
|      1922 |  9853 | `				}` |
|    672567 |  9854 | `				if( sigError ){` |
|         - |  9855 | `					SyBlob sImplSig, sIfaceSig;` |
|         - |  9856 | `					ph7_vm_func_arg *aArgs;` |
|         - |  9857 | `					sxu32 j;` |
|         6 |  9858 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 |  9859 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - |  9860 | `					/* Build implementing method signature */` |
|         6 |  9861 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 |  9862 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 |  9863 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 |  9864 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 |  9865 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9866 | `					}` |
|         - |  9867 | `					/* Build interface method signature */` |
|         6 |  9868 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 |  9869 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 |  9870 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 |  9871 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 |  9872 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9873 | `					}` |
|         8 |  9874 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9875 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 |  9876 | `						&pClass->sName,pMName,` |
|         4 |  9877 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 |  9878 | `						&pIface->sName,pMName,` |
|         4 |  9879 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 |  9880 | `					SyBlobRelease(&sImplSig);` |
|         6 |  9881 | `					SyBlobRelease(&sIfaceSig);` |
|         6 |  9882 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9883 | `						return SXERR_ABORT;` |
|         - |  9884 | `					}` |
|         2 |  9885 | `				}` |
|         - |  9886 | `			}` |
|         5 |  9887 | `		}` |
|    178731 |  9888 | `	}` |
|    355011 |  9889 | `	return SXRET_OK;` |
|    177508 |  9890 | `}` |
|         - |  9891 | `/*` |
|         - |  9892 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - |  9893 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - |  9894 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - |  9895 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - |  9896 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - |  9897 | ` * means that specific hook is still missing.` |
|         - |  9898 | ` */` |
|        38 |  9899 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 |  9900 | `{` |
|         - |  9901 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - |  9902 | `	ph7_class_attr *pProp;` |
|        38 |  9903 | `	if( pMName->nByte <= nPfx` |
|        27 |  9904 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 |  9905 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 |  9906 | `		return 0; /* not a hook stub */` |
|         - |  9907 | `	}` |
|         7 |  9908 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 |  9909 | `	return pProp != 0` |
|         6 |  9910 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 |  9911 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 |  9912 | `}` |
|         - |  9913 | `/*` |
|         - |  9914 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - |  9915 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - |  9916 | ` */` |
|        16 |  9917 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 |  9918 | `{` |
|         - |  9919 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 |  9920 | `	if( pMName->nByte > nPfx` |
|        12 |  9921 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 |  9922 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 |  9923 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 |  9924 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 |  9925 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 |  9926 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 |  9927 | `		return;` |
|         - |  9928 | `	}` |
|        20 |  9929 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 |  9930 | `}` |
|         - |  9931 | `/*` |
|         - |  9932 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - |  9933 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - |  9934 | ` */` |
|    355006 |  9935 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9936 | `{` |
|         - |  9937 | `	ph7_class_method *pMeth;` |
|         - |  9938 | `	SyHashEntry *pEntry;` |
|         - |  9939 | `	sxu32 nAbstract;` |
|         - |  9940 | `	SyBlob sMsg;` |
|         - |  9941 | `	sxi32 rc;` |
|         - |  9942 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    355011 |  9943 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15409 |  9944 | `		return SXRET_OK;` |
|         - |  9945 | `	}` |
|         - |  9946 | `	/* Count abstract methods */` |
|    339607 |  9947 | `	nAbstract = 0;` |
|    339607 |  9948 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   5012364 |  9949 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4502961 |  9950 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4502961 |  9951 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 |  9952 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 |  9953 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - |  9954 | `			}` |
|        20 |  9955 | `			nAbstract++;` |
|         8 |  9956 | `		}` |
|         5 |  9957 | `	}` |
|    339607 |  9958 | `	if( nAbstract == 0 ){` |
|    339593 |  9959 | `		return SXRET_OK;` |
|         - |  9960 | `	}` |
|         - |  9961 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 |  9962 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 |  9963 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - |  9964 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 |  9965 | `		&pClass->sName,nAbstract,` |
|         7 |  9966 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 |  9967 | `		(nAbstract > 1 ? "s" : ""));` |
|         - |  9968 | `	/* Second pass: list methods with origins */` |
|         - |  9969 | `	{` |
|        18 |  9970 | `		sxu32 nListed = 0;` |
|        18 |  9971 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 |  9972 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 |  9973 | `			ph7_class *pOrigin = 0;` |
|         - |  9974 | `			SyString *pMName;` |
|        22 |  9975 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 |  9976 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 |  9977 | `				continue;` |
|         - |  9978 | `			}` |
|        20 |  9979 | `			pMName = &pMeth->sFunc.sName;` |
|        20 |  9980 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 |  9981 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - |  9982 | `			}` |
|        20 |  9983 | `			if( nListed > 0 ){` |
|         3 |  9984 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 |  9985 | `			}` |
|         - |  9986 | `			/* Find the origin of this abstract method.` |
|         - |  9987 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - |  9988 | `			 * inheritance chains) take precedence for interface-declared` |
|         - |  9989 | `			 * methods. Abstract class methods only win when the class` |
|         - |  9990 | `			 * itself declared the abstract method (not inherited from` |
|         - |  9991 | `			 * an interface). Trait methods are adopted into the using` |
|         - |  9992 | `			 * class's namespace.` |
|         - |  9993 | `			 */` |
|         - |  9994 | `			{` |
|         - |  9995 | `				ph7_class **apIface;` |
|         - |  9996 | `				ph7_class **apTrait;` |
|         - |  9997 | `				ph7_class *pWalk;` |
|         - |  9998 | `				sxu32 i;` |
|         - |  9999 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10000 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10001 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10002 | `				 */` |
|        20 | 10003 | `				if( pClass->pBase ){` |
|        11 | 10004 | `					pWalk = pClass->pBase;` |
|        19 | 10005 | `					while( pWalk ){` |
|         - | 10006 | `						ph7_class_method *pParentMeth;` |
|        13 | 10007 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10008 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10009 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10010 | `							 * in this class's ancestor chain.` |
|         - | 10011 | `							 */` |
|        13 | 10012 | `							int fromIface = 0;` |
|        13 | 10013 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10014 | `							while( pAnc ){` |
|         - | 10015 | `								ph7_class **apPI;` |
|         - | 10016 | `								sxu32 j;` |
|        15 | 10017 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10018 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10019 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10020 | `										fromIface = 1;` |
|        10 | 10021 | `										break;` |
|         - | 10022 | `									}` |
|       ! 0 | 10023 | `								}` |
|        15 | 10024 | `								if( fromIface ) break;` |
|         6 | 10025 | `								pAnc = pAnc->pBase;` |
|         2 | 10026 | `							}` |
|        13 | 10027 | `							if( !fromIface ){` |
|         3 | 10028 | `								pOrigin = pWalk;` |
|         3 | 10029 | `								break;` |
|         - | 10030 | `							}` |
|         4 | 10031 | `						}` |
|        10 | 10032 | `						pWalk = pWalk->pBase;` |
|         2 | 10033 | `					}` |
|         4 | 10034 | `				}` |
|         - | 10035 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10036 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10037 | `				 */` |
|        20 | 10038 | `				if( !pOrigin ){` |
|        18 | 10039 | `					pWalk = pClass;` |
|        40 | 10040 | `					while( pWalk && !pOrigin ){` |
|        26 | 10041 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10042 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10043 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10044 | `							ph7_class *pDeepest = 0;` |
|        28 | 10045 | `							while( pIface ){` |
|        16 | 10046 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10047 | `									pDeepest = pIface;` |
|         6 | 10048 | `								}` |
|        16 | 10049 | `								pIface = pIface->pBase;` |
|         4 | 10050 | `							}` |
|        16 | 10051 | `							if( pDeepest ){` |
|        16 | 10052 | `								pOrigin = pDeepest;` |
|        16 | 10053 | `								break;` |
|         - | 10054 | `							}` |
|       ! 0 | 10055 | `						}` |
|        26 | 10056 | `						pWalk = pWalk->pBase;` |
|         4 | 10057 | `					}` |
|         7 | 10058 | `				}` |
|         - | 10059 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10060 | `				if( !pOrigin ){` |
|         3 | 10061 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10062 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10063 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10064 | `							pOrigin = pClass;` |
|         3 | 10065 | `							break;` |
|         - | 10066 | `						}` |
|       ! 0 | 10067 | `					}` |
|         1 | 10068 | `				}` |
|         - | 10069 | `			}` |
|        20 | 10070 | `			if( pOrigin ){` |
|        20 | 10071 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10072 | `			}else{` |
|         - | 10073 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10074 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10075 | `			}` |
|        20 | 10076 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10077 | `			nListed++;` |
|         4 | 10078 | `		}` |
|         - | 10079 | `	}` |
|        18 | 10080 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10081 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10082 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10083 | `	SyBlobRelease(&sMsg);` |
|        18 | 10084 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10085 | `		return SXERR_ABORT;` |
|         - | 10086 | `	}` |
|        18 | 10087 | `	return SXRET_OK;` |
|    177508 | 10088 | `}` |
|         - | 10089 | `/*` |
|         - | 10090 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10091 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10092 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10093 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10094 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10095 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10096 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10097 | ` */` |
|    401384 | 10098 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10099 | `{` |
|    401389 | 10100 | `	int isAbsolute = 0;` |
|    401389 | 10101 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10102 | `	SyBlob sName;` |
|    401389 | 10103 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4429 | 10104 | `		isAbsolute = 1;` |
|      4429 | 10105 | `		pGen->pIn++;` |
|      2212 | 10106 | `	}` |
|    401389 | 10107 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         8 | 10108 | `		pGen->pIn = pStart;` |
|         8 | 10109 | `		return SXERR_INVALID;` |
|         - | 10110 | `	}` |
|    401383 | 10111 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    401383 | 10112 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    401383 | 10113 | `	pGen->pIn++;` |
|    602088 | 10114 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    200715 | 10115 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10116 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10117 | `		pGen->pIn++;` |
|        16 | 10118 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10119 | `		pGen->pIn++;` |
|         2 | 10120 | `	}` |
|    401383 | 10121 | `	if( isAbsolute ){` |
|      4427 | 10122 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2216 | 10123 | `	}else{` |
|         - | 10124 | `		SyString sRaw;` |
|    396961 | 10125 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    396961 | 10126 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10127 | `	}` |
|    401383 | 10128 | `	SyBlobRelease(&sName);` |
|    401383 | 10129 | `	return SXRET_OK;` |
|    200697 | 10130 | `}` |
|         - | 10131 | `/*` |
|         - | 10132 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10133 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10134 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10135 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10136 | ` * either direction cannot run unbounded.` |
|         - | 10137 | ` */` |
|         - | 10138 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    165320 | 10139 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10140 | `{` |
|         - | 10141 | `	ph7_class **apParent;` |
|         - | 10142 | `	sxu32 n;` |
|    430523 | 10143 | `	while( pInterface ){` |
|    272893 | 10144 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10145 | `			return FALSE;` |
|         - | 10146 | `		}` |
|    307472 | 10147 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     69158 | 10148 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7695 | 10149 | `			return TRUE;` |
|         - | 10150 | `		}` |
|    265203 | 10151 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    265203 | 10152 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10153 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10154 | `				return TRUE;` |
|         - | 10155 | `			}` |
|       ! 0 | 10156 | `		}` |
|    265203 | 10157 | `		pInterface = pInterface->pBase;` |
|    265203 | 10158 | `		iDepth++;` |
|         5 | 10159 | `	}` |
|    157635 | 10160 | `	return FALSE;` |
|     82665 | 10161 | `}` |
|    165320 | 10162 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10163 | `{` |
|    165325 | 10164 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10165 | `}` |
|         - | 10166 | `/*` |
|         - | 10167 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10168 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10169 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10170 | ` */` |
|      7690 | 10171 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10172 | `{` |
|      7699 | 10173 | `	while( pBase ){` |
|        10 | 10174 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10175 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10176 | `			return TRUE;` |
|         - | 10177 | `		}` |
|        10 | 10178 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10179 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10180 | `			return TRUE;` |
|         - | 10181 | `		}` |
|         5 | 10182 | `		pBase = pBase->pBase;` |
|         1 | 10183 | `	}` |
|      7691 | 10184 | `	return FALSE;` |
|      3850 | 10185 | `}` |
|         - | 10186 | `/*` |
|         - | 10187 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10188 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10189 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10190 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10191 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10192 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10193 | ` * pClass->aEnumCases for cases().` |
|         - | 10194 | ` */` |
|      7722 | 10195 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10196 | `{` |
|      7727 | 10197 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10198 | `	SySet *pInstrContainer;` |
|         - | 10199 | `	ph7_class_attr *pCase;` |
|         - | 10200 | `	SyString *pName;` |
|         - | 10201 | `	sxi32 rc;` |
|      7727 | 10202 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7727 | 10203 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10204 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10205 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10206 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10207 | `			return SXERR_ABORT;` |
|         - | 10208 | `		}` |
|       ! 0 | 10209 | `		goto Synchronize;` |
|         - | 10210 | `	}` |
|      7727 | 10211 | `	pName = &pGen->pIn->sData;` |
|         - | 10212 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7727 | 10213 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10214 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10215 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10216 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10217 | `			return SXERR_ABORT;` |
|         - | 10218 | `		}` |
|       ! 0 | 10219 | `		goto Synchronize;` |
|         - | 10220 | `	}` |
|      7727 | 10221 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10222 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7727 | 10223 | `	if( pCase == 0 ){` |
|       ! 0 | 10224 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10225 | `		return SXERR_ABORT;` |
|         - | 10226 | `	}` |
|      7727 | 10227 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7727 | 10228 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10229 | `		return SXERR_ABORT;` |
|         - | 10230 | `	}` |
|      7727 | 10231 | `	pGen->pIn++; /* Jump the case name */` |
|      7727 | 10232 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7713 | 10233 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10234 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10235 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10236 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10237 | `				return SXERR_ABORT;` |
|         - | 10238 | `			}` |
|         6 | 10239 | `			goto Synchronize;` |
|         - | 10240 | `		}` |
|      7709 | 10241 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10242 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10243 | `		 * (same technique as class constants). */` |
|      7709 | 10244 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7709 | 10245 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7709 | 10246 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7709 | 10247 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10248 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10249 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10250 | `		}` |
|      7709 | 10251 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7709 | 10252 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7709 | 10253 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10254 | `			return SXERR_ABORT;` |
|         - | 10255 | `		}` |
|      3857 | 10256 | `	}else{` |
|        17 | 10257 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10258 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10259 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10260 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10261 | `				return SXERR_ABORT;` |
|         - | 10262 | `			}` |
|       ! 0 | 10263 | `			goto Synchronize;` |
|         - | 10264 | `		}` |
|         - | 10265 | `	}` |
|      7723 | 10266 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7723 | 10267 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10268 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10269 | `		return SXERR_ABORT;` |
|         - | 10270 | `	}` |
|      7723 | 10271 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7723 | 10272 | `	return SXRET_OK;` |
|         2 | 10273 | `Synchronize:` |
|         - | 10274 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10275 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10276 | `		pGen->pIn++;` |
|         2 | 10277 | `	}` |
|         6 | 10278 | `	return SXERR_CORRUPT;` |
|      3866 | 10279 | `}` |
|         - | 10280 | `/*` |
|         - | 10281 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10282 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10283 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10284 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10285 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10286 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10287 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10288 | ` */` |
|      3864 | 10289 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10290 | `{` |
|         - | 10291 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10292 | `	const char *zBack;` |
|         - | 10293 | `	SySet sToken;` |
|         - | 10294 | `	char *zSrc;` |
|         - | 10295 | `	sxu32 nSrc,nMax;` |
|      3869 | 10296 | `	sxi32 rc = SXRET_OK;` |
|      3869 | 10297 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3864 | 10298 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3869 | 10299 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3869 | 10300 | `	if( zSrc == 0 ){` |
|       ! 0 | 10301 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10302 | `		return SXERR_ABORT;` |
|         - | 10303 | `	}` |
|      3869 | 10304 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3869 | 10305 | `	if( pClass->nEnumBacking != 0 ){` |
|      5783 | 10306 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10307 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10308 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10309 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1926 | 10310 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1931 | 10311 | `	}else{` |
|        21 | 10312 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10313 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10314 | `	}` |
|      3869 | 10315 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3869 | 10316 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3869 | 10317 | `	pSaveIn = pGen->pIn;` |
|      3869 | 10318 | `	pSaveEnd = pGen->pEnd;` |
|      3869 | 10319 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3869 | 10320 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15437 | 10321 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11573 | 10322 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10323 | `	}` |
|      3869 | 10324 | `	pGen->pIn = pSaveIn;` |
|      3869 | 10325 | `	pGen->pEnd = pSaveEnd;` |
|      3869 | 10326 | `	SySetRelease(&sToken);` |
|      3869 | 10327 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1937 | 10328 | `}` |
|         - | 10329 | `/*` |
|         - | 10330 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10331 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10332 | ` */` |
|         - | 10333 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10334 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10335 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10336 | `};` |
|         - | 10337 | `/*` |
|         - | 10338 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10339 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10340 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10341 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10342 | ` * and before the class is installed.` |
|         - | 10343 | ` */` |
|      3864 | 10344 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10345 | `{` |
|         - | 10346 | `	SyHashEntry *pEntry;` |
|         - | 10347 | `	sxi32 rc;` |
|         - | 10348 | `	sxu32 n;` |
|         - | 10349 | `	/* php: "Enum %s cannot include properties" */` |
|      3869 | 10350 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11591 | 10351 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7729 | 10352 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7729 | 10353 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10354 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10355 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10356 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10357 | `				return SXERR_ABORT;` |
|         - | 10358 | `			}` |
|         3 | 10359 | `			break;` |
|         - | 10360 | `		}` |
|         5 | 10361 | `	}` |
|         - | 10362 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     54101 | 10363 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     75348 | 10364 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     50237 | 10365 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10366 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10367 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10368 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10369 | `				return SXERR_ABORT;` |
|         - | 10370 | `			}` |
|       ! 0 | 10371 | `		}` |
|     25121 | 10372 | `	}` |
|         - | 10373 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10374 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10375 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10376 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10377 | `	{` |
|         - | 10378 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10379 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10380 | `		ph7_class_attr *pAttr;` |
|      3869 | 10381 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10382 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3869 | 10383 | `		if( pAttr == 0 ){` |
|       ! 0 | 10384 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10385 | `			return SXERR_ABORT;` |
|         - | 10386 | `		}` |
|      3869 | 10387 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3869 | 10388 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3869 | 10389 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3869 | 10390 | `		if( pClass->nEnumBacking != 0 ){` |
|      3857 | 10391 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10392 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3857 | 10393 | `			if( pAttr == 0 ){` |
|       ! 0 | 10394 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10395 | `				return SXERR_ABORT;` |
|         - | 10396 | `			}` |
|      3857 | 10397 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3857 | 10398 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10399 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10400 | `			}else{` |
|      3851 | 10401 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10402 | `			}` |
|      3857 | 10403 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1926 | 10404 | `		}` |
|         - | 10405 | `	}` |
|      3869 | 10406 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1937 | 10407 | `}` |
|         - | 10408 | `/*` |
|         - | 10409 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10410 | ` *` |
|         - | 10411 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10412 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10413 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10414 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10415 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10416 | ` * implements, body, install) is shared by both paths.` |
|         - | 10417 | ` */` |
|    355050 | 10418 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10419 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10420 | `{` |
|    355055 | 10421 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10422 | `	ph7_class *pClass,*pBase;` |
|         - | 10423 | `	SyToken *pEnd,*pTmp;` |
|         - | 10424 | `	sxi32 iProtection;` |
|         - | 10425 | `	SySet aInterfaces;` |
|         - | 10426 | `	SySet aUseEntries;` |
|         - | 10427 | `	sxi32 iAttrflags;` |
|         - | 10428 | `	SyString *pName;` |
|         - | 10429 | `	sxi32 nKwrd;` |
|         - | 10430 | `	sxi32 rc;` |
|         - | 10431 | `	/* Jump the 'class' keyword */` |
|    355055 | 10432 | `	pGen->pIn++;` |
|    355055 | 10433 | `	if( pAnonName ){` |
|         - | 10434 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10435 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10436 | `		 * then use the synthesized name. */` |
|        32 | 10437 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10438 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10439 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10440 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10441 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10442 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10443 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10444 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10445 | `		}` |
|        32 | 10446 | `		pName = pAnonName;` |
|        32 | 10447 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10448 | `	}else{` |
|    355027 | 10449 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10450 | `			/* Syntax error */` |
|       ! 0 | 10451 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10452 | `			if( rc == SXERR_ABORT ){` |
|         - | 10453 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10454 | `				return SXERR_ABORT;` |
|         - | 10455 | `			}` |
|         - | 10456 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10457 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10458 | `				pGen->pIn++;` |
|       ! 0 | 10459 | `			}` |
|       ! 0 | 10460 | `			return SXRET_OK;` |
|         - | 10461 | `		}` |
|         - | 10462 | `		/* Extract class name */` |
|    355027 | 10463 | `		pName = &pGen->pIn->sData;` |
|         - | 10464 | `		/* Advance the stream cursor */` |
|    355027 | 10465 | `		pGen->pIn++;` |
|         - | 10466 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10467 | `			SyBlob sFQN;` |
|         - | 10468 | `			SyString sFQNStr;` |
|    355027 | 10469 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    355027 | 10470 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    355027 | 10471 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    355027 | 10472 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    355027 | 10473 | `			SyBlobRelease(&sFQN);` |
|         - | 10474 | `		}` |
|         - | 10475 | `	}` |
|    355055 | 10476 | `	if( pClass == 0 ){` |
|       ! 0 | 10477 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10478 | `		return SXERR_ABORT;` |
|         - | 10479 | `	}` |
|    355050 | 10480 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3873 | 10481 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10482 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3859 | 10483 | `		pGen->pIn++; /* Jump ':' */` |
|      3854 | 10484 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3859 | 10485 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10486 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10487 | `			pGen->pIn++;` |
|      3852 | 10488 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3853 | 10489 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3851 | 10490 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3851 | 10491 | `			pGen->pIn++;` |
|      1928 | 10492 | `		}else{` |
|         3 | 10493 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10494 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10495 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10496 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10497 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10498 | `				return SXERR_ABORT;` |
|         - | 10499 | `			}` |
|         3 | 10500 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10501 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10502 | `			}` |
|         - | 10503 | `		}` |
|      1927 | 10504 | `	}` |
|    355055 | 10505 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    355055 | 10506 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10507 | `		return SXERR_ABORT;` |
|         - | 10508 | `	}` |
|         - | 10509 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    355055 | 10510 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    355055 | 10511 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10512 | `	/* Assume a standalone class */` |
|    355055 | 10513 | `	pBase = 0;` |
|    355055 | 10514 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    288435 | 10515 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    288435 | 10516 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10517 | `			SyBlob sResolved;` |
|         - | 10518 | `			SyString sBaseName;` |
|         - | 10519 | `			sxu32 nRefLine;` |
|    184579 | 10520 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10521 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10522 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10523 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10524 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10525 | `					return SXERR_ABORT;` |
|         - | 10526 | `				}` |
|       ! 0 | 10527 | `			}` |
|    184579 | 10528 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    184579 | 10529 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    184579 | 10530 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    184579 | 10531 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10532 | `				SyBlobRelease(&sResolved);` |
|         4 | 10533 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10534 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10535 | `					pName);` |
|         3 | 10536 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10537 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10538 | `					return SXERR_ABORT;` |
|         - | 10539 | `				}` |
|         3 | 10540 | `				return SXRET_OK;` |
|         - | 10541 | `			}` |
|    276863 | 10542 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    184572 | 10543 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    184577 | 10544 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10545 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10546 | `			/* Interfaces are not allowed */` |
|    184577 | 10547 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10548 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10549 | `			}` |
|    184577 | 10550 | `			if( pBase == 0 ){` |
|       ! 0 | 10551 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10552 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10553 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10554 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10555 | `					return SXERR_ABORT;` |
|         - | 10556 | `				}` |
|       ! 0 | 10557 | `			}else{` |
|    184577 | 10558 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10559 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10560 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10561 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10562 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10563 | `						return SXERR_ABORT;` |
|         - | 10564 | `					}` |
|         3 | 10565 | `					pBase = 0; /* Never inherit from an enum */` |
|    184576 | 10566 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10567 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10568 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10569 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10570 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10571 | `						return SXERR_ABORT;` |
|         - | 10572 | `					}` |
|       ! 0 | 10573 | `				}` |
|         - | 10574 | `			}` |
|    184577 | 10575 | `			SyBlobRelease(&sResolved);` |
|    184577 | 10576 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10577 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10578 | `			}` |
|     92286 | 10579 | `		}` |
|    288433 | 10580 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10581 | `			ph7_class *pInterface;` |
|         - | 10582 | `			/* Interface implementation */` |
|    107713 | 10583 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    111466 | 10584 | `			for(;;){` |
|         - | 10585 | `				SyBlob sResolved;` |
|         - | 10586 | `				SyString sIntName;` |
|         - | 10587 | `				sxu32 nRefLine;` |
|    165325 | 10588 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    165325 | 10589 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    165325 | 10590 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10591 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10592 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10593 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10594 | `						pName);` |
|       ! 0 | 10595 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10596 | `						return SXERR_ABORT;` |
|         - | 10597 | `					}` |
|       ! 0 | 10598 | `					break;` |
|         - | 10599 | `				}` |
|    330645 | 10600 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    165320 | 10601 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    165325 | 10602 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10603 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10604 | `				/* Only interfaces are allowed */` |
|    165325 | 10605 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10606 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10607 | `				}` |
|    165325 | 10608 | `				if( pInterface == 0 ){` |
|       ! 0 | 10609 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10610 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10611 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10612 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10613 | `						return SXERR_ABORT;` |
|         - | 10614 | `					}` |
|       ! 0 | 10615 | `				}else{` |
|         - | 10616 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10617 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10618 | `					 * unless they already extend Exception or Error.` |
|         - | 10619 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10620 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10621 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    165325 | 10622 | `					SyString *pFqn = &pClass->sName;` |
|    165325 | 10623 | `					int bIsExceptionOrError =` |
|     86504 | 10624 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    249904 | 10625 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    163407 | 10626 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3854 | 10627 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    169165 | 10628 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11538 | 10629 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3843 | 10630 | `						!bIsExceptionOrError ){` |
|        12 | 10631 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10632 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10633 | `							&pClass->sName);` |
|         9 | 10634 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10635 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10636 | `							return SXERR_ABORT;` |
|         - | 10637 | `						}` |
|         - | 10638 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10639 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10640 | `					}else{` |
|    165319 | 10641 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10642 | `					}` |
|         - | 10643 | `				}` |
|    165325 | 10644 | `				SyBlobRelease(&sResolved);` |
|    165325 | 10645 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53859 | 10646 | `					break;` |
|         - | 10647 | `				}` |
|     57617 | 10648 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10649 | `			}` |
|     53854 | 10650 | `		}` |
|    144214 | 10651 | `	}` |
|    355053 | 10652 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10653 | `		/* Syntax error */` |
|       ! 0 | 10654 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10655 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10656 | `		if( rc == SXERR_ABORT ){` |
|         - | 10657 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10658 | `			return SXERR_ABORT;` |
|         - | 10659 | `		}` |
|       ! 0 | 10660 | `		return SXRET_OK;` |
|         - | 10661 | `	}` |
|    355053 | 10662 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    355053 | 10663 | `	pEnd = 0; /* cc warning */` |
|         - | 10664 | `	/* Delimit the class body */` |
|    355053 | 10665 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    355053 | 10666 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10667 | `		/* Syntax error */` |
|       ! 0 | 10668 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10669 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10670 | `		if( rc == SXERR_ABORT ){` |
|         - | 10671 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10672 | `			return SXERR_ABORT;` |
|         - | 10673 | `		}` |
|       ! 0 | 10674 | `		return SXRET_OK;` |
|         - | 10675 | `	}` |
|         - | 10676 | `	/* The delimiter token is the class body's closing brace */` |
|    355053 | 10677 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10678 | `	/* Swap token stream */` |
|    355053 | 10679 | `	pTmp = pGen->pEnd;` |
|    355053 | 10680 | `	pGen->pEnd = pEnd;` |
|         - | 10681 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    355053 | 10682 | `	pClass->iFlags \|= iFlags;` |
|         - | 10683 | `	/* Start the parse process */` |
|   1374470 | 10684 | `	for(;;){` |
|         - | 10685 | `		/* Jump leading/trailing semi-colons */` |
|   3918457 | 10686 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    707671 | 10687 | `			pGen->pIn++;` |
|         5 | 10688 | `		}` |
|   3210791 | 10689 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10690 | `			/* End of class body */` |
|    355011 | 10691 | `			break;` |
|         - | 10692 | `		}` |
|         - | 10693 | `		/* Bind a directly-preceding docblock to this member */` |
|   2855785 | 10694 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2855780 | 10695 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1427895 | 10696 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10697 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10698 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10699 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10700 | `			if( rc == SXERR_ABORT ){` |
|         - | 10701 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10702 | `				return SXERR_ABORT;` |
|         - | 10703 | `			}` |
|       ! 0 | 10704 | `			goto done;` |
|         - | 10705 | `		}` |
|         - | 10706 | `		/* Assume public visibility */` |
|   2855785 | 10707 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2855785 | 10708 | `		iAttrflags = 0;` |
|         - | 10709 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10710 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10711 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10712 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2855785 | 10713 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10714 | `			int bMod = 0;` |
|       ! 0 | 10715 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10716 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10717 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10718 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10719 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10720 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10721 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10722 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10723 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10724 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10725 | `			}` |
|       ! 0 | 10726 | `			if( !bMod ){` |
|       ! 0 | 10727 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10728 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10729 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10730 | `						return SXERR_ABORT;` |
|         - | 10731 | `					}` |
|       ! 0 | 10732 | `					goto done;` |
|         - | 10733 | `				}` |
|       ! 0 | 10734 | `				continue;` |
|         - | 10735 | `			}` |
|       ! 0 | 10736 | `		}` |
|   2855785 | 10737 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10738 | `			/* Extract the current keyword */` |
|   2855785 | 10739 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2855785 | 10740 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10741 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7727 | 10742 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7727 | 10743 | `				if( rc != SXRET_OK ){` |
|         6 | 10744 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10745 | `						return SXERR_ABORT;` |
|         - | 10746 | `					}` |
|         6 | 10747 | `					goto done;` |
|         - | 10748 | `				}` |
|      7723 | 10749 | `				continue;` |
|         - | 10750 | `			}` |
|   2848063 | 10751 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 10752 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 10753 | `				TraitUseEntry sUse;` |
|     15429 | 10754 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15429 | 10755 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15429 | 10756 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7720 | 10757 | `				for(;;){` |
|         - | 10758 | `					ph7_class *pTrait;` |
|         - | 10759 | `					SyString *pTraitName;` |
|     15437 | 10760 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 10761 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10762 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 10763 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10764 | `							return SXERR_ABORT;` |
|         - | 10765 | `						}` |
|       ! 0 | 10766 | `						break;` |
|         - | 10767 | `					}` |
|     15437 | 10768 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 10769 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 10770 | `						SyBlob sResolved;` |
|     15437 | 10771 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15437 | 10772 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     30869 | 10773 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15432 | 10774 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15437 | 10775 | `						SyBlobRelease(&sResolved);` |
|         - | 10776 | `					}` |
|         - | 10777 | `					/* Only traits are allowed */` |
|     15437 | 10778 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 10779 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 10780 | `					}` |
|     15437 | 10781 | `					if( pTrait == 0 ){` |
|       ! 0 | 10782 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10783 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 10784 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10785 | `							return SXERR_ABORT;` |
|         - | 10786 | `						}` |
|       ! 0 | 10787 | `					}else{` |
|     15437 | 10788 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 10789 | `					}` |
|     15437 | 10790 | `					pGen->pIn++; /* Advance past trait name */` |
|     15437 | 10791 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7717 | 10792 | `						break;` |
|         - | 10793 | `					}` |
|        10 | 10794 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 10795 | `				}` |
|         - | 10796 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15429 | 10797 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 10798 | `					SyToken *pBlock;` |
|        13 | 10799 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 10800 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 10801 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 10802 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 10803 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 10804 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 10805 | `					}else{` |
|       ! 0 | 10806 | `						pGen->pIn = pGen->pEnd;` |
|         - | 10807 | `					}` |
|         5 | 10808 | `				}` |
|     15429 | 10809 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 10810 | `				/* The semicolon will be consumed by the outer loop */` |
|     15429 | 10811 | `				continue;` |
|         - | 10812 | `			}` |
|   2832639 | 10813 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 10814 | `				int nSetTok;` |
|   2586387 | 10815 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2586387 | 10816 | `				if( nSetVis ){` |
|         - | 10817 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 10818 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 10819 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10820 | `					pGen->pIn += nSetTok;` |
|         2 | 10821 | `				}else{` |
|   2586385 | 10822 | `					iProtection = nKwrd;` |
|   2586385 | 10823 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 10824 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 10825 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2586385 | 10826 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2586385 | 10827 | `					if( nSetVis ){` |
|         9 | 10828 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 10829 | `						pGen->pIn += nSetTok;` |
|         4 | 10830 | `					}` |
|         - | 10831 | `				}` |
|         - | 10832 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 10833 | ``				 * `public private(set) readonly int $x`. */`` |
|   2586387 | 10834 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 10835 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 10836 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 10837 | `				}` |
|   2586382 | 10838 | `				if( pGen->pIn >= pGen->pEnd` |
|   2586387 | 10839 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10840 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10841 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10842 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 10843 | `					if( rc == SXERR_ABORT ){` |
|         - | 10844 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 10845 | `						return SXERR_ABORT;` |
|         - | 10846 | `					}` |
|       ! 0 | 10847 | `					goto done;` |
|         - | 10848 | `				}` |
|   2586387 | 10849 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10850 | `					/* Attribute declaration (untyped) */` |
|    411493 | 10851 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    411493 | 10852 | `					if( rc != SXRET_OK ){` |
|        11 | 10853 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10854 | `							return SXERR_ABORT;` |
|         - | 10855 | `						}` |
|        11 | 10856 | `						goto done;` |
|         - | 10857 | `					}` |
|    411629 | 10858 | `					continue;` |
|         - | 10859 | `				}` |
|   2174899 | 10860 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10861 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       299 | 10862 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       299 | 10863 | `					if( rc != SXRET_OK ){` |
|         8 | 10864 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10865 | `							return SXERR_ABORT;` |
|         - | 10866 | `						}` |
|         8 | 10867 | `						goto done;` |
|         - | 10868 | `					}` |
|       293 | 10869 | `					continue;` |
|         - | 10870 | `				}` |
|         - | 10871 | `				/* Extract the keyword */` |
|   2174605 | 10872 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1087300 | 10873 | `			}` |
|   2420857 | 10874 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 10875 | `				/* Process constant declaration */` |
|    238235 | 10876 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    238235 | 10877 | `				if( rc != SXRET_OK ){` |
|        11 | 10878 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10879 | `						return SXERR_ABORT;` |
|         - | 10880 | `					}` |
|        11 | 10881 | `					goto done;` |
|         - | 10882 | `				}` |
|    119116 | 10883 | `			}else{` |
|   2182627 | 10884 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 10885 | `					/* Static method or attribute,record that */` |
|     96145 | 10886 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     96145 | 10887 | `					pGen->pIn++; /* Jump the static keyword */` |
|     96145 | 10888 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10889 | `						int nSetTok;` |
|     69235 | 10890 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     69235 | 10891 | `						if( nSetVis ){` |
|         - | 10892 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 10893 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10894 | `							pGen->pIn += nSetTok;` |
|         2 | 10895 | `						}else{` |
|         - | 10896 | `							/* Extract the keyword */` |
|     69233 | 10897 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     69233 | 10898 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 10899 | `								iProtection = nKwrd;` |
|       ! 0 | 10900 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 10901 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 10902 | `								if( nSetVis ){` |
|       ! 0 | 10903 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 10904 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 10905 | `								}` |
|       ! 0 | 10906 | `							}` |
|         - | 10907 | `						}` |
|     34615 | 10908 | `					}` |
|         - | 10909 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 10910 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 10911 | `					 * than a generic "expecting method" parse error. */` |
|     96145 | 10912 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10913 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10914 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 10915 | `					}` |
|     96140 | 10916 | `					if( pGen->pIn >= pGen->pEnd` |
|     96145 | 10917 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10918 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10919 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 10920 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 10921 | `						if( rc == SXERR_ABORT ){` |
|         - | 10922 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 10923 | `							return SXERR_ABORT;` |
|         - | 10924 | `						}` |
|       ! 0 | 10925 | `						goto done;` |
|         - | 10926 | `					}` |
|     96145 | 10927 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10928 | `						/* Attribute declaration */` |
|     26913 | 10929 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26913 | 10930 | `						if( rc != SXRET_OK ){` |
|         3 | 10931 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10932 | `								return SXERR_ABORT;` |
|         - | 10933 | `							}` |
|         3 | 10934 | `							goto done;` |
|         - | 10935 | `						}` |
|     26911 | 10936 | `						continue;` |
|         - | 10937 | `					}` |
|     69237 | 10938 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10939 | `						/* Typed static attribute declaration */` |
|        17 | 10940 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        17 | 10941 | `						if( rc != SXRET_OK ){` |
|         3 | 10942 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10943 | `								return SXERR_ABORT;` |
|         - | 10944 | `							}` |
|         3 | 10945 | `							goto done;` |
|         - | 10946 | `						}` |
|        15 | 10947 | `						continue;` |
|         - | 10948 | `					}` |
|         - | 10949 | `					/* Extract the keyword */` |
|     69223 | 10950 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2121096 | 10951 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 10952 | `					/* Abstract method,record that */` |
|      7703 | 10953 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 10954 | `					/* Mark the whole class as abstract */` |
|      7703 | 10955 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 10956 | `					/* Advance the stream cursor */` |
|      7703 | 10957 | `					pGen->pIn++;` |
|      7703 | 10958 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7703 | 10959 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7703 | 10960 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7701 | 10961 | `							iProtection = nKwrd;` |
|      7701 | 10962 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3848 | 10963 | `						}` |
|      3849 | 10964 | `					}` |
|      7703 | 10965 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7698 | 10966 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 10967 | `							/* Static method */` |
|       ! 0 | 10968 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 10969 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 10970 | `					}` |
|      7703 | 10971 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7698 | 10972 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 10973 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 10974 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 10975 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 10976 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 10977 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 10978 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 10979 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 10980 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 10981 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 10982 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 10983 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 10984 | `										return SXERR_ABORT;` |
|         - | 10985 | `									}` |
|       ! 0 | 10986 | `									goto done;` |
|         - | 10987 | `								}` |
|         7 | 10988 | `								continue;` |
|         - | 10989 | `							}` |
|       ! 0 | 10990 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10991 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 10992 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 10993 | `							if( rc == SXERR_ABORT ){` |
|         - | 10994 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 10995 | `								return SXERR_ABORT;` |
|         - | 10996 | `							}` |
|       ! 0 | 10997 | `							goto done;` |
|         - | 10998 | `					}` |
|      7697 | 10999 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2082635 | 11000 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11001 | `					/* final method ,record that */` |
|        20 | 11002 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        20 | 11003 | `					pGen->pIn++; /* Jump the final keyword */` |
|        20 | 11004 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11005 | `						/* Extract the keyword */` |
|        20 | 11006 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        20 | 11007 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        10 | 11008 | `							iProtection = nKwrd;` |
|        10 | 11009 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11010 | `						}` |
|         9 | 11011 | `					}` |
|        20 | 11012 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11013 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11014 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11015 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11016 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11017 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11018 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11019 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11020 | `									return SXERR_ABORT;` |
|         - | 11021 | `								}` |
|       ! 0 | 11022 | `								goto done;` |
|         - | 11023 | `							}` |
|        14 | 11024 | `							continue;` |
|         - | 11025 | `					}` |
|         8 | 11026 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11027 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11028 | `							/* Static method */` |
|       ! 0 | 11029 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11030 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11031 | `					}` |
|         8 | 11032 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11033 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11034 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11035 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11036 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11037 | `							if( rc == SXERR_ABORT ){` |
|         - | 11038 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11039 | `								return SXERR_ABORT;` |
|         - | 11040 | `							}` |
|       ! 0 | 11041 | `							goto done;` |
|         - | 11042 | `					}` |
|         8 | 11043 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11044 | `				}` |
|   2155687 | 11045 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11046 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11047 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11048 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11049 | `						if( rc == SXERR_ABORT ){` |
|         - | 11050 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11051 | `							return SXERR_ABORT;` |
|         - | 11052 | `						}` |
|       ! 0 | 11053 | `						goto done;` |
|         - | 11054 | `				}` |
|   2155687 | 11055 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11056 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11057 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11058 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11059 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11060 | `						if( rc == SXERR_ABORT ){` |
|         - | 11061 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11062 | `							return SXERR_ABORT;` |
|         - | 11063 | `						}` |
|       ! 0 | 11064 | `						goto done;` |
|         - | 11065 | `					}` |
|         - | 11066 | `					/* Attribute declaration */` |
|         7 | 11067 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11068 | `				}else{` |
|         - | 11069 | `					/* Process method declaration */` |
|   2155681 | 11070 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11071 | `				}` |
|   2155687 | 11072 | `				if( rc != SXRET_OK ){` |
|        16 | 11073 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11074 | `						return SXERR_ABORT;` |
|         - | 11075 | `					}` |
|        16 | 11076 | `					goto done;` |
|         - | 11077 | `				}` |
|         - | 11078 | `			}` |
|   1196951 | 11079 | `		}else{` |
|         - | 11080 | `			/* Attribute declaration */` |
|       ! 0 | 11081 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11082 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11083 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11084 | `					return SXERR_ABORT;` |
|         - | 11085 | `				}` |
|       ! 0 | 11086 | `				goto done;` |
|         - | 11087 | `			}` |
|         - | 11088 | `		}` |
|         5 | 11089 | `	}` |
|         - | 11090 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11091 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11092 | `	 */` |
|         - | 11093 | `	{` |
|         - | 11094 | `		TraitUseEntry *apUse;` |
|         - | 11095 | `		sxu32 nU;` |
|    355011 | 11096 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    370435 | 11097 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15429 | 11098 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15429 | 11099 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15429 | 11100 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15429 | 11101 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11102 | `			sxu32 nT;` |
|     15429 | 11103 | `			if( !hasResolution ){` |
|         - | 11104 | `				/* No conflict resolution block: use standard trait application */` |
|     30839 | 11105 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15425 | 11106 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15425 | 11107 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11108 | `						break;` |
|         - | 11109 | `					}` |
|      7715 | 11110 | `				}` |
|      7712 | 11111 | `			}else{` |
|         - | 11112 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11113 | `				 * then use the block to resolve method conflicts.` |
|         - | 11114 | `				 */` |
|         - | 11115 | `				SyToken *pR;` |
|        25 | 11116 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11117 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11118 | `					ph7_class_attr *pAR;` |
|         - | 11119 | `					SyHashEntry *pER;` |
|         - | 11120 | `					SyString *pNR;` |
|        15 | 11121 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11122 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11123 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11124 | `						pNR = &pAR->sName;` |
|       ! 0 | 11125 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11126 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11127 | `						}` |
|       ! 0 | 11128 | `					}` |
|        15 | 11129 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11130 | `				}` |
|         - | 11131 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11132 | `				pR = pUse->pResolvStart;` |
|        27 | 11133 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11134 | `					SyString sTrait,sMethod;` |
|         - | 11135 | `					ph7_class *pSrcTrait;` |
|         - | 11136 | `					ph7_class_method *pMeth;` |
|         - | 11137 | `					sxi32 nRKwrd;` |
|        41 | 11138 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11139 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11140 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11141 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11142 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11143 | `					sMethod = pR->sData;` |
|        17 | 11144 | `					pR++;` |
|        17 | 11145 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11146 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11147 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11148 | `							sTrait = sMethod;` |
|         7 | 11149 | `							pR++;` |
|         7 | 11150 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11151 | `							sMethod = pR->sData;` |
|         7 | 11152 | `							pR++;` |
|         3 | 11153 | `						}` |
|         3 | 11154 | `					}` |
|        17 | 11155 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11156 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11157 | `						continue;` |
|         - | 11158 | `					}` |
|        17 | 11159 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11160 | `					pR++;` |
|        17 | 11161 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11162 | `						pSrcTrait = 0;` |
|         7 | 11163 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11164 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11165 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11166 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11167 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11168 | `								break;` |
|         - | 11169 | `							}` |
|         2 | 11170 | `						}` |
|         5 | 11171 | `						if( pSrcTrait ){` |
|         5 | 11172 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11173 | `							if( pMeth ){` |
|         5 | 11174 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11175 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11176 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11177 | `								}` |
|         2 | 11178 | `							}` |
|         2 | 11179 | `						}` |
|         2 | 11180 | `					}` |
|        35 | 11181 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11182 | `				}` |
|         - | 11183 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11184 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11185 | `					ph7_class_method *pMR;` |
|         - | 11186 | `					SyHashEntry *pER;` |
|         - | 11187 | `					SyString *pNR;` |
|        15 | 11188 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11189 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11190 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11191 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11192 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11193 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11194 | `						}` |
|         3 | 11195 | `					}` |
|         9 | 11196 | `				}` |
|         - | 11197 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11198 | `				pR = pUse->pResolvStart;` |
|        27 | 11199 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11200 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11201 | `					ph7_class *pSrcTrait;` |
|         - | 11202 | `					ph7_class_method *pMeth;` |
|        27 | 11203 | `					int hasQual = 0;` |
|         - | 11204 | `					sxi32 nRKwrd;` |
|        41 | 11205 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11206 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11207 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11208 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11209 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11210 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11211 | `					sMethod = pR->sData;` |
|        17 | 11212 | `					pR++;` |
|        17 | 11213 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11214 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11215 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11216 | `							sTrait = sMethod;` |
|         7 | 11217 | `							hasQual = 1;` |
|         7 | 11218 | `							pR++;` |
|         7 | 11219 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11220 | `							sMethod = pR->sData;` |
|         7 | 11221 | `							pR++;` |
|         3 | 11222 | `						}` |
|         3 | 11223 | `					}` |
|        17 | 11224 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11225 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11226 | `						continue;` |
|         - | 11227 | `					}` |
|        17 | 11228 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11229 | `					pR++;` |
|        17 | 11230 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11231 | `						sxi32 iNewVis = -1;` |
|        13 | 11232 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11233 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11234 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11235 | `								iNewVis = nAK;` |
|         7 | 11236 | `								pR++;` |
|         3 | 11237 | `							}` |
|         3 | 11238 | `						}` |
|        13 | 11239 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11240 | `							sAlias = pR->sData;` |
|        11 | 11241 | `							pR++;` |
|         4 | 11242 | `						}` |
|        13 | 11243 | `						pMeth = 0;` |
|        13 | 11244 | `						if( hasQual ){` |
|         3 | 11245 | `							pSrcTrait = 0;` |
|         5 | 11246 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11247 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11248 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11249 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11250 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11251 | `									break;` |
|         - | 11252 | `								}` |
|         2 | 11253 | `							}` |
|         3 | 11254 | `							if( pSrcTrait ){` |
|         3 | 11255 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11256 | `							}` |
|         2 | 11257 | `						}else{` |
|        10 | 11258 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11259 | `						}` |
|        13 | 11260 | `						if( pMeth ){` |
|        13 | 11261 | `							if( sAlias.nByte > 0 ){` |
|         - | 11262 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11263 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11264 | `								 */` |
|         - | 11265 | `								ph7_class_method *pAlias;` |
|         - | 11266 | `								char *zAliasDup;` |
|        11 | 11267 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11268 | `								if( pAlias ){` |
|        11 | 11269 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11270 | `									if( iNewVis >= 0 ){` |
|         5 | 11271 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11272 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11273 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11274 | `									}` |
|        11 | 11275 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11276 | `									if( zAliasDup ){` |
|        11 | 11277 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11278 | `									}` |
|         7 | 11279 | `								}` |
|         7 | 11280 | `							}else if( iNewVis >= 0 ){` |
|         - | 11281 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11282 | `								ph7_class_method *pCopy;` |
|         3 | 11283 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11284 | `								if( pCopy ){` |
|         3 | 11285 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11286 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11287 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11288 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11289 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11290 | `									/* Replace the method in the class hash */` |
|         3 | 11291 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11292 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11293 | `								}` |
|         1 | 11294 | `							}` |
|         5 | 11295 | `						}` |
|         5 | 11296 | `						SXUNUSED(hasQual);` |
|         5 | 11297 | `					}` |
|        21 | 11298 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11299 | `				}` |
|         - | 11300 | `			}` |
|     15429 | 11301 | `			SySetRelease(&pUse->aTraits);` |
|      7717 | 11302 | `		}` |
|         - | 11303 | `	}` |
|    355011 | 11304 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11305 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11306 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3869 | 11307 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3869 | 11308 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11309 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11310 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11311 | `			return SXERR_ABORT;` |
|         - | 11312 | `		}` |
|      1932 | 11313 | `	}` |
|         - | 11314 | `	/* Install the class */` |
|    355011 | 11315 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    355011 | 11316 | `	if( rc == SXRET_OK ){` |
|         - | 11317 | `		ph7_class **apInterface;` |
|         - | 11318 | `		sxu32 n;` |
|    355011 | 11319 | `		if( pBase ){` |
|         - | 11320 | `			/* Inherit from base class and mark as a subclass */` |
|    184575 | 11321 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     92285 | 11322 | `		}` |
|    355011 | 11323 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    520325 | 11324 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11325 | `			/* Implements one or more interface */` |
|    165319 | 11326 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    165319 | 11327 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11328 | `				break;` |
|         - | 11329 | `			}` |
|     82662 | 11330 | `		}` |
|         - | 11331 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11332 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    355011 | 11333 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3869 | 11334 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3869 | 11335 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11336 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11337 | `			}` |
|      3869 | 11338 | `			if( pIntf ){` |
|      3869 | 11339 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1932 | 11340 | `			}` |
|      3869 | 11341 | `			if( pClass->nEnumBacking != 0 ){` |
|      3857 | 11342 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3857 | 11343 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11344 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11345 | `				}` |
|      3857 | 11346 | `				if( pIntf ){` |
|      3857 | 11347 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1926 | 11348 | `				}` |
|      1926 | 11349 | `			}` |
|      1932 | 11350 | `		}` |
|         - | 11351 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11352 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    355006 | 11353 | `		if( rc == SXRET_OK` |
|    355006 | 11354 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    355011 | 11355 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    188269 | 11356 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11357 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    188269 | 11358 | `			if( pStringable ){` |
|    188269 | 11359 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    188269 | 11360 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11361 | `				sxu32 i;` |
|    188269 | 11362 | `				int bAlready = 0;` |
|    226673 | 11363 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     42251 | 11364 | `					if( apImpl[i] == pStringable ){` |
|      3847 | 11365 | `						bAlready = 1;` |
|      3847 | 11366 | `						break;` |
|         - | 11367 | `					}` |
|     19207 | 11368 | `				}` |
|    188269 | 11369 | `				if( !bAlready ){` |
|    184427 | 11370 | `					PH7_ClassImplement(pClass,pStringable);` |
|     92211 | 11371 | `				}` |
|     94132 | 11372 | `			}` |
|     94132 | 11373 | `		}` |
|         - | 11374 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    355011 | 11375 | `		if( rc == SXRET_OK ){` |
|    355011 | 11376 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    355011 | 11377 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11378 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11379 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11380 | `				return SXERR_ABORT;` |
|         - | 11381 | `			}` |
|    177503 | 11382 | `		}` |
|         - | 11383 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    355011 | 11384 | `		if( rc == SXRET_OK ){` |
|    355011 | 11385 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    355011 | 11386 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11387 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11388 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11389 | `				return SXERR_ABORT;` |
|         - | 11390 | `			}` |
|    177503 | 11391 | `		}` |
|    177503 | 11392 | `	}` |
|    355011 | 11393 | `	SySetRelease(&aUseEntries);` |
|    355011 | 11394 | `	SySetRelease(&aInterfaces);` |
|    355011 | 11395 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11396 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11397 | `		return SXERR_ABORT;` |
|         - | 11398 | `	}` |
|    177503 | 11399 | `done:` |
|         - | 11400 | `	/* Point beyond the class body */` |
|    355053 | 11401 | `	pGen->pIn = &pEnd[1];` |
|    355053 | 11402 | `	pGen->pEnd = pTmp;` |
|    355053 | 11403 | `	return PH7_OK;` |
|    177530 | 11404 | `}` |
|         - | 11405 | `/* Compile a named class declaration (the common case). */` |
|    355022 | 11406 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11407 | `{` |
|    355027 | 11408 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11409 | `}` |
|         - | 11410 | `/*` |
|         - | 11411 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11412 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11413 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11414 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11415 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11416 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11417 | ` */` |
|        28 | 11418 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11419 | `{` |
|         - | 11420 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11421 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11422 | `	SyString sName;` |
|         - | 11423 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11424 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11425 | `	                              * is keyed to this 'class' token */` |
|         - | 11426 | `	ph7_value *pObj;` |
|        32 | 11427 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11428 | `	sxu32 nIdx,nLen;` |
|         - | 11429 | `	sxi32 nArg,rc;` |
|        14 | 11430 | `	SXUNUSED(iCompileFlag);` |
|         - | 11431 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11432 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11433 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11434 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11435 | `	}` |
|        32 | 11436 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11437 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11438 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11439 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11440 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11441 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11442 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11443 | `		return rc;` |
|         - | 11444 | `	}` |
|         - | 11445 | `	{` |
|         - | 11446 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11447 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11448 | `		if( pAnonClass` |
|        32 | 11449 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11450 | `			return SXERR_ABORT;` |
|         - | 11451 | `		}` |
|         - | 11452 | `	}` |
|         - | 11453 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11454 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11455 | `	nArg = 0;` |
|        32 | 11456 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11457 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11458 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11459 | `		SyToken *pArgNext;` |
|         7 | 11460 | `		pGen->pIn = pArgStart;` |
|         7 | 11461 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11462 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11463 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11464 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11465 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11466 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11467 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11468 | `					return SXERR_ABORT;` |
|         - | 11469 | `				}` |
|         7 | 11470 | `				nArg++;` |
|         3 | 11471 | `			}` |
|         7 | 11472 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11473 | `		}` |
|         7 | 11474 | `		pGen->pIn = pSavedIn;` |
|         7 | 11475 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11476 | `	}` |
|         - | 11477 | `	/* Load the synthesized class name */` |
|        32 | 11478 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11479 | `	if( pObj == 0 ){` |
|       ! 0 | 11480 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11481 | `		return SXERR_ABORT;` |
|         - | 11482 | `	}` |
|        32 | 11483 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11484 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11485 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11486 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11487 | `	return SXRET_OK;` |
|        18 | 11488 | `}` |
|         - | 11489 | `/*` |
|         - | 11490 | ` * Compile a user-defined abstract class.` |
|         - | 11491 | ` *  According to the PHP language reference manual` |
|         - | 11492 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11493 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11494 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11495 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11496 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11497 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11498 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11499 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11500 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11501 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11502 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11503 | ` *   could differ.` |
|         - | 11504 | ` */` |
|         - | 11505 | `/*` |
|         - | 11506 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11507 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11508 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11509 | ` */` |
|  10956028 | 11510 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11511 | `{` |
|  10956033 | 11512 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   6472601 | 11513 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   6472601 | 11514 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   6426495 | 11515 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3197842 | 11516 | `	}` |
|  10879121 | 11517 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  10879061 | 11518 | `	return FALSE;` |
|   5478019 | 11519 | `}` |
|         - | 11520 | `/*` |
|         - | 11521 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11522 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11523 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11524 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11525 | ` */` |
|  10879056 | 11526 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11527 | `{` |
|  10879061 | 11528 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  10879061 | 11529 | `	sxi32 iFlags = 0,iFlag;` |
|  10956033 | 11530 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76977 | 11531 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11532 | `			pDup = pIn;` |
|         2 | 11533 | `		}` |
|     76977 | 11534 | `		iFlags \|= iFlag;` |
|     76977 | 11535 | `		pIn++;` |
|         5 | 11536 | `	}` |
|  10879061 | 11537 | `	*ppIn = pIn;` |
|  10879061 | 11538 | `	if( ppDup ){ *ppDup = pDup; }` |
|  10879061 | 11539 | `	return iFlags;` |
|         5 | 11540 | `}` |
|         - | 11541 | `/*` |
|         - | 11542 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11543 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11544 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11545 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11546 | `` * `readonly`) to their existing handlers.`` |
|         - | 11547 | ` */` |
|  10844420 | 11548 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11549 | `{` |
|  10844425 | 11550 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   5464533 | 11551 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  10865580 | 11552 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11553 | `}` |
|         - | 11554 | `/*` |
|         - | 11555 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11556 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11557 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11558 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11559 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11560 | ` */` |
|     34636 | 11561 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11562 | `{` |
|         - | 11563 | `	SyToken *pDup;` |
|     34641 | 11564 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11565 | `	sxi32 rc;` |
|     34641 | 11566 | `	if( pDup ){` |
|         4 | 11567 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11568 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11569 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11570 | `			return SXERR_ABORT;` |
|         - | 11571 | `		}` |
|         1 | 11572 | `	}` |
|     34636 | 11573 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17323 | 11574 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11575 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11576 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11577 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11578 | `			return SXERR_ABORT;` |
|         - | 11579 | `		}` |
|         1 | 11580 | `	}` |
|     34641 | 11581 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17323 | 11582 | `}` |
|         - | 11583 | `/*` |
|         - | 11584 | ` * Compile a user-defined trait.` |
|         - | 11585 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11586 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11587 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11588 | ` */` |
|      7758 | 11589 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11590 | `{` |
|      7763 | 11591 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11592 | `	ph7_class *pClass;` |
|         - | 11593 | `	SyToken *pEnd,*pTmp;` |
|         - | 11594 | `	sxi32 iProtection;` |
|         - | 11595 | `	sxi32 iAttrflags;` |
|         - | 11596 | `	SyString *pName;` |
|         - | 11597 | `	sxi32 nKwrd;` |
|         - | 11598 | `	sxi32 rc;` |
|         - | 11599 | `	/* Jump the 'trait' keyword */` |
|      7763 | 11600 | `	pGen->pIn++;` |
|      7763 | 11601 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11602 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11603 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11604 | `			return SXERR_ABORT;` |
|         - | 11605 | `		}` |
|       ! 0 | 11606 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11607 | `			pGen->pIn++;` |
|       ! 0 | 11608 | `		}` |
|       ! 0 | 11609 | `		return SXRET_OK;` |
|         - | 11610 | `	}` |
|         - | 11611 | `	/* Extract trait name */` |
|      7763 | 11612 | `	pName = &pGen->pIn->sData;` |
|      7763 | 11613 | `	pGen->pIn++;` |
|         - | 11614 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11615 | `		SyBlob sFQN;` |
|         - | 11616 | `		SyString sFQNStr;` |
|      7763 | 11617 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7763 | 11618 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7763 | 11619 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7763 | 11620 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7763 | 11621 | `		SyBlobRelease(&sFQN);` |
|         - | 11622 | `	}` |
|      7763 | 11623 | `	if( pClass == 0 ){` |
|       ! 0 | 11624 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11625 | `		return SXERR_ABORT;` |
|         - | 11626 | `	}` |
|      7763 | 11627 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7763 | 11628 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11629 | `		return SXERR_ABORT;` |
|         - | 11630 | `	}` |
|         - | 11631 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7763 | 11632 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11633 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11634 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11635 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11636 | `			return SXERR_ABORT;` |
|         - | 11637 | `		}` |
|       ! 0 | 11638 | `		return SXRET_OK;` |
|         - | 11639 | `	}` |
|      7763 | 11640 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7763 | 11641 | `	pEnd = 0;` |
|      7763 | 11642 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7763 | 11643 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11644 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11645 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11646 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11647 | `			return SXERR_ABORT;` |
|         - | 11648 | `		}` |
|       ! 0 | 11649 | `		return SXRET_OK;` |
|         - | 11650 | `	}` |
|         - | 11651 | `	/* The delimiter token is the trait body's closing brace */` |
|      7763 | 11652 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11653 | `	/* Swap token stream */` |
|      7763 | 11654 | `	pTmp = pGen->pEnd;` |
|      7763 | 11655 | `	pGen->pEnd = pEnd;` |
|         - | 11656 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7763 | 11657 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11658 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     53839 | 11659 | `	for(;;){` |
|    146131 | 11660 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     19231 | 11661 | `			pGen->pIn++;` |
|         5 | 11662 | `		}` |
|    126905 | 11663 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7763 | 11664 | `			break;` |
|         - | 11665 | `		}` |
|         - | 11666 | `		/* Bind a directly-preceding docblock to this member */` |
|    119147 | 11667 | `		GenStateSetPendingDoc(&(*pGen));` |
|    119147 | 11668 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11669 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11670 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11671 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11672 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11673 | `				return SXERR_ABORT;` |
|         - | 11674 | `			}` |
|       ! 0 | 11675 | `			goto done;` |
|         - | 11676 | `		}` |
|    119147 | 11677 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    119147 | 11678 | `		iAttrflags = 0;` |
|    119147 | 11679 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    119147 | 11680 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    119147 | 11681 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11682 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11683 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11684 | `				for(;;){` |
|         - | 11685 | `					ph7_class *pUsedTrait;` |
|         - | 11686 | `					SyString *pUsedName;` |
|         5 | 11687 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11688 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11689 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11690 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11691 | `							return SXERR_ABORT;` |
|         - | 11692 | `						}` |
|       ! 0 | 11693 | `						break;` |
|         - | 11694 | `					}` |
|         5 | 11695 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11696 | `					{` |
|         - | 11697 | `						SyBlob sResolved;` |
|         5 | 11698 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11699 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11700 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11701 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11702 | `						SyBlobRelease(&sResolved);` |
|         - | 11703 | `					}` |
|         5 | 11704 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11705 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11706 | `					}` |
|         5 | 11707 | `					if( pUsedTrait == 0 ){` |
|         4 | 11708 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11709 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11710 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11711 | `							return SXERR_ABORT;` |
|         - | 11712 | `						}` |
|         2 | 11713 | `					}else{` |
|         3 | 11714 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11715 | `					}` |
|         5 | 11716 | `					pGen->pIn++;` |
|         5 | 11717 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11718 | `						break;` |
|         - | 11719 | `					}` |
|       ! 0 | 11720 | `					pGen->pIn++;` |
|       ! 0 | 11721 | `				}` |
|         5 | 11722 | `				continue;` |
|         - | 11723 | `			}` |
|    119143 | 11724 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    119127 | 11725 | `				iProtection = nKwrd;` |
|    119127 | 11726 | `				pGen->pIn++;` |
|    119122 | 11727 | `				if( pGen->pIn >= pGen->pEnd` |
|    119127 | 11728 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11729 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11730 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11731 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11732 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11733 | `						return SXERR_ABORT;` |
|         - | 11734 | `					}` |
|       ! 0 | 11735 | `					goto done;` |
|         - | 11736 | `				}` |
|    119127 | 11737 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     19217 | 11738 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     19217 | 11739 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11740 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11741 | `							return SXERR_ABORT;` |
|         - | 11742 | `						}` |
|       ! 0 | 11743 | `						goto done;` |
|         - | 11744 | `					}` |
|     19217 | 11745 | `					continue;` |
|         - | 11746 | `				}` |
|     99915 | 11747 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 11748 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 11749 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11750 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11751 | `							return SXERR_ABORT;` |
|         - | 11752 | `						}` |
|       ! 0 | 11753 | `						goto done;` |
|         - | 11754 | `					}` |
|         5 | 11755 | `					continue;` |
|         - | 11756 | `				}` |
|     99911 | 11757 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     49953 | 11758 | `			}` |
|     99927 | 11759 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 11760 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11761 | `					"Traits cannot have constants");` |
|       ! 0 | 11762 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11763 | `					return SXERR_ABORT;` |
|         - | 11764 | `				}` |
|       ! 0 | 11765 | `				goto done;` |
|       ! 0 | 11766 | `			}else{` |
|     99927 | 11767 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7695 | 11768 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7695 | 11769 | `					pGen->pIn++;` |
|      7695 | 11770 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7693 | 11771 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7693 | 11772 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11773 | `							iProtection = nKwrd;` |
|       ! 0 | 11774 | `							pGen->pIn++;` |
|       ! 0 | 11775 | `						}` |
|      3844 | 11776 | `					}` |
|      7690 | 11777 | `					if( pGen->pIn >= pGen->pEnd` |
|      7695 | 11778 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11779 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11780 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 11781 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11782 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11783 | `							return SXERR_ABORT;` |
|         - | 11784 | `						}` |
|       ! 0 | 11785 | `						goto done;` |
|         - | 11786 | `					}` |
|      7695 | 11787 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 11788 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 11789 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11790 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11791 | `								return SXERR_ABORT;` |
|         - | 11792 | `							}` |
|       ! 0 | 11793 | `							goto done;` |
|         - | 11794 | `						}` |
|         3 | 11795 | `						continue;` |
|         - | 11796 | `					}` |
|      7693 | 11797 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 11798 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11799 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11800 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11801 | `								return SXERR_ABORT;` |
|         - | 11802 | `							}` |
|       ! 0 | 11803 | `							goto done;` |
|         - | 11804 | `						}` |
|       ! 0 | 11805 | `						continue;` |
|         - | 11806 | `					}` |
|      7693 | 11807 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     96081 | 11808 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 11809 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 11810 | `					pGen->pIn++;` |
|         6 | 11811 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 11812 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 11813 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 11814 | `							iProtection = nKwrd;` |
|         6 | 11815 | `							pGen->pIn++;` |
|         2 | 11816 | `						}` |
|         2 | 11817 | `					}` |
|         6 | 11818 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 11819 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11820 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11821 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 11822 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11823 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11824 | `							return SXERR_ABORT;` |
|         - | 11825 | `						}` |
|       ! 0 | 11826 | `						goto done;` |
|         - | 11827 | `					}` |
|         6 | 11828 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 11829 | `				}` |
|     99925 | 11830 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11831 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11832 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 11833 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11834 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11835 | `						return SXERR_ABORT;` |
|         - | 11836 | `					}` |
|       ! 0 | 11837 | `					goto done;` |
|         - | 11838 | `				}` |
|     99925 | 11839 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 11840 | `					pGen->pIn++;` |
|       ! 0 | 11841 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 11842 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11843 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11844 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11845 | `							return SXERR_ABORT;` |
|         - | 11846 | `						}` |
|       ! 0 | 11847 | `						goto done;` |
|         - | 11848 | `					}` |
|       ! 0 | 11849 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11850 | `				}else{` |
|     99925 | 11851 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11852 | `				}` |
|     99925 | 11853 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11854 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11855 | `						return SXERR_ABORT;` |
|         - | 11856 | `					}` |
|       ! 0 | 11857 | `					goto done;` |
|         - | 11858 | `				}` |
|         - | 11859 | `			}` |
|     49965 | 11860 | `		}else{` |
|       ! 0 | 11861 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11862 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11863 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11864 | `					return SXERR_ABORT;` |
|         - | 11865 | `				}` |
|       ! 0 | 11866 | `				goto done;` |
|         - | 11867 | `			}` |
|         - | 11868 | `		}` |
|         5 | 11869 | `	}` |
|         - | 11870 | `	/* Install the trait */` |
|      7763 | 11871 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7763 | 11872 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11873 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11874 | `		return SXERR_ABORT;` |
|         - | 11875 | `	}` |
|      3879 | 11876 | `done:` |
|         - | 11877 | `	/* Point beyond the trait body */` |
|      7763 | 11878 | `	pGen->pIn = &pEnd[1];` |
|      7763 | 11879 | `	pGen->pEnd = pTmp;` |
|      7763 | 11880 | `	return PH7_OK;` |
|      3884 | 11881 | `}` |
|         - | 11882 | `/*` |
|         - | 11883 | ` * Compile a user-defined class.` |
|         - | 11884 | ` *  According to the PHP language reference manual` |
|         - | 11885 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 11886 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 11887 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 11888 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 11889 | ` *   and functions (called "methods").` |
|         - | 11890 | ` */` |
|    316518 | 11891 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 11892 | `{` |
|         - | 11893 | `	sxi32 rc;` |
|    316523 | 11894 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    316523 | 11895 | `	return rc;` |
|         5 | 11896 | `}` |
|         - | 11897 | `/*` |
|         - | 11898 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 11899 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 11900 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 11901 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 11902 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 11903 | ` */` |
|  10802104 | 11904 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11905 | `{` |
|  10984935 | 11906 | `	return (pIn->nType & PH7_TK_ID)` |
|   5583878 | 11907 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    192553 | 11908 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  10984930 | 11909 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 11910 | `}` |
|         - | 11911 | `/*` |
|         - | 11912 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 11913 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 11914 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 11915 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 11916 | ` */` |
|      3868 | 11917 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 11918 | `{` |
|      3873 | 11919 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 11920 | `}` |
|         - | 11921 | `/*` |
|         - | 11922 | ` * Exception handling.` |
|         - | 11923 | ` *  According to the PHP language reference manual` |
|         - | 11924 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 11925 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 11926 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 11927 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 11928 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 11929 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 11930 | ` *    (or re-thrown) within a catch block.` |
|         - | 11931 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 11932 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 11933 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 11934 | ` *    been defined with set_exception_handler().` |
|         - | 11935 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 11936 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 11937 | ` */` |
|         - | 11938 | `/*` |
|         - | 11939 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 11940 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 11941 | ` * indicates failure.` |
|         - | 11942 | ` */` |
|    480412 | 11943 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 11944 | `{` |
|    480417 | 11945 | `	sxi32 rc = SXRET_OK;` |
|    480417 | 11946 | `	if( pRoot->pOp ){` |
|    480405 | 11947 | `		switch( pRoot->pOp->iOp ){` |
|    240200 | 11948 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 11949 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 11950 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 11951 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 11952 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 11953 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    480405 | 11954 | `			break;` |
|       ! 0 | 11955 | `		default:` |
|         - | 11956 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 11957 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 11958 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 11959 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11960 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 11961 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 11962 | `				rc = SXERR_INVALID;` |
|       ! 0 | 11963 | `			}` |
|       ! 0 | 11964 | `			break;` |
|         - | 11965 | `		}` |
|    240217 | 11966 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 11967 | `		/* Unexpected expression */` |
|       ! 0 | 11968 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11969 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 11970 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 11971 | `			rc = SXERR_INVALID;` |
|       ! 0 | 11972 | `		}` |
|       ! 0 | 11973 | `	}` |
|    480417 | 11974 | `	return rc;` |
|         5 | 11975 | `}` |
|         - | 11976 | `/*` |
|         - | 11977 | ` * Compile a 'throw' statement.` |
|         - | 11978 | ` * throw: This is how you trigger an exception.` |
|         - | 11979 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 11980 | ` */` |
|    480376 | 11981 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 11982 | `{` |
|    480381 | 11983 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11984 | `	GenBlock *pBlock;` |
|         - | 11985 | `	sxu32 nIdx;` |
|         - | 11986 | `	sxi32 rc;` |
|    480381 | 11987 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 11988 | `	/* Compile the expression */` |
|    480381 | 11989 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    480381 | 11990 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 11991 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 11992 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11993 | `			return SXERR_ABORT;` |
|         - | 11994 | `		}` |
|       ! 0 | 11995 | `		return SXRET_OK;` |
|         - | 11996 | `	}` |
|    480381 | 11997 | `	pBlock = pGen->pCurrent;` |
|         - | 11998 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   1897737 | 11999 | `	while(pBlock->pParent){` |
|   1897733 | 12000 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    480377 | 12001 | `			break;` |
|         - | 12002 | `		}` |
|         - | 12003 | `		/* Point to the parent block */` |
|   1417361 | 12004 | `		pBlock = pBlock->pParent;` |
|         5 | 12005 | `	}` |
|         - | 12006 | `	/* Emit the throw instruction */` |
|    480381 | 12007 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12008 | `	/* Emit the jump */` |
|    480381 | 12009 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    480381 | 12010 | `	return SXRET_OK;` |
|    240193 | 12011 | `}` |
|         - | 12012 | `/*` |
|         - | 12013 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12014 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12015 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12016 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12017 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12018 | ` */` |
|        36 | 12019 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12020 | `{` |
|        38 | 12021 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12022 | `	GenBlock *pBlock;` |
|         - | 12023 | `	sxu32 nIdx;` |
|         - | 12024 | `	sxi32 rc;` |
|        18 | 12025 | `	(void)iCompileFlag;` |
|        38 | 12026 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12027 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12028 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12029 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12030 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12031 | `			return SXERR_ABORT;` |
|         - | 12032 | `		}` |
|       ! 0 | 12033 | `		return SXRET_OK;` |
|         - | 12034 | `	}` |
|        38 | 12035 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12036 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12037 | `		return SXERR_ABORT;` |
|         - | 12038 | `	}` |
|        38 | 12039 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12040 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12041 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12042 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12043 | `			return SXERR_ABORT;` |
|         - | 12044 | `		}` |
|       ! 0 | 12045 | `		return SXRET_OK;` |
|         - | 12046 | `	}` |
|         - | 12047 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12048 | `	pBlock = pGen->pCurrent;` |
|        60 | 12049 | `	while( pBlock->pParent ){` |
|        49 | 12050 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12051 | `			break;` |
|         - | 12052 | `		}` |
|        23 | 12053 | `		pBlock = pBlock->pParent;` |
|         1 | 12054 | `	}` |
|        38 | 12055 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12056 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12057 | `	return SXRET_OK;` |
|        20 | 12058 | `}` |
|         - | 12059 | `/*` |
|         - | 12060 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12061 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12062 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12063 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12064 | ` * compile error propagated from the parser.` |
|         - | 12065 | ` */` |
|        54 | 12066 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12067 | `{` |
|         - | 12068 | `	SyString sClassName;` |
|         - | 12069 | `	SyToken *pToken;` |
|         - | 12070 | `	SyString *pName;` |
|         - | 12071 | `	char *zDup;` |
|         - | 12072 | `	sxi32 rc;` |
|        59 | 12073 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        59 | 12074 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        59 | 12075 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        59 | 12076 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        59 | 12077 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12078 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12079 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12080 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12081 | `		return SXERR_INVALID;` |
|         - | 12082 | `	}` |
|        59 | 12083 | `	pGen->pIn++; /* '(' */` |
|        27 | 12084 | `	for(;;){` |
|         - | 12085 | `		SyBlob sResolved;` |
|        59 | 12086 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        59 | 12087 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12088 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12089 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12090 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12091 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12092 | `			return SXERR_INVALID;` |
|         - | 12093 | `		}` |
|        86 | 12094 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        54 | 12095 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        59 | 12096 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        59 | 12097 | `		SyBlobRelease(&sResolved);` |
|        59 | 12098 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12099 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        59 | 12100 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        54 | 12101 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12102 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12103 | `			pGen->pIn++; continue;` |
|         - | 12104 | `		}` |
|        59 | 12105 | `		break;` |
|       ! 0 | 12106 | `	}` |
|        54 | 12107 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12108 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12109 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12110 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12111 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12112 | `		return SXERR_INVALID;` |
|         - | 12113 | `	}` |
|        59 | 12114 | `	pGen->pIn++; /* '$' */` |
|        59 | 12115 | `	pName = &pGen->pIn->sData;` |
|        59 | 12116 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12117 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12118 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12119 | `	pGen->pIn++;` |
|        59 | 12120 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12121 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12122 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12123 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12124 | `		return SXERR_INVALID;` |
|         - | 12125 | `	}` |
|        59 | 12126 | `	pGen->pIn++; /* ')' */` |
|        59 | 12127 | `	return SXRET_OK;` |
|        32 | 12128 | `}` |
|         - | 12129 | `/*` |
|         - | 12130 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12131 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12132 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12133 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12134 | ` * VmThrowException):` |
|         - | 12135 | ` *` |
|         - | 12136 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12137 | ` *    <try body>` |
|         - | 12138 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12139 | ` *    JMP  -> finally\|end` |
|         - | 12140 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12141 | ` *    <catch body>` |
|         - | 12142 | ` *    JMP  -> finally\|end` |
|         - | 12143 | ` *    ... more catches ...` |
|         - | 12144 | ` *  Lfin: <finally body>` |
|         - | 12145 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12146 | ` *  Lend:` |
|         - | 12147 | ` */` |
|        98 | 12148 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12149 | `{` |
|       103 | 12150 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12151 | `	GenBlock *pTry;` |
|         - | 12152 | `	VmInstr *pInstr;` |
|       103 | 12153 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12154 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12155 | `	sxi32 rc;` |
|       103 | 12156 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12157 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       103 | 12158 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       103 | 12159 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       103 | 12160 | `	pTry->pUserData = pException;` |
|       103 | 12161 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       103 | 12162 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       103 | 12163 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       103 | 12164 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       103 | 12165 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       103 | 12166 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12167 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       103 | 12168 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       103 | 12169 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       103 | 12170 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       103 | 12171 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12172 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       103 | 12173 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12174 | `	/* Catch clauses (inline) */` |
|       103 | 12175 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        98 | 12176 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        59 | 12177 | `		sxu32 k = 0;` |
|        81 | 12178 | `		for(;;){` |
|         - | 12179 | `			ph7_exception_block sCatch;` |
|         - | 12180 | `			GenBlock *pCatchBlk;` |
|       113 | 12181 | `			sxu32 idxJmp = 0;` |
|       108 | 12182 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       104 | 12183 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        32 | 12184 | `				break;` |
|         - | 12185 | `			}` |
|        59 | 12186 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        59 | 12187 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12188 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        59 | 12189 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        59 | 12190 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        59 | 12191 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        59 | 12192 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12193 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12194 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12195 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        59 | 12196 | `			pCatchBlk->pUserData = pException;` |
|        59 | 12197 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        59 | 12198 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12199 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        59 | 12200 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12201 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12202 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        59 | 12203 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        59 | 12204 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        59 | 12205 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        59 | 12206 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        59 | 12207 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        59 | 12208 | `			k++;` |
|         5 | 12209 | `		}` |
|        27 | 12210 | `	}` |
|         - | 12211 | `	/* Finally (inline) */` |
|       103 | 12212 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12213 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12214 | `		GenBlock *pFinBlk;` |
|        52 | 12215 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12216 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12217 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12218 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12219 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12220 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12221 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12222 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12223 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12224 | `		pException->iHasFinally = 1;` |
|        24 | 12225 | `	}` |
|       103 | 12226 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       103 | 12227 | `	pException->iInlined = 1;` |
|         - | 12228 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12229 | `	{` |
|       103 | 12230 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12231 | `		sxu32 *aJ; sxu32 n;` |
|       103 | 12232 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       103 | 12233 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       103 | 12234 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       157 | 12235 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        59 | 12236 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        59 | 12237 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        32 | 12238 | `		}` |
|         - | 12239 | `	}` |
|       103 | 12240 | `	SySetRelease(&aCatchJmp);` |
|       103 | 12241 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12242 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12243 | `	}` |
|       103 | 12244 | `	return SXRET_OK;` |
|        54 | 12245 | `}` |
|         - | 12246 | `/*` |
|         - | 12247 | ` * Compile a 'catch' block.` |
|         - | 12248 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12249 | ` * an object containing the exception information.` |
|         - | 12250 | ` */` |
|     24518 | 12251 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12252 | `{` |
|     24523 | 12253 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12254 | `	ph7_exception_block sCatch;` |
|         - | 12255 | `	SySet *pInstrContainer;` |
|         - | 12256 | `	SyString sClassName;` |
|         - | 12257 | `	GenBlock *pCatch;` |
|         - | 12258 | `	SyToken *pToken;` |
|         - | 12259 | `	SyString *pName;` |
|         - | 12260 | `	char *zDup;` |
|         - | 12261 | `	sxi32 rc;` |
|     24523 | 12262 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12263 | `	/* Zero the structure */` |
|     24523 | 12264 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12265 | `	/* Initialize fields */` |
|     24523 | 12266 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24523 | 12267 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24523 | 12268 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12269 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12270 | `			pToken = pGen->pIn;` |
|       ! 0 | 12271 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12272 | `				pToken--;` |
|       ! 0 | 12273 | `			}` |
|       ! 0 | 12274 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12275 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12276 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12277 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12278 | `				return SXERR_ABORT;` |
|         - | 12279 | `			}` |
|       ! 0 | 12280 | `			return SXERR_INVALID;` |
|         - | 12281 | `	}` |
|         - | 12282 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24523 | 12283 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12273 | 12284 | `	for(;;){` |
|         - | 12285 | `		SyBlob sResolved;` |
|     24551 | 12286 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24551 | 12287 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12288 | `			SyBlobRelease(&sResolved);` |
|         6 | 12289 | `			pToken = pGen->pIn;` |
|         6 | 12290 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12291 | `				pToken--;` |
|       ! 0 | 12292 | `			}` |
|         8 | 12293 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12294 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12295 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12296 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12297 | `				return SXERR_ABORT;` |
|         - | 12298 | `			}` |
|         6 | 12299 | `			return SXERR_INVALID;` |
|         - | 12300 | `		}` |
|         - | 12301 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12302 | `		 * transient SyBlob allocation. */` |
|     36818 | 12303 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24542 | 12304 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24547 | 12305 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24547 | 12306 | `		SyBlobRelease(&sResolved);` |
|     24547 | 12307 | `		if( zDup == 0 ){` |
|       ! 0 | 12308 | `			goto Mem;` |
|         - | 12309 | `		}` |
|     24547 | 12310 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24547 | 12311 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12312 | `			goto Mem;` |
|         - | 12313 | `		}` |
|         - | 12314 | `		/* Check for '\|' (multi-catch separator) */` |
|     24542 | 12315 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24542 | 12316 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        33 | 12317 | `			pGen->pIn->sData.nByte == 1 &&` |
|        28 | 12318 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        30 | 12319 | `			pGen->pIn++; /* Consume the '\|' */` |
|        30 | 12320 | `			continue;` |
|         - | 12321 | `		}` |
|     24519 | 12322 | `		break;` |
|       ! 0 | 12323 | `	}` |
|     24514 | 12324 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24519 | 12325 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12326 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12327 | `			pToken = pGen->pIn;` |
|       ! 0 | 12328 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12329 | `				pToken--;` |
|       ! 0 | 12330 | `			}` |
|       ! 0 | 12331 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12332 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12333 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12334 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12335 | `				return SXERR_ABORT;` |
|         - | 12336 | `			}` |
|       ! 0 | 12337 | `			return SXERR_INVALID;` |
|         - | 12338 | `	}` |
|     24519 | 12339 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12340 | `	/* Duplicate instance name */` |
|     24519 | 12341 | `	pName = &pGen->pIn->sData;` |
|     24519 | 12342 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24519 | 12343 | `	if( zDup == 0 ){` |
|       ! 0 | 12344 | `		goto Mem;` |
|         - | 12345 | `	}` |
|     24519 | 12346 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24519 | 12347 | `	pGen->pIn++;` |
|     24519 | 12348 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12349 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12350 | `		pToken = pGen->pIn;` |
|       ! 0 | 12351 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12352 | `			pToken--;` |
|       ! 0 | 12353 | `		}` |
|       ! 0 | 12354 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12355 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12356 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12357 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12358 | `			return SXERR_ABORT;` |
|         - | 12359 | `		}` |
|       ! 0 | 12360 | `		return SXERR_INVALID;` |
|         - | 12361 | `	}` |
|         - | 12362 | `	/* Compile the block */` |
|     24519 | 12363 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12364 | `	/* Create the catch block */` |
|     24519 | 12365 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24519 | 12366 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12367 | `		return SXERR_ABORT;` |
|         - | 12368 | `	}` |
|         - | 12369 | `	/* Swap bytecode container */` |
|     24519 | 12370 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24519 | 12371 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12372 | `	/* Compile the block */` |
|     24519 | 12373 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12374 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24519 | 12375 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12376 | `	/* Emit the DONE instruction */` |
|     24519 | 12377 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12378 | `	/* Leave the block */` |
|     24519 | 12379 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12380 | `	/* Restore the default container */` |
|     24519 | 12381 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12382 | `	/* Install the catch block */` |
|     24519 | 12383 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24519 | 12384 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12385 | `		goto Mem;` |
|         - | 12386 | `	}` |
|     24519 | 12387 | `	return SXRET_OK;` |
|       ! 0 | 12388 | `Mem:` |
|       ! 0 | 12389 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12390 | `	return SXERR_ABORT;` |
|     12264 | 12391 | `}` |
|         - | 12392 | `/*` |
|         - | 12393 | ` * Compile a 'try' block.` |
|         - | 12394 | ` * A function using an exception should be in a "try" block.` |
|         - | 12395 | ` * If the exception does not trigger, the code will continue` |
|         - | 12396 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12397 | ` * is "thrown".` |
|         - | 12398 | ` */` |
|     24674 | 12399 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12400 | `{` |
|         - | 12401 | `	ph7_exception *pException;` |
|     24679 | 12402 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12403 | `	GenBlock *pTry;` |
|         - | 12404 | `	sxu32 nJmpIdx;` |
|         - | 12405 | `	sxi32 rc;` |
|         - | 12406 | `	/* Create the exception container */` |
|     24679 | 12407 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24679 | 12408 | `	if( pException == 0 ){` |
|       ! 0 | 12409 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12410 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12411 | `		return SXERR_ABORT;` |
|         - | 12412 | `	}` |
|         - | 12413 | `	/* Zero the structure */` |
|     24679 | 12414 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12415 | `	/* Initialize fields */` |
|     24679 | 12416 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24679 | 12417 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24679 | 12418 | `	pException->iHasFinally = 0;` |
|     24679 | 12419 | `	pException->iFinallyDone = 0;` |
|     24679 | 12420 | `	pException->pVm = pGen->pVm;` |
|         - | 12421 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12422 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12423 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12424 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12425 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12426 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24679 | 12427 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       103 | 12428 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12429 | `	}` |
|         - | 12430 | `	/* Create the try block */` |
|     24581 | 12431 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24581 | 12432 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12433 | `		return SXERR_ABORT;` |
|         - | 12434 | `	}` |
|         - | 12435 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24581 | 12436 | `	pTry->pUserData = pException;` |
|         - | 12437 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24581 | 12438 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12439 | `	/* Fix the jump later when the destination is resolved */` |
|     24581 | 12440 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24581 | 12441 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12442 | `	/* Compile the block */` |
|     24581 | 12443 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24581 | 12444 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12445 | `		return SXERR_ABORT;` |
|         - | 12446 | `	}` |
|         - | 12447 | `	/* Fix forward jumps now the destination is resolved */` |
|     24581 | 12448 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12449 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24581 | 12450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12451 | `	/* Leave the block */` |
|     24581 | 12452 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12453 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24581 | 12454 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24574 | 12455 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12456 | `		/* Compile one or more catch blocks */` |
|     24514 | 12457 | `		for(;;){` |
|     49028 | 12458 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36833 | 12459 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12260 | 12460 | `					break;` |
|         - | 12461 | `			}` |
|     24523 | 12462 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24523 | 12463 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12464 | `				return SXERR_ABORT;` |
|         - | 12465 | `			}` |
|         5 | 12466 | `		}` |
|     12255 | 12467 | `	}` |
|         - | 12468 | `	/* Compile optional finally block */` |
|     24581 | 12469 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       728 | 12470 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12471 | `		SySet *pInstrContainer;` |
|         - | 12472 | `		GenBlock *pFinBlock;` |
|       129 | 12473 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12474 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12475 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12476 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12477 | `			return SXERR_ABORT;` |
|         - | 12478 | `		}` |
|         - | 12479 | `		/* Swap bytecode container */` |
|       129 | 12480 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12481 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12482 | `		/* Compile the finally body */` |
|       129 | 12483 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12484 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12485 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12486 | `			return SXERR_ABORT;` |
|         - | 12487 | `		}` |
|         - | 12488 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12489 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12490 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12491 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12492 | `		/* Leave the block */` |
|       129 | 12493 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12494 | `		/* Restore the default container */` |
|       129 | 12495 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12496 | `		pException->iHasFinally = 1;` |
|        62 | 12497 | `	}` |
|         - | 12498 | `	/* Must have at least one catch or finally */` |
|     24581 | 12499 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         9 | 12500 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12501 | `			"Cannot use try without catch or finally");` |
|         9 | 12502 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12503 | `			return SXERR_ABORT;` |
|         - | 12504 | `		}` |
|         3 | 12505 | `	}` |
|     24581 | 12506 | `	return SXRET_OK;` |
|     12342 | 12507 | `}` |
|         - | 12508 | `/*` |
|         - | 12509 | ` * Compile a switch block.` |
|         - | 12510 | ` *  (See block-comment below for more information)` |
|         - | 12511 | ` */` |
|       112 | 12512 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12513 | `{` |
|       117 | 12514 | `	sxi32 rc = SXRET_OK;` |
|       117 | 12515 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12516 | `		/* Unexpected token */` |
|       ! 0 | 12517 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12518 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12519 | `			return SXERR_ABORT;` |
|         - | 12520 | `		}` |
|       ! 0 | 12521 | `		pGen->pIn++;` |
|       ! 0 | 12522 | `	}` |
|       117 | 12523 | `	pGen->pIn++;` |
|         - | 12524 | `	/* First instruction to execute in this block. */` |
|       117 | 12525 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12526 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12527 | `	 * or the '}' token */` |
|       206 | 12528 | `	for(;;){` |
|       417 | 12529 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12530 | `			/* No more input to process */` |
|       ! 0 | 12531 | `			break;` |
|         - | 12532 | `		}` |
|       417 | 12533 | `		rc = SXRET_OK;` |
|       417 | 12534 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        85 | 12535 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|        31 | 12536 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12537 | `					/* Unexpected token */` |
|       ! 0 | 12538 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12539 | `						&pGen->pIn->sData);` |
|       ! 0 | 12540 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12541 | `						return SXERR_ABORT;` |
|         - | 12542 | `					}` |
|         - | 12543 | `					/* FALL THROUGH */` |
|       ! 0 | 12544 | `				}` |
|        31 | 12545 | `				rc = SXERR_EOF;` |
|        31 | 12546 | `				break;` |
|         - | 12547 | `			}` |
|        32 | 12548 | `		}else{` |
|         - | 12549 | `			sxi32 nKwrd;` |
|         - | 12550 | `			/* Extract the keyword */` |
|       337 | 12551 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       337 | 12552 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|        47 | 12553 | `				break;` |
|         - | 12554 | `			}` |
|       253 | 12555 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12556 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12557 | `					/* Unexpected token */` |
|       ! 0 | 12558 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12559 | `						&pGen->pIn->sData);` |
|       ! 0 | 12560 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12561 | `						return SXERR_ABORT;` |
|         - | 12562 | `					}` |
|         - | 12563 | `					/* FALL THROUGH */` |
|       ! 0 | 12564 | `				}` |
|         - | 12565 | `				/* Block compiled */` |
|         3 | 12566 | `				break;` |
|         - | 12567 | `			}` |
|         - | 12568 | `		}` |
|         - | 12569 | `		/* Compile block */` |
|       305 | 12570 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       305 | 12571 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12572 | `			return SXERR_ABORT;` |
|         - | 12573 | `		}` |
|         5 | 12574 | `	}` |
|       117 | 12575 | `	return rc;` |
|        61 | 12576 | `}` |
|         - | 12577 | `/*` |
|         - | 12578 | ` * Compile a case eXpression.` |
|         - | 12579 | ` *  (See block-comment below for more information)` |
|         - | 12580 | ` */` |
|        92 | 12581 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12582 | `{` |
|         - | 12583 | `	SySet *pInstrContainer;` |
|         - | 12584 | `	SyToken *pEnd,*pTmp;` |
|        97 | 12585 | `	sxi32 iNest = 0;` |
|         - | 12586 | `	sxi32 rc;` |
|         - | 12587 | `	/* Delimit the expression */` |
|        97 | 12588 | `	pEnd = pGen->pIn;` |
|       197 | 12589 | `	while( pEnd < pGen->pEnd ){` |
|       197 | 12590 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12591 | `			/* Increment nesting level */` |
|         3 | 12592 | `			iNest++;` |
|       196 | 12593 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12594 | `			/* Decrement nesting level */` |
|         3 | 12595 | `			iNest--;` |
|       194 | 12596 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|        97 | 12597 | `			break;` |
|         - | 12598 | `		}` |
|       105 | 12599 | `		pEnd++;` |
|         5 | 12600 | `	}` |
|        97 | 12601 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12602 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12603 | `		if( rc == SXERR_ABORT ){` |
|         - | 12604 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12605 | `			return SXERR_ABORT;` |
|         - | 12606 | `		}` |
|       ! 0 | 12607 | `	}` |
|         - | 12608 | `	/* Swap token stream */` |
|        97 | 12609 | `	pTmp = pGen->pEnd;` |
|        97 | 12610 | `	pGen->pEnd = pEnd;` |
|        97 | 12611 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        97 | 12612 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|        97 | 12613 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12614 | `	/* Emit the done instruction */` |
|        97 | 12615 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        97 | 12616 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12617 | `	/* Update token stream */` |
|        97 | 12618 | `	pGen->pIn  = pEnd;` |
|        97 | 12619 | `	pGen->pEnd = pTmp;` |
|        97 | 12620 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12621 | `		return SXERR_ABORT;` |
|         - | 12622 | `	}` |
|        97 | 12623 | `	return SXRET_OK;` |
|        51 | 12624 | `}` |
|         - | 12625 | `/*` |
|         - | 12626 | ` * Compile the smart switch statement.` |
|         - | 12627 | ` * According to the PHP language reference manual` |
|         - | 12628 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12629 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12630 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12631 | ` *  This is exactly what the switch statement is for.` |
|         - | 12632 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12633 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12634 | ` *  of the outer loop, use continue 2.` |
|         - | 12635 | ` *  Note that switch/case does loose comparision.` |
|         - | 12636 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12637 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12638 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12639 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12640 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12641 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12642 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12643 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12644 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12645 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12646 | ` *  list for the next case.` |
|         - | 12647 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12648 | ` *  or floating-point numbers and strings.` |
|         - | 12649 | ` */` |
|        28 | 12650 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12651 | `{` |
|         - | 12652 | `	GenBlock *pSwitchBlock;` |
|         - | 12653 | `	SyToken *pTmp,*pEnd;` |
|         - | 12654 | `	ph7_switch *pSwitch;` |
|         - | 12655 | `	sxu32 nToken;` |
|         - | 12656 | `	sxu32 nLine;` |
|         - | 12657 | `	sxi32 rc;` |
|        33 | 12658 | `	nLine = pGen->pIn->nLine;` |
|         - | 12659 | `	/* Jump the 'switch' keyword */` |
|        33 | 12660 | `	pGen->pIn++;` |
|        33 | 12661 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12662 | `		/* Syntax error */` |
|       ! 0 | 12663 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12664 | `		if( rc == SXERR_ABORT ){` |
|         - | 12665 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12666 | `			return SXERR_ABORT;` |
|         - | 12667 | `		}` |
|       ! 0 | 12668 | `		goto Synchronize;` |
|         - | 12669 | `	}` |
|         - | 12670 | `	/* Jump the left parenthesis '(' */` |
|        33 | 12671 | `	pGen->pIn++;` |
|        33 | 12672 | `	pEnd = 0; /* cc warning */` |
|         - | 12673 | `	/* Create the loop block */` |
|        47 | 12674 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|        14 | 12675 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|        33 | 12676 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12677 | `		return SXERR_ABORT;` |
|         - | 12678 | `	}` |
|         - | 12679 | `	/* Delimit the condition */` |
|        33 | 12680 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|        33 | 12681 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12682 | `		/* Empty expression */` |
|       ! 0 | 12683 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12684 | `		if( rc == SXERR_ABORT ){` |
|         - | 12685 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12686 | `			return SXERR_ABORT;` |
|         - | 12687 | `		}` |
|       ! 0 | 12688 | `	}` |
|         - | 12689 | `	/* Swap token streams */` |
|        33 | 12690 | `	pTmp = pGen->pEnd;` |
|        33 | 12691 | `	pGen->pEnd = pEnd;` |
|         - | 12692 | `	/* Compile the expression */` |
|        33 | 12693 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        33 | 12694 | `	if( rc == SXERR_ABORT ){` |
|         - | 12695 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12696 | `		return SXERR_ABORT;` |
|         - | 12697 | `	}` |
|         - | 12698 | `	/* Update token stream */` |
|        33 | 12699 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12700 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12701 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12702 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12703 | `			return SXERR_ABORT;` |
|         - | 12704 | `		}` |
|       ! 0 | 12705 | `		pGen->pIn++;` |
|       ! 0 | 12706 | `	}` |
|        33 | 12707 | `	pGen->pIn  = &pEnd[1];` |
|        33 | 12708 | `	pGen->pEnd = pTmp;` |
|        33 | 12709 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        28 | 12710 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12711 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12712 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12713 | `				pTmp--;` |
|       ! 0 | 12714 | `			}` |
|         - | 12715 | `			/* Unexpected token */` |
|       ! 0 | 12716 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12717 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12718 | `				return SXERR_ABORT;` |
|         - | 12719 | `			}` |
|       ! 0 | 12720 | `			goto Synchronize;` |
|         - | 12721 | `	}` |
|         - | 12722 | `	/* Set the delimiter token */` |
|        33 | 12723 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12724 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12725 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12726 | `	}else{` |
|        31 | 12727 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12728 | `	}` |
|        33 | 12729 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12730 | `	/* Create the switch blocks container */` |
|        33 | 12731 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|        33 | 12732 | `	if( pSwitch == 0 ){` |
|         - | 12733 | `		/* Abort compilation */` |
|       ! 0 | 12734 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12735 | `		return SXERR_ABORT;` |
|         - | 12736 | `	}` |
|         - | 12737 | `	/* Zero the structure */` |
|        33 | 12738 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 12739 | `	/* Initialize fields */` |
|        33 | 12740 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 12741 | `	/* Emit the switch instruction */` |
|        33 | 12742 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 12743 | `	/* Compile case blocks */` |
|       100 | 12744 | `	for(;;){` |
|         - | 12745 | `		sxu32 nKwrd;` |
|       119 | 12746 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12747 | `			/* No more input to process */` |
|       ! 0 | 12748 | `			break;` |
|         - | 12749 | `		}` |
|       119 | 12750 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 12751 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 12752 | `				/* Unexpected token */` |
|       ! 0 | 12753 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12754 | `					&pGen->pIn->sData);` |
|       ! 0 | 12755 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12756 | `					return SXERR_ABORT;` |
|         - | 12757 | `				}` |
|         - | 12758 | `				/* FALL THROUGH */` |
|       ! 0 | 12759 | `			}` |
|         - | 12760 | `			/* Block compiled */` |
|       ! 0 | 12761 | `			break;` |
|         - | 12762 | `		}` |
|         - | 12763 | `		/* Extract the keyword */` |
|       119 | 12764 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       119 | 12765 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12766 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 12767 | `				/* Unexpected token */` |
|       ! 0 | 12768 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12769 | `					&pGen->pIn->sData);` |
|       ! 0 | 12770 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12771 | `					return SXERR_ABORT;` |
|         - | 12772 | `				}` |
|         - | 12773 | `				/* FALL THROUGH */` |
|       ! 0 | 12774 | `			}` |
|         - | 12775 | `			/* Block compiled */` |
|         3 | 12776 | `			break;` |
|         - | 12777 | `		}` |
|       117 | 12778 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 12779 | `			/*` |
|         - | 12780 | `			 * Accroding to the PHP language reference manual` |
|         - | 12781 | `			 *  A special case is the default case. This case matches anything` |
|         - | 12782 | `			 *  that wasn't matched by the other cases.` |
|         - | 12783 | `			 */` |
|        25 | 12784 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 12785 | `				/* Default case already compiled */` |
|       ! 0 | 12786 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 12787 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12788 | `					return SXERR_ABORT;` |
|         - | 12789 | `				}` |
|       ! 0 | 12790 | `			}` |
|        25 | 12791 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 12792 | `			/* Compile the default block */` |
|        25 | 12793 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 12794 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12795 | `				return SXERR_ABORT;` |
|        25 | 12796 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 12797 | `				break;` |
|         1 | 12798 | `			}` |
|        98 | 12799 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 12800 | `			ph7_case_expr sCase;` |
|         - | 12801 | `			/* Standard case block */` |
|        97 | 12802 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 12803 | `			/* initialize the structure */` |
|        97 | 12804 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 12805 | `			/* Compile the case expression */` |
|        97 | 12806 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|        97 | 12807 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12808 | `				return SXERR_ABORT;` |
|         - | 12809 | `			}` |
|         - | 12810 | `			/* Compile the case block */` |
|        97 | 12811 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 12812 | `			/* Insert in the switch container */` |
|        97 | 12813 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|        97 | 12814 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12815 | `				return SXERR_ABORT;` |
|        97 | 12816 | `			}else if( rc == SXERR_EOF ){` |
|         9 | 12817 | `				break;` |
|         - | 12818 | `			}` |
|        47 | 12819 | `		}else{` |
|         - | 12820 | `			/* Unexpected token */` |
|       ! 0 | 12821 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12822 | `				&pGen->pIn->sData);` |
|       ! 0 | 12823 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12824 | `				return SXERR_ABORT;` |
|         - | 12825 | `			}` |
|       ! 0 | 12826 | `			break;` |
|         - | 12827 | `		}` |
|         5 | 12828 | `	}` |
|         - | 12829 | `	/* Fix all jumps now the destination is resolved */` |
|        33 | 12830 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|        33 | 12831 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12832 | `	/* Release the loop block */` |
|        33 | 12833 | `	GenStateLeaveBlock(pGen,0);` |
|        33 | 12834 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 12835 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|        33 | 12836 | `		pGen->pIn++;` |
|        14 | 12837 | `	}` |
|         - | 12838 | `	/* Statement successfully compiled */` |
|        33 | 12839 | `	return SXRET_OK;` |
|       ! 0 | 12840 | `Synchronize:` |
|         - | 12841 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 12842 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 12843 | `		pGen->pIn++;` |
|       ! 0 | 12844 | `	}` |
|       ! 0 | 12845 | `	return SXRET_OK;` |
|        19 | 12846 | `}` |
|         - | 12847 | `/*` |
|         - | 12848 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 12849 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 12850 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 12851 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 12852 | ` */` |
|         - | 12853 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 12854 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 12855 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 12856 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 12857 |  |
|         - | 12858 | `/*` |
|         - | 12859 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 12860 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 12861 | ` * patched entries from the pending set.` |
|         - | 12862 | ` */` |
|  40094880 | 12863 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 12864 | `{` |
|  40094885 | 12865 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 12866 | `	sxu32 nTarget;` |
|         - | 12867 | `	sxu32 *aIdx;` |
|         - | 12868 | `	sxu32 i;` |
|  40094885 | 12869 | `	if( nCur <= nBaseline ){` |
|  40094789 | 12870 | `		return;` |
|         - | 12871 | `	}` |
|       100 | 12872 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 12873 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 12874 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 12875 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 12876 | `		if( pInstr ){` |
|       108 | 12877 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 12878 | `		}` |
|        56 | 12879 | `	}` |
|       100 | 12880 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  20047445 | 12881 | `}` |
|         - | 12882 |  |
|         - | 12883 | `/*` |
|         - | 12884 | ` * By-reference out-parameters of builtin functions.` |
|         - | 12885 | ` *` |
|         - | 12886 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 12887 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 12888 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 12889 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 12890 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 12891 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 12892 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 12893 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 12894 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 12895 | ` * creates it" behaviour).` |
|         - | 12896 | ` *` |
|         - | 12897 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 12898 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 12899 | ` */` |
|   5447066 | 12900 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 12901 | `{` |
|         - | 12902 | `	static const struct {` |
|         - | 12903 | `		const char *zName;` |
|         - | 12904 | `		sxu32 nByte;` |
|         - | 12905 | `		sxu32 mask;` |
|         - | 12906 | `	} aByRef[] = {` |
|         - | 12907 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12908 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12909 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12910 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12911 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 12912 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 12913 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 12914 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 12915 | `	};` |
|         - | 12916 | `	sxu32 i;` |
|   5447071 | 12917 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1605881 | 12918 | `		return 0;` |
|         - | 12919 | `	}` |
|  34274245 | 12920 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  30471580 | 12921 | `		if( pName->nByte == aByRef[i].nByte` |
|  15922962 | 12922 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     38535 | 12923 | `			return aByRef[i].mask;` |
|         - | 12924 | `		}` |
|  15216530 | 12925 | `	}` |
|   3802665 | 12926 | `	return 0;` |
|   2723538 | 12927 | `}` |
|         - | 12928 | `/*` |
|         - | 12929 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 12930 | ` *` |
|         - | 12931 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 12932 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 12933 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 12934 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 12935 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 12936 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 12937 | ` */` |
|   5447066 | 12938 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 12939 | `{` |
|         - | 12940 | `	SyToken *p, *pEnd;` |
|   5447071 | 12941 | `	pOut->zString = 0;` |
|   5447071 | 12942 | `	pOut->nByte = 0;` |
|   5447071 | 12943 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 12944 | `		return;` |
|         - | 12945 | `	}` |
|   5447071 | 12946 | `	p = pLeft->pStart;` |
|   5447071 | 12947 | `	pEnd = pLeft->pEnd;` |
|         - | 12948 | `	/* Optional single leading namespace separator (absolute path). */` |
|   5447071 | 12949 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3873 | 12950 | `		p++;` |
|      1934 | 12951 | `	}` |
|   5447071 | 12952 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1605845 | 12953 | `		return;` |
|         - | 12954 | `	}` |
|         - | 12955 | `	/* Must be a single component: nothing follows the name token. */` |
|   3841231 | 12956 | `	if( p + 1 != pEnd ){` |
|        40 | 12957 | `		return;` |
|         - | 12958 | `	}` |
|   3841195 | 12959 | `	*pOut = p->sData;` |
|   2723538 | 12960 | `}` |
|         - | 12961 | `/*` |
|         - | 12962 | ` * Generate bytecode for a given expression tree.` |
|         - | 12963 | ` * If something goes wrong while generating bytecode` |
|         - | 12964 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 12965 | ` * this function takes care of generating the appropriate` |
|         - | 12966 | ` * error message.` |
|         - | 12967 | ` */` |
|  57924124 | 12968 | `static sxi32 GenStateEmitExprCode(` |
|         - | 12969 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 12970 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 12971 | `	sxi32 iFlags /* Control flags */` |
|         - | 12972 | `	)` |
|         5 | 12973 | `{` |
|         - | 12974 | `	VmInstr *pInstr;` |
|         - | 12975 | `	sxu32 nJmpIdx;` |
|  57924129 | 12976 | `	sxi32 iP1 = 0;` |
|  57924129 | 12977 | `	sxu32 iP2 = 0;` |
|  57924129 | 12978 | `	void *p3  = 0;` |
|         - | 12979 | `	sxi32 iVmOp;` |
|         - | 12980 | `	sxi32 rc;` |
|  57924129 | 12981 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  57924129 | 12982 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  57924129 | 12983 | `	sxu32 nRhsNsBase = 0;` |
|  57924129 | 12984 | `	if( pNode->xCode ){` |
|         - | 12985 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 12986 | `		/* Compile node */` |
|  34645587 | 12987 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  34645587 | 12988 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  34645587 | 12989 | `		RE_SWAP_DELIMITER(pGen);` |
|  34645587 | 12990 | `		return rc;` |
|         - | 12991 | `	}` |
|  23278547 | 12992 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 12993 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 12994 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 12995 | `		return SXERR_ABORT;` |
|         - | 12996 | `	}` |
|  23278547 | 12997 | `	iVmOp = pNode->pOp->iVmOp;` |
|  23278547 | 12998 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 12999 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13000 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13001 | `		 * and later errors are still reported. */` |
|         3 | 13002 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13003 | `			"The (unset) cast is no longer supported");` |
|         3 | 13004 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13005 | `			return SXERR_ABORT;` |
|         - | 13006 | `		}` |
|         1 | 13007 | `	}` |
|  23278547 | 13008 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        91 | 13009 | `		sxu32 nJmp = 0;` |
|         - | 13010 | `		sxu32 nNcNsBase;` |
|         - | 13011 | `		VmInstr *pInstrFix;` |
|         - | 13012 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13013 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13014 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13015 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13016 | `		 * stack slot carries a writable nIdx. */` |
|        91 | 13017 | `		if( pNode->pRight ){` |
|        91 | 13018 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13019 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        91 | 13020 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13021 | `				return rc;` |
|         - | 13022 | `			}` |
|        91 | 13023 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13024 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13025 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13026 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13027 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13028 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13029 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13030 | `			 * cascade for the actual write path stays correct. */` |
|        91 | 13031 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        91 | 13032 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13033 | `				pInstrFix->iP2 = 3;` |
|        15 | 13034 | `			}` |
|        44 | 13035 | `		}` |
|         - | 13036 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        91 | 13037 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13038 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        91 | 13039 | `		if( pNode->pLeft ){` |
|        91 | 13040 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13041 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        91 | 13042 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13043 | `				return rc;` |
|         - | 13044 | `			}` |
|        91 | 13045 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        44 | 13046 | `		}` |
|         - | 13047 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        91 | 13048 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13049 | `		/* Patch the short-circuit jump to land after the store. */` |
|        91 | 13050 | `		if( nJmp > 0 ){` |
|        91 | 13051 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        91 | 13052 | `			if( pInstrFix ){` |
|        91 | 13053 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        44 | 13054 | `			}` |
|        44 | 13055 | `		}` |
|        91 | 13056 | `		return SXRET_OK;` |
|         - | 13057 | `	}` |
|  23278459 | 13058 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13059 | `		sxu32 nJz,nJmp;` |
|         - | 13060 | `		sxu32 nTernaryNsBase;` |
|         - | 13061 | `		/* Ternary operator require special handling */` |
|         - | 13062 | `		/* Phase#1: Compile the condition */` |
|    371935 | 13063 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    371935 | 13064 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    371935 | 13065 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13066 | `			return rc;` |
|         - | 13067 | `		}` |
|         - | 13068 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13069 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13070 | `		 * condition expression, not leak past the ternary. */` |
|    371935 | 13071 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    371935 | 13072 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    371935 | 13073 | `		if( pNode->pLeft ){` |
|         - | 13074 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13075 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    368027 | 13076 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13077 | `			/* Phase#3: Compile the 'then' expression  */` |
|    368027 | 13078 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    368027 | 13079 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    368027 | 13080 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13081 | `				return rc;` |
|         - | 13082 | `			}` |
|    368027 | 13083 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    184016 | 13084 | `		}else{` |
|         - | 13085 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13086 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13087 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3913 | 13088 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3913 | 13089 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13090 | `		}` |
|         - | 13091 | `		/* Phase#4: Emit the unconditional jump */` |
|    371935 | 13092 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13093 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    371935 | 13094 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    371935 | 13095 | `		if( pInstr ){` |
|    371935 | 13096 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    185965 | 13097 | `		}` |
|    371935 | 13098 | `		if( !pNode->pLeft ){` |
|         - | 13099 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3913 | 13100 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1954 | 13101 | `		}` |
|         - | 13102 | `		/* Phase#6: Compile the 'else' expression */` |
|    371935 | 13103 | `		if( pNode->pRight ){` |
|    371935 | 13104 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    371935 | 13105 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    371935 | 13106 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13107 | `				return rc;` |
|         - | 13108 | `			}` |
|    371935 | 13109 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    185965 | 13110 | `		}` |
|    371935 | 13111 | `		if( nJmp > 0 ){` |
|         - | 13112 | `			/* Phase#7: Fix the unconditional jump */` |
|    371935 | 13113 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    371935 | 13114 | `			if( pInstr ){` |
|    371935 | 13115 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    185965 | 13116 | `			}` |
|    185965 | 13117 | `		}` |
|         - | 13118 | `		/* All done */` |
|    371935 | 13119 | `		return SXRET_OK;` |
|         - | 13120 | `	}` |
|  22906529 | 13121 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13122 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13123 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13124 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13125 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13126 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13127 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13128 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13129 | `		sxu32 nPipeNsBase;` |
|        27 | 13130 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13131 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13132 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13133 | `				"'\|>': Missing operand");` |
|       ! 0 | 13134 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13135 | `		}` |
|         - | 13136 | `		/* Argument: the LHS value. */` |
|        27 | 13137 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13138 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13139 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13140 | `			return rc;` |
|         - | 13141 | `		}` |
|        27 | 13142 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13143 | `		/* Callable: the RHS. */` |
|        27 | 13144 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13145 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13146 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13147 | `			return rc;` |
|         - | 13148 | `		}` |
|        27 | 13149 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13150 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13151 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13152 | `		return SXRET_OK;` |
|         - | 13153 | `	}` |
|  22906503 | 13154 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13155 | `	/* Generate code for the left tree */` |
|  22906503 | 13156 | `	if( pNode->pLeft ){` |
|  22883461 | 13157 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  22883461 | 13158 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13159 | `			ph7_expr_node **apNode;` |
|   5451227 | 13160 | `			int hasSpread = 0;` |
|   5451227 | 13161 | `			int hasNamed = 0;` |
|   5451227 | 13162 | `			int bAnySpread = 0;` |
|   5451227 | 13163 | `			sxu32 byRefMask = 0;` |
|         - | 13164 | `			sxi32 nArgs;` |
|         - | 13165 | `			sxi32 n;` |
|         - | 13166 | `			/* Recurse and generate bytecodes for function arguments */` |
|   5451227 | 13167 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5451227 | 13168 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13169 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13170 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13171 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   5451227 | 13172 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13173 | `				bFcc = 1;` |
|        81 | 13174 | `				nArgs = 0;` |
|        40 | 13175 | `			}` |
|         - | 13176 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13177 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13178 | `			{` |
|   5451227 | 13179 | `				int seenNamed = 0;` |
|   5451227 | 13180 | `				int seenSpread = 0;` |
|  11131267 | 13181 | `				for( n = 0; n < nArgs; ++n ){` |
|   5680047 | 13182 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4029 | 13183 | `						bAnySpread = 1;` |
|      4029 | 13184 | `						seenSpread = 1;` |
|      4029 | 13185 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13186 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13187 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13188 | `							return SXERR_SYNTAX;` |
|         5 | 13189 | `						}` |
|   5678035 | 13190 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13191 | `						seenNamed = 1;` |
|       289 | 13192 | `						hasNamed = 1;` |
|   5675881 | 13193 | `					}else if( seenNamed ){` |
|         3 | 13194 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13195 | `							"Cannot use positional argument after named argument");` |
|         3 | 13196 | `						return SXERR_SYNTAX;` |
|   5675737 | 13197 | `					}else if( seenSpread ){` |
|       ! 0 | 13198 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13199 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13200 | `						return SXERR_SYNTAX;` |
|         - | 13201 | `					}` |
|   2840025 | 13202 | `				}` |
|         - | 13203 | `			}` |
|         - | 13204 | `			/* Read-only load */` |
|   5451225 | 13205 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13206 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13207 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13208 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13209 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   5451225 | 13210 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   5451225 | 13211 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   5451220 | 13212 | `				if( pCallName->nByte == 5` |
|   3064867 | 13213 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    276865 | 13214 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5312795 | 13215 | `				}else if( pCallName->nByte == 5` |
|   2788007 | 13216 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       107 | 13217 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        51 | 13218 | `				}` |
|         - | 13219 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13220 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13221 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13222 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13223 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13224 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   5451225 | 13225 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13226 | `					SyString sBuiltin;` |
|   5447071 | 13227 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   5447071 | 13228 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   2723533 | 13229 | `				}` |
|   2725610 | 13230 | `			}` |
|  11131263 | 13231 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   5680043 | 13232 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   5680043 | 13233 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13234 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13235 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13236 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13237 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13238 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13239 | `				 * (iP1=0 either way). */` |
|   5680043 | 13240 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     26953 | 13241 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     26953 | 13242 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     13474 | 13243 | `				}` |
|   5680043 | 13244 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   5680043 | 13245 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13246 | `					return rc;` |
|         - | 13247 | `				}` |
|         - | 13248 | `				/* Each argument is an independent nullsafe scope. */` |
|   5680043 | 13249 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   5680043 | 13250 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13251 | `					/* Emit spread opcode to unpack this array argument */` |
|      4029 | 13252 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4029 | 13253 | `					hasSpread = 1;` |
|      2012 | 13254 | `				}` |
|   2840024 | 13255 | `			}` |
|         - | 13256 | `			/* Total number of given arguments */` |
|   5451225 | 13257 | `			iP1 = nArgs;` |
|   5451225 | 13258 | `			iP2 = hasSpread;` |
|         - | 13259 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13260 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   5451225 | 13261 | `			if( hasNamed ){` |
|       178 | 13262 | `				sxu32 nStrBytes = 0;` |
|         - | 13263 | `				char *zBuf;` |
|       534 | 13264 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13265 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13266 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13267 | `					}` |
|       182 | 13268 | `				}` |
|         - | 13269 | `				{` |
|       178 | 13270 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13271 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13272 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13273 | `				if( pMap ){` |
|       178 | 13274 | `					SyZero(pMap, mapSize);` |
|       178 | 13275 | `					pMap->bHasNamed = 1;` |
|       178 | 13276 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13277 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13278 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13279 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13280 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13281 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13282 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13283 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13284 | `							zBuf += nb;` |
|       141 | 13285 | `						}` |
|         - | 13286 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13287 | `					}` |
|       178 | 13288 | `					p3 = (void *)pMap;` |
|        87 | 13289 | `				}` |
|         - | 13290 | `				}` |
|        87 | 13291 | `			}` |
|         - | 13292 | `			/* Remove stale flags now */` |
|   5451225 | 13293 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   2725610 | 13294 | `		}` |
|         - | 13295 | `		{` |
|         - | 13296 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13297 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13298 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13299 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13300 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13301 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13302 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13303 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  22883459 | 13304 | `			sxi32 iLeftFlags = iFlags;` |
|  22883454 | 13305 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  18827070 | 13306 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   7385369 | 13307 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   6265843 | 13308 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2412353 | 13309 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1206174 | 13310 | `			}` |
|         - | 13311 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13312 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13313 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13314 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13315 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13316 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13317 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  22883454 | 13318 | `			if( pNode->pOp` |
|  32048852 | 13319 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  20607172 | 13320 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  18330838 | 13321 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4914515 | 13322 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2457255 | 13323 | `			}` |
|         - | 13324 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13325 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13326 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13327 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13328 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13329 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  22883454 | 13330 | `			if( pNode->pOp` |
|  22883459 | 13331 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    146417 | 13332 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     73206 | 13333 | `			}` |
|  22883459 | 13334 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13335 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13336 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|       211 | 13337 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|       103 | 13338 | `			}` |
|  22883459 | 13339 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13340 | `		}` |
|  22883459 | 13341 | `		if( rc != SXRET_OK ){` |
|        34 | 13342 | `			return rc;` |
|         - | 13343 | `		}` |
|  22883429 | 13344 | `		if( !bIsChainOp ){` |
|         - | 13345 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13346 | `			 * target the end of that LHS chain, which is right here. */` |
|   9991709 | 13347 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   4995852 | 13348 | `		}` |
|  22883429 | 13349 | `		if( iVmOp == PH7_OP_CALL ){` |
|   5451225 | 13350 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5451225 | 13351 | `			if( pInstr ){` |
|   5451225 | 13352 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   3841471 | 13353 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13354 | `					sxu32 nQual;` |
|   3841471 | 13355 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13356 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13357 | `					 * so the later NEW handler (if any) can see it. */` |
|   3841471 | 13358 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13359 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13360 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13361 | `					 * imports — class imports must NOT affect function` |
|         - | 13362 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13363 | `					 * before NEW; we store the original literal index in the` |
|         - | 13364 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13365 | `					 * the unqualified name and re-qualify with class imports. */` |
|   3841471 | 13366 | `					if( bAbsolute ){` |
|      3873 | 13367 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1939 | 13368 | `					}else{` |
|   3837603 | 13369 | `						int fromImport = 0;` |
|   3837603 | 13370 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   3837603 | 13371 | `						pInstr->iP2 = (sxi32)nQual;` |
|   3837603 | 13372 | `						if( nQual != nOrig ){` |
|         - | 13373 | `							/* Record the original literal index in the arg map` |
|         - | 13374 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13375 | `							 * flag) so the NEW handler can recover the` |
|         - | 13376 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13377 | `							 * imports. */` |
|        77 | 13378 | `							if( p3 == 0 ){` |
|        77 | 13379 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        72 | 13380 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        77 | 13381 | `								if( pMap ){` |
|        77 | 13382 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        77 | 13383 | `									p3 = (void *)pMap;` |
|        36 | 13384 | `								}` |
|        36 | 13385 | `							}` |
|        77 | 13386 | `							if( p3 ){` |
|        77 | 13387 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        77 | 13388 | `								if( !fromImport ){` |
|         - | 13389 | `									/* Mark as namespace-qualified */` |
|        67 | 13390 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        31 | 13391 | `								}` |
|        36 | 13392 | `							}` |
|        36 | 13393 | `						}` |
|         5 | 13394 | `					}` |
|   3530492 | 13395 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13396 | `					/* Method call,flag that */` |
|   1589947 | 13397 | `					pInstr->iP2 = 1;` |
|    794971 | 13398 | `				}` |
|   2725615 | 13399 | `			}` |
|  20157819 | 13400 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13401 | `			ph7_expr_node **apNode;` |
|         - | 13402 | `			sxi32 n;` |
|   2525995 | 13403 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13404 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13405 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13406 | `			/* Recurse and generate bytecodes for array index */` |
|   2525995 | 13407 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   4894329 | 13408 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2368339 | 13409 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2368339 | 13410 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2368339 | 13411 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13412 | `					return rc;` |
|         - | 13413 | `				}` |
|         - | 13414 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2368339 | 13415 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1184172 | 13416 | `			}` |
|   2525995 | 13417 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2368339 | 13418 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1184167 | 13419 | `			}` |
|   2525995 | 13420 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13421 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    315149 | 13422 | `				iP2 = 4;` |
|   2368423 | 13423 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13424 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13425 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23121 | 13426 | `				iP2 = 5;` |
|   2199293 | 13427 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13428 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13429 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13430 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 13431 | `				iP2 = 6;` |
|   2187723 | 13432 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13433 | `				/* Create an empty entry when the desired index is not found */` |
|    369381 | 13434 | `				iP2 = 1;` |
|    184693 | 13435 | `			}` |
|  16169214 | 13436 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13437 | `			/* POP the left node */` |
|         5 | 13438 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13439 | `		}` |
|  11441712 | 13440 | `	}` |
|  22906471 | 13441 | `	rc = SXRET_OK;` |
|  22906471 | 13442 | `	nJmpIdx = 0;` |
|         - | 13443 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13444 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13445 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  22906471 | 13446 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    384889 | 13447 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    384889 | 13448 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    384889 | 13449 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    384889 | 13450 | `			int isSpecial = 0;` |
|    384889 | 13451 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    338777 | 13452 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    338777 | 13453 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    338772 | 13454 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    307992 | 13455 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    167438 | 13456 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    103829 | 13457 | `					isSpecial = 1;` |
|     51912 | 13458 | `				}` |
|    180914 | 13459 | `			}` |
|    407945 | 13460 | `			pInstr->iP1 = 0;` |
|    407945 | 13461 | `			if( !isSpecial ){` |
|    258009 | 13462 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    129002 | 13463 | `			}` |
|         - | 13464 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13465 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    361833 | 13466 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    258009 | 13467 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    258009 | 13468 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        60 | 13469 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        62 | 13470 | `					return SXRET_OK;` |
|         - | 13471 | `				}` |
|    128973 | 13472 | `			}` |
|    180885 | 13473 | `		}` |
|    226976 | 13474 | `	}` |
|         - | 13475 | `	/* Generate code for the right tree */` |
|  22883371 | 13476 | `	if( pNode->pRight ){` |
|  13153651 | 13477 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13478 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    315401 | 13479 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  12995953 | 13480 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13481 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    215215 | 13482 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  12730650 | 13483 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13484 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     53915 | 13485 | `			iVmOp = 0; /* No binary operator to emit */` |
|     53915 | 13486 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  12596142 | 13487 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13488 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13489 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13490 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13491 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13492 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13493 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13494 | `			sxu32 nNsJmp = 0;` |
|       108 | 13495 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13496 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  12569083 | 13497 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13498 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13499 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13500 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   3993713 | 13501 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   1996854 | 13502 | `		}` |
|  13153651 | 13503 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  13153651 | 13504 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  13153651 | 13505 | `		if( !bIsChainOp ){` |
|         - | 13506 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13507 | `			 * operator instruction is emitted. */` |
|   8239199 | 13508 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   4119597 | 13509 | `		}` |
|  13153651 | 13510 | `		if( iVmOp == PH7_OP_STORE ){` |
|   3578667 | 13511 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   3578630 | 13512 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13513 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13514 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13515 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13516 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13517 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13518 | `				 */` |
|        91 | 13519 | `				iVmOp = 0;` |
|   3578624 | 13520 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   3578581 | 13521 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13522 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    772551 | 13523 | `					iP2 = 1;` |
|    386278 | 13524 | `				}else{` |
|   2806035 | 13525 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13526 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    350083 | 13527 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    350083 | 13528 | `						iP1 = pInstr->iP1;` |
|    175044 | 13529 | `					}else{` |
|   2455957 | 13530 | `						p3 = pInstr->p3;` |
|         - | 13531 | `					}` |
|         - | 13532 | `					/* POP the last dynamic load instruction */` |
|   2806035 | 13533 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13534 | `				}` |
|   1789293 | 13535 | `			}` |
|  11364320 | 13536 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        62 | 13537 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        62 | 13538 | `			if( pInstr ){` |
|        62 | 13539 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13540 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13541 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13542 | `					 */` |
|        19 | 13543 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13544 | `					iP1 = pInstr->iP1;` |
|        19 | 13545 | `					iP2 = pInstr->iP2;` |
|        19 | 13546 | `					p3  = pInstr->p3;` |
|        10 | 13547 | `				}else{` |
|        44 | 13548 | `					p3 = pInstr->p3;` |
|         - | 13549 | `				}` |
|        30 | 13550 | `			}` |
|        30 | 13551 | `		}` |
|   6576823 | 13552 | `	}` |
|  22883366 | 13553 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    358731 | 13554 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13555 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13556 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13557 | `		iVmOp = 0;` |
|        14 | 13558 | `	}` |
|  22883371 | 13559 | `	if( iVmOp > 0 ){` |
|  22829343 | 13560 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    146417 | 13561 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13562 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     11557 | 13563 | `				iP1 = 1;` |
|      5781 | 13564 | `			}` |
|  22756137 | 13565 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13566 | `			/* Namespace-qualify the class name for NEW */ {` |
|    717105 | 13567 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    717105 | 13568 | `				VmInstr *pCallInstr = 0;` |
|    717105 | 13569 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    716809 | 13570 | `					pCallInstr = pPeek;` |
|    716809 | 13571 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    358402 | 13572 | `				}` |
|    717105 | 13573 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    701741 | 13574 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13575 | `					sxu32 nLitForClass;` |
|    701741 | 13576 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13577 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13578 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13579 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13580 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13581 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13582 | `					 * with class imports. */` |
|    701741 | 13583 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        37 | 13584 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        21 | 13585 | `					}else{` |
|    701709 | 13586 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13587 | `					}` |
|    701741 | 13588 | `					pPeek->iP1 = 0;` |
|    701741 | 13589 | `					if( !bAbsolute ){` |
|    697877 | 13590 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    348941 | 13591 | `					}else{` |
|      3869 | 13592 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13593 | `					}` |
|    350868 | 13594 | `				}` |
|         - | 13595 | `			}` |
|    717105 | 13596 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    717105 | 13597 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13598 | `				VmInstr *pPrev;` |
|    716809 | 13599 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    716809 | 13600 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13601 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13602 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13603 | `					 * accumulator exactly like OP_CALL would have). */` |
|    716809 | 13604 | `					iP1 = pInstr->iP1;` |
|    716809 | 13605 | `					iP2 = pInstr->iP2;` |
|    716809 | 13606 | `					if( pInstr->p3 ){` |
|        47 | 13607 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        21 | 13608 | `					}` |
|    716809 | 13609 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    358402 | 13610 | `				}` |
|    358407 | 13611 | `			}` |
|  22324381 | 13612 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13613 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13614 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     69377 | 13615 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     69377 | 13616 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     69377 | 13617 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     69377 | 13618 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     69377 | 13619 | `				int isSpecialIs = 0;` |
|     69377 | 13620 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     69377 | 13621 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     69377 | 13622 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     69372 | 13623 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     69375 | 13624 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     34686 | 13625 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13626 | `						isSpecialIs = 1;` |
|         5 | 13627 | `					}` |
|     34686 | 13628 | `				}` |
|     69377 | 13629 | `				pInstr->iP1 = 0;` |
|     69377 | 13630 | `				if( !isSpecialIs && !bAbsolute ){` |
|     69357 | 13631 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     34676 | 13632 | `				}` |
|     34691 | 13633 | `			}` |
|  21931145 | 13634 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13635 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13636 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13637 | `			 * should not trigger constant lookup. */` |
|   4914457 | 13638 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4914457 | 13639 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4680139 | 13640 | `				pInstr->iP1 = 0;` |
|   2340067 | 13641 | `			}` |
|   4914457 | 13642 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13643 | `				/* Static member access,remember that */` |
|    361789 | 13644 | `				iP1 = 1;` |
|    361789 | 13645 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    361789 | 13646 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    230467 | 13647 | `					p3 = pInstr->p3;` |
|    230467 | 13648 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    115231 | 13649 | `				}` |
|    180892 | 13650 | `			}` |
|         - | 13651 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13652 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13653 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13654 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4914457 | 13655 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4914457 | 13656 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13657 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4914437 | 13658 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     61543 | 13659 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4883648 | 13660 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13661 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4852871 | 13662 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13663 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    941695 | 13664 | `					iP2 = PH7_MEMBER_WRITE;` |
|    470845 | 13665 | `				}` |
|   2457226 | 13666 | `			}` |
|   2457226 | 13667 | `		}` |
|         - | 13668 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13669 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13670 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13671 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13672 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  22829343 | 13673 | `		if( bFcc ){` |
|        81 | 13674 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13675 | `			iP2 = 0;` |
|        81 | 13676 | `			p3 = 0;` |
|        81 | 13677 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13678 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13679 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13680 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13681 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13682 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13683 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13684 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13685 | `				if( pMemberName ){` |
|         3 | 13686 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13687 | `				}` |
|        37 | 13688 | `				iP1 = 2;` |
|        19 | 13689 | `			}else{` |
|        45 | 13690 | `				iP1 = 1;` |
|         - | 13691 | `			}` |
|        40 | 13692 | `		}` |
|         - | 13693 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13694 | `		 * This is the primary emit path for user-visible calls. */` |
|  22829343 | 13695 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6168245 | 13696 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3084120 | 13697 | `		}` |
|         - | 13698 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  22829343 | 13699 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  11414669 | 13700 | `	}` |
|  22883371 | 13701 | `	if( nJmpIdx > 0 ){` |
|         - | 13702 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    584521 | 13703 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    584521 | 13704 | `		if( pInstr ){` |
|    584521 | 13705 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    292258 | 13706 | `		}` |
|    292258 | 13707 | `	}` |
|  22883371 | 13708 | `	return rc;` |
|  28950546 | 13709 | `}` |
|         - | 13710 | `/*` |
|         - | 13711 | ` * Compile a PHP expression.` |
|         - | 13712 | ` * According to the PHP language reference manual:` |
|         - | 13713 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 13714 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 13715 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 13716 | ` *  is "anything that has a value".` |
|         - | 13717 | ` * If something goes wrong while compiling the expression,this` |
|         - | 13718 | ` * function takes care of generating the appropriate error` |
|         - | 13719 | ` * message.` |
|         - | 13720 | ` */` |
|         - | 13721 | `/*` |
|         - | 13722 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 13723 | ` *` |
|         - | 13724 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 13725 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 13726 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 13727 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 13728 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 13729 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 13730 | ` * except for() now reports php's parse error.` |
|         - | 13731 | ` */` |
| 192051770 | 13732 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 13733 | `{` |
|         - | 13734 | `	ph7_expr_node **apArg;` |
|         - | 13735 | `	sxu32 n;` |
| 192051775 | 13736 | `	if( pNode == 0 ){` |
| 134869769 | 13737 | `		return 0;` |
|         - | 13738 | `	}` |
|  57182011 | 13739 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 13740 | `		return 1;` |
|         - | 13741 | `	}` |
|  57182002 | 13742 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  57182003 | 13743 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 13744 | `		return 1;` |
|         - | 13745 | `	}` |
|  57182003 | 13746 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  65215095 | 13747 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   8033097 | 13748 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 13749 | `			return 1;` |
|         - | 13750 | `		}` |
|   4016551 | 13751 | `	}` |
|  57182003 | 13752 | `	return 0;` |
|  96025890 | 13753 | `}` |
|  12726736 | 13754 | `static sxi32 PH7_CompileExpr(` |
|         - | 13755 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 13756 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 13757 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 13758 | `	)` |
|         5 | 13759 | `{` |
|         - | 13760 | `	ph7_expr_node *pRoot;` |
|         - | 13761 | `	SySet sExprNode;` |
|         - | 13762 | `	SyToken *pEnd;` |
|         - | 13763 | `	sxi32 nExpr;` |
|         - | 13764 | `	sxi32 iNest;` |
|         - | 13765 | `	sxi32 rc;` |
|         - | 13766 | `	sxu32 nNullsafeBase;` |
|         - | 13767 | `	/* Initialize worker variables */` |
|  12726741 | 13768 | `	nExpr = 0;` |
|  12726741 | 13769 | `	pRoot = 0;` |
|         - | 13770 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 13771 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  12726741 | 13772 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  12726741 | 13773 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  12726741 | 13774 | `	SySetAlloc(&sExprNode,0x10);` |
|  12726741 | 13775 | `	rc = SXRET_OK;` |
|         - | 13776 | `	/* Delimit the expression */` |
|  12726741 | 13777 | `	pEnd = pGen->pIn;` |
|  12726741 | 13778 | `	iNest = 0;` |
| 101075549 | 13779 | `	while( pEnd < pGen->pEnd ){` |
|  96414515 | 13780 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 13781 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4559 | 13782 | `			iNest++;` |
|  96412238 | 13783 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4567 | 13784 | `			iNest--;` |
|  96407680 | 13785 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   8066323 | 13786 | `			if( iNest <= 0 ){` |
|   8065707 | 13787 | `				break;` |
|         - | 13788 | `			}` |
|       308 | 13789 | `		}` |
|  88348813 | 13790 | `		pEnd++;` |
|         5 | 13791 | `	}` |
|  12726741 | 13792 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    642289 | 13793 | `		SyToken *pEnd2 = pGen->pIn;` |
|    642289 | 13794 | `		iNest = 0;` |
|         - | 13795 | `		/* Stop at the first comma */` |
|   1400501 | 13796 | `		while( pEnd2 < pEnd ){` |
|    758219 | 13797 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     42349 | 13798 | `				iNest++;` |
|    737047 | 13799 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     42349 | 13800 | `				iNest--;` |
|    694703 | 13801 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|        63 | 13802 | `				if( iNest <= 0 ){` |
|         3 | 13803 | `					break;` |
|         - | 13804 | `				}` |
|        28 | 13805 | `			}` |
|    758217 | 13806 | `			pEnd2++;` |
|         5 | 13807 | `		}` |
|    642289 | 13808 | `		if( pEnd2 <pEnd ){` |
|         3 | 13809 | `			pEnd = pEnd2;` |
|         1 | 13810 | `		}` |
|    321142 | 13811 | `	}` |
|  12726741 | 13812 | `	if( pEnd > pGen->pIn ){` |
|  12703691 | 13813 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 13814 | `		/* Swap delimiter */` |
|  12703691 | 13815 | `		pGen->pEnd = pEnd;` |
|         - | 13816 | `		/* Try to get an expression tree */` |
|  12703691 | 13817 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  12703686 | 13818 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  12588097 | 13819 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 13820 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 13821 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 13822 | `				"syntax error, unexpected token \",\"");` |
|         6 | 13823 | `			pGen->pEnd = pTmp;` |
|         6 | 13824 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13825 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 13826 | `				return SXERR_ABORT;` |
|         - | 13827 | `			}` |
|         6 | 13828 | `			pGen->pIn = pEnd;` |
|         6 | 13829 | `			SySetRelease(&sExprNode);` |
|         6 | 13830 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 13831 | `			return SXRET_OK;` |
|         - | 13832 | `		}` |
|  12703687 | 13833 | `		if( rc == SXRET_OK && pRoot ){` |
|  12703505 | 13834 | `			rc = SXRET_OK;` |
|  12703505 | 13835 | `			if( xTreeValidator ){` |
|         - | 13836 | `				/* Call the upper layer validator callback */` |
|    834461 | 13837 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    417228 | 13838 | `			}` |
|  12703505 | 13839 | `			if( rc != SXERR_ABORT ){` |
|         - | 13840 | `				/* Generate code for the given tree */` |
|  12703505 | 13841 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 13842 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 13843 | `				 * expression so they short-circuit to its end. */` |
|  12703505 | 13844 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   6351750 | 13845 | `			}` |
|  12703505 | 13846 | `			nExpr = 1;` |
|   6351750 | 13847 | `		}` |
|         - | 13848 | `		/* Release the whole tree */` |
|  12703687 | 13849 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 13850 | `		/* Synchronize token stream */` |
|  12703687 | 13851 | `		pGen->pEnd = pTmp;` |
|  12703687 | 13852 | `		pGen->pIn  = pEnd;` |
|  12703687 | 13853 | `		if( rc == SXERR_ABORT ){` |
|        13 | 13854 | `			SySetRelease(&sExprNode);` |
|        13 | 13855 | `			return SXERR_ABORT;` |
|         - | 13856 | `		}` |
|   6351836 | 13857 | `	}` |
|  12726727 | 13858 | `	SySetRelease(&sExprNode);` |
|  12726727 | 13859 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   6363373 | 13860 | `}` |
|         - | 13861 | `/*` |
|         - | 13862 | ` * Return a pointer to the node construct handler associated` |
|         - | 13863 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 13864 | ` */` |
|   7140594 | 13865 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 13866 | `{` |
|   7140599 | 13867 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 13868 | `		/* Numeric literal: Either real or integer */` |
|   2727037 | 13869 | `		return PH7_CompileNumLiteral;` |
|   4413567 | 13870 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 13871 | `		/* Double quoted string */` |
|     80777 | 13872 | `		return PH7_CompileString;` |
|   4332795 | 13873 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 13874 | `		/* Single quoted string */` |
|   4332675 | 13875 | `		return PH7_CompileSimpleString;` |
|       125 | 13876 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 13877 | `		/* Heredoc */` |
|        70 | 13878 | `		return PH7_CompileHereDoc;` |
|        59 | 13879 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 13880 | `		/* Nowdoc */` |
|        51 | 13881 | `		return PH7_CompileNowDoc;` |
|         9 | 13882 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 13883 | `		/* Backtick quoted string */` |
|         6 | 13884 | `		return PH7_CompileBacktic;` |
|         - | 13885 | `	}` |
|         3 | 13886 | `	return 0;` |
|   3570302 | 13887 | `}` |
|         - | 13888 | `/*` |
|         - | 13889 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 13890 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 13891 | ` * in write context" parse error.` |
|         - | 13892 | ` */` |
|     30250 | 13893 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 13894 | `{` |
|         - | 13895 | `	sxi32 rc;` |
|     30255 | 13896 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     30253 | 13897 | `		return SXRET_OK;` |
|         - | 13898 | `	}` |
|         5 | 13899 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 13900 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 13901 | `		"Can't use nullsafe operator in write context");` |
|         3 | 13902 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     15130 | 13903 | `}` |
|         - | 13904 | `/*` |
|         - | 13905 | ` * Compile an unset() statement.` |
|         - | 13906 | ` * unset($var, $arr[$key], ...);` |
|         - | 13907 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 13908 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 13909 | ` * parent array before extracting the element to unset.` |
|         - | 13910 | ` */` |
|     26024 | 13911 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 13912 | `{` |
|     26029 | 13913 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26029 | 13914 | `	sxu32 nIdx = 0;` |
|         - | 13915 | `	SyString sName;` |
|         - | 13916 | `	sxi32 rc;` |
|         - | 13917 | `	/* Jump the 'unset' keyword */` |
|     26029 | 13918 | `	pGen->pIn++;` |
|         - | 13919 | `	/* Save delimiter */` |
|     26029 | 13920 | `	pTmp = pGen->pEnd;` |
|         - | 13921 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26029 | 13922 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26029 | 13923 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 13924 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 13925 | `		SyToken *pClose;` |
|     26029 | 13926 | `		pGen->pIn++;   /* Skip '(' */` |
|     26029 | 13927 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26029 | 13928 | `		pEnd = pClose; /* Stop at ')' */` |
|     13012 | 13929 | `	}` |
|     26029 | 13930 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 13931 | `	/* Resolve the 'unset' builtin name once */` |
|     26029 | 13932 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3845 | 13933 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3845 | 13934 | `		if( pObj == 0 ){` |
|       ! 0 | 13935 | `			return SXERR_ABORT;` |
|         - | 13936 | `		}` |
|      3845 | 13937 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3845 | 13938 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1920 | 13939 | `	}` |
|         - | 13940 | `	/* Compile each comma-separated argument */` |
|     56281 | 13941 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30257 | 13942 | `		if( pGen->pIn < pNext ){` |
|     30257 | 13943 | `			pGen->pEnd = pNext;` |
|     30257 | 13944 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 13945 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 13946 | `				GenStateUnsetValidator);` |
|     30257 | 13947 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13948 | `				return SXERR_ABORT;` |
|         - | 13949 | `			}` |
|     30257 | 13950 | `			if( rc != SXERR_EMPTY ){` |
|         - | 13951 | `				/* Emit call for this single argument */` |
|     30255 | 13952 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     30255 | 13953 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     30255 | 13954 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     15125 | 13955 | `			}` |
|     15126 | 13956 | `		}` |
|         - | 13957 | `		/* Jump trailing commas */` |
|     34487 | 13958 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|      4235 | 13959 | `			pNext++;` |
|         5 | 13960 | `		}` |
|     30257 | 13961 | `		pGen->pIn = pNext;` |
|         5 | 13962 | `	}` |
|         - | 13963 | `	/* Skip past the closing ')' if present */` |
|     26029 | 13964 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26029 | 13965 | `		pGen->pIn++;` |
|     13012 | 13966 | `	}` |
|         - | 13967 | `	/* Restore token stream */` |
|     26029 | 13968 | `	pGen->pEnd = pTmp;` |
|     26029 | 13969 | `	return SXRET_OK;` |
|     13017 | 13970 | `}` |
|         - | 13971 | `/*` |
|         - | 13972 | ` * PHP Language construct table.` |
|         - | 13973 | ` */` |
|         - | 13974 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 13975 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 13976 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 13977 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 13978 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 13979 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 13980 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 13981 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 13982 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 13983 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 13984 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 13985 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 13986 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 13987 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 13988 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 13989 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 13990 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 13991 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 13992 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 13993 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 13994 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 13995 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 13996 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 13997 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 13998 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 13999 | `};` |
|         - | 14000 | `/*` |
|         - | 14001 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14002 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14003 | ` */` |
|   6318678 | 14004 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14005 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14006 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14007 | `	)` |
|         5 | 14008 | `{` |
|   6318683 | 14009 | `	sxu32 n = 0;` |
|  26099696 | 14010 | `	for(;;){` |
|  52199397 | 14011 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    432393 | 14012 | `			break;` |
|         - | 14013 | `		}` |
|  51767009 | 14014 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   5886295 | 14015 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14016 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14017 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14018 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14019 | `					return 0;` |
|         - | 14020 | `				}` |
|       ! 0 | 14021 | `			}` |
|   5886290 | 14022 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|        14 | 14023 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|        14 | 14024 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14025 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14026 | `				return 0;` |
|         - | 14027 | `			}` |
|         - | 14028 | `			/* Return a pointer to the handler.` |
|         - | 14029 | `			*/` |
|   5886293 | 14030 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14031 | `		}` |
|  45880719 | 14032 | `		n++;` |
|         5 | 14033 | `	}` |
|    432393 | 14034 | `	if( pLookahed ){` |
|    432393 | 14035 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     69229 | 14036 | `			return PH7_CompileClassInterface;` |
|    363169 | 14037 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    316523 | 14038 | `			return PH7_CompileClass;` |
|     46651 | 14039 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7763 | 14040 | `			return PH7_CompileTrait;` |
|         - | 14041 | `		}` |
|         - | 14042 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14043 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14044 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14045 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19444 | 14046 | `	}` |
|         - | 14047 | `	/* Not a language construct */` |
|     38893 | 14048 | `	return 0;` |
|   3159344 | 14049 | `}` |
|         - | 14050 | `/*` |
|         - | 14051 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14052 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14053 | ` */` |
|     38890 | 14054 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14055 | `{` |
|         - | 14056 | `	int rc;` |
|     38895 | 14057 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38895 | 14058 | `	if( rc == FALSE ){` |
|     38774 | 14059 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15726 | 14060 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14061 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14062 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14063 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14064 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14065 | `			*/` |
|         - | 14066 | `			){` |
|     38771 | 14067 | `				rc = TRUE;` |
|     19383 | 14068 | `		}` |
|     19387 | 14069 | `	}` |
|     38895 | 14070 | `	return rc;` |
|         5 | 14071 | `}` |
|         - | 14072 | `/*` |
|         - | 14073 | ` * Compile a PHP chunk.` |
|         - | 14074 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14075 | ` * takes care of generating the appropriate error message.` |
|         - | 14076 | ` */` |
|         - | 14077 | `/*` |
|         - | 14078 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14079 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14080 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14081 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14082 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14083 | ` * intervening non-declaration statements.` |
|         - | 14084 | ` */` |
|  13999848 | 14085 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14086 | `{` |
|  13999853 | 14087 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  13999853 | 14088 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  13999853 | 14089 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14090 | `	sxu32 nIdx, n;` |
|  13999848 | 14091 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   1519709 | 14092 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14093 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14094 | `		 * indexes do not map to the sidecar */` |
|  12480151 | 14095 | `		return;` |
|         - | 14096 | `	}` |
|   1519707 | 14097 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14098 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14099 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   1519707 | 14100 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4560605 | 14101 | `	for( n = 0 ; n < nT ; n++ ){` |
|   3040903 | 14102 | `		if( aT[n].nTokIdx != nIdx ){` |
|   3033059 | 14103 | `			continue;` |
|         - | 14104 | `		}` |
|      7849 | 14105 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14106 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7837 | 14107 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7825 | 14108 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3910 | 14109 | `		}` |
|      3927 | 14110 | `	}` |
|   6999929 | 14111 | `}` |
|         - | 14112 | `/*` |
|         - | 14113 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14114 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14115 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14116 | ` */` |
|   3888912 | 14117 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14118 | `{` |
|         - | 14119 | `	char *zDup;` |
|   3888917 | 14120 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   3888897 | 14121 | `		return;` |
|         - | 14122 | `	}` |
|        35 | 14123 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14124 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14125 | `	if( zDup ){` |
|        25 | 14126 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14127 | `	}` |
|        25 | 14128 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   1944461 | 14129 | `}` |
|         - | 14130 | `/*` |
|         - | 14131 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14132 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14133 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14134 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14135 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14136 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14137 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14138 | ` */` |
|      7832 | 14139 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14140 | `{` |
|         - | 14141 | `	SySet *pToken;` |
|         - | 14142 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14143 | `	char *zSpan;` |
|      7837 | 14144 | `	sxi32 rc = SXRET_OK;` |
|      7837 | 14145 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14146 | `		return SXRET_OK;` |
|         - | 14147 | `	}` |
|     11753 | 14148 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3916 | 14149 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7837 | 14150 | `	if( zSpan == 0 ){` |
|       ! 0 | 14151 | `		return SXRET_OK;` |
|         - | 14152 | `	}` |
|         - | 14153 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14154 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14155 | `	 * the number of attribute declarations in the program. */` |
|      7837 | 14156 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7837 | 14157 | `	if( pToken == 0 ){` |
|       ! 0 | 14158 | `		return SXRET_OK;` |
|         - | 14159 | `	}` |
|      7837 | 14160 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7837 | 14161 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7837 | 14162 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7837 | 14163 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7837 | 14164 | `	pSavedIn = pGen->pIn;` |
|      7837 | 14165 | `	pSavedEnd = pGen->pEnd;` |
|      7841 | 14166 | `	while( pIn < pEnd ){` |
|         - | 14167 | `		ph7_attribute sAttr;` |
|         - | 14168 | `		SyBlob sFQN;` |
|      7841 | 14169 | `		int bAbsolute = 0;` |
|      7841 | 14170 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7841 | 14171 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7841 | 14172 | `		sAttr.nLine = pIn->nLine;` |
|      7841 | 14173 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14174 | `			bAbsolute = 1;` |
|        75 | 14175 | `			pIn++;` |
|        35 | 14176 | `		}` |
|      7841 | 14177 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7841 | 14178 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7841 | 14179 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7841 | 14180 | `			pIn++;` |
|      7841 | 14181 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14182 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14183 | `				pIn++;` |
|       ! 0 | 14184 | `				continue;` |
|         - | 14185 | `			}` |
|      7841 | 14186 | `			break;` |
|       ! 0 | 14187 | `		}` |
|      7841 | 14188 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14189 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14190 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14191 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14192 | `			break;` |
|         - | 14193 | `		}` |
|         - | 14194 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14195 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14196 | `		{` |
|      7841 | 14197 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7841 | 14198 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7841 | 14199 | `			char *zDup = 0;` |
|      7841 | 14200 | `			if( !bAbsolute ){` |
|      7771 | 14201 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7771 | 14202 | `				if( pImp ){` |
|       ! 0 | 14203 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14204 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14205 | `					if( zDup ){` |
|       ! 0 | 14206 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14207 | `					}` |
|      7771 | 14208 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14209 | `					SyBlob sTmp;` |
|       ! 0 | 14210 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14211 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14212 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14213 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14214 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14215 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14216 | `					if( zDup ){` |
|       ! 0 | 14217 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14218 | `					}` |
|       ! 0 | 14219 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14220 | `				}` |
|      3883 | 14221 | `			}` |
|      7841 | 14222 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7841 | 14223 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7841 | 14224 | `				if( zDup ){` |
|      7841 | 14225 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3918 | 14226 | `				}` |
|      3918 | 14227 | `			}` |
|         - | 14228 | `		}` |
|      7841 | 14229 | `		SyBlobRelease(&sFQN);` |
|      7841 | 14230 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14231 | `			SyToken *pArgsEnd;` |
|      7739 | 14232 | `			pIn++;` |
|      7739 | 14233 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15487 | 14234 | `			while( pIn < pArgsEnd ){` |
|      7753 | 14235 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7753 | 14236 | `				sxi32 iDepth = 0;` |
|         - | 14237 | `				ph7_attr_arg sArgRec;` |
|     77045 | 14238 | `				while( pArgStop < pArgsEnd ){` |
|     69313 | 14239 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14240 | `						iDepth++;` |
|     69308 | 14241 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14242 | `						iDepth--;` |
|     69298 | 14243 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14244 | `						break;` |
|         - | 14245 | `					}` |
|     69297 | 14246 | `					pArgStop++;` |
|         5 | 14247 | `				}` |
|      7753 | 14248 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7753 | 14249 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7748 | 14250 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7732 | 14251 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14252 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14253 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14254 | `					if( zN ){` |
|        19 | 14255 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14256 | `					}` |
|        19 | 14257 | `					pArgStart += 2;` |
|         9 | 14258 | `				}` |
|      7753 | 14259 | `				if( pArgStart < pArgStop ){` |
|         - | 14260 | `					SySet *pInstrContainer;` |
|      7753 | 14261 | `					pGen->pIn = pArgStart;` |
|      7753 | 14262 | `					pGen->pEnd = pArgStop;` |
|      7753 | 14263 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7753 | 14264 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7753 | 14265 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7753 | 14266 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7753 | 14267 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7753 | 14268 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14269 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14270 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14271 | `						return SXERR_ABORT;` |
|         - | 14272 | `					}` |
|      7753 | 14273 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3874 | 14274 | `				}` |
|      7753 | 14275 | `				pIn = pArgStop;` |
|      7753 | 14276 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14277 | `					pIn++;` |
|         8 | 14278 | `				}` |
|         5 | 14279 | `			}` |
|      7739 | 14280 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3867 | 14281 | `		}` |
|      7841 | 14282 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7841 | 14283 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14284 | `			pIn++;` |
|         5 | 14285 | `			continue;` |
|         - | 14286 | `		}` |
|      7837 | 14287 | `		break;` |
|       ! 0 | 14288 | `	}` |
|      7837 | 14289 | `	pGen->pIn = pSavedIn;` |
|      7837 | 14290 | `	pGen->pEnd = pSavedEnd;` |
|      7837 | 14291 | `	return SXRET_OK;` |
|      3921 | 14292 | `}` |
|         - | 14293 | `/*` |
|         - | 14294 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14295 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14296 | ` */` |
|   3888916 | 14297 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14298 | `{` |
|   3888921 | 14299 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14300 | `	sxu32 n;` |
|         - | 14301 | `	sxi32 rc;` |
|   3896741 | 14302 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7825 | 14303 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7825 | 14304 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14305 | `			return SXERR_ABORT;` |
|         - | 14306 | `		}` |
|      3915 | 14307 | `	}` |
|   3888921 | 14308 | `	SySetReset(&pGen->aPendingAttrs);` |
|   3888921 | 14309 | `	return SXRET_OK;` |
|   1944463 | 14310 | `}` |
|         - | 14311 | `/*` |
|         - | 14312 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14313 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14314 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14315 | ` */` |
|   1666354 | 14316 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14317 | `{` |
|   1666359 | 14318 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   1666359 | 14319 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   1666359 | 14320 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14321 | `	sxu32 nIdx, n;` |
|         - | 14322 | `	sxi32 rc;` |
|   1666354 | 14323 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    192335 | 14324 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1474029 | 14325 | `		return SXRET_OK;` |
|         - | 14326 | `	}` |
|    192335 | 14327 | `	nIdx = (sxu32)(pTok - pBase);` |
|    576993 | 14328 | `	for( n = 0 ; n < nT ; n++ ){` |
|    384663 | 14329 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14330 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14331 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14332 | `				return SXERR_ABORT;` |
|         - | 14333 | `			}` |
|         6 | 14334 | `		}` |
|    192334 | 14335 | `	}` |
|    192335 | 14336 | `	return SXRET_OK;` |
|    833182 | 14337 | `}` |
|  10137986 | 14338 | `static sxi32 GenStateCompileChunk(` |
|         - | 14339 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14340 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14341 | `	)` |
|         5 | 14342 | `{` |
|         - | 14343 | `	ProcLangConstruct xCons;` |
|         - | 14344 | `	sxi32 rc;` |
|  10137991 | 14345 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   5839511 | 14346 | `	for(;;){` |
|  10908509 | 14347 | `		int bStmtIsDeclare = 0;` |
|  10908509 | 14348 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14349 | `			/* No more input to process */` |
|     67969 | 14350 | `			break;` |
|         - | 14351 | `		}` |
|         - | 14352 | `		/* Bind a directly-preceding docblock to this statement */` |
|  10840545 | 14353 | `		GenStateSetPendingDoc(&(*pGen));` |
|  10840545 | 14354 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14355 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14356 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14357 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14358 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14359 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7743 | 14360 | `			int bAttrTarget = 0;` |
|      7738 | 14361 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3903 | 14362 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7685 | 14363 | `				bAttrTarget = 1;` |
|      3899 | 14364 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14365 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14366 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14367 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14368 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14369 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14370 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14371 | `					bAttrTarget = 1;` |
|        29 | 14372 | `				}` |
|        29 | 14373 | `			}` |
|      7743 | 14374 | `			if( !bAttrTarget ){` |
|       ! 0 | 14375 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14376 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14377 | `					&pGen->pIn->sData);` |
|       ! 0 | 14378 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14379 | `					break;` |
|         - | 14380 | `				}` |
|       ! 0 | 14381 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14382 | `			}` |
|      3869 | 14383 | `		}` |
|         - | 14384 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14385 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  10840545 | 14386 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6353293 | 14387 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   6353293 | 14388 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14389 | `				bStmtIsDeclare = 1;` |
|        21 | 14390 | `			}` |
|   3176644 | 14391 | `		}` |
|  10840545 | 14392 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14393 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14394 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    770491 | 14395 | `			pGen->bStrictTypesLocked = 1;` |
|    385243 | 14396 | `		}` |
|  10840545 | 14397 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14398 | `			/* Compile block */` |
|      3863 | 14399 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3863 | 14400 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14401 | `				break;` |
|         - | 14402 | `			}` |
|      1934 | 14403 | `		}else{` |
|  10836687 | 14404 | `			xCons = 0;` |
|  10836687 | 14405 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14406 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14407 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14408 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34641 | 14409 | `				xCons = PH7_CompileClassModifiers;` |
|  10819369 | 14410 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14411 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14412 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3873 | 14413 | `				xCons = PH7_CompileEnum;` |
|  10800117 | 14414 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6318683 | 14415 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14416 | `				/* Try to extract a language construct handler */` |
|   6318683 | 14417 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   6318683 | 14418 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14419 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14420 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14421 | `						&pGen->pIn->sData);` |
|         9 | 14422 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14423 | `						break;` |
|         - | 14424 | `					}` |
|         - | 14425 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14426 | `					 * this erroneous statement.` |
|         - | 14427 | `					 */` |
|         9 | 14428 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14429 | `				}` |
|   7638844 | 14430 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    361789 | 14431 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14432 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14433 | `				xCons = PH7_CompileLabel;` |
|        56 | 14434 | `			}` |
|  10836687 | 14435 | `			if( xCons == 0 ){` |
|         - | 14436 | `				/* Assume an expression an try to compile it */` |
|   4518275 | 14437 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   4518275 | 14438 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14439 | `					/* Pop l-value */` |
|   4518125 | 14440 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2259060 | 14441 | `				}` |
|   2259140 | 14442 | `			}else{` |
|         - | 14443 | `				/* Go compile the sucker */` |
|   6318417 | 14444 | `				rc = xCons(&(*pGen));` |
|         - | 14445 | `			}` |
|  10836687 | 14446 | `			if( rc == SXERR_ABORT ){` |
|         - | 14447 | `				/* Request to abort compilation */` |
|        13 | 14448 | `				break;` |
|         - | 14449 | `			}` |
|         - | 14450 | `		}` |
|         - | 14451 | `		/* Ignore trailing semi-colons ';' */` |
|  18673561 | 14452 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   7833031 | 14453 | `			pGen->pIn++;` |
|         5 | 14454 | `		}` |
|  10840535 | 14455 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14456 | `			/* Compile a single statement and return */` |
|  10070017 | 14457 | `			break;` |
|         - | 14458 | `		}` |
|         - | 14459 | `		/* LOOP ONE */` |
|         - | 14460 | `		/* LOOP TWO */` |
|         - | 14461 | `		/* LOOP THREE */` |
|         - | 14462 | `		/* LOOP FOUR */` |
|         5 | 14463 | `	}` |
|         - | 14464 | `	/* Return compilation status */` |
|  10137991 | 14465 | `	return rc;` |
|         5 | 14466 | `}` |
|         - | 14467 | `/*` |
|         - | 14468 | ` * Compile a Raw PHP chunk.` |
|         - | 14469 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14470 | ` * takes care of generating the appropriate error message.` |
|         - | 14471 | ` */` |
|     67976 | 14472 | `static sxi32 PH7_CompilePHP(` |
|         - | 14473 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14474 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14475 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14476 | `	)` |
|         5 | 14477 | `{` |
|     67981 | 14478 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14479 | `	sxi32 rc;` |
|         - | 14480 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67981 | 14481 | `	SySetReset(&(*pTokenSet));` |
|     67981 | 14482 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14483 | `	/* Mark as the default token set */` |
|     67981 | 14484 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14485 | `	/* Advance the stream cursor */` |
|     67981 | 14486 | `	pGen->pRawIn++;` |
|         - | 14487 | `	/* Tokenize the PHP chunk first */` |
|     67981 | 14488 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14489 | `	/* Point to the head and tail of the token stream. */` |
|     67981 | 14490 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67981 | 14491 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67981 | 14492 | `	if( is_expr ){` |
|       ! 0 | 14493 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14494 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14495 | `			/* A simple expression,compile it */` |
|       ! 0 | 14496 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14497 | `		}` |
|         - | 14498 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14499 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14500 | `		return SXRET_OK;` |
|         - | 14501 | `	}` |
|     67981 | 14502 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14503 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14504 | `		/*` |
|         - | 14505 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14506 | `		 * According to the PHP reference manual:` |
|         - | 14507 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14508 | `		 *  immediately follow` |
|         - | 14509 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14510 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14511 | `		 * Symisc extension:` |
|         - | 14512 | `		 *   This short syntax works with all PHP opening` |
|         - | 14513 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14514 | `		 *   only short tag.` |
|         - | 14515 | `		 */` |
|         - | 14516 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14517 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14518 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14519 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14520 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14521 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14522 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14523 | `		}` |
|         3 | 14524 | `		return SXRET_OK;` |
|         - | 14525 | `	}` |
|         - | 14526 | `	/* Compile the PHP chunk */` |
|     67979 | 14527 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14528 | `	/* Fix exceptions jumps */` |
|     67979 | 14529 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14530 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67979 | 14531 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14532 | `		rc = SXERR_ABORT;` |
|         1 | 14533 | `	}` |
|         - | 14534 | `	/* Reset container */` |
|     67979 | 14535 | `	SySetReset(&pGen->aGoto);` |
|     67979 | 14536 | `	SySetReset(&pGen->aLabel);` |
|     67979 | 14537 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14538 | `	/* Compilation result */` |
|     67979 | 14539 | `	return rc;` |
|     33993 | 14540 | `}` |
|         - | 14541 | `/*` |
|         - | 14542 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14543 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14544 | ` * This is the only compile interface exported from this file.` |
|         - | 14545 | ` */` |
|     71054 | 14546 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14547 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14548 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14549 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14550 | `	)` |
|         5 | 14551 | `{` |
|         - | 14552 | `	SySet aPhpToken,aRawToken;` |
|         - | 14553 | `	ph7_gen_state *pCodeGen;` |
|         - | 14554 | `	ph7_value *pRawObj;` |
|         - | 14555 | `	sxu32 nObjIdx;` |
|         - | 14556 | `	sxi32 nRawObj;` |
|         - | 14557 | `	int is_expr;` |
|         - | 14558 | `	sxi8 bSavedStrict;` |
|         - | 14559 | `	sxi8 bSavedStrictLocked;` |
|         - | 14560 | `	sxi32 rc;` |
|     71059 | 14561 | `	if( pScript->nByte < 1 ){` |
|         - | 14562 | `		/* Nothing to compile */` |
|       ! 0 | 14563 | `		return PH7_OK;` |
|         - | 14564 | `	}` |
|         - | 14565 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14566 | `	 * file's flags so include/require restore them on return. */` |
|     71059 | 14567 | `	pCodeGen = &pVm->sCodeGen;` |
|     71059 | 14568 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     71059 | 14569 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     71059 | 14570 | `	pCodeGen->bStrictTypes = 0;` |
|     71059 | 14571 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14572 | `	/* Initialize the tokens containers */` |
|     71059 | 14573 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     71059 | 14574 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     71059 | 14575 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     71059 | 14576 | `	is_expr = 0;` |
|     71059 | 14577 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14578 | `		SyToken sTmp;` |
|         - | 14579 | `		/* PHP only: -*/` |
|     57717 | 14580 | `		sTmp.nLine = 1;` |
|     57717 | 14581 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57717 | 14582 | `		sTmp.pUserData = 0;` |
|     57717 | 14583 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57717 | 14584 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57717 | 14585 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14586 | `			/* A simple PHP expression */` |
|       ! 0 | 14587 | `			is_expr = 1;` |
|       ! 0 | 14588 | `		}` |
|     28861 | 14589 | `	}else{` |
|         - | 14590 | `		/* Tokenize raw text */` |
|     13347 | 14591 | `		SySetAlloc(&aRawToken,32);` |
|     13347 | 14592 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|         - | 14593 | `	}` |
|         - | 14594 | `	/* Process high-level tokens */` |
|     71059 | 14595 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     71059 | 14596 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     71059 | 14597 | `	rc = PH7_OK;` |
|     71059 | 14598 | `	if( is_expr ){` |
|         - | 14599 | `		/* Compile the expression */` |
|       ! 0 | 14600 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14601 | `		goto cleanup;` |
|         - | 14602 | `	}` |
|     71059 | 14603 | `	nObjIdx = 0;` |
|         - | 14604 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14605 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14606 | `	 * preventing namespace bleeding across include()d files. */` |
|     71059 | 14607 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14608 | `	/* Start the compilation process */` |
|     42203 | 14609 | `	for(;;){` |
|    152375 | 14610 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     71047 | 14611 | `			break; /* No more tokens to process */` |
|         - | 14612 | `		}` |
|     81333 | 14613 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14614 | `			/* Compile the PHP chunk */` |
|     67981 | 14615 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67981 | 14616 | `			if( rc == SXERR_ABORT ){` |
|        16 | 14617 | `				break;` |
|         - | 14618 | `			}` |
|     67969 | 14619 | `			continue;` |
|         - | 14620 | `		}` |
|         - | 14621 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13357 | 14622 | `		nRawObj = 0;` |
|     26709 | 14623 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14624 | `			/* Consume the raw chunk without any processing */` |
|     13357 | 14625 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13357 | 14626 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14627 | `				rc = SXERR_MEM;` |
|       ! 0 | 14628 | `				break;` |
|         - | 14629 | `			}` |
|         - | 14630 | `			/* Mark as constant and emit the load constant instruction */` |
|     13357 | 14631 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13357 | 14632 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13357 | 14633 | `			++nRawObj;` |
|     13357 | 14634 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14635 | `		}` |
|     13357 | 14636 | `		if( nRawObj > 0 ){` |
|         - | 14637 | `			/* Emit the consume instruction */` |
|     13357 | 14638 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6676 | 14639 | `		}` |
|     35532 | 14640 | `	}` |
|     35527 | 14641 | `cleanup:` |
|     71059 | 14642 | `	SySetRelease(&aRawToken);` |
|     71059 | 14643 | `	SySetRelease(&aPhpToken);` |
|         - | 14644 | `	/* Restore outer file's strict_types scope */` |
|     71059 | 14645 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     71059 | 14646 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     71059 | 14647 | `	return rc;` |
|     35532 | 14648 | `}` |
|         - | 14649 | `/*` |
|         - | 14650 | ` * Utility routines.Initialize the code generator.` |
|         - | 14651 | ` */` |
|      3840 | 14652 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 14653 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14654 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14655 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14656 | `	)` |
|         5 | 14657 | `{` |
|      3845 | 14658 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14659 | `	/* Zero the structure */` |
|      3845 | 14660 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 14661 | `	/* Initial state */` |
|      3845 | 14662 | `	pGen->pVm  = &(*pVm);` |
|      3845 | 14663 | `	pGen->xErr = xErr;` |
|      3845 | 14664 | `	pGen->pErrData = pErrData;` |
|      3845 | 14665 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3845 | 14666 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3845 | 14667 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3845 | 14668 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3845 | 14669 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3845 | 14670 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3845 | 14671 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3845 | 14672 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3845 | 14673 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 14674 | `	/* Error log buffer */` |
|      3845 | 14675 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 14676 | `	/* General purpose working buffer */` |
|      3845 | 14677 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 14678 | `	/* Namespace state */` |
|      3845 | 14679 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3845 | 14680 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3845 | 14681 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3845 | 14682 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14683 | `	/* Create the global scope */` |
|      3845 | 14684 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 14685 | `	/* Point to the global scope */` |
|      3845 | 14686 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3845 | 14687 | `	return SXRET_OK;` |
|         5 | 14688 | `}` |
|         - | 14689 | `/*` |
|         - | 14690 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 14691 | ` */` |
|     74442 | 14692 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 14693 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14694 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14695 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14696 | `	)` |
|         5 | 14697 | `{` |
|     74447 | 14698 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14699 | `	GenBlock *pBlock,*pParent;` |
|         - | 14700 | `	/* Reset state */` |
|     74447 | 14701 | `	SySetReset(&pGen->aLabel);` |
|     74447 | 14702 | `	SySetReset(&pGen->aGoto);` |
|     74447 | 14703 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     74447 | 14704 | `	SySetReset(&pGen->aTrivia);` |
|     74447 | 14705 | `	SySetReset(&pGen->aPendingAttrs);` |
|     74447 | 14706 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     74447 | 14707 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     74447 | 14708 | `	SyBlobRelease(&pGen->sWorker);` |
|     74447 | 14709 | `	SyBlobRelease(&pGen->sNamespace);` |
|     74447 | 14710 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     74447 | 14711 | `	SyHashRelease(&pGen->hUseImports);` |
|     74447 | 14712 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     74447 | 14713 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     74447 | 14714 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     74447 | 14715 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     74447 | 14716 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14717 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 14718 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 14719 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 14720 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 14721 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 14722 | `	 * number of unique names, which is acceptable. */` |
|         - | 14723 | `	/* Point to the global scope */` |
|     74447 | 14724 | `	pBlock = pGen->pCurrent;` |
|     74447 | 14725 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 14726 | `		pParent = pBlock->pParent;` |
|       ! 0 | 14727 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 14728 | `		pBlock = pParent;` |
|       ! 0 | 14729 | `	}` |
|     74447 | 14730 | `	pGen->xErr = xErr;` |
|     74447 | 14731 | `	pGen->pErrData = pErrData;` |
|     74447 | 14732 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     74447 | 14733 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     74447 | 14734 | `	pGen->pIn = pGen->pEnd = 0;` |
|     74447 | 14735 | `	pGen->nErr = 0;` |
|     74447 | 14736 | `	return SXRET_OK;` |
|         5 | 14737 | `}` |
|         - | 14738 | `/*` |
|         - | 14739 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 14740 | ` * php's parser prints, e.g.` |
|         - | 14741 | ` *` |
|         - | 14742 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 14743 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 14744 | ` *   syntax error, unexpected end of file` |
|         - | 14745 | ` *` |
|         - | 14746 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 14747 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 14748 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 14749 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 14750 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 14751 | ` *` |
|         - | 14752 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 14753 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 14754 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 14755 | ` */` |
|       178 | 14756 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 14757 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 14758 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 14759 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 14760 | `	)` |
|         5 | 14761 | `{` |
|       183 | 14762 | `	const char *zNoun = "token";` |
|         - | 14763 | `	sxu32 nLine;` |
|       183 | 14764 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 14765 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 14766 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 14767 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 14768 | `		 * it before concluding "end of file". */` |
|        82 | 14769 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        82 | 14770 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        82 | 14771 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        82 | 14772 | `			pTok = pGen->pEnd;` |
|        39 | 14773 | `		}` |
|        39 | 14774 | `	}` |
|       183 | 14775 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       183 | 14776 | `	if( pTok == 0 ){` |
|       ! 0 | 14777 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 14778 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 14779 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 14780 | `			zExpecting);` |
|         - | 14781 | `	}` |
|       183 | 14782 | `	if( pTok->nType & PH7_TK_ID ){` |
|        15 | 14783 | `		zNoun = "identifier";` |
|       176 | 14784 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         7 | 14785 | `		zNoun = "variable";` |
|       167 | 14786 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        23 | 14787 | `		zNoun = "integer";` |
|       154 | 14788 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 14789 | `		zNoun = "float";` |
|       ! 0 | 14790 | `	}` |
|       183 | 14791 | `	if( zExpecting ){` |
|       115 | 14792 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        37 | 14793 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 14794 | `	}` |
|       161 | 14795 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        52 | 14796 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        94 | 14797 | `}` |
|         - | 14798 | `/*` |
|         - | 14799 | ` * Generate a compile-time error message.` |
|         - | 14800 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 14801 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 14802 | ` * abort compilation immediately.` |
|         - | 14803 | ` */` |
|     16026 | 14804 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 14805 | `{` |
|     16031 | 14806 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     16031 | 14807 | `	const char *zErr = "Error";` |
|         - | 14808 | `	SyString *pFile;` |
|         - | 14809 | `	va_list ap;` |
|         - | 14810 | `	sxi32 rc;` |
|         - | 14811 | `	/* Reset the working buffer */` |
|     16031 | 14812 | `	SyBlobReset(pWorker);` |
|         - | 14813 | `	/* Peek the processed file path if available */` |
|     16031 | 14814 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     16031 | 14815 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 14816 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 14817 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 14818 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 14819 | `		 * into execution with a 0 exit status. */` |
|       653 | 14820 | `		pGen->nErr++;` |
|       653 | 14821 | `		if( pGen->nErr > 15 ){` |
|         - | 14822 | `			/* Error count limit reached */` |
|         6 | 14823 | `			if( pGen->xErr ){` |
|         6 | 14824 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 14825 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 14826 | `				if( pFile ){` |
|         6 | 14827 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 14828 | `				}` |
|         6 | 14829 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 14830 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 14831 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 14832 | `				}` |
|         2 | 14833 | `			}` |
|         - | 14834 | `			/* Abort immediately */` |
|         6 | 14835 | `			return SXERR_ABORT;` |
|         - | 14836 | `		}` |
|       322 | 14837 | `	}` |
|     16027 | 14838 | `	if( pGen->xErr == 0 ){` |
|         - | 14839 | `		/* No available error consumer,return immediately */` |
|     15367 | 14840 | `		return SXRET_OK;` |
|         - | 14841 | `	}` |
|       664 | 14842 | `	switch(nErrType){` |
|       310 | 14843 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|         6 | 14844 | `	case E_WARNING: zErr = "Warning";     break;` |
|       340 | 14845 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|        11 | 14846 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 14847 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 14848 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 14849 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|         7 | 14850 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 14851 | `	default:` |
|       ! 0 | 14852 | `		break;` |
|         - | 14853 | `	}` |
|       664 | 14854 | `	rc = SXRET_OK;` |
|         - | 14855 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       664 | 14856 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       664 | 14857 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       664 | 14858 | `	va_start(ap,zFormat);` |
|       664 | 14859 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       664 | 14860 | `	va_end(ap);` |
|       664 | 14861 | `	if( pFile ){` |
|       664 | 14862 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       330 | 14863 | `	}` |
|         - | 14864 | `	/* Append a new line */` |
|       664 | 14865 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       664 | 14866 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 14867 | `		/* Consume the generated error message */` |
|       664 | 14868 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       330 | 14869 | `	}` |
|       664 | 14870 | `	return rc;` |
|      8018 | 14871 | `}` |
|         - | 14872 |  |
