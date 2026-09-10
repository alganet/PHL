# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7307/9024 lines (80.97%)

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
|    149004 |   139 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   140 | `{` |
|    149009 |   141 | `	GenBlock *pBlock = pCurrent;` |
|    336027 |   142 | `	for(;;){` |
|    672059 |   143 | `		if( pBlock->iFlags & iBlockType ){` |
|    149009 |   144 | `			iCount--; /* Decrement nesting level */` |
|    149009 |   145 | `			if( iCount < 1 ){` |
|         - |   146 | `				/* Block meet with the desired criteria */` |
|    148983 |   147 | `				return pBlock;` |
|         - |   148 | `			}` |
|        13 |   149 | `		}` |
|         - |   150 | `		/* Point to the upper block */` |
|    523081 |   151 | `		pBlock = pBlock->pParent;` |
|    523081 |   152 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   153 | `			/* Forbidden */` |
|        16 |   154 | `			break;` |
|         - |   155 | `		}` |
|         5 |   156 | `	}` |
|         - |   157 | `	/* No such block */` |
|        29 |   158 | `	return 0;` |
|     74507 |   159 | `}` |
|         - |   160 | `/*` |
|         - |   161 | ` * Initialize a freshly allocated block instance.` |
|         - |   162 | ` */` |
|  11449988 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|  11449993 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  11449993 |   173 | `	pBlock->pUserData   = pUserData;` |
|  11449993 |   174 | `	pBlock->pGen        = pGen;` |
|  11449993 |   175 | `	pBlock->iFlags      = iType;` |
|  11449993 |   176 | `	pBlock->pParent     = 0;` |
|  11449993 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11449993 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11449993 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|  11446168 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|  11446173 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  11446173 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|  11446173 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  11446173 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|  11446173 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  11446173 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    493347 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    493347 |   214 | `		pGen->nLoopId++;` |
|    493347 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    493347 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    493347 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    493347 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    246671 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|  11446173 |   221 | `	pGen->pCurrent = pBlock;` |
|  11446173 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   5479425 |   224 | `		*ppBlock = pBlock;` |
|   2739710 |   225 | `	}` |
|  11446173 |   226 | `	return SXRET_OK;` |
|   5723089 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|  11446156 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|  11446161 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|  11446161 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|  11446161 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|  11446152 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|  11446157 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  11446157 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|  11446157 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  11446157 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|  11446152 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|  11446157 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  11446157 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|  11446157 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    493339 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    246667 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|  11446157 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|  11446157 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|  11446157 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|  11446157 |   268 | `	return SXRET_OK;` |
|   5723081 |   269 | `}` |
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
|   4341558 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   4341563 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   4341563 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   4341563 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4341563 |   289 | `	return rc;` |
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
|   8021778 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   8021783 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  17286673 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9264895 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   3457163 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   5807737 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1466181 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   4341561 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4341561 |   322 | `		if( pInstr ){` |
|   4341561 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   4341561 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   4341561 |   326 | `			aFix[n].nJumpType = -1;` |
|   2170778 |   327 | `		}` |
|   2170783 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   8021783 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2807308 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2807313 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2807459 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   2807311 |   400 | `	return SXRET_OK;` |
|   1403659 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  14636952 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  14636957 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  14636957 |   409 | `	if( pEntry == 0 ){` |
|   3819149 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|  10817813 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10817813 |   413 | `	return SXRET_OK;` |
|   7318481 |   414 | `}` |
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
|   3819144 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3819149 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3819149 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1909572 |   429 | `	}` |
|   3819149 |   430 | `	return SXRET_OK;` |
|         5 |   431 | `}` |
|         - |   432 | `/*` |
|         - |   433 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   434 | ` * in the constant table.` |
|         - |   435 | ` */` |
|   3547440 |   436 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   437 | `{` |
|         - |   438 | `	ph7_value *pObj;` |
|   3547445 |   439 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   440 | `	/* Reserve a new constant */` |
|   3547445 |   441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3547445 |   442 | `	if( pObj == 0 ){` |
|       ! 0 |   443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   444 | `		return 0;` |
|         - |   445 | `	}` |
|   3547445 |   446 | `	*pIdx = nIdx;` |
|         - |   447 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   448 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   449 | `	 */` |
|   3547445 |   450 | `	return pObj;` |
|   1773725 |   451 | `}` |
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
|   6898352 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6898357 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3449181 |   478 | `}` |
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
|   3556098 |   542 | `static int GenStateFindBadNumericSeparator(` |
|         - |   543 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   544 | `{` |
|   3556103 |   545 | `	const char *z = pRaw->zString;` |
|   3556103 |   546 | `	sxu32 n = pRaw->nByte;` |
|   3556103 |   547 | `	int base = 10;` |
|         - |   548 | `	sxu32 i, start;` |
|   3556103 |   549 | `	if( n < 2 ) return 0;` |
|    743039 |   550 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|    103127 |   551 | `		base = 16;` |
|    691478 |   552 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       284 |   553 | `		base = 2;` |
|       141 |   554 | `	}` |
|   2800925 |   555 | `	for( i = 0; i < n; ++i ){` |
|   2057905 |   556 | `		if( z[i] != '_' ) continue;` |
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
|    743025 |   573 | `	return 0;` |
|   1778054 |   574 | `}` |
|         - |   575 | `/*` |
|         - |   576 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   577 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   578 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   579 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   580 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   581 | ` * so callers can bail from the current construct).` |
|         - |   582 | ` */` |
|   3556098 |   583 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   584 | `{` |
|   3556103 |   585 | `	const char *zBad = 0;` |
|   3556103 |   586 | `	sxu32 nBad = 0;` |
|         - |   587 | `	SyString sBad;` |
|         - |   588 | `	sxi32 rc;` |
|   3556103 |   589 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   3556089 |   590 | `		return SXRET_OK;` |
|         - |   591 | `	}` |
|        18 |   592 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   593 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   594 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   595 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   596 | `		return SXERR_ABORT;` |
|         - |   597 | `	}` |
|        18 |   598 | `	return SXERR_SYNTAX;` |
|   1778054 |   599 | `}` |
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
|   3556084 |   616 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   617 | `	SyMemBackend *pAlloc,` |
|         - |   618 | `	const SyString *pToken,` |
|         - |   619 | `	char *zScratch, sxu32 nScratch,` |
|         - |   620 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   621 | `{` |
|         - |   622 | `	sxu32 i, j;` |
|   3556089 |   623 | `	int hasUnderscore = 0;` |
|         - |   624 | `	char *zBuf;` |
|   3556089 |   625 | `	*pzAlloc = 0;` |
|   8424973 |   626 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   4869141 |   627 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   2434447 |   628 | `	}` |
|   3556089 |   629 | `	if( !hasUnderscore ){` |
|   3555837 |   630 | `		SyStringDupPtr(pOut, pToken);` |
|   3555837 |   631 | `		return SXRET_OK;` |
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
|   1778047 |   648 | `}` |
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
|   3547474 |   684 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   685 | `{` |
|   3547479 |   686 | `	const char *z = pNum->zString;` |
|   3547479 |   687 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   688 | `	const char *p, *q;` |
|         - |   689 | `	int n;` |
|   3547479 |   690 | `	*pbDecimal = FALSE;` |
|   3547479 |   691 | `	if( z >= zEnd ){` |
|       ! 0 |   692 | `		return FALSE;` |
|         - |   693 | `	}` |
|   3547479 |   694 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|         - |   695 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|    103125 |   696 | `		p = z + 2;` |
|    129845 |   697 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    420323 |   698 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|    103125 |   699 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|    103119 |   700 | `			return FALSE;` |
|         - |   701 | `		}` |
|         7 |   702 | `		{ ph7_real dv = 0;` |
|       103 |   703 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|        97 |   704 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|        49 |   705 | `		  }` |
|         7 |   706 | `		  *pReal = dv;` |
|         - |   707 | `		}` |
|         7 |   708 | `		return TRUE;` |
|   3444359 |   709 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|   3444079 |   724 | `	}else if( z[0] == '0' ){` |
|         - |   725 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   726 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   727 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1276913 |   728 | `		p = z;` |
|   2553823 |   729 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1288613 |   730 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1276913 |   731 | `		if( n <= 21 ){` |
|   1276911 |   732 | `			return FALSE;` |
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
|   2167171 |   745 | `	p = z;` |
|   2167171 |   746 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   5168561 |   747 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   2167171 |   748 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   749 | `		*pbDecimal = TRUE;` |
|        25 |   750 | `		return TRUE;` |
|         - |   751 | `	}` |
|   2167147 |   752 | `	return FALSE;` |
|   1773742 |   753 | `}` |
|   3556070 |   754 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   755 | `{` |
|   3556075 |   756 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   3556075 |   757 | `	sxu32 nIdx = 0;` |
|         - |   758 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   3556075 |   759 | `	char *zAlloc = 0;` |
|         - |   760 | `	SyString sNum;` |
|         - |   761 | `	sxi32 rc;` |
|   1778035 |   762 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   3556075 |   763 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   3556075 |   764 | `	if( rc != SXRET_OK ){` |
|        14 |   765 | `		return rc;` |
|         - |   766 | `	}` |
|   5334095 |   767 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1778030 |   768 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   3556065 |   769 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   770 | `		return SXERR_ABORT;` |
|         - |   771 | `	}` |
|   3556065 |   772 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   773 | `		ph7_value *pObj;` |
|         - |   774 | `		sxi64 iValue;` |
|   3547479 |   775 | `		ph7_real rOverflow = 0;` |
|   3547479 |   776 | `		int bDecimalOverflow = 0;` |
|   3547479 |   777 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|   3547445 |   794 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   3547445 |   795 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   3547445 |   796 | `			if( pObj == 0 ){` |
|       ! 0 |   797 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   798 | `				return SXERR_ABORT;` |
|         - |   799 | `			}` |
|   3547445 |   800 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   801 | `		}` |
|   1773742 |   802 | `	}else{` |
|         - |   803 | `		/* Real number */` |
|         - |   804 | `		ph7_value *pObj;` |
|         - |   805 | `		/* Reserve a new constant */` |
|      8591 |   806 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      8591 |   807 | `		if( pObj == 0 ){` |
|       ! 0 |   808 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   809 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   810 | `			return SXERR_ABORT;` |
|         - |   811 | `		}` |
|      8591 |   812 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      8591 |   813 | `		PH7_MemObjToReal(pObj);` |
|         - |   814 | `	}` |
|   3556065 |   815 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   816 | `	/* Emit the load constant instruction */` |
|   3556065 |   817 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   818 | `	/* Node successfully compiled */` |
|   3556065 |   819 | `	return SXRET_OK;` |
|   1778040 |   820 | `}` |
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
|   5076610 |   832 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   833 | `{` |
|   5076615 |   834 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   835 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   836 | `	ph7_value *pObj;` |
|         - |   837 | `	sxu32 nIdx;` |
|   5076615 |   838 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   839 | `	/* Delimit the string */` |
|   5076615 |   840 | `	zIn  = pStr->zString;` |
|   5076615 |   841 | `	zEnd = &zIn[pStr->nByte];` |
|   5076615 |   842 | `	if( zIn >= zEnd ){` |
|         - |   843 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   844 | `		 * rather than reserving a new object each time. */` |
|    324579 |   845 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    324579 |   846 | `		return SXRET_OK;` |
|         - |   847 | `	}` |
|   4752041 |   848 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   849 | `		/* Already processed,emit the load constant instruction` |
|         - |   850 | `		 * and return.` |
|         - |   851 | `		 */` |
|   2816917 |   852 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2816917 |   853 | `		return SXRET_OK;` |
|         - |   854 | `	}` |
|         - |   855 | `	/* Reserve a new constant */` |
|   1935129 |   856 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1935129 |   857 | `	if( pObj == 0 ){` |
|       ! 0 |   858 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   859 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   860 | `		return SXERR_ABORT;` |
|         - |   861 | `	}` |
|   1935129 |   862 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   863 | `	/* Compile the node */` |
|   1982887 |   864 | `	for(;;){` |
|   3965779 |   865 | `		if( zIn >= zEnd ){` |
|         - |   866 | `			/* End of input */` |
|   1935129 |   867 | `			break;` |
|         - |   868 | `		}` |
|   2030655 |   869 | `		zCur = zIn;` |
|  40317795 |   870 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  38287145 |   871 | `			zIn++;` |
|         5 |   872 | `		}` |
|   2030655 |   873 | `		if( zIn > zCur ){` |
|         - |   874 | `			/* Append raw contents*/` |
|   1992463 |   875 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    996229 |   876 | `		}` |
|   2030655 |   877 | `		zIn++;` |
|   2030655 |   878 | `		if( zIn < zEnd ){` |
|    129903 |   879 | `			if( zIn[0] == '\\' ){` |
|         - |   880 | `				/* A literal backslash */` |
|     30569 |   881 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    114621 |   882 | `			}else if( zIn[0] == '\'' ){` |
|         - |   883 | `				/* A single quote */` |
|        11 |   884 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   885 | `			}else{` |
|         - |   886 | `				/* verbatim copy */` |
|     99329 |   887 | `				zIn--;` |
|     99329 |   888 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     99329 |   889 | `				zIn++;` |
|         - |   890 | `			}` |
|     64949 |   891 | `		}` |
|         - |   892 | `		/* Advance the stream cursor */` |
|   2030655 |   893 | `		zIn++;` |
|         5 |   894 | `	}` |
|         - |   895 | `	/* Emit the load constant instruction */` |
|   1935129 |   896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1935129 |   897 | `	if( pStr->nByte < 1024 ){` |
|         - |   898 | `		/* Install in the literal table */` |
|   1935129 |   899 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    967562 |   900 | `	}` |
|         - |   901 | `	/* Node successfully compiled */` |
|   1935129 |   902 | `	return SXRET_OK;` |
|   2538310 |   903 | `}` |
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
|        72 |   931 | `		*pOut = *pIn;` |
|        72 |   932 | `		return SXRET_OK;` |
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
|         4 |  1016 | `{` |
|         - |  1017 | `	SyString sStripped;` |
|         - |  1018 | `	SyString *pStr;` |
|         - |  1019 | `	ph7_value *pObj;` |
|         - |  1020 | `	sxu32 nIdx;` |
|         - |  1021 | `	sxi32 rc;` |
|        52 |  1022 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        52 |  1023 | `	if( rc != SXRET_OK ){` |
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
|        28 |  1047 | `}` |
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
|      2630 |  1070 | `static sxi32 GenStateProcessStringExpression(` |
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
|      2635 |  1081 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1082 | `	/* Preallocate some slots */` |
|      2635 |  1083 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1084 | `	/* Tokenize the text */` |
|      2635 |  1085 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1086 | `	/* Swap delimiter */` |
|      2635 |  1087 | `	pTmpIn  = pGen->pIn;` |
|      2635 |  1088 | `	pTmpEnd = pGen->pEnd;` |
|      2635 |  1089 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2635 |  1090 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1091 | `	/* Compile the expression */` |
|      2635 |  1092 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1093 | `	/* Restore token stream */` |
|      2635 |  1094 | `	pGen->pIn  = pTmpIn;` |
|      2635 |  1095 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1096 | `	/* Release the token set */` |
|      2635 |  1097 | `	SySetRelease(&sToken);` |
|         - |  1098 | `	/* Compilation result */` |
|      2635 |  1099 | `	return rc;` |
|         5 |  1100 | `}` |
|         - |  1101 | `/*` |
|         - |  1102 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1103 | ` */` |
|    121556 |  1104 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1105 | `{` |
|         - |  1106 | `	ph7_value *pConstObj;` |
|    121561 |  1107 | `	sxu32 nIdx = 0;` |
|         - |  1108 | `	/* Reserve a new constant */` |
|    121561 |  1109 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    121561 |  1110 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1111 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1112 | `		return 0;` |
|         - |  1113 | `	}` |
|    121561 |  1114 | `	(*pCount)++;` |
|    121561 |  1115 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1116 | `	/* Emit the load constant instruction */` |
|    121561 |  1117 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    121561 |  1118 | `	return pConstObj;` |
|     60783 |  1119 | `}` |
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
|    119994 |  1182 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1183 | `{` |
|    119999 |  1184 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1185 | `	const char *zIn,*zCur,*zEnd;` |
|    119999 |  1186 | `	ph7_value *pObj = 0;` |
|         - |  1187 | `	sxi32 iCons;` |
|         - |  1188 | `	sxi32 rc;` |
|         - |  1189 | `	/* Delimit the string */` |
|    119999 |  1190 | `	zIn  = pStr->zString;` |
|    119999 |  1191 | `	zEnd = &zIn[pStr->nByte];` |
|    119999 |  1192 | `	if( zIn >= zEnd ){` |
|         - |  1193 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1194 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1195 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1196 | `		 */` |
|       413 |  1197 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       413 |  1198 | `		return SXRET_OK;` |
|         - |  1199 | `	}` |
|    119591 |  1200 | `	zCur = 0;` |
|         - |  1201 | `	/* Compile the node */` |
|    119591 |  1202 | `	iCons = 0;` |
|     61106 |  1203 | `	for(;;){` |
|    162557 |  1204 | `		zCur = zIn;` |
|   1657015 |  1205 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1497093 |  1206 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        69 |  1207 | `				break;` |
|   1496966 |  1208 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2508 |  1209 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1254 |  1210 | `					break;` |
|         - |  1211 | `			}` |
|   1494463 |  1212 | `			zIn++;` |
|         5 |  1213 | `		}` |
|    162557 |  1214 | `		if( zIn > zCur ){` |
|     94899 |  1215 | `			if( pObj == 0 ){` |
|     94281 |  1216 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     94281 |  1217 | `				if( pObj == 0 ){` |
|       ! 0 |  1218 | `					return SXERR_ABORT;` |
|         - |  1219 | `				}` |
|     47138 |  1220 | `			}` |
|     94899 |  1221 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     47447 |  1222 | `		}` |
|    162557 |  1223 | `		if( zIn >= zEnd ){` |
|    119589 |  1224 | `			break;` |
|         - |  1225 | `		}` |
|     42973 |  1226 | `		if( zIn[0] == '\\' ){` |
|     40343 |  1227 | `			const char *zPtr = 0;` |
|         - |  1228 | `			sxu32 n;` |
|     40343 |  1229 | `			zIn++;` |
|     40343 |  1230 | `			if( pObj == 0 ){` |
|     27285 |  1231 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27285 |  1232 | `				if( pObj == 0 ){` |
|       ! 0 |  1233 | `					return SXERR_ABORT;` |
|         - |  1234 | `				}` |
|     13640 |  1235 | `			}` |
|     40343 |  1236 | `			if( zIn >= zEnd ){` |
|         - |  1237 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1238 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1239 | `				break;` |
|         - |  1240 | `			}` |
|     40341 |  1241 | `			n = sizeof(char); /* size of conversion */` |
|     40341 |  1242 | `			switch( zIn[0] ){` |
|        15 |  1243 | `			case '$':` |
|         - |  1244 | `				/* Dollar sign */` |
|        33 |  1245 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        33 |  1246 | `				break;` |
|        55 |  1247 | `			case '\\':` |
|         - |  1248 | `				/* A literal backslash */` |
|       115 |  1249 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       115 |  1250 | `				break;` |
|         1 |  1251 | `			case 'e':` |
|         - |  1252 | `				/* Escape (ESC) ASCII code 27 */` |
|         3 |  1253 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|         3 |  1254 | `				break;` |
|         4 |  1255 | `			case 'f':` |
|         - |  1256 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|         9 |  1257 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|         9 |  1258 | `				break;` |
|     17585 |  1259 | `			case 'n':` |
|         - |  1260 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     35175 |  1261 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     35175 |  1262 | `				break;` |
|        27 |  1263 | `			case 'r':` |
|         - |  1264 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1265 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1266 | `				break;` |
|      1939 |  1267 | `			case 't':` |
|         - |  1268 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      3883 |  1269 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      3883 |  1270 | `				break;` |
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
|     40341 |  1385 | `			zIn += n;` |
|     40341 |  1386 | `			continue;` |
|         - |  1387 | `		}` |
|      2635 |  1388 | `		if( zIn[0] == '{' ){` |
|         - |  1389 | `			/* Curly syntax */` |
|         - |  1390 | `			const char *zExpr;` |
|       135 |  1391 | `			sxi32 iNest = 1;` |
|       135 |  1392 | `			zIn++;` |
|       135 |  1393 | `			zExpr = zIn;` |
|         - |  1394 | `			/* Synchronize with the next closing curly braces */` |
|      1323 |  1395 | `			while( zIn < zEnd ){` |
|      1323 |  1396 | `				if( zIn[0] == '{' ){` |
|         - |  1397 | `					/* Increment nesting level */` |
|         3 |  1398 | `					iNest++;` |
|      1322 |  1399 | `				}else if(zIn[0] == '}' ){` |
|         - |  1400 | `					/* Decrement nesting level */` |
|       137 |  1401 | `					iNest--;` |
|       137 |  1402 | `					if( iNest <= 0 ){` |
|       135 |  1403 | `						break;` |
|         - |  1404 | `					}` |
|         1 |  1405 | `				}` |
|      1191 |  1406 | `				zIn++;` |
|         3 |  1407 | `			}` |
|         - |  1408 | `			/* Process the expression */` |
|       135 |  1409 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|       135 |  1410 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1411 | `				return SXERR_ABORT;` |
|         - |  1412 | `			}` |
|       135 |  1413 | `			if( rc != SXERR_EMPTY ){` |
|       135 |  1414 | `				++iCons;` |
|        66 |  1415 | `			}` |
|       135 |  1416 | `			if( zIn < zEnd ){` |
|         - |  1417 | `				/* Jump the trailing curly */` |
|       135 |  1418 | `				zIn++;` |
|        66 |  1419 | `			}` |
|        69 |  1420 | `		}else{` |
|         - |  1421 | `			/* Simple syntax */` |
|      2503 |  1422 | `			const char *zExpr = zIn;` |
|         - |  1423 | `			/* Assemble variable name */` |
|      1274 |  1424 | `			for(;;){` |
|         - |  1425 | `				/* Jump leading dollars */` |
|      5051 |  1426 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2503 |  1427 | `					zIn++;` |
|         5 |  1428 | `				}` |
|      1274 |  1429 | `				for(;;){` |
|     13055 |  1430 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9233 |  1431 | `						zIn++;` |
|         5 |  1432 | `					}` |
|      2553 |  1433 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1434 | `						/* UTF-8 stream */` |
|       ! 0 |  1435 | `						zIn++;` |
|       ! 0 |  1436 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1437 | `							zIn++;` |
|       ! 0 |  1438 | `						}` |
|       ! 0 |  1439 | `						continue;` |
|         - |  1440 | `					}` |
|      2553 |  1441 | `					break;` |
|       ! 0 |  1442 | `				}` |
|      2553 |  1443 | `				if( zIn >= zEnd ){` |
|       269 |  1444 | `					break;` |
|         - |  1445 | `				}` |
|      2289 |  1446 | `				if( zIn[0] == '[' ){` |
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
|      2279 |  1464 | `				}else if(zIn[0] == '{' ){` |
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
|      2275 |  1482 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1483 | `					/* Member access operator '->' */` |
|        53 |  1484 | `					zIn += 2;` |
|      2250 |  1485 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1486 | `					/* Static member access operator '::' */` |
|       ! 0 |  1487 | `					zIn += 2;` |
|       ! 0 |  1488 | `				}else{` |
|      1115 |  1489 | `					break;` |
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
|      2503 |  1501 | `				const char *zBr = zExpr;` |
|     14347 |  1502 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     11849 |  1503 | `					zBr++;` |
|         5 |  1504 | `				}` |
|      2503 |  1505 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
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
|      2496 |  1546 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' && zIn[-1] == '}'` |
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
|      2499 |  1582 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2499 |  1583 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1584 | `				return SXERR_ABORT;` |
|         - |  1585 | `			}` |
|      2499 |  1586 | `			if( rc != SXERR_EMPTY ){` |
|      2497 |  1587 | `				++iCons;` |
|      1246 |  1588 | `			}` |
|         - |  1589 | `		}` |
|         - |  1590 | `		/* Invalidate the previously used constant */` |
|      2631 |  1591 | `		pObj = 0;` |
|         5 |  1592 | `	}/*for(;;)*/` |
|    119591 |  1593 | `	if( iCons > 1 ){` |
|         - |  1594 | `		/* Concatenate all compiled constants */` |
|      1905 |  1595 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       950 |  1596 | `	}` |
|         - |  1597 | `	/* Node successfully compiled */` |
|    119591 |  1598 | `	return SXRET_OK;` |
|     60002 |  1599 | `}` |
|         - |  1600 | `/*` |
|         - |  1601 | ` * Compile a double quoted string.` |
|         - |  1602 | ` *  See the block-comment above for more information.` |
|         - |  1603 | ` */` |
|    119932 |  1604 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1605 | `{` |
|         - |  1606 | `	sxi32 rc;` |
|    119937 |  1607 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     59966 |  1608 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1609 | `	/* Compilation result */` |
|    119937 |  1610 | `	return rc;` |
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
|        64 |  1628 | `	sOrig = pGen->pIn->sData;` |
|        64 |  1629 | `	pGen->pIn->sData = sStripped;` |
|        64 |  1630 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|        64 |  1631 | `	pGen->pIn->sData = sOrig;` |
|        31 |  1632 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        64 |  1633 | `	return rc;` |
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
|   1436422 |  1654 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   1436427 |  1665 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1666 | `	/* Compile the expression*/` |
|   1436427 |  1667 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1668 | `	/* Restore token stream */` |
|   1436427 |  1669 | `	RE_SWAP_DELIMITER(pGen);` |
|   1436427 |  1670 | `	return rc;` |
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
|         4 |  1681 | `{` |
|        40 |  1682 | `	sxi32 rc = SXRET_OK;` |
|        40 |  1683 | `	if( pRoot->pOp ){` |
|        14 |  1684 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|        12 |  1685 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|        15 |  1686 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1687 | `			/* Unexpected expression */` |
|        12 |  1688 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|        12 |  1689 | `			if( rc != SXERR_ABORT ){` |
|        12 |  1690 | `				rc = SXERR_INVALID;` |
|         5 |  1691 | `			}` |
|         8 |  1692 | `		}` |
|        31 |  1693 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1694 | `		/* Unexpected expression */` |
|         3 |  1695 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|         3 |  1696 | `		if( rc != SXERR_ABORT ){` |
|         3 |  1697 | `			rc = SXERR_INVALID;` |
|         1 |  1698 | `		}` |
|         1 |  1699 | `	}` |
|        40 |  1700 | `	return rc;` |
|         4 |  1701 | `}` |
|         - |  1702 | `/*` |
|         - |  1703 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|         - |  1704 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|         - |  1705 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|         - |  1706 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|         - |  1707 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|         - |  1708 | ` */` |
|   1370274 |  1709 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1710 | `{` |
|   1370279 |  1711 | `	SyToken *pCur = pStart;` |
|   1370279 |  1712 | `	sxi32 iNest = 0;` |
|   3534297 |  1713 | `	while( pCur < pEnd ){` |
|   2666317 |  1714 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    502295 |  1715 | `			return pCur;` |
|         - |  1716 | `		}` |
|         - |  1717 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1718 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1719 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1720 | `		 */` |
|   2164027 |  1721 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23003 |  1722 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23003 |  1723 | `			SyToken *pFn = pCur;` |
|     22998 |  1724 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1725 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1726 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1727 | `				pFn = &pCur[1];` |
|       ! 0 |  1728 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1729 | `			}` |
|     23003 |  1730 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|     22999 |  1761 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|     11496 |  1781 | `		}` |
|   2164021 |  1782 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     53989 |  1783 | `			iNest++;` |
|   2137029 |  1784 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1785 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1786 | `			 * parser will shortly detect any syntax error. */` |
|     53989 |  1787 | `			iNest--;` |
|     26992 |  1788 | `		}` |
|   2164021 |  1789 | `		pCur++;` |
|         5 |  1790 | `	}` |
|    867985 |  1791 | `	return pEnd;` |
|    685142 |  1792 | `}` |
|         - |  1793 | `/*` |
|         - |  1794 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1795 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1796 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1797 | ` */` |
|    603178 |  1798 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1799 | `{` |
|         - |  1800 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1801 | `	SyToken *pKey,*pCur;` |
|    603183 |  1802 | `	sxi32 iEmitRef = 0;` |
|    603183 |  1803 | `	sxi32 iSpread = 0;` |
|    603183 |  1804 | `	sxi32 nPair = 0;` |
|         - |  1805 | `	sxi32 rc;` |
|    603183 |  1806 | `	xValidator = 0;` |
|    835475 |  1807 | `	for(;;){` |
|         - |  1808 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|         - |  1809 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|         - |  1810 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|         - |  1811 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|    533886 |  1812 | `		{` |
|   1670955 |  1813 | `			int nSkip = 0;` |
|   2491133 |  1814 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    820183 |  1815 | `				nSkip++;` |
|    820183 |  1816 | `				pGen->pIn++;` |
|         5 |  1817 | `			}` |
|   1670955 |  1818 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|       ! 0 |  1819 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|         - |  1820 | `					"Cannot use empty array elements in arrays");` |
|       ! 0 |  1821 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1822 | `					return SXERR_ABORT;` |
|         - |  1823 | `				}` |
|       ! 0 |  1824 | `				return SXRET_OK;` |
|         - |  1825 | `			}` |
|         - |  1826 | `		}` |
|   1670955 |  1827 | `		pCur = pGen->pIn;` |
|   1670955 |  1828 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1829 | `			/* No more entry to process */` |
|    603165 |  1830 | `			break;` |
|         - |  1831 | `		}` |
|   1067795 |  1832 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1833 | `			continue;` |
|         - |  1834 | `		}` |
|         - |  1835 | `		/* Compile the key if available */` |
|   1067795 |  1836 | `		pKey = pCur;` |
|   1067795 |  1837 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   1067795 |  1838 | `		rc = SXERR_EMPTY;` |
|   1067795 |  1839 | `		if( pCur < pGen->pIn ){` |
|    368379 |  1840 | `			if( pKey == pCur ){` |
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
|    368377 |  1854 | `			if( &pCur[1] >= pGen->pIn ){` |
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
|    368367 |  1865 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1866 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    368367 |  1867 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1868 | `				return SXERR_ABORT;` |
|         - |  1869 | `			}` |
|    368367 |  1870 | `			pCur++; /* Jump the '=>' operator */` |
|    184186 |  1871 | `		}else{` |
|         - |  1872 | `			/* Reset back the cursor and point to the entry value */` |
|    699421 |  1873 | `			pCur = pKey;` |
|         - |  1874 | `		}` |
|   1067783 |  1875 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1876 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|         - |  1877 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|    699421 |  1878 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|    349708 |  1879 | `		}` |
|   1067783 |  1880 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|         - |  1881 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|        44 |  1882 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|        44 |  1883 | `			iEmitRef = 1;` |
|        44 |  1884 | `			pCur++; /* Jump the '&' token */` |
|        44 |  1885 | `			if( pCur >= pGen->pIn ){` |
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
|   1067781 |  1899 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   1067781 |  1900 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   1601663 |  1917 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    533886 |  1918 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1919 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    533886 |  1920 | `			xValidator);` |
|   1067777 |  1921 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1922 | `			return SXERR_ABORT;` |
|         - |  1923 | `		}` |
|   1067777 |  1924 | `		if( iSpread ){` |
|         - |  1925 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        69 |  1926 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   1067744 |  1927 | `		}else if( iEmitRef ){` |
|         - |  1928 | `			/* Emit the load reference instruction */` |
|        40 |  1929 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1930 | `		}` |
|   1067777 |  1931 | `		xValidator = 0;` |
|   1067777 |  1932 | `		iEmitRef = 0;` |
|   1067777 |  1933 | `		iSpread = 0;` |
|   1067777 |  1934 | `		nPair++;` |
|         5 |  1935 | `	}` |
|         - |  1936 | `	/* Emit the load map instruction */` |
|    603165 |  1937 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1938 | `	/* Node successfully compiled */` |
|    603165 |  1939 | `	return SXRET_OK;` |
|    301594 |  1940 | `}` |
|         - |  1941 | `/*` |
|         - |  1942 | ` * Compile the 'array' language construct.` |
|         - |  1943 | ` *	 According to the PHP language reference manual` |
|         - |  1944 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1945 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1946 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1947 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1948 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1949 | ` */` |
|    387262 |  1950 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1951 | `{` |
|         - |  1952 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    387267 |  1953 | `	pGen->pIn += 2;` |
|    387267 |  1954 | `	pGen->pEnd--;` |
|    193631 |  1955 | `	SXUNUSED(iCompileFlag);` |
|    387267 |  1956 | `	return GenStateCompileArrayBody(pGen);` |
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
|    215916 |  2055 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2056 | `{` |
|         - |  2057 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    215921 |  2058 | `	pGen->pIn++;` |
|    215921 |  2059 | `	pGen->pEnd--;` |
|    107958 |  2060 | `	SXUNUSED(iCompileFlag);` |
|    215921 |  2061 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  2062 | `}` |
|         - |  2063 | `/*` |
|         - |  2064 | ` * Expression tree validator callback for the 'list' language construct.` |
|         - |  2065 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  2066 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  2067 | ` * error message.` |
|         - |  2068 | ` * See the routine responible of compiling the list language construct` |
|         - |  2069 | ` * for more inforation.` |
|         - |  2070 | ` */` |
|       214 |  2071 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  2072 | `{` |
|       219 |  2073 | `	sxi32 rc = SXRET_OK;` |
|       219 |  2074 | `	if( pRoot->pOp ){` |
|         4 |  2075 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|         2 |  2076 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|         - |  2077 | `				/* Unexpected expression */` |
|       ! 0 |  2078 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2079 | `					"Assignments can only happen to writable values");` |
|       ! 0 |  2080 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 |  2081 | `					rc = SXERR_INVALID;` |
|       ! 0 |  2082 | `				}` |
|         1 |  2083 | `		}` |
|       217 |  2084 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  2085 | `		/* Unexpected expression */` |
|         6 |  2086 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  2087 | `			"Assignments can only happen to writable values");` |
|         6 |  2088 | `		if( rc != SXERR_ABORT ){` |
|         6 |  2089 | `			rc = SXERR_INVALID;` |
|         2 |  2090 | `		}` |
|         2 |  2091 | `	}` |
|       219 |  2092 | `	return rc;` |
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
|       124 |  2215 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|         5 |  2216 | `{` |
|         - |  2217 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|         - |  2218 | `	SyToken *pNext;` |
|         - |  2219 | `	SyToken *pClassifyIn;` |
|       129 |  2220 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|         - |  2221 | `	sxi32 nExpr;` |
|         - |  2222 | `	sxi32 rc;` |
|         - |  2223 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|         - |  2224 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|         - |  2225 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|         - |  2226 | `	 * list. */` |
|       129 |  2227 | `	pClassifyIn = pGen->pIn;` |
|       373 |  2228 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       249 |  2229 | `		if( pGen->pIn >= pNext ){` |
|        13 |  2230 | `			nEmpty++;` |
|       243 |  2231 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|        31 |  2232 | `			nKeyed++;` |
|        16 |  2233 | `		}else{` |
|       207 |  2234 | `			nPositional++;` |
|         - |  2235 | `		}` |
|       249 |  2236 | `		pGen->pIn = &pNext[1];` |
|         5 |  2237 | `	}` |
|       129 |  2238 | `	pGen->pIn = pClassifyIn;` |
|       129 |  2239 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|       ! 0 |  2240 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2241 | `			"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2242 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2243 | `	}` |
|       129 |  2244 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|       ! 0 |  2245 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2246 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|       ! 0 |  2247 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2248 | `	}` |
|       129 |  2249 | `	if( nKeyed > 0 ){` |
|        23 |  2250 | `		return GenStateCompileKeyedListBody(pGen);` |
|         - |  2251 | `	}` |
|       107 |  2252 | `	nExpr = 0;` |
|       107 |  2253 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       321 |  2254 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       219 |  2255 | `		if( pGen->pIn < pNext ){` |
|         - |  2256 | `			/* Check for nested list() */` |
|       207 |  2257 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|       206 |  2274 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|       193 |  2290 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       193 |  2291 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  2292 | `					SySetRelease(&sNested);` |
|       ! 0 |  2293 | `					return SXRET_OK;` |
|         - |  2294 | `				}` |
|         - |  2295 | `			}` |
|       106 |  2296 | `		}else{` |
|         - |  2297 | `			/* Empty entry,load NULL */` |
|        13 |  2298 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|         - |  2299 | `		}` |
|       219 |  2300 | `		nExpr++;` |
|         - |  2301 | `		/* Advance the stream cursor */` |
|       219 |  2302 | `		pGen->pIn = &pNext[1];` |
|         5 |  2303 | `	}` |
|         - |  2304 | `	/* Emit the LOAD_LIST instruction */` |
|       107 |  2305 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|         - |  2306 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|         - |  2307 | `	 * For each nested entry, emit code to extract the sub-array` |
|         - |  2308 | `	 * at the corresponding index and recursively destructure it.` |
|         - |  2309 | `	 */` |
|       107 |  2310 | `	if( SySetUsed(&sNested) > 0 ){` |
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
|       107 |  2352 | `	SySetRelease(&sNested);` |
|         - |  2353 | `	/* Node successfully compiled */` |
|       107 |  2354 | `	return SXRET_OK;` |
|        67 |  2355 | `}` |
|        40 |  2356 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2357 | `{` |
|         - |  2358 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|        45 |  2359 | `	pGen->pIn += 2;` |
|        45 |  2360 | `	pGen->pEnd--;` |
|        20 |  2361 | `	SXUNUSED(iCompileFlag);` |
|        45 |  2362 | `	return GenStateCompileListBody(pGen);` |
|         5 |  2363 | `}` |
|        84 |  2364 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2365 | `{` |
|         - |  2366 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        86 |  2367 | `	pGen->pIn++;` |
|        86 |  2368 | `	pGen->pEnd--;` |
|        42 |  2369 | `	SXUNUSED(iCompileFlag);` |
|        86 |  2370 | `	return GenStateCompileListBody(pGen);` |
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
|       570 |  2402 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2403 | `{` |
|       575 |  2404 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2405 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2406 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2407 | `							  * one thread is allowed to compile the script.` |
|         - |  2408 | `						      */` |
|         - |  2409 | `	SyString sName;` |
|       575 |  2410 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2411 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2412 | `	sxu32 nKwLine;` |
|       575 |  2413 | `	sxi32 iFlags = 0;` |
|         - |  2414 | `	sxu32 nLen;` |
|         - |  2415 | `	sxi32 rc;` |
|       285 |  2416 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2417 |  |
|       575 |  2418 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       570 |  2419 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       575 |  2420 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2421 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        11 |  2422 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        11 |  2423 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         5 |  2424 | `	}` |
|       575 |  2425 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       575 |  2426 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2427 | `		pGen->pIn++;` |
|       ! 0 |  2428 | `	}` |
|         - |  2429 | `	/* Generate a unique name */` |
|       575 |  2430 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2431 | `	/* Make sure the generated name is unique */` |
|       575 |  2432 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2433 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2434 | `	}` |
|       575 |  2435 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2436 | `	/* Compile the lambda body */` |
|       575 |  2437 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       575 |  2438 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2439 | `		return SXERR_ABORT;` |
|         - |  2440 | `	}` |
|       575 |  2441 | `	if( pAnnonFunc ){` |
|       575 |  2442 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2443 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2444 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       575 |  2445 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2446 | `			return SXERR_ABORT;` |
|         - |  2447 | `		}` |
|       285 |  2448 | `	}` |
|         - |  2449 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2450 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2451 | `	 * the handler wraps either in a Closure instance. */` |
|       575 |  2452 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2453 | `	/* Node successfully compiled */` |
|       575 |  2454 | `	return SXRET_OK;` |
|       290 |  2455 | `}` |
|         - |  2456 | `/*` |
|         - |  2457 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2458 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2459 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2460 | ` */` |
|       218 |  2461 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2462 | `	ph7_gen_state *pGen,` |
|         - |  2463 | `	ph7_vm_func *pFunc,` |
|         - |  2464 | `	const char *zName,` |
|         - |  2465 | `	sxu32 nByte,` |
|         - |  2466 | `	SyString *aShadow,` |
|         - |  2467 | `	sxu32 nShadow)` |
|         3 |  2468 | `{` |
|         - |  2469 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2470 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2471 | `	sxu32 n, nEnv;` |
|         - |  2472 | `	char *zDup;` |
|       221 |  2473 | `	if( nByte == 0 ){` |
|       ! 0 |  2474 | `		return SXRET_OK;` |
|         - |  2475 | `	}` |
|       218 |  2476 | `	if( nByte == sizeof("this")-1` |
|       118 |  2477 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         3 |  2478 | `		return SXRET_OK;` |
|         - |  2479 | `	}` |
|       273 |  2480 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       204 |  2481 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       198 |  2482 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       153 |  2483 | `			return SXRET_OK;` |
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
|       494 |  2574 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2575 | `	ph7_gen_state *pGen,` |
|         - |  2576 | `	ph7_vm_func *pFunc,` |
|         - |  2577 | `	SyToken *pStart,` |
|         - |  2578 | `	SyToken *pEnd,` |
|         - |  2579 | `	SyString *aShadow,` |
|         - |  2580 | `	sxu32 nShadow)` |
|         4 |  2581 | `{` |
|       498 |  2582 | `	SyToken *pScan = pStart;` |
|         - |  2583 | `	sxi32 rc;` |
|      3340 |  2584 | `	while( pScan < pEnd ){` |
|      2846 |  2585 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
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
|      2740 |  2596 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
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
|      2716 |  2748 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      2522 |  2749 | `			pScan++;` |
|      2522 |  2750 | `			continue;` |
|         - |  2751 | `		}` |
|         - |  2752 | `		{` |
|         - |  2753 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       197 |  2754 | `			SyToken *pDollar = pScan;` |
|       291 |  2755 | `			while( &pDollar[1] < pEnd` |
|       197 |  2756 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2757 | `				pDollar++;` |
|       ! 0 |  2758 | `			}` |
|       197 |  2759 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2760 | `				break;` |
|         - |  2761 | `			}` |
|       197 |  2762 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2763 | `				pScan = pDollar + 1;` |
|       ! 0 |  2764 | `				continue;` |
|         - |  2765 | `			}` |
|       294 |  2766 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       194 |  2767 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|        97 |  2768 | `				aShadow,nShadow);` |
|       197 |  2769 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2770 | `				return SXERR_ABORT;` |
|         - |  2771 | `			}` |
|       197 |  2772 | `			pScan = pDollar + 2;` |
|         - |  2773 | `		}` |
|         3 |  2774 | `	}` |
|       498 |  2775 | `	return SXRET_OK;` |
|       251 |  2776 | `}` |
|         - |  2777 | `/*` |
|         - |  2778 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2779 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2780 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2781 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2782 | ` * $this is also made available.` |
|         - |  2783 | ` */` |
|       470 |  2784 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|       475 |  2801 | `	sxi32 iFlags = 0;` |
|       475 |  2802 | `	int bStatic = 0;` |
|         - |  2803 | `	sxi32 rc;` |
|         - |  2804 | `	sxu32 n;` |
|       235 |  2805 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2806 |  |
|       475 |  2807 | `	nLine = pGen->pIn->nLine;` |
|         - |  2808 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       475 |  2809 | `	pTokKw = pGen->pIn;` |
|         - |  2810 | `	/* Optional 'static' prefix */` |
|       470 |  2811 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       475 |  2812 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  2813 | `		bStatic = 1;` |
|         7 |  2814 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         7 |  2815 | `		pGen->pIn++;` |
|         3 |  2816 | `	}` |
|         - |  2817 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       470 |  2818 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       475 |  2819 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2820 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2821 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2822 | `		return SXERR_SYNTAX;` |
|         - |  2823 | `	}` |
|       475 |  2824 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2825 | `	/* Optional '&' — return by reference */` |
|       475 |  2826 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2827 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2828 | `		pGen->pIn++;` |
|       ! 0 |  2829 | `	}` |
|         - |  2830 | `	/* Expect '(' */` |
|       475 |  2831 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|       473 |  2842 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2843 | `	/* Delimit the parameter list */` |
|       473 |  2844 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       473 |  2845 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2846 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2847 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2848 | `		return SXERR_SYNTAX;` |
|         - |  2849 | `	}` |
|         - |  2850 | `	/* Allocate the function state */` |
|       471 |  2851 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       471 |  2852 | `	if( pFunc == 0 ){` |
|       ! 0 |  2853 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2854 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2855 | `		return SXERR_ABORT;` |
|         - |  2856 | `	}` |
|         - |  2857 | `	/* Generate a unique lambda name */` |
|       471 |  2858 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       471 |  2859 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2860 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2861 | `	}` |
|       471 |  2862 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       471 |  2863 | `	if( zDup == 0 ){` |
|       ! 0 |  2864 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2865 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2866 | `		return SXERR_ABORT;` |
|         - |  2867 | `	}` |
|       471 |  2868 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2869 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       471 |  2870 | `	pFunc->nLine = nLine;` |
|         - |  2871 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       471 |  2872 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2873 | `		return SXERR_ABORT;` |
|         - |  2874 | `	}` |
|         - |  2875 | `	/* Collect function arguments */` |
|       471 |  2876 | `	if( pGen->pIn < pSigEnd ){` |
|       126 |  2877 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       126 |  2878 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2879 | `			return SXERR_ABORT;` |
|         - |  2880 | `		}` |
|        61 |  2881 | `	}` |
|         - |  2882 | `	/* Point past ')' and parse optional return type */` |
|       471 |  2883 | `	pGen->pIn = &pSigEnd[1];` |
|       471 |  2884 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       471 |  2885 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2886 | `		return SXERR_ABORT;` |
|       471 |  2887 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2888 | `		return SXERR_SYNTAX;` |
|         - |  2889 | `	}` |
|         - |  2890 | `	/* Expect '=>' */` |
|       471 |  2891 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|       468 |  2902 | `	pGen->pIn++; /* Jump '=>' */` |
|       468 |  2903 | `	pBodyStart = pGen->pIn;` |
|       468 |  2904 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2905 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2906 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2907 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2908 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       468 |  2909 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2910 | `	{` |
|       468 |  2911 | `		SyString *aShadow = 0;` |
|       468 |  2912 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       468 |  2913 | `		if( nShadow > 0 ){` |
|       123 |  2914 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       120 |  2915 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       123 |  2916 | `			if( aShadow == 0 ){` |
|       ! 0 |  2917 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2918 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2919 | `				return SXERR_ABORT;` |
|         - |  2920 | `			}` |
|       279 |  2921 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       159 |  2922 | `				aShadow[n] = aArgs[n].sName;` |
|        81 |  2923 | `			}` |
|        60 |  2924 | `		}` |
|       700 |  2925 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       232 |  2926 | `			aShadow,nShadow);` |
|       468 |  2927 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2928 | `			return SXERR_ABORT;` |
|         - |  2929 | `		}` |
|         - |  2930 | `	}` |
|         - |  2931 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2932 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2933 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2934 | `	 * $this. */` |
|       468 |  2935 | `	if( !bStatic ){` |
|         - |  2936 | `		char *zThisDup;` |
|       462 |  2937 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       462 |  2938 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2939 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2940 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2941 | `			return SXERR_ABORT;` |
|         - |  2942 | `		}` |
|       462 |  2943 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       462 |  2944 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       462 |  2945 | `		sEnv.nIdx = SXU32_HIGH;` |
|       462 |  2946 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       462 |  2947 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       462 |  2948 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       229 |  2949 | `	}` |
|         - |  2950 | `	/* Arrow functions are always closures */` |
|       468 |  2951 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2952 | `	/* Compile the body expression as an implicit return */` |
|       700 |  2953 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       232 |  2954 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       468 |  2955 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2956 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2957 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2958 | `		return SXERR_ABORT;` |
|         - |  2959 | `	}` |
|       468 |  2960 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       468 |  2961 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       468 |  2962 | `	pSavedEnd = pGen->pEnd;` |
|       468 |  2963 | `	pGen->pIn = pBodyStart;` |
|       468 |  2964 | `	pGen->pEnd = pBodyEnd;` |
|       468 |  2965 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       468 |  2966 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2967 | `		return SXERR_ABORT;` |
|         - |  2968 | `	}` |
|         - |  2969 | `	/* The cursor stopped just past the body expression */` |
|       468 |  2970 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2971 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2972 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2973 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2974 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       468 |  2975 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       468 |  2976 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       468 |  2977 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       468 |  2978 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       468 |  2979 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2980 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       468 |  2981 | `	pGen->pIn = pBodyEnd;` |
|       468 |  2982 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2983 | `	/* Emit the load-closure instruction */` |
|       468 |  2984 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       468 |  2985 | `	return SXRET_OK;` |
|       240 |  2986 | `}` |
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
|        68 |  3268 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3269 | `{` |
|         - |  3270 | `	SyString *pName;` |
|         - |  3271 | `	sxu32 nKeyID;` |
|         - |  3272 | `	sxi32 rc;` |
|         - |  3273 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        73 |  3274 | `	pName = &pGen->pIn->sData;` |
|        73 |  3275 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        73 |  3276 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        73 |  3277 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
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
|        65 |  3312 | `		sxi32 nArg = 0;` |
|        65 |  3313 | `		sxu32 nIdx = 0;` |
|        65 |  3314 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        65 |  3315 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3316 | `			return SXERR_ABORT;` |
|        65 |  3317 | `		}else if(rc != SXERR_EMPTY ){` |
|        65 |  3318 | `			nArg = 1;` |
|        30 |  3319 | `		}` |
|        65 |  3320 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
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
|        65 |  3334 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        65 |  3335 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3336 | `	}` |
|         - |  3337 | `	/* Node successfully compiled */` |
|        73 |  3338 | `	return SXRET_OK;` |
|        39 |  3339 | `}` |
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
|  19148834 |  3361 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3362 | `{` |
|  19148839 |  3363 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3364 | `	sxi32 iVv;` |
|         - |  3365 | `	sxi32 iP1;` |
|         - |  3366 | `	void *p3;` |
|         - |  3367 | `	sxi32 rc;` |
|  19148839 |  3368 | `	iVv = -1; /* Variable variable counter */` |
|  38297685 |  3369 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  19148851 |  3370 | `		pGen->pIn++;` |
|  19148851 |  3371 | `		iVv++;` |
|         5 |  3372 | `	}` |
|  19148839 |  3373 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3374 | `		/* Invalid variable name */` |
|       ! 0 |  3375 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3376 | `		if( rc == SXERR_ABORT ){` |
|         - |  3377 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3378 | `			return SXERR_ABORT;` |
|         - |  3379 | `		}` |
|       ! 0 |  3380 | `		return SXRET_OK;` |
|         - |  3381 | `	}` |
|  19148839 |  3382 | `	p3  = 0;` |
|  19148839 |  3383 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  19148823 |  3409 | `		char *zName = 0;` |
|         - |  3410 | `		/* Extract variable name */` |
|  19148823 |  3411 | `		pName = &pGen->pIn->sData;` |
|         - |  3412 | `		/* Advance the stream cursor */` |
|  19148823 |  3413 | `		pGen->pIn++;` |
|  19148823 |  3414 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  19148823 |  3415 | `		if( pEntry == 0 ){` |
|         - |  3416 | `			/* Duplicate name */` |
|   1151403 |  3417 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   1151403 |  3418 | `			if( zName == 0 ){` |
|       ! 0 |  3419 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3420 | `				return SXERR_ABORT;` |
|         - |  3421 | `			}` |
|         - |  3422 | `			/* Install in the hashtable */` |
|   1151403 |  3423 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    575704 |  3424 | `		}else{` |
|         - |  3425 | `			/* Name already available */` |
|  17997425 |  3426 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3427 | `		}` |
|  19148823 |  3428 | `		p3 = (void *)zName;` |
|         - |  3429 | `	}` |
|  19148835 |  3430 | `	iP1 = 0;` |
|  19148835 |  3431 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   5660805 |  3432 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3433 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   5656965 |  3434 | `			iP1 = 1;` |
|   2828480 |  3435 | `		}` |
|   2830400 |  3436 | `	}` |
|         - |  3437 | `	/* Emit the load instruction */` |
|  19148835 |  3438 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  19148847 |  3439 | `	while( iVv > 0 ){` |
|        13 |  3440 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3441 | `		iVv--;` |
|         1 |  3442 | `	}` |
|         - |  3443 | `	/* Node successfully compiled */` |
|  19148835 |  3444 | `	return SXRET_OK;` |
|   9574422 |  3445 | `}` |
|         - |  3446 | `/*` |
|         - |  3447 | ` * Load a literal.` |
|         - |  3448 | ` */` |
|  11872718 |  3449 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3450 | `{` |
|  11872723 |  3451 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3452 | `	ph7_value *pObj;` |
|         - |  3453 | `	SyString *pStr;` |
|         - |  3454 | `	sxu32 nIdx;` |
|         - |  3455 | `	/* Extract token value */` |
|  11872723 |  3456 | `	pStr = &pToken->sData;` |
|         - |  3457 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  11872723 |  3458 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2232661 |  3459 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3460 | `			/* NULL constant are always indexed at 0 */` |
|    977861 |  3461 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    977861 |  3462 | `			return SXRET_OK;` |
|   1254805 |  3463 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3464 | `			/* TRUE constant are always indexed at 1 */` |
|    321751 |  3465 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    321751 |  3466 | `			return SXRET_OK;` |
|         5 |  3467 | `		}` |
|  11095118 |  3468 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1977048 |  3469 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3470 | `			/* FALSE constant are always indexed at 2 */` |
|    710437 |  3471 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    710437 |  3472 | `			return SXRET_OK;` |
|   9340796 |  3473 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    822322 |  3474 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3475 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      3829 |  3476 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3829 |  3477 | `			if( pObj == 0 ){` |
|       ! 0 |  3478 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3479 | `				return SXERR_ABORT;` |
|         - |  3480 | `			}` |
|      3829 |  3481 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3482 | `			/* Emit the load constant instruction */` |
|      3829 |  3483 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3829 |  3484 | `			return SXRET_OK;` |
|   9335055 |  3485 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   1272545 |  3486 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|   9375964 |  3487 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|    908084 |  3488 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|         - |  3489 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|         - |  3490 | `			 * file being compiled (where the token is written), NOT the runtime` |
|         - |  3491 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|         - |  3492 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|         - |  3493 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|         - |  3494 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|      3911 |  3495 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|      3911 |  3496 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      3911 |  3497 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3911 |  3498 | `			if( pObj == 0 ){` |
|       ! 0 |  3499 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3500 | `				return SXERR_ABORT;` |
|         - |  3501 | `			}` |
|      3911 |  3502 | `			if( pFile && pFile->nByte > 0 ){` |
|        95 |  3503 | `				if( bDir ){` |
|         - |  3504 | `					const char *zDir;` |
|         - |  3505 | `					int nLen;` |
|         - |  3506 | `					SyString sDir;` |
|        48 |  3507 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|        48 |  3508 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|        48 |  3509 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|        26 |  3510 | `				}else{` |
|        51 |  3511 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|         - |  3512 | `				}` |
|        50 |  3513 | `			}else{` |
|         - |  3514 | `				SyString sMem;` |
|      3821 |  3515 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|      3821 |  3516 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|         - |  3517 | `			}` |
|      3911 |  3518 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3911 |  3519 | `			return SXRET_OK;` |
|   9038719 |  3520 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    233628 |  3521 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3522 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         7 |  3523 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3524 | `			if( pObj == 0 ){` |
|       ! 0 |  3525 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3526 | `				return SXERR_ABORT;` |
|         - |  3527 | `			}` |
|         7 |  3528 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3529 | `				SyString sNs;` |
|         7 |  3530 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         7 |  3531 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         4 |  3532 | `			}else{` |
|       ! 0 |  3533 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3534 | `			}` |
|         7 |  3535 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         7 |  3536 | `			return SXRET_OK;` |
|   9038937 |  3537 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    447548 |  3538 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   9135344 |  3539 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    426914 |  3540 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3541 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3542 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3543 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3544 | `				/* Point to the upper block */` |
|        11 |  3545 | `				pBlock = pBlock->pParent;` |
|         1 |  3546 | `			}` |
|        11 |  3547 | `			if( pBlock == 0 ){` |
|         - |  3548 | `				/* Called in the global scope,load NULL */` |
|         5 |  3549 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3550 | `			}else{` |
|         - |  3551 | `				/* Extract the target function/method */` |
|         7 |  3552 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3553 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|         7 |  3554 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3555 | `				if( pObj == 0 ){` |
|       ! 0 |  3556 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3557 | `					return SXERR_ABORT;` |
|         - |  3558 | `				}` |
|         - |  3559 | `				/*` |
|         - |  3560 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|         - |  3561 | `				 * function name inside a plain function (php does not answer "" there —` |
|         - |  3562 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|         - |  3563 | `				 * unqualified in every method).` |
|         - |  3564 | `				 */` |
|         8 |  3565 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 |  3566 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         - |  3567 | `					SyBlob sQual;` |
|         - |  3568 | `					SyString sOut;` |
|         3 |  3569 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|         3 |  3570 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|         3 |  3571 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|         3 |  3572 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|         3 |  3573 | `					SyBlobRelease(&sQual);` |
|         2 |  3574 | `				}else{` |
|         5 |  3575 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3576 | `				}` |
|         - |  3577 | `				/* Emit the load constant instruction */` |
|         7 |  3578 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3579 | `			}` |
|        11 |  3580 | `			return SXRET_OK;` |
|         - |  3581 | `	}` |
|         - |  3582 | `	/* Query literal table */` |
|   9854943 |  3583 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3584 | `		ph7_value *pLitObj;` |
|         - |  3585 | `		/* Unknown literal,install it in the literal table */` |
|   1876249 |  3586 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1876249 |  3587 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3588 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3589 | `			return SXERR_ABORT;` |
|         - |  3590 | `		}` |
|   1876249 |  3591 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1876249 |  3592 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    938122 |  3593 | `	}` |
|         - |  3594 | `	/* Emit the load constant instruction */` |
|   9854943 |  3595 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9854943 |  3596 | `	return SXRET_OK;` |
|   5936364 |  3597 | `}` |
|         - |  3598 | `/*` |
|         - |  3599 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3600 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3601 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3602 | ` * Otherwise, load the simple literal directly.` |
|         - |  3603 | ` */` |
|  11876602 |  3604 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3605 | `{` |
|         - |  3606 | `	sxi32 rc;` |
|  11876607 |  3607 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3608 | `		return SXRET_OK;` |
|         - |  3609 | `	}` |
|         - |  3610 | `	/* Check if this is a multi-token namespace path */` |
|  11876607 |  3611 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3612 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3889 |  3613 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3889 |  3614 | `		int isAbsolute = 0;` |
|      3889 |  3615 | `		SyBlobReset(pWorker);` |
|         - |  3616 | `		/* Check for leading backslash (absolute path) */` |
|      3889 |  3617 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3887 |  3618 | `			isAbsolute = 1;` |
|      3887 |  3619 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1941 |  3620 | `		}` |
|         - |  3621 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3889 |  3622 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3623 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3624 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3625 | `		}` |
|         - |  3626 | `		/* Collect all path components */` |
|      4001 |  3627 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4001 |  3628 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        60 |  3629 | `				SyBlobAppend(pWorker,"\\",1);` |
|        32 |  3630 | `			}else{` |
|      3945 |  3631 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3632 | `			}` |
|      4001 |  3633 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3889 |  3634 | `				pGen->pIn++;` |
|      3889 |  3635 | `				break;` |
|         - |  3636 | `			}` |
|       116 |  3637 | `			pGen->pIn++;` |
|         4 |  3638 | `		}` |
|      3889 |  3639 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3640 | `			ph7_value *pObj;` |
|         - |  3641 | `			SyString sPath;` |
|         - |  3642 | `			sxu32 nIdx;` |
|      3889 |  3643 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3644 | `			/* Install in the literal table */` |
|      3889 |  3645 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3841 |  3646 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3841 |  3647 | `				if( pObj == 0 ){` |
|       ! 0 |  3648 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3649 | `					return SXERR_ABORT;` |
|         - |  3650 | `				}` |
|      3841 |  3651 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3841 |  3652 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1918 |  3653 | `			}` |
|         - |  3654 | `			/* Emit the load constant instruction.` |
|         - |  3655 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3656 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5831 |  3657 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1942 |  3658 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1942 |  3659 | `				nIdx,0,0);` |
|      3889 |  3660 | `			return SXRET_OK;` |
|         - |  3661 | `		}` |
|       ! 0 |  3662 | `	}` |
|         - |  3663 | `	/* Single-token literal: load directly */` |
|  11872723 |  3664 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11872723 |  3665 | `	return rc;` |
|   5938306 |  3666 | `}` |
|         - |  3667 | `/*` |
|         - |  3668 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3669 | ` */` |
|         - |  3670 | `/*` |
|         - |  3671 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3672 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3673 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3674 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3675 | ` */` |
|       ! 0 |  3676 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3677 | `{` |
|       ! 0 |  3678 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3679 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3680 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3681 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3682 | `}` |
|  11876602 |  3683 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3684 | `{` |
|         - |  3685 | `	sxi32 rc;` |
|  11876607 |  3686 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11876607 |  3687 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3688 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3689 | `		return rc;` |
|         - |  3690 | `	}` |
|         - |  3691 | `	/* Node successfully compiled */` |
|  11876607 |  3692 | `	return SXRET_OK;` |
|   5938306 |  3693 | `}` |
|         - |  3694 | `/*` |
|         - |  3695 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3696 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3697 | ` */` |
|         8 |  3698 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3699 | `{` |
|         - |  3700 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3701 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3702 | `		pGen->pIn++;` |
|         1 |  3703 | `	}` |
|         9 |  3704 | `	return SXRET_OK;` |
|         1 |  3705 | `}` |
|         - |  3706 | `/*` |
|         - |  3707 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3708 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3709 | ` */` |
|    290240 |  3710 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3711 | `{` |
|    290245 |  3712 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3865 |  3713 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3714 | `			return TRUE;` |
|      3863 |  3715 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3716 | `			return TRUE;` |
|         5 |  3717 | `		}` |
|    288312 |  3718 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7657 |  3719 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3720 | `			return TRUE;` |
|         - |  3721 | `		}` |
|      3825 |  3722 | `	}` |
|         - |  3723 | `	/* Not a reserved constant */` |
|    290237 |  3724 | `	return FALSE;` |
|    145125 |  3725 | `}` |
|         - |  3726 | `/*` |
|         - |  3727 | ` * Compile the 'const' statement.` |
|         - |  3728 | ` * According to the PHP language reference` |
|         - |  3729 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3730 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3731 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3732 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3733 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3734 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3735 | ` *  Syntax` |
|         - |  3736 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3737 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3738 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3739 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3740 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3741 | ` *  to get a list of all defined constants.` |
|         - |  3742 | ` *` |
|         - |  3743 | ` * Symisc eXtension.` |
|         - |  3744 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3745 | ` *  would allow only simple scalar value.` |
|         - |  3746 | ` *  Example` |
|         - |  3747 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3748 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3749 | ` */` |
|        48 |  3750 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3751 | `{` |
|         - |  3752 | `	SySet *pConsCode,*pInstrContainer;` |
|        53 |  3753 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3754 | `	SyString *pName;` |
|         - |  3755 | `	sxi32 rc;` |
|        53 |  3756 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        53 |  3757 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3758 | `		/* Invalid constant name */` |
|         8 |  3759 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|         8 |  3760 | `		if( rc == SXERR_ABORT ){` |
|         - |  3761 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3762 | `			return SXERR_ABORT;` |
|         - |  3763 | `		}` |
|         8 |  3764 | `		goto Synchronize;` |
|         - |  3765 | `	}` |
|         - |  3766 | `	/* Peek constant name */` |
|        47 |  3767 | `	pName = &pGen->pIn->sData;` |
|         - |  3768 | `	/* Make sure the constant name isn't reserved */` |
|        47 |  3769 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3770 | `		/* Reserved constant */` |
|        10 |  3771 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        10 |  3772 | `		if( rc == SXERR_ABORT ){` |
|         - |  3773 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3774 | `			return SXERR_ABORT;` |
|         - |  3775 | `		}` |
|        10 |  3776 | `		goto Synchronize;` |
|         - |  3777 | `	}` |
|        38 |  3778 | `	pGen->pIn++;` |
|        38 |  3779 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3780 | `		/* Invalid statement*/` |
|         6 |  3781 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|         6 |  3782 | `		if( rc == SXERR_ABORT ){` |
|         - |  3783 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3784 | `			return SXERR_ABORT;` |
|         - |  3785 | `		}` |
|         6 |  3786 | `		goto Synchronize;` |
|         - |  3787 | `	}` |
|        32 |  3788 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3789 | `	/* Allocate a new constant value container */` |
|        32 |  3790 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        32 |  3791 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3792 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3793 | `		return SXERR_ABORT;` |
|         - |  3794 | `	}` |
|        32 |  3795 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3796 | `	/* Swap bytecode container */` |
|        32 |  3797 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        32 |  3798 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3799 | `	/* Compile constant value */` |
|        32 |  3800 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3801 | `	/* Emit the done instruction */` |
|        32 |  3802 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        32 |  3803 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        32 |  3804 | `	if( rc == SXERR_ABORT ){` |
|         - |  3805 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3806 | `		return SXERR_ABORT;` |
|         - |  3807 | `	}` |
|        32 |  3808 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3809 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3810 | `	{` |
|         - |  3811 | `		SyBlob sFQN;` |
|         - |  3812 | `		SyString sFQNStr;` |
|        32 |  3813 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        32 |  3814 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        32 |  3815 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        47 |  3816 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        30 |  3817 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        32 |  3818 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3819 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3820 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3821 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3822 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3823 | `			if( pCEntry ){` |
|         5 |  3824 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3825 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3826 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3827 | `					return SXERR_ABORT;` |
|         - |  3828 | `				}` |
|         2 |  3829 | `			}` |
|         2 |  3830 | `		}` |
|        32 |  3831 | `		SyBlobRelease(&sFQN);` |
|         - |  3832 | `	}` |
|        32 |  3833 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3834 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3835 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3836 | `	}` |
|        32 |  3837 | `	return SXRET_OK;` |
|         9 |  3838 | `Synchronize:` |
|         - |  3839 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3840 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        41 |  3841 | `		pGen->pIn++;` |
|         3 |  3842 | `	}` |
|        22 |  3843 | `	return SXRET_OK;` |
|        29 |  3844 | `}` |
|         - |  3845 | `/*` |
|         - |  3846 | ` * Compile the 'continue' statement.` |
|         - |  3847 | ` * According to the PHP language reference` |
|         - |  3848 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3849 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3850 | ` *  iteration.` |
|         - |  3851 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3852 | ` *  the purposes of continue.` |
|         - |  3853 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3854 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3855 | ` *  Note:` |
|         - |  3856 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3857 | ` */` |
|         - |  3858 | `/*` |
|         - |  3859 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3860 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3861 | ` * break/continue crosses a try boundary.` |
|         - |  3862 | ` *` |
|         - |  3863 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3864 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3865 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3866 | ` */` |
|    148978 |  3867 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3868 | `{` |
|    148983 |  3869 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    148983 |  3870 | `	int nInlineTry = 0;` |
|    672021 |  3871 | `	while( pBlock && pBlock != pTarget ){` |
|    523043 |  3872 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3873 | `			if( pBlock->pUserData ){` |
|         - |  3874 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3875 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3876 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3877 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3878 | `				if( pGen->bInGenerator ){` |
|         3 |  3879 | `					nInlineTry++;` |
|         2 |  3880 | `				}else{` |
|         3 |  3881 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3882 | `				}` |
|         4 |  3883 | `			}else{` |
|         - |  3884 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3885 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3886 | `				break;` |
|         - |  3887 | `			}` |
|         2 |  3888 | `		}` |
|    523043 |  3889 | `		pBlock = pBlock->pParent;` |
|         5 |  3890 | `	}` |
|    148983 |  3891 | `	return nInlineTry;` |
|         5 |  3892 | `}` |
|     84002 |  3893 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3894 | `{` |
|         - |  3895 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3896 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3897 | `	sxu32 nLineLocal;` |
|         - |  3898 | `	sxi32 rc;` |
|     84007 |  3899 | `	nLineLocal = pGen->pIn->nLine;` |
|     84007 |  3900 | `	iLevel = 0;` |
|         - |  3901 | `	/* Jump the 'continue' keyword */` |
|     84007 |  3902 | `	pGen->pIn++;` |
|     84007 |  3903 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3904 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3905 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3906 | `		 */` |
|         - |  3907 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3908 | `		char *zAlloc = 0;` |
|         - |  3909 | `		SyString sNum;` |
|        17 |  3910 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3911 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3912 | `			return SXERR_ABORT;` |
|         - |  3913 | `		}` |
|        17 |  3914 | `		if( rc == SXRET_OK ){` |
|        20 |  3915 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3916 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3917 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3918 | `				return SXERR_ABORT;` |
|         - |  3919 | `			}` |
|        14 |  3920 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3921 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3922 | `		}` |
|        17 |  3923 | `		if( iLevel < 2 ){` |
|         3 |  3924 | `			iLevel = 0;` |
|         1 |  3925 | `		}` |
|        17 |  3926 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3927 | `	}` |
|         - |  3928 | `	/* Point to the target loop */` |
|     84007 |  3929 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     84007 |  3930 | `	if( pLoop == 0 ){` |
|         - |  3931 | `		/* Illegal continue */` |
|        12 |  3932 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        12 |  3933 | `		if( rc == SXERR_ABORT ){` |
|         - |  3934 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3935 | `			return SXERR_ABORT;` |
|         - |  3936 | `		}` |
|         7 |  3937 | `	}else{` |
|     83997 |  3938 | `		sxu32 nInstrIdx = 0;` |
|         - |  3939 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     83997 |  3940 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3941 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3942 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     83997 |  3943 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     83997 |  3944 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3945 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|         - |  3946 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|         - |  3947 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|         - |  3948 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|         5 |  3949 | `			if( iLevel < 1 ){` |
|         5 |  3950 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|         - |  3951 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|         - |  3952 | `					" Did you mean to use \"continue 2\"?");` |
|         2 |  3953 | `			}` |
|         5 |  3954 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3955 | `			if( rc == SXRET_OK ){` |
|         5 |  3956 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3957 | `			}` |
|         3 |  3958 | `		}else{` |
|         - |  3959 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     83993 |  3960 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     83993 |  3961 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  3962 | `				JumpFixup sJumpFix;` |
|         - |  3963 | `				/* Post-continue */` |
|     26729 |  3964 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     26729 |  3965 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     26729 |  3966 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     13362 |  3967 | `			}` |
|         - |  3968 | `		}` |
|         - |  3969 | `	}` |
|     84007 |  3970 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3971 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3972 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  3973 | `	}` |
|         - |  3974 | `	/* Statement successfully compiled */` |
|     84007 |  3975 | `	return SXRET_OK;` |
|     42006 |  3976 | `}` |
|         - |  3977 | `/*` |
|         - |  3978 | ` * Compile the 'break' statement.` |
|         - |  3979 | ` * According to the PHP language reference` |
|         - |  3980 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  3981 | ` *  structure.` |
|         - |  3982 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  3983 | ` *  enclosing structures are to be broken out of.` |
|         - |  3984 | ` */` |
|     65002 |  3985 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  3986 | `{` |
|         - |  3987 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3988 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3989 | `	sxi32 rc;` |
|     65007 |  3990 | `	iLevel = 0;` |
|         - |  3991 | `	/* Jump the 'break' keyword */` |
|     65007 |  3992 | `	pGen->pIn++;` |
|     65007 |  3993 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3994 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3995 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3996 | `		 */` |
|         - |  3997 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        18 |  3998 | `		char *zAlloc = 0;` |
|         - |  3999 | `		SyString sNum;` |
|        18 |  4000 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        18 |  4001 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4002 | `			return SXERR_ABORT;` |
|         - |  4003 | `		}` |
|        18 |  4004 | `		if( rc == SXRET_OK ){` |
|        21 |  4005 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  4006 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  4007 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  4008 | `				return SXERR_ABORT;` |
|         - |  4009 | `			}` |
|        15 |  4010 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  4011 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  4012 | `		}` |
|        18 |  4013 | `		if( iLevel < 2 ){` |
|         3 |  4014 | `			iLevel = 0;` |
|         1 |  4015 | `		}` |
|        18 |  4016 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  4017 | `	}` |
|         - |  4018 | `	/* Extract the target loop */` |
|     65007 |  4019 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     65007 |  4020 | `	if( pLoop == 0 ){` |
|         - |  4021 | `		/* Illegal break */` |
|        18 |  4022 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        18 |  4023 | `		if( rc == SXERR_ABORT ){` |
|         - |  4024 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4025 | `			return SXERR_ABORT;` |
|         - |  4026 | `		}` |
|        10 |  4027 | `	}else{` |
|         - |  4028 | `		sxu32 nInstrIdx;` |
|         - |  4029 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     64991 |  4030 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  4031 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     64991 |  4032 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     64991 |  4033 | `		if( rc == SXRET_OK ){` |
|         - |  4034 | `			/* Fix the jump later when the jump destination is resolved */` |
|     64991 |  4035 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     32493 |  4036 | `		}` |
|         - |  4037 | `	}` |
|     65007 |  4038 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4039 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4040 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  4041 | `	}` |
|         - |  4042 | `	/* Statement successfully compiled */` |
|     65007 |  4043 | `	return SXRET_OK;` |
|     32506 |  4044 | `}` |
|         - |  4045 | `/*` |
|         - |  4046 | ` * Compile or record a label.` |
|         - |  4047 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  4048 | ` * Example` |
|         - |  4049 | ` *  goto LABEL;` |
|         - |  4050 | ` *   echo 'Foo';` |
|         - |  4051 | ` *  LABEL:` |
|         - |  4052 | ` *   echo 'Bar';` |
|         - |  4053 | ` */` |
|       112 |  4054 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  4055 | `{` |
|         - |  4056 | `	GenBlock *pBlock;` |
|         - |  4057 | `	Label sLabel;` |
|         - |  4058 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|         - |  4059 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|         - |  4060 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|         - |  4061 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|         - |  4062 | `	{` |
|       117 |  4063 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4064 | `		char *zDup;` |
|         - |  4065 | `		/* Initialize label fields */` |
|       117 |  4066 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4067 | `		/* Duplicate label name */` |
|       117 |  4068 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       117 |  4069 | `		if( zDup == 0 ){` |
|       ! 0 |  4070 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4071 | `			return SXERR_ABORT;` |
|         - |  4072 | `		}` |
|       117 |  4073 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       117 |  4074 | `		sLabel.bRef  = FALSE;` |
|       117 |  4075 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       117 |  4076 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       117 |  4077 | `		pBlock = pGen->pCurrent;` |
|       233 |  4078 | `		while( pBlock ){` |
|       143 |  4079 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        26 |  4080 | `				break;` |
|         - |  4081 | `			}` |
|         - |  4082 | `			/* Point to the upper block */` |
|       121 |  4083 | `			pBlock = pBlock->pParent;` |
|         5 |  4084 | `		}` |
|       117 |  4085 | `		if( pBlock ){` |
|        26 |  4086 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        15 |  4087 | `		}else{` |
|        95 |  4088 | `			sLabel.pFunc = 0;` |
|         - |  4089 | `		}` |
|         - |  4090 | `		/* Insert in label set */` |
|       117 |  4091 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  4092 | `	}` |
|       117 |  4093 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  4094 | `	return SXRET_OK;` |
|        61 |  4095 | `}` |
|         - |  4096 | `/*` |
|         - |  4097 | ` * Compile the so hated 'goto' statement.` |
|         - |  4098 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  4099 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  4100 | ` * a compiler it has to do this.` |
|         - |  4101 | ` * According to the PHP language reference manual` |
|         - |  4102 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  4103 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  4104 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  4105 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  4106 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  4107 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  4108 | ` *   of a multi-level break` |
|         - |  4109 | ` */` |
|       152 |  4110 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  4111 | `{` |
|         - |  4112 | `	JumpFixup sJump;` |
|         - |  4113 | `	sxi32 rc;` |
|       157 |  4114 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  4115 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  4116 | `		/* Missing label */` |
|       ! 0 |  4117 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  4118 | `		if( rc == SXERR_ABORT ){` |
|         - |  4119 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4120 | `			return SXERR_ABORT;` |
|         - |  4121 | `		}` |
|       ! 0 |  4122 | `		return SXRET_OK;` |
|         - |  4123 | `	}` |
|       157 |  4124 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         5 |  4125 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|         5 |  4126 | `		if( rc == SXERR_ABORT ){` |
|         - |  4127 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4128 | `			return SXERR_ABORT;` |
|         - |  4129 | `		}` |
|         3 |  4130 | `	}else{` |
|       153 |  4131 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4132 | `		GenBlock *pBlock;` |
|         - |  4133 | `		char *zDup;` |
|         - |  4134 | `		/* Prepare the jump destination */` |
|       153 |  4135 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  4136 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  4137 | `		/* Duplicate label name */` |
|       153 |  4138 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  4139 | `		if( zDup == 0 ){` |
|       ! 0 |  4140 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4141 | `			return SXERR_ABORT;` |
|         - |  4142 | `		}` |
|       153 |  4143 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|         - |  4144 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|       153 |  4145 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|         - |  4146 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|         - |  4147 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|       153 |  4148 | `		pBlock = pGen->pCurrent;` |
|       327 |  4149 | `		while( pBlock ){` |
|       205 |  4150 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|        30 |  4151 | `				break;` |
|         - |  4152 | `			}` |
|         - |  4153 | `			/* Point to the upper block */` |
|       179 |  4154 | `			pBlock = pBlock->pParent;` |
|         5 |  4155 | `		}` |
|       153 |  4156 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        30 |  4157 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        17 |  4158 | `		}else{` |
|       127 |  4159 | `			sJump.pFunc = 0;` |
|         - |  4160 | `		}` |
|         - |  4161 | `		/* Emit the unconditional jump */` |
|       153 |  4162 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  4163 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  4164 | `		}` |
|         - |  4165 | `	}` |
|       157 |  4166 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  4167 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  4168 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  4169 | `	}` |
|         - |  4170 | `	/* Statement successfully compiled */` |
|       157 |  4171 | `	return SXRET_OK;` |
|        81 |  4172 | `}` |
|         - |  4173 | `/*` |
|         - |  4174 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  4175 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  4176 | ` * failure.` |
|         - |  4177 | ` */` |
|        20 |  4178 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         1 |  4179 | `{` |
|         - |  4180 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  4181 | `	sxu32 nRawObj;` |
|        10 |  4182 | `	sxu32 nObjIdx;` |
|         - |  4183 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  4184 | `	 * a PHP block.` |
|         - |  4185 | `	 */` |
|        10 |  4186 | `Consume:` |
|        21 |  4187 | `	nRawObj = nObjIdx = 0;` |
|        21 |  4188 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  4189 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  4190 | `		if( pRawObj == 0 ){` |
|       ! 0 |  4191 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4192 | `			return SXERR_ABORT;` |
|         - |  4193 | `		}` |
|         - |  4194 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  4195 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  4196 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  4197 | `		++nRawObj;` |
|       ! 0 |  4198 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  4199 | `	}` |
|        21 |  4200 | `	if( nRawObj > 0 ){` |
|         - |  4201 | `		/* Emit the consume instruction */` |
|       ! 0 |  4202 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4203 | `	}` |
|        21 |  4204 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  4205 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  4206 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  4207 | `		SySetReset(pTokenSet);` |
|       ! 0 |  4208 | `		SySetReset(&pGen->aTrivia);` |
|         - |  4209 | `		/* Tokenize input */` |
|       ! 0 |  4210 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  4211 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  4212 | `		/* Point to the fresh token stream */` |
|       ! 0 |  4213 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  4214 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  4215 | `		/* Advance the stream cursor */` |
|       ! 0 |  4216 | `		pGen->pRawIn++;` |
|         - |  4217 | `		/* TICKET 1433-011 */` |
|       ! 0 |  4218 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4219 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4220 | `			sxi32 rc;` |
|         - |  4221 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4222 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4223 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4224 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4225 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4226 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4227 | `				return SXERR_ABORT;` |
|       ! 0 |  4228 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4229 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4230 | `			}` |
|       ! 0 |  4231 | `			goto Consume;` |
|         - |  4232 | `		}` |
|       ! 0 |  4233 | `	}else{` |
|         - |  4234 | `		/* No more chunks to process */` |
|        21 |  4235 | `		pGen->pIn = pGen->pEnd;` |
|        21 |  4236 | `		return SXERR_EOF;` |
|         - |  4237 | `	}` |
|       ! 0 |  4238 | `	return SXRET_OK;` |
|        11 |  4239 | `}` |
|         - |  4240 | `/*` |
|         - |  4241 | ` * Compile a PHP block.` |
|         - |  4242 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4243 | ` * optionally delimited by braces {}.` |
|         - |  4244 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4245 | ` * and this function takes care of generating the appropriate error` |
|         - |  4246 | ` * message.` |
|         - |  4247 | ` */` |
|   5990734 |  4248 | `static sxi32 PH7_CompileBlock(` |
|         - |  4249 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4250 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4251 | `	)` |
|         5 |  4252 | `{` |
|         - |  4253 | `	sxi32 rc;` |
|         - |  4254 | `	sxu32 nLine;` |
|   5990739 |  4255 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5966753 |  4256 | `		nLine = pGen->pIn->nLine;` |
|   5966753 |  4257 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5966753 |  4258 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4259 | `			return SXERR_ABORT;` |
|         - |  4260 | `		}` |
|   5966753 |  4261 | `		pGen->pIn++;` |
|         - |  4262 | `		/* Compile until we hit the closing braces '}' */` |
|   8812815 |  4263 | `		for(;;){` |
|  17625635 |  4264 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        21 |  4265 | `				rc = GenStateNextChunk(&(*pGen));` |
|        21 |  4266 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4267 | `			 	   return SXERR_ABORT;` |
|         - |  4268 | `				}` |
|        21 |  4269 | `				if( rc == SXERR_EOF ){` |
|         - |  4270 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4271 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        21 |  4272 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        21 |  4273 | `					break;` |
|         - |  4274 | `				}` |
|       ! 0 |  4275 | `			}` |
|  17625615 |  4276 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4277 | `				/* Closing braces found,break immediately*/` |
|   5966733 |  4278 | `				pGen->pIn++;` |
|   5966733 |  4279 | `				break;` |
|         - |  4280 | `			}` |
|         - |  4281 | `			/* Compile a single statement */` |
|  11658887 |  4282 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  11658887 |  4283 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4284 | `				return SXERR_ABORT;` |
|         - |  4285 | `			}` |
|         5 |  4286 | `		}` |
|   5966753 |  4287 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   3007365 |  4288 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4289 | `		pGen->pIn++;` |
|       ! 0 |  4290 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4291 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4292 | `			return SXERR_ABORT;` |
|         - |  4293 | `		}` |
|         - |  4294 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4295 | `		for(;;){` |
|       ! 0 |  4296 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4297 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4298 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4299 | `			 	   return SXERR_ABORT;` |
|         - |  4300 | `				}` |
|       ! 0 |  4301 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4302 | `					/* No more token to process */` |
|       ! 0 |  4303 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4304 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4305 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4306 | `					}` |
|       ! 0 |  4307 | `					break;` |
|         - |  4308 | `				}` |
|       ! 0 |  4309 | `			}` |
|       ! 0 |  4310 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4311 | `				sxi32 nKwrd;` |
|         - |  4312 | `				/* Keyword found */` |
|       ! 0 |  4313 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4314 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4315 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4316 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4317 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4318 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4319 | `						}` |
|       ! 0 |  4320 | `						break;` |
|         - |  4321 | `				}` |
|       ! 0 |  4322 | `			}` |
|         - |  4323 | `			/* Compile a single statement */` |
|       ! 0 |  4324 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4325 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4326 | `				return SXERR_ABORT;` |
|         - |  4327 | `			}` |
|       ! 0 |  4328 | `		}` |
|       ! 0 |  4329 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4330 | `	}else{` |
|         - |  4331 | `		/* Compile a single statement */` |
|     23991 |  4332 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     23991 |  4333 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4334 | `			return SXERR_ABORT;` |
|         - |  4335 | `		}` |
|         - |  4336 | `	}` |
|         - |  4337 | `	/* Jump trailing semi-colons ';' */` |
|   5990739 |  4338 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4339 | `		pGen->pIn++;` |
|       ! 0 |  4340 | `	}` |
|   5990739 |  4341 | `	return SXRET_OK;` |
|   2995372 |  4342 | `}` |
|         - |  4343 | `/*` |
|         - |  4344 | ` * Compile the gentle 'while' statement.` |
|         - |  4345 | ` * According to the PHP language reference` |
|         - |  4346 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4347 | ` *  The basic form of a while statement is:` |
|         - |  4348 | ` *  while (expr)` |
|         - |  4349 | ` *   statement` |
|         - |  4350 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4351 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4352 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4353 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4354 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4355 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4356 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4357 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4358 | ` *  while (expr):` |
|         - |  4359 | ` *    statement` |
|         - |  4360 | ` *   endwhile;` |
|         - |  4361 | ` */` |
|     65016 |  4362 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4363 | `{` |
|     65021 |  4364 | `	GenBlock *pWhileBlock = 0;` |
|     65021 |  4365 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4366 | `	sxu32 nFalseJump;` |
|         - |  4367 | `	sxu32 nLine;` |
|         - |  4368 | `	sxi32 rc;` |
|     65021 |  4369 | `	nLine = pGen->pIn->nLine;` |
|         - |  4370 | `	/* Jump the 'while' keyword */` |
|     65021 |  4371 | `	pGen->pIn++;` |
|     65021 |  4372 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4373 | `		/* Syntax error */` |
|       ! 0 |  4374 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4375 | `		if( rc == SXERR_ABORT ){` |
|         - |  4376 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4377 | `			return SXERR_ABORT;` |
|         - |  4378 | `		}` |
|       ! 0 |  4379 | `		goto Synchronize;` |
|         - |  4380 | `	}` |
|         - |  4381 | `	/* Jump the left parenthesis '(' */` |
|     65021 |  4382 | `	pGen->pIn++;` |
|         - |  4383 | `	/* Create the loop block */` |
|     65021 |  4384 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     65021 |  4385 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4386 | `		return SXERR_ABORT;` |
|         - |  4387 | `	}` |
|         - |  4388 | `	/* Delimit the condition */` |
|     65021 |  4389 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     65021 |  4390 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4391 | `		/* Empty expression */` |
|         3 |  4392 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4393 | `		if( rc == SXERR_ABORT ){` |
|         - |  4394 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4395 | `			return SXERR_ABORT;` |
|         - |  4396 | `		}` |
|         1 |  4397 | `	}` |
|         - |  4398 | `	/* Swap token streams */` |
|     65021 |  4399 | `	pTmp = pGen->pEnd;` |
|     65021 |  4400 | `	pGen->pEnd = pEnd;` |
|         - |  4401 | `	/* Compile the expression */` |
|     65021 |  4402 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     65021 |  4403 | `	if( rc == SXERR_ABORT ){` |
|         - |  4404 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4405 | `		return SXERR_ABORT;` |
|         - |  4406 | `	}` |
|         - |  4407 | `	/* Update token stream */` |
|     65021 |  4408 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4409 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4410 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4411 | `			return SXERR_ABORT;` |
|         - |  4412 | `		}` |
|       ! 0 |  4413 | `		pGen->pIn++;` |
|       ! 0 |  4414 | `	}` |
|         - |  4415 | `	/* Synchronize pointers */` |
|     65021 |  4416 | `	pGen->pIn  = &pEnd[1];` |
|     65021 |  4417 | `	pGen->pEnd = pTmp;` |
|         - |  4418 | `	/* Emit the false jump */` |
|     65021 |  4419 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4420 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     65021 |  4421 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4422 | `	/* Compile the loop body */` |
|     65021 |  4423 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     65021 |  4424 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4425 | `		return SXERR_ABORT;` |
|         - |  4426 | `	}` |
|         - |  4427 | `	/* Emit the unconditional jump to the start of the loop */` |
|     65021 |  4428 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4429 | `	/* Fix all jumps now the destination is resolved */` |
|     65021 |  4430 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4431 | `	/* Release the loop block */` |
|     65021 |  4432 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4433 | `	/* Statement successfully compiled */` |
|     65021 |  4434 | `	return SXRET_OK;` |
|       ! 0 |  4435 | `Synchronize:` |
|         - |  4436 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4437 | `	 * compiling this erroneous block.` |
|         - |  4438 | `	 */` |
|       ! 0 |  4439 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4440 | `		pGen->pIn++;` |
|       ! 0 |  4441 | `	}` |
|       ! 0 |  4442 | `	return SXRET_OK;` |
|     32513 |  4443 | `}` |
|         - |  4444 | `/*` |
|         - |  4445 | ` * Compile the ugly do..while() statement.` |
|         - |  4446 | ` * According to the PHP language reference` |
|         - |  4447 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4448 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4449 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4450 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4451 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4452 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4453 | ` *  would end immediately).` |
|         - |  4454 | ` *  There is just one syntax for do-while loops:` |
|         - |  4455 | ` *  <?php` |
|         - |  4456 | ` *  $i = 0;` |
|         - |  4457 | ` *  do {` |
|         - |  4458 | ` *   echo $i;` |
|         - |  4459 | ` *  } while ($i > 0);` |
|         - |  4460 | ` * ?>` |
|         - |  4461 | ` */` |
|         2 |  4462 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4463 | `{` |
|         3 |  4464 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4465 | `	GenBlock *pDoBlock = 0;` |
|         - |  4466 | `	sxu32 nLine;` |
|         - |  4467 | `	sxi32 rc;` |
|         3 |  4468 | `	nLine = pGen->pIn->nLine;` |
|         - |  4469 | `	/* Jump the 'do' keyword */` |
|         3 |  4470 | `	pGen->pIn++;` |
|         - |  4471 | `	/* Create the loop block */` |
|         3 |  4472 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4473 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4474 | `		return SXERR_ABORT;` |
|         - |  4475 | `	}` |
|         - |  4476 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4477 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4478 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4479 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4480 | `		return SXERR_ABORT;` |
|         - |  4481 | `	}` |
|         3 |  4482 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4483 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4484 | `	}` |
|         3 |  4485 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4486 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4487 | `			/* Missing 'while' statement */` |
|         3 |  4488 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4489 | `			if( rc == SXERR_ABORT ){` |
|         - |  4490 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4491 | `				return SXERR_ABORT;` |
|         - |  4492 | `			}` |
|         3 |  4493 | `			goto Synchronize;` |
|         - |  4494 | `	}` |
|         - |  4495 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4496 | `	pGen->pIn++;` |
|       ! 0 |  4497 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4498 | `		/* Syntax error */` |
|       ! 0 |  4499 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4500 | `		if( rc == SXERR_ABORT ){` |
|         - |  4501 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4502 | `			return SXERR_ABORT;` |
|         - |  4503 | `		}` |
|       ! 0 |  4504 | `		goto Synchronize;` |
|         - |  4505 | `	}` |
|         - |  4506 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4507 | `	pGen->pIn++;` |
|         - |  4508 | `	/* Delimit the condition */` |
|       ! 0 |  4509 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4510 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4511 | `		/* Empty expression */` |
|       ! 0 |  4512 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4513 | `		if( rc == SXERR_ABORT ){` |
|         - |  4514 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4515 | `			return SXERR_ABORT;` |
|         - |  4516 | `		}` |
|       ! 0 |  4517 | `		goto Synchronize;` |
|         - |  4518 | `	}` |
|         - |  4519 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4520 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4521 | `		JumpFixup *aPost;` |
|         - |  4522 | `		VmInstr *pInstr;` |
|         - |  4523 | `		sxu32 nJumpDest;` |
|         - |  4524 | `		sxu32 n;` |
|       ! 0 |  4525 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4526 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4527 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4528 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4529 | `			if( pInstr ){` |
|         - |  4530 | `				/* Fix */` |
|       ! 0 |  4531 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4532 | `			}` |
|       ! 0 |  4533 | `		}` |
|       ! 0 |  4534 | `	}` |
|         - |  4535 | `	/* Swap token streams */` |
|       ! 0 |  4536 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4537 | `	pGen->pEnd = pEnd;` |
|         - |  4538 | `	/* Compile the expression */` |
|       ! 0 |  4539 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4540 | `	if( rc == SXERR_ABORT ){` |
|         - |  4541 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4542 | `		return SXERR_ABORT;` |
|         - |  4543 | `	}` |
|         - |  4544 | `	/* Update token stream */` |
|       ! 0 |  4545 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4546 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4547 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4548 | `			return SXERR_ABORT;` |
|         - |  4549 | `		}` |
|       ! 0 |  4550 | `		pGen->pIn++;` |
|       ! 0 |  4551 | `	}` |
|       ! 0 |  4552 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4553 | `	pGen->pEnd = pTmp;` |
|         - |  4554 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4555 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4556 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4557 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4558 | `	/* Release the loop block */` |
|       ! 0 |  4559 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4560 | `	/* Statement successfully compiled */` |
|       ! 0 |  4561 | `	return SXRET_OK;` |
|         1 |  4562 | `Synchronize:` |
|         - |  4563 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4564 | `	 * compiling this erroneous block.` |
|         - |  4565 | `	 */` |
|         3 |  4566 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4567 | `		pGen->pIn++;` |
|       ! 0 |  4568 | `	}` |
|         3 |  4569 | `	return SXRET_OK;` |
|         2 |  4570 | `}` |
|         - |  4571 | `/*` |
|         - |  4572 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4573 | ` * According to the PHP language reference` |
|         - |  4574 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4575 | ` *  The syntax of a for loop is:` |
|         - |  4576 | ` *  for (expr1; expr2; expr3)` |
|         - |  4577 | ` *   statement` |
|         - |  4578 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4579 | ` *  the beginning of the loop.` |
|         - |  4580 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4581 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4582 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4583 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4584 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4585 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4586 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4587 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4588 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4589 | ` *  of using the for truth expression.` |
|         - |  4590 | ` */` |
|    122258 |  4591 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4592 | `{` |
|    122263 |  4593 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    122263 |  4594 | `	GenBlock *pForBlock = 0;` |
|         - |  4595 | `	sxu32 nFalseJump;` |
|         - |  4596 | `	sxu32 nLine;` |
|         - |  4597 | `	sxi32 rc;` |
|    122263 |  4598 | `	nLine = pGen->pIn->nLine;` |
|         - |  4599 | `	/* Jump the 'for' keyword */` |
|    122263 |  4600 | `	pGen->pIn++;` |
|    122263 |  4601 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4602 | `		/* Syntax error */` |
|       ! 0 |  4603 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4604 | `		if( rc == SXERR_ABORT ){` |
|         - |  4605 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4606 | `			return SXERR_ABORT;` |
|         - |  4607 | `		}` |
|       ! 0 |  4608 | `		return SXRET_OK;` |
|         - |  4609 | `	}` |
|         - |  4610 | `	/* Jump the left parenthesis '(' */` |
|    122263 |  4611 | `	pGen->pIn++;` |
|         - |  4612 | `	/* Delimit the init-expr;condition;post-expr */` |
|    122263 |  4613 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    122263 |  4614 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4615 | `		/* Empty expression */` |
|       ! 0 |  4616 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4617 | `		if( rc == SXERR_ABORT ){` |
|         - |  4618 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4619 | `			return SXERR_ABORT;` |
|         - |  4620 | `		}` |
|         - |  4621 | `		/* Synchronize */` |
|       ! 0 |  4622 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4623 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4624 | `			pGen->pIn++;` |
|       ! 0 |  4625 | `		}` |
|       ! 0 |  4626 | `		return SXRET_OK;` |
|         - |  4627 | `	}` |
|         - |  4628 | `	/* Swap token streams */` |
|    122263 |  4629 | `	pTmp = pGen->pEnd;` |
|    122263 |  4630 | `	pGen->pEnd = pEnd;` |
|         - |  4631 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4632 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4633 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4634 | `	 * compiled through this same window — recorded as a known leniency. */` |
|    122263 |  4635 | `	pGen->nCommaExprOk++;` |
|         - |  4636 | `	/* Compile initialization expressions if available */` |
|    122263 |  4637 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4638 | `	/* Pop operand lvalues */` |
|    122263 |  4639 | `	if( rc == SXERR_ABORT ){` |
|         - |  4640 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4641 | `		return SXERR_ABORT;` |
|    122263 |  4642 | `	}else if( rc != SXERR_EMPTY ){` |
|    110813 |  4643 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55404 |  4644 | `	}` |
|    122263 |  4645 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4646 | `		/* Syntax error */` |
|       ! 0 |  4647 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4648 | `		if( rc == SXERR_ABORT ){` |
|         - |  4649 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4650 | `			return SXERR_ABORT;` |
|         - |  4651 | `		}` |
|       ! 0 |  4652 | `		return SXRET_OK;` |
|         - |  4653 | `	}` |
|         - |  4654 | `	/* Jump the trailing ';' */` |
|    122263 |  4655 | `	pGen->pIn++;` |
|         - |  4656 | `	/* Create the loop block */` |
|    122263 |  4657 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    122263 |  4658 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4659 | `		return SXERR_ABORT;` |
|         - |  4660 | `	}` |
|         - |  4661 | `	/* Deffer continue jumps */` |
|    122263 |  4662 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4663 | `	/* Compile the condition */` |
|    122263 |  4664 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    122263 |  4665 | `	if( rc == SXERR_ABORT ){` |
|         - |  4666 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4667 | `		return SXERR_ABORT;` |
|    122263 |  4668 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4669 | `		/* Emit the false jump */` |
|    110813 |  4670 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4671 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    110813 |  4672 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     55404 |  4673 | `	}` |
|    122263 |  4674 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4675 | `		/* Syntax error */` |
|         6 |  4676 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4677 | `		if( rc == SXERR_ABORT ){` |
|         - |  4678 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4679 | `			return SXERR_ABORT;` |
|         - |  4680 | `		}` |
|         6 |  4681 | `		return SXRET_OK;` |
|         - |  4682 | `	}` |
|         - |  4683 | `	/* Jump the trailing ';' */` |
|    122259 |  4684 | `	pGen->pIn++;` |
|         - |  4685 | `	/* Save the post condition stream */` |
|    122259 |  4686 | `	pPostStart = pGen->pIn;` |
|         - |  4687 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4688 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|    122259 |  4689 | `	pGen->nCommaExprOk--;` |
|    122259 |  4690 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    122259 |  4691 | `	pGen->pEnd = pTmp;` |
|    122259 |  4692 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    122259 |  4693 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4694 | `		return SXERR_ABORT;` |
|         - |  4695 | `	}` |
|         - |  4696 | `	/* Fix post-continue jumps */` |
|    122259 |  4697 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4698 | `		JumpFixup *aPost;` |
|         - |  4699 | `		VmInstr *pInstr;` |
|         - |  4700 | `		sxu32 nJumpDest;` |
|         - |  4701 | `		sxu32 n;` |
|     11465 |  4702 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|     11465 |  4703 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     38189 |  4704 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     26729 |  4705 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     26729 |  4706 | `			if( pInstr ){` |
|         - |  4707 | `				/* Fix jump */` |
|     26729 |  4708 | `				pInstr->iP2 = nJumpDest;` |
|     13362 |  4709 | `			}` |
|     13367 |  4710 | `		}` |
|      5730 |  4711 | `	}` |
|         - |  4712 | `	/* compile the post-expressions if available */` |
|    122259 |  4713 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4714 | `		pPostStart++;` |
|       ! 0 |  4715 | `	}` |
|    122259 |  4716 | `	if( pPostStart < pEnd ){` |
|         - |  4717 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    110811 |  4718 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    110811 |  4719 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|    110811 |  4720 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    110811 |  4721 | `		pGen->nCommaExprOk--;` |
|    110811 |  4722 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4723 | `			/* Syntax error */` |
|       ! 0 |  4724 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4725 | `			if( rc == SXERR_ABORT ){` |
|         - |  4726 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4727 | `				return SXERR_ABORT;` |
|         - |  4728 | `			}` |
|       ! 0 |  4729 | `			return SXRET_OK;` |
|         - |  4730 | `		}` |
|    110811 |  4731 | `		RE_SWAP_DELIMITER(pGen);` |
|    110811 |  4732 | `		if( rc == SXERR_ABORT ){` |
|         - |  4733 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4734 | `			return SXERR_ABORT;` |
|    110811 |  4735 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4736 | `			/* Pop operand lvalue */` |
|    110811 |  4737 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55403 |  4738 | `		}` |
|     55403 |  4739 | `	}` |
|         - |  4740 | `	/* Emit the unconditional jump to the start of the loop */` |
|    122259 |  4741 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4742 | `	/* Fix all jumps now the destination is resolved */` |
|    122259 |  4743 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4744 | `	/* Release the loop block */` |
|    122259 |  4745 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4746 | `	/* Statement successfully compiled */` |
|    122259 |  4747 | `	return SXRET_OK;` |
|     61134 |  4748 | `}` |
|         - |  4749 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4750 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4751 | ` * are allowed.` |
|         - |  4752 | ` */` |
|    436056 |  4753 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4754 | `{` |
|    436061 |  4755 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    436061 |  4756 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4757 | `		/* Unexpected expression */` |
|       ! 0 |  4758 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4759 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4760 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4761 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4762 | `		}` |
|       ! 0 |  4763 | `	}` |
|    436061 |  4764 | `	return rc;` |
|         5 |  4765 | `}` |
|         - |  4766 | `/*` |
|         - |  4767 | ` * Compile the 'foreach' statement.` |
|         - |  4768 | ` * According to the PHP language reference` |
|         - |  4769 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4770 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4771 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4772 | ` *  is a minor but useful extension of the first:` |
|         - |  4773 | ` *  foreach (array_expression as $value)` |
|         - |  4774 | ` *    statement` |
|         - |  4775 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4776 | ` *   statement` |
|         - |  4777 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4778 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4779 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4780 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4781 | ` *  to the variable $key on each loop.` |
|         - |  4782 | ` *  Note:` |
|         - |  4783 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4784 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4785 | ` *  Note:` |
|         - |  4786 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4787 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4788 | ` *  or after the foreach without resetting it.` |
|         - |  4789 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4790 | ` *  of copying the value.` |
|         - |  4791 | ` */` |
|    302222 |  4792 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4793 | `{` |
|    302227 |  4794 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    302227 |  4795 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    302227 |  4796 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4797 | `	ph7_foreach_info *pInfo;` |
|         - |  4798 | `	sxu32 nFalseJump;` |
|         - |  4799 | `	VmInstr *pInstr;` |
|         - |  4800 | `	sxu32 nLine;` |
|         - |  4801 | `	sxi32 rc;` |
|    302227 |  4802 | `	nLine = pGen->pIn->nLine;` |
|         - |  4803 | `	/* Jump the 'foreach' keyword */` |
|    302227 |  4804 | `	pGen->pIn++;` |
|    302227 |  4805 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4806 | `		/* Syntax error */` |
|       ! 0 |  4807 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4808 | `		if( rc == SXERR_ABORT ){` |
|         - |  4809 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4810 | `			return SXERR_ABORT;` |
|         - |  4811 | `		}` |
|       ! 0 |  4812 | `		goto Synchronize;` |
|         - |  4813 | `	}` |
|         - |  4814 | `	/* Jump the left parenthesis '(' */` |
|    302227 |  4815 | `	pGen->pIn++;` |
|         - |  4816 | `	/* Create the loop block */` |
|    302227 |  4817 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    302227 |  4818 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4819 | `		return SXERR_ABORT;` |
|         - |  4820 | `	}` |
|         - |  4821 | `	/* Delimit the expression */` |
|    302227 |  4822 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    302227 |  4823 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4824 | `		/* Empty expression */` |
|       ! 0 |  4825 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4826 | `		if( rc == SXERR_ABORT ){` |
|         - |  4827 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4828 | `			return SXERR_ABORT;` |
|         - |  4829 | `		}` |
|         - |  4830 | `		/* Synchronize */` |
|       ! 0 |  4831 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4832 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4833 | `			pGen->pIn++;` |
|       ! 0 |  4834 | `		}` |
|       ! 0 |  4835 | `		return SXRET_OK;` |
|         - |  4836 | `	}` |
|         - |  4837 | `	/* Compile the array expression */` |
|    302227 |  4838 | `	pCur = pGen->pIn;` |
|   1733055 |  4839 | `	while( pCur < pEnd ){` |
|   1733055 |  4840 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    332769 |  4841 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    332769 |  4842 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4843 | `				/* Break with the first 'as' found */` |
|    302227 |  4844 | `				break;` |
|         - |  4845 | `			}` |
|     15271 |  4846 | `		}` |
|         - |  4847 | `		/* Advance the stream cursor */` |
|   1430833 |  4848 | `		pCur++;` |
|         5 |  4849 | `	}` |
|    302227 |  4850 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4851 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4852 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4853 | `		if( rc == SXERR_ABORT ){` |
|         - |  4854 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4855 | `			return SXERR_ABORT;` |
|         - |  4856 | `		}` |
|       ! 0 |  4857 | `		goto Synchronize;` |
|         - |  4858 | `	}` |
|         - |  4859 | `	/* Swap token streams */` |
|    302227 |  4860 | `	pTmp = pGen->pEnd;` |
|    302227 |  4861 | `	pGen->pEnd = pCur;` |
|    302227 |  4862 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    302227 |  4863 | `	if( rc == SXERR_ABORT ){` |
|         - |  4864 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4865 | `		return SXERR_ABORT;` |
|         - |  4866 | `	}` |
|         - |  4867 | `	/* Update token stream */` |
|    302227 |  4868 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4869 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4870 | `		if( rc == SXERR_ABORT ){` |
|         - |  4871 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4872 | `			return SXERR_ABORT;` |
|         - |  4873 | `		}` |
|       ! 0 |  4874 | `		pGen->pIn++;` |
|       ! 0 |  4875 | `	}` |
|    302227 |  4876 | `	pCur++; /* Jump the 'as' keyword */` |
|    302227 |  4877 | `	pGen->pIn = pCur;` |
|    302227 |  4878 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4879 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4880 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4881 | `			return SXERR_ABORT;` |
|         - |  4882 | `		}` |
|       ! 0 |  4883 | `	}` |
|         - |  4884 | `	/* Create the foreach context */` |
|    302227 |  4885 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    302227 |  4886 | `	if( pInfo == 0 ){` |
|       ! 0 |  4887 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4888 | `		return SXERR_ABORT;` |
|         - |  4889 | `	}` |
|         - |  4890 | `	/* Zero the structure */` |
|    302227 |  4891 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4892 | `	/* Initialize structure fields */` |
|    302227 |  4893 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4894 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4895 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4896 | `	 * '=>'. */` |
|    302227 |  4897 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    302227 |  4898 | `	if( pCur < pEnd ){` |
|         - |  4899 | `		/* Compile the expression holding the key name */` |
|    133861 |  4900 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4901 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4902 | `			if( rc == SXERR_ABORT ){` |
|         - |  4903 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4904 | `				return SXERR_ABORT;` |
|         - |  4905 | `			}` |
|       ! 0 |  4906 | `		}else{` |
|    133861 |  4907 | `			pGen->pEnd = pCur;` |
|    133861 |  4908 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    133861 |  4909 | `			if( rc == SXERR_ABORT ){` |
|         - |  4910 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4911 | `				return SXERR_ABORT;` |
|         - |  4912 | `			}` |
|    133861 |  4913 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    133861 |  4914 | `			if( pInstr->p3 ){` |
|         - |  4915 | `				/* Record key name */` |
|    133861 |  4916 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     66928 |  4917 | `			}` |
|    133861 |  4918 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4919 | `		}` |
|    133861 |  4920 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     66928 |  4921 | `	}` |
|    302227 |  4922 | `	pGen->pEnd = pEnd;` |
|    302227 |  4923 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4924 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4925 | `		if( rc == SXERR_ABORT ){` |
|         - |  4926 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4927 | `			return SXERR_ABORT;` |
|         - |  4928 | `		}` |
|       ! 0 |  4929 | `		goto Synchronize;` |
|         - |  4930 | `	}` |
|    302227 |  4931 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4932 | `		pGen->pIn++;` |
|         - |  4933 | `		/* Pass by reference  */` |
|        33 |  4934 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4935 | `	}` |
|         - |  4936 | `	/* Check if the value target is list() */` |
|    302227 |  4937 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4938 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4939 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4940 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4941 | `		 */` |
|         - |  4942 | `		static int iForeachListCnt = 0;` |
|         - |  4943 | `		char zTmp[128];` |
|         - |  4944 | `		sxu32 nLen;` |
|         - |  4945 | `		char *zDup;` |
|        10 |  4946 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4947 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4948 | `		if( zDup == 0 ){` |
|       ! 0 |  4949 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4950 | `			return SXERR_ABORT;` |
|         - |  4951 | `		}` |
|        10 |  4952 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4953 | `		/* Save list() token boundaries */` |
|        10 |  4954 | `		pListStart = pGen->pIn;` |
|         - |  4955 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4956 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4957 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4958 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  4959 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  4960 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4961 | `				return SXERR_ABORT;` |
|         - |  4962 | `			}` |
|         3 |  4963 | `			goto Synchronize;` |
|         - |  4964 | `		}` |
|         7 |  4965 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  4966 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  4967 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4968 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4969 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  4970 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4971 | `				return SXERR_ABORT;` |
|         - |  4972 | `			}` |
|       ! 0 |  4973 | `			goto Synchronize;` |
|         - |  4974 | `		}` |
|         7 |  4975 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  4976 | `		pListEnd = pGen->pIn;` |
|         7 |  4977 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    302222 |  4978 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  4979 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  4980 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  4981 | `		 */` |
|         - |  4982 | `		static int iForeachShortListCnt = 0;` |
|         - |  4983 | `		char zTmp[128];` |
|         - |  4984 | `		sxu32 nLen;` |
|         - |  4985 | `		char *zDup;` |
|        15 |  4986 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        15 |  4987 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        15 |  4988 | `		if( zDup == 0 ){` |
|       ! 0 |  4989 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4990 | `			return SXERR_ABORT;` |
|         - |  4991 | `		}` |
|        15 |  4992 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4993 | `		/* Save [...] token boundaries */` |
|        15 |  4994 | `		pListStart = pGen->pIn;` |
|         - |  4995 | `		/* Advance past [...] */` |
|        15 |  4996 | `		pGen->pIn++; /* Jump '[' */` |
|        15 |  4997 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        15 |  4998 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4999 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5000 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  5001 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5002 | `				return SXERR_ABORT;` |
|         - |  5003 | `			}` |
|       ! 0 |  5004 | `			goto Synchronize;` |
|         - |  5005 | `		}` |
|        15 |  5006 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        15 |  5007 | `		pListEnd = pGen->pIn;` |
|        15 |  5008 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         8 |  5009 | `	}else{` |
|         - |  5010 | `		/* Compile the expression holding the value name */` |
|    302205 |  5011 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    302205 |  5012 | `		if( rc == SXERR_ABORT ){` |
|         - |  5013 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5014 | `			return SXERR_ABORT;` |
|         - |  5015 | `		}` |
|    302205 |  5016 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    302205 |  5017 | `		if( pInstr->p3 ){` |
|         - |  5018 | `			/* Record value name */` |
|    302205 |  5019 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    151100 |  5020 | `		}` |
|         - |  5021 | `	}` |
|         - |  5022 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    302225 |  5023 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  5024 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    302225 |  5025 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  5026 | `	/* Record the first instruction to execute */` |
|    302225 |  5027 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5028 | `	/* Emit the FOREACH_STEP instruction */` |
|    302225 |  5029 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  5030 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    302225 |  5031 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  5032 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    302225 |  5033 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  5034 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  5035 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  5036 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  5037 | `		 */` |
|        21 |  5038 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  5039 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  5040 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  5041 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  5042 | `		 */` |
|        21 |  5043 | `		pSavedIn = pGen->pIn;` |
|        21 |  5044 | `		pSavedEnd = pGen->pEnd;` |
|        21 |  5045 | `		pGen->pIn = pListStart;` |
|        21 |  5046 | `		pGen->pEnd = pListEnd;` |
|        21 |  5047 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        15 |  5048 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         8 |  5049 | `		}else{` |
|         7 |  5050 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  5051 | `		}` |
|        21 |  5052 | `		pGen->pIn = pSavedIn;` |
|        21 |  5053 | `		pGen->pEnd = pSavedEnd;` |
|        21 |  5054 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5055 | `			return SXERR_ABORT;` |
|         - |  5056 | `		}` |
|         - |  5057 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        21 |  5058 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        10 |  5059 | `	}` |
|         - |  5060 | `	/* Compile the loop body */` |
|    302225 |  5061 | `	pGen->pIn = &pEnd[1];` |
|    302225 |  5062 | `	pGen->pEnd = pTmp;` |
|    302225 |  5063 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    302225 |  5064 | `	if( rc == SXERR_ABORT ){` |
|         - |  5065 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5066 | `		return SXERR_ABORT;` |
|         - |  5067 | `	}` |
|         - |  5068 | `	/* Emit the unconditional jump to the start of the loop */` |
|    302225 |  5069 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  5070 | `	/* Fix all jumps now the destination is resolved */` |
|    302225 |  5071 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  5072 | `	/* Release the loop block */` |
|    302225 |  5073 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5074 | `	/* Statement successfully compiled */` |
|    302225 |  5075 | `	return SXRET_OK;` |
|         1 |  5076 | `Synchronize:` |
|         - |  5077 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  5078 | `	 * compiling this erroneous block.` |
|         - |  5079 | `	 */` |
|         3 |  5080 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  5081 | `		pGen->pIn++;` |
|       ! 0 |  5082 | `	}` |
|         3 |  5083 | `	return SXRET_OK;` |
|    151116 |  5084 | `}` |
|         - |  5085 | `/*` |
|         - |  5086 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  5087 | ` * According to the PHP language reference` |
|         - |  5088 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  5089 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  5090 | ` *  that is similar to that of C:` |
|         - |  5091 | ` *  if (expr)` |
|         - |  5092 | ` *   statement` |
|         - |  5093 | ` *  else construct:` |
|         - |  5094 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  5095 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  5096 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  5097 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  5098 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  5099 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  5100 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  5101 | ` *  elseif` |
|         - |  5102 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  5103 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  5104 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  5105 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  5106 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  5107 | ` *   <?php` |
|         - |  5108 | ` *    if ($a > $b) {` |
|         - |  5109 | ` *     echo "a is bigger than b";` |
|         - |  5110 | ` *    } elseif ($a == $b) {` |
|         - |  5111 | ` *     echo "a is equal to b";` |
|         - |  5112 | ` *    } else {` |
|         - |  5113 | ` *     echo "a is smaller than b";` |
|         - |  5114 | ` *    }` |
|         - |  5115 | ` *    ?>` |
|         - |  5116 | ` */` |
|   2196184 |  5117 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  5118 | `{` |
|   2196189 |  5119 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   2196189 |  5120 | `	GenBlock *pCondBlock = 0;` |
|         - |  5121 | `	sxu32 nJumpIdx;` |
|         - |  5122 | `	sxu32 nKeyID;` |
|         - |  5123 | `	sxi32 rc;` |
|         - |  5124 | `	/* Jump the 'if' keyword */` |
|   2196189 |  5125 | `	pGen->pIn++;` |
|   2196189 |  5126 | `	pToken = pGen->pIn;` |
|         - |  5127 | `	/* Create the conditional block */` |
|   2196189 |  5128 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   2196189 |  5129 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5130 | `		return SXERR_ABORT;` |
|         - |  5131 | `	}` |
|         - |  5132 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1237435 |  5133 | `	for(;;){` |
|   2474875 |  5134 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  5135 | `			/* Syntax error */` |
|       ! 0 |  5136 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5137 | `				pToken--;` |
|       ! 0 |  5138 | `			}` |
|       ! 0 |  5139 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  5140 | `			if( rc == SXERR_ABORT ){` |
|         - |  5141 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5142 | `				return SXERR_ABORT;` |
|         - |  5143 | `			}` |
|       ! 0 |  5144 | `			goto Synchronize;` |
|         - |  5145 | `		}` |
|         - |  5146 | `		/* Jump the left parenthesis '(' */` |
|   2474875 |  5147 | `		pToken++;` |
|         - |  5148 | `		/* Delimit the condition */` |
|   2474875 |  5149 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2474875 |  5150 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  5151 | `			/* Syntax error */` |
|        11 |  5152 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5153 | `				pToken--;` |
|       ! 0 |  5154 | `			}` |
|        11 |  5155 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  5156 | `			if( rc == SXERR_ABORT ){` |
|         - |  5157 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5158 | `				return SXERR_ABORT;` |
|         - |  5159 | `			}` |
|        11 |  5160 | `			goto Synchronize;` |
|         - |  5161 | `		}` |
|         - |  5162 | `		/* Swap token streams */` |
|   2474867 |  5163 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  5164 | `		/* Compile the condition */` |
|   2474867 |  5165 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5166 | `		/* Update token stream */` |
|   2474867 |  5167 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5168 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5169 | `			pGen->pIn++;` |
|       ! 0 |  5170 | `		}` |
|   2474867 |  5171 | `		pGen->pIn  = &pEnd[1];` |
|   2474867 |  5172 | `		pGen->pEnd = pTmp;` |
|   2474867 |  5173 | `		if( rc == SXERR_ABORT ){` |
|         - |  5174 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5175 | `			return SXERR_ABORT;` |
|         - |  5176 | `		}` |
|         - |  5177 | `		/* Emit the false jump */` |
|   2474867 |  5178 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5179 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2474867 |  5180 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5181 | `		/* Compile the body */` |
|   2474867 |  5182 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2474867 |  5183 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5184 | `			return SXERR_ABORT;` |
|         - |  5185 | `		}` |
|   2474867 |  5186 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    483343 |  5187 | `			break;` |
|         - |  5188 | `		}` |
|         - |  5189 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1508191 |  5190 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1508191 |  5191 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   1019175 |  5192 | `			break;` |
|         - |  5193 | `		}` |
|         - |  5194 | `		/* Emit the unconditional jump */` |
|    489021 |  5195 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5196 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    489021 |  5197 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    489021 |  5198 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    294287 |  5199 | `			pToken = &pGen->pIn[1];` |
|    294287 |  5200 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     83990 |  5201 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    105170 |  5202 | `					break;` |
|         - |  5203 | `			}` |
|     83957 |  5204 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     41976 |  5205 | `		}` |
|    278691 |  5206 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5207 | `		/* Synchronize cursors */` |
|    278691 |  5208 | `		pToken = pGen->pIn;` |
|         - |  5209 | `		/* Fix the false jump */` |
|    278691 |  5210 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5211 | `	} /* For(;;) */` |
|         - |  5212 | `	/* Fix the false jump */` |
|   2196181 |  5213 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   2196181 |  5214 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1229500 |  5215 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5216 | `			/* Compile the else block */` |
|    210335 |  5217 | `			pGen->pIn++;` |
|    210335 |  5218 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    210335 |  5219 | `			if( rc == SXERR_ABORT ){` |
|         - |  5220 |  |
|       ! 0 |  5221 | `				return SXERR_ABORT;` |
|         - |  5222 | `			}` |
|    105165 |  5223 | `	}` |
|   2196181 |  5224 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5225 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   2196181 |  5226 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5227 | `	/* Release the conditional block */` |
|   2196181 |  5228 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5229 | `	/* Statement successfully compiled */` |
|   2196181 |  5230 | `	return SXRET_OK;` |
|         4 |  5231 | `Synchronize:` |
|         - |  5232 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5233 | `	 */` |
|        67 |  5234 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5235 | `		pGen->pIn++;` |
|         3 |  5236 | `	}` |
|        11 |  5237 | `	return SXRET_OK;` |
|   1098097 |  5238 | `}` |
|         - |  5239 | `/*` |
|         - |  5240 | ` * Compile the global construct.` |
|         - |  5241 | ` * According to the PHP language reference` |
|         - |  5242 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5243 | ` *  to be used in that function.` |
|         - |  5244 | ` *  Example #1 Using global` |
|         - |  5245 | ` *  <?php` |
|         - |  5246 | ` *   $a = 1;` |
|         - |  5247 | ` *   $b = 2;` |
|         - |  5248 | ` *   function Sum()` |
|         - |  5249 | ` *   {` |
|         - |  5250 | ` *    global $a, $b;` |
|         - |  5251 | ` *    $b = $a + $b;` |
|         - |  5252 | ` *   }` |
|         - |  5253 | ` *   Sum();` |
|         - |  5254 | ` *   echo $b;` |
|         - |  5255 | ` *  ?>` |
|         - |  5256 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5257 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5258 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5259 | ` */` |
|        38 |  5260 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5261 | `{` |
|        43 |  5262 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5263 | `	sxi32 nExpr;` |
|         - |  5264 | `	sxi32 rc;` |
|         - |  5265 | `	/* Jump the 'global' keyword */` |
|        43 |  5266 | `	pGen->pIn++;` |
|        43 |  5267 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5268 | `		/* Nothing to process */` |
|       ! 0 |  5269 | `		return SXRET_OK;` |
|         - |  5270 | `	}` |
|        43 |  5271 | `	pTmp = pGen->pEnd;` |
|        43 |  5272 | `	nExpr = 0;` |
|        91 |  5273 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5274 | `		if( pGen->pIn < pNext ){` |
|        53 |  5275 | `			pGen->pEnd = pNext;` |
|        53 |  5276 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5277 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5278 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5279 | `					return SXERR_ABORT;` |
|         - |  5280 | `				}` |
|       ! 0 |  5281 | `			}else{` |
|        53 |  5282 | `				pGen->pIn++;` |
|        53 |  5283 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5284 | `					/* Emit a warning */` |
|       ! 0 |  5285 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5286 | `				}else{` |
|        53 |  5287 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5288 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5289 | `						return SXERR_ABORT;` |
|        53 |  5290 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5291 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5292 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5293 | `							/* Variable name, not a constant */` |
|        53 |  5294 | `							pLast->iP1 = 0;` |
|        24 |  5295 | `						}` |
|        53 |  5296 | `						nExpr++;` |
|        24 |  5297 | `					}` |
|         - |  5298 | `				}` |
|         - |  5299 | `			}` |
|        24 |  5300 | `		}` |
|         - |  5301 | `		/* Next expression in the stream */` |
|        53 |  5302 | `		pGen->pIn = pNext;` |
|         - |  5303 | `		/* Jump trailing commas */` |
|        63 |  5304 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5305 | `			pGen->pIn++;` |
|         5 |  5306 | `		}` |
|         5 |  5307 | `	}` |
|         - |  5308 | `	/* Restore token stream */` |
|        43 |  5309 | `	pGen->pEnd = pTmp;` |
|        43 |  5310 | `	if( nExpr > 0 ){` |
|         - |  5311 | `		/* Emit the uplink instruction */` |
|        43 |  5312 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5313 | `	}` |
|        43 |  5314 | `	return SXRET_OK;` |
|        24 |  5315 | `}` |
|         - |  5316 | `/*` |
|         - |  5317 | ` * Compile the return statement.` |
|         - |  5318 | ` * According to the PHP language reference` |
|         - |  5319 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5320 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5321 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5322 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5323 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5324 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5325 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5326 | ` *  from within the main script file, then script execution end.` |
|         - |  5327 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5328 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5329 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5330 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5331 | ` */` |
|   2978720 |  5332 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5333 | `{` |
|   2978725 |  5334 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5335 | `	sxi32 rc;` |
|   2978725 |  5336 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2978725 |  5337 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5338 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5339 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5340 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5341 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5342 | `	 * normally below so token processing stays consistent. */` |
|   7843355 |  5343 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4864635 |  5344 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5345 | `	}` |
|   2978720 |  5346 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2978691 |  5347 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5348 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5349 | `			"A never-returning function must not return");` |
|         3 |  5350 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5351 | `			return SXERR_ABORT;` |
|         - |  5352 | `		}` |
|         1 |  5353 | `	}` |
|         - |  5354 | `	/* Jump the 'return' keyword */` |
|   2978725 |  5355 | `	pGen->pIn++;` |
|   2978725 |  5356 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5357 | `		/* Compile the expression */` |
|   2883295 |  5358 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2883295 |  5359 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5360 | `			return SXERR_ABORT;` |
|   2883295 |  5361 | `		}else if(rc != SXERR_EMPTY ){` |
|   2883295 |  5362 | `			nRet = 1;` |
|   1441645 |  5363 | `		}` |
|   1441645 |  5364 | `	}` |
|         - |  5365 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5366 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5367 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5368 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2978725 |  5369 | `	if( pGen->bInGenerator ){` |
|      3849 |  5370 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3849 |  5371 | `		return SXRET_OK;` |
|         - |  5372 | `	}` |
|         - |  5373 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5374 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5375 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5376 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5377 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2974881 |  5378 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2974881 |  5379 | `	return SXRET_OK;` |
|   1489365 |  5380 | `}` |
|         - |  5381 | `/*` |
|         - |  5382 | ` * Compile a yield expression.` |
|         - |  5383 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5384 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5385 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5386 | ` */` |
|     15650 |  5387 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5388 | `{` |
|         - |  5389 | `	SyToken *pTmp, *pSplit;` |
|     15655 |  5390 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15655 |  5391 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5392 | `	sxi32 rc;` |
|      7825 |  5393 | `	(void)iCompileFlag;` |
|         - |  5394 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15655 |  5395 | `	pGen->pIn++;` |
|         - |  5396 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5397 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5398 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5399 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5400 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15650 |  5401 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7860 |  5402 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5403 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5404 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5405 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5406 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5407 | `			return SXERR_ABORT;` |
|         - |  5408 | `		}` |
|        67 |  5409 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5410 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5411 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5412 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5413 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5414 | `				return SXERR_ABORT;` |
|         - |  5415 | `			}` |
|       ! 0 |  5416 | `		}` |
|        67 |  5417 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5418 | `		return SXRET_OK;` |
|         - |  5419 | `	}` |
|     15593 |  5420 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5421 | `		/* Bare yield — no value */` |
|         3 |  5422 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5423 | `		return SXRET_OK;` |
|         - |  5424 | `	}` |
|         - |  5425 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15591 |  5426 | `	pSplit = 0;` |
|         - |  5427 | `	{` |
|     15591 |  5428 | `		SyToken *pCur = pGen->pIn;` |
|     15591 |  5429 | `		sxi32 nNest = 0;` |
|     46577 |  5430 | `		while( pCur < pGen->pEnd ){` |
|     46269 |  5431 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5432 | `				nNest++;` |
|     46261 |  5433 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5434 | `				nNest--;` |
|     46245 |  5435 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15283 |  5436 | `				pSplit = pCur;` |
|     15283 |  5437 | `				break;` |
|         - |  5438 | `			}` |
|     30991 |  5439 | `			pCur++;` |
|         5 |  5440 | `		}` |
|         - |  5441 | `	}` |
|     15591 |  5442 | `	pTmp = pGen->pEnd;` |
|     15591 |  5443 | `	if( pSplit ){` |
|         - |  5444 | `		/* yield $key => $value */` |
|     15283 |  5445 | `		pGen->pEnd = pSplit;` |
|     15283 |  5446 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15283 |  5447 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15283 |  5448 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15283 |  5449 | `		pGen->pEnd = pTmp;` |
|     15283 |  5450 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15283 |  5451 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15283 |  5452 | `		iP1 = 1;` |
|     15283 |  5453 | `		iP2 = 1;` |
|      7644 |  5454 | `	}else{` |
|         - |  5455 | `		/* yield $value */` |
|       313 |  5456 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       313 |  5457 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       313 |  5458 | `		if( rc != SXERR_EMPTY ){` |
|       313 |  5459 | `			iP1 = 1;` |
|       154 |  5460 | `		}` |
|         - |  5461 | `	}` |
|     15591 |  5462 | `	pGen->pEnd = pTmp;` |
|     15591 |  5463 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15591 |  5464 | `	return SXRET_OK;` |
|      7830 |  5465 | `}` |
|         - |  5466 | `/*` |
|         - |  5467 | ` * Compile the die/exit language construct.` |
|         - |  5468 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5469 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5470 | ` */` |
|       128 |  5471 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5472 | `{` |
|       133 |  5473 | `	sxi32 nExpr = 0;` |
|         - |  5474 | `	sxi32 rc;` |
|         - |  5475 | `	/* Jump the die/exit keyword */` |
|       133 |  5476 | `	pGen->pIn++;` |
|       133 |  5477 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5478 | `		/* Compile the expression */` |
|       133 |  5479 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5480 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5481 | `			return SXERR_ABORT;` |
|       133 |  5482 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5483 | `			nExpr = 1;` |
|        64 |  5484 | `		}` |
|        64 |  5485 | `	}` |
|         - |  5486 | `	/* Emit the HALT instruction */` |
|       133 |  5487 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5488 | `	return SXRET_OK;` |
|        69 |  5489 | `}` |
|         - |  5490 | `/*` |
|         - |  5491 | ` * Compile the 'echo' language construct.` |
|         - |  5492 | ` */` |
|     17766 |  5493 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5494 | `{` |
|     17771 |  5495 | `	SyToken *pTmp,*pNext = 0;` |
|     17771 |  5496 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17771 |  5497 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17771 |  5498 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5499 | `	sxi32 rc;` |
|         - |  5500 | `	/* Jump the 'echo' keyword */` |
|     17771 |  5501 | `	pGen->pIn++;` |
|         - |  5502 | `	/* Compile arguments one after one */` |
|     17771 |  5503 | `	pTmp = pGen->pEnd;` |
|     44885 |  5504 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     27121 |  5505 | `		if( pGen->pIn < pNext ){` |
|     27121 |  5506 | `			pGen->pEnd = pNext;` |
|     27121 |  5507 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     27121 |  5508 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5509 | `				return SXERR_ABORT;` |
|     27121 |  5510 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5511 | `				/* Emit the consume instruction */` |
|     27095 |  5512 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     27095 |  5513 | `				nExpr++;` |
|     27095 |  5514 | `				bExpectMore = 0;` |
|     13545 |  5515 | `			}` |
|     13558 |  5516 | `		}` |
|         - |  5517 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5518 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     36477 |  5519 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      9363 |  5520 | `			if( bExpectMore ){` |
|         - |  5521 | `				/* two commas in a row */` |
|         3 |  5522 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5523 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5524 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5525 | `			}` |
|      9361 |  5526 | `			bExpectMore = 1;` |
|      9361 |  5527 | `			pNext++;` |
|         5 |  5528 | `		}` |
|     27119 |  5529 | `		pGen->pIn = pNext;` |
|         5 |  5530 | `	}` |
|         - |  5531 | `	/* Restore token stream */` |
|     17769 |  5532 | `	pGen->pEnd = pTmp;` |
|     17769 |  5533 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5534 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5535 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5536 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5537 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5538 | `	}` |
|     17739 |  5539 | `	return SXRET_OK;` |
|      8888 |  5540 | `}` |
|         - |  5541 | `/*` |
|         - |  5542 | ` * Compile the static statement.` |
|         - |  5543 | ` * According to the PHP language reference` |
|         - |  5544 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5545 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5546 | ` *  when program execution leaves this scope.` |
|         - |  5547 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5548 | ` * Symisc eXtension.` |
|         - |  5549 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5550 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5551 | ` *  Example` |
|         - |  5552 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5553 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5554 | ` */` |
|      7644 |  5555 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         5 |  5556 | `{` |
|         - |  5557 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5558 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5559 | `	GenBlock *pBlock;` |
|         - |  5560 | `	SyString *pName;` |
|         - |  5561 | `	char *zDup;` |
|         - |  5562 | `	sxu32 nLine;` |
|         - |  5563 | `	sxi32 rc;` |
|         - |  5564 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5565 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5566 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|      7644 |  5567 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      3828 |  5568 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5569 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5570 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5571 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5572 | `			return SXERR_ABORT;` |
|         3 |  5573 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5574 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5575 | `		}` |
|         3 |  5576 | `		return SXRET_OK;` |
|         - |  5577 | `	}` |
|         - |  5578 | `	/* Jump the static keyword */` |
|      7647 |  5579 | `	nLine = pGen->pIn->nLine;` |
|      7647 |  5580 | `	pGen->pIn++;` |
|         - |  5581 | `	/* Extract the enclosing function if any */` |
|      7647 |  5582 | `	pBlock = pGen->pCurrent;` |
|     15289 |  5583 | `	while( pBlock ){` |
|     15289 |  5584 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      7647 |  5585 | `			break;` |
|         - |  5586 | `		}` |
|         - |  5587 | `		/* Point to the upper block */` |
|      7647 |  5588 | `		pBlock = pBlock->pParent;` |
|         5 |  5589 | `	}` |
|      7647 |  5590 | `	if( pBlock == 0 ){` |
|         - |  5591 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5592 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5593 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5594 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5595 | `				return SXERR_ABORT;` |
|         - |  5596 | `			}` |
|       ! 0 |  5597 | `			goto Synchronize;` |
|         - |  5598 | `		}` |
|         - |  5599 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5600 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5601 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5602 | `			return SXERR_ABORT;` |
|       ! 0 |  5603 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5604 | `			/* Emit the POP instruction */` |
|       ! 0 |  5605 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5606 | `		}` |
|       ! 0 |  5607 | `		return SXRET_OK;` |
|         - |  5608 | `	}` |
|      7647 |  5609 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5610 | `	/* Make sure we are dealing with a valid statement */` |
|      7647 |  5611 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      7640 |  5612 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5613 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5614 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5615 | `				return SXERR_ABORT;` |
|         - |  5616 | `			}` |
|         3 |  5617 | `			goto Synchronize;` |
|         - |  5618 | `	}` |
|      7645 |  5619 | `	pGen->pIn++;` |
|         - |  5620 | `	/* Extract variable name */` |
|      7645 |  5621 | `	pName = &pGen->pIn->sData;` |
|      7645 |  5622 | `	pGen->pIn++; /* Jump the var name */` |
|      7645 |  5623 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5624 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5625 | `		goto Synchronize;` |
|         - |  5626 | `	}` |
|         - |  5627 | `	/* Initialize the structure describing the static variable */` |
|      7645 |  5628 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7645 |  5629 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5630 | `	/* Duplicate variable name */` |
|      7645 |  5631 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      7645 |  5632 | `	if( zDup == 0 ){` |
|       ! 0 |  5633 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5634 | `		return SXERR_ABORT;` |
|         - |  5635 | `	}` |
|      7645 |  5636 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5637 | `	/* Check if we have an expression to compile */` |
|      7645 |  5638 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5639 | `		SySet *pInstrContainer;` |
|         - |  5640 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5641 | `		 * Static variable can take any complex expression including function` |
|         - |  5642 | `		 * call as their initialization value.` |
|         - |  5643 | `		 * Example:` |
|         - |  5644 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5645 | `		 */` |
|      7645 |  5646 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5647 | `		/* Swap bytecode container */` |
|      7645 |  5648 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7645 |  5649 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5650 | `		/* Compile the expression */` |
|      7645 |  5651 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5652 | `		/* Emit the done instruction */` |
|      7645 |  5653 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5654 | `		/* Restore default bytecode container */` |
|      7645 |  5655 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      3820 |  5656 | `	}` |
|         - |  5657 | `	/* Finally save the compiled static variable in the appropriate container */` |
|      7645 |  5658 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|      7645 |  5659 | `	return SXRET_OK;` |
|         1 |  5660 | `Synchronize:` |
|         - |  5661 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5662 | `	 * statement.` |
|         - |  5663 | `	 */` |
|         5 |  5664 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5665 | `		pGen->pIn++;` |
|         1 |  5666 | `	}` |
|         3 |  5667 | `	return SXRET_OK;` |
|      3827 |  5668 | `}` |
|         - |  5669 | `/*` |
|         - |  5670 | ` * Compile the var statement.` |
|         - |  5671 | ` * Symisc Extension:` |
|         - |  5672 | ` *      var statement can be used outside of a class definition.` |
|         - |  5673 | ` */` |
|         4 |  5674 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5675 | `{` |
|         - |  5676 | `	sxu32 nLine;` |
|         - |  5677 | `	sxi32 rc;` |
|         5 |  5678 | `	nLine = pGen->pIn->nLine;` |
|         - |  5679 | `	/* Jump the 'var' keyword */` |
|         5 |  5680 | `	pGen->pIn++;` |
|         5 |  5681 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5682 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5683 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5684 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5685 | `			pGen->pIn++;` |
|       ! 0 |  5686 | `		}` |
|       ! 0 |  5687 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5688 | `			return SXERR_ABORT;` |
|         - |  5689 | `		}` |
|       ! 0 |  5690 | `	}else{` |
|         - |  5691 | `		/* Compile the expression */` |
|         5 |  5692 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5693 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5694 | `			return SXERR_ABORT;` |
|         5 |  5695 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5696 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5697 | `		}` |
|         - |  5698 | `	}` |
|         5 |  5699 | `	return SXRET_OK;` |
|         3 |  5700 | `}` |
|         - |  5701 | `/*` |
|         - |  5702 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5703 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5704 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5705 | ` */` |
|         - |  5706 | `/*` |
|         - |  5707 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5708 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5709 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5710 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5711 | ` *` |
|         - |  5712 | ` * Resolution order:` |
|         - |  5713 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5714 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5715 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5716 | ` *` |
|         - |  5717 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5718 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5719 | ` * Returns the (possibly new) literal index.` |
|         - |  5720 | ` */` |
|   5570090 |  5721 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5722 | `{` |
|         - |  5723 | `	ph7_value *pLit;` |
|         - |  5724 | `	const char *zLit;` |
|         - |  5725 | `	SyString sQualified;` |
|         - |  5726 | `	sxu32 nLit;` |
|         - |  5727 | `	sxu32 k;` |
|         - |  5728 | `	sxu32 nNewIdx;` |
|         - |  5729 | `	int hasNsSep;` |
|         - |  5730 | `	SyHashEntry *pImport;` |
|         - |  5731 | `	ph7_value *pNew;` |
|   5570095 |  5732 | `	if( pFromImport ){` |
|   4497719 |  5733 | `		*pFromImport = 0;` |
|   2248857 |  5734 | `	}` |
|   5570095 |  5735 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   5570095 |  5736 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5737 | `		return nOrigIdx;` |
|         - |  5738 | `	}` |
|   5570095 |  5739 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   5570095 |  5740 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5741 | `	/* Skip if already qualified (contains backslash) */` |
|   5570095 |  5742 | `	hasNsSep = 0;` |
|  66436815 |  5743 | `	for( k = 0; k < nLit; k++ ){` |
|  60866729 |  5744 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  30433365 |  5745 | `	}` |
|   5570095 |  5746 | `	if( hasNsSep ){` |
|         5 |  5747 | `		return nOrigIdx;` |
|         - |  5748 | `	}` |
|         - |  5749 | `	/* Check use imports first (works even outside namespaces) */` |
|   5570091 |  5750 | `	SyBlobReset(&pGen->sWorker);` |
|   5570091 |  5751 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   5570091 |  5752 | `	if( pImport ){` |
|        41 |  5753 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5754 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5755 | `		if( pFromImport ){` |
|        18 |  5756 | `			*pFromImport = 1;` |
|         8 |  5757 | `		}` |
|        23 |  5758 | `	}else{` |
|   5570055 |  5759 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   5569925 |  5760 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5761 | `		}` |
|         - |  5762 | `		/* Prepend current namespace */` |
|       135 |  5763 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       135 |  5764 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       135 |  5765 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5766 | `	}` |
|         - |  5767 | `	/* Look up or create a new literal for the qualified name */` |
|       171 |  5768 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       171 |  5769 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        77 |  5770 | `		return nNewIdx; /* Already interned */` |
|         - |  5771 | `	}` |
|        99 |  5772 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        99 |  5773 | `	if( pNew == 0 ){` |
|       ! 0 |  5774 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5775 | `	}` |
|        99 |  5776 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        99 |  5777 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        99 |  5778 | `	return nNewIdx;` |
|   2785050 |  5779 | `}` |
|         - |  5780 | `/*` |
|         - |  5781 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5782 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5783 | ` */` |
|    448354 |  5784 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5785 | `{` |
|         - |  5786 | `	SyHashEntry *pImport;` |
|         - |  5787 | `	/* Check use imports first */` |
|    448359 |  5788 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    448359 |  5789 | `	if( pImport ){` |
|        21 |  5790 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        21 |  5791 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        21 |  5792 | `		return;` |
|         - |  5793 | `	}` |
|         - |  5794 | `	/* Prepend current namespace if active */` |
|    448341 |  5795 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        14 |  5796 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        14 |  5797 | `		SyBlobAppend(pOut,"\\",1);` |
|         6 |  5798 | `	}` |
|    448341 |  5799 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    224182 |  5800 | `}` |
|         - |  5801 | `/*` |
|         - |  5802 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5803 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5804 | ` * The caller must release pOut when done.` |
|         - |  5805 | ` */` |
|    429394 |  5806 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5807 | `{` |
|    429399 |  5808 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3891 |  5809 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3891 |  5810 | `		SyBlobAppend(pOut,"\\",1);` |
|      1943 |  5811 | `	}` |
|    429399 |  5812 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    429399 |  5813 | `}` |
|         - |  5814 | `/*` |
|         - |  5815 | ` * Compile a namespace statement` |
|         - |  5816 | ` * According to the PHP language reference manual` |
|         - |  5817 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5818 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5819 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5820 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5821 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5822 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5823 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5824 | ` *  programming world.` |
|         - |  5825 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5826 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5827 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5828 | ` *  classes/functions/constants.` |
|         - |  5829 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5830 | ` *  readability of source code.` |
|         - |  5831 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5832 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5833 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5834 | ` *       class MyClass {}` |
|         - |  5835 | ` *       function myfunction() {}` |
|         - |  5836 | ` *       const MYCONST = 1;` |
|         - |  5837 | ` *       $a = new MyClass;` |
|         - |  5838 | ` *       $c = new \my\name\MyClass;` |
|         - |  5839 | ` *       $a = strlen('hi');` |
|         - |  5840 | ` *       $d = namespace\MYCONST;` |
|         - |  5841 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5842 | ` *       echo constant($d);` |
|         - |  5843 | ` * NOTE` |
|         - |  5844 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5845 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5846 | ` */` |
|         - |  5847 | `/*` |
|         - |  5848 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5849 | ` */` |
|        14 |  5850 | `static const char * TokenTypeName(sxu32 nType)` |
|         4 |  5851 | `{` |
|        18 |  5852 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        11 |  5853 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        11 |  5854 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        11 |  5855 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        11 |  5856 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        11 |  5857 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5858 | `	return "token";` |
|        11 |  5859 | `}` |
|      3932 |  5860 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5861 | `{` |
|         - |  5862 | `	sxu32 nLine;` |
|         - |  5863 | `	sxi32 rc;` |
|      3937 |  5864 | `	nLine = pGen->pIn->nLine;` |
|      3937 |  5865 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5866 | `	/* Reset namespace and clear previous use imports */` |
|      3937 |  5867 | `	SyBlobReset(&pGen->sNamespace);` |
|      3937 |  5868 | `	SyHashRelease(&pGen->hUseImports);` |
|      3937 |  5869 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3937 |  5870 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3937 |  5871 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3937 |  5872 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3937 |  5873 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3937 |  5874 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5875 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5876 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5877 | `		return SXRET_OK;` |
|         - |  5878 | `	}` |
|      3937 |  5879 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5880 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5881 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5882 | `		return SXRET_OK;` |
|         - |  5883 | `	}` |
|      3937 |  5884 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5885 | `		/* namespace { } — global namespace block */` |
|         3 |  5886 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         3 |  5887 | `		return SXRET_OK;` |
|         - |  5888 | `	}` |
|         - |  5889 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7915 |  5890 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3985 |  5891 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5892 | `			/* Append backslash separator */` |
|        30 |  5893 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        30 |  5894 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        13 |  5895 | `			}` |
|        17 |  5896 | `		}else{` |
|         - |  5897 | `			/* Append identifier */` |
|      3959 |  5898 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5899 | `		}` |
|      3985 |  5900 | `		pGen->pIn++;` |
|         5 |  5901 | `	}` |
|         - |  5902 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5903 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5904 | `	{` |
|      3935 |  5905 | `		char *zNsDup = 0;` |
|      3935 |  5906 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5897 |  5907 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3928 |  5908 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1964 |  5909 | `		}` |
|      3935 |  5910 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5911 | `	}` |
|      3935 |  5912 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5913 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5914 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5915 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5916 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5917 | `			return SXERR_ABORT;` |
|         - |  5918 | `		}` |
|         2 |  5919 | `	}` |
|      3935 |  5920 | `	return SXRET_OK;` |
|      1971 |  5921 | `}` |
|         - |  5922 | `/*` |
|         - |  5923 | ` * Compile the 'use' statement` |
|         - |  5924 | ` * According to the PHP language reference manual` |
|         - |  5925 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5926 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5927 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5928 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5929 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5930 | ` *  a function or constant is not supported.` |
|         - |  5931 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5932 | ` * NOTE` |
|         - |  5933 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5934 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5935 | ` */` |
|        74 |  5936 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5937 | `{` |
|         - |  5938 | `	sxu32 nLine;` |
|         - |  5939 | `	sxi32 rc;` |
|         - |  5940 | `	SyBlob sPath;` |
|         - |  5941 | `	SyString sAlias;` |
|         - |  5942 | `	SyToken *pLast;` |
|         - |  5943 | `	char *zDup;` |
|         - |  5944 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5945 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5946 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        79 |  5947 | `	nLine = pGen->pIn->nLine;` |
|        79 |  5948 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5949 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        79 |  5950 | `	iUseType = 0;` |
|        79 |  5951 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5952 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5953 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5954 | `			iUseType = 1;` |
|        16 |  5955 | `			pGen->pIn++;` |
|        23 |  5956 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5957 | `			iUseType = 2;` |
|        16 |  5958 | `			pGen->pIn++;` |
|         7 |  5959 | `		}` |
|        14 |  5960 | `	}` |
|         - |  5961 | `	/* Select target hash tables based on import type */` |
|        79 |  5962 | `	switch( iUseType ){` |
|         7 |  5963 | `		case 1:` |
|        16 |  5964 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5965 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5966 | `			break;` |
|         7 |  5967 | `		case 2:` |
|        16 |  5968 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5969 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5970 | `			break;` |
|        23 |  5971 | `		default:` |
|        51 |  5972 | `			pGenHash = &pGen->hUseImports;` |
|        51 |  5973 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        46 |  5974 | `			break;` |
|         - |  5975 | `	}` |
|        79 |  5976 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5977 | `	/* Process one or more use declarations separated by commas */` |
|        38 |  5978 | `	for(;;){` |
|        81 |  5979 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5980 | `			break;` |
|         - |  5981 | `		}` |
|        81 |  5982 | `		SyBlobReset(&sPath);` |
|        81 |  5983 | `		pLast = 0;` |
|         - |  5984 | `		/* Collect the full namespace path */` |
|       277 |  5985 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       201 |  5986 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       139 |  5987 | `				pLast = pGen->pIn;` |
|       139 |  5988 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        67 |  5989 | `					SyBlobAppend(&sPath,"\\",1);` |
|        31 |  5990 | `				}` |
|       139 |  5991 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        67 |  5992 | `			}` |
|       201 |  5993 | `			pGen->pIn++;` |
|         5 |  5994 | `		}` |
|        81 |  5995 | `		if( pLast == 0 ){` |
|         - |  5996 | `			/* Empty path */` |
|         6 |  5997 | `			break;` |
|         - |  5998 | `		}` |
|         - |  5999 | `		/* Default alias is the last component of the path */` |
|        77 |  6000 | `		sAlias = pLast->sData;` |
|         - |  6001 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        72 |  6002 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        52 |  6003 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        25 |  6004 | `			pGen->pIn++; /* Jump 'as' */` |
|        25 |  6005 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        25 |  6006 | `				sAlias = pGen->pIn->sData;` |
|        25 |  6007 | `				pGen->pIn++;` |
|        11 |  6008 | `			}` |
|        11 |  6009 | `		}` |
|         - |  6010 | `		/* Check for duplicate import alias (per-type) */` |
|        77 |  6011 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  6012 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6013 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  6014 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  6015 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6016 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  6017 | `				return SXERR_ABORT;` |
|         - |  6018 | `			}` |
|         2 |  6019 | `		}` |
|         - |  6020 | `		/* Register the import: alias -> FQN.` |
|         - |  6021 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  6022 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  6023 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       113 |  6024 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        72 |  6025 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        77 |  6026 | `		if( zDup ){` |
|        77 |  6027 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        77 |  6028 | `			if( pVmHash ){` |
|         - |  6029 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  6030 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        49 |  6031 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        49 |  6032 | `				if( zAliasDup ){` |
|        49 |  6033 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        22 |  6034 | `				}` |
|        22 |  6035 | `			}` |
|        77 |  6036 | `			if( iUseType == 2 ){` |
|         - |  6037 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  6038 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  6039 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  6040 | `				if( zAliasDup ){` |
|         - |  6041 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  6042 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  6043 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  6044 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  6045 | `					if( azPair ){` |
|        16 |  6046 | `						azPair[0] = zAliasDup;` |
|        16 |  6047 | `						azPair[1] = zDup;` |
|        16 |  6048 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  6049 | `					}` |
|         7 |  6050 | `				}` |
|         7 |  6051 | `			}` |
|        36 |  6052 | `		}` |
|         - |  6053 | `		/* Check for comma (multiple use declarations) */` |
|        77 |  6054 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  6055 | `			pGen->pIn++;` |
|         2 |  6056 | `		}else{` |
|        40 |  6057 | `			break;` |
|         - |  6058 | `		}` |
|         1 |  6059 | `	}` |
|        79 |  6060 | `	SyBlobRelease(&sPath);` |
|        79 |  6061 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  6062 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  6063 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  6064 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6065 | `			return SXERR_ABORT;` |
|         - |  6066 | `		}` |
|         1 |  6067 | `	}` |
|        79 |  6068 | `	return SXRET_OK;` |
|        42 |  6069 | `}` |
|         - |  6070 | `/*` |
|         - |  6071 | ` * Compile the stupid 'declare' language construct.` |
|         - |  6072 | ` *` |
|         - |  6073 | ` * According to the PHP language reference manual.` |
|         - |  6074 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  6075 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  6076 | ` *  declare (directive)` |
|         - |  6077 | ` *   statement` |
|         - |  6078 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  6079 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  6080 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  6081 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  6082 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  6083 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  6084 | ` * <?php` |
|         - |  6085 | ` * // these are the same:` |
|         - |  6086 | ` * // you can use this:` |
|         - |  6087 | ` * declare(ticks=1) {` |
|         - |  6088 | ` *   // entire script here` |
|         - |  6089 | ` * }` |
|         - |  6090 | ` * // or you can use this:` |
|         - |  6091 | ` * declare(ticks=1);` |
|         - |  6092 | ` * // entire script here` |
|         - |  6093 | ` * ?>` |
|         - |  6094 | ` *` |
|         - |  6095 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  6096 | ` */` |
|         - |  6097 | `/*` |
|         - |  6098 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  6099 | ` */` |
|        72 |  6100 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  6101 | `{` |
|       109 |  6102 | `	return SyStringLength(pName) == nWant` |
|        72 |  6103 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  6104 | `}` |
|         - |  6105 |  |
|        42 |  6106 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  6107 | `{` |
|        47 |  6108 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  6109 | `	SyToken *pBodyEnd = 0;` |
|         - |  6110 | `	SyToken *pBodyStart;` |
|         - |  6111 | `	SyToken *pCursor;` |
|         - |  6112 | `	int bHasStrictTypes;` |
|         - |  6113 | `	int bBlockForm;` |
|         - |  6114 | `	int bPlacementOk;` |
|         - |  6115 | `	sxi32 rc;` |
|        47 |  6116 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  6117 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  6118 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         6 |  6119 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6120 | `			return SXERR_ABORT;` |
|         - |  6121 | `		}` |
|         6 |  6122 | `		goto Synchro;` |
|         - |  6123 | `	}` |
|        43 |  6124 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  6125 | `	pBodyStart = pGen->pIn;` |
|         - |  6126 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  6127 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  6128 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  6129 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  6130 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6131 | `			return SXERR_ABORT;` |
|         - |  6132 | `		}` |
|       ! 0 |  6133 | `		return SXRET_OK;` |
|         - |  6134 | `	}` |
|         - |  6135 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  6136 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  6137 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  6138 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  6139 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  6140 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6141 | `			return SXERR_ABORT;` |
|         - |  6142 | `		}` |
|       ! 0 |  6143 | `	}` |
|        43 |  6144 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  6145 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  6146 | `	bHasStrictTypes = 0;` |
|         - |  6147 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  6148 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  6149 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  6150 | `	pCursor = pBodyStart;` |
|        55 |  6151 | `	while( pCursor < pBodyEnd ){` |
|        51 |  6152 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  6153 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  6154 | `				bHasStrictTypes = 1;` |
|        39 |  6155 | `				break;` |
|         - |  6156 | `			}` |
|         2 |  6157 | `		}` |
|        14 |  6158 | `		pCursor++;` |
|         2 |  6159 | `	}` |
|        43 |  6160 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  6161 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6162 | `			"strict_types declaration must not use block mode");` |
|         3 |  6163 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6164 | `		return SXRET_OK;` |
|         - |  6165 | `	}` |
|        41 |  6166 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  6167 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6168 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  6169 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  6170 | `		return SXRET_OK;` |
|         - |  6171 | `	}` |
|         - |  6172 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  6173 | `	pCursor = pBodyStart;` |
|        69 |  6174 | `	while( pCursor < pBodyEnd ){` |
|         - |  6175 | `		SyToken *pNameTok;` |
|         - |  6176 | `		SyToken *pEqTok;` |
|         - |  6177 | `		SyToken *pValTok;` |
|         - |  6178 | `		SyString *pDirName;` |
|         - |  6179 | `		int bIsStrict;` |
|         - |  6180 | `		int iStrictValue;` |
|        39 |  6181 | `		pNameTok = pCursor;` |
|        39 |  6182 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6183 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6184 | `				"declare: Expecting a directive name");` |
|       ! 0 |  6185 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6186 | `			return SXRET_OK;` |
|         - |  6187 | `		}` |
|        39 |  6188 | `		pEqTok = pNameTok + 1;` |
|        39 |  6189 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6190 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6191 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6192 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6193 | `			return SXRET_OK;` |
|         - |  6194 | `		}` |
|        39 |  6195 | `		pValTok = pEqTok + 1;` |
|        39 |  6196 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6197 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6198 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6199 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6200 | `			return SXRET_OK;` |
|         - |  6201 | `		}` |
|        39 |  6202 | `		pDirName = &pNameTok->sData;` |
|        39 |  6203 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6204 | `		if( bIsStrict ){` |
|         - |  6205 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6206 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6207 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6208 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6209 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6210 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6211 | `				return SXRET_OK;` |
|         - |  6212 | `			}` |
|        35 |  6213 | `			iStrictValue = -1;` |
|        35 |  6214 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6215 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6216 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6217 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6218 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6219 | `			}` |
|        35 |  6220 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6221 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6222 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6223 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6224 | `				return SXRET_OK;` |
|         - |  6225 | `			}` |
|        32 |  6226 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6227 | `		}else{` |
|         - |  6228 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6229 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6230 | `			 * behavior don't regress. */` |
|         8 |  6231 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6232 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6233 | `				ph7_lib_version()` |
|         - |  6234 | `				);` |
|         - |  6235 | `		}` |
|        37 |  6236 | `		pCursor = pValTok + 1;` |
|         - |  6237 | `		/* Consume separating comma (or end). */` |
|        37 |  6238 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6239 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6240 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6241 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6242 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6243 | `				return SXRET_OK;` |
|         - |  6244 | `			}` |
|         3 |  6245 | `			pCursor++;` |
|         1 |  6246 | `		}` |
|         5 |  6247 | `	}` |
|         - |  6248 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6249 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6250 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        35 |  6251 | `	return SXRET_OK;` |
|         2 |  6252 | `Synchro:` |
|         - |  6253 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6254 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6255 | `		pGen->pIn++;` |
|         2 |  6256 | `	}` |
|         6 |  6257 | `	return SXRET_OK;` |
|        26 |  6258 | `}` |
|         - |  6259 | `/*` |
|         - |  6260 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6261 | ` * as follows:` |
|         - |  6262 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6263 | ` * {` |
|         - |  6264 | ` *   return "Making a cup of $type.\n";` |
|         - |  6265 | ` * }` |
|         - |  6266 | ` * Symisc eXtension.` |
|         - |  6267 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6268 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6269 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6270 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6271 | ` *      {` |
|         - |  6272 | ` *       var_dump($a);` |
|         - |  6273 | ` *      }` |
|         - |  6274 | ` *     //call test without args` |
|         - |  6275 | ` *      test();` |
|         - |  6276 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6277 | ` *      Example:` |
|         - |  6278 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6279 | ` * 3 -) Function overloading!!` |
|         - |  6280 | ` *      Example:` |
|         - |  6281 | ` *      function foo($a) {` |
|         - |  6282 | ` *   	  return $a.PHP_EOL;` |
|         - |  6283 | ` *	    }` |
|         - |  6284 | ` *	    function foo($a, $b) {` |
|         - |  6285 | ` *   	  return $a + $b;` |
|         - |  6286 | ` *	    }` |
|         - |  6287 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6288 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6289 | ` *      // Same arg` |
|         - |  6290 | ` *	   function foo(string $a)` |
|         - |  6291 | ` *	   {` |
|         - |  6292 | ` *	     echo "a is a string\n";` |
|         - |  6293 | ` *	     var_dump($a);` |
|         - |  6294 | ` *	   }` |
|         - |  6295 | ` *	  function foo(int $a)` |
|         - |  6296 | ` *	  {` |
|         - |  6297 | ` *	    echo "a is integer\n";` |
|         - |  6298 | ` *	    var_dump($a);` |
|         - |  6299 | ` *	  }` |
|         - |  6300 | ` *	  function foo(array $a)` |
|         - |  6301 | ` *	  {` |
|         - |  6302 | ` * 	    echo "a is an array\n";` |
|         - |  6303 | ` * 	    var_dump($a);` |
|         - |  6304 | ` *	  }` |
|         - |  6305 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6306 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6307 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6308 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6309 | ` * introduced by the PH7 engine.` |
|         - |  6310 | ` */` |
|    564926 |  6311 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6312 | `{` |
|         - |  6313 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6314 | `	SySet *pInstrContainer;` |
|         - |  6315 | `	sxi32 rc;` |
|         - |  6316 | `	/* Swap token stream */` |
|    564931 |  6317 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    564931 |  6318 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    564931 |  6319 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6320 | `	/* Compile the expression holding the argument value */` |
|    564931 |  6321 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6322 | `	/* Emit the done instruction */` |
|    564931 |  6323 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    564931 |  6324 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    564931 |  6325 | `	RE_SWAP_DELIMITER(pGen);` |
|    564931 |  6326 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6327 | `		return SXERR_ABORT;` |
|         - |  6328 | `	}` |
|    564931 |  6329 | `	return SXRET_OK;` |
|    282468 |  6330 | `}` |
|         - |  6331 | `/*` |
|         - |  6332 | ` * Collect function arguments one after one.` |
|         - |  6333 | ` * According to the PHP language reference manual.` |
|         - |  6334 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6335 | ` * list of expressions.` |
|         - |  6336 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6337 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6338 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6339 | ` * for more information.` |
|         - |  6340 | ` * Example #1 Passing arrays to functions` |
|         - |  6341 | ` * <?php` |
|         - |  6342 | ` * function takes_array($input)` |
|         - |  6343 | ` * {` |
|         - |  6344 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6345 | ` * }` |
|         - |  6346 | ` * ?>` |
|         - |  6347 | ` * Making arguments be passed by reference` |
|         - |  6348 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6349 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6350 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6351 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6352 | ` * to the argument name in the function definition:` |
|         - |  6353 | ` * Example #2 Passing function parameters by reference` |
|         - |  6354 | ` * <?php` |
|         - |  6355 | ` * function add_some_extra(&$string)` |
|         - |  6356 | ` * {` |
|         - |  6357 | ` *   $string .= 'and something extra.';` |
|         - |  6358 | ` * }` |
|         - |  6359 | ` * $str = 'This is a string, ';` |
|         - |  6360 | ` * add_some_extra($str);` |
|         - |  6361 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6362 | ` * ?>` |
|         - |  6363 | ` *` |
|         - |  6364 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6365 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6366 | ` * on these extension.` |
|         - |  6367 | ` */` |
|   1284144 |  6368 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6369 | `{` |
|         - |  6370 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6371 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6372 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6373 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6374 | `	sxi32 rc;` |
|         - |  6375 |  |
|   1284149 |  6376 | `	pIn = pGen->pIn;` |
|   1284149 |  6377 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6378 | `	/* Process arguments one after one */` |
|   1666234 |  6379 | `	for(;;){` |
|   3332473 |  6380 | `		if( pIn >= pEnd ){` |
|         - |  6381 | `			/* No more arguments to process */` |
|   1284133 |  6382 | `			break;` |
|         - |  6383 | `		}` |
|   2048345 |  6384 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2048345 |  6385 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2048345 |  6386 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2048345 |  6387 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2048345 |  6388 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6389 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6390 | `		 * first token inside the main token stream */` |
|   2048345 |  6391 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6392 | `			return SXERR_ABORT;` |
|         - |  6393 | `		}` |
|         - |  6394 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6395 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6396 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6397 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6398 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6399 | `		{` |
|   2048345 |  6400 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2048345 |  6401 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2048345 |  6402 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6403 | `			int nSetTok;` |
|         - |  6404 | `			sxi32 nSetVis;` |
|   2048345 |  6405 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6406 | `				bReadonly = 1;` |
|         3 |  6407 | `				pIn++;` |
|         1 |  6408 | `			}` |
|   2048345 |  6409 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2048345 |  6410 | `			if( nSetVis ){` |
|         - |  6411 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6412 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6413 | `				bVisSeen = 1;` |
|         3 |  6414 | `				pIn += nSetTok;` |
|         3 |  6415 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6416 | `					bReadonly = 1;` |
|       ! 0 |  6417 | `					pIn++;` |
|         1 |  6418 | `				}` |
|   2048344 |  6419 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     88167 |  6420 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     88167 |  6421 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6422 | `					bVisSeen = 1;` |
|        89 |  6423 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6424 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6425 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6426 | `					pIn++;` |
|        89 |  6427 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6428 | `					if( nSetVis ){` |
|         - |  6429 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6430 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6431 | `						pIn += nSetTok;` |
|         1 |  6432 | `					}` |
|        89 |  6433 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6434 | `						bReadonly = 1;` |
|        18 |  6435 | `						pIn++;` |
|         7 |  6436 | `					}` |
|        42 |  6437 | `				}` |
|     44081 |  6438 | `			}` |
|   2048345 |  6439 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6440 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2048343 |  6441 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6442 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6443 | `			}` |
|   2048345 |  6444 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6445 | `				if( !bCtorCtx ){` |
|         6 |  6446 | `					if( bAbstractCtx ){` |
|         3 |  6447 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6448 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6449 | `					}else{` |
|         3 |  6450 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6451 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6452 | `					}` |
|         6 |  6453 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6454 | `						return SXERR_ABORT;` |
|         - |  6455 | `					}` |
|         6 |  6456 | `					return SXERR_SYNTAX;` |
|         - |  6457 | `				}` |
|        89 |  6458 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6459 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6460 | `				if( bReadonly ){` |
|        20 |  6461 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6462 | `				}` |
|        42 |  6463 | `			}` |
|         - |  6464 | `		}` |
|         - |  6465 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   2048336 |  6466 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1104622 |  6467 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    149448 |  6468 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    115005 |  6469 | `			sxu32 nLineLocal = pIn->nLine;` |
|    115005 |  6470 | `			sxi32 iTFlags = 0;` |
|    115005 |  6471 | `			pGen->pIn = pIn;` |
|    115005 |  6472 | `			rc = GenStateParseUnionTypeDecl(` |
|     57500 |  6473 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57500 |  6474 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6475 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6476 | `				/* bAllowVoid */ 0,` |
|     57500 |  6477 | `						nLineLocal);` |
|    115005 |  6478 | `			pIn = pGen->pIn;` |
|    115005 |  6479 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6480 | `				return SXERR_ABORT;` |
|    115005 |  6481 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6482 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6483 | `				return SXERR_SYNTAX;` |
|    115003 |  6484 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6485 | `				if( pIn < pEnd ){` |
|        15 |  6486 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6487 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6488 | `						&pIn->sData);` |
|         7 |  6489 | `				}else{` |
|       ! 0 |  6490 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6491 | `						"syntax error, unexpected end of file");` |
|         - |  6492 | `				}` |
|        11 |  6493 | `				return SXERR_SYNTAX;` |
|         - |  6494 | `			}` |
|    114995 |  6495 | `			sArg.iFlags \|= iTFlags;` |
|     57495 |  6496 | `		}` |
|   2048331 |  6497 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6498 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6499 | `			return rc;` |
|         - |  6500 | `		}` |
|   2048331 |  6501 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6502 | `			/* Pass by reference,record that */` |
|     22943 |  6503 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     22943 |  6504 | `			pIn++;` |
|     11469 |  6505 | `		}` |
|   2048331 |  6506 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6507 | `			/* Variadic parameter: ...$args */` |
|     23005 |  6508 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     23005 |  6509 | `			pIn++;` |
|     11500 |  6510 | `		}` |
|   2048331 |  6511 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6512 | `			/* Invalid argument */` |
|       ! 0 |  6513 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6514 | `			return rc;` |
|         - |  6515 | `		}` |
|   2048331 |  6516 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6517 | `		/* Copy argument name */` |
|   2048331 |  6518 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2048331 |  6519 | `		if( zDup == 0 ){` |
|       ! 0 |  6520 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6521 | `			return SXERR_ABORT;` |
|         - |  6522 | `		}` |
|   2048331 |  6523 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2048331 |  6524 | `		pIn++;` |
|   2048331 |  6525 | `		if( pIn < pEnd ){` |
|   1142101 |  6526 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6527 | `				SyToken *pDefend;` |
|    564933 |  6528 | `				sxi32 iNest = 0;` |
|    564933 |  6529 | `				pIn++; /* Jump the equal sign */` |
|    564933 |  6530 | `				pDefend = pIn;` |
|         - |  6531 | `				/* Process the default value associated with this argument */` |
|   1187127 |  6532 | `				while( pDefend < pEnd ){` |
|    809233 |  6533 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    187039 |  6534 | `						break;` |
|         - |  6535 | `					}` |
|    622199 |  6536 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6537 | `						/* Increment nesting level */` |
|     26725 |  6538 | `						iNest++;` |
|    608839 |  6539 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6540 | `						/* Decrement nesting level */` |
|     26725 |  6541 | `						iNest--;` |
|     13360 |  6542 | `					}` |
|    622199 |  6543 | `					pDefend++;` |
|         5 |  6544 | `				}` |
|    564933 |  6545 | `				if( pIn >= pDefend ){` |
|         3 |  6546 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6547 | `					return rc;` |
|         - |  6548 | `				}` |
|         - |  6549 | `				/* Process default value */` |
|    564931 |  6550 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    564931 |  6551 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6552 | `					return rc;` |
|         - |  6553 | `				}` |
|         - |  6554 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6555 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6556 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6557 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6558 | `				 * arg-type check lets null through. */` |
|    564926 |  6559 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    307290 |  6560 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    307287 |  6561 | `					&& &pIn[1] == pDefend` |
|     45831 |  6562 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     34366 |  6563 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     20999 |  6564 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15275 |  6565 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6566 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6567 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6568 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6569 | `					 * already up at this point). */` |
|         - |  6570 | `					{` |
|     15275 |  6571 | `						const char *zSep = "";` |
|     15275 |  6572 | `						SyString sCls = { "", 0 };` |
|     15275 |  6573 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15269 |  6574 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15269 |  6575 | `							zSep = "::";` |
|      7632 |  6576 | `						}` |
|     22910 |  6577 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6578 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7635 |  6579 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6580 | `					}` |
|      7635 |  6581 | `				}` |
|         - |  6582 | `				/* Point beyond the default value */` |
|    564931 |  6583 | `				pIn = pDefend;` |
|    282463 |  6584 | `			}` |
|   1142099 |  6585 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6586 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6587 | `				return rc;` |
|         - |  6588 | `			}` |
|   1142099 |  6589 | `			pIn++; /* Jump the trailing comma */` |
|    571047 |  6590 | `		}` |
|         - |  6591 | `		/* Append argument signature */` |
|   2048329 |  6592 | `		if( sArg.nType > 0 ){` |
|    114933 |  6593 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6594 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26805 |  6595 | `				int marker = 'o';` |
|     26805 |  6596 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26805 |  6597 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13405 |  6598 | `			}else{` |
|         - |  6599 | `				int c;` |
|     88133 |  6600 | `				c = 'n'; /* cc warning */` |
|         - |  6601 | `				/* Type leading character */` |
|     88133 |  6602 | `				switch(sArg.nType){` |
|      5730 |  6603 | `				case MEMOBJ_HASHMAP:` |
|         - |  6604 | `					/* Hashmap aka 'array' */` |
|     11465 |  6605 | `					c = 'h';` |
|     11465 |  6606 | `					break;` |
|      9660 |  6607 | `				case MEMOBJ_INT:` |
|         - |  6608 | `					/* Integer */` |
|     19325 |  6609 | `					c = 'i';` |
|     19325 |  6610 | `					break;` |
|         2 |  6611 | `				case MEMOBJ_BOOL:` |
|         - |  6612 | `					/* Bool */` |
|         5 |  6613 | `					c = 'b';` |
|         5 |  6614 | `					break;` |
|         5 |  6615 | `				case MEMOBJ_REAL:` |
|         - |  6616 | `					/* Float */` |
|        12 |  6617 | `					c = 'f';` |
|        12 |  6618 | `					break;` |
|     28659 |  6619 | `				case MEMOBJ_STRING:` |
|         - |  6620 | `					/* String */` |
|     57323 |  6621 | `					c = 's';` |
|     57323 |  6622 | `					break;` |
|         7 |  6623 | `				case MEMOBJ_OBJ:` |
|         - |  6624 | `					/* Object */` |
|        16 |  6625 | `					c = 'o';` |
|        14 |  6626 | `					break;` |
|         1 |  6627 | `				default:` |
|         2 |  6628 | `					break;` |
|         - |  6629 | `				}` |
|     88133 |  6630 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6631 | `			}` |
|     57469 |  6632 | `		}else{` |
|         - |  6633 | `			/* No type is associated with this parameter which mean` |
|         - |  6634 | `			 * that this function is not condidate for overloading.` |
|         - |  6635 | `			 */` |
|   1933401 |  6636 | `			SyBlobRelease(&sSig);` |
|         - |  6637 | `		}` |
|         - |  6638 | `		/* Save in the argument set */` |
|   2048329 |  6639 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6640 | `	}` |
|   1284133 |  6641 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6642 | `		/* Save function signature */` |
|     84333 |  6643 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     42164 |  6644 | `	}` |
|   1284133 |  6645 | `	return SXRET_OK;` |
|    642077 |  6646 | `}` |
|         - |  6647 | `/*` |
|         - |  6648 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6649 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6650 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6651 | ` */` |
|     34388 |  6652 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6653 | `{` |
|     34393 |  6654 | `	sxi32 iParen = 0;` |
|     34393 |  6655 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6656 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6657 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6658 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    152889 |  6659 | `	while( pIn < pEnd ){` |
|    152889 |  6660 | `		sxu32 t = pIn->nType;` |
|    152889 |  6661 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    149017 |  6662 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    103169 |  6663 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     84045 |  6664 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    118501 |  6665 | `		pIn++;` |
|         5 |  6666 | `	}` |
|     19129 |  6667 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6668 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6669 | `	{` |
|     19129 |  6670 | `		sxi32 d = 0;` |
|    759885 |  6671 | `		while( pIn < pEnd ){` |
|    759885 |  6672 | `			sxu32 t = pIn->nType;` |
|    759885 |  6673 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    729305 |  6674 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    740761 |  6675 | `			pIn++;` |
|         5 |  6676 | `		}` |
|         - |  6677 | `	}` |
|     19129 |  6678 | `	return pIn;` |
|     17199 |  6679 | `}` |
|         - |  6680 | `/*` |
|         - |  6681 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6682 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6683 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6684 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6685 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6686 | ` * detached-mini-program path untouched.` |
|         - |  6687 | ` */` |
|         - |  6688 | `/*` |
|         - |  6689 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6690 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6691 | ` * mixed, object.` |
|         - |  6692 | ` */` |
|     11472 |  6693 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6694 | `{` |
|         - |  6695 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6696 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6697 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6698 | `	};` |
|         - |  6699 | `	sxu32 i;` |
|     11477 |  6700 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6701 | `		zName++;` |
|       ! 0 |  6702 | `		nName--;` |
|       ! 0 |  6703 | `	}` |
|     11485 |  6704 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11485 |  6705 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11477 |  6706 | `			return 1;` |
|         - |  6707 | `		}` |
|         5 |  6708 | `	}` |
|       ! 0 |  6709 | `	return 0;` |
|      5741 |  6710 | `}` |
|         - |  6711 | `/*` |
|         - |  6712 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6713 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6714 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6715 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6716 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6717 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6718 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6719 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6720 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6721 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6722 | ` */` |
|     11474 |  6723 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6724 | `{` |
|     11479 |  6725 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6726 | ``		return 1; /* bare `object` */`` |
|         - |  6727 | `	}` |
|     11479 |  6728 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6729 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6730 | `	}` |
|     11477 |  6731 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11477 |  6732 | `		return 1;` |
|         - |  6733 | `	}` |
|         - |  6734 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6735 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6736 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6737 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6738 | `	{` |
|         - |  6739 | `		SyBlob sFQN;` |
|         - |  6740 | `		int bOk;` |
|       ! 0 |  6741 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       ! 0 |  6742 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       ! 0 |  6743 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       ! 0 |  6744 | `		SyBlobRelease(&sFQN);` |
|       ! 0 |  6745 | `		return bOk;` |
|         - |  6746 | `	}` |
|      5742 |  6747 | `}` |
|         - |  6748 | `/*` |
|         - |  6749 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6750 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6751 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6752 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6753 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6754 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6755 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6756 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6757 | ` */` |
|     11714 |  6758 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6759 | `{` |
|     11719 |  6760 | `	int bOk = 0;` |
|         - |  6761 | `	sxu32 nLine;` |
|         - |  6762 | `	sxi32 rc;` |
|     11719 |  6763 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       245 |  6764 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6765 | `	}` |
|     11479 |  6766 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6767 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6768 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6769 | `		sxu32 i,j;` |
|       ! 0 |  6770 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6771 | `			int bGroupOk;` |
|       ! 0 |  6772 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6773 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6774 | `			}` |
|       ! 0 |  6775 | `			bGroupOk = 1;` |
|       ! 0 |  6776 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6777 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6778 | `					bGroupOk = 0;` |
|       ! 0 |  6779 | `					break;` |
|         - |  6780 | `				}` |
|       ! 0 |  6781 | `			}` |
|       ! 0 |  6782 | `			bOk = bGroupOk;` |
|       ! 0 |  6783 | `		}` |
|       ! 0 |  6784 | `	}else{` |
|     11479 |  6785 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6786 | `	}` |
|     11479 |  6787 | `	if( bOk ){` |
|     11477 |  6788 | `		return SXRET_OK;` |
|         - |  6789 | `	}` |
|         - |  6790 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6791 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6792 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6793 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6794 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6795 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6796 | `	{` |
|         3 |  6797 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6798 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6799 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6800 | `		}` |
|         3 |  6801 | `		if( sGiven.nByte < 1 ){` |
|         - |  6802 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6803 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6804 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6805 | `			const char *zScalar =` |
|       ! 0 |  6806 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6807 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6808 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6809 | `		}` |
|         3 |  6810 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6811 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6812 | `	}` |
|         3 |  6813 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5862 |  6814 | `}` |
|   2739796 |  6815 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6816 | `{` |
|   2739801 |  6817 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2739801 |  6818 | `	SyToken *pEnd = pGen->pEnd;` |
|   2739801 |  6819 | `	sxi32 iDepth = 0;` |
|   2739801 |  6820 | `	int bStarted = 0;` |
| 132267811 |  6821 | `	while( pIn < pEnd ){` |
| 132267811 |  6822 | `		sxu32 t = pIn->nType;` |
| 132267811 |  6823 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 126340653 |  6824 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 120448221 |  6825 | `		if( t & PH7_TK_KEYWORD ){` |
|   8957931 |  6826 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   8957931 |  6827 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   8946217 |  6828 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6829 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   4455912 |  6830 | `		}` |
| 120402119 |  6831 | `		pIn++;` |
|         5 |  6832 | `	}` |
|   2728087 |  6833 | `	return FALSE;` |
|   1369903 |  6834 | `}` |
|         - |  6835 | `/*` |
|         - |  6836 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6837 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6838 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6839 | ` */` |
|   2739796 |  6840 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6841 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6842 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6843 | `	)` |
|         5 |  6844 | `{` |
|         - |  6845 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6846 | `	GenBlock *pBlock;` |
|         - |  6847 | `	sxu32 nGotoOfft;` |
|         - |  6848 | `	sxi32 rc;` |
|         - |  6849 | `	/* Attach the new function */` |
|   2739801 |  6850 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2739801 |  6851 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6852 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6853 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6854 | `		return SXERR_ABORT;` |
|         - |  6855 | `	}` |
|   2739801 |  6856 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6857 | `	/* Swap bytecode containers */` |
|   2739801 |  6858 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2739801 |  6859 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6860 | `	/* Emit constructor property promotion prologue:` |
|         - |  6861 | `	 *   $this->NAME = $NAME;` |
|         - |  6862 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6863 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6864 | `	{` |
|   2739801 |  6865 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6866 | `		sxu32 i;` |
|   4734563 |  6867 | `		for( i = 0; i < nArg; i++ ){` |
|   1994767 |  6868 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6869 | `			char *zSrc;` |
|         - |  6870 | `			sxu32 nSrc,nName;` |
|         - |  6871 | `			SySet sToken;` |
|         - |  6872 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6873 | `			sxi32 rcPromote;` |
|   1994767 |  6874 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1994693 |  6875 | `				continue;` |
|         - |  6876 | `			}` |
|         - |  6877 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6878 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6879 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6880 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6881 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6882 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6883 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6884 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6885 | `			if( zSrc == 0 ){` |
|       ! 0 |  6886 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6887 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6888 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6889 | `				return SXERR_ABORT;` |
|         - |  6890 | `			}` |
|         - |  6891 | `			{` |
|        79 |  6892 | `				char *z = zSrc;` |
|        79 |  6893 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6894 | `				z += sizeof("$this->")-1;` |
|        79 |  6895 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6896 | `				z += nName;` |
|        79 |  6897 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6898 | `				z += sizeof(" = $")-1;` |
|        79 |  6899 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6900 | `				z += nName;` |
|        79 |  6901 | `				*z = 0;` |
|         - |  6902 | `			}` |
|        79 |  6903 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6904 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6905 | `			pTmpIn = pGen->pIn;` |
|        79 |  6906 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6907 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6908 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6909 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6910 | `			pGen->pIn = pTmpIn;` |
|        79 |  6911 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6912 | `			SySetRelease(&sToken);` |
|        79 |  6913 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6914 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6915 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6916 | `				return SXERR_ABORT;` |
|         - |  6917 | `			}` |
|         - |  6918 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6919 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6920 | `		}` |
|         - |  6921 | `	}` |
|         - |  6922 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6923 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6924 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6925 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6926 | `	{` |
|   2739801 |  6927 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2739801 |  6928 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6929 | `		/* Compile the body */` |
|   2739801 |  6930 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2739801 |  6931 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6932 | `	}` |
|         - |  6933 | `	/* Fix exception jumps now the destination is resolved */` |
|   2739801 |  6934 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6935 | `	/* Emit the final return if not yet done */` |
|   2739801 |  6936 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6937 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2739801 |  6938 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6939 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6940 | `	}` |
|   2739801 |  6941 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6942 | `	/* Restore the default container */` |
|   2739801 |  6943 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6944 | `	/* Leave function block */` |
|   2739801 |  6945 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2739801 |  6946 | `	if( rc == SXERR_ABORT ){` |
|         - |  6947 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6948 | `		return SXERR_ABORT;` |
|         - |  6949 | `	}` |
|         - |  6950 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6951 | `	{` |
|   2739801 |  6952 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6953 | `		sxu32 i;` |
|  80796679 |  6954 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  78068597 |  6955 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11719 |  6956 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11719 |  6957 | `				break;` |
|         - |  6958 | `			}` |
|  39028444 |  6959 | `		}` |
|         - |  6960 | `	}` |
|   2739801 |  6961 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6962 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11719 |  6963 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6964 | `			return SXERR_ABORT;` |
|         - |  6965 | `		}` |
|      5857 |  6966 | `	}` |
|         - |  6967 | `	/* All done, function body compiled */` |
|   2739801 |  6968 | `	return SXRET_OK;` |
|   1369903 |  6969 | `}` |
|         - |  6970 | `/*` |
|         - |  6971 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  6972 | ` * According to the PHP language reference manual.` |
|         - |  6973 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  6974 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  6975 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  6976 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  6977 | ` *  Functions need not be defined before they are referenced.` |
|         - |  6978 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  6979 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  6980 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  6981 | ` *  calls with over 32-64 recursion levels.` |
|         - |  6982 | ` *` |
|         - |  6983 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  6984 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  6985 | ` * on these extension.` |
|         - |  6986 | ` */` |
|         - |  6987 | `/*` |
|         - |  6988 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  6989 | ` */` |
|       570 |  6990 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  6991 | `{` |
|         - |  6992 | `	sxu32 i;` |
|      1611 |  6993 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  6994 | `		int a = zA[i], b = zB[i];` |
|      1381 |  6995 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  6996 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  6997 | `		if( a != b ) return a - b;` |
|       523 |  6998 | `	}` |
|       235 |  6999 | `	return 0;` |
|       290 |  7000 | `}` |
|         - |  7001 | `/*` |
|         - |  7002 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  7003 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  7004 | ` * (which are positive bit values stored in sxu32).` |
|         - |  7005 | ` */` |
|         - |  7006 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  7007 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  7008 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  7009 |  |
|         - |  7010 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  7011 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  7012 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  7013 |  |
|         - |  7014 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  7015 | `struct PhlTypeAtom {` |
|         - |  7016 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  7017 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  7018 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  7019 | `	sxu32 nCanon;` |
|         - |  7020 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  7021 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  7022 | `};` |
|         - |  7023 |  |
|         - |  7024 | `/*` |
|         - |  7025 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  7026 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  7027 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  7028 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  7029 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  7030 | ` * already be consumed by the caller.` |
|         - |  7031 | ` */` |
|    127690 |  7032 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  7033 | `{` |
|    127695 |  7034 | `	SyToken *pIn = pGen->pIn;` |
|    127695 |  7035 | `	int bAbsolute = 0;` |
|    127695 |  7036 | `	SyZero(pOut, sizeof(*pOut));` |
|    127695 |  7037 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    127695 |  7038 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7039 | `		return SXERR_SYNTAX;` |
|         - |  7040 | `	}` |
|         - |  7041 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    127695 |  7042 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        10 |  7043 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|        10 |  7044 | `		pIn++;` |
|        10 |  7045 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7046 | `			return SXERR_SYNTAX;` |
|         - |  7047 | `		}` |
|         4 |  7048 | `	}` |
|    127695 |  7049 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7050 | `		return SXERR_SYNTAX;` |
|         - |  7051 | `	}` |
|    127695 |  7052 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     88985 |  7053 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     88985 |  7054 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11523 |  7055 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     83226 |  7056 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        83 |  7057 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     77428 |  7058 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19733 |  7059 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67525 |  7060 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57573 |  7061 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     28877 |  7062 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  7063 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        75 |  7064 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        28 |  7065 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        44 |  7066 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        16 |  7067 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        29 |  7068 | `			pOut->nType = SXU32_HIGH;` |
|        29 |  7069 | `			pOut->sClass = pIn->sData;` |
|        16 |  7070 | `		}else{` |
|         3 |  7071 | `			return SXERR_SYNTAX;` |
|         - |  7072 | `		}` |
|     88983 |  7073 | `		pIn++;` |
|     44494 |  7074 | `	}else{` |
|         - |  7075 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  7076 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38715 |  7077 | `		SyString *pT = &pIn->sData;` |
|     38715 |  7078 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  7079 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  7080 | `			pIn++;` |
|     38700 |  7081 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  7082 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  7083 | `			pIn++;` |
|     38599 |  7084 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        27 |  7085 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        27 |  7086 | `			pIn++;` |
|        16 |  7087 | `		}else{` |
|         - |  7088 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38491 |  7089 | `			SyToken *pFirst = pIn;` |
|     38491 |  7090 | `			SyToken *pLast = pIn;` |
|     38491 |  7091 | `			pOut->nType = SXU32_HIGH;` |
|     38491 |  7092 | `			pOut->sClass = pIn->sData;` |
|     38491 |  7093 | `			pIn++;` |
|     57732 |  7094 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38494 |  7095 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  7096 | `				pLast = &pIn[1];` |
|         3 |  7097 | `				pIn += 2;` |
|         1 |  7098 | `			}` |
|     38491 |  7099 | `			if( pLast != pFirst ){` |
|         3 |  7100 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  7101 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  7102 | `				pOut->sClass.zString = zFirst;` |
|         3 |  7103 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  7104 | `			}` |
|         - |  7105 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|         - |  7106 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|         - |  7107 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|         - |  7108 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|         - |  7109 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|     38491 |  7110 | `			if( !bAbsolute && pLast == pFirst ){` |
|         - |  7111 | `				SyBlob sFqn;` |
|     38483 |  7112 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     38483 |  7113 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|     38478 |  7114 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|     38478 |  7115 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|        17 |  7116 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 |  7117 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|        12 |  7118 | `					if( zDup ){` |
|        12 |  7119 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|         5 |  7120 | `					}` |
|         5 |  7121 | `				}` |
|     38483 |  7122 | `				SyBlobRelease(&sFqn);` |
|     19239 |  7123 | `			}` |
|         - |  7124 | `		}` |
|         - |  7125 | `	}` |
|    127693 |  7126 | `	pGen->pIn = pIn;` |
|    127693 |  7127 | `	return SXRET_OK;` |
|     63850 |  7128 | `}` |
|         - |  7129 |  |
|         - |  7130 | `/*` |
|         - |  7131 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  7132 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  7133 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  7134 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  7135 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  7136 | ` */` |
|    127512 |  7137 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  7138 | `{` |
|         - |  7139 | `	int i;` |
|    127517 |  7140 | `	int nNonNull = 0;` |
|    127517 |  7141 | `	int bAnyIntersection = 0;` |
|         - |  7142 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    127517 |  7143 | `	sxu32 nMaxGroup = 0;` |
|   4207901 |  7144 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    255181 |  7145 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127669 |  7146 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127639 |  7147 | `			nNonNull++;` |
|    127639 |  7148 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    127639 |  7149 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    127639 |  7150 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     63817 |  7151 | `			}` |
|     63817 |  7152 | `		}` |
|     63837 |  7153 | `	}` |
|    255129 |  7154 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127641 |  7155 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  7156 | `			bAnyIntersection = 1;` |
|        29 |  7157 | `			break;` |
|         - |  7158 | `		}` |
|     63811 |  7159 | `	}` |
|    127517 |  7160 | `	if( bAnyIntersection ){` |
|         - |  7161 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  7162 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  7163 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  7164 | `		sxu32 g, nGroups = 0;` |
|        29 |  7165 | `		int bFirstGroup = 1;` |
|        59 |  7166 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  7167 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  7168 | `			int bFirstMember = 1;` |
|         - |  7169 | `			int bWrap;` |
|        35 |  7170 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  7171 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  7172 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  7173 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  7174 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  7175 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  7176 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  7177 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  7178 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  7179 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  7180 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  7181 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  7182 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  7183 | `				}else{` |
|         6 |  7184 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7185 | `				}` |
|        59 |  7186 | `				bFirstMember = 0;` |
|        32 |  7187 | `			}` |
|        35 |  7188 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7189 | `			bFirstGroup = 0;` |
|        20 |  7190 | `		}` |
|        29 |  7191 | `		if( bNullable ){` |
|       ! 0 |  7192 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7193 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7194 | `		}` |
|        85 |  7195 | `		return;` |
|         - |  7196 | `	}` |
|    127493 |  7197 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7198 | `		/* Shorthand: ?T */` |
|       117 |  7199 | `		for( i = 0; i < nAtoms; i++ ){` |
|       117 |  7200 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       117 |  7201 | `			SyBlobAppend(pBlob, "?", 1);` |
|       117 |  7202 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        24 |  7203 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  7204 | `			}else{` |
|        95 |  7205 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7206 | `			}` |
|       117 |  7207 | `			return;` |
|       ! 0 |  7208 | `		}` |
|       ! 0 |  7209 | `	}` |
|         - |  7210 | `	{` |
|    127381 |  7211 | `		int bFirst = 1;` |
|         - |  7212 | `		/* 1) Classes in declaration order */` |
|    254865 |  7213 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127489 |  7214 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38445 |  7215 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38445 |  7216 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38445 |  7217 | `				bFirst = 0;` |
|     19220 |  7218 | `			}` |
|     63747 |  7219 | `		}` |
|         - |  7220 | `		/* 2) Built-ins in canonical order */` |
|         - |  7221 | `		{` |
|         - |  7222 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7223 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7224 | `			int k;` |
|    891637 |  7225 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1440211 |  7226 | `				for( i = 0; i < nAtoms; i++ ){` |
|    764797 |  7227 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     88847 |  7228 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     88847 |  7229 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     88847 |  7230 | `						bFirst = 0;` |
|     88847 |  7231 | `						break;` |
|         - |  7232 | `					}` |
|    337980 |  7233 | `				}` |
|    382133 |  7234 | `			}` |
|         - |  7235 | `		}` |
|         - |  7236 | `		/* 3) null suffix */` |
|    127381 |  7237 | `		if( bNullable ){` |
|        20 |  7238 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        20 |  7239 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7240 | `		}` |
|         - |  7241 | `	}` |
|     63761 |  7242 | `}` |
|         - |  7243 |  |
|         - |  7244 | `/*` |
|         - |  7245 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7246 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7247 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7248 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7249 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7250 | ` * whether it was parenthesized.` |
|         - |  7251 | ` *` |
|         - |  7252 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7253 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7254 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7255 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7256 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7257 | ` */` |
|    127664 |  7258 | `static sxi32 GenStateParsePart(` |
|         - |  7259 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7260 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7261 | `{` |
|         - |  7262 | `	sxi32 rc;` |
|    127669 |  7263 | `	int nMembers = 0;` |
|    127669 |  7264 | `	int bParen = 0;` |
|    127669 |  7265 | `	*pnMembers = 0;` |
|    127669 |  7266 | `	*pbParen = 0;` |
|    127669 |  7267 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7268 | `		bParen = 1;` |
|         9 |  7269 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7270 | `	}` |
|     63832 |  7271 | `	for(;;){` |
|    127695 |  7272 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7273 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7274 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7275 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7276 | `		}` |
|    127695 |  7277 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    127695 |  7278 | `		if( rc != SXRET_OK ){` |
|         3 |  7279 | `			return rc;` |
|         - |  7280 | `		}` |
|    127693 |  7281 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    127693 |  7282 | `		(*pnAtoms)++;` |
|    127693 |  7283 | `		nMembers++;` |
|         - |  7284 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    127693 |  7285 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7286 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7287 | `			if( pNext < pGen->pEnd` |
|        39 |  7288 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7289 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7290 | `				continue;` |
|         - |  7291 | `			}` |
|         4 |  7292 | `		}` |
|    127667 |  7293 | `		break;` |
|       ! 0 |  7294 | `	}` |
|    127667 |  7295 | `	if( bParen ){` |
|         9 |  7296 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7297 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7298 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7299 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7300 | `		}` |
|         9 |  7301 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7302 | `		if( nMembers < 2 ){` |
|       ! 0 |  7303 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7304 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7305 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7306 | `		}` |
|         3 |  7307 | `	}` |
|    127667 |  7308 | `	*pnMembers = nMembers;` |
|    127667 |  7309 | `	*pbParen = bParen;` |
|    127667 |  7310 | `	return SXRET_OK;` |
|     63837 |  7311 | `}` |
|         - |  7312 |  |
|         - |  7313 | `/*` |
|         - |  7314 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7315 | ` *` |
|         - |  7316 | ` * Outputs:` |
|         - |  7317 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7318 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7319 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7320 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7321 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7322 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7323 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7324 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7325 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7326 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7327 | ` *` |
|         - |  7328 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7329 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7330 | ` */` |
|    127528 |  7331 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7332 | `	ph7_gen_state *pGen,` |
|         - |  7333 | `	sxu32 *pnType,` |
|         - |  7334 | `	SyString *pClass,` |
|         - |  7335 | `	SySet *pAlts,` |
|         - |  7336 | `	sxi32 *piTypeFlags,` |
|         - |  7337 | `	SyString *pTypeText,` |
|         - |  7338 | `	int iNullableFlag,` |
|         - |  7339 | `	int iUnionFlag,` |
|         - |  7340 | `	int bAllowVoid,` |
|         - |  7341 | `	sxu32 nLine` |
|         5 |  7342 | `){` |
|         - |  7343 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    127533 |  7344 | `	int nAtoms = 0;` |
|    127533 |  7345 | `	int bShortNullable = 0;` |
|    127533 |  7346 | `	int bExplicitNull = 0;` |
|         - |  7347 | `	sxi32 rc;` |
|    127533 |  7348 | `	*pnType = 0;` |
|    127533 |  7349 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    127533 |  7350 | `	*piTypeFlags = 0;` |
|    127533 |  7351 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7352 |  |
|    127533 |  7353 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7354 | `		return SXRET_OK;` |
|         - |  7355 | `	}` |
|         - |  7356 | ``	/* Optional `?` shorthand prefix */`` |
|    127528 |  7357 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       105 |  7358 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       105 |  7359 | `		bShortNullable = 1;` |
|       105 |  7360 | `		pGen->pIn++;` |
|       105 |  7361 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7362 | `			return SXERR_SYNTAX;` |
|         - |  7363 | `		}` |
|        50 |  7364 | `	}` |
|         - |  7365 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7366 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7367 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7368 | `	{` |
|         - |  7369 | `		int nMembers, bParen;` |
|    127533 |  7370 | `		sxu32 iGroup = 0;` |
|    127533 |  7371 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    127533 |  7372 | `		if( rc != SXRET_OK ){` |
|         4 |  7373 | `			return rc;` |
|         - |  7374 | `		}` |
|         - |  7375 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7376 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7377 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7378 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7379 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    191498 |  7380 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    127740 |  7381 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7382 | `			if( bShortNullable ){` |
|         - |  7383 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7384 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7385 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7386 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7387 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7388 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7389 | `			}` |
|       141 |  7390 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7391 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7392 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7393 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7394 | `			}` |
|       141 |  7395 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7396 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7397 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7398 | `				return rc;` |
|         - |  7399 | `			}` |
|         5 |  7400 | `		}` |
|    127529 |  7401 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7402 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7403 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7404 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7405 | `		}` |
|         - |  7406 | `	}` |
|         - |  7407 | `	/* Validation pass.` |
|         - |  7408 | `	 *` |
|         - |  7409 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7410 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7411 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7412 | `	 */` |
|         - |  7413 | `	{` |
|         - |  7414 | `		int i, j;` |
|    127529 |  7415 | `		int bHasNonNull = 0;` |
|    127529 |  7416 | `		int bAnyIntersection = 0;` |
|         - |  7417 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7418 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7419 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4208297 |  7420 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    255215 |  7421 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127691 |  7422 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     63848 |  7423 | `		}` |
|    255159 |  7424 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127661 |  7425 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     63820 |  7426 | `		}` |
|         - |  7427 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7428 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    127529 |  7429 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7430 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7431 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7432 | `			return SXERR_SYNTAX;` |
|         - |  7433 | `		}` |
|    255201 |  7434 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7435 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7436 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7437 | ``			 * `true`/`false` in an intersection). */`` |
|    127689 |  7438 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7439 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7440 | `				if( bClassLike ){` |
|        53 |  7441 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7442 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7443 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7444 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7445 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7446 | `						bClassLike = 0;` |
|       ! 0 |  7447 | `					}` |
|        24 |  7448 | `				}` |
|        55 |  7449 | `				if( !bClassLike ){` |
|         - |  7450 | `					const char *zName; sxu32 nName;` |
|         3 |  7451 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7452 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7453 | `					}else{` |
|         3 |  7454 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7455 | `					}` |
|         4 |  7456 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7457 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7458 | `						(int)nName, zName);` |
|         3 |  7459 | `					return SXERR_SYNTAX;` |
|         - |  7460 | `				}` |
|        24 |  7461 | `			}` |
|    127687 |  7462 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7463 | `				if( nAtoms > 1 ){` |
|         3 |  7464 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7465 | `						"Void can only be used as a standalone type");` |
|         3 |  7466 | `					return SXERR_SYNTAX;` |
|         - |  7467 | `				}` |
|       175 |  7468 | `				if( !bAllowVoid ){` |
|       ! 0 |  7469 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7470 | `						"void cannot be used here");` |
|       ! 0 |  7471 | `					return SXERR_SYNTAX;` |
|         - |  7472 | `				}` |
|       175 |  7473 | `				if( bShortNullable ){` |
|       ! 0 |  7474 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7475 | `						"Void type cannot be nullable");` |
|       ! 0 |  7476 | `					return SXERR_SYNTAX;` |
|         - |  7477 | `				}` |
|        85 |  7478 | `			}` |
|    127685 |  7479 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7480 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7481 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7482 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7483 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        27 |  7484 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7485 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7486 | `					 * same as any other non-standalone use. */` |
|         6 |  7487 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7488 | `						"never can only be used as a standalone type");` |
|         6 |  7489 | `					return SXERR_SYNTAX;` |
|         - |  7490 | `				}` |
|        21 |  7491 | `				if( !bAllowVoid ){` |
|         - |  7492 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7493 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7494 | `						"never cannot be used as a parameter type");` |
|         3 |  7495 | `					return SXERR_SYNTAX;` |
|         - |  7496 | `				}` |
|         8 |  7497 | `			}` |
|    127679 |  7498 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7499 | `				bExplicitNull = 1;` |
|        19 |  7500 | `			}else{` |
|    127649 |  7501 | `				bHasNonNull = 1;` |
|         - |  7502 | `			}` |
|         - |  7503 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7504 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7505 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7506 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7507 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    127879 |  7508 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7509 | `				int bDup = 0;` |
|       207 |  7510 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7511 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7512 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7513 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7514 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7515 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7516 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7517 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7518 | `								aAtoms[j].sClass.zString,` |
|        34 |  7519 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7520 | `							bDup = 1;` |
|       ! 0 |  7521 | `						}` |
|        27 |  7522 | `					}else{` |
|         3 |  7523 | `						bDup = 1;` |
|         - |  7524 | `					}` |
|        23 |  7525 | `				}` |
|       195 |  7526 | `				if( bDup ){` |
|         - |  7527 | `					const char *zName;` |
|         - |  7528 | `					sxu32 nName;` |
|         3 |  7529 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7530 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7531 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7532 | `					}else{` |
|         3 |  7533 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7534 | `						nName = aAtoms[i].nCanon;` |
|         - |  7535 | `					}` |
|         4 |  7536 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7537 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7538 | `					return SXERR_SYNTAX;` |
|         - |  7539 | `				}` |
|        99 |  7540 | `			}` |
|     63841 |  7541 | `		}` |
|    127517 |  7542 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7543 | `			if( bShortNullable ){` |
|         - |  7544 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7545 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7546 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7547 | `				return SXERR_SYNTAX;` |
|         - |  7548 | `			}` |
|         - |  7549 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7550 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7551 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7552 | `			 * atom, so set it here. */` |
|         7 |  7553 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7554 | `		}` |
|         - |  7555 | `	}` |
|         - |  7556 | `	/* Compute nullability flag */` |
|    127517 |  7557 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       133 |  7558 | `		*piTypeFlags \|= iNullableFlag;` |
|        64 |  7559 | `	}` |
|         - |  7560 | `	/* Build canonical type text */` |
|    127517 |  7561 | `	if( pTypeText ){` |
|         - |  7562 | `		SyBlob sBlob;` |
|    127517 |  7563 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    191224 |  7564 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     63756 |  7565 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    127517 |  7566 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    190994 |  7567 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    127326 |  7568 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    127331 |  7569 | `			if( zDup ){` |
|    127331 |  7570 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     63663 |  7571 | `			}` |
|     63663 |  7572 | `		}` |
|    127517 |  7573 | `		SyBlobRelease(&sBlob);` |
|     63756 |  7574 | `	}` |
|         - |  7575 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7576 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7577 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7578 | `	{` |
|    127517 |  7579 | `		int nNonNull = 0;` |
|    127517 |  7580 | `		int iNonNullIdx = -1;` |
|         - |  7581 | `		int i;` |
|    255181 |  7582 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127669 |  7583 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127639 |  7584 | `				nNonNull++;` |
|    127639 |  7585 | `				iNonNullIdx = i;` |
|     63817 |  7586 | `			}` |
|     63837 |  7587 | `		}` |
|    127517 |  7588 | `		if( nNonNull <= 1 ){` |
|         - |  7589 | `			/* Fast path: store as single type. */` |
|    127411 |  7590 | `			if( iNonNullIdx >= 0 ){` |
|    127405 |  7591 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    127405 |  7592 | `				if( pA->nType == SXU32_HIGH ){` |
|     57632 |  7593 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19209 |  7594 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38423 |  7595 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38423 |  7596 | `					*pnType = SXU32_HIGH;` |
|     38423 |  7597 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    108196 |  7598 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7599 | `					*pnType = MEMOBJ_VOID;` |
|     88902 |  7600 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7601 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7602 | `				}else{` |
|     88801 |  7603 | `					*pnType = pA->nType;` |
|         - |  7604 | `				}` |
|     63700 |  7605 | `			}` |
|     63708 |  7606 | `		}else{` |
|         - |  7607 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7608 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7609 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7610 | `				ph7_type_alt sAlt;` |
|       249 |  7611 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7612 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7613 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7614 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7615 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7616 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7617 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7618 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7619 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7620 | `				}else{` |
|       145 |  7621 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7622 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7623 | `				}` |
|       239 |  7624 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7625 | `			}` |
|         - |  7626 | `		}` |
|         - |  7627 | `	}` |
|    127517 |  7628 | `	return SXRET_OK;` |
|     63769 |  7629 | `}` |
|         - |  7630 |  |
|         - |  7631 | `/*` |
|         - |  7632 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7633 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7634 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7635 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7636 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7637 | `` *          and union types `: T\|U`.`` |
|         - |  7638 | ` */` |
|   2877658 |  7639 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7640 | `{` |
|   2877663 |  7641 | `	sxi32 iFlags = 0;` |
|         - |  7642 | `	sxi32 rc;` |
|         - |  7643 | `	sxu32 nLine;` |
|   2877663 |  7644 | `	pFunc->nReturnType = 0;` |
|   2877663 |  7645 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2877663 |  7646 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7647 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7648 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7649 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7650 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7651 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2877663 |  7652 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2877663 |  7653 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2877663 |  7654 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2865507 |  7655 | `		return SXRET_OK;` |
|         - |  7656 | `	}` |
|     12161 |  7657 | `	pGen->pIn++; /* Skip ':' */` |
|     12161 |  7658 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7659 | `		return SXRET_OK;` |
|         - |  7660 | `	}` |
|     12161 |  7661 | `	nLine = pGen->pIn->nLine;` |
|     12161 |  7662 | `	rc = GenStateParseUnionTypeDecl(` |
|      6078 |  7663 | `		pGen,` |
|      6078 |  7664 | `		&pFunc->nReturnType,` |
|      6078 |  7665 | `		&pFunc->sReturnClass,` |
|      6078 |  7666 | `		&pFunc->aReturnUnion,` |
|         - |  7667 | `		&iFlags,` |
|      6078 |  7668 | `		&pFunc->sReturnTypeName,` |
|         - |  7669 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7670 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7671 | `		/* iUnionFlag */ 0,` |
|         - |  7672 | `		/* bAllowVoid */ 1,` |
|      6078 |  7673 | `		nLine);` |
|     12161 |  7674 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7675 | `		return SXERR_ABORT;` |
|         - |  7676 | `	}` |
|     12161 |  7677 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7678 | `		/* Error already reported */` |
|       ! 0 |  7679 | `		return SXERR_SYNTAX;` |
|         - |  7680 | `	}` |
|     12161 |  7681 | `	if( rc == SXERR_SYNTAX ){` |
|         9 |  7682 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 |  7683 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7684 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7685 | `				&pGen->pIn->sData);` |
|         6 |  7686 | `		}else{` |
|       ! 0 |  7687 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7688 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7689 | `		}` |
|         9 |  7690 | `		return SXERR_SYNTAX;` |
|         - |  7691 | `	}` |
|     12155 |  7692 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12155 |  7693 | `	return SXRET_OK;` |
|   1438834 |  7694 | `}` |
|         - |  7695 |  |
|    482912 |  7696 | `static sxi32 GenStateCompileFunc(` |
|         - |  7697 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7698 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7699 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7700 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7701 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7702 | `	)` |
|         5 |  7703 | `{` |
|         - |  7704 | `	ph7_vm_func *pFunc;` |
|         - |  7705 | `	SyToken *pEnd;` |
|         - |  7706 | `	sxu32 nLine;` |
|         - |  7707 | `	char *zName;` |
|         - |  7708 | `	sxi32 rc;` |
|         - |  7709 | `	/* Extract line number */` |
|    482917 |  7710 | `	nLine = pGen->pIn->nLine;` |
|         - |  7711 | `	/* Jump the left parenthesis '(' */` |
|    482917 |  7712 | `	pGen->pIn++;` |
|         - |  7713 | `	/* Delimit the function signature */` |
|    482917 |  7714 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    482917 |  7715 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7716 | `		/* Syntax error */` |
|         8 |  7717 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7718 | `		(void)pName;` |
|         8 |  7719 | `		if( rc == SXERR_ABORT ){` |
|         - |  7720 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7721 | `			return SXERR_ABORT;` |
|         - |  7722 | `		}` |
|         8 |  7723 | `		pGen->pIn = pGen->pEnd;` |
|         8 |  7724 | `		return SXRET_OK;` |
|         - |  7725 | `	}` |
|         - |  7726 | `	/* Create the function state */` |
|    482911 |  7727 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    482911 |  7728 | `	if( pFunc == 0 ){` |
|       ! 0 |  7729 | `		goto OutOfMem;` |
|         - |  7730 | `	}` |
|         - |  7731 | `	/* Build the function name, prepending namespace if active */` |
|    482918 |  7732 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7733 | `		SyBlob sFQN;` |
|         - |  7734 | `		sxu32 nLen;` |
|        16 |  7735 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7736 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7737 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7738 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7739 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7740 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7741 | `		SyBlobRelease(&sFQN);` |
|        16 |  7742 | `		if( zName == 0 ){` |
|       ! 0 |  7743 | `			goto OutOfMem;` |
|         - |  7744 | `		}` |
|        16 |  7745 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7746 | `	}else{` |
|    482897 |  7747 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    482897 |  7748 | `		if( zName == 0 ){` |
|       ! 0 |  7749 | `			goto OutOfMem;` |
|         - |  7750 | `		}` |
|    482897 |  7751 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7752 | `	}` |
|         - |  7753 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7754 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    482911 |  7755 | `	pFunc->nLine = nLine;` |
|    482911 |  7756 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    482911 |  7757 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7758 | `		return SXERR_ABORT;` |
|         - |  7759 | `	}` |
|    482911 |  7760 | `	if( pGen->pIn < pEnd ){` |
|         - |  7761 | `		/* Collect function arguments */` |
|    420911 |  7762 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    420911 |  7763 | `		if( rc == SXERR_ABORT ){` |
|         - |  7764 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7765 | `			return SXERR_ABORT;` |
|         - |  7766 | `		}` |
|    210453 |  7767 | `	}` |
|         - |  7768 | `	/* Point past ')' and parse optional return type ': type' */` |
|    482911 |  7769 | `	pGen->pIn = &pEnd[1];` |
|         - |  7770 | `	{` |
|    482911 |  7771 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    482911 |  7772 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7773 | `			return SXERR_ABORT;` |
|    482911 |  7774 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 |  7775 | `			return SXERR_SYNTAX;` |
|         - |  7776 | `		}` |
|         - |  7777 | `	}` |
|    482905 |  7778 | `	if( bHandleClosure ){` |
|         - |  7779 | `		ph7_vm_func_closure_env sEnv;` |
|       575 |  7780 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       570 |  7781 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       334 |  7782 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        93 |  7783 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7784 | `				/* Closure,record environment variable */` |
|        93 |  7785 | `				pGen->pIn++;` |
|        93 |  7786 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7787 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7788 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7789 | `						return SXERR_ABORT;` |
|         - |  7790 | `					}` |
|       ! 0 |  7791 | `				}` |
|        93 |  7792 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7793 | `				/* Compile until we hit the first closing parenthesis */` |
|       191 |  7794 | `				while( pGen->pIn < pGen->pEnd ){` |
|       191 |  7795 | `					int iFlagsLocal = 0;` |
|       191 |  7796 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        93 |  7797 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        93 |  7798 | `						break;` |
|         - |  7799 | `					}` |
|       103 |  7800 | `					nLineLocal = pGen->pIn->nLine;` |
|       103 |  7801 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7802 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7803 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7804 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7805 | `						pGen->pIn++;` |
|        27 |  7806 | `					}` |
|        98 |  7807 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       103 |  7808 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7809 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7810 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7811 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7812 | `								return SXERR_ABORT;` |
|         - |  7813 | `							}` |
|         - |  7814 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7815 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7816 | `								pGen->pIn++;` |
|       ! 0 |  7817 | `							}` |
|       ! 0 |  7818 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7819 | `								pGen->pIn++;` |
|       ! 0 |  7820 | `							}` |
|       ! 0 |  7821 | `							break;` |
|         - |  7822 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7823 | `					}else{` |
|         - |  7824 | `						SyString *pNameLocal;` |
|         - |  7825 | `						char *zDup;` |
|         - |  7826 | `						/* Duplicate variable name */` |
|       103 |  7827 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       103 |  7828 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       103 |  7829 | `						if( zDup ){` |
|         - |  7830 | `							/* Zero the structure */` |
|       103 |  7831 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       103 |  7832 | `							sEnv.iFlags = iFlagsLocal;` |
|       103 |  7833 | `							sEnv.nIdx = SXU32_HIGH;` |
|       103 |  7834 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       103 |  7835 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       118 |  7836 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7837 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7838 | `									got_this = 1;` |
|       ! 0 |  7839 | `							}` |
|         - |  7840 | `							/* Save imported variable */` |
|       103 |  7841 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        54 |  7842 | `						}else{` |
|       ! 0 |  7843 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7844 | `							 return SXERR_ABORT;` |
|         - |  7845 | `						}` |
|         - |  7846 | `					}` |
|       103 |  7847 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       115 |  7848 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7849 | `						/* Ignore trailing commas */` |
|        13 |  7850 | `						pGen->pIn++;` |
|         1 |  7851 | `					}` |
|         5 |  7852 | `				}` |
|         - |  7853 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7854 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7855 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7856 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7857 | `				 * legacy pre-use position. */` |
|        93 |  7858 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7859 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7860 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7861 | `						return SXERR_ABORT;` |
|         7 |  7862 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7863 | `						return SXERR_SYNTAX;` |
|         - |  7864 | `					}` |
|         3 |  7865 | `				}` |
|        44 |  7866 | `		}` |
|       575 |  7867 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7868 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7869 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7870 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7871 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7872 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7873 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7874 | `			 * closure never binds $this (php). */` |
|       565 |  7875 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       565 |  7876 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       565 |  7877 | `			sEnv.nIdx = SXU32_HIGH;` |
|       565 |  7878 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       565 |  7879 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       565 |  7880 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       280 |  7881 | `		}` |
|       575 |  7882 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7883 | `			/* Mark as closure */` |
|       567 |  7884 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       281 |  7885 | `		}` |
|       285 |  7886 | `	}` |
|         - |  7887 | `	/* Compile the body */` |
|    482905 |  7888 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    482905 |  7889 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7890 | `		return SXERR_ABORT;` |
|         - |  7891 | `	}` |
|         - |  7892 | `	/* The cursor sits just past the body's closing brace */` |
|    482905 |  7893 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    482905 |  7894 | `	if( ppFunc ){` |
|    482905 |  7895 | `		*ppFunc = pFunc;` |
|    241450 |  7896 | `	}` |
|    482905 |  7897 | `	rc = SXRET_OK;` |
|    482905 |  7898 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7899 | `		/* Finally register the function */` |
|    482343 |  7900 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    241169 |  7901 | `	}` |
|    482905 |  7902 | `	if( rc == SXRET_OK ){` |
|    482905 |  7903 | `		return SXRET_OK;` |
|         - |  7904 | `	}` |
|         - |  7905 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7906 | `OutOfMem:` |
|         - |  7907 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7908 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7909 | `	 */` |
|       ! 0 |  7910 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7911 | `	return SXERR_ABORT;` |
|    241461 |  7912 | `}` |
|         - |  7913 | `/*` |
|         - |  7914 | ` * Compile a standard PHP function.` |
|         - |  7915 | ` *  Refer to the block-comment above for more information.` |
|         - |  7916 | ` */` |
|    482350 |  7917 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7918 | `{` |
|         - |  7919 | `	SyString *pName;` |
|         - |  7920 | `	sxi32 iFlags;` |
|         - |  7921 | `	sxu32 nKwLine;` |
|         - |  7922 | `	sxu32 nLine;` |
|         - |  7923 | `	sxi32 rc;` |
|         - |  7924 |  |
|    482355 |  7925 | `	nLine = pGen->pIn->nLine;` |
|    482355 |  7926 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    482355 |  7927 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    482355 |  7928 | `	iFlags = 0;` |
|    482355 |  7929 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7930 | `		/* Return by reference,remember that */` |
|        12 |  7931 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7932 | `		/* Jump the '&' token */` |
|        12 |  7933 | `		pGen->pIn++;` |
|         5 |  7934 | `	}` |
|    482355 |  7935 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7936 | `		/* Invalid function name */` |
|         8 |  7937 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         8 |  7938 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7939 | `			return SXERR_ABORT;` |
|         - |  7940 | `		}` |
|         - |  7941 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  7942 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  7943 | `			pGen->pIn++;` |
|         2 |  7944 | `		}` |
|         8 |  7945 | `		return SXRET_OK;` |
|         - |  7946 | `	}` |
|    482349 |  7947 | `	pName = &pGen->pIn->sData;` |
|    482349 |  7948 | `	nLine = pGen->pIn->nLine;` |
|         - |  7949 | `	/* Jump the function name */` |
|    482349 |  7950 | `	pGen->pIn++;` |
|    482349 |  7951 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7952 | `		/* Syntax error */` |
|         3 |  7953 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  7954 | `		if( rc == SXERR_ABORT ){` |
|         - |  7955 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7956 | `			return SXERR_ABORT;` |
|         - |  7957 | `		}` |
|         - |  7958 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  7959 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  7960 | `			pGen->pIn++;` |
|       ! 0 |  7961 | `		}` |
|         3 |  7962 | `		return SXRET_OK;` |
|         - |  7963 | `	}` |
|         - |  7964 | `	/* Compile function body */` |
|         - |  7965 | `	{` |
|    482347 |  7966 | `		ph7_vm_func *pFuncState = 0;` |
|    482347 |  7967 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    482347 |  7968 | `		if( pFuncState ){` |
|         - |  7969 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    482335 |  7970 | `			pFuncState->nLine = nKwLine;` |
|    241165 |  7971 | `		}` |
|         - |  7972 | `	}` |
|    482347 |  7973 | `	return rc;` |
|    241180 |  7974 | `}` |
|         - |  7975 | `/*` |
|         - |  7976 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  7977 | ` * According to the PHP language reference manual` |
|         - |  7978 | ` *  Visibility:` |
|         - |  7979 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  7980 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  7981 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  7982 | ` *  Members declared protected can be accessed only within the class` |
|         - |  7983 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  7984 | ` *  may only be accessed by the class that defines the member.` |
|         - |  7985 | ` */` |
|   3143402 |  7986 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  7987 | `{` |
|   3143407 |  7988 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    255873 |  7989 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2887539 |  7990 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    190887 |  7991 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  7992 | `	}` |
|         - |  7993 | `	/* Assume public by default */` |
|   2696657 |  7994 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1571706 |  7995 | `}` |
|         - |  7996 | `/*` |
|         - |  7997 | ` * Compile a class constant.` |
|         - |  7998 | ` * According to the PHP language reference manual` |
|         - |  7999 | ` *  Class Constants` |
|         - |  8000 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  8001 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  8002 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  8003 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  8004 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  8005 | ` *   It's also possible for interfaces to have constants.` |
|         - |  8006 | ` * Symisc eXtension.` |
|         - |  8007 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  8008 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8009 | ` *  Example:` |
|         - |  8010 | ` *   class Test{` |
|         - |  8011 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8012 | ` *   };` |
|         - |  8013 | ` *   var_dump(TEST::MyConst);` |
|         - |  8014 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8015 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8016 | ` */` |
|         - |  8017 | `/*` |
|         - |  8018 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  8019 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  8020 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  8021 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  8022 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  8023 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  8024 | ` */` |
|    290196 |  8025 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  8026 | `{` |
|         - |  8027 | `	SyToken *p0, *p1;` |
|    290201 |  8028 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8029 | `		return 0;` |
|         - |  8030 | `	}` |
|    290201 |  8031 | `	p0 = pGen->pIn;` |
|         - |  8032 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    290201 |  8033 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  8034 | `		return 1;` |
|         - |  8035 | `	}` |
|    290201 |  8036 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  8037 | `		return 1;` |
|         - |  8038 | `	}` |
|         - |  8039 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  8040 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  8041 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    290197 |  8042 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    290197 |  8043 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    290197 |  8044 | `		if( p1 ){` |
|    290197 |  8045 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  8046 | `				return 1;` |
|         - |  8047 | `			}` |
|    290167 |  8048 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  8049 | `				return 1;` |
|         - |  8050 | `			}` |
|    145079 |  8051 | `		}` |
|    145079 |  8052 | `	}` |
|    290163 |  8053 | `	return 0;` |
|    145103 |  8054 | `}` |
|         - |  8055 | `/*` |
|         - |  8056 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  8057 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  8058 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  8059 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  8060 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  8061 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  8062 | ` * Peek only; never consumes tokens.` |
|         - |  8063 | ` */` |
|        24 |  8064 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  8065 | `{` |
|        28 |  8066 | `	SyToken *p = pGen->pIn;` |
|        39 |  8067 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  8068 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  8069 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  8070 | `	}` |
|        28 |  8071 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  8072 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  8073 | `	}` |
|         6 |  8074 | `	p++;` |
|         - |  8075 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  8076 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  8077 | `}` |
|         - |  8078 | `/*` |
|         - |  8079 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  8080 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  8081 | `` * `$o->new`), not a `new` expression.`` |
|         - |  8082 | ` */` |
|       110 |  8083 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  8084 | `{` |
|         - |  8085 | `	sxi32 iOp;` |
|       114 |  8086 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  8087 | `		return 0;` |
|         - |  8088 | `	}` |
|       104 |  8089 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  8090 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  8091 | `}` |
|         - |  8092 | `/*` |
|         - |  8093 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  8094 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  8095 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  8096 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  8097 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  8098 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  8099 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  8100 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  8101 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  8102 | ` *` |
|         - |  8103 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  8104 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  8105 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  8106 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  8107 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  8108 | ` */` |
|    626710 |  8109 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  8110 | `{` |
|    626715 |  8111 | `	SyToken *p = pGen->pIn;` |
|    626715 |  8112 | `	int iDepth = 0;` |
|   1655787 |  8113 | `	while( p < pGen->pEnd ){` |
|   1655787 |  8114 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    626663 |  8115 | `			break; /* end of this initializer */` |
|         - |  8116 | `		}` |
|   1029124 |  8117 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    518399 |  8118 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7664 |  8119 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  8120 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  8121 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  8122 | `			 * expression. */` |
|         3 |  8123 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  8124 | `			p++;` |
|         3 |  8125 | `			if( bArrow ){` |
|         - |  8126 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  8127 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  8128 | `				int iBase = iDepth;` |
|        17 |  8129 | `				while( p < pGen->pEnd ){` |
|        17 |  8130 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  8131 | `						iDepth++;` |
|        15 |  8132 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  8133 | `						if( iDepth <= iBase ){` |
|       ! 0 |  8134 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  8135 | `						}` |
|         5 |  8136 | `						iDepth--;` |
|        11 |  8137 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  8138 | `						break;` |
|         - |  8139 | `					}` |
|        15 |  8140 | `					p++;` |
|         1 |  8141 | `				}` |
|         2 |  8142 | `			}else{` |
|         - |  8143 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  8144 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  8145 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  8146 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  8147 | `				int iLocal = 0;` |
|       ! 0 |  8148 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  8149 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  8150 | `						break; /* body brace */` |
|         - |  8151 | `					}` |
|       ! 0 |  8152 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  8153 | `						iLocal++;` |
|       ! 0 |  8154 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  8155 | `						if( iLocal > 0 ){` |
|       ! 0 |  8156 | `							iLocal--;` |
|       ! 0 |  8157 | `						}` |
|       ! 0 |  8158 | `					}` |
|       ! 0 |  8159 | `					p++;` |
|       ! 0 |  8160 | `				}` |
|       ! 0 |  8161 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  8162 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  8163 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  8164 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  8165 | `							iBrace++;` |
|       ! 0 |  8166 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  8167 | `							iBrace--;` |
|       ! 0 |  8168 | `							if( iBrace == 0 ){` |
|       ! 0 |  8169 | `								p++;` |
|       ! 0 |  8170 | `								break;` |
|         - |  8171 | `							}` |
|       ! 0 |  8172 | `						}` |
|       ! 0 |  8173 | `						p++;` |
|       ! 0 |  8174 | `					}` |
|       ! 0 |  8175 | `				}` |
|         - |  8176 | `			}` |
|         3 |  8177 | `			continue;` |
|         - |  8178 | `		}` |
|   1029127 |  8179 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  8180 | `			if( iDepth == 0 ){` |
|         - |  8181 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  8182 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  8183 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  8184 | `				 * is legal — don't scan into it. */` |
|        45 |  8185 | `				break;` |
|         - |  8186 | `			}` |
|       ! 0 |  8187 | `			iDepth++;` |
|   1029083 |  8188 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42073 |  8189 | `			iDepth++;` |
|   1008049 |  8190 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     42071 |  8191 | `			if( iDepth > 0 ){` |
|     42071 |  8192 | `				iDepth--;` |
|     21033 |  8193 | `			}` |
|    965982 |  8194 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    346435 |  8195 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8196 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8197 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8198 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  8199 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  8200 | `				return 1;` |
|         - |  8201 | `			}` |
|       ! 0 |  8202 | `		}` |
|   1029075 |  8203 | `		p++;` |
|         5 |  8204 | `	}` |
|    626707 |  8205 | `	return 0;` |
|    313360 |  8206 | `}` |
|         - |  8207 | `/*` |
|         - |  8208 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8209 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8210 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8211 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8212 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8213 | ` * share the same backing.` |
|         - |  8214 | ` */` |
|       362 |  8215 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8216 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8217 | `{` |
|       367 |  8218 | `	pAttr->nType = nType;` |
|       367 |  8219 | `	pAttr->sClass = *pClass;` |
|       367 |  8220 | `	pAttr->sTypeName = *pTypeName;` |
|       367 |  8221 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8222 | `		sxu32 i;` |
|        73 |  8223 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8224 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8225 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8226 | `		}` |
|        11 |  8227 | `	}` |
|       367 |  8228 | `}` |
|    290196 |  8229 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8230 | `{` |
|    290201 |  8231 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8232 | `	SySet *pInstrContainer;` |
|         - |  8233 | `	ph7_class_attr *pCons;` |
|         - |  8234 | `	SyString *pName;` |
|         - |  8235 | `	sxi32 rc;` |
|    290201 |  8236 | `	sxu32 nType = 0;` |
|         - |  8237 | `	SyString sTypeClass;` |
|         - |  8238 | `	SyString sTypeText;` |
|         - |  8239 | `	SySet aUnionAlts;` |
|    290201 |  8240 | `	sxi32 iTypeFlags = 0;` |
|    290201 |  8241 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    290201 |  8242 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    290201 |  8243 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8244 | `	/* Extract visibility level */` |
|    290201 |  8245 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8246 | `	/* Mark as constant */` |
|    290201 |  8247 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    290201 |  8248 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8249 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8250 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    290220 |  8251 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8252 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8253 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8254 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8255 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8256 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8257 | `		 * and success paths release. */` |
|        42 |  8258 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8259 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8260 | `			goto Synchronize;` |
|        42 |  8261 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8262 | `			return SXERR_ABORT;` |
|        42 |  8263 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8264 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8265 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8266 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8267 | `				return SXERR_ABORT;` |
|         - |  8268 | `			}` |
|       ! 0 |  8269 | `			goto Synchronize;` |
|         - |  8270 | `		}` |
|        42 |  8271 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8272 | `	}` |
|    145098 |  8273 | `loop:` |
|    290203 |  8274 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8275 | `		/* Invalid constant name */` |
|       ! 0 |  8276 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8277 | `		if( rc == SXERR_ABORT ){` |
|         - |  8278 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8279 | `			return SXERR_ABORT;` |
|         - |  8280 | `		}` |
|       ! 0 |  8281 | `		goto Synchronize;` |
|         - |  8282 | `	}` |
|         - |  8283 | `	/* Peek constant name */` |
|    290203 |  8284 | `	pName = &pGen->pIn->sData;` |
|         - |  8285 | `	/* Make sure the constant name isn't reserved */` |
|    290203 |  8286 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8287 | `		/* Reserved constant name */` |
|       ! 0 |  8288 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8289 | `		if( rc == SXERR_ABORT ){` |
|         - |  8290 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8291 | `			return SXERR_ABORT;` |
|         - |  8292 | `		}` |
|       ! 0 |  8293 | `		goto Synchronize;` |
|         - |  8294 | `	}` |
|         - |  8295 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    290203 |  8296 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8297 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8298 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8299 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8300 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8301 | `			return SXERR_ABORT;` |
|        42 |  8302 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8303 | `			goto Synchronize;` |
|         - |  8304 | `		}` |
|        18 |  8305 | `	}` |
|         - |  8306 | `	/* Advance the stream cursor */` |
|    290201 |  8307 | `	pGen->pIn++;` |
|    290201 |  8308 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8309 | `		/* Invalid declaration */` |
|       ! 0 |  8310 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8311 | `		if( rc == SXERR_ABORT ){` |
|         - |  8312 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8313 | `			return SXERR_ABORT;` |
|         - |  8314 | `		}` |
|       ! 0 |  8315 | `		goto Synchronize;` |
|         - |  8316 | `	}` |
|    290201 |  8317 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8318 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8319 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8320 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8321 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    290196 |  8322 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8323 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8324 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8325 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8326 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8327 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8328 | `			return SXERR_ABORT;` |
|         - |  8329 | `		}` |
|         6 |  8330 | `		goto Synchronize;` |
|         - |  8331 | `	}` |
|         - |  8332 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8333 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8334 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    290197 |  8335 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8336 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8337 | `			"New expressions are not supported in this context");` |
|         5 |  8338 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8339 | `			return SXERR_ABORT;` |
|         - |  8340 | `		}` |
|         5 |  8341 | `		goto Synchronize;` |
|         - |  8342 | `	}` |
|         - |  8343 | `	/* Allocate a new class attribute */` |
|    290193 |  8344 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    290193 |  8345 | `	if( pCons ){` |
|    290193 |  8346 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    290193 |  8347 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8348 | `			return SXERR_ABORT;` |
|         - |  8349 | `		}` |
|    145094 |  8350 | `	}` |
|    290193 |  8351 | `	if( pCons == 0 ){` |
|       ! 0 |  8352 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8353 | `		return SXERR_ABORT;` |
|         - |  8354 | `	}` |
|    290193 |  8355 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8356 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8357 | `	}` |
|         - |  8358 | `	/* Swap bytecode container */` |
|    290193 |  8359 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    290193 |  8360 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8361 | `	/* Compile constant value.` |
|         - |  8362 | `	 */` |
|    290193 |  8363 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    290193 |  8364 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8365 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8366 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8367 | `			return SXERR_ABORT;` |
|         - |  8368 | `		}` |
|         1 |  8369 | `	}` |
|         - |  8370 | `	/* Emit the done instruction */` |
|    290193 |  8371 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    290193 |  8372 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    290193 |  8373 | `	if( rc == SXERR_ABORT ){` |
|         - |  8374 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8375 | `		return SXERR_ABORT;` |
|         - |  8376 | `	}` |
|         - |  8377 | `	/* All done,install the constant */` |
|    290193 |  8378 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    290193 |  8379 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8380 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8381 | `		return SXERR_ABORT;` |
|         - |  8382 | `	}` |
|    290193 |  8383 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8384 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8385 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8386 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8387 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8388 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8389 | `				pTok--;` |
|       ! 0 |  8390 | `			}` |
|       ! 0 |  8391 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8392 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8393 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8394 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8395 | `				return SXERR_ABORT;` |
|         - |  8396 | `			}` |
|       ! 0 |  8397 | `		}else{` |
|         3 |  8398 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8399 | `				goto loop;` |
|         - |  8400 | `			}` |
|         - |  8401 | `		}` |
|       ! 0 |  8402 | `	}` |
|    290191 |  8403 | `	SySetRelease(&aUnionAlts);` |
|    290191 |  8404 | `	return SXRET_OK;` |
|         5 |  8405 | `Synchronize:` |
|        13 |  8406 | `	SySetRelease(&aUnionAlts);` |
|         - |  8407 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8408 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8409 | `		pGen->pIn++;` |
|         3 |  8410 | `	}` |
|        13 |  8411 | `	return SXERR_CORRUPT;` |
|    145103 |  8412 | `}` |
|         - |  8413 | `/*` |
|         - |  8414 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8415 | ` * According to the PHP language reference manual` |
|         - |  8416 | ` *  Properties` |
|         - |  8417 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8418 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8419 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8420 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8421 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8422 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8423 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8424 | ` * Symisc eXtension.` |
|         - |  8425 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8426 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8427 | ` *  Example:` |
|         - |  8428 | ` *   class Test{` |
|         - |  8429 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8430 | ` *   };` |
|         - |  8431 | ` *   var_dump(TEST::myVar);` |
|         - |  8432 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8433 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8434 | ` */` |
|         - |  8435 | `/*` |
|         - |  8436 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8437 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8438 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8439 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8440 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8441 | ` */` |
|   2348544 |  8442 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8443 | `{` |
|   2348549 |  8444 | `	SyToken *p = pStart;` |
|   2348549 |  8445 | `	int bFirst = 1;` |
|   2348549 |  8446 | `	if( p >= pEnd ) return 0;` |
|         - |  8447 | ``	/* Optional nullable `?` shorthand. */`` |
|   2348549 |  8448 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        39 |  8449 | `		p++;` |
|        39 |  8450 | `		if( p >= pEnd ) return 0;` |
|        18 |  8451 | `	}` |
|         - |  8452 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8453 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8454 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8455 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1174272 |  8456 | `	for(;;){` |
|   2348569 |  8457 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8458 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8459 | `			p++;` |
|         9 |  8460 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8461 | `			if( p >= pEnd ) return 0;` |
|         3 |  8462 | `			p++; /* skip ')' */` |
|         2 |  8463 | `		}else{` |
|         - |  8464 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8465 | ``			 * then any `&`-joined intersection members. */`` |
|   2348567 |  8466 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2348567 |  8467 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8468 | `				return 0;` |
|         - |  8469 | `			}` |
|         - |  8470 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8471 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8472 | `			 * may still appear at the initial dispatch site). */` |
|   2348567 |  8473 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2348519 |  8474 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2348514 |  8475 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    103466 |  8476 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2348225 |  8477 | `					return 0;` |
|         - |  8478 | `				}` |
|       147 |  8479 | `			}` |
|       347 |  8480 | `			p++;` |
|       349 |  8481 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8482 | `				p += 2;` |
|         1 |  8483 | `			}` |
|       516 |  8484 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       350 |  8485 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8486 | `				p++; /* skip '&' */` |
|         3 |  8487 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8488 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8489 | `				p++;` |
|         3 |  8490 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8491 | `					p += 2;` |
|       ! 0 |  8492 | `				}` |
|         1 |  8493 | `			}` |
|         - |  8494 | `		}` |
|       349 |  8495 | `		bFirst = 0;` |
|       344 |  8496 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8497 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8498 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8499 | `			continue;` |
|         - |  8500 | `		}` |
|       329 |  8501 | `		break;` |
|       ! 0 |  8502 | `	}` |
|       329 |  8503 | `	if( p >= pEnd ) return 0;` |
|       329 |  8504 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1174277 |  8505 | `}` |
|         - |  8506 |  |
|         - |  8507 | `/*` |
|         - |  8508 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8509 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8510 | ` * if not). Recognized forms:` |
|         - |  8511 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8512 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8513 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8514 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8515 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8516 | ` * on unrecoverable error.` |
|         - |  8517 | ` *` |
|         - |  8518 | ` * When a type is parsed:` |
|         - |  8519 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8520 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8521 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8522 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8523 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8524 | ` */` |
|       334 |  8525 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8526 | `	ph7_gen_state *pGen,` |
|         - |  8527 | `	sxu32 *pnType,` |
|         - |  8528 | `	SyString *pClass,` |
|         - |  8529 | `	sxi32 *piTypeFlags,` |
|         - |  8530 | `	SyString *pTypeText,` |
|         - |  8531 | `	SySet *pAlts` |
|         5 |  8532 | `){` |
|       339 |  8533 | `	sxi32 iFlags = 0;` |
|         - |  8534 | `	sxi32 rc;` |
|       339 |  8535 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8536 | `		return SXRET_OK;` |
|         - |  8537 | `	}` |
|         - |  8538 | `	/* If the first token is '$', there's no type */` |
|       339 |  8539 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8540 | `		return SXRET_OK;` |
|         - |  8541 | `	}` |
|       339 |  8542 | `	rc = GenStateParseUnionTypeDecl(` |
|       167 |  8543 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8544 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8545 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8546 | `		/* bAllowVoid */ 0,` |
|       334 |  8547 | `		pGen->pIn->nLine);` |
|       339 |  8548 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8549 | `		return rc;` |
|         - |  8550 | `	}` |
|         - |  8551 | `	/* Verify next token is '$' (start of property name) */` |
|       339 |  8552 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8553 | `		return SXERR_SYNTAX;` |
|         - |  8554 | `	}` |
|       339 |  8555 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       339 |  8556 | `	return SXRET_OK;` |
|       172 |  8557 | `}` |
|         - |  8558 |  |
|         - |  8559 | `/*` |
|         - |  8560 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8561 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8562 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8563 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8564 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8565 | ` * by the type parser itself before reaching here.` |
|         - |  8566 | ` *` |
|         - |  8567 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8568 | ` * use in the error message.` |
|         - |  8569 | ` */` |
|       510 |  8570 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8571 | `	sxu32 nType,` |
|         - |  8572 | `	const SyString *pClass,` |
|         - |  8573 | `	const char **pzName,` |
|         - |  8574 | `	sxu32 *pnName)` |
|         5 |  8575 | `{` |
|         - |  8576 | `	const char *z;` |
|         - |  8577 | `	sxu32 n;` |
|       515 |  8578 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       459 |  8579 | `		return 0;` |
|         - |  8580 | `	}` |
|        60 |  8581 | `	z = pClass->zString;` |
|        60 |  8582 | `	n = pClass->nByte;` |
|        60 |  8583 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8584 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8585 | `	}` |
|         - |  8586 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8587 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8588 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        54 |  8589 | `	return 0;` |
|       260 |  8590 | `}` |
|         - |  8591 |  |
|         - |  8592 | `/*` |
|         - |  8593 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8594 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8595 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8596 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8597 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8598 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8599 | ` *` |
|         - |  8600 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8601 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8602 | ` */` |
|       448 |  8603 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8604 | `	ph7_gen_state *pGen,` |
|         - |  8605 | `	ph7_class *pClass,` |
|         - |  8606 | `	const SyString *pMemberName,` |
|         - |  8607 | `	sxu32 nType,` |
|         - |  8608 | `	const SyString *pTypeClass,` |
|         - |  8609 | `	const SyString *pTypeText,` |
|         - |  8610 | `	SySet *pUnionAlts,` |
|         - |  8611 | `	const char *zErrFmt,` |
|         - |  8612 | `	sxu32 nLine)` |
|         5 |  8613 | `{` |
|       453 |  8614 | `	const char *zBad = 0;` |
|       453 |  8615 | `	sxu32 nBad = 0;` |
|         - |  8616 | `	SyString sFallback;` |
|         - |  8617 | `	const SyString *pBad;` |
|         - |  8618 | `	sxi32 rc;` |
|       453 |  8619 | `	int bDisallowed = 0;` |
|       453 |  8620 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8621 | `		bDisallowed = 1;` |
|       451 |  8622 | `	}else if( pUnionAlts ){` |
|         - |  8623 | `		sxu32 i;` |
|        95 |  8624 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8625 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8626 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8627 | `				bDisallowed = 1;` |
|         3 |  8628 | `				break;` |
|         - |  8629 | `			}` |
|        35 |  8630 | `		}` |
|        15 |  8631 | `	}` |
|       453 |  8632 | `	if( !bDisallowed ){` |
|       447 |  8633 | `		return SXRET_OK;` |
|         - |  8634 | `	}` |
|         - |  8635 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8636 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8637 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8638 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8639 | `		pBad = pTypeText;` |
|         5 |  8640 | `	}else{` |
|       ! 0 |  8641 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8642 | `		pBad = &sFallback;` |
|         - |  8643 | `	}` |
|        11 |  8644 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8645 | `		zErrFmt,` |
|         3 |  8646 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8647 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8648 | `		return SXERR_ABORT;` |
|         - |  8649 | `	}` |
|         8 |  8650 | `	return SXERR_SYNTAX;` |
|       229 |  8651 | `}` |
|         - |  8652 | `/*` |
|         - |  8653 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8654 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8655 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8656 | ` * than promoted to a lexer keyword.` |
|         - |  8657 | ` */` |
|  20238412 |  8658 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8659 | `{` |
|  20456669 |  8660 | `	return (pTok->nType & PH7_TK_ID)` |
|  10337458 |  8661 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  20456664 |  8662 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8663 | `}` |
|         - |  8664 | `/*` |
|         - |  8665 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8666 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8667 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8668 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8669 | ` */` |
|   7273076 |  8670 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8671 | `{` |
|   7273081 |  8672 | `	*pnTok = 0;` |
|   7273076 |  8673 | `	if( &pTok[3] < pEnd` |
|   6819994 |  8674 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5614290 |  8675 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2430842 |  8676 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8677 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8678 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8679 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8680 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8681 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8682 | `			*pnTok = 4;` |
|        17 |  8683 | `			return nKw;` |
|         - |  8684 | `		}` |
|       ! 0 |  8685 | `	}` |
|   7273065 |  8686 | `	return 0;` |
|   3636543 |  8687 | `}` |
|         - |  8688 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8689 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8690 | `{` |
|        17 |  8691 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8692 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8693 | `	}` |
|         5 |  8694 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8695 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8696 | `	}` |
|         3 |  8697 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8698 | `}` |
|    458924 |  8699 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8700 | `{` |
|    458929 |  8701 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8702 | `	ph7_class_attr *pAttr;` |
|         - |  8703 | `	SyString *pName;` |
|         - |  8704 | `	sxi32 rc;` |
|    458929 |  8705 | `	sxu32 nType = 0;` |
|         - |  8706 | `	SyString sTypeClass;` |
|         - |  8707 | `	SyString sTypeText;` |
|         - |  8708 | `	SySet aUnionAlts;` |
|    458929 |  8709 | `	sxi32 iTypeFlags = 0;` |
|    458929 |  8710 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    458929 |  8711 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    458929 |  8712 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8713 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8714 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8715 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    458929 |  8716 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8717 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8718 | `	}` |
|         - |  8719 | `	/* Extract visibility level */` |
|    458929 |  8720 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8721 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    459096 |  8722 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       339 |  8723 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       339 |  8724 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8725 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8726 | `			goto Synchronize;` |
|       339 |  8727 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8728 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8729 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8730 | `				&pGen->pIn->sData);` |
|       ! 0 |  8731 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8732 | `				return SXERR_ABORT;` |
|         - |  8733 | `			}` |
|       ! 0 |  8734 | `			goto Synchronize;` |
|       339 |  8735 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8736 | `			return SXERR_ABORT;` |
|         - |  8737 | `		}` |
|       167 |  8738 | `	}` |
|       ! 0 |  8739 | `loop:` |
|    458933 |  8740 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8741 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8742 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8743 | `			return SXERR_ABORT;` |
|         - |  8744 | `		}` |
|       ! 0 |  8745 | `		goto Synchronize;` |
|         - |  8746 | `	}` |
|    458933 |  8747 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    458933 |  8748 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8749 | `		/* Invalid attribute name */` |
|       ! 0 |  8750 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8751 | `		if( rc == SXERR_ABORT ){` |
|         - |  8752 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8753 | `			return SXERR_ABORT;` |
|         - |  8754 | `		}` |
|       ! 0 |  8755 | `		goto Synchronize;` |
|         - |  8756 | `	}` |
|         - |  8757 | `	/* Peek attribute name */` |
|    458933 |  8758 | `	pName = &pGen->pIn->sData;` |
|         - |  8759 | `	/* Advance the stream cursor */` |
|    458933 |  8760 | `	pGen->pIn++;` |
|    458933 |  8761 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8762 | `		/* Invalid declaration */` |
|         3 |  8763 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8764 | `		if( rc == SXERR_ABORT ){` |
|         - |  8765 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8766 | `			return SXERR_ABORT;` |
|         - |  8767 | `		}` |
|         3 |  8768 | `		goto Synchronize;` |
|         - |  8769 | `	}` |
|         - |  8770 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8771 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    458931 |  8772 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8773 | `		const char *zAvErr = 0;` |
|        19 |  8774 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8775 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8776 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8777 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8778 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8779 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8780 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8781 | `		}` |
|        13 |  8782 | `		if( zAvErr ){` |
|       ! 0 |  8783 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8784 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8785 | `				return SXERR_ABORT;` |
|         - |  8786 | `			}` |
|       ! 0 |  8787 | `			goto Synchronize;` |
|         - |  8788 | `		}` |
|         6 |  8789 | `	}` |
|         - |  8790 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8791 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    458931 |  8792 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8793 | `		const char *zRoErr = 0;` |
|        43 |  8794 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8795 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8796 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8797 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8798 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8799 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8800 | `		}` |
|        43 |  8801 | `		if( zRoErr ){` |
|        13 |  8802 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8803 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8804 | `				return SXERR_ABORT;` |
|         - |  8805 | `			}` |
|        13 |  8806 | `			goto Synchronize;` |
|         - |  8807 | `		}` |
|        14 |  8808 | `	}` |
|         - |  8809 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8810 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8811 | `	 * by the type parser. */` |
|    458921 |  8812 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       503 |  8813 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8814 | `			&sTypeText,` |
|       332 |  8815 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       166 |  8816 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       337 |  8817 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8818 | `			return SXERR_ABORT;` |
|       337 |  8819 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8820 | `			goto Synchronize;` |
|         - |  8821 | `		}` |
|       166 |  8822 | `	}` |
|         - |  8823 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    458921 |  8824 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8825 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8826 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8827 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8828 | `			return SXERR_ABORT;` |
|         - |  8829 | `		}` |
|         3 |  8830 | `		goto Synchronize;` |
|         - |  8831 | `	}` |
|         - |  8832 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8833 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8834 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8835 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8836 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8837 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    458919 |  8838 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8839 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8840 | `			"New expressions are not supported in this context");` |
|         6 |  8841 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8842 | `			return SXERR_ABORT;` |
|         - |  8843 | `		}` |
|         6 |  8844 | `		goto Synchronize;` |
|         - |  8845 | `	}` |
|         - |  8846 | `	/* Allocate a new class attribute */` |
|    458915 |  8847 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    458915 |  8848 | `	if( pAttr ){` |
|    458915 |  8849 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    458915 |  8850 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8851 | `			return SXERR_ABORT;` |
|         - |  8852 | `		}` |
|    229455 |  8853 | `	}` |
|    458915 |  8854 | `	if( pAttr == 0 ){` |
|       ! 0 |  8855 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8856 | `		return SXERR_ABORT;` |
|         - |  8857 | `	}` |
|    458915 |  8858 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       335 |  8859 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       165 |  8860 | `	}` |
|    458915 |  8861 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8862 | `		SySet *pInstrContainer;` |
|    336519 |  8863 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    336519 |  8864 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8865 | `		{` |
|         - |  8866 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8867 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8868 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8869 | `			 * compiler would otherwise run into the hook tokens. */` |
|    336519 |  8870 | `			SyToken *pScan = pGen->pIn;` |
|    336519 |  8871 | `			sxi32 iNest = 0;` |
|    734803 |  8872 | `			while( pScan < pGen->pEnd ){` |
|    734803 |  8873 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42067 |  8874 | `					iNest++;` |
|    713772 |  8875 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     42067 |  8876 | `					iNest--;` |
|    671710 |  8877 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    336519 |  8878 | `					break;` |
|         - |  8879 | `				}` |
|    398289 |  8880 | `				pScan++;` |
|         5 |  8881 | `			}` |
|    336519 |  8882 | `			pGen->pEnd = pScan;` |
|         - |  8883 | `		}` |
|         - |  8884 | `		/* Swap bytecode container */` |
|    336519 |  8885 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    336519 |  8886 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8887 | `		/* Compile attribute value.` |
|         - |  8888 | `		 */` |
|    336519 |  8889 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    336519 |  8890 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8891 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8892 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8893 | `				return SXERR_ABORT;` |
|         - |  8894 | `			}` |
|       ! 0 |  8895 | `		}` |
|         - |  8896 | `		/* Emit the done instruction */` |
|    336519 |  8897 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    336519 |  8898 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    336519 |  8899 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    336519 |  8900 | `		pGen->pEnd = pSavedDefEnd;` |
|    168257 |  8901 | `	}` |
|         - |  8902 | `	/* All done,install the attribute */` |
|    458915 |  8903 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    458915 |  8904 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8905 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8906 | `		return SXERR_ABORT;` |
|         - |  8907 | `	}` |
|    458915 |  8908 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8909 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8910 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8911 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8912 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8913 | `			return SXERR_ABORT;` |
|         - |  8914 | `		}` |
|        95 |  8915 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8916 | `			goto Synchronize;` |
|         - |  8917 | `		}` |
|        95 |  8918 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8919 | `		return SXRET_OK;` |
|         - |  8920 | `	}` |
|    458821 |  8921 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8922 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8923 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8924 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8925 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8926 | `				? "Interfaces may only include hooked properties"` |
|         - |  8927 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8928 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8929 | `			return SXERR_ABORT;` |
|         - |  8930 | `		}` |
|       ! 0 |  8931 | `		goto Synchronize;` |
|         - |  8932 | `	}` |
|    458821 |  8933 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8934 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8935 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8936 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8937 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8938 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8939 | `				pTok--;` |
|       ! 0 |  8940 | `			}` |
|       ! 0 |  8941 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8942 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8943 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8944 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8945 | `				return SXERR_ABORT;` |
|         - |  8946 | `			}` |
|       ! 0 |  8947 | `		}else{` |
|         5 |  8948 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8949 | `				goto loop;` |
|         - |  8950 | `			}` |
|         - |  8951 | `		}` |
|       ! 0 |  8952 | `	}` |
|    458817 |  8953 | `	SySetRelease(&aUnionAlts);` |
|    458817 |  8954 | `	return SXRET_OK;` |
|         9 |  8955 | `Synchronize:` |
|         - |  8956 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8957 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  8958 | `		pGen->pIn++;` |
|         3 |  8959 | `	}` |
|        22 |  8960 | `	SySetRelease(&aUnionAlts);` |
|        22 |  8961 | `	return SXERR_CORRUPT;` |
|    229467 |  8962 | `}` |
|         - |  8963 | `/*` |
|         - |  8964 | ` * Compile a class method.` |
|         - |  8965 | ` *` |
|         - |  8966 | ` * Refer to the official documentation for more information` |
|         - |  8967 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  8968 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  8969 | ` * overloading and many more.` |
|         - |  8970 | ` */` |
|   2394282 |  8971 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  8972 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  8973 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  8974 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  8975 | `	int doBody,          /* TRUE to process method body */` |
|         - |  8976 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  8977 | `	)` |
|         5 |  8978 | `{` |
|   2394287 |  8979 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2394287 |  8980 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  8981 | `	ph7_class_method *pMeth;` |
|         - |  8982 | `	sxi32 iFuncFlags;` |
|         - |  8983 | `	SyString *pName;` |
|         - |  8984 | `	SyToken *pEnd;` |
|         - |  8985 | `	sxi32 rc;` |
|         - |  8986 | `	/* Extract visibility level */` |
|   2394287 |  8987 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2394287 |  8988 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2394287 |  8989 | `	iFuncFlags = 0;` |
|   2394287 |  8990 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  8991 | `		/* Invalid method name */` |
|       ! 0 |  8992 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8993 | `		if( rc == SXERR_ABORT ){` |
|         - |  8994 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8995 | `			return SXERR_ABORT;` |
|         - |  8996 | `		}` |
|       ! 0 |  8997 | `		goto Synchronize;` |
|         - |  8998 | `	}` |
|   2394287 |  8999 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  9000 | `		/* Return by reference,remember that */` |
|       ! 0 |  9001 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  9002 | `		/* Jump the '&' token */` |
|       ! 0 |  9003 | `		pGen->pIn++;` |
|       ! 0 |  9004 | `	}` |
|   2394287 |  9005 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  9006 | `		/* Invalid method name */` |
|       ! 0 |  9007 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9008 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9009 | `			return SXERR_ABORT;` |
|         - |  9010 | `		}` |
|       ! 0 |  9011 | `		goto Synchronize;` |
|         - |  9012 | `	}` |
|         - |  9013 | `	/* Peek method name */` |
|   2394287 |  9014 | `	pName = &pGen->pIn->sData;` |
|   2394287 |  9015 | `	nLine = pGen->pIn->nLine;` |
|         - |  9016 | `	/* Jump the method name */` |
|   2394287 |  9017 | `	pGen->pIn++;` |
|   2394287 |  9018 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  9019 | `		/* Abstract method */` |
|    137447 |  9020 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  9021 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9022 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  9023 | `				&pClass->sName,pName);` |
|       ! 0 |  9024 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9025 | `				return SXERR_ABORT;` |
|         - |  9026 | `			}` |
|       ! 0 |  9027 | `		}` |
|         - |  9028 | `		/* Assemble method signature only */` |
|    137447 |  9029 | `		doBody = FALSE;` |
|     68721 |  9030 | `	}` |
|   2394287 |  9031 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  9032 | `		/* Syntax error */` |
|       ! 0 |  9033 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  9034 | `		if( rc == SXERR_ABORT ){` |
|         - |  9035 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9036 | `			return SXERR_ABORT;` |
|         - |  9037 | `		}` |
|       ! 0 |  9038 | `		goto Synchronize;` |
|         - |  9039 | `	}` |
|         - |  9040 | `	/* Allocate a new class_method instance */` |
|   2394287 |  9041 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2394287 |  9042 | `	if( pMeth == 0 ){` |
|       ! 0 |  9043 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9044 | `		return SXERR_ABORT;` |
|         - |  9045 | `	}` |
|   2394287 |  9046 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2394287 |  9047 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2394287 |  9048 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9049 | `		return SXERR_ABORT;` |
|         - |  9050 | `	}` |
|         - |  9051 | `	/* Jump the left parenthesis '(' */` |
|   2394287 |  9052 | `	pGen->pIn++;` |
|   2394287 |  9053 | `	pEnd = 0; /* cc warning */` |
|         - |  9054 | `	/* Delimit the method signature */` |
|   2394287 |  9055 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2394287 |  9056 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9057 | `		/* Syntax error */` |
|         3 |  9058 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  9059 | `		if( rc == SXERR_ABORT ){` |
|         - |  9060 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9061 | `			return SXERR_ABORT;` |
|         - |  9062 | `		}` |
|         3 |  9063 | `		goto Synchronize;` |
|         - |  9064 | `	}` |
|         - |  9065 | `	{` |
|   2394285 |  9066 | `		int bIsCtor = 0;` |
|   2394285 |  9067 | `		int bAbstractCtor = 0;` |
|   2394280 |  9068 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1397640 |  9069 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2312136 |  9070 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    164303 |  9071 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  9072 | `				bAbstractCtor = 1;` |
|         2 |  9073 | `			}else{` |
|    164301 |  9074 | `				bIsCtor = 1;` |
|         - |  9075 | `			}` |
|     82149 |  9076 | `		}` |
|   2394285 |  9077 | `		if( pGen->pIn < pEnd ){` |
|         - |  9078 | `			/* Collect method arguments */` |
|    863105 |  9079 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    863105 |  9080 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9081 | `				return SXERR_ABORT;` |
|         - |  9082 | `			}` |
|    431550 |  9083 | `		}` |
|         - |  9084 | `	}` |
|         - |  9085 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2394285 |  9086 | `	pGen->pIn = &pEnd[1];` |
|         - |  9087 | `	{` |
|   2394285 |  9088 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2394285 |  9089 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  9090 | `			return SXERR_ABORT;` |
|   2394285 |  9091 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  9092 | `			goto Synchronize;` |
|         - |  9093 | `		}` |
|         - |  9094 | `	}` |
|         - |  9095 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  9096 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  9097 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  9098 | `	{` |
|   2394285 |  9099 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  9100 | `		sxu32 i;` |
|   3684955 |  9101 | `		for( i = 0; i < nArg; i++ ){` |
|   1290685 |  9102 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  9103 | `			ph7_class_attr *pAttr;` |
|   1290685 |  9104 | `			sxi32 iAttrFlags = 0;` |
|         - |  9105 | `			int bArgTyped;` |
|   1290685 |  9106 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1290601 |  9107 | `				continue;` |
|         - |  9108 | `			}` |
|         - |  9109 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  9110 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  9111 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  9112 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  9113 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  9114 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  9115 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9116 | `					"Cannot declare variadic promoted property");` |
|         3 |  9117 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9118 | `					return SXERR_ABORT;` |
|         - |  9119 | `				}` |
|         3 |  9120 | `				goto Synchronize;` |
|         - |  9121 | `			}` |
|         - |  9122 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  9123 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  9124 | `			 * appear as an alternative of a union type. */` |
|        87 |  9125 | `			if( bArgTyped ){` |
|       122 |  9126 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  9127 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  9128 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  9129 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  9130 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9131 | `					return SXERR_ABORT;` |
|        83 |  9132 | `				}else if( rc != SXRET_OK ){` |
|         6 |  9133 | `					goto Synchronize;` |
|         - |  9134 | `				}` |
|        37 |  9135 | `			}` |
|         - |  9136 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  9137 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  9138 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9139 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  9140 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9141 | `					return SXERR_ABORT;` |
|         - |  9142 | `				}` |
|         3 |  9143 | `				goto Synchronize;` |
|         - |  9144 | `			}` |
|        81 |  9145 | `			if( bArgTyped ){` |
|        77 |  9146 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  9147 | `			}` |
|        81 |  9148 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  9149 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  9150 | `			}` |
|        81 |  9151 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  9152 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  9153 | `			}` |
|        81 |  9154 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  9155 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  9156 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  9157 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  9158 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9159 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  9160 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9161 | `						return SXERR_ABORT;` |
|         - |  9162 | `					}` |
|         3 |  9163 | `					goto Synchronize;` |
|         - |  9164 | `				}` |
|        24 |  9165 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  9166 | `			}` |
|        79 |  9167 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  9168 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  9169 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  9170 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9171 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  9172 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  9173 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9174 | `						return SXERR_ABORT;` |
|         - |  9175 | `					}` |
|       ! 0 |  9176 | `					goto Synchronize;` |
|         - |  9177 | `				}` |
|         5 |  9178 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  9179 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  9180 | `			}` |
|        79 |  9181 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  9182 | `			if( pAttr == 0 ){` |
|       ! 0 |  9183 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9184 | `				return SXERR_ABORT;` |
|         - |  9185 | `			}` |
|        79 |  9186 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  9187 | `				pAttr->nType = pArg->nType;` |
|        77 |  9188 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  9189 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  9190 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9191 | `					sxu32 k;` |
|        20 |  9192 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9193 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9194 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9195 | `					}` |
|         3 |  9196 | `				}` |
|        36 |  9197 | `			}` |
|        79 |  9198 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  9199 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9200 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9201 | `				return SXERR_ABORT;` |
|         - |  9202 | `			}` |
|        42 |  9203 | `		}` |
|         - |  9204 | `	}` |
|   2394275 |  9205 | `	if( doBody ){` |
|         - |  9206 | `		/* Compile method body */` |
|   2256833 |  9207 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2256833 |  9208 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9209 | `			return SXERR_ABORT;` |
|         - |  9210 | `		}` |
|         - |  9211 | `		/* The cursor sits just past the body's closing brace */` |
|   2256833 |  9212 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1128419 |  9213 | `	}else{` |
|         - |  9214 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    137447 |  9215 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    137447 |  9216 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     68721 |  9217 | `		}` |
|         - |  9218 | `		/* Only method signature is allowed */` |
|    137447 |  9219 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9220 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9221 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9222 | `				if( rc == SXERR_ABORT ){` |
|         - |  9223 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9224 | `					return SXERR_ABORT;` |
|         - |  9225 | `				}` |
|       ! 0 |  9226 | `				return SXERR_CORRUPT;` |
|         - |  9227 | `			}` |
|         - |  9228 | `	}` |
|         - |  9229 | `	/* All done,install the method */` |
|   2394275 |  9230 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2394275 |  9231 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9232 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9233 | `		return SXERR_ABORT;` |
|         - |  9234 | `	}` |
|   2394275 |  9235 | `	return SXRET_OK;` |
|         6 |  9236 | `Synchronize:` |
|         - |  9237 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9238 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9239 | `		pGen->pIn++;` |
|         4 |  9240 | `	}` |
|        16 |  9241 | `	return SXERR_CORRUPT;` |
|   1197146 |  9242 | `}` |
|         - |  9243 | `/*` |
|         - |  9244 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9245 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9246 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9247 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9248 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9249 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9250 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9251 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9252 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9253 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9254 | `` * implicit `$value` formal.`` |
|         - |  9255 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9256 | ` */` |
|         - |  9257 | `/*` |
|         - |  9258 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9259 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9260 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9261 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9262 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9263 | ` */` |
|        94 |  9264 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9265 | `{` |
|         - |  9266 | `	SyToken *p;` |
|       345 |  9267 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9268 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9269 | `			continue;` |
|         - |  9270 | `		}` |
|         - |  9271 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9272 | `		if( p + 3 < pEnd` |
|        80 |  9273 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9274 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9275 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9276 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9277 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9278 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9279 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9280 | `			return 1;` |
|         - |  9281 | `		}` |
|         - |  9282 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9283 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9284 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9285 | `		if( p > pStart` |
|        26 |  9286 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9287 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9288 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9289 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9290 | `			return 1;` |
|         - |  9291 | `		}` |
|        15 |  9292 | `	}` |
|        43 |  9293 | `	return 0;` |
|        48 |  9294 | `}` |
|         - |  9295 | `/*` |
|         - |  9296 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9297 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9298 | ` */` |
|       990 |  9299 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9300 | `{` |
|      1167 |  9301 | `	return p + 6 < pEnd` |
|       671 |  9302 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9303 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9304 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9305 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9306 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9307 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9308 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9309 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9310 | `	 && p[5].sData.nByte == 3` |
|         8 |  9311 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9312 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9313 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9314 | `}` |
|         - |  9315 | `/*` |
|         - |  9316 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9317 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9318 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9319 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9320 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9321 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9322 | ` * or SXERR_MEM.` |
|         - |  9323 | ` */` |
|         4 |  9324 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9325 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9326 | `{` |
|         5 |  9327 | `	SyToken *p = pStart;` |
|        35 |  9328 | `	while( p < pEnd ){` |
|        31 |  9329 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9330 | `			SyToken sTok;` |
|         - |  9331 | `			char zName[384];` |
|         - |  9332 | `			sxu32 nName;` |
|         - |  9333 | `			char *zDup;` |
|         - |  9334 | ``			/* `parent` `::` */`` |
|         5 |  9335 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9336 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9337 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9338 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9339 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9340 | `			if( zDup == 0 ){` |
|       ! 0 |  9341 | `				return SXERR_MEM;` |
|         - |  9342 | `			}` |
|         5 |  9343 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9344 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9345 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9346 | `			sTok.pUserData = 0;` |
|         5 |  9347 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9348 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9349 | `			continue;` |
|         - |  9350 | `		}` |
|        27 |  9351 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9352 | `		p++;` |
|         1 |  9353 | `	}` |
|         5 |  9354 | `	return SXRET_OK;` |
|         3 |  9355 | `}` |
|        94 |  9356 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9357 | `{` |
|        95 |  9358 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9359 | `	sxi32 rc;` |
|        95 |  9360 | `	int bRefsSelf = 0;` |
|        95 |  9361 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9362 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9363 | `		char zHook[384];` |
|         - |  9364 | `		SyString sHookName;` |
|         - |  9365 | `		ph7_class_method *pMeth;` |
|         - |  9366 | `		int bGet;` |
|       159 |  9367 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9368 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9369 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9370 | `			continue;` |
|         - |  9371 | `		}` |
|       145 |  9372 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9373 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9374 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9375 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9376 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9377 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9378 | `				return SXERR_ABORT;` |
|         - |  9379 | `			}` |
|       ! 0 |  9380 | `			return SXERR_CORRUPT;` |
|         - |  9381 | `		}` |
|       145 |  9382 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9383 | `			goto HookSyntax;` |
|         - |  9384 | `		}` |
|       144 |  9385 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9386 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9387 | `			bGet = 1;` |
|       106 |  9388 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9389 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9390 | `			bGet = 0;` |
|        34 |  9391 | `		}else{` |
|       ! 0 |  9392 | `			goto HookSyntax;` |
|         - |  9393 | `		}` |
|       145 |  9394 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9395 | `		sHookName.zString = zHook;` |
|       217 |  9396 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9397 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9398 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9399 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9400 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9401 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9402 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9403 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9404 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9405 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9406 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9407 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9408 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9409 | `					return SXERR_ABORT;` |
|         - |  9410 | `				}` |
|       ! 0 |  9411 | `				return SXERR_CORRUPT;` |
|         - |  9412 | `			}` |
|        15 |  9413 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9414 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9415 | `			if( pMeth == 0 ){` |
|       ! 0 |  9416 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9417 | `				return SXERR_ABORT;` |
|         - |  9418 | `			}` |
|        15 |  9419 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9420 | `			if( !bGet ){` |
|         - |  9421 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9422 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9423 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9424 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9425 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9426 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9427 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9428 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9429 | `				if( zVName == 0 ){` |
|       ! 0 |  9430 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9431 | `					return SXERR_ABORT;` |
|         - |  9432 | `				}` |
|         7 |  9433 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9434 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9435 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9436 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9437 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9438 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9439 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9440 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9441 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9442 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9443 | `				}` |
|         7 |  9444 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9445 | `			}` |
|        15 |  9446 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9447 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9448 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9449 | `				return SXERR_ABORT;` |
|         - |  9450 | `			}` |
|        15 |  9451 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9452 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9453 | `		}` |
|       130 |  9454 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9455 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9456 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9457 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9458 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9459 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9460 | `				return SXERR_ABORT;` |
|         - |  9461 | `			}` |
|       ! 0 |  9462 | `			return SXERR_CORRUPT;` |
|         - |  9463 | `		}` |
|       131 |  9464 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9465 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9466 | `		if( pMeth == 0 ){` |
|       ! 0 |  9467 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9468 | `			return SXERR_ABORT;` |
|         - |  9469 | `		}` |
|       131 |  9470 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9471 | `		if( !bGet ){` |
|         - |  9472 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9473 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9474 | `				SyToken *pRp = 0;` |
|        17 |  9475 | `				pGen->pIn++;` |
|        17 |  9476 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9477 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9478 | `					goto HookSyntax;` |
|         - |  9479 | `				}` |
|        17 |  9480 | `				if( pGen->pIn < pRp ){` |
|        17 |  9481 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9482 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9483 | `						return SXERR_ABORT;` |
|         - |  9484 | `					}` |
|         8 |  9485 | `				}` |
|        17 |  9486 | `				pGen->pIn = &pRp[1];` |
|         8 |  9487 | `			}` |
|        61 |  9488 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9489 | `				/* Implicit $value formal */` |
|         - |  9490 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9491 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9492 | `				if( zVName == 0 ){` |
|       ! 0 |  9493 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9494 | `					return SXERR_ABORT;` |
|         - |  9495 | `				}` |
|        45 |  9496 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9497 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9498 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9499 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9500 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9501 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9502 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9503 | `			}` |
|        30 |  9504 | `		}` |
|       165 |  9505 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9506 | `			/* Block body */` |
|        69 |  9507 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9508 | `			SyToken *pCloser = 0;` |
|        69 |  9509 | `			int bParentCall = 0;` |
|        69 |  9510 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9511 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9512 | `				SyToken *pScan;` |
|       753 |  9513 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9514 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9515 | `						bParentCall = 1;` |
|         3 |  9516 | `						break;` |
|         - |  9517 | `					}` |
|       343 |  9518 | `				}` |
|        34 |  9519 | `			}` |
|        69 |  9520 | `			if( bParentCall ){` |
|         - |  9521 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9522 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9523 | `				 * hook method), then continue past the original body. */` |
|         - |  9524 | `				SySet sBody;` |
|         3 |  9525 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9526 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9527 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9528 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9529 | `					SySetRelease(&sBody);` |
|       ! 0 |  9530 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9531 | `					return SXERR_ABORT;` |
|         - |  9532 | `				}` |
|         3 |  9533 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9534 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9535 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9536 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9537 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9538 | `				SySetRelease(&sBody);` |
|         3 |  9539 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9540 | `					return SXERR_ABORT;` |
|         - |  9541 | `				}` |
|         3 |  9542 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9543 | `			}else{` |
|        67 |  9544 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9545 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9546 | `					return SXERR_ABORT;` |
|         - |  9547 | `				}` |
|        67 |  9548 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9549 | `			}` |
|        69 |  9550 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9551 | `				bRefsSelf = 1;` |
|         9 |  9552 | `			}` |
|       128 |  9553 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9554 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9555 | `			GenBlock *pBlock;` |
|         - |  9556 | `			SySet *pInstrContainer;` |
|         - |  9557 | `			SyToken *pBodyStart;` |
|         - |  9558 | `			SyToken *pExprEnd;` |
|        63 |  9559 | `			SyToken *pSavedEnd = 0;` |
|         - |  9560 | `			SySet sBody;` |
|        63 |  9561 | `			int bParentCall = 0;` |
|        63 |  9562 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9563 | `			pBodyStart = pGen->pIn;` |
|         - |  9564 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9565 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9566 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9567 | `			 * method on a token copy. */` |
|         - |  9568 | `			{` |
|        63 |  9569 | `				sxi32 iNest = 0;` |
|        63 |  9570 | `				pExprEnd = pBodyStart;` |
|       355 |  9571 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9572 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9573 | `						iNest++;` |
|       351 |  9574 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9575 | `						if( iNest <= 0 ){` |
|       ! 0 |  9576 | `							break;` |
|         - |  9577 | `						}` |
|         9 |  9578 | `						iNest--;` |
|       343 |  9579 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9580 | `						break;` |
|         - |  9581 | `					}` |
|       293 |  9582 | `					pExprEnd++;` |
|         1 |  9583 | `				}` |
|         - |  9584 | `			}` |
|         - |  9585 | `			{` |
|         - |  9586 | `				SyToken *pScan;` |
|       335 |  9587 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9588 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9589 | `						bParentCall = 1;` |
|         3 |  9590 | `						break;` |
|         - |  9591 | `					}` |
|       137 |  9592 | `				}` |
|         - |  9593 | `			}` |
|        63 |  9594 | `			if( bParentCall ){` |
|         3 |  9595 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9596 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9597 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9598 | `					SySetRelease(&sBody);` |
|       ! 0 |  9599 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9600 | `					return SXERR_ABORT;` |
|         - |  9601 | `				}` |
|         3 |  9602 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9603 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9604 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9605 | `			}` |
|        94 |  9606 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9607 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9608 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9609 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9610 | `				return SXERR_ABORT;` |
|         - |  9611 | `			}` |
|        63 |  9612 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9613 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9614 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9615 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9616 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9617 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9618 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9619 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9620 | `			if( bParentCall ){` |
|         3 |  9621 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9622 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9623 | `				SySetRelease(&sBody);` |
|         1 |  9624 | `			}` |
|        63 |  9625 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9626 | `				return SXERR_ABORT;` |
|         - |  9627 | `			}` |
|        63 |  9628 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9629 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9630 | `				bRefsSelf = 1;` |
|        18 |  9631 | `			}` |
|        63 |  9632 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9633 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9634 | `			}` |
|        63 |  9635 | `			if( !bGet ){` |
|         - |  9636 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9637 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9638 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9639 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9640 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9641 | `				bRefsSelf = 1;` |
|         1 |  9642 | `			}` |
|        32 |  9643 | `		}else{` |
|       ! 0 |  9644 | `			goto HookSyntax;` |
|         - |  9645 | `		}` |
|       131 |  9646 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9647 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9648 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9649 | `			return SXERR_ABORT;` |
|         - |  9650 | `		}` |
|       131 |  9651 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9652 | `	}` |
|        95 |  9653 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9654 | `		goto HookSyntax;` |
|         - |  9655 | `	}` |
|        95 |  9656 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9657 | `	if( !bRefsSelf ){` |
|         - |  9658 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9659 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9660 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9661 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9662 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9663 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9664 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9665 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9666 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9667 | `				return SXERR_ABORT;` |
|         - |  9668 | `			}` |
|       ! 0 |  9669 | `			return SXERR_CORRUPT;` |
|         - |  9670 | `		}` |
|        20 |  9671 | `	}` |
|        95 |  9672 | `	return SXRET_OK;` |
|       ! 0 |  9673 | `HookSyntax:` |
|       ! 0 |  9674 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9675 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9676 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9677 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9678 | `		return SXERR_ABORT;` |
|         - |  9679 | `	}` |
|       ! 0 |  9680 | `	return SXERR_CORRUPT;` |
|        48 |  9681 | `}` |
|         - |  9682 | `/*` |
|         - |  9683 | ` * Compile an object interface.` |
|         - |  9684 | ` *  According to the PHP language reference manual` |
|         - |  9685 | ` *   Object Interfaces:` |
|         - |  9686 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9687 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9688 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9689 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9690 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9691 | ` */` |
|     68798 |  9692 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9693 | `{` |
|     68803 |  9694 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9695 | `	ph7_class *pClass,*pBase;` |
|         - |  9696 | `	SyToken *pEnd,*pTmp;` |
|         - |  9697 | `	SyString *pName;` |
|         - |  9698 | `	sxi32 nKwrd;` |
|         - |  9699 | `	sxi32 rc;` |
|         - |  9700 | `	/* Jump the 'interface' keyword */` |
|     68803 |  9701 | `	pGen->pIn++;` |
|         - |  9702 | `	/* Extract interface name */` |
|     68803 |  9703 | `	pName = &pGen->pIn->sData;` |
|         - |  9704 | `	/* Advance the stream cursor */` |
|     68803 |  9705 | `	pGen->pIn++;` |
|         - |  9706 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9707 | `		SyBlob sFQN;` |
|         - |  9708 | `		SyString sFQNStr;` |
|     68803 |  9709 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     68803 |  9710 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     68803 |  9711 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     68803 |  9712 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     68803 |  9713 | `		SyBlobRelease(&sFQN);` |
|         - |  9714 | `	}` |
|     68803 |  9715 | `	if( pClass == 0 ){` |
|       ! 0 |  9716 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9717 | `		return SXERR_ABORT;` |
|         - |  9718 | `	}` |
|     68803 |  9719 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     68803 |  9720 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9721 | `		return SXERR_ABORT;` |
|         - |  9722 | `	}` |
|         - |  9723 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     68803 |  9724 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9725 | `	/* Assume no base class is given */` |
|     68803 |  9726 | `	pBase = 0;` |
|     68803 |  9727 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26727 |  9728 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26727 |  9729 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9730 | `			SyBlob sResolved;` |
|         - |  9731 | `			SyString sBaseName;` |
|         - |  9732 | `			sxu32 nRefLine;` |
|         - |  9733 | `			/* Extract base interface */` |
|     26727 |  9734 | `			pGen->pIn++;` |
|     26727 |  9735 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26727 |  9736 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26727 |  9737 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9738 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9739 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9740 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9741 | `					pName);` |
|       ! 0 |  9742 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9743 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9744 | `					return SXERR_ABORT;` |
|         - |  9745 | `				}` |
|       ! 0 |  9746 | `				return SXRET_OK;` |
|         - |  9747 | `			}` |
|     40088 |  9748 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     26722 |  9749 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26727 |  9750 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9751 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9752 | `			/* Only interfaces is allowed */` |
|     26727 |  9753 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9754 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9755 | `			}` |
|     26727 |  9756 | `			if( pBase == 0 ){` |
|       ! 0 |  9757 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9758 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9759 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9760 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9761 | `					return SXERR_ABORT;` |
|         - |  9762 | `				}` |
|       ! 0 |  9763 | `			}` |
|     26727 |  9764 | `			SyBlobRelease(&sResolved);` |
|     13361 |  9765 | `		}` |
|     13361 |  9766 | `	}` |
|     68803 |  9767 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9768 | `		/* Syntax error */` |
|       ! 0 |  9769 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9770 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9771 | `		if( rc == SXERR_ABORT ){` |
|         - |  9772 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9773 | `			return SXERR_ABORT;` |
|         - |  9774 | `		}` |
|       ! 0 |  9775 | `		return SXRET_OK;` |
|         - |  9776 | `	}` |
|     68803 |  9777 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     68803 |  9778 | `	pEnd = 0; /* cc warning */` |
|         - |  9779 | `	/* Delimit the interface body */` |
|     68803 |  9780 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     68803 |  9781 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9782 | `		/* Syntax error */` |
|       ! 0 |  9783 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9784 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9785 | `		if( rc == SXERR_ABORT ){` |
|         - |  9786 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9787 | `			return SXERR_ABORT;` |
|         - |  9788 | `		}` |
|       ! 0 |  9789 | `		return SXRET_OK;` |
|         - |  9790 | `	}` |
|         - |  9791 | `	/* The delimiter token is the interface body's closing brace */` |
|     68803 |  9792 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9793 | `	/* Swap token stream */` |
|     68803 |  9794 | `	pTmp = pGen->pEnd;` |
|     68803 |  9795 | `	pGen->pEnd = pEnd;` |
|         - |  9796 | `	/* Start the parse process` |
|         - |  9797 | `	 * Note (According to the PHP reference manual):` |
|         - |  9798 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9799 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9800 | `	 */` |
|    126013 |  9801 | `	for(;;){` |
|         - |  9802 | `		/* Jump leading/trailing semi-colons */` |
|    435263 |  9803 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    183233 |  9804 | `			pGen->pIn++;` |
|         5 |  9805 | `		}` |
|    252035 |  9806 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9807 | `			/* End of interface body */` |
|     68799 |  9808 | `			break;` |
|         - |  9809 | `		}` |
|         - |  9810 | `		/* Bind a directly-preceding docblock to this member */` |
|    183241 |  9811 | `		GenStateSetPendingDoc(&(*pGen));` |
|    183241 |  9812 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9813 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9814 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9815 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9816 | `			if( rc == SXERR_ABORT ){` |
|         - |  9817 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9818 | `				return SXERR_ABORT;` |
|         - |  9819 | `			}` |
|       ! 0 |  9820 | `			goto done;` |
|         - |  9821 | `		}` |
|         - |  9822 | `		/* Extract the current keyword */` |
|    183241 |  9823 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    183241 |  9824 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9825 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9826 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9827 | `			const char *zKind = "member";` |
|         3 |  9828 | `			SyString *pMemberName = 0;` |
|         3 |  9829 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9830 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9831 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9832 | `					zKind = "constant";` |
|         3 |  9833 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9834 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9835 | `					}` |
|         1 |  9836 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9837 | `					zKind = "method";` |
|       ! 0 |  9838 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9839 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9840 | `					}` |
|       ! 0 |  9841 | `				}` |
|         1 |  9842 | `			}` |
|         3 |  9843 | `			if( pMemberName ){` |
|         4 |  9844 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9845 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9846 | `			}else{` |
|       ! 0 |  9847 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9848 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9849 | `			}` |
|         3 |  9850 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9851 | `				return SXERR_ABORT;` |
|         - |  9852 | `			}` |
|         3 |  9853 | `			goto done;` |
|         - |  9854 | `		}` |
|    183239 |  9855 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9856 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9857 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9858 | `			if( rc == SXERR_ABORT ){` |
|         - |  9859 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9860 | `				return SXERR_ABORT;` |
|         - |  9861 | `			}` |
|       ! 0 |  9862 | `			goto done;` |
|         - |  9863 | `		}` |
|    183239 |  9864 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9865 | `			/* Advance the stream cursor */` |
|    129797 |  9866 | `			pGen->pIn++;` |
|    129792 |  9867 | `			if( pGen->pIn < pGen->pEnd` |
|    129797 |  9868 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    129792 |  9869 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9870 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9871 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9872 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9873 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9874 | `				 * hooked properties" error). */` |
|       ! 0 |  9875 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9876 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9877 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9878 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9879 | `						return SXERR_ABORT;` |
|         - |  9880 | `					}` |
|       ! 0 |  9881 | `					goto done;` |
|         - |  9882 | `				}` |
|       ! 0 |  9883 | `				continue;` |
|         - |  9884 | `			}` |
|    129797 |  9885 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9886 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9887 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9888 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9889 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9890 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9891 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9892 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9893 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9894 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9895 | `							return SXERR_ABORT;` |
|         - |  9896 | `						}` |
|       ! 0 |  9897 | `						goto done;` |
|         - |  9898 | `					}` |
|       ! 0 |  9899 | `					continue;` |
|         - |  9900 | `				}` |
|       ! 0 |  9901 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9902 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9903 | `				if( rc == SXERR_ABORT ){` |
|         - |  9904 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9905 | `					return SXERR_ABORT;` |
|         - |  9906 | `				}` |
|       ! 0 |  9907 | `				goto done;` |
|         - |  9908 | `			}` |
|    129797 |  9909 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    129797 |  9910 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9911 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9912 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9913 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9914 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9915 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9916 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9917 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9918 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9919 | `							return SXERR_ABORT;` |
|         - |  9920 | `						}` |
|       ! 0 |  9921 | `						goto done;` |
|         - |  9922 | `					}` |
|         5 |  9923 | `					continue;` |
|         - |  9924 | `				}` |
|       ! 0 |  9925 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9926 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9927 | `				if( rc == SXERR_ABORT ){` |
|         - |  9928 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9929 | `					return SXERR_ABORT;` |
|         - |  9930 | `				}` |
|       ! 0 |  9931 | `				goto done;` |
|         - |  9932 | `			}` |
|     64894 |  9933 | `		}` |
|    183235 |  9934 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9935 | `			/* Parse constant */` |
|     53443 |  9936 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53443 |  9937 | `			if( rc != SXRET_OK ){` |
|         3 |  9938 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9939 | `					return SXERR_ABORT;` |
|         - |  9940 | `				}` |
|         3 |  9941 | `				goto done;` |
|         - |  9942 | `			}` |
|     26723 |  9943 | `		}else{` |
|    129797 |  9944 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    129797 |  9945 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9946 | `				/* Static method,record that */` |
|     11453 |  9947 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9948 | `				/* Advance the stream cursor */` |
|     11453 |  9949 | `				pGen->pIn++;` |
|     11448 |  9950 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11453 |  9951 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9952 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9953 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9954 | `						if( rc == SXERR_ABORT ){` |
|         - |  9955 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9956 | `							return SXERR_ABORT;` |
|         - |  9957 | `						}` |
|       ! 0 |  9958 | `						goto done;` |
|         - |  9959 | `				}` |
|      5724 |  9960 | `			}` |
|         - |  9961 | `			/* Process method signature (no body for interface methods) */` |
|    129797 |  9962 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    129797 |  9963 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9964 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9965 | `					return SXERR_ABORT;` |
|         - |  9966 | `				}` |
|       ! 0 |  9967 | `				goto done;` |
|         - |  9968 | `			}` |
|         - |  9969 | `		}` |
|         5 |  9970 | `	}` |
|         - |  9971 | `	/* Install the interface */` |
|     68799 |  9972 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     68799 |  9973 | `	if( rc == SXRET_OK && pBase ){` |
|         - |  9974 | `		/* Inherit from the base interface */` |
|     26727 |  9975 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13361 |  9976 | `	}` |
|     68799 |  9977 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9978 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9979 | `		return SXERR_ABORT;` |
|         - |  9980 | `	}` |
|     34397 |  9981 | `done:` |
|         - |  9982 | `	/* Point beyond the interface body */` |
|     68803 |  9983 | `	pGen->pIn  = &pEnd[1];` |
|     68803 |  9984 | `	pGen->pEnd = pTmp;` |
|     68803 |  9985 | `	return PH7_OK;` |
|     34404 |  9986 | `}` |
|         - |  9987 | `/*` |
|         - |  9988 | ` * Compile a user-defined class.` |
|         - |  9989 | ` * According to the PHP language reference manual` |
|         - |  9990 | ` *  class` |
|         - |  9991 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - |  9992 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - |  9993 | ` *  of the properties and methods belonging to the class.` |
|         - |  9994 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - |  9995 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - |  9996 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - |  9997 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  9998 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - |  9999 | ` *  (called "methods").` |
|         - | 10000 | ` */` |
|         - | 10001 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - | 10002 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - | 10003 | `struct TraitUseEntry {` |
|         - | 10004 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - | 10005 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - | 10006 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - | 10007 | `};` |
|         - | 10008 | `/*` |
|         - | 10009 | ` * Validate that methods implementing interface contracts have compatible` |
|         - | 10010 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - | 10011 | ` */` |
|    352840 | 10012 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10013 | `{` |
|         - | 10014 | `	ph7_class **apIface;` |
|         - | 10015 | `	sxu32 nIface,i;` |
|         - | 10016 | `	sxi32 rc;` |
|    352845 | 10017 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 | 10018 | `		return SXRET_OK;` |
|         - | 10019 | `	}` |
|    352845 | 10020 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    352845 | 10021 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    708073 | 10022 | `	for(i = 0; i < nIface; i++){` |
|    355233 | 10023 | `		ph7_class *pIface = apIface[i];` |
|         - | 10024 | `		SyHashEntry *pEntry;` |
|    355233 | 10025 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1023619 | 10026 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    668391 | 10027 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - | 10028 | `			ph7_class_method *pImplMeth;` |
|    668391 | 10029 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - | 10030 | `			/* Find the implementing method in the class */` |
|    668391 | 10031 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    668391 | 10032 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 | 10033 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - | 10034 | `			}` |
|         - | 10035 | `			/* Check visibility: interface methods must be implemented as public */` |
|    668373 | 10036 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 | 10037 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10038 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 | 10039 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 | 10040 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10041 | `					return SXERR_ABORT;` |
|         - | 10042 | `				}` |
|         1 | 10043 | `			}` |
|         - | 10044 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - | 10045 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - | 10046 | `			 */` |
|         - | 10047 | `			{` |
|    668373 | 10048 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    668373 | 10049 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    668373 | 10050 | `				int sigError = 0;` |
|    668373 | 10051 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 | 10052 | `					sigError = 1;` |
|    668372 | 10053 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - | 10054 | `					/* Extra parameters must all have default values */` |
|      3825 | 10055 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 10056 | `					sxu32 k;` |
|      7643 | 10057 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3825 | 10058 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 10059 | `							sigError = 1;` |
|         3 | 10060 | `							break;` |
|         - | 10061 | `						}` |
|      1914 | 10062 | `					}` |
|      1910 | 10063 | `				}` |
|    668373 | 10064 | `				if( sigError ){` |
|         - | 10065 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 10066 | `					ph7_vm_func_arg *aArgs;` |
|         - | 10067 | `					sxu32 j;` |
|         6 | 10068 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 10069 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 10070 | `					/* Build implementing method signature */` |
|         6 | 10071 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 10072 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 10073 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 10074 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 10075 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10076 | `					}` |
|         - | 10077 | `					/* Build interface method signature */` |
|         6 | 10078 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 10079 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 10080 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 10081 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 10082 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10083 | `					}` |
|         8 | 10084 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10085 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 10086 | `						&pClass->sName,pMName,` |
|         4 | 10087 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 10088 | `						&pIface->sName,pMName,` |
|         4 | 10089 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 10090 | `					SyBlobRelease(&sImplSig);` |
|         6 | 10091 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 10092 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10093 | `						return SXERR_ABORT;` |
|         - | 10094 | `					}` |
|         2 | 10095 | `				}` |
|         - | 10096 | `			}` |
|         5 | 10097 | `		}` |
|    177619 | 10098 | `	}` |
|    352845 | 10099 | `	return SXRET_OK;` |
|    176425 | 10100 | `}` |
|         - | 10101 | `/*` |
|         - | 10102 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 10103 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 10104 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 10105 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 10106 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 10107 | ` * means that specific hook is still missing.` |
|         - | 10108 | ` */` |
|        38 | 10109 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 10110 | `{` |
|         - | 10111 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 10112 | `	ph7_class_attr *pProp;` |
|        38 | 10113 | `	if( pMName->nByte <= nPfx` |
|        27 | 10114 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 10115 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 10116 | `		return 0; /* not a hook stub */` |
|         - | 10117 | `	}` |
|         7 | 10118 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 10119 | `	return pProp != 0` |
|         6 | 10120 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 10121 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 10122 | `}` |
|         - | 10123 | `/*` |
|         - | 10124 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 10125 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 10126 | ` */` |
|        16 | 10127 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 10128 | `{` |
|         - | 10129 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 10130 | `	if( pMName->nByte > nPfx` |
|        12 | 10131 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 10132 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 10133 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 10134 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 10135 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 10136 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 10137 | `		return;` |
|         - | 10138 | `	}` |
|        20 | 10139 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 10140 | `}` |
|         - | 10141 | `/*` |
|         - | 10142 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 10143 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 10144 | ` */` |
|    352840 | 10145 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10146 | `{` |
|         - | 10147 | `	ph7_class_method *pMeth;` |
|         - | 10148 | `	SyHashEntry *pEntry;` |
|         - | 10149 | `	sxu32 nAbstract;` |
|         - | 10150 | `	SyBlob sMsg;` |
|         - | 10151 | `	sxi32 rc;` |
|         - | 10152 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    352845 | 10153 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15315 | 10154 | `		return SXRET_OK;` |
|         - | 10155 | `	}` |
|         - | 10156 | `	/* Count abstract methods */` |
|    337535 | 10157 | `	nAbstract = 0;` |
|    337535 | 10158 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   4996450 | 10159 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4490155 | 10160 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4490155 | 10161 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 10162 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 10163 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10164 | `			}` |
|        20 | 10165 | `			nAbstract++;` |
|         8 | 10166 | `		}` |
|         5 | 10167 | `	}` |
|    337535 | 10168 | `	if( nAbstract == 0 ){` |
|    337521 | 10169 | `		return SXRET_OK;` |
|         - | 10170 | `	}` |
|         - | 10171 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 10172 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 10173 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 10174 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 10175 | `		&pClass->sName,nAbstract,` |
|         7 | 10176 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 10177 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 10178 | `	/* Second pass: list methods with origins */` |
|         - | 10179 | `	{` |
|        18 | 10180 | `		sxu32 nListed = 0;` |
|        18 | 10181 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 10182 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 10183 | `			ph7_class *pOrigin = 0;` |
|         - | 10184 | `			SyString *pMName;` |
|        22 | 10185 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 10186 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10187 | `				continue;` |
|         - | 10188 | `			}` |
|        20 | 10189 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10190 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10191 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10192 | `			}` |
|        20 | 10193 | `			if( nListed > 0 ){` |
|         3 | 10194 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10195 | `			}` |
|         - | 10196 | `			/* Find the origin of this abstract method.` |
|         - | 10197 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10198 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10199 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10200 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10201 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10202 | `			 * class's namespace.` |
|         - | 10203 | `			 */` |
|         - | 10204 | `			{` |
|         - | 10205 | `				ph7_class **apIface;` |
|         - | 10206 | `				ph7_class **apTrait;` |
|         - | 10207 | `				ph7_class *pWalk;` |
|         - | 10208 | `				sxu32 i;` |
|         - | 10209 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10210 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10211 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10212 | `				 */` |
|        20 | 10213 | `				if( pClass->pBase ){` |
|        11 | 10214 | `					pWalk = pClass->pBase;` |
|        19 | 10215 | `					while( pWalk ){` |
|         - | 10216 | `						ph7_class_method *pParentMeth;` |
|        13 | 10217 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10218 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10219 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10220 | `							 * in this class's ancestor chain.` |
|         - | 10221 | `							 */` |
|        13 | 10222 | `							int fromIface = 0;` |
|        13 | 10223 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10224 | `							while( pAnc ){` |
|         - | 10225 | `								ph7_class **apPI;` |
|         - | 10226 | `								sxu32 j;` |
|        15 | 10227 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10228 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10229 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10230 | `										fromIface = 1;` |
|        10 | 10231 | `										break;` |
|         - | 10232 | `									}` |
|       ! 0 | 10233 | `								}` |
|        15 | 10234 | `								if( fromIface ) break;` |
|         6 | 10235 | `								pAnc = pAnc->pBase;` |
|         2 | 10236 | `							}` |
|        13 | 10237 | `							if( !fromIface ){` |
|         3 | 10238 | `								pOrigin = pWalk;` |
|         3 | 10239 | `								break;` |
|         - | 10240 | `							}` |
|         4 | 10241 | `						}` |
|        10 | 10242 | `						pWalk = pWalk->pBase;` |
|         2 | 10243 | `					}` |
|         4 | 10244 | `				}` |
|         - | 10245 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10246 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10247 | `				 */` |
|        20 | 10248 | `				if( !pOrigin ){` |
|        18 | 10249 | `					pWalk = pClass;` |
|        40 | 10250 | `					while( pWalk && !pOrigin ){` |
|        26 | 10251 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10252 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10253 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10254 | `							ph7_class *pDeepest = 0;` |
|        28 | 10255 | `							while( pIface ){` |
|        16 | 10256 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10257 | `									pDeepest = pIface;` |
|         6 | 10258 | `								}` |
|        16 | 10259 | `								pIface = pIface->pBase;` |
|         4 | 10260 | `							}` |
|        16 | 10261 | `							if( pDeepest ){` |
|        16 | 10262 | `								pOrigin = pDeepest;` |
|        16 | 10263 | `								break;` |
|         - | 10264 | `							}` |
|       ! 0 | 10265 | `						}` |
|        26 | 10266 | `						pWalk = pWalk->pBase;` |
|         4 | 10267 | `					}` |
|         7 | 10268 | `				}` |
|         - | 10269 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10270 | `				if( !pOrigin ){` |
|         3 | 10271 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10272 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10273 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10274 | `							pOrigin = pClass;` |
|         3 | 10275 | `							break;` |
|         - | 10276 | `						}` |
|       ! 0 | 10277 | `					}` |
|         1 | 10278 | `				}` |
|         - | 10279 | `			}` |
|        20 | 10280 | `			if( pOrigin ){` |
|        20 | 10281 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10282 | `			}else{` |
|         - | 10283 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10284 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10285 | `			}` |
|        20 | 10286 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10287 | `			nListed++;` |
|         4 | 10288 | `		}` |
|         - | 10289 | `	}` |
|        18 | 10290 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10291 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10292 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10293 | `	SyBlobRelease(&sMsg);` |
|        18 | 10294 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10295 | `		return SXERR_ABORT;` |
|         - | 10296 | `	}` |
|        18 | 10297 | `	return SXRET_OK;` |
|    176425 | 10298 | `}` |
|         - | 10299 | `/*` |
|         - | 10300 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10301 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10302 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10303 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10304 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10305 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10306 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10307 | ` */` |
|    398950 | 10308 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10309 | `{` |
|    398955 | 10310 | `	int isAbsolute = 0;` |
|    398955 | 10311 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10312 | `	SyBlob sName;` |
|    398955 | 10313 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4415 | 10314 | `		isAbsolute = 1;` |
|      4415 | 10315 | `		pGen->pIn++;` |
|      2205 | 10316 | `	}` |
|    398955 | 10317 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         9 | 10318 | `		pGen->pIn = pStart;` |
|         9 | 10319 | `		return SXERR_INVALID;` |
|         - | 10320 | `	}` |
|    398949 | 10321 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    398949 | 10322 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    398949 | 10323 | `	pGen->pIn++;` |
|    598437 | 10324 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    199498 | 10325 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10326 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10327 | `		pGen->pIn++;` |
|        16 | 10328 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10329 | `		pGen->pIn++;` |
|         2 | 10330 | `	}` |
|    398949 | 10331 | `	if( isAbsolute ){` |
|      4413 | 10332 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2209 | 10333 | `	}else{` |
|         - | 10334 | `		SyString sRaw;` |
|    394541 | 10335 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    394541 | 10336 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10337 | `	}` |
|    398949 | 10338 | `	SyBlobRelease(&sName);` |
|    398949 | 10339 | `	return SXRET_OK;` |
|    199480 | 10340 | `}` |
|         - | 10341 | `/*` |
|         - | 10342 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10343 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10344 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10345 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10346 | ` * either direction cannot run unbounded.` |
|         - | 10347 | ` */` |
|         - | 10348 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    164294 | 10349 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10350 | `{` |
|         - | 10351 | `	ph7_class **apParent;` |
|         - | 10352 | `	sxu32 n;` |
|    427847 | 10353 | `	while( pInterface ){` |
|    271195 | 10354 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10355 | `			return FALSE;` |
|         - | 10356 | `		}` |
|    305559 | 10357 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     68728 | 10358 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7647 | 10359 | `			return TRUE;` |
|         - | 10360 | `		}` |
|    263553 | 10361 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    263553 | 10362 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10363 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10364 | `				return TRUE;` |
|         - | 10365 | `			}` |
|       ! 0 | 10366 | `		}` |
|    263553 | 10367 | `		pInterface = pInterface->pBase;` |
|    263553 | 10368 | `		iDepth++;` |
|         5 | 10369 | `	}` |
|    156657 | 10370 | `	return FALSE;` |
|     82152 | 10371 | `}` |
|    164294 | 10372 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10373 | `{` |
|    164299 | 10374 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10375 | `}` |
|         - | 10376 | `/*` |
|         - | 10377 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10378 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10379 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10380 | ` */` |
|      7642 | 10381 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10382 | `{` |
|      7651 | 10383 | `	while( pBase ){` |
|        10 | 10384 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10385 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10386 | `			return TRUE;` |
|         - | 10387 | `		}` |
|        10 | 10388 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10389 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10390 | `			return TRUE;` |
|         - | 10391 | `		}` |
|         5 | 10392 | `		pBase = pBase->pBase;` |
|         1 | 10393 | `	}` |
|      7643 | 10394 | `	return FALSE;` |
|      3826 | 10395 | `}` |
|         - | 10396 | `/*` |
|         - | 10397 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10398 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10399 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10400 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10401 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10402 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10403 | ` * pClass->aEnumCases for cases().` |
|         - | 10404 | ` */` |
|      7674 | 10405 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10406 | `{` |
|      7679 | 10407 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10408 | `	SySet *pInstrContainer;` |
|         - | 10409 | `	ph7_class_attr *pCase;` |
|         - | 10410 | `	SyString *pName;` |
|         - | 10411 | `	sxi32 rc;` |
|      7679 | 10412 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7679 | 10413 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10414 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10415 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10416 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10417 | `			return SXERR_ABORT;` |
|         - | 10418 | `		}` |
|       ! 0 | 10419 | `		goto Synchronize;` |
|         - | 10420 | `	}` |
|      7679 | 10421 | `	pName = &pGen->pIn->sData;` |
|         - | 10422 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7679 | 10423 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10424 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10425 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10426 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10427 | `			return SXERR_ABORT;` |
|         - | 10428 | `		}` |
|       ! 0 | 10429 | `		goto Synchronize;` |
|         - | 10430 | `	}` |
|      7679 | 10431 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10432 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7679 | 10433 | `	if( pCase == 0 ){` |
|       ! 0 | 10434 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10435 | `		return SXERR_ABORT;` |
|         - | 10436 | `	}` |
|      7679 | 10437 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7679 | 10438 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10439 | `		return SXERR_ABORT;` |
|         - | 10440 | `	}` |
|      7679 | 10441 | `	pGen->pIn++; /* Jump the case name */` |
|      7679 | 10442 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7665 | 10443 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10444 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10445 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10446 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10447 | `				return SXERR_ABORT;` |
|         - | 10448 | `			}` |
|         6 | 10449 | `			goto Synchronize;` |
|         - | 10450 | `		}` |
|      7661 | 10451 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10452 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10453 | `		 * (same technique as class constants). */` |
|      7661 | 10454 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7661 | 10455 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7661 | 10456 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7661 | 10457 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10458 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10459 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10460 | `		}` |
|      7661 | 10461 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7661 | 10462 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7661 | 10463 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10464 | `			return SXERR_ABORT;` |
|         - | 10465 | `		}` |
|      3833 | 10466 | `	}else{` |
|        17 | 10467 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10468 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10469 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10470 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10471 | `				return SXERR_ABORT;` |
|         - | 10472 | `			}` |
|       ! 0 | 10473 | `			goto Synchronize;` |
|         - | 10474 | `		}` |
|         - | 10475 | `	}` |
|      7675 | 10476 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7675 | 10477 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10478 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10479 | `		return SXERR_ABORT;` |
|         - | 10480 | `	}` |
|      7675 | 10481 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7675 | 10482 | `	return SXRET_OK;` |
|         2 | 10483 | `Synchronize:` |
|         - | 10484 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10485 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10486 | `		pGen->pIn++;` |
|         2 | 10487 | `	}` |
|         6 | 10488 | `	return SXERR_CORRUPT;` |
|      3842 | 10489 | `}` |
|         - | 10490 | `/*` |
|         - | 10491 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10492 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10493 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10494 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10495 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10496 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10497 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10498 | ` */` |
|      3840 | 10499 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10500 | `{` |
|         - | 10501 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10502 | `	const char *zBack;` |
|         - | 10503 | `	SySet sToken;` |
|         - | 10504 | `	char *zSrc;` |
|         - | 10505 | `	sxu32 nSrc,nMax;` |
|      3845 | 10506 | `	sxi32 rc = SXRET_OK;` |
|      3845 | 10507 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3840 | 10508 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3845 | 10509 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3845 | 10510 | `	if( zSrc == 0 ){` |
|       ! 0 | 10511 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10512 | `		return SXERR_ABORT;` |
|         - | 10513 | `	}` |
|      3845 | 10514 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3845 | 10515 | `	if( pClass->nEnumBacking != 0 ){` |
|      5747 | 10516 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10517 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10518 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10519 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1914 | 10520 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1919 | 10521 | `	}else{` |
|        21 | 10522 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10523 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10524 | `	}` |
|      3845 | 10525 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3845 | 10526 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3845 | 10527 | `	pSaveIn = pGen->pIn;` |
|      3845 | 10528 | `	pSaveEnd = pGen->pEnd;` |
|      3845 | 10529 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3845 | 10530 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15341 | 10531 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11501 | 10532 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10533 | `	}` |
|      3845 | 10534 | `	pGen->pIn = pSaveIn;` |
|      3845 | 10535 | `	pGen->pEnd = pSaveEnd;` |
|      3845 | 10536 | `	SySetRelease(&sToken);` |
|      3845 | 10537 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1925 | 10538 | `}` |
|         - | 10539 | `/*` |
|         - | 10540 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10541 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10542 | ` */` |
|         - | 10543 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10544 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10545 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10546 | `};` |
|         - | 10547 | `/*` |
|         - | 10548 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10549 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10550 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10551 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10552 | ` * and before the class is installed.` |
|         - | 10553 | ` */` |
|      3840 | 10554 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10555 | `{` |
|         - | 10556 | `	SyHashEntry *pEntry;` |
|         - | 10557 | `	sxi32 rc;` |
|         - | 10558 | `	sxu32 n;` |
|         - | 10559 | `	/* php: "Enum %s cannot include properties" */` |
|      3845 | 10560 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11519 | 10561 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7681 | 10562 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7681 | 10563 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10564 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10565 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10566 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10567 | `				return SXERR_ABORT;` |
|         - | 10568 | `			}` |
|         3 | 10569 | `			break;` |
|         - | 10570 | `		}` |
|         5 | 10571 | `	}` |
|         - | 10572 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     53765 | 10573 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     74880 | 10574 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     49925 | 10575 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10576 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10577 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10578 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10579 | `				return SXERR_ABORT;` |
|         - | 10580 | `			}` |
|       ! 0 | 10581 | `		}` |
|     24965 | 10582 | `	}` |
|         - | 10583 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10584 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10585 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10586 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10587 | `	{` |
|         - | 10588 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10589 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10590 | `		ph7_class_attr *pAttr;` |
|      3845 | 10591 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10592 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3845 | 10593 | `		if( pAttr == 0 ){` |
|       ! 0 | 10594 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10595 | `			return SXERR_ABORT;` |
|         - | 10596 | `		}` |
|      3845 | 10597 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3845 | 10598 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3845 | 10599 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3845 | 10600 | `		if( pClass->nEnumBacking != 0 ){` |
|      3833 | 10601 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10602 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3833 | 10603 | `			if( pAttr == 0 ){` |
|       ! 0 | 10604 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10605 | `				return SXERR_ABORT;` |
|         - | 10606 | `			}` |
|      3833 | 10607 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3833 | 10608 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10609 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10610 | `			}else{` |
|      3827 | 10611 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10612 | `			}` |
|      3833 | 10613 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1914 | 10614 | `		}` |
|         - | 10615 | `	}` |
|      3845 | 10616 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1925 | 10617 | `}` |
|         - | 10618 | `/*` |
|         - | 10619 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10620 | ` *` |
|         - | 10621 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10622 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10623 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10624 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10625 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10626 | ` * implements, body, install) is shared by both paths.` |
|         - | 10627 | ` */` |
|    352884 | 10628 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10629 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10630 | `{` |
|    352889 | 10631 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10632 | `	ph7_class *pClass,*pBase;` |
|         - | 10633 | `	SyToken *pEnd,*pTmp;` |
|         - | 10634 | `	sxi32 iProtection;` |
|         - | 10635 | `	SySet aInterfaces;` |
|         - | 10636 | `	SySet aUseEntries;` |
|         - | 10637 | `	sxi32 iAttrflags;` |
|         - | 10638 | `	SyString *pName;` |
|         - | 10639 | `	sxi32 nKwrd;` |
|         - | 10640 | `	sxi32 rc;` |
|         - | 10641 | `	/* Jump the 'class' keyword */` |
|    352889 | 10642 | `	pGen->pIn++;` |
|    352889 | 10643 | `	if( pAnonName ){` |
|         - | 10644 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10645 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10646 | `		 * then use the synthesized name. */` |
|        32 | 10647 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10648 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10649 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10650 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10651 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10652 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10653 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10654 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10655 | `		}` |
|        32 | 10656 | `		pName = pAnonName;` |
|        32 | 10657 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10658 | `	}else{` |
|    352861 | 10659 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10660 | `			/* Syntax error */` |
|       ! 0 | 10661 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10662 | `			if( rc == SXERR_ABORT ){` |
|         - | 10663 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10664 | `				return SXERR_ABORT;` |
|         - | 10665 | `			}` |
|         - | 10666 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10667 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10668 | `				pGen->pIn++;` |
|       ! 0 | 10669 | `			}` |
|       ! 0 | 10670 | `			return SXRET_OK;` |
|         - | 10671 | `		}` |
|         - | 10672 | `		/* Extract class name */` |
|    352861 | 10673 | `		pName = &pGen->pIn->sData;` |
|         - | 10674 | `		/* Advance the stream cursor */` |
|    352861 | 10675 | `		pGen->pIn++;` |
|         - | 10676 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10677 | `			SyBlob sFQN;` |
|         - | 10678 | `			SyString sFQNStr;` |
|    352861 | 10679 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    352861 | 10680 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    352861 | 10681 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    352861 | 10682 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    352861 | 10683 | `			SyBlobRelease(&sFQN);` |
|         - | 10684 | `		}` |
|         - | 10685 | `	}` |
|    352889 | 10686 | `	if( pClass == 0 ){` |
|       ! 0 | 10687 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10688 | `		return SXERR_ABORT;` |
|         - | 10689 | `	}` |
|    352884 | 10690 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3849 | 10691 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10692 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3835 | 10693 | `		pGen->pIn++; /* Jump ':' */` |
|      3830 | 10694 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3835 | 10695 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10696 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10697 | `			pGen->pIn++;` |
|      3828 | 10698 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3829 | 10699 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3827 | 10700 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3827 | 10701 | `			pGen->pIn++;` |
|      1916 | 10702 | `		}else{` |
|         3 | 10703 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10704 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10705 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10706 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10707 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10708 | `				return SXERR_ABORT;` |
|         - | 10709 | `			}` |
|         3 | 10710 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10711 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10712 | `			}` |
|         - | 10713 | `		}` |
|      1915 | 10714 | `	}` |
|    352889 | 10715 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    352889 | 10716 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10717 | `		return SXERR_ABORT;` |
|         - | 10718 | `	}` |
|         - | 10719 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    352889 | 10720 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    352889 | 10721 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10722 | `	/* Assume a standalone class */` |
|    352889 | 10723 | `	pBase = 0;` |
|    352889 | 10724 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    286645 | 10725 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    286645 | 10726 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10727 | `			SyBlob sResolved;` |
|         - | 10728 | `			SyString sBaseName;` |
|         - | 10729 | `			sxu32 nRefLine;` |
|    183435 | 10730 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10731 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10732 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10733 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10734 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10735 | `					return SXERR_ABORT;` |
|         - | 10736 | `				}` |
|       ! 0 | 10737 | `			}` |
|    183435 | 10738 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    183435 | 10739 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    183435 | 10740 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    183435 | 10741 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10742 | `				SyBlobRelease(&sResolved);` |
|         4 | 10743 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10744 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10745 | `					pName);` |
|         3 | 10746 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10747 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10748 | `					return SXERR_ABORT;` |
|         - | 10749 | `				}` |
|         3 | 10750 | `				return SXRET_OK;` |
|         - | 10751 | `			}` |
|    275147 | 10752 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    183428 | 10753 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    183433 | 10754 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10755 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10756 | `			/* Interfaces are not allowed */` |
|    183433 | 10757 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10758 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10759 | `			}` |
|    183433 | 10760 | `			if( pBase == 0 ){` |
|       ! 0 | 10761 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10762 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10763 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10764 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10765 | `					return SXERR_ABORT;` |
|         - | 10766 | `				}` |
|       ! 0 | 10767 | `			}else{` |
|    183433 | 10768 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10769 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10770 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10771 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10772 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10773 | `						return SXERR_ABORT;` |
|         - | 10774 | `					}` |
|         3 | 10775 | `					pBase = 0; /* Never inherit from an enum */` |
|    183432 | 10776 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10777 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10778 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10779 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10780 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10781 | `						return SXERR_ABORT;` |
|         - | 10782 | `					}` |
|       ! 0 | 10783 | `				}` |
|         - | 10784 | `			}` |
|    183433 | 10785 | `			SyBlobRelease(&sResolved);` |
|    183433 | 10786 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10787 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10788 | `			}` |
|     91714 | 10789 | `		}` |
|    286643 | 10790 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10791 | `			ph7_class *pInterface;` |
|         - | 10792 | `			/* Interface implementation */` |
|    107045 | 10793 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    110774 | 10794 | `			for(;;){` |
|         - | 10795 | `				SyBlob sResolved;` |
|         - | 10796 | `				SyString sIntName;` |
|         - | 10797 | `				sxu32 nRefLine;` |
|    164299 | 10798 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    164299 | 10799 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    164299 | 10800 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10801 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10802 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10803 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10804 | `						pName);` |
|       ! 0 | 10805 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10806 | `						return SXERR_ABORT;` |
|         - | 10807 | `					}` |
|       ! 0 | 10808 | `					break;` |
|         - | 10809 | `				}` |
|    328593 | 10810 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    164294 | 10811 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    164299 | 10812 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10813 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10814 | `				/* Only interfaces are allowed */` |
|    164299 | 10815 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10816 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10817 | `				}` |
|    164299 | 10818 | `				if( pInterface == 0 ){` |
|       ! 0 | 10819 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10820 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10821 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10822 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10823 | `						return SXERR_ABORT;` |
|         - | 10824 | `					}` |
|       ! 0 | 10825 | `				}else{` |
|         - | 10826 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10827 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10828 | `					 * unless they already extend Exception or Error.` |
|         - | 10829 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10830 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10831 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    164299 | 10832 | `					SyString *pFqn = &pClass->sName;` |
|    164299 | 10833 | `					int bIsExceptionOrError =` |
|     85967 | 10834 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    248353 | 10835 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    162393 | 10836 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3830 | 10837 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    168115 | 10838 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11466 | 10839 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3819 | 10840 | `						!bIsExceptionOrError ){` |
|        12 | 10841 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10842 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10843 | `							&pClass->sName);` |
|         9 | 10844 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10845 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10846 | `							return SXERR_ABORT;` |
|         - | 10847 | `						}` |
|         - | 10848 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10849 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10850 | `					}else{` |
|    164293 | 10851 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10852 | `					}` |
|         - | 10853 | `				}` |
|    164299 | 10854 | `				SyBlobRelease(&sResolved);` |
|    164299 | 10855 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53525 | 10856 | `					break;` |
|         - | 10857 | `				}` |
|     57259 | 10858 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10859 | `			}` |
|     53520 | 10860 | `		}` |
|    143319 | 10861 | `	}` |
|    352887 | 10862 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10863 | `		/* Syntax error */` |
|       ! 0 | 10864 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10865 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10866 | `		if( rc == SXERR_ABORT ){` |
|         - | 10867 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10868 | `			return SXERR_ABORT;` |
|         - | 10869 | `		}` |
|       ! 0 | 10870 | `		return SXRET_OK;` |
|         - | 10871 | `	}` |
|    352887 | 10872 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    352887 | 10873 | `	pEnd = 0; /* cc warning */` |
|         - | 10874 | `	/* Delimit the class body */` |
|    352887 | 10875 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    352887 | 10876 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10877 | `		/* Syntax error */` |
|       ! 0 | 10878 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10879 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10880 | `		if( rc == SXERR_ABORT ){` |
|         - | 10881 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10882 | `			return SXERR_ABORT;` |
|         - | 10883 | `		}` |
|       ! 0 | 10884 | `		return SXRET_OK;` |
|         - | 10885 | `	}` |
|         - | 10886 | `	/* The delimiter token is the class body's closing brace */` |
|    352887 | 10887 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10888 | `	/* Swap token stream */` |
|    352887 | 10889 | `	pTmp = pGen->pEnd;` |
|    352887 | 10890 | `	pGen->pEnd = pEnd;` |
|         - | 10891 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    352887 | 10892 | `	pClass->iFlags \|= iFlags;` |
|         - | 10893 | `	/* Start the parse process */` |
|   1369748 | 10894 | `	for(;;){` |
|         - | 10895 | `		/* Jump leading/trailing semi-colons */` |
|   3901775 | 10896 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    703287 | 10897 | `			pGen->pIn++;` |
|         5 | 10898 | `		}` |
|   3198493 | 10899 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10900 | `			/* End of class body */` |
|    352845 | 10901 | `			break;` |
|         - | 10902 | `		}` |
|         - | 10903 | `		/* Bind a directly-preceding docblock to this member */` |
|   2845653 | 10904 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2845648 | 10905 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1422829 | 10906 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10907 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10908 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10909 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10910 | `			if( rc == SXERR_ABORT ){` |
|         - | 10911 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10912 | `				return SXERR_ABORT;` |
|         - | 10913 | `			}` |
|       ! 0 | 10914 | `			goto done;` |
|         - | 10915 | `		}` |
|         - | 10916 | `		/* Assume public visibility */` |
|   2845653 | 10917 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2845653 | 10918 | `		iAttrflags = 0;` |
|         - | 10919 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10920 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10921 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10922 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2845653 | 10923 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10924 | `			int bMod = 0;` |
|       ! 0 | 10925 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10926 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10927 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10928 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10929 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10930 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10931 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10932 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10933 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10934 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10935 | `			}` |
|       ! 0 | 10936 | `			if( !bMod ){` |
|       ! 0 | 10937 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10938 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10939 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10940 | `						return SXERR_ABORT;` |
|         - | 10941 | `					}` |
|       ! 0 | 10942 | `					goto done;` |
|         - | 10943 | `				}` |
|       ! 0 | 10944 | `				continue;` |
|         - | 10945 | `			}` |
|       ! 0 | 10946 | `		}` |
|   2845653 | 10947 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10948 | `			/* Extract the current keyword */` |
|   2845653 | 10949 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2845653 | 10950 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10951 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7679 | 10952 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7679 | 10953 | `				if( rc != SXRET_OK ){` |
|         6 | 10954 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10955 | `						return SXERR_ABORT;` |
|         - | 10956 | `					}` |
|         6 | 10957 | `					goto done;` |
|         - | 10958 | `				}` |
|      7675 | 10959 | `				continue;` |
|         - | 10960 | `			}` |
|   2837979 | 10961 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 10962 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 10963 | `				TraitUseEntry sUse;` |
|     15333 | 10964 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15333 | 10965 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15333 | 10966 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7672 | 10967 | `				for(;;){` |
|         - | 10968 | `					ph7_class *pTrait;` |
|         - | 10969 | `					SyString *pTraitName;` |
|     15341 | 10970 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 10971 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10972 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 10973 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10974 | `							return SXERR_ABORT;` |
|         - | 10975 | `						}` |
|       ! 0 | 10976 | `						break;` |
|         - | 10977 | `					}` |
|     15341 | 10978 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 10979 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 10980 | `						SyBlob sResolved;` |
|     15341 | 10981 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15341 | 10982 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     30677 | 10983 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15336 | 10984 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15341 | 10985 | `						SyBlobRelease(&sResolved);` |
|         - | 10986 | `					}` |
|         - | 10987 | `					/* Only traits are allowed */` |
|     15341 | 10988 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 10989 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 10990 | `					}` |
|     15341 | 10991 | `					if( pTrait == 0 ){` |
|       ! 0 | 10992 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10993 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 10994 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10995 | `							return SXERR_ABORT;` |
|         - | 10996 | `						}` |
|       ! 0 | 10997 | `					}else{` |
|     15341 | 10998 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 10999 | `					}` |
|     15341 | 11000 | `					pGen->pIn++; /* Advance past trait name */` |
|     15341 | 11001 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7669 | 11002 | `						break;` |
|         - | 11003 | `					}` |
|        10 | 11004 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 11005 | `				}` |
|         - | 11006 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15333 | 11007 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 11008 | `					SyToken *pBlock;` |
|        13 | 11009 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 11010 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 11011 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 11012 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 11013 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 11014 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 11015 | `					}else{` |
|       ! 0 | 11016 | `						pGen->pIn = pGen->pEnd;` |
|         - | 11017 | `					}` |
|         5 | 11018 | `				}` |
|     15333 | 11019 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 11020 | `				/* The semicolon will be consumed by the outer loop */` |
|     15333 | 11021 | `				continue;` |
|         - | 11022 | `			}` |
|   2822651 | 11023 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 11024 | `				int nSetTok;` |
|   2577929 | 11025 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2577929 | 11026 | `				if( nSetVis ){` |
|         - | 11027 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 11028 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 11029 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11030 | `					pGen->pIn += nSetTok;` |
|         2 | 11031 | `				}else{` |
|   2577927 | 11032 | `					iProtection = nKwrd;` |
|   2577927 | 11033 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 11034 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 11035 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2577927 | 11036 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2577927 | 11037 | `					if( nSetVis ){` |
|         9 | 11038 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 11039 | `						pGen->pIn += nSetTok;` |
|         4 | 11040 | `					}` |
|         - | 11041 | `				}` |
|         - | 11042 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 11043 | ``				 * `public private(set) readonly int $x`. */`` |
|   2577929 | 11044 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 11045 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 11046 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 11047 | `				}` |
|   2577924 | 11048 | `				if( pGen->pIn >= pGen->pEnd` |
|   2577929 | 11049 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11050 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11051 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11052 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11053 | `					if( rc == SXERR_ABORT ){` |
|         - | 11054 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 11055 | `						return SXERR_ABORT;` |
|         - | 11056 | `					}` |
|       ! 0 | 11057 | `					goto done;` |
|         - | 11058 | `				}` |
|   2577929 | 11059 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11060 | `					/* Attribute declaration (untyped) */` |
|    408937 | 11061 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    408937 | 11062 | `					if( rc != SXRET_OK ){` |
|        11 | 11063 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11064 | `							return SXERR_ABORT;` |
|         - | 11065 | `						}` |
|        11 | 11066 | `						goto done;` |
|         - | 11067 | `					}` |
|    409078 | 11068 | `					continue;` |
|         - | 11069 | `				}` |
|   2168997 | 11070 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11071 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       309 | 11072 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       309 | 11073 | `					if( rc != SXRET_OK ){` |
|         8 | 11074 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11075 | `							return SXERR_ABORT;` |
|         - | 11076 | `						}` |
|         8 | 11077 | `						goto done;` |
|         - | 11078 | `					}` |
|       303 | 11079 | `					continue;` |
|         - | 11080 | `				}` |
|         - | 11081 | `				/* Extract the keyword */` |
|   2168693 | 11082 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1084344 | 11083 | `			}` |
|   2413415 | 11084 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 11085 | `				/* Process constant declaration */` |
|    236751 | 11086 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    236751 | 11087 | `				if( rc != SXRET_OK ){` |
|        11 | 11088 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11089 | `						return SXERR_ABORT;` |
|         - | 11090 | `					}` |
|        11 | 11091 | `					goto done;` |
|         - | 11092 | `				}` |
|    118374 | 11093 | `			}else{` |
|   2176669 | 11094 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 11095 | `					/* Static method or attribute,record that */` |
|     95557 | 11096 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     95557 | 11097 | `					pGen->pIn++; /* Jump the static keyword */` |
|     95557 | 11098 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11099 | `						int nSetTok;` |
|     68811 | 11100 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     68811 | 11101 | `						if( nSetVis ){` |
|         - | 11102 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 11103 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11104 | `							pGen->pIn += nSetTok;` |
|         2 | 11105 | `						}else{` |
|         - | 11106 | `							/* Extract the keyword */` |
|     68809 | 11107 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     68809 | 11108 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11109 | `								iProtection = nKwrd;` |
|       ! 0 | 11110 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 11111 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 11112 | `								if( nSetVis ){` |
|       ! 0 | 11113 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 11114 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 11115 | `								}` |
|       ! 0 | 11116 | `							}` |
|         - | 11117 | `						}` |
|     34403 | 11118 | `					}` |
|         - | 11119 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 11120 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 11121 | `					 * than a generic "expecting method" parse error. */` |
|     95557 | 11122 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11123 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11124 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 11125 | `					}` |
|     95552 | 11126 | `					if( pGen->pIn >= pGen->pEnd` |
|     95557 | 11127 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11128 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11129 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 11130 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11131 | `						if( rc == SXERR_ABORT ){` |
|         - | 11132 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11133 | `							return SXERR_ABORT;` |
|         - | 11134 | `						}` |
|       ! 0 | 11135 | `						goto done;` |
|         - | 11136 | `					}` |
|     95557 | 11137 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11138 | `						/* Attribute declaration */` |
|     26747 | 11139 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26747 | 11140 | `						if( rc != SXRET_OK ){` |
|         3 | 11141 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11142 | `								return SXERR_ABORT;` |
|         - | 11143 | `							}` |
|         3 | 11144 | `							goto done;` |
|         - | 11145 | `						}` |
|     26745 | 11146 | `						continue;` |
|         - | 11147 | `					}` |
|     68815 | 11148 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11149 | `						/* Typed static attribute declaration */` |
|        19 | 11150 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        19 | 11151 | `						if( rc != SXRET_OK ){` |
|         3 | 11152 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11153 | `								return SXERR_ABORT;` |
|         - | 11154 | `							}` |
|         3 | 11155 | `							goto done;` |
|         - | 11156 | `						}` |
|        17 | 11157 | `						continue;` |
|         - | 11158 | `					}` |
|         - | 11159 | `					/* Extract the keyword */` |
|     68799 | 11160 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2115514 | 11161 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 11162 | `					/* Abstract method,record that */` |
|      7657 | 11163 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 11164 | `					/* Mark the whole class as abstract */` |
|      7657 | 11165 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 11166 | `					/* Advance the stream cursor */` |
|      7657 | 11167 | `					pGen->pIn++;` |
|      7657 | 11168 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7657 | 11169 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7657 | 11170 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7655 | 11171 | `							iProtection = nKwrd;` |
|      7655 | 11172 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3825 | 11173 | `						}` |
|      3826 | 11174 | `					}` |
|      7657 | 11175 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7652 | 11176 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11177 | `							/* Static method */` |
|       ! 0 | 11178 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11179 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11180 | `					}` |
|      7657 | 11181 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7652 | 11182 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 11183 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 11184 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 11185 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 11186 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11187 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11188 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11189 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11190 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11191 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11192 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11193 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11194 | `										return SXERR_ABORT;` |
|         - | 11195 | `									}` |
|       ! 0 | 11196 | `									goto done;` |
|         - | 11197 | `								}` |
|         7 | 11198 | `								continue;` |
|         - | 11199 | `							}` |
|       ! 0 | 11200 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11201 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11202 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11203 | `							if( rc == SXERR_ABORT ){` |
|         - | 11204 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11205 | `								return SXERR_ABORT;` |
|         - | 11206 | `							}` |
|       ! 0 | 11207 | `							goto done;` |
|         - | 11208 | `					}` |
|      7651 | 11209 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2077288 | 11210 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11211 | `					/* final method ,record that */` |
|        20 | 11212 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        20 | 11213 | `					pGen->pIn++; /* Jump the final keyword */` |
|        20 | 11214 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11215 | `						/* Extract the keyword */` |
|        20 | 11216 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        20 | 11217 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        10 | 11218 | `							iProtection = nKwrd;` |
|        10 | 11219 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11220 | `						}` |
|         9 | 11221 | `					}` |
|        20 | 11222 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11223 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11224 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11225 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11226 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11227 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11228 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11229 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11230 | `									return SXERR_ABORT;` |
|         - | 11231 | `								}` |
|       ! 0 | 11232 | `								goto done;` |
|         - | 11233 | `							}` |
|        14 | 11234 | `							continue;` |
|         - | 11235 | `					}` |
|         8 | 11236 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11237 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11238 | `							/* Static method */` |
|       ! 0 | 11239 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11240 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11241 | `					}` |
|         8 | 11242 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11243 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11244 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11245 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11246 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11247 | `							if( rc == SXERR_ABORT ){` |
|         - | 11248 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11249 | `								return SXERR_ABORT;` |
|         - | 11250 | `							}` |
|       ! 0 | 11251 | `							goto done;` |
|         - | 11252 | `					}` |
|         8 | 11253 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11254 | `				}` |
|   2149893 | 11255 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11256 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11257 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11258 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11259 | `						if( rc == SXERR_ABORT ){` |
|         - | 11260 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11261 | `							return SXERR_ABORT;` |
|         - | 11262 | `						}` |
|       ! 0 | 11263 | `						goto done;` |
|         - | 11264 | `				}` |
|   2149893 | 11265 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11266 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11267 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11268 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11269 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11270 | `						if( rc == SXERR_ABORT ){` |
|         - | 11271 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11272 | `							return SXERR_ABORT;` |
|         - | 11273 | `						}` |
|       ! 0 | 11274 | `						goto done;` |
|         - | 11275 | `					}` |
|         - | 11276 | `					/* Attribute declaration */` |
|         7 | 11277 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11278 | `				}else{` |
|         - | 11279 | `					/* Process method declaration */` |
|   2149887 | 11280 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11281 | `				}` |
|   2149893 | 11282 | `				if( rc != SXRET_OK ){` |
|        16 | 11283 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11284 | `						return SXERR_ABORT;` |
|         - | 11285 | `					}` |
|        16 | 11286 | `					goto done;` |
|         - | 11287 | `				}` |
|         - | 11288 | `			}` |
|   1193312 | 11289 | `		}else{` |
|         - | 11290 | `			/* Attribute declaration */` |
|       ! 0 | 11291 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11292 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11293 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11294 | `					return SXERR_ABORT;` |
|         - | 11295 | `				}` |
|       ! 0 | 11296 | `				goto done;` |
|         - | 11297 | `			}` |
|         - | 11298 | `		}` |
|         5 | 11299 | `	}` |
|         - | 11300 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11301 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11302 | `	 */` |
|         - | 11303 | `	{` |
|         - | 11304 | `		TraitUseEntry *apUse;` |
|         - | 11305 | `		sxu32 nU;` |
|    352845 | 11306 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    368173 | 11307 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15333 | 11308 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15333 | 11309 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15333 | 11310 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15333 | 11311 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11312 | `			sxu32 nT;` |
|     15333 | 11313 | `			if( !hasResolution ){` |
|         - | 11314 | `				/* No conflict resolution block: use standard trait application */` |
|     30647 | 11315 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15329 | 11316 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15329 | 11317 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11318 | `						break;` |
|         - | 11319 | `					}` |
|      7667 | 11320 | `				}` |
|      7664 | 11321 | `			}else{` |
|         - | 11322 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11323 | `				 * then use the block to resolve method conflicts.` |
|         - | 11324 | `				 */` |
|         - | 11325 | `				SyToken *pR;` |
|        25 | 11326 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11327 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11328 | `					ph7_class_attr *pAR;` |
|         - | 11329 | `					SyHashEntry *pER;` |
|         - | 11330 | `					SyString *pNR;` |
|        15 | 11331 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11332 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11333 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11334 | `						pNR = &pAR->sName;` |
|       ! 0 | 11335 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11336 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11337 | `						}` |
|       ! 0 | 11338 | `					}` |
|        15 | 11339 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11340 | `				}` |
|         - | 11341 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11342 | `				pR = pUse->pResolvStart;` |
|        27 | 11343 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11344 | `					SyString sTrait,sMethod;` |
|         - | 11345 | `					ph7_class *pSrcTrait;` |
|         - | 11346 | `					ph7_class_method *pMeth;` |
|         - | 11347 | `					sxi32 nRKwrd;` |
|        41 | 11348 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11349 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11350 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11351 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11352 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11353 | `					sMethod = pR->sData;` |
|        17 | 11354 | `					pR++;` |
|        17 | 11355 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11356 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11357 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11358 | `							sTrait = sMethod;` |
|         7 | 11359 | `							pR++;` |
|         7 | 11360 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11361 | `							sMethod = pR->sData;` |
|         7 | 11362 | `							pR++;` |
|         3 | 11363 | `						}` |
|         3 | 11364 | `					}` |
|        17 | 11365 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11366 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11367 | `						continue;` |
|         - | 11368 | `					}` |
|        17 | 11369 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11370 | `					pR++;` |
|        17 | 11371 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11372 | `						pSrcTrait = 0;` |
|         7 | 11373 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11374 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11375 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11376 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11377 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11378 | `								break;` |
|         - | 11379 | `							}` |
|         2 | 11380 | `						}` |
|         5 | 11381 | `						if( pSrcTrait ){` |
|         5 | 11382 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11383 | `							if( pMeth ){` |
|         5 | 11384 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11385 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11386 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11387 | `								}` |
|         2 | 11388 | `							}` |
|         2 | 11389 | `						}` |
|         2 | 11390 | `					}` |
|        35 | 11391 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11392 | `				}` |
|         - | 11393 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11394 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11395 | `					ph7_class_method *pMR;` |
|         - | 11396 | `					SyHashEntry *pER;` |
|         - | 11397 | `					SyString *pNR;` |
|        15 | 11398 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11399 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11400 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11401 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11402 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11403 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11404 | `						}` |
|         3 | 11405 | `					}` |
|         9 | 11406 | `				}` |
|         - | 11407 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11408 | `				pR = pUse->pResolvStart;` |
|        27 | 11409 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11410 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11411 | `					ph7_class *pSrcTrait;` |
|         - | 11412 | `					ph7_class_method *pMeth;` |
|        27 | 11413 | `					int hasQual = 0;` |
|         - | 11414 | `					sxi32 nRKwrd;` |
|        41 | 11415 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11416 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11417 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11418 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11419 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11420 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11421 | `					sMethod = pR->sData;` |
|        17 | 11422 | `					pR++;` |
|        17 | 11423 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11424 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11425 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11426 | `							sTrait = sMethod;` |
|         7 | 11427 | `							hasQual = 1;` |
|         7 | 11428 | `							pR++;` |
|         7 | 11429 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11430 | `							sMethod = pR->sData;` |
|         7 | 11431 | `							pR++;` |
|         3 | 11432 | `						}` |
|         3 | 11433 | `					}` |
|        17 | 11434 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11435 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11436 | `						continue;` |
|         - | 11437 | `					}` |
|        17 | 11438 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11439 | `					pR++;` |
|        17 | 11440 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11441 | `						sxi32 iNewVis = -1;` |
|        13 | 11442 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11443 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11444 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11445 | `								iNewVis = nAK;` |
|         7 | 11446 | `								pR++;` |
|         3 | 11447 | `							}` |
|         3 | 11448 | `						}` |
|        13 | 11449 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11450 | `							sAlias = pR->sData;` |
|        11 | 11451 | `							pR++;` |
|         4 | 11452 | `						}` |
|        13 | 11453 | `						pMeth = 0;` |
|        13 | 11454 | `						if( hasQual ){` |
|         3 | 11455 | `							pSrcTrait = 0;` |
|         5 | 11456 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11457 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11458 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11459 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11460 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11461 | `									break;` |
|         - | 11462 | `								}` |
|         2 | 11463 | `							}` |
|         3 | 11464 | `							if( pSrcTrait ){` |
|         3 | 11465 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11466 | `							}` |
|         2 | 11467 | `						}else{` |
|        10 | 11468 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11469 | `						}` |
|        13 | 11470 | `						if( pMeth ){` |
|        13 | 11471 | `							if( sAlias.nByte > 0 ){` |
|         - | 11472 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11473 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11474 | `								 */` |
|         - | 11475 | `								ph7_class_method *pAlias;` |
|         - | 11476 | `								char *zAliasDup;` |
|        11 | 11477 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11478 | `								if( pAlias ){` |
|        11 | 11479 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11480 | `									if( iNewVis >= 0 ){` |
|         5 | 11481 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11482 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11483 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11484 | `									}` |
|        11 | 11485 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11486 | `									if( zAliasDup ){` |
|        11 | 11487 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11488 | `									}` |
|         7 | 11489 | `								}` |
|         7 | 11490 | `							}else if( iNewVis >= 0 ){` |
|         - | 11491 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11492 | `								ph7_class_method *pCopy;` |
|         3 | 11493 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11494 | `								if( pCopy ){` |
|         3 | 11495 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11496 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11497 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11498 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11499 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11500 | `									/* Replace the method in the class hash */` |
|         3 | 11501 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11502 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11503 | `								}` |
|         1 | 11504 | `							}` |
|         5 | 11505 | `						}` |
|         5 | 11506 | `						SXUNUSED(hasQual);` |
|         5 | 11507 | `					}` |
|        21 | 11508 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11509 | `				}` |
|         - | 11510 | `			}` |
|     15333 | 11511 | `			SySetRelease(&pUse->aTraits);` |
|      7669 | 11512 | `		}` |
|         - | 11513 | `	}` |
|    352845 | 11514 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11515 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11516 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3845 | 11517 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3845 | 11518 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11519 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11520 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11521 | `			return SXERR_ABORT;` |
|         - | 11522 | `		}` |
|      1920 | 11523 | `	}` |
|         - | 11524 | `	/* Install the class */` |
|    352845 | 11525 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    352845 | 11526 | `	if( rc == SXRET_OK ){` |
|         - | 11527 | `		ph7_class **apInterface;` |
|         - | 11528 | `		sxu32 n;` |
|    352845 | 11529 | `		if( pBase ){` |
|         - | 11530 | `			/* Inherit from base class and mark as a subclass */` |
|    183431 | 11531 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     91713 | 11532 | `		}` |
|    352845 | 11533 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    517133 | 11534 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11535 | `			/* Implements one or more interface */` |
|    164293 | 11536 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    164293 | 11537 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11538 | `				break;` |
|         - | 11539 | `			}` |
|     82149 | 11540 | `		}` |
|         - | 11541 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11542 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    352845 | 11543 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3845 | 11544 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3845 | 11545 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11546 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11547 | `			}` |
|      3845 | 11548 | `			if( pIntf ){` |
|      3845 | 11549 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1920 | 11550 | `			}` |
|      3845 | 11551 | `			if( pClass->nEnumBacking != 0 ){` |
|      3833 | 11552 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3833 | 11553 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11554 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11555 | `				}` |
|      3833 | 11556 | `				if( pIntf ){` |
|      3833 | 11557 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1914 | 11558 | `				}` |
|      1914 | 11559 | `			}` |
|      1920 | 11560 | `		}` |
|         - | 11561 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11562 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    352840 | 11563 | `		if( rc == SXRET_OK` |
|    352840 | 11564 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    352845 | 11565 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    187095 | 11566 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11567 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    187095 | 11568 | `			if( pStringable ){` |
|    187095 | 11569 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    187095 | 11570 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11571 | `				sxu32 i;` |
|    187095 | 11572 | `				int bAlready = 0;` |
|    225259 | 11573 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     41987 | 11574 | `					if( apImpl[i] == pStringable ){` |
|      3823 | 11575 | `						bAlready = 1;` |
|      3823 | 11576 | `						break;` |
|         - | 11577 | `					}` |
|     19087 | 11578 | `				}` |
|    187095 | 11579 | `				if( !bAlready ){` |
|    183277 | 11580 | `					PH7_ClassImplement(pClass,pStringable);` |
|     91636 | 11581 | `				}` |
|     93545 | 11582 | `			}` |
|     93545 | 11583 | `		}` |
|         - | 11584 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    352845 | 11585 | `		if( rc == SXRET_OK ){` |
|    352845 | 11586 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    352845 | 11587 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11588 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11589 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11590 | `				return SXERR_ABORT;` |
|         - | 11591 | `			}` |
|    176420 | 11592 | `		}` |
|         - | 11593 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    352845 | 11594 | `		if( rc == SXRET_OK ){` |
|    352845 | 11595 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    352845 | 11596 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11597 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11598 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11599 | `				return SXERR_ABORT;` |
|         - | 11600 | `			}` |
|    176420 | 11601 | `		}` |
|    176420 | 11602 | `	}` |
|    352845 | 11603 | `	SySetRelease(&aUseEntries);` |
|    352845 | 11604 | `	SySetRelease(&aInterfaces);` |
|    352845 | 11605 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11606 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11607 | `		return SXERR_ABORT;` |
|         - | 11608 | `	}` |
|    176420 | 11609 | `done:` |
|         - | 11610 | `	/* Point beyond the class body */` |
|    352887 | 11611 | `	pGen->pIn = &pEnd[1];` |
|    352887 | 11612 | `	pGen->pEnd = pTmp;` |
|    352887 | 11613 | `	return PH7_OK;` |
|    176447 | 11614 | `}` |
|         - | 11615 | `/* Compile a named class declaration (the common case). */` |
|    352856 | 11616 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11617 | `{` |
|    352861 | 11618 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11619 | `}` |
|         - | 11620 | `/*` |
|         - | 11621 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11622 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11623 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11624 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11625 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11626 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11627 | ` */` |
|        28 | 11628 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11629 | `{` |
|         - | 11630 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11631 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11632 | `	SyString sName;` |
|         - | 11633 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11634 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11635 | `	                              * is keyed to this 'class' token */` |
|         - | 11636 | `	ph7_value *pObj;` |
|        32 | 11637 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11638 | `	sxu32 nIdx,nLen;` |
|         - | 11639 | `	sxi32 nArg,rc;` |
|        14 | 11640 | `	SXUNUSED(iCompileFlag);` |
|         - | 11641 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11642 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11643 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11644 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11645 | `	}` |
|        32 | 11646 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11647 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11648 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11649 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11650 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11651 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11652 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11653 | `		return rc;` |
|         - | 11654 | `	}` |
|         - | 11655 | `	{` |
|         - | 11656 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11657 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11658 | `		if( pAnonClass` |
|        32 | 11659 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11660 | `			return SXERR_ABORT;` |
|         - | 11661 | `		}` |
|         - | 11662 | `	}` |
|         - | 11663 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11664 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11665 | `	nArg = 0;` |
|        32 | 11666 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11667 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11668 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11669 | `		SyToken *pArgNext;` |
|         7 | 11670 | `		pGen->pIn = pArgStart;` |
|         7 | 11671 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11672 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11673 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11674 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11675 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11676 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11677 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11678 | `					return SXERR_ABORT;` |
|         - | 11679 | `				}` |
|         7 | 11680 | `				nArg++;` |
|         3 | 11681 | `			}` |
|         7 | 11682 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11683 | `		}` |
|         7 | 11684 | `		pGen->pIn = pSavedIn;` |
|         7 | 11685 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11686 | `	}` |
|         - | 11687 | `	/* Load the synthesized class name */` |
|        32 | 11688 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11689 | `	if( pObj == 0 ){` |
|       ! 0 | 11690 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11691 | `		return SXERR_ABORT;` |
|         - | 11692 | `	}` |
|        32 | 11693 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11694 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11695 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11696 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11697 | `	return SXRET_OK;` |
|        18 | 11698 | `}` |
|         - | 11699 | `/*` |
|         - | 11700 | ` * Compile a user-defined abstract class.` |
|         - | 11701 | ` *  According to the PHP language reference manual` |
|         - | 11702 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11703 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11704 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11705 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11706 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11707 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11708 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11709 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11710 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11711 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11712 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11713 | ` *   could differ.` |
|         - | 11714 | ` */` |
|         - | 11715 | `/*` |
|         - | 11716 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11717 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11718 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11719 | ` */` |
|  12747298 | 11720 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11721 | `{` |
|  12747303 | 11722 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   7470493 | 11723 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   7470493 | 11724 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   7424675 | 11725 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3697026 | 11726 | `	}` |
|  12670867 | 11727 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  12670807 | 11728 | `	return FALSE;` |
|   6373654 | 11729 | `}` |
|         - | 11730 | `/*` |
|         - | 11731 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11732 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11733 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11734 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11735 | ` */` |
|  12670802 | 11736 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11737 | `{` |
|  12670807 | 11738 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  12670807 | 11739 | `	sxi32 iFlags = 0,iFlag;` |
|  12747303 | 11740 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76501 | 11741 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11742 | `			pDup = pIn;` |
|         2 | 11743 | `		}` |
|     76501 | 11744 | `		iFlags \|= iFlag;` |
|     76501 | 11745 | `		pIn++;` |
|         5 | 11746 | `	}` |
|  12670807 | 11747 | `	*ppIn = pIn;` |
|  12670807 | 11748 | `	if( ppDup ){ *ppDup = pDup; }` |
|  12670807 | 11749 | `	return iFlags;` |
|         5 | 11750 | `}` |
|         - | 11751 | `/*` |
|         - | 11752 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11753 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11754 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11755 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11756 | `` * `readonly`) to their existing handlers.`` |
|         - | 11757 | ` */` |
|  12636380 | 11758 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11759 | `{` |
|  12636385 | 11760 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   6360251 | 11761 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  12657409 | 11762 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11763 | `}` |
|         - | 11764 | `/*` |
|         - | 11765 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11766 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11767 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11768 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11769 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11770 | ` */` |
|     34422 | 11771 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11772 | `{` |
|         - | 11773 | `	SyToken *pDup;` |
|     34427 | 11774 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11775 | `	sxi32 rc;` |
|     34427 | 11776 | `	if( pDup ){` |
|         4 | 11777 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11778 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11779 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11780 | `			return SXERR_ABORT;` |
|         - | 11781 | `		}` |
|         1 | 11782 | `	}` |
|     34422 | 11783 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17216 | 11784 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11785 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11786 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11787 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11788 | `			return SXERR_ABORT;` |
|         - | 11789 | `		}` |
|         1 | 11790 | `	}` |
|     34427 | 11791 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17216 | 11792 | `}` |
|         - | 11793 | `/*` |
|         - | 11794 | ` * Compile a user-defined trait.` |
|         - | 11795 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11796 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11797 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11798 | ` */` |
|      7710 | 11799 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11800 | `{` |
|      7715 | 11801 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11802 | `	ph7_class *pClass;` |
|         - | 11803 | `	SyToken *pEnd,*pTmp;` |
|         - | 11804 | `	sxi32 iProtection;` |
|         - | 11805 | `	sxi32 iAttrflags;` |
|         - | 11806 | `	SyString *pName;` |
|         - | 11807 | `	sxi32 nKwrd;` |
|         - | 11808 | `	sxi32 rc;` |
|         - | 11809 | `	/* Jump the 'trait' keyword */` |
|      7715 | 11810 | `	pGen->pIn++;` |
|      7715 | 11811 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11812 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11813 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11814 | `			return SXERR_ABORT;` |
|         - | 11815 | `		}` |
|       ! 0 | 11816 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11817 | `			pGen->pIn++;` |
|       ! 0 | 11818 | `		}` |
|       ! 0 | 11819 | `		return SXRET_OK;` |
|         - | 11820 | `	}` |
|         - | 11821 | `	/* Extract trait name */` |
|      7715 | 11822 | `	pName = &pGen->pIn->sData;` |
|      7715 | 11823 | `	pGen->pIn++;` |
|         - | 11824 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11825 | `		SyBlob sFQN;` |
|         - | 11826 | `		SyString sFQNStr;` |
|      7715 | 11827 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7715 | 11828 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7715 | 11829 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7715 | 11830 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7715 | 11831 | `		SyBlobRelease(&sFQN);` |
|         - | 11832 | `	}` |
|      7715 | 11833 | `	if( pClass == 0 ){` |
|       ! 0 | 11834 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11835 | `		return SXERR_ABORT;` |
|         - | 11836 | `	}` |
|      7715 | 11837 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7715 | 11838 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11839 | `		return SXERR_ABORT;` |
|         - | 11840 | `	}` |
|         - | 11841 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7715 | 11842 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11843 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11844 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11845 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11846 | `			return SXERR_ABORT;` |
|         - | 11847 | `		}` |
|       ! 0 | 11848 | `		return SXRET_OK;` |
|         - | 11849 | `	}` |
|      7715 | 11850 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7715 | 11851 | `	pEnd = 0;` |
|      7715 | 11852 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7715 | 11853 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11854 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11855 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11856 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11857 | `			return SXERR_ABORT;` |
|         - | 11858 | `		}` |
|       ! 0 | 11859 | `		return SXRET_OK;` |
|         - | 11860 | `	}` |
|         - | 11861 | `	/* The delimiter token is the trait body's closing brace */` |
|      7715 | 11862 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11863 | `	/* Swap token stream */` |
|      7715 | 11864 | `	pTmp = pGen->pEnd;` |
|      7715 | 11865 | `	pGen->pEnd = pEnd;` |
|         - | 11866 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7715 | 11867 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11868 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     55411 | 11869 | `	for(;;){` |
|    156667 | 11870 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     22927 | 11871 | `			pGen->pIn++;` |
|         5 | 11872 | `		}` |
|    133745 | 11873 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7715 | 11874 | `			break;` |
|         - | 11875 | `		}` |
|         - | 11876 | `		/* Bind a directly-preceding docblock to this member */` |
|    126035 | 11877 | `		GenStateSetPendingDoc(&(*pGen));` |
|    126035 | 11878 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11879 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11880 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11881 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11882 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11883 | `				return SXERR_ABORT;` |
|         - | 11884 | `			}` |
|       ! 0 | 11885 | `			goto done;` |
|         - | 11886 | `		}` |
|    126035 | 11887 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    126035 | 11888 | `		iAttrflags = 0;` |
|    126035 | 11889 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    126035 | 11890 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    126035 | 11891 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11892 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11893 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11894 | `				for(;;){` |
|         - | 11895 | `					ph7_class *pUsedTrait;` |
|         - | 11896 | `					SyString *pUsedName;` |
|         5 | 11897 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11898 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11899 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11900 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11901 | `							return SXERR_ABORT;` |
|         - | 11902 | `						}` |
|       ! 0 | 11903 | `						break;` |
|         - | 11904 | `					}` |
|         5 | 11905 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11906 | `					{` |
|         - | 11907 | `						SyBlob sResolved;` |
|         5 | 11908 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11909 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11910 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11911 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11912 | `						SyBlobRelease(&sResolved);` |
|         - | 11913 | `					}` |
|         5 | 11914 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11915 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11916 | `					}` |
|         5 | 11917 | `					if( pUsedTrait == 0 ){` |
|         4 | 11918 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11919 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11920 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11921 | `							return SXERR_ABORT;` |
|         - | 11922 | `						}` |
|         2 | 11923 | `					}else{` |
|         3 | 11924 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11925 | `					}` |
|         5 | 11926 | `					pGen->pIn++;` |
|         5 | 11927 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11928 | `						break;` |
|         - | 11929 | `					}` |
|       ! 0 | 11930 | `					pGen->pIn++;` |
|       ! 0 | 11931 | `				}` |
|         5 | 11932 | `				continue;` |
|         - | 11933 | `			}` |
|    126031 | 11934 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    126015 | 11935 | `				iProtection = nKwrd;` |
|    126015 | 11936 | `				pGen->pIn++;` |
|    126010 | 11937 | `				if( pGen->pIn >= pGen->pEnd` |
|    126015 | 11938 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11939 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11940 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11941 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11942 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11943 | `						return SXERR_ABORT;` |
|         - | 11944 | `					}` |
|       ! 0 | 11945 | `					goto done;` |
|         - | 11946 | `				}` |
|    126015 | 11947 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     22913 | 11948 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     22913 | 11949 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11950 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11951 | `							return SXERR_ABORT;` |
|         - | 11952 | `						}` |
|       ! 0 | 11953 | `						goto done;` |
|         - | 11954 | `					}` |
|     22913 | 11955 | `					continue;` |
|         - | 11956 | `				}` |
|    103107 | 11957 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 11958 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 11959 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11960 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11961 | `							return SXERR_ABORT;` |
|         - | 11962 | `						}` |
|       ! 0 | 11963 | `						goto done;` |
|         - | 11964 | `					}` |
|         5 | 11965 | `					continue;` |
|         - | 11966 | `				}` |
|    103103 | 11967 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     51549 | 11968 | `			}` |
|    103119 | 11969 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 11970 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11971 | `					"Traits cannot have constants");` |
|       ! 0 | 11972 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11973 | `					return SXERR_ABORT;` |
|         - | 11974 | `				}` |
|       ! 0 | 11975 | `				goto done;` |
|       ! 0 | 11976 | `			}else{` |
|    103119 | 11977 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7647 | 11978 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7647 | 11979 | `					pGen->pIn++;` |
|      7647 | 11980 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7645 | 11981 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7645 | 11982 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11983 | `							iProtection = nKwrd;` |
|       ! 0 | 11984 | `							pGen->pIn++;` |
|       ! 0 | 11985 | `						}` |
|      3820 | 11986 | `					}` |
|      7642 | 11987 | `					if( pGen->pIn >= pGen->pEnd` |
|      7647 | 11988 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11989 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11990 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 11991 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11992 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11993 | `							return SXERR_ABORT;` |
|         - | 11994 | `						}` |
|       ! 0 | 11995 | `						goto done;` |
|         - | 11996 | `					}` |
|      7647 | 11997 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 11998 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 11999 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12000 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12001 | `								return SXERR_ABORT;` |
|         - | 12002 | `							}` |
|       ! 0 | 12003 | `							goto done;` |
|         - | 12004 | `						}` |
|         3 | 12005 | `						continue;` |
|         - | 12006 | `					}` |
|      7645 | 12007 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 12008 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12009 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12010 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12011 | `								return SXERR_ABORT;` |
|         - | 12012 | `							}` |
|       ! 0 | 12013 | `							goto done;` |
|         - | 12014 | `						}` |
|       ! 0 | 12015 | `						continue;` |
|         - | 12016 | `					}` |
|      7645 | 12017 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     99297 | 12018 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 12019 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 12020 | `					pGen->pIn++;` |
|         6 | 12021 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 12022 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 12023 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 12024 | `							iProtection = nKwrd;` |
|         6 | 12025 | `							pGen->pIn++;` |
|         2 | 12026 | `						}` |
|         2 | 12027 | `					}` |
|         6 | 12028 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 12029 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 12030 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12031 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 12032 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12033 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12034 | `							return SXERR_ABORT;` |
|         - | 12035 | `						}` |
|       ! 0 | 12036 | `						goto done;` |
|         - | 12037 | `					}` |
|         6 | 12038 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 12039 | `				}` |
|    103117 | 12040 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 12041 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12042 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 12043 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12044 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12045 | `						return SXERR_ABORT;` |
|         - | 12046 | `					}` |
|       ! 0 | 12047 | `					goto done;` |
|         - | 12048 | `				}` |
|    103117 | 12049 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 12050 | `					pGen->pIn++;` |
|       ! 0 | 12051 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 12052 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12053 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 12054 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12055 | `							return SXERR_ABORT;` |
|         - | 12056 | `						}` |
|       ! 0 | 12057 | `						goto done;` |
|         - | 12058 | `					}` |
|       ! 0 | 12059 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12060 | `				}else{` |
|    103117 | 12061 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 12062 | `				}` |
|    103117 | 12063 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 12064 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12065 | `						return SXERR_ABORT;` |
|         - | 12066 | `					}` |
|       ! 0 | 12067 | `					goto done;` |
|         - | 12068 | `				}` |
|         - | 12069 | `			}` |
|     51561 | 12070 | `		}else{` |
|       ! 0 | 12071 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12072 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12073 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12074 | `					return SXERR_ABORT;` |
|         - | 12075 | `				}` |
|       ! 0 | 12076 | `				goto done;` |
|         - | 12077 | `			}` |
|         - | 12078 | `		}` |
|         5 | 12079 | `	}` |
|         - | 12080 | `	/* Install the trait */` |
|      7715 | 12081 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7715 | 12082 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12083 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12084 | `		return SXERR_ABORT;` |
|         - | 12085 | `	}` |
|      3855 | 12086 | `done:` |
|         - | 12087 | `	/* Point beyond the trait body */` |
|      7715 | 12088 | `	pGen->pIn = &pEnd[1];` |
|      7715 | 12089 | `	pGen->pEnd = pTmp;` |
|      7715 | 12090 | `	return PH7_OK;` |
|      3860 | 12091 | `}` |
|         - | 12092 | `/*` |
|         - | 12093 | ` * Compile a user-defined class.` |
|         - | 12094 | ` *  According to the PHP language reference manual` |
|         - | 12095 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 12096 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 12097 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 12098 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 12099 | ` *   and functions (called "methods").` |
|         - | 12100 | ` */` |
|    314590 | 12101 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 12102 | `{` |
|         - | 12103 | `	sxi32 rc;` |
|    314595 | 12104 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    314595 | 12105 | `	return rc;` |
|         5 | 12106 | `}` |
|         - | 12107 | `/*` |
|         - | 12108 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 12109 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 12110 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 12111 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 12112 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 12113 | ` */` |
|  12594326 | 12114 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 12115 | `{` |
|  12799122 | 12116 | `	return (pIn->nType & PH7_TK_ID)` |
|   6501954 | 12117 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    214464 | 12118 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  12799117 | 12119 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 12120 | `}` |
|         - | 12121 | `/*` |
|         - | 12122 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 12123 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 12124 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 12125 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 12126 | ` */` |
|      3844 | 12127 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 12128 | `{` |
|      3849 | 12129 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 12130 | `}` |
|         - | 12131 | `/*` |
|         - | 12132 | ` * Exception handling.` |
|         - | 12133 | ` *  According to the PHP language reference manual` |
|         - | 12134 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 12135 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 12136 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 12137 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 12138 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 12139 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 12140 | ` *    (or re-thrown) within a catch block.` |
|         - | 12141 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 12142 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 12143 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 12144 | ` *    been defined with set_exception_handler().` |
|         - | 12145 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 12146 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 12147 | ` */` |
|         - | 12148 | `/*` |
|         - | 12149 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 12150 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 12151 | ` * indicates failure.` |
|         - | 12152 | ` */` |
|    507950 | 12153 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 12154 | `{` |
|    507955 | 12155 | `	sxi32 rc = SXRET_OK;` |
|    507955 | 12156 | `	if( pRoot->pOp ){` |
|    507943 | 12157 | `		switch( pRoot->pOp->iOp ){` |
|    253969 | 12158 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 12159 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 12160 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 12161 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 12162 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 12163 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    507943 | 12164 | `			break;` |
|       ! 0 | 12165 | `		default:` |
|         - | 12166 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 12167 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 12168 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 12169 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12170 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 12171 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 12172 | `				rc = SXERR_INVALID;` |
|       ! 0 | 12173 | `			}` |
|       ! 0 | 12174 | `			break;` |
|         - | 12175 | `		}` |
|    253986 | 12176 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 12177 | `		/* Unexpected expression */` |
|       ! 0 | 12178 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12179 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12180 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 12181 | `			rc = SXERR_INVALID;` |
|       ! 0 | 12182 | `		}` |
|       ! 0 | 12183 | `	}` |
|    507955 | 12184 | `	return rc;` |
|         5 | 12185 | `}` |
|         - | 12186 | `/*` |
|         - | 12187 | ` * Compile a 'throw' statement.` |
|         - | 12188 | ` * throw: This is how you trigger an exception.` |
|         - | 12189 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12190 | ` */` |
|    507914 | 12191 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12192 | `{` |
|    507919 | 12193 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12194 | `	GenBlock *pBlock;` |
|         - | 12195 | `	sxu32 nIdx;` |
|         - | 12196 | `	sxi32 rc;` |
|    507919 | 12197 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12198 | `	/* Compile the expression */` |
|    507919 | 12199 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    507919 | 12200 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12201 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12202 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12203 | `			return SXERR_ABORT;` |
|         - | 12204 | `		}` |
|       ! 0 | 12205 | `		return SXRET_OK;` |
|         - | 12206 | `	}` |
|    507919 | 12207 | `	pBlock = pGen->pCurrent;` |
|         - | 12208 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   2023277 | 12209 | `	while(pBlock->pParent){` |
|   2023273 | 12210 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    507915 | 12211 | `			break;` |
|         - | 12212 | `		}` |
|         - | 12213 | `		/* Point to the parent block */` |
|   1515363 | 12214 | `		pBlock = pBlock->pParent;` |
|         5 | 12215 | `	}` |
|         - | 12216 | `	/* Emit the throw instruction */` |
|    507919 | 12217 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12218 | `	/* Emit the jump */` |
|    507919 | 12219 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    507919 | 12220 | `	return SXRET_OK;` |
|    253962 | 12221 | `}` |
|         - | 12222 | `/*` |
|         - | 12223 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12224 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12225 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12226 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12227 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12228 | ` */` |
|        36 | 12229 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12230 | `{` |
|        38 | 12231 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12232 | `	GenBlock *pBlock;` |
|         - | 12233 | `	sxu32 nIdx;` |
|         - | 12234 | `	sxi32 rc;` |
|        18 | 12235 | `	(void)iCompileFlag;` |
|        38 | 12236 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12237 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12238 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12239 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12240 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12241 | `			return SXERR_ABORT;` |
|         - | 12242 | `		}` |
|       ! 0 | 12243 | `		return SXRET_OK;` |
|         - | 12244 | `	}` |
|        38 | 12245 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12246 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12247 | `		return SXERR_ABORT;` |
|         - | 12248 | `	}` |
|        38 | 12249 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12250 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12251 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12252 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12253 | `			return SXERR_ABORT;` |
|         - | 12254 | `		}` |
|       ! 0 | 12255 | `		return SXRET_OK;` |
|         - | 12256 | `	}` |
|         - | 12257 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12258 | `	pBlock = pGen->pCurrent;` |
|        60 | 12259 | `	while( pBlock->pParent ){` |
|        49 | 12260 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12261 | `			break;` |
|         - | 12262 | `		}` |
|        23 | 12263 | `		pBlock = pBlock->pParent;` |
|         1 | 12264 | `	}` |
|        38 | 12265 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12266 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12267 | `	return SXRET_OK;` |
|        20 | 12268 | `}` |
|         - | 12269 | `/*` |
|         - | 12270 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12271 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12272 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12273 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12274 | ` * compile error propagated from the parser.` |
|         - | 12275 | ` */` |
|        56 | 12276 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12277 | `{` |
|         - | 12278 | `	SyString sClassName;` |
|         - | 12279 | `	SyToken *pToken;` |
|         - | 12280 | `	SyString *pName;` |
|         - | 12281 | `	char *zDup;` |
|         - | 12282 | `	sxi32 rc;` |
|        61 | 12283 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        61 | 12284 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        61 | 12285 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        61 | 12286 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        61 | 12287 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12288 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12289 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12290 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12291 | `		return SXERR_INVALID;` |
|         - | 12292 | `	}` |
|        61 | 12293 | `	pGen->pIn++; /* '(' */` |
|        28 | 12294 | `	for(;;){` |
|         - | 12295 | `		SyBlob sResolved;` |
|        61 | 12296 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        61 | 12297 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12298 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12299 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12300 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12301 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12302 | `			return SXERR_INVALID;` |
|         - | 12303 | `		}` |
|        89 | 12304 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        56 | 12305 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        61 | 12306 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        61 | 12307 | `		SyBlobRelease(&sResolved);` |
|        61 | 12308 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        61 | 12309 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        61 | 12310 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        56 | 12311 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12312 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12313 | `			pGen->pIn++; continue;` |
|         - | 12314 | `		}` |
|        61 | 12315 | `		break;` |
|       ! 0 | 12316 | `	}` |
|         - | 12317 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12318 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|        61 | 12319 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|         3 | 12320 | `		pGen->pIn++; /* ')' */` |
|         3 | 12321 | `		return SXRET_OK;` |
|         - | 12322 | `	}` |
|        54 | 12323 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12324 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12325 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12326 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12327 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12328 | `		return SXERR_INVALID;` |
|         - | 12329 | `	}` |
|        59 | 12330 | `	pGen->pIn++; /* '$' */` |
|        59 | 12331 | `	pName = &pGen->pIn->sData;` |
|        59 | 12332 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12333 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12334 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12335 | `	pGen->pIn++;` |
|        59 | 12336 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12337 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12338 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12339 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12340 | `		return SXERR_INVALID;` |
|         - | 12341 | `	}` |
|        59 | 12342 | `	pGen->pIn++; /* ')' */` |
|        59 | 12343 | `	return SXRET_OK;` |
|        33 | 12344 | `}` |
|         - | 12345 | `/*` |
|         - | 12346 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12347 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12348 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12349 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12350 | ` * VmThrowException):` |
|         - | 12351 | ` *` |
|         - | 12352 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12353 | ` *    <try body>` |
|         - | 12354 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12355 | ` *    JMP  -> finally\|end` |
|         - | 12356 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12357 | ` *    <catch body>` |
|         - | 12358 | ` *    JMP  -> finally\|end` |
|         - | 12359 | ` *    ... more catches ...` |
|         - | 12360 | ` *  Lfin: <finally body>` |
|         - | 12361 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12362 | ` *  Lend:` |
|         - | 12363 | ` */` |
|       100 | 12364 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12365 | `{` |
|       105 | 12366 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12367 | `	GenBlock *pTry;` |
|         - | 12368 | `	VmInstr *pInstr;` |
|       105 | 12369 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12370 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12371 | `	sxi32 rc;` |
|       105 | 12372 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12373 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       105 | 12374 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       105 | 12375 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       105 | 12376 | `	pTry->pUserData = pException;` |
|       105 | 12377 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       105 | 12378 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       105 | 12379 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       105 | 12380 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       105 | 12381 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       105 | 12382 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12383 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       105 | 12384 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       105 | 12385 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       105 | 12386 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       105 | 12387 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12388 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       105 | 12389 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12390 | `	/* Catch clauses (inline) */` |
|       105 | 12391 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       100 | 12392 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        61 | 12393 | `		sxu32 k = 0;` |
|        84 | 12394 | `		for(;;){` |
|         - | 12395 | `			ph7_exception_block sCatch;` |
|         - | 12396 | `			GenBlock *pCatchBlk;` |
|       117 | 12397 | `			sxu32 idxJmp = 0;` |
|       112 | 12398 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       107 | 12399 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        33 | 12400 | `				break;` |
|         - | 12401 | `			}` |
|        61 | 12402 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        61 | 12403 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12404 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        61 | 12405 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        61 | 12406 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        61 | 12407 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        61 | 12408 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12409 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12410 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12411 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        61 | 12412 | `			pCatchBlk->pUserData = pException;` |
|        61 | 12413 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        61 | 12414 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12415 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        61 | 12416 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12417 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12418 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        61 | 12419 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        61 | 12420 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        61 | 12421 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        61 | 12422 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        61 | 12423 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        61 | 12424 | `			k++;` |
|         5 | 12425 | `		}` |
|        28 | 12426 | `	}` |
|         - | 12427 | `	/* Finally (inline) */` |
|       105 | 12428 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12429 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12430 | `		GenBlock *pFinBlk;` |
|        52 | 12431 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12432 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12433 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12434 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12435 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12436 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12437 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12438 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12439 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12440 | `		pException->iHasFinally = 1;` |
|        24 | 12441 | `	}` |
|       105 | 12442 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       105 | 12443 | `	pException->iInlined = 1;` |
|         - | 12444 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12445 | `	{` |
|       105 | 12446 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12447 | `		sxu32 *aJ; sxu32 n;` |
|       105 | 12448 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       105 | 12449 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       105 | 12450 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       161 | 12451 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        61 | 12452 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        61 | 12453 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        33 | 12454 | `		}` |
|         - | 12455 | `	}` |
|       105 | 12456 | `	SySetRelease(&aCatchJmp);` |
|       105 | 12457 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12458 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12459 | `	}` |
|       105 | 12460 | `	return SXRET_OK;` |
|        55 | 12461 | `}` |
|         - | 12462 | `/*` |
|         - | 12463 | ` * Compile a 'catch' block.` |
|         - | 12464 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12465 | ` * an object containing the exception information.` |
|         - | 12466 | ` */` |
|     24418 | 12467 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12468 | `{` |
|     24423 | 12469 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12470 | `	ph7_exception_block sCatch;` |
|         - | 12471 | `	SySet *pInstrContainer;` |
|         - | 12472 | `	SyString sClassName;` |
|         - | 12473 | `	GenBlock *pCatch;` |
|         - | 12474 | `	SyToken *pToken;` |
|         - | 12475 | `	SyString *pName;` |
|         - | 12476 | `	char *zDup;` |
|         - | 12477 | `	sxi32 rc;` |
|     24423 | 12478 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12479 | `	/* Zero the structure */` |
|     24423 | 12480 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12481 | `	/* Initialize fields */` |
|     24423 | 12482 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24423 | 12483 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24423 | 12484 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12485 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12486 | `			pToken = pGen->pIn;` |
|       ! 0 | 12487 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12488 | `				pToken--;` |
|       ! 0 | 12489 | `			}` |
|       ! 0 | 12490 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12491 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12492 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12493 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12494 | `				return SXERR_ABORT;` |
|         - | 12495 | `			}` |
|       ! 0 | 12496 | `			return SXERR_INVALID;` |
|         - | 12497 | `	}` |
|         - | 12498 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24423 | 12499 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12224 | 12500 | `	for(;;){` |
|         - | 12501 | `		SyBlob sResolved;` |
|     24453 | 12502 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24453 | 12503 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12504 | `			SyBlobRelease(&sResolved);` |
|         6 | 12505 | `			pToken = pGen->pIn;` |
|         6 | 12506 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12507 | `				pToken--;` |
|       ! 0 | 12508 | `			}` |
|         8 | 12509 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12510 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12511 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12512 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12513 | `				return SXERR_ABORT;` |
|         - | 12514 | `			}` |
|         6 | 12515 | `			return SXERR_INVALID;` |
|         - | 12516 | `		}` |
|         - | 12517 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12518 | `		 * transient SyBlob allocation. */` |
|     36671 | 12519 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24444 | 12520 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24449 | 12521 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24449 | 12522 | `		SyBlobRelease(&sResolved);` |
|     24449 | 12523 | `		if( zDup == 0 ){` |
|       ! 0 | 12524 | `			goto Mem;` |
|         - | 12525 | `		}` |
|     24449 | 12526 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24449 | 12527 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12528 | `			goto Mem;` |
|         - | 12529 | `		}` |
|         - | 12530 | `		/* Check for '\|' (multi-catch separator) */` |
|     24444 | 12531 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24444 | 12532 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        35 | 12533 | `			pGen->pIn->sData.nByte == 1 &&` |
|        30 | 12534 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        32 | 12535 | `			pGen->pIn++; /* Consume the '\|' */` |
|        32 | 12536 | `			continue;` |
|         - | 12537 | `		}` |
|     24419 | 12538 | `		break;` |
|       ! 0 | 12539 | `	}` |
|         - | 12540 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12541 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|         - | 12542 | `	 * jump straight to compiling the block below. */` |
|     24419 | 12543 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|         5 | 12544 | `		goto CatchBody;` |
|         - | 12545 | `	}` |
|     24410 | 12546 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24415 | 12547 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12548 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12549 | `			pToken = pGen->pIn;` |
|       ! 0 | 12550 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12551 | `				pToken--;` |
|       ! 0 | 12552 | `			}` |
|       ! 0 | 12553 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12554 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12555 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12556 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12557 | `				return SXERR_ABORT;` |
|         - | 12558 | `			}` |
|       ! 0 | 12559 | `			return SXERR_INVALID;` |
|         - | 12560 | `	}` |
|     24415 | 12561 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12562 | `	/* Duplicate instance name */` |
|     24415 | 12563 | `	pName = &pGen->pIn->sData;` |
|     24415 | 12564 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24415 | 12565 | `	if( zDup == 0 ){` |
|       ! 0 | 12566 | `		goto Mem;` |
|         - | 12567 | `	}` |
|     24415 | 12568 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24415 | 12569 | `	pGen->pIn++;` |
|     12207 | 12570 | `CatchBody:` |
|     24419 | 12571 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12572 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12573 | `		pToken = pGen->pIn;` |
|       ! 0 | 12574 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12575 | `			pToken--;` |
|       ! 0 | 12576 | `		}` |
|       ! 0 | 12577 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12578 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12579 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12580 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12581 | `			return SXERR_ABORT;` |
|         - | 12582 | `		}` |
|       ! 0 | 12583 | `		return SXERR_INVALID;` |
|         - | 12584 | `	}` |
|         - | 12585 | `	/* Compile the block */` |
|     24419 | 12586 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12587 | `	/* Create the catch block */` |
|     24419 | 12588 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24419 | 12589 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12590 | `		return SXERR_ABORT;` |
|         - | 12591 | `	}` |
|         - | 12592 | `	/* Swap bytecode container */` |
|     24419 | 12593 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24419 | 12594 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12595 | `	/* Compile the block */` |
|     24419 | 12596 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12597 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24419 | 12598 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12599 | `	/* Emit the DONE instruction */` |
|     24419 | 12600 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12601 | `	/* Leave the block */` |
|     24419 | 12602 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12603 | `	/* Restore the default container */` |
|     24419 | 12604 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12605 | `	/* Install the catch block */` |
|     24419 | 12606 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24419 | 12607 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12608 | `		goto Mem;` |
|         - | 12609 | `	}` |
|     24419 | 12610 | `	return SXRET_OK;` |
|       ! 0 | 12611 | `Mem:` |
|       ! 0 | 12612 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12613 | `	return SXERR_ABORT;` |
|     12214 | 12614 | `}` |
|         - | 12615 | `/*` |
|         - | 12616 | ` * Compile a 'try' block.` |
|         - | 12617 | ` * A function using an exception should be in a "try" block.` |
|         - | 12618 | ` * If the exception does not trigger, the code will continue` |
|         - | 12619 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12620 | ` * is "thrown".` |
|         - | 12621 | ` */` |
|     24576 | 12622 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12623 | `{` |
|         - | 12624 | `	ph7_exception *pException;` |
|     24581 | 12625 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12626 | `	GenBlock *pTry;` |
|         - | 12627 | `	sxu32 nJmpIdx;` |
|         - | 12628 | `	sxi32 rc;` |
|         - | 12629 | `	/* Create the exception container */` |
|     24581 | 12630 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24581 | 12631 | `	if( pException == 0 ){` |
|       ! 0 | 12632 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12633 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12634 | `		return SXERR_ABORT;` |
|         - | 12635 | `	}` |
|         - | 12636 | `	/* Zero the structure */` |
|     24581 | 12637 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12638 | `	/* Initialize fields */` |
|     24581 | 12639 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24581 | 12640 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24581 | 12641 | `	pException->iHasFinally = 0;` |
|     24581 | 12642 | `	pException->iFinallyDone = 0;` |
|     24581 | 12643 | `	pException->pVm = pGen->pVm;` |
|         - | 12644 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12645 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12646 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12647 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12648 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12649 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24581 | 12650 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       105 | 12651 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12652 | `	}` |
|         - | 12653 | `	/* Create the try block */` |
|     24481 | 12654 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24481 | 12655 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12656 | `		return SXERR_ABORT;` |
|         - | 12657 | `	}` |
|         - | 12658 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24481 | 12659 | `	pTry->pUserData = pException;` |
|         - | 12660 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24481 | 12661 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12662 | `	/* Fix the jump later when the destination is resolved */` |
|     24481 | 12663 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24481 | 12664 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12665 | `	/* Compile the block */` |
|     24481 | 12666 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24481 | 12667 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12668 | `		return SXERR_ABORT;` |
|         - | 12669 | `	}` |
|         - | 12670 | `	/* Fix forward jumps now the destination is resolved */` |
|     24481 | 12671 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12672 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24481 | 12673 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12674 | `	/* Leave the block */` |
|     24481 | 12675 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12676 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24481 | 12677 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24474 | 12678 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12679 | `		/* Compile one or more catch blocks */` |
|     24414 | 12680 | `		for(;;){` |
|     48828 | 12681 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36679 | 12682 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12210 | 12683 | `					break;` |
|         - | 12684 | `			}` |
|     24423 | 12685 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24423 | 12686 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12687 | `				return SXERR_ABORT;` |
|         - | 12688 | `			}` |
|         5 | 12689 | `		}` |
|     12205 | 12690 | `	}` |
|         - | 12691 | `	/* Compile optional finally block */` |
|     24481 | 12692 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       736 | 12693 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12694 | `		SySet *pInstrContainer;` |
|         - | 12695 | `		GenBlock *pFinBlock;` |
|       129 | 12696 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12697 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12698 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12699 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12700 | `			return SXERR_ABORT;` |
|         - | 12701 | `		}` |
|         - | 12702 | `		/* Swap bytecode container */` |
|       129 | 12703 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12704 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12705 | `		/* Compile the finally body */` |
|       129 | 12706 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12707 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12708 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12709 | `			return SXERR_ABORT;` |
|         - | 12710 | `		}` |
|         - | 12711 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12712 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12713 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12714 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12715 | `		/* Leave the block */` |
|       129 | 12716 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12717 | `		/* Restore the default container */` |
|       129 | 12718 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12719 | `		pException->iHasFinally = 1;` |
|        62 | 12720 | `	}` |
|         - | 12721 | `	/* Must have at least one catch or finally */` |
|     24481 | 12722 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12723 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12724 | `			"Cannot use try without catch or finally");` |
|         8 | 12725 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12726 | `			return SXERR_ABORT;` |
|         - | 12727 | `		}` |
|         3 | 12728 | `	}` |
|     24481 | 12729 | `	return SXRET_OK;` |
|     12293 | 12730 | `}` |
|         - | 12731 | `/*` |
|         - | 12732 | ` * Compile a switch block.` |
|         - | 12733 | ` *  (See block-comment below for more information)` |
|         - | 12734 | ` */` |
|     53536 | 12735 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12736 | `{` |
|     53541 | 12737 | `	sxi32 rc = SXRET_OK;` |
|     53541 | 12738 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12739 | `		/* Unexpected token */` |
|       ! 0 | 12740 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12741 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12742 | `			return SXERR_ABORT;` |
|         - | 12743 | `		}` |
|       ! 0 | 12744 | `		pGen->pIn++;` |
|       ! 0 | 12745 | `	}` |
|     53541 | 12746 | `	pGen->pIn++;` |
|         - | 12747 | `	/* First instruction to execute in this block. */` |
|     53541 | 12748 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12749 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12750 | `	 * or the '}' token */` |
|     38366 | 12751 | `	for(;;){` |
|     76737 | 12752 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12753 | `			/* No more input to process */` |
|       ! 0 | 12754 | `			break;` |
|         - | 12755 | `		}` |
|     76737 | 12756 | `		rc = SXRET_OK;` |
|     76737 | 12757 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      3901 | 12758 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      3847 | 12759 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12760 | `					/* Unexpected token */` |
|       ! 0 | 12761 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12762 | `						&pGen->pIn->sData);` |
|       ! 0 | 12763 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12764 | `						return SXERR_ABORT;` |
|         - | 12765 | `					}` |
|         - | 12766 | `					/* FALL THROUGH */` |
|       ! 0 | 12767 | `				}` |
|      3847 | 12768 | `				rc = SXERR_EOF;` |
|      3847 | 12769 | `				break;` |
|         - | 12770 | `			}` |
|        32 | 12771 | `		}else{` |
|         - | 12772 | `			sxi32 nKwrd;` |
|         - | 12773 | `			/* Extract the keyword */` |
|     72841 | 12774 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     72841 | 12775 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|     24851 | 12776 | `				break;` |
|         - | 12777 | `			}` |
|     23149 | 12778 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12779 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12780 | `					/* Unexpected token */` |
|       ! 0 | 12781 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12782 | `						&pGen->pIn->sData);` |
|       ! 0 | 12783 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12784 | `						return SXERR_ABORT;` |
|         - | 12785 | `					}` |
|         - | 12786 | `					/* FALL THROUGH */` |
|       ! 0 | 12787 | `				}` |
|         - | 12788 | `				/* Block compiled */` |
|         3 | 12789 | `				break;` |
|         - | 12790 | `			}` |
|         - | 12791 | `		}` |
|         - | 12792 | `		/* Compile block */` |
|     23201 | 12793 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     23201 | 12794 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12795 | `			return SXERR_ABORT;` |
|         - | 12796 | `		}` |
|         5 | 12797 | `	}` |
|     53541 | 12798 | `	return rc;` |
|     26773 | 12799 | `}` |
|         - | 12800 | `/*` |
|         - | 12801 | ` * Compile a case eXpression.` |
|         - | 12802 | ` *  (See block-comment below for more information)` |
|         - | 12803 | ` */` |
|     53516 | 12804 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12805 | `{` |
|         - | 12806 | `	SySet *pInstrContainer;` |
|         - | 12807 | `	SyToken *pEnd,*pTmp;` |
|     53521 | 12808 | `	sxi32 iNest = 0;` |
|         - | 12809 | `	sxi32 rc;` |
|         - | 12810 | `	/* Delimit the expression */` |
|     53521 | 12811 | `	pEnd = pGen->pIn;` |
|    107045 | 12812 | `	while( pEnd < pGen->pEnd ){` |
|    107045 | 12813 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12814 | `			/* Increment nesting level */` |
|         3 | 12815 | `			iNest++;` |
|    107044 | 12816 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12817 | `			/* Decrement nesting level */` |
|         3 | 12818 | `			iNest--;` |
|    107042 | 12819 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     53521 | 12820 | `			break;` |
|         - | 12821 | `		}` |
|     53529 | 12822 | `		pEnd++;` |
|         5 | 12823 | `	}` |
|     53521 | 12824 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12825 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12826 | `		if( rc == SXERR_ABORT ){` |
|         - | 12827 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12828 | `			return SXERR_ABORT;` |
|         - | 12829 | `		}` |
|       ! 0 | 12830 | `	}` |
|         - | 12831 | `	/* Swap token stream */` |
|     53521 | 12832 | `	pTmp = pGen->pEnd;` |
|     53521 | 12833 | `	pGen->pEnd = pEnd;` |
|     53521 | 12834 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     53521 | 12835 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     53521 | 12836 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12837 | `	/* Emit the done instruction */` |
|     53521 | 12838 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     53521 | 12839 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12840 | `	/* Update token stream */` |
|     53521 | 12841 | `	pGen->pIn  = pEnd;` |
|     53521 | 12842 | `	pGen->pEnd = pTmp;` |
|     53521 | 12843 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12844 | `		return SXERR_ABORT;` |
|         - | 12845 | `	}` |
|     53521 | 12846 | `	return SXRET_OK;` |
|     26763 | 12847 | `}` |
|         - | 12848 | `/*` |
|         - | 12849 | ` * Compile the smart switch statement.` |
|         - | 12850 | ` * According to the PHP language reference manual` |
|         - | 12851 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12852 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12853 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12854 | ` *  This is exactly what the switch statement is for.` |
|         - | 12855 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12856 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12857 | ` *  of the outer loop, use continue 2.` |
|         - | 12858 | ` *  Note that switch/case does loose comparision.` |
|         - | 12859 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12860 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12861 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12862 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12863 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12864 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12865 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12866 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12867 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12868 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12869 | ` *  list for the next case.` |
|         - | 12870 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12871 | ` *  or floating-point numbers and strings.` |
|         - | 12872 | ` */` |
|      3844 | 12873 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12874 | `{` |
|         - | 12875 | `	GenBlock *pSwitchBlock;` |
|         - | 12876 | `	SyToken *pTmp,*pEnd;` |
|         - | 12877 | `	ph7_switch *pSwitch;` |
|         - | 12878 | `	sxu32 nToken;` |
|         - | 12879 | `	sxu32 nLine;` |
|         - | 12880 | `	sxi32 rc;` |
|      3849 | 12881 | `	nLine = pGen->pIn->nLine;` |
|         - | 12882 | `	/* Jump the 'switch' keyword */` |
|      3849 | 12883 | `	pGen->pIn++;` |
|      3849 | 12884 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12885 | `		/* Syntax error */` |
|       ! 0 | 12886 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12887 | `		if( rc == SXERR_ABORT ){` |
|         - | 12888 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12889 | `			return SXERR_ABORT;` |
|         - | 12890 | `		}` |
|       ! 0 | 12891 | `		goto Synchronize;` |
|         - | 12892 | `	}` |
|         - | 12893 | `	/* Jump the left parenthesis '(' */` |
|      3849 | 12894 | `	pGen->pIn++;` |
|      3849 | 12895 | `	pEnd = 0; /* cc warning */` |
|         - | 12896 | `	/* Create the loop block */` |
|      5771 | 12897 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      1922 | 12898 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      3849 | 12899 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12900 | `		return SXERR_ABORT;` |
|         - | 12901 | `	}` |
|         - | 12902 | `	/* Delimit the condition */` |
|      3849 | 12903 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      3849 | 12904 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12905 | `		/* Empty expression */` |
|       ! 0 | 12906 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12907 | `		if( rc == SXERR_ABORT ){` |
|         - | 12908 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12909 | `			return SXERR_ABORT;` |
|         - | 12910 | `		}` |
|       ! 0 | 12911 | `	}` |
|         - | 12912 | `	/* Swap token streams */` |
|      3849 | 12913 | `	pTmp = pGen->pEnd;` |
|      3849 | 12914 | `	pGen->pEnd = pEnd;` |
|         - | 12915 | `	/* Compile the expression */` |
|      3849 | 12916 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      3849 | 12917 | `	if( rc == SXERR_ABORT ){` |
|         - | 12918 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12919 | `		return SXERR_ABORT;` |
|         - | 12920 | `	}` |
|         - | 12921 | `	/* Update token stream */` |
|      3849 | 12922 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12923 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12924 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12925 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12926 | `			return SXERR_ABORT;` |
|         - | 12927 | `		}` |
|       ! 0 | 12928 | `		pGen->pIn++;` |
|       ! 0 | 12929 | `	}` |
|      3849 | 12930 | `	pGen->pIn  = &pEnd[1];` |
|      3849 | 12931 | `	pGen->pEnd = pTmp;` |
|      3849 | 12932 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      3844 | 12933 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12934 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12935 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12936 | `				pTmp--;` |
|       ! 0 | 12937 | `			}` |
|         - | 12938 | `			/* Unexpected token */` |
|       ! 0 | 12939 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12940 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12941 | `				return SXERR_ABORT;` |
|         - | 12942 | `			}` |
|       ! 0 | 12943 | `			goto Synchronize;` |
|         - | 12944 | `	}` |
|         - | 12945 | `	/* Set the delimiter token */` |
|      3849 | 12946 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12947 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12948 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12949 | `	}else{` |
|      3847 | 12950 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12951 | `	}` |
|      3849 | 12952 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12953 | `	/* Create the switch blocks container */` |
|      3849 | 12954 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      3849 | 12955 | `	if( pSwitch == 0 ){` |
|         - | 12956 | `		/* Abort compilation */` |
|       ! 0 | 12957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12958 | `		return SXERR_ABORT;` |
|         - | 12959 | `	}` |
|         - | 12960 | `	/* Zero the structure */` |
|      3849 | 12961 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 12962 | `	/* Initialize fields */` |
|      3849 | 12963 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 12964 | `	/* Emit the switch instruction */` |
|      3849 | 12965 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 12966 | `	/* Compile case blocks */` |
|     51616 | 12967 | `	for(;;){` |
|         - | 12968 | `		sxu32 nKwrd;` |
|     53543 | 12969 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12970 | `			/* No more input to process */` |
|       ! 0 | 12971 | `			break;` |
|         - | 12972 | `		}` |
|     53543 | 12973 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 12974 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 12975 | `				/* Unexpected token */` |
|       ! 0 | 12976 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12977 | `					&pGen->pIn->sData);` |
|       ! 0 | 12978 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12979 | `					return SXERR_ABORT;` |
|         - | 12980 | `				}` |
|         - | 12981 | `				/* FALL THROUGH */` |
|       ! 0 | 12982 | `			}` |
|         - | 12983 | `			/* Block compiled */` |
|       ! 0 | 12984 | `			break;` |
|         - | 12985 | `		}` |
|         - | 12986 | `		/* Extract the keyword */` |
|     53543 | 12987 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     53543 | 12988 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12989 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 12990 | `				/* Unexpected token */` |
|       ! 0 | 12991 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12992 | `					&pGen->pIn->sData);` |
|       ! 0 | 12993 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12994 | `					return SXERR_ABORT;` |
|         - | 12995 | `				}` |
|         - | 12996 | `				/* FALL THROUGH */` |
|       ! 0 | 12997 | `			}` |
|         - | 12998 | `			/* Block compiled */` |
|         3 | 12999 | `			break;` |
|         - | 13000 | `		}` |
|     53541 | 13001 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 13002 | `			/*` |
|         - | 13003 | `			 * Accroding to the PHP language reference manual` |
|         - | 13004 | `			 *  A special case is the default case. This case matches anything` |
|         - | 13005 | `			 *  that wasn't matched by the other cases.` |
|         - | 13006 | `			 */` |
|        25 | 13007 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 13008 | `				/* Default case already compiled */` |
|       ! 0 | 13009 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 13010 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13011 | `					return SXERR_ABORT;` |
|         - | 13012 | `				}` |
|       ! 0 | 13013 | `			}` |
|        25 | 13014 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 13015 | `			/* Compile the default block */` |
|        25 | 13016 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 13017 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13018 | `				return SXERR_ABORT;` |
|        25 | 13019 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 13020 | `				break;` |
|         1 | 13021 | `			}` |
|     53522 | 13022 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 13023 | `			ph7_case_expr sCase;` |
|         - | 13024 | `			/* Standard case block */` |
|     53521 | 13025 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 13026 | `			/* initialize the structure */` |
|     53521 | 13027 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 13028 | `			/* Compile the case expression */` |
|     53521 | 13029 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     53521 | 13030 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13031 | `				return SXERR_ABORT;` |
|         - | 13032 | `			}` |
|         - | 13033 | `			/* Compile the case block */` |
|     53521 | 13034 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 13035 | `			/* Insert in the switch container */` |
|     53521 | 13036 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     53521 | 13037 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13038 | `				return SXERR_ABORT;` |
|     53521 | 13039 | `			}else if( rc == SXERR_EOF ){` |
|      3829 | 13040 | `				break;` |
|         - | 13041 | `			}` |
|     24851 | 13042 | `		}else{` |
|         - | 13043 | `			/* Unexpected token */` |
|       ! 0 | 13044 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13045 | `				&pGen->pIn->sData);` |
|       ! 0 | 13046 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13047 | `				return SXERR_ABORT;` |
|         - | 13048 | `			}` |
|       ! 0 | 13049 | `			break;` |
|         - | 13050 | `		}` |
|         5 | 13051 | `	}` |
|         - | 13052 | `	/* Fix all jumps now the destination is resolved */` |
|      3849 | 13053 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      3849 | 13054 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 13055 | `	/* Release the loop block */` |
|      3849 | 13056 | `	GenStateLeaveBlock(pGen,0);` |
|      3849 | 13057 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 13058 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      3849 | 13059 | `		pGen->pIn++;` |
|      1922 | 13060 | `	}` |
|         - | 13061 | `	/* Statement successfully compiled */` |
|      3849 | 13062 | `	return SXRET_OK;` |
|       ! 0 | 13063 | `Synchronize:` |
|         - | 13064 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 13065 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 13066 | `		pGen->pIn++;` |
|       ! 0 | 13067 | `	}` |
|       ! 0 | 13068 | `	return SXRET_OK;` |
|      1927 | 13069 | `}` |
|         - | 13070 | `/*` |
|         - | 13071 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 13072 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 13073 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 13074 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 13075 | ` */` |
|         - | 13076 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 13077 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 13078 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 13079 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 13080 |  |
|         - | 13081 | `/*` |
|         - | 13082 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 13083 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 13084 | ` * patched entries from the pending set.` |
|         - | 13085 | ` */` |
|  48100132 | 13086 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 13087 | `{` |
|  48100137 | 13088 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 13089 | `	sxu32 nTarget;` |
|         - | 13090 | `	sxu32 *aIdx;` |
|         - | 13091 | `	sxu32 i;` |
|  48100137 | 13092 | `	if( nCur <= nBaseline ){` |
|  48100041 | 13093 | `		return;` |
|         - | 13094 | `	}` |
|       100 | 13095 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 13096 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 13097 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 13098 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 13099 | `		if( pInstr ){` |
|       108 | 13100 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 13101 | `		}` |
|        56 | 13102 | `	}` |
|       100 | 13103 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  24050071 | 13104 | `}` |
|         - | 13105 |  |
|         - | 13106 | `/*` |
|         - | 13107 | ` * By-reference out-parameters of builtin functions.` |
|         - | 13108 | ` *` |
|         - | 13109 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 13110 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 13111 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 13112 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 13113 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 13114 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 13115 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 13116 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 13117 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 13118 | ` * creates it" behaviour).` |
|         - | 13119 | ` *` |
|         - | 13120 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 13121 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 13122 | ` */` |
|   6120266 | 13123 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 13124 | `{` |
|         - | 13125 | `	static const struct {` |
|         - | 13126 | `		const char *zName;` |
|         - | 13127 | `		sxu32 nByte;` |
|         - | 13128 | `		sxu32 mask;` |
|         - | 13129 | `	} aByRef[] = {` |
|         - | 13130 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - | 13131 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13132 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13133 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13134 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13135 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 13136 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 13137 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 13138 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 13139 | `	};` |
|         - | 13140 | `	sxu32 i;` |
|   6120271 | 13141 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1618981 | 13142 | `		return 0;` |
|         - | 13143 | `	}` |
|  44596051 | 13144 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  40152140 | 13145 | `		if( pName->nByte == aByRef[i].nByte` |
|  21149274 | 13146 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     57389 | 13147 | `			return aByRef[i].mask;` |
|         - | 13148 | `		}` |
|  20047383 | 13149 | `	}` |
|   4443911 | 13150 | `	return 0;` |
|   3060138 | 13151 | `}` |
|         - | 13152 | `/*` |
|         - | 13153 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 13154 | ` *` |
|         - | 13155 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 13156 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 13157 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 13158 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 13159 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 13160 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 13161 | ` */` |
|   6120266 | 13162 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 13163 | `{` |
|         - | 13164 | `	SyToken *p, *pEnd;` |
|   6120271 | 13165 | `	pOut->zString = 0;` |
|   6120271 | 13166 | `	pOut->nByte = 0;` |
|   6120271 | 13167 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 13168 | `		return;` |
|         - | 13169 | `	}` |
|   6120271 | 13170 | `	p = pLeft->pStart;` |
|   6120271 | 13171 | `	pEnd = pLeft->pEnd;` |
|         - | 13172 | `	/* Optional single leading namespace separator (absolute path). */` |
|   6120271 | 13173 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3859 | 13174 | `		p++;` |
|      1927 | 13175 | `	}` |
|   6120271 | 13176 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1618943 | 13177 | `		return;` |
|         - | 13178 | `	}` |
|         - | 13179 | `	/* Must be a single component: nothing follows the name token. */` |
|   4501333 | 13180 | `	if( p + 1 != pEnd ){` |
|        42 | 13181 | `		return;` |
|         - | 13182 | `	}` |
|   4501295 | 13183 | `	*pOut = p->sData;` |
|   3060138 | 13184 | `}` |
|         - | 13185 | `/*` |
|         - | 13186 | ` * Generate bytecode for a given expression tree.` |
|         - | 13187 | ` * If something goes wrong while generating bytecode` |
|         - | 13188 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 13189 | ` * this function takes care of generating the appropriate` |
|         - | 13190 | ` * error message.` |
|         - | 13191 | ` */` |
|  66976196 | 13192 | `static sxi32 GenStateEmitExprCode(` |
|         - | 13193 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 13194 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 13195 | `	sxi32 iFlags /* Control flags */` |
|         - | 13196 | `	)` |
|         5 | 13197 | `{` |
|         - | 13198 | `	VmInstr *pInstr;` |
|         - | 13199 | `	sxu32 nJmpIdx;` |
|  66976201 | 13200 | `	sxi32 iP1 = 0;` |
|  66976201 | 13201 | `	sxu32 iP2 = 0;` |
|  66976201 | 13202 | `	void *p3  = 0;` |
|         - | 13203 | `	sxi32 iVmOp;` |
|         - | 13204 | `	sxi32 rc;` |
|  66976201 | 13205 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  66976201 | 13206 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  66976201 | 13207 | `	sxu32 nRhsNsBase = 0;` |
|  66976201 | 13208 | `	if( pNode->xCode ){` |
|         - | 13209 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13210 | `		/* Compile node */` |
|  40398347 | 13211 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  40398347 | 13212 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  40398347 | 13213 | `		RE_SWAP_DELIMITER(pGen);` |
|  40398347 | 13214 | `		return rc;` |
|         - | 13215 | `	}` |
|  26577859 | 13216 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13217 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13218 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13219 | `		return SXERR_ABORT;` |
|         - | 13220 | `	}` |
|  26577859 | 13221 | `	iVmOp = pNode->pOp->iVmOp;` |
|  26577859 | 13222 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13223 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13224 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13225 | `		 * and later errors are still reported. */` |
|         3 | 13226 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13227 | `			"The (unset) cast is no longer supported");` |
|         3 | 13228 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13229 | `			return SXERR_ABORT;` |
|         - | 13230 | `		}` |
|         1 | 13231 | `	}` |
|  26577859 | 13232 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        93 | 13233 | `		sxu32 nJmp = 0;` |
|         - | 13234 | `		sxu32 nNcNsBase;` |
|         - | 13235 | `		VmInstr *pInstrFix;` |
|         - | 13236 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13237 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13238 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13239 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13240 | `		 * stack slot carries a writable nIdx. */` |
|        93 | 13241 | `		if( pNode->pRight ){` |
|        93 | 13242 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13243 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        93 | 13244 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13245 | `				return rc;` |
|         - | 13246 | `			}` |
|        93 | 13247 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13248 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13249 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13250 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13251 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13252 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13253 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13254 | `			 * cascade for the actual write path stays correct. */` |
|        93 | 13255 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        93 | 13256 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13257 | `				pInstrFix->iP2 = 3;` |
|        15 | 13258 | `			}` |
|        45 | 13259 | `		}` |
|         - | 13260 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        93 | 13261 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13262 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        93 | 13263 | `		if( pNode->pLeft ){` |
|        93 | 13264 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13265 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        93 | 13266 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13267 | `				return rc;` |
|         - | 13268 | `			}` |
|        93 | 13269 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        45 | 13270 | `		}` |
|         - | 13271 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        93 | 13272 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13273 | `		/* Patch the short-circuit jump to land after the store. */` |
|        93 | 13274 | `		if( nJmp > 0 ){` |
|        93 | 13275 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        93 | 13276 | `			if( pInstrFix ){` |
|        93 | 13277 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        45 | 13278 | `			}` |
|        45 | 13279 | `		}` |
|        93 | 13280 | `		return SXRET_OK;` |
|         - | 13281 | `	}` |
|  26577769 | 13282 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13283 | `		sxu32 nJz,nJmp;` |
|         - | 13284 | `		sxu32 nTernaryNsBase;` |
|         - | 13285 | `		/* Ternary operator require special handling */` |
|         - | 13286 | `		/* Phase#1: Compile the condition */` |
|    453619 | 13287 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    453619 | 13288 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    453619 | 13289 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13290 | `			return rc;` |
|         - | 13291 | `		}` |
|         - | 13292 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13293 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13294 | `		 * condition expression, not leak past the ternary. */` |
|    453619 | 13295 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    453619 | 13296 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    453619 | 13297 | `		if( pNode->pLeft ){` |
|         - | 13298 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13299 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    449735 | 13300 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13301 | `			/* Phase#3: Compile the 'then' expression  */` |
|    449735 | 13302 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    449735 | 13303 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    449735 | 13304 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13305 | `				return rc;` |
|         - | 13306 | `			}` |
|    449735 | 13307 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    224870 | 13308 | `		}else{` |
|         - | 13309 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13310 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13311 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3889 | 13312 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3889 | 13313 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13314 | `		}` |
|         - | 13315 | `		/* Phase#4: Emit the unconditional jump */` |
|    453619 | 13316 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13317 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    453619 | 13318 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    453619 | 13319 | `		if( pInstr ){` |
|    453619 | 13320 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    226807 | 13321 | `		}` |
|    453619 | 13322 | `		if( !pNode->pLeft ){` |
|         - | 13323 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3889 | 13324 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1942 | 13325 | `		}` |
|         - | 13326 | `		/* Phase#6: Compile the 'else' expression */` |
|    453619 | 13327 | `		if( pNode->pRight ){` |
|    453619 | 13328 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    453619 | 13329 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    453619 | 13330 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13331 | `				return rc;` |
|         - | 13332 | `			}` |
|    453619 | 13333 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    226807 | 13334 | `		}` |
|    453619 | 13335 | `		if( nJmp > 0 ){` |
|         - | 13336 | `			/* Phase#7: Fix the unconditional jump */` |
|    453619 | 13337 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    453619 | 13338 | `			if( pInstr ){` |
|    453619 | 13339 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    226807 | 13340 | `			}` |
|    226807 | 13341 | `		}` |
|         - | 13342 | `		/* All done */` |
|    453619 | 13343 | `		return SXRET_OK;` |
|         - | 13344 | `	}` |
|  26124155 | 13345 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13346 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13347 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13348 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13349 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13350 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13351 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13352 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13353 | `		sxu32 nPipeNsBase;` |
|        27 | 13354 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13355 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13356 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13357 | `				"'\|>': Missing operand");` |
|       ! 0 | 13358 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13359 | `		}` |
|         - | 13360 | `		/* Argument: the LHS value. */` |
|        27 | 13361 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13362 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13363 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13364 | `			return rc;` |
|         - | 13365 | `		}` |
|        27 | 13366 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13367 | `		/* Callable: the RHS. */` |
|        27 | 13368 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13369 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13370 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13371 | `			return rc;` |
|         - | 13372 | `		}` |
|        27 | 13373 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13374 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13375 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13376 | `		return SXRET_OK;` |
|         - | 13377 | `	}` |
|  26124129 | 13378 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13379 | `	/* Generate code for the left tree */` |
|  26124129 | 13380 | `	if( pNode->pLeft ){` |
|  26101239 | 13381 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  26101239 | 13382 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13383 | `			ph7_expr_node **apNode;` |
|   6124403 | 13384 | `			int hasSpread = 0;` |
|   6124403 | 13385 | `			int hasNamed = 0;` |
|   6124403 | 13386 | `			int bAnySpread = 0;` |
|   6124403 | 13387 | `			sxu32 byRefMask = 0;` |
|         - | 13388 | `			sxi32 nArgs;` |
|         - | 13389 | `			sxi32 n;` |
|         - | 13390 | `			/* Recurse and generate bytecodes for function arguments */` |
|   6124403 | 13391 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6124403 | 13392 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13393 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13394 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13395 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   6124403 | 13396 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13397 | `				bFcc = 1;` |
|        81 | 13398 | `				nArgs = 0;` |
|        40 | 13399 | `			}` |
|         - | 13400 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13401 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13402 | `			{` |
|   6124403 | 13403 | `				int seenNamed = 0;` |
|   6124403 | 13404 | `				int seenSpread = 0;` |
|  12884953 | 13405 | `				for( n = 0; n < nArgs; ++n ){` |
|   6760557 | 13406 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4005 | 13407 | `						bAnySpread = 1;` |
|      4005 | 13408 | `						seenSpread = 1;` |
|      4005 | 13409 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13410 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13411 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13412 | `							return SXERR_SYNTAX;` |
|         5 | 13413 | `						}` |
|   6758557 | 13414 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13415 | `						seenNamed = 1;` |
|       289 | 13416 | `						hasNamed = 1;` |
|   6756415 | 13417 | `					}else if( seenNamed ){` |
|         3 | 13418 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13419 | `							"Cannot use positional argument after named argument");` |
|         3 | 13420 | `						return SXERR_SYNTAX;` |
|   6756271 | 13421 | `					}else if( seenSpread ){` |
|       ! 0 | 13422 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13423 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13424 | `						return SXERR_SYNTAX;` |
|         - | 13425 | `					}` |
|   3380280 | 13426 | `				}` |
|         - | 13427 | `			}` |
|         - | 13428 | `			/* Read-only load */` |
|   6124401 | 13429 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13430 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13431 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13432 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13433 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   6124401 | 13434 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   6124401 | 13435 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   6124396 | 13436 | `				if( pCallName->nByte == 5` |
|   3433769 | 13437 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    305657 | 13438 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5971575 | 13439 | `				}else if( pCallName->nByte == 5` |
|   3128117 | 13440 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       109 | 13441 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        52 | 13442 | `				}` |
|         - | 13443 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13444 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13445 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13446 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13447 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13448 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   6124401 | 13449 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13450 | `					SyString sBuiltin;` |
|   6120271 | 13451 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   6120271 | 13452 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3060133 | 13453 | `				}` |
|   3062198 | 13454 | `			}` |
|  12884949 | 13455 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   6760553 | 13456 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   6760553 | 13457 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13458 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13459 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13460 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13461 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13462 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13463 | `				 * (iP1=0 either way). */` |
|   6760553 | 13464 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38245 | 13465 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38245 | 13466 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19120 | 13467 | `				}` |
|   6760553 | 13468 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   6760553 | 13469 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13470 | `					return rc;` |
|         - | 13471 | `				}` |
|         - | 13472 | `				/* Each argument is an independent nullsafe scope. */` |
|   6760553 | 13473 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   6760553 | 13474 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13475 | `					/* Emit spread opcode to unpack this array argument */` |
|      4005 | 13476 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4005 | 13477 | `					hasSpread = 1;` |
|      2000 | 13478 | `				}` |
|   3380279 | 13479 | `			}` |
|         - | 13480 | `			/* Total number of given arguments */` |
|   6124401 | 13481 | `			iP1 = nArgs;` |
|   6124401 | 13482 | `			iP2 = hasSpread;` |
|         - | 13483 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13484 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   6124401 | 13485 | `			if( hasNamed ){` |
|       178 | 13486 | `				sxu32 nStrBytes = 0;` |
|         - | 13487 | `				char *zBuf;` |
|       534 | 13488 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13489 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13490 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13491 | `					}` |
|       182 | 13492 | `				}` |
|         - | 13493 | `				{` |
|       178 | 13494 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13495 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13496 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13497 | `				if( pMap ){` |
|       178 | 13498 | `					SyZero(pMap, mapSize);` |
|       178 | 13499 | `					pMap->bHasNamed = 1;` |
|       178 | 13500 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13501 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13502 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13503 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13504 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13505 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13506 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13507 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13508 | `							zBuf += nb;` |
|       141 | 13509 | `						}` |
|         - | 13510 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13511 | `					}` |
|       178 | 13512 | `					p3 = (void *)pMap;` |
|        87 | 13513 | `				}` |
|         - | 13514 | `				}` |
|        87 | 13515 | `			}` |
|         - | 13516 | `			/* Remove stale flags now */` |
|   6124401 | 13517 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3062198 | 13518 | `		}` |
|         - | 13519 | `		{` |
|         - | 13520 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13521 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13522 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13523 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13524 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13525 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13526 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13527 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  26101237 | 13528 | `			sxi32 iLeftFlags = iFlags;` |
|  26101232 | 13529 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  21466570 | 13530 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   8415980 | 13531 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7295735 | 13532 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2420377 | 13533 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1210186 | 13534 | `			}` |
|         - | 13535 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13536 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13537 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13538 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13539 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13540 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13541 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  26101232 | 13542 | `			if( pNode->pOp` |
|  36864844 | 13543 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  23814275 | 13544 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  21527266 | 13545 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4941285 | 13546 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2470640 | 13547 | `			}` |
|         - | 13548 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13549 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13550 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13551 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13552 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13553 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  26101232 | 13554 | `			if( pNode->pOp` |
|  26101237 | 13555 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    195119 | 13556 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     97557 | 13557 | `			}` |
|  26101237 | 13558 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13559 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13560 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     11671 | 13561 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      5833 | 13562 | `			}` |
|  26101237 | 13563 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13564 | `		}` |
|  26101237 | 13565 | `		if( rc != SXRET_OK ){` |
|        34 | 13566 | `			return rc;` |
|         - | 13567 | `		}` |
|  26101207 | 13568 | `		if( !bIsChainOp ){` |
|         - | 13569 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13570 | `			 * target the end of that LHS chain, which is right here. */` |
|  12189277 | 13571 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6094636 | 13572 | `		}` |
|  26101207 | 13573 | `		if( iVmOp == PH7_OP_CALL ){` |
|   6124401 | 13574 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6124401 | 13575 | `			if( pInstr ){` |
|   6124401 | 13576 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   4501573 | 13577 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13578 | `					sxu32 nQual;` |
|   4501573 | 13579 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13580 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13581 | `					 * so the later NEW handler (if any) can see it. */` |
|   4501573 | 13582 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13583 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13584 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13585 | `					 * imports — class imports must NOT affect function` |
|         - | 13586 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13587 | `					 * before NEW; we store the original literal index in the` |
|         - | 13588 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13589 | `					 * the unqualified name and re-qualify with class imports. */` |
|   4501573 | 13590 | `					if( bAbsolute ){` |
|      3859 | 13591 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1932 | 13592 | `					}else{` |
|   4497719 | 13593 | `						int fromImport = 0;` |
|   4497719 | 13594 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   4497719 | 13595 | `						pInstr->iP2 = (sxi32)nQual;` |
|   4497719 | 13596 | `						if( nQual != nOrig ){` |
|         - | 13597 | `							/* Record the original literal index in the arg map` |
|         - | 13598 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13599 | `							 * flag) so the NEW handler can recover the` |
|         - | 13600 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13601 | `							 * imports. */` |
|        97 | 13602 | `							if( p3 == 0 ){` |
|        97 | 13603 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        92 | 13604 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        97 | 13605 | `								if( pMap ){` |
|        97 | 13606 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        97 | 13607 | `									p3 = (void *)pMap;` |
|        46 | 13608 | `								}` |
|        46 | 13609 | `							}` |
|        97 | 13610 | `							if( p3 ){` |
|        97 | 13611 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        97 | 13612 | `								if( !fromImport ){` |
|         - | 13613 | `									/* Mark as namespace-qualified */` |
|        87 | 13614 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        41 | 13615 | `								}` |
|        46 | 13616 | `							}` |
|        46 | 13617 | `						}` |
|         5 | 13618 | `					}` |
|   3873617 | 13619 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13620 | `					/* Method call,flag that */` |
|   1603095 | 13621 | `					pInstr->iP2 = 1;` |
|    801545 | 13622 | `				}` |
|   3062203 | 13623 | `			}` |
|  23039009 | 13624 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13625 | `			ph7_expr_node **apNode;` |
|         - | 13626 | `			sxi32 n;` |
|   2846259 | 13627 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13628 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13629 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13630 | `			/* Recurse and generate bytecodes for array index */` |
|   2846259 | 13631 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5474607 | 13632 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2628353 | 13633 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2628353 | 13634 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2628353 | 13635 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13636 | `					return rc;` |
|         - | 13637 | `				}` |
|         - | 13638 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2628353 | 13639 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1314179 | 13640 | `			}` |
|   2846259 | 13641 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2628353 | 13642 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1314174 | 13643 | `			}` |
|   2846259 | 13644 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13645 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    343701 | 13646 | `				iP2 = 4;` |
|   2674411 | 13647 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13648 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13649 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     22977 | 13650 | `				iP2 = 5;` |
|   2491077 | 13651 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13652 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13653 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13654 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        31 | 13655 | `				iP2 = 6;` |
|   2479578 | 13656 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13657 | `				/* Create an empty entry when the desired index is not found */` |
|    519915 | 13658 | `				iP2 = 1;` |
|    259960 | 13659 | `			}` |
|  18553684 | 13660 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13661 | `			/* POP the left node */` |
|         5 | 13662 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13663 | `		}` |
|  13050601 | 13664 | `	}` |
|  26124097 | 13665 | `	rc = SXRET_OK;` |
|  26124097 | 13666 | `	nJmpIdx = 0;` |
|         - | 13667 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13668 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13669 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  26124097 | 13670 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    390157 | 13671 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    390157 | 13672 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    390157 | 13673 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    390157 | 13674 | `			int isSpecial = 0;` |
|    390157 | 13675 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    344349 | 13676 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    344349 | 13677 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    344344 | 13678 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    313744 | 13679 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    170230 | 13680 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    103185 | 13681 | `					isSpecial = 1;` |
|     51590 | 13682 | `				}` |
|    183624 | 13683 | `			}` |
|    413061 | 13684 | `			pInstr->iP1 = 0;` |
|         - | 13685 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 13686 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 13687 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 13688 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 13689 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 13690 | `			{` |
|    596685 | 13691 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    550872 | 13692 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    367253 | 13693 | `				if( !isSpecial && !bAbsolute ){` |
|    264057 | 13694 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    132026 | 13695 | `				}` |
|         - | 13696 | `			}` |
|         - | 13697 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13698 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    367253 | 13699 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    264073 | 13700 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    264073 | 13701 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        68 | 13702 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        70 | 13703 | `					return SXRET_OK;` |
|         - | 13704 | `				}` |
|    132001 | 13705 | `			}` |
|    183591 | 13706 | `		}` |
|    229378 | 13707 | `	}` |
|         - | 13708 | `	/* Generate code for the right tree */` |
|  26101141 | 13709 | `	if( pNode->pRight ){` |
|  14999527 | 13710 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13711 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    412657 | 13712 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14793201 | 13713 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13714 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    282555 | 13715 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14445600 | 13716 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13717 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     57401 | 13718 | `			iVmOp = 0; /* No binary operator to emit */` |
|     57401 | 13719 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  14275679 | 13720 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13721 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13722 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13723 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13724 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13725 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13726 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13727 | `			sxu32 nNsJmp = 0;` |
|       108 | 13728 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13729 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  14246877 | 13730 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13731 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13732 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13733 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4762999 | 13734 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2381497 | 13735 | `		}` |
|  14999527 | 13736 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  14999527 | 13737 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  14999527 | 13738 | `		if( !bIsChainOp ){` |
|         - | 13739 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13740 | `			 * operator instruction is emitted. */` |
|  10058313 | 13741 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5029154 | 13742 | `		}` |
|  14999527 | 13743 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4327645 | 13744 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4327608 | 13745 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13746 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13747 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13748 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13749 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13750 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13751 | `				 */` |
|        91 | 13752 | `				iVmOp = 0;` |
|   4327602 | 13753 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4327559 | 13754 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13755 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    779181 | 13756 | `					iP2 = 1;` |
|    389593 | 13757 | `				}else{` |
|   3548383 | 13758 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13759 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    500737 | 13760 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    500737 | 13761 | `						iP1 = pInstr->iP1;` |
|    250371 | 13762 | `					}else{` |
|   3047651 | 13763 | `						p3 = pInstr->p3;` |
|         - | 13764 | `					}` |
|         - | 13765 | `					/* POP the last dynamic load instruction */` |
|   3548383 | 13766 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13767 | `				}` |
|   2163782 | 13768 | `			}` |
|  12835707 | 13769 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13770 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13771 | `			if( pInstr ){` |
|        63 | 13772 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13773 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13774 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13775 | `					 */` |
|        19 | 13776 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13777 | `					iP1 = pInstr->iP1;` |
|        19 | 13778 | `					iP2 = pInstr->iP2;` |
|        19 | 13779 | `					p3  = pInstr->p3;` |
|        10 | 13780 | `				}else{` |
|        45 | 13781 | `					p3 = pInstr->p3;` |
|         - | 13782 | `				}` |
|        30 | 13783 | `			}` |
|        30 | 13784 | `		}` |
|   7499761 | 13785 | `	}` |
|  26101136 | 13786 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    375641 | 13787 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13788 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13789 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13790 | `		iVmOp = 0;` |
|        14 | 13791 | `	}` |
|  26101141 | 13792 | `	if( iVmOp > 0 ){` |
|  26043627 | 13793 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    195119 | 13794 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13795 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15301 | 13796 | `				iP1 = 1;` |
|      7653 | 13797 | `			}` |
|  25946070 | 13798 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13799 | `			/* Namespace-qualify the class name for NEW */ {` |
|    750909 | 13800 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    750909 | 13801 | `				VmInstr *pCallInstr = 0;` |
|    750909 | 13802 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    750597 | 13803 | `					pCallInstr = pPeek;` |
|    750597 | 13804 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    375296 | 13805 | `				}` |
|    750909 | 13806 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    735641 | 13807 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13808 | `					sxu32 nLitForClass;` |
|    735641 | 13809 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13810 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13811 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13812 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13813 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13814 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13815 | `					 * with class imports. */` |
|    735641 | 13816 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        53 | 13817 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        29 | 13818 | `					}else{` |
|    735593 | 13819 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13820 | `					}` |
|    735641 | 13821 | `					pPeek->iP1 = 0;` |
|    735641 | 13822 | `					if( !bAbsolute ){` |
|         - | 13823 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 13824 | `						 * current class — never namespace-qualify them (else` |
|         - | 13825 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 13826 | `						 * instanceof (IS_A) guard below. */` |
|    731797 | 13827 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    731797 | 13828 | `						int isSpecialNew = 0;` |
|    731797 | 13829 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    731797 | 13830 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    731797 | 13831 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    731792 | 13832 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    731839 | 13833 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    365943 | 13834 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        29 | 13835 | `								isSpecialNew = 1;` |
|        14 | 13836 | `							}` |
|    365896 | 13837 | `						}` |
|    731797 | 13838 | `						if( isSpecialNew ){` |
|        29 | 13839 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|        15 | 13840 | `						}else{` |
|    731769 | 13841 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 13842 | `						}` |
|    365901 | 13843 | `					}else{` |
|      3849 | 13844 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13845 | `					}` |
|    367818 | 13846 | `				}` |
|         - | 13847 | `			}` |
|    750909 | 13848 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    750909 | 13849 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13850 | `				VmInstr *pPrev;` |
|    750597 | 13851 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    750597 | 13852 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13853 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13854 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13855 | `					 * accumulator exactly like OP_CALL would have). */` |
|    750597 | 13856 | `					iP1 = pInstr->iP1;` |
|    750597 | 13857 | `					iP2 = pInstr->iP2;` |
|    750597 | 13858 | `					if( pInstr->p3 ){` |
|        63 | 13859 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        29 | 13860 | `					}` |
|    750597 | 13861 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    375296 | 13862 | `				}` |
|    375301 | 13863 | `			}` |
|  25473061 | 13864 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13865 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13866 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     76585 | 13867 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     76585 | 13868 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     76585 | 13869 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     76585 | 13870 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     76585 | 13871 | `				int isSpecialIs = 0;` |
|     76585 | 13872 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     76585 | 13873 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     76585 | 13874 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     76580 | 13875 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     76583 | 13876 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     38290 | 13877 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13878 | `						isSpecialIs = 1;` |
|         5 | 13879 | `					}` |
|     38290 | 13880 | `				}` |
|     76585 | 13881 | `				pInstr->iP1 = 0;` |
|     76585 | 13882 | `				if( !isSpecialIs && !bAbsolute ){` |
|     76565 | 13883 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     38280 | 13884 | `				}` |
|     38295 | 13885 | `			}` |
|  25059319 | 13886 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13887 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13888 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13889 | `			 * should not trigger constant lookup. */` |
|   4941219 | 13890 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4941219 | 13891 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4708361 | 13892 | `				pInstr->iP1 = 0;` |
|   2354178 | 13893 | `			}` |
|   4941219 | 13894 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13895 | `				/* Static member access,remember that */` |
|    367201 | 13896 | `				iP1 = 1;` |
|    367201 | 13897 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    367201 | 13898 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    229031 | 13899 | `					p3 = pInstr->p3;` |
|    229031 | 13900 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    114513 | 13901 | `				}` |
|    183598 | 13902 | `			}` |
|         - | 13903 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13904 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13905 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13906 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4941219 | 13907 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4941219 | 13908 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13909 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4941199 | 13910 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     61159 | 13911 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4910602 | 13912 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13913 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4880017 | 13914 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13915 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    947271 | 13916 | `					iP2 = PH7_MEMBER_WRITE;` |
|    473633 | 13917 | `				}` |
|   2470607 | 13918 | `			}` |
|   2470607 | 13919 | `		}` |
|         - | 13920 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13921 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13922 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13923 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13924 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  26043627 | 13925 | `		if( bFcc ){` |
|        81 | 13926 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13927 | `			iP2 = 0;` |
|        81 | 13928 | `			p3 = 0;` |
|        81 | 13929 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13930 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13931 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13932 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13933 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13934 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13935 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13936 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13937 | `				if( pMemberName ){` |
|         3 | 13938 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13939 | `				}` |
|        37 | 13940 | `				iP1 = 2;` |
|        19 | 13941 | `			}else{` |
|        45 | 13942 | `				iP1 = 1;` |
|         - | 13943 | `			}` |
|        40 | 13944 | `		}` |
|         - | 13945 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13946 | `		 * This is the primary emit path for user-visible calls. */` |
|  26043627 | 13947 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6875225 | 13948 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3437610 | 13949 | `		}` |
|         - | 13950 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  26043627 | 13951 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  13021811 | 13952 | `	}` |
|  26101141 | 13953 | `	if( nJmpIdx > 0 ){` |
|         - | 13954 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    752603 | 13955 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    752603 | 13956 | `		if( pInstr ){` |
|    752603 | 13957 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    376299 | 13958 | `		}` |
|    376299 | 13959 | `	}` |
|  26101141 | 13960 | `	return rc;` |
|  33476658 | 13961 | `}` |
|         - | 13962 | `/*` |
|         - | 13963 | ` * Compile a PHP expression.` |
|         - | 13964 | ` * According to the PHP language reference manual:` |
|         - | 13965 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 13966 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 13967 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 13968 | ` *  is "anything that has a value".` |
|         - | 13969 | ` * If something goes wrong while compiling the expression,this` |
|         - | 13970 | ` * function takes care of generating the appropriate error` |
|         - | 13971 | ` * message.` |
|         - | 13972 | ` */` |
|         - | 13973 | `/*` |
|         - | 13974 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 13975 | ` *` |
|         - | 13976 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 13977 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 13978 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 13979 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 13980 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 13981 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 13982 | ` * except for() now reports php's parse error.` |
|         - | 13983 | ` */` |
| 221906054 | 13984 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 13985 | `{` |
|         - | 13986 | `	ph7_expr_node **apArg;` |
|         - | 13987 | `	sxu32 n;` |
| 221906059 | 13988 | `	if( pNode == 0 ){` |
| 155984077 | 13989 | `		return 0;` |
|         - | 13990 | `	}` |
|  65921987 | 13991 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 13992 | `		return 1;` |
|         - | 13993 | `	}` |
|  65921978 | 13994 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  65921979 | 13995 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 13996 | `		return 1;` |
|         - | 13997 | `	}` |
|  65921979 | 13998 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  75288059 | 13999 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   9366085 | 14000 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 14001 | `			return 1;` |
|         - | 14002 | `		}` |
|   4683045 | 14003 | `	}` |
|  65921979 | 14004 | `	return 0;` |
| 110953032 | 14005 | `}` |
|  15129558 | 14006 | `static sxi32 PH7_CompileExpr(` |
|         - | 14007 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14008 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 14009 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 14010 | `	)` |
|         5 | 14011 | `{` |
|         - | 14012 | `	ph7_expr_node *pRoot;` |
|         - | 14013 | `	SySet sExprNode;` |
|         - | 14014 | `	SyToken *pEnd;` |
|         - | 14015 | `	sxi32 nExpr;` |
|         - | 14016 | `	sxi32 iNest;` |
|         - | 14017 | `	sxi32 rc;` |
|         - | 14018 | `	sxu32 nNullsafeBase;` |
|         - | 14019 | `	/* Initialize worker variables */` |
|  15129563 | 14020 | `	nExpr = 0;` |
|  15129563 | 14021 | `	pRoot = 0;` |
|         - | 14022 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 14023 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  15129563 | 14024 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15129563 | 14025 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  15129563 | 14026 | `	SySetAlloc(&sExprNode,0x10);` |
|  15129563 | 14027 | `	rc = SXRET_OK;` |
|         - | 14028 | `	/* Delimit the expression */` |
|  15129563 | 14029 | `	pEnd = pGen->pIn;` |
|  15129563 | 14030 | `	iNest = 0;` |
| 118354821 | 14031 | `	while( pEnd < pGen->pEnd ){` |
| 112478099 | 14032 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14033 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4641 | 14034 | `			iNest++;` |
| 112475781 | 14035 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4649 | 14036 | `			iNest--;` |
| 112471141 | 14037 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   9253699 | 14038 | `			if( iNest <= 0 ){` |
|   9252841 | 14039 | `				break;` |
|         - | 14040 | `			}` |
|       429 | 14041 | `		}` |
| 103225263 | 14042 | `		pEnd++;` |
|         5 | 14043 | `	}` |
|  15129563 | 14044 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    642125 | 14045 | `		SyToken *pEnd2 = pGen->pIn;` |
|    642125 | 14046 | `		iNest = 0;` |
|         - | 14047 | `		/* Stop at the first comma */` |
|   1411465 | 14048 | `		while( pEnd2 < pEnd ){` |
|    769347 | 14049 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     42093 | 14050 | `				iNest++;` |
|    748303 | 14051 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     42093 | 14052 | `				iNest--;` |
|    706215 | 14053 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 14054 | `				if( iNest <= 0 ){` |
|         3 | 14055 | `					break;` |
|         - | 14056 | `				}` |
|      3027 | 14057 | `			}` |
|    769345 | 14058 | `			pEnd2++;` |
|         5 | 14059 | `		}` |
|    642125 | 14060 | `		if( pEnd2 <pEnd ){` |
|         3 | 14061 | `			pEnd = pEnd2;` |
|         1 | 14062 | `		}` |
|    321060 | 14063 | `	}` |
|  15129563 | 14064 | `	if( pEnd > pGen->pIn ){` |
|  15106659 | 14065 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 14066 | `		/* Swap delimiter */` |
|  15106659 | 14067 | `		pGen->pEnd = pEnd;` |
|         - | 14068 | `		/* Try to get an expression tree */` |
|  15106659 | 14069 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  15106654 | 14070 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  14940264 | 14071 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 14072 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 14073 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 14074 | `				"syntax error, unexpected token \",\"");` |
|         6 | 14075 | `			pGen->pEnd = pTmp;` |
|         6 | 14076 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14077 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 14078 | `				return SXERR_ABORT;` |
|         - | 14079 | `			}` |
|         6 | 14080 | `			pGen->pIn = pEnd;` |
|         6 | 14081 | `			SySetRelease(&sExprNode);` |
|         6 | 14082 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 14083 | `			return SXRET_OK;` |
|         - | 14084 | `		}` |
|  15106655 | 14085 | `		if( rc == SXRET_OK && pRoot ){` |
|  15106471 | 14086 | `			rc = SXRET_OK;` |
|  15106471 | 14087 | `			if( xTreeValidator ){` |
|         - | 14088 | `				/* Call the upper layer validator callback */` |
|    967275 | 14089 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    483635 | 14090 | `			}` |
|  15106471 | 14091 | `			if( rc != SXERR_ABORT ){` |
|         - | 14092 | `				/* Generate code for the given tree */` |
|  15106471 | 14093 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 14094 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 14095 | `				 * expression so they short-circuit to its end. */` |
|  15106471 | 14096 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   7553233 | 14097 | `			}` |
|  15106471 | 14098 | `			nExpr = 1;` |
|   7553233 | 14099 | `		}` |
|         - | 14100 | `		/* Release the whole tree */` |
|  15106655 | 14101 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 14102 | `		/* Synchronize token stream */` |
|  15106655 | 14103 | `		pGen->pEnd = pTmp;` |
|  15106655 | 14104 | `		pGen->pIn  = pEnd;` |
|  15106655 | 14105 | `		if( rc == SXERR_ABORT ){` |
|        12 | 14106 | `			SySetRelease(&sExprNode);` |
|        12 | 14107 | `			return SXERR_ABORT;` |
|         - | 14108 | `		}` |
|   7553320 | 14109 | `	}` |
|  15129549 | 14110 | `	SySetRelease(&sExprNode);` |
|  15129549 | 14111 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   7564784 | 14112 | `}` |
|         - | 14113 | `/*` |
|         - | 14114 | ` * Return a pointer to the node construct handler associated` |
|         - | 14115 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 14116 | ` */` |
|   8752830 | 14117 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 14118 | `{` |
|   8752835 | 14119 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 14120 | `		/* Numeric literal: Either real or integer */` |
|   3556171 | 14121 | `		return PH7_CompileNumLiteral;` |
|   5196669 | 14122 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 14123 | `		/* Double quoted string */` |
|    119939 | 14124 | `		return PH7_CompileString;` |
|   5076735 | 14125 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 14126 | `		/* Single quoted string */` |
|   5076615 | 14127 | `		return PH7_CompileSimpleString;` |
|       124 | 14128 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 14129 | `		/* Heredoc */` |
|        70 | 14130 | `		return PH7_CompileHereDoc;` |
|        58 | 14131 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 14132 | `		/* Nowdoc */` |
|        52 | 14133 | `		return PH7_CompileNowDoc;` |
|         8 | 14134 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 14135 | `		/* Backtick quoted string */` |
|         6 | 14136 | `		return PH7_CompileBacktic;` |
|         - | 14137 | `	}` |
|         3 | 14138 | `	return 0;` |
|   4376420 | 14139 | `}` |
|         - | 14140 | `/*` |
|         - | 14141 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 14142 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 14143 | ` * in write context" parse error.` |
|         - | 14144 | ` */` |
|     23014 | 14145 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 14146 | `{` |
|         - | 14147 | `	sxi32 rc;` |
|     23019 | 14148 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23017 | 14149 | `		return SXRET_OK;` |
|         - | 14150 | `	}` |
|         5 | 14151 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 14152 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 14153 | `		"Can't use nullsafe operator in write context");` |
|         3 | 14154 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11512 | 14155 | `}` |
|         - | 14156 | `/*` |
|         - | 14157 | ` * Compile an unset() statement.` |
|         - | 14158 | ` * unset($var, $arr[$key], ...);` |
|         - | 14159 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 14160 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 14161 | ` * parent array before extracting the element to unset.` |
|         - | 14162 | ` */` |
|     25864 | 14163 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 14164 | `{` |
|     25869 | 14165 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     25869 | 14166 | `	sxu32 nIdx = 0;` |
|         - | 14167 | `	SyString sName;` |
|         - | 14168 | `	sxi32 rc;` |
|         - | 14169 | `	/* Jump the 'unset' keyword */` |
|     25869 | 14170 | `	pGen->pIn++;` |
|         - | 14171 | `	/* Save delimiter */` |
|     25869 | 14172 | `	pTmp = pGen->pEnd;` |
|         - | 14173 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     25869 | 14174 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     25869 | 14175 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14176 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 14177 | `		SyToken *pClose;` |
|     25869 | 14178 | `		pGen->pIn++;   /* Skip '(' */` |
|     25869 | 14179 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     25869 | 14180 | `		pEnd = pClose; /* Stop at ')' */` |
|     12932 | 14181 | `	}` |
|     25869 | 14182 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 14183 | `	/* Resolve the 'unset' builtin name once */` |
|     25869 | 14184 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3821 | 14185 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3821 | 14186 | `		if( pObj == 0 ){` |
|       ! 0 | 14187 | `			return SXERR_ABORT;` |
|         - | 14188 | `		}` |
|      3821 | 14189 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3821 | 14190 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1908 | 14191 | `	}` |
|         - | 14192 | `	/* Compile each comma-separated argument */` |
|     55959 | 14193 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30095 | 14194 | `		if( pGen->pIn < pNext ){` |
|         - | 14195 | `			/*` |
|         - | 14196 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 14197 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 14198 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 14199 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 14200 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 14201 | `			 * already removes just the element/property.` |
|         - | 14202 | `			 */` |
|     30090 | 14203 | `			if( &pGen->pIn[2] == pNext` |
|     18583 | 14204 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      7081 | 14205 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 14206 | `				SyString *pVarName;` |
|     10616 | 14207 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      7074 | 14208 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      7079 | 14209 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      7079 | 14210 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 14211 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 14212 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 14213 | `					return SXERR_ABORT;` |
|         - | 14214 | `				}` |
|      7079 | 14215 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      7079 | 14216 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      7079 | 14217 | `				pGen->pIn = pNext;` |
|      7079 | 14218 | `				if( pGen->pIn < pEnd ){` |
|      4227 | 14219 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2111 | 14220 | `				}` |
|      7079 | 14221 | `				continue;` |
|         - | 14222 | `			}` |
|     23021 | 14223 | `			pGen->pEnd = pNext;` |
|     23021 | 14224 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 14225 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 14226 | `				GenStateUnsetValidator);` |
|     23021 | 14227 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14228 | `				return SXERR_ABORT;` |
|         - | 14229 | `			}` |
|     23021 | 14230 | `			if( rc != SXERR_EMPTY ){` |
|         - | 14231 | `				/* Emit call for this single argument */` |
|     23019 | 14232 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23019 | 14233 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23019 | 14234 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11507 | 14235 | `			}` |
|     11508 | 14236 | `		}` |
|         - | 14237 | `		/* Jump trailing commas */` |
|     23027 | 14238 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 14239 | `			pNext++;` |
|         1 | 14240 | `		}` |
|     23021 | 14241 | `		pGen->pIn = pNext;` |
|         5 | 14242 | `	}` |
|         - | 14243 | `	/* Skip past the closing ')' if present */` |
|     25869 | 14244 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     25869 | 14245 | `		pGen->pIn++;` |
|     12932 | 14246 | `	}` |
|         - | 14247 | `	/* Restore token stream */` |
|     25869 | 14248 | `	pGen->pEnd = pTmp;` |
|     25869 | 14249 | `	return SXRET_OK;` |
|     12937 | 14250 | `}` |
|         - | 14251 | `/*` |
|         - | 14252 | ` * PHP Language construct table.` |
|         - | 14253 | ` */` |
|         - | 14254 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 14255 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 14256 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14257 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14258 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14259 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14260 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14261 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14262 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14263 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14264 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14265 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14266 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14267 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14268 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14269 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14270 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14271 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14272 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14273 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14274 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14275 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14276 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14277 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14278 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14279 | `};` |
|         - | 14280 | `/*` |
|         - | 14281 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14282 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14283 | ` */` |
|   7317522 | 14284 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14285 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14286 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14287 | `	)` |
|         5 | 14288 | `{` |
|   7317527 | 14289 | `	sxu32 n = 0;` |
|  28989860 | 14290 | `	for(;;){` |
|  57979725 | 14291 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    429743 | 14292 | `			break;` |
|         - | 14293 | `		}` |
|  57549987 | 14294 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6887789 | 14295 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14296 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14297 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14298 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14299 | `					return 0;` |
|         - | 14300 | `				}` |
|       ! 0 | 14301 | `			}` |
|   6887784 | 14302 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|      7646 | 14303 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      3830 | 14304 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14305 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14306 | `				return 0;` |
|         - | 14307 | `			}` |
|         - | 14308 | `			/* Return a pointer to the handler.` |
|         - | 14309 | `			*/` |
|   6887787 | 14310 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14311 | `		}` |
|  50662203 | 14312 | `		n++;` |
|         5 | 14313 | `	}` |
|    429743 | 14314 | `	if( pLookahed ){` |
|    429743 | 14315 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     68803 | 14316 | `			return PH7_CompileClassInterface;` |
|    360945 | 14317 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    314595 | 14318 | `			return PH7_CompileClass;` |
|     46355 | 14319 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7715 | 14320 | `			return PH7_CompileTrait;` |
|         - | 14321 | `		}` |
|         - | 14322 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14323 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14324 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14325 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19320 | 14326 | `	}` |
|         - | 14327 | `	/* Not a language construct */` |
|     38645 | 14328 | `	return 0;` |
|   3658766 | 14329 | `}` |
|         - | 14330 | `/*` |
|         - | 14331 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14332 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14333 | ` */` |
|     38642 | 14334 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14335 | `{` |
|         - | 14336 | `	int rc;` |
|     38647 | 14337 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38647 | 14338 | `	if( rc == FALSE ){` |
|     38536 | 14339 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15632 | 14340 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14341 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14342 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14343 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14344 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14345 | `			*/` |
|         - | 14346 | `			){` |
|     38533 | 14347 | `				rc = TRUE;` |
|     19264 | 14348 | `		}` |
|     19268 | 14349 | `	}` |
|     38647 | 14350 | `	return rc;` |
|         5 | 14351 | `}` |
|         - | 14352 | `/*` |
|         - | 14353 | ` * Compile a PHP chunk.` |
|         - | 14354 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14355 | ` * takes care of generating the appropriate error message.` |
|         - | 14356 | ` */` |
|         - | 14357 | `/*` |
|         - | 14358 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14359 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14360 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14361 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14362 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14363 | ` * intervening non-declaration statements.` |
|         - | 14364 | ` */` |
|  15787444 | 14365 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14366 | `{` |
|  15787449 | 14367 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  15787449 | 14368 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  15787449 | 14369 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14370 | `	sxu32 nIdx, n;` |
|  15787444 | 14371 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3277037 | 14372 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14373 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14374 | `		 * indexes do not map to the sidecar */` |
|  12510419 | 14375 | `		return;` |
|         - | 14376 | `	}` |
|   3277035 | 14377 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14378 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14379 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3277035 | 14380 | `	SySetReset(&pGen->aPendingAttrs);` |
|   9832589 | 14381 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6555559 | 14382 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6547763 | 14383 | `			continue;` |
|         - | 14384 | `		}` |
|      7801 | 14385 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14386 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7789 | 14387 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7777 | 14388 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3886 | 14389 | `		}` |
|      3903 | 14390 | `	}` |
|   7893727 | 14391 | `}` |
|         - | 14392 | `/*` |
|         - | 14393 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14394 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14395 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14396 | ` */` |
|   4063352 | 14397 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14398 | `{` |
|         - | 14399 | `	char *zDup;` |
|   4063357 | 14400 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4063337 | 14401 | `		return;` |
|         - | 14402 | `	}` |
|        35 | 14403 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14404 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14405 | `	if( zDup ){` |
|        25 | 14406 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14407 | `	}` |
|        25 | 14408 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2031681 | 14409 | `}` |
|         - | 14410 | `/*` |
|         - | 14411 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14412 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14413 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14414 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14415 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14416 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14417 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14418 | ` */` |
|      7784 | 14419 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14420 | `{` |
|         - | 14421 | `	SySet *pToken;` |
|         - | 14422 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14423 | `	char *zSpan;` |
|      7789 | 14424 | `	sxi32 rc = SXRET_OK;` |
|      7789 | 14425 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14426 | `		return SXRET_OK;` |
|         - | 14427 | `	}` |
|     11681 | 14428 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3892 | 14429 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7789 | 14430 | `	if( zSpan == 0 ){` |
|       ! 0 | 14431 | `		return SXRET_OK;` |
|         - | 14432 | `	}` |
|         - | 14433 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14434 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14435 | `	 * the number of attribute declarations in the program. */` |
|      7789 | 14436 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7789 | 14437 | `	if( pToken == 0 ){` |
|       ! 0 | 14438 | `		return SXRET_OK;` |
|         - | 14439 | `	}` |
|      7789 | 14440 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7789 | 14441 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7789 | 14442 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7789 | 14443 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7789 | 14444 | `	pSavedIn = pGen->pIn;` |
|      7789 | 14445 | `	pSavedEnd = pGen->pEnd;` |
|      7793 | 14446 | `	while( pIn < pEnd ){` |
|         - | 14447 | `		ph7_attribute sAttr;` |
|         - | 14448 | `		SyBlob sFQN;` |
|      7793 | 14449 | `		int bAbsolute = 0;` |
|      7793 | 14450 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7793 | 14451 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7793 | 14452 | `		sAttr.nLine = pIn->nLine;` |
|      7793 | 14453 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14454 | `			bAbsolute = 1;` |
|        75 | 14455 | `			pIn++;` |
|        35 | 14456 | `		}` |
|      7793 | 14457 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7793 | 14458 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7793 | 14459 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7793 | 14460 | `			pIn++;` |
|      7793 | 14461 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14462 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14463 | `				pIn++;` |
|       ! 0 | 14464 | `				continue;` |
|         - | 14465 | `			}` |
|      7793 | 14466 | `			break;` |
|       ! 0 | 14467 | `		}` |
|      7793 | 14468 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14469 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14470 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14471 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14472 | `			break;` |
|         - | 14473 | `		}` |
|         - | 14474 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14475 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14476 | `		{` |
|      7793 | 14477 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7793 | 14478 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7793 | 14479 | `			char *zDup = 0;` |
|      7793 | 14480 | `			if( !bAbsolute ){` |
|      7723 | 14481 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7723 | 14482 | `				if( pImp ){` |
|       ! 0 | 14483 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14484 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14485 | `					if( zDup ){` |
|       ! 0 | 14486 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14487 | `					}` |
|      7723 | 14488 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14489 | `					SyBlob sTmp;` |
|       ! 0 | 14490 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14491 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14492 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14493 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14494 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14495 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14496 | `					if( zDup ){` |
|       ! 0 | 14497 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14498 | `					}` |
|       ! 0 | 14499 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14500 | `				}` |
|      3859 | 14501 | `			}` |
|      7793 | 14502 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7793 | 14503 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7793 | 14504 | `				if( zDup ){` |
|      7793 | 14505 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3894 | 14506 | `				}` |
|      3894 | 14507 | `			}` |
|         - | 14508 | `		}` |
|      7793 | 14509 | `		SyBlobRelease(&sFQN);` |
|      7793 | 14510 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14511 | `			SyToken *pArgsEnd;` |
|      7691 | 14512 | `			pIn++;` |
|      7691 | 14513 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15391 | 14514 | `			while( pIn < pArgsEnd ){` |
|      7705 | 14515 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7705 | 14516 | `				sxi32 iDepth = 0;` |
|         - | 14517 | `				ph7_attr_arg sArgRec;` |
|     76565 | 14518 | `				while( pArgStop < pArgsEnd ){` |
|     68881 | 14519 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14520 | `						iDepth++;` |
|     68876 | 14521 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14522 | `						iDepth--;` |
|     68866 | 14523 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14524 | `						break;` |
|         - | 14525 | `					}` |
|     68865 | 14526 | `					pArgStop++;` |
|         5 | 14527 | `				}` |
|      7705 | 14528 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7705 | 14529 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7700 | 14530 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7684 | 14531 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14532 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14533 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14534 | `					if( zN ){` |
|        19 | 14535 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14536 | `					}` |
|        19 | 14537 | `					pArgStart += 2;` |
|         9 | 14538 | `				}` |
|      7705 | 14539 | `				if( pArgStart < pArgStop ){` |
|         - | 14540 | `					SySet *pInstrContainer;` |
|      7705 | 14541 | `					pGen->pIn = pArgStart;` |
|      7705 | 14542 | `					pGen->pEnd = pArgStop;` |
|      7705 | 14543 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7705 | 14544 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7705 | 14545 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7705 | 14546 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7705 | 14547 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7705 | 14548 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14549 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14550 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14551 | `						return SXERR_ABORT;` |
|         - | 14552 | `					}` |
|      7705 | 14553 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3850 | 14554 | `				}` |
|      7705 | 14555 | `				pIn = pArgStop;` |
|      7705 | 14556 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14557 | `					pIn++;` |
|         8 | 14558 | `				}` |
|         5 | 14559 | `			}` |
|      7691 | 14560 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3843 | 14561 | `		}` |
|      7793 | 14562 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7793 | 14563 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14564 | `			pIn++;` |
|         5 | 14565 | `			continue;` |
|         - | 14566 | `		}` |
|      7789 | 14567 | `		break;` |
|       ! 0 | 14568 | `	}` |
|      7789 | 14569 | `	pGen->pIn = pSavedIn;` |
|      7789 | 14570 | `	pGen->pEnd = pSavedEnd;` |
|      7789 | 14571 | `	return SXRET_OK;` |
|      3897 | 14572 | `}` |
|         - | 14573 | `/*` |
|         - | 14574 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14575 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14576 | ` */` |
|   4063356 | 14577 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14578 | `{` |
|   4063361 | 14579 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14580 | `	sxu32 n;` |
|         - | 14581 | `	sxi32 rc;` |
|   4071133 | 14582 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7777 | 14583 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7777 | 14584 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14585 | `			return SXERR_ABORT;` |
|         - | 14586 | `		}` |
|      3891 | 14587 | `	}` |
|   4063361 | 14588 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4063361 | 14589 | `	return SXRET_OK;` |
|   2031683 | 14590 | `}` |
|         - | 14591 | `/*` |
|         - | 14592 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14593 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14594 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14595 | ` */` |
|   2049404 | 14596 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14597 | `{` |
|   2049409 | 14598 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2049409 | 14599 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2049409 | 14600 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14601 | `	sxu32 nIdx, n;` |
|         - | 14602 | `	sxi32 rc;` |
|   2049404 | 14603 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    546023 | 14604 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1503391 | 14605 | `		return SXRET_OK;` |
|         - | 14606 | `	}` |
|    546023 | 14607 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1638057 | 14608 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1092039 | 14609 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14610 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14611 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14612 | `				return SXERR_ABORT;` |
|         - | 14613 | `			}` |
|         6 | 14614 | `		}` |
|    546022 | 14615 | `	}` |
|    546023 | 14616 | `	return SXRET_OK;` |
|   1024707 | 14617 | `}` |
|  11750380 | 14618 | `static sxi32 GenStateCompileChunk(` |
|         - | 14619 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14620 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14621 | `	)` |
|         5 | 14622 | `{` |
|         - | 14623 | `	ProcLangConstruct xCons;` |
|         - | 14624 | `	sxi32 rc;` |
|  11750385 | 14625 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   6824842 | 14626 | `	for(;;){` |
|  12700037 | 14627 | `		int bStmtIsDeclare = 0;` |
|  12700037 | 14628 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14629 | `			/* No more input to process */` |
|     67507 | 14630 | `			break;` |
|         - | 14631 | `		}` |
|         - | 14632 | `		/* Bind a directly-preceding docblock to this statement */` |
|  12632535 | 14633 | `		GenStateSetPendingDoc(&(*pGen));` |
|  12632535 | 14634 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14635 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14636 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14637 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14638 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14639 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7695 | 14640 | `			int bAttrTarget = 0;` |
|      7690 | 14641 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3879 | 14642 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7637 | 14643 | `				bAttrTarget = 1;` |
|      3875 | 14644 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14645 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14646 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14647 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14648 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14649 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14650 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14651 | `					bAttrTarget = 1;` |
|        29 | 14652 | `				}` |
|        29 | 14653 | `			}` |
|      7695 | 14654 | `			if( !bAttrTarget ){` |
|       ! 0 | 14655 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14656 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14657 | `					&pGen->pIn->sData);` |
|       ! 0 | 14658 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14659 | `					break;` |
|         - | 14660 | `				}` |
|       ! 0 | 14661 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14662 | `			}` |
|      3845 | 14663 | `		}` |
|         - | 14664 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14665 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  12632535 | 14666 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7351923 | 14667 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   7351923 | 14668 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14669 | `				bStmtIsDeclare = 1;` |
|        21 | 14670 | `			}` |
|   3675959 | 14671 | `		}` |
|  12632535 | 14672 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14673 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14674 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    949625 | 14675 | `			pGen->bStrictTypesLocked = 1;` |
|    474810 | 14676 | `		}` |
|  12632535 | 14677 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14678 | `			/* Compile block */` |
|      3845 | 14679 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3845 | 14680 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14681 | `				break;` |
|         - | 14682 | `			}` |
|      1925 | 14683 | `		}else{` |
|  12628695 | 14684 | `			xCons = 0;` |
|  12628695 | 14685 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14686 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14687 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14688 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34427 | 14689 | `				xCons = PH7_CompileClassModifiers;` |
|  12611484 | 14690 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14691 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14692 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3849 | 14693 | `				xCons = PH7_CompileEnum;` |
|  12592351 | 14694 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7317527 | 14695 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14696 | `				/* Try to extract a language construct handler */` |
|   7317527 | 14697 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   7317527 | 14698 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14699 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14700 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14701 | `						&pGen->pIn->sData);` |
|         9 | 14702 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14703 | `						break;` |
|         - | 14704 | `					}` |
|         - | 14705 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14706 | `					 * this erroneous statement.` |
|         - | 14707 | `					 */` |
|         9 | 14708 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14709 | `				}` |
|   8931668 | 14710 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    405743 | 14711 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14712 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14713 | `				xCons = PH7_CompileLabel;` |
|        56 | 14714 | `			}` |
|  12628695 | 14715 | `			if( xCons == 0 ){` |
|         - | 14716 | `				/* Assume an expression an try to compile it */` |
|   5311429 | 14717 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5311429 | 14718 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14719 | `					/* Pop l-value */` |
|   5311279 | 14720 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2655637 | 14721 | `				}` |
|   2655717 | 14722 | `			}else{` |
|         - | 14723 | `				/* Go compile the sucker */` |
|   7317271 | 14724 | `				rc = xCons(&(*pGen));` |
|         - | 14725 | `			}` |
|  12628695 | 14726 | `			if( rc == SXERR_ABORT ){` |
|         - | 14727 | `				/* Request to abort compilation */` |
|        12 | 14728 | `				break;` |
|         - | 14729 | `			}` |
|         - | 14730 | `		}` |
|         - | 14731 | `		/* Ignore trailing semi-colons ';' */` |
|  21631421 | 14732 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   8998901 | 14733 | `			pGen->pIn++;` |
|         5 | 14734 | `		}` |
|  12632525 | 14735 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14736 | `			/* Compile a single statement and return */` |
|  11682873 | 14737 | `			break;` |
|         - | 14738 | `		}` |
|         - | 14739 | `		/* LOOP ONE */` |
|         - | 14740 | `		/* LOOP TWO */` |
|         - | 14741 | `		/* LOOP THREE */` |
|         - | 14742 | `		/* LOOP FOUR */` |
|         5 | 14743 | `	}` |
|         - | 14744 | `	/* Return compilation status */` |
|  11750385 | 14745 | `	return rc;` |
|         5 | 14746 | `}` |
|         - | 14747 | `/*` |
|         - | 14748 | ` * Compile a Raw PHP chunk.` |
|         - | 14749 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14750 | ` * takes care of generating the appropriate error message.` |
|         - | 14751 | ` */` |
|     67514 | 14752 | `static sxi32 PH7_CompilePHP(` |
|         - | 14753 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14754 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14755 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14756 | `	)` |
|         5 | 14757 | `{` |
|     67519 | 14758 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14759 | `	sxi32 rc;` |
|         - | 14760 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67519 | 14761 | `	SySetReset(&(*pTokenSet));` |
|     67519 | 14762 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14763 | `	/* Mark as the default token set */` |
|     67519 | 14764 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14765 | `	/* Advance the stream cursor */` |
|     67519 | 14766 | `	pGen->pRawIn++;` |
|         - | 14767 | `	/* Tokenize the PHP chunk first */` |
|     67519 | 14768 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14769 | `	/* Point to the head and tail of the token stream. */` |
|     67519 | 14770 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67519 | 14771 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67519 | 14772 | `	if( is_expr ){` |
|       ! 0 | 14773 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14774 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14775 | `			/* A simple expression,compile it */` |
|       ! 0 | 14776 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14777 | `		}` |
|         - | 14778 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14779 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14780 | `		return SXRET_OK;` |
|         - | 14781 | `	}` |
|     67519 | 14782 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14783 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14784 | `		/*` |
|         - | 14785 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14786 | `		 * According to the PHP reference manual:` |
|         - | 14787 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14788 | `		 *  immediately follow` |
|         - | 14789 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14790 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14791 | `		 * Symisc extension:` |
|         - | 14792 | `		 *   This short syntax works with all PHP opening` |
|         - | 14793 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14794 | `		 *   only short tag.` |
|         - | 14795 | `		 */` |
|         - | 14796 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14797 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14798 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14799 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14800 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14801 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14802 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14803 | `		}` |
|         3 | 14804 | `		return SXRET_OK;` |
|         - | 14805 | `	}` |
|         - | 14806 | `	/* Compile the PHP chunk */` |
|     67517 | 14807 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14808 | `	/* Fix exceptions jumps */` |
|     67517 | 14809 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14810 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67517 | 14811 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14812 | `		rc = SXERR_ABORT;` |
|         1 | 14813 | `	}` |
|         - | 14814 | `	/* Reset container */` |
|     67517 | 14815 | `	SySetReset(&pGen->aGoto);` |
|     67517 | 14816 | `	SySetReset(&pGen->aLabel);` |
|     67517 | 14817 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14818 | `	/* Compilation result */` |
|     67517 | 14819 | `	return rc;` |
|     33762 | 14820 | `}` |
|         - | 14821 | `/*` |
|         - | 14822 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14823 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14824 | ` * This is the only compile interface exported from this file.` |
|         - | 14825 | ` */` |
|     70696 | 14826 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14827 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14828 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14829 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14830 | `	)` |
|         5 | 14831 | `{` |
|         - | 14832 | `	SySet aPhpToken,aRawToken;` |
|         - | 14833 | `	ph7_gen_state *pCodeGen;` |
|         - | 14834 | `	ph7_value *pRawObj;` |
|         - | 14835 | `	sxu32 nObjIdx;` |
|         - | 14836 | `	sxi32 nRawObj;` |
|         - | 14837 | `	int is_expr;` |
|         - | 14838 | `	sxi8 bSavedStrict;` |
|         - | 14839 | `	sxi8 bSavedStrictLocked;` |
|         - | 14840 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 14841 | `	sxi32 rc;` |
|     70701 | 14842 | `	sxu32 nBaseLine = 1;` |
|     70701 | 14843 | `	if( pScript->nByte < 1 ){` |
|         - | 14844 | `		/* Nothing to compile */` |
|       ! 0 | 14845 | `		return PH7_OK;` |
|         - | 14846 | `	}` |
|         - | 14847 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 14848 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 14849 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     70701 | 14850 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 14851 | `		const char *z = pScript->zString;` |
|         3 | 14852 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 14853 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 14854 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 14855 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 14856 | `		pScript->zString = z;` |
|         3 | 14857 | `		nBaseLine = 2;` |
|         3 | 14858 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 14859 | `			return PH7_OK;` |
|         - | 14860 | `		}` |
|         1 | 14861 | `	}` |
|         - | 14862 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14863 | `	 * file's flags so include/require restore them on return. */` |
|     70701 | 14864 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 14865 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 14866 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 14867 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 14868 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 14869 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 14870 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     70701 | 14871 | `	pSavedIn = pCodeGen->pIn;` |
|     70701 | 14872 | `	pSavedEnd = pCodeGen->pEnd;` |
|     70701 | 14873 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     70701 | 14874 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     70701 | 14875 | `	pCodeGen->bStrictTypes = 0;` |
|     70701 | 14876 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14877 | `	/* Initialize the tokens containers */` |
|     70701 | 14878 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70701 | 14879 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70701 | 14880 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     70701 | 14881 | `	is_expr = 0;` |
|     70701 | 14882 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14883 | `		SyToken sTmp;` |
|         - | 14884 | `		/* PHP only: -*/` |
|     57363 | 14885 | `		sTmp.nLine = 1;` |
|     57363 | 14886 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57363 | 14887 | `		sTmp.pUserData = 0;` |
|     57363 | 14888 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57363 | 14889 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57363 | 14890 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14891 | `			/* A simple PHP expression */` |
|       ! 0 | 14892 | `			is_expr = 1;` |
|       ! 0 | 14893 | `		}` |
|     28684 | 14894 | `	}else{` |
|         - | 14895 | `		/* Tokenize raw text */` |
|     13343 | 14896 | `		SySetAlloc(&aRawToken,32);` |
|     13343 | 14897 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 14898 | `	}` |
|         - | 14899 | `	/* Process high-level tokens */` |
|     70701 | 14900 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     70701 | 14901 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     70701 | 14902 | `	rc = PH7_OK;` |
|     70701 | 14903 | `	if( is_expr ){` |
|         - | 14904 | `		/* Compile the expression */` |
|       ! 0 | 14905 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14906 | `		goto cleanup;` |
|         - | 14907 | `	}` |
|     70701 | 14908 | `	nObjIdx = 0;` |
|         - | 14909 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14910 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14911 | `	 * preventing namespace bleeding across include()d files. */` |
|     70701 | 14912 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14913 | `	/* Start the compilation process */` |
|     42022 | 14914 | `	for(;;){` |
|    151551 | 14915 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     70689 | 14916 | `			break; /* No more tokens to process */` |
|         - | 14917 | `		}` |
|     80867 | 14918 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14919 | `			/* Compile the PHP chunk */` |
|     67519 | 14920 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67519 | 14921 | `			if( rc == SXERR_ABORT ){` |
|        15 | 14922 | `				break;` |
|         - | 14923 | `			}` |
|     67507 | 14924 | `			continue;` |
|         - | 14925 | `		}` |
|         - | 14926 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13353 | 14927 | `		nRawObj = 0;` |
|     26701 | 14928 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14929 | `			/* Consume the raw chunk without any processing */` |
|     13353 | 14930 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13353 | 14931 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14932 | `				rc = SXERR_MEM;` |
|       ! 0 | 14933 | `				break;` |
|         - | 14934 | `			}` |
|         - | 14935 | `			/* Mark as constant and emit the load constant instruction */` |
|     13353 | 14936 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13353 | 14937 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13353 | 14938 | `			++nRawObj;` |
|     13353 | 14939 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14940 | `		}` |
|     13353 | 14941 | `		if( nRawObj > 0 ){` |
|         - | 14942 | `			/* Emit the consume instruction */` |
|     13353 | 14943 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6674 | 14944 | `		}` |
|     35353 | 14945 | `	}` |
|     35348 | 14946 | `cleanup:` |
|         - | 14947 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     70701 | 14948 | `	pCodeGen->pIn = pSavedIn;` |
|     70701 | 14949 | `	pCodeGen->pEnd = pSavedEnd;` |
|     70701 | 14950 | `	SySetRelease(&aRawToken);` |
|     70701 | 14951 | `	SySetRelease(&aPhpToken);` |
|         - | 14952 | `	/* Restore outer file's strict_types scope */` |
|     70701 | 14953 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     70701 | 14954 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     70701 | 14955 | `	return rc;` |
|     35353 | 14956 | `}` |
|         - | 14957 | `/*` |
|         - | 14958 | ` * Utility routines.Initialize the code generator.` |
|         - | 14959 | ` */` |
|      3816 | 14960 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 14961 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14962 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14963 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14964 | `	)` |
|         5 | 14965 | `{` |
|      3821 | 14966 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14967 | `	/* Zero the structure */` |
|      3821 | 14968 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 14969 | `	/* Initial state */` |
|      3821 | 14970 | `	pGen->pVm  = &(*pVm);` |
|      3821 | 14971 | `	pGen->xErr = xErr;` |
|      3821 | 14972 | `	pGen->pErrData = pErrData;` |
|      3821 | 14973 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3821 | 14974 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3821 | 14975 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3821 | 14976 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3821 | 14977 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3821 | 14978 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3821 | 14979 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3821 | 14980 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3821 | 14981 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 14982 | `	/* Error log buffer */` |
|      3821 | 14983 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 14984 | `	/* General purpose working buffer */` |
|      3821 | 14985 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 14986 | `	/* Namespace state */` |
|      3821 | 14987 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3821 | 14988 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3821 | 14989 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3821 | 14990 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14991 | `	/* Create the global scope */` |
|      3821 | 14992 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 14993 | `	/* Point to the global scope */` |
|      3821 | 14994 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3821 | 14995 | `	return SXRET_OK;` |
|         5 | 14996 | `}` |
|         - | 14997 | `/*` |
|         - | 14998 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 14999 | ` */` |
|     74052 | 15000 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 15001 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15002 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15003 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15004 | `	)` |
|         5 | 15005 | `{` |
|     74057 | 15006 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15007 | `	GenBlock *pBlock,*pParent;` |
|         - | 15008 | `	/* Reset state */` |
|     74057 | 15009 | `	SySetReset(&pGen->aLabel);` |
|     74057 | 15010 | `	SySetReset(&pGen->aGoto);` |
|     74057 | 15011 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     74057 | 15012 | `	SySetReset(&pGen->aTrivia);` |
|     74057 | 15013 | `	SySetReset(&pGen->aPendingAttrs);` |
|     74057 | 15014 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     74057 | 15015 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     74057 | 15016 | `	SyBlobRelease(&pGen->sWorker);` |
|     74057 | 15017 | `	SyBlobRelease(&pGen->sNamespace);` |
|     74057 | 15018 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     74057 | 15019 | `	SyHashRelease(&pGen->hUseImports);` |
|     74057 | 15020 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     74057 | 15021 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     74057 | 15022 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     74057 | 15023 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     74057 | 15024 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15025 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 15026 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 15027 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 15028 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 15029 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 15030 | `	 * number of unique names, which is acceptable. */` |
|         - | 15031 | `	/* Point to the global scope */` |
|     74057 | 15032 | `	pBlock = pGen->pCurrent;` |
|     74057 | 15033 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 15034 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15035 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15036 | `		pBlock = pParent;` |
|       ! 0 | 15037 | `	}` |
|     74057 | 15038 | `	pGen->xErr = xErr;` |
|     74057 | 15039 | `	pGen->pErrData = pErrData;` |
|     74057 | 15040 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     74057 | 15041 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     74057 | 15042 | `	pGen->pIn = pGen->pEnd = 0;` |
|     74057 | 15043 | `	pGen->nErr = 0;` |
|     74057 | 15044 | `	return SXRET_OK;` |
|         5 | 15045 | `}` |
|         - | 15046 | `/*` |
|         - | 15047 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 15048 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 15049 | ` *` |
|         - | 15050 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 15051 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 15052 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 15053 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 15054 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 15055 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 15056 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 15057 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 15058 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 15059 | ` *` |
|         - | 15060 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 15061 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 15062 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 15063 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 15064 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 15065 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 15066 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 15067 | ` */` |
|         4 | 15068 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 15069 | `{` |
|         5 | 15070 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15071 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 15072 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 15073 | `	*pSaved = *pGen;` |
|         5 | 15074 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 15075 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 15076 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15077 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15078 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15079 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15080 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 15081 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 15082 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 15083 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 15084 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 15085 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15086 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 15087 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 15088 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 15089 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 15090 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 15091 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 15092 | `	pGen->pTokenSet = 0;` |
|         5 | 15093 | `	pGen->nErr = 0;` |
|         5 | 15094 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 15095 | `	pGen->nCommaExprOk = 0;` |
|         5 | 15096 | `	pGen->bInGenerator = 0;` |
|         5 | 15097 | `	pGen->bStrictTypes = 0;` |
|         5 | 15098 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 15099 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 15100 | `	pGen->xErr = xErr;` |
|         5 | 15101 | `	pGen->pErrData = pErrData;` |
|         5 | 15102 | `}` |
|         - | 15103 | `/*` |
|         - | 15104 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 15105 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 15106 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 15107 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 15108 | ` */` |
|         4 | 15109 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 15110 | `{` |
|         5 | 15111 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15112 | `	GenBlock *pBlock,*pParent;` |
|         - | 15113 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 15114 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 15115 | `	 * nested global block's own fixup sets. */` |
|         5 | 15116 | `	pBlock = pGen->pCurrent;` |
|         5 | 15117 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 15118 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15119 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15120 | `		pBlock = pParent;` |
|       ! 0 | 15121 | `	}` |
|         5 | 15122 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 15123 | `	/* Release the nested unit's position containers. */` |
|         5 | 15124 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 15125 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 15126 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 15127 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 15128 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 15129 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 15130 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 15131 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 15132 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 15133 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 15134 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 15135 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 15136 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 15137 | `	hVar = pGen->hVar;` |
|         5 | 15138 | `	hLiteral = pGen->hLiteral;` |
|         5 | 15139 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 15140 | `	*pGen = *pSaved;` |
|         5 | 15141 | `	pGen->hVar = hVar;` |
|         5 | 15142 | `	pGen->hLiteral = hLiteral;` |
|         5 | 15143 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 15144 | `}` |
|         - | 15145 | `/*` |
|         - | 15146 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 15147 | ` * php's parser prints, e.g.` |
|         - | 15148 | ` *` |
|         - | 15149 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 15150 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 15151 | ` *   syntax error, unexpected end of file` |
|         - | 15152 | ` *` |
|         - | 15153 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 15154 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 15155 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 15156 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 15157 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 15158 | ` *` |
|         - | 15159 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 15160 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 15161 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 15162 | ` */` |
|       182 | 15163 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 15164 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 15165 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 15166 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 15167 | `	)` |
|         5 | 15168 | `{` |
|       187 | 15169 | `	const char *zNoun = "token";` |
|         - | 15170 | `	sxu32 nLine;` |
|       187 | 15171 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 15172 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 15173 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 15174 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 15175 | `		 * it before concluding "end of file". */` |
|        92 | 15176 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        92 | 15177 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        92 | 15178 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 15179 | `			pTok = pGen->pEnd;` |
|        44 | 15180 | `		}` |
|        44 | 15181 | `	}` |
|       187 | 15182 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       187 | 15183 | `	if( pTok == 0 ){` |
|       ! 0 | 15184 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 15185 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 15186 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 15187 | `			zExpecting);` |
|         - | 15188 | `	}` |
|       187 | 15189 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 15190 | `		zNoun = "identifier";` |
|       180 | 15191 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         8 | 15192 | `		zNoun = "variable";` |
|       171 | 15193 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        23 | 15194 | `		zNoun = "integer";` |
|       158 | 15195 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 15196 | `		zNoun = "float";` |
|       ! 0 | 15197 | `	}` |
|       187 | 15198 | `	if( zExpecting ){` |
|       118 | 15199 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 15200 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 15201 | `	}` |
|       164 | 15202 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 15203 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        96 | 15204 | `}` |
|         - | 15205 | `/*` |
|         - | 15206 | ` * Generate a compile-time error message.` |
|         - | 15207 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 15208 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 15209 | ` * abort compilation immediately.` |
|         - | 15210 | ` */` |
|     15942 | 15211 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 15212 | `{` |
|     15947 | 15213 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     15947 | 15214 | `	const char *zErr = "Error";` |
|         - | 15215 | `	SyString *pFile;` |
|         - | 15216 | `	va_list ap;` |
|         - | 15217 | `	sxi32 rc;` |
|         - | 15218 | `	/* Reset the working buffer */` |
|     15947 | 15219 | `	SyBlobReset(pWorker);` |
|         - | 15220 | `	/* Peek the processed file path if available */` |
|     15947 | 15221 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     15947 | 15222 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 15223 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 15224 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 15225 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 15226 | `		 * into execution with a 0 exit status. */` |
|       659 | 15227 | `		pGen->nErr++;` |
|       659 | 15228 | `		if( pGen->nErr > 15 ){` |
|         - | 15229 | `			/* Error count limit reached */` |
|         6 | 15230 | `			if( pGen->xErr ){` |
|         6 | 15231 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 15232 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 15233 | `				if( pFile ){` |
|         6 | 15234 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 15235 | `				}` |
|         6 | 15236 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 15237 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 15238 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 15239 | `				}` |
|         2 | 15240 | `			}` |
|         - | 15241 | `			/* Abort immediately */` |
|         6 | 15242 | `			return SXERR_ABORT;` |
|         - | 15243 | `		}` |
|       325 | 15244 | `	}` |
|     15943 | 15245 | `	if( pGen->xErr == 0 ){` |
|         - | 15246 | `		/* No available error consumer,return immediately */` |
|     15271 | 15247 | `		return SXRET_OK;` |
|         - | 15248 | `	}` |
|       677 | 15249 | `	switch(nErrType){` |
|       310 | 15250 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 15251 | `	case E_WARNING: zErr = "Warning";     break;` |
|       346 | 15252 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 15253 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 15254 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 15255 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 15256 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        16 | 15257 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 15258 | `	default:` |
|       ! 0 | 15259 | `		break;` |
|         - | 15260 | `	}` |
|       677 | 15261 | `	rc = SXRET_OK;` |
|         - | 15262 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       677 | 15263 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       677 | 15264 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       677 | 15265 | `	va_start(ap,zFormat);` |
|       677 | 15266 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       677 | 15267 | `	va_end(ap);` |
|       677 | 15268 | `	if( pFile ){` |
|       677 | 15269 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       336 | 15270 | `	}` |
|         - | 15271 | `	/* Append a new line */` |
|       677 | 15272 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       677 | 15273 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 15274 | `		/* Consume the generated error message */` |
|       677 | 15275 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       336 | 15276 | `	}` |
|       677 | 15277 | `	return rc;` |
|      7976 | 15278 | `}` |
|         - | 15279 |  |
