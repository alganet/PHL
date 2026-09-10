# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7219/8928 lines (80.86%)

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
|  11434618 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|  11434623 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  11434623 |   173 | `	pBlock->pUserData   = pUserData;` |
|  11434623 |   174 | `	pBlock->pGen        = pGen;` |
|  11434623 |   175 | `	pBlock->iFlags      = iType;` |
|  11434623 |   176 | `	pBlock->pParent     = 0;` |
|  11434623 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11434623 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11434623 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|  11430802 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|  11430807 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  11430807 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|  11430807 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  11430807 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|  11430807 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  11430807 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    493345 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    493345 |   214 | `		pGen->nLoopId++;` |
|    493345 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    493345 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    493345 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    493345 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    246670 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|  11430807 |   221 | `	pGen->pCurrent = pBlock;` |
|  11430807 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   5471745 |   224 | `		*ppBlock = pBlock;` |
|   2735870 |   225 | `	}` |
|  11430807 |   226 | `	return SXRET_OK;` |
|   5715406 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|  11430786 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|  11430791 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|  11430791 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|  11430791 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|  11430786 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|  11430791 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  11430791 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|  11430791 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  11430791 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|  11430786 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|  11430791 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  11430791 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|  11430791 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    493337 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    246666 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|  11430791 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|  11430791 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|  11430791 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|  11430791 |   268 | `	return SXRET_OK;` |
|   5715398 |   269 | `}` |
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
|   4337722 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   4337727 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   4337727 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   4337727 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4337727 |   289 | `	return rc;` |
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
|   8010266 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   8010271 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  17267475 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9257209 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   3453313 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   5803901 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1466181 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   4337725 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4337725 |   322 | `		if( pInstr ){` |
|   4337725 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   4337725 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   4337725 |   326 | `			aFix[n].nJumpType = -1;` |
|   2168860 |   327 | `		}` |
|   2168865 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   8010271 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2803450 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2803455 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2803601 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   2803453 |   400 | `	return SXRET_OK;` |
|   1401730 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  14583338 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  14583343 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  14583343 |   409 | `	if( pEntry == 0 ){` |
|   3811457 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|  10771891 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10771891 |   413 | `	return SXRET_OK;` |
|   7291674 |   414 | `}` |
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
|   3811452 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3811457 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3811457 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1905726 |   429 | `	}` |
|   3811457 |   430 | `	return SXRET_OK;` |
|         5 |   431 | `}` |
|         - |   432 | `/*` |
|         - |   433 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   434 | ` * in the constant table.` |
|         - |   435 | ` */` |
|   3547432 |   436 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   437 | `{` |
|         - |   438 | `	ph7_value *pObj;` |
|   3547437 |   439 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   440 | `	/* Reserve a new constant */` |
|   3547437 |   441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3547437 |   442 | `	if( pObj == 0 ){` |
|       ! 0 |   443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   444 | `		return 0;` |
|         - |   445 | `	}` |
|   3547437 |   446 | `	*pIdx = nIdx;` |
|         - |   447 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   448 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   449 | `	 */` |
|   3547437 |   450 | `	return pObj;` |
|   1773721 |   451 | `}` |
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
|   6898224 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6898229 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3449117 |   478 | `}` |
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
|   3556090 |   542 | `static int GenStateFindBadNumericSeparator(` |
|         - |   543 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   544 | `{` |
|   3556095 |   545 | `	const char *z = pRaw->zString;` |
|   3556095 |   546 | `	sxu32 n = pRaw->nByte;` |
|   3556095 |   547 | `	int base = 10;` |
|         - |   548 | `	sxu32 i, start;` |
|   3556095 |   549 | `	if( n < 2 ) return 0;` |
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
|   1778050 |   574 | `}` |
|         - |   575 | `/*` |
|         - |   576 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   577 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   578 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   579 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   580 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   581 | ` * so callers can bail from the current construct).` |
|         - |   582 | ` */` |
|   3556090 |   583 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   584 | `{` |
|   3556095 |   585 | `	const char *zBad = 0;` |
|   3556095 |   586 | `	sxu32 nBad = 0;` |
|         - |   587 | `	SyString sBad;` |
|         - |   588 | `	sxi32 rc;` |
|   3556095 |   589 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   3556081 |   590 | `		return SXRET_OK;` |
|         - |   591 | `	}` |
|        18 |   592 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   593 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   594 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   595 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   596 | `		return SXERR_ABORT;` |
|         - |   597 | `	}` |
|        18 |   598 | `	return SXERR_SYNTAX;` |
|   1778050 |   599 | `}` |
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
|   3556076 |   616 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   617 | `	SyMemBackend *pAlloc,` |
|         - |   618 | `	const SyString *pToken,` |
|         - |   619 | `	char *zScratch, sxu32 nScratch,` |
|         - |   620 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   621 | `{` |
|         - |   622 | `	sxu32 i, j;` |
|   3556081 |   623 | `	int hasUnderscore = 0;` |
|         - |   624 | `	char *zBuf;` |
|   3556081 |   625 | `	*pzAlloc = 0;` |
|   8424957 |   626 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   4869133 |   627 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   2434443 |   628 | `	}` |
|   3556081 |   629 | `	if( !hasUnderscore ){` |
|   3555829 |   630 | `		SyStringDupPtr(pOut, pToken);` |
|   3555829 |   631 | `		return SXRET_OK;` |
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
|   1778043 |   648 | `}` |
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
|   3547466 |   684 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   685 | `{` |
|   3547471 |   686 | `	const char *z = pNum->zString;` |
|   3547471 |   687 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   688 | `	const char *p, *q;` |
|         - |   689 | `	int n;` |
|   3547471 |   690 | `	*pbDecimal = FALSE;` |
|   3547471 |   691 | `	if( z >= zEnd ){` |
|       ! 0 |   692 | `		return FALSE;` |
|         - |   693 | `	}` |
|   3547471 |   694 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
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
|   3444351 |   709 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|   3444071 |   724 | `	}else if( z[0] == '0' ){` |
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
|   2167163 |   745 | `	p = z;` |
|   2167163 |   746 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   5168545 |   747 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   2167163 |   748 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   749 | `		*pbDecimal = TRUE;` |
|        25 |   750 | `		return TRUE;` |
|         - |   751 | `	}` |
|   2167139 |   752 | `	return FALSE;` |
|   1773738 |   753 | `}` |
|   3556062 |   754 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   755 | `{` |
|   3556067 |   756 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   3556067 |   757 | `	sxu32 nIdx = 0;` |
|         - |   758 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   3556067 |   759 | `	char *zAlloc = 0;` |
|         - |   760 | `	SyString sNum;` |
|         - |   761 | `	sxi32 rc;` |
|   1778031 |   762 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   3556067 |   763 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   3556067 |   764 | `	if( rc != SXRET_OK ){` |
|        14 |   765 | `		return rc;` |
|         - |   766 | `	}` |
|   5334083 |   767 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1778026 |   768 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   3556057 |   769 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   770 | `		return SXERR_ABORT;` |
|         - |   771 | `	}` |
|   3556057 |   772 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   773 | `		ph7_value *pObj;` |
|         - |   774 | `		sxi64 iValue;` |
|   3547471 |   775 | `		ph7_real rOverflow = 0;` |
|   3547471 |   776 | `		int bDecimalOverflow = 0;` |
|   3547471 |   777 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|   3547437 |   794 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   3547437 |   795 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   3547437 |   796 | `			if( pObj == 0 ){` |
|       ! 0 |   797 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   798 | `				return SXERR_ABORT;` |
|         - |   799 | `			}` |
|   3547437 |   800 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   801 | `		}` |
|   1773738 |   802 | `	}else{` |
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
|   3556057 |   815 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   816 | `	/* Emit the load constant instruction */` |
|   3556057 |   817 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   818 | `	/* Node successfully compiled */` |
|   3556057 |   819 | `	return SXRET_OK;` |
|   1778036 |   820 | `}` |
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
|   5023148 |   832 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   833 | `{` |
|   5023153 |   834 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   835 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   836 | `	ph7_value *pObj;` |
|         - |   837 | `	sxu32 nIdx;` |
|   5023153 |   838 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   839 | `	/* Delimit the string */` |
|   5023153 |   840 | `	zIn  = pStr->zString;` |
|   5023153 |   841 | `	zEnd = &zIn[pStr->nByte];` |
|   5023153 |   842 | `	if( zIn >= zEnd ){` |
|         - |   843 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   844 | `		 * rather than reserving a new object each time. */` |
|    324579 |   845 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    324579 |   846 | `		return SXRET_OK;` |
|         - |   847 | `	}` |
|   4698579 |   848 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   849 | `		/* Already processed,emit the load constant instruction` |
|         - |   850 | `		 * and return.` |
|         - |   851 | `		 */` |
|   2771113 |   852 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2771113 |   853 | `		return SXRET_OK;` |
|         - |   854 | `	}` |
|         - |   855 | `	/* Reserve a new constant */` |
|   1927471 |   856 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1927471 |   857 | `	if( pObj == 0 ){` |
|       ! 0 |   858 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   859 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   860 | `		return SXERR_ABORT;` |
|         - |   861 | `	}` |
|   1927471 |   862 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   863 | `	/* Compile the node */` |
|   1975229 |   864 | `	for(;;){` |
|   3950463 |   865 | `		if( zIn >= zEnd ){` |
|         - |   866 | `			/* End of input */` |
|   1927471 |   867 | `			break;` |
|         - |   868 | `		}` |
|   2022997 |   869 | `		zCur = zIn;` |
|  40259761 |   870 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  38236769 |   871 | `			zIn++;` |
|         5 |   872 | `		}` |
|   2022997 |   873 | `		if( zIn > zCur ){` |
|         - |   874 | `			/* Append raw contents*/` |
|   1984805 |   875 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    992400 |   876 | `		}` |
|   2022997 |   877 | `		zIn++;` |
|   2022997 |   878 | `		if( zIn < zEnd ){` |
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
|   2022997 |   893 | `		zIn++;` |
|         5 |   894 | `	}` |
|         - |   895 | `	/* Emit the load constant instruction */` |
|   1927471 |   896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1927471 |   897 | `	if( pStr->nByte < 1024 ){` |
|         - |   898 | `		/* Install in the literal table */` |
|   1927471 |   899 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    963733 |   900 | `	}` |
|         - |   901 | `	/* Node successfully compiled */` |
|   1927471 |   902 | `	return SXRET_OK;` |
|   2511579 |   903 | `}` |
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
|      2614 |  1070 | `static sxi32 GenStateProcessStringExpression(` |
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
|      2619 |  1081 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1082 | `	/* Preallocate some slots */` |
|      2619 |  1083 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1084 | `	/* Tokenize the text */` |
|      2619 |  1085 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1086 | `	/* Swap delimiter */` |
|      2619 |  1087 | `	pTmpIn  = pGen->pIn;` |
|      2619 |  1088 | `	pTmpEnd = pGen->pEnd;` |
|      2619 |  1089 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2619 |  1090 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1091 | `	/* Compile the expression */` |
|      2619 |  1092 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1093 | `	/* Restore token stream */` |
|      2619 |  1094 | `	pGen->pIn  = pTmpIn;` |
|      2619 |  1095 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1096 | `	/* Release the token set */` |
|      2619 |  1097 | `	SySetRelease(&sToken);` |
|         - |  1098 | `	/* Compilation result */` |
|      2619 |  1099 | `	return rc;` |
|         5 |  1100 | `}` |
|         - |  1101 | `/*` |
|         - |  1102 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1103 | ` */` |
|    121492 |  1104 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1105 | `{` |
|         - |  1106 | `	ph7_value *pConstObj;` |
|    121497 |  1107 | `	sxu32 nIdx = 0;` |
|         - |  1108 | `	/* Reserve a new constant */` |
|    121497 |  1109 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    121497 |  1110 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1111 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1112 | `		return 0;` |
|         - |  1113 | `	}` |
|    121497 |  1114 | `	(*pCount)++;` |
|    121497 |  1115 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1116 | `	/* Emit the load constant instruction */` |
|    121497 |  1117 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    121497 |  1118 | `	return pConstObj;` |
|     60751 |  1119 | `}` |
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
|    119932 |  1182 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1183 | `{` |
|    119937 |  1184 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1185 | `	const char *zIn,*zCur,*zEnd;` |
|    119937 |  1186 | `	ph7_value *pObj = 0;` |
|         - |  1187 | `	sxi32 iCons;` |
|         - |  1188 | `	sxi32 rc;` |
|         - |  1189 | `	/* Delimit the string */` |
|    119937 |  1190 | `	zIn  = pStr->zString;` |
|    119937 |  1191 | `	zEnd = &zIn[pStr->nByte];` |
|    119937 |  1192 | `	if( zIn >= zEnd ){` |
|         - |  1193 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1194 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1195 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1196 | `		 */` |
|       413 |  1197 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       413 |  1198 | `		return SXRET_OK;` |
|         - |  1199 | `	}` |
|    119529 |  1200 | `	zCur = 0;` |
|         - |  1201 | `	/* Compile the node */` |
|    119529 |  1202 | `	iCons = 0;` |
|     61067 |  1203 | `	for(;;){` |
|    162435 |  1204 | `		zCur = zIn;` |
|   1656567 |  1205 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1496751 |  1206 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        69 |  1207 | `				break;` |
|   1496624 |  1208 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2492 |  1209 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1246 |  1210 | `					break;` |
|         - |  1211 | `			}` |
|   1494137 |  1212 | `			zIn++;` |
|         5 |  1213 | `		}` |
|    162435 |  1214 | `		if( zIn > zCur ){` |
|     94861 |  1215 | `			if( pObj == 0 ){` |
|     94243 |  1216 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     94243 |  1217 | `				if( pObj == 0 ){` |
|       ! 0 |  1218 | `					return SXERR_ABORT;` |
|         - |  1219 | `				}` |
|     47119 |  1220 | `			}` |
|     94861 |  1221 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     47428 |  1222 | `		}` |
|    162435 |  1223 | `		if( zIn >= zEnd ){` |
|    119527 |  1224 | `			break;` |
|         - |  1225 | `		}` |
|     42913 |  1226 | `		if( zIn[0] == '\\' ){` |
|     40299 |  1227 | `			const char *zPtr = 0;` |
|         - |  1228 | `			sxu32 n;` |
|     40299 |  1229 | `			zIn++;` |
|     40299 |  1230 | `			if( pObj == 0 ){` |
|     27259 |  1231 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27259 |  1232 | `				if( pObj == 0 ){` |
|       ! 0 |  1233 | `					return SXERR_ABORT;` |
|         - |  1234 | `				}` |
|     13627 |  1235 | `			}` |
|     40299 |  1236 | `			if( zIn >= zEnd ){` |
|         - |  1237 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1238 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1239 | `				break;` |
|         - |  1240 | `			}` |
|     40297 |  1241 | `			n = sizeof(char); /* size of conversion */` |
|     40297 |  1242 | `			switch( zIn[0] ){` |
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
|     17563 |  1259 | `			case 'n':` |
|         - |  1260 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     35131 |  1261 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     35131 |  1262 | `				break;` |
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
|     40297 |  1385 | `			zIn += n;` |
|     40297 |  1386 | `			continue;` |
|         - |  1387 | `		}` |
|      2619 |  1388 | `		if( zIn[0] == '{' ){` |
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
|      2487 |  1422 | `			const char *zExpr = zIn;` |
|         - |  1423 | `			/* Assemble variable name */` |
|      1266 |  1424 | `			for(;;){` |
|         - |  1425 | `				/* Jump leading dollars */` |
|      5019 |  1426 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2487 |  1427 | `					zIn++;` |
|         5 |  1428 | `				}` |
|      1266 |  1429 | `				for(;;){` |
|     12987 |  1430 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9189 |  1431 | `						zIn++;` |
|         5 |  1432 | `					}` |
|      2537 |  1433 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1434 | `						/* UTF-8 stream */` |
|       ! 0 |  1435 | `						zIn++;` |
|       ! 0 |  1436 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1437 | `							zIn++;` |
|       ! 0 |  1438 | `						}` |
|       ! 0 |  1439 | `						continue;` |
|         - |  1440 | `					}` |
|      2537 |  1441 | `					break;` |
|       ! 0 |  1442 | `				}` |
|      2537 |  1443 | `				if( zIn >= zEnd ){` |
|       269 |  1444 | `					break;` |
|         - |  1445 | `				}` |
|      2273 |  1446 | `				if( zIn[0] == '[' ){` |
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
|      2263 |  1464 | `				}else if(zIn[0] == '{' ){` |
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
|      2259 |  1482 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1483 | `					/* Member access operator '->' */` |
|        53 |  1484 | `					zIn += 2;` |
|      2234 |  1485 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1486 | `					/* Static member access operator '::' */` |
|       ! 0 |  1487 | `					zIn += 2;` |
|       ! 0 |  1488 | `				}else{` |
|      1107 |  1489 | `					break;` |
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
|      2487 |  1501 | `				const char *zBr = zExpr;` |
|     14271 |  1502 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     11789 |  1503 | `					zBr++;` |
|         5 |  1504 | `				}` |
|      2487 |  1505 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
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
|      2480 |  1546 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' && zIn[-1] == '}'` |
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
|      2483 |  1582 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2483 |  1583 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1584 | `				return SXERR_ABORT;` |
|         - |  1585 | `			}` |
|      2483 |  1586 | `			if( rc != SXERR_EMPTY ){` |
|      2481 |  1587 | `				++iCons;` |
|      1238 |  1588 | `			}` |
|         - |  1589 | `		}` |
|         - |  1590 | `		/* Invalidate the previously used constant */` |
|      2615 |  1591 | `		pObj = 0;` |
|         5 |  1592 | `	}/*for(;;)*/` |
|    119529 |  1593 | `	if( iCons > 1 ){` |
|         - |  1594 | `		/* Concatenate all compiled constants */` |
|      1891 |  1595 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       943 |  1596 | `	}` |
|         - |  1597 | `	/* Node successfully compiled */` |
|    119529 |  1598 | `	return SXRET_OK;` |
|     59971 |  1599 | `}` |
|         - |  1600 | `/*` |
|         - |  1601 | ` * Compile a double quoted string.` |
|         - |  1602 | ` *  See the block-comment above for more information.` |
|         - |  1603 | ` */` |
|    119870 |  1604 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1605 | `{` |
|         - |  1606 | `	sxi32 rc;` |
|    119875 |  1607 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     59935 |  1608 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1609 | `	/* Compilation result */` |
|    119875 |  1610 | `	return rc;` |
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
|   1382992 |  1654 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   1382997 |  1665 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1666 | `	/* Compile the expression*/` |
|   1382997 |  1667 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1668 | `	/* Restore token stream */` |
|   1382997 |  1669 | `	RE_SWAP_DELIMITER(pGen);` |
|   1382997 |  1670 | `	return rc;` |
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
|   1316842 |  1709 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1710 | `{` |
|   1316847 |  1711 | `	SyToken *pCur = pStart;` |
|   1316847 |  1712 | `	sxi32 iNest = 0;` |
|   3427431 |  1713 | `	while( pCur < pEnd ){` |
|   2612883 |  1714 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    502295 |  1715 | `			return pCur;` |
|         - |  1716 | `		}` |
|         - |  1717 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1718 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1719 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1720 | `		 */` |
|   2110593 |  1721 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   2110587 |  1782 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     53989 |  1783 | `			iNest++;` |
|   2083595 |  1784 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1785 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1786 | `			 * parser will shortly detect any syntax error. */` |
|     53989 |  1787 | `			iNest--;` |
|     26992 |  1788 | `		}` |
|   2110587 |  1789 | `		pCur++;` |
|         5 |  1790 | `	}` |
|    814553 |  1791 | `	return pEnd;` |
|    658426 |  1792 | `}` |
|         - |  1793 | `/*` |
|         - |  1794 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1795 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1796 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1797 | ` */` |
|    595544 |  1798 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1799 | `{` |
|         - |  1800 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1801 | `	SyToken *pKey,*pCur;` |
|    595549 |  1802 | `	sxi32 iEmitRef = 0;` |
|    595549 |  1803 | `	sxi32 iSpread = 0;` |
|    595549 |  1804 | `	sxi32 nPair = 0;` |
|         - |  1805 | `	sxi32 rc;` |
|    595549 |  1806 | `	xValidator = 0;` |
|    804943 |  1807 | `	for(;;){` |
|         - |  1808 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|         - |  1809 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|         - |  1810 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|         - |  1811 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|    507171 |  1812 | `		{` |
|   1609891 |  1813 | `			int nSkip = 0;` |
|   2380457 |  1814 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    770571 |  1815 | `				nSkip++;` |
|    770571 |  1816 | `				pGen->pIn++;` |
|         5 |  1817 | `			}` |
|   1609891 |  1818 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|       ! 0 |  1819 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|         - |  1820 | `					"Cannot use empty array elements in arrays");` |
|       ! 0 |  1821 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1822 | `					return SXERR_ABORT;` |
|         - |  1823 | `				}` |
|       ! 0 |  1824 | `				return SXRET_OK;` |
|         - |  1825 | `			}` |
|         - |  1826 | `		}` |
|   1609891 |  1827 | `		pCur = pGen->pIn;` |
|   1609891 |  1828 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1829 | `			/* No more entry to process */` |
|    595531 |  1830 | `			break;` |
|         - |  1831 | `		}` |
|   1014365 |  1832 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1833 | `			continue;` |
|         - |  1834 | `		}` |
|         - |  1835 | `		/* Compile the key if available */` |
|   1014365 |  1836 | `		pKey = pCur;` |
|   1014365 |  1837 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   1014365 |  1838 | `		rc = SXERR_EMPTY;` |
|   1014365 |  1839 | `		if( pCur < pGen->pIn ){` |
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
|    645991 |  1873 | `			pCur = pKey;` |
|         - |  1874 | `		}` |
|   1014353 |  1875 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1876 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|         - |  1877 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|    645991 |  1878 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|    322993 |  1879 | `		}` |
|   1014353 |  1880 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   1014351 |  1899 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   1014351 |  1900 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   1521518 |  1917 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    507171 |  1918 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1919 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    507171 |  1920 | `			xValidator);` |
|   1014347 |  1921 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1922 | `			return SXERR_ABORT;` |
|         - |  1923 | `		}` |
|   1014347 |  1924 | `		if( iSpread ){` |
|         - |  1925 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        69 |  1926 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   1014314 |  1927 | `		}else if( iEmitRef ){` |
|         - |  1928 | `			/* Emit the load reference instruction */` |
|        40 |  1929 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1930 | `		}` |
|   1014347 |  1931 | `		xValidator = 0;` |
|   1014347 |  1932 | `		iEmitRef = 0;` |
|   1014347 |  1933 | `		iSpread = 0;` |
|   1014347 |  1934 | `		nPair++;` |
|         5 |  1935 | `	}` |
|         - |  1936 | `	/* Emit the load map instruction */` |
|    595531 |  1937 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1938 | `	/* Node successfully compiled */` |
|    595531 |  1939 | `	return SXRET_OK;` |
|    297777 |  1940 | `}` |
|         - |  1941 | `/*` |
|         - |  1942 | ` * Compile the 'array' language construct.` |
|         - |  1943 | ` *	 According to the PHP language reference manual` |
|         - |  1944 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1945 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1946 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1947 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1948 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1949 | ` */` |
|    379630 |  1950 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1951 | `{` |
|         - |  1952 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    379635 |  1953 | `	pGen->pIn += 2;` |
|    379635 |  1954 | `	pGen->pEnd--;` |
|    189815 |  1955 | `	SXUNUSED(iCompileFlag);` |
|    379635 |  1956 | `	return GenStateCompileArrayBody(pGen);` |
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
|    215914 |  2055 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2056 | `{` |
|         - |  2057 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    215919 |  2058 | `	pGen->pIn++;` |
|    215919 |  2059 | `	pGen->pEnd--;` |
|    107957 |  2060 | `	SXUNUSED(iCompileFlag);` |
|    215919 |  2061 | `	return GenStateCompileArrayBody(pGen);` |
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
|       568 |  2402 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2403 | `{` |
|       573 |  2404 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2405 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2406 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2407 | `							  * one thread is allowed to compile the script.` |
|         - |  2408 | `						      */` |
|         - |  2409 | `	SyString sName;` |
|       573 |  2410 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2411 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2412 | `	sxu32 nKwLine;` |
|       573 |  2413 | `	sxi32 iFlags = 0;` |
|         - |  2414 | `	sxu32 nLen;` |
|         - |  2415 | `	sxi32 rc;` |
|       284 |  2416 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2417 |  |
|       573 |  2418 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       568 |  2419 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       573 |  2420 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2421 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        11 |  2422 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        11 |  2423 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         5 |  2424 | `	}` |
|       573 |  2425 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       573 |  2426 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2427 | `		pGen->pIn++;` |
|       ! 0 |  2428 | `	}` |
|         - |  2429 | `	/* Generate a unique name */` |
|       573 |  2430 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2431 | `	/* Make sure the generated name is unique */` |
|       573 |  2432 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2433 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2434 | `	}` |
|       573 |  2435 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2436 | `	/* Compile the lambda body */` |
|       573 |  2437 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       573 |  2438 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2439 | `		return SXERR_ABORT;` |
|         - |  2440 | `	}` |
|       573 |  2441 | `	if( pAnnonFunc ){` |
|       573 |  2442 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2443 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2444 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       573 |  2445 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2446 | `			return SXERR_ABORT;` |
|         - |  2447 | `		}` |
|       284 |  2448 | `	}` |
|         - |  2449 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2450 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2451 | `	 * the handler wraps either in a Closure instance. */` |
|       573 |  2452 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2453 | `	/* Node successfully compiled */` |
|       573 |  2454 | `	return SXRET_OK;` |
|       289 |  2455 | `}` |
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
|        66 |  3268 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3269 | `{` |
|         - |  3270 | `	SyString *pName;` |
|         - |  3271 | `	sxu32 nKeyID;` |
|         - |  3272 | `	sxi32 rc;` |
|         - |  3273 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        71 |  3274 | `	pName = &pGen->pIn->sData;` |
|        71 |  3275 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        71 |  3276 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        71 |  3277 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
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
|        63 |  3312 | `		sxi32 nArg = 0;` |
|        63 |  3313 | `		sxu32 nIdx = 0;` |
|        63 |  3314 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        63 |  3315 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3316 | `			return SXERR_ABORT;` |
|        63 |  3317 | `		}else if(rc != SXERR_EMPTY ){` |
|        63 |  3318 | `			nArg = 1;` |
|        29 |  3319 | `		}` |
|        63 |  3320 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
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
|        63 |  3334 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        63 |  3335 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3336 | `	}` |
|         - |  3337 | `	/* Node successfully compiled */` |
|        71 |  3338 | `	return SXRET_OK;` |
|        38 |  3339 | `}` |
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
|  19144954 |  3361 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3362 | `{` |
|  19144959 |  3363 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3364 | `	sxi32 iVv;` |
|         - |  3365 | `	sxi32 iP1;` |
|         - |  3366 | `	void *p3;` |
|         - |  3367 | `	sxi32 rc;` |
|  19144959 |  3368 | `	iVv = -1; /* Variable variable counter */` |
|  38289925 |  3369 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  19144971 |  3370 | `		pGen->pIn++;` |
|  19144971 |  3371 | `		iVv++;` |
|         5 |  3372 | `	}` |
|  19144959 |  3373 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3374 | `		/* Invalid variable name */` |
|       ! 0 |  3375 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3376 | `		if( rc == SXERR_ABORT ){` |
|         - |  3377 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3378 | `			return SXERR_ABORT;` |
|         - |  3379 | `		}` |
|       ! 0 |  3380 | `		return SXRET_OK;` |
|         - |  3381 | `	}` |
|  19144959 |  3382 | `	p3  = 0;` |
|  19144959 |  3383 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  19144943 |  3409 | `		char *zName = 0;` |
|         - |  3410 | `		/* Extract variable name */` |
|  19144943 |  3411 | `		pName = &pGen->pIn->sData;` |
|         - |  3412 | `		/* Advance the stream cursor */` |
|  19144943 |  3413 | `		pGen->pIn++;` |
|  19144943 |  3414 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  19144943 |  3415 | `		if( pEntry == 0 ){` |
|         - |  3416 | `			/* Duplicate name */` |
|   1147587 |  3417 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   1147587 |  3418 | `			if( zName == 0 ){` |
|       ! 0 |  3419 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3420 | `				return SXERR_ABORT;` |
|         - |  3421 | `			}` |
|         - |  3422 | `			/* Install in the hashtable */` |
|   1147587 |  3423 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    573796 |  3424 | `		}else{` |
|         - |  3425 | `			/* Name already available */` |
|  17997361 |  3426 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3427 | `		}` |
|  19144943 |  3428 | `		p3 = (void *)zName;` |
|         - |  3429 | `	}` |
|  19144955 |  3430 | `	iP1 = 0;` |
|  19144955 |  3431 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   5660789 |  3432 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3433 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   5656949 |  3434 | `			iP1 = 1;` |
|   2828472 |  3435 | `		}` |
|   2830392 |  3436 | `	}` |
|         - |  3437 | `	/* Emit the load instruction */` |
|  19144955 |  3438 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  19144967 |  3439 | `	while( iVv > 0 ){` |
|        13 |  3440 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3441 | `		iVv--;` |
|         1 |  3442 | `	}` |
|         - |  3443 | `	/* Node successfully compiled */` |
|  19144955 |  3444 | `	return SXRET_OK;` |
|   9572482 |  3445 | `}` |
|         - |  3446 | `/*` |
|         - |  3447 | ` * Load a literal.` |
|         - |  3448 | ` */` |
|  11868788 |  3449 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3450 | `{` |
|  11868793 |  3451 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3452 | `	ph7_value *pObj;` |
|         - |  3453 | `	SyString *pStr;` |
|         - |  3454 | `	sxu32 nIdx;` |
|         - |  3455 | `	/* Extract token value */` |
|  11868793 |  3456 | `	pStr = &pToken->sData;` |
|         - |  3457 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  11868793 |  3458 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2232645 |  3459 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3460 | `			/* NULL constant are always indexed at 0 */` |
|    977857 |  3461 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    977857 |  3462 | `			return SXRET_OK;` |
|   1254793 |  3463 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3464 | `			/* TRUE constant are always indexed at 1 */` |
|    321751 |  3465 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    321751 |  3466 | `			return SXRET_OK;` |
|         5 |  3467 | `		}` |
|  11089280 |  3468 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1973212 |  3469 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3470 | `			/* FALSE constant are always indexed at 2 */` |
|    706621 |  3471 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    706621 |  3472 | `			return SXRET_OK;` |
|   9340693 |  3473 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    822312 |  3474 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
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
|   9334952 |  3485 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   1272532 |  3486 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|   9375863 |  3487 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|    908078 |  3488 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
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
|   9038621 |  3520 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
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
|   9038838 |  3537 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    447544 |  3538 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   9135244 |  3539 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    426910 |  3540 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|   9854833 |  3583 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3584 | `		ph7_value *pLitObj;` |
|         - |  3585 | `		/* Unknown literal,install it in the literal table */` |
|   1876231 |  3586 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1876231 |  3587 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3588 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3589 | `			return SXERR_ABORT;` |
|         - |  3590 | `		}` |
|   1876231 |  3591 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1876231 |  3592 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    938113 |  3593 | `	}` |
|         - |  3594 | `	/* Emit the load constant instruction */` |
|   9854833 |  3595 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9854833 |  3596 | `	return SXRET_OK;` |
|   5934399 |  3597 | `}` |
|         - |  3598 | `/*` |
|         - |  3599 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3600 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3601 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3602 | ` * Otherwise, load the simple literal directly.` |
|         - |  3603 | ` */` |
|  11872664 |  3604 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3605 | `{` |
|         - |  3606 | `	sxi32 rc;` |
|  11872669 |  3607 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3608 | `		return SXRET_OK;` |
|         - |  3609 | `	}` |
|         - |  3610 | `	/* Check if this is a multi-token namespace path */` |
|  11872669 |  3611 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3612 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3881 |  3613 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3881 |  3614 | `		int isAbsolute = 0;` |
|      3881 |  3615 | `		SyBlobReset(pWorker);` |
|         - |  3616 | `		/* Check for leading backslash (absolute path) */` |
|      3881 |  3617 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3879 |  3618 | `			isAbsolute = 1;` |
|      3879 |  3619 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1937 |  3620 | `		}` |
|         - |  3621 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3881 |  3622 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3623 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3624 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3625 | `		}` |
|         - |  3626 | `		/* Collect all path components */` |
|      3989 |  3627 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      3989 |  3628 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        58 |  3629 | `				SyBlobAppend(pWorker,"\\",1);` |
|        31 |  3630 | `			}else{` |
|      3935 |  3631 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3632 | `			}` |
|      3989 |  3633 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3881 |  3634 | `				pGen->pIn++;` |
|      3881 |  3635 | `				break;` |
|         - |  3636 | `			}` |
|       112 |  3637 | `			pGen->pIn++;` |
|         4 |  3638 | `		}` |
|      3881 |  3639 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3640 | `			ph7_value *pObj;` |
|         - |  3641 | `			SyString sPath;` |
|         - |  3642 | `			sxu32 nIdx;` |
|      3881 |  3643 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3644 | `			/* Install in the literal table */` |
|      3881 |  3645 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3839 |  3646 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3839 |  3647 | `				if( pObj == 0 ){` |
|       ! 0 |  3648 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3649 | `					return SXERR_ABORT;` |
|         - |  3650 | `				}` |
|      3839 |  3651 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3839 |  3652 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1917 |  3653 | `			}` |
|         - |  3654 | `			/* Emit the load constant instruction.` |
|         - |  3655 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3656 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5819 |  3657 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1938 |  3658 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1938 |  3659 | `				nIdx,0,0);` |
|      3881 |  3660 | `			return SXRET_OK;` |
|         - |  3661 | `		}` |
|       ! 0 |  3662 | `	}` |
|         - |  3663 | `	/* Single-token literal: load directly */` |
|  11868793 |  3664 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11868793 |  3665 | `	return rc;` |
|   5936337 |  3666 | `}` |
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
|  11872664 |  3683 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3684 | `{` |
|         - |  3685 | `	sxi32 rc;` |
|  11872669 |  3686 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11872669 |  3687 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3688 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3689 | `		return rc;` |
|         - |  3690 | `	}` |
|         - |  3691 | `	/* Node successfully compiled */` |
|  11872669 |  3692 | `	return SXRET_OK;` |
|   5936337 |  3693 | `}` |
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
|    290238 |  3710 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3711 | `{` |
|    290243 |  3712 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3865 |  3713 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3714 | `			return TRUE;` |
|      3863 |  3715 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3716 | `			return TRUE;` |
|         5 |  3717 | `		}` |
|    288310 |  3718 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7657 |  3719 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3720 | `			return TRUE;` |
|         - |  3721 | `		}` |
|      3825 |  3722 | `	}` |
|         - |  3723 | `	/* Not a reserved constant */` |
|    290235 |  3724 | `	return FALSE;` |
|    145124 |  3725 | `}` |
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
|   5983048 |  4248 | `static sxi32 PH7_CompileBlock(` |
|         - |  4249 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4250 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4251 | `	)` |
|         5 |  4252 | `{` |
|         - |  4253 | `	sxi32 rc;` |
|         - |  4254 | `	sxu32 nLine;` |
|   5983053 |  4255 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5959067 |  4256 | `		nLine = pGen->pIn->nLine;` |
|   5959067 |  4257 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5959067 |  4258 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4259 | `			return SXERR_ABORT;` |
|         - |  4260 | `		}` |
|   5959067 |  4261 | `		pGen->pIn++;` |
|         - |  4262 | `		/* Compile until we hit the closing braces '}' */` |
|   8803212 |  4263 | `		for(;;){` |
|  17606429 |  4264 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  17606409 |  4276 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4277 | `				/* Closing braces found,break immediately*/` |
|   5959047 |  4278 | `				pGen->pIn++;` |
|   5959047 |  4279 | `				break;` |
|         - |  4280 | `			}` |
|         - |  4281 | `			/* Compile a single statement */` |
|  11647367 |  4282 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  11647367 |  4283 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4284 | `				return SXERR_ABORT;` |
|         - |  4285 | `			}` |
|         5 |  4286 | `		}` |
|   5959067 |  4287 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   3003522 |  4288 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|   5983053 |  4338 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4339 | `		pGen->pIn++;` |
|       ! 0 |  4340 | `	}` |
|   5983053 |  4341 | `	return SXRET_OK;` |
|   2991529 |  4342 | `}` |
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
|    436054 |  4753 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4754 | `{` |
|    436059 |  4755 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    436059 |  4756 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4757 | `		/* Unexpected expression */` |
|       ! 0 |  4758 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4759 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4760 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4761 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4762 | `		}` |
|       ! 0 |  4763 | `	}` |
|    436059 |  4764 | `	return rc;` |
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
|    302220 |  4792 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4793 | `{` |
|    302225 |  4794 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    302225 |  4795 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    302225 |  4796 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4797 | `	ph7_foreach_info *pInfo;` |
|         - |  4798 | `	sxu32 nFalseJump;` |
|         - |  4799 | `	VmInstr *pInstr;` |
|         - |  4800 | `	sxu32 nLine;` |
|         - |  4801 | `	sxi32 rc;` |
|    302225 |  4802 | `	nLine = pGen->pIn->nLine;` |
|         - |  4803 | `	/* Jump the 'foreach' keyword */` |
|    302225 |  4804 | `	pGen->pIn++;` |
|    302225 |  4805 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4806 | `		/* Syntax error */` |
|       ! 0 |  4807 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4808 | `		if( rc == SXERR_ABORT ){` |
|         - |  4809 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4810 | `			return SXERR_ABORT;` |
|         - |  4811 | `		}` |
|       ! 0 |  4812 | `		goto Synchronize;` |
|         - |  4813 | `	}` |
|         - |  4814 | `	/* Jump the left parenthesis '(' */` |
|    302225 |  4815 | `	pGen->pIn++;` |
|         - |  4816 | `	/* Create the loop block */` |
|    302225 |  4817 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    302225 |  4818 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4819 | `		return SXERR_ABORT;` |
|         - |  4820 | `	}` |
|         - |  4821 | `	/* Delimit the expression */` |
|    302225 |  4822 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    302225 |  4823 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    302225 |  4838 | `	pCur = pGen->pIn;` |
|   1733047 |  4839 | `	while( pCur < pEnd ){` |
|   1733047 |  4840 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    332767 |  4841 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    332767 |  4842 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4843 | `				/* Break with the first 'as' found */` |
|    302225 |  4844 | `				break;` |
|         - |  4845 | `			}` |
|     15271 |  4846 | `		}` |
|         - |  4847 | `		/* Advance the stream cursor */` |
|   1430827 |  4848 | `		pCur++;` |
|         5 |  4849 | `	}` |
|    302225 |  4850 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4851 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4852 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4853 | `		if( rc == SXERR_ABORT ){` |
|         - |  4854 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4855 | `			return SXERR_ABORT;` |
|         - |  4856 | `		}` |
|       ! 0 |  4857 | `		goto Synchronize;` |
|         - |  4858 | `	}` |
|         - |  4859 | `	/* Swap token streams */` |
|    302225 |  4860 | `	pTmp = pGen->pEnd;` |
|    302225 |  4861 | `	pGen->pEnd = pCur;` |
|    302225 |  4862 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    302225 |  4863 | `	if( rc == SXERR_ABORT ){` |
|         - |  4864 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4865 | `		return SXERR_ABORT;` |
|         - |  4866 | `	}` |
|         - |  4867 | `	/* Update token stream */` |
|    302225 |  4868 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4869 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4870 | `		if( rc == SXERR_ABORT ){` |
|         - |  4871 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4872 | `			return SXERR_ABORT;` |
|         - |  4873 | `		}` |
|       ! 0 |  4874 | `		pGen->pIn++;` |
|       ! 0 |  4875 | `	}` |
|    302225 |  4876 | `	pCur++; /* Jump the 'as' keyword */` |
|    302225 |  4877 | `	pGen->pIn = pCur;` |
|    302225 |  4878 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4879 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4880 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4881 | `			return SXERR_ABORT;` |
|         - |  4882 | `		}` |
|       ! 0 |  4883 | `	}` |
|         - |  4884 | `	/* Create the foreach context */` |
|    302225 |  4885 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    302225 |  4886 | `	if( pInfo == 0 ){` |
|       ! 0 |  4887 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4888 | `		return SXERR_ABORT;` |
|         - |  4889 | `	}` |
|         - |  4890 | `	/* Zero the structure */` |
|    302225 |  4891 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4892 | `	/* Initialize structure fields */` |
|    302225 |  4893 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4894 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4895 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4896 | `	 * '=>'. */` |
|    302225 |  4897 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    302225 |  4898 | `	if( pCur < pEnd ){` |
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
|    302225 |  4922 | `	pGen->pEnd = pEnd;` |
|    302225 |  4923 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4924 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4925 | `		if( rc == SXERR_ABORT ){` |
|         - |  4926 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4927 | `			return SXERR_ABORT;` |
|         - |  4928 | `		}` |
|       ! 0 |  4929 | `		goto Synchronize;` |
|         - |  4930 | `	}` |
|    302225 |  4931 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4932 | `		pGen->pIn++;` |
|         - |  4933 | `		/* Pass by reference  */` |
|        33 |  4934 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4935 | `	}` |
|         - |  4936 | `	/* Check if the value target is list() */` |
|    302225 |  4937 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    302220 |  4978 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|    302203 |  5011 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    302203 |  5012 | `		if( rc == SXERR_ABORT ){` |
|         - |  5013 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5014 | `			return SXERR_ABORT;` |
|         - |  5015 | `		}` |
|    302203 |  5016 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    302203 |  5017 | `		if( pInstr->p3 ){` |
|         - |  5018 | `			/* Record value name */` |
|    302203 |  5019 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    151099 |  5020 | `		}` |
|         - |  5021 | `	}` |
|         - |  5022 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    302223 |  5023 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  5024 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    302223 |  5025 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  5026 | `	/* Record the first instruction to execute */` |
|    302223 |  5027 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5028 | `	/* Emit the FOREACH_STEP instruction */` |
|    302223 |  5029 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  5030 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    302223 |  5031 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  5032 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    302223 |  5033 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    302223 |  5061 | `	pGen->pIn = &pEnd[1];` |
|    302223 |  5062 | `	pGen->pEnd = pTmp;` |
|    302223 |  5063 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    302223 |  5064 | `	if( rc == SXERR_ABORT ){` |
|         - |  5065 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5066 | `		return SXERR_ABORT;` |
|         - |  5067 | `	}` |
|         - |  5068 | `	/* Emit the unconditional jump to the start of the loop */` |
|    302223 |  5069 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  5070 | `	/* Fix all jumps now the destination is resolved */` |
|    302223 |  5071 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  5072 | `	/* Release the loop block */` |
|    302223 |  5073 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5074 | `	/* Statement successfully compiled */` |
|    302223 |  5075 | `	return SXRET_OK;` |
|         1 |  5076 | `Synchronize:` |
|         - |  5077 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  5078 | `	 * compiling this erroneous block.` |
|         - |  5079 | `	 */` |
|         3 |  5080 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  5081 | `		pGen->pIn++;` |
|       ! 0 |  5082 | `	}` |
|         3 |  5083 | `	return SXRET_OK;` |
|    151115 |  5084 | `}` |
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
|   2192366 |  5117 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  5118 | `{` |
|   2192371 |  5119 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   2192371 |  5120 | `	GenBlock *pCondBlock = 0;` |
|         - |  5121 | `	sxu32 nJumpIdx;` |
|         - |  5122 | `	sxu32 nKeyID;` |
|         - |  5123 | `	sxi32 rc;` |
|         - |  5124 | `	/* Jump the 'if' keyword */` |
|   2192371 |  5125 | `	pGen->pIn++;` |
|   2192371 |  5126 | `	pToken = pGen->pIn;` |
|         - |  5127 | `	/* Create the conditional block */` |
|   2192371 |  5128 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   2192371 |  5129 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5130 | `		return SXERR_ABORT;` |
|         - |  5131 | `	}` |
|         - |  5132 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1235526 |  5133 | `	for(;;){` |
|   2471057 |  5134 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|   2471057 |  5147 | `		pToken++;` |
|         - |  5148 | `		/* Delimit the condition */` |
|   2471057 |  5149 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2471057 |  5150 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|   2471049 |  5163 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  5164 | `		/* Compile the condition */` |
|   2471049 |  5165 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5166 | `		/* Update token stream */` |
|   2471049 |  5167 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5168 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5169 | `			pGen->pIn++;` |
|       ! 0 |  5170 | `		}` |
|   2471049 |  5171 | `		pGen->pIn  = &pEnd[1];` |
|   2471049 |  5172 | `		pGen->pEnd = pTmp;` |
|   2471049 |  5173 | `		if( rc == SXERR_ABORT ){` |
|         - |  5174 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5175 | `			return SXERR_ABORT;` |
|         - |  5176 | `		}` |
|         - |  5177 | `		/* Emit the false jump */` |
|   2471049 |  5178 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5179 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2471049 |  5180 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5181 | `		/* Compile the body */` |
|   2471049 |  5182 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2471049 |  5183 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5184 | `			return SXERR_ABORT;` |
|         - |  5185 | `		}` |
|   2471049 |  5186 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    483342 |  5187 | `			break;` |
|         - |  5188 | `		}` |
|         - |  5189 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1504375 |  5190 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1504375 |  5191 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   1015359 |  5192 | `			break;` |
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
|   2192363 |  5213 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   2192363 |  5214 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1225684 |  5215 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5216 | `			/* Compile the else block */` |
|    210335 |  5217 | `			pGen->pIn++;` |
|    210335 |  5218 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    210335 |  5219 | `			if( rc == SXERR_ABORT ){` |
|         - |  5220 |  |
|       ! 0 |  5221 | `				return SXERR_ABORT;` |
|         - |  5222 | `			}` |
|    105165 |  5223 | `	}` |
|   2192363 |  5224 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5225 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   2192363 |  5226 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5227 | `	/* Release the conditional block */` |
|   2192363 |  5228 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5229 | `	/* Statement successfully compiled */` |
|   2192363 |  5230 | `	return SXRET_OK;` |
|         4 |  5231 | `Synchronize:` |
|         - |  5232 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5233 | `	 */` |
|        67 |  5234 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5235 | `		pGen->pIn++;` |
|         3 |  5236 | `	}` |
|        11 |  5237 | `	return SXRET_OK;` |
|   1096188 |  5238 | `}` |
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
|   2971064 |  5332 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5333 | `{` |
|   2971069 |  5334 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5335 | `	sxi32 rc;` |
|   2971069 |  5336 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2971069 |  5337 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5338 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5339 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5340 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5341 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5342 | `	 * normally below so token processing stays consistent. */` |
|   7820411 |  5343 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4849347 |  5344 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5345 | `	}` |
|   2971064 |  5346 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2971035 |  5347 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5348 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5349 | `			"A never-returning function must not return");` |
|         3 |  5350 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5351 | `			return SXERR_ABORT;` |
|         - |  5352 | `		}` |
|         1 |  5353 | `	}` |
|         - |  5354 | `	/* Jump the 'return' keyword */` |
|   2971069 |  5355 | `	pGen->pIn++;` |
|   2971069 |  5356 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5357 | `		/* Compile the expression */` |
|   2875639 |  5358 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2875639 |  5359 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5360 | `			return SXERR_ABORT;` |
|   2875639 |  5361 | `		}else if(rc != SXERR_EMPTY ){` |
|   2875639 |  5362 | `			nRet = 1;` |
|   1437817 |  5363 | `		}` |
|   1437817 |  5364 | `	}` |
|         - |  5365 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5366 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5367 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5368 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2971069 |  5369 | `	if( pGen->bInGenerator ){` |
|      3849 |  5370 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3849 |  5371 | `		return SXRET_OK;` |
|         - |  5372 | `	}` |
|         - |  5373 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5374 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5375 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5376 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5377 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2967225 |  5378 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2967225 |  5379 | `	return SXRET_OK;` |
|   1485537 |  5380 | `}` |
|         - |  5381 | `/*` |
|         - |  5382 | ` * Compile a yield expression.` |
|         - |  5383 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5384 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5385 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5386 | ` */` |
|     15648 |  5387 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5388 | `{` |
|         - |  5389 | `	SyToken *pTmp, *pSplit;` |
|     15653 |  5390 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15653 |  5391 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5392 | `	sxi32 rc;` |
|      7824 |  5393 | `	(void)iCompileFlag;` |
|         - |  5394 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15653 |  5395 | `	pGen->pIn++;` |
|         - |  5396 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5397 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5398 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5399 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5400 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15648 |  5401 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7859 |  5402 | `		&& pGen->pIn->sData.nByte == 4` |
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
|     15591 |  5420 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5421 | `		/* Bare yield — no value */` |
|         3 |  5422 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5423 | `		return SXRET_OK;` |
|         - |  5424 | `	}` |
|         - |  5425 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15589 |  5426 | `	pSplit = 0;` |
|         - |  5427 | `	{` |
|     15589 |  5428 | `		SyToken *pCur = pGen->pIn;` |
|     15589 |  5429 | `		sxi32 nNest = 0;` |
|     46573 |  5430 | `		while( pCur < pGen->pEnd ){` |
|     46267 |  5431 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5432 | `				nNest++;` |
|     46259 |  5433 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5434 | `				nNest--;` |
|     46243 |  5435 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15283 |  5436 | `				pSplit = pCur;` |
|     15283 |  5437 | `				break;` |
|         - |  5438 | `			}` |
|     30989 |  5439 | `			pCur++;` |
|         5 |  5440 | `		}` |
|         - |  5441 | `	}` |
|     15589 |  5442 | `	pTmp = pGen->pEnd;` |
|     15589 |  5443 | `	if( pSplit ){` |
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
|       311 |  5456 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       311 |  5457 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       311 |  5458 | `		if( rc != SXERR_EMPTY ){` |
|       311 |  5459 | `			iP1 = 1;` |
|       153 |  5460 | `		}` |
|         - |  5461 | `	}` |
|     15589 |  5462 | `	pGen->pEnd = pTmp;` |
|     15589 |  5463 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15589 |  5464 | `	return SXRET_OK;` |
|      7829 |  5465 | `}` |
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
|     17728 |  5493 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5494 | `{` |
|     17733 |  5495 | `	SyToken *pTmp,*pNext = 0;` |
|     17733 |  5496 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17733 |  5497 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17733 |  5498 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5499 | `	sxi32 rc;` |
|         - |  5500 | `	/* Jump the 'echo' keyword */` |
|     17733 |  5501 | `	pGen->pIn++;` |
|         - |  5502 | `	/* Compile arguments one after one */` |
|     17733 |  5503 | `	pTmp = pGen->pEnd;` |
|     44781 |  5504 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     27055 |  5505 | `		if( pGen->pIn < pNext ){` |
|     27055 |  5506 | `			pGen->pEnd = pNext;` |
|     27055 |  5507 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     27055 |  5508 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5509 | `				return SXERR_ABORT;` |
|     27055 |  5510 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5511 | `				/* Emit the consume instruction */` |
|     27029 |  5512 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     27029 |  5513 | `				nExpr++;` |
|     27029 |  5514 | `				bExpectMore = 0;` |
|     13512 |  5515 | `			}` |
|     13525 |  5516 | `		}` |
|         - |  5517 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5518 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     36383 |  5519 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      9335 |  5520 | `			if( bExpectMore ){` |
|         - |  5521 | `				/* two commas in a row */` |
|         3 |  5522 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5523 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5524 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5525 | `			}` |
|      9333 |  5526 | `			bExpectMore = 1;` |
|      9333 |  5527 | `			pNext++;` |
|         5 |  5528 | `		}` |
|     27053 |  5529 | `		pGen->pIn = pNext;` |
|         5 |  5530 | `	}` |
|         - |  5531 | `	/* Restore token stream */` |
|     17731 |  5532 | `	pGen->pEnd = pTmp;` |
|     17731 |  5533 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5534 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5535 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5536 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5537 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5538 | `	}` |
|     17701 |  5539 | `	return SXRET_OK;` |
|      8869 |  5540 | `}` |
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
|   5570024 |  5721 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
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
|   5570029 |  5732 | `	if( pFromImport ){` |
|   4497661 |  5733 | `		*pFromImport = 0;` |
|   2248828 |  5734 | `	}` |
|   5570029 |  5735 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   5570029 |  5736 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5737 | `		return nOrigIdx;` |
|         - |  5738 | `	}` |
|   5570029 |  5739 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   5570029 |  5740 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5741 | `	/* Skip if already qualified (contains backslash) */` |
|   5570029 |  5742 | `	hasNsSep = 0;` |
|  66436105 |  5743 | `	for( k = 0; k < nLit; k++ ){` |
|  60866085 |  5744 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  30433043 |  5745 | `	}` |
|   5570029 |  5746 | `	if( hasNsSep ){` |
|         5 |  5747 | `		return nOrigIdx;` |
|         - |  5748 | `	}` |
|         - |  5749 | `	/* Check use imports first (works even outside namespaces) */` |
|   5570025 |  5750 | `	SyBlobReset(&pGen->sWorker);` |
|   5570025 |  5751 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   5570025 |  5752 | `	if( pImport ){` |
|        41 |  5753 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5754 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5755 | `		if( pFromImport ){` |
|        18 |  5756 | `			*pFromImport = 1;` |
|         8 |  5757 | `		}` |
|        23 |  5758 | `	}else{` |
|   5569989 |  5759 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   5569891 |  5760 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5761 | `		}` |
|         - |  5762 | `		/* Prepend current namespace */` |
|       103 |  5763 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       103 |  5764 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       103 |  5765 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5766 | `	}` |
|         - |  5767 | `	/* Look up or create a new literal for the qualified name */` |
|       139 |  5768 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       139 |  5769 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        59 |  5770 | `		return nNewIdx; /* Already interned */` |
|         - |  5771 | `	}` |
|        85 |  5772 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        85 |  5773 | `	if( pNew == 0 ){` |
|       ! 0 |  5774 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5775 | `	}` |
|        85 |  5776 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        85 |  5777 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        85 |  5778 | `	return nNewIdx;` |
|   2785017 |  5779 | `}` |
|         - |  5780 | `/*` |
|         - |  5781 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5782 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5783 | ` */` |
|    409862 |  5784 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5785 | `{` |
|         - |  5786 | `	SyHashEntry *pImport;` |
|         - |  5787 | `	/* Check use imports first */` |
|    409867 |  5788 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    409867 |  5789 | `	if( pImport ){` |
|        19 |  5790 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        19 |  5791 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        19 |  5792 | `		return;` |
|         - |  5793 | `	}` |
|         - |  5794 | `	/* Prepend current namespace if active */` |
|    409851 |  5795 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         8 |  5796 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  5797 | `		SyBlobAppend(pOut,"\\",1);` |
|         3 |  5798 | `	}` |
|    409851 |  5799 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    204936 |  5800 | `}` |
|         - |  5801 | `/*` |
|         - |  5802 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5803 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5804 | ` * The caller must release pOut when done.` |
|         - |  5805 | ` */` |
|    429374 |  5806 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5807 | `{` |
|    429379 |  5808 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3881 |  5809 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3881 |  5810 | `		SyBlobAppend(pOut,"\\",1);` |
|      1938 |  5811 | `	}` |
|    429379 |  5812 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    429379 |  5813 | `}` |
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
|      3924 |  5860 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5861 | `{` |
|         - |  5862 | `	sxu32 nLine;` |
|         - |  5863 | `	sxi32 rc;` |
|      3929 |  5864 | `	nLine = pGen->pIn->nLine;` |
|      3929 |  5865 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5866 | `	/* Reset namespace and clear previous use imports */` |
|      3929 |  5867 | `	SyBlobReset(&pGen->sNamespace);` |
|      3929 |  5868 | `	SyHashRelease(&pGen->hUseImports);` |
|      3929 |  5869 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3929 |  5870 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3929 |  5871 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3929 |  5872 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3929 |  5873 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3929 |  5874 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5875 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5876 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5877 | `		return SXRET_OK;` |
|         - |  5878 | `	}` |
|      3929 |  5879 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5880 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5881 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5882 | `		return SXRET_OK;` |
|         - |  5883 | `	}` |
|      3929 |  5884 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5885 | `		/* namespace { } — global namespace block */` |
|       ! 0 |  5886 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5887 | `		return SXRET_OK;` |
|         - |  5888 | `	}` |
|         - |  5889 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7899 |  5890 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3975 |  5891 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5892 | `			/* Append backslash separator */` |
|        28 |  5893 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        28 |  5894 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        12 |  5895 | `			}` |
|        16 |  5896 | `		}else{` |
|         - |  5897 | `			/* Append identifier */` |
|      3951 |  5898 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5899 | `		}` |
|      3975 |  5900 | `		pGen->pIn++;` |
|         5 |  5901 | `	}` |
|         - |  5902 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5903 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5904 | `	{` |
|      3929 |  5905 | `		char *zNsDup = 0;` |
|      3929 |  5906 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5888 |  5907 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3922 |  5908 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1961 |  5909 | `		}` |
|      3929 |  5910 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5911 | `	}` |
|      3929 |  5912 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5913 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5914 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5915 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5916 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5917 | `			return SXERR_ABORT;` |
|         - |  5918 | `		}` |
|         2 |  5919 | `	}` |
|      3929 |  5920 | `	return SXRET_OK;` |
|      1967 |  5921 | `}` |
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
|        72 |  5936 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
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
|        77 |  5947 | `	nLine = pGen->pIn->nLine;` |
|        77 |  5948 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5949 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        77 |  5950 | `	iUseType = 0;` |
|        77 |  5951 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
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
|        77 |  5962 | `	switch( iUseType ){` |
|         7 |  5963 | `		case 1:` |
|        16 |  5964 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5965 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5966 | `			break;` |
|         7 |  5967 | `		case 2:` |
|        16 |  5968 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5969 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5970 | `			break;` |
|        22 |  5971 | `		default:` |
|        49 |  5972 | `			pGenHash = &pGen->hUseImports;` |
|        49 |  5973 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        44 |  5974 | `			break;` |
|         - |  5975 | `	}` |
|        77 |  5976 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5977 | `	/* Process one or more use declarations separated by commas */` |
|        37 |  5978 | `	for(;;){` |
|        79 |  5979 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5980 | `			break;` |
|         - |  5981 | `		}` |
|        79 |  5982 | `		SyBlobReset(&sPath);` |
|        79 |  5983 | `		pLast = 0;` |
|         - |  5984 | `		/* Collect the full namespace path */` |
|       269 |  5985 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       195 |  5986 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       135 |  5987 | `				pLast = pGen->pIn;` |
|       135 |  5988 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        65 |  5989 | `					SyBlobAppend(&sPath,"\\",1);` |
|        30 |  5990 | `				}` |
|       135 |  5991 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        65 |  5992 | `			}` |
|       195 |  5993 | `			pGen->pIn++;` |
|         5 |  5994 | `		}` |
|        79 |  5995 | `		if( pLast == 0 ){` |
|         - |  5996 | `			/* Empty path */` |
|         6 |  5997 | `			break;` |
|         - |  5998 | `		}` |
|         - |  5999 | `		/* Default alias is the last component of the path */` |
|        75 |  6000 | `		sAlias = pLast->sData;` |
|         - |  6001 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        70 |  6002 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        50 |  6003 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        23 |  6004 | `			pGen->pIn++; /* Jump 'as' */` |
|        23 |  6005 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        23 |  6006 | `				sAlias = pGen->pIn->sData;` |
|        23 |  6007 | `				pGen->pIn++;` |
|        10 |  6008 | `			}` |
|        10 |  6009 | `		}` |
|         - |  6010 | `		/* Check for duplicate import alias (per-type) */` |
|        75 |  6011 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
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
|       110 |  6024 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        70 |  6025 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        75 |  6026 | `		if( zDup ){` |
|        75 |  6027 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        75 |  6028 | `			if( pVmHash ){` |
|         - |  6029 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  6030 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        47 |  6031 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        47 |  6032 | `				if( zAliasDup ){` |
|        47 |  6033 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        21 |  6034 | `				}` |
|        21 |  6035 | `			}` |
|        75 |  6036 | `			if( iUseType == 2 ){` |
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
|        35 |  6052 | `		}` |
|         - |  6053 | `		/* Check for comma (multiple use declarations) */` |
|        75 |  6054 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  6055 | `			pGen->pIn++;` |
|         2 |  6056 | `		}else{` |
|        39 |  6057 | `			break;` |
|         - |  6058 | `		}` |
|         1 |  6059 | `	}` |
|        77 |  6060 | `	SyBlobRelease(&sPath);` |
|        77 |  6061 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  6062 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  6063 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  6064 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6065 | `			return SXERR_ABORT;` |
|         - |  6066 | `		}` |
|         1 |  6067 | `	}` |
|        77 |  6068 | `	return SXRET_OK;` |
|        41 |  6069 | `}` |
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
|    561110 |  6311 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6312 | `{` |
|         - |  6313 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6314 | `	SySet *pInstrContainer;` |
|         - |  6315 | `	sxi32 rc;` |
|         - |  6316 | `	/* Swap token stream */` |
|    561115 |  6317 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    561115 |  6318 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    561115 |  6319 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6320 | `	/* Compile the expression holding the argument value */` |
|    561115 |  6321 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6322 | `	/* Emit the done instruction */` |
|    561115 |  6323 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    561115 |  6324 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    561115 |  6325 | `	RE_SWAP_DELIMITER(pGen);` |
|    561115 |  6326 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6327 | `		return SXERR_ABORT;` |
|         - |  6328 | `	}` |
|    561115 |  6329 | `	return SXRET_OK;` |
|    280560 |  6330 | `}` |
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
|   1280320 |  6368 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6369 | `{` |
|         - |  6370 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6371 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6372 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6373 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6374 | `	sxi32 rc;` |
|         - |  6375 |  |
|   1280325 |  6376 | `	pIn = pGen->pIn;` |
|   1280325 |  6377 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6378 | `	/* Process arguments one after one */` |
|   1662410 |  6379 | `	for(;;){` |
|   3324825 |  6380 | `		if( pIn >= pEnd ){` |
|         - |  6381 | `			/* No more arguments to process */` |
|   1280309 |  6382 | `			break;` |
|         - |  6383 | `		}` |
|   2044521 |  6384 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2044521 |  6385 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2044521 |  6386 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2044521 |  6387 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2044521 |  6388 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6389 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6390 | `		 * first token inside the main token stream */` |
|   2044521 |  6391 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6392 | `			return SXERR_ABORT;` |
|         - |  6393 | `		}` |
|         - |  6394 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6395 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6396 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6397 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6398 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6399 | `		{` |
|   2044521 |  6400 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2044521 |  6401 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2044521 |  6402 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6403 | `			int nSetTok;` |
|         - |  6404 | `			sxi32 nSetVis;` |
|   2044521 |  6405 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6406 | `				bReadonly = 1;` |
|         3 |  6407 | `				pIn++;` |
|         1 |  6408 | `			}` |
|   2044521 |  6409 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2044521 |  6410 | `			if( nSetVis ){` |
|         - |  6411 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6412 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6413 | `				bVisSeen = 1;` |
|         3 |  6414 | `				pIn += nSetTok;` |
|         3 |  6415 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6416 | `					bReadonly = 1;` |
|       ! 0 |  6417 | `					pIn++;` |
|         1 |  6418 | `				}` |
|   2044520 |  6419 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
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
|   2044521 |  6439 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6440 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2044519 |  6441 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6442 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6443 | `			}` |
|   2044521 |  6444 | `			if( bVisSeen \|\| bReadonly ){` |
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
|   2044512 |  6466 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1102707 |  6467 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    149442 |  6468 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    114999 |  6469 | `			sxu32 nLineLocal = pIn->nLine;` |
|    114999 |  6470 | `			sxi32 iTFlags = 0;` |
|    114999 |  6471 | `			pGen->pIn = pIn;` |
|    114999 |  6472 | `			rc = GenStateParseUnionTypeDecl(` |
|     57497 |  6473 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57497 |  6474 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6475 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6476 | `				/* bAllowVoid */ 0,` |
|     57497 |  6477 | `						nLineLocal);` |
|    114999 |  6478 | `			pIn = pGen->pIn;` |
|    114999 |  6479 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6480 | `				return SXERR_ABORT;` |
|    114999 |  6481 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6482 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6483 | `				return SXERR_SYNTAX;` |
|    114997 |  6484 | `			}else if( rc == SXERR_SYNTAX ){` |
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
|    114989 |  6495 | `			sArg.iFlags \|= iTFlags;` |
|     57492 |  6496 | `		}` |
|   2044507 |  6497 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6498 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6499 | `			return rc;` |
|         - |  6500 | `		}` |
|   2044507 |  6501 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6502 | `			/* Pass by reference,record that */` |
|     22943 |  6503 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     22943 |  6504 | `			pIn++;` |
|     11469 |  6505 | `		}` |
|   2044507 |  6506 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6507 | `			/* Variadic parameter: ...$args */` |
|     23005 |  6508 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     23005 |  6509 | `			pIn++;` |
|     11500 |  6510 | `		}` |
|   2044507 |  6511 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6512 | `			/* Invalid argument */` |
|       ! 0 |  6513 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6514 | `			return rc;` |
|         - |  6515 | `		}` |
|   2044507 |  6516 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6517 | `		/* Copy argument name */` |
|   2044507 |  6518 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2044507 |  6519 | `		if( zDup == 0 ){` |
|       ! 0 |  6520 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6521 | `			return SXERR_ABORT;` |
|         - |  6522 | `		}` |
|   2044507 |  6523 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2044507 |  6524 | `		pIn++;` |
|   2044507 |  6525 | `		if( pIn < pEnd ){` |
|   1138285 |  6526 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6527 | `				SyToken *pDefend;` |
|    561117 |  6528 | `				sxi32 iNest = 0;` |
|    561117 |  6529 | `				pIn++; /* Jump the equal sign */` |
|    561117 |  6530 | `				pDefend = pIn;` |
|         - |  6531 | `				/* Process the default value associated with this argument */` |
|   1179495 |  6532 | `				while( pDefend < pEnd ){` |
|    805417 |  6533 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    187039 |  6534 | `						break;` |
|         - |  6535 | `					}` |
|    618383 |  6536 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6537 | `						/* Increment nesting level */` |
|     26725 |  6538 | `						iNest++;` |
|    605023 |  6539 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6540 | `						/* Decrement nesting level */` |
|     26725 |  6541 | `						iNest--;` |
|     13360 |  6542 | `					}` |
|    618383 |  6543 | `					pDefend++;` |
|         5 |  6544 | `				}` |
|    561117 |  6545 | `				if( pIn >= pDefend ){` |
|         3 |  6546 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6547 | `					return rc;` |
|         - |  6548 | `				}` |
|         - |  6549 | `				/* Process default value */` |
|    561115 |  6550 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    561115 |  6551 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6552 | `					return rc;` |
|         - |  6553 | `				}` |
|         - |  6554 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6555 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6556 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6557 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6558 | `				 * arg-type check lets null through. */` |
|    561110 |  6559 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    305382 |  6560 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    305379 |  6561 | `					&& &pIn[1] == pDefend` |
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
|    561115 |  6583 | `				pIn = pDefend;` |
|    280555 |  6584 | `			}` |
|   1138283 |  6585 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6586 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6587 | `				return rc;` |
|         - |  6588 | `			}` |
|   1138283 |  6589 | `			pIn++; /* Jump the trailing comma */` |
|    569139 |  6590 | `		}` |
|         - |  6591 | `		/* Append argument signature */` |
|   2044505 |  6592 | `		if( sArg.nType > 0 ){` |
|    114927 |  6593 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6594 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26799 |  6595 | `				int marker = 'o';` |
|     26799 |  6596 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26799 |  6597 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13402 |  6598 | `			}else{` |
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
|     57466 |  6632 | `		}else{` |
|         - |  6633 | `			/* No type is associated with this parameter which mean` |
|         - |  6634 | `			 * that this function is not condidate for overloading.` |
|         - |  6635 | `			 */` |
|   1929583 |  6636 | `			SyBlobRelease(&sSig);` |
|         - |  6637 | `		}` |
|         - |  6638 | `		/* Save in the argument set */` |
|   2044505 |  6639 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6640 | `	}` |
|   1280309 |  6641 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6642 | `		/* Save function signature */` |
|     84327 |  6643 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     42161 |  6644 | `	}` |
|   1280309 |  6645 | `	return SXRET_OK;` |
|    640165 |  6646 | `}` |
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
|     11476 |  6693 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6694 | `{` |
|         - |  6695 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6696 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6697 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6698 | `	};` |
|         - |  6699 | `	sxu32 i;` |
|     11481 |  6700 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6701 | `		zName++;` |
|       ! 0 |  6702 | `		nName--;` |
|       ! 0 |  6703 | `	}` |
|     11513 |  6704 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11509 |  6705 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11477 |  6706 | `			return 1;` |
|         - |  6707 | `		}` |
|        17 |  6708 | `	}` |
|         5 |  6709 | `	return 0;` |
|      5743 |  6710 | `}` |
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
|     11473 |  6732 | `		return 1;` |
|         - |  6733 | `	}` |
|         - |  6734 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6735 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6736 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6737 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6738 | `	{` |
|         - |  6739 | `		SyBlob sFQN;` |
|         - |  6740 | `		int bOk;` |
|         5 |  6741 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         5 |  6742 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|         5 |  6743 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|         5 |  6744 | `		SyBlobRelease(&sFQN);` |
|         5 |  6745 | `		return bOk;` |
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
|     11712 |  6758 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6759 | `{` |
|     11717 |  6760 | `	int bOk = 0;` |
|         - |  6761 | `	sxu32 nLine;` |
|         - |  6762 | `	sxi32 rc;` |
|     11717 |  6763 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       243 |  6764 | `		return SXRET_OK; /* untyped: nothing to validate */` |
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
|      5861 |  6814 | `}` |
|   2735952 |  6815 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6816 | `{` |
|   2735957 |  6817 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2735957 |  6818 | `	SyToken *pEnd = pGen->pEnd;` |
|   2735957 |  6819 | `	sxi32 iDepth = 0;` |
|   2735957 |  6820 | `	int bStarted = 0;` |
| 132092017 |  6821 | `	while( pIn < pEnd ){` |
| 132092017 |  6822 | `		sxu32 t = pIn->nType;` |
| 132092017 |  6823 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 126172525 |  6824 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 120287755 |  6825 | `		if( t & PH7_TK_KEYWORD ){` |
|   8938805 |  6826 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   8938805 |  6827 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   8927093 |  6828 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6829 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   4446350 |  6830 | `		}` |
| 120241655 |  6831 | `		pIn++;` |
|         5 |  6832 | `	}` |
|   2724245 |  6833 | `	return FALSE;` |
|   1367981 |  6834 | `}` |
|         - |  6835 | `/*` |
|         - |  6836 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6837 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6838 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6839 | ` */` |
|   2735952 |  6840 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6841 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6842 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6843 | `	)` |
|         5 |  6844 | `{` |
|         - |  6845 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6846 | `	GenBlock *pBlock;` |
|         - |  6847 | `	sxu32 nGotoOfft;` |
|         - |  6848 | `	sxi32 rc;` |
|         - |  6849 | `	/* Attach the new function */` |
|   2735957 |  6850 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2735957 |  6851 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6852 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6853 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6854 | `		return SXERR_ABORT;` |
|         - |  6855 | `	}` |
|   2735957 |  6856 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6857 | `	/* Swap bytecode containers */` |
|   2735957 |  6858 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2735957 |  6859 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6860 | `	/* Emit constructor property promotion prologue:` |
|         - |  6861 | `	 *   $this->NAME = $NAME;` |
|         - |  6862 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6863 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6864 | `	{` |
|   2735957 |  6865 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6866 | `		sxu32 i;` |
|   4726895 |  6867 | `		for( i = 0; i < nArg; i++ ){` |
|   1990943 |  6868 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6869 | `			char *zSrc;` |
|         - |  6870 | `			sxu32 nSrc,nName;` |
|         - |  6871 | `			SySet sToken;` |
|         - |  6872 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6873 | `			sxi32 rcPromote;` |
|   1990943 |  6874 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1990869 |  6875 | `				continue;` |
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
|   2735957 |  6927 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2735957 |  6928 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6929 | `		/* Compile the body */` |
|   2735957 |  6930 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2735957 |  6931 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6932 | `	}` |
|         - |  6933 | `	/* Fix exception jumps now the destination is resolved */` |
|   2735957 |  6934 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6935 | `	/* Emit the final return if not yet done */` |
|   2735957 |  6936 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6937 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2735957 |  6938 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6939 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6940 | `	}` |
|   2735957 |  6941 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6942 | `	/* Restore the default container */` |
|   2735957 |  6943 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6944 | `	/* Leave function block */` |
|   2735957 |  6945 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2735957 |  6946 | `	if( rc == SXERR_ABORT ){` |
|         - |  6947 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6948 | `		return SXERR_ABORT;` |
|         - |  6949 | `	}` |
|         - |  6950 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6951 | `	{` |
|   2735957 |  6952 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6953 | `		sxu32 i;` |
|  80659117 |  6954 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  77934877 |  6955 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11717 |  6956 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11717 |  6957 | `				break;` |
|         - |  6958 | `			}` |
|  38961585 |  6959 | `		}` |
|         - |  6960 | `	}` |
|   2735957 |  6961 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6962 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11717 |  6963 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6964 | `			return SXERR_ABORT;` |
|         - |  6965 | `		}` |
|      5856 |  6966 | `	}` |
|         - |  6967 | `	/* All done, function body compiled */` |
|   2735957 |  6968 | `	return SXRET_OK;` |
|   1367981 |  6969 | `}` |
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
|    127650 |  7032 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  7033 | `{` |
|    127655 |  7034 | `	SyToken *pIn = pGen->pIn;` |
|    127655 |  7035 | `	SyZero(pOut, sizeof(*pOut));` |
|    127655 |  7036 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    127655 |  7037 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7038 | `		return SXERR_SYNTAX;` |
|         - |  7039 | `	}` |
|         - |  7040 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    127655 |  7041 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|         8 |  7042 | `		pIn++;` |
|         8 |  7043 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7044 | `			return SXERR_SYNTAX;` |
|         - |  7045 | `		}` |
|         3 |  7046 | `	}` |
|    127655 |  7047 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7048 | `		return SXERR_SYNTAX;` |
|         - |  7049 | `	}` |
|    127655 |  7050 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     88953 |  7051 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     88953 |  7052 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11523 |  7053 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     83194 |  7054 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        81 |  7055 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     77397 |  7056 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19731 |  7057 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67496 |  7058 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57551 |  7059 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     28860 |  7060 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  7061 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        69 |  7062 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        28 |  7063 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        38 |  7064 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        14 |  7065 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        23 |  7066 | `			pOut->nType = SXU32_HIGH;` |
|        23 |  7067 | `			pOut->sClass = pIn->sData;` |
|        13 |  7068 | `		}else{` |
|         3 |  7069 | `			return SXERR_SYNTAX;` |
|         - |  7070 | `		}` |
|     88951 |  7071 | `		pIn++;` |
|     44478 |  7072 | `	}else{` |
|         - |  7073 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  7074 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38707 |  7075 | `		SyString *pT = &pIn->sData;` |
|     38707 |  7076 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  7077 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  7078 | `			pIn++;` |
|     38692 |  7079 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  7080 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  7081 | `			pIn++;` |
|     38591 |  7082 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        27 |  7083 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        27 |  7084 | `			pIn++;` |
|        16 |  7085 | `		}else{` |
|         - |  7086 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38483 |  7087 | `			SyToken *pFirst = pIn;` |
|     38483 |  7088 | `			SyToken *pLast = pIn;` |
|     38483 |  7089 | `			pOut->nType = SXU32_HIGH;` |
|     38483 |  7090 | `			pOut->sClass = pIn->sData;` |
|     38483 |  7091 | `			pIn++;` |
|     57720 |  7092 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38486 |  7093 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  7094 | `				pLast = &pIn[1];` |
|         3 |  7095 | `				pIn += 2;` |
|         1 |  7096 | `			}` |
|     38483 |  7097 | `			if( pLast != pFirst ){` |
|         3 |  7098 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  7099 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  7100 | `				pOut->sClass.zString = zFirst;` |
|         3 |  7101 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  7102 | `			}` |
|         - |  7103 | `		}` |
|         - |  7104 | `	}` |
|    127653 |  7105 | `	pGen->pIn = pIn;` |
|    127653 |  7106 | `	return SXRET_OK;` |
|     63830 |  7107 | `}` |
|         - |  7108 |  |
|         - |  7109 | `/*` |
|         - |  7110 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  7111 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  7112 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  7113 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  7114 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  7115 | ` */` |
|    127472 |  7116 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  7117 | `{` |
|         - |  7118 | `	int i;` |
|    127477 |  7119 | `	int nNonNull = 0;` |
|    127477 |  7120 | `	int bAnyIntersection = 0;` |
|         - |  7121 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    127477 |  7122 | `	sxu32 nMaxGroup = 0;` |
|   4206581 |  7123 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    255101 |  7124 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127629 |  7125 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127599 |  7126 | `			nNonNull++;` |
|    127599 |  7127 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    127599 |  7128 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    127599 |  7129 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     63797 |  7130 | `			}` |
|     63797 |  7131 | `		}` |
|     63817 |  7132 | `	}` |
|    255049 |  7133 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127601 |  7134 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  7135 | `			bAnyIntersection = 1;` |
|        29 |  7136 | `			break;` |
|         - |  7137 | `		}` |
|     63791 |  7138 | `	}` |
|    127477 |  7139 | `	if( bAnyIntersection ){` |
|         - |  7140 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  7141 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  7142 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  7143 | `		sxu32 g, nGroups = 0;` |
|        29 |  7144 | `		int bFirstGroup = 1;` |
|        59 |  7145 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  7146 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  7147 | `			int bFirstMember = 1;` |
|         - |  7148 | `			int bWrap;` |
|        35 |  7149 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  7150 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  7151 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  7152 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  7153 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  7154 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  7155 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  7156 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  7157 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  7158 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  7159 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  7160 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  7161 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  7162 | `				}else{` |
|         6 |  7163 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7164 | `				}` |
|        59 |  7165 | `				bFirstMember = 0;` |
|        32 |  7166 | `			}` |
|        35 |  7167 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7168 | `			bFirstGroup = 0;` |
|        20 |  7169 | `		}` |
|        29 |  7170 | `		if( bNullable ){` |
|       ! 0 |  7171 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7172 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7173 | `		}` |
|        83 |  7174 | `		return;` |
|         - |  7175 | `	}` |
|    127453 |  7176 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7177 | `		/* Shorthand: ?T */` |
|       113 |  7178 | `		for( i = 0; i < nAtoms; i++ ){` |
|       113 |  7179 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       113 |  7180 | `			SyBlobAppend(pBlob, "?", 1);` |
|       113 |  7181 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        22 |  7182 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        12 |  7183 | `			}else{` |
|        93 |  7184 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7185 | `			}` |
|       113 |  7186 | `			return;` |
|       ! 0 |  7187 | `		}` |
|       ! 0 |  7188 | `	}` |
|         - |  7189 | `	{` |
|    127345 |  7190 | `		int bFirst = 1;` |
|         - |  7191 | `		/* 1) Classes in declaration order */` |
|    254793 |  7192 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127453 |  7193 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38433 |  7194 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38433 |  7195 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38433 |  7196 | `				bFirst = 0;` |
|     19214 |  7197 | `			}` |
|     63729 |  7198 | `		}` |
|         - |  7199 | `		/* 2) Built-ins in canonical order */` |
|         - |  7200 | `		{` |
|         - |  7201 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7202 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7203 | `			int k;` |
|    891385 |  7204 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1439803 |  7205 | `				for( i = 0; i < nAtoms; i++ ){` |
|    764581 |  7206 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     88823 |  7207 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     88823 |  7208 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     88823 |  7209 | `						bFirst = 0;` |
|     88823 |  7210 | `						break;` |
|         - |  7211 | `					}` |
|    337884 |  7212 | `				}` |
|    382025 |  7213 | `			}` |
|         - |  7214 | `		}` |
|         - |  7215 | `		/* 3) null suffix */` |
|    127345 |  7216 | `		if( bNullable ){` |
|        20 |  7217 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        20 |  7218 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7219 | `		}` |
|         - |  7220 | `	}` |
|     63741 |  7221 | `}` |
|         - |  7222 |  |
|         - |  7223 | `/*` |
|         - |  7224 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7225 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7226 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7227 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7228 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7229 | ` * whether it was parenthesized.` |
|         - |  7230 | ` *` |
|         - |  7231 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7232 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7233 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7234 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7235 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7236 | ` */` |
|    127624 |  7237 | `static sxi32 GenStateParsePart(` |
|         - |  7238 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7239 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7240 | `{` |
|         - |  7241 | `	sxi32 rc;` |
|    127629 |  7242 | `	int nMembers = 0;` |
|    127629 |  7243 | `	int bParen = 0;` |
|    127629 |  7244 | `	*pnMembers = 0;` |
|    127629 |  7245 | `	*pbParen = 0;` |
|    127629 |  7246 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7247 | `		bParen = 1;` |
|         9 |  7248 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7249 | `	}` |
|     63812 |  7250 | `	for(;;){` |
|    127655 |  7251 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7252 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7253 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7254 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7255 | `		}` |
|    127655 |  7256 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    127655 |  7257 | `		if( rc != SXRET_OK ){` |
|         3 |  7258 | `			return rc;` |
|         - |  7259 | `		}` |
|    127653 |  7260 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    127653 |  7261 | `		(*pnAtoms)++;` |
|    127653 |  7262 | `		nMembers++;` |
|         - |  7263 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    127653 |  7264 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7265 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7266 | `			if( pNext < pGen->pEnd` |
|        39 |  7267 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7268 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7269 | `				continue;` |
|         - |  7270 | `			}` |
|         4 |  7271 | `		}` |
|    127627 |  7272 | `		break;` |
|       ! 0 |  7273 | `	}` |
|    127627 |  7274 | `	if( bParen ){` |
|         9 |  7275 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7276 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7277 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7278 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7279 | `		}` |
|         9 |  7280 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7281 | `		if( nMembers < 2 ){` |
|       ! 0 |  7282 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7283 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7284 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7285 | `		}` |
|         3 |  7286 | `	}` |
|    127627 |  7287 | `	*pnMembers = nMembers;` |
|    127627 |  7288 | `	*pbParen = bParen;` |
|    127627 |  7289 | `	return SXRET_OK;` |
|     63817 |  7290 | `}` |
|         - |  7291 |  |
|         - |  7292 | `/*` |
|         - |  7293 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7294 | ` *` |
|         - |  7295 | ` * Outputs:` |
|         - |  7296 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7297 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7298 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7299 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7300 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7301 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7302 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7303 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7304 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7305 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7306 | ` *` |
|         - |  7307 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7308 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7309 | ` */` |
|    127488 |  7310 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7311 | `	ph7_gen_state *pGen,` |
|         - |  7312 | `	sxu32 *pnType,` |
|         - |  7313 | `	SyString *pClass,` |
|         - |  7314 | `	SySet *pAlts,` |
|         - |  7315 | `	sxi32 *piTypeFlags,` |
|         - |  7316 | `	SyString *pTypeText,` |
|         - |  7317 | `	int iNullableFlag,` |
|         - |  7318 | `	int iUnionFlag,` |
|         - |  7319 | `	int bAllowVoid,` |
|         - |  7320 | `	sxu32 nLine` |
|         5 |  7321 | `){` |
|         - |  7322 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    127493 |  7323 | `	int nAtoms = 0;` |
|    127493 |  7324 | `	int bShortNullable = 0;` |
|    127493 |  7325 | `	int bExplicitNull = 0;` |
|         - |  7326 | `	sxi32 rc;` |
|    127493 |  7327 | `	*pnType = 0;` |
|    127493 |  7328 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    127493 |  7329 | `	*piTypeFlags = 0;` |
|    127493 |  7330 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7331 |  |
|    127493 |  7332 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7333 | `		return SXRET_OK;` |
|         - |  7334 | `	}` |
|         - |  7335 | ``	/* Optional `?` shorthand prefix */`` |
|    127488 |  7336 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       101 |  7337 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       101 |  7338 | `		bShortNullable = 1;` |
|       101 |  7339 | `		pGen->pIn++;` |
|       101 |  7340 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7341 | `			return SXERR_SYNTAX;` |
|         - |  7342 | `		}` |
|        48 |  7343 | `	}` |
|         - |  7344 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7345 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7346 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7347 | `	{` |
|         - |  7348 | `		int nMembers, bParen;` |
|    127493 |  7349 | `		sxu32 iGroup = 0;` |
|    127493 |  7350 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    127493 |  7351 | `		if( rc != SXRET_OK ){` |
|         4 |  7352 | `			return rc;` |
|         - |  7353 | `		}` |
|         - |  7354 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7355 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7356 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7357 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7358 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    191438 |  7359 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    127700 |  7360 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7361 | `			if( bShortNullable ){` |
|         - |  7362 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7363 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7364 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7365 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7366 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7367 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7368 | `			}` |
|       141 |  7369 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7370 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7371 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7372 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7373 | `			}` |
|       141 |  7374 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7375 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7376 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7377 | `				return rc;` |
|         - |  7378 | `			}` |
|         5 |  7379 | `		}` |
|    127489 |  7380 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7381 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7382 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7383 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7384 | `		}` |
|         - |  7385 | `	}` |
|         - |  7386 | `	/* Validation pass.` |
|         - |  7387 | `	 *` |
|         - |  7388 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7389 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7390 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7391 | `	 */` |
|         - |  7392 | `	{` |
|         - |  7393 | `		int i, j;` |
|    127489 |  7394 | `		int bHasNonNull = 0;` |
|    127489 |  7395 | `		int bAnyIntersection = 0;` |
|         - |  7396 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7397 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7398 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4206977 |  7399 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    255135 |  7400 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127651 |  7401 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     63828 |  7402 | `		}` |
|    255079 |  7403 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127621 |  7404 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     63800 |  7405 | `		}` |
|         - |  7406 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7407 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    127489 |  7408 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7409 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7410 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7411 | `			return SXERR_SYNTAX;` |
|         - |  7412 | `		}` |
|    255121 |  7413 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7414 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7415 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7416 | ``			 * `true`/`false` in an intersection). */`` |
|    127649 |  7417 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7418 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7419 | `				if( bClassLike ){` |
|        53 |  7420 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7421 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7422 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7423 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7424 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7425 | `						bClassLike = 0;` |
|       ! 0 |  7426 | `					}` |
|        24 |  7427 | `				}` |
|        55 |  7428 | `				if( !bClassLike ){` |
|         - |  7429 | `					const char *zName; sxu32 nName;` |
|         3 |  7430 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7431 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7432 | `					}else{` |
|         3 |  7433 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7434 | `					}` |
|         4 |  7435 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7436 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7437 | `						(int)nName, zName);` |
|         3 |  7438 | `					return SXERR_SYNTAX;` |
|         - |  7439 | `				}` |
|        24 |  7440 | `			}` |
|    127647 |  7441 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7442 | `				if( nAtoms > 1 ){` |
|         3 |  7443 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7444 | `						"Void can only be used as a standalone type");` |
|         3 |  7445 | `					return SXERR_SYNTAX;` |
|         - |  7446 | `				}` |
|       175 |  7447 | `				if( !bAllowVoid ){` |
|       ! 0 |  7448 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7449 | `						"void cannot be used here");` |
|       ! 0 |  7450 | `					return SXERR_SYNTAX;` |
|         - |  7451 | `				}` |
|       175 |  7452 | `				if( bShortNullable ){` |
|       ! 0 |  7453 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7454 | `						"Void type cannot be nullable");` |
|       ! 0 |  7455 | `					return SXERR_SYNTAX;` |
|         - |  7456 | `				}` |
|        85 |  7457 | `			}` |
|    127645 |  7458 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7459 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7460 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7461 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7462 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        27 |  7463 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7464 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7465 | `					 * same as any other non-standalone use. */` |
|         6 |  7466 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7467 | `						"never can only be used as a standalone type");` |
|         6 |  7468 | `					return SXERR_SYNTAX;` |
|         - |  7469 | `				}` |
|        21 |  7470 | `				if( !bAllowVoid ){` |
|         - |  7471 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7472 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7473 | `						"never cannot be used as a parameter type");` |
|         3 |  7474 | `					return SXERR_SYNTAX;` |
|         - |  7475 | `				}` |
|         8 |  7476 | `			}` |
|    127639 |  7477 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7478 | `				bExplicitNull = 1;` |
|        19 |  7479 | `			}else{` |
|    127609 |  7480 | `				bHasNonNull = 1;` |
|         - |  7481 | `			}` |
|         - |  7482 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7483 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7484 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7485 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7486 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    127839 |  7487 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7488 | `				int bDup = 0;` |
|       207 |  7489 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7490 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7491 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7492 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7493 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7494 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7495 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7496 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7497 | `								aAtoms[j].sClass.zString,` |
|        34 |  7498 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7499 | `							bDup = 1;` |
|       ! 0 |  7500 | `						}` |
|        27 |  7501 | `					}else{` |
|         3 |  7502 | `						bDup = 1;` |
|         - |  7503 | `					}` |
|        23 |  7504 | `				}` |
|       195 |  7505 | `				if( bDup ){` |
|         - |  7506 | `					const char *zName;` |
|         - |  7507 | `					sxu32 nName;` |
|         3 |  7508 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7509 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7510 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7511 | `					}else{` |
|         3 |  7512 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7513 | `						nName = aAtoms[i].nCanon;` |
|         - |  7514 | `					}` |
|         4 |  7515 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7516 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7517 | `					return SXERR_SYNTAX;` |
|         - |  7518 | `				}` |
|        99 |  7519 | `			}` |
|     63821 |  7520 | `		}` |
|    127477 |  7521 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7522 | `			if( bShortNullable ){` |
|         - |  7523 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7524 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7525 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7526 | `				return SXERR_SYNTAX;` |
|         - |  7527 | `			}` |
|         - |  7528 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7529 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7530 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7531 | `			 * atom, so set it here. */` |
|         7 |  7532 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7533 | `		}` |
|         - |  7534 | `	}` |
|         - |  7535 | `	/* Compute nullability flag */` |
|    127477 |  7536 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       129 |  7537 | `		*piTypeFlags \|= iNullableFlag;` |
|        62 |  7538 | `	}` |
|         - |  7539 | `	/* Build canonical type text */` |
|    127477 |  7540 | `	if( pTypeText ){` |
|         - |  7541 | `		SyBlob sBlob;` |
|    127477 |  7542 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    191166 |  7543 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     63736 |  7544 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    127477 |  7545 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    190934 |  7546 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    127286 |  7547 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    127291 |  7548 | `			if( zDup ){` |
|    127291 |  7549 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     63643 |  7550 | `			}` |
|     63643 |  7551 | `		}` |
|    127477 |  7552 | `		SyBlobRelease(&sBlob);` |
|     63736 |  7553 | `	}` |
|         - |  7554 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7555 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7556 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7557 | `	{` |
|    127477 |  7558 | `		int nNonNull = 0;` |
|    127477 |  7559 | `		int iNonNullIdx = -1;` |
|         - |  7560 | `		int i;` |
|    255101 |  7561 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127629 |  7562 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127599 |  7563 | `				nNonNull++;` |
|    127599 |  7564 | `				iNonNullIdx = i;` |
|     63797 |  7565 | `			}` |
|     63817 |  7566 | `		}` |
|    127477 |  7567 | `		if( nNonNull <= 1 ){` |
|         - |  7568 | `			/* Fast path: store as single type. */` |
|    127371 |  7569 | `			if( iNonNullIdx >= 0 ){` |
|    127365 |  7570 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    127365 |  7571 | `				if( pA->nType == SXU32_HIGH ){` |
|     57611 |  7572 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19202 |  7573 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38409 |  7574 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38409 |  7575 | `					*pnType = SXU32_HIGH;` |
|     38409 |  7576 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    108163 |  7577 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7578 | `					*pnType = MEMOBJ_VOID;` |
|     88876 |  7579 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7580 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7581 | `				}else{` |
|     88775 |  7582 | `					*pnType = pA->nType;` |
|         - |  7583 | `				}` |
|     63680 |  7584 | `			}` |
|     63688 |  7585 | `		}else{` |
|         - |  7586 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7587 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7588 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7589 | `				ph7_type_alt sAlt;` |
|       249 |  7590 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7591 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7592 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7593 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7594 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7595 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7596 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7597 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7598 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7599 | `				}else{` |
|       145 |  7600 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7601 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7602 | `				}` |
|       239 |  7603 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7604 | `			}` |
|         - |  7605 | `		}` |
|         - |  7606 | `	}` |
|    127477 |  7607 | `	return SXRET_OK;` |
|     63749 |  7608 | `}` |
|         - |  7609 |  |
|         - |  7610 | `/*` |
|         - |  7611 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7612 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7613 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7614 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7615 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7616 | `` *          and union types `: T\|U`.`` |
|         - |  7617 | ` */` |
|   2873812 |  7618 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7619 | `{` |
|   2873817 |  7620 | `	sxi32 iFlags = 0;` |
|         - |  7621 | `	sxi32 rc;` |
|         - |  7622 | `	sxu32 nLine;` |
|   2873817 |  7623 | `	pFunc->nReturnType = 0;` |
|   2873817 |  7624 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2873817 |  7625 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7626 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7627 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7628 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7629 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7630 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2873817 |  7631 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2873817 |  7632 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2873817 |  7633 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2861683 |  7634 | `		return SXRET_OK;` |
|         - |  7635 | `	}` |
|     12139 |  7636 | `	pGen->pIn++; /* Skip ':' */` |
|     12139 |  7637 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7638 | `		return SXRET_OK;` |
|         - |  7639 | `	}` |
|     12139 |  7640 | `	nLine = pGen->pIn->nLine;` |
|     12139 |  7641 | `	rc = GenStateParseUnionTypeDecl(` |
|      6067 |  7642 | `		pGen,` |
|      6067 |  7643 | `		&pFunc->nReturnType,` |
|      6067 |  7644 | `		&pFunc->sReturnClass,` |
|      6067 |  7645 | `		&pFunc->aReturnUnion,` |
|         - |  7646 | `		&iFlags,` |
|      6067 |  7647 | `		&pFunc->sReturnTypeName,` |
|         - |  7648 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7649 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7650 | `		/* iUnionFlag */ 0,` |
|         - |  7651 | `		/* bAllowVoid */ 1,` |
|      6067 |  7652 | `		nLine);` |
|     12139 |  7653 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7654 | `		return SXERR_ABORT;` |
|         - |  7655 | `	}` |
|     12139 |  7656 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7657 | `		/* Error already reported */` |
|       ! 0 |  7658 | `		return SXERR_SYNTAX;` |
|         - |  7659 | `	}` |
|     12139 |  7660 | `	if( rc == SXERR_SYNTAX ){` |
|         9 |  7661 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 |  7662 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7663 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7664 | `				&pGen->pIn->sData);` |
|         6 |  7665 | `		}else{` |
|       ! 0 |  7666 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7667 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7668 | `		}` |
|         9 |  7669 | `		return SXERR_SYNTAX;` |
|         - |  7670 | `	}` |
|     12133 |  7671 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12133 |  7672 | `	return SXRET_OK;` |
|   1436911 |  7673 | `}` |
|         - |  7674 |  |
|    479092 |  7675 | `static sxi32 GenStateCompileFunc(` |
|         - |  7676 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7677 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7678 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7679 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7680 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7681 | `	)` |
|         5 |  7682 | `{` |
|         - |  7683 | `	ph7_vm_func *pFunc;` |
|         - |  7684 | `	SyToken *pEnd;` |
|         - |  7685 | `	sxu32 nLine;` |
|         - |  7686 | `	char *zName;` |
|         - |  7687 | `	sxi32 rc;` |
|         - |  7688 | `	/* Extract line number */` |
|    479097 |  7689 | `	nLine = pGen->pIn->nLine;` |
|         - |  7690 | `	/* Jump the left parenthesis '(' */` |
|    479097 |  7691 | `	pGen->pIn++;` |
|         - |  7692 | `	/* Delimit the function signature */` |
|    479097 |  7693 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    479097 |  7694 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7695 | `		/* Syntax error */` |
|         8 |  7696 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7697 | `		(void)pName;` |
|         8 |  7698 | `		if( rc == SXERR_ABORT ){` |
|         - |  7699 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7700 | `			return SXERR_ABORT;` |
|         - |  7701 | `		}` |
|         8 |  7702 | `		pGen->pIn = pGen->pEnd;` |
|         8 |  7703 | `		return SXRET_OK;` |
|         - |  7704 | `	}` |
|         - |  7705 | `	/* Create the function state */` |
|    479091 |  7706 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    479091 |  7707 | `	if( pFunc == 0 ){` |
|       ! 0 |  7708 | `		goto OutOfMem;` |
|         - |  7709 | `	}` |
|         - |  7710 | `	/* Build the function name, prepending namespace if active */` |
|    479098 |  7711 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7712 | `		SyBlob sFQN;` |
|         - |  7713 | `		sxu32 nLen;` |
|        16 |  7714 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7715 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7716 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7717 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7718 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7719 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7720 | `		SyBlobRelease(&sFQN);` |
|        16 |  7721 | `		if( zName == 0 ){` |
|       ! 0 |  7722 | `			goto OutOfMem;` |
|         - |  7723 | `		}` |
|        16 |  7724 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7725 | `	}else{` |
|    479077 |  7726 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    479077 |  7727 | `		if( zName == 0 ){` |
|       ! 0 |  7728 | `			goto OutOfMem;` |
|         - |  7729 | `		}` |
|    479077 |  7730 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7731 | `	}` |
|         - |  7732 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7733 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    479091 |  7734 | `	pFunc->nLine = nLine;` |
|    479091 |  7735 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    479091 |  7736 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7737 | `		return SXERR_ABORT;` |
|         - |  7738 | `	}` |
|    479091 |  7739 | `	if( pGen->pIn < pEnd ){` |
|         - |  7740 | `		/* Collect function arguments */` |
|    417093 |  7741 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    417093 |  7742 | `		if( rc == SXERR_ABORT ){` |
|         - |  7743 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7744 | `			return SXERR_ABORT;` |
|         - |  7745 | `		}` |
|    208544 |  7746 | `	}` |
|         - |  7747 | `	/* Point past ')' and parse optional return type ': type' */` |
|    479091 |  7748 | `	pGen->pIn = &pEnd[1];` |
|         - |  7749 | `	{` |
|    479091 |  7750 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    479091 |  7751 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7752 | `			return SXERR_ABORT;` |
|    479091 |  7753 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 |  7754 | `			return SXERR_SYNTAX;` |
|         - |  7755 | `		}` |
|         - |  7756 | `	}` |
|    479085 |  7757 | `	if( bHandleClosure ){` |
|         - |  7758 | `		ph7_vm_func_closure_env sEnv;` |
|       573 |  7759 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       568 |  7760 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       332 |  7761 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        91 |  7762 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7763 | `				/* Closure,record environment variable */` |
|        91 |  7764 | `				pGen->pIn++;` |
|        91 |  7765 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7766 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7767 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7768 | `						return SXERR_ABORT;` |
|         - |  7769 | `					}` |
|       ! 0 |  7770 | `				}` |
|        91 |  7771 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7772 | `				/* Compile until we hit the first closing parenthesis */` |
|       187 |  7773 | `				while( pGen->pIn < pGen->pEnd ){` |
|       187 |  7774 | `					int iFlagsLocal = 0;` |
|       187 |  7775 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        91 |  7776 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        91 |  7777 | `						break;` |
|         - |  7778 | `					}` |
|       101 |  7779 | `					nLineLocal = pGen->pIn->nLine;` |
|       101 |  7780 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7781 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7782 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7783 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7784 | `						pGen->pIn++;` |
|        27 |  7785 | `					}` |
|        96 |  7786 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       101 |  7787 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7788 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7789 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7790 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7791 | `								return SXERR_ABORT;` |
|         - |  7792 | `							}` |
|         - |  7793 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7794 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7795 | `								pGen->pIn++;` |
|       ! 0 |  7796 | `							}` |
|       ! 0 |  7797 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7798 | `								pGen->pIn++;` |
|       ! 0 |  7799 | `							}` |
|       ! 0 |  7800 | `							break;` |
|         - |  7801 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7802 | `					}else{` |
|         - |  7803 | `						SyString *pNameLocal;` |
|         - |  7804 | `						char *zDup;` |
|         - |  7805 | `						/* Duplicate variable name */` |
|       101 |  7806 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       101 |  7807 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       101 |  7808 | `						if( zDup ){` |
|         - |  7809 | `							/* Zero the structure */` |
|       101 |  7810 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       101 |  7811 | `							sEnv.iFlags = iFlagsLocal;` |
|       101 |  7812 | `							sEnv.nIdx = SXU32_HIGH;` |
|       101 |  7813 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       101 |  7814 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       116 |  7815 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7816 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7817 | `									got_this = 1;` |
|       ! 0 |  7818 | `							}` |
|         - |  7819 | `							/* Save imported variable */` |
|       101 |  7820 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        53 |  7821 | `						}else{` |
|       ! 0 |  7822 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7823 | `							 return SXERR_ABORT;` |
|         - |  7824 | `						}` |
|         - |  7825 | `					}` |
|       101 |  7826 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       113 |  7827 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7828 | `						/* Ignore trailing commas */` |
|        13 |  7829 | `						pGen->pIn++;` |
|         1 |  7830 | `					}` |
|         5 |  7831 | `				}` |
|         - |  7832 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7833 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7834 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7835 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7836 | `				 * legacy pre-use position. */` |
|        91 |  7837 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7838 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7839 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7840 | `						return SXERR_ABORT;` |
|         7 |  7841 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7842 | `						return SXERR_SYNTAX;` |
|         - |  7843 | `					}` |
|         3 |  7844 | `				}` |
|        43 |  7845 | `		}` |
|       573 |  7846 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7847 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7848 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7849 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7850 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7851 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7852 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7853 | `			 * closure never binds $this (php). */` |
|       563 |  7854 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       563 |  7855 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       563 |  7856 | `			sEnv.nIdx = SXU32_HIGH;` |
|       563 |  7857 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       563 |  7858 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       563 |  7859 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       279 |  7860 | `		}` |
|       573 |  7861 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7862 | `			/* Mark as closure */` |
|       565 |  7863 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       280 |  7864 | `		}` |
|       284 |  7865 | `	}` |
|         - |  7866 | `	/* Compile the body */` |
|    479085 |  7867 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    479085 |  7868 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7869 | `		return SXERR_ABORT;` |
|         - |  7870 | `	}` |
|         - |  7871 | `	/* The cursor sits just past the body's closing brace */` |
|    479085 |  7872 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    479085 |  7873 | `	if( ppFunc ){` |
|    479085 |  7874 | `		*ppFunc = pFunc;` |
|    239540 |  7875 | `	}` |
|    479085 |  7876 | `	rc = SXRET_OK;` |
|    479085 |  7877 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7878 | `		/* Finally register the function */` |
|    478525 |  7879 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    239260 |  7880 | `	}` |
|    479085 |  7881 | `	if( rc == SXRET_OK ){` |
|    479085 |  7882 | `		return SXRET_OK;` |
|         - |  7883 | `	}` |
|         - |  7884 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7885 | `OutOfMem:` |
|         - |  7886 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7887 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7888 | `	 */` |
|       ! 0 |  7889 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7890 | `	return SXERR_ABORT;` |
|    239551 |  7891 | `}` |
|         - |  7892 | `/*` |
|         - |  7893 | ` * Compile a standard PHP function.` |
|         - |  7894 | ` *  Refer to the block-comment above for more information.` |
|         - |  7895 | ` */` |
|    478532 |  7896 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7897 | `{` |
|         - |  7898 | `	SyString *pName;` |
|         - |  7899 | `	sxi32 iFlags;` |
|         - |  7900 | `	sxu32 nKwLine;` |
|         - |  7901 | `	sxu32 nLine;` |
|         - |  7902 | `	sxi32 rc;` |
|         - |  7903 |  |
|    478537 |  7904 | `	nLine = pGen->pIn->nLine;` |
|    478537 |  7905 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    478537 |  7906 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    478537 |  7907 | `	iFlags = 0;` |
|    478537 |  7908 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7909 | `		/* Return by reference,remember that */` |
|        12 |  7910 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7911 | `		/* Jump the '&' token */` |
|        12 |  7912 | `		pGen->pIn++;` |
|         5 |  7913 | `	}` |
|    478537 |  7914 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7915 | `		/* Invalid function name */` |
|         8 |  7916 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         8 |  7917 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7918 | `			return SXERR_ABORT;` |
|         - |  7919 | `		}` |
|         - |  7920 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  7921 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  7922 | `			pGen->pIn++;` |
|         2 |  7923 | `		}` |
|         8 |  7924 | `		return SXRET_OK;` |
|         - |  7925 | `	}` |
|    478531 |  7926 | `	pName = &pGen->pIn->sData;` |
|    478531 |  7927 | `	nLine = pGen->pIn->nLine;` |
|         - |  7928 | `	/* Jump the function name */` |
|    478531 |  7929 | `	pGen->pIn++;` |
|    478531 |  7930 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7931 | `		/* Syntax error */` |
|         3 |  7932 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  7933 | `		if( rc == SXERR_ABORT ){` |
|         - |  7934 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7935 | `			return SXERR_ABORT;` |
|         - |  7936 | `		}` |
|         - |  7937 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  7938 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  7939 | `			pGen->pIn++;` |
|       ! 0 |  7940 | `		}` |
|         3 |  7941 | `		return SXRET_OK;` |
|         - |  7942 | `	}` |
|         - |  7943 | `	/* Compile function body */` |
|         - |  7944 | `	{` |
|    478529 |  7945 | `		ph7_vm_func *pFuncState = 0;` |
|    478529 |  7946 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    478529 |  7947 | `		if( pFuncState ){` |
|         - |  7948 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    478517 |  7949 | `			pFuncState->nLine = nKwLine;` |
|    239256 |  7950 | `		}` |
|         - |  7951 | `	}` |
|    478529 |  7952 | `	return rc;` |
|    239271 |  7953 | `}` |
|         - |  7954 | `/*` |
|         - |  7955 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  7956 | ` * According to the PHP language reference manual` |
|         - |  7957 | ` *  Visibility:` |
|         - |  7958 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  7959 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  7960 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  7961 | ` *  Members declared protected can be accessed only within the class` |
|         - |  7962 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  7963 | ` *  may only be accessed by the class that defines the member.` |
|         - |  7964 | ` */` |
|   3143362 |  7965 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  7966 | `{` |
|   3143367 |  7967 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    255869 |  7968 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2887503 |  7969 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    190887 |  7970 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  7971 | `	}` |
|         - |  7972 | `	/* Assume public by default */` |
|   2696621 |  7973 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1571686 |  7974 | `}` |
|         - |  7975 | `/*` |
|         - |  7976 | ` * Compile a class constant.` |
|         - |  7977 | ` * According to the PHP language reference manual` |
|         - |  7978 | ` *  Class Constants` |
|         - |  7979 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  7980 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  7981 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  7982 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  7983 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  7984 | ` *   It's also possible for interfaces to have constants.` |
|         - |  7985 | ` * Symisc eXtension.` |
|         - |  7986 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  7987 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  7988 | ` *  Example:` |
|         - |  7989 | ` *   class Test{` |
|         - |  7990 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  7991 | ` *   };` |
|         - |  7992 | ` *   var_dump(TEST::MyConst);` |
|         - |  7993 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  7994 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  7995 | ` */` |
|         - |  7996 | `/*` |
|         - |  7997 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  7998 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  7999 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  8000 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  8001 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  8002 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  8003 | ` */` |
|    290194 |  8004 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  8005 | `{` |
|         - |  8006 | `	SyToken *p0, *p1;` |
|    290199 |  8007 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8008 | `		return 0;` |
|         - |  8009 | `	}` |
|    290199 |  8010 | `	p0 = pGen->pIn;` |
|         - |  8011 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    290199 |  8012 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  8013 | `		return 1;` |
|         - |  8014 | `	}` |
|    290199 |  8015 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  8016 | `		return 1;` |
|         - |  8017 | `	}` |
|         - |  8018 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  8019 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  8020 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    290195 |  8021 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    290195 |  8022 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    290195 |  8023 | `		if( p1 ){` |
|    290195 |  8024 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  8025 | `				return 1;` |
|         - |  8026 | `			}` |
|    290165 |  8027 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  8028 | `				return 1;` |
|         - |  8029 | `			}` |
|    145078 |  8030 | `		}` |
|    145078 |  8031 | `	}` |
|    290161 |  8032 | `	return 0;` |
|    145102 |  8033 | `}` |
|         - |  8034 | `/*` |
|         - |  8035 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  8036 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  8037 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  8038 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  8039 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  8040 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  8041 | ` * Peek only; never consumes tokens.` |
|         - |  8042 | ` */` |
|        24 |  8043 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  8044 | `{` |
|        28 |  8045 | `	SyToken *p = pGen->pIn;` |
|        39 |  8046 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  8047 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  8048 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  8049 | `	}` |
|        28 |  8050 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  8051 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  8052 | `	}` |
|         6 |  8053 | `	p++;` |
|         - |  8054 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  8055 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  8056 | `}` |
|         - |  8057 | `/*` |
|         - |  8058 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  8059 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  8060 | `` * `$o->new`), not a `new` expression.`` |
|         - |  8061 | ` */` |
|       110 |  8062 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  8063 | `{` |
|         - |  8064 | `	sxi32 iOp;` |
|       114 |  8065 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  8066 | `		return 0;` |
|         - |  8067 | `	}` |
|       104 |  8068 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  8069 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  8070 | `}` |
|         - |  8071 | `/*` |
|         - |  8072 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  8073 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  8074 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  8075 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  8076 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  8077 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  8078 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  8079 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  8080 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  8081 | ` *` |
|         - |  8082 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  8083 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  8084 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  8085 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  8086 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  8087 | ` */` |
|    626696 |  8088 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  8089 | `{` |
|    626701 |  8090 | `	SyToken *p = pGen->pIn;` |
|    626701 |  8091 | `	int iDepth = 0;` |
|   1655735 |  8092 | `	while( p < pGen->pEnd ){` |
|   1655735 |  8093 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    626649 |  8094 | `			break; /* end of this initializer */` |
|         - |  8095 | `		}` |
|   1029086 |  8096 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    518380 |  8097 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7664 |  8098 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  8099 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  8100 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  8101 | `			 * expression. */` |
|         3 |  8102 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  8103 | `			p++;` |
|         3 |  8104 | `			if( bArrow ){` |
|         - |  8105 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  8106 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  8107 | `				int iBase = iDepth;` |
|        17 |  8108 | `				while( p < pGen->pEnd ){` |
|        17 |  8109 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  8110 | `						iDepth++;` |
|        15 |  8111 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  8112 | `						if( iDepth <= iBase ){` |
|       ! 0 |  8113 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  8114 | `						}` |
|         5 |  8115 | `						iDepth--;` |
|        11 |  8116 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  8117 | `						break;` |
|         - |  8118 | `					}` |
|        15 |  8119 | `					p++;` |
|         1 |  8120 | `				}` |
|         2 |  8121 | `			}else{` |
|         - |  8122 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  8123 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  8124 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  8125 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  8126 | `				int iLocal = 0;` |
|       ! 0 |  8127 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  8128 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  8129 | `						break; /* body brace */` |
|         - |  8130 | `					}` |
|       ! 0 |  8131 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  8132 | `						iLocal++;` |
|       ! 0 |  8133 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  8134 | `						if( iLocal > 0 ){` |
|       ! 0 |  8135 | `							iLocal--;` |
|       ! 0 |  8136 | `						}` |
|       ! 0 |  8137 | `					}` |
|       ! 0 |  8138 | `					p++;` |
|       ! 0 |  8139 | `				}` |
|       ! 0 |  8140 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  8141 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  8142 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  8143 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  8144 | `							iBrace++;` |
|       ! 0 |  8145 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  8146 | `							iBrace--;` |
|       ! 0 |  8147 | `							if( iBrace == 0 ){` |
|       ! 0 |  8148 | `								p++;` |
|       ! 0 |  8149 | `								break;` |
|         - |  8150 | `							}` |
|       ! 0 |  8151 | `						}` |
|       ! 0 |  8152 | `						p++;` |
|       ! 0 |  8153 | `					}` |
|       ! 0 |  8154 | `				}` |
|         - |  8155 | `			}` |
|         3 |  8156 | `			continue;` |
|         - |  8157 | `		}` |
|   1029089 |  8158 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  8159 | `			if( iDepth == 0 ){` |
|         - |  8160 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  8161 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  8162 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  8163 | `				 * is legal — don't scan into it. */` |
|        45 |  8164 | `				break;` |
|         - |  8165 | `			}` |
|       ! 0 |  8166 | `			iDepth++;` |
|   1029045 |  8167 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42071 |  8168 | `			iDepth++;` |
|   1008012 |  8169 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     42069 |  8170 | `			if( iDepth > 0 ){` |
|     42069 |  8171 | `				iDepth--;` |
|     21032 |  8172 | `			}` |
|    965947 |  8173 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    346419 |  8174 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8175 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8176 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8177 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  8178 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  8179 | `				return 1;` |
|         - |  8180 | `			}` |
|       ! 0 |  8181 | `		}` |
|   1029037 |  8182 | `		p++;` |
|         5 |  8183 | `	}` |
|    626693 |  8184 | `	return 0;` |
|    313353 |  8185 | `}` |
|         - |  8186 | `/*` |
|         - |  8187 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8188 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8189 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8190 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8191 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8192 | ` * share the same backing.` |
|         - |  8193 | ` */` |
|       350 |  8194 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8195 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8196 | `{` |
|       355 |  8197 | `	pAttr->nType = nType;` |
|       355 |  8198 | `	pAttr->sClass = *pClass;` |
|       355 |  8199 | `	pAttr->sTypeName = *pTypeName;` |
|       355 |  8200 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8201 | `		sxu32 i;` |
|        73 |  8202 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8203 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8204 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8205 | `		}` |
|        11 |  8206 | `	}` |
|       355 |  8207 | `}` |
|    290194 |  8208 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8209 | `{` |
|    290199 |  8210 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8211 | `	SySet *pInstrContainer;` |
|         - |  8212 | `	ph7_class_attr *pCons;` |
|         - |  8213 | `	SyString *pName;` |
|         - |  8214 | `	sxi32 rc;` |
|    290199 |  8215 | `	sxu32 nType = 0;` |
|         - |  8216 | `	SyString sTypeClass;` |
|         - |  8217 | `	SyString sTypeText;` |
|         - |  8218 | `	SySet aUnionAlts;` |
|    290199 |  8219 | `	sxi32 iTypeFlags = 0;` |
|    290199 |  8220 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    290199 |  8221 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    290199 |  8222 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8223 | `	/* Extract visibility level */` |
|    290199 |  8224 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8225 | `	/* Mark as constant */` |
|    290199 |  8226 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    290199 |  8227 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8228 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8229 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    290218 |  8230 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8231 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8232 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8233 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8234 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8235 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8236 | `		 * and success paths release. */` |
|        42 |  8237 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8238 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8239 | `			goto Synchronize;` |
|        42 |  8240 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8241 | `			return SXERR_ABORT;` |
|        42 |  8242 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8243 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8244 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8245 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8246 | `				return SXERR_ABORT;` |
|         - |  8247 | `			}` |
|       ! 0 |  8248 | `			goto Synchronize;` |
|         - |  8249 | `		}` |
|        42 |  8250 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8251 | `	}` |
|    145097 |  8252 | `loop:` |
|    290201 |  8253 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8254 | `		/* Invalid constant name */` |
|       ! 0 |  8255 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8256 | `		if( rc == SXERR_ABORT ){` |
|         - |  8257 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8258 | `			return SXERR_ABORT;` |
|         - |  8259 | `		}` |
|       ! 0 |  8260 | `		goto Synchronize;` |
|         - |  8261 | `	}` |
|         - |  8262 | `	/* Peek constant name */` |
|    290201 |  8263 | `	pName = &pGen->pIn->sData;` |
|         - |  8264 | `	/* Make sure the constant name isn't reserved */` |
|    290201 |  8265 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8266 | `		/* Reserved constant name */` |
|       ! 0 |  8267 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8268 | `		if( rc == SXERR_ABORT ){` |
|         - |  8269 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8270 | `			return SXERR_ABORT;` |
|         - |  8271 | `		}` |
|       ! 0 |  8272 | `		goto Synchronize;` |
|         - |  8273 | `	}` |
|         - |  8274 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    290201 |  8275 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8276 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8277 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8278 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8279 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8280 | `			return SXERR_ABORT;` |
|        42 |  8281 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8282 | `			goto Synchronize;` |
|         - |  8283 | `		}` |
|        18 |  8284 | `	}` |
|         - |  8285 | `	/* Advance the stream cursor */` |
|    290199 |  8286 | `	pGen->pIn++;` |
|    290199 |  8287 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8288 | `		/* Invalid declaration */` |
|       ! 0 |  8289 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8290 | `		if( rc == SXERR_ABORT ){` |
|         - |  8291 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8292 | `			return SXERR_ABORT;` |
|         - |  8293 | `		}` |
|       ! 0 |  8294 | `		goto Synchronize;` |
|         - |  8295 | `	}` |
|    290199 |  8296 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8297 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8298 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8299 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8300 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    290194 |  8301 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8302 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8303 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8304 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8305 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8306 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8307 | `			return SXERR_ABORT;` |
|         - |  8308 | `		}` |
|         6 |  8309 | `		goto Synchronize;` |
|         - |  8310 | `	}` |
|         - |  8311 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8312 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8313 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    290195 |  8314 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8315 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8316 | `			"New expressions are not supported in this context");` |
|         5 |  8317 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8318 | `			return SXERR_ABORT;` |
|         - |  8319 | `		}` |
|         5 |  8320 | `		goto Synchronize;` |
|         - |  8321 | `	}` |
|         - |  8322 | `	/* Allocate a new class attribute */` |
|    290191 |  8323 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    290191 |  8324 | `	if( pCons ){` |
|    290191 |  8325 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    290191 |  8326 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8327 | `			return SXERR_ABORT;` |
|         - |  8328 | `		}` |
|    145093 |  8329 | `	}` |
|    290191 |  8330 | `	if( pCons == 0 ){` |
|       ! 0 |  8331 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8332 | `		return SXERR_ABORT;` |
|         - |  8333 | `	}` |
|    290191 |  8334 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8335 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8336 | `	}` |
|         - |  8337 | `	/* Swap bytecode container */` |
|    290191 |  8338 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    290191 |  8339 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8340 | `	/* Compile constant value.` |
|         - |  8341 | `	 */` |
|    290191 |  8342 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    290191 |  8343 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8344 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8345 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8346 | `			return SXERR_ABORT;` |
|         - |  8347 | `		}` |
|         1 |  8348 | `	}` |
|         - |  8349 | `	/* Emit the done instruction */` |
|    290191 |  8350 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    290191 |  8351 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    290191 |  8352 | `	if( rc == SXERR_ABORT ){` |
|         - |  8353 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8354 | `		return SXERR_ABORT;` |
|         - |  8355 | `	}` |
|         - |  8356 | `	/* All done,install the constant */` |
|    290191 |  8357 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    290191 |  8358 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8359 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8360 | `		return SXERR_ABORT;` |
|         - |  8361 | `	}` |
|    290191 |  8362 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8363 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8364 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8365 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8366 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8367 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8368 | `				pTok--;` |
|       ! 0 |  8369 | `			}` |
|       ! 0 |  8370 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8371 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8372 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8373 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8374 | `				return SXERR_ABORT;` |
|         - |  8375 | `			}` |
|       ! 0 |  8376 | `		}else{` |
|         3 |  8377 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8378 | `				goto loop;` |
|         - |  8379 | `			}` |
|         - |  8380 | `		}` |
|       ! 0 |  8381 | `	}` |
|    290189 |  8382 | `	SySetRelease(&aUnionAlts);` |
|    290189 |  8383 | `	return SXRET_OK;` |
|         5 |  8384 | `Synchronize:` |
|        13 |  8385 | `	SySetRelease(&aUnionAlts);` |
|         - |  8386 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8387 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8388 | `		pGen->pIn++;` |
|         3 |  8389 | `	}` |
|        13 |  8390 | `	return SXERR_CORRUPT;` |
|    145102 |  8391 | `}` |
|         - |  8392 | `/*` |
|         - |  8393 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8394 | ` * According to the PHP language reference manual` |
|         - |  8395 | ` *  Properties` |
|         - |  8396 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8397 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8398 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8399 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8400 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8401 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8402 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8403 | ` * Symisc eXtension.` |
|         - |  8404 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8405 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8406 | ` *  Example:` |
|         - |  8407 | ` *   class Test{` |
|         - |  8408 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8409 | ` *   };` |
|         - |  8410 | ` *   var_dump(TEST::myVar);` |
|         - |  8411 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8412 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8413 | ` */` |
|         - |  8414 | `/*` |
|         - |  8415 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8416 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8417 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8418 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8419 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8420 | ` */` |
|   2348502 |  8421 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8422 | `{` |
|   2348507 |  8423 | `	SyToken *p = pStart;` |
|   2348507 |  8424 | `	int bFirst = 1;` |
|   2348507 |  8425 | `	if( p >= pEnd ) return 0;` |
|         - |  8426 | ``	/* Optional nullable `?` shorthand. */`` |
|   2348507 |  8427 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        35 |  8428 | `		p++;` |
|        35 |  8429 | `		if( p >= pEnd ) return 0;` |
|        16 |  8430 | `	}` |
|         - |  8431 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8432 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8433 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8434 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1174251 |  8435 | `	for(;;){` |
|   2348527 |  8436 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8437 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8438 | `			p++;` |
|         9 |  8439 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8440 | `			if( p >= pEnd ) return 0;` |
|         3 |  8441 | `			p++; /* skip ')' */` |
|         2 |  8442 | `		}else{` |
|         - |  8443 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8444 | ``			 * then any `&`-joined intersection members. */`` |
|   2348525 |  8445 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2348525 |  8446 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8447 | `				return 0;` |
|         - |  8448 | `			}` |
|         - |  8449 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8450 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8451 | `			 * may still appear at the initial dispatch site). */` |
|   2348525 |  8452 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2348477 |  8453 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2348472 |  8454 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    103448 |  8455 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2348195 |  8456 | `					return 0;` |
|         - |  8457 | `				}` |
|       141 |  8458 | `			}` |
|       335 |  8459 | `			p++;` |
|       337 |  8460 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8461 | `				p += 2;` |
|         1 |  8462 | `			}` |
|       498 |  8463 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       338 |  8464 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8465 | `				p++; /* skip '&' */` |
|         3 |  8466 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8467 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8468 | `				p++;` |
|         3 |  8469 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8470 | `					p += 2;` |
|       ! 0 |  8471 | `				}` |
|         1 |  8472 | `			}` |
|         - |  8473 | `		}` |
|       337 |  8474 | `		bFirst = 0;` |
|       332 |  8475 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8476 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8477 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8478 | `			continue;` |
|         - |  8479 | `		}` |
|       317 |  8480 | `		break;` |
|       ! 0 |  8481 | `	}` |
|       317 |  8482 | `	if( p >= pEnd ) return 0;` |
|       317 |  8483 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1174256 |  8484 | `}` |
|         - |  8485 |  |
|         - |  8486 | `/*` |
|         - |  8487 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8488 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8489 | ` * if not). Recognized forms:` |
|         - |  8490 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8491 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8492 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8493 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8494 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8495 | ` * on unrecoverable error.` |
|         - |  8496 | ` *` |
|         - |  8497 | ` * When a type is parsed:` |
|         - |  8498 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8499 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8500 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8501 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8502 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8503 | ` */` |
|       322 |  8504 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8505 | `	ph7_gen_state *pGen,` |
|         - |  8506 | `	sxu32 *pnType,` |
|         - |  8507 | `	SyString *pClass,` |
|         - |  8508 | `	sxi32 *piTypeFlags,` |
|         - |  8509 | `	SyString *pTypeText,` |
|         - |  8510 | `	SySet *pAlts` |
|         5 |  8511 | `){` |
|       327 |  8512 | `	sxi32 iFlags = 0;` |
|         - |  8513 | `	sxi32 rc;` |
|       327 |  8514 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8515 | `		return SXRET_OK;` |
|         - |  8516 | `	}` |
|         - |  8517 | `	/* If the first token is '$', there's no type */` |
|       327 |  8518 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8519 | `		return SXRET_OK;` |
|         - |  8520 | `	}` |
|       327 |  8521 | `	rc = GenStateParseUnionTypeDecl(` |
|       161 |  8522 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8523 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8524 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8525 | `		/* bAllowVoid */ 0,` |
|       322 |  8526 | `		pGen->pIn->nLine);` |
|       327 |  8527 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8528 | `		return rc;` |
|         - |  8529 | `	}` |
|         - |  8530 | `	/* Verify next token is '$' (start of property name) */` |
|       327 |  8531 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8532 | `		return SXERR_SYNTAX;` |
|         - |  8533 | `	}` |
|       327 |  8534 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       327 |  8535 | `	return SXRET_OK;` |
|       166 |  8536 | `}` |
|         - |  8537 |  |
|         - |  8538 | `/*` |
|         - |  8539 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8540 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8541 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8542 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8543 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8544 | ` * by the type parser itself before reaching here.` |
|         - |  8545 | ` *` |
|         - |  8546 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8547 | ` * use in the error message.` |
|         - |  8548 | ` */` |
|       498 |  8549 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8550 | `	sxu32 nType,` |
|         - |  8551 | `	const SyString *pClass,` |
|         - |  8552 | `	const char **pzName,` |
|         - |  8553 | `	sxu32 *pnName)` |
|         5 |  8554 | `{` |
|         - |  8555 | `	const char *z;` |
|         - |  8556 | `	sxu32 n;` |
|       503 |  8557 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       449 |  8558 | `		return 0;` |
|         - |  8559 | `	}` |
|        58 |  8560 | `	z = pClass->zString;` |
|        58 |  8561 | `	n = pClass->nByte;` |
|        58 |  8562 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8563 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8564 | `	}` |
|         - |  8565 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8566 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8567 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        52 |  8568 | `	return 0;` |
|       254 |  8569 | `}` |
|         - |  8570 |  |
|         - |  8571 | `/*` |
|         - |  8572 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8573 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8574 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8575 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8576 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8577 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8578 | ` *` |
|         - |  8579 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8580 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8581 | ` */` |
|       436 |  8582 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8583 | `	ph7_gen_state *pGen,` |
|         - |  8584 | `	ph7_class *pClass,` |
|         - |  8585 | `	const SyString *pMemberName,` |
|         - |  8586 | `	sxu32 nType,` |
|         - |  8587 | `	const SyString *pTypeClass,` |
|         - |  8588 | `	const SyString *pTypeText,` |
|         - |  8589 | `	SySet *pUnionAlts,` |
|         - |  8590 | `	const char *zErrFmt,` |
|         - |  8591 | `	sxu32 nLine)` |
|         5 |  8592 | `{` |
|       441 |  8593 | `	const char *zBad = 0;` |
|       441 |  8594 | `	sxu32 nBad = 0;` |
|         - |  8595 | `	SyString sFallback;` |
|         - |  8596 | `	const SyString *pBad;` |
|         - |  8597 | `	sxi32 rc;` |
|       441 |  8598 | `	int bDisallowed = 0;` |
|       441 |  8599 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8600 | `		bDisallowed = 1;` |
|       439 |  8601 | `	}else if( pUnionAlts ){` |
|         - |  8602 | `		sxu32 i;` |
|        95 |  8603 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8604 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8605 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8606 | `				bDisallowed = 1;` |
|         3 |  8607 | `				break;` |
|         - |  8608 | `			}` |
|        35 |  8609 | `		}` |
|        15 |  8610 | `	}` |
|       441 |  8611 | `	if( !bDisallowed ){` |
|       435 |  8612 | `		return SXRET_OK;` |
|         - |  8613 | `	}` |
|         - |  8614 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8615 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8616 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8617 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8618 | `		pBad = pTypeText;` |
|         5 |  8619 | `	}else{` |
|       ! 0 |  8620 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8621 | `		pBad = &sFallback;` |
|         - |  8622 | `	}` |
|        11 |  8623 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8624 | `		zErrFmt,` |
|         3 |  8625 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8626 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8627 | `		return SXERR_ABORT;` |
|         - |  8628 | `	}` |
|         8 |  8629 | `	return SXERR_SYNTAX;` |
|       223 |  8630 | `}` |
|         - |  8631 | `/*` |
|         - |  8632 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8633 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8634 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8635 | ` * than promoted to a lexer keyword.` |
|         - |  8636 | ` */` |
|  20219100 |  8637 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8638 | `{` |
|  20437351 |  8639 | `	return (pTok->nType & PH7_TK_ID)` |
|  10327796 |  8640 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  20437346 |  8641 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8642 | `}` |
|         - |  8643 | `/*` |
|         - |  8644 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8645 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8646 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8647 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8648 | ` */` |
|   7269176 |  8649 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8650 | `{` |
|   7269181 |  8651 | `	*pnTok = 0;` |
|   7269176 |  8652 | `	if( &pTok[3] < pEnd` |
|   6816097 |  8653 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5612306 |  8654 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2430805 |  8655 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8656 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8657 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8658 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8659 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8660 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8661 | `			*pnTok = 4;` |
|        17 |  8662 | `			return nKw;` |
|         - |  8663 | `		}` |
|       ! 0 |  8664 | `	}` |
|   7269165 |  8665 | `	return 0;` |
|   3634593 |  8666 | `}` |
|         - |  8667 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8668 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8669 | `{` |
|        17 |  8670 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8671 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8672 | `	}` |
|         5 |  8673 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8674 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8675 | `	}` |
|         3 |  8676 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8677 | `}` |
|    458912 |  8678 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8679 | `{` |
|    458917 |  8680 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8681 | `	ph7_class_attr *pAttr;` |
|         - |  8682 | `	SyString *pName;` |
|         - |  8683 | `	sxi32 rc;` |
|    458917 |  8684 | `	sxu32 nType = 0;` |
|         - |  8685 | `	SyString sTypeClass;` |
|         - |  8686 | `	SyString sTypeText;` |
|         - |  8687 | `	SySet aUnionAlts;` |
|    458917 |  8688 | `	sxi32 iTypeFlags = 0;` |
|    458917 |  8689 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    458917 |  8690 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    458917 |  8691 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8692 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8693 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8694 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    458917 |  8695 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8696 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8697 | `	}` |
|         - |  8698 | `	/* Extract visibility level */` |
|    458917 |  8699 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8700 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    459078 |  8701 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       327 |  8702 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       327 |  8703 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8704 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8705 | `			goto Synchronize;` |
|       327 |  8706 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8707 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8708 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8709 | `				&pGen->pIn->sData);` |
|       ! 0 |  8710 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8711 | `				return SXERR_ABORT;` |
|         - |  8712 | `			}` |
|       ! 0 |  8713 | `			goto Synchronize;` |
|       327 |  8714 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8715 | `			return SXERR_ABORT;` |
|         - |  8716 | `		}` |
|       161 |  8717 | `	}` |
|       ! 0 |  8718 | `loop:` |
|    458921 |  8719 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8720 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8721 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8722 | `			return SXERR_ABORT;` |
|         - |  8723 | `		}` |
|       ! 0 |  8724 | `		goto Synchronize;` |
|         - |  8725 | `	}` |
|    458921 |  8726 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    458921 |  8727 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8728 | `		/* Invalid attribute name */` |
|       ! 0 |  8729 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8730 | `		if( rc == SXERR_ABORT ){` |
|         - |  8731 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8732 | `			return SXERR_ABORT;` |
|         - |  8733 | `		}` |
|       ! 0 |  8734 | `		goto Synchronize;` |
|         - |  8735 | `	}` |
|         - |  8736 | `	/* Peek attribute name */` |
|    458921 |  8737 | `	pName = &pGen->pIn->sData;` |
|         - |  8738 | `	/* Advance the stream cursor */` |
|    458921 |  8739 | `	pGen->pIn++;` |
|    458921 |  8740 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8741 | `		/* Invalid declaration */` |
|         3 |  8742 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8743 | `		if( rc == SXERR_ABORT ){` |
|         - |  8744 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8745 | `			return SXERR_ABORT;` |
|         - |  8746 | `		}` |
|         3 |  8747 | `		goto Synchronize;` |
|         - |  8748 | `	}` |
|         - |  8749 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8750 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    458919 |  8751 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8752 | `		const char *zAvErr = 0;` |
|        19 |  8753 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8754 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8755 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8756 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8757 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8758 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8759 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8760 | `		}` |
|        13 |  8761 | `		if( zAvErr ){` |
|       ! 0 |  8762 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8763 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8764 | `				return SXERR_ABORT;` |
|         - |  8765 | `			}` |
|       ! 0 |  8766 | `			goto Synchronize;` |
|         - |  8767 | `		}` |
|         6 |  8768 | `	}` |
|         - |  8769 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8770 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    458919 |  8771 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8772 | `		const char *zRoErr = 0;` |
|        43 |  8773 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8774 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8775 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8776 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8777 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8778 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8779 | `		}` |
|        43 |  8780 | `		if( zRoErr ){` |
|        13 |  8781 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8782 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8783 | `				return SXERR_ABORT;` |
|         - |  8784 | `			}` |
|        13 |  8785 | `			goto Synchronize;` |
|         - |  8786 | `		}` |
|        14 |  8787 | `	}` |
|         - |  8788 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8789 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8790 | `	 * by the type parser. */` |
|    458909 |  8791 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       485 |  8792 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8793 | `			&sTypeText,` |
|       320 |  8794 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       160 |  8795 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       325 |  8796 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8797 | `			return SXERR_ABORT;` |
|       325 |  8798 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8799 | `			goto Synchronize;` |
|         - |  8800 | `		}` |
|       160 |  8801 | `	}` |
|         - |  8802 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    458909 |  8803 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8804 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8805 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8806 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8807 | `			return SXERR_ABORT;` |
|         - |  8808 | `		}` |
|         3 |  8809 | `		goto Synchronize;` |
|         - |  8810 | `	}` |
|         - |  8811 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8812 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8813 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8814 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8815 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8816 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    458907 |  8817 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8818 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8819 | `			"New expressions are not supported in this context");` |
|         6 |  8820 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8821 | `			return SXERR_ABORT;` |
|         - |  8822 | `		}` |
|         6 |  8823 | `		goto Synchronize;` |
|         - |  8824 | `	}` |
|         - |  8825 | `	/* Allocate a new class attribute */` |
|    458903 |  8826 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    458903 |  8827 | `	if( pAttr ){` |
|    458903 |  8828 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    458903 |  8829 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8830 | `			return SXERR_ABORT;` |
|         - |  8831 | `		}` |
|    229449 |  8832 | `	}` |
|    458903 |  8833 | `	if( pAttr == 0 ){` |
|       ! 0 |  8834 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8835 | `		return SXERR_ABORT;` |
|         - |  8836 | `	}` |
|    458903 |  8837 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       323 |  8838 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       159 |  8839 | `	}` |
|    458903 |  8840 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8841 | `		SySet *pInstrContainer;` |
|    336507 |  8842 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    336507 |  8843 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8844 | `		{` |
|         - |  8845 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8846 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8847 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8848 | `			 * compiler would otherwise run into the hook tokens. */` |
|    336507 |  8849 | `			SyToken *pScan = pGen->pIn;` |
|    336507 |  8850 | `			sxi32 iNest = 0;` |
|    734779 |  8851 | `			while( pScan < pGen->pEnd ){` |
|    734779 |  8852 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42067 |  8853 | `					iNest++;` |
|    713748 |  8854 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     42067 |  8855 | `					iNest--;` |
|    671686 |  8856 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    336507 |  8857 | `					break;` |
|         - |  8858 | `				}` |
|    398277 |  8859 | `				pScan++;` |
|         5 |  8860 | `			}` |
|    336507 |  8861 | `			pGen->pEnd = pScan;` |
|         - |  8862 | `		}` |
|         - |  8863 | `		/* Swap bytecode container */` |
|    336507 |  8864 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    336507 |  8865 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8866 | `		/* Compile attribute value.` |
|         - |  8867 | `		 */` |
|    336507 |  8868 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    336507 |  8869 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8870 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8871 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8872 | `				return SXERR_ABORT;` |
|         - |  8873 | `			}` |
|       ! 0 |  8874 | `		}` |
|         - |  8875 | `		/* Emit the done instruction */` |
|    336507 |  8876 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    336507 |  8877 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    336507 |  8878 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    336507 |  8879 | `		pGen->pEnd = pSavedDefEnd;` |
|    168251 |  8880 | `	}` |
|         - |  8881 | `	/* All done,install the attribute */` |
|    458903 |  8882 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    458903 |  8883 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8884 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8885 | `		return SXERR_ABORT;` |
|         - |  8886 | `	}` |
|    458903 |  8887 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8888 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8889 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8890 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8891 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8892 | `			return SXERR_ABORT;` |
|         - |  8893 | `		}` |
|        95 |  8894 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8895 | `			goto Synchronize;` |
|         - |  8896 | `		}` |
|        95 |  8897 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8898 | `		return SXRET_OK;` |
|         - |  8899 | `	}` |
|    458809 |  8900 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8901 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8902 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8903 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8904 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8905 | `				? "Interfaces may only include hooked properties"` |
|         - |  8906 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8907 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8908 | `			return SXERR_ABORT;` |
|         - |  8909 | `		}` |
|       ! 0 |  8910 | `		goto Synchronize;` |
|         - |  8911 | `	}` |
|    458809 |  8912 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8913 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8914 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8915 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8916 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8917 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8918 | `				pTok--;` |
|       ! 0 |  8919 | `			}` |
|       ! 0 |  8920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8921 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8922 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8923 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8924 | `				return SXERR_ABORT;` |
|         - |  8925 | `			}` |
|       ! 0 |  8926 | `		}else{` |
|         5 |  8927 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8928 | `				goto loop;` |
|         - |  8929 | `			}` |
|         - |  8930 | `		}` |
|       ! 0 |  8931 | `	}` |
|    458805 |  8932 | `	SySetRelease(&aUnionAlts);` |
|    458805 |  8933 | `	return SXRET_OK;` |
|         9 |  8934 | `Synchronize:` |
|         - |  8935 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8936 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  8937 | `		pGen->pIn++;` |
|         3 |  8938 | `	}` |
|        22 |  8939 | `	SySetRelease(&aUnionAlts);` |
|        22 |  8940 | `	return SXERR_CORRUPT;` |
|    229461 |  8941 | `}` |
|         - |  8942 | `/*` |
|         - |  8943 | ` * Compile a class method.` |
|         - |  8944 | ` *` |
|         - |  8945 | ` * Refer to the official documentation for more information` |
|         - |  8946 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  8947 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  8948 | ` * overloading and many more.` |
|         - |  8949 | ` */` |
|   2394256 |  8950 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  8951 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  8952 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  8953 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  8954 | `	int doBody,          /* TRUE to process method body */` |
|         - |  8955 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  8956 | `	)` |
|         5 |  8957 | `{` |
|   2394261 |  8958 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2394261 |  8959 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  8960 | `	ph7_class_method *pMeth;` |
|         - |  8961 | `	sxi32 iFuncFlags;` |
|         - |  8962 | `	SyString *pName;` |
|         - |  8963 | `	SyToken *pEnd;` |
|         - |  8964 | `	sxi32 rc;` |
|         - |  8965 | `	/* Extract visibility level */` |
|   2394261 |  8966 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2394261 |  8967 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2394261 |  8968 | `	iFuncFlags = 0;` |
|   2394261 |  8969 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  8970 | `		/* Invalid method name */` |
|       ! 0 |  8971 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8972 | `		if( rc == SXERR_ABORT ){` |
|         - |  8973 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8974 | `			return SXERR_ABORT;` |
|         - |  8975 | `		}` |
|       ! 0 |  8976 | `		goto Synchronize;` |
|         - |  8977 | `	}` |
|   2394261 |  8978 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8979 | `		/* Return by reference,remember that */` |
|       ! 0 |  8980 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8981 | `		/* Jump the '&' token */` |
|       ! 0 |  8982 | `		pGen->pIn++;` |
|       ! 0 |  8983 | `	}` |
|   2394261 |  8984 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8985 | `		/* Invalid method name */` |
|       ! 0 |  8986 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8987 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8988 | `			return SXERR_ABORT;` |
|         - |  8989 | `		}` |
|       ! 0 |  8990 | `		goto Synchronize;` |
|         - |  8991 | `	}` |
|         - |  8992 | `	/* Peek method name */` |
|   2394261 |  8993 | `	pName = &pGen->pIn->sData;` |
|   2394261 |  8994 | `	nLine = pGen->pIn->nLine;` |
|         - |  8995 | `	/* Jump the method name */` |
|   2394261 |  8996 | `	pGen->pIn++;` |
|   2394261 |  8997 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8998 | `		/* Abstract method */` |
|    137445 |  8999 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  9000 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9001 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  9002 | `				&pClass->sName,pName);` |
|       ! 0 |  9003 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9004 | `				return SXERR_ABORT;` |
|         - |  9005 | `			}` |
|       ! 0 |  9006 | `		}` |
|         - |  9007 | `		/* Assemble method signature only */` |
|    137445 |  9008 | `		doBody = FALSE;` |
|     68720 |  9009 | `	}` |
|   2394261 |  9010 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  9011 | `		/* Syntax error */` |
|       ! 0 |  9012 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  9013 | `		if( rc == SXERR_ABORT ){` |
|         - |  9014 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9015 | `			return SXERR_ABORT;` |
|         - |  9016 | `		}` |
|       ! 0 |  9017 | `		goto Synchronize;` |
|         - |  9018 | `	}` |
|         - |  9019 | `	/* Allocate a new class_method instance */` |
|   2394261 |  9020 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2394261 |  9021 | `	if( pMeth == 0 ){` |
|       ! 0 |  9022 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9023 | `		return SXERR_ABORT;` |
|         - |  9024 | `	}` |
|   2394261 |  9025 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2394261 |  9026 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2394261 |  9027 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9028 | `		return SXERR_ABORT;` |
|         - |  9029 | `	}` |
|         - |  9030 | `	/* Jump the left parenthesis '(' */` |
|   2394261 |  9031 | `	pGen->pIn++;` |
|   2394261 |  9032 | `	pEnd = 0; /* cc warning */` |
|         - |  9033 | `	/* Delimit the method signature */` |
|   2394261 |  9034 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2394261 |  9035 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9036 | `		/* Syntax error */` |
|         3 |  9037 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  9038 | `		if( rc == SXERR_ABORT ){` |
|         - |  9039 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9040 | `			return SXERR_ABORT;` |
|         - |  9041 | `		}` |
|         3 |  9042 | `		goto Synchronize;` |
|         - |  9043 | `	}` |
|         - |  9044 | `	{` |
|   2394259 |  9045 | `		int bIsCtor = 0;` |
|   2394259 |  9046 | `		int bAbstractCtor = 0;` |
|   2394254 |  9047 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1397627 |  9048 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2312110 |  9049 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    164303 |  9050 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  9051 | `				bAbstractCtor = 1;` |
|         2 |  9052 | `			}else{` |
|    164301 |  9053 | `				bIsCtor = 1;` |
|         - |  9054 | `			}` |
|     82149 |  9055 | `		}` |
|   2394259 |  9056 | `		if( pGen->pIn < pEnd ){` |
|         - |  9057 | `			/* Collect method arguments */` |
|    863099 |  9058 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    863099 |  9059 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9060 | `				return SXERR_ABORT;` |
|         - |  9061 | `			}` |
|    431547 |  9062 | `		}` |
|         - |  9063 | `	}` |
|         - |  9064 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2394259 |  9065 | `	pGen->pIn = &pEnd[1];` |
|         - |  9066 | `	{` |
|   2394259 |  9067 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2394259 |  9068 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  9069 | `			return SXERR_ABORT;` |
|   2394259 |  9070 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  9071 | `			goto Synchronize;` |
|         - |  9072 | `		}` |
|         - |  9073 | `	}` |
|         - |  9074 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  9075 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  9076 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  9077 | `	{` |
|   2394259 |  9078 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  9079 | `		sxu32 i;` |
|   3684923 |  9080 | `		for( i = 0; i < nArg; i++ ){` |
|   1290679 |  9081 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  9082 | `			ph7_class_attr *pAttr;` |
|   1290679 |  9083 | `			sxi32 iAttrFlags = 0;` |
|         - |  9084 | `			int bArgTyped;` |
|   1290679 |  9085 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1290595 |  9086 | `				continue;` |
|         - |  9087 | `			}` |
|         - |  9088 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  9089 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  9090 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  9091 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  9092 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  9093 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  9094 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9095 | `					"Cannot declare variadic promoted property");` |
|         3 |  9096 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9097 | `					return SXERR_ABORT;` |
|         - |  9098 | `				}` |
|         3 |  9099 | `				goto Synchronize;` |
|         - |  9100 | `			}` |
|         - |  9101 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  9102 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  9103 | `			 * appear as an alternative of a union type. */` |
|        87 |  9104 | `			if( bArgTyped ){` |
|       122 |  9105 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  9106 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  9107 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  9108 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  9109 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9110 | `					return SXERR_ABORT;` |
|        83 |  9111 | `				}else if( rc != SXRET_OK ){` |
|         6 |  9112 | `					goto Synchronize;` |
|         - |  9113 | `				}` |
|        37 |  9114 | `			}` |
|         - |  9115 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  9116 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  9117 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9118 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  9119 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9120 | `					return SXERR_ABORT;` |
|         - |  9121 | `				}` |
|         3 |  9122 | `				goto Synchronize;` |
|         - |  9123 | `			}` |
|        81 |  9124 | `			if( bArgTyped ){` |
|        77 |  9125 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  9126 | `			}` |
|        81 |  9127 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  9128 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  9129 | `			}` |
|        81 |  9130 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  9131 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  9132 | `			}` |
|        81 |  9133 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  9134 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  9135 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  9136 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  9137 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9138 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  9139 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9140 | `						return SXERR_ABORT;` |
|         - |  9141 | `					}` |
|         3 |  9142 | `					goto Synchronize;` |
|         - |  9143 | `				}` |
|        24 |  9144 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  9145 | `			}` |
|        79 |  9146 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  9147 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  9148 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  9149 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9150 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  9151 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  9152 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9153 | `						return SXERR_ABORT;` |
|         - |  9154 | `					}` |
|       ! 0 |  9155 | `					goto Synchronize;` |
|         - |  9156 | `				}` |
|         5 |  9157 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  9158 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  9159 | `			}` |
|        79 |  9160 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  9161 | `			if( pAttr == 0 ){` |
|       ! 0 |  9162 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9163 | `				return SXERR_ABORT;` |
|         - |  9164 | `			}` |
|        79 |  9165 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  9166 | `				pAttr->nType = pArg->nType;` |
|        77 |  9167 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  9168 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  9169 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9170 | `					sxu32 k;` |
|        20 |  9171 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9172 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9173 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9174 | `					}` |
|         3 |  9175 | `				}` |
|        36 |  9176 | `			}` |
|        79 |  9177 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  9178 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9179 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9180 | `				return SXERR_ABORT;` |
|         - |  9181 | `			}` |
|        42 |  9182 | `		}` |
|         - |  9183 | `	}` |
|   2394249 |  9184 | `	if( doBody ){` |
|         - |  9185 | `		/* Compile method body */` |
|   2256809 |  9186 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2256809 |  9187 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9188 | `			return SXERR_ABORT;` |
|         - |  9189 | `		}` |
|         - |  9190 | `		/* The cursor sits just past the body's closing brace */` |
|   2256809 |  9191 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1128407 |  9192 | `	}else{` |
|         - |  9193 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    137445 |  9194 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    137445 |  9195 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     68720 |  9196 | `		}` |
|         - |  9197 | `		/* Only method signature is allowed */` |
|    137445 |  9198 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9199 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9200 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9201 | `				if( rc == SXERR_ABORT ){` |
|         - |  9202 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9203 | `					return SXERR_ABORT;` |
|         - |  9204 | `				}` |
|       ! 0 |  9205 | `				return SXERR_CORRUPT;` |
|         - |  9206 | `			}` |
|         - |  9207 | `	}` |
|         - |  9208 | `	/* All done,install the method */` |
|   2394249 |  9209 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2394249 |  9210 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9211 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9212 | `		return SXERR_ABORT;` |
|         - |  9213 | `	}` |
|   2394249 |  9214 | `	return SXRET_OK;` |
|         6 |  9215 | `Synchronize:` |
|         - |  9216 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9217 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9218 | `		pGen->pIn++;` |
|         4 |  9219 | `	}` |
|        16 |  9220 | `	return SXERR_CORRUPT;` |
|   1197133 |  9221 | `}` |
|         - |  9222 | `/*` |
|         - |  9223 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9224 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9225 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9226 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9227 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9228 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9229 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9230 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9231 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9232 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9233 | `` * implicit `$value` formal.`` |
|         - |  9234 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9235 | ` */` |
|         - |  9236 | `/*` |
|         - |  9237 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9238 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9239 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9240 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9241 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9242 | ` */` |
|        94 |  9243 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9244 | `{` |
|         - |  9245 | `	SyToken *p;` |
|       345 |  9246 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9247 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9248 | `			continue;` |
|         - |  9249 | `		}` |
|         - |  9250 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9251 | `		if( p + 3 < pEnd` |
|        80 |  9252 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9253 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9254 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9255 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9256 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9257 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9258 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9259 | `			return 1;` |
|         - |  9260 | `		}` |
|         - |  9261 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9262 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9263 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9264 | `		if( p > pStart` |
|        26 |  9265 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9266 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9267 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9268 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9269 | `			return 1;` |
|         - |  9270 | `		}` |
|        15 |  9271 | `	}` |
|        43 |  9272 | `	return 0;` |
|        48 |  9273 | `}` |
|         - |  9274 | `/*` |
|         - |  9275 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9276 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9277 | ` */` |
|       990 |  9278 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9279 | `{` |
|      1167 |  9280 | `	return p + 6 < pEnd` |
|       671 |  9281 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9282 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9283 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9284 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9285 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9286 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9287 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9288 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9289 | `	 && p[5].sData.nByte == 3` |
|         8 |  9290 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9291 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9292 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9293 | `}` |
|         - |  9294 | `/*` |
|         - |  9295 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9296 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9297 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9298 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9299 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9300 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9301 | ` * or SXERR_MEM.` |
|         - |  9302 | ` */` |
|         4 |  9303 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9304 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9305 | `{` |
|         5 |  9306 | `	SyToken *p = pStart;` |
|        35 |  9307 | `	while( p < pEnd ){` |
|        31 |  9308 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9309 | `			SyToken sTok;` |
|         - |  9310 | `			char zName[384];` |
|         - |  9311 | `			sxu32 nName;` |
|         - |  9312 | `			char *zDup;` |
|         - |  9313 | ``			/* `parent` `::` */`` |
|         5 |  9314 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9315 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9316 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9317 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9318 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9319 | `			if( zDup == 0 ){` |
|       ! 0 |  9320 | `				return SXERR_MEM;` |
|         - |  9321 | `			}` |
|         5 |  9322 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9323 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9324 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9325 | `			sTok.pUserData = 0;` |
|         5 |  9326 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9327 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9328 | `			continue;` |
|         - |  9329 | `		}` |
|        27 |  9330 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9331 | `		p++;` |
|         1 |  9332 | `	}` |
|         5 |  9333 | `	return SXRET_OK;` |
|         3 |  9334 | `}` |
|        94 |  9335 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9336 | `{` |
|        95 |  9337 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9338 | `	sxi32 rc;` |
|        95 |  9339 | `	int bRefsSelf = 0;` |
|        95 |  9340 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9341 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9342 | `		char zHook[384];` |
|         - |  9343 | `		SyString sHookName;` |
|         - |  9344 | `		ph7_class_method *pMeth;` |
|         - |  9345 | `		int bGet;` |
|       159 |  9346 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9347 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9348 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9349 | `			continue;` |
|         - |  9350 | `		}` |
|       145 |  9351 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9352 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9353 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9354 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9355 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9356 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9357 | `				return SXERR_ABORT;` |
|         - |  9358 | `			}` |
|       ! 0 |  9359 | `			return SXERR_CORRUPT;` |
|         - |  9360 | `		}` |
|       145 |  9361 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9362 | `			goto HookSyntax;` |
|         - |  9363 | `		}` |
|       144 |  9364 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9365 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9366 | `			bGet = 1;` |
|       106 |  9367 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9368 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9369 | `			bGet = 0;` |
|        34 |  9370 | `		}else{` |
|       ! 0 |  9371 | `			goto HookSyntax;` |
|         - |  9372 | `		}` |
|       145 |  9373 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9374 | `		sHookName.zString = zHook;` |
|       217 |  9375 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9376 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9377 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9378 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9379 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9380 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9381 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9382 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9383 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9384 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9385 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9386 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9387 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9388 | `					return SXERR_ABORT;` |
|         - |  9389 | `				}` |
|       ! 0 |  9390 | `				return SXERR_CORRUPT;` |
|         - |  9391 | `			}` |
|        15 |  9392 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9393 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9394 | `			if( pMeth == 0 ){` |
|       ! 0 |  9395 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9396 | `				return SXERR_ABORT;` |
|         - |  9397 | `			}` |
|        15 |  9398 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9399 | `			if( !bGet ){` |
|         - |  9400 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9401 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9402 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9403 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9404 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9405 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9406 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9407 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9408 | `				if( zVName == 0 ){` |
|       ! 0 |  9409 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9410 | `					return SXERR_ABORT;` |
|         - |  9411 | `				}` |
|         7 |  9412 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9413 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9414 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9415 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9416 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9417 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9418 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9419 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9420 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9421 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9422 | `				}` |
|         7 |  9423 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9424 | `			}` |
|        15 |  9425 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9426 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9427 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9428 | `				return SXERR_ABORT;` |
|         - |  9429 | `			}` |
|        15 |  9430 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9431 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9432 | `		}` |
|       130 |  9433 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9434 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9435 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9436 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9437 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9438 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9439 | `				return SXERR_ABORT;` |
|         - |  9440 | `			}` |
|       ! 0 |  9441 | `			return SXERR_CORRUPT;` |
|         - |  9442 | `		}` |
|       131 |  9443 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9444 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9445 | `		if( pMeth == 0 ){` |
|       ! 0 |  9446 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9447 | `			return SXERR_ABORT;` |
|         - |  9448 | `		}` |
|       131 |  9449 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9450 | `		if( !bGet ){` |
|         - |  9451 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9452 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9453 | `				SyToken *pRp = 0;` |
|        17 |  9454 | `				pGen->pIn++;` |
|        17 |  9455 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9456 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9457 | `					goto HookSyntax;` |
|         - |  9458 | `				}` |
|        17 |  9459 | `				if( pGen->pIn < pRp ){` |
|        17 |  9460 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9461 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9462 | `						return SXERR_ABORT;` |
|         - |  9463 | `					}` |
|         8 |  9464 | `				}` |
|        17 |  9465 | `				pGen->pIn = &pRp[1];` |
|         8 |  9466 | `			}` |
|        61 |  9467 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9468 | `				/* Implicit $value formal */` |
|         - |  9469 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9470 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9471 | `				if( zVName == 0 ){` |
|       ! 0 |  9472 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9473 | `					return SXERR_ABORT;` |
|         - |  9474 | `				}` |
|        45 |  9475 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9476 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9477 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9478 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9479 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9480 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9481 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9482 | `			}` |
|        30 |  9483 | `		}` |
|       165 |  9484 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9485 | `			/* Block body */` |
|        69 |  9486 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9487 | `			SyToken *pCloser = 0;` |
|        69 |  9488 | `			int bParentCall = 0;` |
|        69 |  9489 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9490 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9491 | `				SyToken *pScan;` |
|       753 |  9492 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9493 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9494 | `						bParentCall = 1;` |
|         3 |  9495 | `						break;` |
|         - |  9496 | `					}` |
|       343 |  9497 | `				}` |
|        34 |  9498 | `			}` |
|        69 |  9499 | `			if( bParentCall ){` |
|         - |  9500 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9501 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9502 | `				 * hook method), then continue past the original body. */` |
|         - |  9503 | `				SySet sBody;` |
|         3 |  9504 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9505 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9506 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9507 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9508 | `					SySetRelease(&sBody);` |
|       ! 0 |  9509 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9510 | `					return SXERR_ABORT;` |
|         - |  9511 | `				}` |
|         3 |  9512 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9513 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9514 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9515 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9516 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9517 | `				SySetRelease(&sBody);` |
|         3 |  9518 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9519 | `					return SXERR_ABORT;` |
|         - |  9520 | `				}` |
|         3 |  9521 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9522 | `			}else{` |
|        67 |  9523 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9524 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9525 | `					return SXERR_ABORT;` |
|         - |  9526 | `				}` |
|        67 |  9527 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9528 | `			}` |
|        69 |  9529 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9530 | `				bRefsSelf = 1;` |
|         9 |  9531 | `			}` |
|       128 |  9532 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9533 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9534 | `			GenBlock *pBlock;` |
|         - |  9535 | `			SySet *pInstrContainer;` |
|         - |  9536 | `			SyToken *pBodyStart;` |
|         - |  9537 | `			SyToken *pExprEnd;` |
|        63 |  9538 | `			SyToken *pSavedEnd = 0;` |
|         - |  9539 | `			SySet sBody;` |
|        63 |  9540 | `			int bParentCall = 0;` |
|        63 |  9541 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9542 | `			pBodyStart = pGen->pIn;` |
|         - |  9543 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9544 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9545 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9546 | `			 * method on a token copy. */` |
|         - |  9547 | `			{` |
|        63 |  9548 | `				sxi32 iNest = 0;` |
|        63 |  9549 | `				pExprEnd = pBodyStart;` |
|       355 |  9550 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9551 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9552 | `						iNest++;` |
|       351 |  9553 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9554 | `						if( iNest <= 0 ){` |
|       ! 0 |  9555 | `							break;` |
|         - |  9556 | `						}` |
|         9 |  9557 | `						iNest--;` |
|       343 |  9558 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9559 | `						break;` |
|         - |  9560 | `					}` |
|       293 |  9561 | `					pExprEnd++;` |
|         1 |  9562 | `				}` |
|         - |  9563 | `			}` |
|         - |  9564 | `			{` |
|         - |  9565 | `				SyToken *pScan;` |
|       335 |  9566 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9567 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9568 | `						bParentCall = 1;` |
|         3 |  9569 | `						break;` |
|         - |  9570 | `					}` |
|       137 |  9571 | `				}` |
|         - |  9572 | `			}` |
|        63 |  9573 | `			if( bParentCall ){` |
|         3 |  9574 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9575 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9576 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9577 | `					SySetRelease(&sBody);` |
|       ! 0 |  9578 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9579 | `					return SXERR_ABORT;` |
|         - |  9580 | `				}` |
|         3 |  9581 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9582 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9583 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9584 | `			}` |
|        94 |  9585 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9586 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9587 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9588 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9589 | `				return SXERR_ABORT;` |
|         - |  9590 | `			}` |
|        63 |  9591 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9592 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9593 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9594 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9595 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9596 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9597 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9598 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9599 | `			if( bParentCall ){` |
|         3 |  9600 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9601 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9602 | `				SySetRelease(&sBody);` |
|         1 |  9603 | `			}` |
|        63 |  9604 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9605 | `				return SXERR_ABORT;` |
|         - |  9606 | `			}` |
|        63 |  9607 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9608 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9609 | `				bRefsSelf = 1;` |
|        18 |  9610 | `			}` |
|        63 |  9611 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9612 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9613 | `			}` |
|        63 |  9614 | `			if( !bGet ){` |
|         - |  9615 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9616 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9617 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9618 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9619 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9620 | `				bRefsSelf = 1;` |
|         1 |  9621 | `			}` |
|        32 |  9622 | `		}else{` |
|       ! 0 |  9623 | `			goto HookSyntax;` |
|         - |  9624 | `		}` |
|       131 |  9625 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9626 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9627 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9628 | `			return SXERR_ABORT;` |
|         - |  9629 | `		}` |
|       131 |  9630 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9631 | `	}` |
|        95 |  9632 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9633 | `		goto HookSyntax;` |
|         - |  9634 | `	}` |
|        95 |  9635 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9636 | `	if( !bRefsSelf ){` |
|         - |  9637 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9638 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9639 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9640 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9641 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9642 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9643 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9644 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9645 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9646 | `				return SXERR_ABORT;` |
|         - |  9647 | `			}` |
|       ! 0 |  9648 | `			return SXERR_CORRUPT;` |
|         - |  9649 | `		}` |
|        20 |  9650 | `	}` |
|        95 |  9651 | `	return SXRET_OK;` |
|       ! 0 |  9652 | `HookSyntax:` |
|       ! 0 |  9653 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9654 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9655 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9656 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9657 | `		return SXERR_ABORT;` |
|         - |  9658 | `	}` |
|       ! 0 |  9659 | `	return SXERR_CORRUPT;` |
|        48 |  9660 | `}` |
|         - |  9661 | `/*` |
|         - |  9662 | ` * Compile an object interface.` |
|         - |  9663 | ` *  According to the PHP language reference manual` |
|         - |  9664 | ` *   Object Interfaces:` |
|         - |  9665 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9666 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9667 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9668 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9669 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9670 | ` */` |
|     68796 |  9671 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9672 | `{` |
|     68801 |  9673 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9674 | `	ph7_class *pClass,*pBase;` |
|         - |  9675 | `	SyToken *pEnd,*pTmp;` |
|         - |  9676 | `	SyString *pName;` |
|         - |  9677 | `	sxi32 nKwrd;` |
|         - |  9678 | `	sxi32 rc;` |
|         - |  9679 | `	/* Jump the 'interface' keyword */` |
|     68801 |  9680 | `	pGen->pIn++;` |
|         - |  9681 | `	/* Extract interface name */` |
|     68801 |  9682 | `	pName = &pGen->pIn->sData;` |
|         - |  9683 | `	/* Advance the stream cursor */` |
|     68801 |  9684 | `	pGen->pIn++;` |
|         - |  9685 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9686 | `		SyBlob sFQN;` |
|         - |  9687 | `		SyString sFQNStr;` |
|     68801 |  9688 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     68801 |  9689 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     68801 |  9690 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     68801 |  9691 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     68801 |  9692 | `		SyBlobRelease(&sFQN);` |
|         - |  9693 | `	}` |
|     68801 |  9694 | `	if( pClass == 0 ){` |
|       ! 0 |  9695 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9696 | `		return SXERR_ABORT;` |
|         - |  9697 | `	}` |
|     68801 |  9698 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     68801 |  9699 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9700 | `		return SXERR_ABORT;` |
|         - |  9701 | `	}` |
|         - |  9702 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     68801 |  9703 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9704 | `	/* Assume no base class is given */` |
|     68801 |  9705 | `	pBase = 0;` |
|     68801 |  9706 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26727 |  9707 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26727 |  9708 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9709 | `			SyBlob sResolved;` |
|         - |  9710 | `			SyString sBaseName;` |
|         - |  9711 | `			sxu32 nRefLine;` |
|         - |  9712 | `			/* Extract base interface */` |
|     26727 |  9713 | `			pGen->pIn++;` |
|     26727 |  9714 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26727 |  9715 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26727 |  9716 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9717 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9718 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9719 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9720 | `					pName);` |
|       ! 0 |  9721 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9722 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9723 | `					return SXERR_ABORT;` |
|         - |  9724 | `				}` |
|       ! 0 |  9725 | `				return SXRET_OK;` |
|         - |  9726 | `			}` |
|     40088 |  9727 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     26722 |  9728 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26727 |  9729 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9730 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9731 | `			/* Only interfaces is allowed */` |
|     26727 |  9732 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9733 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9734 | `			}` |
|     26727 |  9735 | `			if( pBase == 0 ){` |
|       ! 0 |  9736 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9737 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9738 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9739 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9740 | `					return SXERR_ABORT;` |
|         - |  9741 | `				}` |
|       ! 0 |  9742 | `			}` |
|     26727 |  9743 | `			SyBlobRelease(&sResolved);` |
|     13361 |  9744 | `		}` |
|     13361 |  9745 | `	}` |
|     68801 |  9746 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9747 | `		/* Syntax error */` |
|       ! 0 |  9748 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9749 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9750 | `		if( rc == SXERR_ABORT ){` |
|         - |  9751 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9752 | `			return SXERR_ABORT;` |
|         - |  9753 | `		}` |
|       ! 0 |  9754 | `		return SXRET_OK;` |
|         - |  9755 | `	}` |
|     68801 |  9756 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     68801 |  9757 | `	pEnd = 0; /* cc warning */` |
|         - |  9758 | `	/* Delimit the interface body */` |
|     68801 |  9759 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     68801 |  9760 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9761 | `		/* Syntax error */` |
|       ! 0 |  9762 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9763 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9764 | `		if( rc == SXERR_ABORT ){` |
|         - |  9765 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9766 | `			return SXERR_ABORT;` |
|         - |  9767 | `		}` |
|       ! 0 |  9768 | `		return SXRET_OK;` |
|         - |  9769 | `	}` |
|         - |  9770 | `	/* The delimiter token is the interface body's closing brace */` |
|     68801 |  9771 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9772 | `	/* Swap token stream */` |
|     68801 |  9773 | `	pTmp = pGen->pEnd;` |
|     68801 |  9774 | `	pGen->pEnd = pEnd;` |
|         - |  9775 | `	/* Start the parse process` |
|         - |  9776 | `	 * Note (According to the PHP reference manual):` |
|         - |  9777 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9778 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9779 | `	 */` |
|    126011 |  9780 | `	for(;;){` |
|         - |  9781 | `		/* Jump leading/trailing semi-colons */` |
|    435257 |  9782 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    183231 |  9783 | `			pGen->pIn++;` |
|         5 |  9784 | `		}` |
|    252031 |  9785 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9786 | `			/* End of interface body */` |
|     68797 |  9787 | `			break;` |
|         - |  9788 | `		}` |
|         - |  9789 | `		/* Bind a directly-preceding docblock to this member */` |
|    183239 |  9790 | `		GenStateSetPendingDoc(&(*pGen));` |
|    183239 |  9791 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9792 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9793 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9794 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9795 | `			if( rc == SXERR_ABORT ){` |
|         - |  9796 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9797 | `				return SXERR_ABORT;` |
|         - |  9798 | `			}` |
|       ! 0 |  9799 | `			goto done;` |
|         - |  9800 | `		}` |
|         - |  9801 | `		/* Extract the current keyword */` |
|    183239 |  9802 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    183239 |  9803 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9804 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9805 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9806 | `			const char *zKind = "member";` |
|         3 |  9807 | `			SyString *pMemberName = 0;` |
|         3 |  9808 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9809 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9810 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9811 | `					zKind = "constant";` |
|         3 |  9812 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9813 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9814 | `					}` |
|         1 |  9815 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9816 | `					zKind = "method";` |
|       ! 0 |  9817 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9818 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9819 | `					}` |
|       ! 0 |  9820 | `				}` |
|         1 |  9821 | `			}` |
|         3 |  9822 | `			if( pMemberName ){` |
|         4 |  9823 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9824 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9825 | `			}else{` |
|       ! 0 |  9826 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9827 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9828 | `			}` |
|         3 |  9829 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9830 | `				return SXERR_ABORT;` |
|         - |  9831 | `			}` |
|         3 |  9832 | `			goto done;` |
|         - |  9833 | `		}` |
|    183237 |  9834 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9835 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9836 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9837 | `			if( rc == SXERR_ABORT ){` |
|         - |  9838 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9839 | `				return SXERR_ABORT;` |
|         - |  9840 | `			}` |
|       ! 0 |  9841 | `			goto done;` |
|         - |  9842 | `		}` |
|    183237 |  9843 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9844 | `			/* Advance the stream cursor */` |
|    129795 |  9845 | `			pGen->pIn++;` |
|    129790 |  9846 | `			if( pGen->pIn < pGen->pEnd` |
|    129795 |  9847 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    129790 |  9848 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9849 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9850 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9851 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9852 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9853 | `				 * hooked properties" error). */` |
|       ! 0 |  9854 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9855 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9856 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9857 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9858 | `						return SXERR_ABORT;` |
|         - |  9859 | `					}` |
|       ! 0 |  9860 | `					goto done;` |
|         - |  9861 | `				}` |
|       ! 0 |  9862 | `				continue;` |
|         - |  9863 | `			}` |
|    129795 |  9864 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9865 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9866 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9867 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9868 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9869 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9870 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9871 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9872 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9873 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9874 | `							return SXERR_ABORT;` |
|         - |  9875 | `						}` |
|       ! 0 |  9876 | `						goto done;` |
|         - |  9877 | `					}` |
|       ! 0 |  9878 | `					continue;` |
|         - |  9879 | `				}` |
|       ! 0 |  9880 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9881 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9882 | `				if( rc == SXERR_ABORT ){` |
|         - |  9883 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9884 | `					return SXERR_ABORT;` |
|         - |  9885 | `				}` |
|       ! 0 |  9886 | `				goto done;` |
|         - |  9887 | `			}` |
|    129795 |  9888 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    129795 |  9889 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9890 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9891 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9892 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9893 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9894 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9895 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9896 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9897 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9898 | `							return SXERR_ABORT;` |
|         - |  9899 | `						}` |
|       ! 0 |  9900 | `						goto done;` |
|         - |  9901 | `					}` |
|         5 |  9902 | `					continue;` |
|         - |  9903 | `				}` |
|       ! 0 |  9904 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9905 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9906 | `				if( rc == SXERR_ABORT ){` |
|         - |  9907 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9908 | `					return SXERR_ABORT;` |
|         - |  9909 | `				}` |
|       ! 0 |  9910 | `				goto done;` |
|         - |  9911 | `			}` |
|     64893 |  9912 | `		}` |
|    183233 |  9913 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9914 | `			/* Parse constant */` |
|     53443 |  9915 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53443 |  9916 | `			if( rc != SXRET_OK ){` |
|         3 |  9917 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9918 | `					return SXERR_ABORT;` |
|         - |  9919 | `				}` |
|         3 |  9920 | `				goto done;` |
|         - |  9921 | `			}` |
|     26723 |  9922 | `		}else{` |
|    129795 |  9923 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    129795 |  9924 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9925 | `				/* Static method,record that */` |
|     11453 |  9926 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9927 | `				/* Advance the stream cursor */` |
|     11453 |  9928 | `				pGen->pIn++;` |
|     11448 |  9929 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11453 |  9930 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9931 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9932 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9933 | `						if( rc == SXERR_ABORT ){` |
|         - |  9934 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9935 | `							return SXERR_ABORT;` |
|         - |  9936 | `						}` |
|       ! 0 |  9937 | `						goto done;` |
|         - |  9938 | `				}` |
|      5724 |  9939 | `			}` |
|         - |  9940 | `			/* Process method signature (no body for interface methods) */` |
|    129795 |  9941 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    129795 |  9942 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9943 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9944 | `					return SXERR_ABORT;` |
|         - |  9945 | `				}` |
|       ! 0 |  9946 | `				goto done;` |
|         - |  9947 | `			}` |
|         - |  9948 | `		}` |
|         5 |  9949 | `	}` |
|         - |  9950 | `	/* Install the interface */` |
|     68797 |  9951 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     68797 |  9952 | `	if( rc == SXRET_OK && pBase ){` |
|         - |  9953 | `		/* Inherit from the base interface */` |
|     26727 |  9954 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13361 |  9955 | `	}` |
|     68797 |  9956 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9957 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9958 | `		return SXERR_ABORT;` |
|         - |  9959 | `	}` |
|     34396 |  9960 | `done:` |
|         - |  9961 | `	/* Point beyond the interface body */` |
|     68801 |  9962 | `	pGen->pIn  = &pEnd[1];` |
|     68801 |  9963 | `	pGen->pEnd = pTmp;` |
|     68801 |  9964 | `	return PH7_OK;` |
|     34403 |  9965 | `}` |
|         - |  9966 | `/*` |
|         - |  9967 | ` * Compile a user-defined class.` |
|         - |  9968 | ` * According to the PHP language reference manual` |
|         - |  9969 | ` *  class` |
|         - |  9970 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - |  9971 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - |  9972 | ` *  of the properties and methods belonging to the class.` |
|         - |  9973 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - |  9974 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - |  9975 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - |  9976 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  9977 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - |  9978 | ` *  (called "methods").` |
|         - |  9979 | ` */` |
|         - |  9980 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - |  9981 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - |  9982 | `struct TraitUseEntry {` |
|         - |  9983 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - |  9984 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - |  9985 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - |  9986 | `};` |
|         - |  9987 | `/*` |
|         - |  9988 | ` * Validate that methods implementing interface contracts have compatible` |
|         - |  9989 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - |  9990 | ` */` |
|    352822 |  9991 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9992 | `{` |
|         - |  9993 | `	ph7_class **apIface;` |
|         - |  9994 | `	sxu32 nIface,i;` |
|         - |  9995 | `	sxi32 rc;` |
|    352827 |  9996 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 |  9997 | `		return SXRET_OK;` |
|         - |  9998 | `	}` |
|    352827 |  9999 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    352827 | 10000 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    708051 | 10001 | `	for(i = 0; i < nIface; i++){` |
|    355229 | 10002 | `		ph7_class *pIface = apIface[i];` |
|         - | 10003 | `		SyHashEntry *pEntry;` |
|    355229 | 10004 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1023611 | 10005 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    668387 | 10006 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - | 10007 | `			ph7_class_method *pImplMeth;` |
|    668387 | 10008 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - | 10009 | `			/* Find the implementing method in the class */` |
|    668387 | 10010 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    668387 | 10011 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 | 10012 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - | 10013 | `			}` |
|         - | 10014 | `			/* Check visibility: interface methods must be implemented as public */` |
|    668369 | 10015 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 | 10016 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10017 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 | 10018 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 | 10019 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10020 | `					return SXERR_ABORT;` |
|         - | 10021 | `				}` |
|         1 | 10022 | `			}` |
|         - | 10023 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - | 10024 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - | 10025 | `			 */` |
|         - | 10026 | `			{` |
|    668369 | 10027 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    668369 | 10028 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    668369 | 10029 | `				int sigError = 0;` |
|    668369 | 10030 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 | 10031 | `					sigError = 1;` |
|    668368 | 10032 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - | 10033 | `					/* Extra parameters must all have default values */` |
|      3825 | 10034 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 10035 | `					sxu32 k;` |
|      7643 | 10036 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3825 | 10037 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 10038 | `							sigError = 1;` |
|         3 | 10039 | `							break;` |
|         - | 10040 | `						}` |
|      1914 | 10041 | `					}` |
|      1910 | 10042 | `				}` |
|    668369 | 10043 | `				if( sigError ){` |
|         - | 10044 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 10045 | `					ph7_vm_func_arg *aArgs;` |
|         - | 10046 | `					sxu32 j;` |
|         6 | 10047 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 10048 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 10049 | `					/* Build implementing method signature */` |
|         6 | 10050 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 10051 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 10052 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 10053 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 10054 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10055 | `					}` |
|         - | 10056 | `					/* Build interface method signature */` |
|         6 | 10057 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 10058 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 10059 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 10060 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 10061 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10062 | `					}` |
|         8 | 10063 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10064 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 10065 | `						&pClass->sName,pMName,` |
|         4 | 10066 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 10067 | `						&pIface->sName,pMName,` |
|         4 | 10068 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 10069 | `					SyBlobRelease(&sImplSig);` |
|         6 | 10070 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 10071 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10072 | `						return SXERR_ABORT;` |
|         - | 10073 | `					}` |
|         2 | 10074 | `				}` |
|         - | 10075 | `			}` |
|         5 | 10076 | `		}` |
|    177617 | 10077 | `	}` |
|    352827 | 10078 | `	return SXRET_OK;` |
|    176416 | 10079 | `}` |
|         - | 10080 | `/*` |
|         - | 10081 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 10082 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 10083 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 10084 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 10085 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 10086 | ` * means that specific hook is still missing.` |
|         - | 10087 | ` */` |
|        38 | 10088 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 10089 | `{` |
|         - | 10090 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 10091 | `	ph7_class_attr *pProp;` |
|        38 | 10092 | `	if( pMName->nByte <= nPfx` |
|        27 | 10093 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 10094 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 10095 | `		return 0; /* not a hook stub */` |
|         - | 10096 | `	}` |
|         7 | 10097 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 10098 | `	return pProp != 0` |
|         6 | 10099 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 10100 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 10101 | `}` |
|         - | 10102 | `/*` |
|         - | 10103 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 10104 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 10105 | ` */` |
|        16 | 10106 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 10107 | `{` |
|         - | 10108 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 10109 | `	if( pMName->nByte > nPfx` |
|        12 | 10110 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 10111 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 10112 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 10113 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 10114 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 10115 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 10116 | `		return;` |
|         - | 10117 | `	}` |
|        20 | 10118 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 10119 | `}` |
|         - | 10120 | `/*` |
|         - | 10121 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 10122 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 10123 | ` */` |
|    352822 | 10124 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10125 | `{` |
|         - | 10126 | `	ph7_class_method *pMeth;` |
|         - | 10127 | `	SyHashEntry *pEntry;` |
|         - | 10128 | `	sxu32 nAbstract;` |
|         - | 10129 | `	SyBlob sMsg;` |
|         - | 10130 | `	sxi32 rc;` |
|         - | 10131 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    352827 | 10132 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15315 | 10133 | `		return SXRET_OK;` |
|         - | 10134 | `	}` |
|         - | 10135 | `	/* Count abstract methods */` |
|    337517 | 10136 | `	nAbstract = 0;` |
|    337517 | 10137 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   4996393 | 10138 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4490125 | 10139 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4490125 | 10140 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 10141 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 10142 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10143 | `			}` |
|        20 | 10144 | `			nAbstract++;` |
|         8 | 10145 | `		}` |
|         5 | 10146 | `	}` |
|    337517 | 10147 | `	if( nAbstract == 0 ){` |
|    337503 | 10148 | `		return SXRET_OK;` |
|         - | 10149 | `	}` |
|         - | 10150 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 10151 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 10152 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 10153 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 10154 | `		&pClass->sName,nAbstract,` |
|         7 | 10155 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 10156 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 10157 | `	/* Second pass: list methods with origins */` |
|         - | 10158 | `	{` |
|        18 | 10159 | `		sxu32 nListed = 0;` |
|        18 | 10160 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 10161 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 10162 | `			ph7_class *pOrigin = 0;` |
|         - | 10163 | `			SyString *pMName;` |
|        22 | 10164 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 10165 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10166 | `				continue;` |
|         - | 10167 | `			}` |
|        20 | 10168 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10169 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10170 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10171 | `			}` |
|        20 | 10172 | `			if( nListed > 0 ){` |
|         3 | 10173 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10174 | `			}` |
|         - | 10175 | `			/* Find the origin of this abstract method.` |
|         - | 10176 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10177 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10178 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10179 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10180 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10181 | `			 * class's namespace.` |
|         - | 10182 | `			 */` |
|         - | 10183 | `			{` |
|         - | 10184 | `				ph7_class **apIface;` |
|         - | 10185 | `				ph7_class **apTrait;` |
|         - | 10186 | `				ph7_class *pWalk;` |
|         - | 10187 | `				sxu32 i;` |
|         - | 10188 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10189 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10190 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10191 | `				 */` |
|        20 | 10192 | `				if( pClass->pBase ){` |
|        11 | 10193 | `					pWalk = pClass->pBase;` |
|        19 | 10194 | `					while( pWalk ){` |
|         - | 10195 | `						ph7_class_method *pParentMeth;` |
|        13 | 10196 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10197 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10198 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10199 | `							 * in this class's ancestor chain.` |
|         - | 10200 | `							 */` |
|        13 | 10201 | `							int fromIface = 0;` |
|        13 | 10202 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10203 | `							while( pAnc ){` |
|         - | 10204 | `								ph7_class **apPI;` |
|         - | 10205 | `								sxu32 j;` |
|        15 | 10206 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10207 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10208 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10209 | `										fromIface = 1;` |
|        10 | 10210 | `										break;` |
|         - | 10211 | `									}` |
|       ! 0 | 10212 | `								}` |
|        15 | 10213 | `								if( fromIface ) break;` |
|         6 | 10214 | `								pAnc = pAnc->pBase;` |
|         2 | 10215 | `							}` |
|        13 | 10216 | `							if( !fromIface ){` |
|         3 | 10217 | `								pOrigin = pWalk;` |
|         3 | 10218 | `								break;` |
|         - | 10219 | `							}` |
|         4 | 10220 | `						}` |
|        10 | 10221 | `						pWalk = pWalk->pBase;` |
|         2 | 10222 | `					}` |
|         4 | 10223 | `				}` |
|         - | 10224 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10225 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10226 | `				 */` |
|        20 | 10227 | `				if( !pOrigin ){` |
|        18 | 10228 | `					pWalk = pClass;` |
|        40 | 10229 | `					while( pWalk && !pOrigin ){` |
|        26 | 10230 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10231 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10232 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10233 | `							ph7_class *pDeepest = 0;` |
|        28 | 10234 | `							while( pIface ){` |
|        16 | 10235 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10236 | `									pDeepest = pIface;` |
|         6 | 10237 | `								}` |
|        16 | 10238 | `								pIface = pIface->pBase;` |
|         4 | 10239 | `							}` |
|        16 | 10240 | `							if( pDeepest ){` |
|        16 | 10241 | `								pOrigin = pDeepest;` |
|        16 | 10242 | `								break;` |
|         - | 10243 | `							}` |
|       ! 0 | 10244 | `						}` |
|        26 | 10245 | `						pWalk = pWalk->pBase;` |
|         4 | 10246 | `					}` |
|         7 | 10247 | `				}` |
|         - | 10248 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10249 | `				if( !pOrigin ){` |
|         3 | 10250 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10251 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10252 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10253 | `							pOrigin = pClass;` |
|         3 | 10254 | `							break;` |
|         - | 10255 | `						}` |
|       ! 0 | 10256 | `					}` |
|         1 | 10257 | `				}` |
|         - | 10258 | `			}` |
|        20 | 10259 | `			if( pOrigin ){` |
|        20 | 10260 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10261 | `			}else{` |
|         - | 10262 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10263 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10264 | `			}` |
|        20 | 10265 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10266 | `			nListed++;` |
|         4 | 10267 | `		}` |
|         - | 10268 | `	}` |
|        18 | 10269 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10270 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10271 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10272 | `	SyBlobRelease(&sMsg);` |
|        18 | 10273 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10274 | `		return SXERR_ABORT;` |
|         - | 10275 | `	}` |
|        18 | 10276 | `	return SXRET_OK;` |
|    176416 | 10277 | `}` |
|         - | 10278 | `/*` |
|         - | 10279 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10280 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10281 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10282 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10283 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10284 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10285 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10286 | ` */` |
|    398932 | 10287 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10288 | `{` |
|    398937 | 10289 | `	int isAbsolute = 0;` |
|    398937 | 10290 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10291 | `	SyBlob sName;` |
|    398937 | 10292 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4415 | 10293 | `		isAbsolute = 1;` |
|      4415 | 10294 | `		pGen->pIn++;` |
|      2205 | 10295 | `	}` |
|    398937 | 10296 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         9 | 10297 | `		pGen->pIn = pStart;` |
|         9 | 10298 | `		return SXERR_INVALID;` |
|         - | 10299 | `	}` |
|    398931 | 10300 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    398931 | 10301 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    398931 | 10302 | `	pGen->pIn++;` |
|    598410 | 10303 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    199489 | 10304 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10305 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10306 | `		pGen->pIn++;` |
|        16 | 10307 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10308 | `		pGen->pIn++;` |
|         2 | 10309 | `	}` |
|    398931 | 10310 | `	if( isAbsolute ){` |
|      4413 | 10311 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2209 | 10312 | `	}else{` |
|         - | 10313 | `		SyString sRaw;` |
|    394523 | 10314 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    394523 | 10315 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10316 | `	}` |
|    398931 | 10317 | `	SyBlobRelease(&sName);` |
|    398931 | 10318 | `	return SXRET_OK;` |
|    199471 | 10319 | `}` |
|         - | 10320 | `/*` |
|         - | 10321 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10322 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10323 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10324 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10325 | ` * either direction cannot run unbounded.` |
|         - | 10326 | ` */` |
|         - | 10327 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    164290 | 10328 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10329 | `{` |
|         - | 10330 | `	ph7_class **apParent;` |
|         - | 10331 | `	sxu32 n;` |
|    427839 | 10332 | `	while( pInterface ){` |
|    271191 | 10333 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10334 | `			return FALSE;` |
|         - | 10335 | `		}` |
|    305554 | 10336 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     68726 | 10337 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7647 | 10338 | `			return TRUE;` |
|         - | 10339 | `		}` |
|    263549 | 10340 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    263549 | 10341 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10342 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10343 | `				return TRUE;` |
|         - | 10344 | `			}` |
|       ! 0 | 10345 | `		}` |
|    263549 | 10346 | `		pInterface = pInterface->pBase;` |
|    263549 | 10347 | `		iDepth++;` |
|         5 | 10348 | `	}` |
|    156653 | 10349 | `	return FALSE;` |
|     82150 | 10350 | `}` |
|    164290 | 10351 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10352 | `{` |
|    164295 | 10353 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10354 | `}` |
|         - | 10355 | `/*` |
|         - | 10356 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10357 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10358 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10359 | ` */` |
|      7642 | 10360 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10361 | `{` |
|      7651 | 10362 | `	while( pBase ){` |
|        10 | 10363 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10364 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10365 | `			return TRUE;` |
|         - | 10366 | `		}` |
|        10 | 10367 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10368 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10369 | `			return TRUE;` |
|         - | 10370 | `		}` |
|         5 | 10371 | `		pBase = pBase->pBase;` |
|         1 | 10372 | `	}` |
|      7643 | 10373 | `	return FALSE;` |
|      3826 | 10374 | `}` |
|         - | 10375 | `/*` |
|         - | 10376 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10377 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10378 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10379 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10380 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10381 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10382 | ` * pClass->aEnumCases for cases().` |
|         - | 10383 | ` */` |
|      7674 | 10384 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10385 | `{` |
|      7679 | 10386 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10387 | `	SySet *pInstrContainer;` |
|         - | 10388 | `	ph7_class_attr *pCase;` |
|         - | 10389 | `	SyString *pName;` |
|         - | 10390 | `	sxi32 rc;` |
|      7679 | 10391 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7679 | 10392 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10393 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10394 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10395 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10396 | `			return SXERR_ABORT;` |
|         - | 10397 | `		}` |
|       ! 0 | 10398 | `		goto Synchronize;` |
|         - | 10399 | `	}` |
|      7679 | 10400 | `	pName = &pGen->pIn->sData;` |
|         - | 10401 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7679 | 10402 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10403 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10404 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10405 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10406 | `			return SXERR_ABORT;` |
|         - | 10407 | `		}` |
|       ! 0 | 10408 | `		goto Synchronize;` |
|         - | 10409 | `	}` |
|      7679 | 10410 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10411 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7679 | 10412 | `	if( pCase == 0 ){` |
|       ! 0 | 10413 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10414 | `		return SXERR_ABORT;` |
|         - | 10415 | `	}` |
|      7679 | 10416 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7679 | 10417 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10418 | `		return SXERR_ABORT;` |
|         - | 10419 | `	}` |
|      7679 | 10420 | `	pGen->pIn++; /* Jump the case name */` |
|      7679 | 10421 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7665 | 10422 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10423 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10424 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10425 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10426 | `				return SXERR_ABORT;` |
|         - | 10427 | `			}` |
|         6 | 10428 | `			goto Synchronize;` |
|         - | 10429 | `		}` |
|      7661 | 10430 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10431 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10432 | `		 * (same technique as class constants). */` |
|      7661 | 10433 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7661 | 10434 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7661 | 10435 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7661 | 10436 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10437 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10438 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10439 | `		}` |
|      7661 | 10440 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7661 | 10441 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7661 | 10442 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10443 | `			return SXERR_ABORT;` |
|         - | 10444 | `		}` |
|      3833 | 10445 | `	}else{` |
|        17 | 10446 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10447 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10448 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10449 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10450 | `				return SXERR_ABORT;` |
|         - | 10451 | `			}` |
|       ! 0 | 10452 | `			goto Synchronize;` |
|         - | 10453 | `		}` |
|         - | 10454 | `	}` |
|      7675 | 10455 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7675 | 10456 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10457 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10458 | `		return SXERR_ABORT;` |
|         - | 10459 | `	}` |
|      7675 | 10460 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7675 | 10461 | `	return SXRET_OK;` |
|         2 | 10462 | `Synchronize:` |
|         - | 10463 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10464 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10465 | `		pGen->pIn++;` |
|         2 | 10466 | `	}` |
|         6 | 10467 | `	return SXERR_CORRUPT;` |
|      3842 | 10468 | `}` |
|         - | 10469 | `/*` |
|         - | 10470 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10471 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10472 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10473 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10474 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10475 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10476 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10477 | ` */` |
|      3840 | 10478 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10479 | `{` |
|         - | 10480 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10481 | `	const char *zBack;` |
|         - | 10482 | `	SySet sToken;` |
|         - | 10483 | `	char *zSrc;` |
|         - | 10484 | `	sxu32 nSrc,nMax;` |
|      3845 | 10485 | `	sxi32 rc = SXRET_OK;` |
|      3845 | 10486 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3840 | 10487 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3845 | 10488 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3845 | 10489 | `	if( zSrc == 0 ){` |
|       ! 0 | 10490 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10491 | `		return SXERR_ABORT;` |
|         - | 10492 | `	}` |
|      3845 | 10493 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3845 | 10494 | `	if( pClass->nEnumBacking != 0 ){` |
|      5747 | 10495 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10496 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10497 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10498 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1914 | 10499 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1919 | 10500 | `	}else{` |
|        21 | 10501 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10502 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10503 | `	}` |
|      3845 | 10504 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3845 | 10505 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3845 | 10506 | `	pSaveIn = pGen->pIn;` |
|      3845 | 10507 | `	pSaveEnd = pGen->pEnd;` |
|      3845 | 10508 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3845 | 10509 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15341 | 10510 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11501 | 10511 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10512 | `	}` |
|      3845 | 10513 | `	pGen->pIn = pSaveIn;` |
|      3845 | 10514 | `	pGen->pEnd = pSaveEnd;` |
|      3845 | 10515 | `	SySetRelease(&sToken);` |
|      3845 | 10516 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1925 | 10517 | `}` |
|         - | 10518 | `/*` |
|         - | 10519 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10520 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10521 | ` */` |
|         - | 10522 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10523 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10524 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10525 | `};` |
|         - | 10526 | `/*` |
|         - | 10527 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10528 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10529 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10530 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10531 | ` * and before the class is installed.` |
|         - | 10532 | ` */` |
|      3840 | 10533 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10534 | `{` |
|         - | 10535 | `	SyHashEntry *pEntry;` |
|         - | 10536 | `	sxi32 rc;` |
|         - | 10537 | `	sxu32 n;` |
|         - | 10538 | `	/* php: "Enum %s cannot include properties" */` |
|      3845 | 10539 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11519 | 10540 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7681 | 10541 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7681 | 10542 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10543 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10544 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10545 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10546 | `				return SXERR_ABORT;` |
|         - | 10547 | `			}` |
|         3 | 10548 | `			break;` |
|         - | 10549 | `		}` |
|         5 | 10550 | `	}` |
|         - | 10551 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     53765 | 10552 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     74880 | 10553 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     49925 | 10554 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10555 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10556 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10557 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10558 | `				return SXERR_ABORT;` |
|         - | 10559 | `			}` |
|       ! 0 | 10560 | `		}` |
|     24965 | 10561 | `	}` |
|         - | 10562 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10563 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10564 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10565 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10566 | `	{` |
|         - | 10567 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10568 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10569 | `		ph7_class_attr *pAttr;` |
|      3845 | 10570 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10571 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3845 | 10572 | `		if( pAttr == 0 ){` |
|       ! 0 | 10573 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10574 | `			return SXERR_ABORT;` |
|         - | 10575 | `		}` |
|      3845 | 10576 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3845 | 10577 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3845 | 10578 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3845 | 10579 | `		if( pClass->nEnumBacking != 0 ){` |
|      3833 | 10580 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10581 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3833 | 10582 | `			if( pAttr == 0 ){` |
|       ! 0 | 10583 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10584 | `				return SXERR_ABORT;` |
|         - | 10585 | `			}` |
|      3833 | 10586 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3833 | 10587 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10588 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10589 | `			}else{` |
|      3827 | 10590 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10591 | `			}` |
|      3833 | 10592 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1914 | 10593 | `		}` |
|         - | 10594 | `	}` |
|      3845 | 10595 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1925 | 10596 | `}` |
|         - | 10597 | `/*` |
|         - | 10598 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10599 | ` *` |
|         - | 10600 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10601 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10602 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10603 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10604 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10605 | ` * implements, body, install) is shared by both paths.` |
|         - | 10606 | ` */` |
|    352866 | 10607 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10608 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10609 | `{` |
|    352871 | 10610 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10611 | `	ph7_class *pClass,*pBase;` |
|         - | 10612 | `	SyToken *pEnd,*pTmp;` |
|         - | 10613 | `	sxi32 iProtection;` |
|         - | 10614 | `	SySet aInterfaces;` |
|         - | 10615 | `	SySet aUseEntries;` |
|         - | 10616 | `	sxi32 iAttrflags;` |
|         - | 10617 | `	SyString *pName;` |
|         - | 10618 | `	sxi32 nKwrd;` |
|         - | 10619 | `	sxi32 rc;` |
|         - | 10620 | `	/* Jump the 'class' keyword */` |
|    352871 | 10621 | `	pGen->pIn++;` |
|    352871 | 10622 | `	if( pAnonName ){` |
|         - | 10623 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10624 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10625 | `		 * then use the synthesized name. */` |
|        32 | 10626 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10627 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10628 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10629 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10630 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10631 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10632 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10633 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10634 | `		}` |
|        32 | 10635 | `		pName = pAnonName;` |
|        32 | 10636 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10637 | `	}else{` |
|    352843 | 10638 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10639 | `			/* Syntax error */` |
|       ! 0 | 10640 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10641 | `			if( rc == SXERR_ABORT ){` |
|         - | 10642 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10643 | `				return SXERR_ABORT;` |
|         - | 10644 | `			}` |
|         - | 10645 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10646 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10647 | `				pGen->pIn++;` |
|       ! 0 | 10648 | `			}` |
|       ! 0 | 10649 | `			return SXRET_OK;` |
|         - | 10650 | `		}` |
|         - | 10651 | `		/* Extract class name */` |
|    352843 | 10652 | `		pName = &pGen->pIn->sData;` |
|         - | 10653 | `		/* Advance the stream cursor */` |
|    352843 | 10654 | `		pGen->pIn++;` |
|         - | 10655 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10656 | `			SyBlob sFQN;` |
|         - | 10657 | `			SyString sFQNStr;` |
|    352843 | 10658 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    352843 | 10659 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    352843 | 10660 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    352843 | 10661 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    352843 | 10662 | `			SyBlobRelease(&sFQN);` |
|         - | 10663 | `		}` |
|         - | 10664 | `	}` |
|    352871 | 10665 | `	if( pClass == 0 ){` |
|       ! 0 | 10666 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10667 | `		return SXERR_ABORT;` |
|         - | 10668 | `	}` |
|    352866 | 10669 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3849 | 10670 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10671 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3835 | 10672 | `		pGen->pIn++; /* Jump ':' */` |
|      3830 | 10673 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3835 | 10674 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10675 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10676 | `			pGen->pIn++;` |
|      3828 | 10677 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3829 | 10678 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3827 | 10679 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3827 | 10680 | `			pGen->pIn++;` |
|      1916 | 10681 | `		}else{` |
|         3 | 10682 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10683 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10684 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10685 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10686 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10687 | `				return SXERR_ABORT;` |
|         - | 10688 | `			}` |
|         3 | 10689 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10690 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10691 | `			}` |
|         - | 10692 | `		}` |
|      1915 | 10693 | `	}` |
|    352871 | 10694 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    352871 | 10695 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10696 | `		return SXERR_ABORT;` |
|         - | 10697 | `	}` |
|         - | 10698 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    352871 | 10699 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    352871 | 10700 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10701 | `	/* Assume a standalone class */` |
|    352871 | 10702 | `	pBase = 0;` |
|    352871 | 10703 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    286641 | 10704 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    286641 | 10705 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10706 | `			SyBlob sResolved;` |
|         - | 10707 | `			SyString sBaseName;` |
|         - | 10708 | `			sxu32 nRefLine;` |
|    183431 | 10709 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10710 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10711 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10712 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10713 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10714 | `					return SXERR_ABORT;` |
|         - | 10715 | `				}` |
|       ! 0 | 10716 | `			}` |
|    183431 | 10717 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    183431 | 10718 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    183431 | 10719 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    183431 | 10720 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10721 | `				SyBlobRelease(&sResolved);` |
|         4 | 10722 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10723 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10724 | `					pName);` |
|         3 | 10725 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10726 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10727 | `					return SXERR_ABORT;` |
|         - | 10728 | `				}` |
|         3 | 10729 | `				return SXRET_OK;` |
|         - | 10730 | `			}` |
|    275141 | 10731 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    183424 | 10732 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    183429 | 10733 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10734 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10735 | `			/* Interfaces are not allowed */` |
|    183429 | 10736 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10737 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10738 | `			}` |
|    183429 | 10739 | `			if( pBase == 0 ){` |
|       ! 0 | 10740 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10741 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10742 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10743 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10744 | `					return SXERR_ABORT;` |
|         - | 10745 | `				}` |
|       ! 0 | 10746 | `			}else{` |
|    183429 | 10747 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10748 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10749 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10750 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10751 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10752 | `						return SXERR_ABORT;` |
|         - | 10753 | `					}` |
|         3 | 10754 | `					pBase = 0; /* Never inherit from an enum */` |
|    183428 | 10755 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10756 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10757 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10758 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10759 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10760 | `						return SXERR_ABORT;` |
|         - | 10761 | `					}` |
|       ! 0 | 10762 | `				}` |
|         - | 10763 | `			}` |
|    183429 | 10764 | `			SyBlobRelease(&sResolved);` |
|    183429 | 10765 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10766 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10767 | `			}` |
|     91712 | 10768 | `		}` |
|    286639 | 10769 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10770 | `			ph7_class *pInterface;` |
|         - | 10771 | `			/* Interface implementation */` |
|    107043 | 10772 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    110771 | 10773 | `			for(;;){` |
|         - | 10774 | `				SyBlob sResolved;` |
|         - | 10775 | `				SyString sIntName;` |
|         - | 10776 | `				sxu32 nRefLine;` |
|    164295 | 10777 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    164295 | 10778 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    164295 | 10779 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10780 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10781 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10782 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10783 | `						pName);` |
|       ! 0 | 10784 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10785 | `						return SXERR_ABORT;` |
|         - | 10786 | `					}` |
|       ! 0 | 10787 | `					break;` |
|         - | 10788 | `				}` |
|    328585 | 10789 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    164290 | 10790 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    164295 | 10791 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10792 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10793 | `				/* Only interfaces are allowed */` |
|    164295 | 10794 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10795 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10796 | `				}` |
|    164295 | 10797 | `				if( pInterface == 0 ){` |
|       ! 0 | 10798 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10799 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10800 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10801 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10802 | `						return SXERR_ABORT;` |
|         - | 10803 | `					}` |
|       ! 0 | 10804 | `				}else{` |
|         - | 10805 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10806 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10807 | `					 * unless they already extend Exception or Error.` |
|         - | 10808 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10809 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10810 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    164295 | 10811 | `					SyString *pFqn = &pClass->sName;` |
|    164295 | 10812 | `					int bIsExceptionOrError =` |
|     85965 | 10813 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    248347 | 10814 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    162389 | 10815 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3830 | 10816 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    168111 | 10817 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11466 | 10818 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3819 | 10819 | `						!bIsExceptionOrError ){` |
|        12 | 10820 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10821 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10822 | `							&pClass->sName);` |
|         9 | 10823 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10824 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10825 | `							return SXERR_ABORT;` |
|         - | 10826 | `						}` |
|         - | 10827 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10828 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10829 | `					}else{` |
|    164289 | 10830 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10831 | `					}` |
|         - | 10832 | `				}` |
|    164295 | 10833 | `				SyBlobRelease(&sResolved);` |
|    164295 | 10834 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53524 | 10835 | `					break;` |
|         - | 10836 | `				}` |
|     57257 | 10837 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10838 | `			}` |
|     53519 | 10839 | `		}` |
|    143317 | 10840 | `	}` |
|    352869 | 10841 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10842 | `		/* Syntax error */` |
|       ! 0 | 10843 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10844 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10845 | `		if( rc == SXERR_ABORT ){` |
|         - | 10846 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10847 | `			return SXERR_ABORT;` |
|         - | 10848 | `		}` |
|       ! 0 | 10849 | `		return SXRET_OK;` |
|         - | 10850 | `	}` |
|    352869 | 10851 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    352869 | 10852 | `	pEnd = 0; /* cc warning */` |
|         - | 10853 | `	/* Delimit the class body */` |
|    352869 | 10854 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    352869 | 10855 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10856 | `		/* Syntax error */` |
|       ! 0 | 10857 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10858 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10859 | `		if( rc == SXERR_ABORT ){` |
|         - | 10860 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10861 | `			return SXERR_ABORT;` |
|         - | 10862 | `		}` |
|       ! 0 | 10863 | `		return SXRET_OK;` |
|         - | 10864 | `	}` |
|         - | 10865 | `	/* The delimiter token is the class body's closing brace */` |
|    352869 | 10866 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10867 | `	/* Swap token stream */` |
|    352869 | 10868 | `	pTmp = pGen->pEnd;` |
|    352869 | 10869 | `	pGen->pEnd = pEnd;` |
|         - | 10870 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    352869 | 10871 | `	pClass->iFlags \|= iFlags;` |
|         - | 10872 | `	/* Start the parse process */` |
|   1369726 | 10873 | `	for(;;){` |
|         - | 10874 | `		/* Jump leading/trailing semi-colons */` |
|   3901705 | 10875 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    703273 | 10876 | `			pGen->pIn++;` |
|         5 | 10877 | `		}` |
|   3198437 | 10878 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10879 | `			/* End of class body */` |
|    352827 | 10880 | `			break;` |
|         - | 10881 | `		}` |
|         - | 10882 | `		/* Bind a directly-preceding docblock to this member */` |
|   2845615 | 10883 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2845610 | 10884 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1422810 | 10885 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10886 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10887 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10888 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10889 | `			if( rc == SXERR_ABORT ){` |
|         - | 10890 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10891 | `				return SXERR_ABORT;` |
|         - | 10892 | `			}` |
|       ! 0 | 10893 | `			goto done;` |
|         - | 10894 | `		}` |
|         - | 10895 | `		/* Assume public visibility */` |
|   2845615 | 10896 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2845615 | 10897 | `		iAttrflags = 0;` |
|         - | 10898 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10899 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10900 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10901 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2845615 | 10902 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10903 | `			int bMod = 0;` |
|       ! 0 | 10904 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10905 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10906 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10907 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10908 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10909 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10910 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10911 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10912 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10913 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10914 | `			}` |
|       ! 0 | 10915 | `			if( !bMod ){` |
|       ! 0 | 10916 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10917 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10918 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10919 | `						return SXERR_ABORT;` |
|         - | 10920 | `					}` |
|       ! 0 | 10921 | `					goto done;` |
|         - | 10922 | `				}` |
|       ! 0 | 10923 | `				continue;` |
|         - | 10924 | `			}` |
|       ! 0 | 10925 | `		}` |
|   2845615 | 10926 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10927 | `			/* Extract the current keyword */` |
|   2845615 | 10928 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2845615 | 10929 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10930 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7679 | 10931 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7679 | 10932 | `				if( rc != SXRET_OK ){` |
|         6 | 10933 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10934 | `						return SXERR_ABORT;` |
|         - | 10935 | `					}` |
|         6 | 10936 | `					goto done;` |
|         - | 10937 | `				}` |
|      7675 | 10938 | `				continue;` |
|         - | 10939 | `			}` |
|   2837941 | 10940 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 10941 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 10942 | `				TraitUseEntry sUse;` |
|     15333 | 10943 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15333 | 10944 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15333 | 10945 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7672 | 10946 | `				for(;;){` |
|         - | 10947 | `					ph7_class *pTrait;` |
|         - | 10948 | `					SyString *pTraitName;` |
|     15341 | 10949 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 10950 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10951 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 10952 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10953 | `							return SXERR_ABORT;` |
|         - | 10954 | `						}` |
|       ! 0 | 10955 | `						break;` |
|         - | 10956 | `					}` |
|     15341 | 10957 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 10958 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 10959 | `						SyBlob sResolved;` |
|     15341 | 10960 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15341 | 10961 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     30677 | 10962 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15336 | 10963 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15341 | 10964 | `						SyBlobRelease(&sResolved);` |
|         - | 10965 | `					}` |
|         - | 10966 | `					/* Only traits are allowed */` |
|     15341 | 10967 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 10968 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 10969 | `					}` |
|     15341 | 10970 | `					if( pTrait == 0 ){` |
|       ! 0 | 10971 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10972 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 10973 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10974 | `							return SXERR_ABORT;` |
|         - | 10975 | `						}` |
|       ! 0 | 10976 | `					}else{` |
|     15341 | 10977 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 10978 | `					}` |
|     15341 | 10979 | `					pGen->pIn++; /* Advance past trait name */` |
|     15341 | 10980 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7669 | 10981 | `						break;` |
|         - | 10982 | `					}` |
|        10 | 10983 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 10984 | `				}` |
|         - | 10985 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15333 | 10986 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 10987 | `					SyToken *pBlock;` |
|        13 | 10988 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 10989 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 10990 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 10991 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 10992 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 10993 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 10994 | `					}else{` |
|       ! 0 | 10995 | `						pGen->pIn = pGen->pEnd;` |
|         - | 10996 | `					}` |
|         5 | 10997 | `				}` |
|     15333 | 10998 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 10999 | `				/* The semicolon will be consumed by the outer loop */` |
|     15333 | 11000 | `				continue;` |
|         - | 11001 | `			}` |
|   2822613 | 11002 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 11003 | `				int nSetTok;` |
|   2577893 | 11004 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2577893 | 11005 | `				if( nSetVis ){` |
|         - | 11006 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 11007 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 11008 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11009 | `					pGen->pIn += nSetTok;` |
|         2 | 11010 | `				}else{` |
|   2577891 | 11011 | `					iProtection = nKwrd;` |
|   2577891 | 11012 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 11013 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 11014 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2577891 | 11015 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2577891 | 11016 | `					if( nSetVis ){` |
|         9 | 11017 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 11018 | `						pGen->pIn += nSetTok;` |
|         4 | 11019 | `					}` |
|         - | 11020 | `				}` |
|         - | 11021 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 11022 | ``				 * `public private(set) readonly int $x`. */`` |
|   2577893 | 11023 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 11024 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 11025 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 11026 | `				}` |
|   2577888 | 11027 | `				if( pGen->pIn >= pGen->pEnd` |
|   2577893 | 11028 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11029 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11030 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11031 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11032 | `					if( rc == SXERR_ABORT ){` |
|         - | 11033 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 11034 | `						return SXERR_ABORT;` |
|         - | 11035 | `					}` |
|       ! 0 | 11036 | `					goto done;` |
|         - | 11037 | `				}` |
|   2577893 | 11038 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11039 | `					/* Attribute declaration (untyped) */` |
|    408937 | 11040 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    408937 | 11041 | `					if( rc != SXRET_OK ){` |
|        11 | 11042 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11043 | `							return SXERR_ABORT;` |
|         - | 11044 | `						}` |
|        11 | 11045 | `						goto done;` |
|         - | 11046 | `					}` |
|    409073 | 11047 | `					continue;` |
|         - | 11048 | `				}` |
|   2168961 | 11049 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11050 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       299 | 11051 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       299 | 11052 | `					if( rc != SXRET_OK ){` |
|         8 | 11053 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11054 | `							return SXERR_ABORT;` |
|         - | 11055 | `						}` |
|         8 | 11056 | `						goto done;` |
|         - | 11057 | `					}` |
|       293 | 11058 | `					continue;` |
|         - | 11059 | `				}` |
|         - | 11060 | `				/* Extract the keyword */` |
|   2168667 | 11061 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1084331 | 11062 | `			}` |
|   2413387 | 11063 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 11064 | `				/* Process constant declaration */` |
|    236749 | 11065 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    236749 | 11066 | `				if( rc != SXRET_OK ){` |
|        11 | 11067 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11068 | `						return SXERR_ABORT;` |
|         - | 11069 | `					}` |
|        11 | 11070 | `					goto done;` |
|         - | 11071 | `				}` |
|    118373 | 11072 | `			}else{` |
|   2176643 | 11073 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 11074 | `					/* Static method or attribute,record that */` |
|     95551 | 11075 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     95551 | 11076 | `					pGen->pIn++; /* Jump the static keyword */` |
|     95551 | 11077 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11078 | `						int nSetTok;` |
|     68807 | 11079 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     68807 | 11080 | `						if( nSetVis ){` |
|         - | 11081 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 11082 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11083 | `							pGen->pIn += nSetTok;` |
|         2 | 11084 | `						}else{` |
|         - | 11085 | `							/* Extract the keyword */` |
|     68805 | 11086 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     68805 | 11087 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11088 | `								iProtection = nKwrd;` |
|       ! 0 | 11089 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 11090 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 11091 | `								if( nSetVis ){` |
|       ! 0 | 11092 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 11093 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 11094 | `								}` |
|       ! 0 | 11095 | `							}` |
|         - | 11096 | `						}` |
|     34401 | 11097 | `					}` |
|         - | 11098 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 11099 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 11100 | `					 * than a generic "expecting method" parse error. */` |
|     95551 | 11101 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11102 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11103 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 11104 | `					}` |
|     95546 | 11105 | `					if( pGen->pIn >= pGen->pEnd` |
|     95551 | 11106 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11107 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11108 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 11109 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11110 | `						if( rc == SXERR_ABORT ){` |
|         - | 11111 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11112 | `							return SXERR_ABORT;` |
|         - | 11113 | `						}` |
|       ! 0 | 11114 | `						goto done;` |
|         - | 11115 | `					}` |
|     95551 | 11116 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11117 | `						/* Attribute declaration */` |
|     26747 | 11118 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26747 | 11119 | `						if( rc != SXRET_OK ){` |
|         3 | 11120 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11121 | `								return SXERR_ABORT;` |
|         - | 11122 | `							}` |
|         3 | 11123 | `							goto done;` |
|         - | 11124 | `						}` |
|     26745 | 11125 | `						continue;` |
|         - | 11126 | `					}` |
|     68809 | 11127 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11128 | `						/* Typed static attribute declaration */` |
|        17 | 11129 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        17 | 11130 | `						if( rc != SXRET_OK ){` |
|         3 | 11131 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11132 | `								return SXERR_ABORT;` |
|         - | 11133 | `							}` |
|         3 | 11134 | `							goto done;` |
|         - | 11135 | `						}` |
|        15 | 11136 | `						continue;` |
|         - | 11137 | `					}` |
|         - | 11138 | `					/* Extract the keyword */` |
|     68795 | 11139 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2115492 | 11140 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 11141 | `					/* Abstract method,record that */` |
|      7657 | 11142 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 11143 | `					/* Mark the whole class as abstract */` |
|      7657 | 11144 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 11145 | `					/* Advance the stream cursor */` |
|      7657 | 11146 | `					pGen->pIn++;` |
|      7657 | 11147 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7657 | 11148 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7657 | 11149 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7655 | 11150 | `							iProtection = nKwrd;` |
|      7655 | 11151 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3825 | 11152 | `						}` |
|      3826 | 11153 | `					}` |
|      7657 | 11154 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7652 | 11155 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11156 | `							/* Static method */` |
|       ! 0 | 11157 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11158 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11159 | `					}` |
|      7657 | 11160 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7652 | 11161 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 11162 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 11163 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 11164 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 11165 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11166 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11167 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11168 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11169 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11170 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11171 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11172 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11173 | `										return SXERR_ABORT;` |
|         - | 11174 | `									}` |
|       ! 0 | 11175 | `									goto done;` |
|         - | 11176 | `								}` |
|         7 | 11177 | `								continue;` |
|         - | 11178 | `							}` |
|       ! 0 | 11179 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11180 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11181 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11182 | `							if( rc == SXERR_ABORT ){` |
|         - | 11183 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11184 | `								return SXERR_ABORT;` |
|         - | 11185 | `							}` |
|       ! 0 | 11186 | `							goto done;` |
|         - | 11187 | `					}` |
|      7651 | 11188 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2077268 | 11189 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11190 | `					/* final method ,record that */` |
|        20 | 11191 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        20 | 11192 | `					pGen->pIn++; /* Jump the final keyword */` |
|        20 | 11193 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11194 | `						/* Extract the keyword */` |
|        20 | 11195 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        20 | 11196 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        10 | 11197 | `							iProtection = nKwrd;` |
|        10 | 11198 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11199 | `						}` |
|         9 | 11200 | `					}` |
|        20 | 11201 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11202 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11203 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11204 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11205 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11206 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11207 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11208 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11209 | `									return SXERR_ABORT;` |
|         - | 11210 | `								}` |
|       ! 0 | 11211 | `								goto done;` |
|         - | 11212 | `							}` |
|        14 | 11213 | `							continue;` |
|         - | 11214 | `					}` |
|         8 | 11215 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11216 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11217 | `							/* Static method */` |
|       ! 0 | 11218 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11219 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11220 | `					}` |
|         8 | 11221 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11222 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11223 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11224 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11225 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11226 | `							if( rc == SXERR_ABORT ){` |
|         - | 11227 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11228 | `								return SXERR_ABORT;` |
|         - | 11229 | `							}` |
|       ! 0 | 11230 | `							goto done;` |
|         - | 11231 | `					}` |
|         8 | 11232 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11233 | `				}` |
|   2149869 | 11234 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11235 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11236 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11237 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11238 | `						if( rc == SXERR_ABORT ){` |
|         - | 11239 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11240 | `							return SXERR_ABORT;` |
|         - | 11241 | `						}` |
|       ! 0 | 11242 | `						goto done;` |
|         - | 11243 | `				}` |
|   2149869 | 11244 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11245 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11246 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11247 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11248 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11249 | `						if( rc == SXERR_ABORT ){` |
|         - | 11250 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11251 | `							return SXERR_ABORT;` |
|         - | 11252 | `						}` |
|       ! 0 | 11253 | `						goto done;` |
|         - | 11254 | `					}` |
|         - | 11255 | `					/* Attribute declaration */` |
|         7 | 11256 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11257 | `				}else{` |
|         - | 11258 | `					/* Process method declaration */` |
|   2149863 | 11259 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11260 | `				}` |
|   2149869 | 11261 | `				if( rc != SXRET_OK ){` |
|        16 | 11262 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11263 | `						return SXERR_ABORT;` |
|         - | 11264 | `					}` |
|        16 | 11265 | `					goto done;` |
|         - | 11266 | `				}` |
|         - | 11267 | `			}` |
|   1193299 | 11268 | `		}else{` |
|         - | 11269 | `			/* Attribute declaration */` |
|       ! 0 | 11270 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11271 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11272 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11273 | `					return SXERR_ABORT;` |
|         - | 11274 | `				}` |
|       ! 0 | 11275 | `				goto done;` |
|         - | 11276 | `			}` |
|         - | 11277 | `		}` |
|         5 | 11278 | `	}` |
|         - | 11279 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11280 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11281 | `	 */` |
|         - | 11282 | `	{` |
|         - | 11283 | `		TraitUseEntry *apUse;` |
|         - | 11284 | `		sxu32 nU;` |
|    352827 | 11285 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    368155 | 11286 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15333 | 11287 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15333 | 11288 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15333 | 11289 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15333 | 11290 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11291 | `			sxu32 nT;` |
|     15333 | 11292 | `			if( !hasResolution ){` |
|         - | 11293 | `				/* No conflict resolution block: use standard trait application */` |
|     30647 | 11294 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15329 | 11295 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15329 | 11296 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11297 | `						break;` |
|         - | 11298 | `					}` |
|      7667 | 11299 | `				}` |
|      7664 | 11300 | `			}else{` |
|         - | 11301 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11302 | `				 * then use the block to resolve method conflicts.` |
|         - | 11303 | `				 */` |
|         - | 11304 | `				SyToken *pR;` |
|        25 | 11305 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11306 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11307 | `					ph7_class_attr *pAR;` |
|         - | 11308 | `					SyHashEntry *pER;` |
|         - | 11309 | `					SyString *pNR;` |
|        15 | 11310 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11311 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11312 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11313 | `						pNR = &pAR->sName;` |
|       ! 0 | 11314 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11315 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11316 | `						}` |
|       ! 0 | 11317 | `					}` |
|        15 | 11318 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11319 | `				}` |
|         - | 11320 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11321 | `				pR = pUse->pResolvStart;` |
|        27 | 11322 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11323 | `					SyString sTrait,sMethod;` |
|         - | 11324 | `					ph7_class *pSrcTrait;` |
|         - | 11325 | `					ph7_class_method *pMeth;` |
|         - | 11326 | `					sxi32 nRKwrd;` |
|        41 | 11327 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11328 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11329 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11330 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11331 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11332 | `					sMethod = pR->sData;` |
|        17 | 11333 | `					pR++;` |
|        17 | 11334 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11335 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11336 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11337 | `							sTrait = sMethod;` |
|         7 | 11338 | `							pR++;` |
|         7 | 11339 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11340 | `							sMethod = pR->sData;` |
|         7 | 11341 | `							pR++;` |
|         3 | 11342 | `						}` |
|         3 | 11343 | `					}` |
|        17 | 11344 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11345 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11346 | `						continue;` |
|         - | 11347 | `					}` |
|        17 | 11348 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11349 | `					pR++;` |
|        17 | 11350 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11351 | `						pSrcTrait = 0;` |
|         7 | 11352 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11353 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11354 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11355 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11356 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11357 | `								break;` |
|         - | 11358 | `							}` |
|         2 | 11359 | `						}` |
|         5 | 11360 | `						if( pSrcTrait ){` |
|         5 | 11361 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11362 | `							if( pMeth ){` |
|         5 | 11363 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11364 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11365 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11366 | `								}` |
|         2 | 11367 | `							}` |
|         2 | 11368 | `						}` |
|         2 | 11369 | `					}` |
|        35 | 11370 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11371 | `				}` |
|         - | 11372 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11373 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11374 | `					ph7_class_method *pMR;` |
|         - | 11375 | `					SyHashEntry *pER;` |
|         - | 11376 | `					SyString *pNR;` |
|        15 | 11377 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11378 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11379 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11380 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11381 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11382 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11383 | `						}` |
|         3 | 11384 | `					}` |
|         9 | 11385 | `				}` |
|         - | 11386 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11387 | `				pR = pUse->pResolvStart;` |
|        27 | 11388 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11389 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11390 | `					ph7_class *pSrcTrait;` |
|         - | 11391 | `					ph7_class_method *pMeth;` |
|        27 | 11392 | `					int hasQual = 0;` |
|         - | 11393 | `					sxi32 nRKwrd;` |
|        41 | 11394 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11395 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11396 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11397 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11398 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11399 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11400 | `					sMethod = pR->sData;` |
|        17 | 11401 | `					pR++;` |
|        17 | 11402 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11403 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11404 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11405 | `							sTrait = sMethod;` |
|         7 | 11406 | `							hasQual = 1;` |
|         7 | 11407 | `							pR++;` |
|         7 | 11408 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11409 | `							sMethod = pR->sData;` |
|         7 | 11410 | `							pR++;` |
|         3 | 11411 | `						}` |
|         3 | 11412 | `					}` |
|        17 | 11413 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11414 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11415 | `						continue;` |
|         - | 11416 | `					}` |
|        17 | 11417 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11418 | `					pR++;` |
|        17 | 11419 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11420 | `						sxi32 iNewVis = -1;` |
|        13 | 11421 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11422 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11423 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11424 | `								iNewVis = nAK;` |
|         7 | 11425 | `								pR++;` |
|         3 | 11426 | `							}` |
|         3 | 11427 | `						}` |
|        13 | 11428 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11429 | `							sAlias = pR->sData;` |
|        11 | 11430 | `							pR++;` |
|         4 | 11431 | `						}` |
|        13 | 11432 | `						pMeth = 0;` |
|        13 | 11433 | `						if( hasQual ){` |
|         3 | 11434 | `							pSrcTrait = 0;` |
|         5 | 11435 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11436 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11437 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11438 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11439 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11440 | `									break;` |
|         - | 11441 | `								}` |
|         2 | 11442 | `							}` |
|         3 | 11443 | `							if( pSrcTrait ){` |
|         3 | 11444 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11445 | `							}` |
|         2 | 11446 | `						}else{` |
|        10 | 11447 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11448 | `						}` |
|        13 | 11449 | `						if( pMeth ){` |
|        13 | 11450 | `							if( sAlias.nByte > 0 ){` |
|         - | 11451 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11452 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11453 | `								 */` |
|         - | 11454 | `								ph7_class_method *pAlias;` |
|         - | 11455 | `								char *zAliasDup;` |
|        11 | 11456 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11457 | `								if( pAlias ){` |
|        11 | 11458 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11459 | `									if( iNewVis >= 0 ){` |
|         5 | 11460 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11461 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11462 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11463 | `									}` |
|        11 | 11464 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11465 | `									if( zAliasDup ){` |
|        11 | 11466 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11467 | `									}` |
|         7 | 11468 | `								}` |
|         7 | 11469 | `							}else if( iNewVis >= 0 ){` |
|         - | 11470 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11471 | `								ph7_class_method *pCopy;` |
|         3 | 11472 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11473 | `								if( pCopy ){` |
|         3 | 11474 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11475 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11476 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11477 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11478 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11479 | `									/* Replace the method in the class hash */` |
|         3 | 11480 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11481 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11482 | `								}` |
|         1 | 11483 | `							}` |
|         5 | 11484 | `						}` |
|         5 | 11485 | `						SXUNUSED(hasQual);` |
|         5 | 11486 | `					}` |
|        21 | 11487 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11488 | `				}` |
|         - | 11489 | `			}` |
|     15333 | 11490 | `			SySetRelease(&pUse->aTraits);` |
|      7669 | 11491 | `		}` |
|         - | 11492 | `	}` |
|    352827 | 11493 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11494 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11495 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3845 | 11496 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3845 | 11497 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11498 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11499 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11500 | `			return SXERR_ABORT;` |
|         - | 11501 | `		}` |
|      1920 | 11502 | `	}` |
|         - | 11503 | `	/* Install the class */` |
|    352827 | 11504 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    352827 | 11505 | `	if( rc == SXRET_OK ){` |
|         - | 11506 | `		ph7_class **apInterface;` |
|         - | 11507 | `		sxu32 n;` |
|    352827 | 11508 | `		if( pBase ){` |
|         - | 11509 | `			/* Inherit from base class and mark as a subclass */` |
|    183427 | 11510 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     91711 | 11511 | `		}` |
|    352827 | 11512 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    517111 | 11513 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11514 | `			/* Implements one or more interface */` |
|    164289 | 11515 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    164289 | 11516 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11517 | `				break;` |
|         - | 11518 | `			}` |
|     82147 | 11519 | `		}` |
|         - | 11520 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11521 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    352827 | 11522 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3845 | 11523 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3845 | 11524 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11525 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11526 | `			}` |
|      3845 | 11527 | `			if( pIntf ){` |
|      3845 | 11528 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1920 | 11529 | `			}` |
|      3845 | 11530 | `			if( pClass->nEnumBacking != 0 ){` |
|      3833 | 11531 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3833 | 11532 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11533 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11534 | `				}` |
|      3833 | 11535 | `				if( pIntf ){` |
|      3833 | 11536 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1914 | 11537 | `				}` |
|      1914 | 11538 | `			}` |
|      1920 | 11539 | `		}` |
|         - | 11540 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11541 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    352822 | 11542 | `		if( rc == SXRET_OK` |
|    352822 | 11543 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    352827 | 11544 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    187095 | 11545 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11546 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    187095 | 11547 | `			if( pStringable ){` |
|    187095 | 11548 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    187095 | 11549 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11550 | `				sxu32 i;` |
|    187095 | 11551 | `				int bAlready = 0;` |
|    225259 | 11552 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     41987 | 11553 | `					if( apImpl[i] == pStringable ){` |
|      3823 | 11554 | `						bAlready = 1;` |
|      3823 | 11555 | `						break;` |
|         - | 11556 | `					}` |
|     19087 | 11557 | `				}` |
|    187095 | 11558 | `				if( !bAlready ){` |
|    183277 | 11559 | `					PH7_ClassImplement(pClass,pStringable);` |
|     91636 | 11560 | `				}` |
|     93545 | 11561 | `			}` |
|     93545 | 11562 | `		}` |
|         - | 11563 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    352827 | 11564 | `		if( rc == SXRET_OK ){` |
|    352827 | 11565 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    352827 | 11566 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11567 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11568 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11569 | `				return SXERR_ABORT;` |
|         - | 11570 | `			}` |
|    176411 | 11571 | `		}` |
|         - | 11572 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    352827 | 11573 | `		if( rc == SXRET_OK ){` |
|    352827 | 11574 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    352827 | 11575 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11576 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11577 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11578 | `				return SXERR_ABORT;` |
|         - | 11579 | `			}` |
|    176411 | 11580 | `		}` |
|    176411 | 11581 | `	}` |
|    352827 | 11582 | `	SySetRelease(&aUseEntries);` |
|    352827 | 11583 | `	SySetRelease(&aInterfaces);` |
|    352827 | 11584 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11585 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11586 | `		return SXERR_ABORT;` |
|         - | 11587 | `	}` |
|    176411 | 11588 | `done:` |
|         - | 11589 | `	/* Point beyond the class body */` |
|    352869 | 11590 | `	pGen->pIn = &pEnd[1];` |
|    352869 | 11591 | `	pGen->pEnd = pTmp;` |
|    352869 | 11592 | `	return PH7_OK;` |
|    176438 | 11593 | `}` |
|         - | 11594 | `/* Compile a named class declaration (the common case). */` |
|    352838 | 11595 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11596 | `{` |
|    352843 | 11597 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11598 | `}` |
|         - | 11599 | `/*` |
|         - | 11600 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11601 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11602 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11603 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11604 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11605 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11606 | ` */` |
|        28 | 11607 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11608 | `{` |
|         - | 11609 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11610 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11611 | `	SyString sName;` |
|         - | 11612 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11613 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11614 | `	                              * is keyed to this 'class' token */` |
|         - | 11615 | `	ph7_value *pObj;` |
|        32 | 11616 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11617 | `	sxu32 nIdx,nLen;` |
|         - | 11618 | `	sxi32 nArg,rc;` |
|        14 | 11619 | `	SXUNUSED(iCompileFlag);` |
|         - | 11620 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11621 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11622 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11623 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11624 | `	}` |
|        32 | 11625 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11626 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11627 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11628 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11629 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11630 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11631 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11632 | `		return rc;` |
|         - | 11633 | `	}` |
|         - | 11634 | `	{` |
|         - | 11635 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11636 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11637 | `		if( pAnonClass` |
|        32 | 11638 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11639 | `			return SXERR_ABORT;` |
|         - | 11640 | `		}` |
|         - | 11641 | `	}` |
|         - | 11642 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11643 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11644 | `	nArg = 0;` |
|        32 | 11645 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11646 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11647 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11648 | `		SyToken *pArgNext;` |
|         7 | 11649 | `		pGen->pIn = pArgStart;` |
|         7 | 11650 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11651 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11652 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11653 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11654 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11655 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11656 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11657 | `					return SXERR_ABORT;` |
|         - | 11658 | `				}` |
|         7 | 11659 | `				nArg++;` |
|         3 | 11660 | `			}` |
|         7 | 11661 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11662 | `		}` |
|         7 | 11663 | `		pGen->pIn = pSavedIn;` |
|         7 | 11664 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11665 | `	}` |
|         - | 11666 | `	/* Load the synthesized class name */` |
|        32 | 11667 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11668 | `	if( pObj == 0 ){` |
|       ! 0 | 11669 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11670 | `		return SXERR_ABORT;` |
|         - | 11671 | `	}` |
|        32 | 11672 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11673 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11674 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11675 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11676 | `	return SXRET_OK;` |
|        18 | 11677 | `}` |
|         - | 11678 | `/*` |
|         - | 11679 | ` * Compile a user-defined abstract class.` |
|         - | 11680 | ` *  According to the PHP language reference manual` |
|         - | 11681 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11682 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11683 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11684 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11685 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11686 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11687 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11688 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11689 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11690 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11691 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11692 | ` *   could differ.` |
|         - | 11693 | ` */` |
|         - | 11694 | `/*` |
|         - | 11695 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11696 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11697 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11698 | ` */` |
|  12731890 | 11699 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11700 | `{` |
|  12731895 | 11701 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   7455111 | 11702 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   7455111 | 11703 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   7409293 | 11704 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3689335 | 11705 | `	}` |
|  12655459 | 11706 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  12655399 | 11707 | `	return FALSE;` |
|   6365950 | 11708 | `}` |
|         - | 11709 | `/*` |
|         - | 11710 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11711 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11712 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11713 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11714 | ` */` |
|  12655394 | 11715 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11716 | `{` |
|  12655399 | 11717 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  12655399 | 11718 | `	sxi32 iFlags = 0,iFlag;` |
|  12731895 | 11719 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76501 | 11720 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11721 | `			pDup = pIn;` |
|         2 | 11722 | `		}` |
|     76501 | 11723 | `		iFlags \|= iFlag;` |
|     76501 | 11724 | `		pIn++;` |
|         5 | 11725 | `	}` |
|  12655399 | 11726 | `	*ppIn = pIn;` |
|  12655399 | 11727 | `	if( ppDup ){ *ppDup = pDup; }` |
|  12655399 | 11728 | `	return iFlags;` |
|         5 | 11729 | `}` |
|         - | 11730 | `/*` |
|         - | 11731 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11732 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11733 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11734 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11735 | `` * `readonly`) to their existing handlers.`` |
|         - | 11736 | ` */` |
|  12620972 | 11737 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11738 | `{` |
|  12620977 | 11739 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   6352547 | 11740 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  12642001 | 11741 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11742 | `}` |
|         - | 11743 | `/*` |
|         - | 11744 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11745 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11746 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11747 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11748 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11749 | ` */` |
|     34422 | 11750 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11751 | `{` |
|         - | 11752 | `	SyToken *pDup;` |
|     34427 | 11753 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11754 | `	sxi32 rc;` |
|     34427 | 11755 | `	if( pDup ){` |
|         4 | 11756 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11757 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11758 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11759 | `			return SXERR_ABORT;` |
|         - | 11760 | `		}` |
|         1 | 11761 | `	}` |
|     34422 | 11762 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17216 | 11763 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11764 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11765 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11766 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11767 | `			return SXERR_ABORT;` |
|         - | 11768 | `		}` |
|         1 | 11769 | `	}` |
|     34427 | 11770 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17216 | 11771 | `}` |
|         - | 11772 | `/*` |
|         - | 11773 | ` * Compile a user-defined trait.` |
|         - | 11774 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11775 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11776 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11777 | ` */` |
|      7710 | 11778 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11779 | `{` |
|      7715 | 11780 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11781 | `	ph7_class *pClass;` |
|         - | 11782 | `	SyToken *pEnd,*pTmp;` |
|         - | 11783 | `	sxi32 iProtection;` |
|         - | 11784 | `	sxi32 iAttrflags;` |
|         - | 11785 | `	SyString *pName;` |
|         - | 11786 | `	sxi32 nKwrd;` |
|         - | 11787 | `	sxi32 rc;` |
|         - | 11788 | `	/* Jump the 'trait' keyword */` |
|      7715 | 11789 | `	pGen->pIn++;` |
|      7715 | 11790 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11791 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11792 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11793 | `			return SXERR_ABORT;` |
|         - | 11794 | `		}` |
|       ! 0 | 11795 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11796 | `			pGen->pIn++;` |
|       ! 0 | 11797 | `		}` |
|       ! 0 | 11798 | `		return SXRET_OK;` |
|         - | 11799 | `	}` |
|         - | 11800 | `	/* Extract trait name */` |
|      7715 | 11801 | `	pName = &pGen->pIn->sData;` |
|      7715 | 11802 | `	pGen->pIn++;` |
|         - | 11803 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11804 | `		SyBlob sFQN;` |
|         - | 11805 | `		SyString sFQNStr;` |
|      7715 | 11806 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7715 | 11807 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7715 | 11808 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7715 | 11809 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7715 | 11810 | `		SyBlobRelease(&sFQN);` |
|         - | 11811 | `	}` |
|      7715 | 11812 | `	if( pClass == 0 ){` |
|       ! 0 | 11813 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11814 | `		return SXERR_ABORT;` |
|         - | 11815 | `	}` |
|      7715 | 11816 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7715 | 11817 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11818 | `		return SXERR_ABORT;` |
|         - | 11819 | `	}` |
|         - | 11820 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7715 | 11821 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11822 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11823 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11824 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11825 | `			return SXERR_ABORT;` |
|         - | 11826 | `		}` |
|       ! 0 | 11827 | `		return SXRET_OK;` |
|         - | 11828 | `	}` |
|      7715 | 11829 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7715 | 11830 | `	pEnd = 0;` |
|      7715 | 11831 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7715 | 11832 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11833 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11834 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11835 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11836 | `			return SXERR_ABORT;` |
|         - | 11837 | `		}` |
|       ! 0 | 11838 | `		return SXRET_OK;` |
|         - | 11839 | `	}` |
|         - | 11840 | `	/* The delimiter token is the trait body's closing brace */` |
|      7715 | 11841 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11842 | `	/* Swap token stream */` |
|      7715 | 11843 | `	pTmp = pGen->pEnd;` |
|      7715 | 11844 | `	pGen->pEnd = pEnd;` |
|         - | 11845 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7715 | 11846 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11847 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     55411 | 11848 | `	for(;;){` |
|    156667 | 11849 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     22927 | 11850 | `			pGen->pIn++;` |
|         5 | 11851 | `		}` |
|    133745 | 11852 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7715 | 11853 | `			break;` |
|         - | 11854 | `		}` |
|         - | 11855 | `		/* Bind a directly-preceding docblock to this member */` |
|    126035 | 11856 | `		GenStateSetPendingDoc(&(*pGen));` |
|    126035 | 11857 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11858 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11859 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11860 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11861 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11862 | `				return SXERR_ABORT;` |
|         - | 11863 | `			}` |
|       ! 0 | 11864 | `			goto done;` |
|         - | 11865 | `		}` |
|    126035 | 11866 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    126035 | 11867 | `		iAttrflags = 0;` |
|    126035 | 11868 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    126035 | 11869 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    126035 | 11870 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11871 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11872 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11873 | `				for(;;){` |
|         - | 11874 | `					ph7_class *pUsedTrait;` |
|         - | 11875 | `					SyString *pUsedName;` |
|         5 | 11876 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11877 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11878 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11879 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11880 | `							return SXERR_ABORT;` |
|         - | 11881 | `						}` |
|       ! 0 | 11882 | `						break;` |
|         - | 11883 | `					}` |
|         5 | 11884 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11885 | `					{` |
|         - | 11886 | `						SyBlob sResolved;` |
|         5 | 11887 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11888 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11889 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11890 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11891 | `						SyBlobRelease(&sResolved);` |
|         - | 11892 | `					}` |
|         5 | 11893 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11894 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11895 | `					}` |
|         5 | 11896 | `					if( pUsedTrait == 0 ){` |
|         4 | 11897 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11898 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11899 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11900 | `							return SXERR_ABORT;` |
|         - | 11901 | `						}` |
|         2 | 11902 | `					}else{` |
|         3 | 11903 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11904 | `					}` |
|         5 | 11905 | `					pGen->pIn++;` |
|         5 | 11906 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11907 | `						break;` |
|         - | 11908 | `					}` |
|       ! 0 | 11909 | `					pGen->pIn++;` |
|       ! 0 | 11910 | `				}` |
|         5 | 11911 | `				continue;` |
|         - | 11912 | `			}` |
|    126031 | 11913 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    126015 | 11914 | `				iProtection = nKwrd;` |
|    126015 | 11915 | `				pGen->pIn++;` |
|    126010 | 11916 | `				if( pGen->pIn >= pGen->pEnd` |
|    126015 | 11917 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11918 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11919 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11920 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11921 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11922 | `						return SXERR_ABORT;` |
|         - | 11923 | `					}` |
|       ! 0 | 11924 | `					goto done;` |
|         - | 11925 | `				}` |
|    126015 | 11926 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     22913 | 11927 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     22913 | 11928 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11929 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11930 | `							return SXERR_ABORT;` |
|         - | 11931 | `						}` |
|       ! 0 | 11932 | `						goto done;` |
|         - | 11933 | `					}` |
|     22913 | 11934 | `					continue;` |
|         - | 11935 | `				}` |
|    103107 | 11936 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 11937 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 11938 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11939 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11940 | `							return SXERR_ABORT;` |
|         - | 11941 | `						}` |
|       ! 0 | 11942 | `						goto done;` |
|         - | 11943 | `					}` |
|         5 | 11944 | `					continue;` |
|         - | 11945 | `				}` |
|    103103 | 11946 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     51549 | 11947 | `			}` |
|    103119 | 11948 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 11949 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11950 | `					"Traits cannot have constants");` |
|       ! 0 | 11951 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11952 | `					return SXERR_ABORT;` |
|         - | 11953 | `				}` |
|       ! 0 | 11954 | `				goto done;` |
|       ! 0 | 11955 | `			}else{` |
|    103119 | 11956 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7647 | 11957 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7647 | 11958 | `					pGen->pIn++;` |
|      7647 | 11959 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7645 | 11960 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7645 | 11961 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11962 | `							iProtection = nKwrd;` |
|       ! 0 | 11963 | `							pGen->pIn++;` |
|       ! 0 | 11964 | `						}` |
|      3820 | 11965 | `					}` |
|      7642 | 11966 | `					if( pGen->pIn >= pGen->pEnd` |
|      7647 | 11967 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11968 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11969 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 11970 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11971 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11972 | `							return SXERR_ABORT;` |
|         - | 11973 | `						}` |
|       ! 0 | 11974 | `						goto done;` |
|         - | 11975 | `					}` |
|      7647 | 11976 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 11977 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 11978 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11979 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11980 | `								return SXERR_ABORT;` |
|         - | 11981 | `							}` |
|       ! 0 | 11982 | `							goto done;` |
|         - | 11983 | `						}` |
|         3 | 11984 | `						continue;` |
|         - | 11985 | `					}` |
|      7645 | 11986 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 11987 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11988 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11989 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11990 | `								return SXERR_ABORT;` |
|         - | 11991 | `							}` |
|       ! 0 | 11992 | `							goto done;` |
|         - | 11993 | `						}` |
|       ! 0 | 11994 | `						continue;` |
|         - | 11995 | `					}` |
|      7645 | 11996 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     99297 | 11997 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 11998 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 11999 | `					pGen->pIn++;` |
|         6 | 12000 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 12001 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 12002 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 12003 | `							iProtection = nKwrd;` |
|         6 | 12004 | `							pGen->pIn++;` |
|         2 | 12005 | `						}` |
|         2 | 12006 | `					}` |
|         6 | 12007 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 12008 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 12009 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12010 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 12011 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12012 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12013 | `							return SXERR_ABORT;` |
|         - | 12014 | `						}` |
|       ! 0 | 12015 | `						goto done;` |
|         - | 12016 | `					}` |
|         6 | 12017 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 12018 | `				}` |
|    103117 | 12019 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 12020 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12021 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 12022 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12023 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12024 | `						return SXERR_ABORT;` |
|         - | 12025 | `					}` |
|       ! 0 | 12026 | `					goto done;` |
|         - | 12027 | `				}` |
|    103117 | 12028 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 12029 | `					pGen->pIn++;` |
|       ! 0 | 12030 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 12031 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12032 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 12033 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12034 | `							return SXERR_ABORT;` |
|         - | 12035 | `						}` |
|       ! 0 | 12036 | `						goto done;` |
|         - | 12037 | `					}` |
|       ! 0 | 12038 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12039 | `				}else{` |
|    103117 | 12040 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 12041 | `				}` |
|    103117 | 12042 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 12043 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12044 | `						return SXERR_ABORT;` |
|         - | 12045 | `					}` |
|       ! 0 | 12046 | `					goto done;` |
|         - | 12047 | `				}` |
|         - | 12048 | `			}` |
|     51561 | 12049 | `		}else{` |
|       ! 0 | 12050 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12051 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12052 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12053 | `					return SXERR_ABORT;` |
|         - | 12054 | `				}` |
|       ! 0 | 12055 | `				goto done;` |
|         - | 12056 | `			}` |
|         - | 12057 | `		}` |
|         5 | 12058 | `	}` |
|         - | 12059 | `	/* Install the trait */` |
|      7715 | 12060 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7715 | 12061 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12062 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12063 | `		return SXERR_ABORT;` |
|         - | 12064 | `	}` |
|      3855 | 12065 | `done:` |
|         - | 12066 | `	/* Point beyond the trait body */` |
|      7715 | 12067 | `	pGen->pIn = &pEnd[1];` |
|      7715 | 12068 | `	pGen->pEnd = pTmp;` |
|      7715 | 12069 | `	return PH7_OK;` |
|      3860 | 12070 | `}` |
|         - | 12071 | `/*` |
|         - | 12072 | ` * Compile a user-defined class.` |
|         - | 12073 | ` *  According to the PHP language reference manual` |
|         - | 12074 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 12075 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 12076 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 12077 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 12078 | ` *   and functions (called "methods").` |
|         - | 12079 | ` */` |
|    314572 | 12080 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 12081 | `{` |
|         - | 12082 | `	sxi32 rc;` |
|    314577 | 12083 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    314577 | 12084 | `	return rc;` |
|         5 | 12085 | `}` |
|         - | 12086 | `/*` |
|         - | 12087 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 12088 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 12089 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 12090 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 12091 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 12092 | ` */` |
|  12578918 | 12093 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 12094 | `{` |
|  12783710 | 12095 | `	return (pIn->nType & PH7_TK_ID)` |
|   6494246 | 12096 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    214460 | 12097 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  12783705 | 12098 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 12099 | `}` |
|         - | 12100 | `/*` |
|         - | 12101 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 12102 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 12103 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 12104 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 12105 | ` */` |
|      3844 | 12106 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 12107 | `{` |
|      3849 | 12108 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 12109 | `}` |
|         - | 12110 | `/*` |
|         - | 12111 | ` * Exception handling.` |
|         - | 12112 | ` *  According to the PHP language reference manual` |
|         - | 12113 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 12114 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 12115 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 12116 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 12117 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 12118 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 12119 | ` *    (or re-thrown) within a catch block.` |
|         - | 12120 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 12121 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 12122 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 12123 | ` *    been defined with set_exception_handler().` |
|         - | 12124 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 12125 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 12126 | ` */` |
|         - | 12127 | `/*` |
|         - | 12128 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 12129 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 12130 | ` * indicates failure.` |
|         - | 12131 | ` */` |
|    507942 | 12132 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 12133 | `{` |
|    507947 | 12134 | `	sxi32 rc = SXRET_OK;` |
|    507947 | 12135 | `	if( pRoot->pOp ){` |
|    507935 | 12136 | `		switch( pRoot->pOp->iOp ){` |
|    253965 | 12137 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 12138 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 12139 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 12140 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 12141 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 12142 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    507935 | 12143 | `			break;` |
|       ! 0 | 12144 | `		default:` |
|         - | 12145 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 12146 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 12147 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 12148 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12149 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 12150 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 12151 | `				rc = SXERR_INVALID;` |
|       ! 0 | 12152 | `			}` |
|       ! 0 | 12153 | `			break;` |
|         - | 12154 | `		}` |
|    253982 | 12155 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 12156 | `		/* Unexpected expression */` |
|       ! 0 | 12157 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12158 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12159 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 12160 | `			rc = SXERR_INVALID;` |
|       ! 0 | 12161 | `		}` |
|       ! 0 | 12162 | `	}` |
|    507947 | 12163 | `	return rc;` |
|         5 | 12164 | `}` |
|         - | 12165 | `/*` |
|         - | 12166 | ` * Compile a 'throw' statement.` |
|         - | 12167 | ` * throw: This is how you trigger an exception.` |
|         - | 12168 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12169 | ` */` |
|    507906 | 12170 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12171 | `{` |
|    507911 | 12172 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12173 | `	GenBlock *pBlock;` |
|         - | 12174 | `	sxu32 nIdx;` |
|         - | 12175 | `	sxi32 rc;` |
|    507911 | 12176 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12177 | `	/* Compile the expression */` |
|    507911 | 12178 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    507911 | 12179 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12180 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12181 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12182 | `			return SXERR_ABORT;` |
|         - | 12183 | `		}` |
|       ! 0 | 12184 | `		return SXRET_OK;` |
|         - | 12185 | `	}` |
|    507911 | 12186 | `	pBlock = pGen->pCurrent;` |
|         - | 12187 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   2023261 | 12188 | `	while(pBlock->pParent){` |
|   2023257 | 12189 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    507907 | 12190 | `			break;` |
|         - | 12191 | `		}` |
|         - | 12192 | `		/* Point to the parent block */` |
|   1515355 | 12193 | `		pBlock = pBlock->pParent;` |
|         5 | 12194 | `	}` |
|         - | 12195 | `	/* Emit the throw instruction */` |
|    507911 | 12196 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12197 | `	/* Emit the jump */` |
|    507911 | 12198 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    507911 | 12199 | `	return SXRET_OK;` |
|    253958 | 12200 | `}` |
|         - | 12201 | `/*` |
|         - | 12202 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12203 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12204 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12205 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12206 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12207 | ` */` |
|        36 | 12208 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12209 | `{` |
|        38 | 12210 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12211 | `	GenBlock *pBlock;` |
|         - | 12212 | `	sxu32 nIdx;` |
|         - | 12213 | `	sxi32 rc;` |
|        18 | 12214 | `	(void)iCompileFlag;` |
|        38 | 12215 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12216 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12217 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12218 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12219 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12220 | `			return SXERR_ABORT;` |
|         - | 12221 | `		}` |
|       ! 0 | 12222 | `		return SXRET_OK;` |
|         - | 12223 | `	}` |
|        38 | 12224 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12225 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12226 | `		return SXERR_ABORT;` |
|         - | 12227 | `	}` |
|        38 | 12228 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12229 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12230 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12231 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12232 | `			return SXERR_ABORT;` |
|         - | 12233 | `		}` |
|       ! 0 | 12234 | `		return SXRET_OK;` |
|         - | 12235 | `	}` |
|         - | 12236 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12237 | `	pBlock = pGen->pCurrent;` |
|        60 | 12238 | `	while( pBlock->pParent ){` |
|        49 | 12239 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12240 | `			break;` |
|         - | 12241 | `		}` |
|        23 | 12242 | `		pBlock = pBlock->pParent;` |
|         1 | 12243 | `	}` |
|        38 | 12244 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12245 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12246 | `	return SXRET_OK;` |
|        20 | 12247 | `}` |
|         - | 12248 | `/*` |
|         - | 12249 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12250 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12251 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12252 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12253 | ` * compile error propagated from the parser.` |
|         - | 12254 | ` */` |
|        54 | 12255 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12256 | `{` |
|         - | 12257 | `	SyString sClassName;` |
|         - | 12258 | `	SyToken *pToken;` |
|         - | 12259 | `	SyString *pName;` |
|         - | 12260 | `	char *zDup;` |
|         - | 12261 | `	sxi32 rc;` |
|        59 | 12262 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        59 | 12263 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        59 | 12264 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        59 | 12265 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        59 | 12266 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12267 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12268 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12269 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12270 | `		return SXERR_INVALID;` |
|         - | 12271 | `	}` |
|        59 | 12272 | `	pGen->pIn++; /* '(' */` |
|        27 | 12273 | `	for(;;){` |
|         - | 12274 | `		SyBlob sResolved;` |
|        59 | 12275 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        59 | 12276 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12277 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12278 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12279 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12280 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12281 | `			return SXERR_INVALID;` |
|         - | 12282 | `		}` |
|        86 | 12283 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        54 | 12284 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        59 | 12285 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        59 | 12286 | `		SyBlobRelease(&sResolved);` |
|        59 | 12287 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12288 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        59 | 12289 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        54 | 12290 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12291 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12292 | `			pGen->pIn++; continue;` |
|         - | 12293 | `		}` |
|        59 | 12294 | `		break;` |
|       ! 0 | 12295 | `	}` |
|        54 | 12296 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12297 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12298 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12299 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12300 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12301 | `		return SXERR_INVALID;` |
|         - | 12302 | `	}` |
|        59 | 12303 | `	pGen->pIn++; /* '$' */` |
|        59 | 12304 | `	pName = &pGen->pIn->sData;` |
|        59 | 12305 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12306 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12307 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12308 | `	pGen->pIn++;` |
|        59 | 12309 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12310 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12311 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12312 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12313 | `		return SXERR_INVALID;` |
|         - | 12314 | `	}` |
|        59 | 12315 | `	pGen->pIn++; /* ')' */` |
|        59 | 12316 | `	return SXRET_OK;` |
|        32 | 12317 | `}` |
|         - | 12318 | `/*` |
|         - | 12319 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12320 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12321 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12322 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12323 | ` * VmThrowException):` |
|         - | 12324 | ` *` |
|         - | 12325 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12326 | ` *    <try body>` |
|         - | 12327 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12328 | ` *    JMP  -> finally\|end` |
|         - | 12329 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12330 | ` *    <catch body>` |
|         - | 12331 | ` *    JMP  -> finally\|end` |
|         - | 12332 | ` *    ... more catches ...` |
|         - | 12333 | ` *  Lfin: <finally body>` |
|         - | 12334 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12335 | ` *  Lend:` |
|         - | 12336 | ` */` |
|        98 | 12337 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12338 | `{` |
|       103 | 12339 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12340 | `	GenBlock *pTry;` |
|         - | 12341 | `	VmInstr *pInstr;` |
|       103 | 12342 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12343 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12344 | `	sxi32 rc;` |
|       103 | 12345 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12346 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       103 | 12347 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       103 | 12348 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       103 | 12349 | `	pTry->pUserData = pException;` |
|       103 | 12350 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       103 | 12351 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       103 | 12352 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       103 | 12353 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       103 | 12354 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       103 | 12355 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12356 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       103 | 12357 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       103 | 12358 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       103 | 12359 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       103 | 12360 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12361 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       103 | 12362 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12363 | `	/* Catch clauses (inline) */` |
|       103 | 12364 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        98 | 12365 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        59 | 12366 | `		sxu32 k = 0;` |
|        81 | 12367 | `		for(;;){` |
|         - | 12368 | `			ph7_exception_block sCatch;` |
|         - | 12369 | `			GenBlock *pCatchBlk;` |
|       113 | 12370 | `			sxu32 idxJmp = 0;` |
|       108 | 12371 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       104 | 12372 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        32 | 12373 | `				break;` |
|         - | 12374 | `			}` |
|        59 | 12375 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        59 | 12376 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12377 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        59 | 12378 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        59 | 12379 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        59 | 12380 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        59 | 12381 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12382 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12383 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12384 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        59 | 12385 | `			pCatchBlk->pUserData = pException;` |
|        59 | 12386 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        59 | 12387 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12388 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        59 | 12389 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12390 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12391 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        59 | 12392 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        59 | 12393 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        59 | 12394 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        59 | 12395 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        59 | 12396 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        59 | 12397 | `			k++;` |
|         5 | 12398 | `		}` |
|        27 | 12399 | `	}` |
|         - | 12400 | `	/* Finally (inline) */` |
|       103 | 12401 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12402 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12403 | `		GenBlock *pFinBlk;` |
|        52 | 12404 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12405 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12406 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12407 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12408 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12409 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12410 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12411 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12412 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12413 | `		pException->iHasFinally = 1;` |
|        24 | 12414 | `	}` |
|       103 | 12415 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       103 | 12416 | `	pException->iInlined = 1;` |
|         - | 12417 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12418 | `	{` |
|       103 | 12419 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12420 | `		sxu32 *aJ; sxu32 n;` |
|       103 | 12421 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       103 | 12422 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       103 | 12423 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       157 | 12424 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        59 | 12425 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        59 | 12426 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        32 | 12427 | `		}` |
|         - | 12428 | `	}` |
|       103 | 12429 | `	SySetRelease(&aCatchJmp);` |
|       103 | 12430 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12431 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12432 | `	}` |
|       103 | 12433 | `	return SXRET_OK;` |
|        54 | 12434 | `}` |
|         - | 12435 | `/*` |
|         - | 12436 | ` * Compile a 'catch' block.` |
|         - | 12437 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12438 | ` * an object containing the exception information.` |
|         - | 12439 | ` */` |
|     24412 | 12440 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12441 | `{` |
|     24417 | 12442 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12443 | `	ph7_exception_block sCatch;` |
|         - | 12444 | `	SySet *pInstrContainer;` |
|         - | 12445 | `	SyString sClassName;` |
|         - | 12446 | `	GenBlock *pCatch;` |
|         - | 12447 | `	SyToken *pToken;` |
|         - | 12448 | `	SyString *pName;` |
|         - | 12449 | `	char *zDup;` |
|         - | 12450 | `	sxi32 rc;` |
|     24417 | 12451 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12452 | `	/* Zero the structure */` |
|     24417 | 12453 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12454 | `	/* Initialize fields */` |
|     24417 | 12455 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24417 | 12456 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24417 | 12457 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12458 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12459 | `			pToken = pGen->pIn;` |
|       ! 0 | 12460 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12461 | `				pToken--;` |
|       ! 0 | 12462 | `			}` |
|       ! 0 | 12463 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12464 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12465 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12466 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12467 | `				return SXERR_ABORT;` |
|         - | 12468 | `			}` |
|       ! 0 | 12469 | `			return SXERR_INVALID;` |
|         - | 12470 | `	}` |
|         - | 12471 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24417 | 12472 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12220 | 12473 | `	for(;;){` |
|         - | 12474 | `		SyBlob sResolved;` |
|     24445 | 12475 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24445 | 12476 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12477 | `			SyBlobRelease(&sResolved);` |
|         6 | 12478 | `			pToken = pGen->pIn;` |
|         6 | 12479 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12480 | `				pToken--;` |
|       ! 0 | 12481 | `			}` |
|         8 | 12482 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12483 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12484 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12485 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12486 | `				return SXERR_ABORT;` |
|         - | 12487 | `			}` |
|         6 | 12488 | `			return SXERR_INVALID;` |
|         - | 12489 | `		}` |
|         - | 12490 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12491 | `		 * transient SyBlob allocation. */` |
|     36659 | 12492 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24436 | 12493 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24441 | 12494 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24441 | 12495 | `		SyBlobRelease(&sResolved);` |
|     24441 | 12496 | `		if( zDup == 0 ){` |
|       ! 0 | 12497 | `			goto Mem;` |
|         - | 12498 | `		}` |
|     24441 | 12499 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24441 | 12500 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12501 | `			goto Mem;` |
|         - | 12502 | `		}` |
|         - | 12503 | `		/* Check for '\|' (multi-catch separator) */` |
|     24436 | 12504 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24436 | 12505 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        33 | 12506 | `			pGen->pIn->sData.nByte == 1 &&` |
|        28 | 12507 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        30 | 12508 | `			pGen->pIn++; /* Consume the '\|' */` |
|        30 | 12509 | `			continue;` |
|         - | 12510 | `		}` |
|     24413 | 12511 | `		break;` |
|       ! 0 | 12512 | `	}` |
|     24408 | 12513 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24413 | 12514 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12515 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12516 | `			pToken = pGen->pIn;` |
|       ! 0 | 12517 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12518 | `				pToken--;` |
|       ! 0 | 12519 | `			}` |
|       ! 0 | 12520 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12521 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12522 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12523 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12524 | `				return SXERR_ABORT;` |
|         - | 12525 | `			}` |
|       ! 0 | 12526 | `			return SXERR_INVALID;` |
|         - | 12527 | `	}` |
|     24413 | 12528 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12529 | `	/* Duplicate instance name */` |
|     24413 | 12530 | `	pName = &pGen->pIn->sData;` |
|     24413 | 12531 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24413 | 12532 | `	if( zDup == 0 ){` |
|       ! 0 | 12533 | `		goto Mem;` |
|         - | 12534 | `	}` |
|     24413 | 12535 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24413 | 12536 | `	pGen->pIn++;` |
|     24413 | 12537 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12538 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12539 | `		pToken = pGen->pIn;` |
|       ! 0 | 12540 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12541 | `			pToken--;` |
|       ! 0 | 12542 | `		}` |
|       ! 0 | 12543 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12544 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12545 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12546 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12547 | `			return SXERR_ABORT;` |
|         - | 12548 | `		}` |
|       ! 0 | 12549 | `		return SXERR_INVALID;` |
|         - | 12550 | `	}` |
|         - | 12551 | `	/* Compile the block */` |
|     24413 | 12552 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12553 | `	/* Create the catch block */` |
|     24413 | 12554 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24413 | 12555 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12556 | `		return SXERR_ABORT;` |
|         - | 12557 | `	}` |
|         - | 12558 | `	/* Swap bytecode container */` |
|     24413 | 12559 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24413 | 12560 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12561 | `	/* Compile the block */` |
|     24413 | 12562 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12563 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24413 | 12564 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12565 | `	/* Emit the DONE instruction */` |
|     24413 | 12566 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12567 | `	/* Leave the block */` |
|     24413 | 12568 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12569 | `	/* Restore the default container */` |
|     24413 | 12570 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12571 | `	/* Install the catch block */` |
|     24413 | 12572 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24413 | 12573 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12574 | `		goto Mem;` |
|         - | 12575 | `	}` |
|     24413 | 12576 | `	return SXRET_OK;` |
|       ! 0 | 12577 | `Mem:` |
|       ! 0 | 12578 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12579 | `	return SXERR_ABORT;` |
|     12211 | 12580 | `}` |
|         - | 12581 | `/*` |
|         - | 12582 | ` * Compile a 'try' block.` |
|         - | 12583 | ` * A function using an exception should be in a "try" block.` |
|         - | 12584 | ` * If the exception does not trigger, the code will continue` |
|         - | 12585 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12586 | ` * is "thrown".` |
|         - | 12587 | ` */` |
|     24568 | 12588 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12589 | `{` |
|         - | 12590 | `	ph7_exception *pException;` |
|     24573 | 12591 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12592 | `	GenBlock *pTry;` |
|         - | 12593 | `	sxu32 nJmpIdx;` |
|         - | 12594 | `	sxi32 rc;` |
|         - | 12595 | `	/* Create the exception container */` |
|     24573 | 12596 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24573 | 12597 | `	if( pException == 0 ){` |
|       ! 0 | 12598 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12599 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12600 | `		return SXERR_ABORT;` |
|         - | 12601 | `	}` |
|         - | 12602 | `	/* Zero the structure */` |
|     24573 | 12603 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12604 | `	/* Initialize fields */` |
|     24573 | 12605 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24573 | 12606 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24573 | 12607 | `	pException->iHasFinally = 0;` |
|     24573 | 12608 | `	pException->iFinallyDone = 0;` |
|     24573 | 12609 | `	pException->pVm = pGen->pVm;` |
|         - | 12610 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12611 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12612 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12613 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12614 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12615 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24573 | 12616 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       103 | 12617 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12618 | `	}` |
|         - | 12619 | `	/* Create the try block */` |
|     24475 | 12620 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24475 | 12621 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12622 | `		return SXERR_ABORT;` |
|         - | 12623 | `	}` |
|         - | 12624 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24475 | 12625 | `	pTry->pUserData = pException;` |
|         - | 12626 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24475 | 12627 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12628 | `	/* Fix the jump later when the destination is resolved */` |
|     24475 | 12629 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24475 | 12630 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12631 | `	/* Compile the block */` |
|     24475 | 12632 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24475 | 12633 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12634 | `		return SXERR_ABORT;` |
|         - | 12635 | `	}` |
|         - | 12636 | `	/* Fix forward jumps now the destination is resolved */` |
|     24475 | 12637 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12638 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24475 | 12639 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12640 | `	/* Leave the block */` |
|     24475 | 12641 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12642 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24475 | 12643 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24468 | 12644 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12645 | `		/* Compile one or more catch blocks */` |
|     24408 | 12646 | `		for(;;){` |
|     48816 | 12647 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36667 | 12648 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12207 | 12649 | `					break;` |
|         - | 12650 | `			}` |
|     24417 | 12651 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24417 | 12652 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12653 | `				return SXERR_ABORT;` |
|         - | 12654 | `			}` |
|         5 | 12655 | `		}` |
|     12202 | 12656 | `	}` |
|         - | 12657 | `	/* Compile optional finally block */` |
|     24475 | 12658 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       730 | 12659 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12660 | `		SySet *pInstrContainer;` |
|         - | 12661 | `		GenBlock *pFinBlock;` |
|       129 | 12662 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12663 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12664 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12665 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12666 | `			return SXERR_ABORT;` |
|         - | 12667 | `		}` |
|         - | 12668 | `		/* Swap bytecode container */` |
|       129 | 12669 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12670 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12671 | `		/* Compile the finally body */` |
|       129 | 12672 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12673 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12674 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12675 | `			return SXERR_ABORT;` |
|         - | 12676 | `		}` |
|         - | 12677 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12678 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12679 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12680 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12681 | `		/* Leave the block */` |
|       129 | 12682 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12683 | `		/* Restore the default container */` |
|       129 | 12684 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12685 | `		pException->iHasFinally = 1;` |
|        62 | 12686 | `	}` |
|         - | 12687 | `	/* Must have at least one catch or finally */` |
|     24475 | 12688 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12689 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12690 | `			"Cannot use try without catch or finally");` |
|         8 | 12691 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12692 | `			return SXERR_ABORT;` |
|         - | 12693 | `		}` |
|         3 | 12694 | `	}` |
|     24475 | 12695 | `	return SXRET_OK;` |
|     12289 | 12696 | `}` |
|         - | 12697 | `/*` |
|         - | 12698 | ` * Compile a switch block.` |
|         - | 12699 | ` *  (See block-comment below for more information)` |
|         - | 12700 | ` */` |
|     53536 | 12701 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12702 | `{` |
|     53541 | 12703 | `	sxi32 rc = SXRET_OK;` |
|     53541 | 12704 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12705 | `		/* Unexpected token */` |
|       ! 0 | 12706 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12707 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12708 | `			return SXERR_ABORT;` |
|         - | 12709 | `		}` |
|       ! 0 | 12710 | `		pGen->pIn++;` |
|       ! 0 | 12711 | `	}` |
|     53541 | 12712 | `	pGen->pIn++;` |
|         - | 12713 | `	/* First instruction to execute in this block. */` |
|     53541 | 12714 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12715 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12716 | `	 * or the '}' token */` |
|     38366 | 12717 | `	for(;;){` |
|     76737 | 12718 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12719 | `			/* No more input to process */` |
|       ! 0 | 12720 | `			break;` |
|         - | 12721 | `		}` |
|     76737 | 12722 | `		rc = SXRET_OK;` |
|     76737 | 12723 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      3901 | 12724 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      3847 | 12725 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12726 | `					/* Unexpected token */` |
|       ! 0 | 12727 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12728 | `						&pGen->pIn->sData);` |
|       ! 0 | 12729 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12730 | `						return SXERR_ABORT;` |
|         - | 12731 | `					}` |
|         - | 12732 | `					/* FALL THROUGH */` |
|       ! 0 | 12733 | `				}` |
|      3847 | 12734 | `				rc = SXERR_EOF;` |
|      3847 | 12735 | `				break;` |
|         - | 12736 | `			}` |
|        32 | 12737 | `		}else{` |
|         - | 12738 | `			sxi32 nKwrd;` |
|         - | 12739 | `			/* Extract the keyword */` |
|     72841 | 12740 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     72841 | 12741 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|     24851 | 12742 | `				break;` |
|         - | 12743 | `			}` |
|     23149 | 12744 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12745 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12746 | `					/* Unexpected token */` |
|       ! 0 | 12747 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12748 | `						&pGen->pIn->sData);` |
|       ! 0 | 12749 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12750 | `						return SXERR_ABORT;` |
|         - | 12751 | `					}` |
|         - | 12752 | `					/* FALL THROUGH */` |
|       ! 0 | 12753 | `				}` |
|         - | 12754 | `				/* Block compiled */` |
|         3 | 12755 | `				break;` |
|         - | 12756 | `			}` |
|         - | 12757 | `		}` |
|         - | 12758 | `		/* Compile block */` |
|     23201 | 12759 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     23201 | 12760 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12761 | `			return SXERR_ABORT;` |
|         - | 12762 | `		}` |
|         5 | 12763 | `	}` |
|     53541 | 12764 | `	return rc;` |
|     26773 | 12765 | `}` |
|         - | 12766 | `/*` |
|         - | 12767 | ` * Compile a case eXpression.` |
|         - | 12768 | ` *  (See block-comment below for more information)` |
|         - | 12769 | ` */` |
|     53516 | 12770 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12771 | `{` |
|         - | 12772 | `	SySet *pInstrContainer;` |
|         - | 12773 | `	SyToken *pEnd,*pTmp;` |
|     53521 | 12774 | `	sxi32 iNest = 0;` |
|         - | 12775 | `	sxi32 rc;` |
|         - | 12776 | `	/* Delimit the expression */` |
|     53521 | 12777 | `	pEnd = pGen->pIn;` |
|    107045 | 12778 | `	while( pEnd < pGen->pEnd ){` |
|    107045 | 12779 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12780 | `			/* Increment nesting level */` |
|         3 | 12781 | `			iNest++;` |
|    107044 | 12782 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12783 | `			/* Decrement nesting level */` |
|         3 | 12784 | `			iNest--;` |
|    107042 | 12785 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     53521 | 12786 | `			break;` |
|         - | 12787 | `		}` |
|     53529 | 12788 | `		pEnd++;` |
|         5 | 12789 | `	}` |
|     53521 | 12790 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12791 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12792 | `		if( rc == SXERR_ABORT ){` |
|         - | 12793 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12794 | `			return SXERR_ABORT;` |
|         - | 12795 | `		}` |
|       ! 0 | 12796 | `	}` |
|         - | 12797 | `	/* Swap token stream */` |
|     53521 | 12798 | `	pTmp = pGen->pEnd;` |
|     53521 | 12799 | `	pGen->pEnd = pEnd;` |
|     53521 | 12800 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     53521 | 12801 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     53521 | 12802 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12803 | `	/* Emit the done instruction */` |
|     53521 | 12804 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     53521 | 12805 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12806 | `	/* Update token stream */` |
|     53521 | 12807 | `	pGen->pIn  = pEnd;` |
|     53521 | 12808 | `	pGen->pEnd = pTmp;` |
|     53521 | 12809 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12810 | `		return SXERR_ABORT;` |
|         - | 12811 | `	}` |
|     53521 | 12812 | `	return SXRET_OK;` |
|     26763 | 12813 | `}` |
|         - | 12814 | `/*` |
|         - | 12815 | ` * Compile the smart switch statement.` |
|         - | 12816 | ` * According to the PHP language reference manual` |
|         - | 12817 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12818 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12819 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12820 | ` *  This is exactly what the switch statement is for.` |
|         - | 12821 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12822 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12823 | ` *  of the outer loop, use continue 2.` |
|         - | 12824 | ` *  Note that switch/case does loose comparision.` |
|         - | 12825 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12826 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12827 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12828 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12829 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12830 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12831 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12832 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12833 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12834 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12835 | ` *  list for the next case.` |
|         - | 12836 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12837 | ` *  or floating-point numbers and strings.` |
|         - | 12838 | ` */` |
|      3844 | 12839 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12840 | `{` |
|         - | 12841 | `	GenBlock *pSwitchBlock;` |
|         - | 12842 | `	SyToken *pTmp,*pEnd;` |
|         - | 12843 | `	ph7_switch *pSwitch;` |
|         - | 12844 | `	sxu32 nToken;` |
|         - | 12845 | `	sxu32 nLine;` |
|         - | 12846 | `	sxi32 rc;` |
|      3849 | 12847 | `	nLine = pGen->pIn->nLine;` |
|         - | 12848 | `	/* Jump the 'switch' keyword */` |
|      3849 | 12849 | `	pGen->pIn++;` |
|      3849 | 12850 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12851 | `		/* Syntax error */` |
|       ! 0 | 12852 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12853 | `		if( rc == SXERR_ABORT ){` |
|         - | 12854 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12855 | `			return SXERR_ABORT;` |
|         - | 12856 | `		}` |
|       ! 0 | 12857 | `		goto Synchronize;` |
|         - | 12858 | `	}` |
|         - | 12859 | `	/* Jump the left parenthesis '(' */` |
|      3849 | 12860 | `	pGen->pIn++;` |
|      3849 | 12861 | `	pEnd = 0; /* cc warning */` |
|         - | 12862 | `	/* Create the loop block */` |
|      5771 | 12863 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      1922 | 12864 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      3849 | 12865 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12866 | `		return SXERR_ABORT;` |
|         - | 12867 | `	}` |
|         - | 12868 | `	/* Delimit the condition */` |
|      3849 | 12869 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      3849 | 12870 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12871 | `		/* Empty expression */` |
|       ! 0 | 12872 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12873 | `		if( rc == SXERR_ABORT ){` |
|         - | 12874 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12875 | `			return SXERR_ABORT;` |
|         - | 12876 | `		}` |
|       ! 0 | 12877 | `	}` |
|         - | 12878 | `	/* Swap token streams */` |
|      3849 | 12879 | `	pTmp = pGen->pEnd;` |
|      3849 | 12880 | `	pGen->pEnd = pEnd;` |
|         - | 12881 | `	/* Compile the expression */` |
|      3849 | 12882 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      3849 | 12883 | `	if( rc == SXERR_ABORT ){` |
|         - | 12884 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12885 | `		return SXERR_ABORT;` |
|         - | 12886 | `	}` |
|         - | 12887 | `	/* Update token stream */` |
|      3849 | 12888 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12889 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12890 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12891 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12892 | `			return SXERR_ABORT;` |
|         - | 12893 | `		}` |
|       ! 0 | 12894 | `		pGen->pIn++;` |
|       ! 0 | 12895 | `	}` |
|      3849 | 12896 | `	pGen->pIn  = &pEnd[1];` |
|      3849 | 12897 | `	pGen->pEnd = pTmp;` |
|      3849 | 12898 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      3844 | 12899 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12900 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12901 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12902 | `				pTmp--;` |
|       ! 0 | 12903 | `			}` |
|         - | 12904 | `			/* Unexpected token */` |
|       ! 0 | 12905 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12906 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12907 | `				return SXERR_ABORT;` |
|         - | 12908 | `			}` |
|       ! 0 | 12909 | `			goto Synchronize;` |
|         - | 12910 | `	}` |
|         - | 12911 | `	/* Set the delimiter token */` |
|      3849 | 12912 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12913 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12914 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12915 | `	}else{` |
|      3847 | 12916 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12917 | `	}` |
|      3849 | 12918 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12919 | `	/* Create the switch blocks container */` |
|      3849 | 12920 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      3849 | 12921 | `	if( pSwitch == 0 ){` |
|         - | 12922 | `		/* Abort compilation */` |
|       ! 0 | 12923 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12924 | `		return SXERR_ABORT;` |
|         - | 12925 | `	}` |
|         - | 12926 | `	/* Zero the structure */` |
|      3849 | 12927 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 12928 | `	/* Initialize fields */` |
|      3849 | 12929 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 12930 | `	/* Emit the switch instruction */` |
|      3849 | 12931 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 12932 | `	/* Compile case blocks */` |
|     51616 | 12933 | `	for(;;){` |
|         - | 12934 | `		sxu32 nKwrd;` |
|     53543 | 12935 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12936 | `			/* No more input to process */` |
|       ! 0 | 12937 | `			break;` |
|         - | 12938 | `		}` |
|     53543 | 12939 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 12940 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 12941 | `				/* Unexpected token */` |
|       ! 0 | 12942 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12943 | `					&pGen->pIn->sData);` |
|       ! 0 | 12944 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12945 | `					return SXERR_ABORT;` |
|         - | 12946 | `				}` |
|         - | 12947 | `				/* FALL THROUGH */` |
|       ! 0 | 12948 | `			}` |
|         - | 12949 | `			/* Block compiled */` |
|       ! 0 | 12950 | `			break;` |
|         - | 12951 | `		}` |
|         - | 12952 | `		/* Extract the keyword */` |
|     53543 | 12953 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     53543 | 12954 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12955 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 12956 | `				/* Unexpected token */` |
|       ! 0 | 12957 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12958 | `					&pGen->pIn->sData);` |
|       ! 0 | 12959 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12960 | `					return SXERR_ABORT;` |
|         - | 12961 | `				}` |
|         - | 12962 | `				/* FALL THROUGH */` |
|       ! 0 | 12963 | `			}` |
|         - | 12964 | `			/* Block compiled */` |
|         3 | 12965 | `			break;` |
|         - | 12966 | `		}` |
|     53541 | 12967 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 12968 | `			/*` |
|         - | 12969 | `			 * Accroding to the PHP language reference manual` |
|         - | 12970 | `			 *  A special case is the default case. This case matches anything` |
|         - | 12971 | `			 *  that wasn't matched by the other cases.` |
|         - | 12972 | `			 */` |
|        25 | 12973 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 12974 | `				/* Default case already compiled */` |
|       ! 0 | 12975 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 12976 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12977 | `					return SXERR_ABORT;` |
|         - | 12978 | `				}` |
|       ! 0 | 12979 | `			}` |
|        25 | 12980 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 12981 | `			/* Compile the default block */` |
|        25 | 12982 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 12983 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12984 | `				return SXERR_ABORT;` |
|        25 | 12985 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 12986 | `				break;` |
|         1 | 12987 | `			}` |
|     53522 | 12988 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 12989 | `			ph7_case_expr sCase;` |
|         - | 12990 | `			/* Standard case block */` |
|     53521 | 12991 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 12992 | `			/* initialize the structure */` |
|     53521 | 12993 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 12994 | `			/* Compile the case expression */` |
|     53521 | 12995 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     53521 | 12996 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12997 | `				return SXERR_ABORT;` |
|         - | 12998 | `			}` |
|         - | 12999 | `			/* Compile the case block */` |
|     53521 | 13000 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 13001 | `			/* Insert in the switch container */` |
|     53521 | 13002 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     53521 | 13003 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13004 | `				return SXERR_ABORT;` |
|     53521 | 13005 | `			}else if( rc == SXERR_EOF ){` |
|      3829 | 13006 | `				break;` |
|         - | 13007 | `			}` |
|     24851 | 13008 | `		}else{` |
|         - | 13009 | `			/* Unexpected token */` |
|       ! 0 | 13010 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13011 | `				&pGen->pIn->sData);` |
|       ! 0 | 13012 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13013 | `				return SXERR_ABORT;` |
|         - | 13014 | `			}` |
|       ! 0 | 13015 | `			break;` |
|         - | 13016 | `		}` |
|         5 | 13017 | `	}` |
|         - | 13018 | `	/* Fix all jumps now the destination is resolved */` |
|      3849 | 13019 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      3849 | 13020 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 13021 | `	/* Release the loop block */` |
|      3849 | 13022 | `	GenStateLeaveBlock(pGen,0);` |
|      3849 | 13023 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 13024 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      3849 | 13025 | `		pGen->pIn++;` |
|      1922 | 13026 | `	}` |
|         - | 13027 | `	/* Statement successfully compiled */` |
|      3849 | 13028 | `	return SXRET_OK;` |
|       ! 0 | 13029 | `Synchronize:` |
|         - | 13030 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 13031 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 13032 | `		pGen->pIn++;` |
|       ! 0 | 13033 | `	}` |
|       ! 0 | 13034 | `	return SXRET_OK;` |
|      1927 | 13035 | `}` |
|         - | 13036 | `/*` |
|         - | 13037 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 13038 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 13039 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 13040 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 13041 | ` */` |
|         - | 13042 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 13043 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 13044 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 13045 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 13046 |  |
|         - | 13047 | `/*` |
|         - | 13048 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 13049 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 13050 | ` * patched entries from the pending set.` |
|         - | 13051 | ` */` |
|  48031110 | 13052 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 13053 | `{` |
|  48031115 | 13054 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 13055 | `	sxu32 nTarget;` |
|         - | 13056 | `	sxu32 *aIdx;` |
|         - | 13057 | `	sxu32 i;` |
|  48031115 | 13058 | `	if( nCur <= nBaseline ){` |
|  48031019 | 13059 | `		return;` |
|         - | 13060 | `	}` |
|       100 | 13061 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 13062 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 13063 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 13064 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 13065 | `		if( pInstr ){` |
|       108 | 13066 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 13067 | `		}` |
|        56 | 13068 | `	}` |
|       100 | 13069 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  24015560 | 13070 | `}` |
|         - | 13071 |  |
|         - | 13072 | `/*` |
|         - | 13073 | ` * By-reference out-parameters of builtin functions.` |
|         - | 13074 | ` *` |
|         - | 13075 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 13076 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 13077 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 13078 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 13079 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 13080 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 13081 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 13082 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 13083 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 13084 | ` * creates it" behaviour).` |
|         - | 13085 | ` *` |
|         - | 13086 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 13087 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 13088 | ` */` |
|   6120170 | 13089 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 13090 | `{` |
|         - | 13091 | `	static const struct {` |
|         - | 13092 | `		const char *zName;` |
|         - | 13093 | `		sxu32 nByte;` |
|         - | 13094 | `		sxu32 mask;` |
|         - | 13095 | `	} aByRef[] = {` |
|         - | 13096 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - | 13097 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13098 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13099 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13100 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13101 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 13102 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 13103 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 13104 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 13105 | `	};` |
|         - | 13106 | `	sxu32 i;` |
|   6120175 | 13107 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1618949 | 13108 | `		return 0;` |
|         - | 13109 | `	}` |
|  44595411 | 13110 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  40151564 | 13111 | `		if( pName->nByte == aByRef[i].nByte` |
|  21148974 | 13112 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     57389 | 13113 | `			return aByRef[i].mask;` |
|         - | 13114 | `		}` |
|  20047095 | 13115 | `	}` |
|   4443847 | 13116 | `	return 0;` |
|   3060090 | 13117 | `}` |
|         - | 13118 | `/*` |
|         - | 13119 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 13120 | ` *` |
|         - | 13121 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 13122 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 13123 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 13124 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 13125 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 13126 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 13127 | ` */` |
|   6120170 | 13128 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 13129 | `{` |
|         - | 13130 | `	SyToken *p, *pEnd;` |
|   6120175 | 13131 | `	pOut->zString = 0;` |
|   6120175 | 13132 | `	pOut->nByte = 0;` |
|   6120175 | 13133 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 13134 | `		return;` |
|         - | 13135 | `	}` |
|   6120175 | 13136 | `	p = pLeft->pStart;` |
|   6120175 | 13137 | `	pEnd = pLeft->pEnd;` |
|         - | 13138 | `	/* Optional single leading namespace separator (absolute path). */` |
|   6120175 | 13139 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3851 | 13140 | `		p++;` |
|      1923 | 13141 | `	}` |
|   6120175 | 13142 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1618913 | 13143 | `		return;` |
|         - | 13144 | `	}` |
|         - | 13145 | `	/* Must be a single component: nothing follows the name token. */` |
|   4501267 | 13146 | `	if( p + 1 != pEnd ){` |
|        40 | 13147 | `		return;` |
|         - | 13148 | `	}` |
|   4501231 | 13149 | `	*pOut = p->sData;` |
|   3060090 | 13150 | `}` |
|         - | 13151 | `/*` |
|         - | 13152 | ` * Generate bytecode for a given expression tree.` |
|         - | 13153 | ` * If something goes wrong while generating bytecode` |
|         - | 13154 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 13155 | ` * this function takes care of generating the appropriate` |
|         - | 13156 | ` * error message.` |
|         - | 13157 | ` */` |
|  66907002 | 13158 | `static sxi32 GenStateEmitExprCode(` |
|         - | 13159 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 13160 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 13161 | `	sxi32 iFlags /* Control flags */` |
|         - | 13162 | `	)` |
|         5 | 13163 | `{` |
|         - | 13164 | `	VmInstr *pInstr;` |
|         - | 13165 | `	sxu32 nJmpIdx;` |
|  66907007 | 13166 | `	sxi32 iP1 = 0;` |
|  66907007 | 13167 | `	sxu32 iP2 = 0;` |
|  66907007 | 13168 | `	void *p3  = 0;` |
|         - | 13169 | `	sxi32 iVmOp;` |
|         - | 13170 | `	sxi32 rc;` |
|  66907007 | 13171 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  66907007 | 13172 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  66907007 | 13173 | `	sxu32 nRhsNsBase = 0;` |
|  66907007 | 13174 | `	if( pNode->xCode ){` |
|         - | 13175 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13176 | `		/* Compile node */` |
|  40329357 | 13177 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  40329357 | 13178 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  40329357 | 13179 | `		RE_SWAP_DELIMITER(pGen);` |
|  40329357 | 13180 | `		return rc;` |
|         - | 13181 | `	}` |
|  26577655 | 13182 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13183 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13184 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13185 | `		return SXERR_ABORT;` |
|         - | 13186 | `	}` |
|  26577655 | 13187 | `	iVmOp = pNode->pOp->iVmOp;` |
|  26577655 | 13188 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13189 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13190 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13191 | `		 * and later errors are still reported. */` |
|         3 | 13192 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13193 | `			"The (unset) cast is no longer supported");` |
|         3 | 13194 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13195 | `			return SXERR_ABORT;` |
|         - | 13196 | `		}` |
|         1 | 13197 | `	}` |
|  26577655 | 13198 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        91 | 13199 | `		sxu32 nJmp = 0;` |
|         - | 13200 | `		sxu32 nNcNsBase;` |
|         - | 13201 | `		VmInstr *pInstrFix;` |
|         - | 13202 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13203 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13204 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13205 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13206 | `		 * stack slot carries a writable nIdx. */` |
|        91 | 13207 | `		if( pNode->pRight ){` |
|        91 | 13208 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13209 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        91 | 13210 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13211 | `				return rc;` |
|         - | 13212 | `			}` |
|        91 | 13213 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13214 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13215 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13216 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13217 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13218 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13219 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13220 | `			 * cascade for the actual write path stays correct. */` |
|        91 | 13221 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        91 | 13222 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13223 | `				pInstrFix->iP2 = 3;` |
|        15 | 13224 | `			}` |
|        44 | 13225 | `		}` |
|         - | 13226 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        91 | 13227 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13228 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        91 | 13229 | `		if( pNode->pLeft ){` |
|        91 | 13230 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13231 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        91 | 13232 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13233 | `				return rc;` |
|         - | 13234 | `			}` |
|        91 | 13235 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        44 | 13236 | `		}` |
|         - | 13237 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        91 | 13238 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13239 | `		/* Patch the short-circuit jump to land after the store. */` |
|        91 | 13240 | `		if( nJmp > 0 ){` |
|        91 | 13241 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        91 | 13242 | `			if( pInstrFix ){` |
|        91 | 13243 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        44 | 13244 | `			}` |
|        44 | 13245 | `		}` |
|        91 | 13246 | `		return SXRET_OK;` |
|         - | 13247 | `	}` |
|  26577567 | 13248 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13249 | `		sxu32 nJz,nJmp;` |
|         - | 13250 | `		sxu32 nTernaryNsBase;` |
|         - | 13251 | `		/* Ternary operator require special handling */` |
|         - | 13252 | `		/* Phase#1: Compile the condition */` |
|    453613 | 13253 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    453613 | 13254 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    453613 | 13255 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13256 | `			return rc;` |
|         - | 13257 | `		}` |
|         - | 13258 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13259 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13260 | `		 * condition expression, not leak past the ternary. */` |
|    453613 | 13261 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    453613 | 13262 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    453613 | 13263 | `		if( pNode->pLeft ){` |
|         - | 13264 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13265 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    449729 | 13266 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13267 | `			/* Phase#3: Compile the 'then' expression  */` |
|    449729 | 13268 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    449729 | 13269 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    449729 | 13270 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13271 | `				return rc;` |
|         - | 13272 | `			}` |
|    449729 | 13273 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    224867 | 13274 | `		}else{` |
|         - | 13275 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13276 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13277 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3889 | 13278 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3889 | 13279 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13280 | `		}` |
|         - | 13281 | `		/* Phase#4: Emit the unconditional jump */` |
|    453613 | 13282 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13283 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    453613 | 13284 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    453613 | 13285 | `		if( pInstr ){` |
|    453613 | 13286 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    226804 | 13287 | `		}` |
|    453613 | 13288 | `		if( !pNode->pLeft ){` |
|         - | 13289 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3889 | 13290 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1942 | 13291 | `		}` |
|         - | 13292 | `		/* Phase#6: Compile the 'else' expression */` |
|    453613 | 13293 | `		if( pNode->pRight ){` |
|    453613 | 13294 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    453613 | 13295 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    453613 | 13296 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13297 | `				return rc;` |
|         - | 13298 | `			}` |
|    453613 | 13299 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    226804 | 13300 | `		}` |
|    453613 | 13301 | `		if( nJmp > 0 ){` |
|         - | 13302 | `			/* Phase#7: Fix the unconditional jump */` |
|    453613 | 13303 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    453613 | 13304 | `			if( pInstr ){` |
|    453613 | 13305 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    226804 | 13306 | `			}` |
|    226804 | 13307 | `		}` |
|         - | 13308 | `		/* All done */` |
|    453613 | 13309 | `		return SXRET_OK;` |
|         - | 13310 | `	}` |
|  26123959 | 13311 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13312 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13313 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13314 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13315 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13316 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13317 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13318 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13319 | `		sxu32 nPipeNsBase;` |
|        27 | 13320 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13321 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13322 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13323 | `				"'\|>': Missing operand");` |
|       ! 0 | 13324 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13325 | `		}` |
|         - | 13326 | `		/* Argument: the LHS value. */` |
|        27 | 13327 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13328 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13329 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13330 | `			return rc;` |
|         - | 13331 | `		}` |
|        27 | 13332 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13333 | `		/* Callable: the RHS. */` |
|        27 | 13334 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13335 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13336 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13337 | `			return rc;` |
|         - | 13338 | `		}` |
|        27 | 13339 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13340 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13341 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13342 | `		return SXRET_OK;` |
|         - | 13343 | `	}` |
|  26123933 | 13344 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13345 | `	/* Generate code for the left tree */` |
|  26123933 | 13346 | `	if( pNode->pLeft ){` |
|  26101039 | 13347 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  26101039 | 13348 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13349 | `			ph7_expr_node **apNode;` |
|   6124307 | 13350 | `			int hasSpread = 0;` |
|   6124307 | 13351 | `			int hasNamed = 0;` |
|   6124307 | 13352 | `			int bAnySpread = 0;` |
|   6124307 | 13353 | `			sxu32 byRefMask = 0;` |
|         - | 13354 | `			sxi32 nArgs;` |
|         - | 13355 | `			sxi32 n;` |
|         - | 13356 | `			/* Recurse and generate bytecodes for function arguments */` |
|   6124307 | 13357 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6124307 | 13358 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13359 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13360 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13361 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   6124307 | 13362 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13363 | `				bFcc = 1;` |
|        81 | 13364 | `				nArgs = 0;` |
|        40 | 13365 | `			}` |
|         - | 13366 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13367 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13368 | `			{` |
|   6124307 | 13369 | `				int seenNamed = 0;` |
|   6124307 | 13370 | `				int seenSpread = 0;` |
|  12884807 | 13371 | `				for( n = 0; n < nArgs; ++n ){` |
|   6760507 | 13372 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4005 | 13373 | `						bAnySpread = 1;` |
|      4005 | 13374 | `						seenSpread = 1;` |
|      4005 | 13375 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13376 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13377 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13378 | `							return SXERR_SYNTAX;` |
|         5 | 13379 | `						}` |
|   6758507 | 13380 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13381 | `						seenNamed = 1;` |
|       289 | 13382 | `						hasNamed = 1;` |
|   6756365 | 13383 | `					}else if( seenNamed ){` |
|         3 | 13384 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13385 | `							"Cannot use positional argument after named argument");` |
|         3 | 13386 | `						return SXERR_SYNTAX;` |
|   6756221 | 13387 | `					}else if( seenSpread ){` |
|       ! 0 | 13388 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13389 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13390 | `						return SXERR_SYNTAX;` |
|         - | 13391 | `					}` |
|   3380255 | 13392 | `				}` |
|         - | 13393 | `			}` |
|         - | 13394 | `			/* Read-only load */` |
|   6124305 | 13395 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13396 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13397 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13398 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13399 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   6124305 | 13400 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   6124305 | 13401 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   6124300 | 13402 | `				if( pCallName->nByte == 5` |
|   3433714 | 13403 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    305657 | 13404 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5971479 | 13405 | `				}else if( pCallName->nByte == 5` |
|   3128062 | 13406 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       109 | 13407 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        52 | 13408 | `				}` |
|         - | 13409 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13410 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13411 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13412 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13413 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13414 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   6124305 | 13415 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13416 | `					SyString sBuiltin;` |
|   6120175 | 13417 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   6120175 | 13418 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3060085 | 13419 | `				}` |
|   3062150 | 13420 | `			}` |
|  12884803 | 13421 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   6760503 | 13422 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   6760503 | 13423 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13424 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13425 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13426 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13427 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13428 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13429 | `				 * (iP1=0 either way). */` |
|   6760503 | 13430 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38245 | 13431 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38245 | 13432 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19120 | 13433 | `				}` |
|   6760503 | 13434 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   6760503 | 13435 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13436 | `					return rc;` |
|         - | 13437 | `				}` |
|         - | 13438 | `				/* Each argument is an independent nullsafe scope. */` |
|   6760503 | 13439 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   6760503 | 13440 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13441 | `					/* Emit spread opcode to unpack this array argument */` |
|      4005 | 13442 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4005 | 13443 | `					hasSpread = 1;` |
|      2000 | 13444 | `				}` |
|   3380254 | 13445 | `			}` |
|         - | 13446 | `			/* Total number of given arguments */` |
|   6124305 | 13447 | `			iP1 = nArgs;` |
|   6124305 | 13448 | `			iP2 = hasSpread;` |
|         - | 13449 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13450 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   6124305 | 13451 | `			if( hasNamed ){` |
|       178 | 13452 | `				sxu32 nStrBytes = 0;` |
|         - | 13453 | `				char *zBuf;` |
|       534 | 13454 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13455 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13456 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13457 | `					}` |
|       182 | 13458 | `				}` |
|         - | 13459 | `				{` |
|       178 | 13460 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13461 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13462 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13463 | `				if( pMap ){` |
|       178 | 13464 | `					SyZero(pMap, mapSize);` |
|       178 | 13465 | `					pMap->bHasNamed = 1;` |
|       178 | 13466 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13467 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13468 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13469 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13470 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13471 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13472 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13473 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13474 | `							zBuf += nb;` |
|       141 | 13475 | `						}` |
|         - | 13476 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13477 | `					}` |
|       178 | 13478 | `					p3 = (void *)pMap;` |
|        87 | 13479 | `				}` |
|         - | 13480 | `				}` |
|        87 | 13481 | `			}` |
|         - | 13482 | `			/* Remove stale flags now */` |
|   6124305 | 13483 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3062150 | 13484 | `		}` |
|         - | 13485 | `		{` |
|         - | 13486 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13487 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13488 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13489 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13490 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13491 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13492 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13493 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  26101037 | 13494 | `			sxi32 iLeftFlags = iFlags;` |
|  26101032 | 13495 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  21466386 | 13496 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   8415896 | 13497 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7295663 | 13498 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2420347 | 13499 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1210171 | 13500 | `			}` |
|         - | 13501 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13502 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13503 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13504 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13505 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13506 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13507 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  26101032 | 13508 | `			if( pNode->pOp` |
|  36864559 | 13509 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  23814090 | 13510 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  21527096 | 13511 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4941245 | 13512 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2470620 | 13513 | `			}` |
|         - | 13514 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13515 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13516 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13517 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13518 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13519 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  26101032 | 13520 | `			if( pNode->pOp` |
|  26101037 | 13521 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    195119 | 13522 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     97557 | 13523 | `			}` |
|  26101037 | 13524 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13525 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13526 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     11661 | 13527 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      5828 | 13528 | `			}` |
|  26101037 | 13529 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13530 | `		}` |
|  26101037 | 13531 | `		if( rc != SXRET_OK ){` |
|        34 | 13532 | `			return rc;` |
|         - | 13533 | `		}` |
|  26101007 | 13534 | `		if( !bIsChainOp ){` |
|         - | 13535 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13536 | `			 * target the end of that LHS chain, which is right here. */` |
|  12189213 | 13537 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6094604 | 13538 | `		}` |
|  26101007 | 13539 | `		if( iVmOp == PH7_OP_CALL ){` |
|   6124305 | 13540 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6124305 | 13541 | `			if( pInstr ){` |
|   6124305 | 13542 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   4501507 | 13543 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13544 | `					sxu32 nQual;` |
|   4501507 | 13545 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13546 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13547 | `					 * so the later NEW handler (if any) can see it. */` |
|   4501507 | 13548 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13549 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13550 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13551 | `					 * imports — class imports must NOT affect function` |
|         - | 13552 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13553 | `					 * before NEW; we store the original literal index in the` |
|         - | 13554 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13555 | `					 * the unqualified name and re-qualify with class imports. */` |
|   4501507 | 13556 | `					if( bAbsolute ){` |
|      3851 | 13557 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1928 | 13558 | `					}else{` |
|   4497661 | 13559 | `						int fromImport = 0;` |
|   4497661 | 13560 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   4497661 | 13561 | `						pInstr->iP2 = (sxi32)nQual;` |
|   4497661 | 13562 | `						if( nQual != nOrig ){` |
|         - | 13563 | `							/* Record the original literal index in the arg map` |
|         - | 13564 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13565 | `							 * flag) so the NEW handler can recover the` |
|         - | 13566 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13567 | `							 * imports. */` |
|        81 | 13568 | `							if( p3 == 0 ){` |
|        81 | 13569 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        76 | 13570 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        81 | 13571 | `								if( pMap ){` |
|        81 | 13572 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        81 | 13573 | `									p3 = (void *)pMap;` |
|        38 | 13574 | `								}` |
|        38 | 13575 | `							}` |
|        81 | 13576 | `							if( p3 ){` |
|        81 | 13577 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        81 | 13578 | `								if( !fromImport ){` |
|         - | 13579 | `									/* Mark as namespace-qualified */` |
|        71 | 13580 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        33 | 13581 | `								}` |
|        38 | 13582 | `							}` |
|        38 | 13583 | `						}` |
|         5 | 13584 | `					}` |
|   3873554 | 13585 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13586 | `					/* Method call,flag that */` |
|   1603065 | 13587 | `					pInstr->iP2 = 1;` |
|    801530 | 13588 | `				}` |
|   3062155 | 13589 | `			}` |
|  23038857 | 13590 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13591 | `			ph7_expr_node **apNode;` |
|         - | 13592 | `			sxi32 n;` |
|   2846259 | 13593 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13594 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13595 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13596 | `			/* Recurse and generate bytecodes for array index */` |
|   2846259 | 13597 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5474607 | 13598 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2628353 | 13599 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2628353 | 13600 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2628353 | 13601 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13602 | `					return rc;` |
|         - | 13603 | `				}` |
|         - | 13604 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2628353 | 13605 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1314179 | 13606 | `			}` |
|   2846259 | 13607 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2628353 | 13608 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1314174 | 13609 | `			}` |
|   2846259 | 13610 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13611 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    343701 | 13612 | `				iP2 = 4;` |
|   2674411 | 13613 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13614 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13615 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     22977 | 13616 | `				iP2 = 5;` |
|   2491077 | 13617 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13618 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13619 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13620 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        31 | 13621 | `				iP2 = 6;` |
|   2479578 | 13622 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13623 | `				/* Create an empty entry when the desired index is not found */` |
|    519915 | 13624 | `				iP2 = 1;` |
|    259960 | 13625 | `			}` |
|  18553580 | 13626 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13627 | `			/* POP the left node */` |
|         5 | 13628 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13629 | `		}` |
|  13050501 | 13630 | `	}` |
|  26123901 | 13631 | `	rc = SXRET_OK;` |
|  26123901 | 13632 | `	nJmpIdx = 0;` |
|         - | 13633 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13634 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13635 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  26123901 | 13636 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    390151 | 13637 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    390151 | 13638 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    390151 | 13639 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    390151 | 13640 | `			int isSpecial = 0;` |
|    390151 | 13641 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    344335 | 13642 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    344335 | 13643 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    344330 | 13644 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    313738 | 13645 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    170227 | 13646 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    103181 | 13647 | `					isSpecial = 1;` |
|     51588 | 13648 | `				}` |
|    183619 | 13649 | `			}` |
|    413059 | 13650 | `			pInstr->iP1 = 0;` |
|         - | 13651 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 13652 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 13653 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 13654 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 13655 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 13656 | `			{` |
|    596678 | 13657 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    550857 | 13658 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    367243 | 13659 | `				if( !isSpecial && !bAbsolute ){` |
|    264051 | 13660 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    132023 | 13661 | `				}` |
|         - | 13662 | `			}` |
|         - | 13663 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13664 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    367243 | 13665 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    264067 | 13666 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    264067 | 13667 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        68 | 13668 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        70 | 13669 | `					return SXRET_OK;` |
|         - | 13670 | `				}` |
|    131998 | 13671 | `			}` |
|    183586 | 13672 | `		}` |
|    229381 | 13673 | `	}` |
|         - | 13674 | `	/* Generate code for the right tree */` |
|  26100941 | 13675 | `	if( pNode->pRight ){` |
|  14999463 | 13676 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13677 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    412657 | 13678 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14793137 | 13679 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13680 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    282555 | 13681 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14445536 | 13682 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13683 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     57401 | 13684 | `			iVmOp = 0; /* No binary operator to emit */` |
|     57401 | 13685 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  14275615 | 13686 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13687 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13688 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13689 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13690 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13691 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13692 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13693 | `			sxu32 nNsJmp = 0;` |
|       108 | 13694 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13695 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  14246813 | 13696 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13697 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13698 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13699 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4762991 | 13700 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2381493 | 13701 | `		}` |
|  14999463 | 13702 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  14999463 | 13703 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  14999463 | 13704 | `		if( !bIsChainOp ){` |
|         - | 13705 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13706 | `			 * operator instruction is emitted. */` |
|  10058289 | 13707 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5029142 | 13708 | `		}` |
|  14999463 | 13709 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4327637 | 13710 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4327600 | 13711 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13712 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13713 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13714 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13715 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13716 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13717 | `				 */` |
|        91 | 13718 | `				iVmOp = 0;` |
|   4327594 | 13719 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4327551 | 13720 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13721 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    779181 | 13722 | `					iP2 = 1;` |
|    389593 | 13723 | `				}else{` |
|   3548375 | 13724 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13725 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    500737 | 13726 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    500737 | 13727 | `						iP1 = pInstr->iP1;` |
|    250371 | 13728 | `					}else{` |
|   3047643 | 13729 | `						p3 = pInstr->p3;` |
|         - | 13730 | `					}` |
|         - | 13731 | `					/* POP the last dynamic load instruction */` |
|   3548375 | 13732 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13733 | `				}` |
|   2163778 | 13734 | `			}` |
|  12835647 | 13735 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13736 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13737 | `			if( pInstr ){` |
|        63 | 13738 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13739 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13740 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13741 | `					 */` |
|        19 | 13742 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13743 | `					iP1 = pInstr->iP1;` |
|        19 | 13744 | `					iP2 = pInstr->iP2;` |
|        19 | 13745 | `					p3  = pInstr->p3;` |
|        10 | 13746 | `				}else{` |
|        45 | 13747 | `					p3 = pInstr->p3;` |
|         - | 13748 | `				}` |
|        30 | 13749 | `			}` |
|        30 | 13750 | `		}` |
|   7499729 | 13751 | `	}` |
|  26100936 | 13752 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    375626 | 13753 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13754 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13755 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13756 | `		iVmOp = 0;` |
|        14 | 13757 | `	}` |
|  26100941 | 13758 | `	if( iVmOp > 0 ){` |
|  26043427 | 13759 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    195119 | 13760 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13761 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15301 | 13762 | `				iP1 = 1;` |
|      7653 | 13763 | `			}` |
|  25945870 | 13764 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13765 | `			/* Namespace-qualify the class name for NEW */ {` |
|    750879 | 13766 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    750879 | 13767 | `				VmInstr *pCallInstr = 0;` |
|    750879 | 13768 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    750567 | 13769 | `					pCallInstr = pPeek;` |
|    750567 | 13770 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    375281 | 13771 | `				}` |
|    750879 | 13772 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    735611 | 13773 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13774 | `					sxu32 nLitForClass;` |
|    735611 | 13775 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13776 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13777 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13778 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13779 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13780 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13781 | `					 * with class imports. */` |
|    735611 | 13782 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        37 | 13783 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        21 | 13784 | `					}else{` |
|    735579 | 13785 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13786 | `					}` |
|    735611 | 13787 | `					pPeek->iP1 = 0;` |
|    735611 | 13788 | `					if( !bAbsolute ){` |
|    731771 | 13789 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    365888 | 13790 | `					}else{` |
|      3845 | 13791 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13792 | `					}` |
|    367803 | 13793 | `				}` |
|         - | 13794 | `			}` |
|    750879 | 13795 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    750879 | 13796 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13797 | `				VmInstr *pPrev;` |
|    750567 | 13798 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    750567 | 13799 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13800 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13801 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13802 | `					 * accumulator exactly like OP_CALL would have). */` |
|    750567 | 13803 | `					iP1 = pInstr->iP1;` |
|    750567 | 13804 | `					iP2 = pInstr->iP2;` |
|    750567 | 13805 | `					if( pInstr->p3 ){` |
|        47 | 13806 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        21 | 13807 | `					}` |
|    750567 | 13808 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    375281 | 13809 | `				}` |
|    375286 | 13810 | `			}` |
|  25472876 | 13811 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13812 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13813 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     76581 | 13814 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     76581 | 13815 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     76581 | 13816 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     76581 | 13817 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     76581 | 13818 | `				int isSpecialIs = 0;` |
|     76581 | 13819 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     76581 | 13820 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     76581 | 13821 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     76576 | 13822 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     76579 | 13823 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     38288 | 13824 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13825 | `						isSpecialIs = 1;` |
|         5 | 13826 | `					}` |
|     38288 | 13827 | `				}` |
|     76581 | 13828 | `				pInstr->iP1 = 0;` |
|     76581 | 13829 | `				if( !isSpecialIs && !bAbsolute ){` |
|     76561 | 13830 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     38278 | 13831 | `				}` |
|     38293 | 13832 | `			}` |
|  25059151 | 13833 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13834 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13835 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13836 | `			 * should not trigger constant lookup. */` |
|   4941179 | 13837 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4941179 | 13838 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4708323 | 13839 | `				pInstr->iP1 = 0;` |
|   2354159 | 13840 | `			}` |
|   4941179 | 13841 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13842 | `				/* Static member access,remember that */` |
|    367191 | 13843 | `				iP1 = 1;` |
|    367191 | 13844 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    367191 | 13845 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    229029 | 13846 | `					p3 = pInstr->p3;` |
|    229029 | 13847 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    114512 | 13848 | `				}` |
|    183593 | 13849 | `			}` |
|         - | 13850 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13851 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13852 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13853 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4941179 | 13854 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4941179 | 13855 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13856 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4941159 | 13857 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     61159 | 13858 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4910562 | 13859 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13860 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4879977 | 13861 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13862 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    947269 | 13863 | `					iP2 = PH7_MEMBER_WRITE;` |
|    473632 | 13864 | `				}` |
|   2470587 | 13865 | `			}` |
|   2470587 | 13866 | `		}` |
|         - | 13867 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13868 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13869 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13870 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13871 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  26043427 | 13872 | `		if( bFcc ){` |
|        81 | 13873 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13874 | `			iP2 = 0;` |
|        81 | 13875 | `			p3 = 0;` |
|        81 | 13876 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13877 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13878 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13879 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13880 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13881 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13882 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13883 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13884 | `				if( pMemberName ){` |
|         3 | 13885 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13886 | `				}` |
|        37 | 13887 | `				iP1 = 2;` |
|        19 | 13888 | `			}else{` |
|        45 | 13889 | `				iP1 = 1;` |
|         - | 13890 | `			}` |
|        40 | 13891 | `		}` |
|         - | 13892 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13893 | `		 * This is the primary emit path for user-visible calls. */` |
|  26043427 | 13894 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6875099 | 13895 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3437547 | 13896 | `		}` |
|         - | 13897 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  26043427 | 13898 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  13021711 | 13899 | `	}` |
|  26100941 | 13900 | `	if( nJmpIdx > 0 ){` |
|         - | 13901 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    752603 | 13902 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    752603 | 13903 | `		if( pInstr ){` |
|    752603 | 13904 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    376299 | 13905 | `		}` |
|    376299 | 13906 | `	}` |
|  26100941 | 13907 | `	return rc;` |
|  33442059 | 13908 | `}` |
|         - | 13909 | `/*` |
|         - | 13910 | ` * Compile a PHP expression.` |
|         - | 13911 | ` * According to the PHP language reference manual:` |
|         - | 13912 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 13913 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 13914 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 13915 | ` *  is "anything that has a value".` |
|         - | 13916 | ` * If something goes wrong while compiling the expression,this` |
|         - | 13917 | ` * function takes care of generating the appropriate error` |
|         - | 13918 | ` * message.` |
|         - | 13919 | ` */` |
|         - | 13920 | `/*` |
|         - | 13921 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 13922 | ` *` |
|         - | 13923 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 13924 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 13925 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 13926 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 13927 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 13928 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 13929 | ` * except for() now reports php's parse error.` |
|         - | 13930 | ` */` |
| 221629548 | 13931 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 13932 | `{` |
|         - | 13933 | `	ph7_expr_node **apArg;` |
|         - | 13934 | `	sxu32 n;` |
| 221629553 | 13935 | `	if( pNode == 0 ){` |
| 155776769 | 13936 | `		return 0;` |
|         - | 13937 | `	}` |
|  65852789 | 13938 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 13939 | `		return 1;` |
|         - | 13940 | `	}` |
|  65852780 | 13941 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  65852781 | 13942 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 13943 | `		return 1;` |
|         - | 13944 | `	}` |
|  65852781 | 13945 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  75218811 | 13946 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   9366035 | 13947 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 13948 | `			return 1;` |
|         - | 13949 | `		}` |
|   4683020 | 13950 | `	}` |
|  65852781 | 13951 | `	return 0;` |
| 110814779 | 13952 | `}` |
|  15060696 | 13953 | `static sxi32 PH7_CompileExpr(` |
|         - | 13954 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 13955 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 13956 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 13957 | `	)` |
|         5 | 13958 | `{` |
|         - | 13959 | `	ph7_expr_node *pRoot;` |
|         - | 13960 | `	SySet sExprNode;` |
|         - | 13961 | `	SyToken *pEnd;` |
|         - | 13962 | `	sxi32 nExpr;` |
|         - | 13963 | `	sxi32 iNest;` |
|         - | 13964 | `	sxi32 rc;` |
|         - | 13965 | `	sxu32 nNullsafeBase;` |
|         - | 13966 | `	/* Initialize worker variables */` |
|  15060701 | 13967 | `	nExpr = 0;` |
|  15060701 | 13968 | `	pRoot = 0;` |
|         - | 13969 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 13970 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  15060701 | 13971 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15060701 | 13972 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  15060701 | 13973 | `	SySetAlloc(&sExprNode,0x10);` |
|  15060701 | 13974 | `	rc = SXRET_OK;` |
|         - | 13975 | `	/* Delimit the expression */` |
|  15060701 | 13976 | `	pEnd = pGen->pIn;` |
|  15060701 | 13977 | `	iNest = 0;` |
| 118094371 | 13978 | `	while( pEnd < pGen->pEnd ){` |
| 112278815 | 13979 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 13980 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4637 | 13981 | `			iNest++;` |
| 112276499 | 13982 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4645 | 13983 | `			iNest--;` |
| 112271863 | 13984 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   9245999 | 13985 | `			if( iNest <= 0 ){` |
|   9245145 | 13986 | `				break;` |
|         - | 13987 | `			}` |
|       427 | 13988 | `		}` |
| 103033675 | 13989 | `		pEnd++;` |
|         5 | 13990 | `	}` |
|  15060701 | 13991 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    642111 | 13992 | `		SyToken *pEnd2 = pGen->pIn;` |
|    642111 | 13993 | `		iNest = 0;` |
|         - | 13994 | `		/* Stop at the first comma */` |
|   1411425 | 13995 | `		while( pEnd2 < pEnd ){` |
|    769321 | 13996 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     42091 | 13997 | `				iNest++;` |
|    748278 | 13998 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     42091 | 13999 | `				iNest--;` |
|    706192 | 14000 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6057 | 14001 | `				if( iNest <= 0 ){` |
|         3 | 14002 | `					break;` |
|         - | 14003 | `				}` |
|      3025 | 14004 | `			}` |
|    769319 | 14005 | `			pEnd2++;` |
|         5 | 14006 | `		}` |
|    642111 | 14007 | `		if( pEnd2 <pEnd ){` |
|         3 | 14008 | `			pEnd = pEnd2;` |
|         1 | 14009 | `		}` |
|    321053 | 14010 | `	}` |
|  15060701 | 14011 | `	if( pEnd > pGen->pIn ){` |
|  15037797 | 14012 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 14013 | `		/* Swap delimiter */` |
|  15037797 | 14014 | `		pGen->pEnd = pEnd;` |
|         - | 14015 | `		/* Try to get an expression tree */` |
|  15037797 | 14016 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  15037792 | 14017 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  14871402 | 14018 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 14019 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 14020 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 14021 | `				"syntax error, unexpected token \",\"");` |
|         6 | 14022 | `			pGen->pEnd = pTmp;` |
|         6 | 14023 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14024 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 14025 | `				return SXERR_ABORT;` |
|         - | 14026 | `			}` |
|         6 | 14027 | `			pGen->pIn = pEnd;` |
|         6 | 14028 | `			SySetRelease(&sExprNode);` |
|         6 | 14029 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 14030 | `			return SXRET_OK;` |
|         - | 14031 | `		}` |
|  15037793 | 14032 | `		if( rc == SXRET_OK && pRoot ){` |
|  15037609 | 14033 | `			rc = SXRET_OK;` |
|  15037609 | 14034 | `			if( xTreeValidator ){` |
|         - | 14035 | `				/* Call the upper layer validator callback */` |
|    967265 | 14036 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    483630 | 14037 | `			}` |
|  15037609 | 14038 | `			if( rc != SXERR_ABORT ){` |
|         - | 14039 | `				/* Generate code for the given tree */` |
|  15037609 | 14040 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 14041 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 14042 | `				 * expression so they short-circuit to its end. */` |
|  15037609 | 14043 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   7518802 | 14044 | `			}` |
|  15037609 | 14045 | `			nExpr = 1;` |
|   7518802 | 14046 | `		}` |
|         - | 14047 | `		/* Release the whole tree */` |
|  15037793 | 14048 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 14049 | `		/* Synchronize token stream */` |
|  15037793 | 14050 | `		pGen->pEnd = pTmp;` |
|  15037793 | 14051 | `		pGen->pIn  = pEnd;` |
|  15037793 | 14052 | `		if( rc == SXERR_ABORT ){` |
|        12 | 14053 | `			SySetRelease(&sExprNode);` |
|        12 | 14054 | `			return SXERR_ABORT;` |
|         - | 14055 | `		}` |
|   7518889 | 14056 | `	}` |
|  15060687 | 14057 | `	SySetRelease(&sExprNode);` |
|  15060687 | 14058 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   7530353 | 14059 | `}` |
|         - | 14060 | `/*` |
|         - | 14061 | ` * Return a pointer to the node construct handler associated` |
|         - | 14062 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 14063 | ` */` |
|   8699298 | 14064 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 14065 | `{` |
|   8699303 | 14066 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 14067 | `		/* Numeric literal: Either real or integer */` |
|   3556163 | 14068 | `		return PH7_CompileNumLiteral;` |
|   5143145 | 14069 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 14070 | `		/* Double quoted string */` |
|    119877 | 14071 | `		return PH7_CompileString;` |
|   5023273 | 14072 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 14073 | `		/* Single quoted string */` |
|   5023153 | 14074 | `		return PH7_CompileSimpleString;` |
|       124 | 14075 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 14076 | `		/* Heredoc */` |
|        70 | 14077 | `		return PH7_CompileHereDoc;` |
|        58 | 14078 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 14079 | `		/* Nowdoc */` |
|        52 | 14080 | `		return PH7_CompileNowDoc;` |
|         8 | 14081 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 14082 | `		/* Backtick quoted string */` |
|         6 | 14083 | `		return PH7_CompileBacktic;` |
|         - | 14084 | `	}` |
|         3 | 14085 | `	return 0;` |
|   4349654 | 14086 | `}` |
|         - | 14087 | `/*` |
|         - | 14088 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 14089 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 14090 | ` * in write context" parse error.` |
|         - | 14091 | ` */` |
|     23014 | 14092 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 14093 | `{` |
|         - | 14094 | `	sxi32 rc;` |
|     23019 | 14095 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23017 | 14096 | `		return SXRET_OK;` |
|         - | 14097 | `	}` |
|         5 | 14098 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 14099 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 14100 | `		"Can't use nullsafe operator in write context");` |
|         3 | 14101 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11512 | 14102 | `}` |
|         - | 14103 | `/*` |
|         - | 14104 | ` * Compile an unset() statement.` |
|         - | 14105 | ` * unset($var, $arr[$key], ...);` |
|         - | 14106 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 14107 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 14108 | ` * parent array before extracting the element to unset.` |
|         - | 14109 | ` */` |
|     25864 | 14110 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 14111 | `{` |
|     25869 | 14112 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     25869 | 14113 | `	sxu32 nIdx = 0;` |
|         - | 14114 | `	SyString sName;` |
|         - | 14115 | `	sxi32 rc;` |
|         - | 14116 | `	/* Jump the 'unset' keyword */` |
|     25869 | 14117 | `	pGen->pIn++;` |
|         - | 14118 | `	/* Save delimiter */` |
|     25869 | 14119 | `	pTmp = pGen->pEnd;` |
|         - | 14120 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     25869 | 14121 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     25869 | 14122 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14123 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 14124 | `		SyToken *pClose;` |
|     25869 | 14125 | `		pGen->pIn++;   /* Skip '(' */` |
|     25869 | 14126 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     25869 | 14127 | `		pEnd = pClose; /* Stop at ')' */` |
|     12932 | 14128 | `	}` |
|     25869 | 14129 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 14130 | `	/* Resolve the 'unset' builtin name once */` |
|     25869 | 14131 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3821 | 14132 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3821 | 14133 | `		if( pObj == 0 ){` |
|       ! 0 | 14134 | `			return SXERR_ABORT;` |
|         - | 14135 | `		}` |
|      3821 | 14136 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3821 | 14137 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1908 | 14138 | `	}` |
|         - | 14139 | `	/* Compile each comma-separated argument */` |
|     55959 | 14140 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30095 | 14141 | `		if( pGen->pIn < pNext ){` |
|         - | 14142 | `			/*` |
|         - | 14143 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 14144 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 14145 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 14146 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 14147 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 14148 | `			 * already removes just the element/property.` |
|         - | 14149 | `			 */` |
|     30090 | 14150 | `			if( &pGen->pIn[2] == pNext` |
|     18583 | 14151 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      7081 | 14152 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 14153 | `				SyString *pVarName;` |
|     10616 | 14154 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      7074 | 14155 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      7079 | 14156 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      7079 | 14157 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 14158 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 14159 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 14160 | `					return SXERR_ABORT;` |
|         - | 14161 | `				}` |
|      7079 | 14162 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      7079 | 14163 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      7079 | 14164 | `				pGen->pIn = pNext;` |
|      7079 | 14165 | `				if( pGen->pIn < pEnd ){` |
|      4227 | 14166 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2111 | 14167 | `				}` |
|      7079 | 14168 | `				continue;` |
|         - | 14169 | `			}` |
|     23021 | 14170 | `			pGen->pEnd = pNext;` |
|     23021 | 14171 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 14172 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 14173 | `				GenStateUnsetValidator);` |
|     23021 | 14174 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14175 | `				return SXERR_ABORT;` |
|         - | 14176 | `			}` |
|     23021 | 14177 | `			if( rc != SXERR_EMPTY ){` |
|         - | 14178 | `				/* Emit call for this single argument */` |
|     23019 | 14179 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23019 | 14180 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23019 | 14181 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11507 | 14182 | `			}` |
|     11508 | 14183 | `		}` |
|         - | 14184 | `		/* Jump trailing commas */` |
|     23027 | 14185 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 14186 | `			pNext++;` |
|         1 | 14187 | `		}` |
|     23021 | 14188 | `		pGen->pIn = pNext;` |
|         5 | 14189 | `	}` |
|         - | 14190 | `	/* Skip past the closing ')' if present */` |
|     25869 | 14191 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     25869 | 14192 | `		pGen->pIn++;` |
|     12932 | 14193 | `	}` |
|         - | 14194 | `	/* Restore token stream */` |
|     25869 | 14195 | `	pGen->pEnd = pTmp;` |
|     25869 | 14196 | `	return SXRET_OK;` |
|     12937 | 14197 | `}` |
|         - | 14198 | `/*` |
|         - | 14199 | ` * PHP Language construct table.` |
|         - | 14200 | ` */` |
|         - | 14201 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 14202 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 14203 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14204 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14205 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14206 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14207 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14208 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14209 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14210 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14211 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14212 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14213 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14214 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14215 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14216 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14217 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14218 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14219 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14220 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14221 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14222 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14223 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14224 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14225 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14226 | `};` |
|         - | 14227 | `/*` |
|         - | 14228 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14229 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14230 | ` */` |
|   7302140 | 14231 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14232 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14233 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14234 | `	)` |
|         5 | 14235 | `{` |
|   7302145 | 14236 | `	sxu32 n = 0;` |
|  28939574 | 14237 | `	for(;;){` |
|  57879153 | 14238 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    429719 | 14239 | `			break;` |
|         - | 14240 | `		}` |
|  57449439 | 14241 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6872431 | 14242 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14243 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14244 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14245 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14246 | `					return 0;` |
|         - | 14247 | `				}` |
|       ! 0 | 14248 | `			}` |
|   6872426 | 14249 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|      7646 | 14250 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      3830 | 14251 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14252 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14253 | `				return 0;` |
|         - | 14254 | `			}` |
|         - | 14255 | `			/* Return a pointer to the handler.` |
|         - | 14256 | `			*/` |
|   6872429 | 14257 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14258 | `		}` |
|  50577013 | 14259 | `		n++;` |
|         5 | 14260 | `	}` |
|    429719 | 14261 | `	if( pLookahed ){` |
|    429719 | 14262 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     68801 | 14263 | `			return PH7_CompileClassInterface;` |
|    360923 | 14264 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    314577 | 14265 | `			return PH7_CompileClass;` |
|     46351 | 14266 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7715 | 14267 | `			return PH7_CompileTrait;` |
|         - | 14268 | `		}` |
|         - | 14269 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14270 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14271 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14272 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19318 | 14273 | `	}` |
|         - | 14274 | `	/* Not a language construct */` |
|     38641 | 14275 | `	return 0;` |
|   3651075 | 14276 | `}` |
|         - | 14277 | `/*` |
|         - | 14278 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14279 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14280 | ` */` |
|     38638 | 14281 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14282 | `{` |
|         - | 14283 | `	int rc;` |
|     38643 | 14284 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38643 | 14285 | `	if( rc == FALSE ){` |
|     38534 | 14286 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15630 | 14287 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14288 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14289 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14290 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14291 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14292 | `			*/` |
|         - | 14293 | `			){` |
|     38531 | 14294 | `				rc = TRUE;` |
|     19263 | 14295 | `		}` |
|     19267 | 14296 | `	}` |
|     38643 | 14297 | `	return rc;` |
|         5 | 14298 | `}` |
|         - | 14299 | `/*` |
|         - | 14300 | ` * Compile a PHP chunk.` |
|         - | 14301 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14302 | ` * takes care of generating the appropriate error message.` |
|         - | 14303 | ` */` |
|         - | 14304 | `/*` |
|         - | 14305 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14306 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14307 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14308 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14309 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14310 | ` * intervening non-declaration statements.` |
|         - | 14311 | ` */` |
|  15771990 | 14312 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14313 | `{` |
|  15771995 | 14314 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  15771995 | 14315 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  15771995 | 14316 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14317 | `	sxu32 nIdx, n;` |
|  15771990 | 14318 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3261773 | 14319 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14320 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14321 | `		 * indexes do not map to the sidecar */` |
|  12510229 | 14322 | `		return;` |
|         - | 14323 | `	}` |
|   3261771 | 14324 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14325 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14326 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3261771 | 14327 | `	SySetReset(&pGen->aPendingAttrs);` |
|   9786797 | 14328 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6525031 | 14329 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6517235 | 14330 | `			continue;` |
|         - | 14331 | `		}` |
|      7801 | 14332 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14333 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7789 | 14334 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7777 | 14335 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3886 | 14336 | `		}` |
|      3903 | 14337 | `	}` |
|   7886000 | 14338 | `}` |
|         - | 14339 | `/*` |
|         - | 14340 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14341 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14342 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14343 | ` */` |
|   4059472 | 14344 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14345 | `{` |
|         - | 14346 | `	char *zDup;` |
|   4059477 | 14347 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4059457 | 14348 | `		return;` |
|         - | 14349 | `	}` |
|        35 | 14350 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14351 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14352 | `	if( zDup ){` |
|        25 | 14353 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14354 | `	}` |
|        25 | 14355 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2029741 | 14356 | `}` |
|         - | 14357 | `/*` |
|         - | 14358 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14359 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14360 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14361 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14362 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14363 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14364 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14365 | ` */` |
|      7784 | 14366 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14367 | `{` |
|         - | 14368 | `	SySet *pToken;` |
|         - | 14369 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14370 | `	char *zSpan;` |
|      7789 | 14371 | `	sxi32 rc = SXRET_OK;` |
|      7789 | 14372 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14373 | `		return SXRET_OK;` |
|         - | 14374 | `	}` |
|     11681 | 14375 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3892 | 14376 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7789 | 14377 | `	if( zSpan == 0 ){` |
|       ! 0 | 14378 | `		return SXRET_OK;` |
|         - | 14379 | `	}` |
|         - | 14380 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14381 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14382 | `	 * the number of attribute declarations in the program. */` |
|      7789 | 14383 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7789 | 14384 | `	if( pToken == 0 ){` |
|       ! 0 | 14385 | `		return SXRET_OK;` |
|         - | 14386 | `	}` |
|      7789 | 14387 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7789 | 14388 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7789 | 14389 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7789 | 14390 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7789 | 14391 | `	pSavedIn = pGen->pIn;` |
|      7789 | 14392 | `	pSavedEnd = pGen->pEnd;` |
|      7793 | 14393 | `	while( pIn < pEnd ){` |
|         - | 14394 | `		ph7_attribute sAttr;` |
|         - | 14395 | `		SyBlob sFQN;` |
|      7793 | 14396 | `		int bAbsolute = 0;` |
|      7793 | 14397 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7793 | 14398 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7793 | 14399 | `		sAttr.nLine = pIn->nLine;` |
|      7793 | 14400 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14401 | `			bAbsolute = 1;` |
|        75 | 14402 | `			pIn++;` |
|        35 | 14403 | `		}` |
|      7793 | 14404 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7793 | 14405 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7793 | 14406 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7793 | 14407 | `			pIn++;` |
|      7793 | 14408 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14409 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14410 | `				pIn++;` |
|       ! 0 | 14411 | `				continue;` |
|         - | 14412 | `			}` |
|      7793 | 14413 | `			break;` |
|       ! 0 | 14414 | `		}` |
|      7793 | 14415 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14416 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14417 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14418 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14419 | `			break;` |
|         - | 14420 | `		}` |
|         - | 14421 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14422 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14423 | `		{` |
|      7793 | 14424 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7793 | 14425 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7793 | 14426 | `			char *zDup = 0;` |
|      7793 | 14427 | `			if( !bAbsolute ){` |
|      7723 | 14428 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7723 | 14429 | `				if( pImp ){` |
|       ! 0 | 14430 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14431 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14432 | `					if( zDup ){` |
|       ! 0 | 14433 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14434 | `					}` |
|      7723 | 14435 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14436 | `					SyBlob sTmp;` |
|       ! 0 | 14437 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14438 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14439 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14440 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14441 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14442 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14443 | `					if( zDup ){` |
|       ! 0 | 14444 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14445 | `					}` |
|       ! 0 | 14446 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14447 | `				}` |
|      3859 | 14448 | `			}` |
|      7793 | 14449 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7793 | 14450 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7793 | 14451 | `				if( zDup ){` |
|      7793 | 14452 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3894 | 14453 | `				}` |
|      3894 | 14454 | `			}` |
|         - | 14455 | `		}` |
|      7793 | 14456 | `		SyBlobRelease(&sFQN);` |
|      7793 | 14457 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14458 | `			SyToken *pArgsEnd;` |
|      7691 | 14459 | `			pIn++;` |
|      7691 | 14460 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15391 | 14461 | `			while( pIn < pArgsEnd ){` |
|      7705 | 14462 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7705 | 14463 | `				sxi32 iDepth = 0;` |
|         - | 14464 | `				ph7_attr_arg sArgRec;` |
|     76565 | 14465 | `				while( pArgStop < pArgsEnd ){` |
|     68881 | 14466 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14467 | `						iDepth++;` |
|     68876 | 14468 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14469 | `						iDepth--;` |
|     68866 | 14470 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14471 | `						break;` |
|         - | 14472 | `					}` |
|     68865 | 14473 | `					pArgStop++;` |
|         5 | 14474 | `				}` |
|      7705 | 14475 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7705 | 14476 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7700 | 14477 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7684 | 14478 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14479 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14480 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14481 | `					if( zN ){` |
|        19 | 14482 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14483 | `					}` |
|        19 | 14484 | `					pArgStart += 2;` |
|         9 | 14485 | `				}` |
|      7705 | 14486 | `				if( pArgStart < pArgStop ){` |
|         - | 14487 | `					SySet *pInstrContainer;` |
|      7705 | 14488 | `					pGen->pIn = pArgStart;` |
|      7705 | 14489 | `					pGen->pEnd = pArgStop;` |
|      7705 | 14490 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7705 | 14491 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7705 | 14492 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7705 | 14493 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7705 | 14494 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7705 | 14495 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14496 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14497 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14498 | `						return SXERR_ABORT;` |
|         - | 14499 | `					}` |
|      7705 | 14500 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3850 | 14501 | `				}` |
|      7705 | 14502 | `				pIn = pArgStop;` |
|      7705 | 14503 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14504 | `					pIn++;` |
|         8 | 14505 | `				}` |
|         5 | 14506 | `			}` |
|      7691 | 14507 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3843 | 14508 | `		}` |
|      7793 | 14509 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7793 | 14510 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14511 | `			pIn++;` |
|         5 | 14512 | `			continue;` |
|         - | 14513 | `		}` |
|      7789 | 14514 | `		break;` |
|       ! 0 | 14515 | `	}` |
|      7789 | 14516 | `	pGen->pIn = pSavedIn;` |
|      7789 | 14517 | `	pGen->pEnd = pSavedEnd;` |
|      7789 | 14518 | `	return SXRET_OK;` |
|      3897 | 14519 | `}` |
|         - | 14520 | `/*` |
|         - | 14521 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14522 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14523 | ` */` |
|   4059476 | 14524 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14525 | `{` |
|   4059481 | 14526 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14527 | `	sxu32 n;` |
|         - | 14528 | `	sxi32 rc;` |
|   4067253 | 14529 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7777 | 14530 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7777 | 14531 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14532 | `			return SXERR_ABORT;` |
|         - | 14533 | `		}` |
|      3891 | 14534 | `	}` |
|   4059481 | 14535 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4059481 | 14536 | `	return SXRET_OK;` |
|   2029743 | 14537 | `}` |
|         - | 14538 | `/*` |
|         - | 14539 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14540 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14541 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14542 | ` */` |
|   2045578 | 14543 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14544 | `{` |
|   2045583 | 14545 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2045583 | 14546 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2045583 | 14547 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14548 | `	sxu32 nIdx, n;` |
|         - | 14549 | `	sxi32 rc;` |
|   2045578 | 14550 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    542207 | 14551 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1503381 | 14552 | `		return SXRET_OK;` |
|         - | 14553 | `	}` |
|    542207 | 14554 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1626609 | 14555 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1084407 | 14556 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14557 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14558 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14559 | `				return SXERR_ABORT;` |
|         - | 14560 | `			}` |
|         6 | 14561 | `		}` |
|    542206 | 14562 | `	}` |
|    542207 | 14563 | `	return SXRET_OK;` |
|   1022794 | 14564 | `}` |
|  11738846 | 14565 | `static sxi32 GenStateCompileChunk(` |
|         - | 14566 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14567 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14568 | `	)` |
|         5 | 14569 | `{` |
|         - | 14570 | `	ProcLangConstruct xCons;` |
|         - | 14571 | `	sxi32 rc;` |
|  11738851 | 14572 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   6815181 | 14573 | `	for(;;){` |
|  12684609 | 14574 | `		int bStmtIsDeclare = 0;` |
|  12684609 | 14575 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14576 | `			/* No more input to process */` |
|     67493 | 14577 | `			break;` |
|         - | 14578 | `		}` |
|         - | 14579 | `		/* Bind a directly-preceding docblock to this statement */` |
|  12617121 | 14580 | `		GenStateSetPendingDoc(&(*pGen));` |
|  12617121 | 14581 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14582 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14583 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14584 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14585 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14586 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7695 | 14587 | `			int bAttrTarget = 0;` |
|      7690 | 14588 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3879 | 14589 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7637 | 14590 | `				bAttrTarget = 1;` |
|      3875 | 14591 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14592 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14593 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14594 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14595 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14596 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14597 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14598 | `					bAttrTarget = 1;` |
|        29 | 14599 | `				}` |
|        29 | 14600 | `			}` |
|      7695 | 14601 | `			if( !bAttrTarget ){` |
|       ! 0 | 14602 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14603 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14604 | `					&pGen->pIn->sData);` |
|       ! 0 | 14605 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14606 | `					break;` |
|         - | 14607 | `				}` |
|       ! 0 | 14608 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14609 | `			}` |
|      3845 | 14610 | `		}` |
|         - | 14611 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14612 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  12617121 | 14613 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7336541 | 14614 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   7336541 | 14615 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14616 | `				bStmtIsDeclare = 1;` |
|        21 | 14617 | `			}` |
|   3668268 | 14618 | `		}` |
|  12617121 | 14619 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14620 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14621 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    945731 | 14622 | `			pGen->bStrictTypesLocked = 1;` |
|    472863 | 14623 | `		}` |
|  12617121 | 14624 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14625 | `			/* Compile block */` |
|      3839 | 14626 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3839 | 14627 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14628 | `				break;` |
|         - | 14629 | `			}` |
|      1922 | 14630 | `		}else{` |
|  12613287 | 14631 | `			xCons = 0;` |
|  12613287 | 14632 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14633 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14634 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14635 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34427 | 14636 | `				xCons = PH7_CompileClassModifiers;` |
|  12596076 | 14637 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14638 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14639 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3849 | 14640 | `				xCons = PH7_CompileEnum;` |
|  12576943 | 14641 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7302145 | 14642 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14643 | `				/* Try to extract a language construct handler */` |
|   7302145 | 14644 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   7302145 | 14645 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14646 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14647 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14648 | `						&pGen->pIn->sData);` |
|         9 | 14649 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14650 | `						break;` |
|         - | 14651 | `					}` |
|         - | 14652 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14653 | `					 * this erroneous statement.` |
|         - | 14654 | `					 */` |
|         9 | 14655 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14656 | `				}` |
|   8923951 | 14657 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    405735 | 14658 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14659 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14660 | `				xCons = PH7_CompileLabel;` |
|        56 | 14661 | `			}` |
|  12613287 | 14662 | `			if( xCons == 0 ){` |
|         - | 14663 | `				/* Assume an expression an try to compile it */` |
|   5311399 | 14664 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5311399 | 14665 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14666 | `					/* Pop l-value */` |
|   5311249 | 14667 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2655622 | 14668 | `				}` |
|   2655702 | 14669 | `			}else{` |
|         - | 14670 | `				/* Go compile the sucker */` |
|   7301893 | 14671 | `				rc = xCons(&(*pGen));` |
|         - | 14672 | `			}` |
|  12613287 | 14673 | `			if( rc == SXERR_ABORT ){` |
|         - | 14674 | `				/* Request to abort compilation */` |
|        12 | 14675 | `				break;` |
|         - | 14676 | `			}` |
|         - | 14677 | `		}` |
|         - | 14678 | `		/* Ignore trailing semi-colons ';' */` |
|  21608271 | 14679 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   8991165 | 14680 | `			pGen->pIn++;` |
|         5 | 14681 | `		}` |
|  12617111 | 14682 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14683 | `			/* Compile a single statement and return */` |
|  11671353 | 14684 | `			break;` |
|         - | 14685 | `		}` |
|         - | 14686 | `		/* LOOP ONE */` |
|         - | 14687 | `		/* LOOP TWO */` |
|         - | 14688 | `		/* LOOP THREE */` |
|         - | 14689 | `		/* LOOP FOUR */` |
|         5 | 14690 | `	}` |
|         - | 14691 | `	/* Return compilation status */` |
|  11738851 | 14692 | `	return rc;` |
|         5 | 14693 | `}` |
|         - | 14694 | `/*` |
|         - | 14695 | ` * Compile a Raw PHP chunk.` |
|         - | 14696 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14697 | ` * takes care of generating the appropriate error message.` |
|         - | 14698 | ` */` |
|     67500 | 14699 | `static sxi32 PH7_CompilePHP(` |
|         - | 14700 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14701 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14702 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14703 | `	)` |
|         5 | 14704 | `{` |
|     67505 | 14705 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14706 | `	sxi32 rc;` |
|         - | 14707 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67505 | 14708 | `	SySetReset(&(*pTokenSet));` |
|     67505 | 14709 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14710 | `	/* Mark as the default token set */` |
|     67505 | 14711 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14712 | `	/* Advance the stream cursor */` |
|     67505 | 14713 | `	pGen->pRawIn++;` |
|         - | 14714 | `	/* Tokenize the PHP chunk first */` |
|     67505 | 14715 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14716 | `	/* Point to the head and tail of the token stream. */` |
|     67505 | 14717 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67505 | 14718 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67505 | 14719 | `	if( is_expr ){` |
|       ! 0 | 14720 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14721 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14722 | `			/* A simple expression,compile it */` |
|       ! 0 | 14723 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14724 | `		}` |
|         - | 14725 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14726 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14727 | `		return SXRET_OK;` |
|         - | 14728 | `	}` |
|     67505 | 14729 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14730 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14731 | `		/*` |
|         - | 14732 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14733 | `		 * According to the PHP reference manual:` |
|         - | 14734 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14735 | `		 *  immediately follow` |
|         - | 14736 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14737 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14738 | `		 * Symisc extension:` |
|         - | 14739 | `		 *   This short syntax works with all PHP opening` |
|         - | 14740 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14741 | `		 *   only short tag.` |
|         - | 14742 | `		 */` |
|         - | 14743 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14744 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14745 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14746 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14747 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14748 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14749 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14750 | `		}` |
|         3 | 14751 | `		return SXRET_OK;` |
|         - | 14752 | `	}` |
|         - | 14753 | `	/* Compile the PHP chunk */` |
|     67503 | 14754 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14755 | `	/* Fix exceptions jumps */` |
|     67503 | 14756 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14757 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67503 | 14758 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14759 | `		rc = SXERR_ABORT;` |
|         1 | 14760 | `	}` |
|         - | 14761 | `	/* Reset container */` |
|     67503 | 14762 | `	SySetReset(&pGen->aGoto);` |
|     67503 | 14763 | `	SySetReset(&pGen->aLabel);` |
|     67503 | 14764 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14765 | `	/* Compilation result */` |
|     67503 | 14766 | `	return rc;` |
|     33755 | 14767 | `}` |
|         - | 14768 | `/*` |
|         - | 14769 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14770 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14771 | ` * This is the only compile interface exported from this file.` |
|         - | 14772 | ` */` |
|     70674 | 14773 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14774 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14775 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14776 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14777 | `	)` |
|         5 | 14778 | `{` |
|         - | 14779 | `	SySet aPhpToken,aRawToken;` |
|         - | 14780 | `	ph7_gen_state *pCodeGen;` |
|         - | 14781 | `	ph7_value *pRawObj;` |
|         - | 14782 | `	sxu32 nObjIdx;` |
|         - | 14783 | `	sxi32 nRawObj;` |
|         - | 14784 | `	int is_expr;` |
|         - | 14785 | `	sxi8 bSavedStrict;` |
|         - | 14786 | `	sxi8 bSavedStrictLocked;` |
|         - | 14787 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 14788 | `	sxi32 rc;` |
|     70679 | 14789 | `	sxu32 nBaseLine = 1;` |
|     70679 | 14790 | `	if( pScript->nByte < 1 ){` |
|         - | 14791 | `		/* Nothing to compile */` |
|       ! 0 | 14792 | `		return PH7_OK;` |
|         - | 14793 | `	}` |
|         - | 14794 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 14795 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 14796 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     70679 | 14797 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 14798 | `		const char *z = pScript->zString;` |
|         3 | 14799 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 14800 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 14801 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 14802 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 14803 | `		pScript->zString = z;` |
|         3 | 14804 | `		nBaseLine = 2;` |
|         3 | 14805 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 14806 | `			return PH7_OK;` |
|         - | 14807 | `		}` |
|         1 | 14808 | `	}` |
|         - | 14809 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14810 | `	 * file's flags so include/require restore them on return. */` |
|     70679 | 14811 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 14812 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 14813 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 14814 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 14815 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 14816 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 14817 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     70679 | 14818 | `	pSavedIn = pCodeGen->pIn;` |
|     70679 | 14819 | `	pSavedEnd = pCodeGen->pEnd;` |
|     70679 | 14820 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     70679 | 14821 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     70679 | 14822 | `	pCodeGen->bStrictTypes = 0;` |
|     70679 | 14823 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14824 | `	/* Initialize the tokens containers */` |
|     70679 | 14825 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70679 | 14826 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70679 | 14827 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     70679 | 14828 | `	is_expr = 0;` |
|     70679 | 14829 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14830 | `		SyToken sTmp;` |
|         - | 14831 | `		/* PHP only: -*/` |
|     57363 | 14832 | `		sTmp.nLine = 1;` |
|     57363 | 14833 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57363 | 14834 | `		sTmp.pUserData = 0;` |
|     57363 | 14835 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57363 | 14836 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57363 | 14837 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14838 | `			/* A simple PHP expression */` |
|       ! 0 | 14839 | `			is_expr = 1;` |
|       ! 0 | 14840 | `		}` |
|     28684 | 14841 | `	}else{` |
|         - | 14842 | `		/* Tokenize raw text */` |
|     13321 | 14843 | `		SySetAlloc(&aRawToken,32);` |
|     13321 | 14844 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 14845 | `	}` |
|         - | 14846 | `	/* Process high-level tokens */` |
|     70679 | 14847 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     70679 | 14848 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     70679 | 14849 | `	rc = PH7_OK;` |
|     70679 | 14850 | `	if( is_expr ){` |
|         - | 14851 | `		/* Compile the expression */` |
|       ! 0 | 14852 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14853 | `		goto cleanup;` |
|         - | 14854 | `	}` |
|     70679 | 14855 | `	nObjIdx = 0;` |
|         - | 14856 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14857 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14858 | `	 * preventing namespace bleeding across include()d files. */` |
|     70679 | 14859 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14860 | `	/* Start the compilation process */` |
|     42000 | 14861 | `	for(;;){` |
|    151493 | 14862 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     70667 | 14863 | `			break; /* No more tokens to process */` |
|         - | 14864 | `		}` |
|     80831 | 14865 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14866 | `			/* Compile the PHP chunk */` |
|     67505 | 14867 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67505 | 14868 | `			if( rc == SXERR_ABORT ){` |
|        15 | 14869 | `				break;` |
|         - | 14870 | `			}` |
|     67493 | 14871 | `			continue;` |
|         - | 14872 | `		}` |
|         - | 14873 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13331 | 14874 | `		nRawObj = 0;` |
|     26657 | 14875 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14876 | `			/* Consume the raw chunk without any processing */` |
|     13331 | 14877 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13331 | 14878 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14879 | `				rc = SXERR_MEM;` |
|       ! 0 | 14880 | `				break;` |
|         - | 14881 | `			}` |
|         - | 14882 | `			/* Mark as constant and emit the load constant instruction */` |
|     13331 | 14883 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13331 | 14884 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13331 | 14885 | `			++nRawObj;` |
|     13331 | 14886 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14887 | `		}` |
|     13331 | 14888 | `		if( nRawObj > 0 ){` |
|         - | 14889 | `			/* Emit the consume instruction */` |
|     13331 | 14890 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6663 | 14891 | `		}` |
|     35342 | 14892 | `	}` |
|     35337 | 14893 | `cleanup:` |
|         - | 14894 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     70679 | 14895 | `	pCodeGen->pIn = pSavedIn;` |
|     70679 | 14896 | `	pCodeGen->pEnd = pSavedEnd;` |
|     70679 | 14897 | `	SySetRelease(&aRawToken);` |
|     70679 | 14898 | `	SySetRelease(&aPhpToken);` |
|         - | 14899 | `	/* Restore outer file's strict_types scope */` |
|     70679 | 14900 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     70679 | 14901 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     70679 | 14902 | `	return rc;` |
|     35342 | 14903 | `}` |
|         - | 14904 | `/*` |
|         - | 14905 | ` * Utility routines.Initialize the code generator.` |
|         - | 14906 | ` */` |
|      3816 | 14907 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 14908 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14909 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14910 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14911 | `	)` |
|         5 | 14912 | `{` |
|      3821 | 14913 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14914 | `	/* Zero the structure */` |
|      3821 | 14915 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 14916 | `	/* Initial state */` |
|      3821 | 14917 | `	pGen->pVm  = &(*pVm);` |
|      3821 | 14918 | `	pGen->xErr = xErr;` |
|      3821 | 14919 | `	pGen->pErrData = pErrData;` |
|      3821 | 14920 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3821 | 14921 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3821 | 14922 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3821 | 14923 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3821 | 14924 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3821 | 14925 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3821 | 14926 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3821 | 14927 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3821 | 14928 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 14929 | `	/* Error log buffer */` |
|      3821 | 14930 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 14931 | `	/* General purpose working buffer */` |
|      3821 | 14932 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 14933 | `	/* Namespace state */` |
|      3821 | 14934 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3821 | 14935 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3821 | 14936 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3821 | 14937 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14938 | `	/* Create the global scope */` |
|      3821 | 14939 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 14940 | `	/* Point to the global scope */` |
|      3821 | 14941 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3821 | 14942 | `	return SXRET_OK;` |
|         5 | 14943 | `}` |
|         - | 14944 | `/*` |
|         - | 14945 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 14946 | ` */` |
|     74034 | 14947 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 14948 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14949 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14950 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14951 | `	)` |
|         5 | 14952 | `{` |
|     74039 | 14953 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14954 | `	GenBlock *pBlock,*pParent;` |
|         - | 14955 | `	/* Reset state */` |
|     74039 | 14956 | `	SySetReset(&pGen->aLabel);` |
|     74039 | 14957 | `	SySetReset(&pGen->aGoto);` |
|     74039 | 14958 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     74039 | 14959 | `	SySetReset(&pGen->aTrivia);` |
|     74039 | 14960 | `	SySetReset(&pGen->aPendingAttrs);` |
|     74039 | 14961 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     74039 | 14962 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     74039 | 14963 | `	SyBlobRelease(&pGen->sWorker);` |
|     74039 | 14964 | `	SyBlobRelease(&pGen->sNamespace);` |
|     74039 | 14965 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     74039 | 14966 | `	SyHashRelease(&pGen->hUseImports);` |
|     74039 | 14967 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     74039 | 14968 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     74039 | 14969 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     74039 | 14970 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     74039 | 14971 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14972 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 14973 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 14974 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 14975 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 14976 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 14977 | `	 * number of unique names, which is acceptable. */` |
|         - | 14978 | `	/* Point to the global scope */` |
|     74039 | 14979 | `	pBlock = pGen->pCurrent;` |
|     74039 | 14980 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 14981 | `		pParent = pBlock->pParent;` |
|       ! 0 | 14982 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 14983 | `		pBlock = pParent;` |
|       ! 0 | 14984 | `	}` |
|     74039 | 14985 | `	pGen->xErr = xErr;` |
|     74039 | 14986 | `	pGen->pErrData = pErrData;` |
|     74039 | 14987 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     74039 | 14988 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     74039 | 14989 | `	pGen->pIn = pGen->pEnd = 0;` |
|     74039 | 14990 | `	pGen->nErr = 0;` |
|     74039 | 14991 | `	return SXRET_OK;` |
|         5 | 14992 | `}` |
|         - | 14993 | `/*` |
|         - | 14994 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 14995 | ` * php's parser prints, e.g.` |
|         - | 14996 | ` *` |
|         - | 14997 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 14998 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 14999 | ` *   syntax error, unexpected end of file` |
|         - | 15000 | ` *` |
|         - | 15001 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 15002 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 15003 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 15004 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 15005 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 15006 | ` *` |
|         - | 15007 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 15008 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 15009 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 15010 | ` */` |
|       182 | 15011 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 15012 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 15013 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 15014 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 15015 | `	)` |
|         5 | 15016 | `{` |
|       187 | 15017 | `	const char *zNoun = "token";` |
|         - | 15018 | `	sxu32 nLine;` |
|       187 | 15019 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 15020 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 15021 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 15022 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 15023 | `		 * it before concluding "end of file". */` |
|        92 | 15024 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        92 | 15025 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        92 | 15026 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 15027 | `			pTok = pGen->pEnd;` |
|        44 | 15028 | `		}` |
|        44 | 15029 | `	}` |
|       187 | 15030 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       187 | 15031 | `	if( pTok == 0 ){` |
|       ! 0 | 15032 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 15033 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 15034 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 15035 | `			zExpecting);` |
|         - | 15036 | `	}` |
|       187 | 15037 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 15038 | `		zNoun = "identifier";` |
|       180 | 15039 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         8 | 15040 | `		zNoun = "variable";` |
|       171 | 15041 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        23 | 15042 | `		zNoun = "integer";` |
|       158 | 15043 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 15044 | `		zNoun = "float";` |
|       ! 0 | 15045 | `	}` |
|       187 | 15046 | `	if( zExpecting ){` |
|       118 | 15047 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 15048 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 15049 | `	}` |
|       164 | 15050 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 15051 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        96 | 15052 | `}` |
|         - | 15053 | `/*` |
|         - | 15054 | ` * Generate a compile-time error message.` |
|         - | 15055 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 15056 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 15057 | ` * abort compilation immediately.` |
|         - | 15058 | ` */` |
|     15942 | 15059 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 15060 | `{` |
|     15947 | 15061 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     15947 | 15062 | `	const char *zErr = "Error";` |
|         - | 15063 | `	SyString *pFile;` |
|         - | 15064 | `	va_list ap;` |
|         - | 15065 | `	sxi32 rc;` |
|         - | 15066 | `	/* Reset the working buffer */` |
|     15947 | 15067 | `	SyBlobReset(pWorker);` |
|         - | 15068 | `	/* Peek the processed file path if available */` |
|     15947 | 15069 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     15947 | 15070 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 15071 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 15072 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 15073 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 15074 | `		 * into execution with a 0 exit status. */` |
|       659 | 15075 | `		pGen->nErr++;` |
|       659 | 15076 | `		if( pGen->nErr > 15 ){` |
|         - | 15077 | `			/* Error count limit reached */` |
|         6 | 15078 | `			if( pGen->xErr ){` |
|         6 | 15079 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 15080 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 15081 | `				if( pFile ){` |
|         6 | 15082 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 15083 | `				}` |
|         6 | 15084 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 15085 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 15086 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 15087 | `				}` |
|         2 | 15088 | `			}` |
|         - | 15089 | `			/* Abort immediately */` |
|         6 | 15090 | `			return SXERR_ABORT;` |
|         - | 15091 | `		}` |
|       325 | 15092 | `	}` |
|     15943 | 15093 | `	if( pGen->xErr == 0 ){` |
|         - | 15094 | `		/* No available error consumer,return immediately */` |
|     15271 | 15095 | `		return SXRET_OK;` |
|         - | 15096 | `	}` |
|       677 | 15097 | `	switch(nErrType){` |
|       310 | 15098 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 15099 | `	case E_WARNING: zErr = "Warning";     break;` |
|       346 | 15100 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 15101 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 15102 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 15103 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 15104 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        16 | 15105 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 15106 | `	default:` |
|       ! 0 | 15107 | `		break;` |
|         - | 15108 | `	}` |
|       677 | 15109 | `	rc = SXRET_OK;` |
|         - | 15110 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       677 | 15111 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       677 | 15112 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       677 | 15113 | `	va_start(ap,zFormat);` |
|       677 | 15114 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       677 | 15115 | `	va_end(ap);` |
|       677 | 15116 | `	if( pFile ){` |
|       677 | 15117 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       336 | 15118 | `	}` |
|         - | 15119 | `	/* Append a new line */` |
|       677 | 15120 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       677 | 15121 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 15122 | `		/* Consume the generated error message */` |
|       677 | 15123 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       336 | 15124 | `	}` |
|       677 | 15125 | `	return rc;` |
|      7976 | 15126 | `}` |
|         - | 15127 |  |
