# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7322/9045 lines (80.95%)

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
|  11472920 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|  11472925 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  11472925 |   173 | `	pBlock->pUserData   = pUserData;` |
|  11472925 |   174 | `	pBlock->pGen        = pGen;` |
|  11472925 |   175 | `	pBlock->iFlags      = iType;` |
|  11472925 |   176 | `	pBlock->pParent     = 0;` |
|  11472925 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11472925 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11472925 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|  11469100 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|  11469105 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  11469105 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|  11469105 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  11469105 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|  11469105 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  11469105 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    493349 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    493349 |   214 | `		pGen->nLoopId++;` |
|    493349 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    493349 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    493349 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    493349 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    246672 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|  11469105 |   221 | `	pGen->pCurrent = pBlock;` |
|  11469105 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   5490891 |   224 | `		*ppBlock = pBlock;` |
|   2745443 |   225 | `	}` |
|  11469105 |   226 | `	return SXRET_OK;` |
|   5734555 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|  11469088 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|  11469093 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|  11469093 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|  11469093 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|  11469084 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|  11469089 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  11469089 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|  11469089 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  11469089 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|  11469084 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|  11469089 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  11469089 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|  11469089 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    493341 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    246668 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|  11469089 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|  11469089 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|  11469089 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|  11469089 |   268 | `	return SXRET_OK;` |
|   5734547 |   269 | `}` |
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
|   4349194 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   4349199 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   4349199 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   4349199 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4349199 |   289 | `	return rc;` |
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
|   8040886 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   8040891 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  17321065 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9280179 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   3464811 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   5815373 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1466181 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   4349197 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4349197 |   322 | `		if( pInstr ){` |
|   4349197 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   4349197 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   4349197 |   326 | `			aFix[n].nJumpType = -1;` |
|   2174596 |   327 | `		}` |
|   2174601 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   8040891 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2811146 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2811151 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2811297 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   2811149 |   400 | `	return SXRET_OK;` |
|   1405578 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  14652322 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  14652327 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  14652327 |   409 | `	if( pEntry == 0 ){` |
|   3822985 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|  10829347 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10829347 |   413 | `	return SXRET_OK;` |
|   7326166 |   414 | `}` |
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
|   3822980 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3822985 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3822985 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1911490 |   429 | `	}` |
|   3822985 |   430 | `	return SXRET_OK;` |
|         5 |   431 | `}` |
|         - |   432 | `/*` |
|         - |   433 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   434 | ` * in the constant table.` |
|         - |   435 | ` */` |
|   3547476 |   436 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   437 | `{` |
|         - |   438 | `	ph7_value *pObj;` |
|   3547481 |   439 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   440 | `	/* Reserve a new constant */` |
|   3547481 |   441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3547481 |   442 | `	if( pObj == 0 ){` |
|       ! 0 |   443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   444 | `		return 0;` |
|         - |   445 | `	}` |
|   3547481 |   446 | `	*pIdx = nIdx;` |
|         - |   447 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   448 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   449 | `	 */` |
|   3547481 |   450 | `	return pObj;` |
|   1773743 |   451 | `}` |
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
|   6913684 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6913689 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3456847 |   478 | `}` |
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
|   3556134 |   542 | `static int GenStateFindBadNumericSeparator(` |
|         - |   543 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   544 | `{` |
|   3556139 |   545 | `	const char *z = pRaw->zString;` |
|   3556139 |   546 | `	sxu32 n = pRaw->nByte;` |
|   3556139 |   547 | `	int base = 10;` |
|         - |   548 | `	sxu32 i, start;` |
|   3556139 |   549 | `	if( n < 2 ) return 0;` |
|    743061 |   550 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|    103129 |   551 | `		base = 16;` |
|    691499 |   552 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       286 |   553 | `		base = 2;` |
|       142 |   554 | `	}` |
|   2801077 |   555 | `	for( i = 0; i < n; ++i ){` |
|   2058035 |   556 | `		if( z[i] != '_' ) continue;` |
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
|    743047 |   573 | `	return 0;` |
|   1778072 |   574 | `}` |
|         - |   575 | `/*` |
|         - |   576 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   577 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   578 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   579 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   580 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   581 | ` * so callers can bail from the current construct).` |
|         - |   582 | ` */` |
|   3556134 |   583 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   584 | `{` |
|   3556139 |   585 | `	const char *zBad = 0;` |
|   3556139 |   586 | `	sxu32 nBad = 0;` |
|         - |   587 | `	SyString sBad;` |
|         - |   588 | `	sxi32 rc;` |
|   3556139 |   589 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   3556125 |   590 | `		return SXRET_OK;` |
|         - |   591 | `	}` |
|        18 |   592 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   593 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   594 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   595 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   596 | `		return SXERR_ABORT;` |
|         - |   597 | `	}` |
|        18 |   598 | `	return SXERR_SYNTAX;` |
|   1778072 |   599 | `}` |
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
|   3556120 |   616 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   617 | `	SyMemBackend *pAlloc,` |
|         - |   618 | `	const SyString *pToken,` |
|         - |   619 | `	char *zScratch, sxu32 nScratch,` |
|         - |   620 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   621 | `{` |
|         - |   622 | `	sxu32 i, j;` |
|   3556125 |   623 | `	int hasUnderscore = 0;` |
|         - |   624 | `	char *zBuf;` |
|   3556125 |   625 | `	*pzAlloc = 0;` |
|   8425143 |   626 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   4869277 |   627 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   2434514 |   628 | `	}` |
|   3556125 |   629 | `	if( !hasUnderscore ){` |
|   3555871 |   630 | `		SyStringDupPtr(pOut, pToken);` |
|   3555871 |   631 | `		return SXRET_OK;` |
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
|   1778065 |   648 | `}` |
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
|   3547510 |   684 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   685 | `{` |
|   3547515 |   686 | `	const char *z = pNum->zString;` |
|   3547515 |   687 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   688 | `	const char *p, *q;` |
|         - |   689 | `	int n;` |
|   3547515 |   690 | `	*pbDecimal = FALSE;` |
|   3547515 |   691 | `	if( z >= zEnd ){` |
|       ! 0 |   692 | `		return FALSE;` |
|         - |   693 | `	}` |
|   3547515 |   694 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|         - |   695 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|    103127 |   696 | `		p = z + 2;` |
|    129847 |   697 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    420329 |   698 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|    103127 |   699 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|    103121 |   700 | `			return FALSE;` |
|         - |   701 | `		}` |
|         7 |   702 | `		{ ph7_real dv = 0;` |
|       103 |   703 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|        97 |   704 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|        49 |   705 | `		  }` |
|         7 |   706 | `		  *pReal = dv;` |
|         - |   707 | `		}` |
|         7 |   708 | `		return TRUE;` |
|   3444393 |   709 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|   3444111 |   724 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
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
|   3444095 |   739 | `	}else if( z[0] == '0' ){` |
|         - |   740 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   741 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   742 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1276917 |   743 | `		p = z;` |
|   2553831 |   744 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1288621 |   745 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1276917 |   746 | `		if( n <= 21 ){` |
|   1276915 |   747 | `			return FALSE;` |
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
|   2167183 |   760 | `	p = z;` |
|   2167183 |   761 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   5168585 |   762 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   2167183 |   763 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   764 | `		*pbDecimal = TRUE;` |
|        25 |   765 | `		return TRUE;` |
|         - |   766 | `	}` |
|   2167159 |   767 | `	return FALSE;` |
|   1773760 |   768 | `}` |
|   3556106 |   769 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   770 | `{` |
|   3556111 |   771 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   3556111 |   772 | `	sxu32 nIdx = 0;` |
|         - |   773 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   3556111 |   774 | `	char *zAlloc = 0;` |
|         - |   775 | `	SyString sNum;` |
|         - |   776 | `	sxi32 rc;` |
|   1778053 |   777 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   3556111 |   778 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   3556111 |   779 | `	if( rc != SXRET_OK ){` |
|        14 |   780 | `		return rc;` |
|         - |   781 | `	}` |
|   5334149 |   782 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1778048 |   783 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   3556101 |   784 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   785 | `		return SXERR_ABORT;` |
|         - |   786 | `	}` |
|   3556101 |   787 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   788 | `		ph7_value *pObj;` |
|         - |   789 | `		sxi64 iValue;` |
|   3547515 |   790 | `		ph7_real rOverflow = 0;` |
|   3547515 |   791 | `		int bDecimalOverflow = 0;` |
|   3547515 |   792 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|   3547481 |   809 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   3547481 |   810 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   3547481 |   811 | `			if( pObj == 0 ){` |
|       ! 0 |   812 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   813 | `				return SXERR_ABORT;` |
|         - |   814 | `			}` |
|   3547481 |   815 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   816 | `		}` |
|   1773760 |   817 | `	}else{` |
|         - |   818 | `		/* Real number */` |
|         - |   819 | `		ph7_value *pObj;` |
|         - |   820 | `		/* Reserve a new constant */` |
|      8591 |   821 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      8591 |   822 | `		if( pObj == 0 ){` |
|       ! 0 |   823 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   824 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   825 | `			return SXERR_ABORT;` |
|         - |   826 | `		}` |
|      8591 |   827 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      8591 |   828 | `		PH7_MemObjToReal(pObj);` |
|         - |   829 | `	}` |
|   3556101 |   830 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   831 | `	/* Emit the load constant instruction */` |
|   3556101 |   832 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   833 | `	/* Node successfully compiled */` |
|   3556101 |   834 | `	return SXRET_OK;` |
|   1778058 |   835 | `}` |
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
|   5076620 |   847 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   848 | `{` |
|   5076625 |   849 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   850 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   851 | `	ph7_value *pObj;` |
|         - |   852 | `	sxu32 nIdx;` |
|   5076625 |   853 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   854 | `	/* Delimit the string */` |
|   5076625 |   855 | `	zIn  = pStr->zString;` |
|   5076625 |   856 | `	zEnd = &zIn[pStr->nByte];` |
|   5076625 |   857 | `	if( zIn >= zEnd ){` |
|         - |   858 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   859 | `		 * rather than reserving a new object each time. */` |
|    324579 |   860 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    324579 |   861 | `		return SXRET_OK;` |
|         - |   862 | `	}` |
|   4752051 |   863 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   864 | `		/* Already processed,emit the load constant instruction` |
|         - |   865 | `		 * and return.` |
|         - |   866 | `		 */` |
|   2816923 |   867 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2816923 |   868 | `		return SXRET_OK;` |
|         - |   869 | `	}` |
|         - |   870 | `	/* Reserve a new constant */` |
|   1935133 |   871 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1935133 |   872 | `	if( pObj == 0 ){` |
|       ! 0 |   873 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   874 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   875 | `		return SXERR_ABORT;` |
|         - |   876 | `	}` |
|   1935133 |   877 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   878 | `	/* Compile the node */` |
|   1982891 |   879 | `	for(;;){` |
|   3965787 |   880 | `		if( zIn >= zEnd ){` |
|         - |   881 | `			/* End of input */` |
|   1935133 |   882 | `			break;` |
|         - |   883 | `		}` |
|   2030659 |   884 | `		zCur = zIn;` |
|  40317819 |   885 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  38287165 |   886 | `			zIn++;` |
|         5 |   887 | `		}` |
|   2030659 |   888 | `		if( zIn > zCur ){` |
|         - |   889 | `			/* Append raw contents*/` |
|   1992467 |   890 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    996231 |   891 | `		}` |
|   2030659 |   892 | `		zIn++;` |
|   2030659 |   893 | `		if( zIn < zEnd ){` |
|    129903 |   894 | `			if( zIn[0] == '\\' ){` |
|         - |   895 | `				/* A literal backslash */` |
|     30569 |   896 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    114621 |   897 | `			}else if( zIn[0] == '\'' ){` |
|         - |   898 | `				/* A single quote */` |
|        11 |   899 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   900 | `			}else{` |
|         - |   901 | `				/* verbatim copy */` |
|     99329 |   902 | `				zIn--;` |
|     99329 |   903 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     99329 |   904 | `				zIn++;` |
|         - |   905 | `			}` |
|     64949 |   906 | `		}` |
|         - |   907 | `		/* Advance the stream cursor */` |
|   2030659 |   908 | `		zIn++;` |
|         5 |   909 | `	}` |
|         - |   910 | `	/* Emit the load constant instruction */` |
|   1935133 |   911 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1935133 |   912 | `	if( pStr->nByte < 1024 ){` |
|         - |   913 | `		/* Install in the literal table */` |
|   1935133 |   914 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    967564 |   915 | `	}` |
|         - |   916 | `	/* Node successfully compiled */` |
|   1935133 |   917 | `	return SXRET_OK;` |
|   2538315 |   918 | `}` |
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
|      2630 |  1085 | `static sxi32 GenStateProcessStringExpression(` |
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
|      2635 |  1096 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1097 | `	/* Preallocate some slots */` |
|      2635 |  1098 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1099 | `	/* Tokenize the text */` |
|      2635 |  1100 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1101 | `	/* Swap delimiter */` |
|      2635 |  1102 | `	pTmpIn  = pGen->pIn;` |
|      2635 |  1103 | `	pTmpEnd = pGen->pEnd;` |
|      2635 |  1104 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2635 |  1105 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1106 | `	/* Compile the expression */` |
|      2635 |  1107 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1108 | `	/* Restore token stream */` |
|      2635 |  1109 | `	pGen->pIn  = pTmpIn;` |
|      2635 |  1110 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1111 | `	/* Release the token set */` |
|      2635 |  1112 | `	SySetRelease(&sToken);` |
|         - |  1113 | `	/* Compilation result */` |
|      2635 |  1114 | `	return rc;` |
|         5 |  1115 | `}` |
|         - |  1116 | `/*` |
|         - |  1117 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1118 | ` */` |
|    121596 |  1119 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1120 | `{` |
|         - |  1121 | `	ph7_value *pConstObj;` |
|    121601 |  1122 | `	sxu32 nIdx = 0;` |
|         - |  1123 | `	/* Reserve a new constant */` |
|    121601 |  1124 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    121601 |  1125 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1126 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1127 | `		return 0;` |
|         - |  1128 | `	}` |
|    121601 |  1129 | `	(*pCount)++;` |
|    121601 |  1130 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1131 | `	/* Emit the load constant instruction */` |
|    121601 |  1132 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    121601 |  1133 | `	return pConstObj;` |
|     60803 |  1134 | `}` |
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
|    120034 |  1197 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1198 | `{` |
|    120039 |  1199 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1200 | `	const char *zIn,*zCur,*zEnd;` |
|    120039 |  1201 | `	ph7_value *pObj = 0;` |
|         - |  1202 | `	sxi32 iCons;` |
|         - |  1203 | `	sxi32 rc;` |
|         - |  1204 | `	/* Delimit the string */` |
|    120039 |  1205 | `	zIn  = pStr->zString;` |
|    120039 |  1206 | `	zEnd = &zIn[pStr->nByte];` |
|    120039 |  1207 | `	if( zIn >= zEnd ){` |
|         - |  1208 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1209 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1210 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1211 | `		 */` |
|       413 |  1212 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       413 |  1213 | `		return SXRET_OK;` |
|         - |  1214 | `	}` |
|    119631 |  1215 | `	zCur = 0;` |
|         - |  1216 | `	/* Compile the node */` |
|    119631 |  1217 | `	iCons = 0;` |
|     61126 |  1218 | `	for(;;){` |
|    162631 |  1219 | `		zCur = zIn;` |
|   1657179 |  1220 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1497183 |  1221 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        69 |  1222 | `				break;` |
|   1497056 |  1223 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2508 |  1224 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1254 |  1225 | `					break;` |
|         - |  1226 | `			}` |
|   1494553 |  1227 | `			zIn++;` |
|         5 |  1228 | `		}` |
|    162631 |  1229 | `		if( zIn > zCur ){` |
|     94917 |  1230 | `			if( pObj == 0 ){` |
|     94299 |  1231 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     94299 |  1232 | `				if( pObj == 0 ){` |
|       ! 0 |  1233 | `					return SXERR_ABORT;` |
|         - |  1234 | `				}` |
|     47147 |  1235 | `			}` |
|     94917 |  1236 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     47456 |  1237 | `		}` |
|    162631 |  1238 | `		if( zIn >= zEnd ){` |
|    119629 |  1239 | `			break;` |
|         - |  1240 | `		}` |
|     43007 |  1241 | `		if( zIn[0] == '\\' ){` |
|     40377 |  1242 | `			const char *zPtr = 0;` |
|         - |  1243 | `			sxu32 n;` |
|     40377 |  1244 | `			zIn++;` |
|     40377 |  1245 | `			if( pObj == 0 ){` |
|     27307 |  1246 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27307 |  1247 | `				if( pObj == 0 ){` |
|       ! 0 |  1248 | `					return SXERR_ABORT;` |
|         - |  1249 | `				}` |
|     13651 |  1250 | `			}` |
|     40377 |  1251 | `			if( zIn >= zEnd ){` |
|         - |  1252 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1253 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1254 | `				break;` |
|         - |  1255 | `			}` |
|     40375 |  1256 | `			n = sizeof(char); /* size of conversion */` |
|     40375 |  1257 | `			switch( zIn[0] ){` |
|        15 |  1258 | `			case '$':` |
|         - |  1259 | `				/* Dollar sign */` |
|        33 |  1260 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        33 |  1261 | `				break;` |
|        55 |  1262 | `			case '\\':` |
|         - |  1263 | `				/* A literal backslash */` |
|       115 |  1264 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       115 |  1265 | `				break;` |
|         1 |  1266 | `			case 'e':` |
|         - |  1267 | `				/* Escape (ESC) ASCII code 27 */` |
|         3 |  1268 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|         3 |  1269 | `				break;` |
|         4 |  1270 | `			case 'f':` |
|         - |  1271 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|         9 |  1272 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|         9 |  1273 | `				break;` |
|     17602 |  1274 | `			case 'n':` |
|         - |  1275 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     35209 |  1276 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     35209 |  1277 | `				break;` |
|        27 |  1278 | `			case 'r':` |
|         - |  1279 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1280 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1281 | `				break;` |
|      1939 |  1282 | `			case 't':` |
|         - |  1283 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      3883 |  1284 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      3883 |  1285 | `				break;` |
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
|     40375 |  1400 | `			zIn += n;` |
|     40375 |  1401 | `			continue;` |
|         - |  1402 | `		}` |
|      2635 |  1403 | `		if( zIn[0] == '{' ){` |
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
|      2503 |  1437 | `			const char *zExpr = zIn;` |
|         - |  1438 | `			/* Assemble variable name */` |
|      1274 |  1439 | `			for(;;){` |
|         - |  1440 | `				/* Jump leading dollars */` |
|      5051 |  1441 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2503 |  1442 | `					zIn++;` |
|         5 |  1443 | `				}` |
|      1274 |  1444 | `				for(;;){` |
|     13055 |  1445 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9233 |  1446 | `						zIn++;` |
|         5 |  1447 | `					}` |
|      2553 |  1448 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1449 | `						/* UTF-8 stream */` |
|       ! 0 |  1450 | `						zIn++;` |
|       ! 0 |  1451 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1452 | `							zIn++;` |
|       ! 0 |  1453 | `						}` |
|       ! 0 |  1454 | `						continue;` |
|         - |  1455 | `					}` |
|      2553 |  1456 | `					break;` |
|       ! 0 |  1457 | `				}` |
|      2553 |  1458 | `				if( zIn >= zEnd ){` |
|       269 |  1459 | `					break;` |
|         - |  1460 | `				}` |
|      2289 |  1461 | `				if( zIn[0] == '[' ){` |
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
|      2279 |  1479 | `				}else if(zIn[0] == '{' ){` |
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
|      2275 |  1497 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1498 | `					/* Member access operator '->' */` |
|        53 |  1499 | `					zIn += 2;` |
|      2250 |  1500 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1501 | `					/* Static member access operator '::' */` |
|       ! 0 |  1502 | `					zIn += 2;` |
|       ! 0 |  1503 | `				}else{` |
|      1115 |  1504 | `					break;` |
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
|      2503 |  1516 | `				const char *zBr = zExpr;` |
|     14347 |  1517 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     11849 |  1518 | `					zBr++;` |
|         5 |  1519 | `				}` |
|      2503 |  1520 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
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
|      2496 |  1561 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' && zIn[-1] == '}'` |
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
|      2499 |  1597 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2499 |  1598 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1599 | `				return SXERR_ABORT;` |
|         - |  1600 | `			}` |
|      2499 |  1601 | `			if( rc != SXERR_EMPTY ){` |
|      2497 |  1602 | `				++iCons;` |
|      1246 |  1603 | `			}` |
|         - |  1604 | `		}` |
|         - |  1605 | `		/* Invalidate the previously used constant */` |
|      2631 |  1606 | `		pObj = 0;` |
|         5 |  1607 | `	}/*for(;;)*/` |
|    119631 |  1608 | `	if( iCons > 1 ){` |
|         - |  1609 | `		/* Concatenate all compiled constants */` |
|      1905 |  1610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       950 |  1611 | `	}` |
|         - |  1612 | `	/* Node successfully compiled */` |
|    119631 |  1613 | `	return SXRET_OK;` |
|     60022 |  1614 | `}` |
|         - |  1615 | `/*` |
|         - |  1616 | ` * Compile a double quoted string.` |
|         - |  1617 | ` *  See the block-comment above for more information.` |
|         - |  1618 | ` */` |
|    119972 |  1619 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1620 | `{` |
|         - |  1621 | `	sxi32 rc;` |
|    119977 |  1622 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     59986 |  1623 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1624 | `	/* Compilation result */` |
|    119977 |  1625 | `	return rc;` |
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
|   1436438 |  1669 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   1436443 |  1680 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1681 | `	/* Compile the expression*/` |
|   1436443 |  1682 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1683 | `	/* Restore token stream */` |
|   1436443 |  1684 | `	RE_SWAP_DELIMITER(pGen);` |
|   1436443 |  1685 | `	return rc;` |
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
|         4 |  1696 | `{` |
|        40 |  1697 | `	sxi32 rc = SXRET_OK;` |
|        40 |  1698 | `	if( pRoot->pOp ){` |
|        14 |  1699 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|        12 |  1700 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|        15 |  1701 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1702 | `			/* Unexpected expression */` |
|        12 |  1703 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|        12 |  1704 | `			if( rc != SXERR_ABORT ){` |
|        12 |  1705 | `				rc = SXERR_INVALID;` |
|         5 |  1706 | `			}` |
|         8 |  1707 | `		}` |
|        31 |  1708 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1709 | `		/* Unexpected expression */` |
|         3 |  1710 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|         3 |  1711 | `		if( rc != SXERR_ABORT ){` |
|         3 |  1712 | `			rc = SXERR_INVALID;` |
|         1 |  1713 | `		}` |
|         1 |  1714 | `	}` |
|        40 |  1715 | `	return rc;` |
|         4 |  1716 | `}` |
|         - |  1717 | `/*` |
|         - |  1718 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|         - |  1719 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|         - |  1720 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|         - |  1721 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|         - |  1722 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|         - |  1723 | ` */` |
|   1370290 |  1724 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1725 | `{` |
|   1370295 |  1726 | `	SyToken *pCur = pStart;` |
|   1370295 |  1727 | `	sxi32 iNest = 0;` |
|   3534349 |  1728 | `	while( pCur < pEnd ){` |
|   2666353 |  1729 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    502295 |  1730 | `			return pCur;` |
|         - |  1731 | `		}` |
|         - |  1732 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1733 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1734 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1735 | `		 */` |
|   2164063 |  1736 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23007 |  1737 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23007 |  1738 | `			SyToken *pFn = pCur;` |
|     23002 |  1739 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1740 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1741 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1742 | `				pFn = &pCur[1];` |
|       ! 0 |  1743 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1744 | `			}` |
|     23007 |  1745 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|     23003 |  1776 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|     11498 |  1796 | `		}` |
|   2164057 |  1797 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     53993 |  1798 | `			iNest++;` |
|   2137063 |  1799 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1800 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1801 | `			 * parser will shortly detect any syntax error. */` |
|     53993 |  1802 | `			iNest--;` |
|     26994 |  1803 | `		}` |
|   2164057 |  1804 | `		pCur++;` |
|         5 |  1805 | `	}` |
|    868001 |  1806 | `	return pEnd;` |
|    685150 |  1807 | `}` |
|         - |  1808 | `/*` |
|         - |  1809 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1810 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1811 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1812 | ` */` |
|    607010 |  1813 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1814 | `{` |
|         - |  1815 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1816 | `	SyToken *pKey,*pCur;` |
|    607015 |  1817 | `	sxi32 iEmitRef = 0;` |
|    607015 |  1818 | `	sxi32 iSpread = 0;` |
|    607015 |  1819 | `	sxi32 nPair = 0;` |
|         - |  1820 | `	sxi32 rc;` |
|    607015 |  1821 | `	xValidator = 0;` |
|    837399 |  1822 | `	for(;;){` |
|         - |  1823 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|         - |  1824 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|         - |  1825 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|         - |  1826 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|    533894 |  1827 | `		{` |
|   1674803 |  1828 | `			int nSkip = 0;` |
|   2494989 |  1829 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    820191 |  1830 | `				nSkip++;` |
|    820191 |  1831 | `				pGen->pIn++;` |
|         5 |  1832 | `			}` |
|   1674803 |  1833 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|       ! 0 |  1834 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|         - |  1835 | `					"Cannot use empty array elements in arrays");` |
|       ! 0 |  1836 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1837 | `					return SXERR_ABORT;` |
|         - |  1838 | `				}` |
|       ! 0 |  1839 | `				return SXRET_OK;` |
|         - |  1840 | `			}` |
|         - |  1841 | `		}` |
|   1674803 |  1842 | `		pCur = pGen->pIn;` |
|   1674803 |  1843 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1844 | `			/* No more entry to process */` |
|    606997 |  1845 | `			break;` |
|         - |  1846 | `		}` |
|   1067811 |  1847 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1848 | `			continue;` |
|         - |  1849 | `		}` |
|         - |  1850 | `		/* Compile the key if available */` |
|   1067811 |  1851 | `		pKey = pCur;` |
|   1067811 |  1852 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   1067811 |  1853 | `		rc = SXERR_EMPTY;` |
|   1067811 |  1854 | `		if( pCur < pGen->pIn ){` |
|    368379 |  1855 | `			if( pKey == pCur ){` |
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
|    368377 |  1869 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1870 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|         - |  1871 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|         - |  1872 | `				 * makes the helper reach for the token past this entry's slice. */` |
|        13 |  1873 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        13 |  1874 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1875 | `					return SXERR_ABORT;` |
|         - |  1876 | `				}` |
|        13 |  1877 | `				return SXRET_OK;` |
|         - |  1878 | `			}` |
|         - |  1879 | `			/* Compile the expression holding the key */` |
|    368367 |  1880 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1881 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    368367 |  1882 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1883 | `				return SXERR_ABORT;` |
|         - |  1884 | `			}` |
|    368367 |  1885 | `			pCur++; /* Jump the '=>' operator */` |
|    184186 |  1886 | `		}else{` |
|         - |  1887 | `			/* Reset back the cursor and point to the entry value */` |
|    699437 |  1888 | `			pCur = pKey;` |
|         - |  1889 | `		}` |
|   1067799 |  1890 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1891 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|         - |  1892 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|    699437 |  1893 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|    349716 |  1894 | `		}` |
|   1067799 |  1895 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|         - |  1896 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|        44 |  1897 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|        44 |  1898 | `			iEmitRef = 1;` |
|        44 |  1899 | `			pCur++; /* Jump the '&' token */` |
|        44 |  1900 | `			if( pCur >= pGen->pIn ){` |
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
|   1067797 |  1914 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   1067797 |  1915 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   1601687 |  1932 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    533894 |  1933 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1934 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    533894 |  1935 | `			xValidator);` |
|   1067793 |  1936 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1937 | `			return SXERR_ABORT;` |
|         - |  1938 | `		}` |
|   1067793 |  1939 | `		if( iSpread ){` |
|         - |  1940 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        73 |  1941 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   1067758 |  1942 | `		}else if( iEmitRef ){` |
|         - |  1943 | `			/* Emit the load reference instruction */` |
|        40 |  1944 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1945 | `		}` |
|   1067793 |  1946 | `		xValidator = 0;` |
|   1067793 |  1947 | `		iEmitRef = 0;` |
|   1067793 |  1948 | `		iSpread = 0;` |
|   1067793 |  1949 | `		nPair++;` |
|         5 |  1950 | `	}` |
|         - |  1951 | `	/* Emit the load map instruction */` |
|    606997 |  1952 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1953 | `	/* Node successfully compiled */` |
|    606997 |  1954 | `	return SXRET_OK;` |
|    303510 |  1955 | `}` |
|         - |  1956 | `/*` |
|         - |  1957 | ` * Compile the 'array' language construct.` |
|         - |  1958 | ` *	 According to the PHP language reference manual` |
|         - |  1959 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1960 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1961 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1962 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1963 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1964 | ` */` |
|    391078 |  1965 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1966 | `{` |
|         - |  1967 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    391083 |  1968 | `	pGen->pIn += 2;` |
|    391083 |  1969 | `	pGen->pEnd--;` |
|    195539 |  1970 | `	SXUNUSED(iCompileFlag);` |
|    391083 |  1971 | `	return GenStateCompileArrayBody(pGen);` |
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
|    215932 |  2070 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2071 | `{` |
|         - |  2072 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    215937 |  2073 | `	pGen->pIn++;` |
|    215937 |  2074 | `	pGen->pEnd--;` |
|    107966 |  2075 | `	SXUNUSED(iCompileFlag);` |
|    215937 |  2076 | `	return GenStateCompileArrayBody(pGen);` |
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
|       570 |  2417 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2418 | `{` |
|       575 |  2419 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2420 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2421 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2422 | `							  * one thread is allowed to compile the script.` |
|         - |  2423 | `						      */` |
|         - |  2424 | `	SyString sName;` |
|       575 |  2425 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2426 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2427 | `	sxu32 nKwLine;` |
|       575 |  2428 | `	sxi32 iFlags = 0;` |
|         - |  2429 | `	sxu32 nLen;` |
|         - |  2430 | `	sxi32 rc;` |
|       285 |  2431 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2432 |  |
|       575 |  2433 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       570 |  2434 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       575 |  2435 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2436 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        11 |  2437 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        11 |  2438 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         5 |  2439 | `	}` |
|       575 |  2440 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       575 |  2441 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2442 | `		pGen->pIn++;` |
|       ! 0 |  2443 | `	}` |
|         - |  2444 | `	/* Generate a unique name */` |
|       575 |  2445 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2446 | `	/* Make sure the generated name is unique */` |
|       575 |  2447 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2448 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2449 | `	}` |
|       575 |  2450 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2451 | `	/* Compile the lambda body */` |
|       575 |  2452 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       575 |  2453 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2454 | `		return SXERR_ABORT;` |
|         - |  2455 | `	}` |
|       575 |  2456 | `	if( pAnnonFunc ){` |
|       575 |  2457 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2458 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2459 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       575 |  2460 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2461 | `			return SXERR_ABORT;` |
|         - |  2462 | `		}` |
|       285 |  2463 | `	}` |
|         - |  2464 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2465 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2466 | `	 * the handler wraps either in a Closure instance. */` |
|       575 |  2467 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2468 | `	/* Node successfully compiled */` |
|       575 |  2469 | `	return SXRET_OK;` |
|       290 |  2470 | `}` |
|         - |  2471 | `/*` |
|         - |  2472 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2473 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2474 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2475 | ` */` |
|       218 |  2476 | `static sxi32 GenStateArrowAddCapture(` |
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
|       221 |  2488 | `	if( nByte == 0 ){` |
|       ! 0 |  2489 | `		return SXRET_OK;` |
|         - |  2490 | `	}` |
|       218 |  2491 | `	if( nByte == sizeof("this")-1` |
|       118 |  2492 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         3 |  2493 | `		return SXRET_OK;` |
|         - |  2494 | `	}` |
|       273 |  2495 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       204 |  2496 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       198 |  2497 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       153 |  2498 | `			return SXRET_OK;` |
|         - |  2499 | `		}` |
|        29 |  2500 | `	}` |
|        67 |  2501 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        67 |  2502 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|        95 |  2503 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2504 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2505 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2506 | `			return SXRET_OK;` |
|         - |  2507 | `		}` |
|        15 |  2508 | `	}` |
|        65 |  2509 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        65 |  2510 | `	if( zDup == 0 ){` |
|       ! 0 |  2511 | `		return SXERR_ABORT;` |
|         - |  2512 | `	}` |
|        65 |  2513 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        65 |  2514 | `	sEnv.iFlags = 0;` |
|        65 |  2515 | `	sEnv.nIdx = SXU32_HIGH;` |
|        65 |  2516 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        65 |  2517 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        65 |  2518 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        65 |  2519 | `	return SXRET_OK;` |
|       112 |  2520 | `}` |
|         - |  2521 | `/*` |
|         - |  2522 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|         - |  2523 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|         - |  2524 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|         - |  2525 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|         - |  2526 | ` */` |
|       106 |  2527 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|         - |  2528 | `	ph7_gen_state *pGen,` |
|         - |  2529 | `	ph7_vm_func *pFunc,` |
|         - |  2530 | `	const char *zIn,` |
|         - |  2531 | `	const char *zEnd,` |
|         - |  2532 | `	SyString *aShadow,` |
|         - |  2533 | `	sxu32 nShadow)` |
|         2 |  2534 | `{` |
|         - |  2535 | `	sxi32 rc;` |
|       578 |  2536 | `	while( zIn < zEnd ){` |
|       472 |  2537 | `		if( zIn[0] == '\\' ){` |
|        13 |  2538 | `			zIn++;` |
|        13 |  2539 | `			if( zIn < zEnd ){` |
|        13 |  2540 | `				zIn++;` |
|         6 |  2541 | `			}` |
|        13 |  2542 | `			continue;` |
|         - |  2543 | `		}` |
|       458 |  2544 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        26 |  2545 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
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
|       436 |  2574 | `		zIn++;` |
|         2 |  2575 | `	}` |
|       108 |  2576 | `	return SXRET_OK;` |
|        55 |  2577 | `}` |
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
|       494 |  2589 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2590 | `	ph7_gen_state *pGen,` |
|         - |  2591 | `	ph7_vm_func *pFunc,` |
|         - |  2592 | `	SyToken *pStart,` |
|         - |  2593 | `	SyToken *pEnd,` |
|         - |  2594 | `	SyString *aShadow,` |
|         - |  2595 | `	sxu32 nShadow)` |
|         4 |  2596 | `{` |
|       498 |  2597 | `	SyToken *pScan = pStart;` |
|         - |  2598 | `	sxi32 rc;` |
|      3340 |  2599 | `	while( pScan < pEnd ){` |
|      2846 |  2600 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       161 |  2601 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        53 |  2602 | `				pScan->sData.zString,` |
|       106 |  2603 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        53 |  2604 | `				aShadow,nShadow);` |
|       108 |  2605 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2606 | `				return SXERR_ABORT;` |
|         - |  2607 | `			}` |
|       108 |  2608 | `			pScan++;` |
|       108 |  2609 | `			continue;` |
|         - |  2610 | `		}` |
|      2740 |  2611 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        37 |  2612 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        37 |  2613 | `			SyToken *pFnKw = pScan;` |
|        34 |  2614 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2615 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         3 |  2616 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2617 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2618 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2619 | `			}` |
|        37 |  2620 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|         5 |  2762 | `		}` |
|      2716 |  2763 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      2522 |  2764 | `			pScan++;` |
|      2522 |  2765 | `			continue;` |
|         - |  2766 | `		}` |
|         - |  2767 | `		{` |
|         - |  2768 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       197 |  2769 | `			SyToken *pDollar = pScan;` |
|       291 |  2770 | `			while( &pDollar[1] < pEnd` |
|       197 |  2771 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2772 | `				pDollar++;` |
|       ! 0 |  2773 | `			}` |
|       197 |  2774 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2775 | `				break;` |
|         - |  2776 | `			}` |
|       197 |  2777 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2778 | `				pScan = pDollar + 1;` |
|       ! 0 |  2779 | `				continue;` |
|         - |  2780 | `			}` |
|       294 |  2781 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       194 |  2782 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|        97 |  2783 | `				aShadow,nShadow);` |
|       197 |  2784 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2785 | `				return SXERR_ABORT;` |
|         - |  2786 | `			}` |
|       197 |  2787 | `			pScan = pDollar + 2;` |
|         - |  2788 | `		}` |
|         3 |  2789 | `	}` |
|       498 |  2790 | `	return SXRET_OK;` |
|       251 |  2791 | `}` |
|         - |  2792 | `/*` |
|         - |  2793 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2794 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2795 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2796 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2797 | ` * $this is also made available.` |
|         - |  2798 | ` */` |
|       470 |  2799 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|       475 |  2816 | `	sxi32 iFlags = 0;` |
|       475 |  2817 | `	int bStatic = 0;` |
|         - |  2818 | `	sxi32 rc;` |
|         - |  2819 | `	sxu32 n;` |
|       235 |  2820 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2821 |  |
|       475 |  2822 | `	nLine = pGen->pIn->nLine;` |
|         - |  2823 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       475 |  2824 | `	pTokKw = pGen->pIn;` |
|         - |  2825 | `	/* Optional 'static' prefix */` |
|       470 |  2826 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       475 |  2827 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  2828 | `		bStatic = 1;` |
|         7 |  2829 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         7 |  2830 | `		pGen->pIn++;` |
|         3 |  2831 | `	}` |
|         - |  2832 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       470 |  2833 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       475 |  2834 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2835 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2836 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2837 | `		return SXERR_SYNTAX;` |
|         - |  2838 | `	}` |
|       475 |  2839 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2840 | `	/* Optional '&' — return by reference */` |
|       475 |  2841 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2842 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2843 | `		pGen->pIn++;` |
|       ! 0 |  2844 | `	}` |
|         - |  2845 | `	/* Expect '(' */` |
|       475 |  2846 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|       473 |  2857 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2858 | `	/* Delimit the parameter list */` |
|       473 |  2859 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       473 |  2860 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2861 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2862 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2863 | `		return SXERR_SYNTAX;` |
|         - |  2864 | `	}` |
|         - |  2865 | `	/* Allocate the function state */` |
|       471 |  2866 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       471 |  2867 | `	if( pFunc == 0 ){` |
|       ! 0 |  2868 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2869 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2870 | `		return SXERR_ABORT;` |
|         - |  2871 | `	}` |
|         - |  2872 | `	/* Generate a unique lambda name */` |
|       471 |  2873 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       471 |  2874 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2875 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2876 | `	}` |
|       471 |  2877 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       471 |  2878 | `	if( zDup == 0 ){` |
|       ! 0 |  2879 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2880 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2881 | `		return SXERR_ABORT;` |
|         - |  2882 | `	}` |
|       471 |  2883 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2884 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       471 |  2885 | `	pFunc->nLine = nLine;` |
|         - |  2886 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       471 |  2887 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2888 | `		return SXERR_ABORT;` |
|         - |  2889 | `	}` |
|         - |  2890 | `	/* Collect function arguments */` |
|       471 |  2891 | `	if( pGen->pIn < pSigEnd ){` |
|       126 |  2892 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       126 |  2893 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2894 | `			return SXERR_ABORT;` |
|         - |  2895 | `		}` |
|        61 |  2896 | `	}` |
|         - |  2897 | `	/* Point past ')' and parse optional return type */` |
|       471 |  2898 | `	pGen->pIn = &pSigEnd[1];` |
|       471 |  2899 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       471 |  2900 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2901 | `		return SXERR_ABORT;` |
|       471 |  2902 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2903 | `		return SXERR_SYNTAX;` |
|         - |  2904 | `	}` |
|         - |  2905 | `	/* Expect '=>' */` |
|       471 |  2906 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|       468 |  2917 | `	pGen->pIn++; /* Jump '=>' */` |
|       468 |  2918 | `	pBodyStart = pGen->pIn;` |
|       468 |  2919 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2920 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2921 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2922 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2923 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       468 |  2924 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2925 | `	{` |
|       468 |  2926 | `		SyString *aShadow = 0;` |
|       468 |  2927 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       468 |  2928 | `		if( nShadow > 0 ){` |
|       123 |  2929 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       120 |  2930 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       123 |  2931 | `			if( aShadow == 0 ){` |
|       ! 0 |  2932 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2933 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2934 | `				return SXERR_ABORT;` |
|         - |  2935 | `			}` |
|       279 |  2936 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       159 |  2937 | `				aShadow[n] = aArgs[n].sName;` |
|        81 |  2938 | `			}` |
|        60 |  2939 | `		}` |
|       700 |  2940 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       232 |  2941 | `			aShadow,nShadow);` |
|       468 |  2942 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2943 | `			return SXERR_ABORT;` |
|         - |  2944 | `		}` |
|         - |  2945 | `	}` |
|         - |  2946 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2947 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2948 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2949 | `	 * $this. */` |
|       468 |  2950 | `	if( !bStatic ){` |
|         - |  2951 | `		char *zThisDup;` |
|       462 |  2952 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       462 |  2953 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2954 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2955 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2956 | `			return SXERR_ABORT;` |
|         - |  2957 | `		}` |
|       462 |  2958 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       462 |  2959 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       462 |  2960 | `		sEnv.nIdx = SXU32_HIGH;` |
|       462 |  2961 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       462 |  2962 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       462 |  2963 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       229 |  2964 | `	}` |
|         - |  2965 | `	/* Arrow functions are always closures */` |
|       468 |  2966 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2967 | `	/* Compile the body expression as an implicit return */` |
|       700 |  2968 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       232 |  2969 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       468 |  2970 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2971 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2972 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2973 | `		return SXERR_ABORT;` |
|         - |  2974 | `	}` |
|       468 |  2975 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       468 |  2976 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       468 |  2977 | `	pSavedEnd = pGen->pEnd;` |
|       468 |  2978 | `	pGen->pIn = pBodyStart;` |
|       468 |  2979 | `	pGen->pEnd = pBodyEnd;` |
|       468 |  2980 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       468 |  2981 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2982 | `		return SXERR_ABORT;` |
|         - |  2983 | `	}` |
|         - |  2984 | `	/* The cursor stopped just past the body expression */` |
|       468 |  2985 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2986 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2987 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2988 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2989 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       468 |  2990 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       468 |  2991 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       468 |  2992 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       468 |  2993 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       468 |  2994 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2995 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       468 |  2996 | `	pGen->pIn = pBodyEnd;` |
|       468 |  2997 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2998 | `	/* Emit the load-closure instruction */` |
|       468 |  2999 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       468 |  3000 | `	return SXRET_OK;` |
|       240 |  3001 | `}` |
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
|        31 |  3338 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        31 |  3339 | `			if( pObj == 0 ){` |
|       ! 0 |  3340 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3341 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3342 | `				return SXERR_ABORT;` |
|         - |  3343 | `			}` |
|        31 |  3344 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3345 | `			/* Install in the literal table */` |
|        31 |  3346 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        13 |  3347 | `		}` |
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
|  19206112 |  3376 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3377 | `{` |
|  19206117 |  3378 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3379 | `	sxi32 iVv;` |
|         - |  3380 | `	sxi32 iP1;` |
|         - |  3381 | `	void *p3;` |
|         - |  3382 | `	sxi32 rc;` |
|  19206117 |  3383 | `	iVv = -1; /* Variable variable counter */` |
|  38412241 |  3384 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  19206129 |  3385 | `		pGen->pIn++;` |
|  19206129 |  3386 | `		iVv++;` |
|         5 |  3387 | `	}` |
|  19206117 |  3388 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3389 | `		/* Invalid variable name */` |
|       ! 0 |  3390 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3391 | `		if( rc == SXERR_ABORT ){` |
|         - |  3392 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3393 | `			return SXERR_ABORT;` |
|         - |  3394 | `		}` |
|       ! 0 |  3395 | `		return SXRET_OK;` |
|         - |  3396 | `	}` |
|  19206117 |  3397 | `	p3  = 0;` |
|  19206117 |  3398 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  19206101 |  3424 | `		char *zName = 0;` |
|         - |  3425 | `		/* Extract variable name */` |
|  19206101 |  3426 | `		pName = &pGen->pIn->sData;` |
|         - |  3427 | `		/* Advance the stream cursor */` |
|  19206101 |  3428 | `		pGen->pIn++;` |
|  19206101 |  3429 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  19206101 |  3430 | `		if( pEntry == 0 ){` |
|         - |  3431 | `			/* Duplicate name */` |
|   1155219 |  3432 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   1155219 |  3433 | `			if( zName == 0 ){` |
|       ! 0 |  3434 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3435 | `				return SXERR_ABORT;` |
|         - |  3436 | `			}` |
|         - |  3437 | `			/* Install in the hashtable */` |
|   1155219 |  3438 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    577612 |  3439 | `		}else{` |
|         - |  3440 | `			/* Name already available */` |
|  18050887 |  3441 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3442 | `		}` |
|  19206101 |  3443 | `		p3 = (void *)zName;` |
|         - |  3444 | `	}` |
|  19206113 |  3445 | `	iP1 = 0;` |
|  19206113 |  3446 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   5679905 |  3447 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3448 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   5676065 |  3449 | `			iP1 = 1;` |
|   2838030 |  3450 | `		}` |
|   2839950 |  3451 | `	}` |
|         - |  3452 | `	/* Emit the load instruction */` |
|  19206113 |  3453 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  19206125 |  3454 | `	while( iVv > 0 ){` |
|        13 |  3455 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3456 | `		iVv--;` |
|         1 |  3457 | `	}` |
|         - |  3458 | `	/* Node successfully compiled */` |
|  19206113 |  3459 | `	return SXRET_OK;` |
|   9603061 |  3460 | `}` |
|         - |  3461 | `/*` |
|         - |  3462 | ` * Load a literal.` |
|         - |  3463 | ` */` |
|  11891894 |  3464 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3465 | `{` |
|  11891899 |  3466 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3467 | `	ph7_value *pObj;` |
|         - |  3468 | `	SyString *pStr;` |
|         - |  3469 | `	sxu32 nIdx;` |
|         - |  3470 | `	/* Extract token value */` |
|  11891899 |  3471 | `	pStr = &pToken->sData;` |
|         - |  3472 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  11891899 |  3473 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2236485 |  3474 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3475 | `			/* NULL constant are always indexed at 0 */` |
|    981677 |  3476 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    981677 |  3477 | `			return SXRET_OK;` |
|   1254813 |  3478 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3479 | `			/* TRUE constant are always indexed at 1 */` |
|    321751 |  3480 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    321751 |  3481 | `			return SXRET_OK;` |
|         5 |  3482 | `		}` |
|  11112385 |  3483 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1980870 |  3484 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3485 | `			/* FALSE constant are always indexed at 2 */` |
|    710437 |  3486 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    710437 |  3487 | `			return SXRET_OK;` |
|   9356162 |  3488 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    822350 |  3489 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3490 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      3829 |  3491 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3829 |  3492 | `			if( pObj == 0 ){` |
|       ! 0 |  3493 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3494 | `				return SXERR_ABORT;` |
|         - |  3495 | `			}` |
|      3829 |  3496 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3497 | `			/* Emit the load constant instruction */` |
|      3829 |  3498 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3829 |  3499 | `			return SXRET_OK;` |
|   9350421 |  3500 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   1272578 |  3501 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|   9391321 |  3502 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|    908094 |  3503 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|         - |  3504 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|         - |  3505 | `			 * file being compiled (where the token is written), NOT the runtime` |
|         - |  3506 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|         - |  3507 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|         - |  3508 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|         - |  3509 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|      3911 |  3510 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|      3911 |  3511 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      3911 |  3512 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3911 |  3513 | `			if( pObj == 0 ){` |
|       ! 0 |  3514 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3515 | `				return SXERR_ABORT;` |
|         - |  3516 | `			}` |
|      3911 |  3517 | `			if( pFile && pFile->nByte > 0 ){` |
|        95 |  3518 | `				if( bDir ){` |
|         - |  3519 | `					const char *zDir;` |
|         - |  3520 | `					int nLen;` |
|         - |  3521 | `					SyString sDir;` |
|        48 |  3522 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|        48 |  3523 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|        48 |  3524 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|        26 |  3525 | `				}else{` |
|        51 |  3526 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|         - |  3527 | `				}` |
|        50 |  3528 | `			}else{` |
|         - |  3529 | `				SyString sMem;` |
|      3821 |  3530 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|      3821 |  3531 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|         - |  3532 | `			}` |
|      3911 |  3533 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3911 |  3534 | `			return SXRET_OK;` |
|   9054077 |  3535 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    233640 |  3536 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3537 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         7 |  3538 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3539 | `			if( pObj == 0 ){` |
|       ! 0 |  3540 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3541 | `				return SXERR_ABORT;` |
|         - |  3542 | `			}` |
|         7 |  3543 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3544 | `				SyString sNs;` |
|         7 |  3545 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         7 |  3546 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         4 |  3547 | `			}else{` |
|       ! 0 |  3548 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3549 | `			}` |
|         7 |  3550 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         7 |  3551 | `			return SXRET_OK;` |
|   9084817 |  3552 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    508606 |  3553 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   9150698 |  3554 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    426918 |  3555 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3556 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3557 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3558 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3559 | `				/* Point to the upper block */` |
|        11 |  3560 | `				pBlock = pBlock->pParent;` |
|         1 |  3561 | `			}` |
|        11 |  3562 | `			if( pBlock == 0 ){` |
|         - |  3563 | `				/* Called in the global scope,load NULL */` |
|         5 |  3564 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3565 | `			}else{` |
|         - |  3566 | `				/* Extract the target function/method */` |
|         7 |  3567 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3568 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|         7 |  3569 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3570 | `				if( pObj == 0 ){` |
|       ! 0 |  3571 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3572 | `					return SXERR_ABORT;` |
|         - |  3573 | `				}` |
|         - |  3574 | `				/*` |
|         - |  3575 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|         - |  3576 | `				 * function name inside a plain function (php does not answer "" there —` |
|         - |  3577 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|         - |  3578 | `				 * unqualified in every method).` |
|         - |  3579 | `				 */` |
|         8 |  3580 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 |  3581 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         - |  3582 | `					SyBlob sQual;` |
|         - |  3583 | `					SyString sOut;` |
|         3 |  3584 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|         3 |  3585 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|         3 |  3586 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|         3 |  3587 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|         3 |  3588 | `					SyBlobRelease(&sQual);` |
|         2 |  3589 | `				}else{` |
|         5 |  3590 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3591 | `				}` |
|         - |  3592 | `				/* Emit the load constant instruction */` |
|         7 |  3593 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3594 | `			}` |
|        11 |  3595 | `			return SXRET_OK;` |
|         - |  3596 | `	}` |
|         - |  3597 | `	/* Query literal table */` |
|   9870303 |  3598 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3599 | `		ph7_value *pLitObj;` |
|         - |  3600 | `		/* Unknown literal,install it in the literal table */` |
|   1880081 |  3601 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1880081 |  3602 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3603 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3604 | `			return SXERR_ABORT;` |
|         - |  3605 | `		}` |
|   1880081 |  3606 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1880081 |  3607 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    940038 |  3608 | `	}` |
|         - |  3609 | `	/* Emit the load constant instruction */` |
|   9870303 |  3610 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9870303 |  3611 | `	return SXRET_OK;` |
|   5945952 |  3612 | `}` |
|         - |  3613 | `/*` |
|         - |  3614 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3615 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3616 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3617 | ` * Otherwise, load the simple literal directly.` |
|         - |  3618 | ` */` |
|  11895778 |  3619 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3620 | `{` |
|         - |  3621 | `	sxi32 rc;` |
|  11895783 |  3622 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3623 | `		return SXRET_OK;` |
|         - |  3624 | `	}` |
|         - |  3625 | `	/* Check if this is a multi-token namespace path */` |
|  11895783 |  3626 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3627 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3889 |  3628 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3889 |  3629 | `		int isAbsolute = 0;` |
|      3889 |  3630 | `		SyBlobReset(pWorker);` |
|         - |  3631 | `		/* Check for leading backslash (absolute path) */` |
|      3889 |  3632 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3887 |  3633 | `			isAbsolute = 1;` |
|      3887 |  3634 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1941 |  3635 | `		}` |
|         - |  3636 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3889 |  3637 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3638 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3639 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3640 | `		}` |
|         - |  3641 | `		/* Collect all path components */` |
|      4001 |  3642 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4001 |  3643 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        60 |  3644 | `				SyBlobAppend(pWorker,"\\",1);` |
|        32 |  3645 | `			}else{` |
|      3945 |  3646 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3647 | `			}` |
|      4001 |  3648 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3889 |  3649 | `				pGen->pIn++;` |
|      3889 |  3650 | `				break;` |
|         - |  3651 | `			}` |
|       116 |  3652 | `			pGen->pIn++;` |
|         4 |  3653 | `		}` |
|      3889 |  3654 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3655 | `			ph7_value *pObj;` |
|         - |  3656 | `			SyString sPath;` |
|         - |  3657 | `			sxu32 nIdx;` |
|      3889 |  3658 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3659 | `			/* Install in the literal table */` |
|      3889 |  3660 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3841 |  3661 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3841 |  3662 | `				if( pObj == 0 ){` |
|       ! 0 |  3663 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3664 | `					return SXERR_ABORT;` |
|         - |  3665 | `				}` |
|      3841 |  3666 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3841 |  3667 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1918 |  3668 | `			}` |
|         - |  3669 | `			/* Emit the load constant instruction.` |
|         - |  3670 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3671 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5831 |  3672 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1942 |  3673 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1942 |  3674 | `				nIdx,0,0);` |
|      3889 |  3675 | `			return SXRET_OK;` |
|         - |  3676 | `		}` |
|       ! 0 |  3677 | `	}` |
|         - |  3678 | `	/* Single-token literal: load directly */` |
|  11891899 |  3679 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11891899 |  3680 | `	return rc;` |
|   5947894 |  3681 | `}` |
|         - |  3682 | `/*` |
|         - |  3683 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3684 | ` */` |
|         - |  3685 | `/*` |
|         - |  3686 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3687 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3688 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3689 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3690 | ` */` |
|       ! 0 |  3691 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3692 | `{` |
|       ! 0 |  3693 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3694 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3695 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3696 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3697 | `}` |
|  11895778 |  3698 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3699 | `{` |
|         - |  3700 | `	sxi32 rc;` |
|  11895783 |  3701 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11895783 |  3702 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3703 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3704 | `		return rc;` |
|         - |  3705 | `	}` |
|         - |  3706 | `	/* Node successfully compiled */` |
|  11895783 |  3707 | `	return SXRET_OK;` |
|   5947894 |  3708 | `}` |
|         - |  3709 | `/*` |
|         - |  3710 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3711 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3712 | ` */` |
|         8 |  3713 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3714 | `{` |
|         - |  3715 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3716 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3717 | `		pGen->pIn++;` |
|         1 |  3718 | `	}` |
|         9 |  3719 | `	return SXRET_OK;` |
|         1 |  3720 | `}` |
|         - |  3721 | `/*` |
|         - |  3722 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3723 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3724 | ` */` |
|    290244 |  3725 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3726 | `{` |
|    290249 |  3727 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3865 |  3728 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3729 | `			return TRUE;` |
|      3863 |  3730 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3731 | `			return TRUE;` |
|         5 |  3732 | `		}` |
|    288316 |  3733 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7657 |  3734 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3735 | `			return TRUE;` |
|         - |  3736 | `		}` |
|      3825 |  3737 | `	}` |
|         - |  3738 | `	/* Not a reserved constant */` |
|    290241 |  3739 | `	return FALSE;` |
|    145127 |  3740 | `}` |
|         - |  3741 | `/*` |
|         - |  3742 | ` * Compile the 'const' statement.` |
|         - |  3743 | ` * According to the PHP language reference` |
|         - |  3744 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3745 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3746 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3747 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3748 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3749 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3750 | ` *  Syntax` |
|         - |  3751 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3752 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3753 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3754 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3755 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3756 | ` *  to get a list of all defined constants.` |
|         - |  3757 | ` *` |
|         - |  3758 | ` * Symisc eXtension.` |
|         - |  3759 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3760 | ` *  would allow only simple scalar value.` |
|         - |  3761 | ` *  Example` |
|         - |  3762 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3763 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3764 | ` */` |
|        48 |  3765 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3766 | `{` |
|         - |  3767 | `	SySet *pConsCode,*pInstrContainer;` |
|        53 |  3768 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3769 | `	SyString *pName;` |
|         - |  3770 | `	sxi32 rc;` |
|        53 |  3771 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        53 |  3772 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3773 | `		/* Invalid constant name */` |
|         8 |  3774 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|         8 |  3775 | `		if( rc == SXERR_ABORT ){` |
|         - |  3776 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3777 | `			return SXERR_ABORT;` |
|         - |  3778 | `		}` |
|         8 |  3779 | `		goto Synchronize;` |
|         - |  3780 | `	}` |
|         - |  3781 | `	/* Peek constant name */` |
|        47 |  3782 | `	pName = &pGen->pIn->sData;` |
|         - |  3783 | `	/* Make sure the constant name isn't reserved */` |
|        47 |  3784 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3785 | `		/* Reserved constant */` |
|        10 |  3786 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        10 |  3787 | `		if( rc == SXERR_ABORT ){` |
|         - |  3788 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3789 | `			return SXERR_ABORT;` |
|         - |  3790 | `		}` |
|        10 |  3791 | `		goto Synchronize;` |
|         - |  3792 | `	}` |
|        38 |  3793 | `	pGen->pIn++;` |
|        38 |  3794 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3795 | `		/* Invalid statement*/` |
|         6 |  3796 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|         6 |  3797 | `		if( rc == SXERR_ABORT ){` |
|         - |  3798 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3799 | `			return SXERR_ABORT;` |
|         - |  3800 | `		}` |
|         6 |  3801 | `		goto Synchronize;` |
|         - |  3802 | `	}` |
|        32 |  3803 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3804 | `	/* Allocate a new constant value container */` |
|        32 |  3805 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        32 |  3806 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3807 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3808 | `		return SXERR_ABORT;` |
|         - |  3809 | `	}` |
|        32 |  3810 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3811 | `	/* Swap bytecode container */` |
|        32 |  3812 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        32 |  3813 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3814 | `	/* Compile constant value */` |
|        32 |  3815 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3816 | `	/* Emit the done instruction */` |
|        32 |  3817 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        32 |  3818 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        32 |  3819 | `	if( rc == SXERR_ABORT ){` |
|         - |  3820 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3821 | `		return SXERR_ABORT;` |
|         - |  3822 | `	}` |
|        32 |  3823 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3824 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3825 | `	{` |
|         - |  3826 | `		SyBlob sFQN;` |
|         - |  3827 | `		SyString sFQNStr;` |
|        32 |  3828 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        32 |  3829 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        32 |  3830 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        47 |  3831 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        30 |  3832 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        32 |  3833 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3834 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3835 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3836 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3837 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3838 | `			if( pCEntry ){` |
|         5 |  3839 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3840 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3841 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3842 | `					return SXERR_ABORT;` |
|         - |  3843 | `				}` |
|         2 |  3844 | `			}` |
|         2 |  3845 | `		}` |
|        32 |  3846 | `		SyBlobRelease(&sFQN);` |
|         - |  3847 | `	}` |
|        32 |  3848 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3849 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3850 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3851 | `	}` |
|        32 |  3852 | `	return SXRET_OK;` |
|         9 |  3853 | `Synchronize:` |
|         - |  3854 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3855 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        41 |  3856 | `		pGen->pIn++;` |
|         3 |  3857 | `	}` |
|        22 |  3858 | `	return SXRET_OK;` |
|        29 |  3859 | `}` |
|         - |  3860 | `/*` |
|         - |  3861 | ` * Compile the 'continue' statement.` |
|         - |  3862 | ` * According to the PHP language reference` |
|         - |  3863 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3864 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3865 | ` *  iteration.` |
|         - |  3866 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3867 | ` *  the purposes of continue.` |
|         - |  3868 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3869 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3870 | ` *  Note:` |
|         - |  3871 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3872 | ` */` |
|         - |  3873 | `/*` |
|         - |  3874 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3875 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3876 | ` * break/continue crosses a try boundary.` |
|         - |  3877 | ` *` |
|         - |  3878 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3879 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3880 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3881 | ` */` |
|    148978 |  3882 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3883 | `{` |
|    148983 |  3884 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    148983 |  3885 | `	int nInlineTry = 0;` |
|    672021 |  3886 | `	while( pBlock && pBlock != pTarget ){` |
|    523043 |  3887 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3888 | `			if( pBlock->pUserData ){` |
|         - |  3889 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3890 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3891 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3892 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3893 | `				if( pGen->bInGenerator ){` |
|         3 |  3894 | `					nInlineTry++;` |
|         2 |  3895 | `				}else{` |
|         3 |  3896 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3897 | `				}` |
|         4 |  3898 | `			}else{` |
|         - |  3899 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3900 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3901 | `				break;` |
|         - |  3902 | `			}` |
|         2 |  3903 | `		}` |
|    523043 |  3904 | `		pBlock = pBlock->pParent;` |
|         5 |  3905 | `	}` |
|    148983 |  3906 | `	return nInlineTry;` |
|         5 |  3907 | `}` |
|     84002 |  3908 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3909 | `{` |
|         - |  3910 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3911 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3912 | `	sxu32 nLineLocal;` |
|         - |  3913 | `	sxi32 rc;` |
|     84007 |  3914 | `	nLineLocal = pGen->pIn->nLine;` |
|     84007 |  3915 | `	iLevel = 0;` |
|         - |  3916 | `	/* Jump the 'continue' keyword */` |
|     84007 |  3917 | `	pGen->pIn++;` |
|     84007 |  3918 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3919 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3920 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3921 | `		 */` |
|         - |  3922 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3923 | `		char *zAlloc = 0;` |
|         - |  3924 | `		SyString sNum;` |
|        17 |  3925 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3926 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3927 | `			return SXERR_ABORT;` |
|         - |  3928 | `		}` |
|        17 |  3929 | `		if( rc == SXRET_OK ){` |
|        20 |  3930 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3931 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3932 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3933 | `				return SXERR_ABORT;` |
|         - |  3934 | `			}` |
|        14 |  3935 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3936 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3937 | `		}` |
|        17 |  3938 | `		if( iLevel < 2 ){` |
|         3 |  3939 | `			iLevel = 0;` |
|         1 |  3940 | `		}` |
|        17 |  3941 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3942 | `	}` |
|         - |  3943 | `	/* Point to the target loop */` |
|     84007 |  3944 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     84007 |  3945 | `	if( pLoop == 0 ){` |
|         - |  3946 | `		/* Illegal continue */` |
|        12 |  3947 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        12 |  3948 | `		if( rc == SXERR_ABORT ){` |
|         - |  3949 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3950 | `			return SXERR_ABORT;` |
|         - |  3951 | `		}` |
|         7 |  3952 | `	}else{` |
|     83997 |  3953 | `		sxu32 nInstrIdx = 0;` |
|         - |  3954 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     83997 |  3955 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3956 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3957 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     83997 |  3958 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     83997 |  3959 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3960 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|         - |  3961 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|         - |  3962 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|         - |  3963 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|         5 |  3964 | `			if( iLevel < 1 ){` |
|         5 |  3965 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|         - |  3966 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|         - |  3967 | `					" Did you mean to use \"continue 2\"?");` |
|         2 |  3968 | `			}` |
|         5 |  3969 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3970 | `			if( rc == SXRET_OK ){` |
|         5 |  3971 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3972 | `			}` |
|         3 |  3973 | `		}else{` |
|         - |  3974 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     83993 |  3975 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     83993 |  3976 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  3977 | `				JumpFixup sJumpFix;` |
|         - |  3978 | `				/* Post-continue */` |
|     26729 |  3979 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     26729 |  3980 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     26729 |  3981 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     13362 |  3982 | `			}` |
|         - |  3983 | `		}` |
|         - |  3984 | `	}` |
|     84007 |  3985 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3986 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3987 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  3988 | `	}` |
|         - |  3989 | `	/* Statement successfully compiled */` |
|     84007 |  3990 | `	return SXRET_OK;` |
|     42006 |  3991 | `}` |
|         - |  3992 | `/*` |
|         - |  3993 | ` * Compile the 'break' statement.` |
|         - |  3994 | ` * According to the PHP language reference` |
|         - |  3995 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  3996 | ` *  structure.` |
|         - |  3997 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  3998 | ` *  enclosing structures are to be broken out of.` |
|         - |  3999 | ` */` |
|     65002 |  4000 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  4001 | `{` |
|         - |  4002 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  4003 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  4004 | `	sxi32 rc;` |
|     65007 |  4005 | `	iLevel = 0;` |
|         - |  4006 | `	/* Jump the 'break' keyword */` |
|     65007 |  4007 | `	pGen->pIn++;` |
|     65007 |  4008 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  4009 | `		/* optional numeric argument which tells us how many levels` |
|         - |  4010 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  4011 | `		 */` |
|         - |  4012 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        18 |  4013 | `		char *zAlloc = 0;` |
|         - |  4014 | `		SyString sNum;` |
|        18 |  4015 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        18 |  4016 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4017 | `			return SXERR_ABORT;` |
|         - |  4018 | `		}` |
|        18 |  4019 | `		if( rc == SXRET_OK ){` |
|        21 |  4020 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  4021 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  4022 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  4023 | `				return SXERR_ABORT;` |
|         - |  4024 | `			}` |
|        15 |  4025 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  4026 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  4027 | `		}` |
|        18 |  4028 | `		if( iLevel < 2 ){` |
|         3 |  4029 | `			iLevel = 0;` |
|         1 |  4030 | `		}` |
|        18 |  4031 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  4032 | `	}` |
|         - |  4033 | `	/* Extract the target loop */` |
|     65007 |  4034 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     65007 |  4035 | `	if( pLoop == 0 ){` |
|         - |  4036 | `		/* Illegal break */` |
|        18 |  4037 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        18 |  4038 | `		if( rc == SXERR_ABORT ){` |
|         - |  4039 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4040 | `			return SXERR_ABORT;` |
|         - |  4041 | `		}` |
|        10 |  4042 | `	}else{` |
|         - |  4043 | `		sxu32 nInstrIdx;` |
|         - |  4044 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     64991 |  4045 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  4046 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     64991 |  4047 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     64991 |  4048 | `		if( rc == SXRET_OK ){` |
|         - |  4049 | `			/* Fix the jump later when the jump destination is resolved */` |
|     64991 |  4050 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     32493 |  4051 | `		}` |
|         - |  4052 | `	}` |
|     65007 |  4053 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4054 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4055 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  4056 | `	}` |
|         - |  4057 | `	/* Statement successfully compiled */` |
|     65007 |  4058 | `	return SXRET_OK;` |
|     32506 |  4059 | `}` |
|         - |  4060 | `/*` |
|         - |  4061 | ` * Compile or record a label.` |
|         - |  4062 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  4063 | ` * Example` |
|         - |  4064 | ` *  goto LABEL;` |
|         - |  4065 | ` *   echo 'Foo';` |
|         - |  4066 | ` *  LABEL:` |
|         - |  4067 | ` *   echo 'Bar';` |
|         - |  4068 | ` */` |
|       112 |  4069 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  4070 | `{` |
|         - |  4071 | `	GenBlock *pBlock;` |
|         - |  4072 | `	Label sLabel;` |
|         - |  4073 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|         - |  4074 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|         - |  4075 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|         - |  4076 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|         - |  4077 | `	{` |
|       117 |  4078 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4079 | `		char *zDup;` |
|         - |  4080 | `		/* Initialize label fields */` |
|       117 |  4081 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4082 | `		/* Duplicate label name */` |
|       117 |  4083 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       117 |  4084 | `		if( zDup == 0 ){` |
|       ! 0 |  4085 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4086 | `			return SXERR_ABORT;` |
|         - |  4087 | `		}` |
|       117 |  4088 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       117 |  4089 | `		sLabel.bRef  = FALSE;` |
|       117 |  4090 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       117 |  4091 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       117 |  4092 | `		pBlock = pGen->pCurrent;` |
|       233 |  4093 | `		while( pBlock ){` |
|       143 |  4094 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        26 |  4095 | `				break;` |
|         - |  4096 | `			}` |
|         - |  4097 | `			/* Point to the upper block */` |
|       121 |  4098 | `			pBlock = pBlock->pParent;` |
|         5 |  4099 | `		}` |
|       117 |  4100 | `		if( pBlock ){` |
|        26 |  4101 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        15 |  4102 | `		}else{` |
|        95 |  4103 | `			sLabel.pFunc = 0;` |
|         - |  4104 | `		}` |
|         - |  4105 | `		/* Insert in label set */` |
|       117 |  4106 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  4107 | `	}` |
|       117 |  4108 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  4109 | `	return SXRET_OK;` |
|        61 |  4110 | `}` |
|         - |  4111 | `/*` |
|         - |  4112 | ` * Compile the so hated 'goto' statement.` |
|         - |  4113 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  4114 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  4115 | ` * a compiler it has to do this.` |
|         - |  4116 | ` * According to the PHP language reference manual` |
|         - |  4117 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  4118 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  4119 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  4120 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  4121 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  4122 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  4123 | ` *   of a multi-level break` |
|         - |  4124 | ` */` |
|       152 |  4125 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  4126 | `{` |
|         - |  4127 | `	JumpFixup sJump;` |
|         - |  4128 | `	sxi32 rc;` |
|       157 |  4129 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  4130 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  4131 | `		/* Missing label */` |
|       ! 0 |  4132 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  4133 | `		if( rc == SXERR_ABORT ){` |
|         - |  4134 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4135 | `			return SXERR_ABORT;` |
|         - |  4136 | `		}` |
|       ! 0 |  4137 | `		return SXRET_OK;` |
|         - |  4138 | `	}` |
|       157 |  4139 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         5 |  4140 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|         5 |  4141 | `		if( rc == SXERR_ABORT ){` |
|         - |  4142 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4143 | `			return SXERR_ABORT;` |
|         - |  4144 | `		}` |
|         3 |  4145 | `	}else{` |
|       153 |  4146 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4147 | `		GenBlock *pBlock;` |
|         - |  4148 | `		char *zDup;` |
|         - |  4149 | `		/* Prepare the jump destination */` |
|       153 |  4150 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  4151 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  4152 | `		/* Duplicate label name */` |
|       153 |  4153 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  4154 | `		if( zDup == 0 ){` |
|       ! 0 |  4155 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4156 | `			return SXERR_ABORT;` |
|         - |  4157 | `		}` |
|       153 |  4158 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|         - |  4159 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|       153 |  4160 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|         - |  4161 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|         - |  4162 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|       153 |  4163 | `		pBlock = pGen->pCurrent;` |
|       327 |  4164 | `		while( pBlock ){` |
|       205 |  4165 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|        30 |  4166 | `				break;` |
|         - |  4167 | `			}` |
|         - |  4168 | `			/* Point to the upper block */` |
|       179 |  4169 | `			pBlock = pBlock->pParent;` |
|         5 |  4170 | `		}` |
|       153 |  4171 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        30 |  4172 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        17 |  4173 | `		}else{` |
|       127 |  4174 | `			sJump.pFunc = 0;` |
|         - |  4175 | `		}` |
|         - |  4176 | `		/* Emit the unconditional jump */` |
|       153 |  4177 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  4178 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  4179 | `		}` |
|         - |  4180 | `	}` |
|       157 |  4181 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  4182 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  4183 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  4184 | `	}` |
|         - |  4185 | `	/* Statement successfully compiled */` |
|       157 |  4186 | `	return SXRET_OK;` |
|        81 |  4187 | `}` |
|         - |  4188 | `/*` |
|         - |  4189 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  4190 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  4191 | ` * failure.` |
|         - |  4192 | ` */` |
|        20 |  4193 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         1 |  4194 | `{` |
|         - |  4195 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  4196 | `	sxu32 nRawObj;` |
|        10 |  4197 | `	sxu32 nObjIdx;` |
|         - |  4198 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  4199 | `	 * a PHP block.` |
|         - |  4200 | `	 */` |
|        10 |  4201 | `Consume:` |
|        21 |  4202 | `	nRawObj = nObjIdx = 0;` |
|        21 |  4203 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  4204 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  4205 | `		if( pRawObj == 0 ){` |
|       ! 0 |  4206 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4207 | `			return SXERR_ABORT;` |
|         - |  4208 | `		}` |
|         - |  4209 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  4210 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  4211 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  4212 | `		++nRawObj;` |
|       ! 0 |  4213 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  4214 | `	}` |
|        21 |  4215 | `	if( nRawObj > 0 ){` |
|         - |  4216 | `		/* Emit the consume instruction */` |
|       ! 0 |  4217 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4218 | `	}` |
|        21 |  4219 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  4220 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  4221 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  4222 | `		SySetReset(pTokenSet);` |
|       ! 0 |  4223 | `		SySetReset(&pGen->aTrivia);` |
|         - |  4224 | `		/* Tokenize input */` |
|       ! 0 |  4225 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  4226 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  4227 | `		/* Point to the fresh token stream */` |
|       ! 0 |  4228 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  4229 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  4230 | `		/* Advance the stream cursor */` |
|       ! 0 |  4231 | `		pGen->pRawIn++;` |
|         - |  4232 | `		/* TICKET 1433-011 */` |
|       ! 0 |  4233 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4234 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4235 | `			sxi32 rc;` |
|         - |  4236 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4237 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4238 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4239 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4240 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4241 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4242 | `				return SXERR_ABORT;` |
|       ! 0 |  4243 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4244 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4245 | `			}` |
|       ! 0 |  4246 | `			goto Consume;` |
|         - |  4247 | `		}` |
|       ! 0 |  4248 | `	}else{` |
|         - |  4249 | `		/* No more chunks to process */` |
|        21 |  4250 | `		pGen->pIn = pGen->pEnd;` |
|        21 |  4251 | `		return SXERR_EOF;` |
|         - |  4252 | `	}` |
|       ! 0 |  4253 | `	return SXRET_OK;` |
|        11 |  4254 | `}` |
|         - |  4255 | `/*` |
|         - |  4256 | ` * Compile a PHP block.` |
|         - |  4257 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4258 | ` * optionally delimited by braces {}.` |
|         - |  4259 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4260 | ` * and this function takes care of generating the appropriate error` |
|         - |  4261 | ` * message.` |
|         - |  4262 | ` */` |
|   6002200 |  4263 | `static sxi32 PH7_CompileBlock(` |
|         - |  4264 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4265 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4266 | `	)` |
|         5 |  4267 | `{` |
|         - |  4268 | `	sxi32 rc;` |
|         - |  4269 | `	sxu32 nLine;` |
|   6002205 |  4270 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5978219 |  4271 | `		nLine = pGen->pIn->nLine;` |
|   5978219 |  4272 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5978219 |  4273 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4274 | `			return SXERR_ABORT;` |
|         - |  4275 | `		}` |
|   5978219 |  4276 | `		pGen->pIn++;` |
|         - |  4277 | `		/* Compile until we hit the closing braces '}' */` |
|   8833821 |  4278 | `		for(;;){` |
|  17667647 |  4279 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        21 |  4280 | `				rc = GenStateNextChunk(&(*pGen));` |
|        21 |  4281 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4282 | `			 	   return SXERR_ABORT;` |
|         - |  4283 | `				}` |
|        21 |  4284 | `				if( rc == SXERR_EOF ){` |
|         - |  4285 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4286 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        21 |  4287 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        21 |  4288 | `					break;` |
|         - |  4289 | `				}` |
|       ! 0 |  4290 | `			}` |
|  17667627 |  4291 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4292 | `				/* Closing braces found,break immediately*/` |
|   5978199 |  4293 | `				pGen->pIn++;` |
|   5978199 |  4294 | `				break;` |
|         - |  4295 | `			}` |
|         - |  4296 | `			/* Compile a single statement */` |
|  11689433 |  4297 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  11689433 |  4298 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4299 | `				return SXERR_ABORT;` |
|         - |  4300 | `			}` |
|         5 |  4301 | `		}` |
|   5978219 |  4302 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   3013098 |  4303 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4304 | `		pGen->pIn++;` |
|       ! 0 |  4305 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4306 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4307 | `			return SXERR_ABORT;` |
|         - |  4308 | `		}` |
|         - |  4309 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4310 | `		for(;;){` |
|       ! 0 |  4311 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4312 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4313 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4314 | `			 	   return SXERR_ABORT;` |
|         - |  4315 | `				}` |
|       ! 0 |  4316 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4317 | `					/* No more token to process */` |
|       ! 0 |  4318 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4319 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4320 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4321 | `					}` |
|       ! 0 |  4322 | `					break;` |
|         - |  4323 | `				}` |
|       ! 0 |  4324 | `			}` |
|       ! 0 |  4325 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4326 | `				sxi32 nKwrd;` |
|         - |  4327 | `				/* Keyword found */` |
|       ! 0 |  4328 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4329 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4330 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4331 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4332 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4333 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4334 | `						}` |
|       ! 0 |  4335 | `						break;` |
|         - |  4336 | `				}` |
|       ! 0 |  4337 | `			}` |
|         - |  4338 | `			/* Compile a single statement */` |
|       ! 0 |  4339 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4340 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4341 | `				return SXERR_ABORT;` |
|         - |  4342 | `			}` |
|       ! 0 |  4343 | `		}` |
|       ! 0 |  4344 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4345 | `	}else{` |
|         - |  4346 | `		/* Compile a single statement */` |
|     23991 |  4347 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     23991 |  4348 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4349 | `			return SXERR_ABORT;` |
|         - |  4350 | `		}` |
|         - |  4351 | `	}` |
|         - |  4352 | `	/* Jump trailing semi-colons ';' */` |
|   6002205 |  4353 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4354 | `		pGen->pIn++;` |
|       ! 0 |  4355 | `	}` |
|   6002205 |  4356 | `	return SXRET_OK;` |
|   3001105 |  4357 | `}` |
|         - |  4358 | `/*` |
|         - |  4359 | ` * Compile the gentle 'while' statement.` |
|         - |  4360 | ` * According to the PHP language reference` |
|         - |  4361 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4362 | ` *  The basic form of a while statement is:` |
|         - |  4363 | ` *  while (expr)` |
|         - |  4364 | ` *   statement` |
|         - |  4365 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4366 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4367 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4368 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4369 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4370 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4371 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4372 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4373 | ` *  while (expr):` |
|         - |  4374 | ` *    statement` |
|         - |  4375 | ` *   endwhile;` |
|         - |  4376 | ` */` |
|     65016 |  4377 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4378 | `{` |
|     65021 |  4379 | `	GenBlock *pWhileBlock = 0;` |
|     65021 |  4380 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4381 | `	sxu32 nFalseJump;` |
|         - |  4382 | `	sxu32 nLine;` |
|         - |  4383 | `	sxi32 rc;` |
|     65021 |  4384 | `	nLine = pGen->pIn->nLine;` |
|         - |  4385 | `	/* Jump the 'while' keyword */` |
|     65021 |  4386 | `	pGen->pIn++;` |
|     65021 |  4387 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4388 | `		/* Syntax error */` |
|       ! 0 |  4389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4390 | `		if( rc == SXERR_ABORT ){` |
|         - |  4391 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4392 | `			return SXERR_ABORT;` |
|         - |  4393 | `		}` |
|       ! 0 |  4394 | `		goto Synchronize;` |
|         - |  4395 | `	}` |
|         - |  4396 | `	/* Jump the left parenthesis '(' */` |
|     65021 |  4397 | `	pGen->pIn++;` |
|         - |  4398 | `	/* Create the loop block */` |
|     65021 |  4399 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     65021 |  4400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4401 | `		return SXERR_ABORT;` |
|         - |  4402 | `	}` |
|         - |  4403 | `	/* Delimit the condition */` |
|     65021 |  4404 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     65021 |  4405 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4406 | `		/* Empty expression */` |
|         3 |  4407 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4408 | `		if( rc == SXERR_ABORT ){` |
|         - |  4409 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4410 | `			return SXERR_ABORT;` |
|         - |  4411 | `		}` |
|         1 |  4412 | `	}` |
|         - |  4413 | `	/* Swap token streams */` |
|     65021 |  4414 | `	pTmp = pGen->pEnd;` |
|     65021 |  4415 | `	pGen->pEnd = pEnd;` |
|         - |  4416 | `	/* Compile the expression */` |
|     65021 |  4417 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     65021 |  4418 | `	if( rc == SXERR_ABORT ){` |
|         - |  4419 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4420 | `		return SXERR_ABORT;` |
|         - |  4421 | `	}` |
|         - |  4422 | `	/* Update token stream */` |
|     65021 |  4423 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4424 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4425 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4426 | `			return SXERR_ABORT;` |
|         - |  4427 | `		}` |
|       ! 0 |  4428 | `		pGen->pIn++;` |
|       ! 0 |  4429 | `	}` |
|         - |  4430 | `	/* Synchronize pointers */` |
|     65021 |  4431 | `	pGen->pIn  = &pEnd[1];` |
|     65021 |  4432 | `	pGen->pEnd = pTmp;` |
|         - |  4433 | `	/* Emit the false jump */` |
|     65021 |  4434 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4435 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     65021 |  4436 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4437 | `	/* Compile the loop body */` |
|     65021 |  4438 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     65021 |  4439 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4440 | `		return SXERR_ABORT;` |
|         - |  4441 | `	}` |
|         - |  4442 | `	/* Emit the unconditional jump to the start of the loop */` |
|     65021 |  4443 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4444 | `	/* Fix all jumps now the destination is resolved */` |
|     65021 |  4445 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4446 | `	/* Release the loop block */` |
|     65021 |  4447 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4448 | `	/* Statement successfully compiled */` |
|     65021 |  4449 | `	return SXRET_OK;` |
|       ! 0 |  4450 | `Synchronize:` |
|         - |  4451 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4452 | `	 * compiling this erroneous block.` |
|         - |  4453 | `	 */` |
|       ! 0 |  4454 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4455 | `		pGen->pIn++;` |
|       ! 0 |  4456 | `	}` |
|       ! 0 |  4457 | `	return SXRET_OK;` |
|     32513 |  4458 | `}` |
|         - |  4459 | `/*` |
|         - |  4460 | ` * Compile the ugly do..while() statement.` |
|         - |  4461 | ` * According to the PHP language reference` |
|         - |  4462 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4463 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4464 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4465 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4466 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4467 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4468 | ` *  would end immediately).` |
|         - |  4469 | ` *  There is just one syntax for do-while loops:` |
|         - |  4470 | ` *  <?php` |
|         - |  4471 | ` *  $i = 0;` |
|         - |  4472 | ` *  do {` |
|         - |  4473 | ` *   echo $i;` |
|         - |  4474 | ` *  } while ($i > 0);` |
|         - |  4475 | ` * ?>` |
|         - |  4476 | ` */` |
|         2 |  4477 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4478 | `{` |
|         3 |  4479 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4480 | `	GenBlock *pDoBlock = 0;` |
|         - |  4481 | `	sxu32 nLine;` |
|         - |  4482 | `	sxi32 rc;` |
|         3 |  4483 | `	nLine = pGen->pIn->nLine;` |
|         - |  4484 | `	/* Jump the 'do' keyword */` |
|         3 |  4485 | `	pGen->pIn++;` |
|         - |  4486 | `	/* Create the loop block */` |
|         3 |  4487 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4488 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4489 | `		return SXERR_ABORT;` |
|         - |  4490 | `	}` |
|         - |  4491 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4492 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4493 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4494 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4495 | `		return SXERR_ABORT;` |
|         - |  4496 | `	}` |
|         3 |  4497 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4498 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4499 | `	}` |
|         3 |  4500 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4501 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4502 | `			/* Missing 'while' statement */` |
|         3 |  4503 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4504 | `			if( rc == SXERR_ABORT ){` |
|         - |  4505 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4506 | `				return SXERR_ABORT;` |
|         - |  4507 | `			}` |
|         3 |  4508 | `			goto Synchronize;` |
|         - |  4509 | `	}` |
|         - |  4510 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4511 | `	pGen->pIn++;` |
|       ! 0 |  4512 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4513 | `		/* Syntax error */` |
|       ! 0 |  4514 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4515 | `		if( rc == SXERR_ABORT ){` |
|         - |  4516 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4517 | `			return SXERR_ABORT;` |
|         - |  4518 | `		}` |
|       ! 0 |  4519 | `		goto Synchronize;` |
|         - |  4520 | `	}` |
|         - |  4521 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4522 | `	pGen->pIn++;` |
|         - |  4523 | `	/* Delimit the condition */` |
|       ! 0 |  4524 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4525 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4526 | `		/* Empty expression */` |
|       ! 0 |  4527 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4528 | `		if( rc == SXERR_ABORT ){` |
|         - |  4529 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4530 | `			return SXERR_ABORT;` |
|         - |  4531 | `		}` |
|       ! 0 |  4532 | `		goto Synchronize;` |
|         - |  4533 | `	}` |
|         - |  4534 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4535 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4536 | `		JumpFixup *aPost;` |
|         - |  4537 | `		VmInstr *pInstr;` |
|         - |  4538 | `		sxu32 nJumpDest;` |
|         - |  4539 | `		sxu32 n;` |
|       ! 0 |  4540 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4541 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4542 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4543 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4544 | `			if( pInstr ){` |
|         - |  4545 | `				/* Fix */` |
|       ! 0 |  4546 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4547 | `			}` |
|       ! 0 |  4548 | `		}` |
|       ! 0 |  4549 | `	}` |
|         - |  4550 | `	/* Swap token streams */` |
|       ! 0 |  4551 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4552 | `	pGen->pEnd = pEnd;` |
|         - |  4553 | `	/* Compile the expression */` |
|       ! 0 |  4554 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4555 | `	if( rc == SXERR_ABORT ){` |
|         - |  4556 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4557 | `		return SXERR_ABORT;` |
|         - |  4558 | `	}` |
|         - |  4559 | `	/* Update token stream */` |
|       ! 0 |  4560 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4561 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4562 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4563 | `			return SXERR_ABORT;` |
|         - |  4564 | `		}` |
|       ! 0 |  4565 | `		pGen->pIn++;` |
|       ! 0 |  4566 | `	}` |
|       ! 0 |  4567 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4568 | `	pGen->pEnd = pTmp;` |
|         - |  4569 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4570 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4571 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4572 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4573 | `	/* Release the loop block */` |
|       ! 0 |  4574 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4575 | `	/* Statement successfully compiled */` |
|       ! 0 |  4576 | `	return SXRET_OK;` |
|         1 |  4577 | `Synchronize:` |
|         - |  4578 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4579 | `	 * compiling this erroneous block.` |
|         - |  4580 | `	 */` |
|         3 |  4581 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4582 | `		pGen->pIn++;` |
|       ! 0 |  4583 | `	}` |
|         3 |  4584 | `	return SXRET_OK;` |
|         2 |  4585 | `}` |
|         - |  4586 | `/*` |
|         - |  4587 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4588 | ` * According to the PHP language reference` |
|         - |  4589 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4590 | ` *  The syntax of a for loop is:` |
|         - |  4591 | ` *  for (expr1; expr2; expr3)` |
|         - |  4592 | ` *   statement` |
|         - |  4593 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4594 | ` *  the beginning of the loop.` |
|         - |  4595 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4596 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4597 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4598 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4599 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4600 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4601 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4602 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4603 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4604 | ` *  of using the for truth expression.` |
|         - |  4605 | ` */` |
|    122260 |  4606 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4607 | `{` |
|    122265 |  4608 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    122265 |  4609 | `	GenBlock *pForBlock = 0;` |
|         - |  4610 | `	sxu32 nFalseJump;` |
|         - |  4611 | `	sxu32 nLine;` |
|         - |  4612 | `	sxi32 rc;` |
|    122265 |  4613 | `	nLine = pGen->pIn->nLine;` |
|         - |  4614 | `	/* Jump the 'for' keyword */` |
|    122265 |  4615 | `	pGen->pIn++;` |
|    122265 |  4616 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4617 | `		/* Syntax error */` |
|       ! 0 |  4618 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4619 | `		if( rc == SXERR_ABORT ){` |
|         - |  4620 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4621 | `			return SXERR_ABORT;` |
|         - |  4622 | `		}` |
|       ! 0 |  4623 | `		return SXRET_OK;` |
|         - |  4624 | `	}` |
|         - |  4625 | `	/* Jump the left parenthesis '(' */` |
|    122265 |  4626 | `	pGen->pIn++;` |
|         - |  4627 | `	/* Delimit the init-expr;condition;post-expr */` |
|    122265 |  4628 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    122265 |  4629 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4630 | `		/* Empty expression */` |
|       ! 0 |  4631 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4632 | `		if( rc == SXERR_ABORT ){` |
|         - |  4633 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4634 | `			return SXERR_ABORT;` |
|         - |  4635 | `		}` |
|         - |  4636 | `		/* Synchronize */` |
|       ! 0 |  4637 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4638 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4639 | `			pGen->pIn++;` |
|       ! 0 |  4640 | `		}` |
|       ! 0 |  4641 | `		return SXRET_OK;` |
|         - |  4642 | `	}` |
|         - |  4643 | `	/* Swap token streams */` |
|    122265 |  4644 | `	pTmp = pGen->pEnd;` |
|    122265 |  4645 | `	pGen->pEnd = pEnd;` |
|         - |  4646 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4647 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4648 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4649 | `	 * compiled through this same window — recorded as a known leniency. */` |
|    122265 |  4650 | `	pGen->nCommaExprOk++;` |
|         - |  4651 | `	/* Compile initialization expressions if available */` |
|    122265 |  4652 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4653 | `	/* Pop operand lvalues */` |
|    122265 |  4654 | `	if( rc == SXERR_ABORT ){` |
|         - |  4655 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4656 | `		return SXERR_ABORT;` |
|    122265 |  4657 | `	}else if( rc != SXERR_EMPTY ){` |
|    110815 |  4658 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55405 |  4659 | `	}` |
|    122265 |  4660 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4661 | `		/* Syntax error */` |
|       ! 0 |  4662 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4663 | `		if( rc == SXERR_ABORT ){` |
|         - |  4664 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4665 | `			return SXERR_ABORT;` |
|         - |  4666 | `		}` |
|       ! 0 |  4667 | `		return SXRET_OK;` |
|         - |  4668 | `	}` |
|         - |  4669 | `	/* Jump the trailing ';' */` |
|    122265 |  4670 | `	pGen->pIn++;` |
|         - |  4671 | `	/* Create the loop block */` |
|    122265 |  4672 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    122265 |  4673 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4674 | `		return SXERR_ABORT;` |
|         - |  4675 | `	}` |
|         - |  4676 | `	/* Deffer continue jumps */` |
|    122265 |  4677 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4678 | `	/* Compile the condition */` |
|    122265 |  4679 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    122265 |  4680 | `	if( rc == SXERR_ABORT ){` |
|         - |  4681 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4682 | `		return SXERR_ABORT;` |
|    122265 |  4683 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4684 | `		/* Emit the false jump */` |
|    110815 |  4685 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4686 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    110815 |  4687 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     55405 |  4688 | `	}` |
|    122265 |  4689 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4690 | `		/* Syntax error */` |
|         6 |  4691 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4692 | `		if( rc == SXERR_ABORT ){` |
|         - |  4693 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4694 | `			return SXERR_ABORT;` |
|         - |  4695 | `		}` |
|         6 |  4696 | `		return SXRET_OK;` |
|         - |  4697 | `	}` |
|         - |  4698 | `	/* Jump the trailing ';' */` |
|    122261 |  4699 | `	pGen->pIn++;` |
|         - |  4700 | `	/* Save the post condition stream */` |
|    122261 |  4701 | `	pPostStart = pGen->pIn;` |
|         - |  4702 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4703 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|    122261 |  4704 | `	pGen->nCommaExprOk--;` |
|    122261 |  4705 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    122261 |  4706 | `	pGen->pEnd = pTmp;` |
|    122261 |  4707 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    122261 |  4708 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4709 | `		return SXERR_ABORT;` |
|         - |  4710 | `	}` |
|         - |  4711 | `	/* Fix post-continue jumps */` |
|    122261 |  4712 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4713 | `		JumpFixup *aPost;` |
|         - |  4714 | `		VmInstr *pInstr;` |
|         - |  4715 | `		sxu32 nJumpDest;` |
|         - |  4716 | `		sxu32 n;` |
|     11465 |  4717 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|     11465 |  4718 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     38189 |  4719 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     26729 |  4720 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     26729 |  4721 | `			if( pInstr ){` |
|         - |  4722 | `				/* Fix jump */` |
|     26729 |  4723 | `				pInstr->iP2 = nJumpDest;` |
|     13362 |  4724 | `			}` |
|     13367 |  4725 | `		}` |
|      5730 |  4726 | `	}` |
|         - |  4727 | `	/* compile the post-expressions if available */` |
|    122261 |  4728 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4729 | `		pPostStart++;` |
|       ! 0 |  4730 | `	}` |
|    122261 |  4731 | `	if( pPostStart < pEnd ){` |
|         - |  4732 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    110813 |  4733 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    110813 |  4734 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|    110813 |  4735 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    110813 |  4736 | `		pGen->nCommaExprOk--;` |
|    110813 |  4737 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4738 | `			/* Syntax error */` |
|       ! 0 |  4739 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4740 | `			if( rc == SXERR_ABORT ){` |
|         - |  4741 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4742 | `				return SXERR_ABORT;` |
|         - |  4743 | `			}` |
|       ! 0 |  4744 | `			return SXRET_OK;` |
|         - |  4745 | `		}` |
|    110813 |  4746 | `		RE_SWAP_DELIMITER(pGen);` |
|    110813 |  4747 | `		if( rc == SXERR_ABORT ){` |
|         - |  4748 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4749 | `			return SXERR_ABORT;` |
|    110813 |  4750 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4751 | `			/* Pop operand lvalue */` |
|    110813 |  4752 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55404 |  4753 | `		}` |
|     55404 |  4754 | `	}` |
|         - |  4755 | `	/* Emit the unconditional jump to the start of the loop */` |
|    122261 |  4756 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4757 | `	/* Fix all jumps now the destination is resolved */` |
|    122261 |  4758 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4759 | `	/* Release the loop block */` |
|    122261 |  4760 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4761 | `	/* Statement successfully compiled */` |
|    122261 |  4762 | `	return SXRET_OK;` |
|     61135 |  4763 | `}` |
|         - |  4764 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4765 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4766 | ` * are allowed.` |
|         - |  4767 | ` */` |
|    436056 |  4768 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4769 | `{` |
|    436061 |  4770 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    436061 |  4771 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4772 | `		/* Unexpected expression */` |
|       ! 0 |  4773 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4774 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4775 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4776 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4777 | `		}` |
|       ! 0 |  4778 | `	}` |
|    436061 |  4779 | `	return rc;` |
|         5 |  4780 | `}` |
|         - |  4781 | `/*` |
|         - |  4782 | ` * Compile the 'foreach' statement.` |
|         - |  4783 | ` * According to the PHP language reference` |
|         - |  4784 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4785 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4786 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4787 | ` *  is a minor but useful extension of the first:` |
|         - |  4788 | ` *  foreach (array_expression as $value)` |
|         - |  4789 | ` *    statement` |
|         - |  4790 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4791 | ` *   statement` |
|         - |  4792 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4793 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4794 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4795 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4796 | ` *  to the variable $key on each loop.` |
|         - |  4797 | ` *  Note:` |
|         - |  4798 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4799 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4800 | ` *  Note:` |
|         - |  4801 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4802 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4803 | ` *  or after the foreach without resetting it.` |
|         - |  4804 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4805 | ` *  of copying the value.` |
|         - |  4806 | ` */` |
|    302222 |  4807 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4808 | `{` |
|    302227 |  4809 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    302227 |  4810 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    302227 |  4811 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4812 | `	ph7_foreach_info *pInfo;` |
|         - |  4813 | `	sxu32 nFalseJump;` |
|         - |  4814 | `	VmInstr *pInstr;` |
|         - |  4815 | `	sxu32 nLine;` |
|         - |  4816 | `	sxi32 rc;` |
|    302227 |  4817 | `	nLine = pGen->pIn->nLine;` |
|         - |  4818 | `	/* Jump the 'foreach' keyword */` |
|    302227 |  4819 | `	pGen->pIn++;` |
|    302227 |  4820 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4821 | `		/* Syntax error */` |
|       ! 0 |  4822 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4823 | `		if( rc == SXERR_ABORT ){` |
|         - |  4824 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4825 | `			return SXERR_ABORT;` |
|         - |  4826 | `		}` |
|       ! 0 |  4827 | `		goto Synchronize;` |
|         - |  4828 | `	}` |
|         - |  4829 | `	/* Jump the left parenthesis '(' */` |
|    302227 |  4830 | `	pGen->pIn++;` |
|         - |  4831 | `	/* Create the loop block */` |
|    302227 |  4832 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    302227 |  4833 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4834 | `		return SXERR_ABORT;` |
|         - |  4835 | `	}` |
|         - |  4836 | `	/* Delimit the expression */` |
|    302227 |  4837 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    302227 |  4838 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4839 | `		/* Empty expression */` |
|       ! 0 |  4840 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4841 | `		if( rc == SXERR_ABORT ){` |
|         - |  4842 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4843 | `			return SXERR_ABORT;` |
|         - |  4844 | `		}` |
|         - |  4845 | `		/* Synchronize */` |
|       ! 0 |  4846 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4847 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4848 | `			pGen->pIn++;` |
|       ! 0 |  4849 | `		}` |
|       ! 0 |  4850 | `		return SXRET_OK;` |
|         - |  4851 | `	}` |
|         - |  4852 | `	/* Compile the array expression */` |
|    302227 |  4853 | `	pCur = pGen->pIn;` |
|   1733055 |  4854 | `	while( pCur < pEnd ){` |
|   1733055 |  4855 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    332769 |  4856 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    332769 |  4857 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4858 | `				/* Break with the first 'as' found */` |
|    302227 |  4859 | `				break;` |
|         - |  4860 | `			}` |
|     15271 |  4861 | `		}` |
|         - |  4862 | `		/* Advance the stream cursor */` |
|   1430833 |  4863 | `		pCur++;` |
|         5 |  4864 | `	}` |
|    302227 |  4865 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4866 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4867 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4868 | `		if( rc == SXERR_ABORT ){` |
|         - |  4869 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4870 | `			return SXERR_ABORT;` |
|         - |  4871 | `		}` |
|       ! 0 |  4872 | `		goto Synchronize;` |
|         - |  4873 | `	}` |
|         - |  4874 | `	/* Swap token streams */` |
|    302227 |  4875 | `	pTmp = pGen->pEnd;` |
|    302227 |  4876 | `	pGen->pEnd = pCur;` |
|    302227 |  4877 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    302227 |  4878 | `	if( rc == SXERR_ABORT ){` |
|         - |  4879 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4880 | `		return SXERR_ABORT;` |
|         - |  4881 | `	}` |
|         - |  4882 | `	/* Update token stream */` |
|    302227 |  4883 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4884 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4885 | `		if( rc == SXERR_ABORT ){` |
|         - |  4886 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4887 | `			return SXERR_ABORT;` |
|         - |  4888 | `		}` |
|       ! 0 |  4889 | `		pGen->pIn++;` |
|       ! 0 |  4890 | `	}` |
|    302227 |  4891 | `	pCur++; /* Jump the 'as' keyword */` |
|    302227 |  4892 | `	pGen->pIn = pCur;` |
|    302227 |  4893 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4894 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4895 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4896 | `			return SXERR_ABORT;` |
|         - |  4897 | `		}` |
|       ! 0 |  4898 | `	}` |
|         - |  4899 | `	/* Create the foreach context */` |
|    302227 |  4900 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    302227 |  4901 | `	if( pInfo == 0 ){` |
|       ! 0 |  4902 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4903 | `		return SXERR_ABORT;` |
|         - |  4904 | `	}` |
|         - |  4905 | `	/* Zero the structure */` |
|    302227 |  4906 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4907 | `	/* Initialize structure fields */` |
|    302227 |  4908 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4909 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4910 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4911 | `	 * '=>'. */` |
|    302227 |  4912 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    302227 |  4913 | `	if( pCur < pEnd ){` |
|         - |  4914 | `		/* Compile the expression holding the key name */` |
|    133861 |  4915 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4916 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4917 | `			if( rc == SXERR_ABORT ){` |
|         - |  4918 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4919 | `				return SXERR_ABORT;` |
|         - |  4920 | `			}` |
|       ! 0 |  4921 | `		}else{` |
|    133861 |  4922 | `			pGen->pEnd = pCur;` |
|    133861 |  4923 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    133861 |  4924 | `			if( rc == SXERR_ABORT ){` |
|         - |  4925 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4926 | `				return SXERR_ABORT;` |
|         - |  4927 | `			}` |
|    133861 |  4928 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    133861 |  4929 | `			if( pInstr->p3 ){` |
|         - |  4930 | `				/* Record key name */` |
|    133861 |  4931 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     66928 |  4932 | `			}` |
|    133861 |  4933 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4934 | `		}` |
|    133861 |  4935 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     66928 |  4936 | `	}` |
|    302227 |  4937 | `	pGen->pEnd = pEnd;` |
|    302227 |  4938 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4939 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4940 | `		if( rc == SXERR_ABORT ){` |
|         - |  4941 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4942 | `			return SXERR_ABORT;` |
|         - |  4943 | `		}` |
|       ! 0 |  4944 | `		goto Synchronize;` |
|         - |  4945 | `	}` |
|    302227 |  4946 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4947 | `		pGen->pIn++;` |
|         - |  4948 | `		/* Pass by reference  */` |
|        33 |  4949 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4950 | `	}` |
|         - |  4951 | `	/* Check if the value target is list() */` |
|    302227 |  4952 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4953 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4954 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4955 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4956 | `		 */` |
|         - |  4957 | `		static int iForeachListCnt = 0;` |
|         - |  4958 | `		char zTmp[128];` |
|         - |  4959 | `		sxu32 nLen;` |
|         - |  4960 | `		char *zDup;` |
|        10 |  4961 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4962 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4963 | `		if( zDup == 0 ){` |
|       ! 0 |  4964 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4965 | `			return SXERR_ABORT;` |
|         - |  4966 | `		}` |
|        10 |  4967 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4968 | `		/* Save list() token boundaries */` |
|        10 |  4969 | `		pListStart = pGen->pIn;` |
|         - |  4970 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4971 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4972 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4973 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  4974 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  4975 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4976 | `				return SXERR_ABORT;` |
|         - |  4977 | `			}` |
|         3 |  4978 | `			goto Synchronize;` |
|         - |  4979 | `		}` |
|         7 |  4980 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  4981 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  4982 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4983 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4984 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  4985 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4986 | `				return SXERR_ABORT;` |
|         - |  4987 | `			}` |
|       ! 0 |  4988 | `			goto Synchronize;` |
|         - |  4989 | `		}` |
|         7 |  4990 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  4991 | `		pListEnd = pGen->pIn;` |
|         7 |  4992 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    302222 |  4993 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  4994 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  4995 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  4996 | `		 */` |
|         - |  4997 | `		static int iForeachShortListCnt = 0;` |
|         - |  4998 | `		char zTmp[128];` |
|         - |  4999 | `		sxu32 nLen;` |
|         - |  5000 | `		char *zDup;` |
|        15 |  5001 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        15 |  5002 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        15 |  5003 | `		if( zDup == 0 ){` |
|       ! 0 |  5004 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5005 | `			return SXERR_ABORT;` |
|         - |  5006 | `		}` |
|        15 |  5007 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  5008 | `		/* Save [...] token boundaries */` |
|        15 |  5009 | `		pListStart = pGen->pIn;` |
|         - |  5010 | `		/* Advance past [...] */` |
|        15 |  5011 | `		pGen->pIn++; /* Jump '[' */` |
|        15 |  5012 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        15 |  5013 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  5014 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5015 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  5016 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5017 | `				return SXERR_ABORT;` |
|         - |  5018 | `			}` |
|       ! 0 |  5019 | `			goto Synchronize;` |
|         - |  5020 | `		}` |
|        15 |  5021 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        15 |  5022 | `		pListEnd = pGen->pIn;` |
|        15 |  5023 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         8 |  5024 | `	}else{` |
|         - |  5025 | `		/* Compile the expression holding the value name */` |
|    302205 |  5026 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    302205 |  5027 | `		if( rc == SXERR_ABORT ){` |
|         - |  5028 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5029 | `			return SXERR_ABORT;` |
|         - |  5030 | `		}` |
|    302205 |  5031 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    302205 |  5032 | `		if( pInstr->p3 ){` |
|         - |  5033 | `			/* Record value name */` |
|    302205 |  5034 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    151100 |  5035 | `		}` |
|         - |  5036 | `	}` |
|         - |  5037 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    302225 |  5038 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  5039 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    302225 |  5040 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  5041 | `	/* Record the first instruction to execute */` |
|    302225 |  5042 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5043 | `	/* Emit the FOREACH_STEP instruction */` |
|    302225 |  5044 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  5045 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    302225 |  5046 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  5047 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    302225 |  5048 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  5049 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  5050 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  5051 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  5052 | `		 */` |
|        21 |  5053 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  5054 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  5055 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  5056 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  5057 | `		 */` |
|        21 |  5058 | `		pSavedIn = pGen->pIn;` |
|        21 |  5059 | `		pSavedEnd = pGen->pEnd;` |
|        21 |  5060 | `		pGen->pIn = pListStart;` |
|        21 |  5061 | `		pGen->pEnd = pListEnd;` |
|        21 |  5062 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        15 |  5063 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         8 |  5064 | `		}else{` |
|         7 |  5065 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  5066 | `		}` |
|        21 |  5067 | `		pGen->pIn = pSavedIn;` |
|        21 |  5068 | `		pGen->pEnd = pSavedEnd;` |
|        21 |  5069 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5070 | `			return SXERR_ABORT;` |
|         - |  5071 | `		}` |
|         - |  5072 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        21 |  5073 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        10 |  5074 | `	}` |
|         - |  5075 | `	/* Compile the loop body */` |
|    302225 |  5076 | `	pGen->pIn = &pEnd[1];` |
|    302225 |  5077 | `	pGen->pEnd = pTmp;` |
|    302225 |  5078 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    302225 |  5079 | `	if( rc == SXERR_ABORT ){` |
|         - |  5080 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5081 | `		return SXERR_ABORT;` |
|         - |  5082 | `	}` |
|         - |  5083 | `	/* Emit the unconditional jump to the start of the loop */` |
|    302225 |  5084 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  5085 | `	/* Fix all jumps now the destination is resolved */` |
|    302225 |  5086 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  5087 | `	/* Release the loop block */` |
|    302225 |  5088 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5089 | `	/* Statement successfully compiled */` |
|    302225 |  5090 | `	return SXRET_OK;` |
|         1 |  5091 | `Synchronize:` |
|         - |  5092 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  5093 | `	 * compiling this erroneous block.` |
|         - |  5094 | `	 */` |
|         3 |  5095 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  5096 | `		pGen->pIn++;` |
|       ! 0 |  5097 | `	}` |
|         3 |  5098 | `	return SXRET_OK;` |
|    151116 |  5099 | `}` |
|         - |  5100 | `/*` |
|         - |  5101 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  5102 | ` * According to the PHP language reference` |
|         - |  5103 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  5104 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  5105 | ` *  that is similar to that of C:` |
|         - |  5106 | ` *  if (expr)` |
|         - |  5107 | ` *   statement` |
|         - |  5108 | ` *  else construct:` |
|         - |  5109 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  5110 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  5111 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  5112 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  5113 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  5114 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  5115 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  5116 | ` *  elseif` |
|         - |  5117 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  5118 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  5119 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  5120 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  5121 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  5122 | ` *   <?php` |
|         - |  5123 | ` *    if ($a > $b) {` |
|         - |  5124 | ` *     echo "a is bigger than b";` |
|         - |  5125 | ` *    } elseif ($a == $b) {` |
|         - |  5126 | ` *     echo "a is equal to b";` |
|         - |  5127 | ` *    } else {` |
|         - |  5128 | ` *     echo "a is smaller than b";` |
|         - |  5129 | ` *    }` |
|         - |  5130 | ` *    ?>` |
|         - |  5131 | ` */` |
|   2203816 |  5132 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  5133 | `{` |
|   2203821 |  5134 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   2203821 |  5135 | `	GenBlock *pCondBlock = 0;` |
|         - |  5136 | `	sxu32 nJumpIdx;` |
|         - |  5137 | `	sxu32 nKeyID;` |
|         - |  5138 | `	sxi32 rc;` |
|         - |  5139 | `	/* Jump the 'if' keyword */` |
|   2203821 |  5140 | `	pGen->pIn++;` |
|   2203821 |  5141 | `	pToken = pGen->pIn;` |
|         - |  5142 | `	/* Create the conditional block */` |
|   2203821 |  5143 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   2203821 |  5144 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5145 | `		return SXERR_ABORT;` |
|         - |  5146 | `	}` |
|         - |  5147 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1241251 |  5148 | `	for(;;){` |
|   2482507 |  5149 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  5150 | `			/* Syntax error */` |
|       ! 0 |  5151 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5152 | `				pToken--;` |
|       ! 0 |  5153 | `			}` |
|       ! 0 |  5154 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  5155 | `			if( rc == SXERR_ABORT ){` |
|         - |  5156 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5157 | `				return SXERR_ABORT;` |
|         - |  5158 | `			}` |
|       ! 0 |  5159 | `			goto Synchronize;` |
|         - |  5160 | `		}` |
|         - |  5161 | `		/* Jump the left parenthesis '(' */` |
|   2482507 |  5162 | `		pToken++;` |
|         - |  5163 | `		/* Delimit the condition */` |
|   2482507 |  5164 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2482507 |  5165 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  5166 | `			/* Syntax error */` |
|        11 |  5167 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5168 | `				pToken--;` |
|       ! 0 |  5169 | `			}` |
|        11 |  5170 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  5171 | `			if( rc == SXERR_ABORT ){` |
|         - |  5172 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5173 | `				return SXERR_ABORT;` |
|         - |  5174 | `			}` |
|        11 |  5175 | `			goto Synchronize;` |
|         - |  5176 | `		}` |
|         - |  5177 | `		/* Swap token streams */` |
|   2482499 |  5178 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  5179 | `		/* Compile the condition */` |
|   2482499 |  5180 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5181 | `		/* Update token stream */` |
|   2482499 |  5182 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5183 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5184 | `			pGen->pIn++;` |
|       ! 0 |  5185 | `		}` |
|   2482499 |  5186 | `		pGen->pIn  = &pEnd[1];` |
|   2482499 |  5187 | `		pGen->pEnd = pTmp;` |
|   2482499 |  5188 | `		if( rc == SXERR_ABORT ){` |
|         - |  5189 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5190 | `			return SXERR_ABORT;` |
|         - |  5191 | `		}` |
|         - |  5192 | `		/* Emit the false jump */` |
|   2482499 |  5193 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5194 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2482499 |  5195 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5196 | `		/* Compile the body */` |
|   2482499 |  5197 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2482499 |  5198 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5199 | `			return SXERR_ABORT;` |
|         - |  5200 | `		}` |
|   2482499 |  5201 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    485251 |  5202 | `			break;` |
|         - |  5203 | `		}` |
|         - |  5204 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1512007 |  5205 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1512007 |  5206 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   1022991 |  5207 | `			break;` |
|         - |  5208 | `		}` |
|         - |  5209 | `		/* Emit the unconditional jump */` |
|    489021 |  5210 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5211 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    489021 |  5212 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    489021 |  5213 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    294287 |  5214 | `			pToken = &pGen->pIn[1];` |
|    294287 |  5215 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     83990 |  5216 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    105170 |  5217 | `					break;` |
|         - |  5218 | `			}` |
|     83957 |  5219 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     41976 |  5220 | `		}` |
|    278691 |  5221 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5222 | `		/* Synchronize cursors */` |
|    278691 |  5223 | `		pToken = pGen->pIn;` |
|         - |  5224 | `		/* Fix the false jump */` |
|    278691 |  5225 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5226 | `	} /* For(;;) */` |
|         - |  5227 | `	/* Fix the false jump */` |
|   2203813 |  5228 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   2203813 |  5229 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1233316 |  5230 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5231 | `			/* Compile the else block */` |
|    210335 |  5232 | `			pGen->pIn++;` |
|    210335 |  5233 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    210335 |  5234 | `			if( rc == SXERR_ABORT ){` |
|         - |  5235 |  |
|       ! 0 |  5236 | `				return SXERR_ABORT;` |
|         - |  5237 | `			}` |
|    105165 |  5238 | `	}` |
|   2203813 |  5239 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5240 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   2203813 |  5241 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5242 | `	/* Release the conditional block */` |
|   2203813 |  5243 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5244 | `	/* Statement successfully compiled */` |
|   2203813 |  5245 | `	return SXRET_OK;` |
|         4 |  5246 | `Synchronize:` |
|         - |  5247 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5248 | `	 */` |
|        67 |  5249 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5250 | `		pGen->pIn++;` |
|         3 |  5251 | `	}` |
|        11 |  5252 | `	return SXRET_OK;` |
|   1101913 |  5253 | `}` |
|         - |  5254 | `/*` |
|         - |  5255 | ` * Compile the global construct.` |
|         - |  5256 | ` * According to the PHP language reference` |
|         - |  5257 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5258 | ` *  to be used in that function.` |
|         - |  5259 | ` *  Example #1 Using global` |
|         - |  5260 | ` *  <?php` |
|         - |  5261 | ` *   $a = 1;` |
|         - |  5262 | ` *   $b = 2;` |
|         - |  5263 | ` *   function Sum()` |
|         - |  5264 | ` *   {` |
|         - |  5265 | ` *    global $a, $b;` |
|         - |  5266 | ` *    $b = $a + $b;` |
|         - |  5267 | ` *   }` |
|         - |  5268 | ` *   Sum();` |
|         - |  5269 | ` *   echo $b;` |
|         - |  5270 | ` *  ?>` |
|         - |  5271 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5272 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5273 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5274 | ` */` |
|        38 |  5275 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5276 | `{` |
|        43 |  5277 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5278 | `	sxi32 nExpr;` |
|         - |  5279 | `	sxi32 rc;` |
|         - |  5280 | `	/* Jump the 'global' keyword */` |
|        43 |  5281 | `	pGen->pIn++;` |
|        43 |  5282 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5283 | `		/* Nothing to process */` |
|       ! 0 |  5284 | `		return SXRET_OK;` |
|         - |  5285 | `	}` |
|        43 |  5286 | `	pTmp = pGen->pEnd;` |
|        43 |  5287 | `	nExpr = 0;` |
|        91 |  5288 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5289 | `		if( pGen->pIn < pNext ){` |
|        53 |  5290 | `			pGen->pEnd = pNext;` |
|        53 |  5291 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5292 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5293 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5294 | `					return SXERR_ABORT;` |
|         - |  5295 | `				}` |
|       ! 0 |  5296 | `			}else{` |
|        53 |  5297 | `				pGen->pIn++;` |
|        53 |  5298 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5299 | `					/* Emit a warning */` |
|       ! 0 |  5300 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5301 | `				}else{` |
|        53 |  5302 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5303 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5304 | `						return SXERR_ABORT;` |
|        53 |  5305 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5306 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5307 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5308 | `							/* Variable name, not a constant */` |
|        53 |  5309 | `							pLast->iP1 = 0;` |
|        24 |  5310 | `						}` |
|        53 |  5311 | `						nExpr++;` |
|        24 |  5312 | `					}` |
|         - |  5313 | `				}` |
|         - |  5314 | `			}` |
|        24 |  5315 | `		}` |
|         - |  5316 | `		/* Next expression in the stream */` |
|        53 |  5317 | `		pGen->pIn = pNext;` |
|         - |  5318 | `		/* Jump trailing commas */` |
|        63 |  5319 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5320 | `			pGen->pIn++;` |
|         5 |  5321 | `		}` |
|         5 |  5322 | `	}` |
|         - |  5323 | `	/* Restore token stream */` |
|        43 |  5324 | `	pGen->pEnd = pTmp;` |
|        43 |  5325 | `	if( nExpr > 0 ){` |
|         - |  5326 | `		/* Emit the uplink instruction */` |
|        43 |  5327 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5328 | `	}` |
|        43 |  5329 | `	return SXRET_OK;` |
|        24 |  5330 | `}` |
|         - |  5331 | `/*` |
|         - |  5332 | ` * Compile the return statement.` |
|         - |  5333 | ` * According to the PHP language reference` |
|         - |  5334 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5335 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5336 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5337 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5338 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5339 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5340 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5341 | ` *  from within the main script file, then script execution end.` |
|         - |  5342 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5343 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5344 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5345 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5346 | ` */` |
|   2986362 |  5347 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5348 | `{` |
|   2986367 |  5349 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5350 | `	sxi32 rc;` |
|   2986367 |  5351 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2986367 |  5352 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5353 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5354 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5355 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5356 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5357 | `	 * normally below so token processing stays consistent. */` |
|   7866271 |  5358 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4879909 |  5359 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5360 | `	}` |
|   2986362 |  5361 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2986333 |  5362 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5363 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5364 | `			"A never-returning function must not return");` |
|         3 |  5365 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5366 | `			return SXERR_ABORT;` |
|         - |  5367 | `		}` |
|         1 |  5368 | `	}` |
|         - |  5369 | `	/* Jump the 'return' keyword */` |
|   2986367 |  5370 | `	pGen->pIn++;` |
|   2986367 |  5371 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5372 | `		/* Compile the expression */` |
|   2890937 |  5373 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2890937 |  5374 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5375 | `			return SXERR_ABORT;` |
|   2890937 |  5376 | `		}else if(rc != SXERR_EMPTY ){` |
|   2890937 |  5377 | `			nRet = 1;` |
|   1445466 |  5378 | `		}` |
|   1445466 |  5379 | `	}` |
|         - |  5380 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5381 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5382 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5383 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2986367 |  5384 | `	if( pGen->bInGenerator ){` |
|      3849 |  5385 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3849 |  5386 | `		return SXRET_OK;` |
|         - |  5387 | `	}` |
|         - |  5388 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5389 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5390 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5391 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5392 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2982523 |  5393 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2982523 |  5394 | `	return SXRET_OK;` |
|   1493186 |  5395 | `}` |
|         - |  5396 | `/*` |
|         - |  5397 | ` * Compile a yield expression.` |
|         - |  5398 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5399 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5400 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5401 | ` */` |
|     15650 |  5402 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5403 | `{` |
|         - |  5404 | `	SyToken *pTmp, *pSplit;` |
|     15655 |  5405 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15655 |  5406 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5407 | `	sxi32 rc;` |
|      7825 |  5408 | `	(void)iCompileFlag;` |
|         - |  5409 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15655 |  5410 | `	pGen->pIn++;` |
|         - |  5411 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5412 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5413 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5414 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5415 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15650 |  5416 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7860 |  5417 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5418 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5419 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5420 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5421 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5422 | `			return SXERR_ABORT;` |
|         - |  5423 | `		}` |
|        67 |  5424 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5425 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5426 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5427 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5428 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5429 | `				return SXERR_ABORT;` |
|         - |  5430 | `			}` |
|       ! 0 |  5431 | `		}` |
|        67 |  5432 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5433 | `		return SXRET_OK;` |
|         - |  5434 | `	}` |
|     15593 |  5435 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5436 | `		/* Bare yield — no value */` |
|         3 |  5437 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5438 | `		return SXRET_OK;` |
|         - |  5439 | `	}` |
|         - |  5440 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15591 |  5441 | `	pSplit = 0;` |
|         - |  5442 | `	{` |
|     15591 |  5443 | `		SyToken *pCur = pGen->pIn;` |
|     15591 |  5444 | `		sxi32 nNest = 0;` |
|     46577 |  5445 | `		while( pCur < pGen->pEnd ){` |
|     46269 |  5446 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5447 | `				nNest++;` |
|     46261 |  5448 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5449 | `				nNest--;` |
|     46245 |  5450 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15283 |  5451 | `				pSplit = pCur;` |
|     15283 |  5452 | `				break;` |
|         - |  5453 | `			}` |
|     30991 |  5454 | `			pCur++;` |
|         5 |  5455 | `		}` |
|         - |  5456 | `	}` |
|     15591 |  5457 | `	pTmp = pGen->pEnd;` |
|     15591 |  5458 | `	if( pSplit ){` |
|         - |  5459 | `		/* yield $key => $value */` |
|     15283 |  5460 | `		pGen->pEnd = pSplit;` |
|     15283 |  5461 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15283 |  5462 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15283 |  5463 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15283 |  5464 | `		pGen->pEnd = pTmp;` |
|     15283 |  5465 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15283 |  5466 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15283 |  5467 | `		iP1 = 1;` |
|     15283 |  5468 | `		iP2 = 1;` |
|      7644 |  5469 | `	}else{` |
|         - |  5470 | `		/* yield $value */` |
|       313 |  5471 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       313 |  5472 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       313 |  5473 | `		if( rc != SXERR_EMPTY ){` |
|       313 |  5474 | `			iP1 = 1;` |
|       154 |  5475 | `		}` |
|         - |  5476 | `	}` |
|     15591 |  5477 | `	pGen->pEnd = pTmp;` |
|     15591 |  5478 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15591 |  5479 | `	return SXRET_OK;` |
|      7830 |  5480 | `}` |
|         - |  5481 | `/*` |
|         - |  5482 | ` * Compile the die/exit language construct.` |
|         - |  5483 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5484 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5485 | ` */` |
|       128 |  5486 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5487 | `{` |
|       133 |  5488 | `	sxi32 nExpr = 0;` |
|         - |  5489 | `	sxi32 rc;` |
|         - |  5490 | `	/* Jump the die/exit keyword */` |
|       133 |  5491 | `	pGen->pIn++;` |
|       133 |  5492 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5493 | `		/* Compile the expression */` |
|       133 |  5494 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5495 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5496 | `			return SXERR_ABORT;` |
|       133 |  5497 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5498 | `			nExpr = 1;` |
|        64 |  5499 | `		}` |
|        64 |  5500 | `	}` |
|         - |  5501 | `	/* Emit the HALT instruction */` |
|       133 |  5502 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5503 | `	return SXRET_OK;` |
|        69 |  5504 | `}` |
|         - |  5505 | `/*` |
|         - |  5506 | ` * Compile the 'echo' language construct.` |
|         - |  5507 | ` */` |
|     17796 |  5508 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5509 | `{` |
|     17801 |  5510 | `	SyToken *pTmp,*pNext = 0;` |
|     17801 |  5511 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17801 |  5512 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17801 |  5513 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5514 | `	sxi32 rc;` |
|         - |  5515 | `	/* Jump the 'echo' keyword */` |
|     17801 |  5516 | `	pGen->pIn++;` |
|         - |  5517 | `	/* Compile arguments one after one */` |
|     17801 |  5518 | `	pTmp = pGen->pEnd;` |
|     44975 |  5519 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     27181 |  5520 | `		if( pGen->pIn < pNext ){` |
|     27181 |  5521 | `			pGen->pEnd = pNext;` |
|     27181 |  5522 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     27181 |  5523 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5524 | `				return SXERR_ABORT;` |
|     27181 |  5525 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5526 | `				/* Emit the consume instruction */` |
|     27155 |  5527 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     27155 |  5528 | `				nExpr++;` |
|     27155 |  5529 | `				bExpectMore = 0;` |
|     13575 |  5530 | `			}` |
|     13588 |  5531 | `		}` |
|         - |  5532 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5533 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     36567 |  5534 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      9393 |  5535 | `			if( bExpectMore ){` |
|         - |  5536 | `				/* two commas in a row */` |
|         3 |  5537 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5538 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5539 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5540 | `			}` |
|      9391 |  5541 | `			bExpectMore = 1;` |
|      9391 |  5542 | `			pNext++;` |
|         5 |  5543 | `		}` |
|     27179 |  5544 | `		pGen->pIn = pNext;` |
|         5 |  5545 | `	}` |
|         - |  5546 | `	/* Restore token stream */` |
|     17799 |  5547 | `	pGen->pEnd = pTmp;` |
|     17799 |  5548 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5549 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5550 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5551 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5552 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5553 | `	}` |
|     17769 |  5554 | `	return SXRET_OK;` |
|      8903 |  5555 | `}` |
|         - |  5556 | `/*` |
|         - |  5557 | ` * Compile the static statement.` |
|         - |  5558 | ` * According to the PHP language reference` |
|         - |  5559 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5560 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5561 | ` *  when program execution leaves this scope.` |
|         - |  5562 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5563 | ` * Symisc eXtension.` |
|         - |  5564 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5565 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5566 | ` *  Example` |
|         - |  5567 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5568 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5569 | ` */` |
|     11460 |  5570 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         5 |  5571 | `{` |
|         - |  5572 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5573 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5574 | `	GenBlock *pBlock;` |
|         - |  5575 | `	SyString *pName;` |
|         - |  5576 | `	char *zDup;` |
|         - |  5577 | `	sxu32 nLine;` |
|         - |  5578 | `	sxi32 rc;` |
|         - |  5579 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5580 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5581 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|     11460 |  5582 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      5736 |  5583 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5584 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5585 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5586 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5587 | `			return SXERR_ABORT;` |
|         3 |  5588 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5589 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5590 | `		}` |
|         3 |  5591 | `		return SXRET_OK;` |
|         - |  5592 | `	}` |
|         - |  5593 | `	/* Jump the static keyword */` |
|     11463 |  5594 | `	nLine = pGen->pIn->nLine;` |
|     11463 |  5595 | `	pGen->pIn++;` |
|         - |  5596 | `	/* Extract the enclosing function if any */` |
|     11463 |  5597 | `	pBlock = pGen->pCurrent;` |
|     22921 |  5598 | `	while( pBlock ){` |
|     22921 |  5599 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|     11463 |  5600 | `			break;` |
|         - |  5601 | `		}` |
|         - |  5602 | `		/* Point to the upper block */` |
|     11463 |  5603 | `		pBlock = pBlock->pParent;` |
|         5 |  5604 | `	}` |
|     11463 |  5605 | `	if( pBlock == 0 ){` |
|         - |  5606 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5607 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5608 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5609 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5610 | `				return SXERR_ABORT;` |
|         - |  5611 | `			}` |
|       ! 0 |  5612 | `			goto Synchronize;` |
|         - |  5613 | `		}` |
|         - |  5614 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5615 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5616 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5617 | `			return SXERR_ABORT;` |
|       ! 0 |  5618 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5619 | `			/* Emit the POP instruction */` |
|       ! 0 |  5620 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5621 | `		}` |
|       ! 0 |  5622 | `		return SXRET_OK;` |
|         - |  5623 | `	}` |
|     11463 |  5624 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5625 | `	/* Make sure we are dealing with a valid statement */` |
|     11463 |  5626 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     11456 |  5627 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5628 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5629 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5630 | `				return SXERR_ABORT;` |
|         - |  5631 | `			}` |
|         3 |  5632 | `			goto Synchronize;` |
|         - |  5633 | `	}` |
|     11461 |  5634 | `	pGen->pIn++;` |
|         - |  5635 | `	/* Extract variable name */` |
|     11461 |  5636 | `	pName = &pGen->pIn->sData;` |
|     11461 |  5637 | `	pGen->pIn++; /* Jump the var name */` |
|     11461 |  5638 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5639 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5640 | `		goto Synchronize;` |
|         - |  5641 | `	}` |
|         - |  5642 | `	/* Initialize the structure describing the static variable */` |
|     11461 |  5643 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     11461 |  5644 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5645 | `	/* Duplicate variable name */` |
|     11461 |  5646 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     11461 |  5647 | `	if( zDup == 0 ){` |
|       ! 0 |  5648 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5649 | `		return SXERR_ABORT;` |
|         - |  5650 | `	}` |
|     11461 |  5651 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5652 | `	/* Check if we have an expression to compile */` |
|     11461 |  5653 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5654 | `		SySet *pInstrContainer;` |
|         - |  5655 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5656 | `		 * Static variable can take any complex expression including function` |
|         - |  5657 | `		 * call as their initialization value.` |
|         - |  5658 | `		 * Example:` |
|         - |  5659 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5660 | `		 */` |
|     11461 |  5661 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5662 | `		/* Swap bytecode container */` |
|     11461 |  5663 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     11461 |  5664 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5665 | `		/* Compile the expression */` |
|     11461 |  5666 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5667 | `		/* Emit the done instruction */` |
|     11461 |  5668 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5669 | `		/* Restore default bytecode container */` |
|     11461 |  5670 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      5728 |  5671 | `	}` |
|         - |  5672 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     11461 |  5673 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     11461 |  5674 | `	return SXRET_OK;` |
|         1 |  5675 | `Synchronize:` |
|         - |  5676 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5677 | `	 * statement.` |
|         - |  5678 | `	 */` |
|         5 |  5679 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5680 | `		pGen->pIn++;` |
|         1 |  5681 | `	}` |
|         3 |  5682 | `	return SXRET_OK;` |
|      5735 |  5683 | `}` |
|         - |  5684 | `/*` |
|         - |  5685 | ` * Compile the var statement.` |
|         - |  5686 | ` * Symisc Extension:` |
|         - |  5687 | ` *      var statement can be used outside of a class definition.` |
|         - |  5688 | ` */` |
|         4 |  5689 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5690 | `{` |
|         - |  5691 | `	sxu32 nLine;` |
|         - |  5692 | `	sxi32 rc;` |
|         5 |  5693 | `	nLine = pGen->pIn->nLine;` |
|         - |  5694 | `	/* Jump the 'var' keyword */` |
|         5 |  5695 | `	pGen->pIn++;` |
|         5 |  5696 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5697 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5698 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5699 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5700 | `			pGen->pIn++;` |
|       ! 0 |  5701 | `		}` |
|       ! 0 |  5702 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5703 | `			return SXERR_ABORT;` |
|         - |  5704 | `		}` |
|       ! 0 |  5705 | `	}else{` |
|         - |  5706 | `		/* Compile the expression */` |
|         5 |  5707 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5708 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5709 | `			return SXERR_ABORT;` |
|         5 |  5710 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5711 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5712 | `		}` |
|         - |  5713 | `	}` |
|         5 |  5714 | `	return SXRET_OK;` |
|         3 |  5715 | `}` |
|         - |  5716 | `/*` |
|         - |  5717 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5718 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5719 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5720 | ` */` |
|         - |  5721 | `/*` |
|         - |  5722 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5723 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5724 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5725 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5726 | ` *` |
|         - |  5727 | ` * Resolution order:` |
|         - |  5728 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5729 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5730 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5731 | ` *` |
|         - |  5732 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5733 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5734 | ` * Returns the (possibly new) literal index.` |
|         - |  5735 | ` */` |
|   5585426 |  5736 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5737 | `{` |
|         - |  5738 | `	ph7_value *pLit;` |
|         - |  5739 | `	const char *zLit;` |
|         - |  5740 | `	SyString sQualified;` |
|         - |  5741 | `	sxu32 nLit;` |
|         - |  5742 | `	sxu32 k;` |
|         - |  5743 | `	sxu32 nNewIdx;` |
|         - |  5744 | `	int hasNsSep;` |
|         - |  5745 | `	SyHashEntry *pImport;` |
|         - |  5746 | `	ph7_value *pNew;` |
|   5585431 |  5747 | `	if( pFromImport ){` |
|   4513035 |  5748 | `		*pFromImport = 0;` |
|   2256515 |  5749 | `	}` |
|   5585431 |  5750 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   5585431 |  5751 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5752 | `		return nOrigIdx;` |
|         - |  5753 | `	}` |
|   5585431 |  5754 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   5585431 |  5755 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5756 | `	/* Skip if already qualified (contains backslash) */` |
|   5585431 |  5757 | `	hasNsSep = 0;` |
|  66128437 |  5758 | `	for( k = 0; k < nLit; k++ ){` |
|  60543015 |  5759 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  30271508 |  5760 | `	}` |
|   5585431 |  5761 | `	if( hasNsSep ){` |
|         5 |  5762 | `		return nOrigIdx;` |
|         - |  5763 | `	}` |
|         - |  5764 | `	/* Check use imports first (works even outside namespaces) */` |
|   5585427 |  5765 | `	SyBlobReset(&pGen->sWorker);` |
|   5585427 |  5766 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   5585427 |  5767 | `	if( pImport ){` |
|        41 |  5768 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5769 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5770 | `		if( pFromImport ){` |
|        18 |  5771 | `			*pFromImport = 1;` |
|         8 |  5772 | `		}` |
|        23 |  5773 | `	}else{` |
|   5585391 |  5774 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   5585261 |  5775 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5776 | `		}` |
|         - |  5777 | `		/* Prepend current namespace */` |
|       135 |  5778 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       135 |  5779 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       135 |  5780 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5781 | `	}` |
|         - |  5782 | `	/* Look up or create a new literal for the qualified name */` |
|       171 |  5783 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       171 |  5784 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        77 |  5785 | `		return nNewIdx; /* Already interned */` |
|         - |  5786 | `	}` |
|        99 |  5787 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        99 |  5788 | `	if( pNew == 0 ){` |
|       ! 0 |  5789 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5790 | `	}` |
|        99 |  5791 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        99 |  5792 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        99 |  5793 | `	return nNewIdx;` |
|   2792718 |  5794 | `}` |
|         - |  5795 | `/*` |
|         - |  5796 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5797 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5798 | ` */` |
|    448258 |  5799 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5800 | `{` |
|         - |  5801 | `	SyHashEntry *pImport;` |
|         - |  5802 | `	/* Check use imports first */` |
|    448263 |  5803 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    448263 |  5804 | `	if( pImport ){` |
|        21 |  5805 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        21 |  5806 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        21 |  5807 | `		return;` |
|         - |  5808 | `	}` |
|         - |  5809 | `	/* Prepend current namespace if active */` |
|    448245 |  5810 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        14 |  5811 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        14 |  5812 | `		SyBlobAppend(pOut,"\\",1);` |
|         6 |  5813 | `	}` |
|    448245 |  5814 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    224134 |  5815 | `}` |
|         - |  5816 | `/*` |
|         - |  5817 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5818 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5819 | ` * The caller must release pOut when done.` |
|         - |  5820 | ` */` |
|    429404 |  5821 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5822 | `{` |
|    429409 |  5823 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3891 |  5824 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3891 |  5825 | `		SyBlobAppend(pOut,"\\",1);` |
|      1943 |  5826 | `	}` |
|    429409 |  5827 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    429409 |  5828 | `}` |
|         - |  5829 | `/*` |
|         - |  5830 | ` * Compile a namespace statement` |
|         - |  5831 | ` * According to the PHP language reference manual` |
|         - |  5832 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5833 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5834 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5835 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5836 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5837 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5838 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5839 | ` *  programming world.` |
|         - |  5840 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5841 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5842 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5843 | ` *  classes/functions/constants.` |
|         - |  5844 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5845 | ` *  readability of source code.` |
|         - |  5846 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5847 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5848 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5849 | ` *       class MyClass {}` |
|         - |  5850 | ` *       function myfunction() {}` |
|         - |  5851 | ` *       const MYCONST = 1;` |
|         - |  5852 | ` *       $a = new MyClass;` |
|         - |  5853 | ` *       $c = new \my\name\MyClass;` |
|         - |  5854 | ` *       $a = strlen('hi');` |
|         - |  5855 | ` *       $d = namespace\MYCONST;` |
|         - |  5856 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5857 | ` *       echo constant($d);` |
|         - |  5858 | ` * NOTE` |
|         - |  5859 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5860 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5861 | ` */` |
|         - |  5862 | `/*` |
|         - |  5863 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5864 | ` */` |
|        14 |  5865 | `static const char * TokenTypeName(sxu32 nType)` |
|         4 |  5866 | `{` |
|        18 |  5867 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        11 |  5868 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        11 |  5869 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        11 |  5870 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        11 |  5871 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        11 |  5872 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5873 | `	return "token";` |
|        11 |  5874 | `}` |
|      3932 |  5875 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5876 | `{` |
|         - |  5877 | `	sxu32 nLine;` |
|         - |  5878 | `	sxi32 rc;` |
|      3937 |  5879 | `	nLine = pGen->pIn->nLine;` |
|      3937 |  5880 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5881 | `	/* Reset namespace and clear previous use imports */` |
|      3937 |  5882 | `	SyBlobReset(&pGen->sNamespace);` |
|      3937 |  5883 | `	SyHashRelease(&pGen->hUseImports);` |
|      3937 |  5884 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3937 |  5885 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3937 |  5886 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3937 |  5887 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3937 |  5888 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3937 |  5889 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5890 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5891 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5892 | `		return SXRET_OK;` |
|         - |  5893 | `	}` |
|      3937 |  5894 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5895 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5896 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5897 | `		return SXRET_OK;` |
|         - |  5898 | `	}` |
|      3937 |  5899 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5900 | `		/* namespace { } — global namespace block */` |
|         3 |  5901 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         3 |  5902 | `		return SXRET_OK;` |
|         - |  5903 | `	}` |
|         - |  5904 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7915 |  5905 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3985 |  5906 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5907 | `			/* Append backslash separator */` |
|        30 |  5908 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        30 |  5909 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        13 |  5910 | `			}` |
|        17 |  5911 | `		}else{` |
|         - |  5912 | `			/* Append identifier */` |
|      3959 |  5913 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5914 | `		}` |
|      3985 |  5915 | `		pGen->pIn++;` |
|         5 |  5916 | `	}` |
|         - |  5917 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5918 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5919 | `	{` |
|      3935 |  5920 | `		char *zNsDup = 0;` |
|      3935 |  5921 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5897 |  5922 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3928 |  5923 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1964 |  5924 | `		}` |
|      3935 |  5925 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5926 | `	}` |
|      3935 |  5927 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5928 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5929 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5930 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5931 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5932 | `			return SXERR_ABORT;` |
|         - |  5933 | `		}` |
|         2 |  5934 | `	}` |
|      3935 |  5935 | `	return SXRET_OK;` |
|      1971 |  5936 | `}` |
|         - |  5937 | `/*` |
|         - |  5938 | ` * Compile the 'use' statement` |
|         - |  5939 | ` * According to the PHP language reference manual` |
|         - |  5940 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5941 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5942 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5943 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5944 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5945 | ` *  a function or constant is not supported.` |
|         - |  5946 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5947 | ` * NOTE` |
|         - |  5948 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5949 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5950 | ` */` |
|        74 |  5951 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5952 | `{` |
|         - |  5953 | `	sxu32 nLine;` |
|         - |  5954 | `	sxi32 rc;` |
|         - |  5955 | `	SyBlob sPath;` |
|         - |  5956 | `	SyString sAlias;` |
|         - |  5957 | `	SyToken *pLast;` |
|         - |  5958 | `	char *zDup;` |
|         - |  5959 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5960 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5961 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        79 |  5962 | `	nLine = pGen->pIn->nLine;` |
|        79 |  5963 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5964 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        79 |  5965 | `	iUseType = 0;` |
|        79 |  5966 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5967 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5968 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5969 | `			iUseType = 1;` |
|        16 |  5970 | `			pGen->pIn++;` |
|        23 |  5971 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5972 | `			iUseType = 2;` |
|        16 |  5973 | `			pGen->pIn++;` |
|         7 |  5974 | `		}` |
|        14 |  5975 | `	}` |
|         - |  5976 | `	/* Select target hash tables based on import type */` |
|        79 |  5977 | `	switch( iUseType ){` |
|         7 |  5978 | `		case 1:` |
|        16 |  5979 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5980 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5981 | `			break;` |
|         7 |  5982 | `		case 2:` |
|        16 |  5983 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5984 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5985 | `			break;` |
|        23 |  5986 | `		default:` |
|        51 |  5987 | `			pGenHash = &pGen->hUseImports;` |
|        51 |  5988 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        46 |  5989 | `			break;` |
|         - |  5990 | `	}` |
|        79 |  5991 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5992 | `	/* Process one or more use declarations separated by commas */` |
|        38 |  5993 | `	for(;;){` |
|        81 |  5994 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5995 | `			break;` |
|         - |  5996 | `		}` |
|        81 |  5997 | `		SyBlobReset(&sPath);` |
|        81 |  5998 | `		pLast = 0;` |
|         - |  5999 | `		/* Collect the full namespace path */` |
|       277 |  6000 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       201 |  6001 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       139 |  6002 | `				pLast = pGen->pIn;` |
|       139 |  6003 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        67 |  6004 | `					SyBlobAppend(&sPath,"\\",1);` |
|        31 |  6005 | `				}` |
|       139 |  6006 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        67 |  6007 | `			}` |
|       201 |  6008 | `			pGen->pIn++;` |
|         5 |  6009 | `		}` |
|        81 |  6010 | `		if( pLast == 0 ){` |
|         - |  6011 | `			/* Empty path */` |
|         6 |  6012 | `			break;` |
|         - |  6013 | `		}` |
|         - |  6014 | `		/* Default alias is the last component of the path */` |
|        77 |  6015 | `		sAlias = pLast->sData;` |
|         - |  6016 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        72 |  6017 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        52 |  6018 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        25 |  6019 | `			pGen->pIn++; /* Jump 'as' */` |
|        25 |  6020 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        25 |  6021 | `				sAlias = pGen->pIn->sData;` |
|        25 |  6022 | `				pGen->pIn++;` |
|        11 |  6023 | `			}` |
|        11 |  6024 | `		}` |
|         - |  6025 | `		/* Check for duplicate import alias (per-type) */` |
|        77 |  6026 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  6027 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6028 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  6029 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  6030 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6031 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  6032 | `				return SXERR_ABORT;` |
|         - |  6033 | `			}` |
|         2 |  6034 | `		}` |
|         - |  6035 | `		/* Register the import: alias -> FQN.` |
|         - |  6036 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  6037 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  6038 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       113 |  6039 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        72 |  6040 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        77 |  6041 | `		if( zDup ){` |
|        77 |  6042 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        77 |  6043 | `			if( pVmHash ){` |
|         - |  6044 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  6045 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        49 |  6046 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        49 |  6047 | `				if( zAliasDup ){` |
|        49 |  6048 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        22 |  6049 | `				}` |
|        22 |  6050 | `			}` |
|        77 |  6051 | `			if( iUseType == 2 ){` |
|         - |  6052 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  6053 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  6054 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  6055 | `				if( zAliasDup ){` |
|         - |  6056 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  6057 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  6058 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  6059 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  6060 | `					if( azPair ){` |
|        16 |  6061 | `						azPair[0] = zAliasDup;` |
|        16 |  6062 | `						azPair[1] = zDup;` |
|        16 |  6063 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  6064 | `					}` |
|         7 |  6065 | `				}` |
|         7 |  6066 | `			}` |
|        36 |  6067 | `		}` |
|         - |  6068 | `		/* Check for comma (multiple use declarations) */` |
|        77 |  6069 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  6070 | `			pGen->pIn++;` |
|         2 |  6071 | `		}else{` |
|        40 |  6072 | `			break;` |
|         - |  6073 | `		}` |
|         1 |  6074 | `	}` |
|        79 |  6075 | `	SyBlobRelease(&sPath);` |
|        79 |  6076 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  6077 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  6078 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  6079 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6080 | `			return SXERR_ABORT;` |
|         - |  6081 | `		}` |
|         1 |  6082 | `	}` |
|        79 |  6083 | `	return SXRET_OK;` |
|        42 |  6084 | `}` |
|         - |  6085 | `/*` |
|         - |  6086 | ` * Compile the stupid 'declare' language construct.` |
|         - |  6087 | ` *` |
|         - |  6088 | ` * According to the PHP language reference manual.` |
|         - |  6089 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  6090 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  6091 | ` *  declare (directive)` |
|         - |  6092 | ` *   statement` |
|         - |  6093 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  6094 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  6095 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  6096 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  6097 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  6098 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  6099 | ` * <?php` |
|         - |  6100 | ` * // these are the same:` |
|         - |  6101 | ` * // you can use this:` |
|         - |  6102 | ` * declare(ticks=1) {` |
|         - |  6103 | ` *   // entire script here` |
|         - |  6104 | ` * }` |
|         - |  6105 | ` * // or you can use this:` |
|         - |  6106 | ` * declare(ticks=1);` |
|         - |  6107 | ` * // entire script here` |
|         - |  6108 | ` * ?>` |
|         - |  6109 | ` *` |
|         - |  6110 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  6111 | ` */` |
|         - |  6112 | `/*` |
|         - |  6113 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  6114 | ` */` |
|        72 |  6115 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  6116 | `{` |
|       109 |  6117 | `	return SyStringLength(pName) == nWant` |
|        72 |  6118 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  6119 | `}` |
|         - |  6120 |  |
|        42 |  6121 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  6122 | `{` |
|        47 |  6123 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  6124 | `	SyToken *pBodyEnd = 0;` |
|         - |  6125 | `	SyToken *pBodyStart;` |
|         - |  6126 | `	SyToken *pCursor;` |
|         - |  6127 | `	int bHasStrictTypes;` |
|         - |  6128 | `	int bBlockForm;` |
|         - |  6129 | `	int bPlacementOk;` |
|         - |  6130 | `	sxi32 rc;` |
|        47 |  6131 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  6132 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  6133 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         6 |  6134 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6135 | `			return SXERR_ABORT;` |
|         - |  6136 | `		}` |
|         6 |  6137 | `		goto Synchro;` |
|         - |  6138 | `	}` |
|        43 |  6139 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  6140 | `	pBodyStart = pGen->pIn;` |
|         - |  6141 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  6142 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  6143 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  6144 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  6145 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6146 | `			return SXERR_ABORT;` |
|         - |  6147 | `		}` |
|       ! 0 |  6148 | `		return SXRET_OK;` |
|         - |  6149 | `	}` |
|         - |  6150 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  6151 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  6152 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  6153 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  6154 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  6155 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6156 | `			return SXERR_ABORT;` |
|         - |  6157 | `		}` |
|       ! 0 |  6158 | `	}` |
|        43 |  6159 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  6160 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  6161 | `	bHasStrictTypes = 0;` |
|         - |  6162 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  6163 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  6164 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  6165 | `	pCursor = pBodyStart;` |
|        55 |  6166 | `	while( pCursor < pBodyEnd ){` |
|        51 |  6167 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  6168 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  6169 | `				bHasStrictTypes = 1;` |
|        39 |  6170 | `				break;` |
|         - |  6171 | `			}` |
|         2 |  6172 | `		}` |
|        14 |  6173 | `		pCursor++;` |
|         2 |  6174 | `	}` |
|        43 |  6175 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  6176 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6177 | `			"strict_types declaration must not use block mode");` |
|         3 |  6178 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6179 | `		return SXRET_OK;` |
|         - |  6180 | `	}` |
|        41 |  6181 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  6182 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6183 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  6184 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  6185 | `		return SXRET_OK;` |
|         - |  6186 | `	}` |
|         - |  6187 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  6188 | `	pCursor = pBodyStart;` |
|        69 |  6189 | `	while( pCursor < pBodyEnd ){` |
|         - |  6190 | `		SyToken *pNameTok;` |
|         - |  6191 | `		SyToken *pEqTok;` |
|         - |  6192 | `		SyToken *pValTok;` |
|         - |  6193 | `		SyString *pDirName;` |
|         - |  6194 | `		int bIsStrict;` |
|         - |  6195 | `		int iStrictValue;` |
|        39 |  6196 | `		pNameTok = pCursor;` |
|        39 |  6197 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6198 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6199 | `				"declare: Expecting a directive name");` |
|       ! 0 |  6200 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6201 | `			return SXRET_OK;` |
|         - |  6202 | `		}` |
|        39 |  6203 | `		pEqTok = pNameTok + 1;` |
|        39 |  6204 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6205 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6206 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6207 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6208 | `			return SXRET_OK;` |
|         - |  6209 | `		}` |
|        39 |  6210 | `		pValTok = pEqTok + 1;` |
|        39 |  6211 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6212 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6213 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6214 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6215 | `			return SXRET_OK;` |
|         - |  6216 | `		}` |
|        39 |  6217 | `		pDirName = &pNameTok->sData;` |
|        39 |  6218 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6219 | `		if( bIsStrict ){` |
|         - |  6220 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6221 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6222 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6223 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6224 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6225 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6226 | `				return SXRET_OK;` |
|         - |  6227 | `			}` |
|        35 |  6228 | `			iStrictValue = -1;` |
|        35 |  6229 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6230 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6231 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6232 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6233 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6234 | `			}` |
|        35 |  6235 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6236 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6237 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6238 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6239 | `				return SXRET_OK;` |
|         - |  6240 | `			}` |
|        32 |  6241 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6242 | `		}else{` |
|         - |  6243 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6244 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6245 | `			 * behavior don't regress. */` |
|         8 |  6246 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6247 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6248 | `				ph7_lib_version()` |
|         - |  6249 | `				);` |
|         - |  6250 | `		}` |
|        37 |  6251 | `		pCursor = pValTok + 1;` |
|         - |  6252 | `		/* Consume separating comma (or end). */` |
|        37 |  6253 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6254 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6255 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6256 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6257 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6258 | `				return SXRET_OK;` |
|         - |  6259 | `			}` |
|         3 |  6260 | `			pCursor++;` |
|         1 |  6261 | `		}` |
|         5 |  6262 | `	}` |
|         - |  6263 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6264 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6265 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        35 |  6266 | `	return SXRET_OK;` |
|         2 |  6267 | `Synchro:` |
|         - |  6268 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6269 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6270 | `		pGen->pIn++;` |
|         2 |  6271 | `	}` |
|         6 |  6272 | `	return SXRET_OK;` |
|        26 |  6273 | `}` |
|         - |  6274 | `/*` |
|         - |  6275 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6276 | ` * as follows:` |
|         - |  6277 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6278 | ` * {` |
|         - |  6279 | ` *   return "Making a cup of $type.\n";` |
|         - |  6280 | ` * }` |
|         - |  6281 | ` * Symisc eXtension.` |
|         - |  6282 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6283 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6284 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6285 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6286 | ` *      {` |
|         - |  6287 | ` *       var_dump($a);` |
|         - |  6288 | ` *      }` |
|         - |  6289 | ` *     //call test without args` |
|         - |  6290 | ` *      test();` |
|         - |  6291 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6292 | ` *      Example:` |
|         - |  6293 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6294 | ` * 3 -) Function overloading!!` |
|         - |  6295 | ` *      Example:` |
|         - |  6296 | ` *      function foo($a) {` |
|         - |  6297 | ` *   	  return $a.PHP_EOL;` |
|         - |  6298 | ` *	    }` |
|         - |  6299 | ` *	    function foo($a, $b) {` |
|         - |  6300 | ` *   	  return $a + $b;` |
|         - |  6301 | ` *	    }` |
|         - |  6302 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6303 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6304 | ` *      // Same arg` |
|         - |  6305 | ` *	   function foo(string $a)` |
|         - |  6306 | ` *	   {` |
|         - |  6307 | ` *	     echo "a is a string\n";` |
|         - |  6308 | ` *	     var_dump($a);` |
|         - |  6309 | ` *	   }` |
|         - |  6310 | ` *	  function foo(int $a)` |
|         - |  6311 | ` *	  {` |
|         - |  6312 | ` *	    echo "a is integer\n";` |
|         - |  6313 | ` *	    var_dump($a);` |
|         - |  6314 | ` *	  }` |
|         - |  6315 | ` *	  function foo(array $a)` |
|         - |  6316 | ` *	  {` |
|         - |  6317 | ` * 	    echo "a is an array\n";` |
|         - |  6318 | ` * 	    var_dump($a);` |
|         - |  6319 | ` *	  }` |
|         - |  6320 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6321 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6322 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6323 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6324 | ` * introduced by the PH7 engine.` |
|         - |  6325 | ` */` |
|    564926 |  6326 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6327 | `{` |
|         - |  6328 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6329 | `	SySet *pInstrContainer;` |
|         - |  6330 | `	sxi32 rc;` |
|         - |  6331 | `	/* Swap token stream */` |
|    564931 |  6332 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    564931 |  6333 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    564931 |  6334 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6335 | `	/* Compile the expression holding the argument value */` |
|    564931 |  6336 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6337 | `	/* Emit the done instruction */` |
|    564931 |  6338 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    564931 |  6339 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    564931 |  6340 | `	RE_SWAP_DELIMITER(pGen);` |
|    564931 |  6341 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6342 | `		return SXERR_ABORT;` |
|         - |  6343 | `	}` |
|    564931 |  6344 | `	return SXRET_OK;` |
|    282468 |  6345 | `}` |
|         - |  6346 | `/*` |
|         - |  6347 | ` * Collect function arguments one after one.` |
|         - |  6348 | ` * According to the PHP language reference manual.` |
|         - |  6349 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6350 | ` * list of expressions.` |
|         - |  6351 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6352 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6353 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6354 | ` * for more information.` |
|         - |  6355 | ` * Example #1 Passing arrays to functions` |
|         - |  6356 | ` * <?php` |
|         - |  6357 | ` * function takes_array($input)` |
|         - |  6358 | ` * {` |
|         - |  6359 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6360 | ` * }` |
|         - |  6361 | ` * ?>` |
|         - |  6362 | ` * Making arguments be passed by reference` |
|         - |  6363 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6364 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6365 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6366 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6367 | ` * to the argument name in the function definition:` |
|         - |  6368 | ` * Example #2 Passing function parameters by reference` |
|         - |  6369 | ` * <?php` |
|         - |  6370 | ` * function add_some_extra(&$string)` |
|         - |  6371 | ` * {` |
|         - |  6372 | ` *   $string .= 'and something extra.';` |
|         - |  6373 | ` * }` |
|         - |  6374 | ` * $str = 'This is a string, ';` |
|         - |  6375 | ` * add_some_extra($str);` |
|         - |  6376 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6377 | ` * ?>` |
|         - |  6378 | ` *` |
|         - |  6379 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6380 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6381 | ` * on these extension.` |
|         - |  6382 | ` */` |
|   1287960 |  6383 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6384 | `{` |
|         - |  6385 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6386 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6387 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6388 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6389 | `	sxi32 rc;` |
|         - |  6390 |  |
|   1287965 |  6391 | `	pIn = pGen->pIn;` |
|   1287965 |  6392 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6393 | `	/* Process arguments one after one */` |
|   1670050 |  6394 | `	for(;;){` |
|   3340105 |  6395 | `		if( pIn >= pEnd ){` |
|         - |  6396 | `			/* No more arguments to process */` |
|   1287949 |  6397 | `			break;` |
|         - |  6398 | `		}` |
|   2052161 |  6399 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2052161 |  6400 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2052161 |  6401 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2052161 |  6402 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2052161 |  6403 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6404 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6405 | `		 * first token inside the main token stream */` |
|   2052161 |  6406 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6407 | `			return SXERR_ABORT;` |
|         - |  6408 | `		}` |
|         - |  6409 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6410 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6411 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6412 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6413 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6414 | `		{` |
|   2052161 |  6415 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2052161 |  6416 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2052161 |  6417 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6418 | `			int nSetTok;` |
|         - |  6419 | `			sxi32 nSetVis;` |
|   2052161 |  6420 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6421 | `				bReadonly = 1;` |
|         3 |  6422 | `				pIn++;` |
|         1 |  6423 | `			}` |
|   2052161 |  6424 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2052161 |  6425 | `			if( nSetVis ){` |
|         - |  6426 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6427 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6428 | `				bVisSeen = 1;` |
|         3 |  6429 | `				pIn += nSetTok;` |
|         3 |  6430 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6431 | `					bReadonly = 1;` |
|       ! 0 |  6432 | `					pIn++;` |
|         1 |  6433 | `				}` |
|   2052160 |  6434 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     88167 |  6435 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     88167 |  6436 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6437 | `					bVisSeen = 1;` |
|        89 |  6438 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6439 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6440 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6441 | `					pIn++;` |
|        89 |  6442 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6443 | `					if( nSetVis ){` |
|         - |  6444 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6445 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6446 | `						pIn += nSetTok;` |
|         1 |  6447 | `					}` |
|        89 |  6448 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6449 | `						bReadonly = 1;` |
|        18 |  6450 | `						pIn++;` |
|         7 |  6451 | `					}` |
|        42 |  6452 | `				}` |
|     44081 |  6453 | `			}` |
|   2052161 |  6454 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6455 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2052159 |  6456 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6457 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6458 | `			}` |
|   2052161 |  6459 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6460 | `				if( !bCtorCtx ){` |
|         6 |  6461 | `					if( bAbstractCtx ){` |
|         3 |  6462 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6463 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6464 | `					}else{` |
|         3 |  6465 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6466 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6467 | `					}` |
|         6 |  6468 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6469 | `						return SXERR_ABORT;` |
|         - |  6470 | `					}` |
|         6 |  6471 | `					return SXERR_SYNTAX;` |
|         - |  6472 | `				}` |
|        89 |  6473 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6474 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6475 | `				if( bReadonly ){` |
|        20 |  6476 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6477 | `				}` |
|        42 |  6478 | `			}` |
|         - |  6479 | `		}` |
|         - |  6480 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   2052152 |  6481 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1106530 |  6482 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    149448 |  6483 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    115005 |  6484 | `			sxu32 nLineLocal = pIn->nLine;` |
|    115005 |  6485 | `			sxi32 iTFlags = 0;` |
|    115005 |  6486 | `			pGen->pIn = pIn;` |
|    115005 |  6487 | `			rc = GenStateParseUnionTypeDecl(` |
|     57500 |  6488 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57500 |  6489 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6490 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6491 | `				/* bAllowVoid */ 0,` |
|     57500 |  6492 | `						nLineLocal);` |
|    115005 |  6493 | `			pIn = pGen->pIn;` |
|    115005 |  6494 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6495 | `				return SXERR_ABORT;` |
|    115005 |  6496 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6497 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6498 | `				return SXERR_SYNTAX;` |
|    115003 |  6499 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6500 | `				if( pIn < pEnd ){` |
|        15 |  6501 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6502 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6503 | `						&pIn->sData);` |
|         7 |  6504 | `				}else{` |
|       ! 0 |  6505 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6506 | `						"syntax error, unexpected end of file");` |
|         - |  6507 | `				}` |
|        11 |  6508 | `				return SXERR_SYNTAX;` |
|         - |  6509 | `			}` |
|    114995 |  6510 | `			sArg.iFlags \|= iTFlags;` |
|     57495 |  6511 | `		}` |
|   2052147 |  6512 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6513 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6514 | `			return rc;` |
|         - |  6515 | `		}` |
|   2052147 |  6516 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6517 | `			/* Pass by reference,record that */` |
|     22943 |  6518 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     22943 |  6519 | `			pIn++;` |
|     11469 |  6520 | `		}` |
|   2052147 |  6521 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6522 | `			/* Variadic parameter: ...$args */` |
|     23005 |  6523 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     23005 |  6524 | `			pIn++;` |
|     11500 |  6525 | `		}` |
|   2052147 |  6526 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6527 | `			/* Invalid argument */` |
|       ! 0 |  6528 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6529 | `			return rc;` |
|         - |  6530 | `		}` |
|   2052147 |  6531 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6532 | `		/* Copy argument name */` |
|   2052147 |  6533 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2052147 |  6534 | `		if( zDup == 0 ){` |
|       ! 0 |  6535 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6536 | `			return SXERR_ABORT;` |
|         - |  6537 | `		}` |
|   2052147 |  6538 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2052147 |  6539 | `		pIn++;` |
|   2052147 |  6540 | `		if( pIn < pEnd ){` |
|   1142101 |  6541 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6542 | `				SyToken *pDefend;` |
|    564933 |  6543 | `				sxi32 iNest = 0;` |
|    564933 |  6544 | `				pIn++; /* Jump the equal sign */` |
|    564933 |  6545 | `				pDefend = pIn;` |
|         - |  6546 | `				/* Process the default value associated with this argument */` |
|   1187127 |  6547 | `				while( pDefend < pEnd ){` |
|    809233 |  6548 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    187039 |  6549 | `						break;` |
|         - |  6550 | `					}` |
|    622199 |  6551 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6552 | `						/* Increment nesting level */` |
|     26725 |  6553 | `						iNest++;` |
|    608839 |  6554 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6555 | `						/* Decrement nesting level */` |
|     26725 |  6556 | `						iNest--;` |
|     13360 |  6557 | `					}` |
|    622199 |  6558 | `					pDefend++;` |
|         5 |  6559 | `				}` |
|    564933 |  6560 | `				if( pIn >= pDefend ){` |
|         3 |  6561 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6562 | `					return rc;` |
|         - |  6563 | `				}` |
|         - |  6564 | `				/* Process default value */` |
|    564931 |  6565 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    564931 |  6566 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6567 | `					return rc;` |
|         - |  6568 | `				}` |
|         - |  6569 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6570 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6571 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6572 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6573 | `				 * arg-type check lets null through. */` |
|    564926 |  6574 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    307290 |  6575 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    307287 |  6576 | `					&& &pIn[1] == pDefend` |
|     45831 |  6577 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     34366 |  6578 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     20999 |  6579 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15275 |  6580 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6581 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6582 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6583 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6584 | `					 * already up at this point). */` |
|         - |  6585 | `					{` |
|     15275 |  6586 | `						const char *zSep = "";` |
|     15275 |  6587 | `						SyString sCls = { "", 0 };` |
|     15275 |  6588 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15269 |  6589 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15269 |  6590 | `							zSep = "::";` |
|      7632 |  6591 | `						}` |
|     22910 |  6592 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6593 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7635 |  6594 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6595 | `					}` |
|      7635 |  6596 | `				}` |
|         - |  6597 | `				/* Point beyond the default value */` |
|    564931 |  6598 | `				pIn = pDefend;` |
|    282463 |  6599 | `			}` |
|   1142099 |  6600 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6601 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6602 | `				return rc;` |
|         - |  6603 | `			}` |
|   1142099 |  6604 | `			pIn++; /* Jump the trailing comma */` |
|    571047 |  6605 | `		}` |
|         - |  6606 | `		/* Append argument signature */` |
|   2052145 |  6607 | `		if( sArg.nType > 0 ){` |
|    114933 |  6608 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6609 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26805 |  6610 | `				int marker = 'o';` |
|     26805 |  6611 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26805 |  6612 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13405 |  6613 | `			}else{` |
|         - |  6614 | `				int c;` |
|     88133 |  6615 | `				c = 'n'; /* cc warning */` |
|         - |  6616 | `				/* Type leading character */` |
|     88133 |  6617 | `				switch(sArg.nType){` |
|      5730 |  6618 | `				case MEMOBJ_HASHMAP:` |
|         - |  6619 | `					/* Hashmap aka 'array' */` |
|     11465 |  6620 | `					c = 'h';` |
|     11465 |  6621 | `					break;` |
|      9660 |  6622 | `				case MEMOBJ_INT:` |
|         - |  6623 | `					/* Integer */` |
|     19325 |  6624 | `					c = 'i';` |
|     19325 |  6625 | `					break;` |
|         2 |  6626 | `				case MEMOBJ_BOOL:` |
|         - |  6627 | `					/* Bool */` |
|         5 |  6628 | `					c = 'b';` |
|         5 |  6629 | `					break;` |
|         5 |  6630 | `				case MEMOBJ_REAL:` |
|         - |  6631 | `					/* Float */` |
|        12 |  6632 | `					c = 'f';` |
|        12 |  6633 | `					break;` |
|     28659 |  6634 | `				case MEMOBJ_STRING:` |
|         - |  6635 | `					/* String */` |
|     57323 |  6636 | `					c = 's';` |
|     57323 |  6637 | `					break;` |
|         7 |  6638 | `				case MEMOBJ_OBJ:` |
|         - |  6639 | `					/* Object */` |
|        16 |  6640 | `					c = 'o';` |
|        14 |  6641 | `					break;` |
|         1 |  6642 | `				default:` |
|         2 |  6643 | `					break;` |
|         - |  6644 | `				}` |
|     88133 |  6645 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6646 | `			}` |
|     57469 |  6647 | `		}else{` |
|         - |  6648 | `			/* No type is associated with this parameter which mean` |
|         - |  6649 | `			 * that this function is not condidate for overloading.` |
|         - |  6650 | `			 */` |
|   1937217 |  6651 | `			SyBlobRelease(&sSig);` |
|         - |  6652 | `		}` |
|         - |  6653 | `		/* Save in the argument set */` |
|   2052145 |  6654 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6655 | `	}` |
|   1287949 |  6656 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6657 | `		/* Save function signature */` |
|     84333 |  6658 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     42164 |  6659 | `	}` |
|   1287949 |  6660 | `	return SXRET_OK;` |
|    643985 |  6661 | `}` |
|         - |  6662 | `/*` |
|         - |  6663 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6664 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6665 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6666 | ` */` |
|     34388 |  6667 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6668 | `{` |
|     34393 |  6669 | `	sxi32 iParen = 0;` |
|     34393 |  6670 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6671 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6672 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6673 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    152889 |  6674 | `	while( pIn < pEnd ){` |
|    152889 |  6675 | `		sxu32 t = pIn->nType;` |
|    152889 |  6676 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    149017 |  6677 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    103169 |  6678 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     84045 |  6679 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    118501 |  6680 | `		pIn++;` |
|         5 |  6681 | `	}` |
|     19129 |  6682 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6683 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6684 | `	{` |
|     19129 |  6685 | `		sxi32 d = 0;` |
|    759885 |  6686 | `		while( pIn < pEnd ){` |
|    759885 |  6687 | `			sxu32 t = pIn->nType;` |
|    759885 |  6688 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    729305 |  6689 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    740761 |  6690 | `			pIn++;` |
|         5 |  6691 | `		}` |
|         - |  6692 | `	}` |
|     19129 |  6693 | `	return pIn;` |
|     17199 |  6694 | `}` |
|         - |  6695 | `/*` |
|         - |  6696 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6697 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6698 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6699 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6700 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6701 | ` * detached-mini-program path untouched.` |
|         - |  6702 | ` */` |
|         - |  6703 | `/*` |
|         - |  6704 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6705 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6706 | ` * mixed, object.` |
|         - |  6707 | ` */` |
|     11472 |  6708 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6709 | `{` |
|         - |  6710 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6711 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6712 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6713 | `	};` |
|         - |  6714 | `	sxu32 i;` |
|     11477 |  6715 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6716 | `		zName++;` |
|       ! 0 |  6717 | `		nName--;` |
|       ! 0 |  6718 | `	}` |
|     11485 |  6719 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11485 |  6720 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11477 |  6721 | `			return 1;` |
|         - |  6722 | `		}` |
|         5 |  6723 | `	}` |
|       ! 0 |  6724 | `	return 0;` |
|      5741 |  6725 | `}` |
|         - |  6726 | `/*` |
|         - |  6727 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6728 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6729 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6730 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6731 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6732 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6733 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6734 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6735 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6736 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6737 | ` */` |
|     11474 |  6738 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6739 | `{` |
|     11479 |  6740 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6741 | ``		return 1; /* bare `object` */`` |
|         - |  6742 | `	}` |
|     11479 |  6743 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6744 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6745 | `	}` |
|     11477 |  6746 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11477 |  6747 | `		return 1;` |
|         - |  6748 | `	}` |
|         - |  6749 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6750 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6751 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6752 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6753 | `	{` |
|         - |  6754 | `		SyBlob sFQN;` |
|         - |  6755 | `		int bOk;` |
|       ! 0 |  6756 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       ! 0 |  6757 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       ! 0 |  6758 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       ! 0 |  6759 | `		SyBlobRelease(&sFQN);` |
|       ! 0 |  6760 | `		return bOk;` |
|         - |  6761 | `	}` |
|      5742 |  6762 | `}` |
|         - |  6763 | `/*` |
|         - |  6764 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6765 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6766 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6767 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6768 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6769 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6770 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6771 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6772 | ` */` |
|     11714 |  6773 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6774 | `{` |
|     11719 |  6775 | `	int bOk = 0;` |
|         - |  6776 | `	sxu32 nLine;` |
|         - |  6777 | `	sxi32 rc;` |
|     11719 |  6778 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       245 |  6779 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6780 | `	}` |
|     11479 |  6781 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6782 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6783 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6784 | `		sxu32 i,j;` |
|       ! 0 |  6785 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6786 | `			int bGroupOk;` |
|       ! 0 |  6787 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6788 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6789 | `			}` |
|       ! 0 |  6790 | `			bGroupOk = 1;` |
|       ! 0 |  6791 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6792 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6793 | `					bGroupOk = 0;` |
|       ! 0 |  6794 | `					break;` |
|         - |  6795 | `				}` |
|       ! 0 |  6796 | `			}` |
|       ! 0 |  6797 | `			bOk = bGroupOk;` |
|       ! 0 |  6798 | `		}` |
|       ! 0 |  6799 | `	}else{` |
|     11479 |  6800 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6801 | `	}` |
|     11479 |  6802 | `	if( bOk ){` |
|     11477 |  6803 | `		return SXRET_OK;` |
|         - |  6804 | `	}` |
|         - |  6805 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6806 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6807 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6808 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6809 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6810 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6811 | `	{` |
|         3 |  6812 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6813 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6814 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6815 | `		}` |
|         3 |  6816 | `		if( sGiven.nByte < 1 ){` |
|         - |  6817 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6818 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6819 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6820 | `			const char *zScalar =` |
|       ! 0 |  6821 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6822 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6823 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6824 | `		}` |
|         3 |  6825 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6826 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6827 | `	}` |
|         3 |  6828 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5862 |  6829 | `}` |
|   2743624 |  6830 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6831 | `{` |
|   2743629 |  6832 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2743629 |  6833 | `	SyToken *pEnd = pGen->pEnd;` |
|   2743629 |  6834 | `	sxi32 iDepth = 0;` |
|   2743629 |  6835 | `	int bStarted = 0;` |
| 132584613 |  6836 | `	while( pIn < pEnd ){` |
| 132584613 |  6837 | `		sxu32 t = pIn->nType;` |
| 132584613 |  6838 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 126645995 |  6839 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 120742103 |  6840 | `		if( t & PH7_TK_KEYWORD ){` |
|   8984657 |  6841 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   8984657 |  6842 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   8972943 |  6843 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6844 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   4469275 |  6845 | `		}` |
| 120696001 |  6846 | `		pIn++;` |
|         5 |  6847 | `	}` |
|   2731915 |  6848 | `	return FALSE;` |
|   1371817 |  6849 | `}` |
|         - |  6850 | `/*` |
|         - |  6851 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6852 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6853 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6854 | ` */` |
|   2743624 |  6855 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6856 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6857 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6858 | `	)` |
|         5 |  6859 | `{` |
|         - |  6860 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6861 | `	GenBlock *pBlock;` |
|         - |  6862 | `	sxu32 nGotoOfft;` |
|         - |  6863 | `	sxi32 rc;` |
|         - |  6864 | `	/* Attach the new function */` |
|   2743629 |  6865 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2743629 |  6866 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6867 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6868 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6869 | `		return SXERR_ABORT;` |
|         - |  6870 | `	}` |
|   2743629 |  6871 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6872 | `	/* Swap bytecode containers */` |
|   2743629 |  6873 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2743629 |  6874 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6875 | `	/* Emit constructor property promotion prologue:` |
|         - |  6876 | `	 *   $this->NAME = $NAME;` |
|         - |  6877 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6878 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6879 | `	{` |
|   2743629 |  6880 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6881 | `		sxu32 i;` |
|   4742207 |  6882 | `		for( i = 0; i < nArg; i++ ){` |
|   1998583 |  6883 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6884 | `			char *zSrc;` |
|         - |  6885 | `			sxu32 nSrc,nName;` |
|         - |  6886 | `			SySet sToken;` |
|         - |  6887 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6888 | `			sxi32 rcPromote;` |
|   1998583 |  6889 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1998509 |  6890 | `				continue;` |
|         - |  6891 | `			}` |
|         - |  6892 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6893 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6894 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6895 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6896 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6897 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6898 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6899 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6900 | `			if( zSrc == 0 ){` |
|       ! 0 |  6901 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6902 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6903 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6904 | `				return SXERR_ABORT;` |
|         - |  6905 | `			}` |
|         - |  6906 | `			{` |
|        79 |  6907 | `				char *z = zSrc;` |
|        79 |  6908 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6909 | `				z += sizeof("$this->")-1;` |
|        79 |  6910 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6911 | `				z += nName;` |
|        79 |  6912 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6913 | `				z += sizeof(" = $")-1;` |
|        79 |  6914 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6915 | `				z += nName;` |
|        79 |  6916 | `				*z = 0;` |
|         - |  6917 | `			}` |
|        79 |  6918 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6919 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6920 | `			pTmpIn = pGen->pIn;` |
|        79 |  6921 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6922 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6923 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6924 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6925 | `			pGen->pIn = pTmpIn;` |
|        79 |  6926 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6927 | `			SySetRelease(&sToken);` |
|        79 |  6928 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6929 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6930 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6931 | `				return SXERR_ABORT;` |
|         - |  6932 | `			}` |
|         - |  6933 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6934 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6935 | `		}` |
|         - |  6936 | `	}` |
|         - |  6937 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6938 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6939 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6940 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6941 | `	{` |
|   2743629 |  6942 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2743629 |  6943 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6944 | `		/* Compile the body */` |
|   2743629 |  6945 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2743629 |  6946 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6947 | `	}` |
|         - |  6948 | `	/* Fix exception jumps now the destination is resolved */` |
|   2743629 |  6949 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6950 | `	/* Emit the final return if not yet done */` |
|   2743629 |  6951 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6952 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2743629 |  6953 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6954 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6955 | `	}` |
|   2743629 |  6956 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6957 | `	/* Restore the default container */` |
|   2743629 |  6958 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6959 | `	/* Leave function block */` |
|   2743629 |  6960 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2743629 |  6961 | `	if( rc == SXERR_ABORT ){` |
|         - |  6962 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6963 | `		return SXERR_ABORT;` |
|         - |  6964 | `	}` |
|         - |  6965 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6966 | `	{` |
|   2743629 |  6967 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6968 | `		sxu32 i;` |
|  80949375 |  6969 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  78217465 |  6970 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11719 |  6971 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11719 |  6972 | `				break;` |
|         - |  6973 | `			}` |
|  39102878 |  6974 | `		}` |
|         - |  6975 | `	}` |
|   2743629 |  6976 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6977 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11719 |  6978 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6979 | `			return SXERR_ABORT;` |
|         - |  6980 | `		}` |
|      5857 |  6981 | `	}` |
|         - |  6982 | `	/* All done, function body compiled */` |
|   2743629 |  6983 | `	return SXRET_OK;` |
|   1371817 |  6984 | `}` |
|         - |  6985 | `/*` |
|         - |  6986 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  6987 | ` * According to the PHP language reference manual.` |
|         - |  6988 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  6989 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  6990 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  6991 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  6992 | ` *  Functions need not be defined before they are referenced.` |
|         - |  6993 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  6994 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  6995 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  6996 | ` *  calls with over 32-64 recursion levels.` |
|         - |  6997 | ` *` |
|         - |  6998 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  6999 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  7000 | ` * on these extension.` |
|         - |  7001 | ` */` |
|         - |  7002 | `/*` |
|         - |  7003 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  7004 | ` */` |
|       570 |  7005 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  7006 | `{` |
|         - |  7007 | `	sxu32 i;` |
|      1611 |  7008 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  7009 | `		int a = zA[i], b = zB[i];` |
|      1381 |  7010 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  7011 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  7012 | `		if( a != b ) return a - b;` |
|       523 |  7013 | `	}` |
|       235 |  7014 | `	return 0;` |
|       290 |  7015 | `}` |
|         - |  7016 | `/*` |
|         - |  7017 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  7018 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  7019 | ` * (which are positive bit values stored in sxu32).` |
|         - |  7020 | ` */` |
|         - |  7021 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  7022 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  7023 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  7024 |  |
|         - |  7025 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  7026 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  7027 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  7028 |  |
|         - |  7029 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  7030 | `struct PhlTypeAtom {` |
|         - |  7031 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  7032 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  7033 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  7034 | `	sxu32 nCanon;` |
|         - |  7035 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  7036 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  7037 | `};` |
|         - |  7038 |  |
|         - |  7039 | `/*` |
|         - |  7040 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  7041 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  7042 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  7043 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  7044 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  7045 | ` * already be consumed by the caller.` |
|         - |  7046 | ` */` |
|         - |  7047 | `/*` |
|         - |  7048 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|         - |  7049 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|         - |  7050 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|         - |  7051 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|         - |  7052 | ` * null/void/never are matched before the class path), but the full set is listed` |
|         - |  7053 | ` * so the guard is robust to lexer changes.` |
|         - |  7054 | ` */` |
|     38478 |  7055 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|         5 |  7056 | `{` |
|         - |  7057 | `	static const char *azWords[] = {` |
|         - |  7058 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|         - |  7059 | `		"bool","boolean","int","integer","float","double","string","array",` |
|         - |  7060 | `		"object","self","static","parent"` |
|         - |  7061 | `	};` |
|         - |  7062 | `	sxu32 i;` |
|    806245 |  7063 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    767869 |  7064 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    767869 |  7065 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|       107 |  7066 | `			return 1;` |
|         - |  7067 | `		}` |
|    383886 |  7068 | `	}` |
|     38381 |  7069 | `	return 0;` |
|     19244 |  7070 | `}` |
|    127702 |  7071 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  7072 | `{` |
|    127707 |  7073 | `	SyToken *pIn = pGen->pIn;` |
|    127707 |  7074 | `	int bAbsolute = 0;` |
|    127707 |  7075 | `	SyZero(pOut, sizeof(*pOut));` |
|    127707 |  7076 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    127707 |  7077 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7078 | `		return SXERR_SYNTAX;` |
|         - |  7079 | `	}` |
|         - |  7080 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    127707 |  7081 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        10 |  7082 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|        10 |  7083 | `		pIn++;` |
|        10 |  7084 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7085 | `			return SXERR_SYNTAX;` |
|         - |  7086 | `		}` |
|         4 |  7087 | `	}` |
|    127707 |  7088 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7089 | `		return SXERR_SYNTAX;` |
|         - |  7090 | `	}` |
|    127707 |  7091 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     88997 |  7092 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     88997 |  7093 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11523 |  7094 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     83238 |  7095 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        83 |  7096 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     77440 |  7097 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19733 |  7098 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67537 |  7099 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57581 |  7100 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     28885 |  7101 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  7102 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        79 |  7103 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        28 |  7104 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        48 |  7105 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        16 |  7106 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        33 |  7107 | `			pOut->nType = SXU32_HIGH;` |
|        33 |  7108 | `			pOut->sClass = pIn->sData;` |
|        18 |  7109 | `		}else{` |
|         3 |  7110 | `			return SXERR_SYNTAX;` |
|         - |  7111 | `		}` |
|     88995 |  7112 | `		pIn++;` |
|     44500 |  7113 | `	}else{` |
|         - |  7114 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  7115 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38715 |  7116 | `		SyString *pT = &pIn->sData;` |
|     38715 |  7117 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  7118 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  7119 | `			pIn++;` |
|     38700 |  7120 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  7121 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  7122 | `			pIn++;` |
|     38599 |  7123 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        27 |  7124 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        27 |  7125 | `			pIn++;` |
|        16 |  7126 | `		}else{` |
|         - |  7127 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38491 |  7128 | `			SyToken *pFirst = pIn;` |
|     38491 |  7129 | `			SyToken *pLast = pIn;` |
|     38491 |  7130 | `			pOut->nType = SXU32_HIGH;` |
|     38491 |  7131 | `			pOut->sClass = pIn->sData;` |
|     38491 |  7132 | `			pIn++;` |
|     57732 |  7133 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38494 |  7134 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  7135 | `				pLast = &pIn[1];` |
|         3 |  7136 | `				pIn += 2;` |
|         1 |  7137 | `			}` |
|     38491 |  7138 | `			if( pLast != pFirst ){` |
|         3 |  7139 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  7140 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  7141 | `				pOut->sClass.zString = zFirst;` |
|         3 |  7142 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  7143 | `			}` |
|         - |  7144 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|         - |  7145 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|         - |  7146 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|         - |  7147 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|         - |  7148 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|         - |  7149 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|         - |  7150 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|         - |  7151 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|     38491 |  7152 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|         - |  7153 | `				SyBlob sFqn;` |
|     38381 |  7154 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     38381 |  7155 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|     38376 |  7156 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|     38376 |  7157 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|        17 |  7158 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 |  7159 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|        12 |  7160 | `					if( zDup ){` |
|        12 |  7161 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|         5 |  7162 | `					}` |
|         5 |  7163 | `				}` |
|     38381 |  7164 | `				SyBlobRelease(&sFqn);` |
|     19188 |  7165 | `			}` |
|         - |  7166 | `		}` |
|         - |  7167 | `	}` |
|    127705 |  7168 | `	pGen->pIn = pIn;` |
|    127705 |  7169 | `	return SXRET_OK;` |
|     63856 |  7170 | `}` |
|         - |  7171 |  |
|         - |  7172 | `/*` |
|         - |  7173 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  7174 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  7175 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  7176 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  7177 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  7178 | ` */` |
|    127524 |  7179 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  7180 | `{` |
|         - |  7181 | `	int i;` |
|    127529 |  7182 | `	int nNonNull = 0;` |
|    127529 |  7183 | `	int bAnyIntersection = 0;` |
|         - |  7184 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    127529 |  7185 | `	sxu32 nMaxGroup = 0;` |
|   4208297 |  7186 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    255205 |  7187 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127681 |  7188 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127651 |  7189 | `			nNonNull++;` |
|    127651 |  7190 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    127651 |  7191 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    127651 |  7192 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     63823 |  7193 | `			}` |
|     63823 |  7194 | `		}` |
|     63843 |  7195 | `	}` |
|    255153 |  7196 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127653 |  7197 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  7198 | `			bAnyIntersection = 1;` |
|        29 |  7199 | `			break;` |
|         - |  7200 | `		}` |
|     63817 |  7201 | `	}` |
|    127529 |  7202 | `	if( bAnyIntersection ){` |
|         - |  7203 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  7204 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  7205 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  7206 | `		sxu32 g, nGroups = 0;` |
|        29 |  7207 | `		int bFirstGroup = 1;` |
|        59 |  7208 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  7209 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  7210 | `			int bFirstMember = 1;` |
|         - |  7211 | `			int bWrap;` |
|        35 |  7212 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  7213 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  7214 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  7215 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  7216 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  7217 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  7218 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  7219 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  7220 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  7221 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  7222 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  7223 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  7224 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  7225 | `				}else{` |
|         6 |  7226 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7227 | `				}` |
|        59 |  7228 | `				bFirstMember = 0;` |
|        32 |  7229 | `			}` |
|        35 |  7230 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7231 | `			bFirstGroup = 0;` |
|        20 |  7232 | `		}` |
|        29 |  7233 | `		if( bNullable ){` |
|       ! 0 |  7234 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7235 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7236 | `		}` |
|        85 |  7237 | `		return;` |
|         - |  7238 | `	}` |
|    127505 |  7239 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7240 | `		/* Shorthand: ?T */` |
|       117 |  7241 | `		for( i = 0; i < nAtoms; i++ ){` |
|       117 |  7242 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       117 |  7243 | `			SyBlobAppend(pBlob, "?", 1);` |
|       117 |  7244 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        24 |  7245 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  7246 | `			}else{` |
|        95 |  7247 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7248 | `			}` |
|       117 |  7249 | `			return;` |
|       ! 0 |  7250 | `		}` |
|       ! 0 |  7251 | `	}` |
|         - |  7252 | `	{` |
|    127393 |  7253 | `		int bFirst = 1;` |
|         - |  7254 | `		/* 1) Classes in declaration order */` |
|    254889 |  7255 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127501 |  7256 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38449 |  7257 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38449 |  7258 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38449 |  7259 | `				bFirst = 0;` |
|     19222 |  7260 | `			}` |
|     63753 |  7261 | `		}` |
|         - |  7262 | `		/* 2) Built-ins in canonical order */` |
|         - |  7263 | `		{` |
|         - |  7264 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7265 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7266 | `			int k;` |
|    891721 |  7267 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1440347 |  7268 | `				for( i = 0; i < nAtoms; i++ ){` |
|    764869 |  7269 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     88855 |  7270 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     88855 |  7271 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     88855 |  7272 | `						bFirst = 0;` |
|     88855 |  7273 | `						break;` |
|         - |  7274 | `					}` |
|    338012 |  7275 | `				}` |
|    382169 |  7276 | `			}` |
|         - |  7277 | `		}` |
|         - |  7278 | `		/* 3) null suffix */` |
|    127393 |  7279 | `		if( bNullable ){` |
|        20 |  7280 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        20 |  7281 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7282 | `		}` |
|         - |  7283 | `	}` |
|     63767 |  7284 | `}` |
|         - |  7285 |  |
|         - |  7286 | `/*` |
|         - |  7287 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7288 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7289 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7290 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7291 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7292 | ` * whether it was parenthesized.` |
|         - |  7293 | ` *` |
|         - |  7294 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7295 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7296 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7297 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7298 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7299 | ` */` |
|    127676 |  7300 | `static sxi32 GenStateParsePart(` |
|         - |  7301 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7302 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7303 | `{` |
|         - |  7304 | `	sxi32 rc;` |
|    127681 |  7305 | `	int nMembers = 0;` |
|    127681 |  7306 | `	int bParen = 0;` |
|    127681 |  7307 | `	*pnMembers = 0;` |
|    127681 |  7308 | `	*pbParen = 0;` |
|    127681 |  7309 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7310 | `		bParen = 1;` |
|         9 |  7311 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7312 | `	}` |
|     63838 |  7313 | `	for(;;){` |
|    127707 |  7314 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7315 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7316 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7317 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7318 | `		}` |
|    127707 |  7319 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    127707 |  7320 | `		if( rc != SXRET_OK ){` |
|         3 |  7321 | `			return rc;` |
|         - |  7322 | `		}` |
|    127705 |  7323 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    127705 |  7324 | `		(*pnAtoms)++;` |
|    127705 |  7325 | `		nMembers++;` |
|         - |  7326 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    127705 |  7327 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7328 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7329 | `			if( pNext < pGen->pEnd` |
|        39 |  7330 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7331 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7332 | `				continue;` |
|         - |  7333 | `			}` |
|         4 |  7334 | `		}` |
|    127679 |  7335 | `		break;` |
|       ! 0 |  7336 | `	}` |
|    127679 |  7337 | `	if( bParen ){` |
|         9 |  7338 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7339 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7340 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7341 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7342 | `		}` |
|         9 |  7343 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7344 | `		if( nMembers < 2 ){` |
|       ! 0 |  7345 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7346 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7347 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7348 | `		}` |
|         3 |  7349 | `	}` |
|    127679 |  7350 | `	*pnMembers = nMembers;` |
|    127679 |  7351 | `	*pbParen = bParen;` |
|    127679 |  7352 | `	return SXRET_OK;` |
|     63843 |  7353 | `}` |
|         - |  7354 |  |
|         - |  7355 | `/*` |
|         - |  7356 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7357 | ` *` |
|         - |  7358 | ` * Outputs:` |
|         - |  7359 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7360 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7361 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7362 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7363 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7364 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7365 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7366 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7367 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7368 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7369 | ` *` |
|         - |  7370 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7371 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7372 | ` */` |
|    127540 |  7373 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7374 | `	ph7_gen_state *pGen,` |
|         - |  7375 | `	sxu32 *pnType,` |
|         - |  7376 | `	SyString *pClass,` |
|         - |  7377 | `	SySet *pAlts,` |
|         - |  7378 | `	sxi32 *piTypeFlags,` |
|         - |  7379 | `	SyString *pTypeText,` |
|         - |  7380 | `	int iNullableFlag,` |
|         - |  7381 | `	int iUnionFlag,` |
|         - |  7382 | `	int bAllowVoid,` |
|         - |  7383 | `	sxu32 nLine` |
|         5 |  7384 | `){` |
|         - |  7385 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    127545 |  7386 | `	int nAtoms = 0;` |
|    127545 |  7387 | `	int bShortNullable = 0;` |
|    127545 |  7388 | `	int bExplicitNull = 0;` |
|         - |  7389 | `	sxi32 rc;` |
|    127545 |  7390 | `	*pnType = 0;` |
|    127545 |  7391 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    127545 |  7392 | `	*piTypeFlags = 0;` |
|    127545 |  7393 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7394 |  |
|    127545 |  7395 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7396 | `		return SXRET_OK;` |
|         - |  7397 | `	}` |
|         - |  7398 | ``	/* Optional `?` shorthand prefix */`` |
|    127540 |  7399 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       105 |  7400 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       105 |  7401 | `		bShortNullable = 1;` |
|       105 |  7402 | `		pGen->pIn++;` |
|       105 |  7403 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7404 | `			return SXERR_SYNTAX;` |
|         - |  7405 | `		}` |
|        50 |  7406 | `	}` |
|         - |  7407 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7408 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7409 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7410 | `	{` |
|         - |  7411 | `		int nMembers, bParen;` |
|    127545 |  7412 | `		sxu32 iGroup = 0;` |
|    127545 |  7413 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    127545 |  7414 | `		if( rc != SXRET_OK ){` |
|         4 |  7415 | `			return rc;` |
|         - |  7416 | `		}` |
|         - |  7417 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7418 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7419 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7420 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7421 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    191516 |  7422 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    127752 |  7423 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7424 | `			if( bShortNullable ){` |
|         - |  7425 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7426 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7427 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7428 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7429 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7430 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7431 | `			}` |
|       141 |  7432 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7433 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7434 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7435 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7436 | `			}` |
|       141 |  7437 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7438 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7439 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7440 | `				return rc;` |
|         - |  7441 | `			}` |
|         5 |  7442 | `		}` |
|    127541 |  7443 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7444 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7445 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7446 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7447 | `		}` |
|         - |  7448 | `	}` |
|         - |  7449 | `	/* Validation pass.` |
|         - |  7450 | `	 *` |
|         - |  7451 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7452 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7453 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7454 | `	 */` |
|         - |  7455 | `	{` |
|         - |  7456 | `		int i, j;` |
|    127541 |  7457 | `		int bHasNonNull = 0;` |
|    127541 |  7458 | `		int bAnyIntersection = 0;` |
|         - |  7459 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7460 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7461 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4208693 |  7462 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    255239 |  7463 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127703 |  7464 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     63854 |  7465 | `		}` |
|    255183 |  7466 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127673 |  7467 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     63826 |  7468 | `		}` |
|         - |  7469 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7470 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    127541 |  7471 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7472 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7473 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7474 | `			return SXERR_SYNTAX;` |
|         - |  7475 | `		}` |
|    255225 |  7476 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7477 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7478 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7479 | ``			 * `true`/`false` in an intersection). */`` |
|    127701 |  7480 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7481 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7482 | `				if( bClassLike ){` |
|        53 |  7483 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7484 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7485 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7486 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7487 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7488 | `						bClassLike = 0;` |
|       ! 0 |  7489 | `					}` |
|        24 |  7490 | `				}` |
|        55 |  7491 | `				if( !bClassLike ){` |
|         - |  7492 | `					const char *zName; sxu32 nName;` |
|         3 |  7493 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7494 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7495 | `					}else{` |
|         3 |  7496 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7497 | `					}` |
|         4 |  7498 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7499 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7500 | `						(int)nName, zName);` |
|         3 |  7501 | `					return SXERR_SYNTAX;` |
|         - |  7502 | `				}` |
|        24 |  7503 | `			}` |
|    127699 |  7504 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7505 | `				if( nAtoms > 1 ){` |
|         3 |  7506 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7507 | `						"Void can only be used as a standalone type");` |
|         3 |  7508 | `					return SXERR_SYNTAX;` |
|         - |  7509 | `				}` |
|       175 |  7510 | `				if( !bAllowVoid ){` |
|       ! 0 |  7511 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7512 | `						"void cannot be used here");` |
|       ! 0 |  7513 | `					return SXERR_SYNTAX;` |
|         - |  7514 | `				}` |
|       175 |  7515 | `				if( bShortNullable ){` |
|       ! 0 |  7516 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7517 | `						"Void type cannot be nullable");` |
|       ! 0 |  7518 | `					return SXERR_SYNTAX;` |
|         - |  7519 | `				}` |
|        85 |  7520 | `			}` |
|    127697 |  7521 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7522 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7523 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7524 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7525 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        27 |  7526 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7527 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7528 | `					 * same as any other non-standalone use. */` |
|         6 |  7529 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7530 | `						"never can only be used as a standalone type");` |
|         6 |  7531 | `					return SXERR_SYNTAX;` |
|         - |  7532 | `				}` |
|        21 |  7533 | `				if( !bAllowVoid ){` |
|         - |  7534 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7535 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7536 | `						"never cannot be used as a parameter type");` |
|         3 |  7537 | `					return SXERR_SYNTAX;` |
|         - |  7538 | `				}` |
|         8 |  7539 | `			}` |
|    127691 |  7540 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7541 | `				bExplicitNull = 1;` |
|        19 |  7542 | `			}else{` |
|    127661 |  7543 | `				bHasNonNull = 1;` |
|         - |  7544 | `			}` |
|         - |  7545 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7546 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7547 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7548 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7549 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    127891 |  7550 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7551 | `				int bDup = 0;` |
|       207 |  7552 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7553 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7554 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7555 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7556 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7557 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7558 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7559 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7560 | `								aAtoms[j].sClass.zString,` |
|        34 |  7561 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7562 | `							bDup = 1;` |
|       ! 0 |  7563 | `						}` |
|        27 |  7564 | `					}else{` |
|         3 |  7565 | `						bDup = 1;` |
|         - |  7566 | `					}` |
|        23 |  7567 | `				}` |
|       195 |  7568 | `				if( bDup ){` |
|         - |  7569 | `					const char *zName;` |
|         - |  7570 | `					sxu32 nName;` |
|         3 |  7571 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7572 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7573 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7574 | `					}else{` |
|         3 |  7575 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7576 | `						nName = aAtoms[i].nCanon;` |
|         - |  7577 | `					}` |
|         4 |  7578 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7579 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7580 | `					return SXERR_SYNTAX;` |
|         - |  7581 | `				}` |
|        99 |  7582 | `			}` |
|     63847 |  7583 | `		}` |
|    127529 |  7584 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7585 | `			if( bShortNullable ){` |
|         - |  7586 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7587 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7588 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7589 | `				return SXERR_SYNTAX;` |
|         - |  7590 | `			}` |
|         - |  7591 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7592 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7593 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7594 | `			 * atom, so set it here. */` |
|         7 |  7595 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7596 | `		}` |
|         - |  7597 | `	}` |
|         - |  7598 | `	/* Compute nullability flag */` |
|    127529 |  7599 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       133 |  7600 | `		*piTypeFlags \|= iNullableFlag;` |
|        64 |  7601 | `	}` |
|         - |  7602 | `	/* Build canonical type text */` |
|    127529 |  7603 | `	if( pTypeText ){` |
|         - |  7604 | `		SyBlob sBlob;` |
|    127529 |  7605 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    191242 |  7606 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     63762 |  7607 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    127529 |  7608 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    191012 |  7609 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    127338 |  7610 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    127343 |  7611 | `			if( zDup ){` |
|    127343 |  7612 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     63669 |  7613 | `			}` |
|     63669 |  7614 | `		}` |
|    127529 |  7615 | `		SyBlobRelease(&sBlob);` |
|     63762 |  7616 | `	}` |
|         - |  7617 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7618 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7619 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7620 | `	{` |
|    127529 |  7621 | `		int nNonNull = 0;` |
|    127529 |  7622 | `		int iNonNullIdx = -1;` |
|         - |  7623 | `		int i;` |
|    255205 |  7624 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127681 |  7625 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127651 |  7626 | `				nNonNull++;` |
|    127651 |  7627 | `				iNonNullIdx = i;` |
|     63823 |  7628 | `			}` |
|     63843 |  7629 | `		}` |
|    127529 |  7630 | `		if( nNonNull <= 1 ){` |
|         - |  7631 | `			/* Fast path: store as single type. */` |
|    127423 |  7632 | `			if( iNonNullIdx >= 0 ){` |
|    127417 |  7633 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    127417 |  7634 | `				if( pA->nType == SXU32_HIGH ){` |
|     57638 |  7635 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19211 |  7636 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38427 |  7637 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38427 |  7638 | `					*pnType = SXU32_HIGH;` |
|     38427 |  7639 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    108206 |  7640 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7641 | `					*pnType = MEMOBJ_VOID;` |
|     88910 |  7642 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7643 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7644 | `				}else{` |
|     88809 |  7645 | `					*pnType = pA->nType;` |
|         - |  7646 | `				}` |
|     63706 |  7647 | `			}` |
|     63714 |  7648 | `		}else{` |
|         - |  7649 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7650 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7651 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7652 | `				ph7_type_alt sAlt;` |
|       249 |  7653 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7654 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7655 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7656 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7657 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7658 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7659 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7660 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7661 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7662 | `				}else{` |
|       145 |  7663 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7664 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7665 | `				}` |
|       239 |  7666 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7667 | `			}` |
|         - |  7668 | `		}` |
|         - |  7669 | `	}` |
|    127529 |  7670 | `	return SXRET_OK;` |
|     63775 |  7671 | `}` |
|         - |  7672 |  |
|         - |  7673 | `/*` |
|         - |  7674 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7675 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7676 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7677 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7678 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7679 | `` *          and union types `: T\|U`.`` |
|         - |  7680 | ` */` |
|   2881488 |  7681 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7682 | `{` |
|   2881493 |  7683 | `	sxi32 iFlags = 0;` |
|         - |  7684 | `	sxi32 rc;` |
|         - |  7685 | `	sxu32 nLine;` |
|   2881493 |  7686 | `	pFunc->nReturnType = 0;` |
|   2881493 |  7687 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2881493 |  7688 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7689 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7690 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7691 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7692 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7693 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2881493 |  7694 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2881493 |  7695 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2881493 |  7696 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2869325 |  7697 | `		return SXRET_OK;` |
|         - |  7698 | `	}` |
|     12173 |  7699 | `	pGen->pIn++; /* Skip ':' */` |
|     12173 |  7700 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7701 | `		return SXRET_OK;` |
|         - |  7702 | `	}` |
|     12173 |  7703 | `	nLine = pGen->pIn->nLine;` |
|     12173 |  7704 | `	rc = GenStateParseUnionTypeDecl(` |
|      6084 |  7705 | `		pGen,` |
|      6084 |  7706 | `		&pFunc->nReturnType,` |
|      6084 |  7707 | `		&pFunc->sReturnClass,` |
|      6084 |  7708 | `		&pFunc->aReturnUnion,` |
|         - |  7709 | `		&iFlags,` |
|      6084 |  7710 | `		&pFunc->sReturnTypeName,` |
|         - |  7711 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7712 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7713 | `		/* iUnionFlag */ 0,` |
|         - |  7714 | `		/* bAllowVoid */ 1,` |
|      6084 |  7715 | `		nLine);` |
|     12173 |  7716 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7717 | `		return SXERR_ABORT;` |
|         - |  7718 | `	}` |
|     12173 |  7719 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7720 | `		/* Error already reported */` |
|       ! 0 |  7721 | `		return SXERR_SYNTAX;` |
|         - |  7722 | `	}` |
|     12173 |  7723 | `	if( rc == SXERR_SYNTAX ){` |
|         9 |  7724 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 |  7725 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7726 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7727 | `				&pGen->pIn->sData);` |
|         6 |  7728 | `		}else{` |
|       ! 0 |  7729 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7730 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7731 | `		}` |
|         9 |  7732 | `		return SXERR_SYNTAX;` |
|         - |  7733 | `	}` |
|     12167 |  7734 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12167 |  7735 | `	return SXRET_OK;` |
|   1440749 |  7736 | `}` |
|         - |  7737 |  |
|    486728 |  7738 | `static sxi32 GenStateCompileFunc(` |
|         - |  7739 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7740 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7741 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7742 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7743 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7744 | `	)` |
|         5 |  7745 | `{` |
|         - |  7746 | `	ph7_vm_func *pFunc;` |
|         - |  7747 | `	SyToken *pEnd;` |
|         - |  7748 | `	sxu32 nLine;` |
|         - |  7749 | `	char *zName;` |
|         - |  7750 | `	sxi32 rc;` |
|         - |  7751 | `	/* Extract line number */` |
|    486733 |  7752 | `	nLine = pGen->pIn->nLine;` |
|         - |  7753 | `	/* Jump the left parenthesis '(' */` |
|    486733 |  7754 | `	pGen->pIn++;` |
|         - |  7755 | `	/* Delimit the function signature */` |
|    486733 |  7756 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    486733 |  7757 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7758 | `		/* Syntax error */` |
|         8 |  7759 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7760 | `		(void)pName;` |
|         8 |  7761 | `		if( rc == SXERR_ABORT ){` |
|         - |  7762 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7763 | `			return SXERR_ABORT;` |
|         - |  7764 | `		}` |
|         8 |  7765 | `		pGen->pIn = pGen->pEnd;` |
|         8 |  7766 | `		return SXRET_OK;` |
|         - |  7767 | `	}` |
|         - |  7768 | `	/* Create the function state */` |
|    486727 |  7769 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    486727 |  7770 | `	if( pFunc == 0 ){` |
|       ! 0 |  7771 | `		goto OutOfMem;` |
|         - |  7772 | `	}` |
|         - |  7773 | `	/* Build the function name, prepending namespace if active */` |
|    486734 |  7774 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7775 | `		SyBlob sFQN;` |
|         - |  7776 | `		sxu32 nLen;` |
|        16 |  7777 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7778 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7779 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7780 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7781 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7782 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7783 | `		SyBlobRelease(&sFQN);` |
|        16 |  7784 | `		if( zName == 0 ){` |
|       ! 0 |  7785 | `			goto OutOfMem;` |
|         - |  7786 | `		}` |
|        16 |  7787 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7788 | `	}else{` |
|    486713 |  7789 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    486713 |  7790 | `		if( zName == 0 ){` |
|       ! 0 |  7791 | `			goto OutOfMem;` |
|         - |  7792 | `		}` |
|    486713 |  7793 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7794 | `	}` |
|         - |  7795 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7796 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    486727 |  7797 | `	pFunc->nLine = nLine;` |
|    486727 |  7798 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    486727 |  7799 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7800 | `		return SXERR_ABORT;` |
|         - |  7801 | `	}` |
|    486727 |  7802 | `	if( pGen->pIn < pEnd ){` |
|         - |  7803 | `		/* Collect function arguments */` |
|    424727 |  7804 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    424727 |  7805 | `		if( rc == SXERR_ABORT ){` |
|         - |  7806 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7807 | `			return SXERR_ABORT;` |
|         - |  7808 | `		}` |
|    212361 |  7809 | `	}` |
|         - |  7810 | `	/* Point past ')' and parse optional return type ': type' */` |
|    486727 |  7811 | `	pGen->pIn = &pEnd[1];` |
|         - |  7812 | `	{` |
|    486727 |  7813 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    486727 |  7814 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7815 | `			return SXERR_ABORT;` |
|    486727 |  7816 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 |  7817 | `			return SXERR_SYNTAX;` |
|         - |  7818 | `		}` |
|         - |  7819 | `	}` |
|    486721 |  7820 | `	if( bHandleClosure ){` |
|         - |  7821 | `		ph7_vm_func_closure_env sEnv;` |
|       575 |  7822 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       570 |  7823 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       334 |  7824 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        93 |  7825 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7826 | `				/* Closure,record environment variable */` |
|        93 |  7827 | `				pGen->pIn++;` |
|        93 |  7828 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7829 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7830 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7831 | `						return SXERR_ABORT;` |
|         - |  7832 | `					}` |
|       ! 0 |  7833 | `				}` |
|        93 |  7834 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7835 | `				/* Compile until we hit the first closing parenthesis */` |
|       191 |  7836 | `				while( pGen->pIn < pGen->pEnd ){` |
|       191 |  7837 | `					int iFlagsLocal = 0;` |
|       191 |  7838 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        93 |  7839 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        93 |  7840 | `						break;` |
|         - |  7841 | `					}` |
|       103 |  7842 | `					nLineLocal = pGen->pIn->nLine;` |
|       103 |  7843 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7844 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7845 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7846 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7847 | `						pGen->pIn++;` |
|        27 |  7848 | `					}` |
|        98 |  7849 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       103 |  7850 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7851 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7852 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7853 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7854 | `								return SXERR_ABORT;` |
|         - |  7855 | `							}` |
|         - |  7856 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7857 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7858 | `								pGen->pIn++;` |
|       ! 0 |  7859 | `							}` |
|       ! 0 |  7860 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7861 | `								pGen->pIn++;` |
|       ! 0 |  7862 | `							}` |
|       ! 0 |  7863 | `							break;` |
|         - |  7864 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7865 | `					}else{` |
|         - |  7866 | `						SyString *pNameLocal;` |
|         - |  7867 | `						char *zDup;` |
|         - |  7868 | `						/* Duplicate variable name */` |
|       103 |  7869 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       103 |  7870 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       103 |  7871 | `						if( zDup ){` |
|         - |  7872 | `							/* Zero the structure */` |
|       103 |  7873 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       103 |  7874 | `							sEnv.iFlags = iFlagsLocal;` |
|       103 |  7875 | `							sEnv.nIdx = SXU32_HIGH;` |
|       103 |  7876 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       103 |  7877 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       118 |  7878 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7879 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7880 | `									got_this = 1;` |
|       ! 0 |  7881 | `							}` |
|         - |  7882 | `							/* Save imported variable */` |
|       103 |  7883 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        54 |  7884 | `						}else{` |
|       ! 0 |  7885 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7886 | `							 return SXERR_ABORT;` |
|         - |  7887 | `						}` |
|         - |  7888 | `					}` |
|       103 |  7889 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       115 |  7890 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7891 | `						/* Ignore trailing commas */` |
|        13 |  7892 | `						pGen->pIn++;` |
|         1 |  7893 | `					}` |
|         5 |  7894 | `				}` |
|         - |  7895 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7896 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7897 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7898 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7899 | `				 * legacy pre-use position. */` |
|        93 |  7900 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7901 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7902 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7903 | `						return SXERR_ABORT;` |
|         7 |  7904 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7905 | `						return SXERR_SYNTAX;` |
|         - |  7906 | `					}` |
|         3 |  7907 | `				}` |
|        44 |  7908 | `		}` |
|       575 |  7909 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7910 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7911 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7912 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7913 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7914 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7915 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7916 | `			 * closure never binds $this (php). */` |
|       565 |  7917 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       565 |  7918 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       565 |  7919 | `			sEnv.nIdx = SXU32_HIGH;` |
|       565 |  7920 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       565 |  7921 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       565 |  7922 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       280 |  7923 | `		}` |
|       575 |  7924 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7925 | `			/* Mark as closure */` |
|       567 |  7926 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       281 |  7927 | `		}` |
|       285 |  7928 | `	}` |
|         - |  7929 | `	/* Compile the body */` |
|    486721 |  7930 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    486721 |  7931 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7932 | `		return SXERR_ABORT;` |
|         - |  7933 | `	}` |
|         - |  7934 | `	/* The cursor sits just past the body's closing brace */` |
|    486721 |  7935 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    486721 |  7936 | `	if( ppFunc ){` |
|    486721 |  7937 | `		*ppFunc = pFunc;` |
|    243358 |  7938 | `	}` |
|    486721 |  7939 | `	rc = SXRET_OK;` |
|    486721 |  7940 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7941 | `		/* Finally register the function */` |
|    486159 |  7942 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    243077 |  7943 | `	}` |
|    486721 |  7944 | `	if( rc == SXRET_OK ){` |
|    486721 |  7945 | `		return SXRET_OK;` |
|         - |  7946 | `	}` |
|         - |  7947 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7948 | `OutOfMem:` |
|         - |  7949 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7950 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7951 | `	 */` |
|       ! 0 |  7952 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7953 | `	return SXERR_ABORT;` |
|    243369 |  7954 | `}` |
|         - |  7955 | `/*` |
|         - |  7956 | ` * Compile a standard PHP function.` |
|         - |  7957 | ` *  Refer to the block-comment above for more information.` |
|         - |  7958 | ` */` |
|    486166 |  7959 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7960 | `{` |
|         - |  7961 | `	SyString *pName;` |
|         - |  7962 | `	sxi32 iFlags;` |
|         - |  7963 | `	sxu32 nKwLine;` |
|         - |  7964 | `	sxu32 nLine;` |
|         - |  7965 | `	sxi32 rc;` |
|         - |  7966 |  |
|    486171 |  7967 | `	nLine = pGen->pIn->nLine;` |
|    486171 |  7968 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    486171 |  7969 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    486171 |  7970 | `	iFlags = 0;` |
|    486171 |  7971 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7972 | `		/* Return by reference,remember that */` |
|        12 |  7973 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7974 | `		/* Jump the '&' token */` |
|        12 |  7975 | `		pGen->pIn++;` |
|         5 |  7976 | `	}` |
|    486171 |  7977 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7978 | `		/* Invalid function name */` |
|         8 |  7979 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         8 |  7980 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7981 | `			return SXERR_ABORT;` |
|         - |  7982 | `		}` |
|         - |  7983 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  7984 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  7985 | `			pGen->pIn++;` |
|         2 |  7986 | `		}` |
|         8 |  7987 | `		return SXRET_OK;` |
|         - |  7988 | `	}` |
|    486165 |  7989 | `	pName = &pGen->pIn->sData;` |
|    486165 |  7990 | `	nLine = pGen->pIn->nLine;` |
|         - |  7991 | `	/* Jump the function name */` |
|    486165 |  7992 | `	pGen->pIn++;` |
|    486165 |  7993 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7994 | `		/* Syntax error */` |
|         3 |  7995 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  7996 | `		if( rc == SXERR_ABORT ){` |
|         - |  7997 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7998 | `			return SXERR_ABORT;` |
|         - |  7999 | `		}` |
|         - |  8000 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  8001 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  8002 | `			pGen->pIn++;` |
|       ! 0 |  8003 | `		}` |
|         3 |  8004 | `		return SXRET_OK;` |
|         - |  8005 | `	}` |
|         - |  8006 | `	/* Compile function body */` |
|         - |  8007 | `	{` |
|    486163 |  8008 | `		ph7_vm_func *pFuncState = 0;` |
|    486163 |  8009 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    486163 |  8010 | `		if( pFuncState ){` |
|         - |  8011 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    486151 |  8012 | `			pFuncState->nLine = nKwLine;` |
|    243073 |  8013 | `		}` |
|         - |  8014 | `	}` |
|    486163 |  8015 | `	return rc;` |
|    243088 |  8016 | `}` |
|         - |  8017 | `/*` |
|         - |  8018 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  8019 | ` * According to the PHP language reference manual` |
|         - |  8020 | ` *  Visibility:` |
|         - |  8021 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  8022 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  8023 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  8024 | ` *  Members declared protected can be accessed only within the class` |
|         - |  8025 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  8026 | ` *  may only be accessed by the class that defines the member.` |
|         - |  8027 | ` */` |
|   3143420 |  8028 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  8029 | `{` |
|   3143425 |  8030 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    255875 |  8031 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2887555 |  8032 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    190887 |  8033 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  8034 | `	}` |
|         - |  8035 | `	/* Assume public by default */` |
|   2696673 |  8036 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1571715 |  8037 | `}` |
|         - |  8038 | `/*` |
|         - |  8039 | ` * Compile a class constant.` |
|         - |  8040 | ` * According to the PHP language reference manual` |
|         - |  8041 | ` *  Class Constants` |
|         - |  8042 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  8043 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  8044 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  8045 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  8046 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  8047 | ` *   It's also possible for interfaces to have constants.` |
|         - |  8048 | ` * Symisc eXtension.` |
|         - |  8049 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  8050 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8051 | ` *  Example:` |
|         - |  8052 | ` *   class Test{` |
|         - |  8053 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8054 | ` *   };` |
|         - |  8055 | ` *   var_dump(TEST::MyConst);` |
|         - |  8056 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8057 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8058 | ` */` |
|         - |  8059 | `/*` |
|         - |  8060 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  8061 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  8062 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  8063 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  8064 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  8065 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  8066 | ` */` |
|    290200 |  8067 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  8068 | `{` |
|         - |  8069 | `	SyToken *p0, *p1;` |
|    290205 |  8070 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8071 | `		return 0;` |
|         - |  8072 | `	}` |
|    290205 |  8073 | `	p0 = pGen->pIn;` |
|         - |  8074 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    290205 |  8075 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  8076 | `		return 1;` |
|         - |  8077 | `	}` |
|    290205 |  8078 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  8079 | `		return 1;` |
|         - |  8080 | `	}` |
|         - |  8081 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  8082 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  8083 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    290201 |  8084 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    290201 |  8085 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    290201 |  8086 | `		if( p1 ){` |
|    290201 |  8087 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  8088 | `				return 1;` |
|         - |  8089 | `			}` |
|    290171 |  8090 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  8091 | `				return 1;` |
|         - |  8092 | `			}` |
|    145081 |  8093 | `		}` |
|    145081 |  8094 | `	}` |
|    290167 |  8095 | `	return 0;` |
|    145105 |  8096 | `}` |
|         - |  8097 | `/*` |
|         - |  8098 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  8099 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  8100 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  8101 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  8102 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  8103 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  8104 | ` * Peek only; never consumes tokens.` |
|         - |  8105 | ` */` |
|        24 |  8106 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  8107 | `{` |
|        28 |  8108 | `	SyToken *p = pGen->pIn;` |
|        39 |  8109 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  8110 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  8111 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  8112 | `	}` |
|        28 |  8113 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  8114 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  8115 | `	}` |
|         6 |  8116 | `	p++;` |
|         - |  8117 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  8118 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  8119 | `}` |
|         - |  8120 | `/*` |
|         - |  8121 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  8122 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  8123 | `` * `$o->new`), not a `new` expression.`` |
|         - |  8124 | ` */` |
|       110 |  8125 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  8126 | `{` |
|         - |  8127 | `	sxi32 iOp;` |
|       114 |  8128 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  8129 | `		return 0;` |
|         - |  8130 | `	}` |
|       104 |  8131 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  8132 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  8133 | `}` |
|         - |  8134 | `/*` |
|         - |  8135 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  8136 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  8137 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  8138 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  8139 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  8140 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  8141 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  8142 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  8143 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  8144 | ` *` |
|         - |  8145 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  8146 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  8147 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  8148 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  8149 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  8150 | ` */` |
|    626714 |  8151 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  8152 | `{` |
|    626719 |  8153 | `	SyToken *p = pGen->pIn;` |
|    626719 |  8154 | `	int iDepth = 0;` |
|   1655799 |  8155 | `	while( p < pGen->pEnd ){` |
|   1655799 |  8156 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    626667 |  8157 | `			break; /* end of this initializer */` |
|         - |  8158 | `		}` |
|   1029132 |  8159 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    518404 |  8160 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7666 |  8161 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  8162 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  8163 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  8164 | `			 * expression. */` |
|         3 |  8165 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  8166 | `			p++;` |
|         3 |  8167 | `			if( bArrow ){` |
|         - |  8168 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  8169 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  8170 | `				int iBase = iDepth;` |
|        17 |  8171 | `				while( p < pGen->pEnd ){` |
|        17 |  8172 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  8173 | `						iDepth++;` |
|        15 |  8174 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  8175 | `						if( iDepth <= iBase ){` |
|       ! 0 |  8176 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  8177 | `						}` |
|         5 |  8178 | `						iDepth--;` |
|        11 |  8179 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  8180 | `						break;` |
|         - |  8181 | `					}` |
|        15 |  8182 | `					p++;` |
|         1 |  8183 | `				}` |
|         2 |  8184 | `			}else{` |
|         - |  8185 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  8186 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  8187 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  8188 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  8189 | `				int iLocal = 0;` |
|       ! 0 |  8190 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  8191 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  8192 | `						break; /* body brace */` |
|         - |  8193 | `					}` |
|       ! 0 |  8194 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  8195 | `						iLocal++;` |
|       ! 0 |  8196 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  8197 | `						if( iLocal > 0 ){` |
|       ! 0 |  8198 | `							iLocal--;` |
|       ! 0 |  8199 | `						}` |
|       ! 0 |  8200 | `					}` |
|       ! 0 |  8201 | `					p++;` |
|       ! 0 |  8202 | `				}` |
|       ! 0 |  8203 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  8204 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  8205 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  8206 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  8207 | `							iBrace++;` |
|       ! 0 |  8208 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  8209 | `							iBrace--;` |
|       ! 0 |  8210 | `							if( iBrace == 0 ){` |
|       ! 0 |  8211 | `								p++;` |
|       ! 0 |  8212 | `								break;` |
|         - |  8213 | `							}` |
|       ! 0 |  8214 | `						}` |
|       ! 0 |  8215 | `						p++;` |
|       ! 0 |  8216 | `					}` |
|       ! 0 |  8217 | `				}` |
|         - |  8218 | `			}` |
|         3 |  8219 | `			continue;` |
|         - |  8220 | `		}` |
|   1029135 |  8221 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  8222 | `			if( iDepth == 0 ){` |
|         - |  8223 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  8224 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  8225 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  8226 | `				 * is legal — don't scan into it. */` |
|        45 |  8227 | `				break;` |
|         - |  8228 | `			}` |
|       ! 0 |  8229 | `			iDepth++;` |
|   1029091 |  8230 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42073 |  8231 | `			iDepth++;` |
|   1008057 |  8232 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     42071 |  8233 | `			if( iDepth > 0 ){` |
|     42071 |  8234 | `				iDepth--;` |
|     21033 |  8235 | `			}` |
|    965990 |  8236 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    346437 |  8237 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8238 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8239 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8240 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  8241 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  8242 | `				return 1;` |
|         - |  8243 | `			}` |
|       ! 0 |  8244 | `		}` |
|   1029083 |  8245 | `		p++;` |
|         5 |  8246 | `	}` |
|    626711 |  8247 | `	return 0;` |
|    313362 |  8248 | `}` |
|         - |  8249 | `/*` |
|         - |  8250 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8251 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8252 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8253 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8254 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8255 | ` * share the same backing.` |
|         - |  8256 | ` */` |
|       362 |  8257 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8258 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8259 | `{` |
|       367 |  8260 | `	pAttr->nType = nType;` |
|       367 |  8261 | `	pAttr->sClass = *pClass;` |
|       367 |  8262 | `	pAttr->sTypeName = *pTypeName;` |
|       367 |  8263 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8264 | `		sxu32 i;` |
|        73 |  8265 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8266 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8267 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8268 | `		}` |
|        11 |  8269 | `	}` |
|       367 |  8270 | `}` |
|    290200 |  8271 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8272 | `{` |
|    290205 |  8273 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8274 | `	SySet *pInstrContainer;` |
|         - |  8275 | `	ph7_class_attr *pCons;` |
|         - |  8276 | `	SyString *pName;` |
|         - |  8277 | `	sxi32 rc;` |
|    290205 |  8278 | `	sxu32 nType = 0;` |
|         - |  8279 | `	SyString sTypeClass;` |
|         - |  8280 | `	SyString sTypeText;` |
|         - |  8281 | `	SySet aUnionAlts;` |
|    290205 |  8282 | `	sxi32 iTypeFlags = 0;` |
|    290205 |  8283 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    290205 |  8284 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    290205 |  8285 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8286 | `	/* Extract visibility level */` |
|    290205 |  8287 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8288 | `	/* Mark as constant */` |
|    290205 |  8289 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    290205 |  8290 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8291 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8292 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    290224 |  8293 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8294 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8295 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8296 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8297 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8298 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8299 | `		 * and success paths release. */` |
|        42 |  8300 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8301 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8302 | `			goto Synchronize;` |
|        42 |  8303 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8304 | `			return SXERR_ABORT;` |
|        42 |  8305 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8306 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8307 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8308 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8309 | `				return SXERR_ABORT;` |
|         - |  8310 | `			}` |
|       ! 0 |  8311 | `			goto Synchronize;` |
|         - |  8312 | `		}` |
|        42 |  8313 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8314 | `	}` |
|    145100 |  8315 | `loop:` |
|    290207 |  8316 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8317 | `		/* Invalid constant name */` |
|       ! 0 |  8318 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8319 | `		if( rc == SXERR_ABORT ){` |
|         - |  8320 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8321 | `			return SXERR_ABORT;` |
|         - |  8322 | `		}` |
|       ! 0 |  8323 | `		goto Synchronize;` |
|         - |  8324 | `	}` |
|         - |  8325 | `	/* Peek constant name */` |
|    290207 |  8326 | `	pName = &pGen->pIn->sData;` |
|         - |  8327 | `	/* Make sure the constant name isn't reserved */` |
|    290207 |  8328 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8329 | `		/* Reserved constant name */` |
|       ! 0 |  8330 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8331 | `		if( rc == SXERR_ABORT ){` |
|         - |  8332 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8333 | `			return SXERR_ABORT;` |
|         - |  8334 | `		}` |
|       ! 0 |  8335 | `		goto Synchronize;` |
|         - |  8336 | `	}` |
|         - |  8337 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    290207 |  8338 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8339 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8340 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8341 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8342 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8343 | `			return SXERR_ABORT;` |
|        42 |  8344 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8345 | `			goto Synchronize;` |
|         - |  8346 | `		}` |
|        18 |  8347 | `	}` |
|         - |  8348 | `	/* Advance the stream cursor */` |
|    290205 |  8349 | `	pGen->pIn++;` |
|    290205 |  8350 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8351 | `		/* Invalid declaration */` |
|       ! 0 |  8352 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8353 | `		if( rc == SXERR_ABORT ){` |
|         - |  8354 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8355 | `			return SXERR_ABORT;` |
|         - |  8356 | `		}` |
|       ! 0 |  8357 | `		goto Synchronize;` |
|         - |  8358 | `	}` |
|    290205 |  8359 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8360 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8361 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8362 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8363 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    290200 |  8364 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8365 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8366 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8367 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8368 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8369 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8370 | `			return SXERR_ABORT;` |
|         - |  8371 | `		}` |
|         6 |  8372 | `		goto Synchronize;` |
|         - |  8373 | `	}` |
|         - |  8374 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8375 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8376 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    290201 |  8377 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8378 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8379 | `			"New expressions are not supported in this context");` |
|         5 |  8380 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8381 | `			return SXERR_ABORT;` |
|         - |  8382 | `		}` |
|         5 |  8383 | `		goto Synchronize;` |
|         - |  8384 | `	}` |
|         - |  8385 | `	/* Allocate a new class attribute */` |
|    290197 |  8386 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    290197 |  8387 | `	if( pCons ){` |
|    290197 |  8388 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    290197 |  8389 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8390 | `			return SXERR_ABORT;` |
|         - |  8391 | `		}` |
|    145096 |  8392 | `	}` |
|    290197 |  8393 | `	if( pCons == 0 ){` |
|       ! 0 |  8394 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8395 | `		return SXERR_ABORT;` |
|         - |  8396 | `	}` |
|    290197 |  8397 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8398 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8399 | `	}` |
|         - |  8400 | `	/* Swap bytecode container */` |
|    290197 |  8401 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    290197 |  8402 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8403 | `	/* Compile constant value.` |
|         - |  8404 | `	 */` |
|    290197 |  8405 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    290197 |  8406 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8407 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8408 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8409 | `			return SXERR_ABORT;` |
|         - |  8410 | `		}` |
|         1 |  8411 | `	}` |
|         - |  8412 | `	/* Emit the done instruction */` |
|    290197 |  8413 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    290197 |  8414 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    290197 |  8415 | `	if( rc == SXERR_ABORT ){` |
|         - |  8416 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8417 | `		return SXERR_ABORT;` |
|         - |  8418 | `	}` |
|         - |  8419 | `	/* All done,install the constant */` |
|    290197 |  8420 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    290197 |  8421 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8422 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8423 | `		return SXERR_ABORT;` |
|         - |  8424 | `	}` |
|    290197 |  8425 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8426 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8427 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8428 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8429 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8430 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8431 | `				pTok--;` |
|       ! 0 |  8432 | `			}` |
|       ! 0 |  8433 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8434 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8435 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8436 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8437 | `				return SXERR_ABORT;` |
|         - |  8438 | `			}` |
|       ! 0 |  8439 | `		}else{` |
|         3 |  8440 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8441 | `				goto loop;` |
|         - |  8442 | `			}` |
|         - |  8443 | `		}` |
|       ! 0 |  8444 | `	}` |
|    290195 |  8445 | `	SySetRelease(&aUnionAlts);` |
|    290195 |  8446 | `	return SXRET_OK;` |
|         5 |  8447 | `Synchronize:` |
|        13 |  8448 | `	SySetRelease(&aUnionAlts);` |
|         - |  8449 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8450 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8451 | `		pGen->pIn++;` |
|         3 |  8452 | `	}` |
|        13 |  8453 | `	return SXERR_CORRUPT;` |
|    145105 |  8454 | `}` |
|         - |  8455 | `/*` |
|         - |  8456 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8457 | ` * According to the PHP language reference manual` |
|         - |  8458 | ` *  Properties` |
|         - |  8459 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8460 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8461 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8462 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8463 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8464 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8465 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8466 | ` * Symisc eXtension.` |
|         - |  8467 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8468 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8469 | ` *  Example:` |
|         - |  8470 | ` *   class Test{` |
|         - |  8471 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8472 | ` *   };` |
|         - |  8473 | ` *   var_dump(TEST::myVar);` |
|         - |  8474 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8475 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8476 | ` */` |
|         - |  8477 | `/*` |
|         - |  8478 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8479 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8480 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8481 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8482 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8483 | ` */` |
|   2348568 |  8484 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8485 | `{` |
|   2348573 |  8486 | `	SyToken *p = pStart;` |
|   2348573 |  8487 | `	int bFirst = 1;` |
|   2348573 |  8488 | `	if( p >= pEnd ) return 0;` |
|         - |  8489 | ``	/* Optional nullable `?` shorthand. */`` |
|   2348573 |  8490 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        39 |  8491 | `		p++;` |
|        39 |  8492 | `		if( p >= pEnd ) return 0;` |
|        18 |  8493 | `	}` |
|         - |  8494 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8495 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8496 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8497 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1174284 |  8498 | `	for(;;){` |
|   2348593 |  8499 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8500 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8501 | `			p++;` |
|         9 |  8502 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8503 | `			if( p >= pEnd ) return 0;` |
|         3 |  8504 | `			p++; /* skip ')' */` |
|         2 |  8505 | `		}else{` |
|         - |  8506 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8507 | ``			 * then any `&`-joined intersection members. */`` |
|   2348591 |  8508 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2348591 |  8509 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8510 | `				return 0;` |
|         - |  8511 | `			}` |
|         - |  8512 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8513 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8514 | `			 * may still appear at the initial dispatch site). */` |
|   2348591 |  8515 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2348543 |  8516 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2348538 |  8517 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    103476 |  8518 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2348249 |  8519 | `					return 0;` |
|         - |  8520 | `				}` |
|       147 |  8521 | `			}` |
|       347 |  8522 | `			p++;` |
|       349 |  8523 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8524 | `				p += 2;` |
|         1 |  8525 | `			}` |
|       516 |  8526 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       350 |  8527 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8528 | `				p++; /* skip '&' */` |
|         3 |  8529 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8530 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8531 | `				p++;` |
|         3 |  8532 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8533 | `					p += 2;` |
|       ! 0 |  8534 | `				}` |
|         1 |  8535 | `			}` |
|         - |  8536 | `		}` |
|       349 |  8537 | `		bFirst = 0;` |
|       344 |  8538 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8539 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8540 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8541 | `			continue;` |
|         - |  8542 | `		}` |
|       329 |  8543 | `		break;` |
|       ! 0 |  8544 | `	}` |
|       329 |  8545 | `	if( p >= pEnd ) return 0;` |
|       329 |  8546 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1174289 |  8547 | `}` |
|         - |  8548 |  |
|         - |  8549 | `/*` |
|         - |  8550 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8551 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8552 | ` * if not). Recognized forms:` |
|         - |  8553 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8554 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8555 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8556 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8557 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8558 | ` * on unrecoverable error.` |
|         - |  8559 | ` *` |
|         - |  8560 | ` * When a type is parsed:` |
|         - |  8561 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8562 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8563 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8564 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8565 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8566 | ` */` |
|       334 |  8567 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8568 | `	ph7_gen_state *pGen,` |
|         - |  8569 | `	sxu32 *pnType,` |
|         - |  8570 | `	SyString *pClass,` |
|         - |  8571 | `	sxi32 *piTypeFlags,` |
|         - |  8572 | `	SyString *pTypeText,` |
|         - |  8573 | `	SySet *pAlts` |
|         5 |  8574 | `){` |
|       339 |  8575 | `	sxi32 iFlags = 0;` |
|         - |  8576 | `	sxi32 rc;` |
|       339 |  8577 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8578 | `		return SXRET_OK;` |
|         - |  8579 | `	}` |
|         - |  8580 | `	/* If the first token is '$', there's no type */` |
|       339 |  8581 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8582 | `		return SXRET_OK;` |
|         - |  8583 | `	}` |
|       339 |  8584 | `	rc = GenStateParseUnionTypeDecl(` |
|       167 |  8585 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8586 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8587 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8588 | `		/* bAllowVoid */ 0,` |
|       334 |  8589 | `		pGen->pIn->nLine);` |
|       339 |  8590 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8591 | `		return rc;` |
|         - |  8592 | `	}` |
|         - |  8593 | `	/* Verify next token is '$' (start of property name) */` |
|       339 |  8594 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8595 | `		return SXERR_SYNTAX;` |
|         - |  8596 | `	}` |
|       339 |  8597 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       339 |  8598 | `	return SXRET_OK;` |
|       172 |  8599 | `}` |
|         - |  8600 |  |
|         - |  8601 | `/*` |
|         - |  8602 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8603 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8604 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8605 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8606 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8607 | ` * by the type parser itself before reaching here.` |
|         - |  8608 | ` *` |
|         - |  8609 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8610 | ` * use in the error message.` |
|         - |  8611 | ` */` |
|       510 |  8612 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8613 | `	sxu32 nType,` |
|         - |  8614 | `	const SyString *pClass,` |
|         - |  8615 | `	const char **pzName,` |
|         - |  8616 | `	sxu32 *pnName)` |
|         5 |  8617 | `{` |
|         - |  8618 | `	const char *z;` |
|         - |  8619 | `	sxu32 n;` |
|       515 |  8620 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       459 |  8621 | `		return 0;` |
|         - |  8622 | `	}` |
|        60 |  8623 | `	z = pClass->zString;` |
|        60 |  8624 | `	n = pClass->nByte;` |
|        60 |  8625 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8626 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8627 | `	}` |
|         - |  8628 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8629 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8630 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        54 |  8631 | `	return 0;` |
|       260 |  8632 | `}` |
|         - |  8633 |  |
|         - |  8634 | `/*` |
|         - |  8635 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8636 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8637 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8638 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8639 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8640 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8641 | ` *` |
|         - |  8642 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8643 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8644 | ` */` |
|       448 |  8645 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8646 | `	ph7_gen_state *pGen,` |
|         - |  8647 | `	ph7_class *pClass,` |
|         - |  8648 | `	const SyString *pMemberName,` |
|         - |  8649 | `	sxu32 nType,` |
|         - |  8650 | `	const SyString *pTypeClass,` |
|         - |  8651 | `	const SyString *pTypeText,` |
|         - |  8652 | `	SySet *pUnionAlts,` |
|         - |  8653 | `	const char *zErrFmt,` |
|         - |  8654 | `	sxu32 nLine)` |
|         5 |  8655 | `{` |
|       453 |  8656 | `	const char *zBad = 0;` |
|       453 |  8657 | `	sxu32 nBad = 0;` |
|         - |  8658 | `	SyString sFallback;` |
|         - |  8659 | `	const SyString *pBad;` |
|         - |  8660 | `	sxi32 rc;` |
|       453 |  8661 | `	int bDisallowed = 0;` |
|       453 |  8662 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8663 | `		bDisallowed = 1;` |
|       451 |  8664 | `	}else if( pUnionAlts ){` |
|         - |  8665 | `		sxu32 i;` |
|        95 |  8666 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8667 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8668 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8669 | `				bDisallowed = 1;` |
|         3 |  8670 | `				break;` |
|         - |  8671 | `			}` |
|        35 |  8672 | `		}` |
|        15 |  8673 | `	}` |
|       453 |  8674 | `	if( !bDisallowed ){` |
|       447 |  8675 | `		return SXRET_OK;` |
|         - |  8676 | `	}` |
|         - |  8677 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8678 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8679 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8680 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8681 | `		pBad = pTypeText;` |
|         5 |  8682 | `	}else{` |
|       ! 0 |  8683 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8684 | `		pBad = &sFallback;` |
|         - |  8685 | `	}` |
|        11 |  8686 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8687 | `		zErrFmt,` |
|         3 |  8688 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8689 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8690 | `		return SXERR_ABORT;` |
|         - |  8691 | `	}` |
|         8 |  8692 | `	return SXERR_SYNTAX;` |
|       229 |  8693 | `}` |
|         - |  8694 | `/*` |
|         - |  8695 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8696 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8697 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8698 | ` * than promoted to a lexer keyword.` |
|         - |  8699 | ` */` |
|  20276712 |  8700 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8701 | `{` |
|  20494980 |  8702 | `	return (pTok->nType & PH7_TK_ID)` |
|  10356619 |  8703 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  20494975 |  8704 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8705 | `}` |
|         - |  8706 | `/*` |
|         - |  8707 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8708 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8709 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8710 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8711 | ` */` |
|   7276932 |  8712 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8713 | `{` |
|   7276937 |  8714 | `	*pnTok = 0;` |
|   7276932 |  8715 | `	if( &pTok[3] < pEnd` |
|   6821942 |  8716 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5614330 |  8717 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2430862 |  8718 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8719 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8720 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8721 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8722 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8723 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8724 | `			*pnTok = 4;` |
|        17 |  8725 | `			return nKw;` |
|         - |  8726 | `		}` |
|       ! 0 |  8727 | `	}` |
|   7276921 |  8728 | `	return 0;` |
|   3638471 |  8729 | `}` |
|         - |  8730 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8731 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8732 | `{` |
|        17 |  8733 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8734 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8735 | `	}` |
|         5 |  8736 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8737 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8738 | `	}` |
|         3 |  8739 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8740 | `}` |
|    458924 |  8741 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8742 | `{` |
|    458929 |  8743 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8744 | `	ph7_class_attr *pAttr;` |
|         - |  8745 | `	SyString *pName;` |
|         - |  8746 | `	sxi32 rc;` |
|    458929 |  8747 | `	sxu32 nType = 0;` |
|         - |  8748 | `	SyString sTypeClass;` |
|         - |  8749 | `	SyString sTypeText;` |
|         - |  8750 | `	SySet aUnionAlts;` |
|    458929 |  8751 | `	sxi32 iTypeFlags = 0;` |
|    458929 |  8752 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    458929 |  8753 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    458929 |  8754 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8755 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8756 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8757 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    458929 |  8758 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8759 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8760 | `	}` |
|         - |  8761 | `	/* Extract visibility level */` |
|    458929 |  8762 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8763 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    459096 |  8764 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       339 |  8765 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       339 |  8766 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8767 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8768 | `			goto Synchronize;` |
|       339 |  8769 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8770 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8771 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8772 | `				&pGen->pIn->sData);` |
|       ! 0 |  8773 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8774 | `				return SXERR_ABORT;` |
|         - |  8775 | `			}` |
|       ! 0 |  8776 | `			goto Synchronize;` |
|       339 |  8777 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8778 | `			return SXERR_ABORT;` |
|         - |  8779 | `		}` |
|       167 |  8780 | `	}` |
|       ! 0 |  8781 | `loop:` |
|    458933 |  8782 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8783 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8784 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8785 | `			return SXERR_ABORT;` |
|         - |  8786 | `		}` |
|       ! 0 |  8787 | `		goto Synchronize;` |
|         - |  8788 | `	}` |
|    458933 |  8789 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    458933 |  8790 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8791 | `		/* Invalid attribute name */` |
|       ! 0 |  8792 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8793 | `		if( rc == SXERR_ABORT ){` |
|         - |  8794 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8795 | `			return SXERR_ABORT;` |
|         - |  8796 | `		}` |
|       ! 0 |  8797 | `		goto Synchronize;` |
|         - |  8798 | `	}` |
|         - |  8799 | `	/* Peek attribute name */` |
|    458933 |  8800 | `	pName = &pGen->pIn->sData;` |
|         - |  8801 | `	/* Advance the stream cursor */` |
|    458933 |  8802 | `	pGen->pIn++;` |
|    458933 |  8803 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8804 | `		/* Invalid declaration */` |
|         3 |  8805 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8806 | `		if( rc == SXERR_ABORT ){` |
|         - |  8807 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8808 | `			return SXERR_ABORT;` |
|         - |  8809 | `		}` |
|         3 |  8810 | `		goto Synchronize;` |
|         - |  8811 | `	}` |
|         - |  8812 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8813 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    458931 |  8814 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8815 | `		const char *zAvErr = 0;` |
|        19 |  8816 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8817 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8818 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8819 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8820 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8821 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8822 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8823 | `		}` |
|        13 |  8824 | `		if( zAvErr ){` |
|       ! 0 |  8825 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8826 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8827 | `				return SXERR_ABORT;` |
|         - |  8828 | `			}` |
|       ! 0 |  8829 | `			goto Synchronize;` |
|         - |  8830 | `		}` |
|         6 |  8831 | `	}` |
|         - |  8832 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8833 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    458931 |  8834 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8835 | `		const char *zRoErr = 0;` |
|        43 |  8836 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8837 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8838 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8839 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8840 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8841 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8842 | `		}` |
|        43 |  8843 | `		if( zRoErr ){` |
|        13 |  8844 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8845 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8846 | `				return SXERR_ABORT;` |
|         - |  8847 | `			}` |
|        13 |  8848 | `			goto Synchronize;` |
|         - |  8849 | `		}` |
|        14 |  8850 | `	}` |
|         - |  8851 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8852 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8853 | `	 * by the type parser. */` |
|    458921 |  8854 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       503 |  8855 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8856 | `			&sTypeText,` |
|       332 |  8857 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       166 |  8858 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       337 |  8859 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8860 | `			return SXERR_ABORT;` |
|       337 |  8861 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8862 | `			goto Synchronize;` |
|         - |  8863 | `		}` |
|       166 |  8864 | `	}` |
|         - |  8865 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    458921 |  8866 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8867 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8868 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8869 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8870 | `			return SXERR_ABORT;` |
|         - |  8871 | `		}` |
|         3 |  8872 | `		goto Synchronize;` |
|         - |  8873 | `	}` |
|         - |  8874 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8875 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8876 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8877 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8878 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8879 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    458919 |  8880 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8882 | `			"New expressions are not supported in this context");` |
|         6 |  8883 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8884 | `			return SXERR_ABORT;` |
|         - |  8885 | `		}` |
|         6 |  8886 | `		goto Synchronize;` |
|         - |  8887 | `	}` |
|         - |  8888 | `	/* Allocate a new class attribute */` |
|    458915 |  8889 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    458915 |  8890 | `	if( pAttr ){` |
|    458915 |  8891 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    458915 |  8892 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8893 | `			return SXERR_ABORT;` |
|         - |  8894 | `		}` |
|    229455 |  8895 | `	}` |
|    458915 |  8896 | `	if( pAttr == 0 ){` |
|       ! 0 |  8897 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8898 | `		return SXERR_ABORT;` |
|         - |  8899 | `	}` |
|    458915 |  8900 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       335 |  8901 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       165 |  8902 | `	}` |
|    458915 |  8903 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8904 | `		SySet *pInstrContainer;` |
|    336519 |  8905 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    336519 |  8906 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8907 | `		{` |
|         - |  8908 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8909 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8910 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8911 | `			 * compiler would otherwise run into the hook tokens. */` |
|    336519 |  8912 | `			SyToken *pScan = pGen->pIn;` |
|    336519 |  8913 | `			sxi32 iNest = 0;` |
|    734803 |  8914 | `			while( pScan < pGen->pEnd ){` |
|    734803 |  8915 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42067 |  8916 | `					iNest++;` |
|    713772 |  8917 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     42067 |  8918 | `					iNest--;` |
|    671710 |  8919 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    336519 |  8920 | `					break;` |
|         - |  8921 | `				}` |
|    398289 |  8922 | `				pScan++;` |
|         5 |  8923 | `			}` |
|    336519 |  8924 | `			pGen->pEnd = pScan;` |
|         - |  8925 | `		}` |
|         - |  8926 | `		/* Swap bytecode container */` |
|    336519 |  8927 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    336519 |  8928 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8929 | `		/* Compile attribute value.` |
|         - |  8930 | `		 */` |
|    336519 |  8931 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    336519 |  8932 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8933 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8934 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8935 | `				return SXERR_ABORT;` |
|         - |  8936 | `			}` |
|       ! 0 |  8937 | `		}` |
|         - |  8938 | `		/* Emit the done instruction */` |
|    336519 |  8939 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    336519 |  8940 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    336519 |  8941 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    336519 |  8942 | `		pGen->pEnd = pSavedDefEnd;` |
|    168257 |  8943 | `	}` |
|         - |  8944 | `	/* All done,install the attribute */` |
|    458915 |  8945 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    458915 |  8946 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8947 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8948 | `		return SXERR_ABORT;` |
|         - |  8949 | `	}` |
|    458915 |  8950 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8951 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8952 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8953 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8954 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8955 | `			return SXERR_ABORT;` |
|         - |  8956 | `		}` |
|        95 |  8957 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8958 | `			goto Synchronize;` |
|         - |  8959 | `		}` |
|        95 |  8960 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8961 | `		return SXRET_OK;` |
|         - |  8962 | `	}` |
|    458821 |  8963 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8964 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8965 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8966 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8967 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8968 | `				? "Interfaces may only include hooked properties"` |
|         - |  8969 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8970 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8971 | `			return SXERR_ABORT;` |
|         - |  8972 | `		}` |
|       ! 0 |  8973 | `		goto Synchronize;` |
|         - |  8974 | `	}` |
|    458821 |  8975 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8976 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8977 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8978 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8979 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8980 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8981 | `				pTok--;` |
|       ! 0 |  8982 | `			}` |
|       ! 0 |  8983 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8984 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8985 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8986 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8987 | `				return SXERR_ABORT;` |
|         - |  8988 | `			}` |
|       ! 0 |  8989 | `		}else{` |
|         5 |  8990 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8991 | `				goto loop;` |
|         - |  8992 | `			}` |
|         - |  8993 | `		}` |
|       ! 0 |  8994 | `	}` |
|    458817 |  8995 | `	SySetRelease(&aUnionAlts);` |
|    458817 |  8996 | `	return SXRET_OK;` |
|         9 |  8997 | `Synchronize:` |
|         - |  8998 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8999 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  9000 | `		pGen->pIn++;` |
|         3 |  9001 | `	}` |
|        22 |  9002 | `	SySetRelease(&aUnionAlts);` |
|        22 |  9003 | `	return SXERR_CORRUPT;` |
|    229467 |  9004 | `}` |
|         - |  9005 | `/*` |
|         - |  9006 | ` * Compile a class method.` |
|         - |  9007 | ` *` |
|         - |  9008 | ` * Refer to the official documentation for more information` |
|         - |  9009 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  9010 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  9011 | ` * overloading and many more.` |
|         - |  9012 | ` */` |
|   2394296 |  9013 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  9014 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  9015 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  9016 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  9017 | `	int doBody,          /* TRUE to process method body */` |
|         - |  9018 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  9019 | `	)` |
|         5 |  9020 | `{` |
|   2394301 |  9021 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2394301 |  9022 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  9023 | `	ph7_class_method *pMeth;` |
|         - |  9024 | `	sxi32 iFuncFlags;` |
|         - |  9025 | `	SyString *pName;` |
|         - |  9026 | `	SyToken *pEnd;` |
|         - |  9027 | `	sxi32 rc;` |
|         - |  9028 | `	/* Extract visibility level */` |
|   2394301 |  9029 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2394301 |  9030 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2394301 |  9031 | `	iFuncFlags = 0;` |
|   2394301 |  9032 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9033 | `		/* Invalid method name */` |
|       ! 0 |  9034 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9035 | `		if( rc == SXERR_ABORT ){` |
|         - |  9036 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9037 | `			return SXERR_ABORT;` |
|         - |  9038 | `		}` |
|       ! 0 |  9039 | `		goto Synchronize;` |
|         - |  9040 | `	}` |
|   2394301 |  9041 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  9042 | `		/* Return by reference,remember that */` |
|       ! 0 |  9043 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  9044 | `		/* Jump the '&' token */` |
|       ! 0 |  9045 | `		pGen->pIn++;` |
|       ! 0 |  9046 | `	}` |
|   2394301 |  9047 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  9048 | `		/* Invalid method name */` |
|       ! 0 |  9049 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9050 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9051 | `			return SXERR_ABORT;` |
|         - |  9052 | `		}` |
|       ! 0 |  9053 | `		goto Synchronize;` |
|         - |  9054 | `	}` |
|         - |  9055 | `	/* Peek method name */` |
|   2394301 |  9056 | `	pName = &pGen->pIn->sData;` |
|   2394301 |  9057 | `	nLine = pGen->pIn->nLine;` |
|         - |  9058 | `	/* Jump the method name */` |
|   2394301 |  9059 | `	pGen->pIn++;` |
|   2394301 |  9060 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  9061 | `		/* Abstract method */` |
|    137449 |  9062 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  9063 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9064 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  9065 | `				&pClass->sName,pName);` |
|       ! 0 |  9066 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9067 | `				return SXERR_ABORT;` |
|         - |  9068 | `			}` |
|       ! 0 |  9069 | `		}` |
|         - |  9070 | `		/* Assemble method signature only */` |
|    137449 |  9071 | `		doBody = FALSE;` |
|     68722 |  9072 | `	}` |
|   2394301 |  9073 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  9074 | `		/* Syntax error */` |
|       ! 0 |  9075 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  9076 | `		if( rc == SXERR_ABORT ){` |
|         - |  9077 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9078 | `			return SXERR_ABORT;` |
|         - |  9079 | `		}` |
|       ! 0 |  9080 | `		goto Synchronize;` |
|         - |  9081 | `	}` |
|         - |  9082 | `	/* Allocate a new class_method instance */` |
|   2394301 |  9083 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2394301 |  9084 | `	if( pMeth == 0 ){` |
|       ! 0 |  9085 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9086 | `		return SXERR_ABORT;` |
|         - |  9087 | `	}` |
|   2394301 |  9088 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2394301 |  9089 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2394301 |  9090 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9091 | `		return SXERR_ABORT;` |
|         - |  9092 | `	}` |
|         - |  9093 | `	/* Jump the left parenthesis '(' */` |
|   2394301 |  9094 | `	pGen->pIn++;` |
|   2394301 |  9095 | `	pEnd = 0; /* cc warning */` |
|         - |  9096 | `	/* Delimit the method signature */` |
|   2394301 |  9097 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2394301 |  9098 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9099 | `		/* Syntax error */` |
|         3 |  9100 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  9101 | `		if( rc == SXERR_ABORT ){` |
|         - |  9102 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9103 | `			return SXERR_ABORT;` |
|         - |  9104 | `		}` |
|         3 |  9105 | `		goto Synchronize;` |
|         - |  9106 | `	}` |
|         - |  9107 | `	{` |
|   2394299 |  9108 | `		int bIsCtor = 0;` |
|   2394299 |  9109 | `		int bAbstractCtor = 0;` |
|   2394294 |  9110 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1397648 |  9111 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2312149 |  9112 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    164305 |  9113 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  9114 | `				bAbstractCtor = 1;` |
|         2 |  9115 | `			}else{` |
|    164303 |  9116 | `				bIsCtor = 1;` |
|         - |  9117 | `			}` |
|     82150 |  9118 | `		}` |
|   2394299 |  9119 | `		if( pGen->pIn < pEnd ){` |
|         - |  9120 | `			/* Collect method arguments */` |
|    863105 |  9121 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    863105 |  9122 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9123 | `				return SXERR_ABORT;` |
|         - |  9124 | `			}` |
|    431550 |  9125 | `		}` |
|         - |  9126 | `	}` |
|         - |  9127 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2394299 |  9128 | `	pGen->pIn = &pEnd[1];` |
|         - |  9129 | `	{` |
|   2394299 |  9130 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2394299 |  9131 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  9132 | `			return SXERR_ABORT;` |
|   2394299 |  9133 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  9134 | `			goto Synchronize;` |
|         - |  9135 | `		}` |
|         - |  9136 | `	}` |
|         - |  9137 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  9138 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  9139 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  9140 | `	{` |
|   2394299 |  9141 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  9142 | `		sxu32 i;` |
|   3684969 |  9143 | `		for( i = 0; i < nArg; i++ ){` |
|   1290685 |  9144 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  9145 | `			ph7_class_attr *pAttr;` |
|   1290685 |  9146 | `			sxi32 iAttrFlags = 0;` |
|         - |  9147 | `			int bArgTyped;` |
|   1290685 |  9148 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1290601 |  9149 | `				continue;` |
|         - |  9150 | `			}` |
|         - |  9151 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  9152 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  9153 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  9154 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  9155 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  9156 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  9157 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9158 | `					"Cannot declare variadic promoted property");` |
|         3 |  9159 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9160 | `					return SXERR_ABORT;` |
|         - |  9161 | `				}` |
|         3 |  9162 | `				goto Synchronize;` |
|         - |  9163 | `			}` |
|         - |  9164 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  9165 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  9166 | `			 * appear as an alternative of a union type. */` |
|        87 |  9167 | `			if( bArgTyped ){` |
|       122 |  9168 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  9169 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  9170 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  9171 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  9172 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9173 | `					return SXERR_ABORT;` |
|        83 |  9174 | `				}else if( rc != SXRET_OK ){` |
|         6 |  9175 | `					goto Synchronize;` |
|         - |  9176 | `				}` |
|        37 |  9177 | `			}` |
|         - |  9178 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  9179 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  9180 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9181 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  9182 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9183 | `					return SXERR_ABORT;` |
|         - |  9184 | `				}` |
|         3 |  9185 | `				goto Synchronize;` |
|         - |  9186 | `			}` |
|        81 |  9187 | `			if( bArgTyped ){` |
|        77 |  9188 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  9189 | `			}` |
|        81 |  9190 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  9191 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  9192 | `			}` |
|        81 |  9193 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  9194 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  9195 | `			}` |
|        81 |  9196 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  9197 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  9198 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  9199 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  9200 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9201 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  9202 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9203 | `						return SXERR_ABORT;` |
|         - |  9204 | `					}` |
|         3 |  9205 | `					goto Synchronize;` |
|         - |  9206 | `				}` |
|        24 |  9207 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  9208 | `			}` |
|        79 |  9209 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  9210 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  9211 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  9212 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9213 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  9214 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  9215 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9216 | `						return SXERR_ABORT;` |
|         - |  9217 | `					}` |
|       ! 0 |  9218 | `					goto Synchronize;` |
|         - |  9219 | `				}` |
|         5 |  9220 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  9221 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  9222 | `			}` |
|        79 |  9223 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  9224 | `			if( pAttr == 0 ){` |
|       ! 0 |  9225 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9226 | `				return SXERR_ABORT;` |
|         - |  9227 | `			}` |
|        79 |  9228 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  9229 | `				pAttr->nType = pArg->nType;` |
|        77 |  9230 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  9231 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  9232 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9233 | `					sxu32 k;` |
|        20 |  9234 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9235 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9236 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9237 | `					}` |
|         3 |  9238 | `				}` |
|        36 |  9239 | `			}` |
|        79 |  9240 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  9241 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9242 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9243 | `				return SXERR_ABORT;` |
|         - |  9244 | `			}` |
|        42 |  9245 | `		}` |
|         - |  9246 | `	}` |
|   2394289 |  9247 | `	if( doBody ){` |
|         - |  9248 | `		/* Compile method body */` |
|   2256845 |  9249 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2256845 |  9250 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9251 | `			return SXERR_ABORT;` |
|         - |  9252 | `		}` |
|         - |  9253 | `		/* The cursor sits just past the body's closing brace */` |
|   2256845 |  9254 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1128425 |  9255 | `	}else{` |
|         - |  9256 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    137449 |  9257 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    137449 |  9258 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     68722 |  9259 | `		}` |
|         - |  9260 | `		/* Only method signature is allowed */` |
|    137449 |  9261 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9262 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9263 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9264 | `				if( rc == SXERR_ABORT ){` |
|         - |  9265 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9266 | `					return SXERR_ABORT;` |
|         - |  9267 | `				}` |
|       ! 0 |  9268 | `				return SXERR_CORRUPT;` |
|         - |  9269 | `			}` |
|         - |  9270 | `	}` |
|         - |  9271 | `	/* All done,install the method */` |
|   2394289 |  9272 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2394289 |  9273 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9274 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9275 | `		return SXERR_ABORT;` |
|         - |  9276 | `	}` |
|   2394289 |  9277 | `	return SXRET_OK;` |
|         6 |  9278 | `Synchronize:` |
|         - |  9279 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9280 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9281 | `		pGen->pIn++;` |
|         4 |  9282 | `	}` |
|        16 |  9283 | `	return SXERR_CORRUPT;` |
|   1197153 |  9284 | `}` |
|         - |  9285 | `/*` |
|         - |  9286 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9287 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9288 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9289 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9290 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9291 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9292 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9293 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9294 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9295 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9296 | `` * implicit `$value` formal.`` |
|         - |  9297 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9298 | ` */` |
|         - |  9299 | `/*` |
|         - |  9300 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9301 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9302 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9303 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9304 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9305 | ` */` |
|        94 |  9306 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9307 | `{` |
|         - |  9308 | `	SyToken *p;` |
|       345 |  9309 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9310 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9311 | `			continue;` |
|         - |  9312 | `		}` |
|         - |  9313 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9314 | `		if( p + 3 < pEnd` |
|        80 |  9315 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9316 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9317 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9318 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9319 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9320 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9321 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9322 | `			return 1;` |
|         - |  9323 | `		}` |
|         - |  9324 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9325 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9326 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9327 | `		if( p > pStart` |
|        26 |  9328 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9329 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9330 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9331 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9332 | `			return 1;` |
|         - |  9333 | `		}` |
|        15 |  9334 | `	}` |
|        43 |  9335 | `	return 0;` |
|        48 |  9336 | `}` |
|         - |  9337 | `/*` |
|         - |  9338 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9339 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9340 | ` */` |
|       990 |  9341 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9342 | `{` |
|      1167 |  9343 | `	return p + 6 < pEnd` |
|       671 |  9344 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9345 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9346 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9347 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9348 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9349 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9350 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9351 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9352 | `	 && p[5].sData.nByte == 3` |
|         8 |  9353 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9354 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9355 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9356 | `}` |
|         - |  9357 | `/*` |
|         - |  9358 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9359 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9360 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9361 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9362 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9363 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9364 | ` * or SXERR_MEM.` |
|         - |  9365 | ` */` |
|         4 |  9366 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9367 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9368 | `{` |
|         5 |  9369 | `	SyToken *p = pStart;` |
|        35 |  9370 | `	while( p < pEnd ){` |
|        31 |  9371 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9372 | `			SyToken sTok;` |
|         - |  9373 | `			char zName[384];` |
|         - |  9374 | `			sxu32 nName;` |
|         - |  9375 | `			char *zDup;` |
|         - |  9376 | ``			/* `parent` `::` */`` |
|         5 |  9377 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9378 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9379 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9380 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9381 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9382 | `			if( zDup == 0 ){` |
|       ! 0 |  9383 | `				return SXERR_MEM;` |
|         - |  9384 | `			}` |
|         5 |  9385 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9386 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9387 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9388 | `			sTok.pUserData = 0;` |
|         5 |  9389 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9390 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9391 | `			continue;` |
|         - |  9392 | `		}` |
|        27 |  9393 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9394 | `		p++;` |
|         1 |  9395 | `	}` |
|         5 |  9396 | `	return SXRET_OK;` |
|         3 |  9397 | `}` |
|        94 |  9398 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9399 | `{` |
|        95 |  9400 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9401 | `	sxi32 rc;` |
|        95 |  9402 | `	int bRefsSelf = 0;` |
|        95 |  9403 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9404 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9405 | `		char zHook[384];` |
|         - |  9406 | `		SyString sHookName;` |
|         - |  9407 | `		ph7_class_method *pMeth;` |
|         - |  9408 | `		int bGet;` |
|       159 |  9409 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9410 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9411 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9412 | `			continue;` |
|         - |  9413 | `		}` |
|       145 |  9414 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9415 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9416 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9417 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9418 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9419 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9420 | `				return SXERR_ABORT;` |
|         - |  9421 | `			}` |
|       ! 0 |  9422 | `			return SXERR_CORRUPT;` |
|         - |  9423 | `		}` |
|       145 |  9424 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9425 | `			goto HookSyntax;` |
|         - |  9426 | `		}` |
|       144 |  9427 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9428 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9429 | `			bGet = 1;` |
|       106 |  9430 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9431 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9432 | `			bGet = 0;` |
|        34 |  9433 | `		}else{` |
|       ! 0 |  9434 | `			goto HookSyntax;` |
|         - |  9435 | `		}` |
|       145 |  9436 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9437 | `		sHookName.zString = zHook;` |
|       217 |  9438 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9439 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9440 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9441 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9442 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9443 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9444 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9445 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9446 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9447 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9448 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9449 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9450 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9451 | `					return SXERR_ABORT;` |
|         - |  9452 | `				}` |
|       ! 0 |  9453 | `				return SXERR_CORRUPT;` |
|         - |  9454 | `			}` |
|        15 |  9455 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9456 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9457 | `			if( pMeth == 0 ){` |
|       ! 0 |  9458 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9459 | `				return SXERR_ABORT;` |
|         - |  9460 | `			}` |
|        15 |  9461 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9462 | `			if( !bGet ){` |
|         - |  9463 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9464 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9465 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9466 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9467 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9468 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9469 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9470 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9471 | `				if( zVName == 0 ){` |
|       ! 0 |  9472 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9473 | `					return SXERR_ABORT;` |
|         - |  9474 | `				}` |
|         7 |  9475 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9476 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9477 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9478 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9479 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9480 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9481 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9482 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9483 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9484 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9485 | `				}` |
|         7 |  9486 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9487 | `			}` |
|        15 |  9488 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9489 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9490 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9491 | `				return SXERR_ABORT;` |
|         - |  9492 | `			}` |
|        15 |  9493 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9494 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9495 | `		}` |
|       130 |  9496 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9497 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9498 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9499 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9500 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9501 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9502 | `				return SXERR_ABORT;` |
|         - |  9503 | `			}` |
|       ! 0 |  9504 | `			return SXERR_CORRUPT;` |
|         - |  9505 | `		}` |
|       131 |  9506 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9507 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9508 | `		if( pMeth == 0 ){` |
|       ! 0 |  9509 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9510 | `			return SXERR_ABORT;` |
|         - |  9511 | `		}` |
|       131 |  9512 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9513 | `		if( !bGet ){` |
|         - |  9514 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9515 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9516 | `				SyToken *pRp = 0;` |
|        17 |  9517 | `				pGen->pIn++;` |
|        17 |  9518 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9519 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9520 | `					goto HookSyntax;` |
|         - |  9521 | `				}` |
|        17 |  9522 | `				if( pGen->pIn < pRp ){` |
|        17 |  9523 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9524 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9525 | `						return SXERR_ABORT;` |
|         - |  9526 | `					}` |
|         8 |  9527 | `				}` |
|        17 |  9528 | `				pGen->pIn = &pRp[1];` |
|         8 |  9529 | `			}` |
|        61 |  9530 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9531 | `				/* Implicit $value formal */` |
|         - |  9532 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9533 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9534 | `				if( zVName == 0 ){` |
|       ! 0 |  9535 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9536 | `					return SXERR_ABORT;` |
|         - |  9537 | `				}` |
|        45 |  9538 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9539 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9540 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9541 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9542 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9543 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9544 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9545 | `			}` |
|        30 |  9546 | `		}` |
|       165 |  9547 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9548 | `			/* Block body */` |
|        69 |  9549 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9550 | `			SyToken *pCloser = 0;` |
|        69 |  9551 | `			int bParentCall = 0;` |
|        69 |  9552 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9553 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9554 | `				SyToken *pScan;` |
|       753 |  9555 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9556 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9557 | `						bParentCall = 1;` |
|         3 |  9558 | `						break;` |
|         - |  9559 | `					}` |
|       343 |  9560 | `				}` |
|        34 |  9561 | `			}` |
|        69 |  9562 | `			if( bParentCall ){` |
|         - |  9563 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9564 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9565 | `				 * hook method), then continue past the original body. */` |
|         - |  9566 | `				SySet sBody;` |
|         3 |  9567 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9568 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9569 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9570 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9571 | `					SySetRelease(&sBody);` |
|       ! 0 |  9572 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9573 | `					return SXERR_ABORT;` |
|         - |  9574 | `				}` |
|         3 |  9575 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9576 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9577 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9578 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9579 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9580 | `				SySetRelease(&sBody);` |
|         3 |  9581 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9582 | `					return SXERR_ABORT;` |
|         - |  9583 | `				}` |
|         3 |  9584 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9585 | `			}else{` |
|        67 |  9586 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9587 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9588 | `					return SXERR_ABORT;` |
|         - |  9589 | `				}` |
|        67 |  9590 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9591 | `			}` |
|        69 |  9592 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9593 | `				bRefsSelf = 1;` |
|         9 |  9594 | `			}` |
|       128 |  9595 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9596 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9597 | `			GenBlock *pBlock;` |
|         - |  9598 | `			SySet *pInstrContainer;` |
|         - |  9599 | `			SyToken *pBodyStart;` |
|         - |  9600 | `			SyToken *pExprEnd;` |
|        63 |  9601 | `			SyToken *pSavedEnd = 0;` |
|         - |  9602 | `			SySet sBody;` |
|        63 |  9603 | `			int bParentCall = 0;` |
|        63 |  9604 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9605 | `			pBodyStart = pGen->pIn;` |
|         - |  9606 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9607 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9608 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9609 | `			 * method on a token copy. */` |
|         - |  9610 | `			{` |
|        63 |  9611 | `				sxi32 iNest = 0;` |
|        63 |  9612 | `				pExprEnd = pBodyStart;` |
|       355 |  9613 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9614 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9615 | `						iNest++;` |
|       351 |  9616 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9617 | `						if( iNest <= 0 ){` |
|       ! 0 |  9618 | `							break;` |
|         - |  9619 | `						}` |
|         9 |  9620 | `						iNest--;` |
|       343 |  9621 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9622 | `						break;` |
|         - |  9623 | `					}` |
|       293 |  9624 | `					pExprEnd++;` |
|         1 |  9625 | `				}` |
|         - |  9626 | `			}` |
|         - |  9627 | `			{` |
|         - |  9628 | `				SyToken *pScan;` |
|       335 |  9629 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9630 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9631 | `						bParentCall = 1;` |
|         3 |  9632 | `						break;` |
|         - |  9633 | `					}` |
|       137 |  9634 | `				}` |
|         - |  9635 | `			}` |
|        63 |  9636 | `			if( bParentCall ){` |
|         3 |  9637 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9638 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9639 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9640 | `					SySetRelease(&sBody);` |
|       ! 0 |  9641 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9642 | `					return SXERR_ABORT;` |
|         - |  9643 | `				}` |
|         3 |  9644 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9645 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9646 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9647 | `			}` |
|        94 |  9648 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9649 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9650 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9651 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9652 | `				return SXERR_ABORT;` |
|         - |  9653 | `			}` |
|        63 |  9654 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9655 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9656 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9657 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9658 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9659 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9660 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9661 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9662 | `			if( bParentCall ){` |
|         3 |  9663 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9664 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9665 | `				SySetRelease(&sBody);` |
|         1 |  9666 | `			}` |
|        63 |  9667 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9668 | `				return SXERR_ABORT;` |
|         - |  9669 | `			}` |
|        63 |  9670 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9671 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9672 | `				bRefsSelf = 1;` |
|        18 |  9673 | `			}` |
|        63 |  9674 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9675 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9676 | `			}` |
|        63 |  9677 | `			if( !bGet ){` |
|         - |  9678 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9679 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9680 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9681 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9682 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9683 | `				bRefsSelf = 1;` |
|         1 |  9684 | `			}` |
|        32 |  9685 | `		}else{` |
|       ! 0 |  9686 | `			goto HookSyntax;` |
|         - |  9687 | `		}` |
|       131 |  9688 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9689 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9690 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9691 | `			return SXERR_ABORT;` |
|         - |  9692 | `		}` |
|       131 |  9693 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9694 | `	}` |
|        95 |  9695 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9696 | `		goto HookSyntax;` |
|         - |  9697 | `	}` |
|        95 |  9698 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9699 | `	if( !bRefsSelf ){` |
|         - |  9700 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9701 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9702 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9703 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9704 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9705 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9706 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9707 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9708 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9709 | `				return SXERR_ABORT;` |
|         - |  9710 | `			}` |
|       ! 0 |  9711 | `			return SXERR_CORRUPT;` |
|         - |  9712 | `		}` |
|        20 |  9713 | `	}` |
|        95 |  9714 | `	return SXRET_OK;` |
|       ! 0 |  9715 | `HookSyntax:` |
|       ! 0 |  9716 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9717 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9718 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9719 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9720 | `		return SXERR_ABORT;` |
|         - |  9721 | `	}` |
|       ! 0 |  9722 | `	return SXERR_CORRUPT;` |
|        48 |  9723 | `}` |
|         - |  9724 | `/*` |
|         - |  9725 | ` * Compile an object interface.` |
|         - |  9726 | ` *  According to the PHP language reference manual` |
|         - |  9727 | ` *   Object Interfaces:` |
|         - |  9728 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9729 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9730 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9731 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9732 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9733 | ` */` |
|     68798 |  9734 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9735 | `{` |
|     68803 |  9736 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9737 | `	ph7_class *pClass,*pBase;` |
|         - |  9738 | `	SyToken *pEnd,*pTmp;` |
|         - |  9739 | `	SyString *pName;` |
|         - |  9740 | `	sxi32 nKwrd;` |
|         - |  9741 | `	sxi32 rc;` |
|         - |  9742 | `	/* Jump the 'interface' keyword */` |
|     68803 |  9743 | `	pGen->pIn++;` |
|         - |  9744 | `	/* Extract interface name */` |
|     68803 |  9745 | `	pName = &pGen->pIn->sData;` |
|         - |  9746 | `	/* Advance the stream cursor */` |
|     68803 |  9747 | `	pGen->pIn++;` |
|         - |  9748 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9749 | `		SyBlob sFQN;` |
|         - |  9750 | `		SyString sFQNStr;` |
|     68803 |  9751 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     68803 |  9752 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     68803 |  9753 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     68803 |  9754 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     68803 |  9755 | `		SyBlobRelease(&sFQN);` |
|         - |  9756 | `	}` |
|     68803 |  9757 | `	if( pClass == 0 ){` |
|       ! 0 |  9758 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9759 | `		return SXERR_ABORT;` |
|         - |  9760 | `	}` |
|     68803 |  9761 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     68803 |  9762 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9763 | `		return SXERR_ABORT;` |
|         - |  9764 | `	}` |
|         - |  9765 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     68803 |  9766 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9767 | `	/* Assume no base class is given */` |
|     68803 |  9768 | `	pBase = 0;` |
|     68803 |  9769 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26727 |  9770 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26727 |  9771 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9772 | `			SyBlob sResolved;` |
|         - |  9773 | `			SyString sBaseName;` |
|         - |  9774 | `			sxu32 nRefLine;` |
|         - |  9775 | `			/* Extract base interface */` |
|     26727 |  9776 | `			pGen->pIn++;` |
|     26727 |  9777 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26727 |  9778 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26727 |  9779 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9780 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9781 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9782 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9783 | `					pName);` |
|       ! 0 |  9784 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9785 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9786 | `					return SXERR_ABORT;` |
|         - |  9787 | `				}` |
|       ! 0 |  9788 | `				return SXRET_OK;` |
|         - |  9789 | `			}` |
|     40088 |  9790 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     26722 |  9791 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26727 |  9792 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9793 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9794 | `			/* Only interfaces is allowed */` |
|     26727 |  9795 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9796 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9797 | `			}` |
|     26727 |  9798 | `			if( pBase == 0 ){` |
|       ! 0 |  9799 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9800 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9801 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9802 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9803 | `					return SXERR_ABORT;` |
|         - |  9804 | `				}` |
|       ! 0 |  9805 | `			}` |
|     26727 |  9806 | `			SyBlobRelease(&sResolved);` |
|     13361 |  9807 | `		}` |
|     13361 |  9808 | `	}` |
|     68803 |  9809 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9810 | `		/* Syntax error */` |
|       ! 0 |  9811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9812 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9813 | `		if( rc == SXERR_ABORT ){` |
|         - |  9814 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9815 | `			return SXERR_ABORT;` |
|         - |  9816 | `		}` |
|       ! 0 |  9817 | `		return SXRET_OK;` |
|         - |  9818 | `	}` |
|     68803 |  9819 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     68803 |  9820 | `	pEnd = 0; /* cc warning */` |
|         - |  9821 | `	/* Delimit the interface body */` |
|     68803 |  9822 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     68803 |  9823 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9824 | `		/* Syntax error */` |
|       ! 0 |  9825 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9826 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9827 | `		if( rc == SXERR_ABORT ){` |
|         - |  9828 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9829 | `			return SXERR_ABORT;` |
|         - |  9830 | `		}` |
|       ! 0 |  9831 | `		return SXRET_OK;` |
|         - |  9832 | `	}` |
|         - |  9833 | `	/* The delimiter token is the interface body's closing brace */` |
|     68803 |  9834 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9835 | `	/* Swap token stream */` |
|     68803 |  9836 | `	pTmp = pGen->pEnd;` |
|     68803 |  9837 | `	pGen->pEnd = pEnd;` |
|         - |  9838 | `	/* Start the parse process` |
|         - |  9839 | `	 * Note (According to the PHP reference manual):` |
|         - |  9840 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9841 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9842 | `	 */` |
|    126013 |  9843 | `	for(;;){` |
|         - |  9844 | `		/* Jump leading/trailing semi-colons */` |
|    435263 |  9845 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    183233 |  9846 | `			pGen->pIn++;` |
|         5 |  9847 | `		}` |
|    252035 |  9848 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9849 | `			/* End of interface body */` |
|     68799 |  9850 | `			break;` |
|         - |  9851 | `		}` |
|         - |  9852 | `		/* Bind a directly-preceding docblock to this member */` |
|    183241 |  9853 | `		GenStateSetPendingDoc(&(*pGen));` |
|    183241 |  9854 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9855 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9856 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9857 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9858 | `			if( rc == SXERR_ABORT ){` |
|         - |  9859 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9860 | `				return SXERR_ABORT;` |
|         - |  9861 | `			}` |
|       ! 0 |  9862 | `			goto done;` |
|         - |  9863 | `		}` |
|         - |  9864 | `		/* Extract the current keyword */` |
|    183241 |  9865 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    183241 |  9866 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9867 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9868 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9869 | `			const char *zKind = "member";` |
|         3 |  9870 | `			SyString *pMemberName = 0;` |
|         3 |  9871 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9872 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9873 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9874 | `					zKind = "constant";` |
|         3 |  9875 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9876 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9877 | `					}` |
|         1 |  9878 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9879 | `					zKind = "method";` |
|       ! 0 |  9880 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9881 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9882 | `					}` |
|       ! 0 |  9883 | `				}` |
|         1 |  9884 | `			}` |
|         3 |  9885 | `			if( pMemberName ){` |
|         4 |  9886 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9887 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9888 | `			}else{` |
|       ! 0 |  9889 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9890 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9891 | `			}` |
|         3 |  9892 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9893 | `				return SXERR_ABORT;` |
|         - |  9894 | `			}` |
|         3 |  9895 | `			goto done;` |
|         - |  9896 | `		}` |
|    183239 |  9897 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9898 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9899 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9900 | `			if( rc == SXERR_ABORT ){` |
|         - |  9901 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9902 | `				return SXERR_ABORT;` |
|         - |  9903 | `			}` |
|       ! 0 |  9904 | `			goto done;` |
|         - |  9905 | `		}` |
|    183239 |  9906 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9907 | `			/* Advance the stream cursor */` |
|    129797 |  9908 | `			pGen->pIn++;` |
|    129792 |  9909 | `			if( pGen->pIn < pGen->pEnd` |
|    129797 |  9910 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    129792 |  9911 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9912 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9913 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9914 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9915 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9916 | `				 * hooked properties" error). */` |
|       ! 0 |  9917 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9918 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9919 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9920 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9921 | `						return SXERR_ABORT;` |
|         - |  9922 | `					}` |
|       ! 0 |  9923 | `					goto done;` |
|         - |  9924 | `				}` |
|       ! 0 |  9925 | `				continue;` |
|         - |  9926 | `			}` |
|    129797 |  9927 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9928 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9929 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9930 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9931 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9932 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9933 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9934 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9935 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9936 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9937 | `							return SXERR_ABORT;` |
|         - |  9938 | `						}` |
|       ! 0 |  9939 | `						goto done;` |
|         - |  9940 | `					}` |
|       ! 0 |  9941 | `					continue;` |
|         - |  9942 | `				}` |
|       ! 0 |  9943 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9944 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9945 | `				if( rc == SXERR_ABORT ){` |
|         - |  9946 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9947 | `					return SXERR_ABORT;` |
|         - |  9948 | `				}` |
|       ! 0 |  9949 | `				goto done;` |
|         - |  9950 | `			}` |
|    129797 |  9951 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    129797 |  9952 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9953 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9954 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9955 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9956 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9957 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9958 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9959 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9960 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9961 | `							return SXERR_ABORT;` |
|         - |  9962 | `						}` |
|       ! 0 |  9963 | `						goto done;` |
|         - |  9964 | `					}` |
|         5 |  9965 | `					continue;` |
|         - |  9966 | `				}` |
|       ! 0 |  9967 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9968 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9969 | `				if( rc == SXERR_ABORT ){` |
|         - |  9970 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9971 | `					return SXERR_ABORT;` |
|         - |  9972 | `				}` |
|       ! 0 |  9973 | `				goto done;` |
|         - |  9974 | `			}` |
|     64894 |  9975 | `		}` |
|    183235 |  9976 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9977 | `			/* Parse constant */` |
|     53443 |  9978 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53443 |  9979 | `			if( rc != SXRET_OK ){` |
|         3 |  9980 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9981 | `					return SXERR_ABORT;` |
|         - |  9982 | `				}` |
|         3 |  9983 | `				goto done;` |
|         - |  9984 | `			}` |
|     26723 |  9985 | `		}else{` |
|    129797 |  9986 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    129797 |  9987 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9988 | `				/* Static method,record that */` |
|     11453 |  9989 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9990 | `				/* Advance the stream cursor */` |
|     11453 |  9991 | `				pGen->pIn++;` |
|     11448 |  9992 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11453 |  9993 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9994 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9995 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9996 | `						if( rc == SXERR_ABORT ){` |
|         - |  9997 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9998 | `							return SXERR_ABORT;` |
|         - |  9999 | `						}` |
|       ! 0 | 10000 | `						goto done;` |
|         - | 10001 | `				}` |
|      5724 | 10002 | `			}` |
|         - | 10003 | `			/* Process method signature (no body for interface methods) */` |
|    129797 | 10004 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    129797 | 10005 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 10006 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10007 | `					return SXERR_ABORT;` |
|         - | 10008 | `				}` |
|       ! 0 | 10009 | `				goto done;` |
|         - | 10010 | `			}` |
|         - | 10011 | `		}` |
|         5 | 10012 | `	}` |
|         - | 10013 | `	/* Install the interface */` |
|     68799 | 10014 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     68799 | 10015 | `	if( rc == SXRET_OK && pBase ){` |
|         - | 10016 | `		/* Inherit from the base interface */` |
|     26727 | 10017 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13361 | 10018 | `	}` |
|     68799 | 10019 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10020 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10021 | `		return SXERR_ABORT;` |
|         - | 10022 | `	}` |
|     34397 | 10023 | `done:` |
|         - | 10024 | `	/* Point beyond the interface body */` |
|     68803 | 10025 | `	pGen->pIn  = &pEnd[1];` |
|     68803 | 10026 | `	pGen->pEnd = pTmp;` |
|     68803 | 10027 | `	return PH7_OK;` |
|     34404 | 10028 | `}` |
|         - | 10029 | `/*` |
|         - | 10030 | ` * Compile a user-defined class.` |
|         - | 10031 | ` * According to the PHP language reference manual` |
|         - | 10032 | ` *  class` |
|         - | 10033 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - | 10034 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - | 10035 | ` *  of the properties and methods belonging to the class.` |
|         - | 10036 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - | 10037 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - | 10038 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - | 10039 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - | 10040 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - | 10041 | ` *  (called "methods").` |
|         - | 10042 | ` */` |
|         - | 10043 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - | 10044 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - | 10045 | `struct TraitUseEntry {` |
|         - | 10046 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - | 10047 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - | 10048 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - | 10049 | `};` |
|         - | 10050 | `/*` |
|         - | 10051 | ` * Validate that methods implementing interface contracts have compatible` |
|         - | 10052 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - | 10053 | ` */` |
|    352850 | 10054 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10055 | `{` |
|         - | 10056 | `	ph7_class **apIface;` |
|         - | 10057 | `	sxu32 nIface,i;` |
|         - | 10058 | `	sxi32 rc;` |
|    352855 | 10059 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 | 10060 | `		return SXRET_OK;` |
|         - | 10061 | `	}` |
|    352855 | 10062 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    352855 | 10063 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    708083 | 10064 | `	for(i = 0; i < nIface; i++){` |
|    355233 | 10065 | `		ph7_class *pIface = apIface[i];` |
|         - | 10066 | `		SyHashEntry *pEntry;` |
|    355233 | 10067 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1023619 | 10068 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    668391 | 10069 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - | 10070 | `			ph7_class_method *pImplMeth;` |
|    668391 | 10071 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - | 10072 | `			/* Find the implementing method in the class */` |
|    668391 | 10073 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    668391 | 10074 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 | 10075 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - | 10076 | `			}` |
|         - | 10077 | `			/* Check visibility: interface methods must be implemented as public */` |
|    668373 | 10078 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 | 10079 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10080 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 | 10081 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 | 10082 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10083 | `					return SXERR_ABORT;` |
|         - | 10084 | `				}` |
|         1 | 10085 | `			}` |
|         - | 10086 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - | 10087 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - | 10088 | `			 */` |
|         - | 10089 | `			{` |
|    668373 | 10090 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    668373 | 10091 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    668373 | 10092 | `				int sigError = 0;` |
|    668373 | 10093 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 | 10094 | `					sigError = 1;` |
|    668372 | 10095 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - | 10096 | `					/* Extra parameters must all have default values */` |
|      3825 | 10097 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 10098 | `					sxu32 k;` |
|      7643 | 10099 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3825 | 10100 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 10101 | `							sigError = 1;` |
|         3 | 10102 | `							break;` |
|         - | 10103 | `						}` |
|      1914 | 10104 | `					}` |
|      1910 | 10105 | `				}` |
|    668373 | 10106 | `				if( sigError ){` |
|         - | 10107 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 10108 | `					ph7_vm_func_arg *aArgs;` |
|         - | 10109 | `					sxu32 j;` |
|         6 | 10110 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 10111 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 10112 | `					/* Build implementing method signature */` |
|         6 | 10113 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 10114 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 10115 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 10116 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 10117 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10118 | `					}` |
|         - | 10119 | `					/* Build interface method signature */` |
|         6 | 10120 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 10121 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 10122 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 10123 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 10124 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10125 | `					}` |
|         8 | 10126 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10127 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 10128 | `						&pClass->sName,pMName,` |
|         4 | 10129 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 10130 | `						&pIface->sName,pMName,` |
|         4 | 10131 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 10132 | `					SyBlobRelease(&sImplSig);` |
|         6 | 10133 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 10134 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10135 | `						return SXERR_ABORT;` |
|         - | 10136 | `					}` |
|         2 | 10137 | `				}` |
|         - | 10138 | `			}` |
|         5 | 10139 | `		}` |
|    177619 | 10140 | `	}` |
|    352855 | 10141 | `	return SXRET_OK;` |
|    176430 | 10142 | `}` |
|         - | 10143 | `/*` |
|         - | 10144 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 10145 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 10146 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 10147 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 10148 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 10149 | ` * means that specific hook is still missing.` |
|         - | 10150 | ` */` |
|        38 | 10151 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 10152 | `{` |
|         - | 10153 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 10154 | `	ph7_class_attr *pProp;` |
|        38 | 10155 | `	if( pMName->nByte <= nPfx` |
|        27 | 10156 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 10157 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 10158 | `		return 0; /* not a hook stub */` |
|         - | 10159 | `	}` |
|         7 | 10160 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 10161 | `	return pProp != 0` |
|         6 | 10162 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 10163 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 10164 | `}` |
|         - | 10165 | `/*` |
|         - | 10166 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 10167 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 10168 | ` */` |
|        16 | 10169 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 10170 | `{` |
|         - | 10171 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 10172 | `	if( pMName->nByte > nPfx` |
|        12 | 10173 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 10174 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 10175 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 10176 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 10177 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 10178 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 10179 | `		return;` |
|         - | 10180 | `	}` |
|        20 | 10181 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 10182 | `}` |
|         - | 10183 | `/*` |
|         - | 10184 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 10185 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 10186 | ` */` |
|    352850 | 10187 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10188 | `{` |
|         - | 10189 | `	ph7_class_method *pMeth;` |
|         - | 10190 | `	SyHashEntry *pEntry;` |
|         - | 10191 | `	sxu32 nAbstract;` |
|         - | 10192 | `	SyBlob sMsg;` |
|         - | 10193 | `	sxi32 rc;` |
|         - | 10194 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    352855 | 10195 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15321 | 10196 | `		return SXRET_OK;` |
|         - | 10197 | `	}` |
|         - | 10198 | `	/* Count abstract methods */` |
|    337539 | 10199 | `	nAbstract = 0;` |
|    337539 | 10200 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   4996468 | 10201 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4490167 | 10202 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4490167 | 10203 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 10204 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 10205 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10206 | `			}` |
|        20 | 10207 | `			nAbstract++;` |
|         8 | 10208 | `		}` |
|         5 | 10209 | `	}` |
|    337539 | 10210 | `	if( nAbstract == 0 ){` |
|    337525 | 10211 | `		return SXRET_OK;` |
|         - | 10212 | `	}` |
|         - | 10213 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 10214 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 10215 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 10216 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 10217 | `		&pClass->sName,nAbstract,` |
|         7 | 10218 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 10219 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 10220 | `	/* Second pass: list methods with origins */` |
|         - | 10221 | `	{` |
|        18 | 10222 | `		sxu32 nListed = 0;` |
|        18 | 10223 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 10224 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 10225 | `			ph7_class *pOrigin = 0;` |
|         - | 10226 | `			SyString *pMName;` |
|        22 | 10227 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 10228 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10229 | `				continue;` |
|         - | 10230 | `			}` |
|        20 | 10231 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10232 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10233 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10234 | `			}` |
|        20 | 10235 | `			if( nListed > 0 ){` |
|         3 | 10236 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10237 | `			}` |
|         - | 10238 | `			/* Find the origin of this abstract method.` |
|         - | 10239 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10240 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10241 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10242 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10243 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10244 | `			 * class's namespace.` |
|         - | 10245 | `			 */` |
|         - | 10246 | `			{` |
|         - | 10247 | `				ph7_class **apIface;` |
|         - | 10248 | `				ph7_class **apTrait;` |
|         - | 10249 | `				ph7_class *pWalk;` |
|         - | 10250 | `				sxu32 i;` |
|         - | 10251 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10252 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10253 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10254 | `				 */` |
|        20 | 10255 | `				if( pClass->pBase ){` |
|        11 | 10256 | `					pWalk = pClass->pBase;` |
|        19 | 10257 | `					while( pWalk ){` |
|         - | 10258 | `						ph7_class_method *pParentMeth;` |
|        13 | 10259 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10260 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10261 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10262 | `							 * in this class's ancestor chain.` |
|         - | 10263 | `							 */` |
|        13 | 10264 | `							int fromIface = 0;` |
|        13 | 10265 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10266 | `							while( pAnc ){` |
|         - | 10267 | `								ph7_class **apPI;` |
|         - | 10268 | `								sxu32 j;` |
|        15 | 10269 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10270 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10271 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10272 | `										fromIface = 1;` |
|        10 | 10273 | `										break;` |
|         - | 10274 | `									}` |
|       ! 0 | 10275 | `								}` |
|        15 | 10276 | `								if( fromIface ) break;` |
|         6 | 10277 | `								pAnc = pAnc->pBase;` |
|         2 | 10278 | `							}` |
|        13 | 10279 | `							if( !fromIface ){` |
|         3 | 10280 | `								pOrigin = pWalk;` |
|         3 | 10281 | `								break;` |
|         - | 10282 | `							}` |
|         4 | 10283 | `						}` |
|        10 | 10284 | `						pWalk = pWalk->pBase;` |
|         2 | 10285 | `					}` |
|         4 | 10286 | `				}` |
|         - | 10287 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10288 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10289 | `				 */` |
|        20 | 10290 | `				if( !pOrigin ){` |
|        18 | 10291 | `					pWalk = pClass;` |
|        40 | 10292 | `					while( pWalk && !pOrigin ){` |
|        26 | 10293 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10294 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10295 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10296 | `							ph7_class *pDeepest = 0;` |
|        28 | 10297 | `							while( pIface ){` |
|        16 | 10298 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10299 | `									pDeepest = pIface;` |
|         6 | 10300 | `								}` |
|        16 | 10301 | `								pIface = pIface->pBase;` |
|         4 | 10302 | `							}` |
|        16 | 10303 | `							if( pDeepest ){` |
|        16 | 10304 | `								pOrigin = pDeepest;` |
|        16 | 10305 | `								break;` |
|         - | 10306 | `							}` |
|       ! 0 | 10307 | `						}` |
|        26 | 10308 | `						pWalk = pWalk->pBase;` |
|         4 | 10309 | `					}` |
|         7 | 10310 | `				}` |
|         - | 10311 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10312 | `				if( !pOrigin ){` |
|         3 | 10313 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10314 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10315 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10316 | `							pOrigin = pClass;` |
|         3 | 10317 | `							break;` |
|         - | 10318 | `						}` |
|       ! 0 | 10319 | `					}` |
|         1 | 10320 | `				}` |
|         - | 10321 | `			}` |
|        20 | 10322 | `			if( pOrigin ){` |
|        20 | 10323 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10324 | `			}else{` |
|         - | 10325 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10326 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10327 | `			}` |
|        20 | 10328 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10329 | `			nListed++;` |
|         4 | 10330 | `		}` |
|         - | 10331 | `	}` |
|        18 | 10332 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10333 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10334 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10335 | `	SyBlobRelease(&sMsg);` |
|        18 | 10336 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10337 | `		return SXERR_ABORT;` |
|         - | 10338 | `	}` |
|        18 | 10339 | `	return SXRET_OK;` |
|    176430 | 10340 | `}` |
|         - | 10341 | `/*` |
|         - | 10342 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10343 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10344 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10345 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10346 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10347 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10348 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10349 | ` */` |
|    398958 | 10350 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10351 | `{` |
|    398963 | 10352 | `	int isAbsolute = 0;` |
|    398963 | 10353 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10354 | `	SyBlob sName;` |
|    398963 | 10355 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4417 | 10356 | `		isAbsolute = 1;` |
|      4417 | 10357 | `		pGen->pIn++;` |
|      2206 | 10358 | `	}` |
|    398963 | 10359 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         9 | 10360 | `		pGen->pIn = pStart;` |
|         9 | 10361 | `		return SXERR_INVALID;` |
|         - | 10362 | `	}` |
|    398957 | 10363 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    398957 | 10364 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    398957 | 10365 | `	pGen->pIn++;` |
|    598449 | 10366 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    199502 | 10367 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10368 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10369 | `		pGen->pIn++;` |
|        16 | 10370 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10371 | `		pGen->pIn++;` |
|         2 | 10372 | `	}` |
|    398957 | 10373 | `	if( isAbsolute ){` |
|      4415 | 10374 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2210 | 10375 | `	}else{` |
|         - | 10376 | `		SyString sRaw;` |
|    394547 | 10377 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    394547 | 10378 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10379 | `	}` |
|    398957 | 10380 | `	SyBlobRelease(&sName);` |
|    398957 | 10381 | `	return SXRET_OK;` |
|    199484 | 10382 | `}` |
|         - | 10383 | `/*` |
|         - | 10384 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10385 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10386 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10387 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10388 | ` * either direction cannot run unbounded.` |
|         - | 10389 | ` */` |
|         - | 10390 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    164294 | 10391 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10392 | `{` |
|         - | 10393 | `	ph7_class **apParent;` |
|         - | 10394 | `	sxu32 n;` |
|    427847 | 10395 | `	while( pInterface ){` |
|    271195 | 10396 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10397 | `			return FALSE;` |
|         - | 10398 | `		}` |
|    305559 | 10399 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     68728 | 10400 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7647 | 10401 | `			return TRUE;` |
|         - | 10402 | `		}` |
|    263553 | 10403 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    263553 | 10404 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10405 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10406 | `				return TRUE;` |
|         - | 10407 | `			}` |
|       ! 0 | 10408 | `		}` |
|    263553 | 10409 | `		pInterface = pInterface->pBase;` |
|    263553 | 10410 | `		iDepth++;` |
|         5 | 10411 | `	}` |
|    156657 | 10412 | `	return FALSE;` |
|     82152 | 10413 | `}` |
|    164294 | 10414 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10415 | `{` |
|    164299 | 10416 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10417 | `}` |
|         - | 10418 | `/*` |
|         - | 10419 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10420 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10421 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10422 | ` */` |
|      7642 | 10423 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10424 | `{` |
|      7651 | 10425 | `	while( pBase ){` |
|        10 | 10426 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10427 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10428 | `			return TRUE;` |
|         - | 10429 | `		}` |
|        10 | 10430 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10431 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10432 | `			return TRUE;` |
|         - | 10433 | `		}` |
|         5 | 10434 | `		pBase = pBase->pBase;` |
|         1 | 10435 | `	}` |
|      7643 | 10436 | `	return FALSE;` |
|      3826 | 10437 | `}` |
|         - | 10438 | `/*` |
|         - | 10439 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10440 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10441 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10442 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10443 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10444 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10445 | ` * pClass->aEnumCases for cases().` |
|         - | 10446 | ` */` |
|      7674 | 10447 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10448 | `{` |
|      7679 | 10449 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10450 | `	SySet *pInstrContainer;` |
|         - | 10451 | `	ph7_class_attr *pCase;` |
|         - | 10452 | `	SyString *pName;` |
|         - | 10453 | `	sxi32 rc;` |
|      7679 | 10454 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7679 | 10455 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10456 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10457 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10458 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10459 | `			return SXERR_ABORT;` |
|         - | 10460 | `		}` |
|       ! 0 | 10461 | `		goto Synchronize;` |
|         - | 10462 | `	}` |
|      7679 | 10463 | `	pName = &pGen->pIn->sData;` |
|         - | 10464 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7679 | 10465 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10466 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10467 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10468 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10469 | `			return SXERR_ABORT;` |
|         - | 10470 | `		}` |
|       ! 0 | 10471 | `		goto Synchronize;` |
|         - | 10472 | `	}` |
|      7679 | 10473 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10474 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7679 | 10475 | `	if( pCase == 0 ){` |
|       ! 0 | 10476 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10477 | `		return SXERR_ABORT;` |
|         - | 10478 | `	}` |
|      7679 | 10479 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7679 | 10480 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10481 | `		return SXERR_ABORT;` |
|         - | 10482 | `	}` |
|      7679 | 10483 | `	pGen->pIn++; /* Jump the case name */` |
|      7679 | 10484 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7665 | 10485 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10486 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10487 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10488 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10489 | `				return SXERR_ABORT;` |
|         - | 10490 | `			}` |
|         6 | 10491 | `			goto Synchronize;` |
|         - | 10492 | `		}` |
|      7661 | 10493 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10494 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10495 | `		 * (same technique as class constants). */` |
|      7661 | 10496 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7661 | 10497 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7661 | 10498 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7661 | 10499 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10500 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10501 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10502 | `		}` |
|      7661 | 10503 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7661 | 10504 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7661 | 10505 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10506 | `			return SXERR_ABORT;` |
|         - | 10507 | `		}` |
|      3833 | 10508 | `	}else{` |
|        17 | 10509 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10510 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10511 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10512 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10513 | `				return SXERR_ABORT;` |
|         - | 10514 | `			}` |
|       ! 0 | 10515 | `			goto Synchronize;` |
|         - | 10516 | `		}` |
|         - | 10517 | `	}` |
|      7675 | 10518 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7675 | 10519 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10520 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10521 | `		return SXERR_ABORT;` |
|         - | 10522 | `	}` |
|      7675 | 10523 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7675 | 10524 | `	return SXRET_OK;` |
|         2 | 10525 | `Synchronize:` |
|         - | 10526 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10527 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10528 | `		pGen->pIn++;` |
|         2 | 10529 | `	}` |
|         6 | 10530 | `	return SXERR_CORRUPT;` |
|      3842 | 10531 | `}` |
|         - | 10532 | `/*` |
|         - | 10533 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10534 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10535 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10536 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10537 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10538 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10539 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10540 | ` */` |
|      3840 | 10541 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10542 | `{` |
|         - | 10543 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10544 | `	const char *zBack;` |
|         - | 10545 | `	SySet sToken;` |
|         - | 10546 | `	char *zSrc;` |
|         - | 10547 | `	sxu32 nSrc,nMax;` |
|      3845 | 10548 | `	sxi32 rc = SXRET_OK;` |
|      3845 | 10549 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3840 | 10550 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3845 | 10551 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3845 | 10552 | `	if( zSrc == 0 ){` |
|       ! 0 | 10553 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10554 | `		return SXERR_ABORT;` |
|         - | 10555 | `	}` |
|      3845 | 10556 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3845 | 10557 | `	if( pClass->nEnumBacking != 0 ){` |
|      5747 | 10558 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10559 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10560 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10561 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1914 | 10562 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1919 | 10563 | `	}else{` |
|        21 | 10564 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10565 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10566 | `	}` |
|      3845 | 10567 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3845 | 10568 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3845 | 10569 | `	pSaveIn = pGen->pIn;` |
|      3845 | 10570 | `	pSaveEnd = pGen->pEnd;` |
|      3845 | 10571 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3845 | 10572 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15341 | 10573 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11501 | 10574 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10575 | `	}` |
|      3845 | 10576 | `	pGen->pIn = pSaveIn;` |
|      3845 | 10577 | `	pGen->pEnd = pSaveEnd;` |
|      3845 | 10578 | `	SySetRelease(&sToken);` |
|      3845 | 10579 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1925 | 10580 | `}` |
|         - | 10581 | `/*` |
|         - | 10582 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10583 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10584 | ` */` |
|         - | 10585 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10586 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10587 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10588 | `};` |
|         - | 10589 | `/*` |
|         - | 10590 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10591 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10592 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10593 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10594 | ` * and before the class is installed.` |
|         - | 10595 | ` */` |
|      3840 | 10596 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10597 | `{` |
|         - | 10598 | `	SyHashEntry *pEntry;` |
|         - | 10599 | `	sxi32 rc;` |
|         - | 10600 | `	sxu32 n;` |
|         - | 10601 | `	/* php: "Enum %s cannot include properties" */` |
|      3845 | 10602 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11519 | 10603 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7681 | 10604 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7681 | 10605 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10606 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10607 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10608 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10609 | `				return SXERR_ABORT;` |
|         - | 10610 | `			}` |
|         3 | 10611 | `			break;` |
|         - | 10612 | `		}` |
|         5 | 10613 | `	}` |
|         - | 10614 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     53765 | 10615 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     74880 | 10616 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     49925 | 10617 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10618 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10619 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10620 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10621 | `				return SXERR_ABORT;` |
|         - | 10622 | `			}` |
|       ! 0 | 10623 | `		}` |
|     24965 | 10624 | `	}` |
|         - | 10625 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10626 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10627 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10628 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10629 | `	{` |
|         - | 10630 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10631 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10632 | `		ph7_class_attr *pAttr;` |
|      3845 | 10633 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10634 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3845 | 10635 | `		if( pAttr == 0 ){` |
|       ! 0 | 10636 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10637 | `			return SXERR_ABORT;` |
|         - | 10638 | `		}` |
|      3845 | 10639 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3845 | 10640 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3845 | 10641 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3845 | 10642 | `		if( pClass->nEnumBacking != 0 ){` |
|      3833 | 10643 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10644 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3833 | 10645 | `			if( pAttr == 0 ){` |
|       ! 0 | 10646 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10647 | `				return SXERR_ABORT;` |
|         - | 10648 | `			}` |
|      3833 | 10649 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3833 | 10650 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10651 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10652 | `			}else{` |
|      3827 | 10653 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10654 | `			}` |
|      3833 | 10655 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1914 | 10656 | `		}` |
|         - | 10657 | `	}` |
|      3845 | 10658 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1925 | 10659 | `}` |
|         - | 10660 | `/*` |
|         - | 10661 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10662 | ` *` |
|         - | 10663 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10664 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10665 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10666 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10667 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10668 | ` * implements, body, install) is shared by both paths.` |
|         - | 10669 | ` */` |
|    352894 | 10670 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10671 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10672 | `{` |
|    352899 | 10673 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10674 | `	ph7_class *pClass,*pBase;` |
|         - | 10675 | `	SyToken *pEnd,*pTmp;` |
|         - | 10676 | `	sxi32 iProtection;` |
|         - | 10677 | `	SySet aInterfaces;` |
|         - | 10678 | `	SySet aUseEntries;` |
|         - | 10679 | `	sxi32 iAttrflags;` |
|         - | 10680 | `	SyString *pName;` |
|         - | 10681 | `	sxi32 nKwrd;` |
|         - | 10682 | `	sxi32 rc;` |
|         - | 10683 | `	/* Jump the 'class' keyword */` |
|    352899 | 10684 | `	pGen->pIn++;` |
|    352899 | 10685 | `	if( pAnonName ){` |
|         - | 10686 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10687 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10688 | `		 * then use the synthesized name. */` |
|        32 | 10689 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10690 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10691 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10692 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10693 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10694 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10695 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10696 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10697 | `		}` |
|        32 | 10698 | `		pName = pAnonName;` |
|        32 | 10699 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10700 | `	}else{` |
|    352871 | 10701 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10702 | `			/* Syntax error */` |
|       ! 0 | 10703 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10704 | `			if( rc == SXERR_ABORT ){` |
|         - | 10705 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10706 | `				return SXERR_ABORT;` |
|         - | 10707 | `			}` |
|         - | 10708 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10709 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10710 | `				pGen->pIn++;` |
|       ! 0 | 10711 | `			}` |
|       ! 0 | 10712 | `			return SXRET_OK;` |
|         - | 10713 | `		}` |
|         - | 10714 | `		/* Extract class name */` |
|    352871 | 10715 | `		pName = &pGen->pIn->sData;` |
|         - | 10716 | `		/* Advance the stream cursor */` |
|    352871 | 10717 | `		pGen->pIn++;` |
|         - | 10718 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10719 | `			SyBlob sFQN;` |
|         - | 10720 | `			SyString sFQNStr;` |
|    352871 | 10721 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    352871 | 10722 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    352871 | 10723 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    352871 | 10724 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    352871 | 10725 | `			SyBlobRelease(&sFQN);` |
|         - | 10726 | `		}` |
|         - | 10727 | `	}` |
|    352899 | 10728 | `	if( pClass == 0 ){` |
|       ! 0 | 10729 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10730 | `		return SXERR_ABORT;` |
|         - | 10731 | `	}` |
|    352894 | 10732 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3849 | 10733 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10734 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3835 | 10735 | `		pGen->pIn++; /* Jump ':' */` |
|      3830 | 10736 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3835 | 10737 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10738 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10739 | `			pGen->pIn++;` |
|      3828 | 10740 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3829 | 10741 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3827 | 10742 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3827 | 10743 | `			pGen->pIn++;` |
|      1916 | 10744 | `		}else{` |
|         3 | 10745 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10746 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10747 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10748 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10749 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10750 | `				return SXERR_ABORT;` |
|         - | 10751 | `			}` |
|         3 | 10752 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10753 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10754 | `			}` |
|         - | 10755 | `		}` |
|      1915 | 10756 | `	}` |
|    352899 | 10757 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    352899 | 10758 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10759 | `		return SXERR_ABORT;` |
|         - | 10760 | `	}` |
|         - | 10761 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    352899 | 10762 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    352899 | 10763 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10764 | `	/* Assume a standalone class */` |
|    352899 | 10765 | `	pBase = 0;` |
|    352899 | 10766 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    286651 | 10767 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    286651 | 10768 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10769 | `			SyBlob sResolved;` |
|         - | 10770 | `			SyString sBaseName;` |
|         - | 10771 | `			sxu32 nRefLine;` |
|    183441 | 10772 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10773 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10774 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10775 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10776 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10777 | `					return SXERR_ABORT;` |
|         - | 10778 | `				}` |
|       ! 0 | 10779 | `			}` |
|    183441 | 10780 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    183441 | 10781 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    183441 | 10782 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    183441 | 10783 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10784 | `				SyBlobRelease(&sResolved);` |
|         4 | 10785 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10786 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10787 | `					pName);` |
|         3 | 10788 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10789 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10790 | `					return SXERR_ABORT;` |
|         - | 10791 | `				}` |
|         3 | 10792 | `				return SXRET_OK;` |
|         - | 10793 | `			}` |
|    275156 | 10794 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    183434 | 10795 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    183439 | 10796 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10797 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10798 | `			/* Interfaces are not allowed */` |
|    183439 | 10799 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10800 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10801 | `			}` |
|    183439 | 10802 | `			if( pBase == 0 ){` |
|       ! 0 | 10803 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10804 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10805 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10806 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10807 | `					return SXERR_ABORT;` |
|         - | 10808 | `				}` |
|       ! 0 | 10809 | `			}else{` |
|    183439 | 10810 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10811 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10812 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10813 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10814 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10815 | `						return SXERR_ABORT;` |
|         - | 10816 | `					}` |
|         3 | 10817 | `					pBase = 0; /* Never inherit from an enum */` |
|    183438 | 10818 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10819 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10820 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10821 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10822 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10823 | `						return SXERR_ABORT;` |
|         - | 10824 | `					}` |
|       ! 0 | 10825 | `				}` |
|         - | 10826 | `			}` |
|    183439 | 10827 | `			SyBlobRelease(&sResolved);` |
|    183439 | 10828 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10829 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10830 | `			}` |
|     91717 | 10831 | `		}` |
|    286649 | 10832 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10833 | `			ph7_class *pInterface;` |
|         - | 10834 | `			/* Interface implementation */` |
|    107045 | 10835 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    110774 | 10836 | `			for(;;){` |
|         - | 10837 | `				SyBlob sResolved;` |
|         - | 10838 | `				SyString sIntName;` |
|         - | 10839 | `				sxu32 nRefLine;` |
|    164299 | 10840 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    164299 | 10841 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    164299 | 10842 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10843 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10844 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10845 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10846 | `						pName);` |
|       ! 0 | 10847 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10848 | `						return SXERR_ABORT;` |
|         - | 10849 | `					}` |
|       ! 0 | 10850 | `					break;` |
|         - | 10851 | `				}` |
|    328593 | 10852 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    164294 | 10853 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    164299 | 10854 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10855 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10856 | `				/* Only interfaces are allowed */` |
|    164299 | 10857 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10858 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10859 | `				}` |
|    164299 | 10860 | `				if( pInterface == 0 ){` |
|       ! 0 | 10861 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10862 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10863 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10864 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10865 | `						return SXERR_ABORT;` |
|         - | 10866 | `					}` |
|       ! 0 | 10867 | `				}else{` |
|         - | 10868 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10869 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10870 | `					 * unless they already extend Exception or Error.` |
|         - | 10871 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10872 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10873 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    164299 | 10874 | `					SyString *pFqn = &pClass->sName;` |
|    164299 | 10875 | `					int bIsExceptionOrError =` |
|     85967 | 10876 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    248353 | 10877 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    162393 | 10878 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3830 | 10879 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    168115 | 10880 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11466 | 10881 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3819 | 10882 | `						!bIsExceptionOrError ){` |
|        12 | 10883 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10884 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10885 | `							&pClass->sName);` |
|         9 | 10886 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10887 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10888 | `							return SXERR_ABORT;` |
|         - | 10889 | `						}` |
|         - | 10890 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10891 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10892 | `					}else{` |
|    164293 | 10893 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10894 | `					}` |
|         - | 10895 | `				}` |
|    164299 | 10896 | `				SyBlobRelease(&sResolved);` |
|    164299 | 10897 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53525 | 10898 | `					break;` |
|         - | 10899 | `				}` |
|     57259 | 10900 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10901 | `			}` |
|     53520 | 10902 | `		}` |
|    143322 | 10903 | `	}` |
|    352897 | 10904 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10905 | `		/* Syntax error */` |
|       ! 0 | 10906 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10907 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10908 | `		if( rc == SXERR_ABORT ){` |
|         - | 10909 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10910 | `			return SXERR_ABORT;` |
|         - | 10911 | `		}` |
|       ! 0 | 10912 | `		return SXRET_OK;` |
|         - | 10913 | `	}` |
|    352897 | 10914 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    352897 | 10915 | `	pEnd = 0; /* cc warning */` |
|         - | 10916 | `	/* Delimit the class body */` |
|    352897 | 10917 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    352897 | 10918 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10919 | `		/* Syntax error */` |
|       ! 0 | 10920 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10921 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10922 | `		if( rc == SXERR_ABORT ){` |
|         - | 10923 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10924 | `			return SXERR_ABORT;` |
|         - | 10925 | `		}` |
|       ! 0 | 10926 | `		return SXRET_OK;` |
|         - | 10927 | `	}` |
|         - | 10928 | `	/* The delimiter token is the class body's closing brace */` |
|    352897 | 10929 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10930 | `	/* Swap token stream */` |
|    352897 | 10931 | `	pTmp = pGen->pEnd;` |
|    352897 | 10932 | `	pGen->pEnd = pEnd;` |
|         - | 10933 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    352897 | 10934 | `	pClass->iFlags \|= iFlags;` |
|         - | 10935 | `	/* Start the parse process */` |
|   1369762 | 10936 | `	for(;;){` |
|         - | 10937 | `		/* Jump leading/trailing semi-colons */` |
|   3901809 | 10938 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    703293 | 10939 | `			pGen->pIn++;` |
|         5 | 10940 | `		}` |
|   3198521 | 10941 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10942 | `			/* End of class body */` |
|    352855 | 10943 | `			break;` |
|         - | 10944 | `		}` |
|         - | 10945 | `		/* Bind a directly-preceding docblock to this member */` |
|   2845671 | 10946 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2845666 | 10947 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1422838 | 10948 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10949 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10950 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10951 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10952 | `			if( rc == SXERR_ABORT ){` |
|         - | 10953 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10954 | `				return SXERR_ABORT;` |
|         - | 10955 | `			}` |
|       ! 0 | 10956 | `			goto done;` |
|         - | 10957 | `		}` |
|         - | 10958 | `		/* Assume public visibility */` |
|   2845671 | 10959 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2845671 | 10960 | `		iAttrflags = 0;` |
|         - | 10961 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10962 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10963 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10964 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2845671 | 10965 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10966 | `			int bMod = 0;` |
|       ! 0 | 10967 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10968 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10969 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10970 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10971 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10972 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10973 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10974 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10975 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10976 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10977 | `			}` |
|       ! 0 | 10978 | `			if( !bMod ){` |
|       ! 0 | 10979 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10980 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10981 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10982 | `						return SXERR_ABORT;` |
|         - | 10983 | `					}` |
|       ! 0 | 10984 | `					goto done;` |
|         - | 10985 | `				}` |
|       ! 0 | 10986 | `				continue;` |
|         - | 10987 | `			}` |
|       ! 0 | 10988 | `		}` |
|   2845671 | 10989 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10990 | `			/* Extract the current keyword */` |
|   2845671 | 10991 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2845671 | 10992 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10993 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7679 | 10994 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7679 | 10995 | `				if( rc != SXRET_OK ){` |
|         6 | 10996 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10997 | `						return SXERR_ABORT;` |
|         - | 10998 | `					}` |
|         6 | 10999 | `					goto done;` |
|         - | 11000 | `				}` |
|      7675 | 11001 | `				continue;` |
|         - | 11002 | `			}` |
|   2837997 | 11003 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11004 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 11005 | `				TraitUseEntry sUse;` |
|     15333 | 11006 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15333 | 11007 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15333 | 11008 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7672 | 11009 | `				for(;;){` |
|         - | 11010 | `					ph7_class *pTrait;` |
|         - | 11011 | `					SyString *pTraitName;` |
|     15341 | 11012 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11013 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11014 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 11015 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11016 | `							return SXERR_ABORT;` |
|         - | 11017 | `						}` |
|       ! 0 | 11018 | `						break;` |
|         - | 11019 | `					}` |
|     15341 | 11020 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 11021 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 11022 | `						SyBlob sResolved;` |
|     15341 | 11023 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15341 | 11024 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     30677 | 11025 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15336 | 11026 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15341 | 11027 | `						SyBlobRelease(&sResolved);` |
|         - | 11028 | `					}` |
|         - | 11029 | `					/* Only traits are allowed */` |
|     15341 | 11030 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11031 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 11032 | `					}` |
|     15341 | 11033 | `					if( pTrait == 0 ){` |
|       ! 0 | 11034 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11035 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 11036 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11037 | `							return SXERR_ABORT;` |
|         - | 11038 | `						}` |
|       ! 0 | 11039 | `					}else{` |
|     15341 | 11040 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 11041 | `					}` |
|     15341 | 11042 | `					pGen->pIn++; /* Advance past trait name */` |
|     15341 | 11043 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7669 | 11044 | `						break;` |
|         - | 11045 | `					}` |
|        10 | 11046 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 11047 | `				}` |
|         - | 11048 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15333 | 11049 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 11050 | `					SyToken *pBlock;` |
|        13 | 11051 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 11052 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 11053 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 11054 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 11055 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 11056 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 11057 | `					}else{` |
|       ! 0 | 11058 | `						pGen->pIn = pGen->pEnd;` |
|         - | 11059 | `					}` |
|         5 | 11060 | `				}` |
|     15333 | 11061 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 11062 | `				/* The semicolon will be consumed by the outer loop */` |
|     15333 | 11063 | `				continue;` |
|         - | 11064 | `			}` |
|   2822669 | 11065 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 11066 | `				int nSetTok;` |
|   2577945 | 11067 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2577945 | 11068 | `				if( nSetVis ){` |
|         - | 11069 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 11070 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 11071 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11072 | `					pGen->pIn += nSetTok;` |
|         2 | 11073 | `				}else{` |
|   2577943 | 11074 | `					iProtection = nKwrd;` |
|   2577943 | 11075 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 11076 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 11077 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2577943 | 11078 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2577943 | 11079 | `					if( nSetVis ){` |
|         9 | 11080 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 11081 | `						pGen->pIn += nSetTok;` |
|         4 | 11082 | `					}` |
|         - | 11083 | `				}` |
|         - | 11084 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 11085 | ``				 * `public private(set) readonly int $x`. */`` |
|   2577945 | 11086 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 11087 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 11088 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 11089 | `				}` |
|   2577940 | 11090 | `				if( pGen->pIn >= pGen->pEnd` |
|   2577945 | 11091 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11092 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11093 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11094 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11095 | `					if( rc == SXERR_ABORT ){` |
|         - | 11096 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 11097 | `						return SXERR_ABORT;` |
|         - | 11098 | `					}` |
|       ! 0 | 11099 | `					goto done;` |
|         - | 11100 | `				}` |
|   2577945 | 11101 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11102 | `					/* Attribute declaration (untyped) */` |
|    408937 | 11103 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    408937 | 11104 | `					if( rc != SXRET_OK ){` |
|        11 | 11105 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11106 | `							return SXERR_ABORT;` |
|         - | 11107 | `						}` |
|        11 | 11108 | `						goto done;` |
|         - | 11109 | `					}` |
|    409078 | 11110 | `					continue;` |
|         - | 11111 | `				}` |
|   2169013 | 11112 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11113 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       309 | 11114 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       309 | 11115 | `					if( rc != SXRET_OK ){` |
|         8 | 11116 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11117 | `							return SXERR_ABORT;` |
|         - | 11118 | `						}` |
|         8 | 11119 | `						goto done;` |
|         - | 11120 | `					}` |
|       303 | 11121 | `					continue;` |
|         - | 11122 | `				}` |
|         - | 11123 | `				/* Extract the keyword */` |
|   2168709 | 11124 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1084352 | 11125 | `			}` |
|   2413433 | 11126 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 11127 | `				/* Process constant declaration */` |
|    236755 | 11128 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    236755 | 11129 | `				if( rc != SXRET_OK ){` |
|        11 | 11130 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11131 | `						return SXERR_ABORT;` |
|         - | 11132 | `					}` |
|        11 | 11133 | `					goto done;` |
|         - | 11134 | `				}` |
|    118376 | 11135 | `			}else{` |
|   2176683 | 11136 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 11137 | `					/* Static method or attribute,record that */` |
|     95565 | 11138 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     95565 | 11139 | `					pGen->pIn++; /* Jump the static keyword */` |
|     95565 | 11140 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11141 | `						int nSetTok;` |
|     68819 | 11142 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     68819 | 11143 | `						if( nSetVis ){` |
|         - | 11144 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 11145 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11146 | `							pGen->pIn += nSetTok;` |
|         2 | 11147 | `						}else{` |
|         - | 11148 | `							/* Extract the keyword */` |
|     68817 | 11149 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     68817 | 11150 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11151 | `								iProtection = nKwrd;` |
|       ! 0 | 11152 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 11153 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 11154 | `								if( nSetVis ){` |
|       ! 0 | 11155 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 11156 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 11157 | `								}` |
|       ! 0 | 11158 | `							}` |
|         - | 11159 | `						}` |
|     34407 | 11160 | `					}` |
|         - | 11161 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 11162 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 11163 | `					 * than a generic "expecting method" parse error. */` |
|     95565 | 11164 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11165 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11166 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 11167 | `					}` |
|     95560 | 11168 | `					if( pGen->pIn >= pGen->pEnd` |
|     95565 | 11169 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11170 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11171 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 11172 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11173 | `						if( rc == SXERR_ABORT ){` |
|         - | 11174 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11175 | `							return SXERR_ABORT;` |
|         - | 11176 | `						}` |
|       ! 0 | 11177 | `						goto done;` |
|         - | 11178 | `					}` |
|     95565 | 11179 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11180 | `						/* Attribute declaration */` |
|     26747 | 11181 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26747 | 11182 | `						if( rc != SXRET_OK ){` |
|         3 | 11183 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11184 | `								return SXERR_ABORT;` |
|         - | 11185 | `							}` |
|         3 | 11186 | `							goto done;` |
|         - | 11187 | `						}` |
|     26745 | 11188 | `						continue;` |
|         - | 11189 | `					}` |
|     68823 | 11190 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11191 | `						/* Typed static attribute declaration */` |
|        19 | 11192 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        19 | 11193 | `						if( rc != SXRET_OK ){` |
|         3 | 11194 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11195 | `								return SXERR_ABORT;` |
|         - | 11196 | `							}` |
|         3 | 11197 | `							goto done;` |
|         - | 11198 | `						}` |
|        17 | 11199 | `						continue;` |
|         - | 11200 | `					}` |
|         - | 11201 | `					/* Extract the keyword */` |
|     68807 | 11202 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2115524 | 11203 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 11204 | `					/* Abstract method,record that */` |
|      7659 | 11205 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 11206 | `					/* Mark the whole class as abstract */` |
|      7659 | 11207 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 11208 | `					/* Advance the stream cursor */` |
|      7659 | 11209 | `					pGen->pIn++;` |
|      7659 | 11210 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7659 | 11211 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7659 | 11212 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7657 | 11213 | `							iProtection = nKwrd;` |
|      7657 | 11214 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3826 | 11215 | `						}` |
|      3827 | 11216 | `					}` |
|      7659 | 11217 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7654 | 11218 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11219 | `							/* Static method */` |
|       ! 0 | 11220 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11221 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11222 | `					}` |
|      7659 | 11223 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7654 | 11224 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 11225 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 11226 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 11227 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 11228 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11229 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11230 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11231 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11232 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11233 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11234 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11235 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11236 | `										return SXERR_ABORT;` |
|         - | 11237 | `									}` |
|       ! 0 | 11238 | `									goto done;` |
|         - | 11239 | `								}` |
|         7 | 11240 | `								continue;` |
|         - | 11241 | `							}` |
|       ! 0 | 11242 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11243 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11244 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11245 | `							if( rc == SXERR_ABORT ){` |
|         - | 11246 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11247 | `								return SXERR_ABORT;` |
|         - | 11248 | `							}` |
|       ! 0 | 11249 | `							goto done;` |
|         - | 11250 | `					}` |
|      7653 | 11251 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2077293 | 11252 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11253 | `					/* final method ,record that */` |
|        20 | 11254 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        20 | 11255 | `					pGen->pIn++; /* Jump the final keyword */` |
|        20 | 11256 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11257 | `						/* Extract the keyword */` |
|        20 | 11258 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        20 | 11259 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        10 | 11260 | `							iProtection = nKwrd;` |
|        10 | 11261 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11262 | `						}` |
|         9 | 11263 | `					}` |
|        20 | 11264 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11265 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11266 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11267 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11268 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11269 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11270 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11271 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11272 | `									return SXERR_ABORT;` |
|         - | 11273 | `								}` |
|       ! 0 | 11274 | `								goto done;` |
|         - | 11275 | `							}` |
|        14 | 11276 | `							continue;` |
|         - | 11277 | `					}` |
|         8 | 11278 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11279 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11280 | `							/* Static method */` |
|       ! 0 | 11281 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11282 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11283 | `					}` |
|         8 | 11284 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11285 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11286 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11287 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11288 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11289 | `							if( rc == SXERR_ABORT ){` |
|         - | 11290 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11291 | `								return SXERR_ABORT;` |
|         - | 11292 | `							}` |
|       ! 0 | 11293 | `							goto done;` |
|         - | 11294 | `					}` |
|         8 | 11295 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11296 | `				}` |
|   2149907 | 11297 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11298 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11299 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11300 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11301 | `						if( rc == SXERR_ABORT ){` |
|         - | 11302 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11303 | `							return SXERR_ABORT;` |
|         - | 11304 | `						}` |
|       ! 0 | 11305 | `						goto done;` |
|         - | 11306 | `				}` |
|   2149907 | 11307 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11308 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11309 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11310 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11311 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11312 | `						if( rc == SXERR_ABORT ){` |
|         - | 11313 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11314 | `							return SXERR_ABORT;` |
|         - | 11315 | `						}` |
|       ! 0 | 11316 | `						goto done;` |
|         - | 11317 | `					}` |
|         - | 11318 | `					/* Attribute declaration */` |
|         7 | 11319 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11320 | `				}else{` |
|         - | 11321 | `					/* Process method declaration */` |
|   2149901 | 11322 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11323 | `				}` |
|   2149907 | 11324 | `				if( rc != SXRET_OK ){` |
|        16 | 11325 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11326 | `						return SXERR_ABORT;` |
|         - | 11327 | `					}` |
|        16 | 11328 | `					goto done;` |
|         - | 11329 | `				}` |
|         - | 11330 | `			}` |
|   1193321 | 11331 | `		}else{` |
|         - | 11332 | `			/* Attribute declaration */` |
|       ! 0 | 11333 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11334 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11335 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11336 | `					return SXERR_ABORT;` |
|         - | 11337 | `				}` |
|       ! 0 | 11338 | `				goto done;` |
|         - | 11339 | `			}` |
|         - | 11340 | `		}` |
|         5 | 11341 | `	}` |
|         - | 11342 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11343 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11344 | `	 */` |
|         - | 11345 | `	{` |
|         - | 11346 | `		TraitUseEntry *apUse;` |
|         - | 11347 | `		sxu32 nU;` |
|    352855 | 11348 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    368183 | 11349 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15333 | 11350 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15333 | 11351 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15333 | 11352 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15333 | 11353 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11354 | `			sxu32 nT;` |
|     15333 | 11355 | `			if( !hasResolution ){` |
|         - | 11356 | `				/* No conflict resolution block: use standard trait application */` |
|     30647 | 11357 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15329 | 11358 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15329 | 11359 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11360 | `						break;` |
|         - | 11361 | `					}` |
|      7667 | 11362 | `				}` |
|      7664 | 11363 | `			}else{` |
|         - | 11364 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11365 | `				 * then use the block to resolve method conflicts.` |
|         - | 11366 | `				 */` |
|         - | 11367 | `				SyToken *pR;` |
|        25 | 11368 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11369 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11370 | `					ph7_class_attr *pAR;` |
|         - | 11371 | `					SyHashEntry *pER;` |
|         - | 11372 | `					SyString *pNR;` |
|        15 | 11373 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11374 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11375 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11376 | `						pNR = &pAR->sName;` |
|       ! 0 | 11377 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11378 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11379 | `						}` |
|       ! 0 | 11380 | `					}` |
|        15 | 11381 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11382 | `				}` |
|         - | 11383 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11384 | `				pR = pUse->pResolvStart;` |
|        27 | 11385 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11386 | `					SyString sTrait,sMethod;` |
|         - | 11387 | `					ph7_class *pSrcTrait;` |
|         - | 11388 | `					ph7_class_method *pMeth;` |
|         - | 11389 | `					sxi32 nRKwrd;` |
|        41 | 11390 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11391 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11392 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11393 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11394 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11395 | `					sMethod = pR->sData;` |
|        17 | 11396 | `					pR++;` |
|        17 | 11397 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11398 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11399 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11400 | `							sTrait = sMethod;` |
|         7 | 11401 | `							pR++;` |
|         7 | 11402 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11403 | `							sMethod = pR->sData;` |
|         7 | 11404 | `							pR++;` |
|         3 | 11405 | `						}` |
|         3 | 11406 | `					}` |
|        17 | 11407 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11408 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11409 | `						continue;` |
|         - | 11410 | `					}` |
|        17 | 11411 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11412 | `					pR++;` |
|        17 | 11413 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11414 | `						pSrcTrait = 0;` |
|         7 | 11415 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11416 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11417 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11418 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11419 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11420 | `								break;` |
|         - | 11421 | `							}` |
|         2 | 11422 | `						}` |
|         5 | 11423 | `						if( pSrcTrait ){` |
|         5 | 11424 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11425 | `							if( pMeth ){` |
|         5 | 11426 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11427 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11428 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11429 | `								}` |
|         2 | 11430 | `							}` |
|         2 | 11431 | `						}` |
|         2 | 11432 | `					}` |
|        35 | 11433 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11434 | `				}` |
|         - | 11435 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11436 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11437 | `					ph7_class_method *pMR;` |
|         - | 11438 | `					SyHashEntry *pER;` |
|         - | 11439 | `					SyString *pNR;` |
|        15 | 11440 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11441 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11442 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11443 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11444 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11445 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11446 | `						}` |
|         3 | 11447 | `					}` |
|         9 | 11448 | `				}` |
|         - | 11449 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11450 | `				pR = pUse->pResolvStart;` |
|        27 | 11451 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11452 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11453 | `					ph7_class *pSrcTrait;` |
|         - | 11454 | `					ph7_class_method *pMeth;` |
|        27 | 11455 | `					int hasQual = 0;` |
|         - | 11456 | `					sxi32 nRKwrd;` |
|        41 | 11457 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11458 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11459 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11460 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11461 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11462 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11463 | `					sMethod = pR->sData;` |
|        17 | 11464 | `					pR++;` |
|        17 | 11465 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11466 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11467 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11468 | `							sTrait = sMethod;` |
|         7 | 11469 | `							hasQual = 1;` |
|         7 | 11470 | `							pR++;` |
|         7 | 11471 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11472 | `							sMethod = pR->sData;` |
|         7 | 11473 | `							pR++;` |
|         3 | 11474 | `						}` |
|         3 | 11475 | `					}` |
|        17 | 11476 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11477 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11478 | `						continue;` |
|         - | 11479 | `					}` |
|        17 | 11480 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11481 | `					pR++;` |
|        17 | 11482 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11483 | `						sxi32 iNewVis = -1;` |
|        13 | 11484 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11485 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11486 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11487 | `								iNewVis = nAK;` |
|         7 | 11488 | `								pR++;` |
|         3 | 11489 | `							}` |
|         3 | 11490 | `						}` |
|        13 | 11491 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11492 | `							sAlias = pR->sData;` |
|        11 | 11493 | `							pR++;` |
|         4 | 11494 | `						}` |
|        13 | 11495 | `						pMeth = 0;` |
|        13 | 11496 | `						if( hasQual ){` |
|         3 | 11497 | `							pSrcTrait = 0;` |
|         5 | 11498 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11499 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11500 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11501 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11502 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11503 | `									break;` |
|         - | 11504 | `								}` |
|         2 | 11505 | `							}` |
|         3 | 11506 | `							if( pSrcTrait ){` |
|         3 | 11507 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11508 | `							}` |
|         2 | 11509 | `						}else{` |
|        10 | 11510 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11511 | `						}` |
|        13 | 11512 | `						if( pMeth ){` |
|        13 | 11513 | `							if( sAlias.nByte > 0 ){` |
|         - | 11514 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11515 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11516 | `								 */` |
|         - | 11517 | `								ph7_class_method *pAlias;` |
|         - | 11518 | `								char *zAliasDup;` |
|        11 | 11519 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11520 | `								if( pAlias ){` |
|        11 | 11521 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11522 | `									if( iNewVis >= 0 ){` |
|         5 | 11523 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11524 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11525 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11526 | `									}` |
|        11 | 11527 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11528 | `									if( zAliasDup ){` |
|        11 | 11529 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11530 | `									}` |
|         7 | 11531 | `								}` |
|         7 | 11532 | `							}else if( iNewVis >= 0 ){` |
|         - | 11533 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11534 | `								ph7_class_method *pCopy;` |
|         3 | 11535 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11536 | `								if( pCopy ){` |
|         3 | 11537 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11538 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11539 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11540 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11541 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11542 | `									/* Replace the method in the class hash */` |
|         3 | 11543 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11544 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11545 | `								}` |
|         1 | 11546 | `							}` |
|         5 | 11547 | `						}` |
|         5 | 11548 | `						SXUNUSED(hasQual);` |
|         5 | 11549 | `					}` |
|        21 | 11550 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11551 | `				}` |
|         - | 11552 | `			}` |
|     15333 | 11553 | `			SySetRelease(&pUse->aTraits);` |
|      7669 | 11554 | `		}` |
|         - | 11555 | `	}` |
|    352855 | 11556 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11557 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11558 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3845 | 11559 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3845 | 11560 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11561 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11562 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11563 | `			return SXERR_ABORT;` |
|         - | 11564 | `		}` |
|      1920 | 11565 | `	}` |
|         - | 11566 | `	/* Install the class */` |
|    352855 | 11567 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    352855 | 11568 | `	if( rc == SXRET_OK ){` |
|         - | 11569 | `		ph7_class **apInterface;` |
|         - | 11570 | `		sxu32 n;` |
|    352855 | 11571 | `		if( pBase ){` |
|         - | 11572 | `			/* Inherit from base class and mark as a subclass */` |
|    183437 | 11573 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     91716 | 11574 | `		}` |
|    352855 | 11575 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    517143 | 11576 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11577 | `			/* Implements one or more interface */` |
|    164293 | 11578 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    164293 | 11579 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11580 | `				break;` |
|         - | 11581 | `			}` |
|     82149 | 11582 | `		}` |
|         - | 11583 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11584 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    352855 | 11585 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3845 | 11586 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3845 | 11587 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11588 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11589 | `			}` |
|      3845 | 11590 | `			if( pIntf ){` |
|      3845 | 11591 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1920 | 11592 | `			}` |
|      3845 | 11593 | `			if( pClass->nEnumBacking != 0 ){` |
|      3833 | 11594 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3833 | 11595 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11596 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11597 | `				}` |
|      3833 | 11598 | `				if( pIntf ){` |
|      3833 | 11599 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1914 | 11600 | `				}` |
|      1914 | 11601 | `			}` |
|      1920 | 11602 | `		}` |
|         - | 11603 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11604 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    352850 | 11605 | `		if( rc == SXRET_OK` |
|    352850 | 11606 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    352855 | 11607 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    187095 | 11608 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11609 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    187095 | 11610 | `			if( pStringable ){` |
|    187095 | 11611 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    187095 | 11612 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11613 | `				sxu32 i;` |
|    187095 | 11614 | `				int bAlready = 0;` |
|    225259 | 11615 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     41987 | 11616 | `					if( apImpl[i] == pStringable ){` |
|      3823 | 11617 | `						bAlready = 1;` |
|      3823 | 11618 | `						break;` |
|         - | 11619 | `					}` |
|     19087 | 11620 | `				}` |
|    187095 | 11621 | `				if( !bAlready ){` |
|    183277 | 11622 | `					PH7_ClassImplement(pClass,pStringable);` |
|     91636 | 11623 | `				}` |
|     93545 | 11624 | `			}` |
|     93545 | 11625 | `		}` |
|         - | 11626 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    352855 | 11627 | `		if( rc == SXRET_OK ){` |
|    352855 | 11628 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    352855 | 11629 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11630 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11631 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11632 | `				return SXERR_ABORT;` |
|         - | 11633 | `			}` |
|    176425 | 11634 | `		}` |
|         - | 11635 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    352855 | 11636 | `		if( rc == SXRET_OK ){` |
|    352855 | 11637 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    352855 | 11638 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11639 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11640 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11641 | `				return SXERR_ABORT;` |
|         - | 11642 | `			}` |
|    176425 | 11643 | `		}` |
|    176425 | 11644 | `	}` |
|    352855 | 11645 | `	SySetRelease(&aUseEntries);` |
|    352855 | 11646 | `	SySetRelease(&aInterfaces);` |
|    352855 | 11647 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11648 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11649 | `		return SXERR_ABORT;` |
|         - | 11650 | `	}` |
|    176425 | 11651 | `done:` |
|         - | 11652 | `	/* Point beyond the class body */` |
|    352897 | 11653 | `	pGen->pIn = &pEnd[1];` |
|    352897 | 11654 | `	pGen->pEnd = pTmp;` |
|    352897 | 11655 | `	return PH7_OK;` |
|    176452 | 11656 | `}` |
|         - | 11657 | `/* Compile a named class declaration (the common case). */` |
|    352866 | 11658 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11659 | `{` |
|    352871 | 11660 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11661 | `}` |
|         - | 11662 | `/*` |
|         - | 11663 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11664 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11665 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11666 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11667 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11668 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11669 | ` */` |
|        28 | 11670 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11671 | `{` |
|         - | 11672 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11673 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11674 | `	SyString sName;` |
|         - | 11675 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11676 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11677 | `	                              * is keyed to this 'class' token */` |
|         - | 11678 | `	ph7_value *pObj;` |
|        32 | 11679 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11680 | `	sxu32 nIdx,nLen;` |
|         - | 11681 | `	sxi32 nArg,rc;` |
|        14 | 11682 | `	SXUNUSED(iCompileFlag);` |
|         - | 11683 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11684 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11685 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11686 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11687 | `	}` |
|        32 | 11688 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11689 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11690 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11691 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11692 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11693 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11694 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11695 | `		return rc;` |
|         - | 11696 | `	}` |
|         - | 11697 | `	{` |
|         - | 11698 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11699 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11700 | `		if( pAnonClass` |
|        32 | 11701 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11702 | `			return SXERR_ABORT;` |
|         - | 11703 | `		}` |
|         - | 11704 | `	}` |
|         - | 11705 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11706 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11707 | `	nArg = 0;` |
|        32 | 11708 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11709 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11710 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11711 | `		SyToken *pArgNext;` |
|         7 | 11712 | `		pGen->pIn = pArgStart;` |
|         7 | 11713 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11714 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11715 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11716 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11717 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11718 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11719 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11720 | `					return SXERR_ABORT;` |
|         - | 11721 | `				}` |
|         7 | 11722 | `				nArg++;` |
|         3 | 11723 | `			}` |
|         7 | 11724 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11725 | `		}` |
|         7 | 11726 | `		pGen->pIn = pSavedIn;` |
|         7 | 11727 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11728 | `	}` |
|         - | 11729 | `	/* Load the synthesized class name */` |
|        32 | 11730 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11731 | `	if( pObj == 0 ){` |
|       ! 0 | 11732 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11733 | `		return SXERR_ABORT;` |
|         - | 11734 | `	}` |
|        32 | 11735 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11736 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11737 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11738 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11739 | `	return SXRET_OK;` |
|        18 | 11740 | `}` |
|         - | 11741 | `/*` |
|         - | 11742 | ` * Compile a user-defined abstract class.` |
|         - | 11743 | ` *  According to the PHP language reference manual` |
|         - | 11744 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11745 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11746 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11747 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11748 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11749 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11750 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11751 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11752 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11753 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11754 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11755 | ` *   could differ.` |
|         - | 11756 | ` */` |
|         - | 11757 | `/*` |
|         - | 11758 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11759 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11760 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11761 | ` */` |
|  12781760 | 11762 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11763 | `{` |
|  12781765 | 11764 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   7493473 | 11765 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   7493473 | 11766 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   7447647 | 11767 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3708506 | 11768 | `	}` |
|  12705309 | 11769 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  12705249 | 11770 | `	return FALSE;` |
|   6390885 | 11771 | `}` |
|         - | 11772 | `/*` |
|         - | 11773 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11774 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11775 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11776 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11777 | ` */` |
|  12705244 | 11778 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11779 | `{` |
|  12705249 | 11780 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  12705249 | 11781 | `	sxi32 iFlags = 0,iFlag;` |
|  12781765 | 11782 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76521 | 11783 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11784 | `			pDup = pIn;` |
|         2 | 11785 | `		}` |
|     76521 | 11786 | `		iFlags \|= iFlag;` |
|     76521 | 11787 | `		pIn++;` |
|         5 | 11788 | `	}` |
|  12705249 | 11789 | `	*ppIn = pIn;` |
|  12705249 | 11790 | `	if( ppDup ){ *ppDup = pDup; }` |
|  12705249 | 11791 | `	return iFlags;` |
|         5 | 11792 | `}` |
|         - | 11793 | `/*` |
|         - | 11794 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11795 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11796 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11797 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11798 | `` * `readonly`) to their existing handlers.`` |
|         - | 11799 | ` */` |
|  12670812 | 11800 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11801 | `{` |
|  12670817 | 11802 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   6377477 | 11803 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  12691846 | 11804 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11805 | `}` |
|         - | 11806 | `/*` |
|         - | 11807 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11808 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11809 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11810 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11811 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11812 | ` */` |
|     34432 | 11813 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11814 | `{` |
|         - | 11815 | `	SyToken *pDup;` |
|     34437 | 11816 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11817 | `	sxi32 rc;` |
|     34437 | 11818 | `	if( pDup ){` |
|         4 | 11819 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11820 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11821 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11822 | `			return SXERR_ABORT;` |
|         - | 11823 | `		}` |
|         1 | 11824 | `	}` |
|     34432 | 11825 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17221 | 11826 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11827 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11828 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11829 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11830 | `			return SXERR_ABORT;` |
|         - | 11831 | `		}` |
|         1 | 11832 | `	}` |
|     34437 | 11833 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17221 | 11834 | `}` |
|         - | 11835 | `/*` |
|         - | 11836 | ` * Compile a user-defined trait.` |
|         - | 11837 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11838 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11839 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11840 | ` */` |
|      7710 | 11841 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11842 | `{` |
|      7715 | 11843 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11844 | `	ph7_class *pClass;` |
|         - | 11845 | `	SyToken *pEnd,*pTmp;` |
|         - | 11846 | `	sxi32 iProtection;` |
|         - | 11847 | `	sxi32 iAttrflags;` |
|         - | 11848 | `	SyString *pName;` |
|         - | 11849 | `	sxi32 nKwrd;` |
|         - | 11850 | `	sxi32 rc;` |
|         - | 11851 | `	/* Jump the 'trait' keyword */` |
|      7715 | 11852 | `	pGen->pIn++;` |
|      7715 | 11853 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11854 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11855 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11856 | `			return SXERR_ABORT;` |
|         - | 11857 | `		}` |
|       ! 0 | 11858 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11859 | `			pGen->pIn++;` |
|       ! 0 | 11860 | `		}` |
|       ! 0 | 11861 | `		return SXRET_OK;` |
|         - | 11862 | `	}` |
|         - | 11863 | `	/* Extract trait name */` |
|      7715 | 11864 | `	pName = &pGen->pIn->sData;` |
|      7715 | 11865 | `	pGen->pIn++;` |
|         - | 11866 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11867 | `		SyBlob sFQN;` |
|         - | 11868 | `		SyString sFQNStr;` |
|      7715 | 11869 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7715 | 11870 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7715 | 11871 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7715 | 11872 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7715 | 11873 | `		SyBlobRelease(&sFQN);` |
|         - | 11874 | `	}` |
|      7715 | 11875 | `	if( pClass == 0 ){` |
|       ! 0 | 11876 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11877 | `		return SXERR_ABORT;` |
|         - | 11878 | `	}` |
|      7715 | 11879 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7715 | 11880 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11881 | `		return SXERR_ABORT;` |
|         - | 11882 | `	}` |
|         - | 11883 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7715 | 11884 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11885 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11886 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11887 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11888 | `			return SXERR_ABORT;` |
|         - | 11889 | `		}` |
|       ! 0 | 11890 | `		return SXRET_OK;` |
|         - | 11891 | `	}` |
|      7715 | 11892 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7715 | 11893 | `	pEnd = 0;` |
|      7715 | 11894 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7715 | 11895 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11896 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11897 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11898 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11899 | `			return SXERR_ABORT;` |
|         - | 11900 | `		}` |
|       ! 0 | 11901 | `		return SXRET_OK;` |
|         - | 11902 | `	}` |
|         - | 11903 | `	/* The delimiter token is the trait body's closing brace */` |
|      7715 | 11904 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11905 | `	/* Swap token stream */` |
|      7715 | 11906 | `	pTmp = pGen->pEnd;` |
|      7715 | 11907 | `	pGen->pEnd = pEnd;` |
|         - | 11908 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7715 | 11909 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11910 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     55411 | 11911 | `	for(;;){` |
|    156667 | 11912 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     22927 | 11913 | `			pGen->pIn++;` |
|         5 | 11914 | `		}` |
|    133745 | 11915 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7715 | 11916 | `			break;` |
|         - | 11917 | `		}` |
|         - | 11918 | `		/* Bind a directly-preceding docblock to this member */` |
|    126035 | 11919 | `		GenStateSetPendingDoc(&(*pGen));` |
|    126035 | 11920 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11921 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11922 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11923 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11924 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11925 | `				return SXERR_ABORT;` |
|         - | 11926 | `			}` |
|       ! 0 | 11927 | `			goto done;` |
|         - | 11928 | `		}` |
|    126035 | 11929 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    126035 | 11930 | `		iAttrflags = 0;` |
|    126035 | 11931 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    126035 | 11932 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    126035 | 11933 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11934 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11935 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11936 | `				for(;;){` |
|         - | 11937 | `					ph7_class *pUsedTrait;` |
|         - | 11938 | `					SyString *pUsedName;` |
|         5 | 11939 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11940 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11941 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11942 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11943 | `							return SXERR_ABORT;` |
|         - | 11944 | `						}` |
|       ! 0 | 11945 | `						break;` |
|         - | 11946 | `					}` |
|         5 | 11947 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11948 | `					{` |
|         - | 11949 | `						SyBlob sResolved;` |
|         5 | 11950 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11951 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11952 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11953 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11954 | `						SyBlobRelease(&sResolved);` |
|         - | 11955 | `					}` |
|         5 | 11956 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11957 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11958 | `					}` |
|         5 | 11959 | `					if( pUsedTrait == 0 ){` |
|         4 | 11960 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11961 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11962 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11963 | `							return SXERR_ABORT;` |
|         - | 11964 | `						}` |
|         2 | 11965 | `					}else{` |
|         3 | 11966 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11967 | `					}` |
|         5 | 11968 | `					pGen->pIn++;` |
|         5 | 11969 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11970 | `						break;` |
|         - | 11971 | `					}` |
|       ! 0 | 11972 | `					pGen->pIn++;` |
|       ! 0 | 11973 | `				}` |
|         5 | 11974 | `				continue;` |
|         - | 11975 | `			}` |
|    126031 | 11976 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    126015 | 11977 | `				iProtection = nKwrd;` |
|    126015 | 11978 | `				pGen->pIn++;` |
|    126010 | 11979 | `				if( pGen->pIn >= pGen->pEnd` |
|    126015 | 11980 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11981 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11982 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11983 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11984 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11985 | `						return SXERR_ABORT;` |
|         - | 11986 | `					}` |
|       ! 0 | 11987 | `					goto done;` |
|         - | 11988 | `				}` |
|    126015 | 11989 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     22913 | 11990 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     22913 | 11991 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11992 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11993 | `							return SXERR_ABORT;` |
|         - | 11994 | `						}` |
|       ! 0 | 11995 | `						goto done;` |
|         - | 11996 | `					}` |
|     22913 | 11997 | `					continue;` |
|         - | 11998 | `				}` |
|    103107 | 11999 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 12000 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 12001 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 12002 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12003 | `							return SXERR_ABORT;` |
|         - | 12004 | `						}` |
|       ! 0 | 12005 | `						goto done;` |
|         - | 12006 | `					}` |
|         5 | 12007 | `					continue;` |
|         - | 12008 | `				}` |
|    103103 | 12009 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     51549 | 12010 | `			}` |
|    103119 | 12011 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 12012 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12013 | `					"Traits cannot have constants");` |
|       ! 0 | 12014 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12015 | `					return SXERR_ABORT;` |
|         - | 12016 | `				}` |
|       ! 0 | 12017 | `				goto done;` |
|       ! 0 | 12018 | `			}else{` |
|    103119 | 12019 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7647 | 12020 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7647 | 12021 | `					pGen->pIn++;` |
|      7647 | 12022 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7645 | 12023 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7645 | 12024 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 12025 | `							iProtection = nKwrd;` |
|       ! 0 | 12026 | `							pGen->pIn++;` |
|       ! 0 | 12027 | `						}` |
|      3820 | 12028 | `					}` |
|      7642 | 12029 | `					if( pGen->pIn >= pGen->pEnd` |
|      7647 | 12030 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 12031 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12032 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 12033 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12034 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12035 | `							return SXERR_ABORT;` |
|         - | 12036 | `						}` |
|       ! 0 | 12037 | `						goto done;` |
|         - | 12038 | `					}` |
|      7647 | 12039 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 12040 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 12041 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12042 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12043 | `								return SXERR_ABORT;` |
|         - | 12044 | `							}` |
|       ! 0 | 12045 | `							goto done;` |
|         - | 12046 | `						}` |
|         3 | 12047 | `						continue;` |
|         - | 12048 | `					}` |
|      7645 | 12049 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 12050 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12051 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12052 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12053 | `								return SXERR_ABORT;` |
|         - | 12054 | `							}` |
|       ! 0 | 12055 | `							goto done;` |
|         - | 12056 | `						}` |
|       ! 0 | 12057 | `						continue;` |
|         - | 12058 | `					}` |
|      7645 | 12059 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     99297 | 12060 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 12061 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 12062 | `					pGen->pIn++;` |
|         6 | 12063 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 12064 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 12065 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 12066 | `							iProtection = nKwrd;` |
|         6 | 12067 | `							pGen->pIn++;` |
|         2 | 12068 | `						}` |
|         2 | 12069 | `					}` |
|         6 | 12070 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 12071 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 12072 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12073 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 12074 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12075 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12076 | `							return SXERR_ABORT;` |
|         - | 12077 | `						}` |
|       ! 0 | 12078 | `						goto done;` |
|         - | 12079 | `					}` |
|         6 | 12080 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 12081 | `				}` |
|    103117 | 12082 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 12083 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12084 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 12085 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12086 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12087 | `						return SXERR_ABORT;` |
|         - | 12088 | `					}` |
|       ! 0 | 12089 | `					goto done;` |
|         - | 12090 | `				}` |
|    103117 | 12091 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 12092 | `					pGen->pIn++;` |
|       ! 0 | 12093 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 12094 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12095 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 12096 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12097 | `							return SXERR_ABORT;` |
|         - | 12098 | `						}` |
|       ! 0 | 12099 | `						goto done;` |
|         - | 12100 | `					}` |
|       ! 0 | 12101 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12102 | `				}else{` |
|    103117 | 12103 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 12104 | `				}` |
|    103117 | 12105 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 12106 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12107 | `						return SXERR_ABORT;` |
|         - | 12108 | `					}` |
|       ! 0 | 12109 | `					goto done;` |
|         - | 12110 | `				}` |
|         - | 12111 | `			}` |
|     51561 | 12112 | `		}else{` |
|       ! 0 | 12113 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12114 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12115 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12116 | `					return SXERR_ABORT;` |
|         - | 12117 | `				}` |
|       ! 0 | 12118 | `				goto done;` |
|         - | 12119 | `			}` |
|         - | 12120 | `		}` |
|         5 | 12121 | `	}` |
|         - | 12122 | `	/* Install the trait */` |
|      7715 | 12123 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7715 | 12124 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12125 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12126 | `		return SXERR_ABORT;` |
|         - | 12127 | `	}` |
|      3855 | 12128 | `done:` |
|         - | 12129 | `	/* Point beyond the trait body */` |
|      7715 | 12130 | `	pGen->pIn = &pEnd[1];` |
|      7715 | 12131 | `	pGen->pEnd = pTmp;` |
|      7715 | 12132 | `	return PH7_OK;` |
|      3860 | 12133 | `}` |
|         - | 12134 | `/*` |
|         - | 12135 | ` * Compile a user-defined class.` |
|         - | 12136 | ` *  According to the PHP language reference manual` |
|         - | 12137 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 12138 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 12139 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 12140 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 12141 | ` *   and functions (called "methods").` |
|         - | 12142 | ` */` |
|    314590 | 12143 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 12144 | `{` |
|         - | 12145 | `	sxi32 rc;` |
|    314595 | 12146 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    314595 | 12147 | `	return rc;` |
|         5 | 12148 | `}` |
|         - | 12149 | `/*` |
|         - | 12150 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 12151 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 12152 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 12153 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 12154 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 12155 | ` */` |
|  12628748 | 12156 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 12157 | `{` |
|  12833555 | 12158 | `	return (pIn->nType & PH7_TK_ID)` |
|   6519176 | 12159 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    214475 | 12160 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  12833550 | 12161 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 12162 | `}` |
|         - | 12163 | `/*` |
|         - | 12164 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 12165 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 12166 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 12167 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 12168 | ` */` |
|      3844 | 12169 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 12170 | `{` |
|      3849 | 12171 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 12172 | `}` |
|         - | 12173 | `/*` |
|         - | 12174 | ` * Exception handling.` |
|         - | 12175 | ` *  According to the PHP language reference manual` |
|         - | 12176 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 12177 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 12178 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 12179 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 12180 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 12181 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 12182 | ` *    (or re-thrown) within a catch block.` |
|         - | 12183 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 12184 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 12185 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 12186 | ` *    been defined with set_exception_handler().` |
|         - | 12187 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 12188 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 12189 | ` */` |
|         - | 12190 | `/*` |
|         - | 12191 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 12192 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 12193 | ` * indicates failure.` |
|         - | 12194 | ` */` |
|    507950 | 12195 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 12196 | `{` |
|    507955 | 12197 | `	sxi32 rc = SXRET_OK;` |
|    507955 | 12198 | `	if( pRoot->pOp ){` |
|    507943 | 12199 | `		switch( pRoot->pOp->iOp ){` |
|    253969 | 12200 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 12201 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 12202 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 12203 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 12204 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 12205 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    507943 | 12206 | `			break;` |
|       ! 0 | 12207 | `		default:` |
|         - | 12208 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 12209 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 12210 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 12211 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12212 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 12213 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 12214 | `				rc = SXERR_INVALID;` |
|       ! 0 | 12215 | `			}` |
|       ! 0 | 12216 | `			break;` |
|         - | 12217 | `		}` |
|    253986 | 12218 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 12219 | `		/* Unexpected expression */` |
|       ! 0 | 12220 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12221 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12222 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 12223 | `			rc = SXERR_INVALID;` |
|       ! 0 | 12224 | `		}` |
|       ! 0 | 12225 | `	}` |
|    507955 | 12226 | `	return rc;` |
|         5 | 12227 | `}` |
|         - | 12228 | `/*` |
|         - | 12229 | ` * Compile a 'throw' statement.` |
|         - | 12230 | ` * throw: This is how you trigger an exception.` |
|         - | 12231 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12232 | ` */` |
|    507914 | 12233 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12234 | `{` |
|    507919 | 12235 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12236 | `	GenBlock *pBlock;` |
|         - | 12237 | `	sxu32 nIdx;` |
|         - | 12238 | `	sxi32 rc;` |
|    507919 | 12239 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12240 | `	/* Compile the expression */` |
|    507919 | 12241 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    507919 | 12242 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12243 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12244 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12245 | `			return SXERR_ABORT;` |
|         - | 12246 | `		}` |
|       ! 0 | 12247 | `		return SXRET_OK;` |
|         - | 12248 | `	}` |
|    507919 | 12249 | `	pBlock = pGen->pCurrent;` |
|         - | 12250 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   2023277 | 12251 | `	while(pBlock->pParent){` |
|   2023273 | 12252 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    507915 | 12253 | `			break;` |
|         - | 12254 | `		}` |
|         - | 12255 | `		/* Point to the parent block */` |
|   1515363 | 12256 | `		pBlock = pBlock->pParent;` |
|         5 | 12257 | `	}` |
|         - | 12258 | `	/* Emit the throw instruction */` |
|    507919 | 12259 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12260 | `	/* Emit the jump */` |
|    507919 | 12261 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    507919 | 12262 | `	return SXRET_OK;` |
|    253962 | 12263 | `}` |
|         - | 12264 | `/*` |
|         - | 12265 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12266 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12267 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12268 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12269 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12270 | ` */` |
|        36 | 12271 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12272 | `{` |
|        38 | 12273 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12274 | `	GenBlock *pBlock;` |
|         - | 12275 | `	sxu32 nIdx;` |
|         - | 12276 | `	sxi32 rc;` |
|        18 | 12277 | `	(void)iCompileFlag;` |
|        38 | 12278 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12279 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12280 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12281 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12282 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12283 | `			return SXERR_ABORT;` |
|         - | 12284 | `		}` |
|       ! 0 | 12285 | `		return SXRET_OK;` |
|         - | 12286 | `	}` |
|        38 | 12287 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12288 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12289 | `		return SXERR_ABORT;` |
|         - | 12290 | `	}` |
|        38 | 12291 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12292 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12293 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12294 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12295 | `			return SXERR_ABORT;` |
|         - | 12296 | `		}` |
|       ! 0 | 12297 | `		return SXRET_OK;` |
|         - | 12298 | `	}` |
|         - | 12299 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12300 | `	pBlock = pGen->pCurrent;` |
|        60 | 12301 | `	while( pBlock->pParent ){` |
|        49 | 12302 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12303 | `			break;` |
|         - | 12304 | `		}` |
|        23 | 12305 | `		pBlock = pBlock->pParent;` |
|         1 | 12306 | `	}` |
|        38 | 12307 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12308 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12309 | `	return SXRET_OK;` |
|        20 | 12310 | `}` |
|         - | 12311 | `/*` |
|         - | 12312 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12313 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12314 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12315 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12316 | ` * compile error propagated from the parser.` |
|         - | 12317 | ` */` |
|        56 | 12318 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12319 | `{` |
|         - | 12320 | `	SyString sClassName;` |
|         - | 12321 | `	SyToken *pToken;` |
|         - | 12322 | `	SyString *pName;` |
|         - | 12323 | `	char *zDup;` |
|         - | 12324 | `	sxi32 rc;` |
|        61 | 12325 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        61 | 12326 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        61 | 12327 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        61 | 12328 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        61 | 12329 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12330 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12331 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12332 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12333 | `		return SXERR_INVALID;` |
|         - | 12334 | `	}` |
|        61 | 12335 | `	pGen->pIn++; /* '(' */` |
|        28 | 12336 | `	for(;;){` |
|         - | 12337 | `		SyBlob sResolved;` |
|        61 | 12338 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        61 | 12339 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12340 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12341 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12342 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12343 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12344 | `			return SXERR_INVALID;` |
|         - | 12345 | `		}` |
|        89 | 12346 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        56 | 12347 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        61 | 12348 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        61 | 12349 | `		SyBlobRelease(&sResolved);` |
|        61 | 12350 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        61 | 12351 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        61 | 12352 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        56 | 12353 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12354 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12355 | `			pGen->pIn++; continue;` |
|         - | 12356 | `		}` |
|        61 | 12357 | `		break;` |
|       ! 0 | 12358 | `	}` |
|         - | 12359 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12360 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|        61 | 12361 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|         3 | 12362 | `		pGen->pIn++; /* ')' */` |
|         3 | 12363 | `		return SXRET_OK;` |
|         - | 12364 | `	}` |
|        54 | 12365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12366 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12367 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12368 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12369 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12370 | `		return SXERR_INVALID;` |
|         - | 12371 | `	}` |
|        59 | 12372 | `	pGen->pIn++; /* '$' */` |
|        59 | 12373 | `	pName = &pGen->pIn->sData;` |
|        59 | 12374 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12375 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12376 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12377 | `	pGen->pIn++;` |
|        59 | 12378 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12379 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12380 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12381 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12382 | `		return SXERR_INVALID;` |
|         - | 12383 | `	}` |
|        59 | 12384 | `	pGen->pIn++; /* ')' */` |
|        59 | 12385 | `	return SXRET_OK;` |
|        33 | 12386 | `}` |
|         - | 12387 | `/*` |
|         - | 12388 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12389 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12390 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12391 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12392 | ` * VmThrowException):` |
|         - | 12393 | ` *` |
|         - | 12394 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12395 | ` *    <try body>` |
|         - | 12396 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12397 | ` *    JMP  -> finally\|end` |
|         - | 12398 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12399 | ` *    <catch body>` |
|         - | 12400 | ` *    JMP  -> finally\|end` |
|         - | 12401 | ` *    ... more catches ...` |
|         - | 12402 | ` *  Lfin: <finally body>` |
|         - | 12403 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12404 | ` *  Lend:` |
|         - | 12405 | ` */` |
|       100 | 12406 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12407 | `{` |
|       105 | 12408 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12409 | `	GenBlock *pTry;` |
|         - | 12410 | `	VmInstr *pInstr;` |
|       105 | 12411 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12412 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12413 | `	sxi32 rc;` |
|       105 | 12414 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12415 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       105 | 12416 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       105 | 12417 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       105 | 12418 | `	pTry->pUserData = pException;` |
|       105 | 12419 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       105 | 12420 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       105 | 12421 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       105 | 12422 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       105 | 12423 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       105 | 12424 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12425 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       105 | 12426 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       105 | 12427 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       105 | 12428 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       105 | 12429 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12430 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       105 | 12431 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12432 | `	/* Catch clauses (inline) */` |
|       105 | 12433 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       100 | 12434 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        61 | 12435 | `		sxu32 k = 0;` |
|        84 | 12436 | `		for(;;){` |
|         - | 12437 | `			ph7_exception_block sCatch;` |
|         - | 12438 | `			GenBlock *pCatchBlk;` |
|       117 | 12439 | `			sxu32 idxJmp = 0;` |
|       112 | 12440 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       107 | 12441 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        33 | 12442 | `				break;` |
|         - | 12443 | `			}` |
|        61 | 12444 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        61 | 12445 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12446 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        61 | 12447 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        61 | 12448 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        61 | 12449 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        61 | 12450 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12451 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12452 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12453 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        61 | 12454 | `			pCatchBlk->pUserData = pException;` |
|        61 | 12455 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        61 | 12456 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12457 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        61 | 12458 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12459 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12460 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        61 | 12461 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        61 | 12462 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        61 | 12463 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        61 | 12464 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        61 | 12465 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        61 | 12466 | `			k++;` |
|         5 | 12467 | `		}` |
|        28 | 12468 | `	}` |
|         - | 12469 | `	/* Finally (inline) */` |
|       105 | 12470 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12471 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12472 | `		GenBlock *pFinBlk;` |
|        52 | 12473 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12474 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12475 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12476 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12477 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12478 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12479 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12480 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12481 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12482 | `		pException->iHasFinally = 1;` |
|        24 | 12483 | `	}` |
|       105 | 12484 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       105 | 12485 | `	pException->iInlined = 1;` |
|         - | 12486 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12487 | `	{` |
|       105 | 12488 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12489 | `		sxu32 *aJ; sxu32 n;` |
|       105 | 12490 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       105 | 12491 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       105 | 12492 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       161 | 12493 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        61 | 12494 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        61 | 12495 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        33 | 12496 | `		}` |
|         - | 12497 | `	}` |
|       105 | 12498 | `	SySetRelease(&aCatchJmp);` |
|       105 | 12499 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12500 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12501 | `	}` |
|       105 | 12502 | `	return SXRET_OK;` |
|        55 | 12503 | `}` |
|         - | 12504 | `/*` |
|         - | 12505 | ` * Compile a 'catch' block.` |
|         - | 12506 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12507 | ` * an object containing the exception information.` |
|         - | 12508 | ` */` |
|     24420 | 12509 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12510 | `{` |
|     24425 | 12511 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12512 | `	ph7_exception_block sCatch;` |
|         - | 12513 | `	SySet *pInstrContainer;` |
|         - | 12514 | `	SyString sClassName;` |
|         - | 12515 | `	GenBlock *pCatch;` |
|         - | 12516 | `	SyToken *pToken;` |
|         - | 12517 | `	SyString *pName;` |
|         - | 12518 | `	char *zDup;` |
|         - | 12519 | `	sxi32 rc;` |
|     24425 | 12520 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12521 | `	/* Zero the structure */` |
|     24425 | 12522 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12523 | `	/* Initialize fields */` |
|     24425 | 12524 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24425 | 12525 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24425 | 12526 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12527 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12528 | `			pToken = pGen->pIn;` |
|       ! 0 | 12529 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12530 | `				pToken--;` |
|       ! 0 | 12531 | `			}` |
|       ! 0 | 12532 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12533 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12534 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12535 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12536 | `				return SXERR_ABORT;` |
|         - | 12537 | `			}` |
|       ! 0 | 12538 | `			return SXERR_INVALID;` |
|         - | 12539 | `	}` |
|         - | 12540 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24425 | 12541 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12225 | 12542 | `	for(;;){` |
|         - | 12543 | `		SyBlob sResolved;` |
|     24455 | 12544 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24455 | 12545 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12546 | `			SyBlobRelease(&sResolved);` |
|         6 | 12547 | `			pToken = pGen->pIn;` |
|         6 | 12548 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12549 | `				pToken--;` |
|       ! 0 | 12550 | `			}` |
|         8 | 12551 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12552 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12553 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12554 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12555 | `				return SXERR_ABORT;` |
|         - | 12556 | `			}` |
|         6 | 12557 | `			return SXERR_INVALID;` |
|         - | 12558 | `		}` |
|         - | 12559 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12560 | `		 * transient SyBlob allocation. */` |
|     36674 | 12561 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24446 | 12562 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24451 | 12563 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24451 | 12564 | `		SyBlobRelease(&sResolved);` |
|     24451 | 12565 | `		if( zDup == 0 ){` |
|       ! 0 | 12566 | `			goto Mem;` |
|         - | 12567 | `		}` |
|     24451 | 12568 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24451 | 12569 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12570 | `			goto Mem;` |
|         - | 12571 | `		}` |
|         - | 12572 | `		/* Check for '\|' (multi-catch separator) */` |
|     24446 | 12573 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24446 | 12574 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        35 | 12575 | `			pGen->pIn->sData.nByte == 1 &&` |
|        30 | 12576 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        32 | 12577 | `			pGen->pIn++; /* Consume the '\|' */` |
|        32 | 12578 | `			continue;` |
|         - | 12579 | `		}` |
|     24421 | 12580 | `		break;` |
|       ! 0 | 12581 | `	}` |
|         - | 12582 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12583 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|         - | 12584 | `	 * jump straight to compiling the block below. */` |
|     24421 | 12585 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|         5 | 12586 | `		goto CatchBody;` |
|         - | 12587 | `	}` |
|     24412 | 12588 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24417 | 12589 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12590 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12591 | `			pToken = pGen->pIn;` |
|       ! 0 | 12592 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12593 | `				pToken--;` |
|       ! 0 | 12594 | `			}` |
|       ! 0 | 12595 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12596 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12597 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12598 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12599 | `				return SXERR_ABORT;` |
|         - | 12600 | `			}` |
|       ! 0 | 12601 | `			return SXERR_INVALID;` |
|         - | 12602 | `	}` |
|     24417 | 12603 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12604 | `	/* Duplicate instance name */` |
|     24417 | 12605 | `	pName = &pGen->pIn->sData;` |
|     24417 | 12606 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24417 | 12607 | `	if( zDup == 0 ){` |
|       ! 0 | 12608 | `		goto Mem;` |
|         - | 12609 | `	}` |
|     24417 | 12610 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24417 | 12611 | `	pGen->pIn++;` |
|     12208 | 12612 | `CatchBody:` |
|     24421 | 12613 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12614 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12615 | `		pToken = pGen->pIn;` |
|       ! 0 | 12616 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12617 | `			pToken--;` |
|       ! 0 | 12618 | `		}` |
|       ! 0 | 12619 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12620 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12621 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12622 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12623 | `			return SXERR_ABORT;` |
|         - | 12624 | `		}` |
|       ! 0 | 12625 | `		return SXERR_INVALID;` |
|         - | 12626 | `	}` |
|         - | 12627 | `	/* Compile the block */` |
|     24421 | 12628 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12629 | `	/* Create the catch block */` |
|     24421 | 12630 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24421 | 12631 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12632 | `		return SXERR_ABORT;` |
|         - | 12633 | `	}` |
|         - | 12634 | `	/* Swap bytecode container */` |
|     24421 | 12635 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24421 | 12636 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12637 | `	/* Compile the block */` |
|     24421 | 12638 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12639 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24421 | 12640 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12641 | `	/* Emit the DONE instruction */` |
|     24421 | 12642 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12643 | `	/* Leave the block */` |
|     24421 | 12644 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12645 | `	/* Restore the default container */` |
|     24421 | 12646 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12647 | `	/* Install the catch block */` |
|     24421 | 12648 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24421 | 12649 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12650 | `		goto Mem;` |
|         - | 12651 | `	}` |
|     24421 | 12652 | `	return SXRET_OK;` |
|       ! 0 | 12653 | `Mem:` |
|       ! 0 | 12654 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12655 | `	return SXERR_ABORT;` |
|     12215 | 12656 | `}` |
|         - | 12657 | `/*` |
|         - | 12658 | ` * Compile a 'try' block.` |
|         - | 12659 | ` * A function using an exception should be in a "try" block.` |
|         - | 12660 | ` * If the exception does not trigger, the code will continue` |
|         - | 12661 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12662 | ` * is "thrown".` |
|         - | 12663 | ` */` |
|     24578 | 12664 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12665 | `{` |
|         - | 12666 | `	ph7_exception *pException;` |
|     24583 | 12667 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12668 | `	GenBlock *pTry;` |
|         - | 12669 | `	sxu32 nJmpIdx;` |
|         - | 12670 | `	sxi32 rc;` |
|         - | 12671 | `	/* Create the exception container */` |
|     24583 | 12672 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24583 | 12673 | `	if( pException == 0 ){` |
|       ! 0 | 12674 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12675 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12676 | `		return SXERR_ABORT;` |
|         - | 12677 | `	}` |
|         - | 12678 | `	/* Zero the structure */` |
|     24583 | 12679 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12680 | `	/* Initialize fields */` |
|     24583 | 12681 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24583 | 12682 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24583 | 12683 | `	pException->iHasFinally = 0;` |
|     24583 | 12684 | `	pException->iFinallyDone = 0;` |
|     24583 | 12685 | `	pException->pVm = pGen->pVm;` |
|         - | 12686 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12687 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12688 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12689 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12690 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12691 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24583 | 12692 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       105 | 12693 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12694 | `	}` |
|         - | 12695 | `	/* Create the try block */` |
|     24483 | 12696 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24483 | 12697 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12698 | `		return SXERR_ABORT;` |
|         - | 12699 | `	}` |
|         - | 12700 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24483 | 12701 | `	pTry->pUserData = pException;` |
|         - | 12702 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24483 | 12703 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12704 | `	/* Fix the jump later when the destination is resolved */` |
|     24483 | 12705 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24483 | 12706 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12707 | `	/* Compile the block */` |
|     24483 | 12708 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24483 | 12709 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12710 | `		return SXERR_ABORT;` |
|         - | 12711 | `	}` |
|         - | 12712 | `	/* Fix forward jumps now the destination is resolved */` |
|     24483 | 12713 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12714 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24483 | 12715 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12716 | `	/* Leave the block */` |
|     24483 | 12717 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12718 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24483 | 12719 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24476 | 12720 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12721 | `		/* Compile one or more catch blocks */` |
|     24416 | 12722 | `		for(;;){` |
|     48832 | 12723 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36681 | 12724 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12211 | 12725 | `					break;` |
|         - | 12726 | `			}` |
|     24425 | 12727 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24425 | 12728 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12729 | `				return SXERR_ABORT;` |
|         - | 12730 | `			}` |
|         5 | 12731 | `		}` |
|     12206 | 12732 | `	}` |
|         - | 12733 | `	/* Compile optional finally block */` |
|     24483 | 12734 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       736 | 12735 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12736 | `		SySet *pInstrContainer;` |
|         - | 12737 | `		GenBlock *pFinBlock;` |
|       129 | 12738 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12739 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12740 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12741 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12742 | `			return SXERR_ABORT;` |
|         - | 12743 | `		}` |
|         - | 12744 | `		/* Swap bytecode container */` |
|       129 | 12745 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12746 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12747 | `		/* Compile the finally body */` |
|       129 | 12748 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12749 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12750 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12751 | `			return SXERR_ABORT;` |
|         - | 12752 | `		}` |
|         - | 12753 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12754 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12755 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12756 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12757 | `		/* Leave the block */` |
|       129 | 12758 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12759 | `		/* Restore the default container */` |
|       129 | 12760 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12761 | `		pException->iHasFinally = 1;` |
|        62 | 12762 | `	}` |
|         - | 12763 | `	/* Must have at least one catch or finally */` |
|     24483 | 12764 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12765 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12766 | `			"Cannot use try without catch or finally");` |
|         8 | 12767 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12768 | `			return SXERR_ABORT;` |
|         - | 12769 | `		}` |
|         3 | 12770 | `	}` |
|     24483 | 12771 | `	return SXRET_OK;` |
|     12294 | 12772 | `}` |
|         - | 12773 | `/*` |
|         - | 12774 | ` * Compile a switch block.` |
|         - | 12775 | ` *  (See block-comment below for more information)` |
|         - | 12776 | ` */` |
|     53536 | 12777 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12778 | `{` |
|     53541 | 12779 | `	sxi32 rc = SXRET_OK;` |
|     53541 | 12780 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12781 | `		/* Unexpected token */` |
|       ! 0 | 12782 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12783 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12784 | `			return SXERR_ABORT;` |
|         - | 12785 | `		}` |
|       ! 0 | 12786 | `		pGen->pIn++;` |
|       ! 0 | 12787 | `	}` |
|     53541 | 12788 | `	pGen->pIn++;` |
|         - | 12789 | `	/* First instruction to execute in this block. */` |
|     53541 | 12790 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12791 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12792 | `	 * or the '}' token */` |
|     38366 | 12793 | `	for(;;){` |
|     76737 | 12794 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12795 | `			/* No more input to process */` |
|       ! 0 | 12796 | `			break;` |
|         - | 12797 | `		}` |
|     76737 | 12798 | `		rc = SXRET_OK;` |
|     76737 | 12799 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      3901 | 12800 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      3847 | 12801 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12802 | `					/* Unexpected token */` |
|       ! 0 | 12803 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12804 | `						&pGen->pIn->sData);` |
|       ! 0 | 12805 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12806 | `						return SXERR_ABORT;` |
|         - | 12807 | `					}` |
|         - | 12808 | `					/* FALL THROUGH */` |
|       ! 0 | 12809 | `				}` |
|      3847 | 12810 | `				rc = SXERR_EOF;` |
|      3847 | 12811 | `				break;` |
|         - | 12812 | `			}` |
|        32 | 12813 | `		}else{` |
|         - | 12814 | `			sxi32 nKwrd;` |
|         - | 12815 | `			/* Extract the keyword */` |
|     72841 | 12816 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     72841 | 12817 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|     24851 | 12818 | `				break;` |
|         - | 12819 | `			}` |
|     23149 | 12820 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12821 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12822 | `					/* Unexpected token */` |
|       ! 0 | 12823 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12824 | `						&pGen->pIn->sData);` |
|       ! 0 | 12825 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12826 | `						return SXERR_ABORT;` |
|         - | 12827 | `					}` |
|         - | 12828 | `					/* FALL THROUGH */` |
|       ! 0 | 12829 | `				}` |
|         - | 12830 | `				/* Block compiled */` |
|         3 | 12831 | `				break;` |
|         - | 12832 | `			}` |
|         - | 12833 | `		}` |
|         - | 12834 | `		/* Compile block */` |
|     23201 | 12835 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     23201 | 12836 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12837 | `			return SXERR_ABORT;` |
|         - | 12838 | `		}` |
|         5 | 12839 | `	}` |
|     53541 | 12840 | `	return rc;` |
|     26773 | 12841 | `}` |
|         - | 12842 | `/*` |
|         - | 12843 | ` * Compile a case eXpression.` |
|         - | 12844 | ` *  (See block-comment below for more information)` |
|         - | 12845 | ` */` |
|     53516 | 12846 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12847 | `{` |
|         - | 12848 | `	SySet *pInstrContainer;` |
|         - | 12849 | `	SyToken *pEnd,*pTmp;` |
|     53521 | 12850 | `	sxi32 iNest = 0;` |
|         - | 12851 | `	sxi32 rc;` |
|         - | 12852 | `	/* Delimit the expression */` |
|     53521 | 12853 | `	pEnd = pGen->pIn;` |
|    107045 | 12854 | `	while( pEnd < pGen->pEnd ){` |
|    107045 | 12855 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12856 | `			/* Increment nesting level */` |
|         3 | 12857 | `			iNest++;` |
|    107044 | 12858 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12859 | `			/* Decrement nesting level */` |
|         3 | 12860 | `			iNest--;` |
|    107042 | 12861 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     53521 | 12862 | `			break;` |
|         - | 12863 | `		}` |
|     53529 | 12864 | `		pEnd++;` |
|         5 | 12865 | `	}` |
|     53521 | 12866 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12867 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12868 | `		if( rc == SXERR_ABORT ){` |
|         - | 12869 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12870 | `			return SXERR_ABORT;` |
|         - | 12871 | `		}` |
|       ! 0 | 12872 | `	}` |
|         - | 12873 | `	/* Swap token stream */` |
|     53521 | 12874 | `	pTmp = pGen->pEnd;` |
|     53521 | 12875 | `	pGen->pEnd = pEnd;` |
|     53521 | 12876 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     53521 | 12877 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     53521 | 12878 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12879 | `	/* Emit the done instruction */` |
|     53521 | 12880 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     53521 | 12881 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12882 | `	/* Update token stream */` |
|     53521 | 12883 | `	pGen->pIn  = pEnd;` |
|     53521 | 12884 | `	pGen->pEnd = pTmp;` |
|     53521 | 12885 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12886 | `		return SXERR_ABORT;` |
|         - | 12887 | `	}` |
|     53521 | 12888 | `	return SXRET_OK;` |
|     26763 | 12889 | `}` |
|         - | 12890 | `/*` |
|         - | 12891 | ` * Compile the smart switch statement.` |
|         - | 12892 | ` * According to the PHP language reference manual` |
|         - | 12893 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12894 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12895 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12896 | ` *  This is exactly what the switch statement is for.` |
|         - | 12897 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12898 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12899 | ` *  of the outer loop, use continue 2.` |
|         - | 12900 | ` *  Note that switch/case does loose comparision.` |
|         - | 12901 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12902 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12903 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12904 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12905 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12906 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12907 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12908 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12909 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12910 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12911 | ` *  list for the next case.` |
|         - | 12912 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12913 | ` *  or floating-point numbers and strings.` |
|         - | 12914 | ` */` |
|      3844 | 12915 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12916 | `{` |
|         - | 12917 | `	GenBlock *pSwitchBlock;` |
|         - | 12918 | `	SyToken *pTmp,*pEnd;` |
|         - | 12919 | `	ph7_switch *pSwitch;` |
|         - | 12920 | `	sxu32 nToken;` |
|         - | 12921 | `	sxu32 nLine;` |
|         - | 12922 | `	sxi32 rc;` |
|      3849 | 12923 | `	nLine = pGen->pIn->nLine;` |
|         - | 12924 | `	/* Jump the 'switch' keyword */` |
|      3849 | 12925 | `	pGen->pIn++;` |
|      3849 | 12926 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12927 | `		/* Syntax error */` |
|       ! 0 | 12928 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12929 | `		if( rc == SXERR_ABORT ){` |
|         - | 12930 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12931 | `			return SXERR_ABORT;` |
|         - | 12932 | `		}` |
|       ! 0 | 12933 | `		goto Synchronize;` |
|         - | 12934 | `	}` |
|         - | 12935 | `	/* Jump the left parenthesis '(' */` |
|      3849 | 12936 | `	pGen->pIn++;` |
|      3849 | 12937 | `	pEnd = 0; /* cc warning */` |
|         - | 12938 | `	/* Create the loop block */` |
|      5771 | 12939 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      1922 | 12940 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      3849 | 12941 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12942 | `		return SXERR_ABORT;` |
|         - | 12943 | `	}` |
|         - | 12944 | `	/* Delimit the condition */` |
|      3849 | 12945 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      3849 | 12946 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12947 | `		/* Empty expression */` |
|       ! 0 | 12948 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12949 | `		if( rc == SXERR_ABORT ){` |
|         - | 12950 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12951 | `			return SXERR_ABORT;` |
|         - | 12952 | `		}` |
|       ! 0 | 12953 | `	}` |
|         - | 12954 | `	/* Swap token streams */` |
|      3849 | 12955 | `	pTmp = pGen->pEnd;` |
|      3849 | 12956 | `	pGen->pEnd = pEnd;` |
|         - | 12957 | `	/* Compile the expression */` |
|      3849 | 12958 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      3849 | 12959 | `	if( rc == SXERR_ABORT ){` |
|         - | 12960 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12961 | `		return SXERR_ABORT;` |
|         - | 12962 | `	}` |
|         - | 12963 | `	/* Update token stream */` |
|      3849 | 12964 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12965 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12966 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12967 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12968 | `			return SXERR_ABORT;` |
|         - | 12969 | `		}` |
|       ! 0 | 12970 | `		pGen->pIn++;` |
|       ! 0 | 12971 | `	}` |
|      3849 | 12972 | `	pGen->pIn  = &pEnd[1];` |
|      3849 | 12973 | `	pGen->pEnd = pTmp;` |
|      3849 | 12974 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      3844 | 12975 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12976 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12977 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12978 | `				pTmp--;` |
|       ! 0 | 12979 | `			}` |
|         - | 12980 | `			/* Unexpected token */` |
|       ! 0 | 12981 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12982 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12983 | `				return SXERR_ABORT;` |
|         - | 12984 | `			}` |
|       ! 0 | 12985 | `			goto Synchronize;` |
|         - | 12986 | `	}` |
|         - | 12987 | `	/* Set the delimiter token */` |
|      3849 | 12988 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12989 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12990 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12991 | `	}else{` |
|      3847 | 12992 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12993 | `	}` |
|      3849 | 12994 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12995 | `	/* Create the switch blocks container */` |
|      3849 | 12996 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      3849 | 12997 | `	if( pSwitch == 0 ){` |
|         - | 12998 | `		/* Abort compilation */` |
|       ! 0 | 12999 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 13000 | `		return SXERR_ABORT;` |
|         - | 13001 | `	}` |
|         - | 13002 | `	/* Zero the structure */` |
|      3849 | 13003 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 13004 | `	/* Initialize fields */` |
|      3849 | 13005 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 13006 | `	/* Emit the switch instruction */` |
|      3849 | 13007 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 13008 | `	/* Compile case blocks */` |
|     51616 | 13009 | `	for(;;){` |
|         - | 13010 | `		sxu32 nKwrd;` |
|     53543 | 13011 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 13012 | `			/* No more input to process */` |
|       ! 0 | 13013 | `			break;` |
|         - | 13014 | `		}` |
|     53543 | 13015 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 13016 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 13017 | `				/* Unexpected token */` |
|       ! 0 | 13018 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13019 | `					&pGen->pIn->sData);` |
|       ! 0 | 13020 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13021 | `					return SXERR_ABORT;` |
|         - | 13022 | `				}` |
|         - | 13023 | `				/* FALL THROUGH */` |
|       ! 0 | 13024 | `			}` |
|         - | 13025 | `			/* Block compiled */` |
|       ! 0 | 13026 | `			break;` |
|         - | 13027 | `		}` |
|         - | 13028 | `		/* Extract the keyword */` |
|     53543 | 13029 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     53543 | 13030 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 13031 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 13032 | `				/* Unexpected token */` |
|       ! 0 | 13033 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13034 | `					&pGen->pIn->sData);` |
|       ! 0 | 13035 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13036 | `					return SXERR_ABORT;` |
|         - | 13037 | `				}` |
|         - | 13038 | `				/* FALL THROUGH */` |
|       ! 0 | 13039 | `			}` |
|         - | 13040 | `			/* Block compiled */` |
|         3 | 13041 | `			break;` |
|         - | 13042 | `		}` |
|     53541 | 13043 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 13044 | `			/*` |
|         - | 13045 | `			 * Accroding to the PHP language reference manual` |
|         - | 13046 | `			 *  A special case is the default case. This case matches anything` |
|         - | 13047 | `			 *  that wasn't matched by the other cases.` |
|         - | 13048 | `			 */` |
|        25 | 13049 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 13050 | `				/* Default case already compiled */` |
|       ! 0 | 13051 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 13052 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13053 | `					return SXERR_ABORT;` |
|         - | 13054 | `				}` |
|       ! 0 | 13055 | `			}` |
|        25 | 13056 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 13057 | `			/* Compile the default block */` |
|        25 | 13058 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 13059 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13060 | `				return SXERR_ABORT;` |
|        25 | 13061 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 13062 | `				break;` |
|         1 | 13063 | `			}` |
|     53522 | 13064 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 13065 | `			ph7_case_expr sCase;` |
|         - | 13066 | `			/* Standard case block */` |
|     53521 | 13067 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 13068 | `			/* initialize the structure */` |
|     53521 | 13069 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 13070 | `			/* Compile the case expression */` |
|     53521 | 13071 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     53521 | 13072 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13073 | `				return SXERR_ABORT;` |
|         - | 13074 | `			}` |
|         - | 13075 | `			/* Compile the case block */` |
|     53521 | 13076 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 13077 | `			/* Insert in the switch container */` |
|     53521 | 13078 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     53521 | 13079 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13080 | `				return SXERR_ABORT;` |
|     53521 | 13081 | `			}else if( rc == SXERR_EOF ){` |
|      3829 | 13082 | `				break;` |
|         - | 13083 | `			}` |
|     24851 | 13084 | `		}else{` |
|         - | 13085 | `			/* Unexpected token */` |
|       ! 0 | 13086 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13087 | `				&pGen->pIn->sData);` |
|       ! 0 | 13088 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13089 | `				return SXERR_ABORT;` |
|         - | 13090 | `			}` |
|       ! 0 | 13091 | `			break;` |
|         - | 13092 | `		}` |
|         5 | 13093 | `	}` |
|         - | 13094 | `	/* Fix all jumps now the destination is resolved */` |
|      3849 | 13095 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      3849 | 13096 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 13097 | `	/* Release the loop block */` |
|      3849 | 13098 | `	GenStateLeaveBlock(pGen,0);` |
|      3849 | 13099 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 13100 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      3849 | 13101 | `		pGen->pIn++;` |
|      1922 | 13102 | `	}` |
|         - | 13103 | `	/* Statement successfully compiled */` |
|      3849 | 13104 | `	return SXRET_OK;` |
|       ! 0 | 13105 | `Synchronize:` |
|         - | 13106 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 13107 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 13108 | `		pGen->pIn++;` |
|       ! 0 | 13109 | `	}` |
|       ! 0 | 13110 | `	return SXRET_OK;` |
|      1927 | 13111 | `}` |
|         - | 13112 | `/*` |
|         - | 13113 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 13114 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 13115 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 13116 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 13117 | ` */` |
|         - | 13118 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 13119 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 13120 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 13121 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 13122 |  |
|         - | 13123 | `/*` |
|         - | 13124 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 13125 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 13126 | ` * patched entries from the pending set.` |
|         - | 13127 | ` */` |
|  48203414 | 13128 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 13129 | `{` |
|  48203419 | 13130 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 13131 | `	sxu32 nTarget;` |
|         - | 13132 | `	sxu32 *aIdx;` |
|         - | 13133 | `	sxu32 i;` |
|  48203419 | 13134 | `	if( nCur <= nBaseline ){` |
|  48203323 | 13135 | `		return;` |
|         - | 13136 | `	}` |
|       100 | 13137 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 13138 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 13139 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 13140 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 13141 | `		if( pInstr ){` |
|       108 | 13142 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 13143 | `		}` |
|        56 | 13144 | `	}` |
|       100 | 13145 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  24101712 | 13146 | `}` |
|         - | 13147 |  |
|         - | 13148 | `/*` |
|         - | 13149 | ` * By-reference out-parameters of builtin functions.` |
|         - | 13150 | ` *` |
|         - | 13151 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 13152 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 13153 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 13154 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 13155 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 13156 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 13157 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 13158 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 13159 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 13160 | ` * creates it" behaviour).` |
|         - | 13161 | ` *` |
|         - | 13162 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 13163 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 13164 | ` */` |
|   6135582 | 13165 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 13166 | `{` |
|         - | 13167 | `	static const struct {` |
|         - | 13168 | `		const char *zName;` |
|         - | 13169 | `		sxu32 nByte;` |
|         - | 13170 | `		sxu32 mask;` |
|         - | 13171 | `	} aByRef[] = {` |
|         - | 13172 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - | 13173 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13174 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13175 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13176 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13177 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 13178 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 13179 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 13180 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 13181 | `	};` |
|         - | 13182 | `	sxu32 i;` |
|   6135587 | 13183 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1618991 | 13184 | `		return 0;` |
|         - | 13185 | `	}` |
|  44749111 | 13186 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  40289894 | 13187 | `		if( pName->nByte == aByRef[i].nByte` |
|  21258219 | 13188 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     57389 | 13189 | `			return aByRef[i].mask;` |
|         - | 13190 | `		}` |
|  20116260 | 13191 | `	}` |
|   4459217 | 13192 | `	return 0;` |
|   3067796 | 13193 | `}` |
|         - | 13194 | `/*` |
|         - | 13195 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 13196 | ` *` |
|         - | 13197 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 13198 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 13199 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 13200 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 13201 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 13202 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 13203 | ` */` |
|   6135582 | 13204 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 13205 | `{` |
|         - | 13206 | `	SyToken *p, *pEnd;` |
|   6135587 | 13207 | `	pOut->zString = 0;` |
|   6135587 | 13208 | `	pOut->nByte = 0;` |
|   6135587 | 13209 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 13210 | `		return;` |
|         - | 13211 | `	}` |
|   6135587 | 13212 | `	p = pLeft->pStart;` |
|   6135587 | 13213 | `	pEnd = pLeft->pEnd;` |
|         - | 13214 | `	/* Optional single leading namespace separator (absolute path). */` |
|   6135587 | 13215 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3859 | 13216 | `		p++;` |
|      1927 | 13217 | `	}` |
|   6135587 | 13218 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1618953 | 13219 | `		return;` |
|         - | 13220 | `	}` |
|         - | 13221 | `	/* Must be a single component: nothing follows the name token. */` |
|   4516639 | 13222 | `	if( p + 1 != pEnd ){` |
|        42 | 13223 | `		return;` |
|         - | 13224 | `	}` |
|   4516601 | 13225 | `	*pOut = p->sData;` |
|   3067796 | 13226 | `}` |
|         - | 13227 | `/*` |
|         - | 13228 | ` * Generate bytecode for a given expression tree.` |
|         - | 13229 | ` * If something goes wrong while generating bytecode` |
|         - | 13230 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 13231 | ` * this function takes care of generating the appropriate` |
|         - | 13232 | ` * error message.` |
|         - | 13233 | ` */` |
|  67106292 | 13234 | `static sxi32 GenStateEmitExprCode(` |
|         - | 13235 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 13236 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 13237 | `	sxi32 iFlags /* Control flags */` |
|         - | 13238 | `	)` |
|         5 | 13239 | `{` |
|         - | 13240 | `	VmInstr *pInstr;` |
|         - | 13241 | `	sxu32 nJmpIdx;` |
|  67106297 | 13242 | `	sxi32 iP1 = 0;` |
|  67106297 | 13243 | `	sxu32 iP2 = 0;` |
|  67106297 | 13244 | `	void *p3  = 0;` |
|         - | 13245 | `	sxi32 iVmOp;` |
|         - | 13246 | `	sxi32 rc;` |
|  67106297 | 13247 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  67106297 | 13248 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  67106297 | 13249 | `	sxu32 nRhsNsBase = 0;` |
|  67106297 | 13250 | `	if( pNode->xCode ){` |
|         - | 13251 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13252 | `		/* Compile node */` |
|  40478719 | 13253 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  40478719 | 13254 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  40478719 | 13255 | `		RE_SWAP_DELIMITER(pGen);` |
|  40478719 | 13256 | `		return rc;` |
|         - | 13257 | `	}` |
|  26627583 | 13258 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13259 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13260 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13261 | `		return SXERR_ABORT;` |
|         - | 13262 | `	}` |
|  26627583 | 13263 | `	iVmOp = pNode->pOp->iVmOp;` |
|  26627583 | 13264 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13265 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13266 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13267 | `		 * and later errors are still reported. */` |
|         3 | 13268 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13269 | `			"The (unset) cast is no longer supported");` |
|         3 | 13270 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13271 | `			return SXERR_ABORT;` |
|         - | 13272 | `		}` |
|         1 | 13273 | `	}` |
|  26627583 | 13274 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        93 | 13275 | `		sxu32 nJmp = 0;` |
|         - | 13276 | `		sxu32 nNcNsBase;` |
|         - | 13277 | `		VmInstr *pInstrFix;` |
|         - | 13278 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13279 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13280 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13281 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13282 | `		 * stack slot carries a writable nIdx. */` |
|        93 | 13283 | `		if( pNode->pRight ){` |
|        93 | 13284 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13285 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        93 | 13286 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13287 | `				return rc;` |
|         - | 13288 | `			}` |
|        93 | 13289 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13290 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13291 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13292 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13293 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13294 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13295 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13296 | `			 * cascade for the actual write path stays correct. */` |
|        93 | 13297 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        93 | 13298 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13299 | `				pInstrFix->iP2 = 3;` |
|        15 | 13300 | `			}` |
|        45 | 13301 | `		}` |
|         - | 13302 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        93 | 13303 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13304 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        93 | 13305 | `		if( pNode->pLeft ){` |
|        93 | 13306 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13307 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        93 | 13308 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13309 | `				return rc;` |
|         - | 13310 | `			}` |
|        93 | 13311 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        45 | 13312 | `		}` |
|         - | 13313 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        93 | 13314 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13315 | `		/* Patch the short-circuit jump to land after the store. */` |
|        93 | 13316 | `		if( nJmp > 0 ){` |
|        93 | 13317 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        93 | 13318 | `			if( pInstrFix ){` |
|        93 | 13319 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        45 | 13320 | `			}` |
|        45 | 13321 | `		}` |
|        93 | 13322 | `		return SXRET_OK;` |
|         - | 13323 | `	}` |
|  26627493 | 13324 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13325 | `		sxu32 nJz,nJmp;` |
|         - | 13326 | `		sxu32 nTernaryNsBase;` |
|         - | 13327 | `		/* Ternary operator require special handling */` |
|         - | 13328 | `		/* Phase#1: Compile the condition */` |
|    457439 | 13329 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    457439 | 13330 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    457439 | 13331 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13332 | `			return rc;` |
|         - | 13333 | `		}` |
|         - | 13334 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13335 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13336 | `		 * condition expression, not leak past the ternary. */` |
|    457439 | 13337 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    457439 | 13338 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    457439 | 13339 | `		if( pNode->pLeft ){` |
|         - | 13340 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13341 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    453555 | 13342 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13343 | `			/* Phase#3: Compile the 'then' expression  */` |
|    453555 | 13344 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    453555 | 13345 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    453555 | 13346 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13347 | `				return rc;` |
|         - | 13348 | `			}` |
|    453555 | 13349 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    226780 | 13350 | `		}else{` |
|         - | 13351 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13352 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13353 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3889 | 13354 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3889 | 13355 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13356 | `		}` |
|         - | 13357 | `		/* Phase#4: Emit the unconditional jump */` |
|    457439 | 13358 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13359 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    457439 | 13360 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    457439 | 13361 | `		if( pInstr ){` |
|    457439 | 13362 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    228717 | 13363 | `		}` |
|    457439 | 13364 | `		if( !pNode->pLeft ){` |
|         - | 13365 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3889 | 13366 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1942 | 13367 | `		}` |
|         - | 13368 | `		/* Phase#6: Compile the 'else' expression */` |
|    457439 | 13369 | `		if( pNode->pRight ){` |
|    457439 | 13370 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    457439 | 13371 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    457439 | 13372 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13373 | `				return rc;` |
|         - | 13374 | `			}` |
|    457439 | 13375 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    228717 | 13376 | `		}` |
|    457439 | 13377 | `		if( nJmp > 0 ){` |
|         - | 13378 | `			/* Phase#7: Fix the unconditional jump */` |
|    457439 | 13379 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    457439 | 13380 | `			if( pInstr ){` |
|    457439 | 13381 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    228717 | 13382 | `			}` |
|    228717 | 13383 | `		}` |
|         - | 13384 | `		/* All done */` |
|    457439 | 13385 | `		return SXRET_OK;` |
|         - | 13386 | `	}` |
|  26170059 | 13387 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13388 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13389 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13390 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13391 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13392 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13393 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13394 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13395 | `		sxu32 nPipeNsBase;` |
|        27 | 13396 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13397 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13398 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13399 | `				"'\|>': Missing operand");` |
|       ! 0 | 13400 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13401 | `		}` |
|         - | 13402 | `		/* Argument: the LHS value. */` |
|        27 | 13403 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13404 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13405 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13406 | `			return rc;` |
|         - | 13407 | `		}` |
|        27 | 13408 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13409 | `		/* Callable: the RHS. */` |
|        27 | 13410 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13411 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13412 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13413 | `			return rc;` |
|         - | 13414 | `		}` |
|        27 | 13415 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13416 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13417 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13418 | `		return SXRET_OK;` |
|         - | 13419 | `	}` |
|  26170033 | 13420 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13421 | `	/* Generate code for the left tree */` |
|  26170033 | 13422 | `	if( pNode->pLeft ){` |
|  26147143 | 13423 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  26147143 | 13424 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13425 | `			ph7_expr_node **apNode;` |
|   6139729 | 13426 | `			int hasSpread = 0;` |
|   6139729 | 13427 | `			int hasNamed = 0;` |
|   6139729 | 13428 | `			int bAnySpread = 0;` |
|   6139729 | 13429 | `			sxu32 byRefMask = 0;` |
|         - | 13430 | `			sxi32 nArgs;` |
|         - | 13431 | `			sxi32 n;` |
|         - | 13432 | `			/* Recurse and generate bytecodes for function arguments */` |
|   6139729 | 13433 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6139729 | 13434 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13435 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13436 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13437 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   6139729 | 13438 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13439 | `				bFcc = 1;` |
|        81 | 13440 | `				nArgs = 0;` |
|        40 | 13441 | `			}` |
|         - | 13442 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13443 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13444 | `			{` |
|   6139729 | 13445 | `				int seenNamed = 0;` |
|   6139729 | 13446 | `				int seenSpread = 0;` |
|  12915599 | 13447 | `				for( n = 0; n < nArgs; ++n ){` |
|   6775877 | 13448 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4015 | 13449 | `						bAnySpread = 1;` |
|      4015 | 13450 | `						seenSpread = 1;` |
|      4015 | 13451 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13452 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13453 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13454 | `							return SXERR_SYNTAX;` |
|         5 | 13455 | `						}` |
|   6773872 | 13456 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13457 | `						seenNamed = 1;` |
|       289 | 13458 | `						hasNamed = 1;` |
|   6771725 | 13459 | `					}else if( seenNamed ){` |
|         3 | 13460 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13461 | `							"Cannot use positional argument after named argument");` |
|         3 | 13462 | `						return SXERR_SYNTAX;` |
|   6771581 | 13463 | `					}else if( seenSpread ){` |
|       ! 0 | 13464 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13465 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13466 | `						return SXERR_SYNTAX;` |
|         - | 13467 | `					}` |
|   3387940 | 13468 | `				}` |
|         - | 13469 | `			}` |
|         - | 13470 | `			/* Read-only load */` |
|   6139727 | 13471 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13472 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13473 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13474 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13475 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   6139727 | 13476 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   6139727 | 13477 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   6139722 | 13478 | `				if( pCallName->nByte == 5` |
|   3443342 | 13479 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    309473 | 13480 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5984993 | 13481 | `				}else if( pCallName->nByte == 5` |
|   3133874 | 13482 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       109 | 13483 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        52 | 13484 | `				}` |
|         - | 13485 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13486 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13487 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13488 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13489 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13490 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   6139727 | 13491 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13492 | `					SyString sBuiltin;` |
|   6135587 | 13493 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   6135587 | 13494 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3067791 | 13495 | `				}` |
|   3069861 | 13496 | `			}` |
|  12915595 | 13497 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   6775873 | 13498 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   6775873 | 13499 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13500 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13501 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13502 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13503 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13504 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13505 | `				 * (iP1=0 either way). */` |
|   6775873 | 13506 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38245 | 13507 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38245 | 13508 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19120 | 13509 | `				}` |
|   6775873 | 13510 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   6775873 | 13511 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13512 | `					return rc;` |
|         - | 13513 | `				}` |
|         - | 13514 | `				/* Each argument is an independent nullsafe scope. */` |
|   6775873 | 13515 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   6775873 | 13516 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13517 | `					/* Emit spread opcode to unpack this array argument */` |
|      4015 | 13518 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4015 | 13519 | `					hasSpread = 1;` |
|      2005 | 13520 | `				}` |
|   3387939 | 13521 | `			}` |
|         - | 13522 | `			/* Total number of given arguments */` |
|   6139727 | 13523 | `			iP1 = nArgs;` |
|   6139727 | 13524 | `			iP2 = hasSpread;` |
|         - | 13525 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13526 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   6139727 | 13527 | `			if( hasNamed ){` |
|       178 | 13528 | `				sxu32 nStrBytes = 0;` |
|         - | 13529 | `				char *zBuf;` |
|       534 | 13530 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13531 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13532 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13533 | `					}` |
|       182 | 13534 | `				}` |
|         - | 13535 | `				{` |
|       178 | 13536 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13537 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13538 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13539 | `				if( pMap ){` |
|       178 | 13540 | `					SyZero(pMap, mapSize);` |
|       178 | 13541 | `					pMap->bHasNamed = 1;` |
|       178 | 13542 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13543 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13544 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13545 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13546 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13547 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13548 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13549 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13550 | `							zBuf += nb;` |
|       141 | 13551 | `						}` |
|         - | 13552 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13553 | `					}` |
|       178 | 13554 | `					p3 = (void *)pMap;` |
|        87 | 13555 | `				}` |
|         - | 13556 | `				}` |
|        87 | 13557 | `			}` |
|         - | 13558 | `			/* Remove stale flags now */` |
|   6139727 | 13559 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3069861 | 13560 | `		}` |
|         - | 13561 | `		{` |
|         - | 13562 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13563 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13564 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13565 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13566 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13567 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13568 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13569 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  26147141 | 13570 | `			sxi32 iLeftFlags = iFlags;` |
|  26147136 | 13571 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  21497179 | 13572 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   8423637 | 13573 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7303391 | 13574 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2420387 | 13575 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1210191 | 13576 | `			}` |
|         - | 13577 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13578 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13579 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13580 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13581 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13582 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13583 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  26147136 | 13584 | `			if( pNode->pOp` |
|  36933699 | 13585 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  23860178 | 13586 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  21573168 | 13587 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4941305 | 13588 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2470650 | 13589 | `			}` |
|         - | 13590 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13591 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13592 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13593 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13594 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13595 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  26147136 | 13596 | `			if( pNode->pOp` |
|  26147141 | 13597 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    195121 | 13598 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     97558 | 13599 | `			}` |
|  26147141 | 13600 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13601 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13602 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     11671 | 13603 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      5833 | 13604 | `			}` |
|  26147141 | 13605 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13606 | `		}` |
|  26147141 | 13607 | `		if( rc != SXRET_OK ){` |
|        34 | 13608 | `			return rc;` |
|         - | 13609 | `		}` |
|  26147111 | 13610 | `		if( !bIsChainOp ){` |
|         - | 13611 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13612 | `			 * target the end of that LHS chain, which is right here. */` |
|  12208387 | 13613 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6104191 | 13614 | `		}` |
|  26147111 | 13615 | `		if( iVmOp == PH7_OP_CALL ){` |
|   6139727 | 13616 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6139727 | 13617 | `			if( pInstr ){` |
|   6139727 | 13618 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   4516889 | 13619 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13620 | `					sxu32 nQual;` |
|   4516889 | 13621 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13622 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13623 | `					 * so the later NEW handler (if any) can see it. */` |
|   4516889 | 13624 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13625 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13626 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13627 | `					 * imports — class imports must NOT affect function` |
|         - | 13628 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13629 | `					 * before NEW; we store the original literal index in the` |
|         - | 13630 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13631 | `					 * the unqualified name and re-qualify with class imports. */` |
|   4516889 | 13632 | `					if( bAbsolute ){` |
|      3859 | 13633 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1932 | 13634 | `					}else{` |
|   4513035 | 13635 | `						int fromImport = 0;` |
|   4513035 | 13636 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   4513035 | 13637 | `						pInstr->iP2 = (sxi32)nQual;` |
|   4513035 | 13638 | `						if( nQual != nOrig ){` |
|         - | 13639 | `							/* Record the original literal index in the arg map` |
|         - | 13640 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13641 | `							 * flag) so the NEW handler can recover the` |
|         - | 13642 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13643 | `							 * imports. */` |
|        97 | 13644 | `							if( p3 == 0 ){` |
|        97 | 13645 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        92 | 13646 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        97 | 13647 | `								if( pMap ){` |
|        97 | 13648 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        97 | 13649 | `									p3 = (void *)pMap;` |
|        46 | 13650 | `								}` |
|        46 | 13651 | `							}` |
|        97 | 13652 | `							if( p3 ){` |
|        97 | 13653 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        97 | 13654 | `								if( !fromImport ){` |
|         - | 13655 | `									/* Mark as namespace-qualified */` |
|        87 | 13656 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        41 | 13657 | `								}` |
|        46 | 13658 | `							}` |
|        46 | 13659 | `						}` |
|         5 | 13660 | `					}` |
|   3881285 | 13661 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13662 | `					/* Method call,flag that */` |
|   1603105 | 13663 | `					pInstr->iP2 = 1;` |
|    801550 | 13664 | `				}` |
|   3069866 | 13665 | `			}` |
|  23077250 | 13666 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13667 | `			ph7_expr_node **apNode;` |
|         - | 13668 | `			sxi32 n;` |
|   2857707 | 13669 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13670 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13671 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13672 | `			/* Recurse and generate bytecodes for array index */` |
|   2857707 | 13673 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5497503 | 13674 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2639801 | 13675 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2639801 | 13676 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2639801 | 13677 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13678 | `					return rc;` |
|         - | 13679 | `				}` |
|         - | 13680 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2639801 | 13681 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1319903 | 13682 | `			}` |
|   2857707 | 13683 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2639801 | 13684 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1319898 | 13685 | `			}` |
|   2857707 | 13686 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13687 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    347517 | 13688 | `				iP2 = 4;` |
|   2683951 | 13689 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13690 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13691 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     22977 | 13692 | `				iP2 = 5;` |
|   2498709 | 13693 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13694 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13695 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13696 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        31 | 13697 | `				iP2 = 6;` |
|   2487210 | 13698 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13699 | `				/* Create an empty entry when the desired index is not found */` |
|    523731 | 13700 | `				iP2 = 1;` |
|    261868 | 13701 | `			}` |
|  18578538 | 13702 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13703 | `			/* POP the left node */` |
|         5 | 13704 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13705 | `		}` |
|  13073553 | 13706 | `	}` |
|  26170001 | 13707 | `	rc = SXRET_OK;` |
|  26170001 | 13708 | `	nJmpIdx = 0;` |
|         - | 13709 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13710 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13711 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  26170001 | 13712 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    390175 | 13713 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    390175 | 13714 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    390175 | 13715 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    390175 | 13716 | `			int isSpecial = 0;` |
|    390175 | 13717 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    344367 | 13718 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    344367 | 13719 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    344362 | 13720 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    313758 | 13721 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    170237 | 13722 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    103189 | 13723 | `					isSpecial = 1;` |
|     51592 | 13724 | `				}` |
|    183633 | 13725 | `			}` |
|    413079 | 13726 | `			pInstr->iP1 = 0;` |
|         - | 13727 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 13728 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 13729 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 13730 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 13731 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 13732 | `			{` |
|    596712 | 13733 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    550899 | 13734 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    367271 | 13735 | `				if( !isSpecial && !bAbsolute ){` |
|    264071 | 13736 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    132033 | 13737 | `				}` |
|         - | 13738 | `			}` |
|         - | 13739 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13740 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    367271 | 13741 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    264087 | 13742 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    264087 | 13743 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        68 | 13744 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        70 | 13745 | `					return SXRET_OK;` |
|         - | 13746 | `				}` |
|    132008 | 13747 | `			}` |
|    183600 | 13748 | `		}` |
|    229387 | 13749 | `	}` |
|         - | 13750 | `	/* Generate code for the right tree */` |
|  26147045 | 13751 | `	if( pNode->pRight ){` |
|  15014833 | 13752 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13753 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    412657 | 13754 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14808507 | 13755 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13756 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    282555 | 13757 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14460906 | 13758 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13759 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     57401 | 13760 | `			iVmOp = 0; /* No binary operator to emit */` |
|     57401 | 13761 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  14290985 | 13762 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13763 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13764 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13765 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13766 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13767 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13768 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13769 | `			sxu32 nNsJmp = 0;` |
|       108 | 13770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13771 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  14262183 | 13772 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13773 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13774 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13775 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4774461 | 13776 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2387228 | 13777 | `		}` |
|  15014833 | 13778 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15014833 | 13779 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  15014833 | 13780 | `		if( !bIsChainOp ){` |
|         - | 13781 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13782 | `			 * operator instruction is emitted. */` |
|  10073599 | 13783 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5036797 | 13784 | `		}` |
|  15014833 | 13785 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4339107 | 13786 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4339070 | 13787 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13788 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13789 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13790 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13791 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13792 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13793 | `				 */` |
|        91 | 13794 | `				iVmOp = 0;` |
|   4339064 | 13795 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4339021 | 13796 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13797 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    779181 | 13798 | `					iP2 = 1;` |
|    389593 | 13799 | `				}else{` |
|   3559845 | 13800 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13801 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    504553 | 13802 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    504553 | 13803 | `						iP1 = pInstr->iP1;` |
|    252279 | 13804 | `					}else{` |
|   3055297 | 13805 | `						p3 = pInstr->p3;` |
|         - | 13806 | `					}` |
|         - | 13807 | `					/* POP the last dynamic load instruction */` |
|   3559845 | 13808 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13809 | `				}` |
|   2169513 | 13810 | `			}` |
|  12845282 | 13811 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13812 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13813 | `			if( pInstr ){` |
|        63 | 13814 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13815 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13816 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13817 | `					 */` |
|        19 | 13818 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13819 | `					iP1 = pInstr->iP1;` |
|        19 | 13820 | `					iP2 = pInstr->iP2;` |
|        19 | 13821 | `					p3  = pInstr->p3;` |
|        10 | 13822 | `				}else{` |
|        45 | 13823 | `					p3 = pInstr->p3;` |
|         - | 13824 | `				}` |
|        30 | 13825 | `			}` |
|        30 | 13826 | `		}` |
|   7507414 | 13827 | `	}` |
|  26147040 | 13828 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    375644 | 13829 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13830 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13831 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13832 | `		iVmOp = 0;` |
|        14 | 13833 | `	}` |
|  26147045 | 13834 | `	if( iVmOp > 0 ){` |
|  26089531 | 13835 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    195121 | 13836 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13837 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15301 | 13838 | `				iP1 = 1;` |
|      7653 | 13839 | `			}` |
|  25991973 | 13840 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13841 | `			/* Namespace-qualify the class name for NEW */ {` |
|    750915 | 13842 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    750915 | 13843 | `				VmInstr *pCallInstr = 0;` |
|    750915 | 13844 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    750603 | 13845 | `					pCallInstr = pPeek;` |
|    750603 | 13846 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    375299 | 13847 | `				}` |
|    750915 | 13848 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    735647 | 13849 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13850 | `					sxu32 nLitForClass;` |
|    735647 | 13851 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13852 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13853 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13854 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13855 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13856 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13857 | `					 * with class imports. */` |
|    735647 | 13858 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        53 | 13859 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        29 | 13860 | `					}else{` |
|    735599 | 13861 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13862 | `					}` |
|    735647 | 13863 | `					pPeek->iP1 = 0;` |
|    735647 | 13864 | `					if( !bAbsolute ){` |
|         - | 13865 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 13866 | `						 * current class — never namespace-qualify them (else` |
|         - | 13867 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 13868 | `						 * instanceof (IS_A) guard below. */` |
|    731803 | 13869 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    731803 | 13870 | `						int isSpecialNew = 0;` |
|    731803 | 13871 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    731803 | 13872 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    731803 | 13873 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    731798 | 13874 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    731843 | 13875 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    365945 | 13876 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        31 | 13877 | `								isSpecialNew = 1;` |
|        15 | 13878 | `							}` |
|    365899 | 13879 | `						}` |
|    731803 | 13880 | `						if( isSpecialNew ){` |
|        31 | 13881 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|        16 | 13882 | `						}else{` |
|    731773 | 13883 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 13884 | `						}` |
|    365904 | 13885 | `					}else{` |
|      3849 | 13886 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13887 | `					}` |
|    367821 | 13888 | `				}` |
|         - | 13889 | `			}` |
|    750915 | 13890 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    750915 | 13891 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13892 | `				VmInstr *pPrev;` |
|    750603 | 13893 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    750603 | 13894 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13895 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13896 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13897 | `					 * accumulator exactly like OP_CALL would have). */` |
|    750603 | 13898 | `					iP1 = pInstr->iP1;` |
|    750603 | 13899 | `					iP2 = pInstr->iP2;` |
|    750603 | 13900 | `					if( pInstr->p3 ){` |
|        63 | 13901 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        29 | 13902 | `					}` |
|    750603 | 13903 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    375299 | 13904 | `				}` |
|    375304 | 13905 | `			}` |
|  25518960 | 13906 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13907 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13908 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     76587 | 13909 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     76587 | 13910 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     76587 | 13911 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     76587 | 13912 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     76587 | 13913 | `				int isSpecialIs = 0;` |
|     76587 | 13914 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     76587 | 13915 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     76587 | 13916 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     76582 | 13917 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     76585 | 13918 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     38291 | 13919 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13920 | `						isSpecialIs = 1;` |
|         5 | 13921 | `					}` |
|     38291 | 13922 | `				}` |
|     76587 | 13923 | `				pInstr->iP1 = 0;` |
|     76587 | 13924 | `				if( !isSpecialIs && !bAbsolute ){` |
|     76567 | 13925 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     38281 | 13926 | `				}` |
|     38296 | 13927 | `			}` |
|  25105214 | 13928 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13929 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13930 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13931 | `			 * should not trigger constant lookup. */` |
|   4941239 | 13932 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4941239 | 13933 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4708381 | 13934 | `				pInstr->iP1 = 0;` |
|   2354188 | 13935 | `			}` |
|   4941239 | 13936 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13937 | `				/* Static member access,remember that */` |
|    367219 | 13938 | `				iP1 = 1;` |
|    367219 | 13939 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    367219 | 13940 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    229031 | 13941 | `					p3 = pInstr->p3;` |
|    229031 | 13942 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    114513 | 13943 | `				}` |
|    183607 | 13944 | `			}` |
|         - | 13945 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13946 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13947 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13948 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4941239 | 13949 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4941239 | 13950 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13951 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4941219 | 13952 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     61159 | 13953 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4910622 | 13954 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13955 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4880037 | 13956 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13957 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    947271 | 13958 | `					iP2 = PH7_MEMBER_WRITE;` |
|    473633 | 13959 | `				}` |
|   2470617 | 13960 | `			}` |
|   2470617 | 13961 | `		}` |
|         - | 13962 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13963 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13964 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13965 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13966 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  26089531 | 13967 | `		if( bFcc ){` |
|        81 | 13968 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13969 | `			iP2 = 0;` |
|        81 | 13970 | `			p3 = 0;` |
|        81 | 13971 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13972 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13973 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13974 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13975 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13976 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13977 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13978 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13979 | `				if( pMemberName ){` |
|         3 | 13980 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13981 | `				}` |
|        37 | 13982 | `				iP1 = 2;` |
|        19 | 13983 | `			}else{` |
|        45 | 13984 | `				iP1 = 1;` |
|         - | 13985 | `			}` |
|        40 | 13986 | `		}` |
|         - | 13987 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13988 | `		 * This is the primary emit path for user-visible calls. */` |
|  26089531 | 13989 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6890557 | 13990 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3445276 | 13991 | `		}` |
|         - | 13992 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  26089531 | 13993 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  13044763 | 13994 | `	}` |
|  26147045 | 13995 | `	if( nJmpIdx > 0 ){` |
|         - | 13996 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    752603 | 13997 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    752603 | 13998 | `		if( pInstr ){` |
|    752603 | 13999 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    376299 | 14000 | `		}` |
|    376299 | 14001 | `	}` |
|  26147045 | 14002 | `	return rc;` |
|  33541706 | 14003 | `}` |
|         - | 14004 | `/*` |
|         - | 14005 | ` * Compile a PHP expression.` |
|         - | 14006 | ` * According to the PHP language reference manual:` |
|         - | 14007 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 14008 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 14009 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 14010 | ` *  is "anything that has a value".` |
|         - | 14011 | ` * If something goes wrong while compiling the expression,this` |
|         - | 14012 | ` * function takes care of generating the appropriate error` |
|         - | 14013 | ` * message.` |
|         - | 14014 | ` */` |
|         - | 14015 | `/*` |
|         - | 14016 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 14017 | ` *` |
|         - | 14018 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 14019 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 14020 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 14021 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 14022 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 14023 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 14024 | ` * except for() now reports php's parse error.` |
|         - | 14025 | ` */` |
| 222353714 | 14026 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 14027 | `{` |
|         - | 14028 | `	ph7_expr_node **apArg;` |
|         - | 14029 | `	sxu32 n;` |
| 222353719 | 14030 | `	if( pNode == 0 ){` |
| 156301657 | 14031 | `		return 0;` |
|         - | 14032 | `	}` |
|  66052067 | 14033 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 14034 | `		return 1;` |
|         - | 14035 | `	}` |
|  66052058 | 14036 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  66052059 | 14037 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 14038 | `		return 1;` |
|         - | 14039 | `	}` |
|  66052059 | 14040 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  75444907 | 14041 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   9392853 | 14042 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 14043 | `			return 1;` |
|         - | 14044 | `		}` |
|   4696429 | 14045 | `	}` |
|  66052059 | 14046 | `	return 0;` |
| 111176862 | 14047 | `}` |
|  15160216 | 14048 | `static sxi32 PH7_CompileExpr(` |
|         - | 14049 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14050 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 14051 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 14052 | `	)` |
|         5 | 14053 | `{` |
|         - | 14054 | `	ph7_expr_node *pRoot;` |
|         - | 14055 | `	SySet sExprNode;` |
|         - | 14056 | `	SyToken *pEnd;` |
|         - | 14057 | `	sxi32 nExpr;` |
|         - | 14058 | `	sxi32 iNest;` |
|         - | 14059 | `	sxi32 rc;` |
|         - | 14060 | `	sxu32 nNullsafeBase;` |
|         - | 14061 | `	/* Initialize worker variables */` |
|  15160221 | 14062 | `	nExpr = 0;` |
|  15160221 | 14063 | `	pRoot = 0;` |
|         - | 14064 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 14065 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  15160221 | 14066 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15160221 | 14067 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  15160221 | 14068 | `	SySetAlloc(&sExprNode,0x10);` |
|  15160221 | 14069 | `	rc = SXRET_OK;` |
|         - | 14070 | `	/* Delimit the expression */` |
|  15160221 | 14071 | `	pEnd = pGen->pIn;` |
|  15160221 | 14072 | `	iNest = 0;` |
| 118611173 | 14073 | `	while( pEnd < pGen->pEnd ){` |
| 112726741 | 14074 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14075 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4641 | 14076 | `			iNest++;` |
| 112724423 | 14077 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4649 | 14078 | `			iNest--;` |
| 112719783 | 14079 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   9276647 | 14080 | `			if( iNest <= 0 ){` |
|   9275789 | 14081 | `				break;` |
|         - | 14082 | `			}` |
|       429 | 14083 | `		}` |
| 103450957 | 14084 | `		pEnd++;` |
|         5 | 14085 | `	}` |
|  15160221 | 14086 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    642129 | 14087 | `		SyToken *pEnd2 = pGen->pIn;` |
|    642129 | 14088 | `		iNest = 0;` |
|         - | 14089 | `		/* Stop at the first comma */` |
|   1411477 | 14090 | `		while( pEnd2 < pEnd ){` |
|    769355 | 14091 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     42093 | 14092 | `				iNest++;` |
|    748311 | 14093 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     42093 | 14094 | `				iNest--;` |
|    706223 | 14095 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 14096 | `				if( iNest <= 0 ){` |
|         3 | 14097 | `					break;` |
|         - | 14098 | `				}` |
|      3027 | 14099 | `			}` |
|    769353 | 14100 | `			pEnd2++;` |
|         5 | 14101 | `		}` |
|    642129 | 14102 | `		if( pEnd2 <pEnd ){` |
|         3 | 14103 | `			pEnd = pEnd2;` |
|         1 | 14104 | `		}` |
|    321062 | 14105 | `	}` |
|  15160221 | 14106 | `	if( pEnd > pGen->pIn ){` |
|  15137317 | 14107 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 14108 | `		/* Swap delimiter */` |
|  15137317 | 14109 | `		pGen->pEnd = pEnd;` |
|         - | 14110 | `		/* Try to get an expression tree */` |
|  15137317 | 14111 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  15137312 | 14112 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  14970919 | 14113 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 14114 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 14115 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 14116 | `				"syntax error, unexpected token \",\"");` |
|         6 | 14117 | `			pGen->pEnd = pTmp;` |
|         6 | 14118 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14119 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 14120 | `				return SXERR_ABORT;` |
|         - | 14121 | `			}` |
|         6 | 14122 | `			pGen->pIn = pEnd;` |
|         6 | 14123 | `			SySetRelease(&sExprNode);` |
|         6 | 14124 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 14125 | `			return SXRET_OK;` |
|         - | 14126 | `		}` |
|  15137313 | 14127 | `		if( rc == SXRET_OK && pRoot ){` |
|  15137129 | 14128 | `			rc = SXRET_OK;` |
|  15137129 | 14129 | `			if( xTreeValidator ){` |
|         - | 14130 | `				/* Call the upper layer validator callback */` |
|    967275 | 14131 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    483635 | 14132 | `			}` |
|  15137129 | 14133 | `			if( rc != SXERR_ABORT ){` |
|         - | 14134 | `				/* Generate code for the given tree */` |
|  15137129 | 14135 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 14136 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 14137 | `				 * expression so they short-circuit to its end. */` |
|  15137129 | 14138 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   7568562 | 14139 | `			}` |
|  15137129 | 14140 | `			nExpr = 1;` |
|   7568562 | 14141 | `		}` |
|         - | 14142 | `		/* Release the whole tree */` |
|  15137313 | 14143 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 14144 | `		/* Synchronize token stream */` |
|  15137313 | 14145 | `		pGen->pEnd = pTmp;` |
|  15137313 | 14146 | `		pGen->pIn  = pEnd;` |
|  15137313 | 14147 | `		if( rc == SXERR_ABORT ){` |
|        12 | 14148 | `			SySetRelease(&sExprNode);` |
|        12 | 14149 | `			return SXERR_ABORT;` |
|         - | 14150 | `		}` |
|   7568649 | 14151 | `	}` |
|  15160207 | 14152 | `	SySetRelease(&sExprNode);` |
|  15160207 | 14153 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   7580113 | 14154 | `}` |
|         - | 14155 | `/*` |
|         - | 14156 | ` * Return a pointer to the node construct handler associated` |
|         - | 14157 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 14158 | ` */` |
|   8752916 | 14159 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 14160 | `{` |
|   8752921 | 14161 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 14162 | `		/* Numeric literal: Either real or integer */` |
|   3556207 | 14163 | `		return PH7_CompileNumLiteral;` |
|   5196719 | 14164 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 14165 | `		/* Double quoted string */` |
|    119979 | 14166 | `		return PH7_CompileString;` |
|   5076745 | 14167 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 14168 | `		/* Single quoted string */` |
|   5076625 | 14169 | `		return PH7_CompileSimpleString;` |
|       124 | 14170 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 14171 | `		/* Heredoc */` |
|        70 | 14172 | `		return PH7_CompileHereDoc;` |
|        58 | 14173 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 14174 | `		/* Nowdoc */` |
|        52 | 14175 | `		return PH7_CompileNowDoc;` |
|         8 | 14176 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 14177 | `		/* Backtick quoted string */` |
|         6 | 14178 | `		return PH7_CompileBacktic;` |
|         - | 14179 | `	}` |
|         3 | 14180 | `	return 0;` |
|   4376463 | 14181 | `}` |
|         - | 14182 | `/*` |
|         - | 14183 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 14184 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 14185 | ` * in write context" parse error.` |
|         - | 14186 | ` */` |
|     23014 | 14187 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 14188 | `{` |
|         - | 14189 | `	sxi32 rc;` |
|     23019 | 14190 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23017 | 14191 | `		return SXRET_OK;` |
|         - | 14192 | `	}` |
|         5 | 14193 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 14194 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 14195 | `		"Can't use nullsafe operator in write context");` |
|         3 | 14196 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11512 | 14197 | `}` |
|         - | 14198 | `/*` |
|         - | 14199 | ` * Compile an unset() statement.` |
|         - | 14200 | ` * unset($var, $arr[$key], ...);` |
|         - | 14201 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 14202 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 14203 | ` * parent array before extracting the element to unset.` |
|         - | 14204 | ` */` |
|     25864 | 14205 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 14206 | `{` |
|     25869 | 14207 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     25869 | 14208 | `	sxu32 nIdx = 0;` |
|         - | 14209 | `	SyString sName;` |
|         - | 14210 | `	sxi32 rc;` |
|         - | 14211 | `	/* Jump the 'unset' keyword */` |
|     25869 | 14212 | `	pGen->pIn++;` |
|         - | 14213 | `	/* Save delimiter */` |
|     25869 | 14214 | `	pTmp = pGen->pEnd;` |
|         - | 14215 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     25869 | 14216 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     25869 | 14217 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14218 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 14219 | `		SyToken *pClose;` |
|     25869 | 14220 | `		pGen->pIn++;   /* Skip '(' */` |
|     25869 | 14221 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     25869 | 14222 | `		pEnd = pClose; /* Stop at ')' */` |
|     12932 | 14223 | `	}` |
|     25869 | 14224 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 14225 | `	/* Resolve the 'unset' builtin name once */` |
|     25869 | 14226 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3821 | 14227 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3821 | 14228 | `		if( pObj == 0 ){` |
|       ! 0 | 14229 | `			return SXERR_ABORT;` |
|         - | 14230 | `		}` |
|      3821 | 14231 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3821 | 14232 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1908 | 14233 | `	}` |
|         - | 14234 | `	/* Compile each comma-separated argument */` |
|     55959 | 14235 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30095 | 14236 | `		if( pGen->pIn < pNext ){` |
|         - | 14237 | `			/*` |
|         - | 14238 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 14239 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 14240 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 14241 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 14242 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 14243 | `			 * already removes just the element/property.` |
|         - | 14244 | `			 */` |
|     30090 | 14245 | `			if( &pGen->pIn[2] == pNext` |
|     18583 | 14246 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      7081 | 14247 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 14248 | `				SyString *pVarName;` |
|     10616 | 14249 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      7074 | 14250 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      7079 | 14251 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      7079 | 14252 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 14253 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 14254 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 14255 | `					return SXERR_ABORT;` |
|         - | 14256 | `				}` |
|      7079 | 14257 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      7079 | 14258 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      7079 | 14259 | `				pGen->pIn = pNext;` |
|      7079 | 14260 | `				if( pGen->pIn < pEnd ){` |
|      4227 | 14261 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2111 | 14262 | `				}` |
|      7079 | 14263 | `				continue;` |
|         - | 14264 | `			}` |
|     23021 | 14265 | `			pGen->pEnd = pNext;` |
|     23021 | 14266 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 14267 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 14268 | `				GenStateUnsetValidator);` |
|     23021 | 14269 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14270 | `				return SXERR_ABORT;` |
|         - | 14271 | `			}` |
|     23021 | 14272 | `			if( rc != SXERR_EMPTY ){` |
|         - | 14273 | `				/* Emit call for this single argument */` |
|     23019 | 14274 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23019 | 14275 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23019 | 14276 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11507 | 14277 | `			}` |
|     11508 | 14278 | `		}` |
|         - | 14279 | `		/* Jump trailing commas */` |
|     23027 | 14280 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 14281 | `			pNext++;` |
|         1 | 14282 | `		}` |
|     23021 | 14283 | `		pGen->pIn = pNext;` |
|         5 | 14284 | `	}` |
|         - | 14285 | `	/* Skip past the closing ')' if present */` |
|     25869 | 14286 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     25869 | 14287 | `		pGen->pIn++;` |
|     12932 | 14288 | `	}` |
|         - | 14289 | `	/* Restore token stream */` |
|     25869 | 14290 | `	pGen->pEnd = pTmp;` |
|     25869 | 14291 | `	return SXRET_OK;` |
|     12937 | 14292 | `}` |
|         - | 14293 | `/*` |
|         - | 14294 | ` * PHP Language construct table.` |
|         - | 14295 | ` */` |
|         - | 14296 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 14297 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 14298 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14299 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14300 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14301 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14302 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14303 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14304 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14305 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14306 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14307 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14308 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14309 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14310 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14311 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14312 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14313 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14314 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14315 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14316 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14317 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14318 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14319 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14320 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14321 | `};` |
|         - | 14322 | `/*` |
|         - | 14323 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14324 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14325 | ` */` |
|   7340462 | 14326 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14327 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14328 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14329 | `	)` |
|         5 | 14330 | `{` |
|   7340467 | 14331 | `	sxu32 n = 0;` |
|  29068167 | 14332 | `	for(;;){` |
|  58136339 | 14333 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    429743 | 14334 | `			break;` |
|         - | 14335 | `		}` |
|  57706601 | 14336 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6910729 | 14337 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14338 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14339 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14340 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14341 | `					return 0;` |
|         - | 14342 | `				}` |
|       ! 0 | 14343 | `			}` |
|   6910724 | 14344 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11462 | 14345 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5738 | 14346 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14347 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14348 | `				return 0;` |
|         - | 14349 | `			}` |
|         - | 14350 | `			/* Return a pointer to the handler.` |
|         - | 14351 | `			*/` |
|   6910727 | 14352 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14353 | `		}` |
|  50795877 | 14354 | `		n++;` |
|         5 | 14355 | `	}` |
|    429743 | 14356 | `	if( pLookahed ){` |
|    429743 | 14357 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     68803 | 14358 | `			return PH7_CompileClassInterface;` |
|    360945 | 14359 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    314595 | 14360 | `			return PH7_CompileClass;` |
|     46355 | 14361 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7715 | 14362 | `			return PH7_CompileTrait;` |
|         - | 14363 | `		}` |
|         - | 14364 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14365 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14366 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14367 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19320 | 14368 | `	}` |
|         - | 14369 | `	/* Not a language construct */` |
|     38645 | 14370 | `	return 0;` |
|   3670236 | 14371 | `}` |
|         - | 14372 | `/*` |
|         - | 14373 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14374 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14375 | ` */` |
|     38642 | 14376 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14377 | `{` |
|         - | 14378 | `	int rc;` |
|     38647 | 14379 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38647 | 14380 | `	if( rc == FALSE ){` |
|     38536 | 14381 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15632 | 14382 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14383 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14384 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14385 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14386 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14387 | `			*/` |
|         - | 14388 | `			){` |
|     38533 | 14389 | `				rc = TRUE;` |
|     19264 | 14390 | `		}` |
|     19268 | 14391 | `	}` |
|     38647 | 14392 | `	return rc;` |
|         5 | 14393 | `}` |
|         - | 14394 | `/*` |
|         - | 14395 | ` * Compile a PHP chunk.` |
|         - | 14396 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14397 | ` * takes care of generating the appropriate error message.` |
|         - | 14398 | ` */` |
|         - | 14399 | `/*` |
|         - | 14400 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14401 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14402 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14403 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14404 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14405 | ` * intervening non-declaration statements.` |
|         - | 14406 | ` */` |
|  15821894 | 14407 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14408 | `{` |
|  15821899 | 14409 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  15821899 | 14410 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  15821899 | 14411 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14412 | `	sxu32 nIdx, n;` |
|  15821894 | 14413 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3277037 | 14414 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14415 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14416 | `		 * indexes do not map to the sidecar */` |
|  12544869 | 14417 | `		return;` |
|         - | 14418 | `	}` |
|   3277035 | 14419 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14420 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14421 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3277035 | 14422 | `	SySetReset(&pGen->aPendingAttrs);` |
|   9832589 | 14423 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6555559 | 14424 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6547763 | 14425 | `			continue;` |
|         - | 14426 | `		}` |
|      7801 | 14427 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14428 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7789 | 14429 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7777 | 14430 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3886 | 14431 | `		}` |
|      3903 | 14432 | `	}` |
|   7910952 | 14433 | `}` |
|         - | 14434 | `/*` |
|         - | 14435 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14436 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14437 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14438 | ` */` |
|   4067196 | 14439 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14440 | `{` |
|         - | 14441 | `	char *zDup;` |
|   4067201 | 14442 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4067181 | 14443 | `		return;` |
|         - | 14444 | `	}` |
|        35 | 14445 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14446 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14447 | `	if( zDup ){` |
|        25 | 14448 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14449 | `	}` |
|        25 | 14450 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2033603 | 14451 | `}` |
|         - | 14452 | `/*` |
|         - | 14453 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14454 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14455 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14456 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14457 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14458 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14459 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14460 | ` */` |
|      7784 | 14461 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14462 | `{` |
|         - | 14463 | `	SySet *pToken;` |
|         - | 14464 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14465 | `	char *zSpan;` |
|      7789 | 14466 | `	sxi32 rc = SXRET_OK;` |
|      7789 | 14467 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14468 | `		return SXRET_OK;` |
|         - | 14469 | `	}` |
|     11681 | 14470 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3892 | 14471 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7789 | 14472 | `	if( zSpan == 0 ){` |
|       ! 0 | 14473 | `		return SXRET_OK;` |
|         - | 14474 | `	}` |
|         - | 14475 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14476 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14477 | `	 * the number of attribute declarations in the program. */` |
|      7789 | 14478 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7789 | 14479 | `	if( pToken == 0 ){` |
|       ! 0 | 14480 | `		return SXRET_OK;` |
|         - | 14481 | `	}` |
|      7789 | 14482 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7789 | 14483 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7789 | 14484 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7789 | 14485 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7789 | 14486 | `	pSavedIn = pGen->pIn;` |
|      7789 | 14487 | `	pSavedEnd = pGen->pEnd;` |
|      7793 | 14488 | `	while( pIn < pEnd ){` |
|         - | 14489 | `		ph7_attribute sAttr;` |
|         - | 14490 | `		SyBlob sFQN;` |
|      7793 | 14491 | `		int bAbsolute = 0;` |
|      7793 | 14492 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7793 | 14493 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7793 | 14494 | `		sAttr.nLine = pIn->nLine;` |
|      7793 | 14495 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14496 | `			bAbsolute = 1;` |
|        75 | 14497 | `			pIn++;` |
|        35 | 14498 | `		}` |
|      7793 | 14499 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7793 | 14500 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7793 | 14501 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7793 | 14502 | `			pIn++;` |
|      7793 | 14503 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14504 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14505 | `				pIn++;` |
|       ! 0 | 14506 | `				continue;` |
|         - | 14507 | `			}` |
|      7793 | 14508 | `			break;` |
|       ! 0 | 14509 | `		}` |
|      7793 | 14510 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14511 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14512 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14513 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14514 | `			break;` |
|         - | 14515 | `		}` |
|         - | 14516 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14517 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14518 | `		{` |
|      7793 | 14519 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7793 | 14520 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7793 | 14521 | `			char *zDup = 0;` |
|      7793 | 14522 | `			if( !bAbsolute ){` |
|      7723 | 14523 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7723 | 14524 | `				if( pImp ){` |
|       ! 0 | 14525 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14526 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14527 | `					if( zDup ){` |
|       ! 0 | 14528 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14529 | `					}` |
|      7723 | 14530 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14531 | `					SyBlob sTmp;` |
|       ! 0 | 14532 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14533 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14534 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14535 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14536 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14537 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14538 | `					if( zDup ){` |
|       ! 0 | 14539 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14540 | `					}` |
|       ! 0 | 14541 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14542 | `				}` |
|      3859 | 14543 | `			}` |
|      7793 | 14544 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7793 | 14545 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7793 | 14546 | `				if( zDup ){` |
|      7793 | 14547 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3894 | 14548 | `				}` |
|      3894 | 14549 | `			}` |
|         - | 14550 | `		}` |
|      7793 | 14551 | `		SyBlobRelease(&sFQN);` |
|      7793 | 14552 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14553 | `			SyToken *pArgsEnd;` |
|      7691 | 14554 | `			pIn++;` |
|      7691 | 14555 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15391 | 14556 | `			while( pIn < pArgsEnd ){` |
|      7705 | 14557 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7705 | 14558 | `				sxi32 iDepth = 0;` |
|         - | 14559 | `				ph7_attr_arg sArgRec;` |
|     76565 | 14560 | `				while( pArgStop < pArgsEnd ){` |
|     68881 | 14561 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14562 | `						iDepth++;` |
|     68876 | 14563 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14564 | `						iDepth--;` |
|     68866 | 14565 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14566 | `						break;` |
|         - | 14567 | `					}` |
|     68865 | 14568 | `					pArgStop++;` |
|         5 | 14569 | `				}` |
|      7705 | 14570 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7705 | 14571 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7700 | 14572 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7684 | 14573 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14574 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14575 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14576 | `					if( zN ){` |
|        19 | 14577 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14578 | `					}` |
|        19 | 14579 | `					pArgStart += 2;` |
|         9 | 14580 | `				}` |
|      7705 | 14581 | `				if( pArgStart < pArgStop ){` |
|         - | 14582 | `					SySet *pInstrContainer;` |
|      7705 | 14583 | `					pGen->pIn = pArgStart;` |
|      7705 | 14584 | `					pGen->pEnd = pArgStop;` |
|      7705 | 14585 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7705 | 14586 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7705 | 14587 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7705 | 14588 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7705 | 14589 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7705 | 14590 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14591 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14592 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14593 | `						return SXERR_ABORT;` |
|         - | 14594 | `					}` |
|      7705 | 14595 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3850 | 14596 | `				}` |
|      7705 | 14597 | `				pIn = pArgStop;` |
|      7705 | 14598 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14599 | `					pIn++;` |
|         8 | 14600 | `				}` |
|         5 | 14601 | `			}` |
|      7691 | 14602 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3843 | 14603 | `		}` |
|      7793 | 14604 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7793 | 14605 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14606 | `			pIn++;` |
|         5 | 14607 | `			continue;` |
|         - | 14608 | `		}` |
|      7789 | 14609 | `		break;` |
|       ! 0 | 14610 | `	}` |
|      7789 | 14611 | `	pGen->pIn = pSavedIn;` |
|      7789 | 14612 | `	pGen->pEnd = pSavedEnd;` |
|      7789 | 14613 | `	return SXRET_OK;` |
|      3897 | 14614 | `}` |
|         - | 14615 | `/*` |
|         - | 14616 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14617 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14618 | ` */` |
|   4067200 | 14619 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14620 | `{` |
|   4067205 | 14621 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14622 | `	sxu32 n;` |
|         - | 14623 | `	sxi32 rc;` |
|   4074977 | 14624 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7777 | 14625 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7777 | 14626 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14627 | `			return SXERR_ABORT;` |
|         - | 14628 | `		}` |
|      3891 | 14629 | `	}` |
|   4067205 | 14630 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4067205 | 14631 | `	return SXRET_OK;` |
|   2033605 | 14632 | `}` |
|         - | 14633 | `/*` |
|         - | 14634 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14635 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14636 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14637 | ` */` |
|   2053220 | 14638 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14639 | `{` |
|   2053225 | 14640 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2053225 | 14641 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2053225 | 14642 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14643 | `	sxu32 nIdx, n;` |
|         - | 14644 | `	sxi32 rc;` |
|   2053220 | 14645 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    546023 | 14646 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1507207 | 14647 | `		return SXRET_OK;` |
|         - | 14648 | `	}` |
|    546023 | 14649 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1638057 | 14650 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1092039 | 14651 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14652 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14653 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14654 | `				return SXERR_ABORT;` |
|         - | 14655 | `			}` |
|         6 | 14656 | `		}` |
|    546022 | 14657 | `	}` |
|    546023 | 14658 | `	return SXRET_OK;` |
|   1026615 | 14659 | `}` |
|  11780936 | 14660 | `static sxi32 GenStateCompileChunk(` |
|         - | 14661 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14662 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14663 | `	)` |
|         5 | 14664 | `{` |
|         - | 14665 | `	ProcLangConstruct xCons;` |
|         - | 14666 | `	sxi32 rc;` |
|  11780941 | 14667 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   6844006 | 14668 | `	for(;;){` |
|  12734479 | 14669 | `		int bStmtIsDeclare = 0;` |
|  12734479 | 14670 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14671 | `			/* No more input to process */` |
|     67517 | 14672 | `			break;` |
|         - | 14673 | `		}` |
|         - | 14674 | `		/* Bind a directly-preceding docblock to this statement */` |
|  12666967 | 14675 | `		GenStateSetPendingDoc(&(*pGen));` |
|  12666967 | 14676 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14677 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14678 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14679 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14680 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14681 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7695 | 14682 | `			int bAttrTarget = 0;` |
|      7690 | 14683 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3879 | 14684 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7637 | 14685 | `				bAttrTarget = 1;` |
|      3875 | 14686 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14687 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14688 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14689 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14690 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14691 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14692 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14693 | `					bAttrTarget = 1;` |
|        29 | 14694 | `				}` |
|        29 | 14695 | `			}` |
|      7695 | 14696 | `			if( !bAttrTarget ){` |
|       ! 0 | 14697 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14698 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14699 | `					&pGen->pIn->sData);` |
|       ! 0 | 14700 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14701 | `					break;` |
|         - | 14702 | `				}` |
|       ! 0 | 14703 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14704 | `			}` |
|      3845 | 14705 | `		}` |
|         - | 14706 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14707 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  12666967 | 14708 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7374873 | 14709 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   7374873 | 14710 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14711 | `				bStmtIsDeclare = 1;` |
|        21 | 14712 | `			}` |
|   3687434 | 14713 | `		}` |
|  12666967 | 14714 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14715 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14716 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    953511 | 14717 | `			pGen->bStrictTypesLocked = 1;` |
|    476753 | 14718 | `		}` |
|  12666967 | 14719 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14720 | `			/* Compile block */` |
|      3845 | 14721 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3845 | 14722 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14723 | `				break;` |
|         - | 14724 | `			}` |
|      1925 | 14725 | `		}else{` |
|  12663127 | 14726 | `			xCons = 0;` |
|  12663127 | 14727 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14728 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14729 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14730 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34437 | 14731 | `				xCons = PH7_CompileClassModifiers;` |
|  12645911 | 14732 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14733 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14734 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3849 | 14735 | `				xCons = PH7_CompileEnum;` |
|  12626773 | 14736 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7340467 | 14737 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14738 | `				/* Try to extract a language construct handler */` |
|   7340467 | 14739 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   7340467 | 14740 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14741 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14742 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14743 | `						&pGen->pIn->sData);` |
|         9 | 14744 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14745 | `						break;` |
|         - | 14746 | `					}` |
|         - | 14747 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14748 | `					 * this erroneous statement.` |
|         - | 14749 | `					 */` |
|         9 | 14750 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14751 | `				}` |
|   8954620 | 14752 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    405765 | 14753 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14754 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14755 | `				xCons = PH7_CompileLabel;` |
|        56 | 14756 | `			}` |
|  12663127 | 14757 | `			if( xCons == 0 ){` |
|         - | 14758 | `				/* Assume an expression an try to compile it */` |
|   5322911 | 14759 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5322911 | 14760 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14761 | `					/* Pop l-value */` |
|   5322761 | 14762 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2661378 | 14763 | `				}` |
|   2661458 | 14764 | `			}else{` |
|         - | 14765 | `				/* Go compile the sucker */` |
|   7340221 | 14766 | `				rc = xCons(&(*pGen));` |
|         - | 14767 | `			}` |
|  12663127 | 14768 | `			if( rc == SXERR_ABORT ){` |
|         - | 14769 | `				/* Request to abort compilation */` |
|        12 | 14770 | `				break;` |
|         - | 14771 | `			}` |
|         - | 14772 | `		}` |
|         - | 14773 | `		/* Ignore trailing semi-colons ';' */` |
|  21688823 | 14774 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   9021871 | 14775 | `			pGen->pIn++;` |
|         5 | 14776 | `		}` |
|  12666957 | 14777 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14778 | `			/* Compile a single statement and return */` |
|  11713419 | 14779 | `			break;` |
|         - | 14780 | `		}` |
|         - | 14781 | `		/* LOOP ONE */` |
|         - | 14782 | `		/* LOOP TWO */` |
|         - | 14783 | `		/* LOOP THREE */` |
|         - | 14784 | `		/* LOOP FOUR */` |
|         5 | 14785 | `	}` |
|         - | 14786 | `	/* Return compilation status */` |
|  11780941 | 14787 | `	return rc;` |
|         5 | 14788 | `}` |
|         - | 14789 | `/*` |
|         - | 14790 | ` * Compile a Raw PHP chunk.` |
|         - | 14791 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14792 | ` * takes care of generating the appropriate error message.` |
|         - | 14793 | ` */` |
|     67524 | 14794 | `static sxi32 PH7_CompilePHP(` |
|         - | 14795 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14796 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14797 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14798 | `	)` |
|         5 | 14799 | `{` |
|     67529 | 14800 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14801 | `	sxi32 rc;` |
|         - | 14802 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67529 | 14803 | `	SySetReset(&(*pTokenSet));` |
|     67529 | 14804 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14805 | `	/* Mark as the default token set */` |
|     67529 | 14806 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14807 | `	/* Advance the stream cursor */` |
|     67529 | 14808 | `	pGen->pRawIn++;` |
|         - | 14809 | `	/* Tokenize the PHP chunk first */` |
|     67529 | 14810 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14811 | `	/* Point to the head and tail of the token stream. */` |
|     67529 | 14812 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67529 | 14813 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67529 | 14814 | `	if( is_expr ){` |
|       ! 0 | 14815 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14816 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14817 | `			/* A simple expression,compile it */` |
|       ! 0 | 14818 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14819 | `		}` |
|         - | 14820 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14821 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14822 | `		return SXRET_OK;` |
|         - | 14823 | `	}` |
|     67529 | 14824 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14825 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14826 | `		/*` |
|         - | 14827 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14828 | `		 * According to the PHP reference manual:` |
|         - | 14829 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14830 | `		 *  immediately follow` |
|         - | 14831 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14832 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14833 | `		 * Symisc extension:` |
|         - | 14834 | `		 *   This short syntax works with all PHP opening` |
|         - | 14835 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14836 | `		 *   only short tag.` |
|         - | 14837 | `		 */` |
|         - | 14838 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14839 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14840 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14841 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14842 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14843 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14844 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14845 | `		}` |
|         3 | 14846 | `		return SXRET_OK;` |
|         - | 14847 | `	}` |
|         - | 14848 | `	/* Compile the PHP chunk */` |
|     67527 | 14849 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14850 | `	/* Fix exceptions jumps */` |
|     67527 | 14851 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14852 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67527 | 14853 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14854 | `		rc = SXERR_ABORT;` |
|         1 | 14855 | `	}` |
|         - | 14856 | `	/* Reset container */` |
|     67527 | 14857 | `	SySetReset(&pGen->aGoto);` |
|     67527 | 14858 | `	SySetReset(&pGen->aLabel);` |
|     67527 | 14859 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14860 | `	/* Compilation result */` |
|     67527 | 14861 | `	return rc;` |
|     33767 | 14862 | `}` |
|         - | 14863 | `/*` |
|         - | 14864 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14865 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14866 | ` * This is the only compile interface exported from this file.` |
|         - | 14867 | ` */` |
|     70716 | 14868 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14869 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14870 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14871 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14872 | `	)` |
|         5 | 14873 | `{` |
|         - | 14874 | `	SySet aPhpToken,aRawToken;` |
|         - | 14875 | `	ph7_gen_state *pCodeGen;` |
|         - | 14876 | `	ph7_value *pRawObj;` |
|         - | 14877 | `	sxu32 nObjIdx;` |
|         - | 14878 | `	sxi32 nRawObj;` |
|         - | 14879 | `	int is_expr;` |
|         - | 14880 | `	sxi8 bSavedStrict;` |
|         - | 14881 | `	sxi8 bSavedStrictLocked;` |
|         - | 14882 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 14883 | `	sxi32 rc;` |
|     70721 | 14884 | `	sxu32 nBaseLine = 1;` |
|     70721 | 14885 | `	if( pScript->nByte < 1 ){` |
|         - | 14886 | `		/* Nothing to compile */` |
|       ! 0 | 14887 | `		return PH7_OK;` |
|         - | 14888 | `	}` |
|         - | 14889 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 14890 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 14891 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     70721 | 14892 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 14893 | `		const char *z = pScript->zString;` |
|         3 | 14894 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 14895 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 14896 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 14897 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 14898 | `		pScript->zString = z;` |
|         3 | 14899 | `		nBaseLine = 2;` |
|         3 | 14900 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 14901 | `			return PH7_OK;` |
|         - | 14902 | `		}` |
|         1 | 14903 | `	}` |
|         - | 14904 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14905 | `	 * file's flags so include/require restore them on return. */` |
|     70721 | 14906 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 14907 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 14908 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 14909 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 14910 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 14911 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 14912 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     70721 | 14913 | `	pSavedIn = pCodeGen->pIn;` |
|     70721 | 14914 | `	pSavedEnd = pCodeGen->pEnd;` |
|     70721 | 14915 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     70721 | 14916 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     70721 | 14917 | `	pCodeGen->bStrictTypes = 0;` |
|     70721 | 14918 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14919 | `	/* Initialize the tokens containers */` |
|     70721 | 14920 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70721 | 14921 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70721 | 14922 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     70721 | 14923 | `	is_expr = 0;` |
|     70721 | 14924 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14925 | `		SyToken sTmp;` |
|         - | 14926 | `		/* PHP only: -*/` |
|     57363 | 14927 | `		sTmp.nLine = 1;` |
|     57363 | 14928 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57363 | 14929 | `		sTmp.pUserData = 0;` |
|     57363 | 14930 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57363 | 14931 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57363 | 14932 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14933 | `			/* A simple PHP expression */` |
|       ! 0 | 14934 | `			is_expr = 1;` |
|       ! 0 | 14935 | `		}` |
|     28684 | 14936 | `	}else{` |
|         - | 14937 | `		/* Tokenize raw text */` |
|     13363 | 14938 | `		SySetAlloc(&aRawToken,32);` |
|     13363 | 14939 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 14940 | `	}` |
|         - | 14941 | `	/* Process high-level tokens */` |
|     70721 | 14942 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     70721 | 14943 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     70721 | 14944 | `	rc = PH7_OK;` |
|     70721 | 14945 | `	if( is_expr ){` |
|         - | 14946 | `		/* Compile the expression */` |
|       ! 0 | 14947 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14948 | `		goto cleanup;` |
|         - | 14949 | `	}` |
|     70721 | 14950 | `	nObjIdx = 0;` |
|         - | 14951 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14952 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14953 | `	 * preventing namespace bleeding across include()d files. */` |
|     70721 | 14954 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14955 | `	/* Start the compilation process */` |
|     42042 | 14956 | `	for(;;){` |
|    151601 | 14957 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     70709 | 14958 | `			break; /* No more tokens to process */` |
|         - | 14959 | `		}` |
|     80897 | 14960 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14961 | `			/* Compile the PHP chunk */` |
|     67529 | 14962 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67529 | 14963 | `			if( rc == SXERR_ABORT ){` |
|        15 | 14964 | `				break;` |
|         - | 14965 | `			}` |
|     67517 | 14966 | `			continue;` |
|         - | 14967 | `		}` |
|         - | 14968 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13373 | 14969 | `		nRawObj = 0;` |
|     26741 | 14970 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14971 | `			/* Consume the raw chunk without any processing */` |
|     13373 | 14972 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13373 | 14973 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14974 | `				rc = SXERR_MEM;` |
|       ! 0 | 14975 | `				break;` |
|         - | 14976 | `			}` |
|         - | 14977 | `			/* Mark as constant and emit the load constant instruction */` |
|     13373 | 14978 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13373 | 14979 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13373 | 14980 | `			++nRawObj;` |
|     13373 | 14981 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14982 | `		}` |
|     13373 | 14983 | `		if( nRawObj > 0 ){` |
|         - | 14984 | `			/* Emit the consume instruction */` |
|     13373 | 14985 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6684 | 14986 | `		}` |
|     35363 | 14987 | `	}` |
|     35358 | 14988 | `cleanup:` |
|         - | 14989 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     70721 | 14990 | `	pCodeGen->pIn = pSavedIn;` |
|     70721 | 14991 | `	pCodeGen->pEnd = pSavedEnd;` |
|     70721 | 14992 | `	SySetRelease(&aRawToken);` |
|     70721 | 14993 | `	SySetRelease(&aPhpToken);` |
|         - | 14994 | `	/* Restore outer file's strict_types scope */` |
|     70721 | 14995 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     70721 | 14996 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     70721 | 14997 | `	return rc;` |
|     35363 | 14998 | `}` |
|         - | 14999 | `/*` |
|         - | 15000 | ` * Utility routines.Initialize the code generator.` |
|         - | 15001 | ` */` |
|      3816 | 15002 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 15003 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15004 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15005 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15006 | `	)` |
|         5 | 15007 | `{` |
|      3821 | 15008 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15009 | `	/* Zero the structure */` |
|      3821 | 15010 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 15011 | `	/* Initial state */` |
|      3821 | 15012 | `	pGen->pVm  = &(*pVm);` |
|      3821 | 15013 | `	pGen->xErr = xErr;` |
|      3821 | 15014 | `	pGen->pErrData = pErrData;` |
|      3821 | 15015 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3821 | 15016 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3821 | 15017 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3821 | 15018 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3821 | 15019 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3821 | 15020 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3821 | 15021 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3821 | 15022 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3821 | 15023 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 15024 | `	/* Error log buffer */` |
|      3821 | 15025 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 15026 | `	/* General purpose working buffer */` |
|      3821 | 15027 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 15028 | `	/* Namespace state */` |
|      3821 | 15029 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3821 | 15030 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3821 | 15031 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3821 | 15032 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15033 | `	/* Create the global scope */` |
|      3821 | 15034 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 15035 | `	/* Point to the global scope */` |
|      3821 | 15036 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3821 | 15037 | `	return SXRET_OK;` |
|         5 | 15038 | `}` |
|         - | 15039 | `/*` |
|         - | 15040 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 15041 | ` */` |
|     74072 | 15042 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 15043 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15044 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15045 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15046 | `	)` |
|         5 | 15047 | `{` |
|     74077 | 15048 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15049 | `	GenBlock *pBlock,*pParent;` |
|         - | 15050 | `	/* Reset state */` |
|     74077 | 15051 | `	SySetReset(&pGen->aLabel);` |
|     74077 | 15052 | `	SySetReset(&pGen->aGoto);` |
|     74077 | 15053 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     74077 | 15054 | `	SySetReset(&pGen->aTrivia);` |
|     74077 | 15055 | `	SySetReset(&pGen->aPendingAttrs);` |
|     74077 | 15056 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     74077 | 15057 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     74077 | 15058 | `	SyBlobRelease(&pGen->sWorker);` |
|     74077 | 15059 | `	SyBlobRelease(&pGen->sNamespace);` |
|     74077 | 15060 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     74077 | 15061 | `	SyHashRelease(&pGen->hUseImports);` |
|     74077 | 15062 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     74077 | 15063 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     74077 | 15064 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     74077 | 15065 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     74077 | 15066 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15067 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 15068 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 15069 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 15070 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 15071 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 15072 | `	 * number of unique names, which is acceptable. */` |
|         - | 15073 | `	/* Point to the global scope */` |
|     74077 | 15074 | `	pBlock = pGen->pCurrent;` |
|     74077 | 15075 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 15076 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15077 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15078 | `		pBlock = pParent;` |
|       ! 0 | 15079 | `	}` |
|     74077 | 15080 | `	pGen->xErr = xErr;` |
|     74077 | 15081 | `	pGen->pErrData = pErrData;` |
|     74077 | 15082 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     74077 | 15083 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     74077 | 15084 | `	pGen->pIn = pGen->pEnd = 0;` |
|     74077 | 15085 | `	pGen->nErr = 0;` |
|     74077 | 15086 | `	return SXRET_OK;` |
|         5 | 15087 | `}` |
|         - | 15088 | `/*` |
|         - | 15089 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 15090 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 15091 | ` *` |
|         - | 15092 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 15093 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 15094 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 15095 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 15096 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 15097 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 15098 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 15099 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 15100 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 15101 | ` *` |
|         - | 15102 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 15103 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 15104 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 15105 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 15106 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 15107 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 15108 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 15109 | ` */` |
|         4 | 15110 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 15111 | `{` |
|         5 | 15112 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15113 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 15114 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 15115 | `	*pSaved = *pGen;` |
|         5 | 15116 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 15117 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 15118 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15119 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15120 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15121 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15122 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 15123 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 15124 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 15125 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 15126 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 15127 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15128 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 15129 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 15130 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 15131 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 15132 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 15133 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 15134 | `	pGen->pTokenSet = 0;` |
|         5 | 15135 | `	pGen->nErr = 0;` |
|         5 | 15136 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 15137 | `	pGen->nCommaExprOk = 0;` |
|         5 | 15138 | `	pGen->bInGenerator = 0;` |
|         5 | 15139 | `	pGen->bStrictTypes = 0;` |
|         5 | 15140 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 15141 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 15142 | `	pGen->xErr = xErr;` |
|         5 | 15143 | `	pGen->pErrData = pErrData;` |
|         5 | 15144 | `}` |
|         - | 15145 | `/*` |
|         - | 15146 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 15147 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 15148 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 15149 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 15150 | ` */` |
|         4 | 15151 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 15152 | `{` |
|         5 | 15153 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15154 | `	GenBlock *pBlock,*pParent;` |
|         - | 15155 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 15156 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 15157 | `	 * nested global block's own fixup sets. */` |
|         5 | 15158 | `	pBlock = pGen->pCurrent;` |
|         5 | 15159 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 15160 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15161 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15162 | `		pBlock = pParent;` |
|       ! 0 | 15163 | `	}` |
|         5 | 15164 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 15165 | `	/* Release the nested unit's position containers. */` |
|         5 | 15166 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 15167 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 15168 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 15169 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 15170 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 15171 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 15172 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 15173 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 15174 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 15175 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 15176 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 15177 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 15178 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 15179 | `	hVar = pGen->hVar;` |
|         5 | 15180 | `	hLiteral = pGen->hLiteral;` |
|         5 | 15181 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 15182 | `	*pGen = *pSaved;` |
|         5 | 15183 | `	pGen->hVar = hVar;` |
|         5 | 15184 | `	pGen->hLiteral = hLiteral;` |
|         5 | 15185 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 15186 | `}` |
|         - | 15187 | `/*` |
|         - | 15188 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 15189 | ` * php's parser prints, e.g.` |
|         - | 15190 | ` *` |
|         - | 15191 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 15192 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 15193 | ` *   syntax error, unexpected end of file` |
|         - | 15194 | ` *` |
|         - | 15195 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 15196 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 15197 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 15198 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 15199 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 15200 | ` *` |
|         - | 15201 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 15202 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 15203 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 15204 | ` */` |
|       182 | 15205 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 15206 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 15207 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 15208 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 15209 | `	)` |
|         5 | 15210 | `{` |
|       187 | 15211 | `	const char *zNoun = "token";` |
|         - | 15212 | `	sxu32 nLine;` |
|       187 | 15213 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 15214 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 15215 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 15216 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 15217 | `		 * it before concluding "end of file". */` |
|        92 | 15218 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        92 | 15219 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        92 | 15220 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 15221 | `			pTok = pGen->pEnd;` |
|        44 | 15222 | `		}` |
|        44 | 15223 | `	}` |
|       187 | 15224 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       187 | 15225 | `	if( pTok == 0 ){` |
|       ! 0 | 15226 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 15227 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 15228 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 15229 | `			zExpecting);` |
|         - | 15230 | `	}` |
|       187 | 15231 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 15232 | `		zNoun = "identifier";` |
|       180 | 15233 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         8 | 15234 | `		zNoun = "variable";` |
|       171 | 15235 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        23 | 15236 | `		zNoun = "integer";` |
|       158 | 15237 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 15238 | `		zNoun = "float";` |
|       ! 0 | 15239 | `	}` |
|       187 | 15240 | `	if( zExpecting ){` |
|       118 | 15241 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 15242 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 15243 | `	}` |
|       164 | 15244 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 15245 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        96 | 15246 | `}` |
|         - | 15247 | `/*` |
|         - | 15248 | ` * Generate a compile-time error message.` |
|         - | 15249 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 15250 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 15251 | ` * abort compilation immediately.` |
|         - | 15252 | ` */` |
|     15942 | 15253 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 15254 | `{` |
|     15947 | 15255 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     15947 | 15256 | `	const char *zErr = "Error";` |
|         - | 15257 | `	SyString *pFile;` |
|         - | 15258 | `	va_list ap;` |
|         - | 15259 | `	sxi32 rc;` |
|         - | 15260 | `	/* Reset the working buffer */` |
|     15947 | 15261 | `	SyBlobReset(pWorker);` |
|         - | 15262 | `	/* Peek the processed file path if available */` |
|     15947 | 15263 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     15947 | 15264 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 15265 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 15266 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 15267 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 15268 | `		 * into execution with a 0 exit status. */` |
|       659 | 15269 | `		pGen->nErr++;` |
|       659 | 15270 | `		if( pGen->nErr > 15 ){` |
|         - | 15271 | `			/* Error count limit reached */` |
|         6 | 15272 | `			if( pGen->xErr ){` |
|         6 | 15273 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 15274 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 15275 | `				if( pFile ){` |
|         6 | 15276 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 15277 | `				}` |
|         6 | 15278 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 15279 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 15280 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 15281 | `				}` |
|         2 | 15282 | `			}` |
|         - | 15283 | `			/* Abort immediately */` |
|         6 | 15284 | `			return SXERR_ABORT;` |
|         - | 15285 | `		}` |
|       325 | 15286 | `	}` |
|     15943 | 15287 | `	if( pGen->xErr == 0 ){` |
|         - | 15288 | `		/* No available error consumer,return immediately */` |
|     15271 | 15289 | `		return SXRET_OK;` |
|         - | 15290 | `	}` |
|       677 | 15291 | `	switch(nErrType){` |
|       310 | 15292 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 15293 | `	case E_WARNING: zErr = "Warning";     break;` |
|       346 | 15294 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 15295 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 15296 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 15297 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 15298 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        16 | 15299 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 15300 | `	default:` |
|       ! 0 | 15301 | `		break;` |
|         - | 15302 | `	}` |
|       677 | 15303 | `	rc = SXRET_OK;` |
|         - | 15304 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       677 | 15305 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       677 | 15306 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       677 | 15307 | `	va_start(ap,zFormat);` |
|       677 | 15308 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       677 | 15309 | `	va_end(ap);` |
|       677 | 15310 | `	if( pFile ){` |
|       677 | 15311 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       336 | 15312 | `	}` |
|         - | 15313 | `	/* Append a new line */` |
|       677 | 15314 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       677 | 15315 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 15316 | `		/* Consume the generated error message */` |
|       677 | 15317 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       336 | 15318 | `	}` |
|       677 | 15319 | `	return rc;` |
|      7976 | 15320 | `}` |
|         - | 15321 |  |
