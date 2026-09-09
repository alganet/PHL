# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7186/8892 lines (80.81%)

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
|        93 |   130 | `	}` |
|         - |   131 | `	/* No such destination */` |
|        60 |   132 | `	return SXERR_NOTFOUND;` |
|        79 |   133 | `}` |
|         - |   134 | `/*` |
|         - |   135 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   136 | ` * compiled blocks.` |
|         - |   137 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   138 | ` */` |
|    145036 |   139 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   140 | `{` |
|    145041 |   141 | `	GenBlock *pBlock = pCurrent;` |
|    328051 |   142 | `	for(;;){` |
|    656107 |   143 | `		if( pBlock->iFlags & iBlockType ){` |
|    145041 |   144 | `			iCount--; /* Decrement nesting level */` |
|    145041 |   145 | `			if( iCount < 1 ){` |
|         - |   146 | `				/* Block meet with the desired criteria */` |
|    145015 |   147 | `				return pBlock;` |
|         - |   148 | `			}` |
|        13 |   149 | `		}` |
|         - |   150 | `		/* Point to the upper block */` |
|    511097 |   151 | `		pBlock = pBlock->pParent;` |
|    511097 |   152 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   153 | `			/* Forbidden */` |
|        16 |   154 | `			break;` |
|         - |   155 | `		}` |
|         5 |   156 | `	}` |
|         - |   157 | `	/* No such block */` |
|        29 |   158 | `	return 0;` |
|     72523 |   159 | `}` |
|         - |   160 | `/*` |
|         - |   161 | ` * Initialize a freshly allocated block instance.` |
|         - |   162 | ` */` |
|  11243412 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|  11243417 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  11243417 |   173 | `	pBlock->pUserData   = pUserData;` |
|  11243417 |   174 | `	pBlock->pGen        = pGen;` |
|  11243417 |   175 | `	pBlock->iFlags      = iType;` |
|  11243417 |   176 | `	pBlock->pParent     = 0;` |
|  11243417 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11243417 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11243417 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|  11239600 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|  11239605 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  11239605 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|  11239605 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  11239605 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|  11239605 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  11239605 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    477567 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    477567 |   214 | `		pGen->nLoopId++;` |
|    477567 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    477567 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    477567 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    477567 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    238781 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|  11239605 |   221 | `	pGen->pCurrent = pBlock;` |
|  11239605 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   5382117 |   224 | `		*ppBlock = pBlock;` |
|   2691056 |   225 | `	}` |
|  11239605 |   226 | `	return SXRET_OK;` |
|   5619805 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|  11239584 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|  11239589 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|  11239589 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|  11239589 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|  11239584 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|  11239589 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  11239589 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|  11239589 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  11239589 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|  11239584 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|  11239589 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  11239589 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|  11239589 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    477559 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    238777 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|  11239589 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|  11239589 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|  11239589 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|  11239589 |   268 | `	return SXRET_OK;` |
|   5619797 |   269 | `}` |
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
|   4256912 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   4256917 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   4256917 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   4256917 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4256917 |   289 | `	return rc;` |
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
|   7876016 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   7876021 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  16967201 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9091185 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   3396319 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   5694871 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1437961 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   4256915 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4256915 |   322 | `		if( pInstr ){` |
|   4256915 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   4256915 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   4256915 |   326 | `			aFix[n].nJumpType = -1;` |
|   2128455 |   327 | `		}` |
|   2128460 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   7876021 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2766164 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2766169 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2766315 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|        10 |   387 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        10 |   388 | `			if( rc == SXERR_ABORT ){` |
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
|   2766167 |   400 | `	return SXRET_OK;` |
|   1383087 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  14228532 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  14228537 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  14228537 |   409 | `	if( pEntry == 0 ){` |
|   3674107 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|  10554435 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10554435 |   413 | `	return SXRET_OK;` |
|   7114271 |   414 | `}` |
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
|   3674102 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3674107 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3674107 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1837051 |   429 | `	}` |
|   3674107 |   430 | `	return SXRET_OK;` |
|         5 |   431 | `}` |
|         - |   432 | `/*` |
|         - |   433 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   434 | ` * in the constant table.` |
|         - |   435 | ` */` |
|   3350832 |   436 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   437 | `{` |
|         - |   438 | `	ph7_value *pObj;` |
|   3350837 |   439 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   440 | `	/* Reserve a new constant */` |
|   3350837 |   441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3350837 |   442 | `	if( pObj == 0 ){` |
|       ! 0 |   443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   444 | `		return 0;` |
|         - |   445 | `	}` |
|   3350837 |   446 | `	*pIdx = nIdx;` |
|         - |   447 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   448 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   449 | `	 */` |
|   3350837 |   450 | `	return pObj;` |
|   1675421 |   451 | `}` |
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
|   6803030 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6803035 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3401520 |   478 | `}` |
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
|   3359482 |   542 | `static int GenStateFindBadNumericSeparator(` |
|         - |   543 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   544 | `{` |
|   3359487 |   545 | `	const char *z = pRaw->zString;` |
|   3359487 |   546 | `	sxu32 n = pRaw->nByte;` |
|   3359487 |   547 | `	int base = 10;` |
|         - |   548 | `	sxu32 i, start;` |
|   3359487 |   549 | `	if( n < 2 ) return 0;` |
|    736291 |   550 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|    103019 |   551 | `		base = 16;` |
|    684784 |   552 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       284 |   553 | `		base = 2;` |
|       141 |   554 | `	}` |
|   2774587 |   555 | `	for( i = 0; i < n; ++i ){` |
|   2038315 |   556 | `		if( z[i] != '_' ) continue;` |
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
|    736277 |   573 | `	return 0;` |
|   1679746 |   574 | `}` |
|         - |   575 | `/*` |
|         - |   576 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   577 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   578 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   579 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   580 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   581 | ` * so callers can bail from the current construct).` |
|         - |   582 | ` */` |
|   3359482 |   583 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   584 | `{` |
|   3359487 |   585 | `	const char *zBad = 0;` |
|   3359487 |   586 | `	sxu32 nBad = 0;` |
|         - |   587 | `	SyString sBad;` |
|         - |   588 | `	sxi32 rc;` |
|   3359487 |   589 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   3359473 |   590 | `		return SXRET_OK;` |
|         - |   591 | `	}` |
|        18 |   592 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   593 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   594 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   595 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   596 | `		return SXERR_ABORT;` |
|         - |   597 | `	}` |
|        18 |   598 | `	return SXERR_SYNTAX;` |
|   1679746 |   599 | `}` |
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
|   3359468 |   616 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   617 | `	SyMemBackend *pAlloc,` |
|         - |   618 | `	const SyString *pToken,` |
|         - |   619 | `	char *zScratch, sxu32 nScratch,` |
|         - |   620 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   621 | `{` |
|         - |   622 | `	sxu32 i, j;` |
|   3359473 |   623 | `	int hasUnderscore = 0;` |
|         - |   624 | `	char *zBuf;` |
|   3359473 |   625 | `	*pzAlloc = 0;` |
|   8018899 |   626 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   4659683 |   627 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   2329718 |   628 | `	}` |
|   3359473 |   629 | `	if( !hasUnderscore ){` |
|   3359221 |   630 | `		SyStringDupPtr(pOut, pToken);` |
|   3359221 |   631 | `		return SXRET_OK;` |
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
|   1679739 |   648 | `}` |
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
|   3350866 |   684 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   685 | `{` |
|   3350871 |   686 | `	const char *z = pNum->zString;` |
|   3350871 |   687 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   688 | `	const char *p, *q;` |
|         - |   689 | `	int n;` |
|   3350871 |   690 | `	*pbDecimal = FALSE;` |
|   3350871 |   691 | `	if( z >= zEnd ){` |
|       ! 0 |   692 | `		return FALSE;` |
|         - |   693 | `	}` |
|   3350871 |   694 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|         - |   695 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|    103017 |   696 | `		p = z + 2;` |
|    129709 |   697 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    419883 |   698 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|    103017 |   699 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|    103011 |   700 | `			return FALSE;` |
|         - |   701 | `		}` |
|         7 |   702 | `		{ ph7_real dv = 0;` |
|       103 |   703 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|        97 |   704 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|        49 |   705 | `		  }` |
|         7 |   706 | `		  *pReal = dv;` |
|         - |   707 | `		}` |
|         7 |   708 | `		return TRUE;` |
|   3247859 |   709 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|   3247579 |   724 | `	}else if( z[0] == '0' ){` |
|         - |   725 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   726 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   727 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1214545 |   728 | `		p = z;` |
|   2429087 |   729 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1226233 |   730 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1214545 |   731 | `		if( n <= 21 ){` |
|   1214543 |   732 | `			return FALSE;` |
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
|   2033039 |   745 | `	p = z;` |
|   2033039 |   746 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   4887951 |   747 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   2033039 |   748 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   749 | `		*pbDecimal = TRUE;` |
|        25 |   750 | `		return TRUE;` |
|         - |   751 | `	}` |
|   2033015 |   752 | `	return FALSE;` |
|   1675438 |   753 | `}` |
|   3359454 |   754 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   755 | `{` |
|   3359459 |   756 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   3359459 |   757 | `	sxu32 nIdx = 0;` |
|         - |   758 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   3359459 |   759 | `	char *zAlloc = 0;` |
|         - |   760 | `	SyString sNum;` |
|         - |   761 | `	sxi32 rc;` |
|   1679727 |   762 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   3359459 |   763 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   3359459 |   764 | `	if( rc != SXRET_OK ){` |
|        14 |   765 | `		return rc;` |
|         - |   766 | `	}` |
|   5039171 |   767 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1679722 |   768 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   3359449 |   769 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   770 | `		return SXERR_ABORT;` |
|         - |   771 | `	}` |
|   3359449 |   772 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   773 | `		ph7_value *pObj;` |
|         - |   774 | `		sxi64 iValue;` |
|   3350871 |   775 | `		ph7_real rOverflow = 0;` |
|   3350871 |   776 | `		int bDecimalOverflow = 0;` |
|   3350871 |   777 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|   3350837 |   794 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   3350837 |   795 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   3350837 |   796 | `			if( pObj == 0 ){` |
|       ! 0 |   797 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   798 | `				return SXERR_ABORT;` |
|         - |   799 | `			}` |
|   3350837 |   800 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   801 | `		}` |
|   1675438 |   802 | `	}else{` |
|         - |   803 | `		/* Real number */` |
|         - |   804 | `		ph7_value *pObj;` |
|         - |   805 | `		/* Reserve a new constant */` |
|      8583 |   806 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      8583 |   807 | `		if( pObj == 0 ){` |
|       ! 0 |   808 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   809 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   810 | `			return SXERR_ABORT;` |
|         - |   811 | `		}` |
|      8583 |   812 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      8583 |   813 | `		PH7_MemObjToReal(pObj);` |
|         - |   814 | `	}` |
|   3359449 |   815 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   816 | `	/* Emit the load constant instruction */` |
|   3359449 |   817 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   818 | `	/* Node successfully compiled */` |
|   3359449 |   819 | `	return SXRET_OK;` |
|   1679732 |   820 | `}` |
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
|   4792954 |   832 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   833 | `{` |
|   4792959 |   834 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   835 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   836 | `	ph7_value *pObj;` |
|         - |   837 | `	sxu32 nIdx;` |
|   4792959 |   838 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   839 | `	/* Delimit the string */` |
|   4792959 |   840 | `	zIn  = pStr->zString;` |
|   4792959 |   841 | `	zEnd = &zIn[pStr->nByte];` |
|   4792959 |   842 | `	if( zIn >= zEnd ){` |
|         - |   843 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   844 | `		 * rather than reserving a new object each time. */` |
|    305179 |   845 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    305179 |   846 | `		return SXRET_OK;` |
|         - |   847 | `	}` |
|   4487785 |   848 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   849 | `		/* Already processed,emit the load constant instruction` |
|         - |   850 | `		 * and return.` |
|         - |   851 | `		 */` |
|   2680449 |   852 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2680449 |   853 | `		return SXRET_OK;` |
|         - |   854 | `	}` |
|         - |   855 | `	/* Reserve a new constant */` |
|   1807341 |   856 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1807341 |   857 | `	if( pObj == 0 ){` |
|       ! 0 |   858 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   859 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   860 | `		return SXERR_ABORT;` |
|         - |   861 | `	}` |
|   1807341 |   862 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   863 | `	/* Compile the node */` |
|   1855048 |   864 | `	for(;;){` |
|   3710101 |   865 | `		if( zIn >= zEnd ){` |
|         - |   866 | `			/* End of input */` |
|   1807341 |   867 | `			break;` |
|         - |   868 | `		}` |
|   1902765 |   869 | `		zCur = zIn;` |
|  39725777 |   870 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  37823017 |   871 | `			zIn++;` |
|         5 |   872 | `		}` |
|   1902765 |   873 | `		if( zIn > zCur ){` |
|         - |   874 | `			/* Append raw contents*/` |
|   1864613 |   875 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    932304 |   876 | `		}` |
|   1902765 |   877 | `		zIn++;` |
|   1902765 |   878 | `		if( zIn < zEnd ){` |
|    129765 |   879 | `			if( zIn[0] == '\\' ){` |
|         - |   880 | `				/* A literal backslash */` |
|     30537 |   881 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    114499 |   882 | `			}else if( zIn[0] == '\'' ){` |
|         - |   883 | `				/* A single quote */` |
|        11 |   884 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   885 | `			}else{` |
|         - |   886 | `				/* verbatim copy */` |
|     99223 |   887 | `				zIn--;` |
|     99223 |   888 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     99223 |   889 | `				zIn++;` |
|         - |   890 | `			}` |
|     64880 |   891 | `		}` |
|         - |   892 | `		/* Advance the stream cursor */` |
|   1902765 |   893 | `		zIn++;` |
|         5 |   894 | `	}` |
|         - |   895 | `	/* Emit the load constant instruction */` |
|   1807341 |   896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1807341 |   897 | `	if( pStr->nByte < 1024 ){` |
|         - |   898 | `		/* Install in the literal table */` |
|   1807341 |   899 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    903668 |   900 | `	}` |
|         - |   901 | `	/* Node successfully compiled */` |
|   1807341 |   902 | `	return SXRET_OK;` |
|   2396482 |   903 | `}` |
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
|         - |  1029 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|         - |  1030 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|         7 |  1031 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|         7 |  1032 | `		return SXRET_OK;` |
|         - |  1033 | `	}` |
|         - |  1034 | `	/* Reserve a new constant */` |
|        40 |  1035 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        40 |  1036 | `	if( pObj == 0 ){` |
|       ! 0 |  1037 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1038 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  1039 | `		return SXERR_ABORT;` |
|         - |  1040 | `	}` |
|         - |  1041 | `	/* No processing is done here, simply a memcpy() operation */` |
|        40 |  1042 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|         - |  1043 | `	/* Emit the load constant instruction */` |
|        40 |  1044 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  1045 | `	/* Node successfully compiled */` |
|        40 |  1046 | `	return SXRET_OK;` |
|        27 |  1047 | `}` |
|         - |  1048 | `/*` |
|         - |  1049 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|         - |  1050 | ` * According to the PHP language reference manual` |
|         - |  1051 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|         - |  1052 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|         - |  1053 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|         - |  1054 | ` *  property in a string with a minimum of effort.` |
|         - |  1055 | ` *  Simple syntax` |
|         - |  1056 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|         - |  1057 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|         - |  1058 | ` *   the end of the name.` |
|         - |  1059 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|         - |  1060 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|         - |  1061 | ` *   as to simple variables.` |
|         - |  1062 | ` *  Complex (curly) syntax` |
|         - |  1063 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|         - |  1064 | ` *   of complex expressions.` |
|         - |  1065 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|         - |  1066 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|         - |  1067 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|         - |  1068 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|         - |  1069 | ` */` |
|      2594 |  1070 | `static sxi32 GenStateProcessStringExpression(` |
|         - |  1071 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1072 | `	sxu32 nLine,         /* Line number */` |
|         - |  1073 | `	const char *zIn,     /* Raw expression */` |
|         - |  1074 | `	const char *zEnd     /* End of the expression */` |
|         - |  1075 | `	)` |
|         5 |  1076 | `{` |
|         - |  1077 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1078 | `	SySet sToken;` |
|         - |  1079 | `	sxi32 rc;` |
|         - |  1080 | `	/* Initialize the token set */` |
|      2599 |  1081 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1082 | `	/* Preallocate some slots */` |
|      2599 |  1083 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1084 | `	/* Tokenize the text */` |
|      2599 |  1085 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1086 | `	/* Swap delimiter */` |
|      2599 |  1087 | `	pTmpIn  = pGen->pIn;` |
|      2599 |  1088 | `	pTmpEnd = pGen->pEnd;` |
|      2599 |  1089 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2599 |  1090 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1091 | `	/* Compile the expression */` |
|      2599 |  1092 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1093 | `	/* Restore token stream */` |
|      2599 |  1094 | `	pGen->pIn  = pTmpIn;` |
|      2599 |  1095 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1096 | `	/* Release the token set */` |
|      2599 |  1097 | `	SySetRelease(&sToken);` |
|         - |  1098 | `	/* Compilation result */` |
|      2599 |  1099 | `	return rc;` |
|         5 |  1100 | `}` |
|         - |  1101 | `/*` |
|         - |  1102 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1103 | ` */` |
|    120968 |  1104 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1105 | `{` |
|         - |  1106 | `	ph7_value *pConstObj;` |
|    120973 |  1107 | `	sxu32 nIdx = 0;` |
|         - |  1108 | `	/* Reserve a new constant */` |
|    120973 |  1109 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    120973 |  1110 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1111 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1112 | `		return 0;` |
|         - |  1113 | `	}` |
|    120973 |  1114 | `	(*pCount)++;` |
|    120973 |  1115 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1116 | `	/* Emit the load constant instruction */` |
|    120973 |  1117 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    120973 |  1118 | `	return pConstObj;` |
|     60489 |  1119 | `}` |
|         - |  1120 | `/*` |
|         - |  1121 | ` * Compile a double quoted/heredoc string.` |
|         - |  1122 | ` * According to the PHP language reference manual` |
|         - |  1123 | ` * Heredoc` |
|         - |  1124 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - |  1125 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - |  1126 | ` *  to close the quotation.` |
|         - |  1127 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - |  1128 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - |  1129 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - |  1130 | ` *  Warning` |
|         - |  1131 | ` *  It is very important to note that the line with the closing identifier must contain` |
|         - |  1132 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|         - |  1133 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|         - |  1134 | ` *  It's also important to realize that the first character before the closing identifier must` |
|         - |  1135 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|         - |  1136 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|         - |  1137 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|         - |  1138 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|         - |  1139 | ` *  the end of the current file, a parse error will result at the last line.` |
|         - |  1140 | ` *  Heredocs can not be used for initializing class properties.` |
|         - |  1141 | ` * Double quoted` |
|         - |  1142 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|         - |  1143 | ` *  Escaped characters Sequence 	Meaning` |
|         - |  1144 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|         - |  1145 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|         - |  1146 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|         - |  1147 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|         - |  1148 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|         - |  1149 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|         - |  1150 | ` *  \\ backslash` |
|         - |  1151 | ` *  \$ dollar sign` |
|         - |  1152 | ` *  \" double-quote` |
|         - |  1153 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|         - |  1154 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|         - |  1155 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|         - |  1156 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|         - |  1157 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|         - |  1158 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|         - |  1159 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|         - |  1160 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|         - |  1161 | ` * See string parsing for details.` |
|         - |  1162 | ` */` |
|         - |  1163 | `/*` |
|         - |  1164 | ` * Line number of an escape sequence inside the string body being compiled:` |
|         - |  1165 | ` * the token's line plus every newline before the escape (php reports the` |
|         - |  1166 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|         - |  1167 | ` * on the line after the '<<<' marker, hence the +1.` |
|         - |  1168 | ` */` |
|         6 |  1169 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|         3 |  1170 | `{` |
|         9 |  1171 | `	const char *z = pGen->pIn->sData.zString;` |
|         9 |  1172 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|        15 |  1173 | `	for( ; z < zPos ; z++ ){` |
|         9 |  1174 | `		if( z[0] == '\n' ){` |
|       ! 0 |  1175 | `			nLine++;` |
|       ! 0 |  1176 | `		}` |
|         6 |  1177 | `	}` |
|         9 |  1178 | `	return nLine;` |
|         3 |  1179 | `}` |
|         - |  1180 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|         - |  1181 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|    119416 |  1182 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1183 | `{` |
|    119421 |  1184 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1185 | `	const char *zIn,*zCur,*zEnd;` |
|    119421 |  1186 | `	ph7_value *pObj = 0;` |
|         - |  1187 | `	sxi32 iCons;` |
|         - |  1188 | `	sxi32 rc;` |
|         - |  1189 | `	/* Delimit the string */` |
|    119421 |  1190 | `	zIn  = pStr->zString;` |
|    119421 |  1191 | `	zEnd = &zIn[pStr->nByte];` |
|    119421 |  1192 | `	if( zIn >= zEnd ){` |
|         - |  1193 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1194 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1195 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1196 | `		 */` |
|       411 |  1197 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       411 |  1198 | `		return SXRET_OK;` |
|         - |  1199 | `	}` |
|    119015 |  1200 | `	zCur = 0;` |
|         - |  1201 | `	/* Compile the node */` |
|    119015 |  1202 | `	iCons = 0;` |
|     60800 |  1203 | `	for(;;){` |
|    161791 |  1204 | `		zCur = zIn;` |
|   1651169 |  1205 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1491977 |  1206 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        68 |  1207 | `				break;` |
|   1491852 |  1208 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2474 |  1209 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1237 |  1210 | `					break;` |
|         - |  1211 | `			}` |
|   1489383 |  1212 | `			zIn++;` |
|         5 |  1213 | `		}` |
|    161791 |  1214 | `		if( zIn > zCur ){` |
|     94421 |  1215 | `			if( pObj == 0 ){` |
|     93811 |  1216 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     93811 |  1217 | `				if( pObj == 0 ){` |
|       ! 0 |  1218 | `					return SXERR_ABORT;` |
|         - |  1219 | `				}` |
|     46903 |  1220 | `			}` |
|     94421 |  1221 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     47208 |  1222 | `		}` |
|    161791 |  1223 | `		if( zIn >= zEnd ){` |
|    119013 |  1224 | `			break;` |
|         - |  1225 | `		}` |
|     42783 |  1226 | `		if( zIn[0] == '\\' ){` |
|     40189 |  1227 | `			const char *zPtr = 0;` |
|         - |  1228 | `			sxu32 n;` |
|     40189 |  1229 | `			zIn++;` |
|     40189 |  1230 | `			if( pObj == 0 ){` |
|     27167 |  1231 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27167 |  1232 | `				if( pObj == 0 ){` |
|       ! 0 |  1233 | `					return SXERR_ABORT;` |
|         - |  1234 | `				}` |
|     13581 |  1235 | `			}` |
|     40189 |  1236 | `			if( zIn >= zEnd ){` |
|         - |  1237 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1238 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1239 | `				break;` |
|         - |  1240 | `			}` |
|     40187 |  1241 | `			n = sizeof(char); /* size of conversion */` |
|     40187 |  1242 | `			switch( zIn[0] ){` |
|        13 |  1243 | `			case '$':` |
|         - |  1244 | `				/* Dollar sign */` |
|        29 |  1245 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        29 |  1246 | `				break;` |
|        52 |  1247 | `			case '\\':` |
|         - |  1248 | `				/* A literal backslash */` |
|       109 |  1249 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       109 |  1250 | `				break;` |
|         1 |  1251 | `			case 'e':` |
|         - |  1252 | `				/* Escape (ESC) ASCII code 27 */` |
|         3 |  1253 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|         3 |  1254 | `				break;` |
|         4 |  1255 | `			case 'f':` |
|         - |  1256 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|         9 |  1257 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|         9 |  1258 | `				break;` |
|     17515 |  1259 | `			case 'n':` |
|         - |  1260 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     35035 |  1261 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     35035 |  1262 | `				break;` |
|        27 |  1263 | `			case 'r':` |
|         - |  1264 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1265 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1266 | `				break;` |
|      1937 |  1267 | `			case 't':` |
|         - |  1268 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      3879 |  1269 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      3879 |  1270 | `				break;` |
|         3 |  1271 | `			case 'v':` |
|         - |  1272 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|         7 |  1273 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|         7 |  1274 | `				break;` |
|       141 |  1275 | `			case '"':` |
|       287 |  1276 | `				if( bHeredoc ){` |
|         - |  1277 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|         5 |  1278 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|         3 |  1279 | `				}else{` |
|         - |  1280 | `					/* Double quote */` |
|       283 |  1281 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|         - |  1282 | `				}` |
|       287 |  1283 | `				break;` |
|        24 |  1284 | `			case '0': case '1': case '2': case '3':` |
|         - |  1285 | `			case '4': case '5': case '6': case '7': {` |
|         - |  1286 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|         - |  1287 | `				 * warns and wraps to the low byte, matching php 8. */` |
|        50 |  1288 | `				int c = 0;` |
|         - |  1289 | `				char cOut;` |
|       144 |  1290 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|       122 |  1291 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|        14 |  1292 | `						break;` |
|         - |  1293 | `					}` |
|        96 |  1294 | `					c = c * 8 + (zPtr[0] - '0');` |
|        49 |  1295 | `				}` |
|        50 |  1296 | `				if( c > 0xFF ){` |
|         - |  1297 | `					SyString sSeq;` |
|         3 |  1298 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|         3 |  1299 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1300 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|         3 |  1301 | `					c &= 0xFF;` |
|         1 |  1302 | `				}` |
|        50 |  1303 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|        50 |  1304 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|        50 |  1305 | `				n = (sxu32)(zPtr-zIn);` |
|        50 |  1306 | `				break;` |
|         - |  1307 | `			}` |
|       349 |  1308 | `			case 'x':` |
|      1047 |  1309 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|         - |  1310 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|       696 |  1311 | `					int c = SyHexToint(zIn[1]);` |
|         - |  1312 | `					char cOut;` |
|       696 |  1313 | `					n += sizeof(char);` |
|       696 |  1314 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|       692 |  1315 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|       692 |  1316 | `						n += sizeof(char);` |
|       345 |  1317 | `					}` |
|       696 |  1318 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|       696 |  1319 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       349 |  1320 | `				}else{` |
|         - |  1321 | `					/* Not an escape: keep the backslash, as php does */` |
|         5 |  1322 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|         - |  1323 | `				}` |
|       700 |  1324 | `				break;` |
|         9 |  1325 | `			case 'u':` |
|        18 |  1326 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|        22 |  1327 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|         - |  1328 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|         - |  1329 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|         - |  1330 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|         - |  1331 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|         - |  1332 | `					 * followed by {$...} curly interpolation. */` |
|        15 |  1333 | `					sxu32 nCp = 0;` |
|        15 |  1334 | `					zPtr = &zIn[2];` |
|        59 |  1335 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|        46 |  1336 | `						if( nCp <= 0x10FFFF ){` |
|         - |  1337 | `							/* stop accumulating once out of range: keeps a long` |
|         - |  1338 | `							 * digit run from wrapping sxu32 */` |
|        46 |  1339 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|        22 |  1340 | `						}` |
|        46 |  1341 | `						zPtr++;` |
|         2 |  1342 | `					}` |
|        15 |  1343 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|         - |  1344 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|         - |  1345 | `						 * malformed sequence so later errors are still reported. */` |
|         3 |  1346 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1347 | `							"Invalid UTF-8 codepoint escape sequence");` |
|         3 |  1348 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1349 | `							return SXERR_ABORT;` |
|         - |  1350 | `						}` |
|         3 |  1351 | `						n = (sxu32)(zPtr-zIn);` |
|         3 |  1352 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|         3 |  1353 | `							n += sizeof(char);` |
|         1 |  1354 | `						}` |
|         3 |  1355 | `						break;` |
|         - |  1356 | `					}` |
|        12 |  1357 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|        12 |  1358 | `					if( nCp > 0x10FFFF ){` |
|         3 |  1359 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1360 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|         3 |  1361 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1362 | `							return SXERR_ABORT;` |
|         - |  1363 | `						}` |
|         3 |  1364 | `						break;` |
|         - |  1365 | `					}` |
|         - |  1366 | `					{` |
|         - |  1367 | `						char zUtf[4];` |
|         9 |  1368 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|         9 |  1369 | `						SX_WRITE_UTF8(zOut,nCp);` |
|         9 |  1370 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|         - |  1371 | `					}` |
|         5 |  1372 | `				}else{` |
|         - |  1373 | `					/* Not an escape: keep the backslash, as php does */` |
|         7 |  1374 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|         - |  1375 | `				}` |
|        15 |  1376 | `				break;` |
|        16 |  1377 | `			default:` |
|         - |  1378 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|         - |  1379 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|         - |  1380 | `				 * in the source buffer — one batched append. */` |
|        33 |  1381 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|        32 |  1382 | `				break;` |
|         - |  1383 | `			}` |
|         - |  1384 | `			/* Advance the stream cursor */` |
|     40187 |  1385 | `			zIn += n;` |
|     40187 |  1386 | `			continue;` |
|         - |  1387 | `		}` |
|      2599 |  1388 | `		if( zIn[0] == '{' ){` |
|         - |  1389 | `			/* Curly syntax */` |
|         - |  1390 | `			const char *zExpr;` |
|       133 |  1391 | `			sxi32 iNest = 1;` |
|       133 |  1392 | `			zIn++;` |
|       133 |  1393 | `			zExpr = zIn;` |
|         - |  1394 | `			/* Synchronize with the next closing curly braces */` |
|      1311 |  1395 | `			while( zIn < zEnd ){` |
|      1311 |  1396 | `				if( zIn[0] == '{' ){` |
|         - |  1397 | `					/* Increment nesting level */` |
|         3 |  1398 | `					iNest++;` |
|      1310 |  1399 | `				}else if(zIn[0] == '}' ){` |
|         - |  1400 | `					/* Decrement nesting level */` |
|       135 |  1401 | `					iNest--;` |
|       135 |  1402 | `					if( iNest <= 0 ){` |
|       133 |  1403 | `						break;` |
|         - |  1404 | `					}` |
|         1 |  1405 | `				}` |
|      1181 |  1406 | `				zIn++;` |
|         3 |  1407 | `			}` |
|         - |  1408 | `			/* Process the expression */` |
|       133 |  1409 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|       133 |  1410 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1411 | `				return SXERR_ABORT;` |
|         - |  1412 | `			}` |
|       133 |  1413 | `			if( rc != SXERR_EMPTY ){` |
|       133 |  1414 | `				++iCons;` |
|        65 |  1415 | `			}` |
|       133 |  1416 | `			if( zIn < zEnd ){` |
|         - |  1417 | `				/* Jump the trailing curly */` |
|       133 |  1418 | `				zIn++;` |
|        65 |  1419 | `			}` |
|        68 |  1420 | `		}else{` |
|         - |  1421 | `			/* Simple syntax */` |
|      2469 |  1422 | `			const char *zExpr = zIn;` |
|         - |  1423 | `			/* Assemble variable name */` |
|      1257 |  1424 | `			for(;;){` |
|         - |  1425 | `				/* Jump leading dollars */` |
|      4983 |  1426 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2469 |  1427 | `					zIn++;` |
|         5 |  1428 | `				}` |
|      1257 |  1429 | `				for(;;){` |
|     12926 |  1430 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9155 |  1431 | `						zIn++;` |
|         5 |  1432 | `					}` |
|      2519 |  1433 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1434 | `						/* UTF-8 stream */` |
|       ! 0 |  1435 | `						zIn++;` |
|       ! 0 |  1436 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1437 | `							zIn++;` |
|       ! 0 |  1438 | `						}` |
|       ! 0 |  1439 | `						continue;` |
|         - |  1440 | `					}` |
|      2519 |  1441 | `					break;` |
|       ! 0 |  1442 | `				}` |
|      2519 |  1443 | `				if( zIn >= zEnd ){` |
|       263 |  1444 | `					break;` |
|         - |  1445 | `				}` |
|      2261 |  1446 | `				if( zIn[0] == '[' ){` |
|        12 |  1447 | `					sxi32 iSquare = 1;` |
|        12 |  1448 | `					zIn++;` |
|        28 |  1449 | `					while( zIn < zEnd ){` |
|        28 |  1450 | `						if( zIn[0] == '[' ){` |
|       ! 0 |  1451 | `							iSquare++;` |
|        28 |  1452 | `						}else if (zIn[0] == ']' ){` |
|        12 |  1453 | `							iSquare--;` |
|        12 |  1454 | `							if( iSquare <= 0 ){` |
|        12 |  1455 | `								break;` |
|         - |  1456 | `							}` |
|       ! 0 |  1457 | `						}` |
|        18 |  1458 | `						zIn++;` |
|         2 |  1459 | `					}` |
|        12 |  1460 | `					if( zIn < zEnd ){` |
|        12 |  1461 | `						zIn++;` |
|         5 |  1462 | `					}` |
|        12 |  1463 | `					break;` |
|      2251 |  1464 | `				}else if(zIn[0] == '{' ){` |
|         6 |  1465 | `					sxi32 iCurly = 1;` |
|         6 |  1466 | `					zIn++;` |
|        18 |  1467 | `					while( zIn < zEnd ){` |
|        16 |  1468 | `						if( zIn[0] == '{' ){` |
|       ! 0 |  1469 | `							iCurly++;` |
|        16 |  1470 | `						}else if (zIn[0] == '}' ){` |
|         3 |  1471 | `							iCurly--;` |
|         3 |  1472 | `							if( iCurly <= 0 ){` |
|         3 |  1473 | `								break;` |
|         - |  1474 | `							}` |
|       ! 0 |  1475 | `						}` |
|        14 |  1476 | `						zIn++;` |
|         2 |  1477 | `					}` |
|         6 |  1478 | `					if( zIn < zEnd ){` |
|         3 |  1479 | `						zIn++;` |
|         1 |  1480 | `					}` |
|         6 |  1481 | `					break;` |
|      2247 |  1482 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1483 | `					/* Member access operator '->' */` |
|        53 |  1484 | `					zIn += 2;` |
|      2222 |  1485 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1486 | `					/* Static member access operator '::' */` |
|       ! 0 |  1487 | `					zIn += 2;` |
|       ! 0 |  1488 | `				}else{` |
|      1101 |  1489 | `					break;` |
|         - |  1490 | `				}` |
|         3 |  1491 | `			}` |
|         - |  1492 | `			/*` |
|         - |  1493 | `			 * "$a[name]" — php's SIMPLE syntax takes an unquoted subscript as the string key` |
|         - |  1494 | `			 * 'name', never as a constant. PH7 handed "$a[name]" straight to the expression` |
|         - |  1495 | `			 * compiler, where the bare word only resolved because an unknown constant used to` |
|         - |  1496 | `			 * fall back to its own name as a string. With undefined constants now a real` |
|         - |  1497 | `			 * Error, quote the key here so the simple syntax keeps meaning what php means.` |
|         - |  1498 | `			 * A numeric ($a[0]) or variable ($a[$k]) subscript is already unambiguous.` |
|         - |  1499 | `			 */` |
|         - |  1500 | `			{` |
|      2469 |  1501 | `				const char *zBr = zExpr;` |
|     14201 |  1502 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     11737 |  1503 | `					zBr++;` |
|         5 |  1504 | `				}` |
|      2469 |  1505 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
|        12 |  1506 | `					const char *zKey = &zBr[1];` |
|        12 |  1507 | `					const char *zKeyEnd = &zIn[-1];` |
|        12 |  1508 | `					const char *zScan = zKey;` |
|        12 |  1509 | `					int bBare = (zKey < zKeyEnd) && !SyisDigit(zKey[0]);` |
|        20 |  1510 | `					while( bBare && zScan < zKeyEnd ){` |
|         9 |  1511 | `						if( !SyisAlphaNum(zScan[0]) && zScan[0] != '_' ){` |
|       ! 0 |  1512 | `							bBare = 0;` |
|       ! 0 |  1513 | `						}` |
|         9 |  1514 | `						zScan++;` |
|         1 |  1515 | `					}` |
|        12 |  1516 | `					if( bBare ){` |
|         - |  1517 | `						SyBlob sSub;` |
|         3 |  1518 | `						SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|         3 |  1519 | `						SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|         3 |  1520 | `						SyBlobAppend(&sSub,"['",2);` |
|         3 |  1521 | `						SyBlobAppend(&sSub,zKey,(sxu32)(zKeyEnd - zKey));` |
|         3 |  1522 | `						SyBlobAppend(&sSub,"']",2);` |
|         4 |  1523 | `						rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|         2 |  1524 | `							(const char *)SyBlobData(&sSub),` |
|         2 |  1525 | `							(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|         3 |  1526 | `						SyBlobRelease(&sSub);` |
|         3 |  1527 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1528 | `							return SXERR_ABORT;` |
|         - |  1529 | `						}` |
|         3 |  1530 | `						if( rc != SXERR_EMPTY ){` |
|         3 |  1531 | `							++iCons;` |
|         1 |  1532 | `						}` |
|         3 |  1533 | `						pObj = 0;` |
|         3 |  1534 | `						continue;` |
|         - |  1535 | `					}` |
|         4 |  1536 | `				}` |
|         - |  1537 | `			}` |
|         - |  1538 | `			/*` |
|         - |  1539 | `			 * "${name}" is php's DEPRECATED (8.2) spelling of the variable $name — NOT an` |
|         - |  1540 | `			 * expression. PH7 handed the whole "${name}" to the expression compiler, whose` |
|         - |  1541 | ``			 * `${expr}` (variable-variable) rule evaluated the bare word `name`; that only`` |
|         - |  1542 | `			 * appeared to work while an unknown bare word fell back to its own name as a` |
|         - |  1543 | `			 * string. Now that an undefined constant is a real Error, rewrite the simple` |
|         - |  1544 | `			 * form to the variable it means. "${$x}" keeps the variable-variable meaning.` |
|         - |  1545 | `			 */` |
|      2462 |  1546 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' && zIn[-1] == '}'` |
|         8 |  1547 | `				&& zExpr[2] != '$' ){` |
|         3 |  1548 | `				const char *zName = &zExpr[2];` |
|         3 |  1549 | `				const char *zStop = &zIn[-1];` |
|         3 |  1550 | `				const char *zScan = zName;` |
|        12 |  1551 | `				while( zScan < zStop && (SyisAlphaNum(zScan[0]) \|\| zScan[0] == '_') ){` |
|         9 |  1552 | `					zScan++;` |
|         1 |  1553 | `				}` |
|         3 |  1554 | `				if( zScan == zStop && zName < zStop ){` |
|         - |  1555 | `					SyBlob sVar;` |
|         3 |  1556 | `					PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pGen->pIn->nLine,` |
|         - |  1557 | `						"Using ${var} in strings is deprecated, use {$var} instead");` |
|         3 |  1558 | `					SyBlobInit(&sVar,&pGen->pVm->sAllocator);` |
|         3 |  1559 | `					SyBlobAppend(&sVar,"$",1);` |
|         3 |  1560 | `					SyBlobAppend(&sVar,zName,(sxu32)(zStop - zName));` |
|         - |  1561 | `					/* The scanner reads one byte PAST the length it is given, so the rewritten` |
|         - |  1562 | `					 * source has to be NUL-terminated: in the ordinary path the byte after the` |
|         - |  1563 | `					 * expression is the string's own closing quote, which stops an identifier,` |
|         - |  1564 | `					 * but here it is whatever the allocator left after the blob -- and an` |
|         - |  1565 | `					 * identifier byte there silently EXTENDS the variable name. */` |
|         3 |  1566 | `					SyBlobNullAppend(&sVar);` |
|         4 |  1567 | `					rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|         2 |  1568 | `						(const char *)SyBlobData(&sVar),` |
|         2 |  1569 | `						(const char *)SyBlobData(&sVar) + SyBlobLength(&sVar));` |
|         3 |  1570 | `					SyBlobRelease(&sVar);` |
|         3 |  1571 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  1572 | `						return SXERR_ABORT;` |
|         - |  1573 | `					}` |
|         3 |  1574 | `					if( rc != SXERR_EMPTY ){` |
|         3 |  1575 | `						++iCons;` |
|         1 |  1576 | `					}` |
|         3 |  1577 | `					pObj = 0;` |
|         3 |  1578 | `					continue;` |
|         - |  1579 | `				}` |
|       ! 0 |  1580 | `			}` |
|         - |  1581 | `			/* Process the expression */` |
|      2465 |  1582 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2465 |  1583 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1584 | `				return SXERR_ABORT;` |
|         - |  1585 | `			}` |
|      2465 |  1586 | `			if( rc != SXERR_EMPTY ){` |
|      2463 |  1587 | `				++iCons;` |
|      1229 |  1588 | `			}` |
|         - |  1589 | `		}` |
|         - |  1590 | `		/* Invalidate the previously used constant */` |
|      2595 |  1591 | `		pObj = 0;` |
|         5 |  1592 | `	}/*for(;;)*/` |
|    119015 |  1593 | `	if( iCons > 1 ){` |
|         - |  1594 | `		/* Concatenate all compiled constants */` |
|      1873 |  1595 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       934 |  1596 | `	}` |
|         - |  1597 | `	/* Node successfully compiled */` |
|    119015 |  1598 | `	return SXRET_OK;` |
|     59713 |  1599 | `}` |
|         - |  1600 | `/*` |
|         - |  1601 | ` * Compile a double quoted string.` |
|         - |  1602 | ` *  See the block-comment above for more information.` |
|         - |  1603 | ` */` |
|    119354 |  1604 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1605 | `{` |
|         - |  1606 | `	sxi32 rc;` |
|    119359 |  1607 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     59677 |  1608 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1609 | `	/* Compilation result */` |
|    119359 |  1610 | `	return rc;` |
|         5 |  1611 | `}` |
|         - |  1612 | `/*` |
|         - |  1613 | ` * Compile a Heredoc string.` |
|         - |  1614 | ` *  See the block-comment above for more information.` |
|         - |  1615 | ` */` |
|        66 |  1616 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 |  1617 | `{` |
|         - |  1618 | `	SyString sOrig, sStripped;` |
|         - |  1619 | `	sxi32 rc;` |
|        70 |  1620 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        70 |  1621 | `	if( rc != SXRET_OK ){` |
|         6 |  1622 | `		return rc;` |
|         - |  1623 | `	}` |
|         - |  1624 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|         - |  1625 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|         - |  1626 | `	 * Restore before returning so downstream code that references pIn is` |
|         - |  1627 | `	 * unaffected, including on the error path. */` |
|        65 |  1628 | `	sOrig = pGen->pIn->sData;` |
|        65 |  1629 | `	pGen->pIn->sData = sStripped;` |
|        65 |  1630 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|        65 |  1631 | `	pGen->pIn->sData = sOrig;` |
|        31 |  1632 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        65 |  1633 | `	return rc;` |
|        37 |  1634 | `}` |
|         - |  1635 | `/*` |
|         - |  1636 | ` * Compile an array entry whether it is a key or a value.` |
|         - |  1637 | ` *  Notes on array entries.` |
|         - |  1638 | ` *  According to the PHP language reference manual` |
|         - |  1639 | ` *  An array can be created by the array() language construct.` |
|         - |  1640 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|         - |  1641 | ` *  array(  key =>  value` |
|         - |  1642 | ` *    , ...` |
|         - |  1643 | ` *    )` |
|         - |  1644 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|         - |  1645 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|         - |  1646 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|         - |  1647 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|         - |  1648 | ` *  contain integer and string indices.` |
|         - |  1649 | ` *  A value can be any PHP type.` |
|         - |  1650 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|         - |  1651 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|         - |  1652 | ` *  is specified, that value will be overwritten.` |
|         - |  1653 | ` */` |
|   1200012 |  1654 | `static sxi32 GenStateCompileArrayEntry(` |
|         - |  1655 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1656 | `	SyToken *pIn,        /* Token stream */` |
|         - |  1657 | `	SyToken *pEnd,       /* End of the token stream */` |
|         - |  1658 | `	sxi32 iFlags,        /* Compilation flags */` |
|         - |  1659 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|         - |  1660 | `	)` |
|         5 |  1661 | `{` |
|         - |  1662 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1663 | `	sxi32 rc;` |
|         - |  1664 | `	/* Swap token stream */` |
|   1200017 |  1665 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1666 | `	/* Compile the expression*/` |
|   1200017 |  1667 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1668 | `	/* Restore token stream */` |
|   1200017 |  1669 | `	RE_SWAP_DELIMITER(pGen);` |
|   1200017 |  1670 | `	return rc;` |
|         5 |  1671 | `}` |
|         - |  1672 | `/*` |
|         - |  1673 | ` * Expression tree validator callback for the 'array' language construct.` |
|         - |  1674 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1675 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1676 | ` * error message.` |
|         - |  1677 | ` * See the routine responible of compiling the array language construct` |
|         - |  1678 | ` * for more inforation.` |
|         - |  1679 | ` */` |
|        36 |  1680 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  1681 | `{` |
|        41 |  1682 | `	sxi32 rc = SXRET_OK;` |
|        41 |  1683 | `	if( pRoot->pOp ){` |
|        14 |  1684 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|        12 |  1685 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|        16 |  1686 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1687 | `			/* Unexpected expression */` |
|        13 |  1688 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|        13 |  1689 | `			if( rc != SXERR_ABORT ){` |
|        13 |  1690 | `				rc = SXERR_INVALID;` |
|         5 |  1691 | `			}` |
|         9 |  1692 | `		}` |
|        31 |  1693 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1694 | `		/* Unexpected expression */` |
|         3 |  1695 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|         3 |  1696 | `		if( rc != SXERR_ABORT ){` |
|         3 |  1697 | `			rc = SXERR_INVALID;` |
|         1 |  1698 | `		}` |
|         1 |  1699 | `	}` |
|        41 |  1700 | `	return rc;` |
|         5 |  1701 | `}` |
|         - |  1702 | `/*` |
|         - |  1703 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|         - |  1704 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|         - |  1705 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|         - |  1706 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|         - |  1707 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|         - |  1708 | ` */` |
|   1217784 |  1709 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1710 | `{` |
|   1217789 |  1711 | `	SyToken *pCur = pStart;` |
|   1217789 |  1712 | `	sxi32 iNest = 0;` |
|   3224531 |  1713 | `	while( pCur < pEnd ){` |
|   2417029 |  1714 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    410283 |  1715 | `			return pCur;` |
|         - |  1716 | `		}` |
|         - |  1717 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1718 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1719 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1720 | `		 */` |
|   2006751 |  1721 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     22979 |  1722 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     22979 |  1723 | `			SyToken *pFn = pCur;` |
|     22974 |  1724 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1725 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1726 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1727 | `				pFn = &pCur[1];` |
|       ! 0 |  1728 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1729 | `			}` |
|     22979 |  1730 | `			if( nKw == PH7_TKWRD_FN ){` |
|         5 |  1731 | `				pCur = pFn + 1; /* past 'fn' */` |
|         5 |  1732 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  1733 | `					pCur++;` |
|       ! 0 |  1734 | `				}` |
|         5 |  1735 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         5 |  1736 | `					pCur++;` |
|         5 |  1737 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1738 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         5 |  1739 | `					if( pCur < pEnd ){` |
|         5 |  1740 | `						pCur++;` |
|         2 |  1741 | `					}` |
|         2 |  1742 | `				}` |
|         5 |  1743 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|       ! 0 |  1744 | `					pCur++;` |
|       ! 0 |  1745 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|       ! 0 |  1746 | `						&& pCur->sData.nByte == 1` |
|       ! 0 |  1747 | `						&& pCur->sData.zString[0] == '?' ){` |
|       ! 0 |  1748 | `						pCur++;` |
|       ! 0 |  1749 | `					}` |
|       ! 0 |  1750 | `					if( pCur < pEnd` |
|       ! 0 |  1751 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  1752 | `						pCur++;` |
|       ! 0 |  1753 | `					}` |
|       ! 0 |  1754 | `				}` |
|         - |  1755 | `				/* The rest of the entry is the arrow-function body — no outer` |
|         - |  1756 | `				 * key to extract. */` |
|         5 |  1757 | `				return pEnd;` |
|         - |  1758 | `			}` |
|         - |  1759 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|         - |  1760 | `			 * entry separator. Skip past the full match span. */` |
|     22975 |  1761 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|         3 |  1762 | `				pCur++; /* past 'match' */` |
|         3 |  1763 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         3 |  1764 | `					pCur++;` |
|         3 |  1765 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1766 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         3 |  1767 | `					if( pCur < pEnd ){` |
|         3 |  1768 | `						pCur++;` |
|         1 |  1769 | `					}` |
|         1 |  1770 | `				}` |
|         3 |  1771 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|         3 |  1772 | `					pCur++;` |
|         3 |  1773 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1774 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|         3 |  1775 | `					if( pCur < pEnd ){` |
|         3 |  1776 | `						pCur++;` |
|         1 |  1777 | `					}` |
|         1 |  1778 | `				}` |
|         3 |  1779 | `				continue;` |
|         - |  1780 | `			}` |
|     11484 |  1781 | `		}` |
|   2006745 |  1782 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     53901 |  1783 | `			iNest++;` |
|   1979797 |  1784 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1785 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1786 | `			 * parser will shortly detect any syntax error. */` |
|     53901 |  1787 | `			iNest--;` |
|     26948 |  1788 | `		}` |
|   2006745 |  1789 | `		pCur++;` |
|         5 |  1790 | `	}` |
|    807507 |  1791 | `	return pEnd;` |
|    608897 |  1792 | `}` |
|         - |  1793 | `/*` |
|         - |  1794 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1795 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1796 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1797 | ` */` |
|    587234 |  1798 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1799 | `{` |
|         - |  1800 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1801 | `	SyToken *pKey,*pCur;` |
|    587239 |  1802 | `	sxi32 iEmitRef = 0;` |
|    587239 |  1803 | `	sxi32 iSpread = 0;` |
|    587239 |  1804 | `	sxi32 nPair = 0;` |
|         - |  1805 | `	sxi32 rc;` |
|    587239 |  1806 | `	xValidator = 0;` |
|    753330 |  1807 | `	for(;;){` |
|         - |  1808 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|         - |  1809 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|         - |  1810 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|         - |  1811 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|    459713 |  1812 | `		{` |
|   1506665 |  1813 | `			int nSkip = 0;` |
|   2190231 |  1814 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    683571 |  1815 | `				nSkip++;` |
|    683571 |  1816 | `				pGen->pIn++;` |
|         5 |  1817 | `			}` |
|   1506665 |  1818 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|       ! 0 |  1819 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|         - |  1820 | `					"Cannot use empty array elements in arrays");` |
|       ! 0 |  1821 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1822 | `					return SXERR_ABORT;` |
|         - |  1823 | `				}` |
|       ! 0 |  1824 | `				return SXRET_OK;` |
|         - |  1825 | `			}` |
|         - |  1826 | `		}` |
|   1506665 |  1827 | `		pCur = pGen->pIn;` |
|   1506665 |  1828 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1829 | `			/* No more entry to process */` |
|    587221 |  1830 | `			break;` |
|         - |  1831 | `		}` |
|    919449 |  1832 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1833 | `			continue;` |
|         - |  1834 | `		}` |
|         - |  1835 | `		/* Compile the key if available */` |
|    919449 |  1836 | `		pKey = pCur;` |
|    919449 |  1837 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|    919449 |  1838 | `		rc = SXERR_EMPTY;` |
|    919449 |  1839 | `		if( pCur < pGen->pIn ){` |
|    280319 |  1840 | `			if( pKey == pCur ){` |
|         - |  1841 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|         - |  1842 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|         - |  1843 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|         - |  1844 | `				 * IS found here, so control never reached it.)` |
|         - |  1845 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|         3 |  1846 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|         - |  1847 | `					? "\"]\"" : "\")\"";` |
|         3 |  1848 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|         3 |  1849 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1850 | `					return SXERR_ABORT;` |
|         - |  1851 | `				}` |
|         3 |  1852 | `				return SXRET_OK;` |
|         - |  1853 | `			}` |
|    280317 |  1854 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1855 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|         - |  1856 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|         - |  1857 | `				 * makes the helper reach for the token past this entry's slice. */` |
|        13 |  1858 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        13 |  1859 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1860 | `					return SXERR_ABORT;` |
|         - |  1861 | `				}` |
|        13 |  1862 | `				return SXRET_OK;` |
|         - |  1863 | `			}` |
|         - |  1864 | `			/* Compile the expression holding the key */` |
|    280307 |  1865 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1866 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    280307 |  1867 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1868 | `				return SXERR_ABORT;` |
|         - |  1869 | `			}` |
|    280307 |  1870 | `			pCur++; /* Jump the '=>' operator */` |
|    140156 |  1871 | `		}else{` |
|         - |  1872 | `			/* Reset back the cursor and point to the entry value */` |
|    639135 |  1873 | `			pCur = pKey;` |
|         - |  1874 | `		}` |
|    919437 |  1875 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1876 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|         - |  1877 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|    639135 |  1878 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|    319565 |  1879 | `		}` |
|    919437 |  1880 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|         - |  1881 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|        45 |  1882 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|        45 |  1883 | `			iEmitRef = 1;` |
|        45 |  1884 | `			pCur++; /* Jump the '&' token */` |
|        45 |  1885 | `			if( pCur >= pGen->pIn ){` |
|         - |  1886 | `				/* Missing value */` |
|         3 |  1887 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|         3 |  1888 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1889 | `					return SXERR_ABORT;` |
|         - |  1890 | `				}` |
|         3 |  1891 | `				return SXRET_OK;` |
|         - |  1892 | `			}` |
|        19 |  1893 | `		}` |
|         - |  1894 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|         - |  1895 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|         - |  1896 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|         - |  1897 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|         - |  1898 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|    919435 |  1899 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|    919435 |  1900 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|         - |  1901 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|         - |  1902 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|         - |  1903 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|         - |  1904 | `			 * output is engine-portable. */` |
|         6 |  1905 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|         - |  1906 | `				"syntax error, unexpected token \"...\"");` |
|         6 |  1907 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1908 | `				return SXERR_ABORT;` |
|         - |  1909 | `			}` |
|         6 |  1910 | `			return SXRET_OK;` |
|         - |  1911 | `		}` |
|         - |  1912 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|         - |  1913 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|         - |  1914 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|         - |  1915 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|         - |  1916 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|   1379144 |  1917 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    459713 |  1918 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1919 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    459713 |  1920 | `			xValidator);` |
|    919431 |  1921 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1922 | `			return SXERR_ABORT;` |
|         - |  1923 | `		}` |
|    919431 |  1924 | `		if( iSpread ){` |
|         - |  1925 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        68 |  1926 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|    919398 |  1927 | `		}else if( iEmitRef ){` |
|         - |  1928 | `			/* Emit the load reference instruction */` |
|        41 |  1929 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1930 | `		}` |
|    919431 |  1931 | `		xValidator = 0;` |
|    919431 |  1932 | `		iEmitRef = 0;` |
|    919431 |  1933 | `		iSpread = 0;` |
|    919431 |  1934 | `		nPair++;` |
|         5 |  1935 | `	}` |
|         - |  1936 | `	/* Emit the load map instruction */` |
|    587221 |  1937 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1938 | `	/* Node successfully compiled */` |
|    587221 |  1939 | `	return SXRET_OK;` |
|    293622 |  1940 | `}` |
|         - |  1941 | `/*` |
|         - |  1942 | ` * Compile the 'array' language construct.` |
|         - |  1943 | ` *	 According to the PHP language reference manual` |
|         - |  1944 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1945 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1946 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1947 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1948 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1949 | ` */` |
|    371610 |  1950 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1951 | `{` |
|         - |  1952 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    371615 |  1953 | `	pGen->pIn += 2;` |
|    371615 |  1954 | `	pGen->pEnd--;` |
|    185805 |  1955 | `	SXUNUSED(iCompileFlag);` |
|    371615 |  1956 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1957 | `}` |
|         - |  1958 | `/*` |
|         - |  1959 | ` * Compile the PHP 8.5 clone(...) call form:` |
|         - |  1960 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|         - |  1961 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|         - |  1962 | ` *                                              property updates as scope-aware writes` |
|         - |  1963 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|         - |  1964 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|         - |  1965 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|         - |  1966 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|         - |  1967 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|         - |  1968 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|         - |  1969 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|         - |  1970 | ` */` |
|        22 |  1971 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  1972 | `{` |
|         - |  1973 | `	SyToken *pIn,*pEnd,*pNext;` |
|        24 |  1974 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|        24 |  1975 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|        24 |  1976 | `	int nArg = 0;` |
|         - |  1977 | `	sxi32 rc;` |
|        11 |  1978 | `	SXUNUSED(iCompileFlag);` |
|         - |  1979 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|        24 |  1980 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|        24 |  1981 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|         - |  1982 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|        24 |  1983 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       ! 0 |  1984 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  1985 | `			"clone(...) first-class callable form is not yet supported");` |
|         - |  1986 | `	}` |
|         - |  1987 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|        62 |  1988 | `	while( pIn < pEnd ){` |
|        40 |  1989 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|        40 |  1990 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|       ! 0 |  1991 | `			break;` |
|         - |  1992 | `		}` |
|        40 |  1993 | `		pArgStart = pIn;` |
|        40 |  1994 | `		pArgEnd   = pNext;` |
|         - |  1995 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|         - |  1996 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|        38 |  1997 | `		if( (pArgEnd - pArgStart) >= 2` |
|        37 |  1998 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        23 |  1999 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|         5 |  2000 | `			pName = pArgStart;` |
|         5 |  2001 | `			pArgStart += 2;` |
|         2 |  2002 | `		}` |
|        40 |  2003 | `		if( pName ){` |
|         - |  2004 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|         - |  2005 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|         4 |  2006 | `			if( pName->sData.nByte == sizeof("object")-1` |
|         4 |  2007 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|         3 |  2008 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|         4 |  2009 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|         3 |  2010 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|         3 |  2011 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         2 |  2012 | `			}else{` |
|       ! 0 |  2013 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|       ! 0 |  2014 | `					"Unknown named parameter $%z",&pName->sData);` |
|         1 |  2015 | `			}` |
|        38 |  2016 | `		}else if( nArg == 0 ){` |
|        22 |  2017 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        25 |  2018 | `		}else if( nArg == 1 ){` |
|        15 |  2019 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         8 |  2020 | `		}else{` |
|       ! 0 |  2021 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|         - |  2022 | `				"clone() expects at most 2 arguments");` |
|         - |  2023 | `		}` |
|        40 |  2024 | `		nArg++;` |
|        40 |  2025 | `		pIn = pNext;` |
|        40 |  2026 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 |  2027 | `			pIn++; /* step over the argument separator */` |
|         8 |  2028 | `		}` |
|         2 |  2029 | `	}` |
|        24 |  2030 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|       ! 0 |  2031 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  2032 | `			"clone() expects at least 1 argument, 0 given");` |
|         - |  2033 | `	}` |
|         - |  2034 | `	/* Object argument -> clone (+ __clone()). */` |
|        24 |  2035 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        24 |  2036 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2037 | `		return SXERR_ABORT;` |
|         - |  2038 | `	}` |
|        24 |  2039 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|         - |  2040 | `	/* Property updates (evaluated after __clone runs). */` |
|        24 |  2041 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|        17 |  2042 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        17 |  2043 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2044 | `			return SXERR_ABORT;` |
|         - |  2045 | `		}` |
|        17 |  2046 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|         8 |  2047 | `	}` |
|        24 |  2048 | `	return SXRET_OK;` |
|        13 |  2049 | `}` |
|         - |  2050 | `/*` |
|         - |  2051 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|         - |  2052 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|         - |  2053 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|         - |  2054 | ` */` |
|    215624 |  2055 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2056 | `{` |
|         - |  2057 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    215629 |  2058 | `	pGen->pIn++;` |
|    215629 |  2059 | `	pGen->pEnd--;` |
|    107812 |  2060 | `	SXUNUSED(iCompileFlag);` |
|    215629 |  2061 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  2062 | `}` |
|         - |  2063 | `/*` |
|         - |  2064 | ` * Expression tree validator callback for the 'list' language construct.` |
|         - |  2065 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  2066 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  2067 | ` * error message.` |
|         - |  2068 | ` * See the routine responible of compiling the list language construct` |
|         - |  2069 | ` * for more inforation.` |
|         - |  2070 | ` */` |
|       210 |  2071 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  2072 | `{` |
|       215 |  2073 | `	sxi32 rc = SXRET_OK;` |
|       215 |  2074 | `	if( pRoot->pOp ){` |
|         4 |  2075 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|         2 |  2076 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|         - |  2077 | `				/* Unexpected expression */` |
|       ! 0 |  2078 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2079 | `					"Assignments can only happen to writable values");` |
|       ! 0 |  2080 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 |  2081 | `					rc = SXERR_INVALID;` |
|       ! 0 |  2082 | `				}` |
|         1 |  2083 | `		}` |
|       213 |  2084 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  2085 | `		/* Unexpected expression */` |
|         6 |  2086 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2087 | `			"Assignments can only happen to writable values");` |
|         6 |  2088 | `		if( rc != SXERR_ABORT ){` |
|         6 |  2089 | `			rc = SXERR_INVALID;` |
|         2 |  2090 | `		}` |
|         2 |  2091 | `	}` |
|       215 |  2092 | `	return rc;` |
|         5 |  2093 | `}` |
|         - |  2094 | `/*` |
|         - |  2095 | ` * Compile the 'list' language construct.` |
|         - |  2096 | ` *  According to the PHP language reference` |
|         - |  2097 | ` *  list(): Assign variables as if they were an array.` |
|         - |  2098 | ` *  list() is used to assign a list of variables in one operation.` |
|         - |  2099 | ` *  Description` |
|         - |  2100 | ` *   array list (mixed $varname [, mixed $... ] )` |
|         - |  2101 | ` *   Like array(), this is not really a function, but a language construct.` |
|         - |  2102 | ` *   list() is used to assign a list of variables in one operation.` |
|         - |  2103 | ` *  Parameters` |
|         - |  2104 | ` *   $varname: A variable.` |
|         - |  2105 | ` *  Return Values` |
|         - |  2106 | ` *   The assigned array.` |
|         - |  2107 | ` */` |
|         - |  2108 | `/* Nested list entry recorded during first pass of list body compilation */` |
|         - |  2109 | `struct NestedListEntry {` |
|         - |  2110 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|         - |  2111 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|         - |  2112 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|         - |  2113 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|         - |  2114 | `};` |
|         - |  2115 | `/*` |
|         - |  2116 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|         - |  2117 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|         - |  2118 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|         - |  2119 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|         - |  2120 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|         - |  2121 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|         - |  2122 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|         - |  2123 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|         - |  2124 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|         - |  2125 | ` */` |
|        22 |  2126 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|         1 |  2127 | `{` |
|         - |  2128 | `	SyToken *pNext;` |
|         - |  2129 | `	sxi32 rc;` |
|        53 |  2130 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|         - |  2131 | `		SyToken *pArrow,*pTarget;` |
|         - |  2132 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|        31 |  2133 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|        31 |  2134 | `		pTarget = &pArrow[1];` |
|        31 |  2135 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|         - |  2136 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|         - |  2137 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|       ! 0 |  2138 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2139 | `				"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2140 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2141 | `		}` |
|         - |  2142 | `		/* DUP the source array (it is on the stack top) */` |
|        31 |  2143 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2144 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|        31 |  2145 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|        31 |  2146 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2147 | `			return SXERR_ABORT;` |
|         - |  2148 | `		}` |
|         - |  2149 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|         - |  2150 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|         - |  2151 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|         - |  2152 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|         - |  2153 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|         - |  2154 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|        31 |  2155 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|        31 |  2156 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|        28 |  2157 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|        15 |  2158 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|         - |  2159 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|         - |  2160 | `			 * Treat source[key] as the inner body's source, then drop the` |
|         - |  2161 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|         5 |  2162 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|         5 |  2163 | `			SyToken *pSavedIn = pGen->pIn;` |
|         5 |  2164 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         5 |  2165 | `			pGen->pIn = pTarget;` |
|         5 |  2166 | `			pGen->pEnd = pNext;` |
|         5 |  2167 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|         2 |  2168 | `			             : PH7_CompileList(&(*pGen),0);` |
|         5 |  2169 | `			pGen->pIn = pSavedIn;` |
|         5 |  2170 | `			pGen->pEnd = pSavedEnd;` |
|         5 |  2171 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2172 | `				return SXERR_ABORT;` |
|         - |  2173 | `			}` |
|         5 |  2174 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         3 |  2175 | `		}else{` |
|         - |  2176 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|         - |  2177 | `			 * is already on the stack as the value; compiling the target appends` |
|         - |  2178 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|         - |  2179 | `			 * assignment does. */` |
|         - |  2180 | `			VmInstr *pInstr;` |
|        27 |  2181 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|        27 |  2182 | `			sxi32 iP1 = 0, iP2 = 0;` |
|        27 |  2183 | `			void *p3 = 0;` |
|        27 |  2184 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|         - |  2185 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|        27 |  2186 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2187 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2188 | `			}` |
|        27 |  2189 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|        27 |  2190 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|         3 |  2191 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|        26 |  2192 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         3 |  2193 | `					iVmOp = PH7_OP_STORE_IDX;` |
|         3 |  2194 | `					iP1 = pInstr->iP1;` |
|         3 |  2195 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         2 |  2196 | `				}else{` |
|        23 |  2197 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|        23 |  2198 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - |  2199 | `				}` |
|        13 |  2200 | `			}` |
|        27 |  2201 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|         - |  2202 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|         - |  2203 | `			 * source array is back on top for the next entry. */` |
|        27 |  2204 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         - |  2205 | `		}` |
|        31 |  2206 | `		pGen->pIn = &pNext[1];` |
|         1 |  2207 | `	}` |
|        23 |  2208 | `	return SXRET_OK;` |
|        12 |  2209 | `}` |
|         - |  2210 | `/*` |
|         - |  2211 | ` * Shared body for list() and short list [...] compilation.` |
|         - |  2212 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|         - |  2213 | ` * the opening delimiter and before the closing delimiter.` |
|         - |  2214 | ` */` |
|       122 |  2215 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|         5 |  2216 | `{` |
|         - |  2217 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|         - |  2218 | `	SyToken *pNext;` |
|         - |  2219 | `	SyToken *pClassifyIn;` |
|       127 |  2220 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|         - |  2221 | `	sxi32 nExpr;` |
|         - |  2222 | `	sxi32 rc;` |
|         - |  2223 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|         - |  2224 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|         - |  2225 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|         - |  2226 | `	 * list. */` |
|       127 |  2227 | `	pClassifyIn = pGen->pIn;` |
|       367 |  2228 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       245 |  2229 | `		if( pGen->pIn >= pNext ){` |
|        13 |  2230 | `			nEmpty++;` |
|       239 |  2231 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|        31 |  2232 | `			nKeyed++;` |
|        16 |  2233 | `		}else{` |
|       203 |  2234 | `			nPositional++;` |
|         - |  2235 | `		}` |
|       245 |  2236 | `		pGen->pIn = &pNext[1];` |
|         5 |  2237 | `	}` |
|       127 |  2238 | `	pGen->pIn = pClassifyIn;` |
|       127 |  2239 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|       ! 0 |  2240 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2241 | `			"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2242 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2243 | `	}` |
|       127 |  2244 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|       ! 0 |  2245 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2246 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|       ! 0 |  2247 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2248 | `	}` |
|       127 |  2249 | `	if( nKeyed > 0 ){` |
|        23 |  2250 | `		return GenStateCompileKeyedListBody(pGen);` |
|         - |  2251 | `	}` |
|       105 |  2252 | `	nExpr = 0;` |
|       105 |  2253 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       315 |  2254 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       215 |  2255 | `		if( pGen->pIn < pNext ){` |
|         - |  2256 | `			/* Check for nested list() */` |
|       203 |  2257 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         3 |  2258 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  2259 | `				/* Record this nested list for post-processing */` |
|         3 |  2260 | `				SyToken *pListEnd = 0;` |
|         3 |  2261 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|         3 |  2262 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         1 |  2263 | `				}` |
|         3 |  2264 | `				if( pListEnd ){` |
|         - |  2265 | `					struct NestedListEntry sEntry;` |
|         3 |  2266 | `					sEntry.nIndex = nExpr;` |
|         3 |  2267 | `					sEntry.pStart = pGen->pIn;` |
|         3 |  2268 | `					sEntry.pEnd = pListEnd + 1;` |
|         3 |  2269 | `					sEntry.isShort = 0;` |
|         3 |  2270 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         1 |  2271 | `				}` |
|         - |  2272 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|         3 |  2273 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       202 |  2274 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  2275 | `				/* Nested short destructuring [...] */` |
|        13 |  2276 | `				SyToken *pBracketEnd = 0;` |
|        13 |  2277 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|        13 |  2278 | `				if( pBracketEnd ){` |
|         - |  2279 | `					struct NestedListEntry sEntry;` |
|        13 |  2280 | `					sEntry.nIndex = nExpr;` |
|        13 |  2281 | `					sEntry.pStart = pGen->pIn;` |
|        13 |  2282 | `					sEntry.pEnd = pBracketEnd + 1;` |
|        13 |  2283 | `					sEntry.isShort = 1;` |
|        13 |  2284 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         6 |  2285 | `				}` |
|         - |  2286 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        13 |  2287 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |  2288 | `			}else{` |
|         - |  2289 | `				/* Compile the expression holding the variable */` |
|       189 |  2290 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       189 |  2291 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  2292 | `					SySetRelease(&sNested);` |
|       ! 0 |  2293 | `					return SXRET_OK;` |
|         - |  2294 | `				}` |
|         - |  2295 | `			}` |
|       104 |  2296 | `		}else{` |
|         - |  2297 | `			/* Empty entry,load NULL */` |
|        13 |  2298 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|         - |  2299 | `		}` |
|       215 |  2300 | `		nExpr++;` |
|         - |  2301 | `		/* Advance the stream cursor */` |
|       215 |  2302 | `		pGen->pIn = &pNext[1];` |
|         5 |  2303 | `	}` |
|         - |  2304 | `	/* Emit the LOAD_LIST instruction */` |
|       105 |  2305 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|         - |  2306 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|         - |  2307 | `	 * For each nested entry, emit code to extract the sub-array` |
|         - |  2308 | `	 * at the corresponding index and recursively destructure it.` |
|         - |  2309 | `	 */` |
|       105 |  2310 | `	if( SySetUsed(&sNested) > 0 ){` |
|        13 |  2311 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|         - |  2312 | `		sxu32 i;` |
|        27 |  2313 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|        15 |  2314 | `			SyToken *pSavedIn = pGen->pIn;` |
|        15 |  2315 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         - |  2316 | `			ph7_value *pIdx;` |
|         - |  2317 | `			sxu32 nConstIdx;` |
|         - |  2318 | `			/* DUP the source array (it's on stack top) */` |
|        15 |  2319 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2320 | `			/* Push the integer index for this nested entry */` |
|        15 |  2321 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|        15 |  2322 | `			if( pIdx == 0 ){` |
|       ! 0 |  2323 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2324 | `				SySetRelease(&sNested);` |
|       ! 0 |  2325 | `				return SXERR_ABORT;` |
|         - |  2326 | `			}` |
|        15 |  2327 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|        15 |  2328 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|         - |  2329 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|         - |  2330 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|         - |  2331 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|         - |  2332 | `			 */` |
|        15 |  2333 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|         - |  2334 | `			/* Recursively compile the inner list */` |
|        15 |  2335 | `			pGen->pIn = apNested[i].pStart;` |
|        15 |  2336 | `			pGen->pEnd = apNested[i].pEnd;` |
|        15 |  2337 | `			if( apNested[i].isShort ){` |
|        13 |  2338 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  2339 | `			}else{` |
|         3 |  2340 | `				rc = PH7_CompileList(&(*pGen),0);` |
|         - |  2341 | `			}` |
|        15 |  2342 | `			pGen->pIn = pSavedIn;` |
|        15 |  2343 | `			pGen->pEnd = pSavedEnd;` |
|        15 |  2344 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2345 | `				SySetRelease(&sNested);` |
|       ! 0 |  2346 | `				return SXERR_ABORT;` |
|         - |  2347 | `			}` |
|         - |  2348 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|        15 |  2349 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         8 |  2350 | `		}` |
|         6 |  2351 | `	}` |
|       105 |  2352 | `	SySetRelease(&sNested);` |
|         - |  2353 | `	/* Node successfully compiled */` |
|       105 |  2354 | `	return SXRET_OK;` |
|        66 |  2355 | `}` |
|        40 |  2356 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2357 | `{` |
|         - |  2358 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|        45 |  2359 | `	pGen->pIn += 2;` |
|        45 |  2360 | `	pGen->pEnd--;` |
|        20 |  2361 | `	SXUNUSED(iCompileFlag);` |
|        45 |  2362 | `	return GenStateCompileListBody(pGen);` |
|         5 |  2363 | `}` |
|        82 |  2364 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2365 | `{` |
|         - |  2366 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        84 |  2367 | `	pGen->pIn++;` |
|        84 |  2368 | `	pGen->pEnd--;` |
|        41 |  2369 | `	SXUNUSED(iCompileFlag);` |
|        84 |  2370 | `	return GenStateCompileListBody(pGen);` |
|         2 |  2371 | `}` |
|         - |  2372 | `/* Forward declarations */` |
|         - |  2373 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|         - |  2374 | `static int GenStateIsReservedConstant(SyString *pName);` |
|         - |  2375 | `static int GenStateIsReadonly(SyToken *pTok);` |
|         - |  2376 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|         - |  2377 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|         - |  2378 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - |  2379 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|         - |  2380 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|         - |  2381 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|         - |  2382 | `/*` |
|         - |  2383 | ` * Compile an annoynmous function or a closure.` |
|         - |  2384 | ` * According to the PHP language reference` |
|         - |  2385 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  2386 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  2387 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|         - |  2388 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|         - |  2389 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|         - |  2390 | ` *  Example Anonymous function variable assignment example` |
|         - |  2391 | ` * <?php` |
|         - |  2392 | ` * $greet = function($name)` |
|         - |  2393 | ` * {` |
|         - |  2394 | ` *    printf("Hello %s\r\n", $name);` |
|         - |  2395 | ` * };` |
|         - |  2396 | ` * $greet('World');` |
|         - |  2397 | ` * $greet('PHP');` |
|         - |  2398 | ` * ?>` |
|         - |  2399 | ` * Note that the implementation of annoynmous function and closure under` |
|         - |  2400 | ` * PH7 is completely different from the one used by the zend engine.` |
|         - |  2401 | ` */` |
|       566 |  2402 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2403 | `{` |
|       571 |  2404 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2405 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2406 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2407 | `							  * one thread is allowed to compile the script.` |
|         - |  2408 | `						      */` |
|         - |  2409 | `	SyString sName;` |
|       571 |  2410 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2411 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2412 | `	sxu32 nKwLine;` |
|       571 |  2413 | `	sxi32 iFlags = 0;` |
|         - |  2414 | `	sxu32 nLen;` |
|         - |  2415 | `	sxi32 rc;` |
|       283 |  2416 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2417 |  |
|       571 |  2418 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       566 |  2419 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       571 |  2420 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2421 | `		/* Static closure: no $this auto-capture, bind refused */` |
|         9 |  2422 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         9 |  2423 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         4 |  2424 | `	}` |
|       571 |  2425 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       571 |  2426 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2427 | `		pGen->pIn++;` |
|       ! 0 |  2428 | `	}` |
|         - |  2429 | `	/* Generate a unique name */` |
|       571 |  2430 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2431 | `	/* Make sure the generated name is unique */` |
|       571 |  2432 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2433 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2434 | `	}` |
|       571 |  2435 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2436 | `	/* Compile the lambda body */` |
|       571 |  2437 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       571 |  2438 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2439 | `		return SXERR_ABORT;` |
|         - |  2440 | `	}` |
|       571 |  2441 | `	if( pAnnonFunc ){` |
|       571 |  2442 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2443 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2444 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       571 |  2445 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2446 | `			return SXERR_ABORT;` |
|         - |  2447 | `		}` |
|       283 |  2448 | `	}` |
|         - |  2449 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2450 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2451 | `	 * the handler wraps either in a Closure instance. */` |
|       571 |  2452 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2453 | `	/* Node successfully compiled */` |
|       571 |  2454 | `	return SXRET_OK;` |
|       288 |  2455 | `}` |
|         - |  2456 | `/*` |
|         - |  2457 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2458 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2459 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2460 | ` */` |
|       216 |  2461 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2462 | `	ph7_gen_state *pGen,` |
|         - |  2463 | `	ph7_vm_func *pFunc,` |
|         - |  2464 | `	const char *zName,` |
|         - |  2465 | `	sxu32 nByte,` |
|         - |  2466 | `	SyString *aShadow,` |
|         - |  2467 | `	sxu32 nShadow)` |
|         4 |  2468 | `{` |
|         - |  2469 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2470 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2471 | `	sxu32 n, nEnv;` |
|         - |  2472 | `	char *zDup;` |
|       220 |  2473 | `	if( nByte == 0 ){` |
|       ! 0 |  2474 | `		return SXRET_OK;` |
|         - |  2475 | `	}` |
|       216 |  2476 | `	if( nByte == sizeof("this")-1` |
|       118 |  2477 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         3 |  2478 | `		return SXRET_OK;` |
|         - |  2479 | `	}` |
|       272 |  2480 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       202 |  2481 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       197 |  2482 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       152 |  2483 | `			return SXRET_OK;` |
|         - |  2484 | `		}` |
|        29 |  2485 | `	}` |
|        67 |  2486 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        67 |  2487 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|        95 |  2488 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2489 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2490 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2491 | `			return SXRET_OK;` |
|         - |  2492 | `		}` |
|        15 |  2493 | `	}` |
|        65 |  2494 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        65 |  2495 | `	if( zDup == 0 ){` |
|       ! 0 |  2496 | `		return SXERR_ABORT;` |
|         - |  2497 | `	}` |
|        65 |  2498 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        65 |  2499 | `	sEnv.iFlags = 0;` |
|        65 |  2500 | `	sEnv.nIdx = SXU32_HIGH;` |
|        65 |  2501 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        65 |  2502 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        65 |  2503 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        65 |  2504 | `	return SXRET_OK;` |
|       112 |  2505 | `}` |
|         - |  2506 | `/*` |
|         - |  2507 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|         - |  2508 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|         - |  2509 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|         - |  2510 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|         - |  2511 | ` */` |
|       106 |  2512 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|         - |  2513 | `	ph7_gen_state *pGen,` |
|         - |  2514 | `	ph7_vm_func *pFunc,` |
|         - |  2515 | `	const char *zIn,` |
|         - |  2516 | `	const char *zEnd,` |
|         - |  2517 | `	SyString *aShadow,` |
|         - |  2518 | `	sxu32 nShadow)` |
|         2 |  2519 | `{` |
|         - |  2520 | `	sxi32 rc;` |
|       578 |  2521 | `	while( zIn < zEnd ){` |
|       472 |  2522 | `		if( zIn[0] == '\\' ){` |
|        13 |  2523 | `			zIn++;` |
|        13 |  2524 | `			if( zIn < zEnd ){` |
|        13 |  2525 | `				zIn++;` |
|         6 |  2526 | `			}` |
|        13 |  2527 | `			continue;` |
|         - |  2528 | `		}` |
|       458 |  2529 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        26 |  2530 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|        24 |  2531 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|         - |  2532 | `			const char *zName;` |
|        26 |  2533 | `			zIn++; /* skip '$' */` |
|        26 |  2534 | `			zName = zIn;` |
|        82 |  2535 | `			while( zIn < zEnd ){` |
|        76 |  2536 | `				unsigned char c = (unsigned char)zIn[0];` |
|        76 |  2537 | `				if( c >= 0xc0 ){` |
|       ! 0 |  2538 | `					zIn++;` |
|       ! 0 |  2539 | `					while( zIn < zEnd` |
|       ! 0 |  2540 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  2541 | `						zIn++;` |
|       ! 0 |  2542 | `					}` |
|       ! 0 |  2543 | `					continue;` |
|         - |  2544 | `				}` |
|        76 |  2545 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        20 |  2546 | `					break;` |
|         - |  2547 | `				}` |
|        58 |  2548 | `				zIn++;` |
|         2 |  2549 | `			}` |
|        26 |  2550 | `			if( zIn > zName ){` |
|        38 |  2551 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|        24 |  2552 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|        26 |  2553 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2554 | `					return SXERR_ABORT;` |
|         - |  2555 | `				}` |
|        12 |  2556 | `			}` |
|        26 |  2557 | `			continue;` |
|         - |  2558 | `		}` |
|       436 |  2559 | `		zIn++;` |
|         2 |  2560 | `	}` |
|       108 |  2561 | `	return SXRET_OK;` |
|        55 |  2562 | `}` |
|         - |  2563 | `/*` |
|         - |  2564 | ` * Scan the body token range of an arrow function for free-variable` |
|         - |  2565 | ` * references and record them in pFunc's closure environment. Handles:` |
|         - |  2566 | ` *   - plain $<id> pairs` |
|         - |  2567 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|         - |  2568 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|         - |  2569 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|         - |  2570 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|         - |  2571 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|         - |  2572 | ` *     are never mistakenly captured.` |
|         - |  2573 | ` */` |
|       492 |  2574 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2575 | `	ph7_gen_state *pGen,` |
|         - |  2576 | `	ph7_vm_func *pFunc,` |
|         - |  2577 | `	SyToken *pStart,` |
|         - |  2578 | `	SyToken *pEnd,` |
|         - |  2579 | `	SyString *aShadow,` |
|         - |  2580 | `	sxu32 nShadow)` |
|         5 |  2581 | `{` |
|       497 |  2582 | `	SyToken *pScan = pStart;` |
|         - |  2583 | `	sxi32 rc;` |
|      3335 |  2584 | `	while( pScan < pEnd ){` |
|      2843 |  2585 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       161 |  2586 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        53 |  2587 | `				pScan->sData.zString,` |
|       106 |  2588 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        53 |  2589 | `				aShadow,nShadow);` |
|       108 |  2590 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2591 | `				return SXERR_ABORT;` |
|         - |  2592 | `			}` |
|       108 |  2593 | `			pScan++;` |
|       108 |  2594 | `			continue;` |
|         - |  2595 | `		}` |
|      2737 |  2596 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        37 |  2597 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        37 |  2598 | `			SyToken *pFnKw = pScan;` |
|        34 |  2599 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2600 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         3 |  2601 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2602 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2603 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2604 | `			}` |
|        37 |  2605 | `			if( nKw == PH7_TKWRD_FN ){` |
|         - |  2606 | `				SyToken *pInnerSigStart;` |
|         - |  2607 | `				SyToken *pInnerSigEnd;` |
|         - |  2608 | `				SyToken *pInnerBodyEnd;` |
|         - |  2609 | `				SyString *aInnerShadow;` |
|         - |  2610 | `				sxu32 nInnerShadow;` |
|         - |  2611 | `				sxu32 nInnerParamMax;` |
|         - |  2612 | `				SyToken *p;` |
|         - |  2613 | `				int iNestInner;` |
|        26 |  2614 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|        26 |  2615 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2616 | `					pScan++;` |
|       ! 0 |  2617 | `				}` |
|        26 |  2618 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2619 | `					pScan++;` |
|       ! 0 |  2620 | `					continue;` |
|         - |  2621 | `				}` |
|        26 |  2622 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|        26 |  2623 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|         - |  2624 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|        26 |  2625 | `				if( pInnerSigEnd >= pEnd ){` |
|       ! 0 |  2626 | `					pScan = pEnd;` |
|       ! 0 |  2627 | `					continue;` |
|         - |  2628 | `				}` |
|         - |  2629 | `				/* Build an augmented shadow list: inherited + inner params */` |
|        26 |  2630 | `				nInnerParamMax = 0;` |
|        76 |  2631 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        52 |  2632 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|        20 |  2633 | `						nInnerParamMax++;` |
|         9 |  2634 | `					}` |
|        27 |  2635 | `				}` |
|        26 |  2636 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|        24 |  2637 | `					&pGen->pVm->sAllocator,` |
|        24 |  2638 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|        26 |  2639 | `				if( aInnerShadow == 0 ){` |
|       ! 0 |  2640 | `					return SXERR_ABORT;` |
|         - |  2641 | `				}` |
|        26 |  2642 | `				nInnerShadow = 0;` |
|        32 |  2643 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|         7 |  2644 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|         4 |  2645 | `				}` |
|        76 |  2646 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        52 |  2647 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|        34 |  2648 | `						continue;` |
|         - |  2649 | `					}` |
|        20 |  2650 | `					if( &p[1] >= pInnerSigEnd ){` |
|       ! 0 |  2651 | `						break;` |
|         - |  2652 | `					}` |
|        20 |  2653 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2654 | `						continue;` |
|         - |  2655 | `					}` |
|        20 |  2656 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        11 |  2657 | `				}` |
|        26 |  2658 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|        26 |  2659 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|       ! 0 |  2660 | `					pScan++;` |
|       ! 0 |  2661 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|       ! 0 |  2662 | `						&& pScan->sData.nByte == 1` |
|       ! 0 |  2663 | `						&& pScan->sData.zString[0] == '?' ){` |
|       ! 0 |  2664 | `						pScan++;` |
|       ! 0 |  2665 | `					}` |
|       ! 0 |  2666 | `					if( pScan < pEnd` |
|       ! 0 |  2667 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  2668 | `						pScan++;` |
|       ! 0 |  2669 | `					}` |
|       ! 0 |  2670 | `				}` |
|        26 |  2671 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|        26 |  2672 | `					pScan++; /* past '=>' */` |
|        12 |  2673 | `				}` |
|        26 |  2674 | `				pInnerBodyEnd = pScan;` |
|        26 |  2675 | `				iNestInner = 0;` |
|       156 |  2676 | `				while( pInnerBodyEnd < pEnd ){` |
|       138 |  2677 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|         - |  2678 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|         - |  2679 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|         7 |  2680 | `						break;` |
|         - |  2681 | `					}` |
|       132 |  2682 | `					if( pInnerBodyEnd->nType &` |
|         - |  2683 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         6 |  2684 | `						iNestInner++;` |
|       130 |  2685 | `					}else if( pInnerBodyEnd->nType &` |
|         - |  2686 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         6 |  2687 | `						iNestInner--;` |
|         2 |  2688 | `					}` |
|       132 |  2689 | `					pInnerBodyEnd++;` |
|         2 |  2690 | `				}` |
|         - |  2691 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|         - |  2692 | `				 * the outer's body: a default value is evaluated at call time` |
|         - |  2693 | `				 * in the outer frame, so any free variable it references is` |
|         - |  2694 | `				 * an outer capture. We must NOT scan the parameter-name` |
|         - |  2695 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|         - |  2696 | `				 * or those names leak into the outer's closure environment.` |
|         - |  2697 | `				 *` |
|         - |  2698 | `				 * Walk the signature argument-by-argument, splitting on` |
|         - |  2699 | `				 * top-level commas, and for each argument scan only the token` |
|         - |  2700 | `				 * range after the '=' sign. */` |
|         - |  2701 | `				{` |
|        26 |  2702 | `					SyToken *pArgStart = pInnerSigStart;` |
|        44 |  2703 | `					while( pArgStart < pInnerSigEnd ){` |
|        20 |  2704 | `						SyToken *pArgEnd = pArgStart;` |
|        20 |  2705 | `						SyToken *pEq = 0;` |
|        20 |  2706 | `						int iNestArg = 0;` |
|        68 |  2707 | `						while( pArgEnd < pInnerSigEnd ){` |
|        50 |  2708 | `							if( iNestArg == 0` |
|        52 |  2709 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|         3 |  2710 | `								break;` |
|         - |  2711 | `							}` |
|        50 |  2712 | `							if( pArgEnd->nType &` |
|         - |  2713 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  2714 | `								iNestArg++;` |
|        50 |  2715 | `							}else if( pArgEnd->nType &` |
|         - |  2716 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  2717 | `								iNestArg--;` |
|       ! 0 |  2718 | `							}` |
|        48 |  2719 | `							if( pEq == 0 && iNestArg == 0` |
|        44 |  2720 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|         7 |  2721 | `								pEq = pArgEnd;` |
|         3 |  2722 | `							}` |
|        50 |  2723 | `							pArgEnd++;` |
|         2 |  2724 | `						}` |
|        20 |  2725 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|        10 |  2726 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         3 |  2727 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|         7 |  2728 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  2729 | `								return SXERR_ABORT;` |
|         - |  2730 | `							}` |
|         3 |  2731 | `						}` |
|        20 |  2732 | `						pArgStart = pArgEnd;` |
|        18 |  2733 | `						if( pArgStart < pInnerSigEnd` |
|        12 |  2734 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|         3 |  2735 | `							pArgStart++;` |
|         1 |  2736 | `						}` |
|         2 |  2737 | `					}` |
|         - |  2738 | `				}` |
|        38 |  2739 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        12 |  2740 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|        26 |  2741 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2742 | `					return SXERR_ABORT;` |
|         - |  2743 | `				}` |
|        26 |  2744 | `				pScan = pInnerBodyEnd;` |
|        26 |  2745 | `				continue;` |
|         - |  2746 | `			}` |
|         5 |  2747 | `		}` |
|      2713 |  2748 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      2521 |  2749 | `			pScan++;` |
|      2521 |  2750 | `			continue;` |
|         - |  2751 | `		}` |
|         - |  2752 | `		{` |
|         - |  2753 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       196 |  2754 | `			SyToken *pDollar = pScan;` |
|       288 |  2755 | `			while( &pDollar[1] < pEnd` |
|       196 |  2756 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2757 | `				pDollar++;` |
|       ! 0 |  2758 | `			}` |
|       196 |  2759 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2760 | `				break;` |
|         - |  2761 | `			}` |
|       196 |  2762 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2763 | `				pScan = pDollar + 1;` |
|       ! 0 |  2764 | `				continue;` |
|         - |  2765 | `			}` |
|       292 |  2766 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       192 |  2767 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|        96 |  2768 | `				aShadow,nShadow);` |
|       196 |  2769 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2770 | `				return SXERR_ABORT;` |
|         - |  2771 | `			}` |
|       196 |  2772 | `			pScan = pDollar + 2;` |
|         - |  2773 | `		}` |
|         4 |  2774 | `	}` |
|       497 |  2775 | `	return SXRET_OK;` |
|       251 |  2776 | `}` |
|         - |  2777 | `/*` |
|         - |  2778 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2779 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2780 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2781 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2782 | ` * $this is also made available.` |
|         - |  2783 | ` */` |
|       468 |  2784 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2785 | `{` |
|         - |  2786 | `	ph7_vm_func *pFunc;` |
|         - |  2787 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2788 | `	GenBlock *pBlock;` |
|         - |  2789 | `	SySet *pInstrContainer;` |
|         - |  2790 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|         - |  2791 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|         - |  2792 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|         - |  2793 | `	SyToken *pSavedEnd;` |
|         - |  2794 | `	ph7_vm_func_arg *aArgs;` |
|         - |  2795 | `	char zName[512];` |
|         - |  2796 | `	static int iCnt = 1;` |
|         - |  2797 | `	char *zDup;` |
|         - |  2798 | `	SyToken *pTokKw;` |
|         - |  2799 | `	sxu32 nLen;` |
|         - |  2800 | `	sxu32 nLine;` |
|       473 |  2801 | `	sxi32 iFlags = 0;` |
|       473 |  2802 | `	int bStatic = 0;` |
|         - |  2803 | `	sxi32 rc;` |
|         - |  2804 | `	sxu32 n;` |
|       234 |  2805 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2806 |  |
|       473 |  2807 | `	nLine = pGen->pIn->nLine;` |
|         - |  2808 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       473 |  2809 | `	pTokKw = pGen->pIn;` |
|         - |  2810 | `	/* Optional 'static' prefix */` |
|       468 |  2811 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       473 |  2812 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  2813 | `		bStatic = 1;` |
|         7 |  2814 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         7 |  2815 | `		pGen->pIn++;` |
|         3 |  2816 | `	}` |
|         - |  2817 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       468 |  2818 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       473 |  2819 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2820 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2821 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2822 | `		return SXERR_SYNTAX;` |
|         - |  2823 | `	}` |
|       473 |  2824 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2825 | `	/* Optional '&' — return by reference */` |
|       473 |  2826 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2827 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2828 | `		pGen->pIn++;` |
|       ! 0 |  2829 | `	}` |
|         - |  2830 | `	/* Expect '(' */` |
|       473 |  2831 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  2832 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2833 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2834 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|         2 |  2835 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2836 | `		}else{` |
|       ! 0 |  2837 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2838 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|         - |  2839 | `		}` |
|         3 |  2840 | `		return SXERR_SYNTAX;` |
|         - |  2841 | `	}` |
|       471 |  2842 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2843 | `	/* Delimit the parameter list */` |
|       471 |  2844 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       471 |  2845 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2846 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2847 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2848 | `		return SXERR_SYNTAX;` |
|         - |  2849 | `	}` |
|         - |  2850 | `	/* Allocate the function state */` |
|       469 |  2851 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       469 |  2852 | `	if( pFunc == 0 ){` |
|       ! 0 |  2853 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2854 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2855 | `		return SXERR_ABORT;` |
|         - |  2856 | `	}` |
|         - |  2857 | `	/* Generate a unique lambda name */` |
|       469 |  2858 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       469 |  2859 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2860 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2861 | `	}` |
|       469 |  2862 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       469 |  2863 | `	if( zDup == 0 ){` |
|       ! 0 |  2864 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2865 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2866 | `		return SXERR_ABORT;` |
|         - |  2867 | `	}` |
|       469 |  2868 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2869 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       469 |  2870 | `	pFunc->nLine = nLine;` |
|         - |  2871 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       469 |  2872 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2873 | `		return SXERR_ABORT;` |
|         - |  2874 | `	}` |
|         - |  2875 | `	/* Collect function arguments */` |
|       469 |  2876 | `	if( pGen->pIn < pSigEnd ){` |
|       124 |  2877 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       124 |  2878 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2879 | `			return SXERR_ABORT;` |
|         - |  2880 | `		}` |
|        60 |  2881 | `	}` |
|         - |  2882 | `	/* Point past ')' and parse optional return type */` |
|       469 |  2883 | `	pGen->pIn = &pSigEnd[1];` |
|       469 |  2884 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       469 |  2885 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2886 | `		return SXERR_ABORT;` |
|       469 |  2887 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2888 | `		return SXERR_SYNTAX;` |
|         - |  2889 | `	}` |
|         - |  2890 | `	/* Expect '=>' */` |
|       469 |  2891 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  2892 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2893 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2894 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|         2 |  2895 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2896 | `		}else{` |
|       ! 0 |  2897 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2898 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|         - |  2899 | `		}` |
|         3 |  2900 | `		return SXERR_SYNTAX;` |
|         - |  2901 | `	}` |
|       467 |  2902 | `	pGen->pIn++; /* Jump '=>' */` |
|       467 |  2903 | `	pBodyStart = pGen->pIn;` |
|       467 |  2904 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2905 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2906 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2907 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2908 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       467 |  2909 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2910 | `	{` |
|       467 |  2911 | `		SyString *aShadow = 0;` |
|       467 |  2912 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       467 |  2913 | `		if( nShadow > 0 ){` |
|       122 |  2914 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       118 |  2915 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       122 |  2916 | `			if( aShadow == 0 ){` |
|       ! 0 |  2917 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2918 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2919 | `				return SXERR_ABORT;` |
|         - |  2920 | `			}` |
|       276 |  2921 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       158 |  2922 | `				aShadow[n] = aArgs[n].sName;` |
|        81 |  2923 | `			}` |
|        59 |  2924 | `		}` |
|       698 |  2925 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       231 |  2926 | `			aShadow,nShadow);` |
|       467 |  2927 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2928 | `			return SXERR_ABORT;` |
|         - |  2929 | `		}` |
|         - |  2930 | `	}` |
|         - |  2931 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2932 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2933 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2934 | `	 * $this. */` |
|       467 |  2935 | `	if( !bStatic ){` |
|         - |  2936 | `		char *zThisDup;` |
|       461 |  2937 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       461 |  2938 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2939 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2940 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2941 | `			return SXERR_ABORT;` |
|         - |  2942 | `		}` |
|       461 |  2943 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       461 |  2944 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       461 |  2945 | `		sEnv.nIdx = SXU32_HIGH;` |
|       461 |  2946 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       461 |  2947 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       461 |  2948 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       228 |  2949 | `	}` |
|         - |  2950 | `	/* Arrow functions are always closures */` |
|       467 |  2951 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2952 | `	/* Compile the body expression as an implicit return */` |
|       698 |  2953 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       231 |  2954 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       467 |  2955 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2956 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2957 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2958 | `		return SXERR_ABORT;` |
|         - |  2959 | `	}` |
|       467 |  2960 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       467 |  2961 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       467 |  2962 | `	pSavedEnd = pGen->pEnd;` |
|       467 |  2963 | `	pGen->pIn = pBodyStart;` |
|       467 |  2964 | `	pGen->pEnd = pBodyEnd;` |
|       467 |  2965 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       467 |  2966 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2967 | `		return SXERR_ABORT;` |
|         - |  2968 | `	}` |
|         - |  2969 | `	/* The cursor stopped just past the body expression */` |
|       467 |  2970 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2971 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2972 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2973 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2974 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       467 |  2975 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       467 |  2976 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       467 |  2977 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       467 |  2978 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       467 |  2979 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2980 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       467 |  2981 | `	pGen->pIn = pBodyEnd;` |
|       467 |  2982 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2983 | `	/* Emit the load-closure instruction */` |
|       467 |  2984 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       467 |  2985 | `	return SXRET_OK;` |
|       239 |  2986 | `}` |
|         - |  2987 | `/*` |
|         - |  2988 | ` * Compile a single arm's expression range into a freshly-allocated` |
|         - |  2989 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|         - |  2990 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|         - |  2991 | ` * expression's value.` |
|         - |  2992 | ` */` |
|       354 |  2993 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|         - |  2994 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|         3 |  2995 | `{` |
|         - |  2996 | `	SySet *pInstrContainer;` |
|         - |  2997 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  2998 | `	GenBlock *pArmBlock;` |
|         - |  2999 | `	sxi32 rc;` |
|       357 |  3000 | `	pTmpIn  = pGen->pIn;` |
|       357 |  3001 | `	pTmpEnd = pGen->pEnd;` |
|       357 |  3002 | `	pGen->pIn  = pStart;` |
|       357 |  3003 | `	pGen->pEnd = pStop;` |
|       357 |  3004 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       357 |  3005 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|         - |  3006 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|         - |  3007 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|         - |  3008 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|         - |  3009 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|         - |  3010 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|       534 |  3011 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       177 |  3012 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|       357 |  3013 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3014 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  3015 | `		pGen->pIn  = pTmpIn;` |
|       ! 0 |  3016 | `		pGen->pEnd = pTmpEnd;` |
|       ! 0 |  3017 | `		return SXERR_ABORT;` |
|         - |  3018 | `	}` |
|       357 |  3019 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       357 |  3020 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       357 |  3021 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       357 |  3022 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       357 |  3023 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       357 |  3024 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       357 |  3025 | `	pGen->pIn  = pTmpIn;` |
|       357 |  3026 | `	pGen->pEnd = pTmpEnd;` |
|       357 |  3027 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3028 | `		return SXERR_ABORT;` |
|         - |  3029 | `	}` |
|       357 |  3030 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 |  3031 | `		return SXERR_EMPTY;` |
|         - |  3032 | `	}` |
|       357 |  3033 | `	return SXRET_OK;` |
|       180 |  3034 | `}` |
|         - |  3035 | `/*` |
|         - |  3036 | ` * Compile a PHP 8.0 match expression:` |
|         - |  3037 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|         - |  3038 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|         - |  3039 | ` * Strict comparison (===) is used between the subject and each condition.` |
|         - |  3040 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|         - |  3041 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|         - |  3042 | ` */` |
|         - |  3043 | `/*` |
|         - |  3044 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|         - |  3045 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|         - |  3046 | ` * caller can bail out of the current expression.` |
|         - |  3047 | ` */` |
|         2 |  3048 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|         1 |  3049 | `{` |
|         - |  3050 | `	va_list ap;` |
|         - |  3051 | `	sxi32 rc;` |
|         - |  3052 | `	SyBlob sMsg;` |
|         3 |  3053 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|         3 |  3054 | `	va_start(ap,zFmt);` |
|         3 |  3055 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|         3 |  3056 | `	va_end(ap);` |
|         3 |  3057 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|         3 |  3058 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|         3 |  3059 | `	SyBlobRelease(&sMsg);` |
|         3 |  3060 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3061 | `		return SXERR_ABORT;` |
|         - |  3062 | `	}` |
|         3 |  3063 | `	return SXERR_SYNTAX;` |
|         2 |  3064 | `}` |
|         - |  3065 | `/*` |
|         - |  3066 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|         - |  3067 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|         - |  3068 | ` * Returns the stop token pointer (or pEnd if none found).` |
|         - |  3069 | ` */` |
|       356 |  3070 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|         4 |  3071 | `{` |
|       360 |  3072 | `	SyToken *pCur = pStart;` |
|       360 |  3073 | `	int iNest = 0;` |
|       838 |  3074 | `	while( pCur < pEnd ){` |
|       802 |  3075 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        13 |  3076 | `			iNest++;` |
|       796 |  3077 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        13 |  3078 | `			iNest--;` |
|       784 |  3079 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|       323 |  3080 | `			return pCur;` |
|         - |  3081 | `		}` |
|       482 |  3082 | `		pCur++;` |
|         4 |  3083 | `	}` |
|        39 |  3084 | `	return pEnd;` |
|       182 |  3085 | `}` |
|        72 |  3086 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3087 | `{` |
|         - |  3088 | `	ph7_match *pMatch;` |
|         - |  3089 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|        77 |  3090 | `	int bHasDefault = 0;` |
|         - |  3091 | `	sxu32 nLine;` |
|         - |  3092 | `	sxi32 rc;` |
|        36 |  3093 | `	SXUNUSED(iCompileFlag);` |
|        77 |  3094 | `	nLine = pGen->pIn->nLine;` |
|        77 |  3095 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|         - |  3096 | `	/* Expect '(' */` |
|        77 |  3097 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  3098 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3099 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|       ! 0 |  3100 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|         - |  3101 | `	}` |
|        77 |  3102 | `	pGen->pIn++; /* Jump '(' */` |
|        77 |  3103 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|        77 |  3104 | `	if( pSubjEnd >= pGen->pEnd ){` |
|       ! 0 |  3105 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3106 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         - |  3107 | `	}` |
|        77 |  3108 | `	if( pGen->pIn >= pSubjEnd ){` |
|       ! 0 |  3109 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3110 | `			"syntax error, unexpected \")\", expecting match subject");` |
|         - |  3111 | `	}` |
|         - |  3112 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|        77 |  3113 | `	pSavedEnd = pGen->pEnd;` |
|        77 |  3114 | `	pGen->pEnd = pSubjEnd;` |
|        77 |  3115 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        77 |  3116 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3117 | `		return SXERR_ABORT;` |
|         - |  3118 | `	}` |
|        77 |  3119 | `	pGen->pEnd = pSavedEnd;` |
|        77 |  3120 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|         - |  3121 | `	/* Expect '{' */` |
|        77 |  3122 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 |  3123 | `		return GenStateMatchError(pGen,` |
|       ! 0 |  3124 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  3125 | `			"syntax error, expecting \"{\" after match subject");` |
|         - |  3126 | `	}` |
|        77 |  3127 | `	pGen->pIn++; /* Jump '{' */` |
|        77 |  3128 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|        77 |  3129 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  3130 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3131 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|         - |  3132 | `	}` |
|         - |  3133 | `	/* Allocate ph7_match container */` |
|        77 |  3134 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|        77 |  3135 | `	if( pMatch == 0 ){` |
|       ! 0 |  3136 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  3137 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3138 | `		return SXERR_ABORT;` |
|         - |  3139 | `	}` |
|        77 |  3140 | `	SyZero(pMatch,sizeof(ph7_match));` |
|        77 |  3141 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|         - |  3142 | `	/* Iterate arms */` |
|       259 |  3143 | `	while( pGen->pIn < pBodyEnd ){` |
|         - |  3144 | `		ph7_match_arm sArm;` |
|         - |  3145 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|       190 |  3146 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|       190 |  3147 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|       190 |  3148 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|       190 |  3149 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3150 | `		/* 'default' arm? */` |
|       186 |  3151 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       107 |  3152 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|        22 |  3153 | `			if( bHasDefault ){` |
|         3 |  3154 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|         - |  3155 | `					"Match expressions may only contain one default arm");` |
|         4 |  3156 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  3157 | `			}` |
|        20 |  3158 | `			sArm.bDefault = 1;` |
|        20 |  3159 | `			bHasDefault = 1;` |
|        20 |  3160 | `			pGen->pIn++;` |
|        20 |  3161 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       ! 0 |  3162 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3163 | `					"syntax error, expecting \"=>\" after 'default'");` |
|         - |  3164 | `			}` |
|        20 |  3165 | `			pGen->pIn++; /* Jump '=>' */` |
|        11 |  3166 | `		}else{` |
|         - |  3167 | `			/* Condition list: cond (',' cond)* '=>' */` |
|       170 |  3168 | `			pCondStart = pGen->pIn;` |
|       170 |  3169 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|         - |  3170 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       178 |  3171 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|         - |  3172 | `				SySet sCondBc;` |
|         9 |  3173 | `				if( pCondStart >= pArrow ){` |
|       ! 0 |  3174 | `					return GenStateMatchError(pGen,nArmLine,` |
|         - |  3175 | `						"syntax error, empty match condition expression");` |
|         - |  3176 | `				}` |
|         9 |  3177 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         9 |  3178 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|         9 |  3179 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3180 | `					return SXERR_ABORT;` |
|         - |  3181 | `				}` |
|         9 |  3182 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         9 |  3183 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|         9 |  3184 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|         - |  3185 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|         1 |  3186 | `			}` |
|       170 |  3187 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  3188 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3189 | `					"syntax error, expecting \"=>\" in match arm");` |
|         - |  3190 | `			}` |
|       167 |  3191 | `			if( pCondStart >= pArrow ){` |
|       ! 0 |  3192 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3193 | `					"syntax error, empty match condition expression");` |
|         - |  3194 | `			}` |
|         - |  3195 | `			{` |
|         - |  3196 | `				SySet sCondBc;` |
|       167 |  3197 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       167 |  3198 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       167 |  3199 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3200 | `					return SXERR_ABORT;` |
|         - |  3201 | `				}` |
|       167 |  3202 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         - |  3203 | `			}` |
|       167 |  3204 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|         - |  3205 | `		}` |
|         - |  3206 | `		/* Compile result expression: up to top-level ',' or body end */` |
|       185 |  3207 | `		pResStart = pGen->pIn;` |
|       185 |  3208 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|       185 |  3209 | `		if( pResStart >= pResEnd ){` |
|       ! 0 |  3210 | `			return GenStateMatchError(pGen,nArmLine,` |
|         - |  3211 | `				"syntax error, expected expression after \"=>\"");` |
|         - |  3212 | `		}` |
|       185 |  3213 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|       185 |  3214 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3215 | `			return SXERR_ABORT;` |
|         - |  3216 | `		}` |
|       185 |  3217 | `		pGen->pIn = pResEnd;` |
|       185 |  3218 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       151 |  3219 | `			pGen->pIn++; /* Skip trailing ',' */` |
|        74 |  3220 | `		}` |
|       185 |  3221 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|         3 |  3222 | `	}` |
|        71 |  3223 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|        71 |  3224 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|        71 |  3225 | `	return SXRET_OK;` |
|        41 |  3226 | `}` |
|         - |  3227 | `/*` |
|         - |  3228 | ` * Compile a backtick quoted string.` |
|         - |  3229 | ` */` |
|         4 |  3230 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  3231 | `{` |
|         - |  3232 | `	static const SyString sName = { "shell_exec", sizeof("shell_exec")-1 };` |
|         6 |  3233 | `	sxu32 nIdx = 0;` |
|         - |  3234 | `	sxi32 rc;` |
|         - |  3235 | `	/*` |
|         - |  3236 | ``	 * `cmd` IS shell_exec("cmd") in php — it interpolates like a double-quoted string,`` |
|         - |  3237 | `	 * runs the command and yields its output. PH7 refused to run it at all (TICKET` |
|         - |  3238 | `	 * 1433-40) and quietly evaluated to NULL. php 8.5 deprecates the syntax but still` |
|         - |  3239 | `	 * executes it, so compile it to the real call and say what php says.` |
|         - |  3240 | `	 */` |
|         6 |  3241 | `	PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pGen->pIn->nLine,` |
|         - |  3242 | ``		"The backtick (`) operator is deprecated, use shell_exec() instead");`` |
|         - |  3243 | `	/* The body interpolates exactly like a double-quoted string */` |
|         6 |  3244 | `	pGen->pIn->nType &= ~PH7_TK_BSTR;` |
|         6 |  3245 | `	pGen->pIn->nType \|= PH7_TK_DSTR;` |
|         6 |  3246 | `	rc = PH7_CompileString(&(*pGen),iCompileFlag);` |
|         6 |  3247 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3248 | `		return rc;` |
|         - |  3249 | `	}` |
|         - |  3250 | `	/* ... and the command string is then handed to shell_exec() */` |
|         6 |  3251 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|         6 |  3252 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         6 |  3253 | `		if( pObj == 0 ){` |
|       ! 0 |  3254 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  3255 | `			return SXERR_ABORT;` |
|         - |  3256 | `		}` |
|         6 |  3257 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|         6 |  3258 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|         2 |  3259 | `	}` |
|         6 |  3260 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         6 |  3261 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         6 |  3262 | `	return SXRET_OK;` |
|         4 |  3263 | `}` |
|         - |  3264 | `/*` |
|         - |  3265 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|         - |  3266 | ` * construct.` |
|         - |  3267 | ` */` |
|        64 |  3268 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3269 | `{` |
|         - |  3270 | `	SyString *pName;` |
|         - |  3271 | `	sxu32 nKeyID;` |
|         - |  3272 | `	sxi32 rc;` |
|         - |  3273 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        69 |  3274 | `	pName = &pGen->pIn->sData;` |
|        69 |  3275 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        69 |  3276 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        69 |  3277 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|         9 |  3278 | `		SyToken *pTmp,*pNext = 0;` |
|         - |  3279 | `		/* Compile arguments one after one */` |
|         9 |  3280 | `		pTmp = pGen->pEnd;` |
|         - |  3281 | `		/* Symisc eXtension to the PHP programming language:` |
|         - |  3282 | `		 * 'echo' can be used in the context of a function which` |
|         - |  3283 | `		 *  mean that the following expression is valid:` |
|         - |  3284 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|         - |  3285 | `		 */` |
|         9 |  3286 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        17 |  3287 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|         9 |  3288 | `			if( pGen->pIn < pNext ){` |
|         9 |  3289 | `				pGen->pEnd = pNext;` |
|         9 |  3290 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|         9 |  3291 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3292 | `					return SXERR_ABORT;` |
|         - |  3293 | `				}` |
|         9 |  3294 | `				if( rc != SXERR_EMPTY ){` |
|         - |  3295 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|         - |  3296 | `					 * without the overhead of a function call.` |
|         - |  3297 | `					 * This is a very powerful optimization that improve` |
|         - |  3298 | `					 * performance greatly.` |
|         - |  3299 | `					 */` |
|         9 |  3300 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|         4 |  3301 | `				}` |
|         4 |  3302 | `			}` |
|         - |  3303 | `			/* Jump trailing commas */` |
|         9 |  3304 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|       ! 0 |  3305 | `				pNext++;` |
|       ! 0 |  3306 | `			}` |
|         9 |  3307 | `			pGen->pIn = pNext;` |
|         1 |  3308 | `		}` |
|         - |  3309 | `		/* Restore token stream */` |
|         9 |  3310 | `		pGen->pEnd = pTmp;` |
|         5 |  3311 | `	}else{` |
|        61 |  3312 | `		sxi32 nArg = 0;` |
|        61 |  3313 | `		sxu32 nIdx = 0;` |
|        61 |  3314 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        61 |  3315 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3316 | `			return SXERR_ABORT;` |
|        61 |  3317 | `		}else if(rc != SXERR_EMPTY ){` |
|        61 |  3318 | `			nArg = 1;` |
|        28 |  3319 | `		}` |
|        61 |  3320 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3321 | `			ph7_value *pObj;` |
|         - |  3322 | `			/* Emit the call instruction */` |
|        31 |  3323 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        31 |  3324 | `			if( pObj == 0 ){` |
|       ! 0 |  3325 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3326 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3327 | `				return SXERR_ABORT;` |
|         - |  3328 | `			}` |
|        31 |  3329 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3330 | `			/* Install in the literal table */` |
|        31 |  3331 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        13 |  3332 | `		}` |
|         - |  3333 | `		/* Emit the call instruction */` |
|        61 |  3334 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        61 |  3335 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3336 | `	}` |
|         - |  3337 | `	/* Node successfully compiled */` |
|        69 |  3338 | `	return SXRET_OK;` |
|        37 |  3339 | `}` |
|         - |  3340 | `/*` |
|         - |  3341 | ` * Compile a node holding a variable declaration.` |
|         - |  3342 | ` * According to the PHP language reference` |
|         - |  3343 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|         - |  3344 | ` *  The variable name is case-sensitive.` |
|         - |  3345 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|         - |  3346 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3347 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|         - |  3348 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|         - |  3349 | ` *  Note: $this is a special variable that can't be assigned.` |
|         - |  3350 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|         - |  3351 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|         - |  3352 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|         - |  3353 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|         - |  3354 | ` *  the chapter on Expressions.` |
|         - |  3355 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|         - |  3356 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|         - |  3357 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|         - |  3358 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|         - |  3359 | ` *  is being assigned (the source variable).` |
|         - |  3360 | ` */` |
|  18587066 |  3361 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3362 | `{` |
|  18587071 |  3363 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3364 | `	sxi32 iVv;` |
|         - |  3365 | `	sxi32 iP1;` |
|         - |  3366 | `	void *p3;` |
|         - |  3367 | `	sxi32 rc;` |
|  18587071 |  3368 | `	iVv = -1; /* Variable variable counter */` |
|  37174149 |  3369 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  18587083 |  3370 | `		pGen->pIn++;` |
|  18587083 |  3371 | `		iVv++;` |
|         5 |  3372 | `	}` |
|  18587071 |  3373 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3374 | `		/* Invalid variable name */` |
|       ! 0 |  3375 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3376 | `		if( rc == SXERR_ABORT ){` |
|         - |  3377 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3378 | `			return SXERR_ABORT;` |
|         - |  3379 | `		}` |
|       ! 0 |  3380 | `		return SXRET_OK;` |
|         - |  3381 | `	}` |
|  18587071 |  3382 | `	p3  = 0;` |
|  18587071 |  3383 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|         - |  3384 | `		/* Dynamic variable creation */` |
|        19 |  3385 | `		pGen->pIn++;  /* Jump the open curly */` |
|        19 |  3386 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|        19 |  3387 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3388 | `			/* Empty expression */` |
|         - |  3389 | `			{` |
|         - |  3390 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|         - |  3391 | `			 * the "expecting" tail only appears when something could still follow. */` |
|         3 |  3392 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|         3 |  3393 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|         1 |  3394 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|         - |  3395 | `			}` |
|         3 |  3396 | `			return SXRET_OK;` |
|         - |  3397 | `		}` |
|         - |  3398 | `		/* Compile the expression holding the variable name */` |
|        16 |  3399 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        16 |  3400 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3401 | `			return SXERR_ABORT;` |
|        16 |  3402 | `		}else if( rc == SXERR_EMPTY ){` |
|         3 |  3403 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|         3 |  3404 | `			return SXRET_OK;` |
|         - |  3405 | `		}` |
|         7 |  3406 | `	}else{` |
|         - |  3407 | `		SyHashEntry *pEntry;` |
|         - |  3408 | `		SyString *pName;` |
|  18587055 |  3409 | `		char *zName = 0;` |
|         - |  3410 | `		/* Extract variable name */` |
|  18587055 |  3411 | `		pName = &pGen->pIn->sData;` |
|         - |  3412 | `		/* Advance the stream cursor */` |
|  18587055 |  3413 | `		pGen->pIn++;` |
|  18587055 |  3414 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  18587055 |  3415 | `		if( pEntry == 0 ){` |
|         - |  3416 | `			/* Duplicate name */` |
|   1089233 |  3417 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   1089233 |  3418 | `			if( zName == 0 ){` |
|       ! 0 |  3419 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3420 | `				return SXERR_ABORT;` |
|         - |  3421 | `			}` |
|         - |  3422 | `			/* Install in the hashtable */` |
|   1089233 |  3423 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    544619 |  3424 | `		}else{` |
|         - |  3425 | `			/* Name already available */` |
|  17497827 |  3426 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3427 | `		}` |
|  18587055 |  3428 | `		p3 = (void *)zName;` |
|         - |  3429 | `	}` |
|  18587067 |  3430 | `	iP1 = 0;` |
|  18587067 |  3431 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   5548063 |  3432 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3433 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   5544227 |  3434 | `			iP1 = 1;` |
|   2772111 |  3435 | `		}` |
|   2774029 |  3436 | `	}` |
|         - |  3437 | `	/* Emit the load instruction */` |
|  18587067 |  3438 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  18587079 |  3439 | `	while( iVv > 0 ){` |
|        13 |  3440 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3441 | `		iVv--;` |
|         1 |  3442 | `	}` |
|         - |  3443 | `	/* Node successfully compiled */` |
|  18587067 |  3444 | `	return SXRET_OK;` |
|   9293538 |  3445 | `}` |
|         - |  3446 | `/*` |
|         - |  3447 | ` * Load a literal.` |
|         - |  3448 | ` */` |
|  11684486 |  3449 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3450 | `{` |
|  11684491 |  3451 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3452 | `	ph7_value *pObj;` |
|         - |  3453 | `	SyString *pStr;` |
|         - |  3454 | `	sxu32 nIdx;` |
|         - |  3455 | `	/* Extract token value */` |
|  11684491 |  3456 | `	pStr = &pToken->sData;` |
|         - |  3457 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  11684491 |  3458 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2195965 |  3459 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3460 | `			/* NULL constant are always indexed at 0 */` |
|    942523 |  3461 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    942523 |  3462 | `			return SXRET_OK;` |
|   1253447 |  3463 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3464 | `			/* TRUE constant are always indexed at 1 */` |
|    321399 |  3465 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    321399 |  3466 | `			return SXRET_OK;` |
|         5 |  3467 | `		}` |
|  10934406 |  3468 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1959702 |  3469 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3470 | `			/* FALSE constant are always indexed at 2 */` |
|    705877 |  3471 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    705877 |  3472 | `			return SXRET_OK;` |
|   9191454 |  3473 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    817590 |  3474 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3475 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      3823 |  3476 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3823 |  3477 | `			if( pObj == 0 ){` |
|       ! 0 |  3478 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3479 | `				return SXERR_ABORT;` |
|         - |  3480 | `			}` |
|      3823 |  3481 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3482 | `			/* Emit the load constant instruction */` |
|      3823 |  3483 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3823 |  3484 | `			return SXRET_OK;` |
|   8895533 |  3485 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    233384 |  3486 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3487 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         7 |  3488 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3489 | `			if( pObj == 0 ){` |
|       ! 0 |  3490 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3491 | `				return SXERR_ABORT;` |
|         - |  3492 | `			}` |
|         7 |  3493 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3494 | `				SyString sNs;` |
|         7 |  3495 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         7 |  3496 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         4 |  3497 | `			}else{` |
|       ! 0 |  3498 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3499 | `			}` |
|         7 |  3500 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         7 |  3501 | `			return SXRET_OK;` |
|   8891931 |  3502 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    437525 |  3503 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   8990141 |  3504 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    422636 |  3505 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3506 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3507 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3508 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3509 | `				/* Point to the upper block */` |
|        11 |  3510 | `				pBlock = pBlock->pParent;` |
|         1 |  3511 | `			}` |
|        11 |  3512 | `			if( pBlock == 0 ){` |
|         - |  3513 | `				/* Called in the global scope,load NULL */` |
|         5 |  3514 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3515 | `			}else{` |
|         - |  3516 | `				/* Extract the target function/method */` |
|         7 |  3517 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3518 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|         7 |  3519 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3520 | `				if( pObj == 0 ){` |
|       ! 0 |  3521 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3522 | `					return SXERR_ABORT;` |
|         - |  3523 | `				}` |
|         - |  3524 | `				/*` |
|         - |  3525 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|         - |  3526 | `				 * function name inside a plain function (php does not answer "" there —` |
|         - |  3527 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|         - |  3528 | `				 * unqualified in every method).` |
|         - |  3529 | `				 */` |
|         8 |  3530 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 |  3531 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         - |  3532 | `					SyBlob sQual;` |
|         - |  3533 | `					SyString sOut;` |
|         3 |  3534 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|         3 |  3535 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|         3 |  3536 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|         3 |  3537 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|         3 |  3538 | `					SyBlobRelease(&sQual);` |
|         2 |  3539 | `				}else{` |
|         5 |  3540 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3541 | `				}` |
|         - |  3542 | `				/* Emit the load constant instruction */` |
|         7 |  3543 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3544 | `			}` |
|        11 |  3545 | `			return SXRET_OK;` |
|         - |  3546 | `	}` |
|         - |  3547 | `	/* Query literal table */` |
|   9710873 |  3548 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3549 | `		ph7_value *pLitObj;` |
|         - |  3550 | `		/* Unknown literal,install it in the literal table */` |
|   1859025 |  3551 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1859025 |  3552 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3553 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3554 | `			return SXERR_ABORT;` |
|         - |  3555 | `		}` |
|   1859025 |  3556 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1859025 |  3557 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    929510 |  3558 | `	}` |
|         - |  3559 | `	/* Emit the load constant instruction */` |
|   9710873 |  3560 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9710873 |  3561 | `	return SXRET_OK;` |
|   5842248 |  3562 | `}` |
|         - |  3563 | `/*` |
|         - |  3564 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3565 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3566 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3567 | ` * Otherwise, load the simple literal directly.` |
|         - |  3568 | ` */` |
|  11688346 |  3569 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3570 | `{` |
|         - |  3571 | `	sxi32 rc;` |
|  11688351 |  3572 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3573 | `		return SXRET_OK;` |
|         - |  3574 | `	}` |
|         - |  3575 | `	/* Check if this is a multi-token namespace path */` |
|  11688351 |  3576 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3577 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3865 |  3578 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3865 |  3579 | `		int isAbsolute = 0;` |
|      3865 |  3580 | `		SyBlobReset(pWorker);` |
|         - |  3581 | `		/* Check for leading backslash (absolute path) */` |
|      3865 |  3582 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3863 |  3583 | `			isAbsolute = 1;` |
|      3863 |  3584 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1929 |  3585 | `		}` |
|         - |  3586 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3865 |  3587 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3588 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3589 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3590 | `		}` |
|         - |  3591 | `		/* Collect all path components */` |
|      3973 |  3592 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      3973 |  3593 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        58 |  3594 | `				SyBlobAppend(pWorker,"\\",1);` |
|        31 |  3595 | `			}else{` |
|      3919 |  3596 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3597 | `			}` |
|      3973 |  3598 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3865 |  3599 | `				pGen->pIn++;` |
|      3865 |  3600 | `				break;` |
|         - |  3601 | `			}` |
|       112 |  3602 | `			pGen->pIn++;` |
|         4 |  3603 | `		}` |
|      3865 |  3604 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3605 | `			ph7_value *pObj;` |
|         - |  3606 | `			SyString sPath;` |
|         - |  3607 | `			sxu32 nIdx;` |
|      3865 |  3608 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3609 | `			/* Install in the literal table */` |
|      3865 |  3610 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3835 |  3611 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3835 |  3612 | `				if( pObj == 0 ){` |
|       ! 0 |  3613 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3614 | `					return SXERR_ABORT;` |
|         - |  3615 | `				}` |
|      3835 |  3616 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3835 |  3617 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1915 |  3618 | `			}` |
|         - |  3619 | `			/* Emit the load constant instruction.` |
|         - |  3620 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3621 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5795 |  3622 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1930 |  3623 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1930 |  3624 | `				nIdx,0,0);` |
|      3865 |  3625 | `			return SXRET_OK;` |
|         - |  3626 | `		}` |
|       ! 0 |  3627 | `	}` |
|         - |  3628 | `	/* Single-token literal: load directly */` |
|  11684491 |  3629 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11684491 |  3630 | `	return rc;` |
|   5844178 |  3631 | `}` |
|         - |  3632 | `/*` |
|         - |  3633 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3634 | ` */` |
|         - |  3635 | `/*` |
|         - |  3636 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3637 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3638 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3639 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3640 | ` */` |
|       ! 0 |  3641 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3642 | `{` |
|       ! 0 |  3643 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3644 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3645 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3646 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3647 | `}` |
|  11688346 |  3648 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3649 | `{` |
|         - |  3650 | `	sxi32 rc;` |
|  11688351 |  3651 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11688351 |  3652 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3653 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3654 | `		return rc;` |
|         - |  3655 | `	}` |
|         - |  3656 | `	/* Node successfully compiled */` |
|  11688351 |  3657 | `	return SXRET_OK;` |
|   5844178 |  3658 | `}` |
|         - |  3659 | `/*` |
|         - |  3660 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3661 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3662 | ` */` |
|         8 |  3663 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3664 | `{` |
|         - |  3665 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3666 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3667 | `		pGen->pIn++;` |
|         1 |  3668 | `	}` |
|         9 |  3669 | `	return SXRET_OK;` |
|         1 |  3670 | `}` |
|         - |  3671 | `/*` |
|         - |  3672 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3673 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3674 | ` */` |
|    289932 |  3675 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3676 | `{` |
|    289937 |  3677 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3861 |  3678 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3679 | `			return TRUE;` |
|      3859 |  3680 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         5 |  3681 | `			return TRUE;` |
|         5 |  3682 | `		}` |
|    288006 |  3683 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7649 |  3684 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3685 | `			return TRUE;` |
|         - |  3686 | `		}` |
|      3821 |  3687 | `	}` |
|         - |  3688 | `	/* Not a reserved constant */` |
|    289929 |  3689 | `	return FALSE;` |
|    144971 |  3690 | `}` |
|         - |  3691 | `/*` |
|         - |  3692 | ` * Compile the 'const' statement.` |
|         - |  3693 | ` * According to the PHP language reference` |
|         - |  3694 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3695 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3696 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3697 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3698 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3699 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3700 | ` *  Syntax` |
|         - |  3701 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3702 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3703 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3704 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3705 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3706 | ` *  to get a list of all defined constants.` |
|         - |  3707 | ` *` |
|         - |  3708 | ` * Symisc eXtension.` |
|         - |  3709 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3710 | ` *  would allow only simple scalar value.` |
|         - |  3711 | ` *  Example` |
|         - |  3712 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3713 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3714 | ` */` |
|        48 |  3715 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3716 | `{` |
|         - |  3717 | `	SySet *pConsCode,*pInstrContainer;` |
|        53 |  3718 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3719 | `	SyString *pName;` |
|         - |  3720 | `	sxi32 rc;` |
|        53 |  3721 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        53 |  3722 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3723 | `		/* Invalid constant name */` |
|         8 |  3724 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|         8 |  3725 | `		if( rc == SXERR_ABORT ){` |
|         - |  3726 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3727 | `			return SXERR_ABORT;` |
|         - |  3728 | `		}` |
|         8 |  3729 | `		goto Synchronize;` |
|         - |  3730 | `	}` |
|         - |  3731 | `	/* Peek constant name */` |
|        46 |  3732 | `	pName = &pGen->pIn->sData;` |
|         - |  3733 | `	/* Make sure the constant name isn't reserved */` |
|        46 |  3734 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3735 | `		/* Reserved constant */` |
|         9 |  3736 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|         9 |  3737 | `		if( rc == SXERR_ABORT ){` |
|         - |  3738 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3739 | `			return SXERR_ABORT;` |
|         - |  3740 | `		}` |
|         9 |  3741 | `		goto Synchronize;` |
|         - |  3742 | `	}` |
|        37 |  3743 | `	pGen->pIn++;` |
|        37 |  3744 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3745 | `		/* Invalid statement*/` |
|         6 |  3746 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|         6 |  3747 | `		if( rc == SXERR_ABORT ){` |
|         - |  3748 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3749 | `			return SXERR_ABORT;` |
|         - |  3750 | `		}` |
|         6 |  3751 | `		goto Synchronize;` |
|         - |  3752 | `	}` |
|        32 |  3753 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3754 | `	/* Allocate a new constant value container */` |
|        32 |  3755 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        32 |  3756 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3757 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3758 | `		return SXERR_ABORT;` |
|         - |  3759 | `	}` |
|        32 |  3760 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3761 | `	/* Swap bytecode container */` |
|        32 |  3762 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        32 |  3763 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3764 | `	/* Compile constant value */` |
|        32 |  3765 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3766 | `	/* Emit the done instruction */` |
|        32 |  3767 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        32 |  3768 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        32 |  3769 | `	if( rc == SXERR_ABORT ){` |
|         - |  3770 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3771 | `		return SXERR_ABORT;` |
|         - |  3772 | `	}` |
|        32 |  3773 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3774 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3775 | `	{` |
|         - |  3776 | `		SyBlob sFQN;` |
|         - |  3777 | `		SyString sFQNStr;` |
|        32 |  3778 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        32 |  3779 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        32 |  3780 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        47 |  3781 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        30 |  3782 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        32 |  3783 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3784 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3785 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3786 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3787 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3788 | `			if( pCEntry ){` |
|         5 |  3789 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3790 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3791 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3792 | `					return SXERR_ABORT;` |
|         - |  3793 | `				}` |
|         2 |  3794 | `			}` |
|         2 |  3795 | `		}` |
|        32 |  3796 | `		SyBlobRelease(&sFQN);` |
|         - |  3797 | `	}` |
|        32 |  3798 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3799 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3800 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3801 | `	}` |
|        32 |  3802 | `	return SXRET_OK;` |
|         9 |  3803 | `Synchronize:` |
|         - |  3804 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3805 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        41 |  3806 | `		pGen->pIn++;` |
|         3 |  3807 | `	}` |
|        22 |  3808 | `	return SXRET_OK;` |
|        29 |  3809 | `}` |
|         - |  3810 | `/*` |
|         - |  3811 | ` * Compile the 'continue' statement.` |
|         - |  3812 | ` * According to the PHP language reference` |
|         - |  3813 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3814 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3815 | ` *  iteration.` |
|         - |  3816 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3817 | ` *  the purposes of continue.` |
|         - |  3818 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3819 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3820 | ` *  Note:` |
|         - |  3821 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3822 | ` */` |
|         - |  3823 | `/*` |
|         - |  3824 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3825 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3826 | ` * break/continue crosses a try boundary.` |
|         - |  3827 | ` *` |
|         - |  3828 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3829 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3830 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3831 | ` */` |
|    145010 |  3832 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3833 | `{` |
|    145015 |  3834 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    145015 |  3835 | `	int nInlineTry = 0;` |
|    656069 |  3836 | `	while( pBlock && pBlock != pTarget ){` |
|    511059 |  3837 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3838 | `			if( pBlock->pUserData ){` |
|         - |  3839 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3840 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3841 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3842 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3843 | `				if( pGen->bInGenerator ){` |
|         3 |  3844 | `					nInlineTry++;` |
|         2 |  3845 | `				}else{` |
|         3 |  3846 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3847 | `				}` |
|         4 |  3848 | `			}else{` |
|         - |  3849 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3850 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3851 | `				break;` |
|         - |  3852 | `			}` |
|         2 |  3853 | `		}` |
|    511059 |  3854 | `		pBlock = pBlock->pParent;` |
|         5 |  3855 | `	}` |
|    145015 |  3856 | `	return nInlineTry;` |
|         5 |  3857 | `}` |
|     83914 |  3858 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3859 | `{` |
|         - |  3860 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3861 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3862 | `	sxu32 nLineLocal;` |
|         - |  3863 | `	sxi32 rc;` |
|     83919 |  3864 | `	nLineLocal = pGen->pIn->nLine;` |
|     83919 |  3865 | `	iLevel = 0;` |
|         - |  3866 | `	/* Jump the 'continue' keyword */` |
|     83919 |  3867 | `	pGen->pIn++;` |
|     83919 |  3868 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3869 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3870 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3871 | `		 */` |
|         - |  3872 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3873 | `		char *zAlloc = 0;` |
|         - |  3874 | `		SyString sNum;` |
|        17 |  3875 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3876 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3877 | `			return SXERR_ABORT;` |
|         - |  3878 | `		}` |
|        17 |  3879 | `		if( rc == SXRET_OK ){` |
|        20 |  3880 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3881 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3882 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3883 | `				return SXERR_ABORT;` |
|         - |  3884 | `			}` |
|        14 |  3885 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3886 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3887 | `		}` |
|        17 |  3888 | `		if( iLevel < 2 ){` |
|         3 |  3889 | `			iLevel = 0;` |
|         1 |  3890 | `		}` |
|        17 |  3891 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3892 | `	}` |
|         - |  3893 | `	/* Point to the target loop */` |
|     83919 |  3894 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     83919 |  3895 | `	if( pLoop == 0 ){` |
|         - |  3896 | `		/* Illegal continue */` |
|        12 |  3897 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        12 |  3898 | `		if( rc == SXERR_ABORT ){` |
|         - |  3899 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3900 | `			return SXERR_ABORT;` |
|         - |  3901 | `		}` |
|         7 |  3902 | `	}else{` |
|     83909 |  3903 | `		sxu32 nInstrIdx = 0;` |
|         - |  3904 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     83909 |  3905 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3906 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3907 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     83909 |  3908 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     83909 |  3909 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3910 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|         - |  3911 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|         - |  3912 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|         - |  3913 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|         5 |  3914 | `			if( iLevel < 1 ){` |
|         5 |  3915 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|         - |  3916 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|         - |  3917 | `					" Did you mean to use \"continue 2\"?");` |
|         2 |  3918 | `			}` |
|         5 |  3919 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3920 | `			if( rc == SXRET_OK ){` |
|         5 |  3921 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3922 | `			}` |
|         3 |  3923 | `		}else{` |
|         - |  3924 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     83905 |  3925 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     83905 |  3926 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  3927 | `				JumpFixup sJumpFix;` |
|         - |  3928 | `				/* Post-continue */` |
|     26701 |  3929 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     26701 |  3930 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     26701 |  3931 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     13348 |  3932 | `			}` |
|         - |  3933 | `		}` |
|         - |  3934 | `	}` |
|     83919 |  3935 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3936 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3937 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  3938 | `	}` |
|         - |  3939 | `	/* Statement successfully compiled */` |
|     83919 |  3940 | `	return SXRET_OK;` |
|     41962 |  3941 | `}` |
|         - |  3942 | `/*` |
|         - |  3943 | ` * Compile the 'break' statement.` |
|         - |  3944 | ` * According to the PHP language reference` |
|         - |  3945 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  3946 | ` *  structure.` |
|         - |  3947 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  3948 | ` *  enclosing structures are to be broken out of.` |
|         - |  3949 | ` */` |
|     61122 |  3950 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  3951 | `{` |
|         - |  3952 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3953 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3954 | `	sxi32 rc;` |
|     61127 |  3955 | `	iLevel = 0;` |
|         - |  3956 | `	/* Jump the 'break' keyword */` |
|     61127 |  3957 | `	pGen->pIn++;` |
|     61127 |  3958 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3959 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3960 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3961 | `		 */` |
|         - |  3962 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3963 | `		char *zAlloc = 0;` |
|         - |  3964 | `		SyString sNum;` |
|        17 |  3965 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3966 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3967 | `			return SXERR_ABORT;` |
|         - |  3968 | `		}` |
|        17 |  3969 | `		if( rc == SXRET_OK ){` |
|        20 |  3970 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3971 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3972 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3973 | `				return SXERR_ABORT;` |
|         - |  3974 | `			}` |
|        14 |  3975 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3976 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3977 | `		}` |
|        17 |  3978 | `		if( iLevel < 2 ){` |
|         3 |  3979 | `			iLevel = 0;` |
|         1 |  3980 | `		}` |
|        17 |  3981 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3982 | `	}` |
|         - |  3983 | `	/* Extract the target loop */` |
|     61127 |  3984 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     61127 |  3985 | `	if( pLoop == 0 ){` |
|         - |  3986 | `		/* Illegal break */` |
|        18 |  3987 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        18 |  3988 | `		if( rc == SXERR_ABORT ){` |
|         - |  3989 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3990 | `			return SXERR_ABORT;` |
|         - |  3991 | `		}` |
|        10 |  3992 | `	}else{` |
|         - |  3993 | `		sxu32 nInstrIdx;` |
|         - |  3994 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     61111 |  3995 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3996 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     61111 |  3997 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     61111 |  3998 | `		if( rc == SXRET_OK ){` |
|         - |  3999 | `			/* Fix the jump later when the jump destination is resolved */` |
|     61111 |  4000 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     30553 |  4001 | `		}` |
|         - |  4002 | `	}` |
|     61127 |  4003 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4004 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4005 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  4006 | `	}` |
|         - |  4007 | `	/* Statement successfully compiled */` |
|     61127 |  4008 | `	return SXRET_OK;` |
|     30566 |  4009 | `}` |
|         - |  4010 | `/*` |
|         - |  4011 | ` * Compile or record a label.` |
|         - |  4012 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  4013 | ` * Example` |
|         - |  4014 | ` *  goto LABEL;` |
|         - |  4015 | ` *   echo 'Foo';` |
|         - |  4016 | ` *  LABEL:` |
|         - |  4017 | ` *   echo 'Bar';` |
|         - |  4018 | ` */` |
|       112 |  4019 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  4020 | `{` |
|         - |  4021 | `	GenBlock *pBlock;` |
|         - |  4022 | `	Label sLabel;` |
|         - |  4023 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|         - |  4024 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|         - |  4025 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|         - |  4026 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|         - |  4027 | `	{` |
|       117 |  4028 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4029 | `		char *zDup;` |
|         - |  4030 | `		/* Initialize label fields */` |
|       117 |  4031 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4032 | `		/* Duplicate label name */` |
|       117 |  4033 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       117 |  4034 | `		if( zDup == 0 ){` |
|       ! 0 |  4035 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4036 | `			return SXERR_ABORT;` |
|         - |  4037 | `		}` |
|       117 |  4038 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       117 |  4039 | `		sLabel.bRef  = FALSE;` |
|       117 |  4040 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       117 |  4041 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       117 |  4042 | `		pBlock = pGen->pCurrent;` |
|       233 |  4043 | `		while( pBlock ){` |
|       143 |  4044 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        27 |  4045 | `				break;` |
|         - |  4046 | `			}` |
|         - |  4047 | `			/* Point to the upper block */` |
|       121 |  4048 | `			pBlock = pBlock->pParent;` |
|         5 |  4049 | `		}` |
|       117 |  4050 | `		if( pBlock ){` |
|        27 |  4051 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        16 |  4052 | `		}else{` |
|        95 |  4053 | `			sLabel.pFunc = 0;` |
|         - |  4054 | `		}` |
|         - |  4055 | `		/* Insert in label set */` |
|       117 |  4056 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  4057 | `	}` |
|       117 |  4058 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  4059 | `	return SXRET_OK;` |
|        61 |  4060 | `}` |
|         - |  4061 | `/*` |
|         - |  4062 | ` * Compile the so hated 'goto' statement.` |
|         - |  4063 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  4064 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  4065 | ` * a compiler it has to do this.` |
|         - |  4066 | ` * According to the PHP language reference manual` |
|         - |  4067 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  4068 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  4069 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  4070 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  4071 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  4072 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  4073 | ` *   of a multi-level break` |
|         - |  4074 | ` */` |
|       152 |  4075 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  4076 | `{` |
|         - |  4077 | `	JumpFixup sJump;` |
|         - |  4078 | `	sxi32 rc;` |
|       157 |  4079 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  4080 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  4081 | `		/* Missing label */` |
|       ! 0 |  4082 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  4083 | `		if( rc == SXERR_ABORT ){` |
|         - |  4084 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4085 | `			return SXERR_ABORT;` |
|         - |  4086 | `		}` |
|       ! 0 |  4087 | `		return SXRET_OK;` |
|         - |  4088 | `	}` |
|       157 |  4089 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         6 |  4090 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|         6 |  4091 | `		if( rc == SXERR_ABORT ){` |
|         - |  4092 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4093 | `			return SXERR_ABORT;` |
|         - |  4094 | `		}` |
|         4 |  4095 | `	}else{` |
|       153 |  4096 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4097 | `		GenBlock *pBlock;` |
|         - |  4098 | `		char *zDup;` |
|         - |  4099 | `		/* Prepare the jump destination */` |
|       153 |  4100 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  4101 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  4102 | `		/* Duplicate label name */` |
|       153 |  4103 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  4104 | `		if( zDup == 0 ){` |
|       ! 0 |  4105 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4106 | `			return SXERR_ABORT;` |
|         - |  4107 | `		}` |
|       153 |  4108 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|         - |  4109 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|       153 |  4110 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|         - |  4111 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|         - |  4112 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|       153 |  4113 | `		pBlock = pGen->pCurrent;` |
|       327 |  4114 | `		while( pBlock ){` |
|       205 |  4115 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|        30 |  4116 | `				break;` |
|         - |  4117 | `			}` |
|         - |  4118 | `			/* Point to the upper block */` |
|       179 |  4119 | `			pBlock = pBlock->pParent;` |
|         5 |  4120 | `		}` |
|       153 |  4121 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        30 |  4122 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        17 |  4123 | `		}else{` |
|       127 |  4124 | `			sJump.pFunc = 0;` |
|         - |  4125 | `		}` |
|         - |  4126 | `		/* Emit the unconditional jump */` |
|       153 |  4127 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  4128 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  4129 | `		}` |
|         - |  4130 | `	}` |
|       157 |  4131 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  4132 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  4133 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  4134 | `	}` |
|         - |  4135 | `	/* Statement successfully compiled */` |
|       157 |  4136 | `	return SXRET_OK;` |
|        81 |  4137 | `}` |
|         - |  4138 | `/*` |
|         - |  4139 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  4140 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  4141 | ` * failure.` |
|         - |  4142 | ` */` |
|        20 |  4143 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         2 |  4144 | `{` |
|         - |  4145 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  4146 | `	sxu32 nRawObj;` |
|        10 |  4147 | `	sxu32 nObjIdx;` |
|         - |  4148 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  4149 | `	 * a PHP block.` |
|         - |  4150 | `	 */` |
|        10 |  4151 | `Consume:` |
|        22 |  4152 | `	nRawObj = nObjIdx = 0;` |
|        22 |  4153 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  4154 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  4155 | `		if( pRawObj == 0 ){` |
|       ! 0 |  4156 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4157 | `			return SXERR_ABORT;` |
|         - |  4158 | `		}` |
|         - |  4159 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  4160 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  4161 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  4162 | `		++nRawObj;` |
|       ! 0 |  4163 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  4164 | `	}` |
|        22 |  4165 | `	if( nRawObj > 0 ){` |
|         - |  4166 | `		/* Emit the consume instruction */` |
|       ! 0 |  4167 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4168 | `	}` |
|        22 |  4169 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  4170 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  4171 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  4172 | `		SySetReset(pTokenSet);` |
|       ! 0 |  4173 | `		SySetReset(&pGen->aTrivia);` |
|         - |  4174 | `		/* Tokenize input */` |
|       ! 0 |  4175 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  4176 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  4177 | `		/* Point to the fresh token stream */` |
|       ! 0 |  4178 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  4179 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  4180 | `		/* Advance the stream cursor */` |
|       ! 0 |  4181 | `		pGen->pRawIn++;` |
|         - |  4182 | `		/* TICKET 1433-011 */` |
|       ! 0 |  4183 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4184 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4185 | `			sxi32 rc;` |
|         - |  4186 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4187 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4188 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4189 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4190 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4191 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4192 | `				return SXERR_ABORT;` |
|       ! 0 |  4193 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4194 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4195 | `			}` |
|       ! 0 |  4196 | `			goto Consume;` |
|         - |  4197 | `		}` |
|       ! 0 |  4198 | `	}else{` |
|         - |  4199 | `		/* No more chunks to process */` |
|        22 |  4200 | `		pGen->pIn = pGen->pEnd;` |
|        22 |  4201 | `		return SXERR_EOF;` |
|         - |  4202 | `	}` |
|       ! 0 |  4203 | `	return SXRET_OK;` |
|        12 |  4204 | `}` |
|         - |  4205 | `/*` |
|         - |  4206 | ` * Compile a PHP block.` |
|         - |  4207 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4208 | ` * optionally delimited by braces {}.` |
|         - |  4209 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4210 | ` * and this function takes care of generating the appropriate error` |
|         - |  4211 | ` * message.` |
|         - |  4212 | ` */` |
|   5858578 |  4213 | `static sxi32 PH7_CompileBlock(` |
|         - |  4214 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4215 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4216 | `	)` |
|         5 |  4217 | `{` |
|         - |  4218 | `	sxi32 rc;` |
|         - |  4219 | `	sxu32 nLine;` |
|   5858583 |  4220 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5857493 |  4221 | `		nLine = pGen->pIn->nLine;` |
|   5857493 |  4222 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5857493 |  4223 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4224 | `			return SXERR_ABORT;` |
|         - |  4225 | `		}` |
|   5857493 |  4226 | `		pGen->pIn++;` |
|         - |  4227 | `		/* Compile until we hit the closing braces '}' */` |
|   8626175 |  4228 | `		for(;;){` |
|  17252355 |  4229 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        22 |  4230 | `				rc = GenStateNextChunk(&(*pGen));` |
|        22 |  4231 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4232 | `			 	   return SXERR_ABORT;` |
|         - |  4233 | `				}` |
|        22 |  4234 | `				if( rc == SXERR_EOF ){` |
|         - |  4235 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4236 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        22 |  4237 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        22 |  4238 | `					break;` |
|         - |  4239 | `				}` |
|       ! 0 |  4240 | `			}` |
|  17252335 |  4241 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4242 | `				/* Closing braces found,break immediately*/` |
|   5857473 |  4243 | `				pGen->pIn++;` |
|   5857473 |  4244 | `				break;` |
|         - |  4245 | `			}` |
|         - |  4246 | `			/* Compile a single statement */` |
|  11394867 |  4247 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  11394867 |  4248 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4249 | `				return SXERR_ABORT;` |
|         - |  4250 | `			}` |
|         5 |  4251 | `		}` |
|   5857493 |  4252 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   2929839 |  4253 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4254 | `		pGen->pIn++;` |
|       ! 0 |  4255 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4256 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4257 | `			return SXERR_ABORT;` |
|         - |  4258 | `		}` |
|         - |  4259 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4260 | `		for(;;){` |
|       ! 0 |  4261 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4262 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4263 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4264 | `			 	   return SXERR_ABORT;` |
|         - |  4265 | `				}` |
|       ! 0 |  4266 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4267 | `					/* No more token to process */` |
|       ! 0 |  4268 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4269 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4270 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4271 | `					}` |
|       ! 0 |  4272 | `					break;` |
|         - |  4273 | `				}` |
|       ! 0 |  4274 | `			}` |
|       ! 0 |  4275 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4276 | `				sxi32 nKwrd;` |
|         - |  4277 | `				/* Keyword found */` |
|       ! 0 |  4278 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4279 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4280 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4281 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4282 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4283 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4284 | `						}` |
|       ! 0 |  4285 | `						break;` |
|         - |  4286 | `				}` |
|       ! 0 |  4287 | `			}` |
|         - |  4288 | `			/* Compile a single statement */` |
|       ! 0 |  4289 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4290 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4291 | `				return SXERR_ABORT;` |
|         - |  4292 | `			}` |
|       ! 0 |  4293 | `		}` |
|       ! 0 |  4294 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4295 | `	}else{` |
|         - |  4296 | `		/* Compile a single statement */` |
|      1095 |  4297 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      1095 |  4298 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4299 | `			return SXERR_ABORT;` |
|         - |  4300 | `		}` |
|         - |  4301 | `	}` |
|         - |  4302 | `	/* Jump trailing semi-colons ';' */` |
|   5858583 |  4303 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4304 | `		pGen->pIn++;` |
|       ! 0 |  4305 | `	}` |
|   5858583 |  4306 | `	return SXRET_OK;` |
|   2929294 |  4307 | `}` |
|         - |  4308 | `/*` |
|         - |  4309 | ` * Compile the gentle 'while' statement.` |
|         - |  4310 | ` * According to the PHP language reference` |
|         - |  4311 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4312 | ` *  The basic form of a while statement is:` |
|         - |  4313 | ` *  while (expr)` |
|         - |  4314 | ` *   statement` |
|         - |  4315 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4316 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4317 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4318 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4319 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4320 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4321 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4322 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4323 | ` *  while (expr):` |
|         - |  4324 | ` *    statement` |
|         - |  4325 | ` *   endwhile;` |
|         - |  4326 | ` */` |
|     64946 |  4327 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4328 | `{` |
|     64951 |  4329 | `	GenBlock *pWhileBlock = 0;` |
|     64951 |  4330 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4331 | `	sxu32 nFalseJump;` |
|         - |  4332 | `	sxu32 nLine;` |
|         - |  4333 | `	sxi32 rc;` |
|     64951 |  4334 | `	nLine = pGen->pIn->nLine;` |
|         - |  4335 | `	/* Jump the 'while' keyword */` |
|     64951 |  4336 | `	pGen->pIn++;` |
|     64951 |  4337 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4338 | `		/* Syntax error */` |
|       ! 0 |  4339 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4340 | `		if( rc == SXERR_ABORT ){` |
|         - |  4341 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4342 | `			return SXERR_ABORT;` |
|         - |  4343 | `		}` |
|       ! 0 |  4344 | `		goto Synchronize;` |
|         - |  4345 | `	}` |
|         - |  4346 | `	/* Jump the left parenthesis '(' */` |
|     64951 |  4347 | `	pGen->pIn++;` |
|         - |  4348 | `	/* Create the loop block */` |
|     64951 |  4349 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     64951 |  4350 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4351 | `		return SXERR_ABORT;` |
|         - |  4352 | `	}` |
|         - |  4353 | `	/* Delimit the condition */` |
|     64951 |  4354 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     64951 |  4355 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4356 | `		/* Empty expression */` |
|         3 |  4357 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4358 | `		if( rc == SXERR_ABORT ){` |
|         - |  4359 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4360 | `			return SXERR_ABORT;` |
|         - |  4361 | `		}` |
|         1 |  4362 | `	}` |
|         - |  4363 | `	/* Swap token streams */` |
|     64951 |  4364 | `	pTmp = pGen->pEnd;` |
|     64951 |  4365 | `	pGen->pEnd = pEnd;` |
|         - |  4366 | `	/* Compile the expression */` |
|     64951 |  4367 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     64951 |  4368 | `	if( rc == SXERR_ABORT ){` |
|         - |  4369 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4370 | `		return SXERR_ABORT;` |
|         - |  4371 | `	}` |
|         - |  4372 | `	/* Update token stream */` |
|     64951 |  4373 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4374 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4375 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4376 | `			return SXERR_ABORT;` |
|         - |  4377 | `		}` |
|       ! 0 |  4378 | `		pGen->pIn++;` |
|       ! 0 |  4379 | `	}` |
|         - |  4380 | `	/* Synchronize pointers */` |
|     64951 |  4381 | `	pGen->pIn  = &pEnd[1];` |
|     64951 |  4382 | `	pGen->pEnd = pTmp;` |
|         - |  4383 | `	/* Emit the false jump */` |
|     64951 |  4384 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4385 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     64951 |  4386 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4387 | `	/* Compile the loop body */` |
|     64951 |  4388 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     64951 |  4389 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4390 | `		return SXERR_ABORT;` |
|         - |  4391 | `	}` |
|         - |  4392 | `	/* Emit the unconditional jump to the start of the loop */` |
|     64951 |  4393 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4394 | `	/* Fix all jumps now the destination is resolved */` |
|     64951 |  4395 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4396 | `	/* Release the loop block */` |
|     64951 |  4397 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4398 | `	/* Statement successfully compiled */` |
|     64951 |  4399 | `	return SXRET_OK;` |
|       ! 0 |  4400 | `Synchronize:` |
|         - |  4401 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4402 | `	 * compiling this erroneous block.` |
|         - |  4403 | `	 */` |
|       ! 0 |  4404 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4405 | `		pGen->pIn++;` |
|       ! 0 |  4406 | `	}` |
|       ! 0 |  4407 | `	return SXRET_OK;` |
|     32478 |  4408 | `}` |
|         - |  4409 | `/*` |
|         - |  4410 | ` * Compile the ugly do..while() statement.` |
|         - |  4411 | ` * According to the PHP language reference` |
|         - |  4412 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4413 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4414 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4415 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4416 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4417 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4418 | ` *  would end immediately).` |
|         - |  4419 | ` *  There is just one syntax for do-while loops:` |
|         - |  4420 | ` *  <?php` |
|         - |  4421 | ` *  $i = 0;` |
|         - |  4422 | ` *  do {` |
|         - |  4423 | ` *   echo $i;` |
|         - |  4424 | ` *  } while ($i > 0);` |
|         - |  4425 | ` * ?>` |
|         - |  4426 | ` */` |
|         2 |  4427 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4428 | `{` |
|         3 |  4429 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4430 | `	GenBlock *pDoBlock = 0;` |
|         - |  4431 | `	sxu32 nLine;` |
|         - |  4432 | `	sxi32 rc;` |
|         3 |  4433 | `	nLine = pGen->pIn->nLine;` |
|         - |  4434 | `	/* Jump the 'do' keyword */` |
|         3 |  4435 | `	pGen->pIn++;` |
|         - |  4436 | `	/* Create the loop block */` |
|         3 |  4437 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4438 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4439 | `		return SXERR_ABORT;` |
|         - |  4440 | `	}` |
|         - |  4441 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4442 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4443 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4444 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4445 | `		return SXERR_ABORT;` |
|         - |  4446 | `	}` |
|         3 |  4447 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4448 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4449 | `	}` |
|         3 |  4450 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4451 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4452 | `			/* Missing 'while' statement */` |
|         3 |  4453 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4454 | `			if( rc == SXERR_ABORT ){` |
|         - |  4455 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4456 | `				return SXERR_ABORT;` |
|         - |  4457 | `			}` |
|         3 |  4458 | `			goto Synchronize;` |
|         - |  4459 | `	}` |
|         - |  4460 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4461 | `	pGen->pIn++;` |
|       ! 0 |  4462 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4463 | `		/* Syntax error */` |
|       ! 0 |  4464 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4465 | `		if( rc == SXERR_ABORT ){` |
|         - |  4466 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4467 | `			return SXERR_ABORT;` |
|         - |  4468 | `		}` |
|       ! 0 |  4469 | `		goto Synchronize;` |
|         - |  4470 | `	}` |
|         - |  4471 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4472 | `	pGen->pIn++;` |
|         - |  4473 | `	/* Delimit the condition */` |
|       ! 0 |  4474 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4475 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4476 | `		/* Empty expression */` |
|       ! 0 |  4477 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4478 | `		if( rc == SXERR_ABORT ){` |
|         - |  4479 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4480 | `			return SXERR_ABORT;` |
|         - |  4481 | `		}` |
|       ! 0 |  4482 | `		goto Synchronize;` |
|         - |  4483 | `	}` |
|         - |  4484 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4485 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4486 | `		JumpFixup *aPost;` |
|         - |  4487 | `		VmInstr *pInstr;` |
|         - |  4488 | `		sxu32 nJumpDest;` |
|         - |  4489 | `		sxu32 n;` |
|       ! 0 |  4490 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4491 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4492 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4493 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4494 | `			if( pInstr ){` |
|         - |  4495 | `				/* Fix */` |
|       ! 0 |  4496 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4497 | `			}` |
|       ! 0 |  4498 | `		}` |
|       ! 0 |  4499 | `	}` |
|         - |  4500 | `	/* Swap token streams */` |
|       ! 0 |  4501 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4502 | `	pGen->pEnd = pEnd;` |
|         - |  4503 | `	/* Compile the expression */` |
|       ! 0 |  4504 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4505 | `	if( rc == SXERR_ABORT ){` |
|         - |  4506 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4507 | `		return SXERR_ABORT;` |
|         - |  4508 | `	}` |
|         - |  4509 | `	/* Update token stream */` |
|       ! 0 |  4510 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4511 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4512 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4513 | `			return SXERR_ABORT;` |
|         - |  4514 | `		}` |
|       ! 0 |  4515 | `		pGen->pIn++;` |
|       ! 0 |  4516 | `	}` |
|       ! 0 |  4517 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4518 | `	pGen->pEnd = pTmp;` |
|         - |  4519 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4520 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4521 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4522 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4523 | `	/* Release the loop block */` |
|       ! 0 |  4524 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4525 | `	/* Statement successfully compiled */` |
|       ! 0 |  4526 | `	return SXRET_OK;` |
|         1 |  4527 | `Synchronize:` |
|         - |  4528 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4529 | `	 * compiling this erroneous block.` |
|         - |  4530 | `	 */` |
|         3 |  4531 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4532 | `		pGen->pIn++;` |
|       ! 0 |  4533 | `	}` |
|         3 |  4534 | `	return SXRET_OK;` |
|         2 |  4535 | `}` |
|         - |  4536 | `/*` |
|         - |  4537 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4538 | ` * According to the PHP language reference` |
|         - |  4539 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4540 | ` *  The syntax of a for loop is:` |
|         - |  4541 | ` *  for (expr1; expr2; expr3)` |
|         - |  4542 | ` *   statement` |
|         - |  4543 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4544 | ` *  the beginning of the loop.` |
|         - |  4545 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4546 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4547 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4548 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4549 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4550 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4551 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4552 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4553 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4554 | ` *  of using the for truth expression.` |
|         - |  4555 | ` */` |
|    114504 |  4556 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4557 | `{` |
|    114509 |  4558 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    114509 |  4559 | `	GenBlock *pForBlock = 0;` |
|         - |  4560 | `	sxu32 nFalseJump;` |
|         - |  4561 | `	sxu32 nLine;` |
|         - |  4562 | `	sxi32 rc;` |
|    114509 |  4563 | `	nLine = pGen->pIn->nLine;` |
|         - |  4564 | `	/* Jump the 'for' keyword */` |
|    114509 |  4565 | `	pGen->pIn++;` |
|    114509 |  4566 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4567 | `		/* Syntax error */` |
|       ! 0 |  4568 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4569 | `		if( rc == SXERR_ABORT ){` |
|         - |  4570 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4571 | `			return SXERR_ABORT;` |
|         - |  4572 | `		}` |
|       ! 0 |  4573 | `		return SXRET_OK;` |
|         - |  4574 | `	}` |
|         - |  4575 | `	/* Jump the left parenthesis '(' */` |
|    114509 |  4576 | `	pGen->pIn++;` |
|         - |  4577 | `	/* Delimit the init-expr;condition;post-expr */` |
|    114509 |  4578 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    114509 |  4579 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4580 | `		/* Empty expression */` |
|       ! 0 |  4581 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4582 | `		if( rc == SXERR_ABORT ){` |
|         - |  4583 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4584 | `			return SXERR_ABORT;` |
|         - |  4585 | `		}` |
|         - |  4586 | `		/* Synchronize */` |
|       ! 0 |  4587 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4588 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4589 | `			pGen->pIn++;` |
|       ! 0 |  4590 | `		}` |
|       ! 0 |  4591 | `		return SXRET_OK;` |
|         - |  4592 | `	}` |
|         - |  4593 | `	/* Swap token streams */` |
|    114509 |  4594 | `	pTmp = pGen->pEnd;` |
|    114509 |  4595 | `	pGen->pEnd = pEnd;` |
|         - |  4596 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4597 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4598 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4599 | `	 * compiled through this same window — recorded as a known leniency. */` |
|    114509 |  4600 | `	pGen->nCommaExprOk++;` |
|         - |  4601 | `	/* Compile initialization expressions if available */` |
|    114509 |  4602 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4603 | `	/* Pop operand lvalues */` |
|    114509 |  4604 | `	if( rc == SXERR_ABORT ){` |
|         - |  4605 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4606 | `		return SXERR_ABORT;` |
|    114509 |  4607 | `	}else if( rc != SXERR_EMPTY ){` |
|    103071 |  4608 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     51533 |  4609 | `	}` |
|    114509 |  4610 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4611 | `		/* Syntax error */` |
|       ! 0 |  4612 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4613 | `		if( rc == SXERR_ABORT ){` |
|         - |  4614 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4615 | `			return SXERR_ABORT;` |
|         - |  4616 | `		}` |
|       ! 0 |  4617 | `		return SXRET_OK;` |
|         - |  4618 | `	}` |
|         - |  4619 | `	/* Jump the trailing ';' */` |
|    114509 |  4620 | `	pGen->pIn++;` |
|         - |  4621 | `	/* Create the loop block */` |
|    114509 |  4622 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    114509 |  4623 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4624 | `		return SXERR_ABORT;` |
|         - |  4625 | `	}` |
|         - |  4626 | `	/* Deffer continue jumps */` |
|    114509 |  4627 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4628 | `	/* Compile the condition */` |
|    114509 |  4629 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    114509 |  4630 | `	if( rc == SXERR_ABORT ){` |
|         - |  4631 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4632 | `		return SXERR_ABORT;` |
|    114509 |  4633 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4634 | `		/* Emit the false jump */` |
|    103071 |  4635 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4636 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    103071 |  4637 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     51533 |  4638 | `	}` |
|    114509 |  4639 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4640 | `		/* Syntax error */` |
|         6 |  4641 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4642 | `		if( rc == SXERR_ABORT ){` |
|         - |  4643 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4644 | `			return SXERR_ABORT;` |
|         - |  4645 | `		}` |
|         6 |  4646 | `		return SXRET_OK;` |
|         - |  4647 | `	}` |
|         - |  4648 | `	/* Jump the trailing ';' */` |
|    114505 |  4649 | `	pGen->pIn++;` |
|         - |  4650 | `	/* Save the post condition stream */` |
|    114505 |  4651 | `	pPostStart = pGen->pIn;` |
|         - |  4652 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4653 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|    114505 |  4654 | `	pGen->nCommaExprOk--;` |
|    114505 |  4655 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    114505 |  4656 | `	pGen->pEnd = pTmp;` |
|    114505 |  4657 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    114505 |  4658 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4659 | `		return SXERR_ABORT;` |
|         - |  4660 | `	}` |
|         - |  4661 | `	/* Fix post-continue jumps */` |
|    114505 |  4662 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4663 | `		JumpFixup *aPost;` |
|         - |  4664 | `		VmInstr *pInstr;` |
|         - |  4665 | `		sxu32 nJumpDest;` |
|         - |  4666 | `		sxu32 n;` |
|     11453 |  4667 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|     11453 |  4668 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     38149 |  4669 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     26701 |  4670 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     26701 |  4671 | `			if( pInstr ){` |
|         - |  4672 | `				/* Fix jump */` |
|     26701 |  4673 | `				pInstr->iP2 = nJumpDest;` |
|     13348 |  4674 | `			}` |
|     13353 |  4675 | `		}` |
|      5724 |  4676 | `	}` |
|         - |  4677 | `	/* compile the post-expressions if available */` |
|    114505 |  4678 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4679 | `		pPostStart++;` |
|       ! 0 |  4680 | `	}` |
|    114505 |  4681 | `	if( pPostStart < pEnd ){` |
|         - |  4682 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    103069 |  4683 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    103069 |  4684 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|    103069 |  4685 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    103069 |  4686 | `		pGen->nCommaExprOk--;` |
|    103069 |  4687 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4688 | `			/* Syntax error */` |
|       ! 0 |  4689 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4690 | `			if( rc == SXERR_ABORT ){` |
|         - |  4691 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4692 | `				return SXERR_ABORT;` |
|         - |  4693 | `			}` |
|       ! 0 |  4694 | `			return SXRET_OK;` |
|         - |  4695 | `		}` |
|    103069 |  4696 | `		RE_SWAP_DELIMITER(pGen);` |
|    103069 |  4697 | `		if( rc == SXERR_ABORT ){` |
|         - |  4698 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4699 | `			return SXERR_ABORT;` |
|    103069 |  4700 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4701 | `			/* Pop operand lvalue */` |
|    103069 |  4702 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     51532 |  4703 | `		}` |
|     51532 |  4704 | `	}` |
|         - |  4705 | `	/* Emit the unconditional jump to the start of the loop */` |
|    114505 |  4706 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4707 | `	/* Fix all jumps now the destination is resolved */` |
|    114505 |  4708 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4709 | `	/* Release the loop block */` |
|    114505 |  4710 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4711 | `	/* Statement successfully compiled */` |
|    114505 |  4712 | `	return SXRET_OK;` |
|     57257 |  4713 | `}` |
|         - |  4714 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4715 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4716 | ` * are allowed.` |
|         - |  4717 | ` */` |
|    427966 |  4718 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4719 | `{` |
|    427971 |  4720 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    427971 |  4721 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4722 | `		/* Unexpected expression */` |
|       ! 0 |  4723 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4724 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4725 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4726 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4727 | `		}` |
|       ! 0 |  4728 | `	}` |
|    427971 |  4729 | `	return rc;` |
|         5 |  4730 | `}` |
|         - |  4731 | `/*` |
|         - |  4732 | ` * Compile the 'foreach' statement.` |
|         - |  4733 | ` * According to the PHP language reference` |
|         - |  4734 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4735 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4736 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4737 | ` *  is a minor but useful extension of the first:` |
|         - |  4738 | ` *  foreach (array_expression as $value)` |
|         - |  4739 | ` *    statement` |
|         - |  4740 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4741 | ` *   statement` |
|         - |  4742 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4743 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4744 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4745 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4746 | ` *  to the variable $key on each loop.` |
|         - |  4747 | ` *  Note:` |
|         - |  4748 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4749 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4750 | ` *  Note:` |
|         - |  4751 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4752 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4753 | ` *  or after the foreach without resetting it.` |
|         - |  4754 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4755 | ` *  of copying the value.` |
|         - |  4756 | ` */` |
|    298082 |  4757 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4758 | `{` |
|    298087 |  4759 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    298087 |  4760 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    298087 |  4761 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4762 | `	ph7_foreach_info *pInfo;` |
|         - |  4763 | `	sxu32 nFalseJump;` |
|         - |  4764 | `	VmInstr *pInstr;` |
|         - |  4765 | `	sxu32 nLine;` |
|         - |  4766 | `	sxi32 rc;` |
|    298087 |  4767 | `	nLine = pGen->pIn->nLine;` |
|         - |  4768 | `	/* Jump the 'foreach' keyword */` |
|    298087 |  4769 | `	pGen->pIn++;` |
|    298087 |  4770 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4771 | `		/* Syntax error */` |
|       ! 0 |  4772 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4773 | `		if( rc == SXERR_ABORT ){` |
|         - |  4774 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4775 | `			return SXERR_ABORT;` |
|         - |  4776 | `		}` |
|       ! 0 |  4777 | `		goto Synchronize;` |
|         - |  4778 | `	}` |
|         - |  4779 | `	/* Jump the left parenthesis '(' */` |
|    298087 |  4780 | `	pGen->pIn++;` |
|         - |  4781 | `	/* Create the loop block */` |
|    298087 |  4782 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    298087 |  4783 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4784 | `		return SXERR_ABORT;` |
|         - |  4785 | `	}` |
|         - |  4786 | `	/* Delimit the expression */` |
|    298087 |  4787 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    298087 |  4788 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4789 | `		/* Empty expression */` |
|       ! 0 |  4790 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4791 | `		if( rc == SXERR_ABORT ){` |
|         - |  4792 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4793 | `			return SXERR_ABORT;` |
|         - |  4794 | `		}` |
|         - |  4795 | `		/* Synchronize */` |
|       ! 0 |  4796 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4797 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4798 | `			pGen->pIn++;` |
|       ! 0 |  4799 | `		}` |
|       ! 0 |  4800 | `		return SXRET_OK;` |
|         - |  4801 | `	}` |
|         - |  4802 | `	/* Compile the array expression */` |
|    298087 |  4803 | `	pCur = pGen->pIn;` |
|   1719765 |  4804 | `	while( pCur < pEnd ){` |
|   1719765 |  4805 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    328597 |  4806 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    328597 |  4807 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4808 | `				/* Break with the first 'as' found */` |
|    298087 |  4809 | `				break;` |
|         - |  4810 | `			}` |
|     15255 |  4811 | `		}` |
|         - |  4812 | `		/* Advance the stream cursor */` |
|   1421683 |  4813 | `		pCur++;` |
|         5 |  4814 | `	}` |
|    298087 |  4815 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4816 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4817 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4818 | `		if( rc == SXERR_ABORT ){` |
|         - |  4819 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4820 | `			return SXERR_ABORT;` |
|         - |  4821 | `		}` |
|       ! 0 |  4822 | `		goto Synchronize;` |
|         - |  4823 | `	}` |
|         - |  4824 | `	/* Swap token streams */` |
|    298087 |  4825 | `	pTmp = pGen->pEnd;` |
|    298087 |  4826 | `	pGen->pEnd = pCur;` |
|    298087 |  4827 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    298087 |  4828 | `	if( rc == SXERR_ABORT ){` |
|         - |  4829 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4830 | `		return SXERR_ABORT;` |
|         - |  4831 | `	}` |
|         - |  4832 | `	/* Update token stream */` |
|    298087 |  4833 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4834 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4835 | `		if( rc == SXERR_ABORT ){` |
|         - |  4836 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4837 | `			return SXERR_ABORT;` |
|         - |  4838 | `		}` |
|       ! 0 |  4839 | `		pGen->pIn++;` |
|       ! 0 |  4840 | `	}` |
|    298087 |  4841 | `	pCur++; /* Jump the 'as' keyword */` |
|    298087 |  4842 | `	pGen->pIn = pCur;` |
|    298087 |  4843 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4844 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4845 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4846 | `			return SXERR_ABORT;` |
|         - |  4847 | `		}` |
|       ! 0 |  4848 | `	}` |
|         - |  4849 | `	/* Create the foreach context */` |
|    298087 |  4850 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    298087 |  4851 | `	if( pInfo == 0 ){` |
|       ! 0 |  4852 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4853 | `		return SXERR_ABORT;` |
|         - |  4854 | `	}` |
|         - |  4855 | `	/* Zero the structure */` |
|    298087 |  4856 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4857 | `	/* Initialize structure fields */` |
|    298087 |  4858 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4859 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4860 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4861 | `	 * '=>'. */` |
|    298087 |  4862 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    298087 |  4863 | `	if( pCur < pEnd ){` |
|         - |  4864 | `		/* Compile the expression holding the key name */` |
|    129909 |  4865 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4866 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4867 | `			if( rc == SXERR_ABORT ){` |
|         - |  4868 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4869 | `				return SXERR_ABORT;` |
|         - |  4870 | `			}` |
|       ! 0 |  4871 | `		}else{` |
|    129909 |  4872 | `			pGen->pEnd = pCur;` |
|    129909 |  4873 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    129909 |  4874 | `			if( rc == SXERR_ABORT ){` |
|         - |  4875 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4876 | `				return SXERR_ABORT;` |
|         - |  4877 | `			}` |
|    129909 |  4878 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    129909 |  4879 | `			if( pInstr->p3 ){` |
|         - |  4880 | `				/* Record key name */` |
|    129909 |  4881 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     64952 |  4882 | `			}` |
|    129909 |  4883 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4884 | `		}` |
|    129909 |  4885 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     64952 |  4886 | `	}` |
|    298087 |  4887 | `	pGen->pEnd = pEnd;` |
|    298087 |  4888 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4889 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4890 | `		if( rc == SXERR_ABORT ){` |
|         - |  4891 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4892 | `			return SXERR_ABORT;` |
|         - |  4893 | `		}` |
|       ! 0 |  4894 | `		goto Synchronize;` |
|         - |  4895 | `	}` |
|    298087 |  4896 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4897 | `		pGen->pIn++;` |
|         - |  4898 | `		/* Pass by reference  */` |
|        33 |  4899 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4900 | `	}` |
|         - |  4901 | `	/* Check if the value target is list() */` |
|    298087 |  4902 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4903 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4904 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4905 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4906 | `		 */` |
|         - |  4907 | `		static int iForeachListCnt = 0;` |
|         - |  4908 | `		char zTmp[128];` |
|         - |  4909 | `		sxu32 nLen;` |
|         - |  4910 | `		char *zDup;` |
|        10 |  4911 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4912 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4913 | `		if( zDup == 0 ){` |
|       ! 0 |  4914 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4915 | `			return SXERR_ABORT;` |
|         - |  4916 | `		}` |
|        10 |  4917 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4918 | `		/* Save list() token boundaries */` |
|        10 |  4919 | `		pListStart = pGen->pIn;` |
|         - |  4920 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4921 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4922 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4923 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  4924 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  4925 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4926 | `				return SXERR_ABORT;` |
|         - |  4927 | `			}` |
|         3 |  4928 | `			goto Synchronize;` |
|         - |  4929 | `		}` |
|         7 |  4930 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  4931 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  4932 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4933 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4934 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  4935 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4936 | `				return SXERR_ABORT;` |
|         - |  4937 | `			}` |
|       ! 0 |  4938 | `			goto Synchronize;` |
|         - |  4939 | `		}` |
|         7 |  4940 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  4941 | `		pListEnd = pGen->pIn;` |
|         7 |  4942 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    298082 |  4943 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  4944 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  4945 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  4946 | `		 */` |
|         - |  4947 | `		static int iForeachShortListCnt = 0;` |
|         - |  4948 | `		char zTmp[128];` |
|         - |  4949 | `		sxu32 nLen;` |
|         - |  4950 | `		char *zDup;` |
|        13 |  4951 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        13 |  4952 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        13 |  4953 | `		if( zDup == 0 ){` |
|       ! 0 |  4954 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4955 | `			return SXERR_ABORT;` |
|         - |  4956 | `		}` |
|        13 |  4957 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4958 | `		/* Save [...] token boundaries */` |
|        13 |  4959 | `		pListStart = pGen->pIn;` |
|         - |  4960 | `		/* Advance past [...] */` |
|        13 |  4961 | `		pGen->pIn++; /* Jump '[' */` |
|        13 |  4962 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        13 |  4963 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4964 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4965 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  4966 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4967 | `				return SXERR_ABORT;` |
|         - |  4968 | `			}` |
|       ! 0 |  4969 | `			goto Synchronize;` |
|         - |  4970 | `		}` |
|        13 |  4971 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        13 |  4972 | `		pListEnd = pGen->pIn;` |
|        13 |  4973 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         7 |  4974 | `	}else{` |
|         - |  4975 | `		/* Compile the expression holding the value name */` |
|    298067 |  4976 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    298067 |  4977 | `		if( rc == SXERR_ABORT ){` |
|         - |  4978 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4979 | `			return SXERR_ABORT;` |
|         - |  4980 | `		}` |
|    298067 |  4981 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    298067 |  4982 | `		if( pInstr->p3 ){` |
|         - |  4983 | `			/* Record value name */` |
|    298067 |  4984 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    149031 |  4985 | `		}` |
|         - |  4986 | `	}` |
|         - |  4987 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    298085 |  4988 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  4989 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    298085 |  4990 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  4991 | `	/* Record the first instruction to execute */` |
|    298085 |  4992 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4993 | `	/* Emit the FOREACH_STEP instruction */` |
|    298085 |  4994 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  4995 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    298085 |  4996 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  4997 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    298085 |  4998 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  4999 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  5000 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  5001 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  5002 | `		 */` |
|        19 |  5003 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  5004 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  5005 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  5006 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  5007 | `		 */` |
|        19 |  5008 | `		pSavedIn = pGen->pIn;` |
|        19 |  5009 | `		pSavedEnd = pGen->pEnd;` |
|        19 |  5010 | `		pGen->pIn = pListStart;` |
|        19 |  5011 | `		pGen->pEnd = pListEnd;` |
|        19 |  5012 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        13 |  5013 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  5014 | `		}else{` |
|         7 |  5015 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  5016 | `		}` |
|        19 |  5017 | `		pGen->pIn = pSavedIn;` |
|        19 |  5018 | `		pGen->pEnd = pSavedEnd;` |
|        19 |  5019 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5020 | `			return SXERR_ABORT;` |
|         - |  5021 | `		}` |
|         - |  5022 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        19 |  5023 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         9 |  5024 | `	}` |
|         - |  5025 | `	/* Compile the loop body */` |
|    298085 |  5026 | `	pGen->pIn = &pEnd[1];` |
|    298085 |  5027 | `	pGen->pEnd = pTmp;` |
|    298085 |  5028 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    298085 |  5029 | `	if( rc == SXERR_ABORT ){` |
|         - |  5030 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5031 | `		return SXERR_ABORT;` |
|         - |  5032 | `	}` |
|         - |  5033 | `	/* Emit the unconditional jump to the start of the loop */` |
|    298085 |  5034 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  5035 | `	/* Fix all jumps now the destination is resolved */` |
|    298085 |  5036 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  5037 | `	/* Release the loop block */` |
|    298085 |  5038 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5039 | `	/* Statement successfully compiled */` |
|    298085 |  5040 | `	return SXRET_OK;` |
|         1 |  5041 | `Synchronize:` |
|         - |  5042 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  5043 | `	 * compiling this erroneous block.` |
|         - |  5044 | `	 */` |
|         3 |  5045 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  5046 | `		pGen->pIn++;` |
|       ! 0 |  5047 | `	}` |
|         3 |  5048 | `	return SXRET_OK;` |
|    149046 |  5049 | `}` |
|         - |  5050 | `/*` |
|         - |  5051 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  5052 | ` * According to the PHP language reference` |
|         - |  5053 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  5054 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  5055 | ` *  that is similar to that of C:` |
|         - |  5056 | ` *  if (expr)` |
|         - |  5057 | ` *   statement` |
|         - |  5058 | ` *  else construct:` |
|         - |  5059 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  5060 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  5061 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  5062 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  5063 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  5064 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  5065 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  5066 | ` *  elseif` |
|         - |  5067 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  5068 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  5069 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  5070 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  5071 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  5072 | ` *   <?php` |
|         - |  5073 | ` *    if ($a > $b) {` |
|         - |  5074 | ` *     echo "a is bigger than b";` |
|         - |  5075 | ` *    } elseif ($a == $b) {` |
|         - |  5076 | ` *     echo "a is equal to b";` |
|         - |  5077 | ` *    } else {` |
|         - |  5078 | ` *     echo "a is smaller than b";` |
|         - |  5079 | ` *    }` |
|         - |  5080 | ` *    ?>` |
|         - |  5081 | ` */` |
|   2155756 |  5082 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  5083 | `{` |
|   2155761 |  5084 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   2155761 |  5085 | `	GenBlock *pCondBlock = 0;` |
|         - |  5086 | `	sxu32 nJumpIdx;` |
|         - |  5087 | `	sxu32 nKeyID;` |
|         - |  5088 | `	sxi32 rc;` |
|         - |  5089 | `	/* Jump the 'if' keyword */` |
|   2155761 |  5090 | `	pGen->pIn++;` |
|   2155761 |  5091 | `	pToken = pGen->pIn;` |
|         - |  5092 | `	/* Create the conditional block */` |
|   2155761 |  5093 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   2155761 |  5094 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5095 | `		return SXERR_ABORT;` |
|         - |  5096 | `	}` |
|         - |  5097 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1213263 |  5098 | `	for(;;){` |
|   2426531 |  5099 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  5100 | `			/* Syntax error */` |
|       ! 0 |  5101 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5102 | `				pToken--;` |
|       ! 0 |  5103 | `			}` |
|       ! 0 |  5104 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  5105 | `			if( rc == SXERR_ABORT ){` |
|         - |  5106 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5107 | `				return SXERR_ABORT;` |
|         - |  5108 | `			}` |
|       ! 0 |  5109 | `			goto Synchronize;` |
|         - |  5110 | `		}` |
|         - |  5111 | `		/* Jump the left parenthesis '(' */` |
|   2426531 |  5112 | `		pToken++;` |
|         - |  5113 | `		/* Delimit the condition */` |
|   2426531 |  5114 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2426531 |  5115 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  5116 | `			/* Syntax error */` |
|        11 |  5117 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5118 | `				pToken--;` |
|       ! 0 |  5119 | `			}` |
|        11 |  5120 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  5121 | `			if( rc == SXERR_ABORT ){` |
|         - |  5122 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5123 | `				return SXERR_ABORT;` |
|         - |  5124 | `			}` |
|        11 |  5125 | `			goto Synchronize;` |
|         - |  5126 | `		}` |
|         - |  5127 | `		/* Swap token streams */` |
|   2426523 |  5128 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  5129 | `		/* Compile the condition */` |
|   2426523 |  5130 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5131 | `		/* Update token stream */` |
|   2426523 |  5132 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5133 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5134 | `			pGen->pIn++;` |
|       ! 0 |  5135 | `		}` |
|   2426523 |  5136 | `		pGen->pIn  = &pEnd[1];` |
|   2426523 |  5137 | `		pGen->pEnd = pTmp;` |
|   2426523 |  5138 | `		if( rc == SXERR_ABORT ){` |
|         - |  5139 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5140 | `			return SXERR_ABORT;` |
|         - |  5141 | `		}` |
|         - |  5142 | `		/* Emit the false jump */` |
|   2426523 |  5143 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5144 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2426523 |  5145 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5146 | `		/* Compile the body */` |
|   2426523 |  5147 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2426523 |  5148 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5149 | `			return SXERR_ABORT;` |
|         - |  5150 | `		}` |
|   2426523 |  5151 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    473303 |  5152 | `			break;` |
|         - |  5153 | `		}` |
|         - |  5154 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1479927 |  5155 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1479927 |  5156 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   1006671 |  5157 | `			break;` |
|         - |  5158 | `		}` |
|         - |  5159 | `		/* Emit the unconditional jump */` |
|    473261 |  5160 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5161 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    473261 |  5162 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    473261 |  5163 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    286355 |  5164 | `			pToken = &pGen->pIn[1];` |
|    286355 |  5165 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     83902 |  5166 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    101248 |  5167 | `					break;` |
|         - |  5168 | `			}` |
|     83869 |  5169 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     41932 |  5170 | `		}` |
|    270775 |  5171 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5172 | `		/* Synchronize cursors */` |
|    270775 |  5173 | `		pToken = pGen->pIn;` |
|         - |  5174 | `		/* Fix the false jump */` |
|    270775 |  5175 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5176 | `	} /* For(;;) */` |
|         - |  5177 | `	/* Fix the false jump */` |
|   2155753 |  5178 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   2155753 |  5179 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1209152 |  5180 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5181 | `			/* Compile the else block */` |
|    202491 |  5182 | `			pGen->pIn++;` |
|    202491 |  5183 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    202491 |  5184 | `			if( rc == SXERR_ABORT ){` |
|         - |  5185 |  |
|       ! 0 |  5186 | `				return SXERR_ABORT;` |
|         - |  5187 | `			}` |
|    101243 |  5188 | `	}` |
|   2155753 |  5189 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5190 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   2155753 |  5191 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5192 | `	/* Release the conditional block */` |
|   2155753 |  5193 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5194 | `	/* Statement successfully compiled */` |
|   2155753 |  5195 | `	return SXRET_OK;` |
|         4 |  5196 | `Synchronize:` |
|         - |  5197 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5198 | `	 */` |
|        67 |  5199 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5200 | `		pGen->pIn++;` |
|         3 |  5201 | `	}` |
|        11 |  5202 | `	return SXRET_OK;` |
|   1077883 |  5203 | `}` |
|         - |  5204 | `/*` |
|         - |  5205 | ` * Compile the global construct.` |
|         - |  5206 | ` * According to the PHP language reference` |
|         - |  5207 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5208 | ` *  to be used in that function.` |
|         - |  5209 | ` *  Example #1 Using global` |
|         - |  5210 | ` *  <?php` |
|         - |  5211 | ` *   $a = 1;` |
|         - |  5212 | ` *   $b = 2;` |
|         - |  5213 | ` *   function Sum()` |
|         - |  5214 | ` *   {` |
|         - |  5215 | ` *    global $a, $b;` |
|         - |  5216 | ` *    $b = $a + $b;` |
|         - |  5217 | ` *   }` |
|         - |  5218 | ` *   Sum();` |
|         - |  5219 | ` *   echo $b;` |
|         - |  5220 | ` *  ?>` |
|         - |  5221 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5222 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5223 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5224 | ` */` |
|        38 |  5225 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5226 | `{` |
|        43 |  5227 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5228 | `	sxi32 nExpr;` |
|         - |  5229 | `	sxi32 rc;` |
|         - |  5230 | `	/* Jump the 'global' keyword */` |
|        43 |  5231 | `	pGen->pIn++;` |
|        43 |  5232 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5233 | `		/* Nothing to process */` |
|       ! 0 |  5234 | `		return SXRET_OK;` |
|         - |  5235 | `	}` |
|        43 |  5236 | `	pTmp = pGen->pEnd;` |
|        43 |  5237 | `	nExpr = 0;` |
|        91 |  5238 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5239 | `		if( pGen->pIn < pNext ){` |
|        53 |  5240 | `			pGen->pEnd = pNext;` |
|        53 |  5241 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5242 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5243 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5244 | `					return SXERR_ABORT;` |
|         - |  5245 | `				}` |
|       ! 0 |  5246 | `			}else{` |
|        53 |  5247 | `				pGen->pIn++;` |
|        53 |  5248 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5249 | `					/* Emit a warning */` |
|       ! 0 |  5250 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5251 | `				}else{` |
|        53 |  5252 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5253 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5254 | `						return SXERR_ABORT;` |
|        53 |  5255 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5256 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5257 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5258 | `							/* Variable name, not a constant */` |
|        53 |  5259 | `							pLast->iP1 = 0;` |
|        24 |  5260 | `						}` |
|        53 |  5261 | `						nExpr++;` |
|        24 |  5262 | `					}` |
|         - |  5263 | `				}` |
|         - |  5264 | `			}` |
|        24 |  5265 | `		}` |
|         - |  5266 | `		/* Next expression in the stream */` |
|        53 |  5267 | `		pGen->pIn = pNext;` |
|         - |  5268 | `		/* Jump trailing commas */` |
|        63 |  5269 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5270 | `			pGen->pIn++;` |
|         5 |  5271 | `		}` |
|         5 |  5272 | `	}` |
|         - |  5273 | `	/* Restore token stream */` |
|        43 |  5274 | `	pGen->pEnd = pTmp;` |
|        43 |  5275 | `	if( nExpr > 0 ){` |
|         - |  5276 | `		/* Emit the uplink instruction */` |
|        43 |  5277 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5278 | `	}` |
|        43 |  5279 | `	return SXRET_OK;` |
|        24 |  5280 | `}` |
|         - |  5281 | `/*` |
|         - |  5282 | ` * Compile the return statement.` |
|         - |  5283 | ` * According to the PHP language reference` |
|         - |  5284 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5285 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5286 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5287 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5288 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5289 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5290 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5291 | ` *  from within the main script file, then script execution end.` |
|         - |  5292 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5293 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5294 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5295 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5296 | ` */` |
|   2895504 |  5297 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5298 | `{` |
|   2895509 |  5299 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5300 | `	sxi32 rc;` |
|   2895509 |  5301 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2895509 |  5302 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5303 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5304 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5305 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5306 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5307 | `	 * normally below so token processing stays consistent. */` |
|   7606331 |  5308 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4710827 |  5309 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5310 | `	}` |
|   2895504 |  5311 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2895477 |  5312 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5313 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5314 | `			"A never-returning function must not return");` |
|         3 |  5315 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5316 | `			return SXERR_ABORT;` |
|         - |  5317 | `		}` |
|         1 |  5318 | `	}` |
|         - |  5319 | `	/* Jump the 'return' keyword */` |
|   2895509 |  5320 | `	pGen->pIn++;` |
|   2895509 |  5321 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5322 | `		/* Compile the expression */` |
|   2800179 |  5323 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2800179 |  5324 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5325 | `			return SXERR_ABORT;` |
|   2800179 |  5326 | `		}else if(rc != SXERR_EMPTY ){` |
|   2800179 |  5327 | `			nRet = 1;` |
|   1400087 |  5328 | `		}` |
|   1400087 |  5329 | `	}` |
|         - |  5330 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5331 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5332 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5333 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2895509 |  5334 | `	if( pGen->bInGenerator ){` |
|      3845 |  5335 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3845 |  5336 | `		return SXRET_OK;` |
|         - |  5337 | `	}` |
|         - |  5338 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5339 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5340 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5341 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5342 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2891669 |  5343 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2891669 |  5344 | `	return SXRET_OK;` |
|   1447757 |  5345 | `}` |
|         - |  5346 | `/*` |
|         - |  5347 | ` * Compile a yield expression.` |
|         - |  5348 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5349 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5350 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5351 | ` */` |
|     15632 |  5352 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5353 | `{` |
|         - |  5354 | `	SyToken *pTmp, *pSplit;` |
|     15637 |  5355 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15637 |  5356 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5357 | `	sxi32 rc;` |
|      7816 |  5358 | `	(void)iCompileFlag;` |
|         - |  5359 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15637 |  5360 | `	pGen->pIn++;` |
|         - |  5361 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5362 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5363 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5364 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5365 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15632 |  5366 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7851 |  5367 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5368 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5369 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5370 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5371 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5372 | `			return SXERR_ABORT;` |
|         - |  5373 | `		}` |
|        67 |  5374 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5375 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5376 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5377 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5378 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5379 | `				return SXERR_ABORT;` |
|         - |  5380 | `			}` |
|       ! 0 |  5381 | `		}` |
|        67 |  5382 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5383 | `		return SXRET_OK;` |
|         - |  5384 | `	}` |
|     15575 |  5385 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5386 | `		/* Bare yield — no value */` |
|         3 |  5387 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5388 | `		return SXRET_OK;` |
|         - |  5389 | `	}` |
|         - |  5390 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15573 |  5391 | `	pSplit = 0;` |
|         - |  5392 | `	{` |
|     15573 |  5393 | `		SyToken *pCur = pGen->pIn;` |
|     15573 |  5394 | `		sxi32 nNest = 0;` |
|     46525 |  5395 | `		while( pCur < pGen->pEnd ){` |
|     46219 |  5396 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5397 | `				nNest++;` |
|     46211 |  5398 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5399 | `				nNest--;` |
|     46195 |  5400 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15267 |  5401 | `				pSplit = pCur;` |
|     15267 |  5402 | `				break;` |
|         - |  5403 | `			}` |
|     30957 |  5404 | `			pCur++;` |
|         5 |  5405 | `		}` |
|         - |  5406 | `	}` |
|     15573 |  5407 | `	pTmp = pGen->pEnd;` |
|     15573 |  5408 | `	if( pSplit ){` |
|         - |  5409 | `		/* yield $key => $value */` |
|     15267 |  5410 | `		pGen->pEnd = pSplit;` |
|     15267 |  5411 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15267 |  5412 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15267 |  5413 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15267 |  5414 | `		pGen->pEnd = pTmp;` |
|     15267 |  5415 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15267 |  5416 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15267 |  5417 | `		iP1 = 1;` |
|     15267 |  5418 | `		iP2 = 1;` |
|      7636 |  5419 | `	}else{` |
|         - |  5420 | `		/* yield $value */` |
|       311 |  5421 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       311 |  5422 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       311 |  5423 | `		if( rc != SXERR_EMPTY ){` |
|       311 |  5424 | `			iP1 = 1;` |
|       153 |  5425 | `		}` |
|         - |  5426 | `	}` |
|     15573 |  5427 | `	pGen->pEnd = pTmp;` |
|     15573 |  5428 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15573 |  5429 | `	return SXRET_OK;` |
|      7821 |  5430 | `}` |
|         - |  5431 | `/*` |
|         - |  5432 | ` * Compile the die/exit language construct.` |
|         - |  5433 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5434 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5435 | ` */` |
|       128 |  5436 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5437 | `{` |
|       133 |  5438 | `	sxi32 nExpr = 0;` |
|         - |  5439 | `	sxi32 rc;` |
|         - |  5440 | `	/* Jump the die/exit keyword */` |
|       133 |  5441 | `	pGen->pIn++;` |
|       133 |  5442 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5443 | `		/* Compile the expression */` |
|       133 |  5444 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5445 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5446 | `			return SXERR_ABORT;` |
|       133 |  5447 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5448 | `			nExpr = 1;` |
|        64 |  5449 | `		}` |
|        64 |  5450 | `	}` |
|         - |  5451 | `	/* Emit the HALT instruction */` |
|       133 |  5452 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5453 | `	return SXRET_OK;` |
|        69 |  5454 | `}` |
|         - |  5455 | `/*` |
|         - |  5456 | ` * Compile the 'echo' language construct.` |
|         - |  5457 | ` */` |
|     17670 |  5458 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5459 | `{` |
|     17675 |  5460 | `	SyToken *pTmp,*pNext = 0;` |
|     17675 |  5461 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17675 |  5462 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17675 |  5463 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5464 | `	sxi32 rc;` |
|         - |  5465 | `	/* Jump the 'echo' keyword */` |
|     17675 |  5466 | `	pGen->pIn++;` |
|         - |  5467 | `	/* Compile arguments one after one */` |
|     17675 |  5468 | `	pTmp = pGen->pEnd;` |
|     44601 |  5469 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     26933 |  5470 | `		if( pGen->pIn < pNext ){` |
|     26933 |  5471 | `			pGen->pEnd = pNext;` |
|     26933 |  5472 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     26933 |  5473 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5474 | `				return SXERR_ABORT;` |
|     26933 |  5475 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5476 | `				/* Emit the consume instruction */` |
|     26907 |  5477 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     26907 |  5478 | `				nExpr++;` |
|     26907 |  5479 | `				bExpectMore = 0;` |
|     13451 |  5480 | `			}` |
|     13464 |  5481 | `		}` |
|         - |  5482 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5483 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     36197 |  5484 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      9271 |  5485 | `			if( bExpectMore ){` |
|         - |  5486 | `				/* two commas in a row */` |
|         3 |  5487 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5488 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5489 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5490 | `			}` |
|      9269 |  5491 | `			bExpectMore = 1;` |
|      9269 |  5492 | `			pNext++;` |
|         5 |  5493 | `		}` |
|     26931 |  5494 | `		pGen->pIn = pNext;` |
|         5 |  5495 | `	}` |
|         - |  5496 | `	/* Restore token stream */` |
|     17673 |  5497 | `	pGen->pEnd = pTmp;` |
|     17673 |  5498 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5499 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5500 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5501 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5502 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5503 | `	}` |
|     17643 |  5504 | `	return SXRET_OK;` |
|      8840 |  5505 | `}` |
|         - |  5506 | `/*` |
|         - |  5507 | ` * Compile the static statement.` |
|         - |  5508 | ` * According to the PHP language reference` |
|         - |  5509 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5510 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5511 | ` *  when program execution leaves this scope.` |
|         - |  5512 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5513 | ` * Symisc eXtension.` |
|         - |  5514 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5515 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5516 | ` *  Example` |
|         - |  5517 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5518 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5519 | ` */` |
|      3824 |  5520 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         5 |  5521 | `{` |
|         - |  5522 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5523 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5524 | `	GenBlock *pBlock;` |
|         - |  5525 | `	SyString *pName;` |
|         - |  5526 | `	char *zDup;` |
|         - |  5527 | `	sxu32 nLine;` |
|         - |  5528 | `	sxi32 rc;` |
|         - |  5529 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5530 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5531 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|      3824 |  5532 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      1918 |  5533 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5534 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5535 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5536 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5537 | `			return SXERR_ABORT;` |
|         3 |  5538 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5539 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5540 | `		}` |
|         3 |  5541 | `		return SXRET_OK;` |
|         - |  5542 | `	}` |
|         - |  5543 | `	/* Jump the static keyword */` |
|      3827 |  5544 | `	nLine = pGen->pIn->nLine;` |
|      3827 |  5545 | `	pGen->pIn++;` |
|         - |  5546 | `	/* Extract the enclosing function if any */` |
|      3827 |  5547 | `	pBlock = pGen->pCurrent;` |
|      7649 |  5548 | `	while( pBlock ){` |
|      7649 |  5549 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      3827 |  5550 | `			break;` |
|         - |  5551 | `		}` |
|         - |  5552 | `		/* Point to the upper block */` |
|      3827 |  5553 | `		pBlock = pBlock->pParent;` |
|         5 |  5554 | `	}` |
|      3827 |  5555 | `	if( pBlock == 0 ){` |
|         - |  5556 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5557 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5558 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5559 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5560 | `				return SXERR_ABORT;` |
|         - |  5561 | `			}` |
|       ! 0 |  5562 | `			goto Synchronize;` |
|         - |  5563 | `		}` |
|         - |  5564 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5565 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5566 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5567 | `			return SXERR_ABORT;` |
|       ! 0 |  5568 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5569 | `			/* Emit the POP instruction */` |
|       ! 0 |  5570 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5571 | `		}` |
|       ! 0 |  5572 | `		return SXRET_OK;` |
|         - |  5573 | `	}` |
|      3827 |  5574 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5575 | `	/* Make sure we are dealing with a valid statement */` |
|      3827 |  5576 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      3820 |  5577 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5578 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5579 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5580 | `				return SXERR_ABORT;` |
|         - |  5581 | `			}` |
|         3 |  5582 | `			goto Synchronize;` |
|         - |  5583 | `	}` |
|      3825 |  5584 | `	pGen->pIn++;` |
|         - |  5585 | `	/* Extract variable name */` |
|      3825 |  5586 | `	pName = &pGen->pIn->sData;` |
|      3825 |  5587 | `	pGen->pIn++; /* Jump the var name */` |
|      3825 |  5588 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5589 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5590 | `		goto Synchronize;` |
|         - |  5591 | `	}` |
|         - |  5592 | `	/* Initialize the structure describing the static variable */` |
|      3825 |  5593 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      3825 |  5594 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5595 | `	/* Duplicate variable name */` |
|      3825 |  5596 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      3825 |  5597 | `	if( zDup == 0 ){` |
|       ! 0 |  5598 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5599 | `		return SXERR_ABORT;` |
|         - |  5600 | `	}` |
|      3825 |  5601 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5602 | `	/* Check if we have an expression to compile */` |
|      3825 |  5603 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5604 | `		SySet *pInstrContainer;` |
|         - |  5605 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5606 | `		 * Static variable can take any complex expression including function` |
|         - |  5607 | `		 * call as their initialization value.` |
|         - |  5608 | `		 * Example:` |
|         - |  5609 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5610 | `		 */` |
|      3825 |  5611 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5612 | `		/* Swap bytecode container */` |
|      3825 |  5613 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      3825 |  5614 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5615 | `		/* Compile the expression */` |
|      3825 |  5616 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5617 | `		/* Emit the done instruction */` |
|      3825 |  5618 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5619 | `		/* Restore default bytecode container */` |
|      3825 |  5620 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      1910 |  5621 | `	}` |
|         - |  5622 | `	/* Finally save the compiled static variable in the appropriate container */` |
|      3825 |  5623 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|      3825 |  5624 | `	return SXRET_OK;` |
|         1 |  5625 | `Synchronize:` |
|         - |  5626 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5627 | `	 * statement.` |
|         - |  5628 | `	 */` |
|         5 |  5629 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5630 | `		pGen->pIn++;` |
|         1 |  5631 | `	}` |
|         3 |  5632 | `	return SXRET_OK;` |
|      1917 |  5633 | `}` |
|         - |  5634 | `/*` |
|         - |  5635 | ` * Compile the var statement.` |
|         - |  5636 | ` * Symisc Extension:` |
|         - |  5637 | ` *      var statement can be used outside of a class definition.` |
|         - |  5638 | ` */` |
|         4 |  5639 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5640 | `{` |
|         - |  5641 | `	sxu32 nLine;` |
|         - |  5642 | `	sxi32 rc;` |
|         5 |  5643 | `	nLine = pGen->pIn->nLine;` |
|         - |  5644 | `	/* Jump the 'var' keyword */` |
|         5 |  5645 | `	pGen->pIn++;` |
|         5 |  5646 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5647 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5648 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5649 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5650 | `			pGen->pIn++;` |
|       ! 0 |  5651 | `		}` |
|       ! 0 |  5652 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5653 | `			return SXERR_ABORT;` |
|         - |  5654 | `		}` |
|       ! 0 |  5655 | `	}else{` |
|         - |  5656 | `		/* Compile the expression */` |
|         5 |  5657 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5658 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5659 | `			return SXERR_ABORT;` |
|         5 |  5660 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5661 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5662 | `		}` |
|         - |  5663 | `	}` |
|         5 |  5664 | `	return SXRET_OK;` |
|         3 |  5665 | `}` |
|         - |  5666 | `/*` |
|         - |  5667 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5668 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5669 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5670 | ` */` |
|         - |  5671 | `/*` |
|         - |  5672 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5673 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5674 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5675 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5676 | ` *` |
|         - |  5677 | ` * Resolution order:` |
|         - |  5678 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5679 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5680 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5681 | ` *` |
|         - |  5682 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5683 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5684 | ` * Returns the (possibly new) literal index.` |
|         - |  5685 | ` */` |
|   5480096 |  5686 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5687 | `{` |
|         - |  5688 | `	ph7_value *pLit;` |
|         - |  5689 | `	const char *zLit;` |
|         - |  5690 | `	SyString sQualified;` |
|         - |  5691 | `	sxu32 nLit;` |
|         - |  5692 | `	sxu32 k;` |
|         - |  5693 | `	sxu32 nNewIdx;` |
|         - |  5694 | `	int hasNsSep;` |
|         - |  5695 | `	SyHashEntry *pImport;` |
|         - |  5696 | `	ph7_value *pNew;` |
|   5480101 |  5697 | `	if( pFromImport ){` |
|   4416535 |  5698 | `		*pFromImport = 0;` |
|   2208265 |  5699 | `	}` |
|   5480101 |  5700 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   5480101 |  5701 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5702 | `		return nOrigIdx;` |
|         - |  5703 | `	}` |
|   5480101 |  5704 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   5480101 |  5705 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5706 | `	/* Skip if already qualified (contains backslash) */` |
|   5480101 |  5707 | `	hasNsSep = 0;` |
|  65498415 |  5708 | `	for( k = 0; k < nLit; k++ ){` |
|  60018327 |  5709 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  30009162 |  5710 | `	}` |
|   5480101 |  5711 | `	if( hasNsSep ){` |
|        10 |  5712 | `		return nOrigIdx;` |
|         - |  5713 | `	}` |
|         - |  5714 | `	/* Check use imports first (works even outside namespaces) */` |
|   5480093 |  5715 | `	SyBlobReset(&pGen->sWorker);` |
|   5480093 |  5716 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   5480093 |  5717 | `	if( pImport ){` |
|        41 |  5718 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5719 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5720 | `		if( pFromImport ){` |
|        18 |  5721 | `			*pFromImport = 1;` |
|         8 |  5722 | `		}` |
|        23 |  5723 | `	}else{` |
|   5480057 |  5724 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   5479967 |  5725 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5726 | `		}` |
|         - |  5727 | `		/* Prepend current namespace */` |
|        95 |  5728 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        95 |  5729 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|        95 |  5730 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5731 | `	}` |
|         - |  5732 | `	/* Look up or create a new literal for the qualified name */` |
|       131 |  5733 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       131 |  5734 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        57 |  5735 | `		return nNewIdx; /* Already interned */` |
|         - |  5736 | `	}` |
|        79 |  5737 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        79 |  5738 | `	if( pNew == 0 ){` |
|       ! 0 |  5739 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5740 | `	}` |
|        79 |  5741 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        79 |  5742 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        79 |  5743 | `	return nNewIdx;` |
|   2740053 |  5744 | `}` |
|         - |  5745 | `/*` |
|         - |  5746 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5747 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5748 | ` */` |
|    409434 |  5749 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5750 | `{` |
|         - |  5751 | `	SyHashEntry *pImport;` |
|         - |  5752 | `	/* Check use imports first */` |
|    409439 |  5753 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    409439 |  5754 | `	if( pImport ){` |
|        20 |  5755 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        20 |  5756 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        20 |  5757 | `		return;` |
|         - |  5758 | `	}` |
|         - |  5759 | `	/* Prepend current namespace if active */` |
|    409423 |  5760 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         8 |  5761 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  5762 | `		SyBlobAppend(pOut,"\\",1);` |
|         3 |  5763 | `	}` |
|    409423 |  5764 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    204722 |  5765 | `}` |
|         - |  5766 | `/*` |
|         - |  5767 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5768 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5769 | ` * The caller must release pOut when done.` |
|         - |  5770 | ` */` |
|    428918 |  5771 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5772 | `{` |
|    428923 |  5773 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3875 |  5774 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3875 |  5775 | `		SyBlobAppend(pOut,"\\",1);` |
|      1935 |  5776 | `	}` |
|    428923 |  5777 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    428923 |  5778 | `}` |
|         - |  5779 | `/*` |
|         - |  5780 | ` * Compile a namespace statement` |
|         - |  5781 | ` * According to the PHP language reference manual` |
|         - |  5782 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5783 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5784 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5785 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5786 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5787 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5788 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5789 | ` *  programming world.` |
|         - |  5790 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5791 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5792 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5793 | ` *  classes/functions/constants.` |
|         - |  5794 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5795 | ` *  readability of source code.` |
|         - |  5796 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5797 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5798 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5799 | ` *       class MyClass {}` |
|         - |  5800 | ` *       function myfunction() {}` |
|         - |  5801 | ` *       const MYCONST = 1;` |
|         - |  5802 | ` *       $a = new MyClass;` |
|         - |  5803 | ` *       $c = new \my\name\MyClass;` |
|         - |  5804 | ` *       $a = strlen('hi');` |
|         - |  5805 | ` *       $d = namespace\MYCONST;` |
|         - |  5806 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5807 | ` *       echo constant($d);` |
|         - |  5808 | ` * NOTE` |
|         - |  5809 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5810 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5811 | ` */` |
|         - |  5812 | `/*` |
|         - |  5813 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5814 | ` */` |
|        14 |  5815 | `static const char * TokenTypeName(sxu32 nType)` |
|         3 |  5816 | `{` |
|        17 |  5817 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        11 |  5818 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        11 |  5819 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        11 |  5820 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        11 |  5821 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        11 |  5822 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5823 | `	return "token";` |
|        10 |  5824 | `}` |
|      3918 |  5825 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5826 | `{` |
|         - |  5827 | `	sxu32 nLine;` |
|         - |  5828 | `	sxi32 rc;` |
|      3923 |  5829 | `	nLine = pGen->pIn->nLine;` |
|      3923 |  5830 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5831 | `	/* Reset namespace and clear previous use imports */` |
|      3923 |  5832 | `	SyBlobReset(&pGen->sNamespace);` |
|      3923 |  5833 | `	SyHashRelease(&pGen->hUseImports);` |
|      3923 |  5834 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3923 |  5835 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3923 |  5836 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3923 |  5837 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3923 |  5838 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3923 |  5839 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5840 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5841 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5842 | `		return SXRET_OK;` |
|         - |  5843 | `	}` |
|      3923 |  5844 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5845 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5846 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5847 | `		return SXRET_OK;` |
|         - |  5848 | `	}` |
|      3923 |  5849 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5850 | `		/* namespace { } — global namespace block */` |
|       ! 0 |  5851 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5852 | `		return SXRET_OK;` |
|         - |  5853 | `	}` |
|         - |  5854 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7883 |  5855 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3965 |  5856 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5857 | `			/* Append backslash separator */` |
|        26 |  5858 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        26 |  5859 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        11 |  5860 | `			}` |
|        15 |  5861 | `		}else{` |
|         - |  5862 | `			/* Append identifier */` |
|      3943 |  5863 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5864 | `		}` |
|      3965 |  5865 | `		pGen->pIn++;` |
|         5 |  5866 | `	}` |
|         - |  5867 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5868 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5869 | `	{` |
|      3923 |  5870 | `		char *zNsDup = 0;` |
|      3923 |  5871 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5879 |  5872 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3916 |  5873 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1958 |  5874 | `		}` |
|      3923 |  5875 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5876 | `	}` |
|      3923 |  5877 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5878 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5879 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5880 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5881 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5882 | `			return SXERR_ABORT;` |
|         - |  5883 | `		}` |
|         2 |  5884 | `	}` |
|      3923 |  5885 | `	return SXRET_OK;` |
|      1964 |  5886 | `}` |
|         - |  5887 | `/*` |
|         - |  5888 | ` * Compile the 'use' statement` |
|         - |  5889 | ` * According to the PHP language reference manual` |
|         - |  5890 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5891 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5892 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5893 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5894 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5895 | ` *  a function or constant is not supported.` |
|         - |  5896 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5897 | ` * NOTE` |
|         - |  5898 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5899 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5900 | ` */` |
|        72 |  5901 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5902 | `{` |
|         - |  5903 | `	sxu32 nLine;` |
|         - |  5904 | `	sxi32 rc;` |
|         - |  5905 | `	SyBlob sPath;` |
|         - |  5906 | `	SyString sAlias;` |
|         - |  5907 | `	SyToken *pLast;` |
|         - |  5908 | `	char *zDup;` |
|         - |  5909 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5910 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5911 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        77 |  5912 | `	nLine = pGen->pIn->nLine;` |
|        77 |  5913 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5914 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        77 |  5915 | `	iUseType = 0;` |
|        77 |  5916 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5917 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5918 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5919 | `			iUseType = 1;` |
|        16 |  5920 | `			pGen->pIn++;` |
|        23 |  5921 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5922 | `			iUseType = 2;` |
|        16 |  5923 | `			pGen->pIn++;` |
|         7 |  5924 | `		}` |
|        14 |  5925 | `	}` |
|         - |  5926 | `	/* Select target hash tables based on import type */` |
|        77 |  5927 | `	switch( iUseType ){` |
|         7 |  5928 | `		case 1:` |
|        16 |  5929 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5930 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5931 | `			break;` |
|         7 |  5932 | `		case 2:` |
|        16 |  5933 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5934 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5935 | `			break;` |
|        22 |  5936 | `		default:` |
|        49 |  5937 | `			pGenHash = &pGen->hUseImports;` |
|        49 |  5938 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        44 |  5939 | `			break;` |
|         - |  5940 | `	}` |
|        77 |  5941 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5942 | `	/* Process one or more use declarations separated by commas */` |
|        37 |  5943 | `	for(;;){` |
|        79 |  5944 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5945 | `			break;` |
|         - |  5946 | `		}` |
|        79 |  5947 | `		SyBlobReset(&sPath);` |
|        79 |  5948 | `		pLast = 0;` |
|         - |  5949 | `		/* Collect the full namespace path */` |
|       269 |  5950 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       195 |  5951 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       135 |  5952 | `				pLast = pGen->pIn;` |
|       135 |  5953 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        65 |  5954 | `					SyBlobAppend(&sPath,"\\",1);` |
|        30 |  5955 | `				}` |
|       135 |  5956 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        65 |  5957 | `			}` |
|       195 |  5958 | `			pGen->pIn++;` |
|         5 |  5959 | `		}` |
|        79 |  5960 | `		if( pLast == 0 ){` |
|         - |  5961 | `			/* Empty path */` |
|         6 |  5962 | `			break;` |
|         - |  5963 | `		}` |
|         - |  5964 | `		/* Default alias is the last component of the path */` |
|        75 |  5965 | `		sAlias = pLast->sData;` |
|         - |  5966 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        70 |  5967 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        50 |  5968 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        23 |  5969 | `			pGen->pIn++; /* Jump 'as' */` |
|        23 |  5970 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        23 |  5971 | `				sAlias = pGen->pIn->sData;` |
|        23 |  5972 | `				pGen->pIn++;` |
|        10 |  5973 | `			}` |
|        10 |  5974 | `		}` |
|         - |  5975 | `		/* Check for duplicate import alias (per-type) */` |
|        75 |  5976 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  5977 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  5978 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  5979 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  5980 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5981 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  5982 | `				return SXERR_ABORT;` |
|         - |  5983 | `			}` |
|         2 |  5984 | `		}` |
|         - |  5985 | `		/* Register the import: alias -> FQN.` |
|         - |  5986 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  5987 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  5988 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       110 |  5989 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        70 |  5990 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        75 |  5991 | `		if( zDup ){` |
|        75 |  5992 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        75 |  5993 | `			if( pVmHash ){` |
|         - |  5994 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  5995 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        47 |  5996 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        47 |  5997 | `				if( zAliasDup ){` |
|        47 |  5998 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        21 |  5999 | `				}` |
|        21 |  6000 | `			}` |
|        75 |  6001 | `			if( iUseType == 2 ){` |
|         - |  6002 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  6003 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  6004 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  6005 | `				if( zAliasDup ){` |
|         - |  6006 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  6007 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  6008 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  6009 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  6010 | `					if( azPair ){` |
|        16 |  6011 | `						azPair[0] = zAliasDup;` |
|        16 |  6012 | `						azPair[1] = zDup;` |
|        16 |  6013 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  6014 | `					}` |
|         7 |  6015 | `				}` |
|         7 |  6016 | `			}` |
|        35 |  6017 | `		}` |
|         - |  6018 | `		/* Check for comma (multiple use declarations) */` |
|        75 |  6019 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  6020 | `			pGen->pIn++;` |
|         2 |  6021 | `		}else{` |
|        39 |  6022 | `			break;` |
|         - |  6023 | `		}` |
|         1 |  6024 | `	}` |
|        77 |  6025 | `	SyBlobRelease(&sPath);` |
|        77 |  6026 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  6027 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  6028 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  6029 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6030 | `			return SXERR_ABORT;` |
|         - |  6031 | `		}` |
|         1 |  6032 | `	}` |
|        77 |  6033 | `	return SXRET_OK;` |
|        41 |  6034 | `}` |
|         - |  6035 | `/*` |
|         - |  6036 | ` * Compile the stupid 'declare' language construct.` |
|         - |  6037 | ` *` |
|         - |  6038 | ` * According to the PHP language reference manual.` |
|         - |  6039 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  6040 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  6041 | ` *  declare (directive)` |
|         - |  6042 | ` *   statement` |
|         - |  6043 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  6044 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  6045 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  6046 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  6047 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  6048 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  6049 | ` * <?php` |
|         - |  6050 | ` * // these are the same:` |
|         - |  6051 | ` * // you can use this:` |
|         - |  6052 | ` * declare(ticks=1) {` |
|         - |  6053 | ` *   // entire script here` |
|         - |  6054 | ` * }` |
|         - |  6055 | ` * // or you can use this:` |
|         - |  6056 | ` * declare(ticks=1);` |
|         - |  6057 | ` * // entire script here` |
|         - |  6058 | ` * ?>` |
|         - |  6059 | ` *` |
|         - |  6060 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  6061 | ` */` |
|         - |  6062 | `/*` |
|         - |  6063 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  6064 | ` */` |
|        72 |  6065 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  6066 | `{` |
|       109 |  6067 | `	return SyStringLength(pName) == nWant` |
|        72 |  6068 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  6069 | `}` |
|         - |  6070 |  |
|        42 |  6071 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  6072 | `{` |
|        47 |  6073 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  6074 | `	SyToken *pBodyEnd = 0;` |
|         - |  6075 | `	SyToken *pBodyStart;` |
|         - |  6076 | `	SyToken *pCursor;` |
|         - |  6077 | `	int bHasStrictTypes;` |
|         - |  6078 | `	int bBlockForm;` |
|         - |  6079 | `	int bPlacementOk;` |
|         - |  6080 | `	sxi32 rc;` |
|        47 |  6081 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  6082 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  6083 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         6 |  6084 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6085 | `			return SXERR_ABORT;` |
|         - |  6086 | `		}` |
|         6 |  6087 | `		goto Synchro;` |
|         - |  6088 | `	}` |
|        43 |  6089 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  6090 | `	pBodyStart = pGen->pIn;` |
|         - |  6091 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  6092 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  6093 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  6094 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  6095 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6096 | `			return SXERR_ABORT;` |
|         - |  6097 | `		}` |
|       ! 0 |  6098 | `		return SXRET_OK;` |
|         - |  6099 | `	}` |
|         - |  6100 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  6101 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  6102 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  6103 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  6104 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  6105 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6106 | `			return SXERR_ABORT;` |
|         - |  6107 | `		}` |
|       ! 0 |  6108 | `	}` |
|        43 |  6109 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  6110 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  6111 | `	bHasStrictTypes = 0;` |
|         - |  6112 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  6113 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  6114 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  6115 | `	pCursor = pBodyStart;` |
|        55 |  6116 | `	while( pCursor < pBodyEnd ){` |
|        51 |  6117 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  6118 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  6119 | `				bHasStrictTypes = 1;` |
|        39 |  6120 | `				break;` |
|         - |  6121 | `			}` |
|         2 |  6122 | `		}` |
|        14 |  6123 | `		pCursor++;` |
|         2 |  6124 | `	}` |
|        43 |  6125 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  6126 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6127 | `			"strict_types declaration must not use block mode");` |
|         3 |  6128 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6129 | `		return SXRET_OK;` |
|         - |  6130 | `	}` |
|        41 |  6131 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  6132 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6133 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  6134 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  6135 | `		return SXRET_OK;` |
|         - |  6136 | `	}` |
|         - |  6137 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  6138 | `	pCursor = pBodyStart;` |
|        69 |  6139 | `	while( pCursor < pBodyEnd ){` |
|         - |  6140 | `		SyToken *pNameTok;` |
|         - |  6141 | `		SyToken *pEqTok;` |
|         - |  6142 | `		SyToken *pValTok;` |
|         - |  6143 | `		SyString *pDirName;` |
|         - |  6144 | `		int bIsStrict;` |
|         - |  6145 | `		int iStrictValue;` |
|        39 |  6146 | `		pNameTok = pCursor;` |
|        39 |  6147 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6148 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6149 | `				"declare: Expecting a directive name");` |
|       ! 0 |  6150 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6151 | `			return SXRET_OK;` |
|         - |  6152 | `		}` |
|        39 |  6153 | `		pEqTok = pNameTok + 1;` |
|        39 |  6154 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6155 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6156 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6157 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6158 | `			return SXRET_OK;` |
|         - |  6159 | `		}` |
|        39 |  6160 | `		pValTok = pEqTok + 1;` |
|        39 |  6161 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6162 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6163 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6164 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6165 | `			return SXRET_OK;` |
|         - |  6166 | `		}` |
|        39 |  6167 | `		pDirName = &pNameTok->sData;` |
|        39 |  6168 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6169 | `		if( bIsStrict ){` |
|         - |  6170 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6171 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6172 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6173 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6174 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6175 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6176 | `				return SXRET_OK;` |
|         - |  6177 | `			}` |
|        35 |  6178 | `			iStrictValue = -1;` |
|        35 |  6179 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6180 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6181 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6182 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6183 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6184 | `			}` |
|        35 |  6185 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6186 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6187 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6188 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6189 | `				return SXRET_OK;` |
|         - |  6190 | `			}` |
|        32 |  6191 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6192 | `		}else{` |
|         - |  6193 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6194 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6195 | `			 * behavior don't regress. */` |
|         8 |  6196 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6197 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6198 | `				ph7_lib_version()` |
|         - |  6199 | `				);` |
|         - |  6200 | `		}` |
|        36 |  6201 | `		pCursor = pValTok + 1;` |
|         - |  6202 | `		/* Consume separating comma (or end). */` |
|        36 |  6203 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6204 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6205 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6206 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6207 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6208 | `				return SXRET_OK;` |
|         - |  6209 | `			}` |
|         3 |  6210 | `			pCursor++;` |
|         1 |  6211 | `		}` |
|         4 |  6212 | `	}` |
|         - |  6213 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6214 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6215 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        34 |  6216 | `	return SXRET_OK;` |
|         2 |  6217 | `Synchro:` |
|         - |  6218 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6219 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6220 | `		pGen->pIn++;` |
|         2 |  6221 | `	}` |
|         6 |  6222 | `	return SXRET_OK;` |
|        26 |  6223 | `}` |
|         - |  6224 | `/*` |
|         - |  6225 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6226 | ` * as follows:` |
|         - |  6227 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6228 | ` * {` |
|         - |  6229 | ` *   return "Making a cup of $type.\n";` |
|         - |  6230 | ` * }` |
|         - |  6231 | ` * Symisc eXtension.` |
|         - |  6232 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6233 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6234 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6235 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6236 | ` *      {` |
|         - |  6237 | ` *       var_dump($a);` |
|         - |  6238 | ` *      }` |
|         - |  6239 | ` *     //call test without args` |
|         - |  6240 | ` *      test();` |
|         - |  6241 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6242 | ` *      Example:` |
|         - |  6243 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6244 | ` * 3 -) Function overloading!!` |
|         - |  6245 | ` *      Example:` |
|         - |  6246 | ` *      function foo($a) {` |
|         - |  6247 | ` *   	  return $a.PHP_EOL;` |
|         - |  6248 | ` *	    }` |
|         - |  6249 | ` *	    function foo($a, $b) {` |
|         - |  6250 | ` *   	  return $a + $b;` |
|         - |  6251 | ` *	    }` |
|         - |  6252 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6253 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6254 | ` *      // Same arg` |
|         - |  6255 | ` *	   function foo(string $a)` |
|         - |  6256 | ` *	   {` |
|         - |  6257 | ` *	     echo "a is a string\n";` |
|         - |  6258 | ` *	     var_dump($a);` |
|         - |  6259 | ` *	   }` |
|         - |  6260 | ` *	  function foo(int $a)` |
|         - |  6261 | ` *	  {` |
|         - |  6262 | ` *	    echo "a is integer\n";` |
|         - |  6263 | ` *	    var_dump($a);` |
|         - |  6264 | ` *	  }` |
|         - |  6265 | ` *	  function foo(array $a)` |
|         - |  6266 | ` *	  {` |
|         - |  6267 | ` * 	    echo "a is an array\n";` |
|         - |  6268 | ` * 	    var_dump($a);` |
|         - |  6269 | ` *	  }` |
|         - |  6270 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6271 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6272 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6273 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6274 | ` * introduced by the PH7 engine.` |
|         - |  6275 | ` */` |
|    549086 |  6276 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6277 | `{` |
|         - |  6278 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6279 | `	SySet *pInstrContainer;` |
|         - |  6280 | `	sxi32 rc;` |
|         - |  6281 | `	/* Swap token stream */` |
|    549091 |  6282 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    549091 |  6283 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    549091 |  6284 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6285 | `	/* Compile the expression holding the argument value */` |
|    549091 |  6286 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6287 | `	/* Emit the done instruction */` |
|    549091 |  6288 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    549091 |  6289 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    549091 |  6290 | `	RE_SWAP_DELIMITER(pGen);` |
|    549091 |  6291 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6292 | `		return SXERR_ABORT;` |
|         - |  6293 | `	}` |
|    549091 |  6294 | `	return SXRET_OK;` |
|    274548 |  6295 | `}` |
|         - |  6296 | `/*` |
|         - |  6297 | ` * Collect function arguments one after one.` |
|         - |  6298 | ` * According to the PHP language reference manual.` |
|         - |  6299 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6300 | ` * list of expressions.` |
|         - |  6301 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6302 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6303 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6304 | ` * for more information.` |
|         - |  6305 | ` * Example #1 Passing arrays to functions` |
|         - |  6306 | ` * <?php` |
|         - |  6307 | ` * function takes_array($input)` |
|         - |  6308 | ` * {` |
|         - |  6309 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6310 | ` * }` |
|         - |  6311 | ` * ?>` |
|         - |  6312 | ` * Making arguments be passed by reference` |
|         - |  6313 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6314 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6315 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6316 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6317 | ` * to the argument name in the function definition:` |
|         - |  6318 | ` * Example #2 Passing function parameters by reference` |
|         - |  6319 | ` * <?php` |
|         - |  6320 | ` * function add_some_extra(&$string)` |
|         - |  6321 | ` * {` |
|         - |  6322 | ` *   $string .= 'and something extra.';` |
|         - |  6323 | ` * }` |
|         - |  6324 | ` * $str = 'This is a string, ';` |
|         - |  6325 | ` * add_some_extra($str);` |
|         - |  6326 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6327 | ` * ?>` |
|         - |  6328 | ` *` |
|         - |  6329 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6330 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6331 | ` * on these extension.` |
|         - |  6332 | ` */` |
|   1248480 |  6333 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6334 | `{` |
|         - |  6335 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6336 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6337 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6338 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6339 | `	sxi32 rc;` |
|         - |  6340 |  |
|   1248485 |  6341 | `	pIn = pGen->pIn;` |
|   1248485 |  6342 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6343 | `	/* Process arguments one after one */` |
|   1618733 |  6344 | `	for(;;){` |
|   3237471 |  6345 | `		if( pIn >= pEnd ){` |
|         - |  6346 | `			/* No more arguments to process */` |
|   1248469 |  6347 | `			break;` |
|         - |  6348 | `		}` |
|   1989007 |  6349 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   1989007 |  6350 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   1989007 |  6351 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   1989007 |  6352 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   1989007 |  6353 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6354 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6355 | `		 * first token inside the main token stream */` |
|   1989007 |  6356 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6357 | `			return SXERR_ABORT;` |
|         - |  6358 | `		}` |
|         - |  6359 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6360 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6361 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6362 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6363 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6364 | `		{` |
|   1989007 |  6365 | `			int bReadonly = 0, bVisSeen = 0;` |
|   1989007 |  6366 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   1989007 |  6367 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6368 | `			int nSetTok;` |
|         - |  6369 | `			sxi32 nSetVis;` |
|   1989007 |  6370 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6371 | `				bReadonly = 1;` |
|         3 |  6372 | `				pIn++;` |
|         1 |  6373 | `			}` |
|   1989007 |  6374 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   1989007 |  6375 | `			if( nSetVis ){` |
|         - |  6376 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6377 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6378 | `				bVisSeen = 1;` |
|         3 |  6379 | `				pIn += nSetTok;` |
|         3 |  6380 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6381 | `					bReadonly = 1;` |
|       ! 0 |  6382 | `					pIn++;` |
|         1 |  6383 | `				}` |
|   1989006 |  6384 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     88075 |  6385 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     88075 |  6386 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6387 | `					bVisSeen = 1;` |
|        89 |  6388 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6389 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6390 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6391 | `					pIn++;` |
|        89 |  6392 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6393 | `					if( nSetVis ){` |
|         - |  6394 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6395 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6396 | `						pIn += nSetTok;` |
|         1 |  6397 | `					}` |
|        89 |  6398 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6399 | `						bReadonly = 1;` |
|        18 |  6400 | `						pIn++;` |
|         7 |  6401 | `					}` |
|        42 |  6402 | `				}` |
|     44035 |  6403 | `			}` |
|   1989007 |  6404 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6405 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   1989005 |  6406 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6407 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6408 | `			}` |
|   1989007 |  6409 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6410 | `				if( !bCtorCtx ){` |
|         6 |  6411 | `					if( bAbstractCtx ){` |
|         3 |  6412 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6413 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6414 | `					}else{` |
|         3 |  6415 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6416 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6417 | `					}` |
|         6 |  6418 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6419 | `						return SXERR_ABORT;` |
|         - |  6420 | `					}` |
|         6 |  6421 | `					return SXERR_SYNTAX;` |
|         - |  6422 | `				}` |
|        89 |  6423 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6424 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6425 | `				if( bReadonly ){` |
|        20 |  6426 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6427 | `				}` |
|        42 |  6428 | `			}` |
|         - |  6429 | `		}` |
|         - |  6430 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   1988998 |  6431 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1074866 |  6432 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    149286 |  6433 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    114879 |  6434 | `			sxu32 nLineLocal = pIn->nLine;` |
|    114879 |  6435 | `			sxi32 iTFlags = 0;` |
|    114879 |  6436 | `			pGen->pIn = pIn;` |
|    114879 |  6437 | `			rc = GenStateParseUnionTypeDecl(` |
|     57437 |  6438 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57437 |  6439 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6440 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6441 | `				/* bAllowVoid */ 0,` |
|     57437 |  6442 | `						nLineLocal);` |
|    114879 |  6443 | `			pIn = pGen->pIn;` |
|    114879 |  6444 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6445 | `				return SXERR_ABORT;` |
|    114879 |  6446 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6447 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6448 | `				return SXERR_SYNTAX;` |
|    114877 |  6449 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6450 | `				if( pIn < pEnd ){` |
|        15 |  6451 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6452 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6453 | `						&pIn->sData);` |
|         7 |  6454 | `				}else{` |
|       ! 0 |  6455 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6456 | `						"syntax error, unexpected end of file");` |
|         - |  6457 | `				}` |
|        11 |  6458 | `				return SXERR_SYNTAX;` |
|         - |  6459 | `			}` |
|    114869 |  6460 | `			sArg.iFlags \|= iTFlags;` |
|     57432 |  6461 | `		}` |
|   1988993 |  6462 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6463 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6464 | `			return rc;` |
|         - |  6465 | `		}` |
|   1988993 |  6466 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6467 | `			/* Pass by reference,record that */` |
|     22919 |  6468 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     22919 |  6469 | `			pIn++;` |
|     11457 |  6470 | `		}` |
|   1988993 |  6471 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6472 | `			/* Variadic parameter: ...$args */` |
|     22981 |  6473 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     22981 |  6474 | `			pIn++;` |
|     11488 |  6475 | `		}` |
|   1988993 |  6476 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6477 | `			/* Invalid argument */` |
|       ! 0 |  6478 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6479 | `			return rc;` |
|         - |  6480 | `		}` |
|   1988993 |  6481 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6482 | `		/* Copy argument name */` |
|   1988993 |  6483 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   1988993 |  6484 | `		if( zDup == 0 ){` |
|       ! 0 |  6485 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6486 | `			return SXERR_ABORT;` |
|         - |  6487 | `		}` |
|   1988993 |  6488 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   1988993 |  6489 | `		pIn++;` |
|   1988993 |  6490 | `		if( pIn < pEnd ){` |
|   1102783 |  6491 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6492 | `				SyToken *pDefend;` |
|    549093 |  6493 | `				sxi32 iNest = 0;` |
|    549093 |  6494 | `				pIn++; /* Jump the equal sign */` |
|    549093 |  6495 | `				pDefend = pIn;` |
|         - |  6496 | `				/* Process the default value associated with this argument */` |
|   1155387 |  6497 | `				while( pDefend < pEnd ){` |
|    793137 |  6498 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    186843 |  6499 | `						break;` |
|         - |  6500 | `					}` |
|    606299 |  6501 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6502 | `						/* Increment nesting level */` |
|     26697 |  6503 | `						iNest++;` |
|    592953 |  6504 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6505 | `						/* Decrement nesting level */` |
|     26697 |  6506 | `						iNest--;` |
|     13346 |  6507 | `					}` |
|    606299 |  6508 | `					pDefend++;` |
|         5 |  6509 | `				}` |
|    549093 |  6510 | `				if( pIn >= pDefend ){` |
|         3 |  6511 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6512 | `					return rc;` |
|         - |  6513 | `				}` |
|         - |  6514 | `				/* Process default value */` |
|    549091 |  6515 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    549091 |  6516 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6517 | `					return rc;` |
|         - |  6518 | `				}` |
|         - |  6519 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6520 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6521 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6522 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6523 | `				 * arg-type check lets null through. */` |
|    549086 |  6524 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    299344 |  6525 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    299341 |  6526 | `					&& &pIn[1] == pDefend` |
|     45783 |  6527 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     34330 |  6528 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     20977 |  6529 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15259 |  6530 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6531 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6532 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6533 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6534 | `					 * already up at this point). */` |
|         - |  6535 | `					{` |
|     15259 |  6536 | `						const char *zSep = "";` |
|     15259 |  6537 | `						SyString sCls = { "", 0 };` |
|     15259 |  6538 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15253 |  6539 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15253 |  6540 | `							zSep = "::";` |
|      7624 |  6541 | `						}` |
|     22886 |  6542 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6543 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7627 |  6544 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6545 | `					}` |
|      7627 |  6546 | `				}` |
|         - |  6547 | `				/* Point beyond the default value */` |
|    549091 |  6548 | `				pIn = pDefend;` |
|    274543 |  6549 | `			}` |
|   1102781 |  6550 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6551 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6552 | `				return rc;` |
|         - |  6553 | `			}` |
|   1102781 |  6554 | `			pIn++; /* Jump the trailing comma */` |
|    551388 |  6555 | `		}` |
|         - |  6556 | `		/* Append argument signature */` |
|   1988991 |  6557 | `		if( sArg.nType > 0 ){` |
|    114807 |  6558 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6559 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26771 |  6560 | `				int marker = 'o';` |
|     26771 |  6561 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26771 |  6562 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13388 |  6563 | `			}else{` |
|         - |  6564 | `				int c;` |
|     88041 |  6565 | `				c = 'n'; /* cc warning */` |
|         - |  6566 | `				/* Type leading character */` |
|     88041 |  6567 | `				switch(sArg.nType){` |
|      5724 |  6568 | `				case MEMOBJ_HASHMAP:` |
|         - |  6569 | `					/* Hashmap aka 'array' */` |
|     11453 |  6570 | `					c = 'h';` |
|     11453 |  6571 | `					break;` |
|      9650 |  6572 | `				case MEMOBJ_INT:` |
|         - |  6573 | `					/* Integer */` |
|     19305 |  6574 | `					c = 'i';` |
|     19305 |  6575 | `					break;` |
|         2 |  6576 | `				case MEMOBJ_BOOL:` |
|         - |  6577 | `					/* Bool */` |
|         5 |  6578 | `					c = 'b';` |
|         5 |  6579 | `					break;` |
|         5 |  6580 | `				case MEMOBJ_REAL:` |
|         - |  6581 | `					/* Float */` |
|        12 |  6582 | `					c = 'f';` |
|        12 |  6583 | `					break;` |
|     28629 |  6584 | `				case MEMOBJ_STRING:` |
|         - |  6585 | `					/* String */` |
|     57263 |  6586 | `					c = 's';` |
|     57263 |  6587 | `					break;` |
|         7 |  6588 | `				case MEMOBJ_OBJ:` |
|         - |  6589 | `					/* Object */` |
|        16 |  6590 | `					c = 'o';` |
|        14 |  6591 | `					break;` |
|         1 |  6592 | `				default:` |
|         2 |  6593 | `					break;` |
|         - |  6594 | `				}` |
|     88041 |  6595 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6596 | `			}` |
|     57406 |  6597 | `		}else{` |
|         - |  6598 | `			/* No type is associated with this parameter which mean` |
|         - |  6599 | `			 * that this function is not condidate for overloading.` |
|         - |  6600 | `			 */` |
|   1874189 |  6601 | `			SyBlobRelease(&sSig);` |
|         - |  6602 | `		}` |
|         - |  6603 | `		/* Save in the argument set */` |
|   1988991 |  6604 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6605 | `	}` |
|   1248469 |  6606 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6607 | `		/* Save function signature */` |
|     84239 |  6608 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     42117 |  6609 | `	}` |
|   1248469 |  6610 | `	return SXRET_OK;` |
|    624245 |  6611 | `}` |
|         - |  6612 | `/*` |
|         - |  6613 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6614 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6615 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6616 | ` */` |
|     34352 |  6617 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6618 | `{` |
|     34357 |  6619 | `	sxi32 iParen = 0;` |
|     34357 |  6620 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6621 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6622 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6623 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    152729 |  6624 | `	while( pIn < pEnd ){` |
|    152729 |  6625 | `		sxu32 t = pIn->nType;` |
|    152729 |  6626 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    148861 |  6627 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    103061 |  6628 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     83957 |  6629 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    118377 |  6630 | `		pIn++;` |
|         5 |  6631 | `	}` |
|     19109 |  6632 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6633 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6634 | `	{` |
|     19109 |  6635 | `		sxi32 d = 0;` |
|    759089 |  6636 | `		while( pIn < pEnd ){` |
|    759089 |  6637 | `			sxu32 t = pIn->nType;` |
|    759089 |  6638 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    728541 |  6639 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    739985 |  6640 | `			pIn++;` |
|         5 |  6641 | `		}` |
|         - |  6642 | `	}` |
|     19109 |  6643 | `	return pIn;` |
|     17181 |  6644 | `}` |
|         - |  6645 | `/*` |
|         - |  6646 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6647 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6648 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6649 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6650 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6651 | ` * detached-mini-program path untouched.` |
|         - |  6652 | ` */` |
|         - |  6653 | `/*` |
|         - |  6654 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6655 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6656 | ` * mixed, object.` |
|         - |  6657 | ` */` |
|     11464 |  6658 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6659 | `{` |
|         - |  6660 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6661 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6662 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6663 | `	};` |
|         - |  6664 | `	sxu32 i;` |
|     11469 |  6665 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6666 | `		zName++;` |
|       ! 0 |  6667 | `		nName--;` |
|       ! 0 |  6668 | `	}` |
|     11501 |  6669 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11497 |  6670 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11465 |  6671 | `			return 1;` |
|         - |  6672 | `		}` |
|        17 |  6673 | `	}` |
|         5 |  6674 | `	return 0;` |
|      5737 |  6675 | `}` |
|         - |  6676 | `/*` |
|         - |  6677 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6678 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6679 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6680 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6681 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6682 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6683 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6684 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6685 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6686 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6687 | ` */` |
|     11462 |  6688 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6689 | `{` |
|     11467 |  6690 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6691 | ``		return 1; /* bare `object` */`` |
|         - |  6692 | `	}` |
|     11467 |  6693 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6694 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6695 | `	}` |
|     11465 |  6696 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11461 |  6697 | `		return 1;` |
|         - |  6698 | `	}` |
|         - |  6699 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6700 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6701 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6702 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6703 | `	{` |
|         - |  6704 | `		SyBlob sFQN;` |
|         - |  6705 | `		int bOk;` |
|         5 |  6706 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         5 |  6707 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|         5 |  6708 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|         5 |  6709 | `		SyBlobRelease(&sFQN);` |
|         5 |  6710 | `		return bOk;` |
|         - |  6711 | `	}` |
|      5736 |  6712 | `}` |
|         - |  6713 | `/*` |
|         - |  6714 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6715 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6716 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6717 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6718 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6719 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6720 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6721 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6722 | ` */` |
|     11700 |  6723 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6724 | `{` |
|     11705 |  6725 | `	int bOk = 0;` |
|         - |  6726 | `	sxu32 nLine;` |
|         - |  6727 | `	sxi32 rc;` |
|     11705 |  6728 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       243 |  6729 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6730 | `	}` |
|     11467 |  6731 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6732 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6733 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6734 | `		sxu32 i,j;` |
|       ! 0 |  6735 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6736 | `			int bGroupOk;` |
|       ! 0 |  6737 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6738 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6739 | `			}` |
|       ! 0 |  6740 | `			bGroupOk = 1;` |
|       ! 0 |  6741 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6742 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6743 | `					bGroupOk = 0;` |
|       ! 0 |  6744 | `					break;` |
|         - |  6745 | `				}` |
|       ! 0 |  6746 | `			}` |
|       ! 0 |  6747 | `			bOk = bGroupOk;` |
|       ! 0 |  6748 | `		}` |
|       ! 0 |  6749 | `	}else{` |
|     11467 |  6750 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6751 | `	}` |
|     11467 |  6752 | `	if( bOk ){` |
|     11465 |  6753 | `		return SXRET_OK;` |
|         - |  6754 | `	}` |
|         - |  6755 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6756 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6757 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6758 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6759 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6760 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6761 | `	{` |
|         3 |  6762 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6763 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6764 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6765 | `		}` |
|         3 |  6766 | `		if( sGiven.nByte < 1 ){` |
|         - |  6767 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6768 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6769 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6770 | `			const char *zScalar =` |
|       ! 0 |  6771 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6772 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6773 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6774 | `		}` |
|         3 |  6775 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6776 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6777 | `	}` |
|         3 |  6778 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5855 |  6779 | `}` |
|   2698762 |  6780 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6781 | `{` |
|   2698767 |  6782 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2698767 |  6783 | `	SyToken *pEnd = pGen->pEnd;` |
|   2698767 |  6784 | `	sxi32 iDepth = 0;` |
|   2698767 |  6785 | `	int bStarted = 0;` |
| 128460031 |  6786 | `	while( pIn < pEnd ){` |
| 128460031 |  6787 | `		sxu32 t = pIn->nType;` |
| 128460031 |  6788 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 122645875 |  6789 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 116866405 |  6790 | `		if( t & PH7_TK_KEYWORD ){` |
|   8712127 |  6791 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   8712127 |  6792 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   8700427 |  6793 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6794 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   4333035 |  6795 | `		}` |
| 116820353 |  6796 | `		pIn++;` |
|         5 |  6797 | `	}` |
|   2687067 |  6798 | `	return FALSE;` |
|   1349386 |  6799 | `}` |
|         - |  6800 | `/*` |
|         - |  6801 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6802 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6803 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6804 | ` */` |
|   2698762 |  6805 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6806 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6807 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6808 | `	)` |
|         5 |  6809 | `{` |
|         - |  6810 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6811 | `	GenBlock *pBlock;` |
|         - |  6812 | `	sxu32 nGotoOfft;` |
|         - |  6813 | `	sxi32 rc;` |
|         - |  6814 | `	/* Attach the new function */` |
|   2698767 |  6815 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2698767 |  6816 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6817 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6818 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6819 | `		return SXERR_ABORT;` |
|         - |  6820 | `	}` |
|   2698767 |  6821 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6822 | `	/* Swap bytecode containers */` |
|   2698767 |  6823 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2698767 |  6824 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6825 | `	/* Emit constructor property promotion prologue:` |
|         - |  6826 | `	 *   $this->NAME = $NAME;` |
|         - |  6827 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6828 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6829 | `	{` |
|   2698767 |  6830 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6831 | `		sxu32 i;` |
|   4634249 |  6832 | `		for( i = 0; i < nArg; i++ ){` |
|   1935487 |  6833 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6834 | `			char *zSrc;` |
|         - |  6835 | `			sxu32 nSrc,nName;` |
|         - |  6836 | `			SySet sToken;` |
|         - |  6837 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6838 | `			sxi32 rcPromote;` |
|   1935487 |  6839 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1935413 |  6840 | `				continue;` |
|         - |  6841 | `			}` |
|         - |  6842 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6843 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6844 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6845 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6846 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6847 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6848 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6849 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6850 | `			if( zSrc == 0 ){` |
|       ! 0 |  6851 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6852 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6853 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6854 | `				return SXERR_ABORT;` |
|         - |  6855 | `			}` |
|         - |  6856 | `			{` |
|        79 |  6857 | `				char *z = zSrc;` |
|        79 |  6858 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6859 | `				z += sizeof("$this->")-1;` |
|        79 |  6860 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6861 | `				z += nName;` |
|        79 |  6862 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6863 | `				z += sizeof(" = $")-1;` |
|        79 |  6864 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6865 | `				z += nName;` |
|        79 |  6866 | `				*z = 0;` |
|         - |  6867 | `			}` |
|        79 |  6868 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6869 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6870 | `			pTmpIn = pGen->pIn;` |
|        79 |  6871 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6872 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6873 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6874 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6875 | `			pGen->pIn = pTmpIn;` |
|        79 |  6876 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6877 | `			SySetRelease(&sToken);` |
|        79 |  6878 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6879 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6880 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6881 | `				return SXERR_ABORT;` |
|         - |  6882 | `			}` |
|         - |  6883 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6884 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6885 | `		}` |
|         - |  6886 | `	}` |
|         - |  6887 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6888 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6889 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6890 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6891 | `	{` |
|   2698767 |  6892 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2698767 |  6893 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6894 | `		/* Compile the body */` |
|   2698767 |  6895 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2698767 |  6896 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6897 | `	}` |
|         - |  6898 | `	/* Fix exception jumps now the destination is resolved */` |
|   2698767 |  6899 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6900 | `	/* Emit the final return if not yet done */` |
|   2698767 |  6901 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6902 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2698767 |  6903 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6904 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6905 | `	}` |
|   2698767 |  6906 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6907 | `	/* Restore the default container */` |
|   2698767 |  6908 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6909 | `	/* Leave function block */` |
|   2698767 |  6910 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2698767 |  6911 | `	if( rc == SXERR_ABORT ){` |
|         - |  6912 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6913 | `		return SXERR_ABORT;` |
|         - |  6914 | `	}` |
|         - |  6915 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6916 | `	{` |
|   2698767 |  6917 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6918 | `		sxu32 i;` |
|  78663625 |  6919 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  75976563 |  6920 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11705 |  6921 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11705 |  6922 | `				break;` |
|         - |  6923 | `			}` |
|  37982434 |  6924 | `		}` |
|         - |  6925 | `	}` |
|   2698767 |  6926 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6927 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11705 |  6928 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6929 | `			return SXERR_ABORT;` |
|         - |  6930 | `		}` |
|      5850 |  6931 | `	}` |
|         - |  6932 | `	/* All done, function body compiled */` |
|   2698767 |  6933 | `	return SXRET_OK;` |
|   1349386 |  6934 | `}` |
|         - |  6935 | `/*` |
|         - |  6936 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  6937 | ` * According to the PHP language reference manual.` |
|         - |  6938 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  6939 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  6940 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  6941 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  6942 | ` *  Functions need not be defined before they are referenced.` |
|         - |  6943 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  6944 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  6945 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  6946 | ` *  calls with over 32-64 recursion levels.` |
|         - |  6947 | ` *` |
|         - |  6948 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  6949 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  6950 | ` * on these extension.` |
|         - |  6951 | ` */` |
|         - |  6952 | `/*` |
|         - |  6953 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  6954 | ` */` |
|       570 |  6955 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  6956 | `{` |
|         - |  6957 | `	sxu32 i;` |
|      1611 |  6958 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  6959 | `		int a = zA[i], b = zB[i];` |
|      1381 |  6960 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  6961 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  6962 | `		if( a != b ) return a - b;` |
|       523 |  6963 | `	}` |
|       235 |  6964 | `	return 0;` |
|       290 |  6965 | `}` |
|         - |  6966 | `/*` |
|         - |  6967 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  6968 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  6969 | ` * (which are positive bit values stored in sxu32).` |
|         - |  6970 | ` */` |
|         - |  6971 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  6972 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  6973 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  6974 |  |
|         - |  6975 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  6976 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  6977 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  6978 |  |
|         - |  6979 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  6980 | `struct PhlTypeAtom {` |
|         - |  6981 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  6982 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  6983 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  6984 | `	sxu32 nCanon;` |
|         - |  6985 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  6986 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  6987 | `};` |
|         - |  6988 |  |
|         - |  6989 | `/*` |
|         - |  6990 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  6991 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  6992 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  6993 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  6994 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  6995 | ` * already be consumed by the caller.` |
|         - |  6996 | ` */` |
|    127506 |  6997 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  6998 | `{` |
|    127511 |  6999 | `	SyToken *pIn = pGen->pIn;` |
|    127511 |  7000 | `	SyZero(pOut, sizeof(*pOut));` |
|    127511 |  7001 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    127511 |  7002 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7003 | `		return SXERR_SYNTAX;` |
|         - |  7004 | `	}` |
|         - |  7005 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    127511 |  7006 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|         8 |  7007 | `		pIn++;` |
|         8 |  7008 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7009 | `			return SXERR_SYNTAX;` |
|         - |  7010 | `		}` |
|         3 |  7011 | `	}` |
|    127511 |  7012 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7013 | `		return SXERR_SYNTAX;` |
|         - |  7014 | `	}` |
|    127511 |  7015 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     88849 |  7016 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     88849 |  7017 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11499 |  7018 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     83102 |  7019 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        81 |  7020 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     77317 |  7021 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19711 |  7022 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67426 |  7023 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57491 |  7024 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     28830 |  7025 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        40 |  7026 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        69 |  7027 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        27 |  7028 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        37 |  7029 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        14 |  7030 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        23 |  7031 | `			pOut->nType = SXU32_HIGH;` |
|        23 |  7032 | `			pOut->sClass = pIn->sData;` |
|        13 |  7033 | `		}else{` |
|         3 |  7034 | `			return SXERR_SYNTAX;` |
|         - |  7035 | `		}` |
|     88847 |  7036 | `		pIn++;` |
|     44426 |  7037 | `	}else{` |
|         - |  7038 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  7039 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38667 |  7040 | `		SyString *pT = &pIn->sData;` |
|     38667 |  7041 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  7042 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  7043 | `			pIn++;` |
|     38652 |  7044 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  7045 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  7046 | `			pIn++;` |
|     38551 |  7047 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        26 |  7048 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        26 |  7049 | `			pIn++;` |
|        15 |  7050 | `		}else{` |
|         - |  7051 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38443 |  7052 | `			SyToken *pFirst = pIn;` |
|     38443 |  7053 | `			SyToken *pLast = pIn;` |
|     38443 |  7054 | `			pOut->nType = SXU32_HIGH;` |
|     38443 |  7055 | `			pOut->sClass = pIn->sData;` |
|     38443 |  7056 | `			pIn++;` |
|     57660 |  7057 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38446 |  7058 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  7059 | `				pLast = &pIn[1];` |
|         3 |  7060 | `				pIn += 2;` |
|         1 |  7061 | `			}` |
|     38443 |  7062 | `			if( pLast != pFirst ){` |
|         3 |  7063 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  7064 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  7065 | `				pOut->sClass.zString = zFirst;` |
|         3 |  7066 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  7067 | `			}` |
|         - |  7068 | `		}` |
|         - |  7069 | `	}` |
|    127509 |  7070 | `	pGen->pIn = pIn;` |
|    127509 |  7071 | `	return SXRET_OK;` |
|     63758 |  7072 | `}` |
|         - |  7073 |  |
|         - |  7074 | `/*` |
|         - |  7075 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  7076 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  7077 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  7078 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  7079 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  7080 | ` */` |
|    127328 |  7081 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  7082 | `{` |
|         - |  7083 | `	int i;` |
|    127333 |  7084 | `	int nNonNull = 0;` |
|    127333 |  7085 | `	int bAnyIntersection = 0;` |
|         - |  7086 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    127333 |  7087 | `	sxu32 nMaxGroup = 0;` |
|   4201829 |  7088 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    254813 |  7089 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127485 |  7090 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127455 |  7091 | `			nNonNull++;` |
|    127455 |  7092 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    127455 |  7093 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    127455 |  7094 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     63725 |  7095 | `			}` |
|     63725 |  7096 | `		}` |
|     63745 |  7097 | `	}` |
|    254761 |  7098 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127457 |  7099 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  7100 | `			bAnyIntersection = 1;` |
|        29 |  7101 | `			break;` |
|         - |  7102 | `		}` |
|     63719 |  7103 | `	}` |
|    127333 |  7104 | `	if( bAnyIntersection ){` |
|         - |  7105 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  7106 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  7107 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  7108 | `		sxu32 g, nGroups = 0;` |
|        29 |  7109 | `		int bFirstGroup = 1;` |
|        59 |  7110 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  7111 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  7112 | `			int bFirstMember = 1;` |
|         - |  7113 | `			int bWrap;` |
|        35 |  7114 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  7115 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  7116 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  7117 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  7118 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  7119 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  7120 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  7121 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  7122 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  7123 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  7124 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  7125 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  7126 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  7127 | `				}else{` |
|         6 |  7128 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7129 | `				}` |
|        59 |  7130 | `				bFirstMember = 0;` |
|        32 |  7131 | `			}` |
|        35 |  7132 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7133 | `			bFirstGroup = 0;` |
|        20 |  7134 | `		}` |
|        29 |  7135 | `		if( bNullable ){` |
|       ! 0 |  7136 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7137 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7138 | `		}` |
|        83 |  7139 | `		return;` |
|         - |  7140 | `	}` |
|    127309 |  7141 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7142 | `		/* Shorthand: ?T */` |
|       113 |  7143 | `		for( i = 0; i < nAtoms; i++ ){` |
|       113 |  7144 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       113 |  7145 | `			SyBlobAppend(pBlob, "?", 1);` |
|       113 |  7146 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        23 |  7147 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  7148 | `			}else{` |
|        93 |  7149 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7150 | `			}` |
|       113 |  7151 | `			return;` |
|       ! 0 |  7152 | `		}` |
|       ! 0 |  7153 | `	}` |
|         - |  7154 | `	{` |
|    127201 |  7155 | `		int bFirst = 1;` |
|         - |  7156 | `		/* 1) Classes in declaration order */` |
|    254505 |  7157 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127309 |  7158 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38393 |  7159 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38393 |  7160 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38393 |  7161 | `				bFirst = 0;` |
|     19194 |  7162 | `			}` |
|     63657 |  7163 | `		}` |
|         - |  7164 | `		/* 2) Built-ins in canonical order */` |
|         - |  7165 | `		{` |
|         - |  7166 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7167 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7168 | `			int k;` |
|    890377 |  7169 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1438179 |  7170 | `				for( i = 0; i < nAtoms; i++ ){` |
|    763717 |  7171 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     88719 |  7172 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     88719 |  7173 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     88719 |  7174 | `						bFirst = 0;` |
|     88719 |  7175 | `						break;` |
|         - |  7176 | `					}` |
|    337504 |  7177 | `				}` |
|    381593 |  7178 | `			}` |
|         - |  7179 | `		}` |
|         - |  7180 | `		/* 3) null suffix */` |
|    127201 |  7181 | `		if( bNullable ){` |
|        19 |  7182 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        19 |  7183 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7184 | `		}` |
|         - |  7185 | `	}` |
|     63669 |  7186 | `}` |
|         - |  7187 |  |
|         - |  7188 | `/*` |
|         - |  7189 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7190 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7191 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7192 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7193 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7194 | ` * whether it was parenthesized.` |
|         - |  7195 | ` *` |
|         - |  7196 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7197 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7198 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7199 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7200 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7201 | ` */` |
|    127480 |  7202 | `static sxi32 GenStateParsePart(` |
|         - |  7203 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7204 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7205 | `{` |
|         - |  7206 | `	sxi32 rc;` |
|    127485 |  7207 | `	int nMembers = 0;` |
|    127485 |  7208 | `	int bParen = 0;` |
|    127485 |  7209 | `	*pnMembers = 0;` |
|    127485 |  7210 | `	*pbParen = 0;` |
|    127485 |  7211 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7212 | `		bParen = 1;` |
|         9 |  7213 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7214 | `	}` |
|     63740 |  7215 | `	for(;;){` |
|    127511 |  7216 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7217 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7218 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7219 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7220 | `		}` |
|    127511 |  7221 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    127511 |  7222 | `		if( rc != SXRET_OK ){` |
|         3 |  7223 | `			return rc;` |
|         - |  7224 | `		}` |
|    127509 |  7225 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    127509 |  7226 | `		(*pnAtoms)++;` |
|    127509 |  7227 | `		nMembers++;` |
|         - |  7228 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    127509 |  7229 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7230 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7231 | `			if( pNext < pGen->pEnd` |
|        39 |  7232 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7233 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7234 | `				continue;` |
|         - |  7235 | `			}` |
|         4 |  7236 | `		}` |
|    127483 |  7237 | `		break;` |
|       ! 0 |  7238 | `	}` |
|    127483 |  7239 | `	if( bParen ){` |
|         9 |  7240 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7241 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7242 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7243 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7244 | `		}` |
|         9 |  7245 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7246 | `		if( nMembers < 2 ){` |
|       ! 0 |  7247 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7248 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7249 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7250 | `		}` |
|         3 |  7251 | `	}` |
|    127483 |  7252 | `	*pnMembers = nMembers;` |
|    127483 |  7253 | `	*pbParen = bParen;` |
|    127483 |  7254 | `	return SXRET_OK;` |
|     63745 |  7255 | `}` |
|         - |  7256 |  |
|         - |  7257 | `/*` |
|         - |  7258 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7259 | ` *` |
|         - |  7260 | ` * Outputs:` |
|         - |  7261 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7262 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7263 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7264 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7265 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7266 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7267 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7268 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7269 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7270 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7271 | ` *` |
|         - |  7272 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7273 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7274 | ` */` |
|    127344 |  7275 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7276 | `	ph7_gen_state *pGen,` |
|         - |  7277 | `	sxu32 *pnType,` |
|         - |  7278 | `	SyString *pClass,` |
|         - |  7279 | `	SySet *pAlts,` |
|         - |  7280 | `	sxi32 *piTypeFlags,` |
|         - |  7281 | `	SyString *pTypeText,` |
|         - |  7282 | `	int iNullableFlag,` |
|         - |  7283 | `	int iUnionFlag,` |
|         - |  7284 | `	int bAllowVoid,` |
|         - |  7285 | `	sxu32 nLine` |
|         5 |  7286 | `){` |
|         - |  7287 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    127349 |  7288 | `	int nAtoms = 0;` |
|    127349 |  7289 | `	int bShortNullable = 0;` |
|    127349 |  7290 | `	int bExplicitNull = 0;` |
|         - |  7291 | `	sxi32 rc;` |
|    127349 |  7292 | `	*pnType = 0;` |
|    127349 |  7293 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    127349 |  7294 | `	*piTypeFlags = 0;` |
|    127349 |  7295 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7296 |  |
|    127349 |  7297 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7298 | `		return SXRET_OK;` |
|         - |  7299 | `	}` |
|         - |  7300 | ``	/* Optional `?` shorthand prefix */`` |
|    127344 |  7301 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       101 |  7302 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       101 |  7303 | `		bShortNullable = 1;` |
|       101 |  7304 | `		pGen->pIn++;` |
|       101 |  7305 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7306 | `			return SXERR_SYNTAX;` |
|         - |  7307 | `		}` |
|        48 |  7308 | `	}` |
|         - |  7309 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7310 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7311 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7312 | `	{` |
|         - |  7313 | `		int nMembers, bParen;` |
|    127349 |  7314 | `		sxu32 iGroup = 0;` |
|    127349 |  7315 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    127349 |  7316 | `		if( rc != SXRET_OK ){` |
|         4 |  7317 | `			return rc;` |
|         - |  7318 | `		}` |
|         - |  7319 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7320 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7321 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7322 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7323 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    191222 |  7324 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    127556 |  7325 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7326 | `			if( bShortNullable ){` |
|         - |  7327 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7328 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7329 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7330 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7331 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7332 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7333 | `			}` |
|       141 |  7334 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7335 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7336 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7337 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7338 | `			}` |
|       141 |  7339 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7340 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7341 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7342 | `				return rc;` |
|         - |  7343 | `			}` |
|         5 |  7344 | `		}` |
|    127345 |  7345 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7346 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7347 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7348 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7349 | `		}` |
|         - |  7350 | `	}` |
|         - |  7351 | `	/* Validation pass.` |
|         - |  7352 | `	 *` |
|         - |  7353 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7354 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7355 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7356 | `	 */` |
|         - |  7357 | `	{` |
|         - |  7358 | `		int i, j;` |
|    127345 |  7359 | `		int bHasNonNull = 0;` |
|    127345 |  7360 | `		int bAnyIntersection = 0;` |
|         - |  7361 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7362 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7363 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4202225 |  7364 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    254847 |  7365 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127507 |  7366 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     63756 |  7367 | `		}` |
|    254791 |  7368 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127477 |  7369 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     63728 |  7370 | `		}` |
|         - |  7371 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7372 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    127345 |  7373 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7374 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7375 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7376 | `			return SXERR_SYNTAX;` |
|         - |  7377 | `		}` |
|    254833 |  7378 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7379 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7380 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7381 | ``			 * `true`/`false` in an intersection). */`` |
|    127505 |  7382 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7383 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7384 | `				if( bClassLike ){` |
|        53 |  7385 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7386 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7387 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7388 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7389 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7390 | `						bClassLike = 0;` |
|       ! 0 |  7391 | `					}` |
|        24 |  7392 | `				}` |
|        55 |  7393 | `				if( !bClassLike ){` |
|         - |  7394 | `					const char *zName; sxu32 nName;` |
|         3 |  7395 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7396 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7397 | `					}else{` |
|         3 |  7398 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7399 | `					}` |
|         4 |  7400 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7401 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7402 | `						(int)nName, zName);` |
|         3 |  7403 | `					return SXERR_SYNTAX;` |
|         - |  7404 | `				}` |
|        24 |  7405 | `			}` |
|    127503 |  7406 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7407 | `				if( nAtoms > 1 ){` |
|         3 |  7408 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7409 | `						"Void can only be used as a standalone type");` |
|         3 |  7410 | `					return SXERR_SYNTAX;` |
|         - |  7411 | `				}` |
|       175 |  7412 | `				if( !bAllowVoid ){` |
|       ! 0 |  7413 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7414 | `						"void cannot be used here");` |
|       ! 0 |  7415 | `					return SXERR_SYNTAX;` |
|         - |  7416 | `				}` |
|       175 |  7417 | `				if( bShortNullable ){` |
|       ! 0 |  7418 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7419 | `						"Void type cannot be nullable");` |
|       ! 0 |  7420 | `					return SXERR_SYNTAX;` |
|         - |  7421 | `				}` |
|        85 |  7422 | `			}` |
|    127501 |  7423 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7424 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7425 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7426 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7427 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        26 |  7428 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7429 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7430 | `					 * same as any other non-standalone use. */` |
|         6 |  7431 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7432 | `						"never can only be used as a standalone type");` |
|         6 |  7433 | `					return SXERR_SYNTAX;` |
|         - |  7434 | `				}` |
|        21 |  7435 | `				if( !bAllowVoid ){` |
|         - |  7436 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7437 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7438 | `						"never cannot be used as a parameter type");` |
|         3 |  7439 | `					return SXERR_SYNTAX;` |
|         - |  7440 | `				}` |
|         8 |  7441 | `			}` |
|    127495 |  7442 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7443 | `				bExplicitNull = 1;` |
|        19 |  7444 | `			}else{` |
|    127465 |  7445 | `				bHasNonNull = 1;` |
|         - |  7446 | `			}` |
|         - |  7447 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7448 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7449 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7450 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7451 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    127695 |  7452 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7453 | `				int bDup = 0;` |
|       207 |  7454 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7455 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7456 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7457 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7458 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7459 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7460 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7461 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7462 | `								aAtoms[j].sClass.zString,` |
|        34 |  7463 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7464 | `							bDup = 1;` |
|       ! 0 |  7465 | `						}` |
|        27 |  7466 | `					}else{` |
|         3 |  7467 | `						bDup = 1;` |
|         - |  7468 | `					}` |
|        23 |  7469 | `				}` |
|       195 |  7470 | `				if( bDup ){` |
|         - |  7471 | `					const char *zName;` |
|         - |  7472 | `					sxu32 nName;` |
|         3 |  7473 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7474 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7475 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7476 | `					}else{` |
|         3 |  7477 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7478 | `						nName = aAtoms[i].nCanon;` |
|         - |  7479 | `					}` |
|         4 |  7480 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7481 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7482 | `					return SXERR_SYNTAX;` |
|         - |  7483 | `				}` |
|        99 |  7484 | `			}` |
|     63749 |  7485 | `		}` |
|    127333 |  7486 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7487 | `			if( bShortNullable ){` |
|         - |  7488 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7489 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7490 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7491 | `				return SXERR_SYNTAX;` |
|         - |  7492 | `			}` |
|         - |  7493 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7494 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7495 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7496 | `			 * atom, so set it here. */` |
|         7 |  7497 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7498 | `		}` |
|         - |  7499 | `	}` |
|         - |  7500 | `	/* Compute nullability flag */` |
|    127333 |  7501 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       129 |  7502 | `		*piTypeFlags \|= iNullableFlag;` |
|        62 |  7503 | `	}` |
|         - |  7504 | `	/* Build canonical type text */` |
|    127333 |  7505 | `	if( pTypeText ){` |
|         - |  7506 | `		SyBlob sBlob;` |
|    127333 |  7507 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    190950 |  7508 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     63664 |  7509 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    127333 |  7510 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    190718 |  7511 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    127142 |  7512 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    127147 |  7513 | `			if( zDup ){` |
|    127147 |  7514 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     63571 |  7515 | `			}` |
|     63571 |  7516 | `		}` |
|    127333 |  7517 | `		SyBlobRelease(&sBlob);` |
|     63664 |  7518 | `	}` |
|         - |  7519 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7520 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7521 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7522 | `	{` |
|    127333 |  7523 | `		int nNonNull = 0;` |
|    127333 |  7524 | `		int iNonNullIdx = -1;` |
|         - |  7525 | `		int i;` |
|    254813 |  7526 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127485 |  7527 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127455 |  7528 | `				nNonNull++;` |
|    127455 |  7529 | `				iNonNullIdx = i;` |
|     63725 |  7530 | `			}` |
|     63745 |  7531 | `		}` |
|    127333 |  7532 | `		if( nNonNull <= 1 ){` |
|         - |  7533 | `			/* Fast path: store as single type. */` |
|    127227 |  7534 | `			if( iNonNullIdx >= 0 ){` |
|    127221 |  7535 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    127221 |  7536 | `				if( pA->nType == SXU32_HIGH ){` |
|     57551 |  7537 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19182 |  7538 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38369 |  7539 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38369 |  7540 | `					*pnType = SXU32_HIGH;` |
|     38369 |  7541 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    108039 |  7542 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7543 | `					*pnType = MEMOBJ_VOID;` |
|     88772 |  7544 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7545 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7546 | `				}else{` |
|     88671 |  7547 | `					*pnType = pA->nType;` |
|         - |  7548 | `				}` |
|     63608 |  7549 | `			}` |
|     63616 |  7550 | `		}else{` |
|         - |  7551 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7552 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7553 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7554 | `				ph7_type_alt sAlt;` |
|       249 |  7555 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7556 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7557 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7558 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7559 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7560 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7561 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7562 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7563 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7564 | `				}else{` |
|       145 |  7565 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7566 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7567 | `				}` |
|       239 |  7568 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7569 | `			}` |
|         - |  7570 | `		}` |
|         - |  7571 | `	}` |
|    127333 |  7572 | `	return SXRET_OK;` |
|     63677 |  7573 | `}` |
|         - |  7574 |  |
|         - |  7575 | `/*` |
|         - |  7576 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7577 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7578 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7579 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7580 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7581 | `` *          and union types `: T\|U`.`` |
|         - |  7582 | ` */` |
|   2836476 |  7583 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7584 | `{` |
|   2836481 |  7585 | `	sxi32 iFlags = 0;` |
|         - |  7586 | `	sxi32 rc;` |
|         - |  7587 | `	sxu32 nLine;` |
|   2836481 |  7588 | `	pFunc->nReturnType = 0;` |
|   2836481 |  7589 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2836481 |  7590 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7591 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7592 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7593 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7594 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7595 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2836481 |  7596 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2836481 |  7597 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2836481 |  7598 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2824371 |  7599 | `		return SXRET_OK;` |
|         - |  7600 | `	}` |
|     12115 |  7601 | `	pGen->pIn++; /* Skip ':' */` |
|     12115 |  7602 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7603 | `		return SXRET_OK;` |
|         - |  7604 | `	}` |
|     12115 |  7605 | `	nLine = pGen->pIn->nLine;` |
|     12115 |  7606 | `	rc = GenStateParseUnionTypeDecl(` |
|      6055 |  7607 | `		pGen,` |
|      6055 |  7608 | `		&pFunc->nReturnType,` |
|      6055 |  7609 | `		&pFunc->sReturnClass,` |
|      6055 |  7610 | `		&pFunc->aReturnUnion,` |
|         - |  7611 | `		&iFlags,` |
|      6055 |  7612 | `		&pFunc->sReturnTypeName,` |
|         - |  7613 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7614 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7615 | `		/* iUnionFlag */ 0,` |
|         - |  7616 | `		/* bAllowVoid */ 1,` |
|      6055 |  7617 | `		nLine);` |
|     12115 |  7618 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7619 | `		return SXERR_ABORT;` |
|         - |  7620 | `	}` |
|     12115 |  7621 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7622 | `		/* Error already reported */` |
|       ! 0 |  7623 | `		return SXERR_SYNTAX;` |
|         - |  7624 | `	}` |
|     12115 |  7625 | `	if( rc == SXERR_SYNTAX ){` |
|         9 |  7626 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 |  7627 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7628 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7629 | `				&pGen->pIn->sData);` |
|         6 |  7630 | `		}else{` |
|       ! 0 |  7631 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7632 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7633 | `		}` |
|         9 |  7634 | `		return SXERR_SYNTAX;` |
|         - |  7635 | `	}` |
|     12109 |  7636 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12109 |  7637 | `	return SXRET_OK;` |
|   1418243 |  7638 | `}` |
|         - |  7639 |  |
|    455704 |  7640 | `static sxi32 GenStateCompileFunc(` |
|         - |  7641 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7642 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7643 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7644 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7645 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7646 | `	)` |
|         5 |  7647 | `{` |
|         - |  7648 | `	ph7_vm_func *pFunc;` |
|         - |  7649 | `	SyToken *pEnd;` |
|         - |  7650 | `	sxu32 nLine;` |
|         - |  7651 | `	char *zName;` |
|         - |  7652 | `	sxi32 rc;` |
|         - |  7653 | `	/* Extract line number */` |
|    455709 |  7654 | `	nLine = pGen->pIn->nLine;` |
|         - |  7655 | `	/* Jump the left parenthesis '(' */` |
|    455709 |  7656 | `	pGen->pIn++;` |
|         - |  7657 | `	/* Delimit the function signature */` |
|    455709 |  7658 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    455709 |  7659 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7660 | `		/* Syntax error */` |
|         9 |  7661 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7662 | `		(void)pName;` |
|         9 |  7663 | `		if( rc == SXERR_ABORT ){` |
|         - |  7664 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7665 | `			return SXERR_ABORT;` |
|         - |  7666 | `		}` |
|         9 |  7667 | `		pGen->pIn = pGen->pEnd;` |
|         9 |  7668 | `		return SXRET_OK;` |
|         - |  7669 | `	}` |
|         - |  7670 | `	/* Create the function state */` |
|    455703 |  7671 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    455703 |  7672 | `	if( pFunc == 0 ){` |
|       ! 0 |  7673 | `		goto OutOfMem;` |
|         - |  7674 | `	}` |
|         - |  7675 | `	/* Build the function name, prepending namespace if active */` |
|    455710 |  7676 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7677 | `		SyBlob sFQN;` |
|         - |  7678 | `		sxu32 nLen;` |
|        16 |  7679 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7680 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7681 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7682 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7683 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7684 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7685 | `		SyBlobRelease(&sFQN);` |
|        16 |  7686 | `		if( zName == 0 ){` |
|       ! 0 |  7687 | `			goto OutOfMem;` |
|         - |  7688 | `		}` |
|        16 |  7689 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7690 | `	}else{` |
|    455689 |  7691 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    455689 |  7692 | `		if( zName == 0 ){` |
|       ! 0 |  7693 | `			goto OutOfMem;` |
|         - |  7694 | `		}` |
|    455689 |  7695 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7696 | `	}` |
|         - |  7697 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7698 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    455703 |  7699 | `	pFunc->nLine = nLine;` |
|    455703 |  7700 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    455703 |  7701 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7702 | `		return SXERR_ABORT;` |
|         - |  7703 | `	}` |
|    455703 |  7704 | `	if( pGen->pIn < pEnd ){` |
|         - |  7705 | `		/* Collect function arguments */` |
|    393783 |  7706 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    393783 |  7707 | `		if( rc == SXERR_ABORT ){` |
|         - |  7708 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7709 | `			return SXERR_ABORT;` |
|         - |  7710 | `		}` |
|    196889 |  7711 | `	}` |
|         - |  7712 | `	/* Point past ')' and parse optional return type ': type' */` |
|    455703 |  7713 | `	pGen->pIn = &pEnd[1];` |
|         - |  7714 | `	{` |
|    455703 |  7715 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    455703 |  7716 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7717 | `			return SXERR_ABORT;` |
|    455703 |  7718 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 |  7719 | `			return SXERR_SYNTAX;` |
|         - |  7720 | `		}` |
|         - |  7721 | `	}` |
|    455697 |  7722 | `	if( bHandleClosure ){` |
|         - |  7723 | `		ph7_vm_func_closure_env sEnv;` |
|       571 |  7724 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       566 |  7725 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       331 |  7726 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        91 |  7727 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7728 | `				/* Closure,record environment variable */` |
|        91 |  7729 | `				pGen->pIn++;` |
|        91 |  7730 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7731 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7732 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7733 | `						return SXERR_ABORT;` |
|         - |  7734 | `					}` |
|       ! 0 |  7735 | `				}` |
|        91 |  7736 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7737 | `				/* Compile until we hit the first closing parenthesis */` |
|       187 |  7738 | `				while( pGen->pIn < pGen->pEnd ){` |
|       187 |  7739 | `					int iFlagsLocal = 0;` |
|       187 |  7740 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        91 |  7741 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        91 |  7742 | `						break;` |
|         - |  7743 | `					}` |
|       101 |  7744 | `					nLineLocal = pGen->pIn->nLine;` |
|       101 |  7745 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7746 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7747 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7748 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7749 | `						pGen->pIn++;` |
|        27 |  7750 | `					}` |
|        96 |  7751 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       101 |  7752 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7753 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7754 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7755 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7756 | `								return SXERR_ABORT;` |
|         - |  7757 | `							}` |
|         - |  7758 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7759 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7760 | `								pGen->pIn++;` |
|       ! 0 |  7761 | `							}` |
|       ! 0 |  7762 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7763 | `								pGen->pIn++;` |
|       ! 0 |  7764 | `							}` |
|       ! 0 |  7765 | `							break;` |
|         - |  7766 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7767 | `					}else{` |
|         - |  7768 | `						SyString *pNameLocal;` |
|         - |  7769 | `						char *zDup;` |
|         - |  7770 | `						/* Duplicate variable name */` |
|       101 |  7771 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       101 |  7772 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       101 |  7773 | `						if( zDup ){` |
|         - |  7774 | `							/* Zero the structure */` |
|       101 |  7775 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       101 |  7776 | `							sEnv.iFlags = iFlagsLocal;` |
|       101 |  7777 | `							sEnv.nIdx = SXU32_HIGH;` |
|       101 |  7778 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       101 |  7779 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       116 |  7780 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7781 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7782 | `									got_this = 1;` |
|       ! 0 |  7783 | `							}` |
|         - |  7784 | `							/* Save imported variable */` |
|       101 |  7785 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        53 |  7786 | `						}else{` |
|       ! 0 |  7787 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7788 | `							 return SXERR_ABORT;` |
|         - |  7789 | `						}` |
|         - |  7790 | `					}` |
|       101 |  7791 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       113 |  7792 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7793 | `						/* Ignore trailing commas */` |
|        13 |  7794 | `						pGen->pIn++;` |
|         1 |  7795 | `					}` |
|         5 |  7796 | `				}` |
|         - |  7797 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7798 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7799 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7800 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7801 | `				 * legacy pre-use position. */` |
|        91 |  7802 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7803 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7804 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7805 | `						return SXERR_ABORT;` |
|         7 |  7806 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7807 | `						return SXERR_SYNTAX;` |
|         - |  7808 | `					}` |
|         3 |  7809 | `				}` |
|        43 |  7810 | `		}` |
|       571 |  7811 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7812 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7813 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7814 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7815 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7816 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7817 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7818 | `			 * closure never binds $this (php). */` |
|       563 |  7819 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       563 |  7820 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       563 |  7821 | `			sEnv.nIdx = SXU32_HIGH;` |
|       563 |  7822 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       563 |  7823 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       563 |  7824 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       279 |  7825 | `		}` |
|       571 |  7826 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7827 | `			/* Mark as closure */` |
|       565 |  7828 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       280 |  7829 | `		}` |
|       283 |  7830 | `	}` |
|         - |  7831 | `	/* Compile the body */` |
|    455697 |  7832 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    455697 |  7833 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7834 | `		return SXERR_ABORT;` |
|         - |  7835 | `	}` |
|         - |  7836 | `	/* The cursor sits just past the body's closing brace */` |
|    455697 |  7837 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    455697 |  7838 | `	if( ppFunc ){` |
|    455697 |  7839 | `		*ppFunc = pFunc;` |
|    227846 |  7840 | `	}` |
|    455697 |  7841 | `	rc = SXRET_OK;` |
|    455697 |  7842 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7843 | `		/* Finally register the function */` |
|    455137 |  7844 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    227566 |  7845 | `	}` |
|    455697 |  7846 | `	if( rc == SXRET_OK ){` |
|    455697 |  7847 | `		return SXRET_OK;` |
|         - |  7848 | `	}` |
|         - |  7849 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7850 | `OutOfMem:` |
|         - |  7851 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7852 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7853 | `	 */` |
|       ! 0 |  7854 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7855 | `	return SXERR_ABORT;` |
|    227857 |  7856 | `}` |
|         - |  7857 | `/*` |
|         - |  7858 | ` * Compile a standard PHP function.` |
|         - |  7859 | ` *  Refer to the block-comment above for more information.` |
|         - |  7860 | ` */` |
|    455146 |  7861 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7862 | `{` |
|         - |  7863 | `	SyString *pName;` |
|         - |  7864 | `	sxi32 iFlags;` |
|         - |  7865 | `	sxu32 nKwLine;` |
|         - |  7866 | `	sxu32 nLine;` |
|         - |  7867 | `	sxi32 rc;` |
|         - |  7868 |  |
|    455151 |  7869 | `	nLine = pGen->pIn->nLine;` |
|    455151 |  7870 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    455151 |  7871 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    455151 |  7872 | `	iFlags = 0;` |
|    455151 |  7873 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7874 | `		/* Return by reference,remember that */` |
|        12 |  7875 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7876 | `		/* Jump the '&' token */` |
|        12 |  7877 | `		pGen->pIn++;` |
|         5 |  7878 | `	}` |
|    455151 |  7879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7880 | `		/* Invalid function name */` |
|         7 |  7881 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         7 |  7882 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7883 | `			return SXERR_ABORT;` |
|         - |  7884 | `		}` |
|         - |  7885 | `		/* Sychronize with the next semi-colon or braces*/` |
|        21 |  7886 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        15 |  7887 | `			pGen->pIn++;` |
|         1 |  7888 | `		}` |
|         7 |  7889 | `		return SXRET_OK;` |
|         - |  7890 | `	}` |
|    455145 |  7891 | `	pName = &pGen->pIn->sData;` |
|    455145 |  7892 | `	nLine = pGen->pIn->nLine;` |
|         - |  7893 | `	/* Jump the function name */` |
|    455145 |  7894 | `	pGen->pIn++;` |
|    455145 |  7895 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7896 | `		/* Syntax error */` |
|         3 |  7897 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  7898 | `		if( rc == SXERR_ABORT ){` |
|         - |  7899 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7900 | `			return SXERR_ABORT;` |
|         - |  7901 | `		}` |
|         - |  7902 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  7903 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  7904 | `			pGen->pIn++;` |
|       ! 0 |  7905 | `		}` |
|         3 |  7906 | `		return SXRET_OK;` |
|         - |  7907 | `	}` |
|         - |  7908 | `	/* Compile function body */` |
|         - |  7909 | `	{` |
|    455143 |  7910 | `		ph7_vm_func *pFuncState = 0;` |
|    455143 |  7911 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    455143 |  7912 | `		if( pFuncState ){` |
|         - |  7913 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    455131 |  7914 | `			pFuncState->nLine = nKwLine;` |
|    227563 |  7915 | `		}` |
|         - |  7916 | `	}` |
|    455143 |  7917 | `	return rc;` |
|    227578 |  7918 | `}` |
|         - |  7919 | `/*` |
|         - |  7920 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  7921 | ` * According to the PHP language reference manual` |
|         - |  7922 | ` *  Visibility:` |
|         - |  7923 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  7924 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  7925 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  7926 | ` *  Members declared protected can be accessed only within the class` |
|         - |  7927 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  7928 | ` *  may only be accessed by the class that defines the member.` |
|         - |  7929 | ` */` |
|   3124814 |  7930 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  7931 | `{` |
|   3124819 |  7932 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    251789 |  7933 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2873035 |  7934 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    190687 |  7935 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  7936 | `	}` |
|         - |  7937 | `	/* Assume public by default */` |
|   2682353 |  7938 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1562412 |  7939 | `}` |
|         - |  7940 | `/*` |
|         - |  7941 | ` * Compile a class constant.` |
|         - |  7942 | ` * According to the PHP language reference manual` |
|         - |  7943 | ` *  Class Constants` |
|         - |  7944 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  7945 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  7946 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  7947 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  7948 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  7949 | ` *   It's also possible for interfaces to have constants.` |
|         - |  7950 | ` * Symisc eXtension.` |
|         - |  7951 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  7952 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  7953 | ` *  Example:` |
|         - |  7954 | ` *   class Test{` |
|         - |  7955 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  7956 | ` *   };` |
|         - |  7957 | ` *   var_dump(TEST::MyConst);` |
|         - |  7958 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  7959 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  7960 | ` */` |
|         - |  7961 | `/*` |
|         - |  7962 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  7963 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  7964 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  7965 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  7966 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  7967 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  7968 | ` */` |
|    289888 |  7969 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  7970 | `{` |
|         - |  7971 | `	SyToken *p0, *p1;` |
|    289893 |  7972 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7973 | `		return 0;` |
|         - |  7974 | `	}` |
|    289893 |  7975 | `	p0 = pGen->pIn;` |
|         - |  7976 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    289893 |  7977 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  7978 | `		return 1;` |
|         - |  7979 | `	}` |
|    289893 |  7980 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  7981 | `		return 1;` |
|         - |  7982 | `	}` |
|         - |  7983 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  7984 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  7985 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    289889 |  7986 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    289889 |  7987 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    289889 |  7988 | `		if( p1 ){` |
|    289889 |  7989 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  7990 | `				return 1;` |
|         - |  7991 | `			}` |
|    289859 |  7992 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  7993 | `				return 1;` |
|         - |  7994 | `			}` |
|    144925 |  7995 | `		}` |
|    144925 |  7996 | `	}` |
|    289855 |  7997 | `	return 0;` |
|    144949 |  7998 | `}` |
|         - |  7999 | `/*` |
|         - |  8000 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  8001 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  8002 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  8003 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  8004 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  8005 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  8006 | ` * Peek only; never consumes tokens.` |
|         - |  8007 | ` */` |
|        24 |  8008 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  8009 | `{` |
|        28 |  8010 | `	SyToken *p = pGen->pIn;` |
|        39 |  8011 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  8012 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  8013 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  8014 | `	}` |
|        28 |  8015 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  8016 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  8017 | `	}` |
|         6 |  8018 | `	p++;` |
|         - |  8019 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  8020 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  8021 | `}` |
|         - |  8022 | `/*` |
|         - |  8023 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  8024 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  8025 | `` * `$o->new`), not a `new` expression.`` |
|         - |  8026 | ` */` |
|       110 |  8027 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  8028 | `{` |
|         - |  8029 | `	sxi32 iOp;` |
|       114 |  8030 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  8031 | `		return 0;` |
|         - |  8032 | `	}` |
|       104 |  8033 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  8034 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  8035 | `}` |
|         - |  8036 | `/*` |
|         - |  8037 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  8038 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  8039 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  8040 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  8041 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  8042 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  8043 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  8044 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  8045 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  8046 | ` *` |
|         - |  8047 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  8048 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  8049 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  8050 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  8051 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  8052 | ` */` |
|    622222 |  8053 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  8054 | `{` |
|    622227 |  8055 | `	SyToken *p = pGen->pIn;` |
|    622227 |  8056 | `	int iDepth = 0;` |
|   1630563 |  8057 | `	while( p < pGen->pEnd ){` |
|   1630563 |  8058 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    622175 |  8059 | `			break; /* end of this initializer */` |
|         - |  8060 | `		}` |
|   1008388 |  8061 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    508027 |  8062 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7656 |  8063 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  8064 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  8065 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  8066 | `			 * expression. */` |
|         3 |  8067 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  8068 | `			p++;` |
|         3 |  8069 | `			if( bArrow ){` |
|         - |  8070 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  8071 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  8072 | `				int iBase = iDepth;` |
|        17 |  8073 | `				while( p < pGen->pEnd ){` |
|        17 |  8074 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  8075 | `						iDepth++;` |
|        15 |  8076 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  8077 | `						if( iDepth <= iBase ){` |
|       ! 0 |  8078 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  8079 | `						}` |
|         5 |  8080 | `						iDepth--;` |
|        11 |  8081 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  8082 | `						break;` |
|         - |  8083 | `					}` |
|        15 |  8084 | `					p++;` |
|         1 |  8085 | `				}` |
|         2 |  8086 | `			}else{` |
|         - |  8087 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  8088 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  8089 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  8090 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  8091 | `				int iLocal = 0;` |
|       ! 0 |  8092 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  8093 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  8094 | `						break; /* body brace */` |
|         - |  8095 | `					}` |
|       ! 0 |  8096 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  8097 | `						iLocal++;` |
|       ! 0 |  8098 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  8099 | `						if( iLocal > 0 ){` |
|       ! 0 |  8100 | `							iLocal--;` |
|       ! 0 |  8101 | `						}` |
|       ! 0 |  8102 | `					}` |
|       ! 0 |  8103 | `					p++;` |
|       ! 0 |  8104 | `				}` |
|       ! 0 |  8105 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  8106 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  8107 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  8108 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  8109 | `							iBrace++;` |
|       ! 0 |  8110 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  8111 | `							iBrace--;` |
|       ! 0 |  8112 | `							if( iBrace == 0 ){` |
|       ! 0 |  8113 | `								p++;` |
|       ! 0 |  8114 | `								break;` |
|         - |  8115 | `							}` |
|       ! 0 |  8116 | `						}` |
|       ! 0 |  8117 | `						p++;` |
|       ! 0 |  8118 | `					}` |
|       ! 0 |  8119 | `				}` |
|         - |  8120 | `			}` |
|         3 |  8121 | `			continue;` |
|         - |  8122 | `		}` |
|   1008391 |  8123 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  8124 | `			if( iDepth == 0 ){` |
|         - |  8125 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  8126 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  8127 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  8128 | `				 * is legal — don't scan into it. */` |
|        45 |  8129 | `				break;` |
|         - |  8130 | `			}` |
|       ! 0 |  8131 | `			iDepth++;` |
|   1008347 |  8132 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42021 |  8133 | `			iDepth++;` |
|    987339 |  8134 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     42019 |  8135 | `			if( iDepth > 0 ){` |
|     42019 |  8136 | `				iDepth--;` |
|     21007 |  8137 | `			}` |
|    945324 |  8138 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    336253 |  8139 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8140 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8141 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8142 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  8143 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  8144 | `				return 1;` |
|         - |  8145 | `			}` |
|       ! 0 |  8146 | `		}` |
|   1008339 |  8147 | `		p++;` |
|         5 |  8148 | `	}` |
|    622219 |  8149 | `	return 0;` |
|    311116 |  8150 | `}` |
|         - |  8151 | `/*` |
|         - |  8152 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8153 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8154 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8155 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8156 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8157 | ` * share the same backing.` |
|         - |  8158 | ` */` |
|       350 |  8159 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8160 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8161 | `{` |
|       355 |  8162 | `	pAttr->nType = nType;` |
|       355 |  8163 | `	pAttr->sClass = *pClass;` |
|       355 |  8164 | `	pAttr->sTypeName = *pTypeName;` |
|       355 |  8165 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8166 | `		sxu32 i;` |
|        73 |  8167 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8168 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8169 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8170 | `		}` |
|        11 |  8171 | `	}` |
|       355 |  8172 | `}` |
|    289888 |  8173 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8174 | `{` |
|    289893 |  8175 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8176 | `	SySet *pInstrContainer;` |
|         - |  8177 | `	ph7_class_attr *pCons;` |
|         - |  8178 | `	SyString *pName;` |
|         - |  8179 | `	sxi32 rc;` |
|    289893 |  8180 | `	sxu32 nType = 0;` |
|         - |  8181 | `	SyString sTypeClass;` |
|         - |  8182 | `	SyString sTypeText;` |
|         - |  8183 | `	SySet aUnionAlts;` |
|    289893 |  8184 | `	sxi32 iTypeFlags = 0;` |
|    289893 |  8185 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    289893 |  8186 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    289893 |  8187 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8188 | `	/* Extract visibility level */` |
|    289893 |  8189 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8190 | `	/* Mark as constant */` |
|    289893 |  8191 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    289893 |  8192 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8193 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8194 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    289912 |  8195 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8196 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8197 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8198 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8199 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8200 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8201 | `		 * and success paths release. */` |
|        42 |  8202 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8203 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8204 | `			goto Synchronize;` |
|        42 |  8205 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8206 | `			return SXERR_ABORT;` |
|        42 |  8207 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8208 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8209 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8210 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8211 | `				return SXERR_ABORT;` |
|         - |  8212 | `			}` |
|       ! 0 |  8213 | `			goto Synchronize;` |
|         - |  8214 | `		}` |
|        42 |  8215 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8216 | `	}` |
|    144944 |  8217 | `loop:` |
|    289895 |  8218 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8219 | `		/* Invalid constant name */` |
|       ! 0 |  8220 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8221 | `		if( rc == SXERR_ABORT ){` |
|         - |  8222 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8223 | `			return SXERR_ABORT;` |
|         - |  8224 | `		}` |
|       ! 0 |  8225 | `		goto Synchronize;` |
|         - |  8226 | `	}` |
|         - |  8227 | `	/* Peek constant name */` |
|    289895 |  8228 | `	pName = &pGen->pIn->sData;` |
|         - |  8229 | `	/* Make sure the constant name isn't reserved */` |
|    289895 |  8230 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8231 | `		/* Reserved constant name */` |
|       ! 0 |  8232 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8233 | `		if( rc == SXERR_ABORT ){` |
|         - |  8234 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8235 | `			return SXERR_ABORT;` |
|         - |  8236 | `		}` |
|       ! 0 |  8237 | `		goto Synchronize;` |
|         - |  8238 | `	}` |
|         - |  8239 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    289895 |  8240 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8241 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8242 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8243 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8244 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8245 | `			return SXERR_ABORT;` |
|        42 |  8246 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8247 | `			goto Synchronize;` |
|         - |  8248 | `		}` |
|        18 |  8249 | `	}` |
|         - |  8250 | `	/* Advance the stream cursor */` |
|    289893 |  8251 | `	pGen->pIn++;` |
|    289893 |  8252 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8253 | `		/* Invalid declaration */` |
|       ! 0 |  8254 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8255 | `		if( rc == SXERR_ABORT ){` |
|         - |  8256 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8257 | `			return SXERR_ABORT;` |
|         - |  8258 | `		}` |
|       ! 0 |  8259 | `		goto Synchronize;` |
|         - |  8260 | `	}` |
|    289893 |  8261 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8262 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8263 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8264 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8265 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    289888 |  8266 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8267 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8268 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8269 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8270 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8271 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8272 | `			return SXERR_ABORT;` |
|         - |  8273 | `		}` |
|         6 |  8274 | `		goto Synchronize;` |
|         - |  8275 | `	}` |
|         - |  8276 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8277 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8278 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    289889 |  8279 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8280 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8281 | `			"New expressions are not supported in this context");` |
|         5 |  8282 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8283 | `			return SXERR_ABORT;` |
|         - |  8284 | `		}` |
|         5 |  8285 | `		goto Synchronize;` |
|         - |  8286 | `	}` |
|         - |  8287 | `	/* Allocate a new class attribute */` |
|    289885 |  8288 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    289885 |  8289 | `	if( pCons ){` |
|    289885 |  8290 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    289885 |  8291 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8292 | `			return SXERR_ABORT;` |
|         - |  8293 | `		}` |
|    144940 |  8294 | `	}` |
|    289885 |  8295 | `	if( pCons == 0 ){` |
|       ! 0 |  8296 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8297 | `		return SXERR_ABORT;` |
|         - |  8298 | `	}` |
|    289885 |  8299 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8300 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8301 | `	}` |
|         - |  8302 | `	/* Swap bytecode container */` |
|    289885 |  8303 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    289885 |  8304 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8305 | `	/* Compile constant value.` |
|         - |  8306 | `	 */` |
|    289885 |  8307 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    289885 |  8308 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8309 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8310 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8311 | `			return SXERR_ABORT;` |
|         - |  8312 | `		}` |
|         1 |  8313 | `	}` |
|         - |  8314 | `	/* Emit the done instruction */` |
|    289885 |  8315 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    289885 |  8316 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    289885 |  8317 | `	if( rc == SXERR_ABORT ){` |
|         - |  8318 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8319 | `		return SXERR_ABORT;` |
|         - |  8320 | `	}` |
|         - |  8321 | `	/* All done,install the constant */` |
|    289885 |  8322 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    289885 |  8323 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8324 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8325 | `		return SXERR_ABORT;` |
|         - |  8326 | `	}` |
|    289885 |  8327 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8328 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8329 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8330 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8331 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8332 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8333 | `				pTok--;` |
|       ! 0 |  8334 | `			}` |
|       ! 0 |  8335 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8336 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8337 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8338 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8339 | `				return SXERR_ABORT;` |
|         - |  8340 | `			}` |
|       ! 0 |  8341 | `		}else{` |
|         3 |  8342 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8343 | `				goto loop;` |
|         - |  8344 | `			}` |
|         - |  8345 | `		}` |
|       ! 0 |  8346 | `	}` |
|    289883 |  8347 | `	SySetRelease(&aUnionAlts);` |
|    289883 |  8348 | `	return SXRET_OK;` |
|         5 |  8349 | `Synchronize:` |
|        13 |  8350 | `	SySetRelease(&aUnionAlts);` |
|         - |  8351 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8352 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8353 | `		pGen->pIn++;` |
|         3 |  8354 | `	}` |
|        13 |  8355 | `	return SXERR_CORRUPT;` |
|    144949 |  8356 | `}` |
|         - |  8357 | `/*` |
|         - |  8358 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8359 | ` * According to the PHP language reference manual` |
|         - |  8360 | ` *  Properties` |
|         - |  8361 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8362 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8363 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8364 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8365 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8366 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8367 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8368 | ` * Symisc eXtension.` |
|         - |  8369 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8370 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8371 | ` *  Example:` |
|         - |  8372 | ` *   class Test{` |
|         - |  8373 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8374 | ` *   };` |
|         - |  8375 | ` *   var_dump(TEST::myVar);` |
|         - |  8376 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8377 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8378 | ` */` |
|         - |  8379 | `/*` |
|         - |  8380 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8381 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8382 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8383 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8384 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8385 | ` */` |
|   2334600 |  8386 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8387 | `{` |
|   2334605 |  8388 | `	SyToken *p = pStart;` |
|   2334605 |  8389 | `	int bFirst = 1;` |
|   2334605 |  8390 | `	if( p >= pEnd ) return 0;` |
|         - |  8391 | ``	/* Optional nullable `?` shorthand. */`` |
|   2334605 |  8392 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        35 |  8393 | `		p++;` |
|        35 |  8394 | `		if( p >= pEnd ) return 0;` |
|        16 |  8395 | `	}` |
|         - |  8396 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8397 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8398 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8399 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1167300 |  8400 | `	for(;;){` |
|   2334625 |  8401 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8402 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8403 | `			p++;` |
|         9 |  8404 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8405 | `			if( p >= pEnd ) return 0;` |
|         3 |  8406 | `			p++; /* skip ')' */` |
|         2 |  8407 | `		}else{` |
|         - |  8408 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8409 | ``			 * then any `&`-joined intersection members. */`` |
|   2334623 |  8410 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2334623 |  8411 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8412 | `				return 0;` |
|         - |  8413 | `			}` |
|         - |  8414 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8415 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8416 | `			 * may still appear at the initial dispatch site). */` |
|   2334623 |  8417 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2334575 |  8418 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2334570 |  8419 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    103336 |  8420 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2334293 |  8421 | `					return 0;` |
|         - |  8422 | `				}` |
|       141 |  8423 | `			}` |
|       335 |  8424 | `			p++;` |
|       337 |  8425 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8426 | `				p += 2;` |
|         1 |  8427 | `			}` |
|       498 |  8428 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       338 |  8429 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8430 | `				p++; /* skip '&' */` |
|         3 |  8431 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8432 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8433 | `				p++;` |
|         3 |  8434 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8435 | `					p += 2;` |
|       ! 0 |  8436 | `				}` |
|         1 |  8437 | `			}` |
|         - |  8438 | `		}` |
|       337 |  8439 | `		bFirst = 0;` |
|       332 |  8440 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8441 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8442 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8443 | `			continue;` |
|         - |  8444 | `		}` |
|       317 |  8445 | `		break;` |
|       ! 0 |  8446 | `	}` |
|       317 |  8447 | `	if( p >= pEnd ) return 0;` |
|       317 |  8448 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1167305 |  8449 | `}` |
|         - |  8450 |  |
|         - |  8451 | `/*` |
|         - |  8452 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8453 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8454 | ` * if not). Recognized forms:` |
|         - |  8455 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8456 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8457 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8458 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8459 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8460 | ` * on unrecoverable error.` |
|         - |  8461 | ` *` |
|         - |  8462 | ` * When a type is parsed:` |
|         - |  8463 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8464 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8465 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8466 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8467 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8468 | ` */` |
|       322 |  8469 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8470 | `	ph7_gen_state *pGen,` |
|         - |  8471 | `	sxu32 *pnType,` |
|         - |  8472 | `	SyString *pClass,` |
|         - |  8473 | `	sxi32 *piTypeFlags,` |
|         - |  8474 | `	SyString *pTypeText,` |
|         - |  8475 | `	SySet *pAlts` |
|         5 |  8476 | `){` |
|       327 |  8477 | `	sxi32 iFlags = 0;` |
|         - |  8478 | `	sxi32 rc;` |
|       327 |  8479 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8480 | `		return SXRET_OK;` |
|         - |  8481 | `	}` |
|         - |  8482 | `	/* If the first token is '$', there's no type */` |
|       327 |  8483 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8484 | `		return SXRET_OK;` |
|         - |  8485 | `	}` |
|       327 |  8486 | `	rc = GenStateParseUnionTypeDecl(` |
|       161 |  8487 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8488 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8489 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8490 | `		/* bAllowVoid */ 0,` |
|       322 |  8491 | `		pGen->pIn->nLine);` |
|       327 |  8492 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8493 | `		return rc;` |
|         - |  8494 | `	}` |
|         - |  8495 | `	/* Verify next token is '$' (start of property name) */` |
|       327 |  8496 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8497 | `		return SXERR_SYNTAX;` |
|         - |  8498 | `	}` |
|       327 |  8499 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       327 |  8500 | `	return SXRET_OK;` |
|       166 |  8501 | `}` |
|         - |  8502 |  |
|         - |  8503 | `/*` |
|         - |  8504 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8505 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8506 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8507 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8508 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8509 | ` * by the type parser itself before reaching here.` |
|         - |  8510 | ` *` |
|         - |  8511 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8512 | ` * use in the error message.` |
|         - |  8513 | ` */` |
|       498 |  8514 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8515 | `	sxu32 nType,` |
|         - |  8516 | `	const SyString *pClass,` |
|         - |  8517 | `	const char **pzName,` |
|         - |  8518 | `	sxu32 *pnName)` |
|         5 |  8519 | `{` |
|         - |  8520 | `	const char *z;` |
|         - |  8521 | `	sxu32 n;` |
|       503 |  8522 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       449 |  8523 | `		return 0;` |
|         - |  8524 | `	}` |
|        59 |  8525 | `	z = pClass->zString;` |
|        59 |  8526 | `	n = pClass->nByte;` |
|        59 |  8527 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8528 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8529 | `	}` |
|         - |  8530 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8531 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8532 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        53 |  8533 | `	return 0;` |
|       254 |  8534 | `}` |
|         - |  8535 |  |
|         - |  8536 | `/*` |
|         - |  8537 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8538 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8539 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8540 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8541 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8542 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8543 | ` *` |
|         - |  8544 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8545 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8546 | ` */` |
|       436 |  8547 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8548 | `	ph7_gen_state *pGen,` |
|         - |  8549 | `	ph7_class *pClass,` |
|         - |  8550 | `	const SyString *pMemberName,` |
|         - |  8551 | `	sxu32 nType,` |
|         - |  8552 | `	const SyString *pTypeClass,` |
|         - |  8553 | `	const SyString *pTypeText,` |
|         - |  8554 | `	SySet *pUnionAlts,` |
|         - |  8555 | `	const char *zErrFmt,` |
|         - |  8556 | `	sxu32 nLine)` |
|         5 |  8557 | `{` |
|       441 |  8558 | `	const char *zBad = 0;` |
|       441 |  8559 | `	sxu32 nBad = 0;` |
|         - |  8560 | `	SyString sFallback;` |
|         - |  8561 | `	const SyString *pBad;` |
|         - |  8562 | `	sxi32 rc;` |
|       441 |  8563 | `	int bDisallowed = 0;` |
|       441 |  8564 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8565 | `		bDisallowed = 1;` |
|       439 |  8566 | `	}else if( pUnionAlts ){` |
|         - |  8567 | `		sxu32 i;` |
|        95 |  8568 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8569 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8570 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8571 | `				bDisallowed = 1;` |
|         3 |  8572 | `				break;` |
|         - |  8573 | `			}` |
|        35 |  8574 | `		}` |
|        15 |  8575 | `	}` |
|       441 |  8576 | `	if( !bDisallowed ){` |
|       435 |  8577 | `		return SXRET_OK;` |
|         - |  8578 | `	}` |
|         - |  8579 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8580 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8581 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8582 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8583 | `		pBad = pTypeText;` |
|         5 |  8584 | `	}else{` |
|       ! 0 |  8585 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8586 | `		pBad = &sFallback;` |
|         - |  8587 | `	}` |
|        11 |  8588 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8589 | `		zErrFmt,` |
|         3 |  8590 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8591 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8592 | `		return SXERR_ABORT;` |
|         - |  8593 | `	}` |
|         8 |  8594 | `	return SXERR_SYNTAX;` |
|       223 |  8595 | `}` |
|         - |  8596 | `/*` |
|         - |  8597 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8598 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8599 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8600 | ` * than promoted to a lexer keyword.` |
|         - |  8601 | ` */` |
|  19843104 |  8602 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8603 | `{` |
|  20061121 |  8604 | `	return (pTok->nType & PH7_TK_ID)` |
|  10139564 |  8605 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  20061116 |  8606 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8607 | `}` |
|         - |  8608 | `/*` |
|         - |  8609 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8610 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8611 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8612 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8613 | ` */` |
|   7192928 |  8614 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8615 | `{` |
|   7192933 |  8616 | `	*pnTok = 0;` |
|   7192928 |  8617 | `	if( &pTok[3] < pEnd` |
|   6749855 |  8618 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5574012 |  8619 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2420629 |  8620 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8621 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8622 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8623 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8624 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8625 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8626 | `			*pnTok = 4;` |
|        17 |  8627 | `			return nKw;` |
|         - |  8628 | `		}` |
|       ! 0 |  8629 | `	}` |
|   7192917 |  8630 | `	return 0;` |
|   3596469 |  8631 | `}` |
|         - |  8632 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8633 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8634 | `{` |
|        17 |  8635 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8636 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8637 | `	}` |
|         5 |  8638 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8639 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8640 | `	}` |
|         3 |  8641 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8642 | `}` |
|    454616 |  8643 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8644 | `{` |
|    454621 |  8645 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8646 | `	ph7_class_attr *pAttr;` |
|         - |  8647 | `	SyString *pName;` |
|         - |  8648 | `	sxi32 rc;` |
|    454621 |  8649 | `	sxu32 nType = 0;` |
|         - |  8650 | `	SyString sTypeClass;` |
|         - |  8651 | `	SyString sTypeText;` |
|         - |  8652 | `	SySet aUnionAlts;` |
|    454621 |  8653 | `	sxi32 iTypeFlags = 0;` |
|    454621 |  8654 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    454621 |  8655 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    454621 |  8656 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8657 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8658 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8659 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    454621 |  8660 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8661 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8662 | `	}` |
|         - |  8663 | `	/* Extract visibility level */` |
|    454621 |  8664 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8665 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    454782 |  8666 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       327 |  8667 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       327 |  8668 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8669 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8670 | `			goto Synchronize;` |
|       327 |  8671 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8672 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8673 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8674 | `				&pGen->pIn->sData);` |
|       ! 0 |  8675 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8676 | `				return SXERR_ABORT;` |
|         - |  8677 | `			}` |
|       ! 0 |  8678 | `			goto Synchronize;` |
|       327 |  8679 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8680 | `			return SXERR_ABORT;` |
|         - |  8681 | `		}` |
|       161 |  8682 | `	}` |
|       ! 0 |  8683 | `loop:` |
|    454625 |  8684 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8685 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8686 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8687 | `			return SXERR_ABORT;` |
|         - |  8688 | `		}` |
|       ! 0 |  8689 | `		goto Synchronize;` |
|         - |  8690 | `	}` |
|    454625 |  8691 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    454625 |  8692 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8693 | `		/* Invalid attribute name */` |
|       ! 0 |  8694 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8695 | `		if( rc == SXERR_ABORT ){` |
|         - |  8696 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8697 | `			return SXERR_ABORT;` |
|         - |  8698 | `		}` |
|       ! 0 |  8699 | `		goto Synchronize;` |
|         - |  8700 | `	}` |
|         - |  8701 | `	/* Peek attribute name */` |
|    454625 |  8702 | `	pName = &pGen->pIn->sData;` |
|         - |  8703 | `	/* Advance the stream cursor */` |
|    454625 |  8704 | `	pGen->pIn++;` |
|    454625 |  8705 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8706 | `		/* Invalid declaration */` |
|         3 |  8707 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8708 | `		if( rc == SXERR_ABORT ){` |
|         - |  8709 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8710 | `			return SXERR_ABORT;` |
|         - |  8711 | `		}` |
|         3 |  8712 | `		goto Synchronize;` |
|         - |  8713 | `	}` |
|         - |  8714 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8715 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    454623 |  8716 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8717 | `		const char *zAvErr = 0;` |
|        19 |  8718 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8719 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8720 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8721 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8722 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8723 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8724 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8725 | `		}` |
|        13 |  8726 | `		if( zAvErr ){` |
|       ! 0 |  8727 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8728 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8729 | `				return SXERR_ABORT;` |
|         - |  8730 | `			}` |
|       ! 0 |  8731 | `			goto Synchronize;` |
|         - |  8732 | `		}` |
|         6 |  8733 | `	}` |
|         - |  8734 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8735 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    454623 |  8736 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8737 | `		const char *zRoErr = 0;` |
|        43 |  8738 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8739 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8740 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8741 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8742 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8743 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8744 | `		}` |
|        43 |  8745 | `		if( zRoErr ){` |
|        13 |  8746 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8747 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8748 | `				return SXERR_ABORT;` |
|         - |  8749 | `			}` |
|        13 |  8750 | `			goto Synchronize;` |
|         - |  8751 | `		}` |
|        14 |  8752 | `	}` |
|         - |  8753 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8754 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8755 | `	 * by the type parser. */` |
|    454613 |  8756 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       485 |  8757 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8758 | `			&sTypeText,` |
|       320 |  8759 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       160 |  8760 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       325 |  8761 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8762 | `			return SXERR_ABORT;` |
|       325 |  8763 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8764 | `			goto Synchronize;` |
|         - |  8765 | `		}` |
|       160 |  8766 | `	}` |
|         - |  8767 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    454613 |  8768 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8769 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8770 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8771 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8772 | `			return SXERR_ABORT;` |
|         - |  8773 | `		}` |
|         3 |  8774 | `		goto Synchronize;` |
|         - |  8775 | `	}` |
|         - |  8776 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8777 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8778 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8779 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8780 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8781 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    454611 |  8782 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8783 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8784 | `			"New expressions are not supported in this context");` |
|         6 |  8785 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8786 | `			return SXERR_ABORT;` |
|         - |  8787 | `		}` |
|         6 |  8788 | `		goto Synchronize;` |
|         - |  8789 | `	}` |
|         - |  8790 | `	/* Allocate a new class attribute */` |
|    454607 |  8791 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    454607 |  8792 | `	if( pAttr ){` |
|    454607 |  8793 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    454607 |  8794 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8795 | `			return SXERR_ABORT;` |
|         - |  8796 | `		}` |
|    227301 |  8797 | `	}` |
|    454607 |  8798 | `	if( pAttr == 0 ){` |
|       ! 0 |  8799 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8800 | `		return SXERR_ABORT;` |
|         - |  8801 | `	}` |
|    454607 |  8802 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       323 |  8803 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       159 |  8804 | `	}` |
|    454607 |  8805 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8806 | `		SySet *pInstrContainer;` |
|    332339 |  8807 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    332339 |  8808 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8809 | `		{` |
|         - |  8810 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8811 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8812 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8813 | `			 * compiler would otherwise run into the hook tokens. */` |
|    332339 |  8814 | `			SyToken *pScan = pGen->pIn;` |
|    332339 |  8815 | `			sxi32 iNest = 0;` |
|    718387 |  8816 | `			while( pScan < pGen->pEnd ){` |
|    718387 |  8817 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42019 |  8818 | `					iNest++;` |
|    697380 |  8819 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     42019 |  8820 | `					iNest--;` |
|    655366 |  8821 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    332339 |  8822 | `					break;` |
|         - |  8823 | `				}` |
|    386053 |  8824 | `				pScan++;` |
|         5 |  8825 | `			}` |
|    332339 |  8826 | `			pGen->pEnd = pScan;` |
|         - |  8827 | `		}` |
|         - |  8828 | `		/* Swap bytecode container */` |
|    332339 |  8829 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    332339 |  8830 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8831 | `		/* Compile attribute value.` |
|         - |  8832 | `		 */` |
|    332339 |  8833 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    332339 |  8834 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8835 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8836 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8837 | `				return SXERR_ABORT;` |
|         - |  8838 | `			}` |
|       ! 0 |  8839 | `		}` |
|         - |  8840 | `		/* Emit the done instruction */` |
|    332339 |  8841 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    332339 |  8842 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    332339 |  8843 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    332339 |  8844 | `		pGen->pEnd = pSavedDefEnd;` |
|    166167 |  8845 | `	}` |
|         - |  8846 | `	/* All done,install the attribute */` |
|    454607 |  8847 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    454607 |  8848 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8849 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8850 | `		return SXERR_ABORT;` |
|         - |  8851 | `	}` |
|    454607 |  8852 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8853 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8854 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8855 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8856 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8857 | `			return SXERR_ABORT;` |
|         - |  8858 | `		}` |
|        95 |  8859 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8860 | `			goto Synchronize;` |
|         - |  8861 | `		}` |
|        95 |  8862 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8863 | `		return SXRET_OK;` |
|         - |  8864 | `	}` |
|    454513 |  8865 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8866 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8867 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8868 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8869 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8870 | `				? "Interfaces may only include hooked properties"` |
|         - |  8871 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8872 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8873 | `			return SXERR_ABORT;` |
|         - |  8874 | `		}` |
|       ! 0 |  8875 | `		goto Synchronize;` |
|         - |  8876 | `	}` |
|    454513 |  8877 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8878 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8879 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8880 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8881 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8882 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8883 | `				pTok--;` |
|       ! 0 |  8884 | `			}` |
|       ! 0 |  8885 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8886 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8887 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8888 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8889 | `				return SXERR_ABORT;` |
|         - |  8890 | `			}` |
|       ! 0 |  8891 | `		}else{` |
|         5 |  8892 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8893 | `				goto loop;` |
|         - |  8894 | `			}` |
|         - |  8895 | `		}` |
|       ! 0 |  8896 | `	}` |
|    454509 |  8897 | `	SySetRelease(&aUnionAlts);` |
|    454509 |  8898 | `	return SXRET_OK;` |
|         9 |  8899 | `Synchronize:` |
|         - |  8900 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8901 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  8902 | `		pGen->pIn++;` |
|         3 |  8903 | `	}` |
|        22 |  8904 | `	SySetRelease(&aUnionAlts);` |
|        22 |  8905 | `	return SXERR_CORRUPT;` |
|    227313 |  8906 | `}` |
|         - |  8907 | `/*` |
|         - |  8908 | ` * Compile a class method.` |
|         - |  8909 | ` *` |
|         - |  8910 | ` * Refer to the official documentation for more information` |
|         - |  8911 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  8912 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  8913 | ` * overloading and many more.` |
|         - |  8914 | ` */` |
|   2380310 |  8915 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  8916 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  8917 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  8918 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  8919 | `	int doBody,          /* TRUE to process method body */` |
|         - |  8920 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  8921 | `	)` |
|         5 |  8922 | `{` |
|   2380315 |  8923 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2380315 |  8924 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  8925 | `	ph7_class_method *pMeth;` |
|         - |  8926 | `	sxi32 iFuncFlags;` |
|         - |  8927 | `	SyString *pName;` |
|         - |  8928 | `	SyToken *pEnd;` |
|         - |  8929 | `	sxi32 rc;` |
|         - |  8930 | `	/* Extract visibility level */` |
|   2380315 |  8931 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2380315 |  8932 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2380315 |  8933 | `	iFuncFlags = 0;` |
|   2380315 |  8934 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  8935 | `		/* Invalid method name */` |
|       ! 0 |  8936 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8937 | `		if( rc == SXERR_ABORT ){` |
|         - |  8938 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8939 | `			return SXERR_ABORT;` |
|         - |  8940 | `		}` |
|       ! 0 |  8941 | `		goto Synchronize;` |
|         - |  8942 | `	}` |
|   2380315 |  8943 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8944 | `		/* Return by reference,remember that */` |
|       ! 0 |  8945 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8946 | `		/* Jump the '&' token */` |
|       ! 0 |  8947 | `		pGen->pIn++;` |
|       ! 0 |  8948 | `	}` |
|   2380315 |  8949 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8950 | `		/* Invalid method name */` |
|       ! 0 |  8951 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8952 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8953 | `			return SXERR_ABORT;` |
|         - |  8954 | `		}` |
|       ! 0 |  8955 | `		goto Synchronize;` |
|         - |  8956 | `	}` |
|         - |  8957 | `	/* Peek method name */` |
|   2380315 |  8958 | `	pName = &pGen->pIn->sData;` |
|   2380315 |  8959 | `	nLine = pGen->pIn->nLine;` |
|         - |  8960 | `	/* Jump the method name */` |
|   2380315 |  8961 | `	pGen->pIn++;` |
|   2380315 |  8962 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8963 | `		/* Abstract method */` |
|    137301 |  8964 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  8965 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8966 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  8967 | `				&pClass->sName,pName);` |
|       ! 0 |  8968 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8969 | `				return SXERR_ABORT;` |
|         - |  8970 | `			}` |
|       ! 0 |  8971 | `		}` |
|         - |  8972 | `		/* Assemble method signature only */` |
|    137301 |  8973 | `		doBody = FALSE;` |
|     68648 |  8974 | `	}` |
|   2380315 |  8975 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8976 | `		/* Syntax error */` |
|       ! 0 |  8977 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  8978 | `		if( rc == SXERR_ABORT ){` |
|         - |  8979 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8980 | `			return SXERR_ABORT;` |
|         - |  8981 | `		}` |
|       ! 0 |  8982 | `		goto Synchronize;` |
|         - |  8983 | `	}` |
|         - |  8984 | `	/* Allocate a new class_method instance */` |
|   2380315 |  8985 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2380315 |  8986 | `	if( pMeth == 0 ){` |
|       ! 0 |  8987 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8988 | `		return SXERR_ABORT;` |
|         - |  8989 | `	}` |
|   2380315 |  8990 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2380315 |  8991 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2380315 |  8992 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8993 | `		return SXERR_ABORT;` |
|         - |  8994 | `	}` |
|         - |  8995 | `	/* Jump the left parenthesis '(' */` |
|   2380315 |  8996 | `	pGen->pIn++;` |
|   2380315 |  8997 | `	pEnd = 0; /* cc warning */` |
|         - |  8998 | `	/* Delimit the method signature */` |
|   2380315 |  8999 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2380315 |  9000 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9001 | `		/* Syntax error */` |
|         3 |  9002 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  9003 | `		if( rc == SXERR_ABORT ){` |
|         - |  9004 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9005 | `			return SXERR_ABORT;` |
|         - |  9006 | `		}` |
|         3 |  9007 | `		goto Synchronize;` |
|         - |  9008 | `	}` |
|         - |  9009 | `	{` |
|   2380313 |  9010 | `		int bIsCtor = 0;` |
|   2380313 |  9011 | `		int bAbstractCtor = 0;` |
|   2380308 |  9012 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1390444 |  9013 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2298250 |  9014 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    164131 |  9015 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  9016 | `				bAbstractCtor = 1;` |
|         2 |  9017 | `			}else{` |
|    164129 |  9018 | `				bIsCtor = 1;` |
|         - |  9019 | `			}` |
|     82063 |  9020 | `		}` |
|   2380313 |  9021 | `		if( pGen->pIn < pEnd ){` |
|         - |  9022 | `			/* Collect method arguments */` |
|    854571 |  9023 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    854571 |  9024 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9025 | `				return SXERR_ABORT;` |
|         - |  9026 | `			}` |
|    427283 |  9027 | `		}` |
|         - |  9028 | `	}` |
|         - |  9029 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2380313 |  9030 | `	pGen->pIn = &pEnd[1];` |
|         - |  9031 | `	{` |
|   2380313 |  9032 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2380313 |  9033 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  9034 | `			return SXERR_ABORT;` |
|   2380313 |  9035 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  9036 | `			goto Synchronize;` |
|         - |  9037 | `		}` |
|         - |  9038 | `	}` |
|         - |  9039 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  9040 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  9041 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  9042 | `	{` |
|   2380313 |  9043 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  9044 | `		sxu32 i;` |
|   3662001 |  9045 | `		for( i = 0; i < nArg; i++ ){` |
|   1281703 |  9046 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  9047 | `			ph7_class_attr *pAttr;` |
|   1281703 |  9048 | `			sxi32 iAttrFlags = 0;` |
|         - |  9049 | `			int bArgTyped;` |
|   1281703 |  9050 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1281619 |  9051 | `				continue;` |
|         - |  9052 | `			}` |
|         - |  9053 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  9054 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  9055 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  9056 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  9057 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  9058 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  9059 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9060 | `					"Cannot declare variadic promoted property");` |
|         3 |  9061 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9062 | `					return SXERR_ABORT;` |
|         - |  9063 | `				}` |
|         3 |  9064 | `				goto Synchronize;` |
|         - |  9065 | `			}` |
|         - |  9066 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  9067 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  9068 | `			 * appear as an alternative of a union type. */` |
|        87 |  9069 | `			if( bArgTyped ){` |
|       122 |  9070 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  9071 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  9072 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  9073 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  9074 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9075 | `					return SXERR_ABORT;` |
|        83 |  9076 | `				}else if( rc != SXRET_OK ){` |
|         6 |  9077 | `					goto Synchronize;` |
|         - |  9078 | `				}` |
|        37 |  9079 | `			}` |
|         - |  9080 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  9081 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  9082 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9083 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  9084 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9085 | `					return SXERR_ABORT;` |
|         - |  9086 | `				}` |
|         3 |  9087 | `				goto Synchronize;` |
|         - |  9088 | `			}` |
|        81 |  9089 | `			if( bArgTyped ){` |
|        77 |  9090 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  9091 | `			}` |
|        81 |  9092 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  9093 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  9094 | `			}` |
|        81 |  9095 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  9096 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  9097 | `			}` |
|        81 |  9098 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  9099 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  9100 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  9101 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  9102 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9103 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  9104 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9105 | `						return SXERR_ABORT;` |
|         - |  9106 | `					}` |
|         3 |  9107 | `					goto Synchronize;` |
|         - |  9108 | `				}` |
|        24 |  9109 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  9110 | `			}` |
|        79 |  9111 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  9112 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  9113 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  9114 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9115 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  9116 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  9117 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9118 | `						return SXERR_ABORT;` |
|         - |  9119 | `					}` |
|       ! 0 |  9120 | `					goto Synchronize;` |
|         - |  9121 | `				}` |
|         5 |  9122 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  9123 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  9124 | `			}` |
|        79 |  9125 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  9126 | `			if( pAttr == 0 ){` |
|       ! 0 |  9127 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9128 | `				return SXERR_ABORT;` |
|         - |  9129 | `			}` |
|        79 |  9130 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  9131 | `				pAttr->nType = pArg->nType;` |
|        77 |  9132 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  9133 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  9134 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9135 | `					sxu32 k;` |
|        20 |  9136 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9137 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9138 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9139 | `					}` |
|         3 |  9140 | `				}` |
|        36 |  9141 | `			}` |
|        79 |  9142 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  9143 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9144 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9145 | `				return SXERR_ABORT;` |
|         - |  9146 | `			}` |
|        42 |  9147 | `		}` |
|         - |  9148 | `	}` |
|   2380303 |  9149 | `	if( doBody ){` |
|         - |  9150 | `		/* Compile method body */` |
|   2243007 |  9151 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2243007 |  9152 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9153 | `			return SXERR_ABORT;` |
|         - |  9154 | `		}` |
|         - |  9155 | `		/* The cursor sits just past the body's closing brace */` |
|   2243007 |  9156 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1121506 |  9157 | `	}else{` |
|         - |  9158 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    137301 |  9159 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    137301 |  9160 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     68648 |  9161 | `		}` |
|         - |  9162 | `		/* Only method signature is allowed */` |
|    137301 |  9163 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9164 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9165 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9166 | `				if( rc == SXERR_ABORT ){` |
|         - |  9167 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9168 | `					return SXERR_ABORT;` |
|         - |  9169 | `				}` |
|       ! 0 |  9170 | `				return SXERR_CORRUPT;` |
|         - |  9171 | `			}` |
|         - |  9172 | `	}` |
|         - |  9173 | `	/* All done,install the method */` |
|   2380303 |  9174 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2380303 |  9175 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9176 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9177 | `		return SXERR_ABORT;` |
|         - |  9178 | `	}` |
|   2380303 |  9179 | `	return SXRET_OK;` |
|         6 |  9180 | `Synchronize:` |
|         - |  9181 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9182 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9183 | `		pGen->pIn++;` |
|         4 |  9184 | `	}` |
|        16 |  9185 | `	return SXERR_CORRUPT;` |
|   1190160 |  9186 | `}` |
|         - |  9187 | `/*` |
|         - |  9188 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9189 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9190 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9191 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9192 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9193 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9194 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9195 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9196 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9197 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9198 | `` * implicit `$value` formal.`` |
|         - |  9199 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9200 | ` */` |
|         - |  9201 | `/*` |
|         - |  9202 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9203 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9204 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9205 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9206 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9207 | ` */` |
|        94 |  9208 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9209 | `{` |
|         - |  9210 | `	SyToken *p;` |
|       345 |  9211 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9212 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9213 | `			continue;` |
|         - |  9214 | `		}` |
|         - |  9215 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9216 | `		if( p + 3 < pEnd` |
|        80 |  9217 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9218 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9219 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9220 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9221 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9222 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9223 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9224 | `			return 1;` |
|         - |  9225 | `		}` |
|         - |  9226 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9227 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9228 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9229 | `		if( p > pStart` |
|        26 |  9230 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9231 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9232 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9233 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9234 | `			return 1;` |
|         - |  9235 | `		}` |
|        15 |  9236 | `	}` |
|        43 |  9237 | `	return 0;` |
|        48 |  9238 | `}` |
|         - |  9239 | `/*` |
|         - |  9240 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9241 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9242 | ` */` |
|       990 |  9243 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9244 | `{` |
|      1167 |  9245 | `	return p + 6 < pEnd` |
|       671 |  9246 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9247 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9248 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9249 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9250 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9251 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9252 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9253 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9254 | `	 && p[5].sData.nByte == 3` |
|         8 |  9255 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9256 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9257 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9258 | `}` |
|         - |  9259 | `/*` |
|         - |  9260 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9261 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9262 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9263 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9264 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9265 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9266 | ` * or SXERR_MEM.` |
|         - |  9267 | ` */` |
|         4 |  9268 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9269 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9270 | `{` |
|         5 |  9271 | `	SyToken *p = pStart;` |
|        35 |  9272 | `	while( p < pEnd ){` |
|        31 |  9273 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9274 | `			SyToken sTok;` |
|         - |  9275 | `			char zName[384];` |
|         - |  9276 | `			sxu32 nName;` |
|         - |  9277 | `			char *zDup;` |
|         - |  9278 | ``			/* `parent` `::` */`` |
|         5 |  9279 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9280 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9281 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9282 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9283 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9284 | `			if( zDup == 0 ){` |
|       ! 0 |  9285 | `				return SXERR_MEM;` |
|         - |  9286 | `			}` |
|         5 |  9287 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9288 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9289 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9290 | `			sTok.pUserData = 0;` |
|         5 |  9291 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9292 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9293 | `			continue;` |
|         - |  9294 | `		}` |
|        27 |  9295 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9296 | `		p++;` |
|         1 |  9297 | `	}` |
|         5 |  9298 | `	return SXRET_OK;` |
|         3 |  9299 | `}` |
|        94 |  9300 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9301 | `{` |
|        95 |  9302 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9303 | `	sxi32 rc;` |
|        95 |  9304 | `	int bRefsSelf = 0;` |
|        95 |  9305 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9306 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9307 | `		char zHook[384];` |
|         - |  9308 | `		SyString sHookName;` |
|         - |  9309 | `		ph7_class_method *pMeth;` |
|         - |  9310 | `		int bGet;` |
|       159 |  9311 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9312 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9313 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9314 | `			continue;` |
|         - |  9315 | `		}` |
|       145 |  9316 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9317 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9318 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9319 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9320 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9321 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9322 | `				return SXERR_ABORT;` |
|         - |  9323 | `			}` |
|       ! 0 |  9324 | `			return SXERR_CORRUPT;` |
|         - |  9325 | `		}` |
|       145 |  9326 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9327 | `			goto HookSyntax;` |
|         - |  9328 | `		}` |
|       144 |  9329 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9330 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9331 | `			bGet = 1;` |
|       106 |  9332 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9333 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9334 | `			bGet = 0;` |
|        34 |  9335 | `		}else{` |
|       ! 0 |  9336 | `			goto HookSyntax;` |
|         - |  9337 | `		}` |
|       145 |  9338 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9339 | `		sHookName.zString = zHook;` |
|       217 |  9340 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9341 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9342 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9343 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9344 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9345 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9346 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9347 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9348 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9349 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9350 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9351 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9352 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9353 | `					return SXERR_ABORT;` |
|         - |  9354 | `				}` |
|       ! 0 |  9355 | `				return SXERR_CORRUPT;` |
|         - |  9356 | `			}` |
|        15 |  9357 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9358 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9359 | `			if( pMeth == 0 ){` |
|       ! 0 |  9360 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9361 | `				return SXERR_ABORT;` |
|         - |  9362 | `			}` |
|        15 |  9363 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9364 | `			if( !bGet ){` |
|         - |  9365 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9366 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9367 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9368 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9369 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9370 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9371 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9372 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9373 | `				if( zVName == 0 ){` |
|       ! 0 |  9374 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9375 | `					return SXERR_ABORT;` |
|         - |  9376 | `				}` |
|         7 |  9377 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9378 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9379 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9380 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9381 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9382 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9383 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9384 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9385 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9386 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9387 | `				}` |
|         7 |  9388 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9389 | `			}` |
|        15 |  9390 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9391 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9392 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9393 | `				return SXERR_ABORT;` |
|         - |  9394 | `			}` |
|        15 |  9395 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9396 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9397 | `		}` |
|       130 |  9398 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9399 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9400 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9401 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9402 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9403 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9404 | `				return SXERR_ABORT;` |
|         - |  9405 | `			}` |
|       ! 0 |  9406 | `			return SXERR_CORRUPT;` |
|         - |  9407 | `		}` |
|       131 |  9408 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9409 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9410 | `		if( pMeth == 0 ){` |
|       ! 0 |  9411 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9412 | `			return SXERR_ABORT;` |
|         - |  9413 | `		}` |
|       131 |  9414 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9415 | `		if( !bGet ){` |
|         - |  9416 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9417 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9418 | `				SyToken *pRp = 0;` |
|        17 |  9419 | `				pGen->pIn++;` |
|        17 |  9420 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9421 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9422 | `					goto HookSyntax;` |
|         - |  9423 | `				}` |
|        17 |  9424 | `				if( pGen->pIn < pRp ){` |
|        17 |  9425 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9426 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9427 | `						return SXERR_ABORT;` |
|         - |  9428 | `					}` |
|         8 |  9429 | `				}` |
|        17 |  9430 | `				pGen->pIn = &pRp[1];` |
|         8 |  9431 | `			}` |
|        61 |  9432 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9433 | `				/* Implicit $value formal */` |
|         - |  9434 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9435 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9436 | `				if( zVName == 0 ){` |
|       ! 0 |  9437 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9438 | `					return SXERR_ABORT;` |
|         - |  9439 | `				}` |
|        45 |  9440 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9441 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9442 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9443 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9444 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9445 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9446 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9447 | `			}` |
|        30 |  9448 | `		}` |
|       165 |  9449 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9450 | `			/* Block body */` |
|        69 |  9451 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9452 | `			SyToken *pCloser = 0;` |
|        69 |  9453 | `			int bParentCall = 0;` |
|        69 |  9454 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9455 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9456 | `				SyToken *pScan;` |
|       753 |  9457 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9458 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9459 | `						bParentCall = 1;` |
|         3 |  9460 | `						break;` |
|         - |  9461 | `					}` |
|       343 |  9462 | `				}` |
|        34 |  9463 | `			}` |
|        69 |  9464 | `			if( bParentCall ){` |
|         - |  9465 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9466 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9467 | `				 * hook method), then continue past the original body. */` |
|         - |  9468 | `				SySet sBody;` |
|         3 |  9469 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9470 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9471 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9472 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9473 | `					SySetRelease(&sBody);` |
|       ! 0 |  9474 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9475 | `					return SXERR_ABORT;` |
|         - |  9476 | `				}` |
|         3 |  9477 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9478 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9479 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9480 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9481 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9482 | `				SySetRelease(&sBody);` |
|         3 |  9483 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9484 | `					return SXERR_ABORT;` |
|         - |  9485 | `				}` |
|         3 |  9486 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9487 | `			}else{` |
|        67 |  9488 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9489 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9490 | `					return SXERR_ABORT;` |
|         - |  9491 | `				}` |
|        67 |  9492 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9493 | `			}` |
|        69 |  9494 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9495 | `				bRefsSelf = 1;` |
|         9 |  9496 | `			}` |
|       128 |  9497 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9498 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9499 | `			GenBlock *pBlock;` |
|         - |  9500 | `			SySet *pInstrContainer;` |
|         - |  9501 | `			SyToken *pBodyStart;` |
|         - |  9502 | `			SyToken *pExprEnd;` |
|        63 |  9503 | `			SyToken *pSavedEnd = 0;` |
|         - |  9504 | `			SySet sBody;` |
|        63 |  9505 | `			int bParentCall = 0;` |
|        63 |  9506 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9507 | `			pBodyStart = pGen->pIn;` |
|         - |  9508 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9509 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9510 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9511 | `			 * method on a token copy. */` |
|         - |  9512 | `			{` |
|        63 |  9513 | `				sxi32 iNest = 0;` |
|        63 |  9514 | `				pExprEnd = pBodyStart;` |
|       355 |  9515 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9516 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9517 | `						iNest++;` |
|       351 |  9518 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9519 | `						if( iNest <= 0 ){` |
|       ! 0 |  9520 | `							break;` |
|         - |  9521 | `						}` |
|         9 |  9522 | `						iNest--;` |
|       343 |  9523 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9524 | `						break;` |
|         - |  9525 | `					}` |
|       293 |  9526 | `					pExprEnd++;` |
|         1 |  9527 | `				}` |
|         - |  9528 | `			}` |
|         - |  9529 | `			{` |
|         - |  9530 | `				SyToken *pScan;` |
|       335 |  9531 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9532 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9533 | `						bParentCall = 1;` |
|         3 |  9534 | `						break;` |
|         - |  9535 | `					}` |
|       137 |  9536 | `				}` |
|         - |  9537 | `			}` |
|        63 |  9538 | `			if( bParentCall ){` |
|         3 |  9539 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9540 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9541 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9542 | `					SySetRelease(&sBody);` |
|       ! 0 |  9543 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9544 | `					return SXERR_ABORT;` |
|         - |  9545 | `				}` |
|         3 |  9546 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9547 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9548 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9549 | `			}` |
|        94 |  9550 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9551 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9552 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9553 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9554 | `				return SXERR_ABORT;` |
|         - |  9555 | `			}` |
|        63 |  9556 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9557 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9558 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9559 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9560 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9561 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9562 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9563 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9564 | `			if( bParentCall ){` |
|         3 |  9565 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9566 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9567 | `				SySetRelease(&sBody);` |
|         1 |  9568 | `			}` |
|        63 |  9569 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9570 | `				return SXERR_ABORT;` |
|         - |  9571 | `			}` |
|        63 |  9572 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9573 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9574 | `				bRefsSelf = 1;` |
|        18 |  9575 | `			}` |
|        63 |  9576 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9577 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9578 | `			}` |
|        63 |  9579 | `			if( !bGet ){` |
|         - |  9580 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9581 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9582 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9583 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9584 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9585 | `				bRefsSelf = 1;` |
|         1 |  9586 | `			}` |
|        32 |  9587 | `		}else{` |
|       ! 0 |  9588 | `			goto HookSyntax;` |
|         - |  9589 | `		}` |
|       131 |  9590 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9591 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9592 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9593 | `			return SXERR_ABORT;` |
|         - |  9594 | `		}` |
|       131 |  9595 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9596 | `	}` |
|        95 |  9597 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9598 | `		goto HookSyntax;` |
|         - |  9599 | `	}` |
|        95 |  9600 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9601 | `	if( !bRefsSelf ){` |
|         - |  9602 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9603 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9604 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9605 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9606 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9607 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9608 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9609 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9610 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9611 | `				return SXERR_ABORT;` |
|         - |  9612 | `			}` |
|       ! 0 |  9613 | `			return SXERR_CORRUPT;` |
|         - |  9614 | `		}` |
|        20 |  9615 | `	}` |
|        95 |  9616 | `	return SXRET_OK;` |
|       ! 0 |  9617 | `HookSyntax:` |
|       ! 0 |  9618 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9619 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9620 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9621 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9622 | `		return SXERR_ABORT;` |
|         - |  9623 | `	}` |
|       ! 0 |  9624 | `	return SXERR_CORRUPT;` |
|        48 |  9625 | `}` |
|         - |  9626 | `/*` |
|         - |  9627 | ` * Compile an object interface.` |
|         - |  9628 | ` *  According to the PHP language reference manual` |
|         - |  9629 | ` *   Object Interfaces:` |
|         - |  9630 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9631 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9632 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9633 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9634 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9635 | ` */` |
|     68724 |  9636 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9637 | `{` |
|     68729 |  9638 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9639 | `	ph7_class *pClass,*pBase;` |
|         - |  9640 | `	SyToken *pEnd,*pTmp;` |
|         - |  9641 | `	SyString *pName;` |
|         - |  9642 | `	sxi32 nKwrd;` |
|         - |  9643 | `	sxi32 rc;` |
|         - |  9644 | `	/* Jump the 'interface' keyword */` |
|     68729 |  9645 | `	pGen->pIn++;` |
|         - |  9646 | `	/* Extract interface name */` |
|     68729 |  9647 | `	pName = &pGen->pIn->sData;` |
|         - |  9648 | `	/* Advance the stream cursor */` |
|     68729 |  9649 | `	pGen->pIn++;` |
|         - |  9650 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9651 | `		SyBlob sFQN;` |
|         - |  9652 | `		SyString sFQNStr;` |
|     68729 |  9653 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     68729 |  9654 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     68729 |  9655 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     68729 |  9656 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     68729 |  9657 | `		SyBlobRelease(&sFQN);` |
|         - |  9658 | `	}` |
|     68729 |  9659 | `	if( pClass == 0 ){` |
|       ! 0 |  9660 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9661 | `		return SXERR_ABORT;` |
|         - |  9662 | `	}` |
|     68729 |  9663 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     68729 |  9664 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9665 | `		return SXERR_ABORT;` |
|         - |  9666 | `	}` |
|         - |  9667 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     68729 |  9668 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9669 | `	/* Assume no base class is given */` |
|     68729 |  9670 | `	pBase = 0;` |
|     68729 |  9671 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26699 |  9672 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26699 |  9673 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9674 | `			SyBlob sResolved;` |
|         - |  9675 | `			SyString sBaseName;` |
|         - |  9676 | `			sxu32 nRefLine;` |
|         - |  9677 | `			/* Extract base interface */` |
|     26699 |  9678 | `			pGen->pIn++;` |
|     26699 |  9679 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26699 |  9680 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26699 |  9681 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9682 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9683 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9684 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9685 | `					pName);` |
|       ! 0 |  9686 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9687 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9688 | `					return SXERR_ABORT;` |
|         - |  9689 | `				}` |
|       ! 0 |  9690 | `				return SXRET_OK;` |
|         - |  9691 | `			}` |
|     40046 |  9692 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     26694 |  9693 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26699 |  9694 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9695 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9696 | `			/* Only interfaces is allowed */` |
|     26699 |  9697 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9698 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9699 | `			}` |
|     26699 |  9700 | `			if( pBase == 0 ){` |
|       ! 0 |  9701 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9702 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9703 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9704 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9705 | `					return SXERR_ABORT;` |
|         - |  9706 | `				}` |
|       ! 0 |  9707 | `			}` |
|     26699 |  9708 | `			SyBlobRelease(&sResolved);` |
|     13347 |  9709 | `		}` |
|     13347 |  9710 | `	}` |
|     68729 |  9711 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9712 | `		/* Syntax error */` |
|       ! 0 |  9713 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9714 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9715 | `		if( rc == SXERR_ABORT ){` |
|         - |  9716 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9717 | `			return SXERR_ABORT;` |
|         - |  9718 | `		}` |
|       ! 0 |  9719 | `		return SXRET_OK;` |
|         - |  9720 | `	}` |
|     68729 |  9721 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     68729 |  9722 | `	pEnd = 0; /* cc warning */` |
|         - |  9723 | `	/* Delimit the interface body */` |
|     68729 |  9724 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     68729 |  9725 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9726 | `		/* Syntax error */` |
|       ! 0 |  9727 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9728 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9729 | `		if( rc == SXERR_ABORT ){` |
|         - |  9730 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9731 | `			return SXERR_ABORT;` |
|         - |  9732 | `		}` |
|       ! 0 |  9733 | `		return SXRET_OK;` |
|         - |  9734 | `	}` |
|         - |  9735 | `	/* The delimiter token is the interface body's closing brace */` |
|     68729 |  9736 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9737 | `	/* Swap token stream */` |
|     68729 |  9738 | `	pTmp = pGen->pEnd;` |
|     68729 |  9739 | `	pGen->pEnd = pEnd;` |
|         - |  9740 | `	/* Start the parse process` |
|         - |  9741 | `	 * Note (According to the PHP reference manual):` |
|         - |  9742 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9743 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9744 | `	 */` |
|    125879 |  9745 | `	for(;;){` |
|         - |  9746 | `		/* Jump leading/trailing semi-colons */` |
|    434801 |  9747 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    183039 |  9748 | `			pGen->pIn++;` |
|         5 |  9749 | `		}` |
|    251767 |  9750 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9751 | `			/* End of interface body */` |
|     68725 |  9752 | `			break;` |
|         - |  9753 | `		}` |
|         - |  9754 | `		/* Bind a directly-preceding docblock to this member */` |
|    183047 |  9755 | `		GenStateSetPendingDoc(&(*pGen));` |
|    183047 |  9756 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9757 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9758 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9759 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9760 | `			if( rc == SXERR_ABORT ){` |
|         - |  9761 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9762 | `				return SXERR_ABORT;` |
|         - |  9763 | `			}` |
|       ! 0 |  9764 | `			goto done;` |
|         - |  9765 | `		}` |
|         - |  9766 | `		/* Extract the current keyword */` |
|    183047 |  9767 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    183047 |  9768 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9769 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9770 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9771 | `			const char *zKind = "member";` |
|         3 |  9772 | `			SyString *pMemberName = 0;` |
|         3 |  9773 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9774 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9775 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9776 | `					zKind = "constant";` |
|         3 |  9777 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9778 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9779 | `					}` |
|         1 |  9780 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9781 | `					zKind = "method";` |
|       ! 0 |  9782 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9783 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9784 | `					}` |
|       ! 0 |  9785 | `				}` |
|         1 |  9786 | `			}` |
|         3 |  9787 | `			if( pMemberName ){` |
|         4 |  9788 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9789 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9790 | `			}else{` |
|       ! 0 |  9791 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9792 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9793 | `			}` |
|         3 |  9794 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9795 | `				return SXERR_ABORT;` |
|         - |  9796 | `			}` |
|         3 |  9797 | `			goto done;` |
|         - |  9798 | `		}` |
|    183045 |  9799 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9800 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9801 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9802 | `			if( rc == SXERR_ABORT ){` |
|         - |  9803 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9804 | `				return SXERR_ABORT;` |
|         - |  9805 | `			}` |
|       ! 0 |  9806 | `			goto done;` |
|         - |  9807 | `		}` |
|    183045 |  9808 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9809 | `			/* Advance the stream cursor */` |
|    129659 |  9810 | `			pGen->pIn++;` |
|    129654 |  9811 | `			if( pGen->pIn < pGen->pEnd` |
|    129659 |  9812 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    129654 |  9813 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9814 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9815 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9816 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9817 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9818 | `				 * hooked properties" error). */` |
|       ! 0 |  9819 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9820 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9821 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9822 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9823 | `						return SXERR_ABORT;` |
|         - |  9824 | `					}` |
|       ! 0 |  9825 | `					goto done;` |
|         - |  9826 | `				}` |
|       ! 0 |  9827 | `				continue;` |
|         - |  9828 | `			}` |
|    129659 |  9829 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9830 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9831 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9832 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9833 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9834 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9835 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9836 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9837 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9838 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9839 | `							return SXERR_ABORT;` |
|         - |  9840 | `						}` |
|       ! 0 |  9841 | `						goto done;` |
|         - |  9842 | `					}` |
|       ! 0 |  9843 | `					continue;` |
|         - |  9844 | `				}` |
|       ! 0 |  9845 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9846 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9847 | `				if( rc == SXERR_ABORT ){` |
|         - |  9848 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9849 | `					return SXERR_ABORT;` |
|         - |  9850 | `				}` |
|       ! 0 |  9851 | `				goto done;` |
|         - |  9852 | `			}` |
|    129659 |  9853 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    129659 |  9854 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9855 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9856 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9857 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9858 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9859 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9860 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9861 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9862 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9863 | `							return SXERR_ABORT;` |
|         - |  9864 | `						}` |
|       ! 0 |  9865 | `						goto done;` |
|         - |  9866 | `					}` |
|         5 |  9867 | `					continue;` |
|         - |  9868 | `				}` |
|       ! 0 |  9869 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9870 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9871 | `				if( rc == SXERR_ABORT ){` |
|         - |  9872 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9873 | `					return SXERR_ABORT;` |
|         - |  9874 | `				}` |
|       ! 0 |  9875 | `				goto done;` |
|         - |  9876 | `			}` |
|     64825 |  9877 | `		}` |
|    183041 |  9878 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9879 | `			/* Parse constant */` |
|     53387 |  9880 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53387 |  9881 | `			if( rc != SXRET_OK ){` |
|         3 |  9882 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9883 | `					return SXERR_ABORT;` |
|         - |  9884 | `				}` |
|         3 |  9885 | `				goto done;` |
|         - |  9886 | `			}` |
|     26695 |  9887 | `		}else{` |
|    129659 |  9888 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    129659 |  9889 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9890 | `				/* Static method,record that */` |
|     11441 |  9891 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9892 | `				/* Advance the stream cursor */` |
|     11441 |  9893 | `				pGen->pIn++;` |
|     11436 |  9894 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11441 |  9895 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9896 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9897 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9898 | `						if( rc == SXERR_ABORT ){` |
|         - |  9899 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9900 | `							return SXERR_ABORT;` |
|         - |  9901 | `						}` |
|       ! 0 |  9902 | `						goto done;` |
|         - |  9903 | `				}` |
|      5718 |  9904 | `			}` |
|         - |  9905 | `			/* Process method signature (no body for interface methods) */` |
|    129659 |  9906 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    129659 |  9907 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9908 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9909 | `					return SXERR_ABORT;` |
|         - |  9910 | `				}` |
|       ! 0 |  9911 | `				goto done;` |
|         - |  9912 | `			}` |
|         - |  9913 | `		}` |
|         5 |  9914 | `	}` |
|         - |  9915 | `	/* Install the interface */` |
|     68725 |  9916 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     68725 |  9917 | `	if( rc == SXRET_OK && pBase ){` |
|         - |  9918 | `		/* Inherit from the base interface */` |
|     26699 |  9919 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13347 |  9920 | `	}` |
|     68725 |  9921 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9922 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9923 | `		return SXERR_ABORT;` |
|         - |  9924 | `	}` |
|     34360 |  9925 | `done:` |
|         - |  9926 | `	/* Point beyond the interface body */` |
|     68729 |  9927 | `	pGen->pIn  = &pEnd[1];` |
|     68729 |  9928 | `	pGen->pEnd = pTmp;` |
|     68729 |  9929 | `	return PH7_OK;` |
|     34367 |  9930 | `}` |
|         - |  9931 | `/*` |
|         - |  9932 | ` * Compile a user-defined class.` |
|         - |  9933 | ` * According to the PHP language reference manual` |
|         - |  9934 | ` *  class` |
|         - |  9935 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - |  9936 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - |  9937 | ` *  of the properties and methods belonging to the class.` |
|         - |  9938 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - |  9939 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - |  9940 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - |  9941 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  9942 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - |  9943 | ` *  (called "methods").` |
|         - |  9944 | ` */` |
|         - |  9945 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - |  9946 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - |  9947 | `struct TraitUseEntry {` |
|         - |  9948 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - |  9949 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - |  9950 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - |  9951 | `};` |
|         - |  9952 | `/*` |
|         - |  9953 | ` * Validate that methods implementing interface contracts have compatible` |
|         - |  9954 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - |  9955 | ` */` |
|    352446 |  9956 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9957 | `{` |
|         - |  9958 | `	ph7_class **apIface;` |
|         - |  9959 | `	sxu32 nIface,i;` |
|         - |  9960 | `	sxi32 rc;` |
|    352451 |  9961 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 |  9962 | `		return SXRET_OK;` |
|         - |  9963 | `	}` |
|    352451 |  9964 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    352451 |  9965 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    707303 |  9966 | `	for(i = 0; i < nIface; i++){` |
|    354857 |  9967 | `		ph7_class *pIface = apIface[i];` |
|         - |  9968 | `		SyHashEntry *pEntry;` |
|    354857 |  9969 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1022539 |  9970 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    667687 |  9971 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - |  9972 | `			ph7_class_method *pImplMeth;` |
|    667687 |  9973 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - |  9974 | `			/* Find the implementing method in the class */` |
|    667687 |  9975 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    667687 |  9976 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 |  9977 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - |  9978 | `			}` |
|         - |  9979 | `			/* Check visibility: interface methods must be implemented as public */` |
|    667669 |  9980 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 |  9981 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9982 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 |  9983 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 |  9984 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9985 | `					return SXERR_ABORT;` |
|         - |  9986 | `				}` |
|         1 |  9987 | `			}` |
|         - |  9988 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - |  9989 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - |  9990 | `			 */` |
|         - |  9991 | `			{` |
|    667669 |  9992 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    667669 |  9993 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    667669 |  9994 | `				int sigError = 0;` |
|    667669 |  9995 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 |  9996 | `					sigError = 1;` |
|    667668 |  9997 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - |  9998 | `					/* Extra parameters must all have default values */` |
|      3821 |  9999 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 10000 | `					sxu32 k;` |
|      7635 | 10001 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3821 | 10002 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 10003 | `							sigError = 1;` |
|         3 | 10004 | `							break;` |
|         - | 10005 | `						}` |
|      1912 | 10006 | `					}` |
|      1908 | 10007 | `				}` |
|    667669 | 10008 | `				if( sigError ){` |
|         - | 10009 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 10010 | `					ph7_vm_func_arg *aArgs;` |
|         - | 10011 | `					sxu32 j;` |
|         6 | 10012 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 10013 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 10014 | `					/* Build implementing method signature */` |
|         6 | 10015 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 10016 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 10017 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 10018 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 10019 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10020 | `					}` |
|         - | 10021 | `					/* Build interface method signature */` |
|         6 | 10022 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 10023 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 10024 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 10025 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 10026 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10027 | `					}` |
|         8 | 10028 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10029 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 10030 | `						&pClass->sName,pMName,` |
|         4 | 10031 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 10032 | `						&pIface->sName,pMName,` |
|         4 | 10033 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 10034 | `					SyBlobRelease(&sImplSig);` |
|         6 | 10035 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 10036 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10037 | `						return SXERR_ABORT;` |
|         - | 10038 | `					}` |
|         2 | 10039 | `				}` |
|         - | 10040 | `			}` |
|         5 | 10041 | `		}` |
|    177431 | 10042 | `	}` |
|    352451 | 10043 | `	return SXRET_OK;` |
|    176228 | 10044 | `}` |
|         - | 10045 | `/*` |
|         - | 10046 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 10047 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 10048 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 10049 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 10050 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 10051 | ` * means that specific hook is still missing.` |
|         - | 10052 | ` */` |
|        38 | 10053 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 10054 | `{` |
|         - | 10055 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 10056 | `	ph7_class_attr *pProp;` |
|        38 | 10057 | `	if( pMName->nByte <= nPfx` |
|        27 | 10058 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 10059 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 10060 | `		return 0; /* not a hook stub */` |
|         - | 10061 | `	}` |
|         7 | 10062 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 10063 | `	return pProp != 0` |
|         6 | 10064 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 10065 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 10066 | `}` |
|         - | 10067 | `/*` |
|         - | 10068 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 10069 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 10070 | ` */` |
|        16 | 10071 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 10072 | `{` |
|         - | 10073 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 10074 | `	if( pMName->nByte > nPfx` |
|        12 | 10075 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 10076 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 10077 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 10078 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 10079 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 10080 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 10081 | `		return;` |
|         - | 10082 | `	}` |
|        20 | 10083 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 10084 | `}` |
|         - | 10085 | `/*` |
|         - | 10086 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 10087 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 10088 | ` */` |
|    352446 | 10089 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10090 | `{` |
|         - | 10091 | `	ph7_class_method *pMeth;` |
|         - | 10092 | `	SyHashEntry *pEntry;` |
|         - | 10093 | `	sxu32 nAbstract;` |
|         - | 10094 | `	SyBlob sMsg;` |
|         - | 10095 | `	sxi32 rc;` |
|         - | 10096 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    352451 | 10097 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15299 | 10098 | `		return SXRET_OK;` |
|         - | 10099 | `	}` |
|         - | 10100 | `	/* Count abstract methods */` |
|    337157 | 10101 | `	nAbstract = 0;` |
|    337157 | 10102 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   4975899 | 10103 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4470171 | 10104 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4470171 | 10105 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 10106 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 10107 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10108 | `			}` |
|        20 | 10109 | `			nAbstract++;` |
|         8 | 10110 | `		}` |
|         5 | 10111 | `	}` |
|    337157 | 10112 | `	if( nAbstract == 0 ){` |
|    337143 | 10113 | `		return SXRET_OK;` |
|         - | 10114 | `	}` |
|         - | 10115 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 10116 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 10117 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 10118 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 10119 | `		&pClass->sName,nAbstract,` |
|         7 | 10120 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 10121 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 10122 | `	/* Second pass: list methods with origins */` |
|         - | 10123 | `	{` |
|        18 | 10124 | `		sxu32 nListed = 0;` |
|        18 | 10125 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 10126 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 10127 | `			ph7_class *pOrigin = 0;` |
|         - | 10128 | `			SyString *pMName;` |
|        22 | 10129 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 10130 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10131 | `				continue;` |
|         - | 10132 | `			}` |
|        20 | 10133 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10134 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10135 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10136 | `			}` |
|        20 | 10137 | `			if( nListed > 0 ){` |
|         3 | 10138 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10139 | `			}` |
|         - | 10140 | `			/* Find the origin of this abstract method.` |
|         - | 10141 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10142 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10143 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10144 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10145 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10146 | `			 * class's namespace.` |
|         - | 10147 | `			 */` |
|         - | 10148 | `			{` |
|         - | 10149 | `				ph7_class **apIface;` |
|         - | 10150 | `				ph7_class **apTrait;` |
|         - | 10151 | `				ph7_class *pWalk;` |
|         - | 10152 | `				sxu32 i;` |
|         - | 10153 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10154 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10155 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10156 | `				 */` |
|        20 | 10157 | `				if( pClass->pBase ){` |
|        11 | 10158 | `					pWalk = pClass->pBase;` |
|        19 | 10159 | `					while( pWalk ){` |
|         - | 10160 | `						ph7_class_method *pParentMeth;` |
|        13 | 10161 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10162 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10163 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10164 | `							 * in this class's ancestor chain.` |
|         - | 10165 | `							 */` |
|        13 | 10166 | `							int fromIface = 0;` |
|        13 | 10167 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10168 | `							while( pAnc ){` |
|         - | 10169 | `								ph7_class **apPI;` |
|         - | 10170 | `								sxu32 j;` |
|        15 | 10171 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10172 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10173 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10174 | `										fromIface = 1;` |
|        10 | 10175 | `										break;` |
|         - | 10176 | `									}` |
|       ! 0 | 10177 | `								}` |
|        15 | 10178 | `								if( fromIface ) break;` |
|         6 | 10179 | `								pAnc = pAnc->pBase;` |
|         2 | 10180 | `							}` |
|        13 | 10181 | `							if( !fromIface ){` |
|         3 | 10182 | `								pOrigin = pWalk;` |
|         3 | 10183 | `								break;` |
|         - | 10184 | `							}` |
|         4 | 10185 | `						}` |
|        10 | 10186 | `						pWalk = pWalk->pBase;` |
|         2 | 10187 | `					}` |
|         4 | 10188 | `				}` |
|         - | 10189 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10190 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10191 | `				 */` |
|        20 | 10192 | `				if( !pOrigin ){` |
|        18 | 10193 | `					pWalk = pClass;` |
|        40 | 10194 | `					while( pWalk && !pOrigin ){` |
|        26 | 10195 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10196 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10197 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10198 | `							ph7_class *pDeepest = 0;` |
|        28 | 10199 | `							while( pIface ){` |
|        16 | 10200 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10201 | `									pDeepest = pIface;` |
|         6 | 10202 | `								}` |
|        16 | 10203 | `								pIface = pIface->pBase;` |
|         4 | 10204 | `							}` |
|        16 | 10205 | `							if( pDeepest ){` |
|        16 | 10206 | `								pOrigin = pDeepest;` |
|        16 | 10207 | `								break;` |
|         - | 10208 | `							}` |
|       ! 0 | 10209 | `						}` |
|        26 | 10210 | `						pWalk = pWalk->pBase;` |
|         4 | 10211 | `					}` |
|         7 | 10212 | `				}` |
|         - | 10213 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10214 | `				if( !pOrigin ){` |
|         3 | 10215 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10216 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10217 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10218 | `							pOrigin = pClass;` |
|         3 | 10219 | `							break;` |
|         - | 10220 | `						}` |
|       ! 0 | 10221 | `					}` |
|         1 | 10222 | `				}` |
|         - | 10223 | `			}` |
|        20 | 10224 | `			if( pOrigin ){` |
|        20 | 10225 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10226 | `			}else{` |
|         - | 10227 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10228 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10229 | `			}` |
|        20 | 10230 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10231 | `			nListed++;` |
|         4 | 10232 | `		}` |
|         - | 10233 | `	}` |
|        18 | 10234 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10235 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10236 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10237 | `	SyBlobRelease(&sMsg);` |
|        18 | 10238 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10239 | `		return SXERR_ABORT;` |
|         - | 10240 | `	}` |
|        18 | 10241 | `	return SXRET_OK;` |
|    176228 | 10242 | `}` |
|         - | 10243 | `/*` |
|         - | 10244 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10245 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10246 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10247 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10248 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10249 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10250 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10251 | ` */` |
|    398516 | 10252 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10253 | `{` |
|    398521 | 10254 | `	int isAbsolute = 0;` |
|    398521 | 10255 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10256 | `	SyBlob sName;` |
|    398521 | 10257 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4411 | 10258 | `		isAbsolute = 1;` |
|      4411 | 10259 | `		pGen->pIn++;` |
|      2203 | 10260 | `	}` |
|    398521 | 10261 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         9 | 10262 | `		pGen->pIn = pStart;` |
|         9 | 10263 | `		return SXERR_INVALID;` |
|         - | 10264 | `	}` |
|    398515 | 10265 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    398515 | 10266 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    398515 | 10267 | `	pGen->pIn++;` |
|    597786 | 10268 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    199281 | 10269 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10270 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10271 | `		pGen->pIn++;` |
|        16 | 10272 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10273 | `		pGen->pIn++;` |
|         2 | 10274 | `	}` |
|    398515 | 10275 | `	if( isAbsolute ){` |
|      4409 | 10276 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2207 | 10277 | `	}else{` |
|         - | 10278 | `		SyString sRaw;` |
|    394111 | 10279 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    394111 | 10280 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10281 | `	}` |
|    398515 | 10282 | `	SyBlobRelease(&sName);` |
|    398515 | 10283 | `	return SXRET_OK;` |
|    199263 | 10284 | `}` |
|         - | 10285 | `/*` |
|         - | 10286 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10287 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10288 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10289 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10290 | ` * either direction cannot run unbounded.` |
|         - | 10291 | ` */` |
|         - | 10292 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    164118 | 10293 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10294 | `{` |
|         - | 10295 | `	ph7_class **apParent;` |
|         - | 10296 | `	sxu32 n;` |
|    427391 | 10297 | `	while( pInterface ){` |
|    270907 | 10298 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10299 | `			return FALSE;` |
|         - | 10300 | `		}` |
|    305234 | 10301 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     68654 | 10302 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7639 | 10303 | `			return TRUE;` |
|         - | 10304 | `		}` |
|    263273 | 10305 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    263273 | 10306 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10307 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10308 | `				return TRUE;` |
|         - | 10309 | `			}` |
|       ! 0 | 10310 | `		}` |
|    263273 | 10311 | `		pInterface = pInterface->pBase;` |
|    263273 | 10312 | `		iDepth++;` |
|         5 | 10313 | `	}` |
|    156489 | 10314 | `	return FALSE;` |
|     82064 | 10315 | `}` |
|    164118 | 10316 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10317 | `{` |
|    164123 | 10318 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10319 | `}` |
|         - | 10320 | `/*` |
|         - | 10321 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10322 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10323 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10324 | ` */` |
|      7634 | 10325 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10326 | `{` |
|      7643 | 10327 | `	while( pBase ){` |
|        10 | 10328 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10329 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10330 | `			return TRUE;` |
|         - | 10331 | `		}` |
|        10 | 10332 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10333 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10334 | `			return TRUE;` |
|         - | 10335 | `		}` |
|         5 | 10336 | `		pBase = pBase->pBase;` |
|         1 | 10337 | `	}` |
|      7635 | 10338 | `	return FALSE;` |
|      3822 | 10339 | `}` |
|         - | 10340 | `/*` |
|         - | 10341 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10342 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10343 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10344 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10345 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10346 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10347 | ` * pClass->aEnumCases for cases().` |
|         - | 10348 | ` */` |
|      7666 | 10349 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10350 | `{` |
|      7671 | 10351 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10352 | `	SySet *pInstrContainer;` |
|         - | 10353 | `	ph7_class_attr *pCase;` |
|         - | 10354 | `	SyString *pName;` |
|         - | 10355 | `	sxi32 rc;` |
|      7671 | 10356 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7671 | 10357 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10358 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10359 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10360 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10361 | `			return SXERR_ABORT;` |
|         - | 10362 | `		}` |
|       ! 0 | 10363 | `		goto Synchronize;` |
|         - | 10364 | `	}` |
|      7671 | 10365 | `	pName = &pGen->pIn->sData;` |
|         - | 10366 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7671 | 10367 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10368 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10369 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10370 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10371 | `			return SXERR_ABORT;` |
|         - | 10372 | `		}` |
|       ! 0 | 10373 | `		goto Synchronize;` |
|         - | 10374 | `	}` |
|      7671 | 10375 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10376 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7671 | 10377 | `	if( pCase == 0 ){` |
|       ! 0 | 10378 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10379 | `		return SXERR_ABORT;` |
|         - | 10380 | `	}` |
|      7671 | 10381 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7671 | 10382 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10383 | `		return SXERR_ABORT;` |
|         - | 10384 | `	}` |
|      7671 | 10385 | `	pGen->pIn++; /* Jump the case name */` |
|      7671 | 10386 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7657 | 10387 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10388 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10389 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10390 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10391 | `				return SXERR_ABORT;` |
|         - | 10392 | `			}` |
|         6 | 10393 | `			goto Synchronize;` |
|         - | 10394 | `		}` |
|      7653 | 10395 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10396 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10397 | `		 * (same technique as class constants). */` |
|      7653 | 10398 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7653 | 10399 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7653 | 10400 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7653 | 10401 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10402 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10403 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10404 | `		}` |
|      7653 | 10405 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7653 | 10406 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7653 | 10407 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10408 | `			return SXERR_ABORT;` |
|         - | 10409 | `		}` |
|      3829 | 10410 | `	}else{` |
|        17 | 10411 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10412 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10413 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10414 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10415 | `				return SXERR_ABORT;` |
|         - | 10416 | `			}` |
|       ! 0 | 10417 | `			goto Synchronize;` |
|         - | 10418 | `		}` |
|         - | 10419 | `	}` |
|      7667 | 10420 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7667 | 10421 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10422 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10423 | `		return SXERR_ABORT;` |
|         - | 10424 | `	}` |
|      7667 | 10425 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7667 | 10426 | `	return SXRET_OK;` |
|         2 | 10427 | `Synchronize:` |
|         - | 10428 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10429 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10430 | `		pGen->pIn++;` |
|         2 | 10431 | `	}` |
|         6 | 10432 | `	return SXERR_CORRUPT;` |
|      3838 | 10433 | `}` |
|         - | 10434 | `/*` |
|         - | 10435 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10436 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10437 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10438 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10439 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10440 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10441 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10442 | ` */` |
|      3836 | 10443 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10444 | `{` |
|         - | 10445 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10446 | `	const char *zBack;` |
|         - | 10447 | `	SySet sToken;` |
|         - | 10448 | `	char *zSrc;` |
|         - | 10449 | `	sxu32 nSrc,nMax;` |
|      3841 | 10450 | `	sxi32 rc = SXRET_OK;` |
|      3841 | 10451 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3836 | 10452 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3841 | 10453 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3841 | 10454 | `	if( zSrc == 0 ){` |
|       ! 0 | 10455 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10456 | `		return SXERR_ABORT;` |
|         - | 10457 | `	}` |
|      3841 | 10458 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3841 | 10459 | `	if( pClass->nEnumBacking != 0 ){` |
|      5741 | 10460 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10461 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10462 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10463 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1912 | 10464 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1917 | 10465 | `	}else{` |
|        21 | 10466 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10467 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10468 | `	}` |
|      3841 | 10469 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3841 | 10470 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3841 | 10471 | `	pSaveIn = pGen->pIn;` |
|      3841 | 10472 | `	pSaveEnd = pGen->pEnd;` |
|      3841 | 10473 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3841 | 10474 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15325 | 10475 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11489 | 10476 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10477 | `	}` |
|      3841 | 10478 | `	pGen->pIn = pSaveIn;` |
|      3841 | 10479 | `	pGen->pEnd = pSaveEnd;` |
|      3841 | 10480 | `	SySetRelease(&sToken);` |
|      3841 | 10481 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1923 | 10482 | `}` |
|         - | 10483 | `/*` |
|         - | 10484 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10485 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10486 | ` */` |
|         - | 10487 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10488 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10489 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10490 | `};` |
|         - | 10491 | `/*` |
|         - | 10492 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10493 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10494 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10495 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10496 | ` * and before the class is installed.` |
|         - | 10497 | ` */` |
|      3836 | 10498 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10499 | `{` |
|         - | 10500 | `	SyHashEntry *pEntry;` |
|         - | 10501 | `	sxi32 rc;` |
|         - | 10502 | `	sxu32 n;` |
|         - | 10503 | `	/* php: "Enum %s cannot include properties" */` |
|      3841 | 10504 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11507 | 10505 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7673 | 10506 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7673 | 10507 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10508 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10509 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10510 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10511 | `				return SXERR_ABORT;` |
|         - | 10512 | `			}` |
|         3 | 10513 | `			break;` |
|         - | 10514 | `		}` |
|         5 | 10515 | `	}` |
|         - | 10516 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     53709 | 10517 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     74802 | 10518 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     49873 | 10519 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10520 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10521 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10522 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10523 | `				return SXERR_ABORT;` |
|         - | 10524 | `			}` |
|       ! 0 | 10525 | `		}` |
|     24939 | 10526 | `	}` |
|         - | 10527 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10528 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10529 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10530 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10531 | `	{` |
|         - | 10532 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10533 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10534 | `		ph7_class_attr *pAttr;` |
|      3841 | 10535 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10536 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3841 | 10537 | `		if( pAttr == 0 ){` |
|       ! 0 | 10538 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10539 | `			return SXERR_ABORT;` |
|         - | 10540 | `		}` |
|      3841 | 10541 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3841 | 10542 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3841 | 10543 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3841 | 10544 | `		if( pClass->nEnumBacking != 0 ){` |
|      3829 | 10545 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10546 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3829 | 10547 | `			if( pAttr == 0 ){` |
|       ! 0 | 10548 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10549 | `				return SXERR_ABORT;` |
|         - | 10550 | `			}` |
|      3829 | 10551 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3829 | 10552 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10553 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10554 | `			}else{` |
|      3823 | 10555 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10556 | `			}` |
|      3829 | 10557 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1912 | 10558 | `		}` |
|         - | 10559 | `	}` |
|      3841 | 10560 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1923 | 10561 | `}` |
|         - | 10562 | `/*` |
|         - | 10563 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10564 | ` *` |
|         - | 10565 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10566 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10567 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10568 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10569 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10570 | ` * implements, body, install) is shared by both paths.` |
|         - | 10571 | ` */` |
|    352490 | 10572 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10573 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10574 | `{` |
|    352495 | 10575 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10576 | `	ph7_class *pClass,*pBase;` |
|         - | 10577 | `	SyToken *pEnd,*pTmp;` |
|         - | 10578 | `	sxi32 iProtection;` |
|         - | 10579 | `	SySet aInterfaces;` |
|         - | 10580 | `	SySet aUseEntries;` |
|         - | 10581 | `	sxi32 iAttrflags;` |
|         - | 10582 | `	SyString *pName;` |
|         - | 10583 | `	sxi32 nKwrd;` |
|         - | 10584 | `	sxi32 rc;` |
|         - | 10585 | `	/* Jump the 'class' keyword */` |
|    352495 | 10586 | `	pGen->pIn++;` |
|    352495 | 10587 | `	if( pAnonName ){` |
|         - | 10588 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10589 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10590 | `		 * then use the synthesized name. */` |
|        32 | 10591 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10592 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10593 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10594 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10595 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10596 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10597 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10598 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10599 | `		}` |
|        32 | 10600 | `		pName = pAnonName;` |
|        32 | 10601 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10602 | `	}else{` |
|    352467 | 10603 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10604 | `			/* Syntax error */` |
|       ! 0 | 10605 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10606 | `			if( rc == SXERR_ABORT ){` |
|         - | 10607 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10608 | `				return SXERR_ABORT;` |
|         - | 10609 | `			}` |
|         - | 10610 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10611 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10612 | `				pGen->pIn++;` |
|       ! 0 | 10613 | `			}` |
|       ! 0 | 10614 | `			return SXRET_OK;` |
|         - | 10615 | `		}` |
|         - | 10616 | `		/* Extract class name */` |
|    352467 | 10617 | `		pName = &pGen->pIn->sData;` |
|         - | 10618 | `		/* Advance the stream cursor */` |
|    352467 | 10619 | `		pGen->pIn++;` |
|         - | 10620 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10621 | `			SyBlob sFQN;` |
|         - | 10622 | `			SyString sFQNStr;` |
|    352467 | 10623 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    352467 | 10624 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    352467 | 10625 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    352467 | 10626 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    352467 | 10627 | `			SyBlobRelease(&sFQN);` |
|         - | 10628 | `		}` |
|         - | 10629 | `	}` |
|    352495 | 10630 | `	if( pClass == 0 ){` |
|       ! 0 | 10631 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10632 | `		return SXERR_ABORT;` |
|         - | 10633 | `	}` |
|    352490 | 10634 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3845 | 10635 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10636 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3831 | 10637 | `		pGen->pIn++; /* Jump ':' */` |
|      3826 | 10638 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3831 | 10639 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10640 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10641 | `			pGen->pIn++;` |
|      3824 | 10642 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3825 | 10643 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3823 | 10644 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3823 | 10645 | `			pGen->pIn++;` |
|      1914 | 10646 | `		}else{` |
|         3 | 10647 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10648 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10649 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10650 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10651 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10652 | `				return SXERR_ABORT;` |
|         - | 10653 | `			}` |
|         3 | 10654 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10655 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10656 | `			}` |
|         - | 10657 | `		}` |
|      1913 | 10658 | `	}` |
|    352495 | 10659 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    352495 | 10660 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10661 | `		return SXERR_ABORT;` |
|         - | 10662 | `	}` |
|         - | 10663 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    352495 | 10664 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    352495 | 10665 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10666 | `	/* Assume a standalone class */` |
|    352495 | 10667 | `	pBase = 0;` |
|    352495 | 10668 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    286341 | 10669 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    286341 | 10670 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10671 | `			SyBlob sResolved;` |
|         - | 10672 | `			SyString sBaseName;` |
|         - | 10673 | `			sxu32 nRefLine;` |
|    183239 | 10674 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10675 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10676 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10677 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10678 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10679 | `					return SXERR_ABORT;` |
|         - | 10680 | `				}` |
|       ! 0 | 10681 | `			}` |
|    183239 | 10682 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    183239 | 10683 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    183239 | 10684 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    183239 | 10685 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10686 | `				SyBlobRelease(&sResolved);` |
|         4 | 10687 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10688 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10689 | `					pName);` |
|         3 | 10690 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10691 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10692 | `					return SXERR_ABORT;` |
|         - | 10693 | `				}` |
|         3 | 10694 | `				return SXRET_OK;` |
|         - | 10695 | `			}` |
|    274853 | 10696 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    183232 | 10697 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    183237 | 10698 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10699 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10700 | `			/* Interfaces are not allowed */` |
|    183237 | 10701 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10702 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10703 | `			}` |
|    183237 | 10704 | `			if( pBase == 0 ){` |
|       ! 0 | 10705 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10706 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10707 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10708 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10709 | `					return SXERR_ABORT;` |
|         - | 10710 | `				}` |
|       ! 0 | 10711 | `			}else{` |
|    183237 | 10712 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10713 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10714 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10715 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10716 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10717 | `						return SXERR_ABORT;` |
|         - | 10718 | `					}` |
|         3 | 10719 | `					pBase = 0; /* Never inherit from an enum */` |
|    183236 | 10720 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10721 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10722 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10723 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10724 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10725 | `						return SXERR_ABORT;` |
|         - | 10726 | `					}` |
|       ! 0 | 10727 | `				}` |
|         - | 10728 | `			}` |
|    183237 | 10729 | `			SyBlobRelease(&sResolved);` |
|    183237 | 10730 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10731 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10732 | `			}` |
|     91616 | 10733 | `		}` |
|    286339 | 10734 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10735 | `			ph7_class *pInterface;` |
|         - | 10736 | `			/* Interface implementation */` |
|    106931 | 10737 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    110655 | 10738 | `			for(;;){` |
|         - | 10739 | `				SyBlob sResolved;` |
|         - | 10740 | `				SyString sIntName;` |
|         - | 10741 | `				sxu32 nRefLine;` |
|    164123 | 10742 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    164123 | 10743 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    164123 | 10744 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10745 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10746 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10747 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10748 | `						pName);` |
|       ! 0 | 10749 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10750 | `						return SXERR_ABORT;` |
|         - | 10751 | `					}` |
|       ! 0 | 10752 | `					break;` |
|         - | 10753 | `				}` |
|    328241 | 10754 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    164118 | 10755 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    164123 | 10756 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10757 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10758 | `				/* Only interfaces are allowed */` |
|    164123 | 10759 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10760 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10761 | `				}` |
|    164123 | 10762 | `				if( pInterface == 0 ){` |
|       ! 0 | 10763 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10764 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10765 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10766 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10767 | `						return SXERR_ABORT;` |
|         - | 10768 | `					}` |
|       ! 0 | 10769 | `				}else{` |
|         - | 10770 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10771 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10772 | `					 * unless they already extend Exception or Error.` |
|         - | 10773 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10774 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10775 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    164123 | 10776 | `					SyString *pFqn = &pClass->sName;` |
|    164123 | 10777 | `					int bIsExceptionOrError =` |
|     85875 | 10778 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    248087 | 10779 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    162219 | 10780 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3826 | 10781 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    167935 | 10782 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11454 | 10783 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3815 | 10784 | `						!bIsExceptionOrError ){` |
|        12 | 10785 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10786 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10787 | `							&pClass->sName);` |
|         9 | 10788 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10789 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10790 | `							return SXERR_ABORT;` |
|         - | 10791 | `						}` |
|         - | 10792 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10793 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10794 | `					}else{` |
|    164117 | 10795 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10796 | `					}` |
|         - | 10797 | `				}` |
|    164123 | 10798 | `				SyBlobRelease(&sResolved);` |
|    164123 | 10799 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53468 | 10800 | `					break;` |
|         - | 10801 | `				}` |
|     57197 | 10802 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10803 | `			}` |
|     53463 | 10804 | `		}` |
|    143167 | 10805 | `	}` |
|    352493 | 10806 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10807 | `		/* Syntax error */` |
|       ! 0 | 10808 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10809 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10810 | `		if( rc == SXERR_ABORT ){` |
|         - | 10811 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10812 | `			return SXERR_ABORT;` |
|         - | 10813 | `		}` |
|       ! 0 | 10814 | `		return SXRET_OK;` |
|         - | 10815 | `	}` |
|    352493 | 10816 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    352493 | 10817 | `	pEnd = 0; /* cc warning */` |
|         - | 10818 | `	/* Delimit the class body */` |
|    352493 | 10819 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    352493 | 10820 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10821 | `		/* Syntax error */` |
|       ! 0 | 10822 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10823 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10824 | `		if( rc == SXERR_ABORT ){` |
|         - | 10825 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10826 | `			return SXERR_ABORT;` |
|         - | 10827 | `		}` |
|       ! 0 | 10828 | `		return SXRET_OK;` |
|         - | 10829 | `	}` |
|         - | 10830 | `	/* The delimiter token is the class body's closing brace */` |
|    352493 | 10831 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10832 | `	/* Swap token stream */` |
|    352493 | 10833 | `	pTmp = pGen->pEnd;` |
|    352493 | 10834 | `	pGen->pEnd = pEnd;` |
|         - | 10835 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    352493 | 10836 | `	pClass->iFlags \|= iFlags;` |
|         - | 10837 | `	/* Start the parse process */` |
|   1364474 | 10838 | `	for(;;){` |
|         - | 10839 | `		/* Jump leading/trailing semi-colons */` |
|   3889975 | 10840 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    702531 | 10841 | `			pGen->pIn++;` |
|         5 | 10842 | `		}` |
|   3187449 | 10843 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10844 | `			/* End of class body */` |
|    352451 | 10845 | `			break;` |
|         - | 10846 | `		}` |
|         - | 10847 | `		/* Bind a directly-preceding docblock to this member */` |
|   2835003 | 10848 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2834998 | 10849 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1417504 | 10850 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10851 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10852 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10853 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10854 | `			if( rc == SXERR_ABORT ){` |
|         - | 10855 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10856 | `				return SXERR_ABORT;` |
|         - | 10857 | `			}` |
|       ! 0 | 10858 | `			goto done;` |
|         - | 10859 | `		}` |
|         - | 10860 | `		/* Assume public visibility */` |
|   2835003 | 10861 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2835003 | 10862 | `		iAttrflags = 0;` |
|         - | 10863 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10864 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10865 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10866 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2835003 | 10867 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10868 | `			int bMod = 0;` |
|       ! 0 | 10869 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10870 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10871 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10872 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10873 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10874 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10875 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10876 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10877 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10878 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10879 | `			}` |
|       ! 0 | 10880 | `			if( !bMod ){` |
|       ! 0 | 10881 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10882 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10883 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10884 | `						return SXERR_ABORT;` |
|         - | 10885 | `					}` |
|       ! 0 | 10886 | `					goto done;` |
|         - | 10887 | `				}` |
|       ! 0 | 10888 | `				continue;` |
|         - | 10889 | `			}` |
|       ! 0 | 10890 | `		}` |
|   2835003 | 10891 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10892 | `			/* Extract the current keyword */` |
|   2835003 | 10893 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2835003 | 10894 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10895 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7671 | 10896 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7671 | 10897 | `				if( rc != SXRET_OK ){` |
|         6 | 10898 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10899 | `						return SXERR_ABORT;` |
|         - | 10900 | `					}` |
|         6 | 10901 | `					goto done;` |
|         - | 10902 | `				}` |
|      7667 | 10903 | `				continue;` |
|         - | 10904 | `			}` |
|   2827337 | 10905 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 10906 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 10907 | `				TraitUseEntry sUse;` |
|     15317 | 10908 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15317 | 10909 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15317 | 10910 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7664 | 10911 | `				for(;;){` |
|         - | 10912 | `					ph7_class *pTrait;` |
|         - | 10913 | `					SyString *pTraitName;` |
|     15325 | 10914 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 10915 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10916 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 10917 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10918 | `							return SXERR_ABORT;` |
|         - | 10919 | `						}` |
|       ! 0 | 10920 | `						break;` |
|         - | 10921 | `					}` |
|     15325 | 10922 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 10923 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 10924 | `						SyBlob sResolved;` |
|     15325 | 10925 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15325 | 10926 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     30645 | 10927 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15320 | 10928 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15325 | 10929 | `						SyBlobRelease(&sResolved);` |
|         - | 10930 | `					}` |
|         - | 10931 | `					/* Only traits are allowed */` |
|     15325 | 10932 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 10933 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 10934 | `					}` |
|     15325 | 10935 | `					if( pTrait == 0 ){` |
|       ! 0 | 10936 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10937 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 10938 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10939 | `							return SXERR_ABORT;` |
|         - | 10940 | `						}` |
|       ! 0 | 10941 | `					}else{` |
|     15325 | 10942 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 10943 | `					}` |
|     15325 | 10944 | `					pGen->pIn++; /* Advance past trait name */` |
|     15325 | 10945 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7661 | 10946 | `						break;` |
|         - | 10947 | `					}` |
|        10 | 10948 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 10949 | `				}` |
|         - | 10950 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15317 | 10951 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 10952 | `					SyToken *pBlock;` |
|        13 | 10953 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 10954 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 10955 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 10956 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 10957 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 10958 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 10959 | `					}else{` |
|       ! 0 | 10960 | `						pGen->pIn = pGen->pEnd;` |
|         - | 10961 | `					}` |
|         5 | 10962 | `				}` |
|     15317 | 10963 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 10964 | `				/* The semicolon will be consumed by the outer loop */` |
|     15317 | 10965 | `				continue;` |
|         - | 10966 | `			}` |
|   2812025 | 10967 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 10968 | `				int nSetTok;` |
|   2567563 | 10969 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2567563 | 10970 | `				if( nSetVis ){` |
|         - | 10971 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 10972 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 10973 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10974 | `					pGen->pIn += nSetTok;` |
|         2 | 10975 | `				}else{` |
|   2567561 | 10976 | `					iProtection = nKwrd;` |
|   2567561 | 10977 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 10978 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 10979 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2567561 | 10980 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2567561 | 10981 | `					if( nSetVis ){` |
|         9 | 10982 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 10983 | `						pGen->pIn += nSetTok;` |
|         4 | 10984 | `					}` |
|         - | 10985 | `				}` |
|         - | 10986 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 10987 | ``				 * `public private(set) readonly int $x`. */`` |
|   2567563 | 10988 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 10989 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 10990 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 10991 | `				}` |
|   2567558 | 10992 | `				if( pGen->pIn >= pGen->pEnd` |
|   2567563 | 10993 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10994 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10995 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10996 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 10997 | `					if( rc == SXERR_ABORT ){` |
|         - | 10998 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 10999 | `						return SXERR_ABORT;` |
|         - | 11000 | `					}` |
|       ! 0 | 11001 | `					goto done;` |
|         - | 11002 | `				}` |
|   2567563 | 11003 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11004 | `					/* Attribute declaration (untyped) */` |
|    408507 | 11005 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    408507 | 11006 | `					if( rc != SXRET_OK ){` |
|        11 | 11007 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11008 | `							return SXERR_ABORT;` |
|         - | 11009 | `						}` |
|        11 | 11010 | `						goto done;` |
|         - | 11011 | `					}` |
|    408643 | 11012 | `					continue;` |
|         - | 11013 | `				}` |
|   2159061 | 11014 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11015 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       299 | 11016 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       299 | 11017 | `					if( rc != SXRET_OK ){` |
|         8 | 11018 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11019 | `							return SXERR_ABORT;` |
|         - | 11020 | `						}` |
|         8 | 11021 | `						goto done;` |
|         - | 11022 | `					}` |
|       293 | 11023 | `					continue;` |
|         - | 11024 | `				}` |
|         - | 11025 | `				/* Extract the keyword */` |
|   2158767 | 11026 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1079381 | 11027 | `			}` |
|   2403229 | 11028 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 11029 | `				/* Process constant declaration */` |
|    236499 | 11030 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    236499 | 11031 | `				if( rc != SXRET_OK ){` |
|        11 | 11032 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11033 | `						return SXERR_ABORT;` |
|         - | 11034 | `					}` |
|        11 | 11035 | `					goto done;` |
|         - | 11036 | `				}` |
|    118248 | 11037 | `			}else{` |
|   2166735 | 11038 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 11039 | `					/* Static method or attribute,record that */` |
|     95447 | 11040 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     95447 | 11041 | `					pGen->pIn++; /* Jump the static keyword */` |
|     95447 | 11042 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11043 | `						int nSetTok;` |
|     68733 | 11044 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     68733 | 11045 | `						if( nSetVis ){` |
|         - | 11046 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 11047 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11048 | `							pGen->pIn += nSetTok;` |
|         2 | 11049 | `						}else{` |
|         - | 11050 | `							/* Extract the keyword */` |
|     68731 | 11051 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     68731 | 11052 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11053 | `								iProtection = nKwrd;` |
|       ! 0 | 11054 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 11055 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 11056 | `								if( nSetVis ){` |
|       ! 0 | 11057 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 11058 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 11059 | `								}` |
|       ! 0 | 11060 | `							}` |
|         - | 11061 | `						}` |
|     34364 | 11062 | `					}` |
|         - | 11063 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 11064 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 11065 | `					 * than a generic "expecting method" parse error. */` |
|     95447 | 11066 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11067 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11068 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 11069 | `					}` |
|     95442 | 11070 | `					if( pGen->pIn >= pGen->pEnd` |
|     95447 | 11071 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11072 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11073 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 11074 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11075 | `						if( rc == SXERR_ABORT ){` |
|         - | 11076 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11077 | `							return SXERR_ABORT;` |
|         - | 11078 | `						}` |
|       ! 0 | 11079 | `						goto done;` |
|         - | 11080 | `					}` |
|     95447 | 11081 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11082 | `						/* Attribute declaration */` |
|     26717 | 11083 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26717 | 11084 | `						if( rc != SXRET_OK ){` |
|         3 | 11085 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11086 | `								return SXERR_ABORT;` |
|         - | 11087 | `							}` |
|         3 | 11088 | `							goto done;` |
|         - | 11089 | `						}` |
|     26715 | 11090 | `						continue;` |
|         - | 11091 | `					}` |
|     68735 | 11092 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11093 | `						/* Typed static attribute declaration */` |
|        17 | 11094 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        17 | 11095 | `						if( rc != SXRET_OK ){` |
|         3 | 11096 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11097 | `								return SXERR_ABORT;` |
|         - | 11098 | `							}` |
|         3 | 11099 | `							goto done;` |
|         - | 11100 | `						}` |
|        15 | 11101 | `						continue;` |
|         - | 11102 | `					}` |
|         - | 11103 | `					/* Extract the keyword */` |
|     68721 | 11104 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2105651 | 11105 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 11106 | `					/* Abstract method,record that */` |
|      7649 | 11107 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 11108 | `					/* Mark the whole class as abstract */` |
|      7649 | 11109 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 11110 | `					/* Advance the stream cursor */` |
|      7649 | 11111 | `					pGen->pIn++;` |
|      7649 | 11112 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7649 | 11113 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7649 | 11114 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7647 | 11115 | `							iProtection = nKwrd;` |
|      7647 | 11116 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3821 | 11117 | `						}` |
|      3822 | 11118 | `					}` |
|      7649 | 11119 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7644 | 11120 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11121 | `							/* Static method */` |
|       ! 0 | 11122 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11123 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11124 | `					}` |
|      7649 | 11125 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7644 | 11126 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 11127 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 11128 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 11129 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 11130 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11131 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11132 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11133 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11134 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11135 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11136 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11137 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11138 | `										return SXERR_ABORT;` |
|         - | 11139 | `									}` |
|       ! 0 | 11140 | `									goto done;` |
|         - | 11141 | `								}` |
|         7 | 11142 | `								continue;` |
|         - | 11143 | `							}` |
|       ! 0 | 11144 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11145 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11146 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11147 | `							if( rc == SXERR_ABORT ){` |
|         - | 11148 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11149 | `								return SXERR_ABORT;` |
|         - | 11150 | `							}` |
|       ! 0 | 11151 | `							goto done;` |
|         - | 11152 | `					}` |
|      7643 | 11153 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2067468 | 11154 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11155 | `					/* final method ,record that */` |
|        21 | 11156 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        21 | 11157 | `					pGen->pIn++; /* Jump the final keyword */` |
|        21 | 11158 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11159 | `						/* Extract the keyword */` |
|        21 | 11160 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        21 | 11161 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        11 | 11162 | `							iProtection = nKwrd;` |
|        11 | 11163 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11164 | `						}` |
|         9 | 11165 | `					}` |
|        21 | 11166 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11167 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11168 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11169 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11170 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11171 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11172 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11173 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11174 | `									return SXERR_ABORT;` |
|         - | 11175 | `								}` |
|       ! 0 | 11176 | `								goto done;` |
|         - | 11177 | `							}` |
|        14 | 11178 | `							continue;` |
|         - | 11179 | `					}` |
|         9 | 11180 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11181 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11182 | `							/* Static method */` |
|       ! 0 | 11183 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11184 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11185 | `					}` |
|         9 | 11186 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11187 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11188 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11189 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11190 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11191 | `							if( rc == SXERR_ABORT ){` |
|         - | 11192 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11193 | `								return SXERR_ABORT;` |
|         - | 11194 | `							}` |
|       ! 0 | 11195 | `							goto done;` |
|         - | 11196 | `					}` |
|         9 | 11197 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11198 | `				}` |
|   2139991 | 11199 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11200 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11201 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11202 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11203 | `						if( rc == SXERR_ABORT ){` |
|         - | 11204 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11205 | `							return SXERR_ABORT;` |
|         - | 11206 | `						}` |
|       ! 0 | 11207 | `						goto done;` |
|         - | 11208 | `				}` |
|   2139991 | 11209 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11210 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11211 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11212 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11213 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11214 | `						if( rc == SXERR_ABORT ){` |
|         - | 11215 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11216 | `							return SXERR_ABORT;` |
|         - | 11217 | `						}` |
|       ! 0 | 11218 | `						goto done;` |
|         - | 11219 | `					}` |
|         - | 11220 | `					/* Attribute declaration */` |
|         7 | 11221 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11222 | `				}else{` |
|         - | 11223 | `					/* Process method declaration */` |
|   2139985 | 11224 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11225 | `				}` |
|   2139991 | 11226 | `				if( rc != SXRET_OK ){` |
|        16 | 11227 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11228 | `						return SXERR_ABORT;` |
|         - | 11229 | `					}` |
|        16 | 11230 | `					goto done;` |
|         - | 11231 | `				}` |
|         - | 11232 | `			}` |
|   1188235 | 11233 | `		}else{` |
|         - | 11234 | `			/* Attribute declaration */` |
|       ! 0 | 11235 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11236 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11237 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11238 | `					return SXERR_ABORT;` |
|         - | 11239 | `				}` |
|       ! 0 | 11240 | `				goto done;` |
|         - | 11241 | `			}` |
|         - | 11242 | `		}` |
|         5 | 11243 | `	}` |
|         - | 11244 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11245 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11246 | `	 */` |
|         - | 11247 | `	{` |
|         - | 11248 | `		TraitUseEntry *apUse;` |
|         - | 11249 | `		sxu32 nU;` |
|    352451 | 11250 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    367763 | 11251 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15317 | 11252 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15317 | 11253 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15317 | 11254 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15317 | 11255 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11256 | `			sxu32 nT;` |
|     15317 | 11257 | `			if( !hasResolution ){` |
|         - | 11258 | `				/* No conflict resolution block: use standard trait application */` |
|     30615 | 11259 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15313 | 11260 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15313 | 11261 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11262 | `						break;` |
|         - | 11263 | `					}` |
|      7659 | 11264 | `				}` |
|      7656 | 11265 | `			}else{` |
|         - | 11266 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11267 | `				 * then use the block to resolve method conflicts.` |
|         - | 11268 | `				 */` |
|         - | 11269 | `				SyToken *pR;` |
|        25 | 11270 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11271 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11272 | `					ph7_class_attr *pAR;` |
|         - | 11273 | `					SyHashEntry *pER;` |
|         - | 11274 | `					SyString *pNR;` |
|        15 | 11275 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11276 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11277 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11278 | `						pNR = &pAR->sName;` |
|       ! 0 | 11279 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11280 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11281 | `						}` |
|       ! 0 | 11282 | `					}` |
|        15 | 11283 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11284 | `				}` |
|         - | 11285 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11286 | `				pR = pUse->pResolvStart;` |
|        27 | 11287 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11288 | `					SyString sTrait,sMethod;` |
|         - | 11289 | `					ph7_class *pSrcTrait;` |
|         - | 11290 | `					ph7_class_method *pMeth;` |
|         - | 11291 | `					sxi32 nRKwrd;` |
|        41 | 11292 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11293 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11294 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11295 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11296 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11297 | `					sMethod = pR->sData;` |
|        17 | 11298 | `					pR++;` |
|        17 | 11299 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11300 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11301 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11302 | `							sTrait = sMethod;` |
|         7 | 11303 | `							pR++;` |
|         7 | 11304 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11305 | `							sMethod = pR->sData;` |
|         7 | 11306 | `							pR++;` |
|         3 | 11307 | `						}` |
|         3 | 11308 | `					}` |
|        17 | 11309 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11310 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11311 | `						continue;` |
|         - | 11312 | `					}` |
|        17 | 11313 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11314 | `					pR++;` |
|        17 | 11315 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11316 | `						pSrcTrait = 0;` |
|         7 | 11317 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11318 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11319 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11320 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11321 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11322 | `								break;` |
|         - | 11323 | `							}` |
|         2 | 11324 | `						}` |
|         5 | 11325 | `						if( pSrcTrait ){` |
|         5 | 11326 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11327 | `							if( pMeth ){` |
|         5 | 11328 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11329 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11330 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11331 | `								}` |
|         2 | 11332 | `							}` |
|         2 | 11333 | `						}` |
|         2 | 11334 | `					}` |
|        35 | 11335 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11336 | `				}` |
|         - | 11337 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11338 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11339 | `					ph7_class_method *pMR;` |
|         - | 11340 | `					SyHashEntry *pER;` |
|         - | 11341 | `					SyString *pNR;` |
|        15 | 11342 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11343 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11344 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11345 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11346 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11347 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11348 | `						}` |
|         3 | 11349 | `					}` |
|         9 | 11350 | `				}` |
|         - | 11351 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11352 | `				pR = pUse->pResolvStart;` |
|        27 | 11353 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11354 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11355 | `					ph7_class *pSrcTrait;` |
|         - | 11356 | `					ph7_class_method *pMeth;` |
|        27 | 11357 | `					int hasQual = 0;` |
|         - | 11358 | `					sxi32 nRKwrd;` |
|        41 | 11359 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11360 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11361 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11362 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11363 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11364 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11365 | `					sMethod = pR->sData;` |
|        17 | 11366 | `					pR++;` |
|        17 | 11367 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11368 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11369 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11370 | `							sTrait = sMethod;` |
|         7 | 11371 | `							hasQual = 1;` |
|         7 | 11372 | `							pR++;` |
|         7 | 11373 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11374 | `							sMethod = pR->sData;` |
|         7 | 11375 | `							pR++;` |
|         3 | 11376 | `						}` |
|         3 | 11377 | `					}` |
|        17 | 11378 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11379 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11380 | `						continue;` |
|         - | 11381 | `					}` |
|        17 | 11382 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11383 | `					pR++;` |
|        17 | 11384 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11385 | `						sxi32 iNewVis = -1;` |
|        13 | 11386 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11387 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11388 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11389 | `								iNewVis = nAK;` |
|         7 | 11390 | `								pR++;` |
|         3 | 11391 | `							}` |
|         3 | 11392 | `						}` |
|        13 | 11393 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11394 | `							sAlias = pR->sData;` |
|        11 | 11395 | `							pR++;` |
|         4 | 11396 | `						}` |
|        13 | 11397 | `						pMeth = 0;` |
|        13 | 11398 | `						if( hasQual ){` |
|         3 | 11399 | `							pSrcTrait = 0;` |
|         5 | 11400 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11401 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11402 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11403 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11404 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11405 | `									break;` |
|         - | 11406 | `								}` |
|         2 | 11407 | `							}` |
|         3 | 11408 | `							if( pSrcTrait ){` |
|         3 | 11409 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11410 | `							}` |
|         2 | 11411 | `						}else{` |
|        10 | 11412 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11413 | `						}` |
|        13 | 11414 | `						if( pMeth ){` |
|        13 | 11415 | `							if( sAlias.nByte > 0 ){` |
|         - | 11416 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11417 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11418 | `								 */` |
|         - | 11419 | `								ph7_class_method *pAlias;` |
|         - | 11420 | `								char *zAliasDup;` |
|        11 | 11421 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11422 | `								if( pAlias ){` |
|        11 | 11423 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11424 | `									if( iNewVis >= 0 ){` |
|         5 | 11425 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11426 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11427 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11428 | `									}` |
|        11 | 11429 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11430 | `									if( zAliasDup ){` |
|        11 | 11431 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11432 | `									}` |
|         7 | 11433 | `								}` |
|         7 | 11434 | `							}else if( iNewVis >= 0 ){` |
|         - | 11435 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11436 | `								ph7_class_method *pCopy;` |
|         3 | 11437 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11438 | `								if( pCopy ){` |
|         3 | 11439 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11440 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11441 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11442 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11443 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11444 | `									/* Replace the method in the class hash */` |
|         3 | 11445 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11446 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11447 | `								}` |
|         1 | 11448 | `							}` |
|         5 | 11449 | `						}` |
|         5 | 11450 | `						SXUNUSED(hasQual);` |
|         5 | 11451 | `					}` |
|        21 | 11452 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11453 | `				}` |
|         - | 11454 | `			}` |
|     15317 | 11455 | `			SySetRelease(&pUse->aTraits);` |
|      7661 | 11456 | `		}` |
|         - | 11457 | `	}` |
|    352451 | 11458 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11459 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11460 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3841 | 11461 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3841 | 11462 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11463 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11464 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11465 | `			return SXERR_ABORT;` |
|         - | 11466 | `		}` |
|      1918 | 11467 | `	}` |
|         - | 11468 | `	/* Install the class */` |
|    352451 | 11469 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    352451 | 11470 | `	if( rc == SXRET_OK ){` |
|         - | 11471 | `		ph7_class **apInterface;` |
|         - | 11472 | `		sxu32 n;` |
|    352451 | 11473 | `		if( pBase ){` |
|         - | 11474 | `			/* Inherit from base class and mark as a subclass */` |
|    183235 | 11475 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     91615 | 11476 | `		}` |
|    352451 | 11477 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    516563 | 11478 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11479 | `			/* Implements one or more interface */` |
|    164117 | 11480 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    164117 | 11481 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11482 | `				break;` |
|         - | 11483 | `			}` |
|     82061 | 11484 | `		}` |
|         - | 11485 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11486 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    352451 | 11487 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3841 | 11488 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3841 | 11489 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11490 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11491 | `			}` |
|      3841 | 11492 | `			if( pIntf ){` |
|      3841 | 11493 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1918 | 11494 | `			}` |
|      3841 | 11495 | `			if( pClass->nEnumBacking != 0 ){` |
|      3829 | 11496 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3829 | 11497 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11498 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11499 | `				}` |
|      3829 | 11500 | `				if( pIntf ){` |
|      3829 | 11501 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1912 | 11502 | `				}` |
|      1912 | 11503 | `			}` |
|      1918 | 11504 | `		}` |
|         - | 11505 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11506 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    352446 | 11507 | `		if( rc == SXRET_OK` |
|    352446 | 11508 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    352451 | 11509 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    186899 | 11510 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11511 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    186899 | 11512 | `			if( pStringable ){` |
|    186899 | 11513 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    186899 | 11514 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11515 | `				sxu32 i;` |
|    186899 | 11516 | `				int bAlready = 0;` |
|    225023 | 11517 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     41943 | 11518 | `					if( apImpl[i] == pStringable ){` |
|      3819 | 11519 | `						bAlready = 1;` |
|      3819 | 11520 | `						break;` |
|         - | 11521 | `					}` |
|     19067 | 11522 | `				}` |
|    186899 | 11523 | `				if( !bAlready ){` |
|    183085 | 11524 | `					PH7_ClassImplement(pClass,pStringable);` |
|     91540 | 11525 | `				}` |
|     93447 | 11526 | `			}` |
|     93447 | 11527 | `		}` |
|         - | 11528 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    352451 | 11529 | `		if( rc == SXRET_OK ){` |
|    352451 | 11530 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    352451 | 11531 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11532 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11533 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11534 | `				return SXERR_ABORT;` |
|         - | 11535 | `			}` |
|    176223 | 11536 | `		}` |
|         - | 11537 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    352451 | 11538 | `		if( rc == SXRET_OK ){` |
|    352451 | 11539 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    352451 | 11540 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11541 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11542 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11543 | `				return SXERR_ABORT;` |
|         - | 11544 | `			}` |
|    176223 | 11545 | `		}` |
|    176223 | 11546 | `	}` |
|    352451 | 11547 | `	SySetRelease(&aUseEntries);` |
|    352451 | 11548 | `	SySetRelease(&aInterfaces);` |
|    352451 | 11549 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11550 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11551 | `		return SXERR_ABORT;` |
|         - | 11552 | `	}` |
|    176223 | 11553 | `done:` |
|         - | 11554 | `	/* Point beyond the class body */` |
|    352493 | 11555 | `	pGen->pIn = &pEnd[1];` |
|    352493 | 11556 | `	pGen->pEnd = pTmp;` |
|    352493 | 11557 | `	return PH7_OK;` |
|    176250 | 11558 | `}` |
|         - | 11559 | `/* Compile a named class declaration (the common case). */` |
|    352462 | 11560 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11561 | `{` |
|    352467 | 11562 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11563 | `}` |
|         - | 11564 | `/*` |
|         - | 11565 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11566 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11567 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11568 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11569 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11570 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11571 | ` */` |
|        28 | 11572 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11573 | `{` |
|         - | 11574 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11575 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11576 | `	SyString sName;` |
|         - | 11577 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11578 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11579 | `	                              * is keyed to this 'class' token */` |
|         - | 11580 | `	ph7_value *pObj;` |
|        32 | 11581 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11582 | `	sxu32 nIdx,nLen;` |
|         - | 11583 | `	sxi32 nArg,rc;` |
|        14 | 11584 | `	SXUNUSED(iCompileFlag);` |
|         - | 11585 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11586 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11587 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11588 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11589 | `	}` |
|        32 | 11590 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11591 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11592 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11593 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11594 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11595 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11596 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11597 | `		return rc;` |
|         - | 11598 | `	}` |
|         - | 11599 | `	{` |
|         - | 11600 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11601 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11602 | `		if( pAnonClass` |
|        32 | 11603 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11604 | `			return SXERR_ABORT;` |
|         - | 11605 | `		}` |
|         - | 11606 | `	}` |
|         - | 11607 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11608 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11609 | `	nArg = 0;` |
|        32 | 11610 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11611 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11612 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11613 | `		SyToken *pArgNext;` |
|         7 | 11614 | `		pGen->pIn = pArgStart;` |
|         7 | 11615 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11616 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11617 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11618 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11619 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11620 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11621 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11622 | `					return SXERR_ABORT;` |
|         - | 11623 | `				}` |
|         7 | 11624 | `				nArg++;` |
|         3 | 11625 | `			}` |
|         7 | 11626 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11627 | `		}` |
|         7 | 11628 | `		pGen->pIn = pSavedIn;` |
|         7 | 11629 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11630 | `	}` |
|         - | 11631 | `	/* Load the synthesized class name */` |
|        32 | 11632 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11633 | `	if( pObj == 0 ){` |
|       ! 0 | 11634 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11635 | `		return SXERR_ABORT;` |
|         - | 11636 | `	}` |
|        32 | 11637 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11638 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11639 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11640 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11641 | `	return SXRET_OK;` |
|        18 | 11642 | `}` |
|         - | 11643 | `/*` |
|         - | 11644 | ` * Compile a user-defined abstract class.` |
|         - | 11645 | ` *  According to the PHP language reference manual` |
|         - | 11646 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11647 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11648 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11649 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11650 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11651 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11652 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11653 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11654 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11655 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11656 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11657 | ` *   could differ.` |
|         - | 11658 | ` */` |
|         - | 11659 | `/*` |
|         - | 11660 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11661 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11662 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11663 | ` */` |
|  12432374 | 11664 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11665 | `{` |
|  12432379 | 11666 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   7294721 | 11667 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   7294721 | 11668 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   7248951 | 11669 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3609180 | 11670 | `	}` |
|  12356023 | 11671 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  12355963 | 11672 | `	return FALSE;` |
|   6216192 | 11673 | `}` |
|         - | 11674 | `/*` |
|         - | 11675 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11676 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11677 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11678 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11679 | ` */` |
|  12355958 | 11680 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11681 | `{` |
|  12355963 | 11682 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  12355963 | 11683 | `	sxi32 iFlags = 0,iFlag;` |
|  12432379 | 11684 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76421 | 11685 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11686 | `			pDup = pIn;` |
|         2 | 11687 | `		}` |
|     76421 | 11688 | `		iFlags \|= iFlag;` |
|     76421 | 11689 | `		pIn++;` |
|         5 | 11690 | `	}` |
|  12355963 | 11691 | `	*ppIn = pIn;` |
|  12355963 | 11692 | `	if( ppDup ){ *ppDup = pDup; }` |
|  12355963 | 11693 | `	return iFlags;` |
|         5 | 11694 | `}` |
|         - | 11695 | `/*` |
|         - | 11696 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11697 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11698 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11699 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11700 | `` * `readonly`) to their existing handlers.`` |
|         - | 11701 | ` */` |
|  12321572 | 11702 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11703 | `{` |
|  12321577 | 11704 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   6202803 | 11705 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  12342579 | 11706 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11707 | `}` |
|         - | 11708 | `/*` |
|         - | 11709 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11710 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11711 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11712 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11713 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11714 | ` */` |
|     34386 | 11715 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11716 | `{` |
|         - | 11717 | `	SyToken *pDup;` |
|     34391 | 11718 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11719 | `	sxi32 rc;` |
|     34391 | 11720 | `	if( pDup ){` |
|         4 | 11721 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11722 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11723 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11724 | `			return SXERR_ABORT;` |
|         - | 11725 | `		}` |
|         1 | 11726 | `	}` |
|     34386 | 11727 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17198 | 11728 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11729 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11730 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11731 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11732 | `			return SXERR_ABORT;` |
|         - | 11733 | `		}` |
|         1 | 11734 | `	}` |
|     34391 | 11735 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17198 | 11736 | `}` |
|         - | 11737 | `/*` |
|         - | 11738 | ` * Compile a user-defined trait.` |
|         - | 11739 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11740 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11741 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11742 | ` */` |
|      7702 | 11743 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11744 | `{` |
|      7707 | 11745 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11746 | `	ph7_class *pClass;` |
|         - | 11747 | `	SyToken *pEnd,*pTmp;` |
|         - | 11748 | `	sxi32 iProtection;` |
|         - | 11749 | `	sxi32 iAttrflags;` |
|         - | 11750 | `	SyString *pName;` |
|         - | 11751 | `	sxi32 nKwrd;` |
|         - | 11752 | `	sxi32 rc;` |
|         - | 11753 | `	/* Jump the 'trait' keyword */` |
|      7707 | 11754 | `	pGen->pIn++;` |
|      7707 | 11755 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11756 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11757 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11758 | `			return SXERR_ABORT;` |
|         - | 11759 | `		}` |
|       ! 0 | 11760 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11761 | `			pGen->pIn++;` |
|       ! 0 | 11762 | `		}` |
|       ! 0 | 11763 | `		return SXRET_OK;` |
|         - | 11764 | `	}` |
|         - | 11765 | `	/* Extract trait name */` |
|      7707 | 11766 | `	pName = &pGen->pIn->sData;` |
|      7707 | 11767 | `	pGen->pIn++;` |
|         - | 11768 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11769 | `		SyBlob sFQN;` |
|         - | 11770 | `		SyString sFQNStr;` |
|      7707 | 11771 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7707 | 11772 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7707 | 11773 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7707 | 11774 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7707 | 11775 | `		SyBlobRelease(&sFQN);` |
|         - | 11776 | `	}` |
|      7707 | 11777 | `	if( pClass == 0 ){` |
|       ! 0 | 11778 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11779 | `		return SXERR_ABORT;` |
|         - | 11780 | `	}` |
|      7707 | 11781 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7707 | 11782 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11783 | `		return SXERR_ABORT;` |
|         - | 11784 | `	}` |
|         - | 11785 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7707 | 11786 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11787 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11788 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11789 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11790 | `			return SXERR_ABORT;` |
|         - | 11791 | `		}` |
|       ! 0 | 11792 | `		return SXRET_OK;` |
|         - | 11793 | `	}` |
|      7707 | 11794 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7707 | 11795 | `	pEnd = 0;` |
|      7707 | 11796 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7707 | 11797 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11798 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11799 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11800 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11801 | `			return SXERR_ABORT;` |
|         - | 11802 | `		}` |
|       ! 0 | 11803 | `		return SXRET_OK;` |
|         - | 11804 | `	}` |
|         - | 11805 | `	/* The delimiter token is the trait body's closing brace */` |
|      7707 | 11806 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11807 | `	/* Swap token stream */` |
|      7707 | 11808 | `	pTmp = pGen->pEnd;` |
|      7707 | 11809 | `	pGen->pEnd = pEnd;` |
|         - | 11810 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7707 | 11811 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11812 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     53447 | 11813 | `	for(;;){` |
|    145067 | 11814 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     19091 | 11815 | `			pGen->pIn++;` |
|         5 | 11816 | `		}` |
|    125981 | 11817 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7707 | 11818 | `			break;` |
|         - | 11819 | `		}` |
|         - | 11820 | `		/* Bind a directly-preceding docblock to this member */` |
|    118279 | 11821 | `		GenStateSetPendingDoc(&(*pGen));` |
|    118279 | 11822 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11823 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11824 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11825 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11826 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11827 | `				return SXERR_ABORT;` |
|         - | 11828 | `			}` |
|       ! 0 | 11829 | `			goto done;` |
|         - | 11830 | `		}` |
|    118279 | 11831 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    118279 | 11832 | `		iAttrflags = 0;` |
|    118279 | 11833 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    118279 | 11834 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    118279 | 11835 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11836 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11837 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11838 | `				for(;;){` |
|         - | 11839 | `					ph7_class *pUsedTrait;` |
|         - | 11840 | `					SyString *pUsedName;` |
|         5 | 11841 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11842 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11843 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11844 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11845 | `							return SXERR_ABORT;` |
|         - | 11846 | `						}` |
|       ! 0 | 11847 | `						break;` |
|         - | 11848 | `					}` |
|         5 | 11849 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11850 | `					{` |
|         - | 11851 | `						SyBlob sResolved;` |
|         5 | 11852 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11853 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11854 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11855 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11856 | `						SyBlobRelease(&sResolved);` |
|         - | 11857 | `					}` |
|         5 | 11858 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11859 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11860 | `					}` |
|         5 | 11861 | `					if( pUsedTrait == 0 ){` |
|         4 | 11862 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11863 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11864 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11865 | `							return SXERR_ABORT;` |
|         - | 11866 | `						}` |
|         2 | 11867 | `					}else{` |
|         3 | 11868 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11869 | `					}` |
|         5 | 11870 | `					pGen->pIn++;` |
|         5 | 11871 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11872 | `						break;` |
|         - | 11873 | `					}` |
|       ! 0 | 11874 | `					pGen->pIn++;` |
|       ! 0 | 11875 | `				}` |
|         5 | 11876 | `				continue;` |
|         - | 11877 | `			}` |
|    118275 | 11878 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    118259 | 11879 | `				iProtection = nKwrd;` |
|    118259 | 11880 | `				pGen->pIn++;` |
|    118254 | 11881 | `				if( pGen->pIn >= pGen->pEnd` |
|    118259 | 11882 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11883 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11884 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11885 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11886 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11887 | `						return SXERR_ABORT;` |
|         - | 11888 | `					}` |
|       ! 0 | 11889 | `					goto done;` |
|         - | 11890 | `				}` |
|    118259 | 11891 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     19077 | 11892 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     19077 | 11893 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11894 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11895 | `							return SXERR_ABORT;` |
|         - | 11896 | `						}` |
|       ! 0 | 11897 | `						goto done;` |
|         - | 11898 | `					}` |
|     19077 | 11899 | `					continue;` |
|         - | 11900 | `				}` |
|     99187 | 11901 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 11902 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 11903 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11904 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11905 | `							return SXERR_ABORT;` |
|         - | 11906 | `						}` |
|       ! 0 | 11907 | `						goto done;` |
|         - | 11908 | `					}` |
|         5 | 11909 | `					continue;` |
|         - | 11910 | `				}` |
|     99183 | 11911 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     49589 | 11912 | `			}` |
|     99199 | 11913 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 11914 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11915 | `					"Traits cannot have constants");` |
|       ! 0 | 11916 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11917 | `					return SXERR_ABORT;` |
|         - | 11918 | `				}` |
|       ! 0 | 11919 | `				goto done;` |
|       ! 0 | 11920 | `			}else{` |
|     99199 | 11921 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7639 | 11922 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7639 | 11923 | `					pGen->pIn++;` |
|      7639 | 11924 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7637 | 11925 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7637 | 11926 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11927 | `							iProtection = nKwrd;` |
|       ! 0 | 11928 | `							pGen->pIn++;` |
|       ! 0 | 11929 | `						}` |
|      3816 | 11930 | `					}` |
|      7634 | 11931 | `					if( pGen->pIn >= pGen->pEnd` |
|      7639 | 11932 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11933 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11934 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 11935 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11936 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11937 | `							return SXERR_ABORT;` |
|         - | 11938 | `						}` |
|       ! 0 | 11939 | `						goto done;` |
|         - | 11940 | `					}` |
|      7639 | 11941 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 11942 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 11943 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11944 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11945 | `								return SXERR_ABORT;` |
|         - | 11946 | `							}` |
|       ! 0 | 11947 | `							goto done;` |
|         - | 11948 | `						}` |
|         3 | 11949 | `						continue;` |
|         - | 11950 | `					}` |
|      7637 | 11951 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 11952 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11953 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11954 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11955 | `								return SXERR_ABORT;` |
|         - | 11956 | `							}` |
|       ! 0 | 11957 | `							goto done;` |
|         - | 11958 | `						}` |
|       ! 0 | 11959 | `						continue;` |
|         - | 11960 | `					}` |
|      7637 | 11961 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     95381 | 11962 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 11963 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 11964 | `					pGen->pIn++;` |
|         6 | 11965 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 11966 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 11967 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 11968 | `							iProtection = nKwrd;` |
|         6 | 11969 | `							pGen->pIn++;` |
|         2 | 11970 | `						}` |
|         2 | 11971 | `					}` |
|         6 | 11972 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 11973 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11974 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11975 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 11976 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11977 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11978 | `							return SXERR_ABORT;` |
|         - | 11979 | `						}` |
|       ! 0 | 11980 | `						goto done;` |
|         - | 11981 | `					}` |
|         6 | 11982 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 11983 | `				}` |
|     99197 | 11984 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11985 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11986 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 11987 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11988 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11989 | `						return SXERR_ABORT;` |
|         - | 11990 | `					}` |
|       ! 0 | 11991 | `					goto done;` |
|         - | 11992 | `				}` |
|     99197 | 11993 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 11994 | `					pGen->pIn++;` |
|       ! 0 | 11995 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 11996 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11997 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11998 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11999 | `							return SXERR_ABORT;` |
|         - | 12000 | `						}` |
|       ! 0 | 12001 | `						goto done;` |
|         - | 12002 | `					}` |
|       ! 0 | 12003 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12004 | `				}else{` |
|     99197 | 12005 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 12006 | `				}` |
|     99197 | 12007 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 12008 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12009 | `						return SXERR_ABORT;` |
|         - | 12010 | `					}` |
|       ! 0 | 12011 | `					goto done;` |
|         - | 12012 | `				}` |
|         - | 12013 | `			}` |
|     49601 | 12014 | `		}else{` |
|       ! 0 | 12015 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12016 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12017 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12018 | `					return SXERR_ABORT;` |
|         - | 12019 | `				}` |
|       ! 0 | 12020 | `				goto done;` |
|         - | 12021 | `			}` |
|         - | 12022 | `		}` |
|         5 | 12023 | `	}` |
|         - | 12024 | `	/* Install the trait */` |
|      7707 | 12025 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7707 | 12026 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12027 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12028 | `		return SXERR_ABORT;` |
|         - | 12029 | `	}` |
|      3851 | 12030 | `done:` |
|         - | 12031 | `	/* Point beyond the trait body */` |
|      7707 | 12032 | `	pGen->pIn = &pEnd[1];` |
|      7707 | 12033 | `	pGen->pEnd = pTmp;` |
|      7707 | 12034 | `	return PH7_OK;` |
|      3856 | 12035 | `}` |
|         - | 12036 | `/*` |
|         - | 12037 | ` * Compile a user-defined class.` |
|         - | 12038 | ` *  According to the PHP language reference manual` |
|         - | 12039 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 12040 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 12041 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 12042 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 12043 | ` *   and functions (called "methods").` |
|         - | 12044 | ` */` |
|    314236 | 12045 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 12046 | `{` |
|         - | 12047 | `	sxi32 rc;` |
|    314241 | 12048 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    314241 | 12049 | `	return rc;` |
|         5 | 12050 | `}` |
|         - | 12051 | `/*` |
|         - | 12052 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 12053 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 12054 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 12055 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 12056 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 12057 | ` */` |
|  12279562 | 12058 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 12059 | `{` |
|  12484134 | 12060 | `	return (pIn->nType & PH7_TK_ID)` |
|   6344348 | 12061 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    214230 | 12062 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  12484129 | 12063 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 12064 | `}` |
|         - | 12065 | `/*` |
|         - | 12066 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 12067 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 12068 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 12069 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 12070 | ` */` |
|      3840 | 12071 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 12072 | `{` |
|      3845 | 12073 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 12074 | `}` |
|         - | 12075 | `/*` |
|         - | 12076 | ` * Exception handling.` |
|         - | 12077 | ` *  According to the PHP language reference manual` |
|         - | 12078 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 12079 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 12080 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 12081 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 12082 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 12083 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 12084 | ` *    (or re-thrown) within a catch block.` |
|         - | 12085 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 12086 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 12087 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 12088 | ` *    been defined with set_exception_handler().` |
|         - | 12089 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 12090 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 12091 | ` */` |
|         - | 12092 | `/*` |
|         - | 12093 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 12094 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 12095 | ` * indicates failure.` |
|         - | 12096 | ` */` |
|    507410 | 12097 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 12098 | `{` |
|    507415 | 12099 | `	sxi32 rc = SXRET_OK;` |
|    507415 | 12100 | `	if( pRoot->pOp ){` |
|    507403 | 12101 | `		switch( pRoot->pOp->iOp ){` |
|    253699 | 12102 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 12103 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 12104 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 12105 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 12106 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 12107 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    507403 | 12108 | `			break;` |
|       ! 0 | 12109 | `		default:` |
|         - | 12110 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 12111 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 12112 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 12113 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12114 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 12115 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 12116 | `				rc = SXERR_INVALID;` |
|       ! 0 | 12117 | `			}` |
|       ! 0 | 12118 | `			break;` |
|         - | 12119 | `		}` |
|    253716 | 12120 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 12121 | `		/* Unexpected expression */` |
|       ! 0 | 12122 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12123 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12124 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 12125 | `			rc = SXERR_INVALID;` |
|       ! 0 | 12126 | `		}` |
|       ! 0 | 12127 | `	}` |
|    507415 | 12128 | `	return rc;` |
|         5 | 12129 | `}` |
|         - | 12130 | `/*` |
|         - | 12131 | ` * Compile a 'throw' statement.` |
|         - | 12132 | ` * throw: This is how you trigger an exception.` |
|         - | 12133 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12134 | ` */` |
|    507374 | 12135 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12136 | `{` |
|    507379 | 12137 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12138 | `	GenBlock *pBlock;` |
|         - | 12139 | `	sxu32 nIdx;` |
|         - | 12140 | `	sxi32 rc;` |
|    507379 | 12141 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12142 | `	/* Compile the expression */` |
|    507379 | 12143 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    507379 | 12144 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12145 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12146 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12147 | `			return SXERR_ABORT;` |
|         - | 12148 | `		}` |
|       ! 0 | 12149 | `		return SXRET_OK;` |
|         - | 12150 | `	}` |
|    507379 | 12151 | `	pBlock = pGen->pCurrent;` |
|         - | 12152 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   2021141 | 12153 | `	while(pBlock->pParent){` |
|   2021137 | 12154 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    507375 | 12155 | `			break;` |
|         - | 12156 | `		}` |
|         - | 12157 | `		/* Point to the parent block */` |
|   1513767 | 12158 | `		pBlock = pBlock->pParent;` |
|         5 | 12159 | `	}` |
|         - | 12160 | `	/* Emit the throw instruction */` |
|    507379 | 12161 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12162 | `	/* Emit the jump */` |
|    507379 | 12163 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    507379 | 12164 | `	return SXRET_OK;` |
|    253692 | 12165 | `}` |
|         - | 12166 | `/*` |
|         - | 12167 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12168 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12169 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12170 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12171 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12172 | ` */` |
|        36 | 12173 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12174 | `{` |
|        38 | 12175 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12176 | `	GenBlock *pBlock;` |
|         - | 12177 | `	sxu32 nIdx;` |
|         - | 12178 | `	sxi32 rc;` |
|        18 | 12179 | `	(void)iCompileFlag;` |
|        38 | 12180 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12181 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12182 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12183 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12184 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12185 | `			return SXERR_ABORT;` |
|         - | 12186 | `		}` |
|       ! 0 | 12187 | `		return SXRET_OK;` |
|         - | 12188 | `	}` |
|        38 | 12189 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12190 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12191 | `		return SXERR_ABORT;` |
|         - | 12192 | `	}` |
|        38 | 12193 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12194 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12195 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12196 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12197 | `			return SXERR_ABORT;` |
|         - | 12198 | `		}` |
|       ! 0 | 12199 | `		return SXRET_OK;` |
|         - | 12200 | `	}` |
|         - | 12201 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12202 | `	pBlock = pGen->pCurrent;` |
|        60 | 12203 | `	while( pBlock->pParent ){` |
|        49 | 12204 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12205 | `			break;` |
|         - | 12206 | `		}` |
|        23 | 12207 | `		pBlock = pBlock->pParent;` |
|         1 | 12208 | `	}` |
|        38 | 12209 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12210 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12211 | `	return SXRET_OK;` |
|        20 | 12212 | `}` |
|         - | 12213 | `/*` |
|         - | 12214 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12215 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12216 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12217 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12218 | ` * compile error propagated from the parser.` |
|         - | 12219 | ` */` |
|        54 | 12220 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12221 | `{` |
|         - | 12222 | `	SyString sClassName;` |
|         - | 12223 | `	SyToken *pToken;` |
|         - | 12224 | `	SyString *pName;` |
|         - | 12225 | `	char *zDup;` |
|         - | 12226 | `	sxi32 rc;` |
|        59 | 12227 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        59 | 12228 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        59 | 12229 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        59 | 12230 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        59 | 12231 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12232 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12233 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12234 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12235 | `		return SXERR_INVALID;` |
|         - | 12236 | `	}` |
|        59 | 12237 | `	pGen->pIn++; /* '(' */` |
|        27 | 12238 | `	for(;;){` |
|         - | 12239 | `		SyBlob sResolved;` |
|        59 | 12240 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        59 | 12241 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12242 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12243 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12244 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12245 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12246 | `			return SXERR_INVALID;` |
|         - | 12247 | `		}` |
|        86 | 12248 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        54 | 12249 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        59 | 12250 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        59 | 12251 | `		SyBlobRelease(&sResolved);` |
|        59 | 12252 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12253 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        59 | 12254 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        54 | 12255 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12256 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12257 | `			pGen->pIn++; continue;` |
|         - | 12258 | `		}` |
|        59 | 12259 | `		break;` |
|       ! 0 | 12260 | `	}` |
|        54 | 12261 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12262 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12263 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12264 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12265 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12266 | `		return SXERR_INVALID;` |
|         - | 12267 | `	}` |
|        59 | 12268 | `	pGen->pIn++; /* '$' */` |
|        59 | 12269 | `	pName = &pGen->pIn->sData;` |
|        59 | 12270 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12271 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12272 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12273 | `	pGen->pIn++;` |
|        59 | 12274 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12275 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12276 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12277 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12278 | `		return SXERR_INVALID;` |
|         - | 12279 | `	}` |
|        59 | 12280 | `	pGen->pIn++; /* ')' */` |
|        59 | 12281 | `	return SXRET_OK;` |
|        32 | 12282 | `}` |
|         - | 12283 | `/*` |
|         - | 12284 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12285 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12286 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12287 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12288 | ` * VmThrowException):` |
|         - | 12289 | ` *` |
|         - | 12290 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12291 | ` *    <try body>` |
|         - | 12292 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12293 | ` *    JMP  -> finally\|end` |
|         - | 12294 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12295 | ` *    <catch body>` |
|         - | 12296 | ` *    JMP  -> finally\|end` |
|         - | 12297 | ` *    ... more catches ...` |
|         - | 12298 | ` *  Lfin: <finally body>` |
|         - | 12299 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12300 | ` *  Lend:` |
|         - | 12301 | ` */` |
|        98 | 12302 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12303 | `{` |
|       103 | 12304 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12305 | `	GenBlock *pTry;` |
|         - | 12306 | `	VmInstr *pInstr;` |
|       103 | 12307 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12308 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12309 | `	sxi32 rc;` |
|       103 | 12310 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12311 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       103 | 12312 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       103 | 12313 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       103 | 12314 | `	pTry->pUserData = pException;` |
|       103 | 12315 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       103 | 12316 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       103 | 12317 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       103 | 12318 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       103 | 12319 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       103 | 12320 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12321 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       103 | 12322 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       103 | 12323 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       103 | 12324 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       103 | 12325 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12326 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       103 | 12327 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12328 | `	/* Catch clauses (inline) */` |
|       103 | 12329 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        98 | 12330 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        59 | 12331 | `		sxu32 k = 0;` |
|        81 | 12332 | `		for(;;){` |
|         - | 12333 | `			ph7_exception_block sCatch;` |
|         - | 12334 | `			GenBlock *pCatchBlk;` |
|       113 | 12335 | `			sxu32 idxJmp = 0;` |
|       108 | 12336 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       104 | 12337 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        32 | 12338 | `				break;` |
|         - | 12339 | `			}` |
|        59 | 12340 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        59 | 12341 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12342 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        59 | 12343 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        59 | 12344 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        59 | 12345 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        59 | 12346 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12347 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12348 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12349 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        59 | 12350 | `			pCatchBlk->pUserData = pException;` |
|        59 | 12351 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        59 | 12352 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12353 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        59 | 12354 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12355 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12356 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        59 | 12357 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        59 | 12358 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        59 | 12359 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        59 | 12360 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        59 | 12361 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        59 | 12362 | `			k++;` |
|         5 | 12363 | `		}` |
|        27 | 12364 | `	}` |
|         - | 12365 | `	/* Finally (inline) */` |
|       103 | 12366 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12367 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12368 | `		GenBlock *pFinBlk;` |
|        52 | 12369 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12370 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12371 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12372 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12373 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12374 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12375 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12376 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12377 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12378 | `		pException->iHasFinally = 1;` |
|        24 | 12379 | `	}` |
|       103 | 12380 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       103 | 12381 | `	pException->iInlined = 1;` |
|         - | 12382 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12383 | `	{` |
|       103 | 12384 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12385 | `		sxu32 *aJ; sxu32 n;` |
|       103 | 12386 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       103 | 12387 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       103 | 12388 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       157 | 12389 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        59 | 12390 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        59 | 12391 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        32 | 12392 | `		}` |
|         - | 12393 | `	}` |
|       103 | 12394 | `	SySetRelease(&aCatchJmp);` |
|       103 | 12395 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12396 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12397 | `	}` |
|       103 | 12398 | `	return SXRET_OK;` |
|        54 | 12399 | `}` |
|         - | 12400 | `/*` |
|         - | 12401 | ` * Compile a 'catch' block.` |
|         - | 12402 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12403 | ` * an object containing the exception information.` |
|         - | 12404 | ` */` |
|     24388 | 12405 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12406 | `{` |
|     24393 | 12407 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12408 | `	ph7_exception_block sCatch;` |
|         - | 12409 | `	SySet *pInstrContainer;` |
|         - | 12410 | `	SyString sClassName;` |
|         - | 12411 | `	GenBlock *pCatch;` |
|         - | 12412 | `	SyToken *pToken;` |
|         - | 12413 | `	SyString *pName;` |
|         - | 12414 | `	char *zDup;` |
|         - | 12415 | `	sxi32 rc;` |
|     24393 | 12416 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12417 | `	/* Zero the structure */` |
|     24393 | 12418 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12419 | `	/* Initialize fields */` |
|     24393 | 12420 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24393 | 12421 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24393 | 12422 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12423 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12424 | `			pToken = pGen->pIn;` |
|       ! 0 | 12425 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12426 | `				pToken--;` |
|       ! 0 | 12427 | `			}` |
|       ! 0 | 12428 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12429 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12430 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12431 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12432 | `				return SXERR_ABORT;` |
|         - | 12433 | `			}` |
|       ! 0 | 12434 | `			return SXERR_INVALID;` |
|         - | 12435 | `	}` |
|         - | 12436 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24393 | 12437 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12208 | 12438 | `	for(;;){` |
|         - | 12439 | `		SyBlob sResolved;` |
|     24421 | 12440 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24421 | 12441 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12442 | `			SyBlobRelease(&sResolved);` |
|         6 | 12443 | `			pToken = pGen->pIn;` |
|         6 | 12444 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12445 | `				pToken--;` |
|       ! 0 | 12446 | `			}` |
|         8 | 12447 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12448 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12449 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12450 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12451 | `				return SXERR_ABORT;` |
|         - | 12452 | `			}` |
|         6 | 12453 | `			return SXERR_INVALID;` |
|         - | 12454 | `		}` |
|         - | 12455 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12456 | `		 * transient SyBlob allocation. */` |
|     36623 | 12457 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24412 | 12458 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24417 | 12459 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24417 | 12460 | `		SyBlobRelease(&sResolved);` |
|     24417 | 12461 | `		if( zDup == 0 ){` |
|       ! 0 | 12462 | `			goto Mem;` |
|         - | 12463 | `		}` |
|     24417 | 12464 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24417 | 12465 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12466 | `			goto Mem;` |
|         - | 12467 | `		}` |
|         - | 12468 | `		/* Check for '\|' (multi-catch separator) */` |
|     24412 | 12469 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24412 | 12470 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        33 | 12471 | `			pGen->pIn->sData.nByte == 1 &&` |
|        28 | 12472 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        30 | 12473 | `			pGen->pIn++; /* Consume the '\|' */` |
|        30 | 12474 | `			continue;` |
|         - | 12475 | `		}` |
|     24389 | 12476 | `		break;` |
|       ! 0 | 12477 | `	}` |
|     24384 | 12478 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24389 | 12479 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12480 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12481 | `			pToken = pGen->pIn;` |
|       ! 0 | 12482 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12483 | `				pToken--;` |
|       ! 0 | 12484 | `			}` |
|       ! 0 | 12485 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12486 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12487 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12488 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12489 | `				return SXERR_ABORT;` |
|         - | 12490 | `			}` |
|       ! 0 | 12491 | `			return SXERR_INVALID;` |
|         - | 12492 | `	}` |
|     24389 | 12493 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12494 | `	/* Duplicate instance name */` |
|     24389 | 12495 | `	pName = &pGen->pIn->sData;` |
|     24389 | 12496 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24389 | 12497 | `	if( zDup == 0 ){` |
|       ! 0 | 12498 | `		goto Mem;` |
|         - | 12499 | `	}` |
|     24389 | 12500 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24389 | 12501 | `	pGen->pIn++;` |
|     24389 | 12502 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12503 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12504 | `		pToken = pGen->pIn;` |
|       ! 0 | 12505 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12506 | `			pToken--;` |
|       ! 0 | 12507 | `		}` |
|       ! 0 | 12508 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12509 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12510 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12511 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12512 | `			return SXERR_ABORT;` |
|         - | 12513 | `		}` |
|       ! 0 | 12514 | `		return SXERR_INVALID;` |
|         - | 12515 | `	}` |
|         - | 12516 | `	/* Compile the block */` |
|     24389 | 12517 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12518 | `	/* Create the catch block */` |
|     24389 | 12519 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24389 | 12520 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12521 | `		return SXERR_ABORT;` |
|         - | 12522 | `	}` |
|         - | 12523 | `	/* Swap bytecode container */` |
|     24389 | 12524 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24389 | 12525 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12526 | `	/* Compile the block */` |
|     24389 | 12527 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12528 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24389 | 12529 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12530 | `	/* Emit the DONE instruction */` |
|     24389 | 12531 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12532 | `	/* Leave the block */` |
|     24389 | 12533 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12534 | `	/* Restore the default container */` |
|     24389 | 12535 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12536 | `	/* Install the catch block */` |
|     24389 | 12537 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24389 | 12538 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12539 | `		goto Mem;` |
|         - | 12540 | `	}` |
|     24389 | 12541 | `	return SXRET_OK;` |
|       ! 0 | 12542 | `Mem:` |
|       ! 0 | 12543 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12544 | `	return SXERR_ABORT;` |
|     12199 | 12545 | `}` |
|         - | 12546 | `/*` |
|         - | 12547 | ` * Compile a 'try' block.` |
|         - | 12548 | ` * A function using an exception should be in a "try" block.` |
|         - | 12549 | ` * If the exception does not trigger, the code will continue` |
|         - | 12550 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12551 | ` * is "thrown".` |
|         - | 12552 | ` */` |
|     24544 | 12553 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12554 | `{` |
|         - | 12555 | `	ph7_exception *pException;` |
|     24549 | 12556 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12557 | `	GenBlock *pTry;` |
|         - | 12558 | `	sxu32 nJmpIdx;` |
|         - | 12559 | `	sxi32 rc;` |
|         - | 12560 | `	/* Create the exception container */` |
|     24549 | 12561 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24549 | 12562 | `	if( pException == 0 ){` |
|       ! 0 | 12563 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12564 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12565 | `		return SXERR_ABORT;` |
|         - | 12566 | `	}` |
|         - | 12567 | `	/* Zero the structure */` |
|     24549 | 12568 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12569 | `	/* Initialize fields */` |
|     24549 | 12570 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24549 | 12571 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24549 | 12572 | `	pException->iHasFinally = 0;` |
|     24549 | 12573 | `	pException->iFinallyDone = 0;` |
|     24549 | 12574 | `	pException->pVm = pGen->pVm;` |
|         - | 12575 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12576 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12577 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12578 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12579 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12580 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24549 | 12581 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       103 | 12582 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12583 | `	}` |
|         - | 12584 | `	/* Create the try block */` |
|     24451 | 12585 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24451 | 12586 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12587 | `		return SXERR_ABORT;` |
|         - | 12588 | `	}` |
|         - | 12589 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24451 | 12590 | `	pTry->pUserData = pException;` |
|         - | 12591 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24451 | 12592 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12593 | `	/* Fix the jump later when the destination is resolved */` |
|     24451 | 12594 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24451 | 12595 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12596 | `	/* Compile the block */` |
|     24451 | 12597 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24451 | 12598 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12599 | `		return SXERR_ABORT;` |
|         - | 12600 | `	}` |
|         - | 12601 | `	/* Fix forward jumps now the destination is resolved */` |
|     24451 | 12602 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12603 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24451 | 12604 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12605 | `	/* Leave the block */` |
|     24451 | 12606 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12607 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24451 | 12608 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24444 | 12609 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12610 | `		/* Compile one or more catch blocks */` |
|     24384 | 12611 | `		for(;;){` |
|     48768 | 12612 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36631 | 12613 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12195 | 12614 | `					break;` |
|         - | 12615 | `			}` |
|     24393 | 12616 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24393 | 12617 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12618 | `				return SXERR_ABORT;` |
|         - | 12619 | `			}` |
|         5 | 12620 | `		}` |
|     12190 | 12621 | `	}` |
|         - | 12622 | `	/* Compile optional finally block */` |
|     24451 | 12623 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       730 | 12624 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12625 | `		SySet *pInstrContainer;` |
|         - | 12626 | `		GenBlock *pFinBlock;` |
|       129 | 12627 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12628 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12629 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12630 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12631 | `			return SXERR_ABORT;` |
|         - | 12632 | `		}` |
|         - | 12633 | `		/* Swap bytecode container */` |
|       129 | 12634 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12635 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12636 | `		/* Compile the finally body */` |
|       129 | 12637 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12638 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12639 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12640 | `			return SXERR_ABORT;` |
|         - | 12641 | `		}` |
|         - | 12642 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12643 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12644 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12645 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12646 | `		/* Leave the block */` |
|       129 | 12647 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12648 | `		/* Restore the default container */` |
|       129 | 12649 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12650 | `		pException->iHasFinally = 1;` |
|        62 | 12651 | `	}` |
|         - | 12652 | `	/* Must have at least one catch or finally */` |
|     24451 | 12653 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12654 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12655 | `			"Cannot use try without catch or finally");` |
|         8 | 12656 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12657 | `			return SXERR_ABORT;` |
|         - | 12658 | `		}` |
|         3 | 12659 | `	}` |
|     24451 | 12660 | `	return SXRET_OK;` |
|     12277 | 12661 | `}` |
|         - | 12662 | `/*` |
|         - | 12663 | ` * Compile a switch block.` |
|         - | 12664 | ` *  (See block-comment below for more information)` |
|         - | 12665 | ` */` |
|       112 | 12666 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12667 | `{` |
|       117 | 12668 | `	sxi32 rc = SXRET_OK;` |
|       117 | 12669 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12670 | `		/* Unexpected token */` |
|       ! 0 | 12671 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12672 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12673 | `			return SXERR_ABORT;` |
|         - | 12674 | `		}` |
|       ! 0 | 12675 | `		pGen->pIn++;` |
|       ! 0 | 12676 | `	}` |
|       117 | 12677 | `	pGen->pIn++;` |
|         - | 12678 | `	/* First instruction to execute in this block. */` |
|       117 | 12679 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12680 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12681 | `	 * or the '}' token */` |
|       206 | 12682 | `	for(;;){` |
|       417 | 12683 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12684 | `			/* No more input to process */` |
|       ! 0 | 12685 | `			break;` |
|         - | 12686 | `		}` |
|       417 | 12687 | `		rc = SXRET_OK;` |
|       417 | 12688 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        85 | 12689 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|        31 | 12690 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12691 | `					/* Unexpected token */` |
|       ! 0 | 12692 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12693 | `						&pGen->pIn->sData);` |
|       ! 0 | 12694 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12695 | `						return SXERR_ABORT;` |
|         - | 12696 | `					}` |
|         - | 12697 | `					/* FALL THROUGH */` |
|       ! 0 | 12698 | `				}` |
|        31 | 12699 | `				rc = SXERR_EOF;` |
|        31 | 12700 | `				break;` |
|         - | 12701 | `			}` |
|        32 | 12702 | `		}else{` |
|         - | 12703 | `			sxi32 nKwrd;` |
|         - | 12704 | `			/* Extract the keyword */` |
|       337 | 12705 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       337 | 12706 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|        47 | 12707 | `				break;` |
|         - | 12708 | `			}` |
|       253 | 12709 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12710 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12711 | `					/* Unexpected token */` |
|       ! 0 | 12712 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12713 | `						&pGen->pIn->sData);` |
|       ! 0 | 12714 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12715 | `						return SXERR_ABORT;` |
|         - | 12716 | `					}` |
|         - | 12717 | `					/* FALL THROUGH */` |
|       ! 0 | 12718 | `				}` |
|         - | 12719 | `				/* Block compiled */` |
|         3 | 12720 | `				break;` |
|         - | 12721 | `			}` |
|         - | 12722 | `		}` |
|         - | 12723 | `		/* Compile block */` |
|       305 | 12724 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       305 | 12725 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12726 | `			return SXERR_ABORT;` |
|         - | 12727 | `		}` |
|         5 | 12728 | `	}` |
|       117 | 12729 | `	return rc;` |
|        61 | 12730 | `}` |
|         - | 12731 | `/*` |
|         - | 12732 | ` * Compile a case eXpression.` |
|         - | 12733 | ` *  (See block-comment below for more information)` |
|         - | 12734 | ` */` |
|        92 | 12735 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12736 | `{` |
|         - | 12737 | `	SySet *pInstrContainer;` |
|         - | 12738 | `	SyToken *pEnd,*pTmp;` |
|        97 | 12739 | `	sxi32 iNest = 0;` |
|         - | 12740 | `	sxi32 rc;` |
|         - | 12741 | `	/* Delimit the expression */` |
|        97 | 12742 | `	pEnd = pGen->pIn;` |
|       197 | 12743 | `	while( pEnd < pGen->pEnd ){` |
|       197 | 12744 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12745 | `			/* Increment nesting level */` |
|         3 | 12746 | `			iNest++;` |
|       196 | 12747 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12748 | `			/* Decrement nesting level */` |
|         3 | 12749 | `			iNest--;` |
|       194 | 12750 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|        97 | 12751 | `			break;` |
|         - | 12752 | `		}` |
|       105 | 12753 | `		pEnd++;` |
|         5 | 12754 | `	}` |
|        97 | 12755 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12756 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12757 | `		if( rc == SXERR_ABORT ){` |
|         - | 12758 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12759 | `			return SXERR_ABORT;` |
|         - | 12760 | `		}` |
|       ! 0 | 12761 | `	}` |
|         - | 12762 | `	/* Swap token stream */` |
|        97 | 12763 | `	pTmp = pGen->pEnd;` |
|        97 | 12764 | `	pGen->pEnd = pEnd;` |
|        97 | 12765 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        97 | 12766 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|        97 | 12767 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12768 | `	/* Emit the done instruction */` |
|        97 | 12769 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        97 | 12770 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12771 | `	/* Update token stream */` |
|        97 | 12772 | `	pGen->pIn  = pEnd;` |
|        97 | 12773 | `	pGen->pEnd = pTmp;` |
|        97 | 12774 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12775 | `		return SXERR_ABORT;` |
|         - | 12776 | `	}` |
|        97 | 12777 | `	return SXRET_OK;` |
|        51 | 12778 | `}` |
|         - | 12779 | `/*` |
|         - | 12780 | ` * Compile the smart switch statement.` |
|         - | 12781 | ` * According to the PHP language reference manual` |
|         - | 12782 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12783 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12784 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12785 | ` *  This is exactly what the switch statement is for.` |
|         - | 12786 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12787 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12788 | ` *  of the outer loop, use continue 2.` |
|         - | 12789 | ` *  Note that switch/case does loose comparision.` |
|         - | 12790 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12791 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12792 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12793 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12794 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12795 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12796 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12797 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12798 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12799 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12800 | ` *  list for the next case.` |
|         - | 12801 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12802 | ` *  or floating-point numbers and strings.` |
|         - | 12803 | ` */` |
|        28 | 12804 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12805 | `{` |
|         - | 12806 | `	GenBlock *pSwitchBlock;` |
|         - | 12807 | `	SyToken *pTmp,*pEnd;` |
|         - | 12808 | `	ph7_switch *pSwitch;` |
|         - | 12809 | `	sxu32 nToken;` |
|         - | 12810 | `	sxu32 nLine;` |
|         - | 12811 | `	sxi32 rc;` |
|        33 | 12812 | `	nLine = pGen->pIn->nLine;` |
|         - | 12813 | `	/* Jump the 'switch' keyword */` |
|        33 | 12814 | `	pGen->pIn++;` |
|        33 | 12815 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12816 | `		/* Syntax error */` |
|       ! 0 | 12817 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12818 | `		if( rc == SXERR_ABORT ){` |
|         - | 12819 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12820 | `			return SXERR_ABORT;` |
|         - | 12821 | `		}` |
|       ! 0 | 12822 | `		goto Synchronize;` |
|         - | 12823 | `	}` |
|         - | 12824 | `	/* Jump the left parenthesis '(' */` |
|        33 | 12825 | `	pGen->pIn++;` |
|        33 | 12826 | `	pEnd = 0; /* cc warning */` |
|         - | 12827 | `	/* Create the loop block */` |
|        47 | 12828 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|        14 | 12829 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|        33 | 12830 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12831 | `		return SXERR_ABORT;` |
|         - | 12832 | `	}` |
|         - | 12833 | `	/* Delimit the condition */` |
|        33 | 12834 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|        33 | 12835 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12836 | `		/* Empty expression */` |
|       ! 0 | 12837 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12838 | `		if( rc == SXERR_ABORT ){` |
|         - | 12839 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12840 | `			return SXERR_ABORT;` |
|         - | 12841 | `		}` |
|       ! 0 | 12842 | `	}` |
|         - | 12843 | `	/* Swap token streams */` |
|        33 | 12844 | `	pTmp = pGen->pEnd;` |
|        33 | 12845 | `	pGen->pEnd = pEnd;` |
|         - | 12846 | `	/* Compile the expression */` |
|        33 | 12847 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        33 | 12848 | `	if( rc == SXERR_ABORT ){` |
|         - | 12849 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12850 | `		return SXERR_ABORT;` |
|         - | 12851 | `	}` |
|         - | 12852 | `	/* Update token stream */` |
|        33 | 12853 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12854 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12855 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12856 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12857 | `			return SXERR_ABORT;` |
|         - | 12858 | `		}` |
|       ! 0 | 12859 | `		pGen->pIn++;` |
|       ! 0 | 12860 | `	}` |
|        33 | 12861 | `	pGen->pIn  = &pEnd[1];` |
|        33 | 12862 | `	pGen->pEnd = pTmp;` |
|        33 | 12863 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        28 | 12864 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12865 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12866 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12867 | `				pTmp--;` |
|       ! 0 | 12868 | `			}` |
|         - | 12869 | `			/* Unexpected token */` |
|       ! 0 | 12870 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12871 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12872 | `				return SXERR_ABORT;` |
|         - | 12873 | `			}` |
|       ! 0 | 12874 | `			goto Synchronize;` |
|         - | 12875 | `	}` |
|         - | 12876 | `	/* Set the delimiter token */` |
|        33 | 12877 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12878 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12879 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12880 | `	}else{` |
|        31 | 12881 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12882 | `	}` |
|        33 | 12883 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12884 | `	/* Create the switch blocks container */` |
|        33 | 12885 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|        33 | 12886 | `	if( pSwitch == 0 ){` |
|         - | 12887 | `		/* Abort compilation */` |
|       ! 0 | 12888 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12889 | `		return SXERR_ABORT;` |
|         - | 12890 | `	}` |
|         - | 12891 | `	/* Zero the structure */` |
|        33 | 12892 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 12893 | `	/* Initialize fields */` |
|        33 | 12894 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 12895 | `	/* Emit the switch instruction */` |
|        33 | 12896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 12897 | `	/* Compile case blocks */` |
|       100 | 12898 | `	for(;;){` |
|         - | 12899 | `		sxu32 nKwrd;` |
|       119 | 12900 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12901 | `			/* No more input to process */` |
|       ! 0 | 12902 | `			break;` |
|         - | 12903 | `		}` |
|       119 | 12904 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 12905 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 12906 | `				/* Unexpected token */` |
|       ! 0 | 12907 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12908 | `					&pGen->pIn->sData);` |
|       ! 0 | 12909 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12910 | `					return SXERR_ABORT;` |
|         - | 12911 | `				}` |
|         - | 12912 | `				/* FALL THROUGH */` |
|       ! 0 | 12913 | `			}` |
|         - | 12914 | `			/* Block compiled */` |
|       ! 0 | 12915 | `			break;` |
|         - | 12916 | `		}` |
|         - | 12917 | `		/* Extract the keyword */` |
|       119 | 12918 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       119 | 12919 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12920 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 12921 | `				/* Unexpected token */` |
|       ! 0 | 12922 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12923 | `					&pGen->pIn->sData);` |
|       ! 0 | 12924 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12925 | `					return SXERR_ABORT;` |
|         - | 12926 | `				}` |
|         - | 12927 | `				/* FALL THROUGH */` |
|       ! 0 | 12928 | `			}` |
|         - | 12929 | `			/* Block compiled */` |
|         3 | 12930 | `			break;` |
|         - | 12931 | `		}` |
|       117 | 12932 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 12933 | `			/*` |
|         - | 12934 | `			 * Accroding to the PHP language reference manual` |
|         - | 12935 | `			 *  A special case is the default case. This case matches anything` |
|         - | 12936 | `			 *  that wasn't matched by the other cases.` |
|         - | 12937 | `			 */` |
|        25 | 12938 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 12939 | `				/* Default case already compiled */` |
|       ! 0 | 12940 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 12941 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12942 | `					return SXERR_ABORT;` |
|         - | 12943 | `				}` |
|       ! 0 | 12944 | `			}` |
|        25 | 12945 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 12946 | `			/* Compile the default block */` |
|        25 | 12947 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 12948 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12949 | `				return SXERR_ABORT;` |
|        25 | 12950 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 12951 | `				break;` |
|         1 | 12952 | `			}` |
|        98 | 12953 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 12954 | `			ph7_case_expr sCase;` |
|         - | 12955 | `			/* Standard case block */` |
|        97 | 12956 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 12957 | `			/* initialize the structure */` |
|        97 | 12958 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 12959 | `			/* Compile the case expression */` |
|        97 | 12960 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|        97 | 12961 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12962 | `				return SXERR_ABORT;` |
|         - | 12963 | `			}` |
|         - | 12964 | `			/* Compile the case block */` |
|        97 | 12965 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 12966 | `			/* Insert in the switch container */` |
|        97 | 12967 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|        97 | 12968 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12969 | `				return SXERR_ABORT;` |
|        97 | 12970 | `			}else if( rc == SXERR_EOF ){` |
|         9 | 12971 | `				break;` |
|         - | 12972 | `			}` |
|        47 | 12973 | `		}else{` |
|         - | 12974 | `			/* Unexpected token */` |
|       ! 0 | 12975 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12976 | `				&pGen->pIn->sData);` |
|       ! 0 | 12977 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12978 | `				return SXERR_ABORT;` |
|         - | 12979 | `			}` |
|       ! 0 | 12980 | `			break;` |
|         - | 12981 | `		}` |
|         5 | 12982 | `	}` |
|         - | 12983 | `	/* Fix all jumps now the destination is resolved */` |
|        33 | 12984 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|        33 | 12985 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12986 | `	/* Release the loop block */` |
|        33 | 12987 | `	GenStateLeaveBlock(pGen,0);` |
|        33 | 12988 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 12989 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|        33 | 12990 | `		pGen->pIn++;` |
|        14 | 12991 | `	}` |
|         - | 12992 | `	/* Statement successfully compiled */` |
|        33 | 12993 | `	return SXRET_OK;` |
|       ! 0 | 12994 | `Synchronize:` |
|         - | 12995 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 12996 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 12997 | `		pGen->pIn++;` |
|       ! 0 | 12998 | `	}` |
|       ! 0 | 12999 | `	return SXRET_OK;` |
|        19 | 13000 | `}` |
|         - | 13001 | `/*` |
|         - | 13002 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 13003 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 13004 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 13005 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 13006 | ` */` |
|         - | 13007 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 13008 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 13009 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 13010 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 13011 |  |
|         - | 13012 | `/*` |
|         - | 13013 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 13014 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 13015 | ` * patched entries from the pending set.` |
|         - | 13016 | ` */` |
|  46459952 | 13017 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 13018 | `{` |
|  46459957 | 13019 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 13020 | `	sxu32 nTarget;` |
|         - | 13021 | `	sxu32 *aIdx;` |
|         - | 13022 | `	sxu32 i;` |
|  46459957 | 13023 | `	if( nCur <= nBaseline ){` |
|  46459861 | 13024 | `		return;` |
|         - | 13025 | `	}` |
|       100 | 13026 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 13027 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 13028 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 13029 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 13030 | `		if( pInstr ){` |
|       108 | 13031 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 13032 | `		}` |
|        56 | 13033 | `	}` |
|       100 | 13034 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  23229981 | 13035 | `}` |
|         - | 13036 |  |
|         - | 13037 | `/*` |
|         - | 13038 | ` * By-reference out-parameters of builtin functions.` |
|         - | 13039 | ` *` |
|         - | 13040 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 13041 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 13042 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 13043 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 13044 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 13045 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 13046 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 13047 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 13048 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 13049 | ` * creates it" behaviour).` |
|         - | 13050 | ` *` |
|         - | 13051 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 13052 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 13053 | ` */` |
|   6025830 | 13054 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 13055 | `{` |
|         - | 13056 | `	static const struct {` |
|         - | 13057 | `		const char *zName;` |
|         - | 13058 | `		sxu32 nByte;` |
|         - | 13059 | `		sxu32 mask;` |
|         - | 13060 | `	} aByRef[] = {` |
|         - | 13061 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - | 13062 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13063 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13064 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13065 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13066 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 13067 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 13068 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 13069 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 13070 | `	};` |
|         - | 13071 | `	sxu32 i;` |
|   6025835 | 13072 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1605741 | 13073 | `		return 0;` |
|         - | 13074 | `	}` |
|  43784527 | 13075 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  39421752 | 13076 | `		if( pName->nByte == aByRef[i].nByte` |
|  20771470 | 13077 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     57329 | 13078 | `			return aByRef[i].mask;` |
|         - | 13079 | `		}` |
|  19682219 | 13080 | `	}` |
|   4362775 | 13081 | `	return 0;` |
|   3012920 | 13082 | `}` |
|         - | 13083 | `/*` |
|         - | 13084 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 13085 | ` *` |
|         - | 13086 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 13087 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 13088 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 13089 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 13090 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 13091 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 13092 | ` */` |
|   6025830 | 13093 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 13094 | `{` |
|         - | 13095 | `	SyToken *p, *pEnd;` |
|   6025835 | 13096 | `	pOut->zString = 0;` |
|   6025835 | 13097 | `	pOut->nByte = 0;` |
|   6025835 | 13098 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 13099 | `		return;` |
|         - | 13100 | `	}` |
|   6025835 | 13101 | `	p = pLeft->pStart;` |
|   6025835 | 13102 | `	pEnd = pLeft->pEnd;` |
|         - | 13103 | `	/* Optional single leading namespace separator (absolute path). */` |
|   6025835 | 13104 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3845 | 13105 | `		p++;` |
|      1920 | 13106 | `	}` |
|   6025835 | 13107 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1605705 | 13108 | `		return;` |
|         - | 13109 | `	}` |
|         - | 13110 | `	/* Must be a single component: nothing follows the name token. */` |
|   4420135 | 13111 | `	if( p + 1 != pEnd ){` |
|        40 | 13112 | `		return;` |
|         - | 13113 | `	}` |
|   4420099 | 13114 | `	*pOut = p->sData;` |
|   3012920 | 13115 | `}` |
|         - | 13116 | `/*` |
|         - | 13117 | ` * Generate bytecode for a given expression tree.` |
|         - | 13118 | ` * If something goes wrong while generating bytecode` |
|         - | 13119 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 13120 | ` * this function takes care of generating the appropriate` |
|         - | 13121 | ` * error message.` |
|         - | 13122 | ` */` |
|  65094440 | 13123 | `static sxi32 GenStateEmitExprCode(` |
|         - | 13124 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 13125 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 13126 | `	sxi32 iFlags /* Control flags */` |
|         - | 13127 | `	)` |
|         5 | 13128 | `{` |
|         - | 13129 | `	VmInstr *pInstr;` |
|         - | 13130 | `	sxu32 nJmpIdx;` |
|  65094445 | 13131 | `	sxi32 iP1 = 0;` |
|  65094445 | 13132 | `	sxu32 iP2 = 0;` |
|  65094445 | 13133 | `	void *p3  = 0;` |
|         - | 13134 | `	sxi32 iVmOp;` |
|         - | 13135 | `	sxi32 rc;` |
|  65094445 | 13136 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  65094445 | 13137 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  65094445 | 13138 | `	sxu32 nRhsNsBase = 0;` |
|  65094445 | 13139 | `	if( pNode->xCode ){` |
|         - | 13140 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13141 | `		/* Compile node */` |
|  39151501 | 13142 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  39151501 | 13143 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  39151501 | 13144 | `		RE_SWAP_DELIMITER(pGen);` |
|  39151501 | 13145 | `		return rc;` |
|         - | 13146 | `	}` |
|  25942949 | 13147 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13148 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13149 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13150 | `		return SXERR_ABORT;` |
|         - | 13151 | `	}` |
|  25942949 | 13152 | `	iVmOp = pNode->pOp->iVmOp;` |
|  25942949 | 13153 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13154 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13155 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13156 | `		 * and later errors are still reported. */` |
|         3 | 13157 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13158 | `			"The (unset) cast is no longer supported");` |
|         3 | 13159 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13160 | `			return SXERR_ABORT;` |
|         - | 13161 | `		}` |
|         1 | 13162 | `	}` |
|  25942949 | 13163 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        91 | 13164 | `		sxu32 nJmp = 0;` |
|         - | 13165 | `		sxu32 nNcNsBase;` |
|         - | 13166 | `		VmInstr *pInstrFix;` |
|         - | 13167 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13168 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13169 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13170 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13171 | `		 * stack slot carries a writable nIdx. */` |
|        91 | 13172 | `		if( pNode->pRight ){` |
|        91 | 13173 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13174 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        91 | 13175 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13176 | `				return rc;` |
|         - | 13177 | `			}` |
|        91 | 13178 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13179 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13180 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13181 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13182 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13183 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13184 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13185 | `			 * cascade for the actual write path stays correct. */` |
|        91 | 13186 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        91 | 13187 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13188 | `				pInstrFix->iP2 = 3;` |
|        15 | 13189 | `			}` |
|        44 | 13190 | `		}` |
|         - | 13191 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        91 | 13192 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13193 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        91 | 13194 | `		if( pNode->pLeft ){` |
|        91 | 13195 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13196 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        91 | 13197 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13198 | `				return rc;` |
|         - | 13199 | `			}` |
|        91 | 13200 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        44 | 13201 | `		}` |
|         - | 13202 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        91 | 13203 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13204 | `		/* Patch the short-circuit jump to land after the store. */` |
|        91 | 13205 | `		if( nJmp > 0 ){` |
|        91 | 13206 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        91 | 13207 | `			if( pInstrFix ){` |
|        91 | 13208 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        44 | 13209 | `			}` |
|        44 | 13210 | `		}` |
|        91 | 13211 | `		return SXRET_OK;` |
|         - | 13212 | `	}` |
|  25942861 | 13213 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13214 | `		sxu32 nJz,nJmp;` |
|         - | 13215 | `		sxu32 nTernaryNsBase;` |
|         - | 13216 | `		/* Ternary operator require special handling */` |
|         - | 13217 | `		/* Phase#1: Compile the condition */` |
|    430259 | 13218 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    430259 | 13219 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    430259 | 13220 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13221 | `			return rc;` |
|         - | 13222 | `		}` |
|         - | 13223 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13224 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13225 | `		 * condition expression, not leak past the ternary. */` |
|    430259 | 13226 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    430259 | 13227 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    430259 | 13228 | `		if( pNode->pLeft ){` |
|         - | 13229 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13230 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    426379 | 13231 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13232 | `			/* Phase#3: Compile the 'then' expression  */` |
|    426379 | 13233 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    426379 | 13234 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    426379 | 13235 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13236 | `				return rc;` |
|         - | 13237 | `			}` |
|    426379 | 13238 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    213192 | 13239 | `		}else{` |
|         - | 13240 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13241 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13242 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3885 | 13243 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3885 | 13244 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13245 | `		}` |
|         - | 13246 | `		/* Phase#4: Emit the unconditional jump */` |
|    430259 | 13247 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13248 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    430259 | 13249 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    430259 | 13250 | `		if( pInstr ){` |
|    430259 | 13251 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    215127 | 13252 | `		}` |
|    430259 | 13253 | `		if( !pNode->pLeft ){` |
|         - | 13254 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3885 | 13255 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1940 | 13256 | `		}` |
|         - | 13257 | `		/* Phase#6: Compile the 'else' expression */` |
|    430259 | 13258 | `		if( pNode->pRight ){` |
|    430259 | 13259 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    430259 | 13260 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    430259 | 13261 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13262 | `				return rc;` |
|         - | 13263 | `			}` |
|    430259 | 13264 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    215127 | 13265 | `		}` |
|    430259 | 13266 | `		if( nJmp > 0 ){` |
|         - | 13267 | `			/* Phase#7: Fix the unconditional jump */` |
|    430259 | 13268 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    430259 | 13269 | `			if( pInstr ){` |
|    430259 | 13270 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    215127 | 13271 | `			}` |
|    215127 | 13272 | `		}` |
|         - | 13273 | `		/* All done */` |
|    430259 | 13274 | `		return SXRET_OK;` |
|         - | 13275 | `	}` |
|  25512607 | 13276 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13277 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13278 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13279 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13280 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13281 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13282 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13283 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13284 | `		sxu32 nPipeNsBase;` |
|        27 | 13285 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13286 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13287 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13288 | `				"'\|>': Missing operand");` |
|       ! 0 | 13289 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13290 | `		}` |
|         - | 13291 | `		/* Argument: the LHS value. */` |
|        27 | 13292 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13293 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13294 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13295 | `			return rc;` |
|         - | 13296 | `		}` |
|        27 | 13297 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13298 | `		/* Callable: the RHS. */` |
|        27 | 13299 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13300 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13301 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13302 | `			return rc;` |
|         - | 13303 | `		}` |
|        27 | 13304 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13305 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13306 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13307 | `		return SXRET_OK;` |
|         - | 13308 | `	}` |
|  25512581 | 13309 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13310 | `	/* Generate code for the left tree */` |
|  25512581 | 13311 | `	if( pNode->pLeft ){` |
|  25489711 | 13312 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  25489711 | 13313 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13314 | `			ph7_expr_node **apNode;` |
|   6029963 | 13315 | `			int hasSpread = 0;` |
|   6029963 | 13316 | `			int hasNamed = 0;` |
|   6029963 | 13317 | `			int bAnySpread = 0;` |
|   6029963 | 13318 | `			sxu32 byRefMask = 0;` |
|         - | 13319 | `			sxi32 nArgs;` |
|         - | 13320 | `			sxi32 n;` |
|         - | 13321 | `			/* Recurse and generate bytecodes for function arguments */` |
|   6029963 | 13322 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6029963 | 13323 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13324 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13325 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13326 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   6029963 | 13327 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13328 | `				bFcc = 1;` |
|        81 | 13329 | `				nArgs = 0;` |
|        40 | 13330 | `			}` |
|         - | 13331 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13332 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13333 | `			{` |
|   6029963 | 13334 | `				int seenNamed = 0;` |
|   6029963 | 13335 | `				int seenSpread = 0;` |
|  12664839 | 13336 | `				for( n = 0; n < nArgs; ++n ){` |
|   6634883 | 13337 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4001 | 13338 | `						bAnySpread = 1;` |
|      4001 | 13339 | `						seenSpread = 1;` |
|      4001 | 13340 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13341 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13342 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13343 | `							return SXERR_SYNTAX;` |
|         5 | 13344 | `						}` |
|   6632885 | 13345 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13346 | `						seenNamed = 1;` |
|       289 | 13347 | `						hasNamed = 1;` |
|   6630745 | 13348 | `					}else if( seenNamed ){` |
|         3 | 13349 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13350 | `							"Cannot use positional argument after named argument");` |
|         3 | 13351 | `						return SXERR_SYNTAX;` |
|   6630601 | 13352 | `					}else if( seenSpread ){` |
|       ! 0 | 13353 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13354 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13355 | `						return SXERR_SYNTAX;` |
|         - | 13356 | `					}` |
|   3317443 | 13357 | `				}` |
|         - | 13358 | `			}` |
|         - | 13359 | `			/* Read-only load */` |
|   6029961 | 13360 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13361 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13362 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13363 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13364 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   6029961 | 13365 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   6029961 | 13366 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   6029956 | 13367 | `				if( pCallName->nByte == 5` |
|   3380433 | 13368 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    301525 | 13369 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5879201 | 13370 | `				}else if( pCallName->nByte == 5` |
|   3078913 | 13371 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       109 | 13372 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        52 | 13373 | `				}` |
|         - | 13374 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13375 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13376 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13377 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13378 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13379 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   6029961 | 13380 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13381 | `					SyString sBuiltin;` |
|   6025835 | 13382 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   6025835 | 13383 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3012915 | 13384 | `				}` |
|   3014978 | 13385 | `			}` |
|  12664835 | 13386 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   6634879 | 13387 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   6634879 | 13388 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13389 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13390 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13391 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13392 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13393 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13394 | `				 * (iP1=0 either way). */` |
|   6634879 | 13395 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38205 | 13396 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38205 | 13397 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19100 | 13398 | `				}` |
|   6634879 | 13399 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   6634879 | 13400 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13401 | `					return rc;` |
|         - | 13402 | `				}` |
|         - | 13403 | `				/* Each argument is an independent nullsafe scope. */` |
|   6634879 | 13404 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   6634879 | 13405 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13406 | `					/* Emit spread opcode to unpack this array argument */` |
|      4001 | 13407 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4001 | 13408 | `					hasSpread = 1;` |
|      1998 | 13409 | `				}` |
|   3317442 | 13410 | `			}` |
|         - | 13411 | `			/* Total number of given arguments */` |
|   6029961 | 13412 | `			iP1 = nArgs;` |
|   6029961 | 13413 | `			iP2 = hasSpread;` |
|         - | 13414 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13415 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   6029961 | 13416 | `			if( hasNamed ){` |
|       178 | 13417 | `				sxu32 nStrBytes = 0;` |
|         - | 13418 | `				char *zBuf;` |
|       534 | 13419 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13420 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13421 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13422 | `					}` |
|       182 | 13423 | `				}` |
|         - | 13424 | `				{` |
|       178 | 13425 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13426 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13427 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13428 | `				if( pMap ){` |
|       178 | 13429 | `					SyZero(pMap, mapSize);` |
|       178 | 13430 | `					pMap->bHasNamed = 1;` |
|       178 | 13431 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13432 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13433 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13434 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13435 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13436 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13437 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13438 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13439 | `							zBuf += nb;` |
|       141 | 13440 | `						}` |
|         - | 13441 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13442 | `					}` |
|       178 | 13443 | `					p3 = (void *)pMap;` |
|        87 | 13444 | `				}` |
|         - | 13445 | `				}` |
|        87 | 13446 | `			}` |
|         - | 13447 | `			/* Remove stale flags now */` |
|   6029961 | 13448 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3014978 | 13449 | `		}` |
|         - | 13450 | `		{` |
|         - | 13451 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13452 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13453 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13454 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13455 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13456 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13457 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13458 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  25489709 | 13459 | `			sxi32 iLeftFlags = iFlags;` |
|  25489704 | 13460 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  20966750 | 13461 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   8221924 | 13462 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7104801 | 13463 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2406291 | 13464 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1203143 | 13465 | `			}` |
|         - | 13466 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13467 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13468 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13469 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13470 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13471 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13472 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  25489704 | 13473 | `			if( pNode->pOp` |
|  35972869 | 13474 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  23228064 | 13475 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  20966372 | 13476 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4882603 | 13477 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2441299 | 13478 | `			}` |
|         - | 13479 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13480 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13481 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13482 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13483 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13484 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  25489704 | 13485 | `			if( pNode->pOp` |
|  25489709 | 13486 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    187287 | 13487 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     93641 | 13488 | `			}` |
|  25489709 | 13489 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13490 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13491 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     11649 | 13492 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      5822 | 13493 | `			}` |
|  25489709 | 13494 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13495 | `		}` |
|  25489709 | 13496 | `		if( rc != SXRET_OK ){` |
|        34 | 13497 | `			return rc;` |
|         - | 13498 | `		}` |
|  25489679 | 13499 | `		if( !bIsChainOp ){` |
|         - | 13500 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13501 | `			 * target the end of that LHS chain, which is right here. */` |
|  11760631 | 13502 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   5880313 | 13503 | `		}` |
|  25489679 | 13504 | `		if( iVmOp == PH7_OP_CALL ){` |
|   6029961 | 13505 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6029961 | 13506 | `			if( pInstr ){` |
|   6029961 | 13507 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   4420375 | 13508 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13509 | `					sxu32 nQual;` |
|   4420375 | 13510 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13511 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13512 | `					 * so the later NEW handler (if any) can see it. */` |
|   4420375 | 13513 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13514 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13515 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13516 | `					 * imports — class imports must NOT affect function` |
|         - | 13517 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13518 | `					 * before NEW; we store the original literal index in the` |
|         - | 13519 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13520 | `					 * the unqualified name and re-qualify with class imports. */` |
|   4420375 | 13521 | `					if( bAbsolute ){` |
|      3845 | 13522 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1925 | 13523 | `					}else{` |
|   4416535 | 13524 | `						int fromImport = 0;` |
|   4416535 | 13525 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   4416535 | 13526 | `						pInstr->iP2 = (sxi32)nQual;` |
|   4416535 | 13527 | `						if( nQual != nOrig ){` |
|         - | 13528 | `							/* Record the original literal index in the arg map` |
|         - | 13529 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13530 | `							 * flag) so the NEW handler can recover the` |
|         - | 13531 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13532 | `							 * imports. */` |
|        77 | 13533 | `							if( p3 == 0 ){` |
|        77 | 13534 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        72 | 13535 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        77 | 13536 | `								if( pMap ){` |
|        77 | 13537 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        77 | 13538 | `									p3 = (void *)pMap;` |
|        36 | 13539 | `								}` |
|        36 | 13540 | `							}` |
|        77 | 13541 | `							if( p3 ){` |
|        77 | 13542 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        77 | 13543 | `								if( !fromImport ){` |
|         - | 13544 | `									/* Mark as namespace-qualified */` |
|        67 | 13545 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        31 | 13546 | `								}` |
|        36 | 13547 | `							}` |
|        36 | 13548 | `						}` |
|         5 | 13549 | `					}` |
|   3819776 | 13550 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13551 | `					/* Method call,flag that */` |
|   1589877 | 13552 | `					pInstr->iP2 = 1;` |
|    794936 | 13553 | `				}` |
|   3014983 | 13554 | `			}` |
|  22474701 | 13555 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13556 | `			ph7_expr_node **apNode;` |
|         - | 13557 | `			sxi32 n;` |
|   2816499 | 13558 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13559 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13560 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13561 | `			/* Recurse and generate bytecodes for array index */` |
|   2816499 | 13562 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5415401 | 13563 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2598907 | 13564 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2598907 | 13565 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2598907 | 13566 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13567 | `					return rc;` |
|         - | 13568 | `				}` |
|         - | 13569 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2598907 | 13570 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1299456 | 13571 | `			}` |
|   2816499 | 13572 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2598907 | 13573 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1299451 | 13574 | `			}` |
|   2816499 | 13575 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13576 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    339529 | 13577 | `				iP2 = 4;` |
|   2646737 | 13578 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13579 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13580 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     22953 | 13581 | `				iP2 = 5;` |
|   2465501 | 13582 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13583 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13584 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13585 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        31 | 13586 | `				iP2 = 6;` |
|   2454014 | 13587 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13588 | `				/* Create an empty entry when the desired index is not found */` |
|    519285 | 13589 | `				iP2 = 1;` |
|    259645 | 13590 | `			}` |
|  18051476 | 13591 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13592 | `			/* POP the left node */` |
|         5 | 13593 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13594 | `		}` |
|  12744837 | 13595 | `	}` |
|  25512549 | 13596 | `	rc = SXRET_OK;` |
|  25512549 | 13597 | `	nJmpIdx = 0;` |
|         - | 13598 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13599 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13600 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  25512549 | 13601 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    382089 | 13602 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    382089 | 13603 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    382089 | 13604 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    382089 | 13605 | `			int isSpecial = 0;` |
|    382089 | 13606 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    336321 | 13607 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    336321 | 13608 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    336316 | 13609 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    305756 | 13610 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    166222 | 13611 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    103073 | 13612 | `					isSpecial = 1;` |
|     51534 | 13613 | `				}` |
|    179600 | 13614 | `			}` |
|    404973 | 13615 | `			pInstr->iP1 = 0;` |
|    404973 | 13616 | `			if( !isSpecial ){` |
|    256137 | 13617 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    128066 | 13618 | `			}` |
|         - | 13619 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13620 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    359205 | 13621 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    256137 | 13622 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    256137 | 13623 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        60 | 13624 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        62 | 13625 | `					return SXRET_OK;` |
|         - | 13626 | `				}` |
|    128037 | 13627 | `			}` |
|    179571 | 13628 | `		}` |
|    225318 | 13629 | `	}` |
|         - | 13630 | `	/* Generate code for the right tree */` |
|  25489621 | 13631 | `	if( pNode->pRight ){` |
|  14579345 | 13632 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13633 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    374103 | 13634 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14392296 | 13635 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13636 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    267011 | 13637 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14071744 | 13638 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13639 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     53529 | 13640 | `			iVmOp = 0; /* No binary operator to emit */` |
|     53529 | 13641 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  13911531 | 13642 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13643 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13644 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13645 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13646 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13647 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13648 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13649 | `			sxu32 nNsJmp = 0;` |
|       108 | 13650 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13651 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  13884665 | 13652 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13653 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13654 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13655 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4616785 | 13656 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2308390 | 13657 | `		}` |
|  14579345 | 13658 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  14579345 | 13659 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  14579345 | 13660 | `		if( !bIsChainOp ){` |
|         - | 13661 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13662 | `			 * operator instruction is emitted. */` |
|   9696805 | 13663 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   4848400 | 13664 | `		}` |
|  14579345 | 13665 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4193323 | 13666 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4193286 | 13667 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13668 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13669 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13670 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13671 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13672 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13673 | `				 */` |
|        91 | 13674 | `				iVmOp = 0;` |
|   4193280 | 13675 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4193237 | 13676 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13677 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    744053 | 13678 | `					iP2 = 1;` |
|    372029 | 13679 | `				}else{` |
|   3449189 | 13680 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13681 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    500127 | 13682 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    500127 | 13683 | `						iP1 = pInstr->iP1;` |
|    250066 | 13684 | `					}else{` |
|   2949067 | 13685 | `						p3 = pInstr->p3;` |
|         - | 13686 | `					}` |
|         - | 13687 | `					/* POP the last dynamic load instruction */` |
|   3449189 | 13688 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13689 | `				}` |
|   2096621 | 13690 | `			}` |
|  12482686 | 13691 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13692 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13693 | `			if( pInstr ){` |
|        63 | 13694 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13695 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13696 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13697 | `					 */` |
|        19 | 13698 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13699 | `					iP1 = pInstr->iP1;` |
|        19 | 13700 | `					iP2 = pInstr->iP2;` |
|        19 | 13701 | `					p3  = pInstr->p3;` |
|        10 | 13702 | `				}else{` |
|        45 | 13703 | `					p3 = pInstr->p3;` |
|         - | 13704 | `				}` |
|        30 | 13705 | `			}` |
|        30 | 13706 | `		}` |
|   7289670 | 13707 | `	}` |
|  25489616 | 13708 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    375213 | 13709 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13710 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13711 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13712 | `		iVmOp = 0;` |
|        14 | 13713 | `	}` |
|  25489621 | 13714 | `	if( iVmOp > 0 ){` |
|  25435979 | 13715 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    187287 | 13716 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13717 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15285 | 13718 | `				iP1 = 1;` |
|      7645 | 13719 | `			}` |
|  25342338 | 13720 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13721 | `			/* Namespace-qualify the class name for NEW */ {` |
|    750055 | 13722 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    750055 | 13723 | `				VmInstr *pCallInstr = 0;` |
|    750055 | 13724 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    749745 | 13725 | `					pCallInstr = pPeek;` |
|    749745 | 13726 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    374870 | 13727 | `				}` |
|    750055 | 13728 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    734803 | 13729 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13730 | `					sxu32 nLitForClass;` |
|    734803 | 13731 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13732 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13733 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13734 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13735 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13736 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13737 | `					 * with class imports. */` |
|    734803 | 13738 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        37 | 13739 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        21 | 13740 | `					}else{` |
|    734771 | 13741 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13742 | `					}` |
|    734803 | 13743 | `					pPeek->iP1 = 0;` |
|    734803 | 13744 | `					if( !bAbsolute ){` |
|    730967 | 13745 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    365486 | 13746 | `					}else{` |
|      3841 | 13747 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13748 | `					}` |
|    367399 | 13749 | `				}` |
|         - | 13750 | `			}` |
|    750055 | 13751 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    750055 | 13752 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13753 | `				VmInstr *pPrev;` |
|    749745 | 13754 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    749745 | 13755 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13756 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13757 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13758 | `					 * accumulator exactly like OP_CALL would have). */` |
|    749745 | 13759 | `					iP1 = pInstr->iP1;` |
|    749745 | 13760 | `					iP2 = pInstr->iP2;` |
|    749745 | 13761 | `					if( pInstr->p3 ){` |
|        47 | 13762 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        21 | 13763 | `					}` |
|    749745 | 13764 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    374870 | 13765 | `				}` |
|    374875 | 13766 | `			}` |
|  24873672 | 13767 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13768 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13769 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     76497 | 13770 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     76497 | 13771 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     76497 | 13772 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     76497 | 13773 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     76497 | 13774 | `				int isSpecialIs = 0;` |
|     76497 | 13775 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     76497 | 13776 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     76497 | 13777 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     76492 | 13778 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     76495 | 13779 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     38246 | 13780 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13781 | `						isSpecialIs = 1;` |
|         5 | 13782 | `					}` |
|     38246 | 13783 | `				}` |
|     76497 | 13784 | `				pInstr->iP1 = 0;` |
|     76497 | 13785 | `				if( !isSpecialIs && !bAbsolute ){` |
|     76477 | 13786 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     38236 | 13787 | `				}` |
|     38251 | 13788 | `			}` |
|  24460401 | 13789 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13790 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13791 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13792 | `			 * should not trigger constant lookup. */` |
|   4882545 | 13793 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4882545 | 13794 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4649935 | 13795 | `				pInstr->iP1 = 0;` |
|   2324965 | 13796 | `			}` |
|   4882545 | 13797 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13798 | `				/* Static member access,remember that */` |
|    359161 | 13799 | `				iP1 = 1;` |
|    359161 | 13800 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    359161 | 13801 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    228787 | 13802 | `					p3 = pInstr->p3;` |
|    228787 | 13803 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    114391 | 13804 | `				}` |
|    179578 | 13805 | `			}` |
|         - | 13806 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13807 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13808 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13809 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4882545 | 13810 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4882545 | 13811 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13812 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4882525 | 13813 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     61095 | 13814 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4851960 | 13815 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13816 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4821407 | 13817 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13818 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    911965 | 13819 | `					iP2 = PH7_MEMBER_WRITE;` |
|    455980 | 13820 | `				}` |
|   2441270 | 13821 | `			}` |
|   2441270 | 13822 | `		}` |
|         - | 13823 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13824 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13825 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13826 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13827 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  25435979 | 13828 | `		if( bFcc ){` |
|        81 | 13829 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13830 | `			iP2 = 0;` |
|        81 | 13831 | `			p3 = 0;` |
|        81 | 13832 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13833 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13834 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13835 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13836 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13837 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13838 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13839 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13840 | `				if( pMemberName ){` |
|         3 | 13841 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13842 | `				}` |
|        37 | 13843 | `				iP1 = 2;` |
|        19 | 13844 | `			}else{` |
|        45 | 13845 | `				iP1 = 1;` |
|         - | 13846 | `			}` |
|        40 | 13847 | `		}` |
|         - | 13848 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13849 | `		 * This is the primary emit path for user-visible calls. */` |
|  25435979 | 13850 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6779931 | 13851 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3389963 | 13852 | `		}` |
|         - | 13853 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  25435979 | 13854 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  12717987 | 13855 | `	}` |
|  25489621 | 13856 | `	if( nJmpIdx > 0 ){` |
|         - | 13857 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    694633 | 13858 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    694633 | 13859 | `		if( pInstr ){` |
|    694633 | 13860 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    347314 | 13861 | `		}` |
|    347314 | 13862 | `	}` |
|  25489621 | 13863 | `	return rc;` |
|  32535790 | 13864 | `}` |
|         - | 13865 | `/*` |
|         - | 13866 | ` * Compile a PHP expression.` |
|         - | 13867 | ` * According to the PHP language reference manual:` |
|         - | 13868 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 13869 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 13870 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 13871 | ` *  is "anything that has a value".` |
|         - | 13872 | ` * If something goes wrong while compiling the expression,this` |
|         - | 13873 | ` * function takes care of generating the appropriate error` |
|         - | 13874 | ` * message.` |
|         - | 13875 | ` */` |
|         - | 13876 | `/*` |
|         - | 13877 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 13878 | ` *` |
|         - | 13879 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 13880 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 13881 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 13882 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 13883 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 13884 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 13885 | ` * except for() now reports php's parse error.` |
|         - | 13886 | ` */` |
| 215690390 | 13887 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 13888 | `{` |
|         - | 13889 | `	ph7_expr_node **apArg;` |
|         - | 13890 | `	sxu32 n;` |
| 215690395 | 13891 | `	if( pNode == 0 ){` |
| 151588069 | 13892 | `		return 0;` |
|         - | 13893 | `	}` |
|  64102331 | 13894 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 13895 | `		return 1;` |
|         - | 13896 | `	}` |
|  64102322 | 13897 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  64102323 | 13898 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 13899 | `		return 1;` |
|         - | 13900 | `	}` |
|  64102323 | 13901 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  73313307 | 13902 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   9210989 | 13903 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 13904 | `			return 1;` |
|         - | 13905 | `		}` |
|   4605497 | 13906 | `	}` |
|  64102323 | 13907 | `	return 0;` |
| 107845200 | 13908 | `}` |
|  14504708 | 13909 | `static sxi32 PH7_CompileExpr(` |
|         - | 13910 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 13911 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 13912 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 13913 | `	)` |
|         5 | 13914 | `{` |
|         - | 13915 | `	ph7_expr_node *pRoot;` |
|         - | 13916 | `	SySet sExprNode;` |
|         - | 13917 | `	SyToken *pEnd;` |
|         - | 13918 | `	sxi32 nExpr;` |
|         - | 13919 | `	sxi32 iNest;` |
|         - | 13920 | `	sxi32 rc;` |
|         - | 13921 | `	sxu32 nNullsafeBase;` |
|         - | 13922 | `	/* Initialize worker variables */` |
|  14504713 | 13923 | `	nExpr = 0;` |
|  14504713 | 13924 | `	pRoot = 0;` |
|         - | 13925 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 13926 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  14504713 | 13927 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  14504713 | 13928 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  14504713 | 13929 | `	SySetAlloc(&sExprNode,0x10);` |
|  14504713 | 13930 | `	rc = SXRET_OK;` |
|         - | 13931 | `	/* Delimit the expression */` |
|  14504713 | 13932 | `	pEnd = pGen->pIn;` |
|  14504713 | 13933 | `	iNest = 0;` |
| 114538551 | 13934 | `	while( pEnd < pGen->pEnd ){` |
| 109044181 | 13935 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 13936 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4631 | 13937 | `			iNest++;` |
| 109041868 | 13938 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4639 | 13939 | `			iNest--;` |
| 109037238 | 13940 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   9011195 | 13941 | `			if( iNest <= 0 ){` |
|   9010343 | 13942 | `				break;` |
|         - | 13943 | `			}` |
|       426 | 13944 | `		}` |
| 100033843 | 13945 | `		pEnd++;` |
|         5 | 13946 | `	}` |
|  14504713 | 13947 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    637621 | 13948 | `		SyToken *pEnd2 = pGen->pIn;` |
|    637621 | 13949 | `		iNest = 0;` |
|         - | 13950 | `		/* Stop at the first comma */` |
|   1390325 | 13951 | `		while( pEnd2 < pEnd ){` |
|    752711 | 13952 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     42041 | 13953 | `				iNest++;` |
|    731693 | 13954 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     42041 | 13955 | `				iNest--;` |
|    689657 | 13956 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|        63 | 13957 | `				if( iNest <= 0 ){` |
|         3 | 13958 | `					break;` |
|         - | 13959 | `				}` |
|        28 | 13960 | `			}` |
|    752709 | 13961 | `			pEnd2++;` |
|         5 | 13962 | `		}` |
|    637621 | 13963 | `		if( pEnd2 <pEnd ){` |
|         3 | 13964 | `			pEnd = pEnd2;` |
|         1 | 13965 | `		}` |
|    318808 | 13966 | `	}` |
|  14504713 | 13967 | `	if( pEnd > pGen->pIn ){` |
|  14481833 | 13968 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 13969 | `		/* Swap delimiter */` |
|  14481833 | 13970 | `		pGen->pEnd = pEnd;` |
|         - | 13971 | `		/* Try to get an expression tree */` |
|  14481833 | 13972 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  14481828 | 13973 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  14327051 | 13974 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 13975 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 13976 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 13977 | `				"syntax error, unexpected token \",\"");` |
|         6 | 13978 | `			pGen->pEnd = pTmp;` |
|         6 | 13979 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13980 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 13981 | `				return SXERR_ABORT;` |
|         - | 13982 | `			}` |
|         6 | 13983 | `			pGen->pIn = pEnd;` |
|         6 | 13984 | `			SySetRelease(&sExprNode);` |
|         6 | 13985 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 13986 | `			return SXRET_OK;` |
|         - | 13987 | `		}` |
|  14481829 | 13988 | `		if( rc == SXRET_OK && pRoot ){` |
|  14481645 | 13989 | `			rc = SXRET_OK;` |
|  14481645 | 13990 | `			if( xTreeValidator ){` |
|         - | 13991 | `				/* Call the upper layer validator callback */` |
|    958617 | 13992 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    479306 | 13993 | `			}` |
|  14481645 | 13994 | `			if( rc != SXERR_ABORT ){` |
|         - | 13995 | `				/* Generate code for the given tree */` |
|  14481645 | 13996 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 13997 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 13998 | `				 * expression so they short-circuit to its end. */` |
|  14481645 | 13999 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   7240820 | 14000 | `			}` |
|  14481645 | 14001 | `			nExpr = 1;` |
|   7240820 | 14002 | `		}` |
|         - | 14003 | `		/* Release the whole tree */` |
|  14481829 | 14004 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 14005 | `		/* Synchronize token stream */` |
|  14481829 | 14006 | `		pGen->pEnd = pTmp;` |
|  14481829 | 14007 | `		pGen->pIn  = pEnd;` |
|  14481829 | 14008 | `		if( rc == SXERR_ABORT ){` |
|        13 | 14009 | `			SySetRelease(&sExprNode);` |
|        13 | 14010 | `			return SXERR_ABORT;` |
|         - | 14011 | `		}` |
|   7240907 | 14012 | `	}` |
|  14504699 | 14013 | `	SySetRelease(&sExprNode);` |
|  14504699 | 14014 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   7252359 | 14015 | `}` |
|         - | 14016 | `/*` |
|         - | 14017 | ` * Return a pointer to the node construct handler associated` |
|         - | 14018 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 14019 | ` */` |
|   8271980 | 14020 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 14021 | `{` |
|   8271985 | 14022 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 14023 | `		/* Numeric literal: Either real or integer */` |
|   3359555 | 14024 | `		return PH7_CompileNumLiteral;` |
|   4912435 | 14025 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 14026 | `		/* Double quoted string */` |
|    119361 | 14027 | `		return PH7_CompileString;` |
|   4793079 | 14028 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 14029 | `		/* Single quoted string */` |
|   4792959 | 14030 | `		return PH7_CompileSimpleString;` |
|       124 | 14031 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 14032 | `		/* Heredoc */` |
|        70 | 14033 | `		return PH7_CompileHereDoc;` |
|        58 | 14034 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 14035 | `		/* Nowdoc */` |
|        51 | 14036 | `		return PH7_CompileNowDoc;` |
|         8 | 14037 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 14038 | `		/* Backtick quoted string */` |
|         6 | 14039 | `		return PH7_CompileBacktic;` |
|         - | 14040 | `	}` |
|         3 | 14041 | `	return 0;` |
|   4135995 | 14042 | `}` |
|         - | 14043 | `/*` |
|         - | 14044 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 14045 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 14046 | ` * in write context" parse error.` |
|         - | 14047 | ` */` |
|     22990 | 14048 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 14049 | `{` |
|         - | 14050 | `	sxi32 rc;` |
|     22995 | 14051 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     22993 | 14052 | `		return SXRET_OK;` |
|         - | 14053 | `	}` |
|         5 | 14054 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 14055 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 14056 | `		"Can't use nullsafe operator in write context");` |
|         3 | 14057 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11500 | 14058 | `}` |
|         - | 14059 | `/*` |
|         - | 14060 | ` * Compile an unset() statement.` |
|         - | 14061 | ` * unset($var, $arr[$key], ...);` |
|         - | 14062 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 14063 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 14064 | ` * parent array before extracting the element to unset.` |
|         - | 14065 | ` */` |
|     25838 | 14066 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 14067 | `{` |
|     25843 | 14068 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     25843 | 14069 | `	sxu32 nIdx = 0;` |
|         - | 14070 | `	SyString sName;` |
|         - | 14071 | `	sxi32 rc;` |
|         - | 14072 | `	/* Jump the 'unset' keyword */` |
|     25843 | 14073 | `	pGen->pIn++;` |
|         - | 14074 | `	/* Save delimiter */` |
|     25843 | 14075 | `	pTmp = pGen->pEnd;` |
|         - | 14076 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     25843 | 14077 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     25843 | 14078 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14079 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 14080 | `		SyToken *pClose;` |
|     25843 | 14081 | `		pGen->pIn++;   /* Skip '(' */` |
|     25843 | 14082 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     25843 | 14083 | `		pEnd = pClose; /* Stop at ')' */` |
|     12919 | 14084 | `	}` |
|     25843 | 14085 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 14086 | `	/* Resolve the 'unset' builtin name once */` |
|     25843 | 14087 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3817 | 14088 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3817 | 14089 | `		if( pObj == 0 ){` |
|       ! 0 | 14090 | `			return SXERR_ABORT;` |
|         - | 14091 | `		}` |
|      3817 | 14092 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3817 | 14093 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1906 | 14094 | `	}` |
|         - | 14095 | `	/* Compile each comma-separated argument */` |
|     55903 | 14096 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30065 | 14097 | `		if( pGen->pIn < pNext ){` |
|         - | 14098 | `			/*` |
|         - | 14099 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 14100 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 14101 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 14102 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 14103 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 14104 | `			 * already removes just the element/property.` |
|         - | 14105 | `			 */` |
|     30060 | 14106 | `			if( &pGen->pIn[2] == pNext` |
|     18565 | 14107 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      7075 | 14108 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 14109 | `				SyString *pVarName;` |
|     10607 | 14110 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      7068 | 14111 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      7073 | 14112 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      7073 | 14113 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 14114 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 14115 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 14116 | `					return SXERR_ABORT;` |
|         - | 14117 | `				}` |
|      7073 | 14118 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      7073 | 14119 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      7073 | 14120 | `				pGen->pIn = pNext;` |
|      7073 | 14121 | `				if( pGen->pIn < pEnd ){` |
|      4223 | 14122 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2109 | 14123 | `				}` |
|      7073 | 14124 | `				continue;` |
|         - | 14125 | `			}` |
|     22997 | 14126 | `			pGen->pEnd = pNext;` |
|     22997 | 14127 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 14128 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 14129 | `				GenStateUnsetValidator);` |
|     22997 | 14130 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14131 | `				return SXERR_ABORT;` |
|         - | 14132 | `			}` |
|     22997 | 14133 | `			if( rc != SXERR_EMPTY ){` |
|         - | 14134 | `				/* Emit call for this single argument */` |
|     22995 | 14135 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     22995 | 14136 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     22995 | 14137 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11495 | 14138 | `			}` |
|     11496 | 14139 | `		}` |
|         - | 14140 | `		/* Jump trailing commas */` |
|     23003 | 14141 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 14142 | `			pNext++;` |
|         1 | 14143 | `		}` |
|     22997 | 14144 | `		pGen->pIn = pNext;` |
|         5 | 14145 | `	}` |
|         - | 14146 | `	/* Skip past the closing ')' if present */` |
|     25843 | 14147 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     25843 | 14148 | `		pGen->pIn++;` |
|     12919 | 14149 | `	}` |
|         - | 14150 | `	/* Restore token stream */` |
|     25843 | 14151 | `	pGen->pEnd = pTmp;` |
|     25843 | 14152 | `	return SXRET_OK;` |
|     12924 | 14153 | `}` |
|         - | 14154 | `/*` |
|         - | 14155 | ` * PHP Language construct table.` |
|         - | 14156 | ` */` |
|         - | 14157 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 14158 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 14159 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14160 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14161 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14162 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14163 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14164 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14165 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14166 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14167 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14168 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14169 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14170 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14171 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14172 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14173 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14174 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14175 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14176 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14177 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14178 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14179 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14180 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14181 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14182 | `};` |
|         - | 14183 | `/*` |
|         - | 14184 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14185 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14186 | ` */` |
|   7141910 | 14187 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14188 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14189 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14190 | `	)` |
|         5 | 14191 | `{` |
|   7141915 | 14192 | `	sxu32 n = 0;` |
|  28400039 | 14193 | `	for(;;){` |
|  56800083 | 14194 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    429257 | 14195 | `			break;` |
|         - | 14196 | `		}` |
|  56370831 | 14197 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6712663 | 14198 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14199 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14200 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14201 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14202 | `					return 0;` |
|         - | 14203 | `				}` |
|       ! 0 | 14204 | `			}` |
|   6712658 | 14205 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|      3826 | 14206 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      1920 | 14207 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14208 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14209 | `				return 0;` |
|         - | 14210 | `			}` |
|         - | 14211 | `			/* Return a pointer to the handler.` |
|         - | 14212 | `			*/` |
|   6712661 | 14213 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14214 | `		}` |
|  49658173 | 14215 | `		n++;` |
|         5 | 14216 | `	}` |
|    429257 | 14217 | `	if( pLookahed ){` |
|    429257 | 14218 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     68729 | 14219 | `			return PH7_CompileClassInterface;` |
|    360533 | 14220 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    314241 | 14221 | `			return PH7_CompileClass;` |
|     46297 | 14222 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7707 | 14223 | `			return PH7_CompileTrait;` |
|         - | 14224 | `		}` |
|         - | 14225 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14226 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14227 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14228 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19295 | 14229 | `	}` |
|         - | 14230 | `	/* Not a language construct */` |
|     38595 | 14231 | `	return 0;` |
|   3570960 | 14232 | `}` |
|         - | 14233 | `/*` |
|         - | 14234 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14235 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14236 | ` */` |
|     38592 | 14237 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14238 | `{` |
|         - | 14239 | `	int rc;` |
|     38597 | 14240 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38597 | 14241 | `	if( rc == FALSE ){` |
|     38494 | 14242 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15614 | 14243 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14244 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14245 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14246 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14247 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14248 | `			*/` |
|         - | 14249 | `			){` |
|     38491 | 14250 | `				rc = TRUE;` |
|     19243 | 14251 | `		}` |
|     19247 | 14252 | `	}` |
|     38597 | 14253 | `	return rc;` |
|         5 | 14254 | `}` |
|         - | 14255 | `/*` |
|         - | 14256 | ` * Compile a PHP chunk.` |
|         - | 14257 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14258 | ` * takes care of generating the appropriate error message.` |
|         - | 14259 | ` */` |
|         - | 14260 | `/*` |
|         - | 14261 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14262 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14263 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14264 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14265 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14266 | ` * intervening non-declaration statements.` |
|         - | 14267 | ` */` |
|  15454034 | 14268 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14269 | `{` |
|  15454039 | 14270 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  15454039 | 14271 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  15454039 | 14272 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14273 | `	sxu32 nIdx, n;` |
|  15454034 | 14274 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3037261 | 14275 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14276 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14277 | `		 * indexes do not map to the sidecar */` |
|  12416785 | 14278 | `		return;` |
|         - | 14279 | `	}` |
|   3037259 | 14280 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14281 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14282 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3037259 | 14283 | `	SySetReset(&pGen->aPendingAttrs);` |
|   9113261 | 14284 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6076007 | 14285 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6068219 | 14286 | `			continue;` |
|         - | 14287 | `		}` |
|      7793 | 14288 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14289 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7781 | 14290 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7769 | 14291 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3882 | 14292 | `		}` |
|      3899 | 14293 | `	}` |
|   7727022 | 14294 | `}` |
|         - | 14295 | `/*` |
|         - | 14296 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14297 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14298 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14299 | ` */` |
|   4017072 | 14300 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14301 | `{` |
|         - | 14302 | `	char *zDup;` |
|   4017077 | 14303 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4017057 | 14304 | `		return;` |
|         - | 14305 | `	}` |
|        35 | 14306 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14307 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14308 | `	if( zDup ){` |
|        25 | 14309 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14310 | `	}` |
|        25 | 14311 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2008541 | 14312 | `}` |
|         - | 14313 | `/*` |
|         - | 14314 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14315 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14316 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14317 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14318 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14319 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14320 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14321 | ` */` |
|      7776 | 14322 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14323 | `{` |
|         - | 14324 | `	SySet *pToken;` |
|         - | 14325 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14326 | `	char *zSpan;` |
|      7781 | 14327 | `	sxi32 rc = SXRET_OK;` |
|      7781 | 14328 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14329 | `		return SXRET_OK;` |
|         - | 14330 | `	}` |
|     11669 | 14331 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3888 | 14332 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7781 | 14333 | `	if( zSpan == 0 ){` |
|       ! 0 | 14334 | `		return SXRET_OK;` |
|         - | 14335 | `	}` |
|         - | 14336 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14337 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14338 | `	 * the number of attribute declarations in the program. */` |
|      7781 | 14339 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7781 | 14340 | `	if( pToken == 0 ){` |
|       ! 0 | 14341 | `		return SXRET_OK;` |
|         - | 14342 | `	}` |
|      7781 | 14343 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7781 | 14344 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7781 | 14345 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7781 | 14346 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7781 | 14347 | `	pSavedIn = pGen->pIn;` |
|      7781 | 14348 | `	pSavedEnd = pGen->pEnd;` |
|      7785 | 14349 | `	while( pIn < pEnd ){` |
|         - | 14350 | `		ph7_attribute sAttr;` |
|         - | 14351 | `		SyBlob sFQN;` |
|      7785 | 14352 | `		int bAbsolute = 0;` |
|      7785 | 14353 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7785 | 14354 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7785 | 14355 | `		sAttr.nLine = pIn->nLine;` |
|      7785 | 14356 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14357 | `			bAbsolute = 1;` |
|        75 | 14358 | `			pIn++;` |
|        35 | 14359 | `		}` |
|      7785 | 14360 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7785 | 14361 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7785 | 14362 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7785 | 14363 | `			pIn++;` |
|      7785 | 14364 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14365 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14366 | `				pIn++;` |
|       ! 0 | 14367 | `				continue;` |
|         - | 14368 | `			}` |
|      7785 | 14369 | `			break;` |
|       ! 0 | 14370 | `		}` |
|      7785 | 14371 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14372 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14373 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14374 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14375 | `			break;` |
|         - | 14376 | `		}` |
|         - | 14377 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14378 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14379 | `		{` |
|      7785 | 14380 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7785 | 14381 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7785 | 14382 | `			char *zDup = 0;` |
|      7785 | 14383 | `			if( !bAbsolute ){` |
|      7715 | 14384 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7715 | 14385 | `				if( pImp ){` |
|       ! 0 | 14386 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14387 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14388 | `					if( zDup ){` |
|       ! 0 | 14389 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14390 | `					}` |
|      7715 | 14391 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14392 | `					SyBlob sTmp;` |
|       ! 0 | 14393 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14394 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14395 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14396 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14397 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14398 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14399 | `					if( zDup ){` |
|       ! 0 | 14400 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14401 | `					}` |
|       ! 0 | 14402 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14403 | `				}` |
|      3855 | 14404 | `			}` |
|      7785 | 14405 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7785 | 14406 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7785 | 14407 | `				if( zDup ){` |
|      7785 | 14408 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3890 | 14409 | `				}` |
|      3890 | 14410 | `			}` |
|         - | 14411 | `		}` |
|      7785 | 14412 | `		SyBlobRelease(&sFQN);` |
|      7785 | 14413 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14414 | `			SyToken *pArgsEnd;` |
|      7683 | 14415 | `			pIn++;` |
|      7683 | 14416 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15375 | 14417 | `			while( pIn < pArgsEnd ){` |
|      7697 | 14418 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7697 | 14419 | `				sxi32 iDepth = 0;` |
|         - | 14420 | `				ph7_attr_arg sArgRec;` |
|     76485 | 14421 | `				while( pArgStop < pArgsEnd ){` |
|     68809 | 14422 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14423 | `						iDepth++;` |
|     68804 | 14424 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14425 | `						iDepth--;` |
|     68794 | 14426 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14427 | `						break;` |
|         - | 14428 | `					}` |
|     68793 | 14429 | `					pArgStop++;` |
|         5 | 14430 | `				}` |
|      7697 | 14431 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7697 | 14432 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7692 | 14433 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7676 | 14434 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14435 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14436 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14437 | `					if( zN ){` |
|        19 | 14438 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14439 | `					}` |
|        19 | 14440 | `					pArgStart += 2;` |
|         9 | 14441 | `				}` |
|      7697 | 14442 | `				if( pArgStart < pArgStop ){` |
|         - | 14443 | `					SySet *pInstrContainer;` |
|      7697 | 14444 | `					pGen->pIn = pArgStart;` |
|      7697 | 14445 | `					pGen->pEnd = pArgStop;` |
|      7697 | 14446 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7697 | 14447 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7697 | 14448 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7697 | 14449 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7697 | 14450 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7697 | 14451 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14452 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14453 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14454 | `						return SXERR_ABORT;` |
|         - | 14455 | `					}` |
|      7697 | 14456 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3846 | 14457 | `				}` |
|      7697 | 14458 | `				pIn = pArgStop;` |
|      7697 | 14459 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14460 | `					pIn++;` |
|         8 | 14461 | `				}` |
|         5 | 14462 | `			}` |
|      7683 | 14463 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3839 | 14464 | `		}` |
|      7785 | 14465 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7785 | 14466 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14467 | `			pIn++;` |
|         5 | 14468 | `			continue;` |
|         - | 14469 | `		}` |
|      7781 | 14470 | `		break;` |
|       ! 0 | 14471 | `	}` |
|      7781 | 14472 | `	pGen->pIn = pSavedIn;` |
|      7781 | 14473 | `	pGen->pEnd = pSavedEnd;` |
|      7781 | 14474 | `	return SXRET_OK;` |
|      3893 | 14475 | `}` |
|         - | 14476 | `/*` |
|         - | 14477 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14478 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14479 | ` */` |
|   4017076 | 14480 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14481 | `{` |
|   4017081 | 14482 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14483 | `	sxu32 n;` |
|         - | 14484 | `	sxi32 rc;` |
|   4024845 | 14485 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7769 | 14486 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7769 | 14487 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14488 | `			return SXERR_ABORT;` |
|         - | 14489 | `		}` |
|      3887 | 14490 | `	}` |
|   4017081 | 14491 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4017081 | 14492 | `	return SXRET_OK;` |
|   2008543 | 14493 | `}` |
|         - | 14494 | `/*` |
|         - | 14495 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14496 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14497 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14498 | ` */` |
|   1990060 | 14499 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14500 | `{` |
|   1990065 | 14501 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   1990065 | 14502 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   1990065 | 14503 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14504 | `	sxu32 nIdx, n;` |
|         - | 14505 | `	sxi32 rc;` |
|   1990060 | 14506 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    518767 | 14507 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1471303 | 14508 | `		return SXRET_OK;` |
|         - | 14509 | `	}` |
|    518767 | 14510 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1556289 | 14511 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1037527 | 14512 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14513 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14514 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14515 | `				return SXERR_ABORT;` |
|         - | 14516 | `			}` |
|         6 | 14517 | `		}` |
|    518766 | 14518 | `	}` |
|    518767 | 14519 | `	return SXRET_OK;` |
|    995035 | 14520 | `}` |
|  11463354 | 14521 | `static sxi32 GenStateCompileChunk(` |
|         - | 14522 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14523 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14524 | `	)` |
|         5 | 14525 | `{` |
|         - | 14526 | `	ProcLangConstruct xCons;` |
|         - | 14527 | `	sxi32 rc;` |
|  11463359 | 14528 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   6653435 | 14529 | `	for(;;){` |
|  12385117 | 14530 | `		int bStmtIsDeclare = 0;` |
|  12385117 | 14531 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14532 | `			/* No more input to process */` |
|     67397 | 14533 | `			break;` |
|         - | 14534 | `		}` |
|         - | 14535 | `		/* Bind a directly-preceding docblock to this statement */` |
|  12317725 | 14536 | `		GenStateSetPendingDoc(&(*pGen));` |
|  12317725 | 14537 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14538 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14539 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14540 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14541 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14542 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7687 | 14543 | `			int bAttrTarget = 0;` |
|      7682 | 14544 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3875 | 14545 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7629 | 14546 | `				bAttrTarget = 1;` |
|      3871 | 14547 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14548 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14549 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14550 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14551 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14552 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14553 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14554 | `					bAttrTarget = 1;` |
|        29 | 14555 | `				}` |
|        29 | 14556 | `			}` |
|      7687 | 14557 | `			if( !bAttrTarget ){` |
|       ! 0 | 14558 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14559 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14560 | `					&pGen->pIn->sData);` |
|       ! 0 | 14561 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14562 | `					break;` |
|         - | 14563 | `				}` |
|       ! 0 | 14564 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14565 | `			}` |
|      3841 | 14566 | `		}` |
|         - | 14567 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14568 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  12317725 | 14569 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7176275 | 14570 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   7176275 | 14571 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14572 | `				bStmtIsDeclare = 1;` |
|        21 | 14573 | `			}` |
|   3588135 | 14574 | `		}` |
|  12317725 | 14575 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14576 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14577 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    921731 | 14578 | `			pGen->bStrictTypesLocked = 1;` |
|    460863 | 14579 | `		}` |
|  12317725 | 14580 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14581 | `			/* Compile block */` |
|      3835 | 14582 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3835 | 14583 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14584 | `				break;` |
|         - | 14585 | `			}` |
|      1920 | 14586 | `		}else{` |
|  12313895 | 14587 | `			xCons = 0;` |
|  12313895 | 14588 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14589 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14590 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14591 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34391 | 14592 | `				xCons = PH7_CompileClassModifiers;` |
|  12296702 | 14593 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14594 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14595 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3845 | 14596 | `				xCons = PH7_CompileEnum;` |
|  12277589 | 14597 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7141915 | 14598 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14599 | `				/* Try to extract a language construct handler */` |
|   7141915 | 14600 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   7141915 | 14601 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14602 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14603 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14604 | `						&pGen->pIn->sData);` |
|         9 | 14605 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14606 | `						break;` |
|         - | 14607 | `					}` |
|         - | 14608 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14609 | `					 * this erroneous statement.` |
|         - | 14610 | `					 */` |
|         9 | 14611 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14612 | `				}` |
|   8704714 | 14613 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    405299 | 14614 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14615 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14616 | `				xCons = PH7_CompileLabel;` |
|        56 | 14617 | `			}` |
|  12313895 | 14618 | `			if( xCons == 0 ){` |
|         - | 14619 | `				/* Assume an expression an try to compile it */` |
|   5172231 | 14620 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5172231 | 14621 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14622 | `					/* Pop l-value */` |
|   5172081 | 14623 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2586038 | 14624 | `				}` |
|   2586118 | 14625 | `			}else{` |
|         - | 14626 | `				/* Go compile the sucker */` |
|   7141669 | 14627 | `				rc = xCons(&(*pGen));` |
|         - | 14628 | `			}` |
|  12313895 | 14629 | `			if( rc == SXERR_ABORT ){` |
|         - | 14630 | `				/* Request to abort compilation */` |
|        13 | 14631 | `				break;` |
|         - | 14632 | `			}` |
|         - | 14633 | `		}` |
|         - | 14634 | `		/* Ignore trailing semi-colons ';' */` |
|  21085741 | 14635 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   8768031 | 14636 | `			pGen->pIn++;` |
|         5 | 14637 | `		}` |
|  12317715 | 14638 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14639 | `			/* Compile a single statement and return */` |
|  11395957 | 14640 | `			break;` |
|         - | 14641 | `		}` |
|         - | 14642 | `		/* LOOP ONE */` |
|         - | 14643 | `		/* LOOP TWO */` |
|         - | 14644 | `		/* LOOP THREE */` |
|         - | 14645 | `		/* LOOP FOUR */` |
|         5 | 14646 | `	}` |
|         - | 14647 | `	/* Return compilation status */` |
|  11463359 | 14648 | `	return rc;` |
|         5 | 14649 | `}` |
|         - | 14650 | `/*` |
|         - | 14651 | ` * Compile a Raw PHP chunk.` |
|         - | 14652 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14653 | ` * takes care of generating the appropriate error message.` |
|         - | 14654 | ` */` |
|     67404 | 14655 | `static sxi32 PH7_CompilePHP(` |
|         - | 14656 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14657 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14658 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14659 | `	)` |
|         5 | 14660 | `{` |
|     67409 | 14661 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14662 | `	sxi32 rc;` |
|         - | 14663 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67409 | 14664 | `	SySetReset(&(*pTokenSet));` |
|     67409 | 14665 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14666 | `	/* Mark as the default token set */` |
|     67409 | 14667 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14668 | `	/* Advance the stream cursor */` |
|     67409 | 14669 | `	pGen->pRawIn++;` |
|         - | 14670 | `	/* Tokenize the PHP chunk first */` |
|     67409 | 14671 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14672 | `	/* Point to the head and tail of the token stream. */` |
|     67409 | 14673 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67409 | 14674 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67409 | 14675 | `	if( is_expr ){` |
|       ! 0 | 14676 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14677 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14678 | `			/* A simple expression,compile it */` |
|       ! 0 | 14679 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14680 | `		}` |
|         - | 14681 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14682 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14683 | `		return SXRET_OK;` |
|         - | 14684 | `	}` |
|     67409 | 14685 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14686 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14687 | `		/*` |
|         - | 14688 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14689 | `		 * According to the PHP reference manual:` |
|         - | 14690 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14691 | `		 *  immediately follow` |
|         - | 14692 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14693 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14694 | `		 * Symisc extension:` |
|         - | 14695 | `		 *   This short syntax works with all PHP opening` |
|         - | 14696 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14697 | `		 *   only short tag.` |
|         - | 14698 | `		 */` |
|         - | 14699 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14700 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14701 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14702 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14703 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14704 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14705 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14706 | `		}` |
|         3 | 14707 | `		return SXRET_OK;` |
|         - | 14708 | `	}` |
|         - | 14709 | `	/* Compile the PHP chunk */` |
|     67407 | 14710 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14711 | `	/* Fix exceptions jumps */` |
|     67407 | 14712 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14713 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67407 | 14714 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14715 | `		rc = SXERR_ABORT;` |
|         1 | 14716 | `	}` |
|         - | 14717 | `	/* Reset container */` |
|     67407 | 14718 | `	SySetReset(&pGen->aGoto);` |
|     67407 | 14719 | `	SySetReset(&pGen->aLabel);` |
|     67407 | 14720 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14721 | `	/* Compilation result */` |
|     67407 | 14722 | `	return rc;` |
|     33707 | 14723 | `}` |
|         - | 14724 | `/*` |
|         - | 14725 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14726 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14727 | ` * This is the only compile interface exported from this file.` |
|         - | 14728 | ` */` |
|     70554 | 14729 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14730 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14731 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14732 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14733 | `	)` |
|         5 | 14734 | `{` |
|         - | 14735 | `	SySet aPhpToken,aRawToken;` |
|         - | 14736 | `	ph7_gen_state *pCodeGen;` |
|         - | 14737 | `	ph7_value *pRawObj;` |
|         - | 14738 | `	sxu32 nObjIdx;` |
|         - | 14739 | `	sxi32 nRawObj;` |
|         - | 14740 | `	int is_expr;` |
|         - | 14741 | `	sxi8 bSavedStrict;` |
|         - | 14742 | `	sxi8 bSavedStrictLocked;` |
|         - | 14743 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 14744 | `	sxi32 rc;` |
|     70559 | 14745 | `	if( pScript->nByte < 1 ){` |
|         - | 14746 | `		/* Nothing to compile */` |
|       ! 0 | 14747 | `		return PH7_OK;` |
|         - | 14748 | `	}` |
|         - | 14749 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14750 | `	 * file's flags so include/require restore them on return. */` |
|     70559 | 14751 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 14752 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 14753 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 14754 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 14755 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 14756 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 14757 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     70559 | 14758 | `	pSavedIn = pCodeGen->pIn;` |
|     70559 | 14759 | `	pSavedEnd = pCodeGen->pEnd;` |
|     70559 | 14760 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     70559 | 14761 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     70559 | 14762 | `	pCodeGen->bStrictTypes = 0;` |
|     70559 | 14763 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14764 | `	/* Initialize the tokens containers */` |
|     70559 | 14765 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70559 | 14766 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70559 | 14767 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     70559 | 14768 | `	is_expr = 0;` |
|     70559 | 14769 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14770 | `		SyToken sTmp;` |
|         - | 14771 | `		/* PHP only: -*/` |
|     57297 | 14772 | `		sTmp.nLine = 1;` |
|     57297 | 14773 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57297 | 14774 | `		sTmp.pUserData = 0;` |
|     57297 | 14775 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57297 | 14776 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57297 | 14777 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14778 | `			/* A simple PHP expression */` |
|       ! 0 | 14779 | `			is_expr = 1;` |
|       ! 0 | 14780 | `		}` |
|     28651 | 14781 | `	}else{` |
|         - | 14782 | `		/* Tokenize raw text */` |
|     13267 | 14783 | `		SySetAlloc(&aRawToken,32);` |
|     13267 | 14784 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|         - | 14785 | `	}` |
|         - | 14786 | `	/* Process high-level tokens */` |
|     70559 | 14787 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     70559 | 14788 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     70559 | 14789 | `	rc = PH7_OK;` |
|     70559 | 14790 | `	if( is_expr ){` |
|         - | 14791 | `		/* Compile the expression */` |
|       ! 0 | 14792 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14793 | `		goto cleanup;` |
|         - | 14794 | `	}` |
|     70559 | 14795 | `	nObjIdx = 0;` |
|         - | 14796 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14797 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14798 | `	 * preventing namespace bleeding across include()d files. */` |
|     70559 | 14799 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14800 | `	/* Start the compilation process */` |
|     41913 | 14801 | `	for(;;){` |
|    151223 | 14802 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     70547 | 14803 | `			break; /* No more tokens to process */` |
|         - | 14804 | `		}` |
|     80681 | 14805 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14806 | `			/* Compile the PHP chunk */` |
|     67409 | 14807 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67409 | 14808 | `			if( rc == SXERR_ABORT ){` |
|        15 | 14809 | `				break;` |
|         - | 14810 | `			}` |
|     67397 | 14811 | `			continue;` |
|         - | 14812 | `		}` |
|         - | 14813 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13277 | 14814 | `		nRawObj = 0;` |
|     26549 | 14815 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14816 | `			/* Consume the raw chunk without any processing */` |
|     13277 | 14817 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13277 | 14818 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14819 | `				rc = SXERR_MEM;` |
|       ! 0 | 14820 | `				break;` |
|         - | 14821 | `			}` |
|         - | 14822 | `			/* Mark as constant and emit the load constant instruction */` |
|     13277 | 14823 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13277 | 14824 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13277 | 14825 | `			++nRawObj;` |
|     13277 | 14826 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14827 | `		}` |
|     13277 | 14828 | `		if( nRawObj > 0 ){` |
|         - | 14829 | `			/* Emit the consume instruction */` |
|     13277 | 14830 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6636 | 14831 | `		}` |
|     35282 | 14832 | `	}` |
|     35277 | 14833 | `cleanup:` |
|         - | 14834 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     70559 | 14835 | `	pCodeGen->pIn = pSavedIn;` |
|     70559 | 14836 | `	pCodeGen->pEnd = pSavedEnd;` |
|     70559 | 14837 | `	SySetRelease(&aRawToken);` |
|     70559 | 14838 | `	SySetRelease(&aPhpToken);` |
|         - | 14839 | `	/* Restore outer file's strict_types scope */` |
|     70559 | 14840 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     70559 | 14841 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     70559 | 14842 | `	return rc;` |
|     35282 | 14843 | `}` |
|         - | 14844 | `/*` |
|         - | 14845 | ` * Utility routines.Initialize the code generator.` |
|         - | 14846 | ` */` |
|      3812 | 14847 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 14848 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14849 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14850 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14851 | `	)` |
|         5 | 14852 | `{` |
|      3817 | 14853 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14854 | `	/* Zero the structure */` |
|      3817 | 14855 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 14856 | `	/* Initial state */` |
|      3817 | 14857 | `	pGen->pVm  = &(*pVm);` |
|      3817 | 14858 | `	pGen->xErr = xErr;` |
|      3817 | 14859 | `	pGen->pErrData = pErrData;` |
|      3817 | 14860 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3817 | 14861 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3817 | 14862 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3817 | 14863 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3817 | 14864 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3817 | 14865 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3817 | 14866 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3817 | 14867 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3817 | 14868 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 14869 | `	/* Error log buffer */` |
|      3817 | 14870 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 14871 | `	/* General purpose working buffer */` |
|      3817 | 14872 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 14873 | `	/* Namespace state */` |
|      3817 | 14874 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3817 | 14875 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3817 | 14876 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3817 | 14877 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14878 | `	/* Create the global scope */` |
|      3817 | 14879 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 14880 | `	/* Point to the global scope */` |
|      3817 | 14881 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3817 | 14882 | `	return SXRET_OK;` |
|         5 | 14883 | `}` |
|         - | 14884 | `/*` |
|         - | 14885 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 14886 | ` */` |
|     73910 | 14887 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 14888 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14889 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14890 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14891 | `	)` |
|         5 | 14892 | `{` |
|     73915 | 14893 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14894 | `	GenBlock *pBlock,*pParent;` |
|         - | 14895 | `	/* Reset state */` |
|     73915 | 14896 | `	SySetReset(&pGen->aLabel);` |
|     73915 | 14897 | `	SySetReset(&pGen->aGoto);` |
|     73915 | 14898 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     73915 | 14899 | `	SySetReset(&pGen->aTrivia);` |
|     73915 | 14900 | `	SySetReset(&pGen->aPendingAttrs);` |
|     73915 | 14901 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     73915 | 14902 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     73915 | 14903 | `	SyBlobRelease(&pGen->sWorker);` |
|     73915 | 14904 | `	SyBlobRelease(&pGen->sNamespace);` |
|     73915 | 14905 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     73915 | 14906 | `	SyHashRelease(&pGen->hUseImports);` |
|     73915 | 14907 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     73915 | 14908 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     73915 | 14909 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     73915 | 14910 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     73915 | 14911 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14912 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 14913 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 14914 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 14915 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 14916 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 14917 | `	 * number of unique names, which is acceptable. */` |
|         - | 14918 | `	/* Point to the global scope */` |
|     73915 | 14919 | `	pBlock = pGen->pCurrent;` |
|     73915 | 14920 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 14921 | `		pParent = pBlock->pParent;` |
|       ! 0 | 14922 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 14923 | `		pBlock = pParent;` |
|       ! 0 | 14924 | `	}` |
|     73915 | 14925 | `	pGen->xErr = xErr;` |
|     73915 | 14926 | `	pGen->pErrData = pErrData;` |
|     73915 | 14927 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     73915 | 14928 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     73915 | 14929 | `	pGen->pIn = pGen->pEnd = 0;` |
|     73915 | 14930 | `	pGen->nErr = 0;` |
|     73915 | 14931 | `	return SXRET_OK;` |
|         5 | 14932 | `}` |
|         - | 14933 | `/*` |
|         - | 14934 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 14935 | ` * php's parser prints, e.g.` |
|         - | 14936 | ` *` |
|         - | 14937 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 14938 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 14939 | ` *   syntax error, unexpected end of file` |
|         - | 14940 | ` *` |
|         - | 14941 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 14942 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 14943 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 14944 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 14945 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 14946 | ` *` |
|         - | 14947 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 14948 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 14949 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 14950 | ` */` |
|       182 | 14951 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 14952 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 14953 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 14954 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 14955 | `	)` |
|         5 | 14956 | `{` |
|       187 | 14957 | `	const char *zNoun = "token";` |
|         - | 14958 | `	sxu32 nLine;` |
|       187 | 14959 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 14960 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 14961 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 14962 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 14963 | `		 * it before concluding "end of file". */` |
|        92 | 14964 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        92 | 14965 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        92 | 14966 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 14967 | `			pTok = pGen->pEnd;` |
|        44 | 14968 | `		}` |
|        44 | 14969 | `	}` |
|       187 | 14970 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       187 | 14971 | `	if( pTok == 0 ){` |
|       ! 0 | 14972 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 14973 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 14974 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 14975 | `			zExpecting);` |
|         - | 14976 | `	}` |
|       187 | 14977 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 14978 | `		zNoun = "identifier";` |
|       180 | 14979 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         9 | 14980 | `		zNoun = "variable";` |
|       171 | 14981 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        23 | 14982 | `		zNoun = "integer";` |
|       158 | 14983 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 14984 | `		zNoun = "float";` |
|       ! 0 | 14985 | `	}` |
|       187 | 14986 | `	if( zExpecting ){` |
|       118 | 14987 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 14988 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 14989 | `	}` |
|       164 | 14990 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 14991 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        96 | 14992 | `}` |
|         - | 14993 | `/*` |
|         - | 14994 | ` * Generate a compile-time error message.` |
|         - | 14995 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 14996 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 14997 | ` * abort compilation immediately.` |
|         - | 14998 | ` */` |
|     15926 | 14999 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 15000 | `{` |
|     15931 | 15001 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     15931 | 15002 | `	const char *zErr = "Error";` |
|         - | 15003 | `	SyString *pFile;` |
|         - | 15004 | `	va_list ap;` |
|         - | 15005 | `	sxi32 rc;` |
|         - | 15006 | `	/* Reset the working buffer */` |
|     15931 | 15007 | `	SyBlobReset(pWorker);` |
|         - | 15008 | `	/* Peek the processed file path if available */` |
|     15931 | 15009 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     15931 | 15010 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 15011 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 15012 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 15013 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 15014 | `		 * into execution with a 0 exit status. */` |
|       659 | 15015 | `		pGen->nErr++;` |
|       659 | 15016 | `		if( pGen->nErr > 15 ){` |
|         - | 15017 | `			/* Error count limit reached */` |
|         5 | 15018 | `			if( pGen->xErr ){` |
|         5 | 15019 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         5 | 15020 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         5 | 15021 | `				if( pFile ){` |
|         5 | 15022 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 15023 | `				}` |
|         5 | 15024 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         5 | 15025 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         5 | 15026 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 15027 | `				}` |
|         2 | 15028 | `			}` |
|         - | 15029 | `			/* Abort immediately */` |
|         5 | 15030 | `			return SXERR_ABORT;` |
|         - | 15031 | `		}` |
|       325 | 15032 | `	}` |
|     15927 | 15033 | `	if( pGen->xErr == 0 ){` |
|         - | 15034 | `		/* No available error consumer,return immediately */` |
|     15255 | 15035 | `		return SXRET_OK;` |
|         - | 15036 | `	}` |
|       677 | 15037 | `	switch(nErrType){` |
|       310 | 15038 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 15039 | `	case E_WARNING: zErr = "Warning";     break;` |
|       346 | 15040 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 15041 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 15042 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 15043 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 15044 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        16 | 15045 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 15046 | `	default:` |
|       ! 0 | 15047 | `		break;` |
|         - | 15048 | `	}` |
|       677 | 15049 | `	rc = SXRET_OK;` |
|         - | 15050 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       677 | 15051 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       677 | 15052 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       677 | 15053 | `	va_start(ap,zFormat);` |
|       677 | 15054 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       677 | 15055 | `	va_end(ap);` |
|       677 | 15056 | `	if( pFile ){` |
|       677 | 15057 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       336 | 15058 | `	}` |
|         - | 15059 | `	/* Append a new line */` |
|       677 | 15060 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       677 | 15061 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 15062 | `		/* Consume the generated error message */` |
|       677 | 15063 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       336 | 15064 | `	}` |
|       677 | 15065 | `	return rc;` |
|      7968 | 15066 | `}` |
|         - | 15067 |  |
